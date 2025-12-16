// Copyright Epic Games, Inc. All Rights Reserved.

#include "LanguageOneSettings.h"

ULanguageOneSettings::ULanguageOneSettings()
	: TargetEditorLanguage(EEditorLanguage::English)  // 默认目标：英文
	, CurrentEditorLanguage(TEXT(""))
	, PreviousLanguage(TEXT("zh-CN"))  // 默认上一次：中文
	, TranslateProvider(ETranslateProvider::MicrosoftFree)  // 默认使用微软Edge翻译（免费且稳定）
	, BaiduAppId(TEXT(""))
	, BaiduSecretKey(TEXT(""))
	, GoogleApiKey(TEXT(""))
	, CustomApiUrl(TEXT(""))
	, CustomApiKey(TEXT(""))
	, TargetLanguage(ETranslateTargetLanguage::Chinese)
	, bTranslationAboveOriginal(false)  // 默认译文在下方（原文在上方）
	, bConfirmBeforeAssetTranslation(false)  // 默认不需要确认
	, bAutoSaveTranslatedAssets(false)  // 默认不自动保存（让用户自己保存）
	, bVerboseAssetTranslationLog(false)  // 默认不显示详细日志
{
}

