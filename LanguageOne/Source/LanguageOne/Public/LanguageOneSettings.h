// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "LanguageOneSettings.generated.h"

UENUM(BlueprintType)
enum class ETranslateTargetLanguage : uint8
{
	Chinese UMETA(DisplayName = "中文"),
	English UMETA(DisplayName = "English"),
	Japanese UMETA(DisplayName = "日本語"),
	Korean UMETA(DisplayName = "한국어"),
	German UMETA(DisplayName = "Deutsch"),
	French UMETA(DisplayName = "Français"),
	Spanish UMETA(DisplayName = "Español"),
	Russian UMETA(DisplayName = "Русский")
};

UENUM(BlueprintType)
enum class ETranslateProvider : uint8
{
	GoogleFree UMETA(DisplayName = "谷歌翻译(免费) | Google Translate (Free)"),
	MicrosoftFree UMETA(DisplayName = "微软翻译(免费) | Microsoft Translator (Free)"),
	YoudaoFree UMETA(DisplayName = "有道翻译(免费) | Youdao Translate (Free)"),
	Baidu UMETA(DisplayName = "百度翻译(需API) | Baidu (API Required)"),
	Google UMETA(DisplayName = "Google翻译(需API) | Google (API Required)"),
	Custom UMETA(DisplayName = "自定义API | Custom API")
};

UENUM(BlueprintType)
enum class EEditorLanguage : uint8
{
	English UMETA(DisplayName = "English"),
	ChineseSimplified UMETA(DisplayName = "中文(简体) | Chinese Simplified"),
	ChineseTraditional UMETA(DisplayName = "中文(繁體) | Chinese Traditional"),
	Japanese UMETA(DisplayName = "日本語 | Japanese"),
	Korean UMETA(DisplayName = "한국어 | Korean"),
	German UMETA(DisplayName = "Deutsch | German"),
	French UMETA(DisplayName = "Français | French"),
	Spanish UMETA(DisplayName = "Español | Spanish"),
	Russian UMETA(DisplayName = "Русский | Russian"),
	Portuguese UMETA(DisplayName = "Português | Portuguese"),
	Italian UMETA(DisplayName = "Italiano | Italian")
};

/**
 * LanguageOne 插件设置
 */
UCLASS(config = EditorPerProjectUserSettings, defaultconfig)
class LANGUAGEONE_API ULanguageOneSettings : public UObject
{
	GENERATED_BODY()

public:
	ULanguageOneSettings();

	// ========== 语言切换 ==========
	/** 目标编辑器语言 */
	UPROPERTY(Config, EditAnywhere, Category = "语言切换 | Language Switch", meta = (DisplayName = "目标语言 | Target Language", Tooltip = "按 Alt+Q 在当前语言和目标语言之间切换 | Press Alt+Q to toggle between current and target language"))
	EEditorLanguage TargetEditorLanguage;

	/** 当前编辑器语言 */
	UPROPERTY(VisibleAnywhere, Category = "语言切换 | Language Switch", meta = (DisplayName = "当前语言 | Current Language"))
	FString CurrentEditorLanguage;

	/** 上一次的语言（用于来回切换） */
	UPROPERTY(Config)
	FString PreviousLanguage;

	// ========== 注释翻译 ==========
	/** 翻译服务 */
	UPROPERTY(Config, EditAnywhere, Category = "注释翻译 | Comment Translation", meta = (DisplayName = "翻译服务 | Translation Service", Tooltip = "免费服务开箱即用 | Free services work out of the box"))
	ETranslateProvider TranslateProvider;

	/** 百度 APP ID */
	UPROPERTY(Config, EditAnywhere, Category = "注释翻译 | Comment Translation", meta = (DisplayName = "百度 APP ID", EditCondition = "TranslateProvider == ETranslateProvider::Baidu"))
	FString BaiduAppId;

	/** 百度密钥 */
	UPROPERTY(Config, EditAnywhere, Category = "注释翻译 | Comment Translation", meta = (DisplayName = "百度密钥 | Baidu Key", EditCondition = "TranslateProvider == ETranslateProvider::Baidu", PasswordField = true))
	FString BaiduSecretKey;

	/** Google API 密钥 */
	UPROPERTY(Config, EditAnywhere, Category = "注释翻译 | Comment Translation", meta = (DisplayName = "Google API Key", EditCondition = "TranslateProvider == ETranslateProvider::Google", PasswordField = true))
	FString GoogleApiKey;

	/** 自定义 API 地址 */
	UPROPERTY(Config, EditAnywhere, Category = "注释翻译 | Comment Translation", meta = (DisplayName = "API 地址 | API URL", EditCondition = "TranslateProvider == ETranslateProvider::Custom"))
	FString CustomApiUrl;

	/** 自定义 API 密钥 */
	UPROPERTY(Config, EditAnywhere, Category = "注释翻译 | Comment Translation", meta = (DisplayName = "API 密钥 | API Key", EditCondition = "TranslateProvider == ETranslateProvider::Custom", PasswordField = true))
	FString CustomApiKey;

	/** 翻译目标语言 */
	UPROPERTY(Config, EditAnywhere, Category = "注释翻译 | Comment Translation", meta = (DisplayName = "翻译成 | Translate To"))
	ETranslateTargetLanguage TargetLanguage;

	/** 保留原文 */
	UPROPERTY(Config, EditAnywhere, Category = "注释翻译 | Comment Translation", meta = (DisplayName = "保留原文 | Keep Original"))
	bool bKeepOriginalText;

	/** 译文显示在原文上方 */
	UPROPERTY(Config, EditAnywhere, Category = "注释翻译 | Comment Translation", meta = (DisplayName = "译文在上方 | Translation Above", EditCondition = "bKeepOriginalText", Tooltip = "勾选=译文在上，原文在下；不勾选=原文在上，译文在下 | Checked=Translation above, Original below"))
	bool bTranslationAboveOriginal;
};

