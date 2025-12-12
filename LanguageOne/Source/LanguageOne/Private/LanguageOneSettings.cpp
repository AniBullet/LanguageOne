// Copyright Epic Games, Inc. All Rights Reserved.

#include "LanguageOneSettings.h"

ULanguageOneSettings::ULanguageOneSettings()
	: TargetEditorLanguage(EEditorLanguage::English)  // 默认目标：英文
	, CurrentEditorLanguage(TEXT(""))
	, PreviousLanguage(TEXT("zh-CN"))  // 默认上一次：中文
	, TranslateProvider(ETranslateProvider::GoogleFree)
	, BaiduAppId(TEXT(""))
	, BaiduSecretKey(TEXT(""))
	, GoogleApiKey(TEXT(""))
	, CustomApiUrl(TEXT(""))
	, CustomApiKey(TEXT(""))
	, TargetLanguage(ETranslateTargetLanguage::Chinese)
	, bKeepOriginalText(true)
	, bTranslationAboveOriginal(false)
{
}

