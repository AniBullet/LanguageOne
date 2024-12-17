// Copyright Epic Games, Inc. All Rights Reserved.

#include "LanguageOneCommands.h"

#define LOCTEXT_NAMESPACE "FLanguageOneModule"


void FLanguageOneCommands::RegisterCommands()
{
    UI_COMMAND(PluginAction, "Switch Language", "Switch between Chinese and English", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Alt, EKeys::Q));
}

#undef LOCTEXT_NAMESPACE
