// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTranslatorUI.h"
#include "AssetTranslator.h"
#include "LanguageOneCompatibility.h"
#include "LanguageOneSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Styling/AppStyle.h"
#include "Misc/MessageDialog.h"

// 静态成员初始化
TSharedPtr<SWindow> FAssetTranslatorUI::ProgressWindow = nullptr;
TSharedPtr<STranslationProgressWindow> FAssetTranslatorUI::ProgressWidget = nullptr;
TSharedPtr<SWindow> FAssetTranslatorUI::ToolWindow = nullptr;
TSharedPtr<STranslationProgressWindow> FAssetTranslatorUI::ToolProgressWidget = nullptr;
bool FAssetTranslatorUI::bIsProcessing = false;

//////////////////////////////////////////////////////////////////////////
// STranslationProgressWindow
//////////////////////////////////////////////////////////////////////////

void STranslationProgressWindow::Construct(const FArguments& InArgs, int32 InTotalItems)
{
	TotalItems = InTotalItems;
	CurrentItems = 0;
	SuccessItems = 0;
	FailedItems = 0;
	UnsupportedItems = 0;
	// TranslatedItems = 0; // Removed
	OperationName = TEXT("处理"); // 默认操作名称
	bIsComplete = false;
	bIsFailed = false;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(16.0f))
		[
			SNew(SVerticalBox)
			
			// 标题
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 16)
			[
				SNew(SHorizontalBox)
				
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 8, 0)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.Transform"))
				]
				
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("🌐 翻译进度 | Translation Progress")))
					.Font(FAppStyle::GetFontStyle("HeadingLarge"))
				]
			]
			
			// 进度百分比文本（大字体）
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 8, 0, 12)
			[
				SNew(STextBlock)
				.Text(this, &STranslationProgressWindow::GetProgressPercentText)
				.Font(FAppStyle::GetFontStyle("HeadingExtraLarge"))
				.Justification(ETextJustify::Center)
				.ColorAndOpacity(this, &STranslationProgressWindow::GetProgressColor)
			]
			
			// 进度条（增加高度）
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 12)
			[
				SNew(SBox)
				.HeightOverride(24.0f)  // 增加进度条高度
				[
					SNew(SProgressBar)
					.Percent(this, &STranslationProgressWindow::GetProgressPercent)
					.FillColorAndOpacity(this, &STranslationProgressWindow::GetProgressColor)
					.BorderPadding(FVector2D(0, 0))
				]
			]
			
			// 进度详细文本
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 16)
			[
				SNew(STextBlock)
				.Text(this, &STranslationProgressWindow::GetProgressText)
				.Font(FAppStyle::GetFontStyle("NormalFontBold"))
				.Justification(ETextJustify::Center)
				.ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.85f, 1.0f))
			]
			
			// 统计信息（卡片样式）
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 12)
			[
				SNew(SHorizontalBox)
				
				// 成功数量
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0, 0, 3, 0)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
					.Padding(FMargin(8.0f, 6.0f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("✓ 成功")))
							.Font(FAppStyle::GetFontStyle("SmallText"))
							.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f))
							.Justification(ETextJustify::Center)
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0, 3, 0, 0)
						[
							SNew(STextBlock)
							.Text(this, &STranslationProgressWindow::GetSuccessCountText)
							.Font(FAppStyle::GetFontStyle("HeadingLarge"))
							.ColorAndOpacity(FLinearColor(0.2f, 0.8f, 0.4f, 1.0f))
							.Justification(ETextJustify::Center)
						]
					]
				]
				
				// 失败数量
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(3, 0, 3, 0)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
					.Padding(FMargin(8.0f, 6.0f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("✗ 失败")))
							.Font(FAppStyle::GetFontStyle("SmallText"))
							.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f))
							.Justification(ETextJustify::Center)
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0, 3, 0, 0)
						[
							SNew(STextBlock)
							.Text(this, &STranslationProgressWindow::GetFailedCountText)
							.Font(FAppStyle::GetFontStyle("HeadingLarge"))
							.ColorAndOpacity(FLinearColor(0.9f, 0.3f, 0.3f, 1.0f))
							.Justification(ETextJustify::Center)
						]
					]
				]
				
				// 不支持数量
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(3, 0, 0, 0)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
					.Padding(FMargin(8.0f, 6.0f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("⚠ 不支持")))
							.Font(FAppStyle::GetFontStyle("SmallText"))
							.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f))
							.Justification(ETextJustify::Center)
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0, 3, 0, 0)
						[
							SNew(STextBlock)
							.Text(this, &STranslationProgressWindow::GetUnsupportedCountText)
							.Font(FAppStyle::GetFontStyle("HeadingLarge"))
							.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.3f, 1.0f))
							.Justification(ETextJustify::Center)
						]
					]
				]
			]
			
			// 当前处理项
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 0)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(FMargin(12.0f, 8.0f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("📄 当前处理 | Current Item:")))
						.Font(FAppStyle::GetFontStyle("SmallFontBold"))
						.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 4, 0, 0)
					[
						SNew(STextBlock)
						.Text(this, &STranslationProgressWindow::GetStatusText)
						.Font(FAppStyle::GetFontStyle("NormalText"))
						.AutoWrapText(true)
						.ColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.9f, 1.0f))
					]
				]
			]
		]
	];
}

void STranslationProgressWindow::UpdateProgress(int32 CurrentItem, const FString& InCurrentItemName)
{
	CurrentItems = CurrentItem;
	CurrentItemName = InCurrentItemName;
}

void STranslationProgressWindow::MarkComplete(int32 InSuccessCount, int32 InFailedCount)
{
	bIsComplete = true;
	SuccessItems = InSuccessCount;
	FailedItems = InFailedCount;
}

void STranslationProgressWindow::MarkFailed(const FString& InErrorMessage)
{
	bIsFailed = true;
	ErrorMessage = InErrorMessage;
}

void STranslationProgressWindow::Reset(int32 InTotalItems)
{
	TotalItems = InTotalItems;
	CurrentItems = 0;
	SuccessItems = 0;
	FailedItems = 0;
	// 重要：不重置 UnsupportedItems，因为不支持的资产数量是固定的
	// UnsupportedItems = 0;  // 注释掉，保持原值
	CurrentItemName.Empty();
	bIsComplete = false;
	bIsFailed = false;
	ErrorMessage.Empty();
}

void STranslationProgressWindow::IncrementSuccess()
{
	SuccessItems++;
}

void STranslationProgressWindow::IncrementFailed()
{
	FailedItems++;
}

void STranslationProgressWindow::SetUnsupportedCount(int32 Count)
{
	UnsupportedItems = Count;
}

void STranslationProgressWindow::SetOperationName(const FString& Name)
{
	OperationName = Name;
}

TOptional<float> STranslationProgressWindow::GetProgressPercent() const
{
	if (TotalItems == 0)
	{
		return 0.0f;
	}
	return (float)CurrentItems / (float)TotalItems;
}

FText STranslationProgressWindow::GetProgressPercentText() const
{
	float Percent = GetProgressPercent().Get(0.0f) * 100.0f;
	return FText::FromString(FString::Printf(TEXT("%.1f%%"), Percent));
}

FSlateColor STranslationProgressWindow::GetProgressColor() const
{
	if (bIsComplete)
	{
		if (FailedItems > 0)
		{
			return FLinearColor(0.9f, 0.6f, 0.2f, 1.0f); // 橙色：部分失败
		}
		return FLinearColor(0.2f, 0.8f, 0.4f, 1.0f); // 绿色：全部成功
	}
	if (bIsFailed)
	{
		return FLinearColor(0.9f, 0.3f, 0.3f, 1.0f); // 红色：失败
	}
	return FLinearColor(0.2f, 0.7f, 1.0f, 1.0f); // 蓝色：进行中
}

FText STranslationProgressWindow::GetSuccessCountText() const
{
	return FText::FromString(FString::Printf(TEXT("%d"), SuccessItems));
}

FText STranslationProgressWindow::GetFailedCountText() const
{
	return FText::FromString(FString::Printf(TEXT("%d"), FailedItems));
}

FText STranslationProgressWindow::GetUnsupportedCountText() const
{
	return FText::FromString(FString::Printf(TEXT("%d"), UnsupportedItems));
}

FText STranslationProgressWindow::GetProgressText() const
{
	if (bIsComplete)
	{
		return FText::FromString(FString::Printf(
			TEXT("✓ %s完成 | Completed: %d/%d 项 | %d/%d items"),
			*OperationName, CurrentItems, TotalItems, CurrentItems, TotalItems
		));
	}
	else if (bIsFailed)
	{
		return FText::FromString(FString::Printf(TEXT("✗ %s失败 | Operation Failed"), *OperationName));
	}
	else
	{
		return FText::FromString(FString::Printf(
			TEXT("⏳ 正在%s | Processing: %d/%d"),
			*OperationName, CurrentItems, TotalItems
		));
	}
}

FText STranslationProgressWindow::GetStatusText() const
{
	if (bIsComplete)
	{
		// 根据不同的操作类型显示不同的完成信息
		if (SuccessItems > 0 && FailedItems > 0)
		{
			// 部分成功
			return FText::FromString(FString::Printf(
				TEXT("✓ 完成！成功 %d 个，失败 %d 个 | Completed! %d succeeded, %d failed"),
				SuccessItems, FailedItems, SuccessItems, FailedItems));
		}
		else if (SuccessItems > 0)
		{
			// 全部成功
			return FText::FromString(FString::Printf(
				TEXT("✓ 完成！已成功处理 %d 个资产 | Completed! %d assets processed successfully"),
				SuccessItems, SuccessItems));
		}
		else if (FailedItems > 0)
		{
			// 全部失败
			return FText::FromString(FString::Printf(
				TEXT("✗ %d 个资产处理失败 | %d assets failed"),
				FailedItems, FailedItems));
		}
		
		return FText::FromString(TEXT("✓ 操作完成 | Operation completed"));
	}
	else if (bIsFailed)
	{
		return FText::FromString(FString::Printf(TEXT("❌ 错误 | Error: %s"), *ErrorMessage));
	}
	else if (!CurrentItemName.IsEmpty())
	{
		return FText::FromString(CurrentItemName);
	}
	else
	{
		return FText::FromString(TEXT("⏳ 准备中... | Preparing..."));
	}
}

//////////////////////////////////////////////////////////////////////////
// FAssetTranslatorUI
//////////////////////////////////////////////////////////////////////////

void FAssetTranslatorUI::ShowTranslationConfirmDialog(
	const TArray<FAssetData>& SelectedAssets,
	TFunction<void()> OnConfirm,
	TFunction<void()> OnCancel)
{
	const ULanguageOneSettings* Settings = GetDefault<ULanguageOneSettings>();
	
	// 如果禁用了确认对话框，直接执行
	if (!Settings->bConfirmBeforeAssetTranslation)
	{
		OnConfirm();
		return;
	}

	// 构建资产列表文本
	FString AssetListText;
	int32 DisplayCount = FMath::Min(SelectedAssets.Num(), 10);
	for (int32 i = 0; i < DisplayCount; i++)
	{
		AssetListText += FString::Printf(TEXT("  • %s\n"), *SelectedAssets[i].AssetName.ToString());
	}
	if (SelectedAssets.Num() > 10)
	{
		AssetListText += FString::Printf(TEXT("  ... 还有 %d 个资产 | and %d more assets\n"), 
			SelectedAssets.Num() - 10, SelectedAssets.Num() - 10);
	}

	// 创建对话框内容
	FText DialogTitle = FText::FromString(TEXT("确认翻译 | Confirm Translation"));
	FText DialogMessage = FText::FromString(FString::Printf(
		TEXT("准备翻译以下 %d 个资产：\nPrepare to translate %d assets:\n\n%s\n⚠️ 提示：翻译会直接修改资产内容，建议先备份！\n⚠️ Note: Translation will modify assets directly. Backup recommended!"),
		SelectedAssets.Num(), SelectedAssets.Num(), *AssetListText
	));

	// 创建自定义对话框窗口
	TSharedRef<SWindow> ConfirmWindow = SNew(SWindow)
		.Title(DialogTitle)
		.SizingRule(ESizingRule::Autosized)
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.SupportsMaximize(false)
		.SupportsMinimize(false);

	TSharedRef<SVerticalBox> ContentBox = SNew(SVerticalBox)
		
		// 图标和消息
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(16.0f)
		[
			SNew(SHorizontalBox)
			
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Top)
			.Padding(0, 0, 16, 0)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.Warning"))
				.DesiredSizeOverride(FVector2D(48, 48))
			]
			
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SNew(STextBlock)
					.Text(DialogMessage)
					.AutoWrapText(true)
					.Font(FAppStyle::GetFontStyle("NormalText"))
				]
			]
		]
		
		// 按钮
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(16.0f, 0, 16.0f, 16.0f)
		[
			SNew(SHorizontalBox)
			
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SSpacer)
			]
			
			// 取消按钮
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 8, 0)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("取消 | Cancel")))
				.HAlign(HAlign_Center)
				.ContentPadding(FMargin(24, 6))
				.OnClicked_Lambda([ConfirmWindow, OnCancel]() -> FReply
				{
					ConfirmWindow->RequestDestroyWindow();
					if (OnCancel)
					{
						OnCancel();
					}
					return FReply::Handled();
				})
			]
			
			// 确认按钮
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("✓ 开始翻译 | Start Translation")))
				.HAlign(HAlign_Center)
				.ContentPadding(FMargin(24, 6))
				.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
				.OnClicked_Lambda([ConfirmWindow, OnConfirm]() -> FReply
				{
					ConfirmWindow->RequestDestroyWindow();
					if (OnConfirm)
					{
						OnConfirm();
					}
					return FReply::Handled();
				})
			]
		];

	ConfirmWindow->SetContent(ContentBox);

	FSlateApplication::Get().AddWindow(ConfirmWindow);
}

void FAssetTranslatorUI::ShowClearOriginalConfirmDialog(
	const TArray<FAssetData>& SelectedAssets,
	TFunction<void()> OnConfirm,
	TFunction<void()> OnCancel)
{
	// 构建资产列表文本
	FString AssetListText;
	int32 DisplayCount = FMath::Min(SelectedAssets.Num(), 10);
	for (int32 i = 0; i < DisplayCount; i++)
	{
		AssetListText += FString::Printf(TEXT("  • %s\n"), *SelectedAssets[i].AssetName.ToString());
	}
	if (SelectedAssets.Num() > 10)
	{
		AssetListText += FString::Printf(TEXT("  ... 还有 %d 个资产 | and %d more assets\n"), 
			SelectedAssets.Num() - 10, SelectedAssets.Num() - 10);
	}

	// 创建对话框内容
	FText DialogTitle = FText::FromString(TEXT("确认清除原文 | Confirm Clear Original Text"));
	FText DialogMessage = FText::FromString(FString::Printf(
		TEXT("准备清除以下 %d 个资产中的原文：\nPrepare to clear original text from %d assets:\n\n%s\n⚠️ 警告：清除原文后，原文将无法还原！\n⚠️ Warning: Original text cannot be restored after clearing!"),
		SelectedAssets.Num(), SelectedAssets.Num(), *AssetListText
	));

	// 创建自定义对话框窗口
	TSharedRef<SWindow> ConfirmWindow = SNew(SWindow)
		.Title(DialogTitle)
		.SizingRule(ESizingRule::Autosized)
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.SupportsMaximize(false)
		.SupportsMinimize(false);

	TSharedRef<SVerticalBox> ContentBox = SNew(SVerticalBox)
		
		// 图标和消息
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(16.0f)
		[
			SNew(SHorizontalBox)
			
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Top)
			.Padding(0, 0, 16, 0)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.Error"))
				.DesiredSizeOverride(FVector2D(48, 48))
			]
			
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SNew(STextBlock)
					.Text(DialogMessage)
					.AutoWrapText(true)
					.Font(FAppStyle::GetFontStyle("NormalText"))
				]
			]
		]
		
		// 按钮
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(16.0f, 0, 16.0f, 16.0f)
		[
			SNew(SHorizontalBox)
			
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SSpacer)
			]
			
			// 取消按钮
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 8, 0)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("取消 | Cancel")))
				.HAlign(HAlign_Center)
				.ContentPadding(FMargin(24, 6))
				.OnClicked_Lambda([ConfirmWindow, OnCancel]() -> FReply
				{
					ConfirmWindow->RequestDestroyWindow();
					if (OnCancel)
					{
						OnCancel();
					}
					return FReply::Handled();
				})
			]
			
			// 确认按钮
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("⚠ 确认清除 | Confirm Clear")))
				.HAlign(HAlign_Center)
				.ContentPadding(FMargin(24, 6))
				.ButtonStyle(FAppStyle::Get(), "ErrorButton")
				.OnClicked_Lambda([ConfirmWindow, OnConfirm]() -> FReply
				{
					ConfirmWindow->RequestDestroyWindow();
					if (OnConfirm)
					{
						OnConfirm();
					}
					return FReply::Handled();
				})
			]
		];

	ConfirmWindow->SetContent(ContentBox);
	FSlateApplication::Get().AddWindow(ConfirmWindow);
}

TSharedPtr<STranslationProgressWindow> FAssetTranslatorUI::ShowTranslationProgress(int32 TotalItems)
{
	CloseTranslationProgress();

	ProgressWindow = SNew(SWindow)
		.Title(FText::FromString(TEXT("🌐 翻译进度 | Translation Progress")))
		.ClientSize(FVector2D(700, 350))  // 增加大小以显示更多信息
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.IsTopmostWindow(true)
		.SizingRule(ESizingRule::FixedSize);

	ProgressWidget = SNew(STranslationProgressWindow, TotalItems);
	ProgressWindow->SetContent(ProgressWidget.ToSharedRef());

	FSlateApplication::Get().AddWindow(ProgressWindow.ToSharedRef());

	return ProgressWidget;
}

void FAssetTranslatorUI::CloseTranslationProgress()
{
	if (ProgressWindow.IsValid())
	{
		ProgressWindow->RequestDestroyWindow();
		ProgressWindow.Reset();
		ProgressWidget.Reset();
	}
}

void FAssetTranslatorUI::ShowTranslationCompleteNotification(int32 SuccessCount, int32 TotalCount)
{
	FNotificationInfo Info(FText::FromString(FString::Printf(
		TEXT("✓ 翻译完成！成功 %d/%d | Translation completed! %d/%d successful"),
		SuccessCount, TotalCount, SuccessCount, TotalCount
	)));
	Info.ExpireDuration = 5.0f;
	Info.bUseSuccessFailIcons = true;
	Info.bUseLargeFont = true;
	FSlateNotificationManager::Get().AddNotification(Info);
}

void FAssetTranslatorUI::ShowErrorNotification(const FString& Message)
{
	FNotificationInfo Info(FText::FromString(FString::Printf(
		TEXT("✗ 错误 | Error: %s"), *Message
	)));
	Info.ExpireDuration = 5.0f;
	Info.bUseSuccessFailIcons = true;
	FSlateNotificationManager::Get().AddNotification(Info);
}

void FAssetTranslatorUI::ShowInfoNotification(const FString& Message)
{
	FNotificationInfo Info(FText::FromString(Message));
	Info.ExpireDuration = 3.0f;
	FSlateNotificationManager::Get().AddNotification(Info);
}

void FAssetTranslatorUI::ShowWarningNotification(const FString& Message)
{
	FNotificationInfo Info(FText::FromString(Message));
	Info.ExpireDuration = 5.0f;
	Info.bFireAndForget = true;
	Info.bUseThrobber = false;
	Info.bUseSuccessFailIcons = true;
	// 设置为警告样式（橙黄色）
	Info.Image = FCoreStyle::Get().GetBrush(TEXT("MessageLog.Warning"));
	FSlateNotificationManager::Get().AddNotification(Info);
}

void FAssetTranslatorUI::ShowAssetTranslationTool(const TArray<FAssetData>& SelectedAssets)
{
	// 强制重置处理状态，防止之前的操作异常退出导致状态卡死
	bIsProcessing = false;

	if (SelectedAssets.Num() == 0)
	{
		ShowInfoNotification(TEXT("请先选择要操作的资产 | Please select assets first"));
		return;
	}

	// 如果窗口已存在，关闭旧窗口
	if (ToolWindow.IsValid())
	{
		ToolWindow->RequestDestroyWindow();
		ToolWindow.Reset();
		ToolProgressWidget.Reset();
	}

	// 分离支持的和不支持的资产
	TArray<FAssetData> SupportedAssets;
	TArray<FAssetData> UnsupportedAssets;
	
	for (const FAssetData& AssetData : SelectedAssets)
	{
		if (FAssetTranslator::CanTranslateAsset(AssetData))
		{
			SupportedAssets.Add(AssetData);
		}
		else
		{
			UnsupportedAssets.Add(AssetData);
		}
	}
	
	// 如果没有支持的资产，显示提示并返回
	if (SupportedAssets.Num() == 0)
	{
		ShowInfoNotification(TEXT("所选资产均不支持翻译 | None of the selected assets support translation"));
		return;
	}

	// 创建进度组件（只针对支持的资产）
	ToolProgressWidget = SNew(STranslationProgressWindow, SupportedAssets.Num());
	
	// 设置不支持的资产数量
	if (ToolProgressWidget.IsValid())
	{
		ToolProgressWidget->SetUnsupportedCount(UnsupportedAssets.Num());
	}

	// 创建工具窗口
	FText DialogTitle;
	if (UnsupportedAssets.Num() > 0)
	{
		DialogTitle = FText::FromString(FString::Printf(
			TEXT("资产翻译工具 | Asset Translation Tool (支持 %d / 总共 %d | %d / %d total)"), 
			SupportedAssets.Num(), SelectedAssets.Num(), SupportedAssets.Num(), SelectedAssets.Num()));
	}
	else
	{
		DialogTitle = FText::FromString(FString::Printf(
			TEXT("资产翻译工具 | Asset Translation Tool (%d 个资产 | %d assets)"), 
			SupportedAssets.Num(), SupportedAssets.Num()));
	}
	
	ToolWindow = SNew(SWindow)
		.Title(DialogTitle)
		.ClientSize(FVector2D(850, 550))  // 增加宽度以适配更多按钮
		.SizingRule(ESizingRule::UserSized)
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.SupportsMaximize(true)  // 允许最大化
		.SupportsMinimize(false);

	TSharedRef<SVerticalBox> ContentBox = SNew(SVerticalBox)
		
		// 标题栏（带图标）
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(16.0f, 16.0f, 16.0f, 8.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 12, 0)
			[
				SNew(SImage)
				.Image(LANGUAGEONE_EDITOR_STYLE::GetBrush("Icons.Transform"))
				.DesiredSizeOverride(FVector2D(32, 32))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("🌐 资产翻译工具 | Asset Translation Tool")))
							.Font(FAppStyle::GetFontStyle("HeadingLarge"))
				.Justification(ETextJustify::Left)
			]
		]
		
		// 分隔线
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(16.0f, 0, 16.0f, 8.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(0)
			[
				SNew(SBox)
				.HeightOverride(1.0f)
			]
		]
		
		// 说明文本（简化显示，只显示固定信息）
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(16.0f, 8.0f, 16.0f, 16.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(FMargin(12.0f, 8.0f))
			[
				SNew(SVerticalBox)
				
				// 资产数量信息（固定不变）
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString([SupportedAssets, UnsupportedAssets]()
					{
						if (UnsupportedAssets.Num() > 0)
						{
							return FString::Printf(
								TEXT("📦 可处理资产：%d 个  |  ⚠ 不支持：%d 个"),
								SupportedAssets.Num(), UnsupportedAssets.Num());
						}
						else
						{
							return FString::Printf(
								TEXT("📦 可处理资产：%d 个"),
								SupportedAssets.Num());
						}
					}()))
					.Font(FAppStyle::GetFontStyle("NormalFontBold"))
					.ColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.9f, 1.0f))
					.Justification(ETextJustify::Center)
				]
				
				// 提示信息
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 8, 0, 0)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("💡 提示：点击按钮执行操作，进度条将显示处理结果")))
					.Font(LANGUAGEONE_EDITOR_STYLE::GetFontStyle("SmallText"))
					.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f))
					.Justification(ETextJustify::Center)
					.AutoWrapText(true)
				]
			]
		]
		
		// 进度显示区域
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(16.0f, 0, 16.0f, 16.0f)
		[
			ToolProgressWidget.ToSharedRef()
		]
		
		// 分隔线
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(16.0f, 0, 16.0f, 12.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(0)
			[
				SNew(SBox)
				.HeightOverride(1.0f)
			]
		]
		
		// 按钮区域（垂直布局，更清晰）
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(16.0f, 0, 16.0f, 16.0f)
		[
			SNew(SVerticalBox)
			
			// 第一行：主要操作按钮
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 8.0f)
			[
				SNew(SHorizontalBox)
				
				// 翻译/切换按钮（主要操作）
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0, 0, 8, 0)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("✨ 翻译 | Translate")))
					.HAlign(HAlign_Center)
					.ContentPadding(FMargin(32, 10))
					.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
					.ToolTipText(FText::FromString(TEXT("翻译所有选中的资产（自动跳过已翻译部分） | Translate all selected assets (skip already translated parts)")))
					.OnClicked_Lambda([SupportedAssets]() -> FReply
					{
						// 检查是否正在处理
						if (FAssetTranslatorUI::IsProcessing())
						{
							FAssetTranslatorUI::ShowWarningNotification(TEXT("正在处理中，请等待当前操作完成 | Processing, please wait for current operation to complete"));
							return FReply::Handled();
						}

						// 重置进度组件
						if (ToolProgressWidget.IsValid())
						{
							ToolProgressWidget->Reset(SupportedAssets.Num());
						}
						
						// 直接执行翻译（补全）操作
						// 内部逻辑已优化：会对已翻译部分进行格式检查/跳过，只翻译未翻译部分
						FAssetTranslatorUI::ShowInfoNotification(
							FString::Printf(TEXT("开始翻译 %d 个资产 | Starting translation for %d assets"), SupportedAssets.Num(), SupportedAssets.Num()));
						
						FAssetTranslator::PerformTranslation(SupportedAssets, false);
						
						return FReply::Handled();
					})
				]
				
				// 还原按钮
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("↩ 还原 | Restore")))
					.HAlign(HAlign_Center)
					.ContentPadding(FMargin(32, 10))
					.ButtonStyle(FAppStyle::Get(), "Button")
					.ToolTipText(FText::FromString(TEXT("还原存在翻译的资产，移除翻译内容，恢复原文 | Restore assets with translation, remove translations, restore original text")))
					.OnClicked_Lambda([SupportedAssets]() -> FReply
					{
						// 检查是否正在处理
						if (FAssetTranslatorUI::IsProcessing())
						{
							FAssetTranslatorUI::ShowWarningNotification(TEXT("正在处理中，请等待当前操作完成 | Processing, please wait for current operation to complete"));
							return FReply::Handled();
						}
						
						// 重置进度组件（确保进度条被重置）
						if (ToolProgressWidget.IsValid())
						{
							ToolProgressWidget->Reset(SupportedAssets.Num());
						}
						
						// 直接执行还原（所有资产都已被过滤为支持的）
						FAssetTranslator::PerformRestore(SupportedAssets);
						return FReply::Handled();
					})
				]
			]
			
			// 第二行：清除原文和取消按钮
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				
				// 清除原文按钮（警告样式）
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0, 0, 8, 0)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("🗑 清除原文 | Clear Original")))
					.HAlign(HAlign_Center)
					.ContentPadding(FMargin(32, 10))
					.ButtonStyle(FAppStyle::Get(), "Button")
					.ForegroundColor(FLinearColor(1.0f, 0.3f, 0.3f, 1.0f))  // 红色文字
					.ToolTipText(FText::FromString(TEXT("⚠️ 清除原文，只保留译文（不可还原）| Clear original text, keep translation only (irreversible)")))
					.OnClicked_Lambda([SupportedAssets]() -> FReply
					{
						// 重置进度组件（确保进度条被重置）
						if (ToolProgressWidget.IsValid())
						{
							ToolProgressWidget->Reset(SupportedAssets.Num());
						}
						
						// 清除原文（会显示确认对话框）
						FAssetTranslator::ClearOriginalText(SupportedAssets);
						return FReply::Handled();
					})
				]
				
				// 关闭按钮
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("✖ 关闭 | Close")))
					.HAlign(HAlign_Center)
					.ContentPadding(FMargin(32, 10))
					.ButtonStyle(FAppStyle::Get(), "Button")
					.ToolTipText(FText::FromString(TEXT("关闭工具窗口 | Close tool window")))
					.OnClicked_Lambda([]() -> FReply
					{
						if (ToolWindow.IsValid())
						{
							ToolWindow->RequestDestroyWindow();
							ToolWindow.Reset();
							ToolProgressWidget.Reset();
						}
						return FReply::Handled();
					})
				]
			]
		];

	ToolWindow->SetContent(ContentBox);
	FSlateApplication::Get().AddWindow(ToolWindow.ToSharedRef());
}

TSharedPtr<STranslationProgressWindow> FAssetTranslatorUI::GetToolWindowProgress()
{
	return ToolProgressWidget;
}

bool FAssetTranslatorUI::IsProcessing()
{
	return bIsProcessing;
}

void FAssetTranslatorUI::SetProcessing(bool bInProcessing)
{
	bIsProcessing = bInProcessing;
	UE_LOG(LogTemp, Log, TEXT("Asset translation processing state changed: %s"), bInProcessing ? TEXT("BUSY") : TEXT("IDLE"));
}
