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

	FString TargetLang = GetLanguageCode();

	switch (Settings->TranslateProvider)
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
	case ETranslateProvider::Baidu:
		TranslateWithBaidu(SourceText, TargetLang, OnComplete, OnError);
		break;
	case ETranslateProvider::Google:
		TranslateWithGoogle(SourceText, TargetLang, OnComplete, OnError);
		break;
	case ETranslateProvider::Custom:
		TranslateWithCustom(SourceText, TargetLang, OnComplete, OnError);
		break;
	default:
		OnError.ExecuteIfBound(TEXT("未知的翻译服务商 | Unknown translation provider"));
		break;
	}
}

void FCommentTranslator::TranslateWithGoogleFree(const FString& SourceText, const FString& TargetLang, FOnTranslationComplete OnComplete, FOnTranslationError OnError)
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
	HttpRequest->OnProcessRequestComplete().BindLambda([OnComplete, OnError](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
	{
		if (!bSuccess || !Response.IsValid())
		{
			OnError.ExecuteIfBound(TEXT("网络请求失败 | Network request failed"));
			return;
		}

		FString ResponseStr = Response->GetContentAsString();
		
		// Google 免费接口返回的是一个数组格式：[[["翻译结果","原文"]]]
		TSharedPtr<FJsonValue> JsonValue;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseStr);

		if (!FJsonSerializer::Deserialize(Reader, JsonValue) || !JsonValue.IsValid())
		{
			OnError.ExecuteIfBound(TEXT("解析响应失败 | Failed to parse response"));
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

		OnError.ExecuteIfBound(TEXT("未找到翻译结果 | No translation result found"));
	});

	HttpRequest->ProcessRequest();
}

void FCommentTranslator::TranslateWithMicrosoftFree(const FString& SourceText, const FString& TargetLang, FOnTranslationComplete OnComplete, FOnTranslationError OnError)
{
	// 使用 Microsoft Edge 浏览器内置翻译接口（无需 Key，免费且稳定）
	// 这是目前最推荐的免费方案，支持多语言，速度快
	
	// 第一步：获取 Authorization Token
	// 这个 Token 是动态的，必须每次（或定期）获取
	FString AuthUrl = TEXT("https://edge.microsoft.com/translate/auth");
	
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> AuthRequest = FHttpModule::Get().CreateRequest();
	AuthRequest->SetURL(AuthUrl);
	AuthRequest->SetVerb(TEXT("GET"));
	AuthRequest->SetHeader(TEXT("User-Agent"), TEXT("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36 Edg/120.0.0.0"));
	
	AuthRequest->OnProcessRequestComplete().BindLambda([SourceText, TargetLang, OnComplete, OnError](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
	{
		if (!bSuccess || !Response.IsValid())
		{
			OnError.ExecuteIfBound(TEXT("获取微软翻译授权失败，请检查网络 | Failed to get Microsoft auth"));
			return;
		}
		
		FString Token = Response->GetContentAsString();
		if (Token.IsEmpty())
		{
			OnError.ExecuteIfBound(TEXT("获取到的微软授权Token为空 | Microsoft auth token is empty"));
			return;
		}
		
		// 第二步：使用 Token 调用翻译接口
		// Edge API 需要特定的语言代码格式 (例如中文必须是 zh-Hans)
		FString EdgeTargetLang = TargetLang;
		if (EdgeTargetLang == TEXT("zh") || EdgeTargetLang == TEXT("zh-CN")) EdgeTargetLang = TEXT("zh-Hans");
		
		// API URL
		FString TranslateUrl = FString::Printf(TEXT("https://api-edge.cognitive.microsofttranslator.com/translate?from=&to=%s&api-version=3.0&includeSentenceLength=true"), *EdgeTargetLang);
		
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> TransRequest = FHttpModule::Get().CreateRequest();
		TransRequest->SetURL(TranslateUrl);
		TransRequest->SetVerb(TEXT("POST"));
		
		// 必须带上 Bearer Token
		TransRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Token));
		TransRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
		TransRequest->SetHeader(TEXT("User-Agent"), TEXT("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36 Edg/120.0.0.0"));
		
		// 构造请求体 [{"Text": "..."}]
		TSharedPtr<FJsonObject> TextObj = MakeShareable(new FJsonObject);
		TextObj->SetStringField(TEXT("Text"), SourceText);
		TArray<TSharedPtr<FJsonValue>> RequestArray;
		RequestArray.Add(MakeShareable(new FJsonValueObject(TextObj)));
		
		FString RequestBody;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
		FJsonSerializer::Serialize(RequestArray, Writer);
		
		TransRequest->SetContentAsString(RequestBody);
		
		TransRequest->OnProcessRequestComplete().BindLambda([OnComplete, OnError](FHttpRequestPtr TransReq, FHttpResponsePtr TransResp, bool bTransSuccess)
		{
			if (!bTransSuccess || !TransResp.IsValid())
			{
				OnError.ExecuteIfBound(TEXT("微软翻译请求失败 | Microsoft Translation request failed"));
				return;
			}
			
			FString TransRespStr = TransResp->GetContentAsString();
			TArray<TSharedPtr<FJsonValue>> JsonArray;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(TransRespStr);
			
			if (!FJsonSerializer::Deserialize(Reader, JsonArray) || JsonArray.Num() == 0)
			{
				OnError.ExecuteIfBound(TEXT("解析微软翻译响应失败 | Failed to parse Microsoft response"));
				return;
			}
			
			// 响应格式: [{"translations":[{"text":"..."}]}]
			TSharedPtr<FJsonObject> FirstItem = JsonArray[0]->AsObject();
			if (FirstItem.IsValid())
			{
				const TArray<TSharedPtr<FJsonValue>>* Translations;
				if (FirstItem->TryGetArrayField(TEXT("translations"), Translations) && Translations->Num() > 0)
				{
					TSharedPtr<FJsonObject> FirstTrans = (*Translations)[0]->AsObject();
					if (FirstTrans.IsValid())
					{
						FString TranslatedText = FirstTrans->GetStringField(TEXT("text"));
						if (!TranslatedText.IsEmpty())
						{
							OnComplete.ExecuteIfBound(TranslatedText);
							return;
						}
					}
				}
			}
			
			OnError.ExecuteIfBound(TEXT("未找到翻译结果 | No translation result found"));
		});
		
		TransRequest->ProcessRequest();
	});
	
	AuthRequest->ProcessRequest();
}

void FCommentTranslator::TranslateWithYoudaoFree(const FString& SourceText, const FString& TargetLang, FOnTranslationComplete OnComplete, FOnTranslationError OnError)
{
	// 使用 MyMemory 翻译 API（免费，无需密钥，作为备用）
	// 注意：MyMemory 不支持 auto 源语言检测，需要手动检测
	
	// 简单的语言检测逻辑
	FString SourceLang = TEXT("en");  // 默认英文
	
	for (int32 i = 0; i < SourceText.Len(); i++)
	{
		TCHAR Char = SourceText[i];
		// 中文字符 (CJK统一汉字: 0x4E00 - 0x9FFF)
		if (Char >= 0x4E00 && Char <= 0x9FFF)
		{
			SourceLang = TEXT("zh-CN");
			break;
		}
		// 日文 (平假名/片假名)
		else if ((Char >= 0x3040 && Char <= 0x309F) || (Char >= 0x30A0 && Char <= 0x30FF))
		{
			SourceLang = TEXT("ja");
			break;
		}
		// 韩文
		else if (Char >= 0xAC00 && Char <= 0xD7AF)
		{
			SourceLang = TEXT("ko");
			break;
		}
		// 俄文
		else if (Char >= 0x0400 && Char <= 0x04FF)
		{
			SourceLang = TEXT("ru");
			break;
		}
	}
	
	FString EncodedText = LANGUAGEONE_URL_ENCODE(SourceText);
	// 使用 langpair=source|target 格式
	FString Url = FString::Printf(TEXT("https://api.mymemory.translated.net/get?q=%s&langpair=%s|%s"),
		*EncodedText, *SourceLang, *TargetLang);

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

		// 检查 MyMemory 错误状态
		if (JsonObject->HasField(TEXT("responseStatus")))
		{
			int32 Status = JsonObject->GetIntegerField(TEXT("responseStatus"));
			if (Status != 200)
			{
				FString ErrorMsg = JsonObject->HasField(TEXT("responseDetails")) 
					? JsonObject->GetStringField(TEXT("responseDetails"))
					: TEXT("翻译服务返回错误 | Translation service error");
				OnError.ExecuteIfBound(ErrorMsg);
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

		OnError.ExecuteIfBound(TEXT("未找到翻译结果 | No translation result found"));
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

FString FCommentTranslator::GetLanguageCode(bool bIsBaidu)
{
	const ULanguageOneSettings* Settings = GetDefault<ULanguageOneSettings>();
	
	// 根据不同的翻译服务返回对应的语言代码
	switch (Settings->TranslateProvider)
	{
	case ETranslateProvider::GoogleFree:
	case ETranslateProvider::Google:
		// Google 翻译语言代码
		switch (Settings->TargetLanguage)
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
		
	case ETranslateProvider::MicrosoftFree:
	case ETranslateProvider::YoudaoFree:
		// 通用语言代码（ISO 639-1）
		switch (Settings->TargetLanguage)
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
		
	case ETranslateProvider::Baidu:
	default:
		// 百度翻译语言代码
		switch (Settings->TargetLanguage)
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
	}
}

FString FCommentTranslator::GenerateMD5(const FString& Text)
{
	return FMD5::HashAnsiString(*Text);
}

