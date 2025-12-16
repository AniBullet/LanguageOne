/*
 * @Description: 
 * @Author: Bullet.S
 * @Date: 2025-12-12 20:45:47
 * @LastEditors: Bullet.S
 * @LastEditTime: 2025-12-15 23:01:11
 * @Email: animator.bullet@foxmail.com
 */
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * 版本兼容性处理 - Version Compatibility Layer
 * 
 * 本文件统一处理不同 UE 版本的 API 差异，确保插件在 UE 4.26 ~ 5.7+ 上通用。
 * This file handles API differences across UE versions, ensuring the plugin works on UE 4.26 ~ 5.7+.
 * 
 * 使用方法 - Usage:
 * 1. 在需要的 .cpp 文件中包含此头文件
 * 2. 使用 LANGUAGEONE_XXX 宏代替版本特定的 API
 * 
 * 一个包通用所有版本 - One package for all versions
 */

// ========== 版本检测宏 ==========
#ifndef ENGINE_PATCH_VERSION
	#define ENGINE_PATCH_VERSION 0
#endif

// 两参数版本（只比较 Major.Minor）
#define UE_VERSION_OLDER_THAN(Major, Minor, ...) UE_VERSION_OLDER_THAN_IMPL(Major, Minor, ##__VA_ARGS__, 0)
#define UE_VERSION_NEWER_THAN(Major, Minor, ...) UE_VERSION_NEWER_THAN_IMPL(Major, Minor, ##__VA_ARGS__, 0)

// 内部实现（支持 Major.Minor.Patch）
#define UE_VERSION_OLDER_THAN_IMPL(Major, Minor, Patch, ...) \
	(ENGINE_MAJOR_VERSION < Major || \
	(ENGINE_MAJOR_VERSION == Major && ENGINE_MINOR_VERSION < Minor) || \
	(ENGINE_MAJOR_VERSION == Major && ENGINE_MINOR_VERSION == Minor && ENGINE_PATCH_VERSION < Patch))

#define UE_VERSION_NEWER_THAN_IMPL(Major, Minor, Patch, ...) \
	(ENGINE_MAJOR_VERSION > Major || \
	(ENGINE_MAJOR_VERSION == Major && ENGINE_MINOR_VERSION > Minor) || \
	(ENGINE_MAJOR_VERSION == Major && ENGINE_MINOR_VERSION == Minor && ENGINE_PATCH_VERSION >= Patch))

// ========== URL 编码兼容 ==========
// 所有版本统一使用 FGenericPlatformHttp::UrlEncode
#include "GenericPlatform/GenericPlatformHttp.h"
#define LANGUAGEONE_URL_ENCODE(Text) FGenericPlatformHttp::UrlEncode(Text)

// ========== Slate 样式兼容 ==========
// UE5.1+ 使用 FAppStyle
// UE5.0 及以下使用 FEditorStyle
#if UE_VERSION_NEWER_THAN(5, 1)
	#include "Styling/AppStyle.h"
	#define LANGUAGEONE_EDITOR_STYLE FAppStyle
#else
	#include "EditorStyle.h"
	#define LANGUAGEONE_EDITOR_STYLE FEditorStyle
#endif

// ========== StringTable API 兼容 ==========
// UE 不同版本的 StringTable API 可能有差异
#include "Internationalization/StringTable.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/Text.h"

// 前向声明
class ULanguageOneSettings;

// StringTable 辅助函数 - 跨版本兼容
namespace LanguageOneStringTableHelper
{
	// 枚举 StringTable 的所有键
	inline void EnumerateStringTableKeys(const FStringTableConstRef& StringTableData, TArray<FString>& OutKeys)
	{
		StringTableData->EnumerateKeysAndSourceStrings([&OutKeys](const FTextKey& InKey, const FString& InSourceString) -> bool
		{
			// FTextKey 兼容性处理: 使用 ToString()
			OutKeys.Add(InKey.ToString());
			return true; // 继续枚举
		});
	}
	
	// 查找 StringTable 条目
	inline FString FindStringTableEntry(const FStringTableConstRef& StringTableData, const FString& Key)
	{
#if UE_VERSION_NEWER_THAN(5, 0)
		// UE 5.0+ 返回 FStringTableEntryConstPtr (TSharedPtr<const FStringTableEntry>)
		auto Entry = StringTableData->FindEntry(FTextKey(Key));
		if (Entry.IsValid())
		{
			return Entry->GetSourceString();
		}
#else
		// UE 4.x 返回 FTextDisplayStringPtr (TSharedPtr<FString>)
		FTextDisplayStringPtr SourceText = StringTableData->FindEntry(FTextKey(Key));
		if (SourceText.IsValid())
		{
			return *SourceText;
		}
#endif
		return FString();
	}
	
	// 设置 StringTable 条目
	inline void SetStringTableEntry(UStringTable* StringTable, const FString& Key, const FString& Value)
	{
		if (StringTable)
		{
			FStringTableRef MutableData = StringTable->GetMutableStringTable();
			MutableData->SetSourceString(FTextKey(Key), Value);
		}
	}
	
	// 设置 StringTable 条目的元数据（用于存储原文，不显示在 UI 中）
	inline void SetStringTableEntryMetaData(UStringTable* StringTable, const FString& Key, const FName& MetaDataId, const FString& MetaDataValue)
	{
		if (StringTable)
		{
			FStringTableRef MutableData = StringTable->GetMutableStringTable();
			MutableData->SetMetaData(FTextKey(Key), MetaDataId, MetaDataValue);
		}
	}
	
	// 获取 StringTable 条目的元数据
	inline FString GetStringTableEntryMetaData(const FStringTableConstRef& StringTableData, const FString& Key, const FName& MetaDataId)
	{
		return StringTableData->GetMetaData(FTextKey(Key), MetaDataId);
	}
	
	// 获取显示文本（移除隐藏标记，只返回译文部分）
	// 用于在游戏 UI 中显示时过滤掉隐藏的原文标记
	// 注意：这个函数需要在 .cpp 文件中实现，因为需要访问 ULanguageOneSettings
	FString GetDisplayText(const FString& Text);
}

// ========== Blueprint MetaData 兼容 ==========
#include "Engine/Blueprint.h"

namespace LanguageOneBlueprintHelper
{
	// 获取变量 Tooltip（安全版本，避免崩溃）
	inline FString GetVariableTooltip(const FBPVariableDescription& Variable)
	{
		// 安全检查：某些蓝图类型（如 ControlRig）可能有特殊的变量结构
		// 使用 try-catch 或检查 MetaData 是否有效
		try
		{
			if (Variable.VarName.IsNone())
			{
				return FString();
			}
			
			// 检查 MetaData 是否有效
			if (Variable.HasMetaData(FBlueprintMetadata::MD_Tooltip))
			{
				return Variable.GetMetaData(FBlueprintMetadata::MD_Tooltip);
			}
		}
		catch (...)
		{
			// 如果访问失败，返回空字符串
			UE_LOG(LogTemp, Warning, TEXT("Failed to get variable tooltip for: %s"), *Variable.VarName.ToString());
			return FString();
		}
		return FString();
	}
	
	// 设置变量 Tooltip（安全版本）
	inline void SetVariableTooltip(FBPVariableDescription& Variable, const FString& Tooltip)
	{
		try
		{
			if (!Variable.VarName.IsNone())
			{
				Variable.SetMetaData(FBlueprintMetadata::MD_Tooltip, Tooltip);
			}
		}
		catch (...)
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to set variable tooltip for: %s"), *Variable.VarName.ToString());
		}
	}
}

// ========== AssetData 兼容 ==========
#include "AssetRegistry/AssetData.h"

namespace LanguageOneAssetDataHelper
{
	// 获取资产类名
	inline FString GetAssetClassName(const FAssetData& AssetData)
	{
#if UE_VERSION_NEWER_THAN(5, 1)
		return AssetData.AssetClassPath.ToString();
#else
		return AssetData.AssetClass.ToString();
#endif
	}

    // 检查类名是否包含特定字符串
    inline bool ClassNameContains(const FAssetData& AssetData, const TCHAR* Pattern)
    {
        return GetAssetClassName(AssetData).Contains(Pattern);
    }
}

// ========== 其他可能的兼容性问题预留 ==========
// 后续遇到版本差异可以在这里添加

