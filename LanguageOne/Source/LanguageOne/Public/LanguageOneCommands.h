// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "LanguageOneStyle.h"
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FLanguageOneCommands : public TCommands<FLanguageOneCommands>
{
public:

	FLanguageOneCommands()
		: TCommands<FLanguageOneCommands>(
			TEXT("LanguageOne"),
			NSLOCTEXT("Contexts", "LanguageOne", "LanguageOne Plugin"),
			NAME_None,
			FLanguageOneStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > PluginAction;
	TSharedPtr< FUICommandInfo > TranslateCommentAction;
};
