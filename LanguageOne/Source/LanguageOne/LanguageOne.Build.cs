// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LanguageOne : ModuleRules
{
	public LanguageOne(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		// 版本兼容性处理 (UE 4.26 - 5.7+)
		if (Target.Version.MajorVersion >= 5)
		{
			// UE 5.x 根据版本设置 include order
			// 避免使用已过时的 Unreal5_5
			if (Target.Version.MajorVersion == 5 && Target.Version.MinorVersion >= 6)
			{
				// UE 5.6+ 使用 Latest
				IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
			}
			else if (Target.Version.MajorVersion == 5 && Target.Version.MinorVersion >= 4)
			{
				// UE 5.4-5.5 使用 Unreal5_4（避免过时的 Unreal5_5）
				IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_4;
			}
			else
			{
				// UE 5.0-5.3 使用 Latest
				IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
			}
		}

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				"InputCore",
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
				"JsonUtilities",
				"AssetRegistry",
				"ContentBrowser",
				"UMG",
				"UMGEditor",
				"StringTableEditor"
			}
		);
		
		// EditorFramework only exists in UE5+
		if (Target.Version.MajorVersion >= 5)
		{
			PrivateDependencyModuleNames.Add("EditorFramework");
		}
	}
}


