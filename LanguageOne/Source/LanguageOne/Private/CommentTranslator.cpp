// Copyright Epic Games, Inc. All Rights Reserved.

#include "CommentTranslator.h"
#include "LanguageOneCompatibility.h"
#include "LanguageOneSettings.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Misc/SecureHash.h"
#include "HAL/PlatformTime.h"

namespace
{
	// 先访问 www.bing.com；国内网络会被跳转到 cn.bing.com，实际落地域名从页面的 canonical 链接读取。
	// 失败重试时换另一个入口，cn.bing.com 国内外都能直连。
	const TCHAR* const BingPrimaryHost = TEXT("https://www.bing.com");
	const TCHAR* const BingFallbackHost = TEXT("https://cn.bing.com");
	const TCHAR* const BrowserUserAgent = TEXT("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0.0.0 Safari/537.36");
	constexpr float FreeServiceTimeoutSeconds = 10.0f;

	// 返回空字符串表示响应可用；返回的错误都属于"连不上"，会触发熔断
	FString GetFreeServiceHttpError(const TCHAR* ServiceName, const FHttpResponsePtr& Response, bool bSuccess)
	{
		if (!bSuccess || !Response.IsValid())
		{
			return FString::Printf(TEXT("%s 请求失败，可能需要代理或切换翻译服务 | %s request failed, try a proxy or another translation service"), ServiceName, ServiceName);
		}

		const int32 ResponseCode = Response->GetResponseCode();
		if (!EHttpResponseCodes::IsOk(ResponseCode))
		{
			return FString::Printf(TEXT("%s 返回 HTTP %d，请稍后重试或切换翻译服务 | %s returned HTTP %d, retry later or switch translation service"), ServiceName, ResponseCode, ServiceName, ResponseCode);
		}

		return FString();
	}

	FString ExtractBetween(const FString& Text, const TCHAR* Prefix, const TCHAR* Suffix, int32 SearchFrom = 0)
	{
		const int32 PrefixPos = Text.Find(Prefix, ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchFrom);
		if (PrefixPos == INDEX_NONE)
		{
			return FString();
		}

		const int32 ValueStart = PrefixPos + FCString::Strlen(Prefix);
		const int32 ValueEnd = Text.Find(Suffix, ESearchCase::CaseSensitive, ESearchDir::FromStart, ValueStart);
		if (ValueEnd == INDEX_NONE)
		{
			return FString();
		}

		return Text.Mid(ValueStart, ValueEnd - ValueStart);
	}

	// 简单按字符区间判断源语言，返回 ISO 639-1 代码；需要显式源语言的服务（MyMemory、TranSmart）使用
	FString DetectSourceLanguage(const FString& Text)
	{
		for (const TCHAR Char : Text)
		{
			// 中文字符 (CJK统一汉字: 0x4E00 - 0x9FFF)
			if (Char >= 0x4E00 && Char <= 0x9FFF)
			{
				return TEXT("zh");
			}
			// 日文 (平假名/片假名)
			if ((Char >= 0x3040 && Char <= 0x309F) || (Char >= 0x30A0 && Char <= 0x30FF))
			{
				return TEXT("ja");
			}
			// 韩文
			if (Char >= 0xAC00 && Char <= 0xD7AF)
			{
				return TEXT("ko");
			}
			// 俄文
			if (Char >= 0x0400 && Char <= 0x04FF)
			{
				return TEXT("ru");
			}
		}
		return TEXT("en");
	}

	struct FBingAuth
	{
		FString Host;
		FString IG;
		FString IID;
		FString Key;
		FString Token;
		FString Cookie;
		double ExpireTime = 0.0;

		bool IsValid() const
		{
			return !Key.IsEmpty() && !Token.IsEmpty() && FPlatformTime::Seconds() < ExpireTime;
		}
	};

	FBingAuth BingAuth;
	FString BingEntryHost = BingPrimaryHost;
	TArray<TFunction<void(const FString&, bool)>> PendingBingAuthCallbacks;

	// 页面中的格式：params_AbusePreventionHelper = [key,"token",有效期毫秒];
	bool ParseBingAuth(const FString& Page, FBingAuth& OutAuth)
	{
		const int32 HelperPos = Page.Find(TEXT("params_AbusePreventionHelper"), ESearchCase::CaseSensitive);
		if (HelperPos == INDEX_NONE)
		{
			return false;
		}

		TArray<FString> Params;
		ExtractBetween(Page, TEXT("["), TEXT("]"), HelperPos).ParseIntoArray(Params, TEXT(","));
		if (Params.Num() < 3)
		{
			return false;
		}

		OutAuth.Key = Params[0].TrimStartAndEnd();
		OutAuth.Token = Params[1].TrimStartAndEnd().TrimQuotes();
		OutAuth.IG = ExtractBetween(Page, TEXT("IG:\""), TEXT("\""));
		OutAuth.IID = ExtractBetween(Page, TEXT("data-iid=\""), TEXT("\""));

		// 提前一分钟刷新，避免请求途中过期
		const double LifetimeSeconds = FCString::Atod(*Params[2].TrimStartAndEnd()) / 1000.0;
		OutAuth.ExpireTime = FPlatformTime::Seconds() + FMath::Max(LifetimeSeconds - 60.0, 60.0);

		return !OutAuth.Key.IsEmpty() && !OutAuth.Token.IsEmpty();
	}

	// 页面格式：<link rel="canonical" href="https://cn.bing.com/translator" />。不依赖 GetEffectiveURL，各引擎版本都可用
	FString ParseBingLandingHost(const FString& Page, const FString& EntryHost)
	{
		const FString Canonical = ExtractBetween(Page, TEXT("<link rel=\"canonical\" href=\""), TEXT("\""));
		const int32 PathPos = Canonical.Find(TEXT("/translator"), ESearchCase::IgnoreCase);
		if (PathPos != INDEX_NONE && Canonical.StartsWith(TEXT("https://")) && Canonical.Left(PathPos).EndsWith(TEXT(".bing.com")))
		{
			return Canonical.Left(PathPos);
		}
		return EntryHost;
	}

	// 只取每个 Set-Cookie 的 name=value。HTTP 层可能把多个 Set-Cookie 合并成一行，用逗号分隔
	FString BuildCookieHeader(const FHttpResponsePtr& Response)
	{
		TArray<FString> Pairs;
		for (const FString& Header : Response->GetAllHeaders())
		{
			FString Name;
			FString Value;
			if (!Header.Split(TEXT(":"), &Name, &Value) || !Name.TrimStartAndEnd().Equals(TEXT("Set-Cookie"), ESearchCase::IgnoreCase))
			{
				continue;
			}

			TArray<FString> Pieces;
			Value.ParseIntoArray(Pieces, TEXT(","));
			for (const FString& Piece : Pieces)
			{
				FString Pair;
				if (!Piece.Split(TEXT(";"), &Pair, nullptr))
				{
					Pair = Piece;
				}
				Pair.TrimStartAndEndInline();

				// expires 日期里的逗号会切出 "03-Nov-2027 17:44:16 GMT" 这样的片段，它不含 name=，跳过
				int32 EqualsPos = INDEX_NONE;
				if (Pair.FindChar(TEXT('='), EqualsPos) && EqualsPos > 0 && !Pair.Left(EqualsPos).Contains(TEXT(" ")))
				{
					Pairs.Add(Pair);
				}
			}
		}
		return FString::Join(Pairs, TEXT("; "));
	}

	constexpr int32 BingChunkLimit = 900;

	TArray<FString> SplitForBing(const FString& Text)
	{
		TArray<FString> Chunks;
		int32 Start = 0;
		while (Start < Text.Len())
		{
			int32 End = FMath::Min(Start + BingChunkLimit, Text.Len());
			if (End < Text.Len())
			{
				for (int32 Index = End - 1; Index > Start + BingChunkLimit / 2; --Index)
				{
					const TCHAR Char = Text[Index];
					if (Char == TEXT('\n') || Char == TEXT('.') || Char == TEXT('。') || Char == TEXT('!') || Char == TEXT('！') || Char == TEXT('?') || Char == TEXT('？'))
					{
						End = Index + 1;
						break;
					}
				}
			}
			Chunks.Add(Text.Mid(Start, End - Start));
			Start = End;
		}
		return Chunks;
	}

	// 回调参数：错误信息（空表示授权可用）、是否属于连不上。并发请求共享同一次页面抓取
	void RequestBingAuth(TFunction<void(const FString&, bool)> OnReady)
	{
		if (BingAuth.IsValid())
		{
			OnReady(FString(), false);
			return;
		}

		PendingBingAuthCallbacks.Add(MoveTemp(OnReady));
		if (PendingBingAuthCallbacks.Num() > 1)
		{
			return;
		}

		const FString EntryHost = BingEntryHost;
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> AuthRequest = FHttpModule::Get().CreateRequest();
		AuthRequest->SetURL(EntryHost + TEXT("/translator"));
		AuthRequest->SetVerb(TEXT("GET"));
		AuthRequest->SetHeader(TEXT("User-Agent"), BrowserUserAgent);
		AuthRequest->SetTimeout(FreeServiceTimeoutSeconds);
		AuthRequest->OnProcessRequestComplete().BindLambda([EntryHost](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
		{
			FString Error = GetFreeServiceHttpError(TEXT("Bing"), Response, bSuccess);
			const bool bUnavailable = !Error.IsEmpty();
			if (!bUnavailable)
			{
				const FString Page = Response->GetContentAsString();
				FBingAuth NewAuth;
				if (ParseBingAuth(Page, NewAuth))
				{
					NewAuth.Host = ParseBingLandingHost(Page, EntryHost);
					NewAuth.Cookie = BuildCookieHeader(Response);
					BingAuth = NewAuth;
				}
				else
				{
					Error = TEXT("无法解析微软Bing翻译授权，请切换翻译服务 | Failed to parse Microsoft Bing auth, please switch translation service");
				}
			}

			TArray<TFunction<void(const FString&, bool)>> Callbacks = MoveTemp(PendingBingAuthCallbacks);
			PendingBingAuthCallbacks.Reset();
			for (TFunction<void(const FString&, bool)>& Callback : Callbacks)
			{
				Callback(Error, bUnavailable);
			}
		});
		AuthRequest->ProcessRequest();
	}

	void SendBingTranslation(const FString& SourceText, const FString& BingTargetLang, FOnTranslationComplete OnComplete, FOnFreeServiceError OnError, bool bRetryOnFailure);

	// 鉴权或翻译失败时清掉缓存、换另一个入口域名，整体重试一次
	void RetryBingOrFail(const FString& SourceText, const FString& BingTargetLang, const FOnTranslationComplete& OnComplete, const FOnFreeServiceError& OnError,
		bool bRetryOnFailure, const FString& FailedEntryHost, const FString& ErrorMessage, bool bServiceUnavailable)
	{
		if (!bRetryOnFailure)
		{
			OnError.ExecuteIfBound(ErrorMessage, bServiceUnavailable);
			return;
		}

		// 并发请求同时失败时只切换一次
		if (BingEntryHost == FailedEntryHost)
		{
			BingEntryHost = (FailedEntryHost == BingPrimaryHost) ? BingFallbackHost : BingPrimaryHost;
			BingAuth = FBingAuth();
		}
		SendBingTranslation(SourceText, BingTargetLang, OnComplete, OnError, false);
	}

	void SendBingTranslation(const FString& SourceText, const FString& BingTargetLang, FOnTranslationComplete OnComplete, FOnFreeServiceError OnError, bool bRetryOnFailure)
	{
		const FString EntryHost = BingEntryHost;
		RequestBingAuth([SourceText, BingTargetLang, OnComplete, OnError, bRetryOnFailure, EntryHost](const FString& AuthError, bool bAuthUnavailable)
		{
			if (!AuthError.IsEmpty())
			{
				RetryBingOrFail(SourceText, BingTargetLang, OnComplete, OnError, bRetryOnFailure, EntryHost, AuthError, bAuthUnavailable);
				return;
			}

			TSharedRef<IHttpRequest, ESPMode::ThreadSafe> TransRequest = FHttpModule::Get().CreateRequest();
			TransRequest->SetURL(FString::Printf(TEXT("%s/ttranslatev3?isVertical=1&IG=%s&IID=%s.1"), *BingAuth.Host, *BingAuth.IG, *BingAuth.IID));
			TransRequest->SetVerb(TEXT("POST"));
			TransRequest->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));
			TransRequest->SetHeader(TEXT("User-Agent"), BrowserUserAgent);
			TransRequest->SetHeader(TEXT("Origin"), BingAuth.Host);
			TransRequest->SetHeader(TEXT("Referer"), BingAuth.Host + TEXT("/translator"));
			if (!BingAuth.Cookie.IsEmpty())
			{
				TransRequest->SetHeader(TEXT("Cookie"), BingAuth.Cookie);
			}
			TransRequest->SetTimeout(FreeServiceTimeoutSeconds);
			TransRequest->SetContentAsString(FString::Printf(TEXT("fromLang=auto-detect&to=%s&text=%s&token=%s&key=%s"),
				*BingTargetLang, *LANGUAGEONE_URL_ENCODE(SourceText), *LANGUAGEONE_URL_ENCODE(BingAuth.Token), *BingAuth.Key));

			TransRequest->OnProcessRequestComplete().BindLambda([SourceText, BingTargetLang, OnComplete, OnError, bRetryOnFailure, EntryHost](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
			{
				const FString HttpError = GetFreeServiceHttpError(TEXT("Bing"), Response, bSuccess);
				if (!HttpError.IsEmpty())
				{
					RetryBingOrFail(SourceText, BingTargetLang, OnComplete, OnError, bRetryOnFailure, EntryHost, HttpError, true);
					return;
				}

				TSharedPtr<FJsonValue> JsonValue;
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
				if (!FJsonSerializer::Deserialize(Reader, JsonValue) || !JsonValue.IsValid())
				{
					OnError.ExecuteIfBound(TEXT("解析微软Bing翻译响应失败 | Failed to parse Microsoft Bing response"), false);
					return;
				}

				// 授权失效时 HTTP 仍是 200，响应体为 {"statusCode":205,...}
				const TSharedPtr<FJsonObject>* ErrorObject = nullptr;
				if (JsonValue->TryGetObject(ErrorObject))
				{
					const int32 StatusCode = (*ErrorObject)->HasField(TEXT("statusCode")) ? (*ErrorObject)->GetIntegerField(TEXT("statusCode")) : 0;
					const FString RejectError = FString::Printf(TEXT("微软Bing翻译拒绝请求 (statusCode=%d)，请稍后重试或切换翻译服务 | Microsoft Bing rejected the request (statusCode=%d), retry later or switch translation service"), StatusCode, StatusCode);
					RetryBingOrFail(SourceText, BingTargetLang, OnComplete, OnError, bRetryOnFailure, EntryHost, RejectError, false);
					return;
				}

				// 响应格式: [{"translations":[{"text":"..."}]}]
				const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
				if (JsonValue->TryGetArray(Items) && Items->Num() > 0)
				{
					const TSharedPtr<FJsonObject>* FirstItem = nullptr;
					const TArray<TSharedPtr<FJsonValue>>* Translations = nullptr;
					if ((*Items)[0]->TryGetObject(FirstItem) && (*FirstItem)->TryGetArrayField(TEXT("translations"), Translations) && Translations->Num() > 0)
					{
						const TSharedPtr<FJsonObject>* FirstTranslation = nullptr;
						FString TranslatedText;
						if ((*Translations)[0]->TryGetObject(FirstTranslation) && (*FirstTranslation)->TryGetStringField(TEXT("text"), TranslatedText) && !TranslatedText.IsEmpty())
						{
							OnComplete.ExecuteIfBound(TranslatedText);
							return;
						}
					}
				}

				OnError.ExecuteIfBound(TEXT("未找到翻译结果 | No translation result found"), false);
			});
			TransRequest->ProcessRequest();
		});
	}

	struct FBingChunkState
	{
		TArray<FString> Chunks;
		int32 Index = 0;
		FString Accumulated;
		FString TargetLang;
		FOnTranslationComplete OnComplete;
		FOnFreeServiceError OnError;
	};

	// 按顺序逐段翻译后拼接
	void TranslateNextBingChunk(const TSharedRef<FBingChunkState>& State)
	{
		if (State->Index >= State->Chunks.Num())
		{
			State->OnComplete.ExecuteIfBound(State->Accumulated);
			return;
		}

		// 分段点在换行上时，接口可能吞掉段尾换行，拼接时补回
		const bool bChunkEndsWithLineBreak = State->Chunks[State->Index].EndsWith(TEXT("\n"));
		SendBingTranslation(State->Chunks[State->Index], State->TargetLang,
			FOnTranslationComplete::CreateLambda([State, bChunkEndsWithLineBreak](const FString& TranslatedChunk)
			{
				State->Accumulated += TranslatedChunk;
				if (bChunkEndsWithLineBreak && !TranslatedChunk.EndsWith(TEXT("\n")))
				{
					State->Accumulated += TEXT("\n");
				}
				State->Index++;
				TranslateNextBingChunk(State);
			}),
			FOnFreeServiceError::CreateLambda([State](const FString& ErrorMessage, bool bServiceUnavailable)
			{
				State->OnError.ExecuteIfBound(ErrorMessage, bServiceUnavailable);
			}),
			true);
	}

	constexpr double FreeServiceCooldownSeconds = 300.0;

	// 本次编辑器会话内各免费服务的可用状态
	struct FFreeServiceHealth
	{
		double CooldownUntil = 0.0;
		// 成功过且之后没有再连不上。未确认的服务同一时间只放一个请求去试探，其余排队等结果，
		// 这样批量翻译时不可达的服务最多只超时一次
		bool bConfirmed = false;
		bool bProbing = false;
		TArray<TFunction<void()>> Waiters;
	};

	TMap<ETranslateProvider, FFreeServiceHealth> FreeServiceHealth;
	bool bHasLastFreeSuccess = false;
	ETranslateProvider LastFreeSuccess = ETranslateProvider::MicrosoftFree;
	bool bHasNotifiedFallback = false;
	ETranslateProvider LastNotifiedFallback = ETranslateProvider::MicrosoftFree;

	bool IsFreeProvider(ETranslateProvider Provider)
	{
		return Provider == ETranslateProvider::GoogleFree
			|| Provider == ETranslateProvider::MicrosoftFree
			|| Provider == ETranslateProvider::YoudaoFree
			|| Provider == ETranslateProvider::TencentFree;
	}

	bool IsFreeServiceCoolingDown(ETranslateProvider Provider)
	{
		const FFreeServiceHealth* Health = FreeServiceHealth.Find(Provider);
		return Health && FPlatformTime::Seconds() < Health->CooldownUntil;
	}

	// 只有连不上才进入冷却；解析失败、额度用完等不影响下次尝试
	void RecordFreeServiceResult(ETranslateProvider Provider, bool bSucceeded, bool bServiceUnavailable)
	{
		FFreeServiceHealth& Health = FreeServiceHealth.FindOrAdd(Provider);
		if (bSucceeded)
		{
			Health.CooldownUntil = 0.0;
			Health.bConfirmed = true;
			LastFreeSuccess = Provider;
			bHasLastFreeSuccess = true;
		}
		else if (bServiceUnavailable)
		{
			Health.CooldownUntil = FPlatformTime::Seconds() + FreeServiceCooldownSeconds;
			Health.bConfirmed = false;
		}
	}

	// 试探请求结束后放行排队的请求，它们会按最新状态重新判断要不要用这个服务
	void FinishFreeServiceProbe(ETranslateProvider Provider)
	{
		FFreeServiceHealth& Health = FreeServiceHealth.FindOrAdd(Provider);
		Health.bProbing = false;
		TArray<TFunction<void()>> Waiters = MoveTemp(Health.Waiters);
		Health.Waiters.Reset();
		for (TFunction<void()>& Waiter : Waiters)
		{
			Waiter();
		}
	}

	FString AllFreeServicesCoolingDownError()
	{
		const double Now = FPlatformTime::Seconds();
		double SecondsLeft = FreeServiceCooldownSeconds;
		for (const TPair<ETranslateProvider, FFreeServiceHealth>& Pair : FreeServiceHealth)
		{
			if (Pair.Value.CooldownUntil > Now)
			{
				SecondsLeft = FMath::Min(SecondsLeft, Pair.Value.CooldownUntil - Now);
			}
		}
		const int32 Seconds = FMath::Max(1, FMath::CeilToInt(SecondsLeft));
		return FString::Printf(TEXT("免费翻译服务最近都连接失败，约 %d 秒后再自动尝试；请检查网络或代理 | All free translation services failed recently, retrying in about %d s. Check your network or proxy"), Seconds, Seconds);
	}

	const TCHAR* FreeProviderLabel(ETranslateProvider Provider)
	{
		switch (Provider)
		{
		case ETranslateProvider::MicrosoftFree: return TEXT("Bing");
		case ETranslateProvider::TencentFree: return TEXT("TranSmart");
		case ETranslateProvider::GoogleFree: return TEXT("Google Web");
		case ETranslateProvider::YoudaoFree: return TEXT("MyMemory");
		default: return TEXT("Translation");
		}
	}

	// 记录实际使用的服务；切换到新的备用服务时只提示一次，回到首选服务后重置
	void NoteFreeServiceUsed(ETranslateProvider Selected, ETranslateProvider Provider)
	{
		UE_LOG(LogTemp, Log, TEXT("LanguageOne: 本次翻译使用 %s | Translated with %s"), FreeProviderLabel(Provider), FreeProviderLabel(Provider));
		if (Provider == Selected)
		{
			bHasNotifiedFallback = false;
			return;
		}
		if (bHasNotifiedFallback && LastNotifiedFallback == Provider)
		{
			return;
		}
		bHasNotifiedFallback = true;
		LastNotifiedFallback = Provider;
		FCommentTranslator::OnFreeServiceSwitched().Broadcast(FreeProviderLabel(Provider));
	}

	TArray<ETranslateProvider> BuildFreeFallbackChain(ETranslateProvider Selected)
	{
		const ETranslateProvider DefaultOrder[] = {
			ETranslateProvider::MicrosoftFree,
			ETranslateProvider::TencentFree,
			ETranslateProvider::GoogleFree,
			ETranslateProvider::YoudaoFree
		};

		TArray<ETranslateProvider> Chain;
		Chain.Add(Selected);
		if (bHasLastFreeSuccess && LastFreeSuccess != Selected && IsFreeProvider(LastFreeSuccess))
		{
			Chain.Add(LastFreeSuccess);
		}
		for (ETranslateProvider Provider : DefaultOrder)
		{
			if (!Chain.Contains(Provider))
			{
				Chain.Add(Provider);
			}
		}
		return Chain;
	}

	FString LanguageCodeFor(ETranslateProvider Provider, ETranslateTargetLanguage TargetLanguage)
	{
		auto GoogleCode = [TargetLanguage]()
		{
			switch (TargetLanguage)
			{
			case ETranslateTargetLanguage::Chinese: return TEXT("zh-CN");
			case ETranslateTargetLanguage::English: return TEXT("en");
			case ETranslateTargetLanguage::Japanese: return TEXT("ja");
			case ETranslateTargetLanguage::Korean: return TEXT("ko");
			case ETranslateTargetLanguage::German: return TEXT("de");
			case ETranslateTargetLanguage::French: return TEXT("fr");
			case ETranslateTargetLanguage::Spanish: return TEXT("es");
			case ETranslateTargetLanguage::Russian: return TEXT("ru");
			default: return TEXT("zh-CN");
			}
		};
		auto CommonCode = [TargetLanguage]()
		{
			switch (TargetLanguage)
			{
			case ETranslateTargetLanguage::Chinese: return TEXT("zh");
			case ETranslateTargetLanguage::English: return TEXT("en");
			case ETranslateTargetLanguage::Japanese: return TEXT("ja");
			case ETranslateTargetLanguage::Korean: return TEXT("ko");
			case ETranslateTargetLanguage::German: return TEXT("de");
			case ETranslateTargetLanguage::French: return TEXT("fr");
			case ETranslateTargetLanguage::Spanish: return TEXT("es");
			case ETranslateTargetLanguage::Russian: return TEXT("ru");
			default: return TEXT("zh");
			}
		};

		switch (Provider)
		{
		case ETranslateProvider::GoogleFree:
		case ETranslateProvider::Google:
			return GoogleCode();
		case ETranslateProvider::Baidu:
			switch (TargetLanguage)
			{
			case ETranslateTargetLanguage::Chinese: return TEXT("zh");
			case ETranslateTargetLanguage::English: return TEXT("en");
			case ETranslateTargetLanguage::Japanese: return TEXT("jp");
			case ETranslateTargetLanguage::Korean: return TEXT("kor");
			case ETranslateTargetLanguage::German: return TEXT("de");
			case ETranslateTargetLanguage::French: return TEXT("fra");
			case ETranslateTargetLanguage::Spanish: return TEXT("spa");
			case ETranslateTargetLanguage::Russian: return TEXT("ru");
			default: return TEXT("zh");
			}
		case ETranslateProvider::MicrosoftFree:
		case ETranslateProvider::YoudaoFree:
		case ETranslateProvider::TencentFree:
		case ETranslateProvider::Custom:
		default:
			return CommonCode();
		}
	}
}

struct FCommentTranslator::FFreeFallbackState
{
	TArray<ETranslateProvider> Chain;
	int32 Index = 0;
	FString Errors;
	FString SourceText;
	ETranslateProvider Selected = ETranslateProvider::MicrosoftFree;
	FOnTranslationComplete OnComplete;
	FOnTranslationError OnError;
};

FOnFreeServiceSwitched& FCommentTranslator::OnFreeServiceSwitched()
{
	static FOnFreeServiceSwitched Event;
	return Event;
}

void FCommentTranslator::DispatchTranslation(ETranslateProvider Provider, const FString& SourceText, FOnTranslationComplete OnComplete, FOnFreeServiceError OnError)
{
	const ULanguageOneSettings* Settings = GetDefault<ULanguageOneSettings>();
	const FString TargetLang = LanguageCodeFor(Provider, Settings ? Settings->TargetLanguage : ETranslateTargetLanguage::Chinese);

	// 付费服务不参与熔断，失败一律视为非"连不上"
	const FOnTranslationError PaidError = FOnTranslationError::CreateLambda([OnError](const FString& ErrorMessage)
	{
		OnError.ExecuteIfBound(ErrorMessage, false);
	});

	switch (Provider)
	{
	case ETranslateProvider::GoogleFree:
		TranslateWithGoogleFree(SourceText, TargetLang, OnComplete, OnError);
		break;
	case ETranslateProvider::MicrosoftFree:
		TranslateWithMicrosoftFree(SourceText, TargetLang, OnComplete, OnError);
		break;
	case ETranslateProvider::YoudaoFree:
		TranslateWithYoudaoFree(SourceText, TargetLang, OnComplete, OnError);
		break;
	case ETranslateProvider::TencentFree:
		TranslateWithTencentFree(SourceText, TargetLang, OnComplete, OnError);
		break;
	case ETranslateProvider::Baidu:
		TranslateWithBaidu(SourceText, TargetLang, OnComplete, PaidError);
		break;
	case ETranslateProvider::Google:
		TranslateWithGoogle(SourceText, TargetLang, OnComplete, PaidError);
		break;
	case ETranslateProvider::Custom:
		TranslateWithCustom(SourceText, TargetLang, OnComplete, PaidError);
		break;
	default:
		OnError.ExecuteIfBound(TEXT("未知的翻译服务商 | Unknown translation provider"), false);
		break;
	}
}

void FCommentTranslator::TranslateText(const FString& SourceText, FOnTranslationComplete OnComplete, FOnTranslationError OnError)
{
	if (SourceText.IsEmpty())
	{
		OnError.ExecuteIfBound(TEXT("源文本为空 | Source text is empty"));
		return;
	}

	const ULanguageOneSettings* Settings = GetDefault<ULanguageOneSettings>();
	if (!Settings)
	{
		OnError.ExecuteIfBound(TEXT("无法获取设置 | Cannot get settings"));
		return;
	}

	const ETranslateProvider Selected = Settings->TranslateProvider;
	if (!IsFreeProvider(Selected))
	{
		DispatchTranslation(Selected, SourceText, OnComplete, FOnFreeServiceError::CreateLambda([OnError](const FString& ErrorMessage, bool)
		{
			OnError.ExecuteIfBound(ErrorMessage + TEXT("\n请检查 API Key 或余额，或在设置中切换到免费翻译服务（支持自动切换） | Check the API key or quota, or switch to a free translation service"));
		}));
		return;
	}

	if (!Settings->bEnableAutoFallback)
	{
		DispatchTranslation(Selected, SourceText,
			FOnTranslationComplete::CreateLambda([OnComplete, Selected](const FString& TranslatedText)
			{
				RecordFreeServiceResult(Selected, true, false);
				NoteFreeServiceUsed(Selected, Selected);
				OnComplete.ExecuteIfBound(TranslatedText);
			}),
			FOnFreeServiceError::CreateLambda([OnError, Selected](const FString& ErrorMessage, bool bServiceUnavailable)
			{
				RecordFreeServiceResult(Selected, false, bServiceUnavailable);
				OnError.ExecuteIfBound(ErrorMessage);
			}));
		return;
	}

	TSharedRef<FFreeFallbackState> State = MakeShared<FFreeFallbackState>();
	State->Chain = BuildFreeFallbackChain(Selected);
	State->Selected = Selected;
	State->SourceText = SourceText;
	State->OnComplete = OnComplete;
	State->OnError = OnError;
	TryNextFreeService(State);
}

void FCommentTranslator::TryNextFreeService(const TSharedRef<FFreeFallbackState>& State)
{
	while (State->Index < State->Chain.Num() && IsFreeServiceCoolingDown(State->Chain[State->Index]))
	{
		State->Index++;
	}

	if (State->Index >= State->Chain.Num())
	{
		State->OnError.ExecuteIfBound(State->Errors.IsEmpty() ? AllFreeServicesCoolingDownError() : State->Errors);
		return;
	}

	const ETranslateProvider Provider = State->Chain[State->Index];
	FFreeServiceHealth& Health = FreeServiceHealth.FindOrAdd(Provider);
	const bool bIsProbe = !Health.bConfirmed;
	if (bIsProbe)
	{
		if (Health.bProbing)
		{
			Health.Waiters.Add([State]()
			{
				TryNextFreeService(State);
			});
			return;
		}
		Health.bProbing = true;
	}

	DispatchTranslation(
		Provider,
		State->SourceText,
		FOnTranslationComplete::CreateLambda([State, Provider, bIsProbe](const FString& TranslatedText)
		{
			RecordFreeServiceResult(Provider, true, false);
			if (bIsProbe)
			{
				FinishFreeServiceProbe(Provider);
			}
			NoteFreeServiceUsed(State->Selected, Provider);
			State->OnComplete.ExecuteIfBound(TranslatedText);
		}),
		FOnFreeServiceError::CreateLambda([State, Provider, bIsProbe](const FString& ErrorMessage, bool bServiceUnavailable)
		{
			RecordFreeServiceResult(Provider, false, bServiceUnavailable);
			if (bIsProbe)
			{
				FinishFreeServiceProbe(Provider);
			}
			State->Errors += FString::Printf(TEXT("%s: %s\n"), FreeProviderLabel(Provider), *ErrorMessage);
			State->Index++;
			TryNextFreeService(State);
		}));
}

void FCommentTranslator::TranslateWithGoogleFree(const FString& SourceText, const FString& TargetLang, FOnTranslationComplete OnComplete, FOnFreeServiceError OnError)
{
	// 使用 Google Translate 的免费接口（通过 translate.googleapis.com 的公开端点）
	// 注意：这个接口不稳定，可能随时失效
	FString EncodedText = LANGUAGEONE_URL_ENCODE(SourceText);
	FString Url = FString::Printf(TEXT("https://translate.googleapis.com/translate_a/single?client=gtx&sl=auto&tl=%s&dt=t&q=%s"),
		*TargetLang, *EncodedText);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(Url);
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->SetHeader(TEXT("User-Agent"), TEXT("Mozilla/5.0"));
	HttpRequest->SetTimeout(FreeServiceTimeoutSeconds);
	HttpRequest->OnProcessRequestComplete().BindLambda([OnComplete, OnError](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
	{
		const FString HttpError = GetFreeServiceHttpError(TEXT("Google Web"), Response, bSuccess);
		if (!HttpError.IsEmpty())
		{
			OnError.ExecuteIfBound(HttpError, true);
			return;
		}

		FString ResponseStr = Response->GetContentAsString();

		// Google 免费接口返回的是一个数组格式：[[["翻译结果","原文"]]]
		TSharedPtr<FJsonValue> JsonValue;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseStr);

		if (!FJsonSerializer::Deserialize(Reader, JsonValue) || !JsonValue.IsValid())
		{
			OnError.ExecuteIfBound(TEXT("解析响应失败 | Failed to parse response"), false);
			return;
		}

		// 解析多层数组结构
		const TArray<TSharedPtr<FJsonValue>>* OuterArray;
		if (JsonValue->TryGetArray(OuterArray) && OuterArray->Num() > 0)
		{
			const TArray<TSharedPtr<FJsonValue>>* MiddleArray;
			if ((*OuterArray)[0]->TryGetArray(MiddleArray) && MiddleArray->Num() > 0)
			{
				FString TranslatedText;
				for (const TSharedPtr<FJsonValue>& Item : *MiddleArray)
				{
					const TArray<TSharedPtr<FJsonValue>>* InnerArray;
					if (Item->TryGetArray(InnerArray) && InnerArray->Num() > 0)
					{
						FString Segment;
						if ((*InnerArray)[0]->TryGetString(Segment))
						{
							TranslatedText += Segment;
						}
					}
				}

				if (!TranslatedText.IsEmpty())
				{
					OnComplete.ExecuteIfBound(TranslatedText);
					return;
				}
			}
		}

		OnError.ExecuteIfBound(TEXT("未找到翻译结果 | No translation result found"), false);
	});

	HttpRequest->ProcessRequest();
}

void FCommentTranslator::TranslateWithMicrosoftFree(const FString& SourceText, const FString& TargetLang, FOnTranslationComplete OnComplete, FOnFreeServiceError OnError)
{
	// 使用 Bing Translator 网页接口，中文必须使用 zh-Hans。长文本按句分段，避免被接口截断。
	FString BingTargetLang = TargetLang;
	if (BingTargetLang == TEXT("zh") || BingTargetLang == TEXT("zh-CN"))
	{
		BingTargetLang = TEXT("zh-Hans");
	}

	TSharedRef<FBingChunkState> State = MakeShared<FBingChunkState>();
	State->Chunks = SplitForBing(SourceText);
	State->TargetLang = BingTargetLang;
	State->OnComplete = OnComplete;
	State->OnError = OnError;
	TranslateNextBingChunk(State);
}

void FCommentTranslator::TranslateWithYoudaoFree(const FString& SourceText, const FString& TargetLang, FOnTranslationComplete OnComplete, FOnFreeServiceError OnError)
{
	// 使用 MyMemory 翻译 API（免费，无需密钥，作为备用）
	// 注意：MyMemory 不支持 auto 源语言检测，需要手动检测
	
	FString SourceLang = DetectSourceLanguage(SourceText);
	if (SourceLang == TEXT("zh"))
	{
		SourceLang = TEXT("zh-CN");
	}
	
	FString EncodedText = LANGUAGEONE_URL_ENCODE(SourceText);
	// 使用 langpair=source|target 格式
	FString Url = FString::Printf(TEXT("https://api.mymemory.translated.net/get?q=%s&langpair=%s|%s"),
		*EncodedText, *SourceLang, *TargetLang);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(Url);
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->SetTimeout(FreeServiceTimeoutSeconds);
	HttpRequest->OnProcessRequestComplete().BindLambda([OnComplete, OnError](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
	{
		const FString HttpError = GetFreeServiceHttpError(TEXT("MyMemory"), Response, bSuccess);
		if (!HttpError.IsEmpty())
		{
			OnError.ExecuteIfBound(HttpError, true);
			return;
		}

		FString ResponseStr = Response->GetContentAsString();
		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseStr);

		if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
		{
			OnError.ExecuteIfBound(TEXT("解析响应失败 | Failed to parse response"), false);
			return;
		}

		// 检查 MyMemory 错误状态
		if (JsonObject->HasField(TEXT("responseStatus")))
		{
			int32 Status = JsonObject->GetIntegerField(TEXT("responseStatus"));
			if (Status != 200)
			{
				FString ErrorMsg = JsonObject->HasField(TEXT("responseDetails")) 
					? JsonObject->GetStringField(TEXT("responseDetails"))
					: TEXT("翻译服务返回错误 | Translation service error");
				OnError.ExecuteIfBound(ErrorMsg, false);
				return;
			}
		}

		// 解析结果 {"responseData":{"translatedText":"..."}}
		TSharedPtr<FJsonObject> ResponseData = JsonObject->GetObjectField(TEXT("responseData"));
		if (ResponseData.IsValid())
		{
			FString TranslatedText = ResponseData->GetStringField(TEXT("translatedText"));
			if (!TranslatedText.IsEmpty())
			{
				OnComplete.ExecuteIfBound(TranslatedText);
				return;
			}
		}

		OnError.ExecuteIfBound(TEXT("未找到翻译结果 | No translation result found"), false);
	});

	HttpRequest->ProcessRequest();
}

void FCommentTranslator::TranslateWithTencentFree(const FString& SourceText, const FString& TargetLang, FOnTranslationComplete OnComplete, FOnFreeServiceError OnError)
{
	const FString SourceLang = DetectSourceLanguage(SourceText);

	FString TencentTargetLang = TargetLang;
	if (TencentTargetLang == TEXT("zh-CN") || TencentTargetLang == TEXT("zh-Hans"))
	{
		TencentTargetLang = TEXT("zh");
	}

	TSharedPtr<FJsonObject> Header = MakeShareable(new FJsonObject);
	Header->SetStringField(TEXT("fn"), TEXT("auto_translation"));
	Header->SetStringField(TEXT("client_key"), FString::Printf(TEXT("browser-chrome-126-Windows-%s"), *FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens)));
	TSharedPtr<FJsonObject> Source = MakeShareable(new FJsonObject);
	Source->SetStringField(TEXT("lang"), SourceLang);
	TArray<TSharedPtr<FJsonValue>> TextList;
	TextList.Add(MakeShareable(new FJsonValueString(SourceText)));
	Source->SetArrayField(TEXT("text_list"), TextList);
	TSharedPtr<FJsonObject> Target = MakeShareable(new FJsonObject);
	Target->SetStringField(TEXT("lang"), TencentTargetLang);

	TSharedPtr<FJsonObject> Body = MakeShareable(new FJsonObject);
	Body->SetObjectField(TEXT("header"), Header);
	Body->SetStringField(TEXT("type"), TEXT("plain"));
	Body->SetStringField(TEXT("model_category"), TEXT("normal"));
	Body->SetObjectField(TEXT("source"), Source);
	Body->SetObjectField(TEXT("target"), Target);

	FString RequestBody;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
	FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(TEXT("https://transmart.qq.com/api/imt"));
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetTimeout(FreeServiceTimeoutSeconds);
	HttpRequest->SetContentAsString(RequestBody);
	HttpRequest->OnProcessRequestComplete().BindLambda([OnComplete, OnError](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
	{
		const FString HttpError = GetFreeServiceHttpError(TEXT("TranSmart"), Response, bSuccess);
		if (!HttpError.IsEmpty())
		{
			OnError.ExecuteIfBound(HttpError, true);
			return;
		}

		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
		if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
		{
			OnError.ExecuteIfBound(TEXT("解析腾讯交互翻译响应失败 | Failed to parse Tencent TranSmart response"), false);
			return;
		}

		FString ResultCode;
		const TSharedPtr<FJsonObject>* HeaderObject = nullptr;
		if (JsonObject->TryGetObjectField(TEXT("header"), HeaderObject))
		{
			(*HeaderObject)->TryGetStringField(TEXT("ret_code"), ResultCode);
		}
		if (ResultCode != TEXT("succ"))
		{
			OnError.ExecuteIfBound(TEXT("腾讯交互翻译没有返回成功结果 | Tencent TranSmart did not return a successful result"), false);
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* Translations = nullptr;
		FString TranslatedText;
		if (JsonObject->TryGetArrayField(TEXT("auto_translation"), Translations) && Translations->Num() > 0 && (*Translations)[0]->TryGetString(TranslatedText) && !TranslatedText.IsEmpty())
		{
			OnComplete.ExecuteIfBound(TranslatedText);
			return;
		}

		OnError.ExecuteIfBound(TEXT("未找到翻译结果 | No translation result found"), false);
	});
	HttpRequest->ProcessRequest();
}

void FCommentTranslator::TranslateWithBaidu(const FString& SourceText, const FString& TargetLang, FOnTranslationComplete OnComplete, FOnTranslationError OnError)
{
	const ULanguageOneSettings* Settings = GetDefault<ULanguageOneSettings>();
	
	if (Settings->BaiduAppId.IsEmpty() || Settings->BaiduSecretKey.IsEmpty())
	{
		OnError.ExecuteIfBound(TEXT("请先在编辑器设置中配置百度翻译 APP ID 和密钥\nPlease configure Baidu Translation APP ID and Secret Key in Editor Settings"));
		return;
	}

	// 百度翻译 API: https://fanyi-api.baidu.com/api/trans/vip/translate
	FString Salt = FString::FromInt(FMath::Rand());
	FString Sign = GenerateMD5(Settings->BaiduAppId + SourceText + Salt + Settings->BaiduSecretKey);
	
	FString EncodedText = LANGUAGEONE_URL_ENCODE(SourceText);
	FString Url = FString::Printf(TEXT("https://fanyi-api.baidu.com/api/trans/vip/translate?q=%s&from=auto&to=%s&appid=%s&salt=%s&sign=%s"),
		*EncodedText, *TargetLang, *Settings->BaiduAppId, *Salt, *Sign);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(Url);
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->OnProcessRequestComplete().BindLambda([OnComplete, OnError](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
	{
		if (!bSuccess || !Response.IsValid())
		{
			OnError.ExecuteIfBound(TEXT("网络请求失败 | Network request failed"));
			return;
		}

		FString ResponseStr = Response->GetContentAsString();
		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseStr);

		if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
		{
			OnError.ExecuteIfBound(TEXT("解析响应失败 | Failed to parse response"));
			return;
		}

		// 检查错误
		if (JsonObject->HasField(TEXT("error_code")))
		{
			FString ErrorMsg = JsonObject->GetStringField(TEXT("error_msg"));
			OnError.ExecuteIfBound(FString::Printf(TEXT("百度翻译错误: %s | Baidu Translation Error: %s"), *ErrorMsg, *ErrorMsg));
			return;
		}

		// 获取翻译结果
		const TArray<TSharedPtr<FJsonValue>>* TransResults;
		if (JsonObject->TryGetArrayField(TEXT("trans_result"), TransResults) && TransResults->Num() > 0)
		{
			TSharedPtr<FJsonObject> FirstResult = (*TransResults)[0]->AsObject();
			if (FirstResult.IsValid())
			{
				FString TranslatedText = FirstResult->GetStringField(TEXT("dst"));
				OnComplete.ExecuteIfBound(TranslatedText);
				return;
			}
		}

		OnError.ExecuteIfBound(TEXT("未找到翻译结果 | No translation result found"));
	});

	HttpRequest->ProcessRequest();
}

void FCommentTranslator::TranslateWithGoogle(const FString& SourceText, const FString& TargetLang, FOnTranslationComplete OnComplete, FOnTranslationError OnError)
{
	const ULanguageOneSettings* Settings = GetDefault<ULanguageOneSettings>();
	
	if (Settings->GoogleApiKey.IsEmpty())
	{
		OnError.ExecuteIfBound(TEXT("请先在编辑器设置中配置 Google API 密钥\nPlease configure Google API Key in Editor Settings"));
		return;
	}

	FString EncodedText = LANGUAGEONE_URL_ENCODE(SourceText);
	FString Url = FString::Printf(TEXT("https://translation.googleapis.com/language/translate/v2?key=%s&q=%s&target=%s"),
		*Settings->GoogleApiKey, *EncodedText, *TargetLang);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(Url);
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->OnProcessRequestComplete().BindLambda([OnComplete, OnError](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
	{
		if (!bSuccess || !Response.IsValid())
		{
			OnError.ExecuteIfBound(TEXT("网络请求失败 | Network request failed"));
			return;
		}

		FString ResponseStr = Response->GetContentAsString();
		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseStr);

		if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
		{
			OnError.ExecuteIfBound(TEXT("解析响应失败 | Failed to parse response"));
			return;
		}

		// 检查错误
		if (JsonObject->HasField(TEXT("error")))
		{
			TSharedPtr<FJsonObject> ErrorObj = JsonObject->GetObjectField(TEXT("error"));
			FString ErrorMsg = ErrorObj->GetStringField(TEXT("message"));
			OnError.ExecuteIfBound(FString::Printf(TEXT("Google 翻译错误: %s | Google Translation Error: %s"), *ErrorMsg, *ErrorMsg));
			return;
		}

		// 获取翻译结果
		TSharedPtr<FJsonObject> DataObj = JsonObject->GetObjectField(TEXT("data"));
		if (DataObj.IsValid())
		{
			const TArray<TSharedPtr<FJsonValue>>* Translations;
			if (DataObj->TryGetArrayField(TEXT("translations"), Translations) && Translations->Num() > 0)
			{
				TSharedPtr<FJsonObject> FirstTranslation = (*Translations)[0]->AsObject();
				if (FirstTranslation.IsValid())
				{
					FString TranslatedText = FirstTranslation->GetStringField(TEXT("translatedText"));
					OnComplete.ExecuteIfBound(TranslatedText);
					return;
				}
			}
		}

		OnError.ExecuteIfBound(TEXT("未找到翻译结果 | No translation result found"));
	});

	HttpRequest->ProcessRequest();
}

void FCommentTranslator::TranslateWithCustom(const FString& SourceText, const FString& TargetLang, FOnTranslationComplete OnComplete, FOnTranslationError OnError)
{
	const ULanguageOneSettings* Settings = GetDefault<ULanguageOneSettings>();
	
	if (Settings->CustomApiUrl.IsEmpty())
	{
		OnError.ExecuteIfBound(TEXT("请先在编辑器设置中配置自定义 API 地址\nPlease configure Custom API URL in Editor Settings"));
		return;
	}

	// 创建 JSON 请求体
	TSharedPtr<FJsonObject> RequestObj = MakeShareable(new FJsonObject);
	RequestObj->SetStringField(TEXT("text"), SourceText);
	RequestObj->SetStringField(TEXT("target_lang"), TargetLang);

	FString RequestBody;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
	FJsonSerializer::Serialize(RequestObj.ToSharedRef(), Writer);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(Settings->CustomApiUrl);
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	
	if (!Settings->CustomApiKey.IsEmpty())
	{
		HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Settings->CustomApiKey));
	}
	
	HttpRequest->SetContentAsString(RequestBody);
	HttpRequest->OnProcessRequestComplete().BindLambda([OnComplete, OnError](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
	{
		if (!bSuccess || !Response.IsValid())
		{
			OnError.ExecuteIfBound(TEXT("网络请求失败 | Network request failed"));
			return;
		}

		FString ResponseStr = Response->GetContentAsString();
		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseStr);

		if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
		{
			OnError.ExecuteIfBound(TEXT("解析响应失败 | Failed to parse response"));
			return;
		}

		// 假设自定义 API 返回格式为 {"translated_text": "..."}
		if (JsonObject->HasField(TEXT("translated_text")))
		{
			FString TranslatedText = JsonObject->GetStringField(TEXT("translated_text"));
			OnComplete.ExecuteIfBound(TranslatedText);
		}
		else
		{
			OnError.ExecuteIfBound(TEXT("未找到翻译结果 | No translation result found"));
		}
	});

	HttpRequest->ProcessRequest();
}

FString FCommentTranslator::GenerateMD5(const FString& Text)
{
	return FMD5::HashAnsiString(*Text);
}

