// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

/**
 * 翻译进度窗口 - 显示美观的翻译进度
 */
class LANGUAGEONE_API STranslationProgressWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STranslationProgressWindow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, int32 InTotalItems);
	
	/** 更新进度 */
	void UpdateProgress(int32 CurrentItem, const FString& InCurrentItemName);
	
	/** 标记完成 */
	void MarkComplete(int32 SuccessCount, int32 FailedCount);
	
	/** 标记失败 */
	void MarkFailed(const FString& ErrorMessage);
	
	/** 重置状态 */
	void Reset(int32 InTotalItems);
	
	/** 增加成功计数 */
	void IncrementSuccess();
	
	/** 增加失败计数 */
	void IncrementFailed();
	
	/** 增加已翻译计数 */
	void IncrementTranslated();
	
	/** 设置不支持的资产数量 */
	void SetUnsupportedCount(int32 Count);

private:
	/** 获取进度百分比 */
	TOptional<float> GetProgressPercent() const;
	
	/** 获取进度百分比文本 */
	FText GetProgressPercentText() const;
	
	/** 获取进度文本 */
	FText GetProgressText() const;
	
	/** 获取状态文本 */
	FText GetStatusText() const;
	
	/** 获取进度条颜色 */
	FSlateColor GetProgressColor() const;
	
	/** 获取成功数量文本 */
	FText GetSuccessCountText() const;
	
	/** 获取失败数量文本 */
	FText GetFailedCountText() const;
	
	/** 获取不支持数量文本 */
	FText GetUnsupportedCountText() const;
	
	/** 获取已翻译（切换显示）数量文本 */
	FText GetTranslatedCountText() const;

private:
	int32 TotalItems;
	int32 CurrentItems;
	int32 SuccessItems;
	int32 FailedItems;
	int32 UnsupportedItems;
	int32 TranslatedItems;  // 已翻译的资产（切换显示）
	FString CurrentItemName;
	bool bIsComplete;
	bool bIsFailed;
	FString ErrorMessage;
};

/**
 * 资产翻译 UI 工具类
 */
class LANGUAGEONE_API FAssetTranslatorUI
{
public:
	/** 显示翻译确认对话框 */
	static void ShowTranslationConfirmDialog(
		const TArray<FAssetData>& SelectedAssets,
		TFunction<void()> OnConfirm,
		TFunction<void()> OnCancel
	);
	
	/** 显示清除原文确认对话框 */
	static void ShowClearOriginalConfirmDialog(
		const TArray<FAssetData>& SelectedAssets,
		TFunction<void()> OnConfirm,
		TFunction<void()> OnCancel
	);
	
	/** 显示翻译进度窗口（独立窗口，已弃用） */
	static TSharedPtr<STranslationProgressWindow> ShowTranslationProgress(int32 TotalItems);
	
	/** 关闭进度窗口（独立窗口，已弃用） */
	static void CloseTranslationProgress();
	
	/** 显示翻译完成通知 */
	static void ShowTranslationCompleteNotification(int32 SuccessCount, int32 TotalCount);
	
	/** 显示错误通知 */
	static void ShowErrorNotification(const FString& Message);
	
	/** 显示信息通知 */
	static void ShowInfoNotification(const FString& Message);
	
	/** 显示资产翻译工具窗口（集成进度条） */
	static void ShowAssetTranslationTool(const TArray<FAssetData>& SelectedAssets);
	
	/** 获取当前工具窗口的进度组件 */
	static TSharedPtr<STranslationProgressWindow> GetToolWindowProgress();

private:
	static TSharedPtr<SWindow> ProgressWindow;  // 独立进度窗口（已弃用）
	static TSharedPtr<STranslationProgressWindow> ProgressWidget;  // 独立进度组件（已弃用）
	static TSharedPtr<SWindow> ToolWindow;  // 工具窗口
	static TSharedPtr<STranslationProgressWindow> ToolProgressWidget;  // 工具窗口内的进度组件
};
