// Copyright Epic Games, Inc. All Rights Reserved.

#include "LanguageOneCommands.h"
#include "LanguageOneStyle.h"

#define LOCTEXT_NAMESPACE "FLanguageOneModule"


void FLanguageOneCommands::RegisterCommands()
{
	UI_COMMAND(PluginAction, "Switch Language", "Switch between editor languages (Alt+Q)", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Alt, EKeys::Q));
	UI_COMMAND(TranslateCommentAction, "Translate Comment", "Translate blueprint node comments (Ctrl+T)", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::T));
}

#undef LOCTEXT_NAMESPACE
