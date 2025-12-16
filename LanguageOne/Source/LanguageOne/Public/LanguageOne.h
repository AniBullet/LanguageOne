// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FToolBarBuilder;
class FMenuBuilder;

class FLanguageOneModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	/** This function will be bound to Command. */
	void PluginButtonClicked();
	
	/** Public functions for global input processor */
	void SwitchLanguage();
	void TranslateComment();
	
private:

	void RegisterMenus();
	void RegisterContentBrowserExtensions();
	void TranslateSelectedNodes();
	void TranslateCurrentAsset();
	bool TranslateActiveAssetEditor();
	bool IsNodeTranslated(UEdGraphNode* Node);
	void RestoreNodeComment(UEdGraphNode* Node);
	
	/** Content Browser asset context menu handler */
	void OnExtendContentBrowserAssetSelectionMenu(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets);

private:
	TSharedPtr<class FUICommandList> PluginCommands;
	TSharedPtr<class FLanguageOneInputProcessor> InputProcessor;
	
	// 存储已翻译节点的原始注释，用于还原
	TMap<TWeakObjectPtr<UEdGraphNode>, FString> OriginalComments;
};
