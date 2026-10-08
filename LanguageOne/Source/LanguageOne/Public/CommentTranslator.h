// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LanguageOneSettings.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

DECLARE_DELEGATE_OneParam(FOnTranslationComplete, const FString&);
DECLARE_DELEGATE_OneParam(FOnTranslationError, const FString&);

/**
 * 注释翻译器类
 */
class LANGUAGEONE_API FCommentTranslator
{
public:
	/** 翻译文本 */
	static void TranslateText(const FString& SourceText, FOnTranslationComplete OnComplete, FOnTranslationError OnError);

private:
	/** 使用 Google 翻译免费接口 */
	static void TranslateWithGoogleFree(const FString& SourceText, const FString& TargetLang, FOnTranslationComplete OnComplete, FOnTranslationError OnError);
	
	/** 使用微软 Bing 网页翻译 */
	static void TranslateWithMicrosoftFree(const FString& SourceText, const FString& TargetLang, FOnTranslationComplete OnComplete, FOnTranslationError OnError);

	/** 使用腾讯交互翻译 */
	static void TranslateWithTencentFree(const FString& SourceText, const FString& TargetLang, FOnTranslationComplete OnComplete, FOnTranslationError OnError);
	
	/** 使用 MyMemory 翻译 */
	static void TranslateWithYoudaoFree(const FString& SourceText, const FString& TargetLang, FOnTranslationComplete OnComplete, FOnTranslationError OnError);

	/** 使用百度翻译 API */
	static void TranslateWithBaidu(const FString& SourceText, const FString& TargetLang, FOnTranslationComplete OnComplete, FOnTranslationError OnError);
	
	/** 使用 Google 翻译 API */
	static void TranslateWithGoogle(const FString& SourceText, const FString& TargetLang, FOnTranslationComplete OnComplete, FOnTranslationError OnError);
	
	/** 使用自定义翻译 API */
	static void TranslateWithCustom(const FString& SourceText, const FString& TargetLang, FOnTranslationComplete OnComplete, FOnTranslationError OnError);
	
	/** 按服务分发；语言代码由该服务决定，不读当前设置里的服务项 */
	static void DispatchTranslation(ETranslateProvider Provider, const FString& SourceText, FOnTranslationComplete OnComplete, FOnTranslationError OnError);

	/** 获取语言代码 */
	static FString GetLanguageCode(bool bIsBaidu = true);
	
	/** 生成 MD5 签名（用于百度翻译） */
	static FString GenerateMD5(const FString& Text);

	/** HTTP 请求回调 */
	static void OnHttpRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess, FOnTranslationComplete OnComplete, FOnTranslationError OnError);
};

