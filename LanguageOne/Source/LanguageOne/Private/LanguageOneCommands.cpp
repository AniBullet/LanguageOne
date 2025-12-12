// Copyright Epic Games, Inc. All Rights Reserved.

#include "LanguageOneCommands.h"

#define LOCTEXT_NAMESPACE "FLanguageOneModule"


void FLanguageOneCommands::RegisterCommands()
{
    UI_COMMAND(PluginAction, "Switch Language", "Switch between Chinese and English", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Alt, EKeys::Q));
    UI_COMMAND(TranslateCommentAction, "Translate Comment", "Translate selected blueprint node comments", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::T));
}

#undef LOCTEXT_NAMESPACE
