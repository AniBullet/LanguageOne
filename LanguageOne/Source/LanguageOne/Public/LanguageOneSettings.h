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
	GoogleFree UMETA(DisplayName = "谷歌翻译(Web版) | Google Translate (Web)"),
	MicrosoftFree UMETA(DisplayName = "微软Edge翻译(推荐) | Microsoft Edge (Recommended)"),
	YoudaoFree UMETA(DisplayName = "MyMemory翻译(备用) | MyMemory (Backup)"),
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
	/** 语言 A (默认语言) */
	UPROPERTY(Config, EditAnywhere, Category = "语言切换 | Language Switch", meta = (DisplayName = "语言 A | Language A", Tooltip = "双语切换的第一种语言（通常为英文） | First language for toggling (usually English)"))
	EEditorLanguage SourceEditorLanguage;

	/** 语言 B (目标语言) */
	UPROPERTY(Config, EditAnywhere, Category = "语言切换 | Language Switch", meta = (DisplayName = "语言 B | Language B", Tooltip = "双语切换的第二种语言（通常为中文） | Second language for toggling (usually Chinese)"))
	EEditorLanguage TargetEditorLanguage;

	/** 当前编辑器语言（只读显示） */
	UPROPERTY(VisibleAnywhere, Category = "语言切换 | Language Switch", meta = (DisplayName = "当前编辑器语言 | Current Editor Language"))
	FString CurrentEditorLanguage;

	// ========== 翻译设置 ==========
	/** 翻译服务 */
	UPROPERTY(Config, EditAnywhere, Category = "翻译设置 | Translation Settings", meta = (DisplayName = "翻译服务 | Translation Service", Tooltip = "免费服务开箱即用 | Free services work out of the box"))
	ETranslateProvider TranslateProvider;

	/** 百度 APP ID */
	UPROPERTY(Config, EditAnywhere, Category = "翻译设置 | Translation Settings", meta = (DisplayName = "百度 APP ID", EditCondition = "TranslateProvider == ETranslateProvider::Baidu"))
	FString BaiduAppId;

	/** 百度密钥 */
	UPROPERTY(Config, EditAnywhere, Category = "翻译设置 | Translation Settings", meta = (DisplayName = "百度密钥 | Baidu Key", EditCondition = "TranslateProvider == ETranslateProvider::Baidu", PasswordField = true))
	FString BaiduSecretKey;

	/** Google API 密钥 */
	UPROPERTY(Config, EditAnywhere, Category = "翻译设置 | Translation Settings", meta = (DisplayName = "Google API Key", EditCondition = "TranslateProvider == ETranslateProvider::Google", PasswordField = true))
	FString GoogleApiKey;

	/** 自定义 API 地址 */
	UPROPERTY(Config, EditAnywhere, Category = "翻译设置 | Translation Settings", meta = (DisplayName = "API 地址 | API URL", EditCondition = "TranslateProvider == ETranslateProvider::Custom"))
	FString CustomApiUrl;

	/** 自定义 API 密钥 */
	UPROPERTY(Config, EditAnywhere, Category = "翻译设置 | Translation Settings", meta = (DisplayName = "API 密钥 | API Key", EditCondition = "TranslateProvider == ETranslateProvider::Custom", PasswordField = true))
	FString CustomApiKey;

	/** 翻译目标语言 */
	UPROPERTY(Config, EditAnywhere, Category = "翻译设置 | Translation Settings", meta = (DisplayName = "翻译成 | Translate To"))
	ETranslateTargetLanguage TargetLanguage;

	/** 译文显示在原文上方 */
	UPROPERTY(Config, EditAnywhere, Category = "翻译设置 | Translation Settings", meta = (DisplayName = "译文在上方 | Translation Above", Tooltip = "勾选=译文在上，原文在下；不勾选=原文在上，译文在下 | Checked=Translation above, Original below"))
	bool bTranslationAboveOriginal;

	// ========== 翻译行为设置 ==========
	/** 翻译前显示确认对话框 */
	UPROPERTY(Config, EditAnywhere, Category = "翻译设置 | Translation Settings", meta = (DisplayName = "翻译前确认 | Confirm Before Translation", Tooltip = "批量翻译资产前显示确认对话框 | Show confirmation dialog before batch translation"))
	bool bConfirmBeforeAssetTranslation;

	/** 显示详细翻译日志 */
	UPROPERTY(Config, EditAnywhere, Category = "翻译设置 | Translation Settings", meta = (DisplayName = "详细日志 | Verbose Logging", Tooltip = "在输出日志显示详细的翻译信息 | Show detailed translation info in output log"))
	bool bVerboseAssetTranslationLog;
};

