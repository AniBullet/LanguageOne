// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LanguageOne : ModuleRules
{
	public LanguageOne(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				"InputCore",
				"EditorFramework",
				"UnrealEd",
				"ToolMenus",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
                "LevelEditor",
                "EditorStyle",
                "ApplicationCore",
                "AppFramework",
				"BlueprintGraph",
				"Kismet",
				"GraphEditor",
				"HTTP",
				"Json",
				"JsonUtilities"
				// ... add private dependencies that you statically link with here ...	
			}
			);
	}
}
