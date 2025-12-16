// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTranslator.h"
#include "AssetTranslatorUI.h"
#include "LanguageOneSettings.h"
#include "LanguageOneCompatibility.h"
#include "CommentTranslator.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
// IStringTableEditor 接口用于刷新 StringTable 编辑器
#include "IStringTableEditor.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/DataTable.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/EditableText.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableText.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/RichTextBlock.h"
#include "Engine/Blueprint.h"
#include "EdGraphSchema_K2.h"
#include "K2Node.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_Event.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "FileHelpers.h" // 包含 UEditorLoadingAndSavingUtils

// 辅助函数：从已翻译文本中提取原文
// 支持多种格式：
// 1. 新格式（译文在下，默认）: "标记开始原文标记结束\n---\n译文"
// 2. 新格式（译文在上）: "译文\n---\n标记开始原文标记结束"
// 3. 旧格式: "Translated\n---\nOriginal"（兼容）
static FString StripExistingTranslation(const FString& Text)
{
	if (Text.IsEmpty())
	{
		return Text;
	}
	
	// 检查是否有零宽字符隐藏标记（新格式）
	// U+200B = Zero Width Space (ZWSP)
	// U+200C = Zero Width Non-Joiner (ZWNJ) - 标记原文开始
	// U+200D = Zero Width Joiner (ZWJ) - 标记原文结束
	const TCHAR HiddenStartMarker[] = { 0x200B, 0x200C, 0 }; // ZWSP + ZWNJ
	const TCHAR HiddenEndMarker[] = { 0x200B, 0x200D, 0 };   // ZWSP + ZWJ
	const FString HiddenStart(HiddenStartMarker);
	const FString HiddenEnd(HiddenEndMarker);
	
	// 优先从隐藏标记中提取原文（隐藏标记中总是原文）
	int32 StartPos = Text.Find(HiddenStart, ESearchCase::CaseSensitive);
	
	if (StartPos != INDEX_NONE && StartPos >= 0)
	{
		// 从开始标记之后查找结束标记
		int32 SearchStartPos = StartPos + HiddenStart.Len();
		int32 EndPos = Text.Find(HiddenEnd, ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchStartPos);
		
		if (EndPos != INDEX_NONE && EndPos > StartPos)
		{
			// 提取隐藏的原文（在标记之间的内容）
			// 重要：只提取标记之间的内容，不做额外清理
			// 因为标记之间的内容就是用户的原文，可能本身就包含零宽字符
			int32 OriginalStart = StartPos + HiddenStart.Len();
			int32 OriginalLen = EndPos - OriginalStart;
			
			if (OriginalLen >= 0) // 允许空字符串
			{
				FString OriginalText = Text.Mid(OriginalStart, OriginalLen);
				
				UE_LOG(LogTemp, VeryVerbose, TEXT("StripExistingTranslation: Extracted from hidden markers: '%s'"), *OriginalText);
				return OriginalText;
			}
		}
	}
	
	// 检查是否有 --- 分隔符
	const FString Separator = TEXT("\n---\n");
	if (Text.Contains(Separator))
	{
		FString Left, Right;
		if (Text.Split(Separator, &Left, &Right))
		{
			// 从左边或右边提取原文
			// 需要判断哪边包含隐藏标记
			bool bLeftHasMarker = Left.Contains(HiddenStart) && Left.Contains(HiddenEnd);
			bool bRightHasMarker = Right.Contains(HiddenStart) && Right.Contains(HiddenEnd);
			
			FString OriginalText;
			if (bLeftHasMarker)
			{
				// 左边有标记，说明原文在左边（格式：​‌原文​‍\n---\n译文）
				int32 LeftStartPos = Left.Find(HiddenStart, ESearchCase::CaseSensitive);
				int32 LeftEndPos = Left.Find(HiddenEnd, ESearchCase::CaseSensitive, ESearchDir::FromStart, LeftStartPos + HiddenStart.Len());
				if (LeftStartPos != INDEX_NONE && LeftEndPos != INDEX_NONE)
				{
					int32 OrigStart = LeftStartPos + HiddenStart.Len();
					OriginalText = Left.Mid(OrigStart, LeftEndPos - OrigStart);
				}
			}
			else if (bRightHasMarker)
			{
				// 右边有标记，说明原文在右边（格式：译文\n---\n​‌原文​‍）
				int32 RightStartPos = Right.Find(HiddenStart, ESearchCase::CaseSensitive);
				int32 RightEndPos = Right.Find(HiddenEnd, ESearchCase::CaseSensitive, ESearchDir::FromStart, RightStartPos + HiddenStart.Len());
				if (RightStartPos != INDEX_NONE && RightEndPos != INDEX_NONE)
				{
					int32 OrigStart = RightStartPos + HiddenStart.Len();
					OriginalText = Right.Mid(OrigStart, RightEndPos - OrigStart);
				}
			}
			else
			{
				// 没有标记，默认左边是原文（兼容旧格式）
				OriginalText = Left.TrimStartAndEnd();
			}
			
			if (!OriginalText.IsEmpty())
			{
				UE_LOG(LogTemp, VeryVerbose, TEXT("StripExistingTranslation: Extracted from separator: '%s'"), *OriginalText);
				return OriginalText;
			}
		}
	}
	
	// 如果没有找到任何翻译标记，返回原文本（不做任何修改）
	// 重要：不清理零宽字符，因为用户原文可能本身就包含这些字符
	UE_LOG(LogTemp, VeryVerbose, TEXT("StripExistingTranslation: No translation markers, returning original: '%s'"), *Text);
	return Text;
}

// 辅助函数：检查文本是否已经包含翻译
// 注意：对于 StringTable，还需要检查元数据中是否有原文
static bool HasTranslation(const FString& Text)
{
	if (Text.IsEmpty())
	{
		return false;
	}
	
	// 检查是否有可见的分隔符
	if (Text.Contains(TEXT("\n---\n")))
	{
		return true;
	}
	
	// 检查是否有隐藏标记（使用零宽字符，用于非 StringTable 资产）
	// U+200B = Zero Width Space (ZWSP)
	// U+200C = Zero Width Non-Joiner (ZWNJ)
	// U+200D = Zero Width Joiner (ZWJ)
	const TCHAR HiddenStartMarker[] = { 0x200B, 0x200C, 0 }; // ZWSP + ZWNJ
	const TCHAR HiddenEndMarker[] = { 0x200B, 0x200D, 0 };   // ZWSP + ZWJ
	const FString HiddenStart(HiddenStartMarker);
	const FString HiddenEnd(HiddenEndMarker);
	
	if (Text.Contains(HiddenStart) && Text.Contains(HiddenEnd))
	{
		// 验证标记的顺序是否正确
		int32 StartPos = Text.Find(HiddenStart, ESearchCase::CaseSensitive);
		if (StartPos != INDEX_NONE)
		{
			int32 EndPos = Text.Find(HiddenEnd, ESearchCase::CaseSensitive, ESearchDir::FromStart, StartPos);
			if (EndPos != INDEX_NONE && EndPos > StartPos)
			{
				return true;
			}
		}
	}
	
	return false;
}

// 辅助函数：安全地刷新 StringTable 编辑器
// 注意：不传入 Key 参数，避免改变编辑器的选中项
static void RefreshStringTableEditor(UStringTable* StringTable, const FString& Key = FString())
{
	if (!StringTable)
	{
		return;
	}

	// 触发 PostEditChange 通知
	StringTable->PostEditChange();
	
	// 尝试使用 IStringTableEditor 接口刷新编辑器
	if (FModuleManager::Get().IsModuleLoaded("StringTableEditor") && GEditor)
	{
		UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		if (AssetEditorSubsystem)
		{
			IAssetEditorInstance* EditorInstance = AssetEditorSubsystem->FindEditorForAsset(StringTable, false);
			if (EditorInstance)
			{
				// 尝试转换为 IStringTableEditor 接口
				IStringTableEditor* StringTableEditor = static_cast<IStringTableEditor*>(EditorInstance);
				if (StringTableEditor)
				{
					// 使用官方 API 刷新编辑器
					// 传入空字符串可以刷新整个表格而不改变选中项
					StringTableEditor->RefreshStringTableEditor(FString());
				}
			}
		}
	}
}

void FAssetTranslator::TranslateSelectedAssets(const TArray<FAssetData>& SelectedAssets, bool bIsFromEditor)
{
	if (SelectedAssets.Num() == 0)
	{
		if (!bIsFromEditor)
		{
			FAssetTranslatorUI::ShowInfoNotification(TEXT("请先选择要翻译的资产 | Please select assets to translate"));
		}
		return;
	}

	// 统计可翻译资产
	TArray<FAssetData> TranslatableAssets;
	for (const FAssetData& AssetData : SelectedAssets)
	{
		if (CanTranslateAsset(AssetData))
		{
			TranslatableAssets.Add(AssetData);
		}
	}

	if (TranslatableAssets.Num() == 0)
	{
		if (!bIsFromEditor)
		{
			FAssetTranslatorUI::ShowInfoNotification(TEXT("所选资产不支持翻译 | Selected assets cannot be translated"));
		}
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Starting translation for %d assets"), TranslatableAssets.Num());

	const ULanguageOneSettings* Settings = GetDefault<ULanguageOneSettings>();
	
	// 如果是编辑器快捷键触发，或者是批量翻译且设置需要确认
	if (!bIsFromEditor && (Settings->bConfirmBeforeAssetTranslation || TranslatableAssets.Num() > 1))
	{
		// 显示确认对话框
		FAssetTranslatorUI::ShowTranslationConfirmDialog(
			TranslatableAssets,
			[TranslatableAssets]() // OnConfirm
			{
				PerformTranslation(TranslatableAssets);
			},
			[]() // OnCancel
			{
				UE_LOG(LogTemp, Log, TEXT("Translation cancelled by user"));
			}
		);
	}
	else
	{
		// 直接开始翻译（静默模式如果是在编辑器中）
		PerformTranslation(TranslatableAssets, bIsFromEditor);
	}
}

void FAssetTranslator::PerformTranslation(const TArray<FAssetData>& TranslatableAssets, bool bSilent)
{
	// 设置处理状态
	FAssetTranslatorUI::SetProcessing(true);
	
	const ULanguageOneSettings* Settings = GetDefault<ULanguageOneSettings>();

	// 显示进度窗口 (如果不是静默模式或批量翻译)
	TSharedPtr<STranslationProgressWindow> ProgressWidget;
	if (!bSilent || TranslatableAssets.Num() > 1)
	{
		ProgressWidget = FAssetTranslatorUI::GetToolWindowProgress();
	}
	
	// 创建翻译状态追踪
	struct FTranslationState
	{
		int32 TotalAssets;
		int32 CompletedAssets;
		int32 SuccessAssets;
		int32 FailedAssets;
		TSharedPtr<STranslationProgressWindow> ProgressWidget;
		bool bSilent;
		
		FTranslationState(int32 Total, TSharedPtr<STranslationProgressWindow> Widget, bool InSilent)
			: TotalAssets(Total), CompletedAssets(0), SuccessAssets(0), FailedAssets(0), ProgressWidget(Widget), bSilent(InSilent)
		{}
	};
	
	TSharedPtr<FTranslationState> State = MakeShared<FTranslationState>(TranslatableAssets.Num(), ProgressWidget, bSilent);
	
	// 翻译每个资产
	for (int32 i = 0; i < TranslatableAssets.Num(); i++)
	{
		const FAssetData& AssetData = TranslatableAssets[i];
		
		// 更新进度（先更新进度，确保即使失败也能看到进度变化）
		if (State->ProgressWidget.IsValid())
		{
			State->ProgressWidget->UpdateProgress(i + 1, AssetData.AssetName.ToString());
		}
		
		// 检查资产是否可以被加载
		UObject* Asset = AssetData.GetAsset();
		if (!Asset)
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to load asset %d/%d: %s"), 
				i + 1, TranslatableAssets.Num(), *AssetData.AssetName.ToString());
			State->CompletedAssets++;
			State->FailedAssets++;
			if (State->ProgressWidget.IsValid())
			{
				State->ProgressWidget->IncrementFailed();
			}
			continue;
		}
		
		// 检查资产类型是否支持翻译
		if (!CanTranslateAsset(AssetData))
		{
			UE_LOG(LogTemp, Warning, TEXT("Unsupported asset type %d/%d: %s (Type: %s)"), 
				i + 1, TranslatableAssets.Num(), *AssetData.AssetName.ToString(), 
				*LanguageOneAssetDataHelper::GetAssetClassName(AssetData));
			State->CompletedAssets++;
			State->FailedAssets++;
			if (State->ProgressWidget.IsValid())
			{
				State->ProgressWidget->IncrementFailed();
			}
			continue;
		}

		UE_LOG(LogTemp, Log, TEXT("Translating asset %d/%d: %s (Type: %s)"), 
			i + 1, TranslatableAssets.Num(), *AssetData.AssetName.ToString(), 
			*LanguageOneAssetDataHelper::GetAssetClassName(AssetData));

		// 根据资产类型调用相应的翻译函数
		FString ClassName = LanguageOneAssetDataHelper::GetAssetClassName(AssetData);
		
		if (ClassName.Contains(TEXT("StringTable")))
		{
			TranslateStringTable(Asset, State->bSilent);
		}
		else if (ClassName.Contains(TEXT("DataTable")))
		{
			TranslateDataTable(Asset, State->bSilent);
		}
		else if (ClassName.Contains(TEXT("WidgetBlueprint")) || ClassName.Contains(TEXT("UserWidget")))
		{
			TranslateWidgetBlueprint(Asset, State->bSilent);
		}
		else if (ClassName.Contains(TEXT("Blueprint")))
		{
			TranslateBlueprint(Asset, State->bSilent);
		}
		else
		{
			TranslateAssetMetadata(Asset, State->bSilent);
		}
		
		// 标记为成功
		State->SuccessAssets++;
		State->CompletedAssets++;
		if (State->ProgressWidget.IsValid())
		{
			State->ProgressWidget->IncrementSuccess();
		}
	}
	
	// 标记完成
	if (State->ProgressWidget.IsValid())
	{
		State->ProgressWidget->MarkComplete(State->SuccessAssets, State->FailedAssets);
	}
	
	// 延迟关闭进度窗口
	if (State->ProgressWidget.IsValid())
	{
		FTimerHandle TimerHandle;
		GEditor->GetTimerManager()->SetTimer(TimerHandle, []()
		{
			FAssetTranslatorUI::CloseTranslationProgress();
		}, 3.0f, false);
	}
	
	// 静默模式下只在日志输出，不显示弹窗
	if (bSilent)
	{
		UE_LOG(LogTemp, Log, TEXT("Asset translation completed silently: %d/%d success"), State->SuccessAssets, State->TotalAssets);
		
		// 特殊处理：对于无法实时刷新的资产（如 StringTable），给一个温和的提示
		// 检查是否有 StringTable 被翻译成功
		if (State->SuccessAssets > 0)
		{
			// 这里我们不能直接访问 TranslatableAssets，因为它是按值捕获的还是参数传递的需要确认上下文
			// 简单起见，我们只能在日志里提示，或者使用一个很短的通知
			// 但用户明确要求“蓝图是不会有的”，所以我们必须非常小心
			// 只有 StringTable 才提示
			// 由于这里无法轻易判断具体翻译了什么类型（State 里没存），我们只能尽量保持静默
			// 如果必须提示，可以在上面的 TranslateStringTable 里加标记
		}
	}
}

bool FAssetTranslator::CanTranslateAsset(const FAssetData& AssetData)
{
	FString ClassName = LanguageOneAssetDataHelper::GetAssetClassName(AssetData);
	
	// 支持的资产类型
	return ClassName.Contains(TEXT("StringTable")) ||
		   ClassName.Contains(TEXT("DataTable")) ||
		   ClassName.Contains(TEXT("WidgetBlueprint")) ||
		   ClassName.Contains(TEXT("Blueprint")) ||
		   ClassName.Contains(TEXT("Material")) ||
		   ClassName.Contains(TEXT("Texture")) ||
		   ClassName.Contains(TEXT("StaticMesh")) ||
		   ClassName.Contains(TEXT("SkeletalMesh"));
}

int32 FAssetTranslator::GetTranslatableTextCount(const FAssetData& AssetData)
{
	// 这里可以实现更精确的统计，暂时返回估算值
	return 1;
}

bool FAssetTranslator::HasAssetTranslation(const FAssetData& AssetData)
{
	// 重要：每次调用都重新获取资产，不使用缓存
	UObject* Asset = AssetData.GetAsset();
	if (!Asset)
	{
		return false;
	}

	FString ClassName = LanguageOneAssetDataHelper::GetAssetClassName(AssetData);

	if (ClassName.Contains(TEXT("StringTable")))
	{
		UStringTable* StringTable = Cast<UStringTable>(Asset);
		if (StringTable)
		{
			// 重新获取 StringTable 数据以确保是最新的
			FStringTableConstRef StringTableData = StringTable->GetStringTable();
			TArray<FString> Keys;
			LanguageOneStringTableHelper::EnumerateStringTableKeys(StringTableData, Keys);

			for (const FString& Key : Keys)
			{
				// 每次都重新读取条目内容
				FString Text = LanguageOneStringTableHelper::FindStringTableEntry(StringTableData, Key);
				
				// 关键：只检查文本内容，不检查元数据
				// 这样还原后（文本中没有翻译标记），就会被识别为未翻译
				if (HasTranslation(Text))
				{
					UE_LOG(LogTemp, VeryVerbose, TEXT("HasAssetTranslation: %s key '%s' HAS translation (text contains markers)"), 
						*AssetData.AssetName.ToString(), *Key);
					return true;
				}
				
				// 调试日志：记录没有翻译的情况
				UE_LOG(LogTemp, VeryVerbose, TEXT("HasAssetTranslation: %s key '%s' NO translation (text is clean)"), 
					*AssetData.AssetName.ToString(), *Key);
			}
			
			// 如果所有条目都没有翻译标记，返回 false
			UE_LOG(LogTemp, VeryVerbose, TEXT("HasAssetTranslation: %s - NO translation found in any entry"), 
				*AssetData.AssetName.ToString());
			return false;
		}
	}
	else if (ClassName.Contains(TEXT("DataTable")))
	{
		UDataTable* DataTable = Cast<UDataTable>(Asset);
		if (DataTable)
		{
			TArray<FName> RowNames = DataTable->GetRowNames();
			const UScriptStruct* RowStruct = DataTable->GetRowStruct();
			if (RowStruct)
			{
				for (const FName& RowName : RowNames)
				{
					uint8* RowData = DataTable->FindRowUnchecked(RowName);
					if (RowData)
					{
						for (TFieldIterator<FProperty> It(const_cast<UScriptStruct*>(RowStruct)); It; ++It)
						{
							FProperty* Property = *It;
							if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
							{
								FText* TextValue = TextProperty->ContainerPtrToValuePtr<FText>(RowData);
								if (TextValue && HasTranslation(TextValue->ToString()))
								{
									return true;
								}
							}
							else if (FStrProperty* StrProperty = CastField<FStrProperty>(Property))
							{
								FString* StrValue = StrProperty->ContainerPtrToValuePtr<FString>(RowData);
								if (StrValue && HasTranslation(*StrValue))
								{
									return true;
								}
							}
						}
					}
				}
			}
		}
	}
	// 其他资产类型的检查类似...
	
	return false;
}

void FAssetTranslator::TranslateStringTable(UObject* Asset, bool bSilent)
{
	UStringTable* StringTable = Cast<UStringTable>(Asset);
	if (!StringTable)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Translating StringTable: %s"), *StringTable->GetName());

	// 获取 String Table 的所有条目
	FStringTableConstRef StringTableData = StringTable->GetStringTable();
	
	TArray<FString> Keys;
	LanguageOneStringTableHelper::EnumerateStringTableKeys(StringTableData, Keys);

	if (Keys.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("StringTable is empty: %s"), *StringTable->GetName());
		return;
	}

	// 如果是在编辑器中触发（静默模式），尝试获取选中的条目
	TArray<FString> SelectedKeys = Keys; // 默认翻译全部
	if (bSilent && GEditor)
	{
		// 尝试从 StringTable 编辑器获取选中的条目
		// 注意：IStringTableEditor 接口可能不存在于所有版本，这里只做基本检查
		UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		if (AssetEditorSubsystem)
		{
			IAssetEditorInstance* EditorInstance = AssetEditorSubsystem->FindEditorForAsset(StringTable, false);
			if (EditorInstance)
			{
				// 注意：IStringTableEditor 可能没有直接获取选中项的方法
				// 目前先翻译全部，后续可以根据需要扩展
				// 如果用户选中了特定条目，可以通过其他方式获取
				UE_LOG(LogTemp, Log, TEXT("StringTable editor found, translating all entries"));
			}
		}
	}

	// 调用翻译条目的函数
	TranslateStringTableEntries(Asset, SelectedKeys, bSilent);
}

void FAssetTranslator::TranslateStringTableEntries(UObject* Asset, const TArray<FString>& SelectedKeys, bool bSilent)
{
	UStringTable* StringTable = Cast<UStringTable>(Asset);
	if (!StringTable || SelectedKeys.Num() == 0)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Translating %d entries in StringTable: %s"), SelectedKeys.Num(), *StringTable->GetName());

	// 获取 StringTable 数据
	FStringTableConstRef StringTableData = StringTable->GetStringTable();

	// 创建翻译状态追踪
	struct FStringTableTranslationState
	{
		UStringTable* StringTable;
		int32 TotalCount;
		int32 CompletedCount;
		int32 SuccessCount;
		TMap<FString, FString> OriginalTexts;
		bool bSilent;
		
		FStringTableTranslationState(UStringTable* InTable, int32 Total, bool InSilent)
			: StringTable(InTable), TotalCount(Total), CompletedCount(0), SuccessCount(0), bSilent(InSilent)
		{}
	};
	
	TSharedPtr<FStringTableTranslationState> State = MakeShared<FStringTableTranslationState>(StringTable, SelectedKeys.Num(), bSilent);

	for (const FString& Key : SelectedKeys)
	{
		// 查找源文本（使用兼容性辅助函数）
		FString SourceText = LanguageOneStringTableHelper::FindStringTableEntry(StringTableData, Key);
		if (SourceText.IsEmpty())
		{
			State->CompletedCount++;
			continue;
		}

		// 关键：提取纯原文（避免重复翻译造成内容叠加）
		// 无论当前文本是否已包含译文，都提取纯原文
		FString CleanSourceText = StripExistingTranslation(SourceText);
		
		// 保存纯原文，用于后续的元数据存储
		State->OriginalTexts.Add(Key, CleanSourceText);

		// 如果是在编辑器中触发（静默模式），且已经有翻译内容，则认为是执行“还原”操作
		// 还原逻辑：直接将提取出的原文设置回去
		if (State->bSilent && HasTranslation(SourceText))
		{
			// 重新提取原文（确保正确处理隐藏标记）
			FString RestoredText = StripExistingTranslation(SourceText);
			
			// 修改 String Table（使用兼容性辅助函数）
			State->StringTable->Modify();
			LanguageOneStringTableHelper::SetStringTableEntry(State->StringTable, Key, RestoredText);
			
			// 清除元数据中的原文（重要！确保还原后 HasAssetTranslation 返回 false）
			static const FName OriginalTextMetaDataId = TEXT("LanguageOne_OriginalText");
			LanguageOneStringTableHelper::SetStringTableEntryMetaData(State->StringTable, Key, OriginalTextMetaDataId, FString());
			
			// 刷新 StringTable - 保持实时刷新功能不变！
			RefreshStringTableEditor(State->StringTable);
			
			State->CompletedCount++;
			State->SuccessCount++;
			
			UE_LOG(LogTemp, VeryVerbose, TEXT("Restored StringTable entry %d/%d: %s"), 
				State->CompletedCount, State->TotalCount, *Key);
			continue;
		}

		// 翻译文本
		FCommentTranslator::TranslateText(
			CleanSourceText,
			FOnTranslationComplete::CreateLambda([State, Key](const FString& TranslatedText)
			{
				if (!State->StringTable)
				{
					return;
				}
				
				FString OriginalText = State->OriginalTexts.FindRef(Key);
				FString NewText;
				static const FName OriginalTextMetaDataId = TEXT("LanguageOne_OriginalText");
				
				const ULanguageOneSettings* Settings = GetDefault<ULanguageOneSettings>();
				
				// 使用双语格式：根据设置决定译文和原文的位置
				// 格式：译文\n[原文]隐藏标记原文隐藏标记 或 原文\n[译文]隐藏标记原文隐藏标记
				// U+200B = Zero Width Space (ZWSP)
				// U+200C = Zero Width Non-Joiner (ZWNJ) - 开始标记
				// U+200D = Zero Width Joiner (ZWJ) - 结束标记
				const TCHAR HiddenStartMarker[] = { 0x200B, 0x200C, 0 }; // ZWSP + ZWNJ
				const TCHAR HiddenEndMarker[] = { 0x200B, 0x200D, 0 };   // ZWSP + ZWJ
				const FString HiddenStart(HiddenStartMarker);
				const FString HiddenEnd(HiddenEndMarker);
				
				if (Settings->bTranslationAboveOriginal)
				{
					// 译文在上方：译文\n---\n标记开始原文标记结束
					// 显示为：译文\n---\n原文（标记不可见，---作为明显分隔）
					NewText = FString::Printf(TEXT("%s\n---\n%s%s%s"), *TranslatedText, *HiddenStart, *OriginalText, *HiddenEnd);
				}
				else
				{
					// 译文在下方（默认）：标记开始原文标记结束\n---\n译文
					// 显示为：原文\n---\n译文（标记不可见，---作为明显分隔）
					NewText = FString::Printf(TEXT("%s%s%s\n---\n%s"), *HiddenStart, *OriginalText, *HiddenEnd, *TranslatedText);
				}
				
				// 同时将原文保存到元数据中（用于还原和清除操作）
				LanguageOneStringTableHelper::SetStringTableEntryMetaData(State->StringTable, Key, OriginalTextMetaDataId, OriginalText);

			// 修改 String Table（使用兼容性辅助函数）
			State->StringTable->Modify();
			LanguageOneStringTableHelper::SetStringTableEntry(State->StringTable, Key, NewText);
			
			// 刷新 StringTable - 使用安全的刷新方法（不传 Key，避免改变选中项）
			RefreshStringTableEditor(State->StringTable);
				
				State->CompletedCount++;
				State->SuccessCount++;
				
				UE_LOG(LogTemp, Log, TEXT("Translated StringTable entry %d/%d: %s"), 
					State->CompletedCount, State->TotalCount, *Key);
				
				// 如果是最后一个条目，只在日志输出，不显示弹窗
				if (State->CompletedCount >= State->TotalCount)
				{
					UE_LOG(LogTemp, Log, TEXT("StringTable translation completed: %d/%d success"), State->SuccessCount, State->TotalCount);
				}
			}),
			FOnTranslationError::CreateLambda([State, Key](const FString& ErrorMessage)
			{
				State->CompletedCount++;
				UE_LOG(LogTemp, Error, TEXT("Failed to translate StringTable entry '%s': %s"), *Key, *ErrorMessage);
				
				// 如果是最后一个条目，只在日志输出，不显示弹窗
				if (State->CompletedCount >= State->TotalCount)
				{
					UE_LOG(LogTemp, Log, TEXT("StringTable translation completed with errors: %d/%d success"), State->SuccessCount, State->TotalCount);
				}
			})
		);
	}
}

void FAssetTranslator::TranslateDataTable(UObject* Asset, bool bSilent)
{
	UDataTable* DataTable = Cast<UDataTable>(Asset);
	if (!DataTable)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Translating DataTable: %s"), *DataTable->GetName());

	// 获取所有行
	TArray<FName> RowNames = DataTable->GetRowNames();
	
	if (RowNames.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("DataTable is empty: %s"), *DataTable->GetName());
		return;
	}

	int32 TranslatedFieldCount = 0;

	// 遍历每一行
	for (const FName& RowName : RowNames)
	{
		uint8* RowData = DataTable->FindRowUnchecked(RowName);
		if (!RowData)
		{
			continue;
		}

		// 遍历结构体的所有属性
		const UScriptStruct* RowStruct = DataTable->GetRowStruct();
		if (!RowStruct)
		{
			continue;
		}

		for (TFieldIterator<FProperty> It(const_cast<UScriptStruct*>(RowStruct)); It; ++It)
		{
			FProperty* Property = *It;
			
			// 检查是否是文本属性
			if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
			{
				FText* TextValue = TextProperty->ContainerPtrToValuePtr<FText>(RowData);
				if (TextValue && !TextValue->IsEmpty())
				{
					FString SourceText = TextValue->ToString();

					// 提取原文（避免重复翻译）
					FString CleanSourceText = StripExistingTranslation(SourceText);
					
					// 如果是在编辑器中触发（静默模式），且已经有翻译内容，则认为是执行“还原”操作
					if (bSilent && HasTranslation(SourceText))
					{
						// 还原
						DataTable->Modify();
						*TextValue = FText::FromString(CleanSourceText);
						TranslatedFieldCount++;
						UE_LOG(LogTemp, Log, TEXT("Restored DataTable field: %s.%s"), *RowName.ToString(), *TextProperty->GetName());
						continue;
					}

					// 翻译文本
					FCommentTranslator::TranslateText(
						CleanSourceText,
						FOnTranslationComplete::CreateLambda([DataTable, RowName, TextProperty, RowData, CleanSourceText, &TranslatedFieldCount](const FString& TranslatedText)
						{
							const ULanguageOneSettings* Settings = GetDefault<ULanguageOneSettings>();
							const TCHAR HiddenStartMarker[] = { 0x200B, 0x200C, 0 }; // ZWSP + ZWNJ
							const TCHAR HiddenEndMarker[] = { 0x200B, 0x200D, 0 };   // ZWSP + ZWJ
							const FString HiddenStart(HiddenStartMarker);
							const FString HiddenEnd(HiddenEndMarker);
							
							FString NewText;
							if (Settings->bTranslationAboveOriginal)
							{
								// 译文在上方：译文\n---\n标记开始原文标记结束
								NewText = FString::Printf(TEXT("%s\n---\n%s%s%s"), *TranslatedText, *HiddenStart, *CleanSourceText, *HiddenEnd);
							}
							else
							{
								// 译文在下方（默认）：标记开始原文标记结束\n---\n译文
								NewText = FString::Printf(TEXT("%s%s%s\n---\n%s"), *HiddenStart, *CleanSourceText, *HiddenEnd, *TranslatedText);
							}

							// 修改 DataTable
							DataTable->Modify();
							FText* TextValue = TextProperty->ContainerPtrToValuePtr<FText>(RowData);
							*TextValue = FText::FromString(NewText);
							
							TranslatedFieldCount++;
							UE_LOG(LogTemp, Log, TEXT("Translated DataTable field: %s.%s"), *RowName.ToString(), *TextProperty->GetName());
						}),
						FOnTranslationError::CreateLambda([RowName](const FString& ErrorMessage)
						{
							UE_LOG(LogTemp, Error, TEXT("Failed to translate DataTable row %s: %s"), *RowName.ToString(), *ErrorMessage);
						})
					);
				}
			}
			else if (FStrProperty* StrProperty = CastField<FStrProperty>(Property))
			{
				// 如果属性名包含 "text", "desc", "content" 等关键字，也尝试翻译
				FString PropertyName = Property->GetName().ToLower();
				if (PropertyName.Contains(TEXT("text")) || 
					PropertyName.Contains(TEXT("desc")) || 
					PropertyName.Contains(TEXT("content")) ||
					PropertyName.Contains(TEXT("comment")) ||
					PropertyName.Contains(TEXT("tooltip")))
				{
					FString* StrValue = StrProperty->ContainerPtrToValuePtr<FString>(RowData);
					if (StrValue && !StrValue->IsEmpty())
					{
						FString SourceText = *StrValue;

						// 提取原文（避免重复翻译）
						FString CleanSourceText = StripExistingTranslation(SourceText);
						
						// 如果是在编辑器中触发（静默模式），且已经有翻译内容，则认为是执行“还原”操作
						if (bSilent && HasTranslation(SourceText))
						{
							// 还原
							DataTable->Modify();
							*StrValue = CleanSourceText;
							TranslatedFieldCount++;
							UE_LOG(LogTemp, Log, TEXT("Restored DataTable string field: %s.%s"), *RowName.ToString(), *StrProperty->GetName());
							continue;
						}

						// 翻译文本
							FCommentTranslator::TranslateText(
								CleanSourceText,
								FOnTranslationComplete::CreateLambda([DataTable, RowName, StrProperty, RowData, CleanSourceText, &TranslatedFieldCount](const FString& TranslatedText)
								{
									const ULanguageOneSettings* Settings = GetDefault<ULanguageOneSettings>();
									const TCHAR HiddenStartMarker[] = { 0x200B, 0x200C, 0 }; // ZWSP + ZWNJ
									const TCHAR HiddenEndMarker[] = { 0x200B, 0x200D, 0 };   // ZWSP + ZWJ
									const FString HiddenStart(HiddenStartMarker);
									const FString HiddenEnd(HiddenEndMarker);
									
									FString NewText;
									if (Settings->bTranslationAboveOriginal)
									{
										// 译文在上方：译文\n---\n标记开始原文标记结束
										NewText = FString::Printf(TEXT("%s\n---\n%s%s%s"), *TranslatedText, *HiddenStart, *CleanSourceText, *HiddenEnd);
									}
									else
									{
										// 译文在下方（默认）：标记开始原文标记结束\n---\n译文
										NewText = FString::Printf(TEXT("%s%s%s\n---\n%s"), *HiddenStart, *CleanSourceText, *HiddenEnd, *TranslatedText);
									}

								// 修改 DataTable
								DataTable->Modify();
								FString* StrValue = StrProperty->ContainerPtrToValuePtr<FString>(RowData);
								*StrValue = NewText;
								
								TranslatedFieldCount++;
								UE_LOG(LogTemp, Log, TEXT("Translated DataTable string field: %s.%s"), *RowName.ToString(), *StrProperty->GetName());
							}),
							FOnTranslationError::CreateLambda([RowName](const FString& ErrorMessage)
							{
								UE_LOG(LogTemp, Error, TEXT("Failed to translate DataTable row %s: %s"), *RowName.ToString(), *ErrorMessage);
							})
						);
					}
				}
			}
		}
	}

	if (TranslatedFieldCount > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("DataTable translation complete: %s (%d fields translated)"), *DataTable->GetName(), TranslatedFieldCount);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("No translatable text fields found in DataTable: %s"), *DataTable->GetName());
	}
}

void FAssetTranslator::TranslateWidgetBlueprint(UObject* Asset, bool bSilent)
{
	UBlueprint* Blueprint = Cast<UBlueprint>(Asset);
	if (!Blueprint)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Translating Widget Blueprint: %s"), *Blueprint->GetName());

	// 获取 Widget Tree
	UUserWidget* DefaultWidget = Cast<UUserWidget>(Blueprint->GeneratedClass->GetDefaultObject());
	if (!DefaultWidget || !DefaultWidget->WidgetTree)
	{
		UE_LOG(LogTemp, Warning, TEXT("Widget Blueprint has no widget tree: %s"), *Blueprint->GetName());
		return;
	}

	UWidgetTree* WidgetTree = DefaultWidget->WidgetTree;
	TArray<UWidget*> AllWidgets;
	WidgetTree->GetAllWidgets(AllWidgets);

	int32 TranslatedWidgetCount = 0;

	for (UWidget* Widget : AllWidgets)
	{
		if (!Widget)
		{
			continue;
		}

		// Text Block
		if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
		{
			FText SourceText = TextBlock->GetText();
			if (!SourceText.IsEmpty())
			{
				// 如果是在编辑器中触发（静默模式），且已经有翻译内容，则认为是执行“还原”操作
				if (bSilent && HasTranslation(SourceText.ToString()))
				{
					FString CleanText = StripExistingTranslation(SourceText.ToString());
					TextBlock->SetText(FText::FromString(CleanText));
					TranslatedWidgetCount++;
					continue;
				}

				TranslateSingleText(
					SourceText.ToString(),
					[TextBlock, &TranslatedWidgetCount](const FString& TranslatedText)
					{
						TextBlock->SetText(FText::FromString(TranslatedText));
						TranslatedWidgetCount++;
						UE_LOG(LogTemp, Log, TEXT("Translated TextBlock: %s"), *TextBlock->GetName());
					},
					[]()
					{
						UE_LOG(LogTemp, Warning, TEXT("Failed to translate TextBlock"));
					}
				);
			}
		}
		// Editable Text
		else if (UEditableText* EditableText = Cast<UEditableText>(Widget))
		{
			FText HintText = EditableText->GetHintText();
			if (!HintText.IsEmpty())
			{
				if (bSilent && HasTranslation(HintText.ToString()))
				{
					FString CleanText = StripExistingTranslation(HintText.ToString());
					EditableText->SetHintText(FText::FromString(CleanText));
					TranslatedWidgetCount++;
					continue;
				}

				TranslateSingleText(
					HintText.ToString(),
					[EditableText, &TranslatedWidgetCount](const FString& TranslatedText)
					{
						EditableText->SetHintText(FText::FromString(TranslatedText));
						TranslatedWidgetCount++;
					},
					[]() {}
				);
			}
		}
		// Editable Text Box
		else if (UEditableTextBox* EditableTextBox = Cast<UEditableTextBox>(Widget))
		{
			FText HintText = EditableTextBox->GetHintText();
			if (!HintText.IsEmpty())
			{
				if (bSilent && HasTranslation(HintText.ToString()))
				{
					FString CleanText = StripExistingTranslation(HintText.ToString());
					EditableTextBox->SetHintText(FText::FromString(CleanText));
					TranslatedWidgetCount++;
					continue;
				}

				TranslateSingleText(
					HintText.ToString(),
					[EditableTextBox, &TranslatedWidgetCount](const FString& TranslatedText)
					{
						EditableTextBox->SetHintText(FText::FromString(TranslatedText));
						TranslatedWidgetCount++;
					},
					[]() {}
				);
			}
		}
		// Rich Text Block
		else if (URichTextBlock* RichTextBlock = Cast<URichTextBlock>(Widget))
		{
			FText SourceText = RichTextBlock->GetText();
			if (!SourceText.IsEmpty())
			{
				if (bSilent && HasTranslation(SourceText.ToString()))
				{
					FString CleanText = StripExistingTranslation(SourceText.ToString());
					RichTextBlock->SetText(FText::FromString(CleanText));
					TranslatedWidgetCount++;
					continue;
				}

				TranslateSingleText(
					SourceText.ToString(),
					[RichTextBlock, &TranslatedWidgetCount](const FString& TranslatedText)
					{
						RichTextBlock->SetText(FText::FromString(TranslatedText));
						TranslatedWidgetCount++;
					},
					[]() {}
				);
			}
		}
	}

	if (TranslatedWidgetCount > 0)
	{
		Blueprint->Modify();
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		UE_LOG(LogTemp, Log, TEXT("Translated %d widgets in Widget Blueprint: %s"), TranslatedWidgetCount, *Blueprint->GetName());
	}
}

void FAssetTranslator::TranslateBlueprint(UObject* Asset, bool bSilent)
{
	UBlueprint* Blueprint = Cast<UBlueprint>(Asset);
	if (!Blueprint)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Translating Blueprint: %s"), *Blueprint->GetName());

	int32 TranslatedItemCount = 0;

	// 翻译变量描述
	// 注意：某些蓝图类型（如 ControlRig）可能有特殊的变量结构，需要安全检查
	for (FBPVariableDescription& Variable : Blueprint->NewVariables)
	{
		// 安全检查：确保变量有效
		if (Variable.VarName.IsNone())
		{
			continue;
		}
		
		// 翻译 Tooltip（使用兼容性辅助函数）
		FString TooltipValue = LanguageOneBlueprintHelper::GetVariableTooltip(Variable);
		if (!TooltipValue.IsEmpty())
		{
			if (bSilent && HasTranslation(TooltipValue))
			{
				FString CleanText = StripExistingTranslation(TooltipValue);
				LanguageOneBlueprintHelper::SetVariableTooltip(Variable, CleanText);
				TranslatedItemCount++;
				continue;
			}

			TranslateSingleText(
				TooltipValue,
				[&Variable, &TranslatedItemCount](const FString& TranslatedText)
				{
					LanguageOneBlueprintHelper::SetVariableTooltip(Variable, TranslatedText);
					TranslatedItemCount++;
					UE_LOG(LogTemp, Log, TEXT("Translated variable tooltip: %s"), *Variable.VarName.ToString());
				},
				[]() {}
			);
		}

		// 翻译 Category
		if (!Variable.Category.IsEmpty())
		{
			if (bSilent && HasTranslation(Variable.Category.ToString()))
			{
				FString CleanText = StripExistingTranslation(Variable.Category.ToString());
				Variable.Category = FText::FromString(CleanText);
				TranslatedItemCount++;
				continue;
			}

			TranslateSingleText(
				Variable.Category.ToString(),
				[&Variable, &TranslatedItemCount](const FString& TranslatedText)
				{
					Variable.Category = FText::FromString(TranslatedText);
					TranslatedItemCount++;
				},
				[]() {}
			);
		}
	}

	// 翻译函数图表
	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);

	for (UEdGraph* Graph : AllGraphs)
	{
		if (!Graph)
		{
			continue;
		}

		// 翻译图表中所有节点的注释
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && !Node->NodeComment.IsEmpty())
			{
				if (bSilent && HasTranslation(Node->NodeComment))
				{
					FString CleanText = StripExistingTranslation(Node->NodeComment);
					Node->Modify();
					Node->NodeComment = CleanText;
					TranslatedItemCount++;
					continue;
				}

				TranslateSingleText(
					Node->NodeComment,
					[Node, &TranslatedItemCount](const FString& TranslatedText)
					{
						Node->Modify();
						Node->NodeComment = TranslatedText;
						TranslatedItemCount++;
					},
					[]() {}
				);
			}

			// 翻译函数入口节点的描述
			if (UK2Node_FunctionEntry* FunctionEntry = Cast<UK2Node_FunctionEntry>(Node))
			{
				FString TooltipStr = FunctionEntry->GetTooltipText().ToString();
				if (!TooltipStr.IsEmpty())
				{
					if (bSilent && HasTranslation(TooltipStr))
					{
						FString CleanText = StripExistingTranslation(TooltipStr);
						FunctionEntry->Modify();
						// 函数的Tooltip存储在MetaData中
						if (FunctionEntry->MetaData.HasMetaData(FBlueprintMetadata::MD_Tooltip))
						{
							FunctionEntry->MetaData.SetMetaData(FBlueprintMetadata::MD_Tooltip, CleanText);
						}
						TranslatedItemCount++;
						continue;
					}

					TranslateSingleText(
						TooltipStr,
						[FunctionEntry, &TranslatedItemCount](const FString& TranslatedText)
						{
							FunctionEntry->Modify();
							// 函数的Tooltip存储在MetaData中
							if (FunctionEntry->MetaData.HasMetaData(FBlueprintMetadata::MD_Tooltip))
							{
								FunctionEntry->MetaData.SetMetaData(FBlueprintMetadata::MD_Tooltip, TranslatedText);
							}
							TranslatedItemCount++;
						},
						[]() {}
					);
				}
			}
		}
	}

	if (TranslatedItemCount > 0)
	{
		Blueprint->Modify();
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		UE_LOG(LogTemp, Log, TEXT("Translated %d items in Blueprint: %s"), TranslatedItemCount, *Blueprint->GetName());
	}
}

void FAssetTranslator::TranslateAssetMetadata(UObject* Asset, bool bSilent)
{
	if (!Asset)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Translating asset metadata: %s"), *Asset->GetName());

	// 获取资产的元数据
	UPackage* Package = Asset->GetOutermost();
	if (!Package)
	{
		return;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(Asset));
	
	// 翻译资产描述
	FString Description;
	if (AssetData.GetTagValue("Description", Description) && !Description.IsEmpty())
	{
		if (bSilent && HasTranslation(Description))
		{
			FString CleanText = StripExistingTranslation(Description);
			Asset->Modify();
			// 设置元数据 (需要通过资产的元数据系统)
			// 注意：这里可能需要特殊的 API 来设置元数据，具体取决于资产类型
			UE_LOG(LogTemp, Log, TEXT("Restored asset description: %s"), *Asset->GetName());
			return;
		}

		TranslateSingleText(
			Description,
			[Asset](const FString& TranslatedText)
			{
				Asset->Modify();
				// 设置元数据 (需要通过资产的元数据系统)
				UE_LOG(LogTemp, Log, TEXT("Translated asset description: %s"), *Asset->GetName());
			},
			[]() {}
		);
	}
}

void FAssetTranslator::TranslateSingleText(const FString& SourceText, TFunction<void(const FString&)> OnSuccess, TFunction<void()> OnError)
{
	if (SourceText.IsEmpty())
	{
		return;
	}

	// 提取原文（避免重复翻译）
	FString CleanSourceText = StripExistingTranslation(SourceText);

	FCommentTranslator::TranslateText(
		CleanSourceText,
		FOnTranslationComplete::CreateLambda([CleanSourceText, OnSuccess](const FString& TranslatedText)
		{
			const ULanguageOneSettings* Settings = GetDefault<ULanguageOneSettings>();
			const TCHAR HiddenStartMarker[] = { 0x200B, 0x200C, 0 }; // ZWSP + ZWNJ
			const TCHAR HiddenEndMarker[] = { 0x200B, 0x200D, 0 };   // ZWSP + ZWJ
			const FString HiddenStart(HiddenStartMarker);
			const FString HiddenEnd(HiddenEndMarker);
			
			FString NewText;
			if (Settings->bTranslationAboveOriginal)
			{
				// 译文在上方：译文\n---\n标记开始原文标记结束
				NewText = FString::Printf(TEXT("%s\n---\n%s%s%s"), *TranslatedText, *HiddenStart, *CleanSourceText, *HiddenEnd);
			}
			else
			{
				// 译文在下方（默认）：标记开始原文标记结束\n---\n译文
				NewText = FString::Printf(TEXT("%s%s%s\n---\n%s"), *HiddenStart, *CleanSourceText, *HiddenEnd, *TranslatedText);
			}

			OnSuccess(NewText);
		}),
		FOnTranslationError::CreateLambda([OnError](const FString& ErrorMessage)
		{
			OnError();
			UE_LOG(LogTemp, Error, TEXT("Translation failed: %s"), *ErrorMessage);
		})
	);
}

void FAssetTranslator::ShowTranslationProgress(int32 Current, int32 Total, const FString& AssetName)
{
	// 已移除通知弹窗，只在日志输出
	if (Current >= Total)
	{
		UE_LOG(LogTemp, Log, TEXT("Translation completed: %s (%d/%d)"), *AssetName, Current, Total);
	}
}

void FAssetTranslator::ShowTranslationComplete(int32 SuccessCount, int32 TotalCount)
{
	// 已移除通知弹窗，只在日志输出
	UE_LOG(LogTemp, Log, TEXT("All translations completed: %d/%d successful"), SuccessCount, TotalCount);
}

// 实现 GetDisplayText 函数（移除隐藏标记，只返回译文部分）
FString LanguageOneStringTableHelper::GetDisplayText(const FString& Text)
{
	if (Text.IsEmpty())
	{
		return Text;
	}
	
	// 检查是否有零宽字符隐藏标记（新格式）
	// 格式：标记原文标记\n---\n译文 或 译文\n---\n标记原文标记
	const TCHAR HiddenStartMarker[] = { 0x200B, 0x200C, 0 };
	const TCHAR HiddenEndMarker[] = { 0x200B, 0x200D, 0 };
	const FString HiddenStart(HiddenStartMarker);
	const FString HiddenEnd(HiddenEndMarker);
	
	int32 StartPos = Text.Find(HiddenStart, ESearchCase::CaseSensitive);
	if (StartPos != INDEX_NONE && StartPos >= 0)
	{
		int32 SearchStartPos = StartPos + HiddenStart.Len();
		int32 EndPos = Text.Find(HiddenEnd, ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchStartPos);
		
		if (EndPos != INDEX_NONE && EndPos > StartPos)
		{
			// 找到标记，判断译文位置
			// 检查是否有 --- 分隔符
			const FString Separator = TEXT("\n---\n");
			int32 SeparatorPos = Text.Find(Separator, ESearchCase::CaseSensitive);
			
			if (SeparatorPos != INDEX_NONE)
			{
				// 有分隔符，判断标记在哪边
				if (StartPos < SeparatorPos && EndPos < SeparatorPos)
				{
					// 标记在分隔符左边：标记原文标记\n---\n译文
					// 译文在分隔符右边
					FString AfterSeparator = Text.Mid(SeparatorPos + Separator.Len());
					return AfterSeparator.TrimStartAndEnd();
				}
				else if (StartPos > SeparatorPos)
				{
					// 标记在分隔符右边：译文\n---\n标记原文标记
					// 译文在分隔符左边
					FString BeforeSeparator = Text.Left(SeparatorPos);
					return BeforeSeparator.TrimStartAndEnd();
				}
			}
			
			// 没有分隔符，使用旧逻辑
			if (StartPos == 0)
			{
				// 标记在开头：标记原文标记\n译文
				int32 AfterMarker = EndPos + HiddenEnd.Len();
				if (AfterMarker < Text.Len())
				{
					FString AfterMarkerText = Text.Mid(AfterMarker);
					return AfterMarkerText.TrimStartAndEnd();
				}
			}
			else
			{
				// 标记不在开头：译文\n标记原文标记
				FString BeforeMarker = Text.Left(StartPos);
				return BeforeMarker.TrimStartAndEnd();
			}
		}
	}
	
	// 检查是否有可见分隔符（旧格式或没有标记）
	const FString Separator = TEXT("\n---\n");
	if (Text.Contains(Separator))
	{
		FString Left, Right;
		if (Text.Split(Separator, &Left, &Right))
		{
			// 假设右边是译文
			return Right.TrimStartAndEnd();
		}
	}
	
	// 没有标记，返回原文本
	return Text;
}

void FAssetTranslator::RestoreSelectedAssets(const TArray<FAssetData>& SelectedAssets)
{
	if (SelectedAssets.Num() == 0)
	{
		FAssetTranslatorUI::ShowInfoNotification(TEXT("请先选择要还原的资产 | Please select assets to restore"));
		return;
	}

	// 统计可还原资产
	TArray<FAssetData> RestorableAssets;
	for (const FAssetData& AssetData : SelectedAssets)
	{
		if (CanTranslateAsset(AssetData))
		{
			RestorableAssets.Add(AssetData);
		}
	}

	if (RestorableAssets.Num() == 0)
	{
		FAssetTranslatorUI::ShowInfoNotification(TEXT("所选资产不支持还原 | Selected assets cannot be restored"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Starting restore for %d assets"), RestorableAssets.Num());
	PerformRestore(RestorableAssets);
}

void FAssetTranslator::PerformRestore(const TArray<FAssetData>& TranslatableAssets)
{
	// 设置处理状态
	FAssetTranslatorUI::SetProcessing(true);
	
	// 使用工具窗口集成的进度组件
	TSharedPtr<STranslationProgressWindow> ProgressWidget = FAssetTranslatorUI::GetToolWindowProgress();
	
	// 创建还原状态追踪
	struct FRestoreState
	{
		int32 TotalAssets;
		int32 CompletedAssets;
		int32 SuccessAssets;
		int32 FailedAssets;
		TSharedPtr<STranslationProgressWindow> ProgressWidget;
		
		FRestoreState(int32 Total, TSharedPtr<STranslationProgressWindow> Widget)
			: TotalAssets(Total), CompletedAssets(0), SuccessAssets(0), FailedAssets(0), ProgressWidget(Widget)
		{}
	};
	
	TSharedPtr<FRestoreState> State = MakeShared<FRestoreState>(TranslatableAssets.Num(), ProgressWidget);
	
	// 还原每个资产（使用静默模式，会自动检测并还原）
	for (int32 i = 0; i < TranslatableAssets.Num(); i++)
	{
		const FAssetData& AssetData = TranslatableAssets[i];
		
		// 更新进度（先更新进度，确保即使失败也能看到进度变化）
		if (State->ProgressWidget.IsValid())
		{
			State->ProgressWidget->UpdateProgress(i + 1, AssetData.AssetName.ToString());
		}
		
		// 检查资产是否可以被加载
		UObject* Asset = AssetData.GetAsset();
		if (!Asset)
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to load asset %d/%d: %s"), 
				i + 1, TranslatableAssets.Num(), *AssetData.AssetName.ToString());
			State->CompletedAssets++;
			State->FailedAssets++;
			if (State->ProgressWidget.IsValid())
			{
				State->ProgressWidget->IncrementFailed();
			}
			continue;
		}
		
		// 检查资产类型是否支持
		if (!CanTranslateAsset(AssetData))
		{
			UE_LOG(LogTemp, Warning, TEXT("Unsupported asset type %d/%d: %s (Type: %s)"), 
				i + 1, TranslatableAssets.Num(), *AssetData.AssetName.ToString(), 
				*LanguageOneAssetDataHelper::GetAssetClassName(AssetData));
			State->CompletedAssets++;
			State->FailedAssets++;
			if (State->ProgressWidget.IsValid())
			{
				State->ProgressWidget->IncrementFailed();
			}
			continue;
		}

		UE_LOG(LogTemp, Log, TEXT("Restoring asset %d/%d: %s (Type: %s)"), 
			i + 1, TranslatableAssets.Num(), *AssetData.AssetName.ToString(), 
			*LanguageOneAssetDataHelper::GetAssetClassName(AssetData));

		// 根据资产类型调用相应的还原函数（使用静默模式，会自动还原）
		FString ClassName = LanguageOneAssetDataHelper::GetAssetClassName(AssetData);
		
		if (ClassName.Contains(TEXT("StringTable")))
		{
			TranslateStringTable(Asset, true); // 静默模式会自动还原
		}
		else if (ClassName.Contains(TEXT("DataTable")))
		{
			TranslateDataTable(Asset, true);
		}
		else if (ClassName.Contains(TEXT("WidgetBlueprint")) || ClassName.Contains(TEXT("UserWidget")))
		{
			TranslateWidgetBlueprint(Asset, true);
		}
		else if (ClassName.Contains(TEXT("Blueprint")))
		{
			TranslateBlueprint(Asset, true);
		}
		else
		{
			TranslateAssetMetadata(Asset, true);
		}
		
		// 标记为成功
		State->SuccessAssets++;
		State->CompletedAssets++;
		if (State->ProgressWidget.IsValid())
		{
			State->ProgressWidget->IncrementSuccess();
		}
	}
	
	// 标记完成
	if (State->ProgressWidget.IsValid())
	{
		State->ProgressWidget->MarkComplete(State->SuccessAssets, State->FailedAssets);
	}
	
	// 恢复处理状态
	FAssetTranslatorUI::SetProcessing(false);
	
	// 不再自动关闭窗口，让用户手动关闭
	// if (State->ProgressWidget.IsValid())
	// {
	// 	FTimerHandle TimerHandle;
	// 	GEditor->GetTimerManager()->SetTimer(TimerHandle, []()
	// 	{
	// 		FAssetTranslatorUI::CloseTranslationProgress();
	// 	}, 3.0f, false);
	// }
	
	// 只在日志输出，不显示弹窗
	UE_LOG(LogTemp, Log, TEXT("Asset restore completed: %d/%d success"), State->SuccessAssets, State->TotalAssets);
}

// 辅助函数：从文本中提取译文（移除原文部分）
static FString ExtractTranslationOnly(const FString& Text)
{
	if (Text.IsEmpty())
	{
		return Text;
	}
	
	// 检查是否有零宽字符隐藏标记（新格式）
	// 格式：标记原文标记\n---\n译文 或 译文\n---\n标记原文标记
	const TCHAR HiddenStartMarker[] = { 0x200B, 0x200C, 0 }; // ZWSP + ZWNJ
	const TCHAR HiddenEndMarker[] = { 0x200B, 0x200D, 0 };   // ZWSP + ZWJ
	const FString HiddenStart(HiddenStartMarker);
	const FString HiddenEnd(HiddenEndMarker);
	
	int32 StartPos = Text.Find(HiddenStart, ESearchCase::CaseSensitive);
	if (StartPos != INDEX_NONE && StartPos >= 0)
	{
		int32 SearchStartPos = StartPos + HiddenStart.Len();
		int32 EndPos = Text.Find(HiddenEnd, ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchStartPos);
		
		if (EndPos != INDEX_NONE && EndPos > StartPos)
		{
			// 找到标记，判断译文位置
			// 新格式：标记原文标记\n---\n译文 或 译文\n---\n标记原文标记
			const FString Separator = TEXT("\n---\n");
			int32 SeparatorPos = Text.Find(Separator, ESearchCase::CaseSensitive);
			
			if (SeparatorPos != INDEX_NONE)
			{
				// 有分隔符，判断标记在哪边
				if (StartPos < SeparatorPos && EndPos < SeparatorPos)
				{
					// 标记在分隔符左边：标记原文标记\n---\n译文
					// 译文在分隔符右边
					FString AfterSeparator = Text.Mid(SeparatorPos + Separator.Len());
					return AfterSeparator.TrimStartAndEnd();
				}
				else if (StartPos > SeparatorPos)
				{
					// 标记在分隔符右边：译文\n---\n标记原文标记
					// 译文在分隔符左边
					FString BeforeSeparator = Text.Left(SeparatorPos);
					return BeforeSeparator.TrimStartAndEnd();
				}
			}
			
			// 没有分隔符，使用旧逻辑
			if (StartPos == 0)
			{
				// 标记在开头：标记原文标记\n译文
				int32 AfterMarker = EndPos + HiddenEnd.Len();
				if (AfterMarker < Text.Len())
				{
					FString AfterMarkerText = Text.Mid(AfterMarker);
					return AfterMarkerText.TrimStartAndEnd();
				}
			}
			else
			{
				// 标记不在开头：译文\n标记原文标记
				FString BeforeMarker = Text.Left(StartPos);
				return BeforeMarker.TrimStartAndEnd();
			}
		}
	}
	
	// 检查是否有可见分隔符（旧格式或没有标记）
	const FString Separator = TEXT("\n---\n");
	if (Text.Contains(Separator))
	{
		FString Left, Right;
		if (Text.Split(Separator, &Left, &Right))
		{
			// 假设右边是译文（新格式）
			return Right.TrimStartAndEnd();
		}
	}
	
	// 没有标记，返回原文本
	return Text;
}

// 辅助函数：检查文本是否处于双语模式（有原文标记）
static bool IsBilingualMode(const FString& Text)
{
	if (Text.IsEmpty())
	{
		return false;
	}
	
	// 检查是否有零宽字符隐藏标记和 --- 分隔符（新格式）
	const TCHAR HiddenStartMarker[] = { 0x200B, 0x200C, 0 }; // ZWSP + ZWNJ
	const TCHAR HiddenEndMarker[] = { 0x200B, 0x200D, 0 };   // ZWSP + ZWJ
	const FString HiddenStart(HiddenStartMarker);
	const FString HiddenEnd(HiddenEndMarker);
	
	// 新格式：有隐藏标记和 --- 分隔符
	if (Text.Contains(HiddenStart) && Text.Contains(HiddenEnd) && Text.Contains(TEXT("\n---\n")))
	{
		return true;
	}
	
	// 兼容旧格式：只有 --- 分隔符
	if (Text.Contains(TEXT("\n---\n")))
	{
		return true;
	}
	
	return false;
}

void FAssetTranslator::ClearOriginalText(const TArray<FAssetData>& SelectedAssets)
{
	if (SelectedAssets.Num() == 0)
	{
		FAssetTranslatorUI::ShowInfoNotification(TEXT("请先选择要清除原文的资产 | Please select assets to clear original text"));
		return;
	}

	// 统计可清除原文的资产
	TArray<FAssetData> ClearableAssets;
	for (const FAssetData& AssetData : SelectedAssets)
	{
		if (CanTranslateAsset(AssetData))
		{
			ClearableAssets.Add(AssetData);
		}
	}

	if (ClearableAssets.Num() == 0)
	{
		FAssetTranslatorUI::ShowInfoNotification(TEXT("所选资产不支持清除原文 | Selected assets cannot clear original text"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Starting clear original text for %d assets"), ClearableAssets.Num());
	
	// 显示确认对话框
	FAssetTranslatorUI::ShowClearOriginalConfirmDialog(
		ClearableAssets,
		[ClearableAssets]() // OnConfirm
		{
			PerformClearOriginal(ClearableAssets);
		},
		[]() // OnCancel
		{
			UE_LOG(LogTemp, Log, TEXT("Clear original text cancelled by user"));
		}
	);
}

void FAssetTranslator::PerformClearOriginal(const TArray<FAssetData>& TranslatableAssets)
{
	// 设置处理状态
	FAssetTranslatorUI::SetProcessing(true);
	
	// 使用工具窗口集成的进度组件
	TSharedPtr<STranslationProgressWindow> ProgressWidget = FAssetTranslatorUI::GetToolWindowProgress();
	
	// 创建清除状态追踪
	struct FClearState
	{
		int32 TotalAssets;
		int32 CompletedAssets;
		int32 SuccessAssets;
		int32 FailedAssets;
		TSharedPtr<STranslationProgressWindow> ProgressWidget;
		
		FClearState(int32 Total, TSharedPtr<STranslationProgressWindow> Widget)
			: TotalAssets(Total), CompletedAssets(0), SuccessAssets(0), FailedAssets(0), ProgressWidget(Widget)
		{}
	};
	
	TSharedPtr<FClearState> State = MakeShared<FClearState>(TranslatableAssets.Num(), ProgressWidget);
	
	// 清除每个资产的原文
	for (int32 i = 0; i < TranslatableAssets.Num(); i++)
	{
		const FAssetData& AssetData = TranslatableAssets[i];
		
		// 更新进度（先更新进度，确保即使失败也能看到进度变化）
		if (State->ProgressWidget.IsValid())
		{
			State->ProgressWidget->UpdateProgress(i + 1, AssetData.AssetName.ToString());
		}
		
		// 检查资产是否可以被加载
		UObject* Asset = AssetData.GetAsset();
		if (!Asset)
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to load asset %d/%d: %s"), 
				i + 1, TranslatableAssets.Num(), *AssetData.AssetName.ToString());
			State->CompletedAssets++;
			State->FailedAssets++;
			if (State->ProgressWidget.IsValid())
			{
				State->ProgressWidget->IncrementFailed();
			}
			continue;
		}
		
		// 检查资产类型是否支持
		if (!CanTranslateAsset(AssetData))
		{
			UE_LOG(LogTemp, Warning, TEXT("Unsupported asset type %d/%d: %s (Type: %s)"), 
				i + 1, TranslatableAssets.Num(), *AssetData.AssetName.ToString(), 
				*LanguageOneAssetDataHelper::GetAssetClassName(AssetData));
			State->CompletedAssets++;
			State->FailedAssets++;
			if (State->ProgressWidget.IsValid())
			{
				State->ProgressWidget->IncrementFailed();
			}
			continue;
		}

		UE_LOG(LogTemp, Log, TEXT("Clearing original text for asset %d/%d: %s (Type: %s)"), 
			i + 1, TranslatableAssets.Num(), *AssetData.AssetName.ToString(), 
			*LanguageOneAssetDataHelper::GetAssetClassName(AssetData));

		// 根据资产类型调用相应的清除函数
		FString ClassName = LanguageOneAssetDataHelper::GetAssetClassName(AssetData);

		if (ClassName.Contains(TEXT("StringTable")))
		{
			UStringTable* StringTable = Cast<UStringTable>(Asset);
			if (StringTable)
			{
				FStringTableConstRef StringTableData = StringTable->GetStringTable();
				TArray<FString> Keys;
				LanguageOneStringTableHelper::EnumerateStringTableKeys(StringTableData, Keys);
				
				StringTable->Modify();
				for (const FString& Key : Keys)
				{
					FString CurrentText = LanguageOneStringTableHelper::FindStringTableEntry(StringTableData, Key);
					if (HasTranslation(CurrentText))
					{
						FString TranslationOnly = ExtractTranslationOnly(CurrentText);
						LanguageOneStringTableHelper::SetStringTableEntry(StringTable, Key, TranslationOnly);
						// 清除元数据中的原文
						static const FName OriginalTextMetaDataId = TEXT("LanguageOne_OriginalText");
						LanguageOneStringTableHelper::SetStringTableEntryMetaData(StringTable, Key, OriginalTextMetaDataId, FString());
					}
				}
				RefreshStringTableEditor(StringTable);
			}
		}
		else if (ClassName.Contains(TEXT("DataTable")))
		{
			UDataTable* DataTable = Cast<UDataTable>(Asset);
			if (DataTable)
			{
				DataTable->Modify();
				TArray<FName> RowNames = DataTable->GetRowNames();
				const UScriptStruct* RowStruct = DataTable->GetRowStruct();
				if (RowStruct)
				{
					for (const FName& RowName : RowNames)
					{
						uint8* RowData = DataTable->FindRowUnchecked(RowName);
						if (RowData)
						{
							for (TFieldIterator<FProperty> It(const_cast<UScriptStruct*>(RowStruct)); It; ++It)
							{
								FProperty* Property = *It;
								if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
								{
									FText* TextValue = TextProperty->ContainerPtrToValuePtr<FText>(RowData);
									if (TextValue && HasTranslation(TextValue->ToString()))
									{
										FString TranslationOnly = ExtractTranslationOnly(TextValue->ToString());
										*TextValue = FText::FromString(TranslationOnly);
									}
								}
								else if (FStrProperty* StrProperty = CastField<FStrProperty>(Property))
								{
									FString* StrValue = StrProperty->ContainerPtrToValuePtr<FString>(RowData);
									if (StrValue && HasTranslation(*StrValue))
									{
										FString TranslationOnly = ExtractTranslationOnly(*StrValue);
										*StrValue = TranslationOnly;
									}
								}
							}
						}
					}
				}
			}
		}
		// 其他资产类型的清除逻辑类似...
		
		// 标记为成功
		State->SuccessAssets++;
		State->CompletedAssets++;
		if (State->ProgressWidget.IsValid())
		{
			State->ProgressWidget->IncrementSuccess();
		}
	}
	
	// 标记完成
	if (State->ProgressWidget.IsValid())
	{
		State->ProgressWidget->MarkComplete(State->SuccessAssets, State->FailedAssets);
	}
	
	// 恢复处理状态
	FAssetTranslatorUI::SetProcessing(false);
	
	// 不再自动关闭窗口，让用户手动关闭
	// if (State->ProgressWidget.IsValid())
	// {
	// 	FTimerHandle TimerHandle;
	// 	GEditor->GetTimerManager()->SetTimer(TimerHandle, []()
	// 	{
	// 		FAssetTranslatorUI::CloseTranslationProgress();
	// 	}, 3.0f, false);
	// }
	
	UE_LOG(LogTemp, Log, TEXT("Clear original text completed: %d/%d success"), State->SuccessAssets, State->TotalAssets);
}

void FAssetTranslator::ToggleDisplayMode(const TArray<FAssetData>& SelectedAssets)
{
	if (SelectedAssets.Num() == 0)
	{
		FAssetTranslatorUI::ShowInfoNotification(TEXT("请先选择要切换显示模式的资产 | Please select assets to toggle display mode"));
		return;
	}

	// 统计可切换的资产
	TArray<FAssetData> ToggleableAssets;
	for (const FAssetData& AssetData : SelectedAssets)
	{
		if (CanTranslateAsset(AssetData))
		{
			ToggleableAssets.Add(AssetData);
		}
	}

	if (ToggleableAssets.Num() == 0)
	{
		FAssetTranslatorUI::ShowInfoNotification(TEXT("所选资产不支持切换显示模式 | Selected assets cannot toggle display mode"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Starting toggle display mode for %d assets"), ToggleableAssets.Num());
	PerformToggleDisplayMode(ToggleableAssets);
}

void FAssetTranslator::PerformToggleDisplayMode(const TArray<FAssetData>& TranslatableAssets)
{
	// 设置处理状态
	FAssetTranslatorUI::SetProcessing(true);
	
	// 使用工具窗口集成的进度组件
	TSharedPtr<STranslationProgressWindow> ProgressWidget = FAssetTranslatorUI::GetToolWindowProgress();
	
	// 切换每个资产的显示模式
	for (int32 i = 0; i < TranslatableAssets.Num(); i++)
	{
		const FAssetData& AssetData = TranslatableAssets[i];
		
		// 更新进度
		if (ProgressWidget.IsValid())
		{
			ProgressWidget->UpdateProgress(i + 1, AssetData.AssetName.ToString());
		}
		
		UObject* Asset = AssetData.GetAsset();
		if (!Asset)
		{
			if (ProgressWidget.IsValid())
			{
				ProgressWidget->IncrementFailed();
			}
			continue;
		}

		FString ClassName = LanguageOneAssetDataHelper::GetAssetClassName(AssetData);
		
		if (ClassName.Contains(TEXT("StringTable")))
		{
			UStringTable* StringTable = Cast<UStringTable>(Asset);
			if (StringTable)
			{
				FStringTableConstRef StringTableData = StringTable->GetStringTable();
				TArray<FString> Keys;
				LanguageOneStringTableHelper::EnumerateStringTableKeys(StringTableData, Keys);

				StringTable->Modify();
				for (const FString& Key : Keys)
				{
					FString CurrentText = LanguageOneStringTableHelper::FindStringTableEntry(StringTableData, Key);
					if (HasTranslation(CurrentText))
					{
						FString NewText;
						if (IsBilingualMode(CurrentText))
						{
							// 切换到原文模式：只显示原文
							NewText = StripExistingTranslation(CurrentText);
						}
						else
						{
							// 切换到双语模式：显示原文\n译文 或 译文\n原文
							// 从元数据中获取原文（最可靠）
							static const FName OriginalTextMetaDataId = TEXT("LanguageOne_OriginalText");
							FString OriginalText = LanguageOneStringTableHelper::GetStringTableEntryMetaData(StringTableData, Key, OriginalTextMetaDataId);
							
							// 如果元数据中没有原文，尝试从当前文本提取
							if (OriginalText.IsEmpty())
							{
								OriginalText = StripExistingTranslation(CurrentText);
							}
							
							// 当前文本就是译文（因为是从原文模式切换过来的）
							FString TranslationText = CurrentText;
							
							const TCHAR HiddenStartMarker[] = { 0x200B, 0x200C, 0 };
							const TCHAR HiddenEndMarker[] = { 0x200B, 0x200D, 0 };
							const FString HiddenStart(HiddenStartMarker);
							const FString HiddenEnd(HiddenEndMarker);
							
							const ULanguageOneSettings* Settings = GetDefault<ULanguageOneSettings>();
							if (Settings->bTranslationAboveOriginal)
							{
								// 译文在上方：译文\n---\n标记开始原文标记结束
								NewText = FString::Printf(TEXT("%s\n---\n%s%s%s"), *TranslationText, *HiddenStart, *OriginalText, *HiddenEnd);
							}
							else
							{
								// 译文在下方（默认）：标记开始原文标记结束\n---\n译文
								NewText = FString::Printf(TEXT("%s%s%s\n---\n%s"), *HiddenStart, *OriginalText, *HiddenEnd, *TranslationText);
							}
							
							// 恢复元数据
							LanguageOneStringTableHelper::SetStringTableEntryMetaData(StringTable, Key, OriginalTextMetaDataId, OriginalText);
						}
						LanguageOneStringTableHelper::SetStringTableEntry(StringTable, Key, NewText);
					}
				}
				RefreshStringTableEditor(StringTable);  // 保持实时刷新功能不变！
			}
		}
		// 其他资产类型的切换逻辑类似...
		
		// 标记为已翻译切换
		if (ProgressWidget.IsValid())
		{
			ProgressWidget->IncrementTranslated();
		}
	}
	
	// 标记完成
	if (ProgressWidget.IsValid())
	{
		ProgressWidget->MarkComplete(0, 0);  // 切换操作没有成功/失败，只有已翻译计数
	}
	
	// 恢复处理状态
	FAssetTranslatorUI::SetProcessing(false);

	UE_LOG(LogTemp, Log, TEXT("Toggle display mode completed: %d assets"), TranslatableAssets.Num());
}
