// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommentTranslator.h"

/**
 * 资产翻译器类 - 支持翻译各种 UE 资产中的文本内容
 * 
 * 支持的资产类型:
 * - String Table: 本地化字符串表
 * - Data Table: 数据表中的文本字段
 * - Widget Blueprint: UI 控件的文本属性
 * - Blueprint: 变量描述、函数注释等
 * - Material/Texture: 资产描述信息
 */
class LANGUAGEONE_API FAssetTranslator
{
public:
	/** 翻译选中的资产 */
	static void TranslateSelectedAssets(const TArray<FAssetData>& SelectedAssets, bool bIsFromEditor = false);
	
	/** 还原选中的资产（移除翻译，恢复原文） */
	static void RestoreSelectedAssets(const TArray<FAssetData>& SelectedAssets);
	
	/** 清除选中资产中的原文（只保留译文，不可还原） */
	static void ClearOriginalText(const TArray<FAssetData>& SelectedAssets);
	
	/** 切换显示模式（双语/原文之间切换） */
	static void ToggleDisplayMode(const TArray<FAssetData>& SelectedAssets);
	
	/** 检查资产是否支持翻译 */
	static bool CanTranslateAsset(const FAssetData& AssetData);
	
	/** 检查资产是否有翻译内容 */
	static bool HasAssetTranslation(const FAssetData& AssetData);
	
	/** 获取资产可翻译文本数量 */
	static int32 GetTranslatableTextCount(const FAssetData& AssetData);
	
	/** 执行翻译逻辑（公开给工具窗口使用） */
	static void PerformTranslation(const TArray<FAssetData>& TranslatableAssets, bool bSilent = false);
	
	/** 执行还原逻辑（公开给工具窗口使用） */
	static void PerformRestore(const TArray<FAssetData>& TranslatableAssets);

private:
	/** String Table 翻译 */
	static void TranslateStringTable(UObject* Asset, bool bSilent = false);
	
	/** String Table 翻译（支持选择特定条目） */
	static void TranslateStringTableEntries(UObject* Asset, const TArray<FString>& SelectedKeys, bool bSilent = false);
	
	/** Data Table 翻译 */
	static void TranslateDataTable(UObject* Asset, bool bSilent = false);
	
	/** Widget Blueprint 翻译 */
	static void TranslateWidgetBlueprint(UObject* Asset, bool bSilent = false);
	
	/** Blueprint 翻译 (变量描述、注释等) */
	static void TranslateBlueprint(UObject* Asset, bool bSilent = false);
	
	/** 通用资产元数据翻译 (描述、工具提示等) */
	static void TranslateAssetMetadata(UObject* Asset, bool bSilent = false);
	
	/** 执行清除原文逻辑 */
	static void PerformClearOriginal(const TArray<FAssetData>& TranslatableAssets);
	
	/** 执行切换显示模式逻辑 */
	static void PerformToggleDisplayMode(const TArray<FAssetData>& TranslatableAssets);

	/** 辅助函数: 翻译单个文本并应用格式 */
	static void TranslateSingleText(const FString& SourceText, TFunction<void(const FString&)> OnSuccess, TFunction<void()> OnError);
	
	/** 显示翻译进度通知 */
	static void ShowTranslationProgress(int32 Current, int32 Total, const FString& AssetName);
	
	/** 显示翻译完成通知 */
	static void ShowTranslationComplete(int32 SuccessCount, int32 TotalCount);
};
