// Copyright Epic Games, Inc. All Rights Reserved.

#include "LanguageOne.h"
#include "LanguageOneStyle.h"
#include "LanguageOneCommands.h"
#include "LanguageOneSettings.h"
#include "LanguageOneCompatibility.h"
#include "CommentTranslator.h"
#include "AssetTranslator.h"
#include "AssetTranslatorUI.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Misc/MessageDialog.h"
#include "ToolMenus.h"
#include "LevelEditor.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandList.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/IInputProcessor.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraphNode_Comment.h"
#include "SGraphPanel.h"
#include "GraphEditor.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Input/Events.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Subsystems/AssetEditorSubsystem.h"

static const FName LanguageOneTabName("LanguageOne");

#define LOCTEXT_NAMESPACE "FLanguageOneModule"

// 前向声明
FString GetLanguageCode(EEditorLanguage Language);

// 全局快捷键处理器
class FLanguageOneInputProcessor : public IInputProcessor
{
public:
	FLanguageOneInputProcessor(FLanguageOneModule* InModule)
		: Module(InModule)
	{
	}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override
	{
	}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		// Alt + Q - 切换语言
		if (InKeyEvent.GetKey() == EKeys::Q && InKeyEvent.IsAltDown() && !InKeyEvent.IsControlDown() && !InKeyEvent.IsShiftDown())
		{
			if (Module)
			{
				Module->SwitchLanguage();
			}
			return true;
		}

		// Alt + E - 翻译蓝图节点注释
		if (InKeyEvent.GetKey() == EKeys::E && InKeyEvent.IsAltDown() && !InKeyEvent.IsControlDown() && !InKeyEvent.IsShiftDown())
		{
			if (Module)
			{
				Module->TranslateComment();
			}
			return true;
		}

		return false;
	}

private:
	FLanguageOneModule* Module;
};

void FLanguageOneModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FLanguageOneStyle::Initialize();
	FLanguageOneStyle::ReloadTextures();

	FLanguageOneCommands::Register();

	UE_LOG(LogTemp, Log, TEXT("LanguageOne module starting up"));
	UE_LOG(LogTemp, Log, TEXT("LanguageOne commands registered"));

	PluginCommands = MakeShareable(new FUICommandList);
	
	// 注册语言切换命令
	PluginCommands->MapAction(
		FLanguageOneCommands::Get().PluginAction,
		FExecuteAction::CreateRaw(this, &FLanguageOneModule::SwitchLanguage),
		FCanExecuteAction());
	UE_LOG(LogTemp, Log, TEXT("Mapped PluginAction (Switch Language): Alt+Q"));
	
	// 注册翻译蓝图注释命令
	PluginCommands->MapAction(
		FLanguageOneCommands::Get().TranslateCommentAction,
		FExecuteAction::CreateRaw(this, &FLanguageOneModule::TranslateComment),
		FCanExecuteAction());
	UE_LOG(LogTemp, Log, TEXT("Mapped TranslateCommentAction (Translate Comment): Alt+E"));

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FLanguageOneModule::RegisterMenus));
	
	// 注册 Content Browser 扩展
	RegisterContentBrowserExtensions();

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		"LanguageOne",
		FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&) { return SNew(SDockTab); })
	)
		.SetDisplayName(LOCTEXT("LanguageOneTab", "Switch Language"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	// 注册全局快捷键处理器（在所有编辑器中都能工作）
	if (FSlateApplication::IsInitialized())
	{
		InputProcessor = MakeShareable(new FLanguageOneInputProcessor(this));
		FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor, 0);
		UE_LOG(LogTemp, Log, TEXT("LanguageOne global input processor registered (Alt+Q, Alt+E)"));
	}

	// 同时也注册到 LevelEditor 的命令列表（用于菜单）
	if (FModuleManager::Get().IsModuleLoaded("LevelEditor"))
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditorModule.GetGlobalLevelEditorActions()->Append(PluginCommands.ToSharedRef());
		UE_LOG(LogTemp, Log, TEXT("LanguageOne commands registered to LevelEditor for menus"));
	}

	// Register settings
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings("Editor", "Plugins", "LanguageOne",
			LOCTEXT("LanguageOneSettingsName", "LanguageOne"),
			LOCTEXT("LanguageOneSettingsDescription", "Configure LanguageOne plugin settings"),
			GetMutableDefault<ULanguageOneSettings>()
		);
	}

	// 初始化当前语言显示
	ULanguageOneSettings* Settings = GetMutableDefault<ULanguageOneSettings>();
	if (Settings)
	{
		FString CurrentCulture = FInternationalization::Get().GetCurrentCulture()->GetName();
		Settings->CurrentEditorLanguage = CurrentCulture;
		
		UE_LOG(LogTemp, Log, TEXT("Current=%s, LanguageA=%s, LanguageB=%s"), 
			*CurrentCulture, *GetLanguageCode(Settings->SourceEditorLanguage), *GetLanguageCode(Settings->TargetEditorLanguage));
	}
}

void FLanguageOneModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	// 注销全局快捷键处理器
	if (FSlateApplication::IsInitialized() && InputProcessor.IsValid())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
		UE_LOG(LogTemp, Log, TEXT("LanguageOne global input processor unregistered"));
	}

	// Unregister settings
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Editor", "Plugins", "LanguageOne");
	}

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FLanguageOneStyle::Shutdown();

	FLanguageOneCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner("LanguageOne");
}

void FLanguageOneModule::RegisterMenus()
{
	UE_LOG(LogTemp, Log, TEXT("LanguageOne registering menus"));

	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FLanguageOneCommands::Get().PluginAction, PluginCommands);
			UE_LOG(LogTemp, Log, TEXT("Added PluginAction to Window menu"));
		}
	}

	// 主编辑器工具栏 - 版本兼容
	{
#if ENGINE_MAJOR_VERSION >= 5
		// UE5+ 使用细分的工具栏
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
#else
		// UE 4.26-4.27 使用统一的工具栏
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar");
#endif
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("LanguageOneTools");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FLanguageOneCommands::Get().PluginAction));
				Entry.SetCommandList(PluginCommands);
				UE_LOG(LogTemp, Log, TEXT("Added PluginAction button to LevelEditor toolbar"));
			}
		}
	}

	// 资源编辑器工具栏
	{
		// UE 5.1+ 显式指定图标
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("AssetEditorToolbar.CommonActions");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("LanguageOneTools");
			{
				FToolMenuEntry LanguageEntry = FToolMenuEntry::InitToolBarButton(
					FLanguageOneCommands::Get().PluginAction,
					LOCTEXT("SwitchLanguage", "Switch Language"),
					LOCTEXT("SwitchLanguageTooltip", "Switch editor language (Alt+Q)"),
					FSlateIcon(FLanguageOneStyle::GetStyleSetName(), "LanguageOne.PluginAction")
				);
				LanguageEntry.SetCommandList(PluginCommands);
				Section.AddEntry(LanguageEntry);
				UE_LOG(LogTemp, Log, TEXT("Added PluginAction button to AssetEditor toolbar"));
			}
			{
				FToolMenuEntry TranslateEntry = FToolMenuEntry::InitToolBarButton(
					FLanguageOneCommands::Get().TranslateCommentAction,
					LOCTEXT("TranslateComment", "Translate Comment"),
					LOCTEXT("TranslateCommentTooltip", "Translate blueprint node comments (Alt+E)"),
					FSlateIcon(FLanguageOneStyle::GetStyleSetName(), "LanguageOne.TranslateCommentAction")
				);
				TranslateEntry.SetCommandList(PluginCommands);
				Section.AddEntry(TranslateEntry);
				UE_LOG(LogTemp, Log, TEXT("Added TranslateCommentAction button to AssetEditor toolbar"));
			}
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("LanguageOne menus registered successfully"));
}

void FLanguageOneModule::RegisterContentBrowserExtensions()
{
	UE_LOG(LogTemp, Log, TEXT("LanguageOne registering Content Browser extensions"));

	// 注册 Content Browser 资产菜单扩展
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	
	TArray<FContentBrowserMenuExtender_SelectedAssets>& MenuExtenders = ContentBrowserModule.GetAllAssetViewContextMenuExtenders();
	
	MenuExtenders.Add(FContentBrowserMenuExtender_SelectedAssets::CreateLambda(
		[this](const TArray<FAssetData>& SelectedAssets)
		{
			TSharedRef<FExtender> Extender = MakeShareable(new FExtender());
			
			// 检查是否有可翻译的资产
			bool bHasTranslatableAssets = false;
			for (const FAssetData& AssetData : SelectedAssets)
			{
				if (FAssetTranslator::CanTranslateAsset(AssetData))
				{
					bHasTranslatableAssets = true;
					break;
				}
			}
			
			if (bHasTranslatableAssets)
			{
				Extender->AddMenuExtension(
					"CommonAssetActions",
					EExtensionHook::After,
					nullptr,
					FMenuExtensionDelegate::CreateLambda([this, SelectedAssets](FMenuBuilder& MenuBuilder)
					{
						this->OnExtendContentBrowserAssetSelectionMenu(MenuBuilder, SelectedAssets);
					})
				);
			}
			
			return Extender;
		}
	));
	
	UE_LOG(LogTemp, Log, TEXT("LanguageOne Content Browser extensions registered"));
}

void FLanguageOneModule::OnExtendContentBrowserAssetSelectionMenu(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets)
{
	MenuBuilder.BeginSection("LanguageOneAssetActions", FText::FromString(TEXT("LanguageOne 翻译 | Translation")));
	{
		// 添加资产翻译菜单项
		MenuBuilder.AddMenuEntry(
			FText::FromString(TEXT("资产翻译 | Asset Translation")),
			FText::FromString(TEXT("打开资产翻译窗口，可选择翻译或还原\nOpen asset translation window, choose to translate or restore\n\n支持/Supported:\nString Table, Data Table, Widget Blueprint, Blueprint, Material, Texture")),
			FSlateIcon(FLanguageOneStyle::GetStyleSetName(), "LanguageOne.TranslateCommentAction"),
			FUIAction(
				FExecuteAction::CreateLambda([SelectedAssets]()
				{
					FAssetTranslatorUI::ShowAssetTranslationTool(SelectedAssets);
				}),
				FCanExecuteAction::CreateLambda([SelectedAssets]()
				{
					// 检查是否有可翻译的资产
					for (const FAssetData& AssetData : SelectedAssets)
					{
						if (FAssetTranslator::CanTranslateAsset(AssetData))
						{
							return true;
						}
					}
					return false;
				})
			)
		);
	}
	MenuBuilder.EndSection();
}

void FLanguageOneModule::TranslateCurrentAsset()
{
	UE_LOG(LogTemp, Log, TEXT("TranslateCurrentAsset called"));
	
	// 获取当前编辑的资产
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AssetEditorSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to get AssetEditorSubsystem"));
		return;
	}
	
	TArray<UObject*> EditedAssets = AssetEditorSubsystem->GetAllEditedAssets();
	if (EditedAssets.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("No assets currently being edited"));
		
		FNotificationInfo Info(FText::FromString(TEXT("没有打开的资产 | No assets are currently open")));
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
	}
	
	// 转换为 FAssetData 数组
	TArray<FAssetData> AssetsToTranslate;
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	
	for (UObject* Asset : EditedAssets)
	{
		if (Asset)
		{
			FAssetData AssetData = LanguageOneAssetDataHelper::GetAssetByObjectPath(AssetRegistry, FSoftObjectPath(Asset));
			if (AssetData.IsValid())
			{
				AssetsToTranslate.Add(AssetData);
			}
		}
	}
	
	if (AssetsToTranslate.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Found %d assets to translate"), AssetsToTranslate.Num());
		FAssetTranslator::TranslateSelectedAssets(AssetsToTranslate);
	}
}

FString GetLanguageCode(EEditorLanguage Language)
{
	switch (Language)
	{
	case EEditorLanguage::English: return TEXT("en");
	case EEditorLanguage::ChineseSimplified: return TEXT("zh-CN");
	case EEditorLanguage::ChineseTraditional: return TEXT("zh-TW");
	case EEditorLanguage::Japanese: return TEXT("ja");
	case EEditorLanguage::Korean: return TEXT("ko");
	case EEditorLanguage::German: return TEXT("de");
	case EEditorLanguage::French: return TEXT("fr");
	case EEditorLanguage::Spanish: return TEXT("es");
	case EEditorLanguage::Russian: return TEXT("ru");
	case EEditorLanguage::Portuguese: return TEXT("pt");
	case EEditorLanguage::Italian: return TEXT("it");
	default: return TEXT("en");
	}
}

void FLanguageOneModule::SwitchLanguage()
{
	UE_LOG(LogTemp, Log, TEXT("SwitchLanguage called"));

	ULanguageOneSettings* Settings = GetMutableDefault<ULanguageOneSettings>();
	if (!Settings)
	{
		return;
	}

	FString CurrentCulture = FInternationalization::Get().GetCurrentCulture()->GetName();
	FString LanguageACode = GetLanguageCode(Settings->SourceEditorLanguage);
	FString LanguageBCode = GetLanguageCode(Settings->TargetEditorLanguage);
	FString NewCulture;

	// 检查 A 和 B 是否相同
	if (LanguageACode == LanguageBCode)
	{
		FNotificationInfo Info(FText::FromString(
			FString::Printf(TEXT("⚠️ 语言A和语言B相同！\nLanguage A and B are the same!\n\n请在设置中修改\nPlease change in settings"))
		));
		Info.ExpireDuration = 5.0f;
		Info.bUseSuccessFailIcons = true;
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
	}

	// 简单的 A/B 切换逻辑
	// 1. 如果当前是 A (或 A 的变体)，切换到 B
	// 2. 如果当前是 B (或 B 的变体)，切换到 A
	// 3. 既不是 A 也不是 B，切换到 A（默认行为）

	bool bIsA = CurrentCulture.StartsWith(LanguageACode);
	bool bIsB = CurrentCulture.StartsWith(LanguageBCode);

	if (bIsA)
	{
		// 当前是 A -> 切换到 B
		NewCulture = LanguageBCode;
		UE_LOG(LogTemp, Log, TEXT("Switching from Language A (%s) to Language B (%s)"), *CurrentCulture, *NewCulture);
	}
	else if (bIsB)
	{
		// 当前是 B -> 切换到 A
		NewCulture = LanguageACode;
		UE_LOG(LogTemp, Log, TEXT("Switching from Language B (%s) to Language A (%s)"), *CurrentCulture, *NewCulture);
	}
	else
	{
		// 既不是 A 也不是 B -> 切换到 A
		NewCulture = LanguageACode;
		UE_LOG(LogTemp, Log, TEXT("Current (%s) matches neither A nor B. Switching to Language A (%s)"), *CurrentCulture, *NewCulture);
	}

	// 切换语言
	FInternationalization::Get().SetCurrentCulture(NewCulture);

	// 更新当前语言显示
	Settings->CurrentEditorLanguage = NewCulture;
	Settings->SaveConfig();

	// Refresh the UI
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().InvalidateAllWidgets(true);
	}

	UE_LOG(LogTemp, Log, TEXT("Language switched to: %s"), *NewCulture);
}

void FLanguageOneModule::TranslateComment()
{
	UE_LOG(LogTemp, Log, TEXT("TranslateComment called"));
	
	// 优先尝试翻译当前活动的资产编辑器（如 StringTable, DataTable）
	if (TranslateActiveAssetEditor())
	{
		return;
	}

	// 专注于蓝图节点注释的翻译/还原
	// 资产翻译通过 Content Browser 右键菜单进行
	TranslateSelectedNodes();
}

bool FLanguageOneModule::TranslateActiveAssetEditor()
{
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}

	TSharedPtr<SWidget> FocusedWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();
	if (!FocusedWidget.IsValid())
	{
		return false;
	}

	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AssetEditorSubsystem)
	{
		return false;
	}

	TArray<UObject*> EditedAssets = AssetEditorSubsystem->GetAllEditedAssets();
	TSet<FAssetEditorToolkit*> CheckedToolkits;

	for (UObject* Asset : EditedAssets)
	{
		IAssetEditorInstance* EditorInstance = AssetEditorSubsystem->FindEditorForAsset(Asset, false);
		if (!EditorInstance)
		{
			continue;
		}

		// Check if this editor owns the focused widget
		// Most editors inherit from FAssetEditorToolkit
		FAssetEditorToolkit* Toolkit = static_cast<FAssetEditorToolkit*>(EditorInstance);
		if (CheckedToolkits.Contains(Toolkit))
		{
			continue;
		}
		CheckedToolkits.Add(Toolkit);

		TSharedPtr<FTabManager> TabManager = Toolkit->GetTabManager();
		if (TabManager.IsValid())
		{
			// 检查焦点 Widget 是否在该编辑器的 Widget 树中
			TSharedPtr<SDockTab> OwnerTab = TabManager->GetOwnerTab();
            
			bool bIsDescendant = false;
			if (OwnerTab.IsValid())
			{
				TSharedPtr<SWidget> ParentContent = OwnerTab->GetContent();
				TSharedPtr<SWidget> CurrentWidget = FocusedWidget;
				
				// 手动检查父子关系，因为 SWidget::IsDescendantOf 不是公开 API
				while (CurrentWidget.IsValid())
				{
					if (CurrentWidget == ParentContent)
					{
						bIsDescendant = true;
						break;
					}
					CurrentWidget = CurrentWidget->GetParentWidget();
				}
			}

			if (bIsDescendant)
			{
				UE_LOG(LogTemp, Log, TEXT("Found active asset editor for: %s"), *Asset->GetName());
				
				TArray<FAssetData> AssetsToTranslate;
				FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
				IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
				
				// Iterate all edited assets to find those belonging to this editor instance
				// This avoids using GetEditingObjects() which might be protected or unavailable in IAssetEditorInstance
				for (UObject* EditedAsset : EditedAssets)
				{
					if (AssetEditorSubsystem->FindEditorForAsset(EditedAsset, false) == EditorInstance)
					{
						FAssetData AssetData = LanguageOneAssetDataHelper::GetAssetByObjectPath(AssetRegistry, FSoftObjectPath(EditedAsset));
						if (AssetData.IsValid())
						{
							AssetsToTranslate.Add(AssetData);
						}
					}
				}

				if (AssetsToTranslate.Num() > 0)
				{
					// 编辑器中触发，开启静默模式，开启Toggle逻辑(自动判断还原)
					// 注意：目前 TranslateSelectedAssets 内部没有实现 Toggle 逻辑，
					// 但我们传 true 进去作为 bIsFromEditor 标志，
					// 在 TranslateSelectedAssets 内部我们可以根据此标志决定行为
					FAssetTranslator::TranslateSelectedAssets(AssetsToTranslate, true);
					return true;
				}
			}
		}
	}

	return false;
}

void FLanguageOneModule::TranslateSelectedNodes()
{
	// 收集所有可能的选中节点（使用多种方法）
	TSet<UEdGraphNode*> NodesToTranslate;
	bool bTranslatingAllNodes = false;  // 标记是翻译全部还是选中的
	
	UE_LOG(LogTemp, Log, TEXT("TranslateSelectedNodes: Starting node detection..."));
	
	// 方法1: 尝试从当前焦点的 GraphPanel 获取
	if (FSlateApplication::IsInitialized())
	{
		TSharedPtr<SWidget> FocusedWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();
		if (FocusedWidget.IsValid())
		{
			UE_LOG(LogTemp, Log, TEXT("Found focused widget: %s"), *FocusedWidget->GetTypeAsString());
			
			TSharedPtr<SWidget> CurrentWidget = FocusedWidget;
			const int32 MaxDepth = 50; // 增加深度
			int32 Depth = 0;
			
			while (CurrentWidget.IsValid() && Depth < MaxDepth)
			{
				FName WidgetType = CurrentWidget->GetType();
				FString WidgetTypeName = WidgetType.ToString();
				
				if (WidgetTypeName.Contains(TEXT("GraphPanel")) || WidgetType == FName("SGraphPanel"))
				{
					TSharedPtr<SGraphPanel> GraphPanel = StaticCastSharedPtr<SGraphPanel, SWidget>(CurrentWidget);
					if (GraphPanel.IsValid())
					{
						FGraphPanelSelectionSet SelectedNodes = GraphPanel->SelectionManager.GetSelectedNodes();
						
						if (SelectedNodes.Num() > 0)
						{
							// 有选中的节点，只翻译选中的
							UE_LOG(LogTemp, Log, TEXT("Found GraphPanel with %d selected nodes"), SelectedNodes.Num());
							
							for (UObject* Obj : SelectedNodes)
							{
								if (UEdGraphNode* Node = Cast<UEdGraphNode>(Obj))
								{
									NodesToTranslate.Add(Node);
								}
							}
						}
						else
						{
							// 没有选中节点，翻译当前图表的所有节点
							bTranslatingAllNodes = true;
							UEdGraph* Graph = GraphPanel->GetGraphObj();
							if (Graph)
							{
								UE_LOG(LogTemp, Log, TEXT("No selection, translating all nodes in graph: %s"), *Graph->GetName());
								
								for (UEdGraphNode* Node : Graph->Nodes)
								{
									if (Node && !Node->NodeComment.IsEmpty())
									{
										NodesToTranslate.Add(Node);
									}
								}
								
								UE_LOG(LogTemp, Log, TEXT("Found %d nodes with comments in graph"), NodesToTranslate.Num());
							}
						}
						break;
					}
				}
				
				CurrentWidget = CurrentWidget->GetParentWidget();
				Depth++;
			}
			
			UE_LOG(LogTemp, Log, TEXT("Widget tree traversal depth: %d"), Depth);
		}
	}
	
	// 方法2: 尝试从所有打开的窗口查找 GraphEditor
	if (NodesToTranslate.Num() == 0 && FSlateApplication::IsInitialized())
	{
		UE_LOG(LogTemp, Log, TEXT("Method 1 failed, trying method 2..."));
		
		TArray<TSharedRef<SWindow>> Windows;
		FSlateApplication::Get().GetAllVisibleWindowsOrdered(Windows);
		
		for (TSharedRef<SWindow> Window : Windows)
		{
			TSharedRef<const SWidget> WindowContent = Window->GetContent();
			
			// 递归查找 GraphPanel
			TArray<TSharedRef<const SWidget>> WidgetsToCheck;
			WidgetsToCheck.Add(WindowContent);
			
			while (WidgetsToCheck.Num() > 0 && NodesToTranslate.Num() == 0)
			{
				TSharedRef<const SWidget> Widget = WidgetsToCheck.Pop();
				FName WidgetType = Widget->GetType();
				FString WidgetTypeName = WidgetType.ToString();
				
				if (WidgetTypeName.Contains(TEXT("GraphPanel")) || WidgetType == FName("SGraphPanel"))
				{
					// 移除 const 限定符以便调用非 const 方法
					TSharedRef<SWidget> MutableWidget = ConstCastSharedRef<SWidget>(Widget);
					TSharedRef<SGraphPanel> GraphPanelRef = StaticCastSharedRef<SGraphPanel>(MutableWidget);
					TSharedPtr<SGraphPanel> GraphPanel = GraphPanelRef;
					if (GraphPanel.IsValid())
					{
						FGraphPanelSelectionSet SelectedNodes = GraphPanel->SelectionManager.GetSelectedNodes();
						
						if (SelectedNodes.Num() > 0)
						{
							// 有选中的节点
							UE_LOG(LogTemp, Log, TEXT("Found GraphPanel with %d selected nodes"), SelectedNodes.Num());
							
							for (UObject* Obj : SelectedNodes)
							{
								if (UEdGraphNode* Node = Cast<UEdGraphNode>(Obj))
								{
									NodesToTranslate.Add(Node);
								}
							}
						}
						else
						{
							// 没有选中，翻译整个图表
							bTranslatingAllNodes = true;
							UEdGraph* Graph = GraphPanel->GetGraphObj();
							if (Graph)
							{
								UE_LOG(LogTemp, Log, TEXT("No selection in method 2, translating all nodes in graph"));
								
								for (UEdGraphNode* Node : Graph->Nodes)
								{
									if (Node && !Node->NodeComment.IsEmpty())
									{
										NodesToTranslate.Add(Node);
									}
								}
							}
						}
						break;
					}
				}
				
				// 添加子 Widget 到检查列表（需要移除 const）
				TSharedRef<SWidget> MutableWidgetForChildren = ConstCastSharedRef<SWidget>(Widget);
				FChildren* Children = MutableWidgetForChildren->GetChildren();
				if (Children)
				{
					for (int32 i = 0; i < Children->Num(); ++i)
					{
						WidgetsToCheck.Add(Children->GetChildAt(i));
					}
				}
			}
		}
	}
	
	if (bTranslatingAllNodes)
	{
		UE_LOG(LogTemp, Log, TEXT("Translating ALL nodes in graph: Total %d nodes with comments"), NodesToTranslate.Num());
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("Translating SELECTED nodes: Total %d nodes"), NodesToTranslate.Num());
	}
	
	// 分类节点：已翻译的和未翻译的
	TArray<UEdGraphNode*> TranslatedNodes;
	TArray<UEdGraphNode*> UntranslatedNodes;
	
	for (UEdGraphNode* Node : NodesToTranslate)
	{
		if (Node && !Node->NodeComment.IsEmpty())
		{
			if (IsNodeTranslated(Node))
			{
				TranslatedNodes.Add(Node);
				UE_LOG(LogTemp, Log, TEXT("Already translated node: %s"), *Node->GetName());
			}
			else
			{
				UntranslatedNodes.Add(Node);
				UE_LOG(LogTemp, Log, TEXT("Node to translate: %s, Comment: %s"), *Node->GetName(), *Node->NodeComment.Left(50));
			}
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("Translated nodes: %d, Untranslated nodes: %d"), TranslatedNodes.Num(), UntranslatedNodes.Num());
	
	// 如果有已翻译的节点，优先还原
	if (TranslatedNodes.Num() > 0)
	{
		for (UEdGraphNode* Node : TranslatedNodes)
		{
			RestoreNodeComment(Node);
		}
		
		UE_LOG(LogTemp, Log, TEXT("Restored %d node comments"), TranslatedNodes.Num());
		return;
	}
	
	// 如果没有找到有注释的节点
	if (UntranslatedNodes.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("No comments to translate. Total nodes: %d"), NodesToTranslate.Num());
		return;
	}

	// 开始翻译未翻译的节点
	int32 TotalCount = UntranslatedNodes.Num();
	int32 CurrentIndex = 0;

	for (UEdGraphNode* Node : UntranslatedNodes)
	{
		FString NodeComment = Node->NodeComment;
		
		// 保存原始注释
		OriginalComments.Add(TWeakObjectPtr<UEdGraphNode>(Node), NodeComment);
		
		CurrentIndex++;
		int32 IndexCapture = CurrentIndex; // 按值捕获索引用于日志
		UE_LOG(LogTemp, Log, TEXT("Translating comment %d/%d"), IndexCapture, TotalCount);

		// 使用 TWeakObjectPtr 安全地捕获节点指针
		TWeakObjectPtr<UEdGraphNode> WeakNode(Node);
		FCommentTranslator::TranslateText(
			NodeComment,
			FOnTranslationComplete::CreateLambda([WeakNode, NodeComment, IndexCapture](const FString& TranslatedText)
					{
						// 检查节点是否仍然有效
						if (!WeakNode.IsValid())
						{
							UE_LOG(LogTemp, Warning, TEXT("Node was destroyed before translation completed"));
							return;
						}
						
					UEdGraphNode* NodeToModify = WeakNode.Get();
					const ULanguageOneSettings* Settings = GetDefault<ULanguageOneSettings>();
					FString NewComment;
					
					const TCHAR HiddenStartMarker[] = { 0x200B, 0x200C, 0 }; // ZWSP + ZWNJ
					const TCHAR HiddenEndMarker[] = { 0x200B, 0x200D, 0 };   // ZWSP + ZWJ
					const FString HiddenStart(HiddenStartMarker);
					const FString HiddenEnd(HiddenEndMarker);
					
					if (Settings->bTranslationAboveOriginal)
					{
						// 译文在上方：译文\n---\n标记开始原文标记结束
						NewComment = FString::Printf(TEXT("%s\n---\n%s%s%s"), *TranslatedText, *HiddenStart, *NodeComment, *HiddenEnd);
					}
					else
					{
						// 译文在下方（默认）：标记开始原文标记结束\n---\n译文
						NewComment = FString::Printf(TEXT("%s%s%s\n---\n%s"), *HiddenStart, *NodeComment, *HiddenEnd, *TranslatedText);
					}

						NodeToModify->Modify();
						NodeToModify->NodeComment = NewComment;
						NodeToModify->GetGraph()->NotifyGraphChanged();

						UE_LOG(LogTemp, Log, TEXT("Comment %d translated successfully"), IndexCapture);
					}),
					FOnTranslationError::CreateLambda([IndexCapture](const FString& ErrorMessage)
					{
						FNotificationInfo Info(FText::FromString(FString::Printf(TEXT("翻译失败 | Translation failed: %s"), *ErrorMessage)));
						Info.ExpireDuration = 5.0f;
						FSlateNotificationManager::Get().AddNotification(Info);

						UE_LOG(LogTemp, Error, TEXT("Translation %d failed: %s"), IndexCapture, *ErrorMessage);
					})
				);
	}
}

bool FLanguageOneModule::IsNodeTranslated(UEdGraphNode* Node)
{
	if (!Node)
	{
		return false;
	}
	
	TWeakObjectPtr<UEdGraphNode> WeakNode(Node);
	return OriginalComments.Contains(WeakNode);
}

void FLanguageOneModule::RestoreNodeComment(UEdGraphNode* Node)
{
	if (!Node)
	{
		return;
	}
	
	TWeakObjectPtr<UEdGraphNode> WeakNode(Node);
	if (FString* OriginalComment = OriginalComments.Find(WeakNode))
	{
		Node->Modify();
		Node->NodeComment = *OriginalComment;
		Node->GetGraph()->NotifyGraphChanged();
		
		// 从映射中移除
		OriginalComments.Remove(WeakNode);
		
		UE_LOG(LogTemp, Log, TEXT("Restored comment for node: %s"), *Node->GetName());
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FLanguageOneModule, LanguageOne)