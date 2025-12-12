// Copyright Epic Games, Inc. All Rights Reserved.

#include "CommentTranslator.h"
#include "LanguageOneSettings.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Misc/SecureHash.h"
#include "GenericPlatform/GenericPlatformHttp.h"

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
	FString EncodedText = FGenericPlatformHttp::UrlEncode(SourceText);
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
	// 使用 MyMemory 翻译 API（免费，无需密钥）
	// 这是一个公开的翻译服务，每天有配额限制
	FString EncodedText = FGenericPlatformHttp::UrlEncode(SourceText);
	FString Url = FString::Printf(TEXT("https://api.mymemory.translated.net/get?q=%s&langpair=auto|%s"),
		*EncodedText, *TargetLang);

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

		// MyMemory API 响应格式：{"responseData":{"translatedText":"..."}}
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

void FCommentTranslator::TranslateWithYoudaoFree(const FString& SourceText, const FString& TargetLang, FOnTranslationComplete OnComplete, FOnTranslationError OnError)
{
	// 使用 LibreTranslate 公开服务（完全免费开源）
	// 使用公共实例：translate.argosopentech.com 或 libretranslate.de
	FString Url = TEXT("https://translate.argosopentech.com/translate");

	// 创建 JSON 请求体
	TSharedPtr<FJsonObject> RequestObj = MakeShareable(new FJsonObject);
	RequestObj->SetStringField(TEXT("q"), SourceText);
	RequestObj->SetStringField(TEXT("source"), TEXT("auto"));
	RequestObj->SetStringField(TEXT("target"), TargetLang);
	RequestObj->SetStringField(TEXT("format"), TEXT("text"));

	FString RequestBody;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
	FJsonSerializer::Serialize(RequestObj.ToSharedRef(), Writer);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(Url);
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
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

		// LibreTranslate API 响应格式：{"translatedText":"..."}
		if (JsonObject->HasField(TEXT("translatedText")))
		{
			FString TranslatedText = JsonObject->GetStringField(TEXT("translatedText"));
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
	
	FString EncodedText = FGenericPlatformHttp::UrlEncode(SourceText);
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

	FString EncodedText = FGenericPlatformHttp::UrlEncode(SourceText);
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

