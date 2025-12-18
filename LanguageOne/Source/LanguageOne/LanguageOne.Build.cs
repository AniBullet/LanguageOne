// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LanguageOne : ModuleRules
{
	public LanguageOne(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
	// 版本兼容性处理 (UE 5.1 及后续版本)
	// IncludeOrderVersion 属性从 UE 5.2 开始引入，5.1 不存在此属性。
	// 逻辑：如果是 5.2+，则设置为 Latest；如果是 5.1，则什么都不做。
	if (Target.Version.MajorVersion > 5 || (Target.Version.MajorVersion == 5 && Target.Version.MinorVersion >= 2))
		{
			try
			{
				// 获取 UnrealBuildTool 程序集
				System.Reflection.Assembly BuildToolAssembly = typeof(ModuleRules).Assembly;
				
				// 获取 EngineIncludeOrderVersion 枚举类型
				System.Type EnumType = BuildToolAssembly.GetType("UnrealBuildTool.EngineIncludeOrderVersion");
				
				// 查找 IncludeOrderVersion 属性或字段
				System.Reflection.PropertyInfo Prop = GetType().GetProperty("IncludeOrderVersion");
				System.Reflection.FieldInfo Field = GetType().GetField("IncludeOrderVersion");
				
				if (EnumType != null)
				{
					object LatestValue = System.Enum.Parse(EnumType, "Latest");
					
					if (Prop != null)
					{
						Prop.SetValue(this, LatestValue);
					}
					else if (Field != null)
					{
						Field.SetValue(this, LatestValue);
					}
				}
			}
			catch (System.Exception)
			{
				// 忽略反射异常，使用默认值
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
		
		// UE 5.0+ 需要显式添加 EditorFramework
		if (Target.Version.MajorVersion >= 5)
		{
			PrivateDependencyModuleNames.Add("EditorFramework");
		}
	}
}
