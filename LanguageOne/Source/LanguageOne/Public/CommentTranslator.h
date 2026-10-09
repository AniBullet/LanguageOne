// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LanguageOneSettings.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

DECLARE_DELEGATE_OneParam(FOnTranslationComplete, const FString&);
DECLARE_DELEGATE_OneParam(FOnTranslationError, const FString&);
/** 免费服务的失败回调；bServiceUnavailable 表示连不上（网络失败、超时、非 2xx），用于熔断 */
DECLARE_DELEGATE_TwoParams(FOnFreeServiceError, const FString& /*ErrorMessage*/, bool /*bServiceUnavailable*/);
/** 自动切换到另一个免费服务时广播，参数为服务名 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnFreeServiceSwitched, const FString&);

/**
 * 注释翻译器类
 */
class LANGUAGEONE_API FCommentTranslator
{
public:
	/** 翻译文本 */
	static void TranslateText(const FString& SourceText, FOnTranslationComplete OnComplete, FOnTranslationError OnError);

	/** 自动切换免费服务的事件，由界面层订阅后提示用户 */
	static FOnFreeServiceSwitched& OnFreeServiceSwitched();

private:
	/** 使用 Google 翻译免费接口 */
	static void TranslateWithGoogleFree(const FString& SourceText, const FString& TargetLang, FOnTranslationComplete OnComplete, FOnFreeServiceError OnError);
	
	/** 使用微软 Bing 网页翻译 */
	static void TranslateWithMicrosoftFree(const FString& SourceText, const FString& TargetLang, FOnTranslationComplete OnComplete, FOnFreeServiceError OnError);

	/** 使用腾讯交互翻译 */
	static void TranslateWithTencentFree(const FString& SourceText, const FString& TargetLang, FOnTranslationComplete OnComplete, FOnFreeServiceError OnError);
	
	/** 使用 MyMemory 翻译 */
	static void TranslateWithYoudaoFree(const FString& SourceText, const FString& TargetLang, FOnTranslationComplete OnComplete, FOnFreeServiceError OnError);

	/** 使用百度翻译 API */
	static void TranslateWithBaidu(const FString& SourceText, const FString& TargetLang, FOnTranslationComplete OnComplete, FOnTranslationError OnError);
	
	/** 使用 Google 翻译 API */
	static void TranslateWithGoogle(const FString& SourceText, const FString& TargetLang, FOnTranslationComplete OnComplete, FOnTranslationError OnError);
	
	/** 使用自定义翻译 API */
	static void TranslateWithCustom(const FString& SourceText, const FString& TargetLang, FOnTranslationComplete OnComplete, FOnTranslationError OnError);
	
	/** 按服务分发；语言代码由该服务决定，不读当前设置里的服务项 */
	static void DispatchTranslation(ETranslateProvider Provider, const FString& SourceText, FOnTranslationComplete OnComplete, FOnFreeServiceError OnError);

	/** 免费服务 fallback 链的进度 */
	struct FFreeFallbackState;

	/** 从当前位置继续尝试链上的下一个免费服务 */
	static void TryNextFreeService(const TSharedRef<FFreeFallbackState>& State);
	
	/** 生成 MD5 签名（用于百度翻译） */
	static FString GenerateMD5(const FString& Text);

	/** HTTP 请求回调 */
	static void OnHttpRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess, FOnTranslationComplete OnComplete, FOnTranslationError OnError);
};

