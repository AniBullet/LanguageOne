// Copyright Epic Games, Inc. All Rights Reserved.

#include "LanguageOneCommands.h"
#include "LanguageOneStyle.h"

#define LOCTEXT_NAMESPACE "FLanguageOneModule"


void FLanguageOneCommands::RegisterCommands()
{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
	// UE 5.1+ 使用默认方式（图标通过 Style 自动查找）
	UI_COMMAND(PluginAction, "Switch Language", "Switch between editor languages (Alt+Q)", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Alt, EKeys::Q));
	UI_COMMAND(TranslateCommentAction, "Translate Comment", "Translate blueprint node comments (Ctrl+T)", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::T));
#else
	// UE 4.x ~ 5.0 显式指定图标（避免自动查找失败）
	UI_COMMAND(PluginAction, "Switch Language", "Switch between editor languages (Alt+Q)", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Alt, EKeys::Q));
	if (PluginAction.IsValid())
	{
		PluginAction->SetIcon(FSlateIcon(FLanguageOneStyle::GetStyleSetName(), "LanguageOne.PluginAction"));
	}
	
	UI_COMMAND(TranslateCommentAction, "Translate Comment", "Translate blueprint node comments (Ctrl+T)", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::T));
	if (TranslateCommentAction.IsValid())
	{
		TranslateCommentAction->SetIcon(FSlateIcon(FLanguageOneStyle::GetStyleSetName(), "LanguageOne.TranslateCommentAction"));
	}
#endif
	
	UE_LOG(LogTemp, Log, TEXT("LanguageOne commands registered with icons"));
}

#undef LOCTEXT_NAMESPACE
