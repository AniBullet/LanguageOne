/*
 * @Description: 
 * @Author: Bullet.S
 * @Date: 2025-12-12 16:28:22
 * @LastEditors: Bullet.S
 * @LastEditTime: 2025-12-17 11:59:15
 * @Email: animator.bullet@foxmail.com
 */
// Copyright Epic Games, Inc. All Rights Reserved.

#include "LanguageOneSettings.h"

ULanguageOneSettings::ULanguageOneSettings()
	: SourceEditorLanguage(EEditorLanguage::English) // 默认语言A：英文
	, TargetEditorLanguage(EEditorLanguage::ChineseSimplified) // 默认语言B：中文
	, CurrentEditorLanguage(TEXT(""))
	, TranslateProvider(ETranslateProvider::MicrosoftFree)  // 默认使用微软Edge翻译（免费且稳定）
	, BaiduAppId(TEXT(""))
	, BaiduSecretKey(TEXT(""))
	, GoogleApiKey(TEXT(""))
	, CustomApiUrl(TEXT(""))
	, CustomApiKey(TEXT(""))
	, TargetLanguage(ETranslateTargetLanguage::Chinese)
	, bTranslationAboveOriginal(false)  // 默认译文在下方（原文在上方）
	, bConfirmBeforeAssetTranslation(false)  // 默认不需要确认
	, bVerboseAssetTranslationLog(false)  // 默认不显示详细日志
{
}

