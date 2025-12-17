// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LanguageOne : ModuleRules
{
	public LanguageOne(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		// 版本兼容性处理 (UE 4.26 - 5.7)
		// 使用预处理指令确保代码在 UE 4.26/4.27 下能通过编译（这些版本没有 IncludeOrderVersion 属性）
#if UE_5_0_OR_LATER
		// UE 5.0+ 使用 Latest 以获得最佳编译速度和未来版本兼容性 (包括 5.6, 5.7)
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
#endif

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
		
		// UE 5.0+ 需要显式添加 EditorFramework
		if (Target.Version.MajorVersion >= 5)
		{
			PrivateDependencyModuleNames.Add("EditorFramework");
		}
	}
}
