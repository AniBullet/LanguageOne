/*
 * @Description: 
 * @Author: Bullet.S
 * @Date: 2025-12-12 20:45:47
 * @LastEditors: Bullet.S
 * @LastEditTime: 2025-12-17 11:33:11
 * @Email: animator.bullet@foxmail.com
 */
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/Launch/Resources/Version.h"

/**
 * 版本兼容性处理 - Version Compatibility Layer
 * 
 * 仅支持 UE 5.0 及以上版本。
 * Only supports UE 5.0 and later versions.
 */

// ========== 版本检测宏 ==========
// 仅保留运行时检测，不再需要编译时兼容性宏
#ifndef ENGINE_PATCH_VERSION
	#define ENGINE_PATCH_VERSION 0
#endif

// ========== URL 编码兼容 ==========
// 所有版本统一使用 FGenericPlatformHttp::UrlEncode
#include "GenericPlatform/GenericPlatformHttp.h"
#define LANGUAGEONE_URL_ENCODE(Text) FGenericPlatformHttp::UrlEncode(Text)

// ========== Slate 样式兼容 ==========
// UE5+ 统一使用 FAppStyle
#include "Styling/AppStyle.h"
// 保留别名以防万一，但建议直接使用 FAppStyle
#define LANGUAGEONE_EDITOR_STYLE FAppStyle

// ========== StringTable API 兼容 ==========
#include "Internationalization/StringTable.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/Text.h"

// 前向声明
class ULanguageOneSettings;

// StringTable 辅助函数
namespace LanguageOneStringTableHelper
{
	// 枚举 StringTable 的所有键
	inline void EnumerateStringTableKeys(const FStringTableConstRef& StringTableData, TArray<FString>& OutKeys)
	{
		StringTableData->EnumerateKeysAndSourceStrings([&OutKeys](const FTextKey& InKey, const FString& InSourceString) -> bool
		{
			// UE 5.0+ 支持 FTextKey
#if (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
			OutKeys.Add(InKey.ToString());
#else
			OutKeys.Add(FString(InKey.GetChars()));
#endif
			return true; // 继续枚举
		});
	}
	
	// 查找 StringTable 条目
	inline FString FindStringTableEntry(const FStringTableConstRef& StringTableData, const FString& Key)
	{
		// UE 5.0+ 返回 FStringTableEntryConstPtr (TSharedPtr<const FStringTableEntry>)
		auto Entry = StringTableData->FindEntry(FTextKey(Key));
		if (Entry.IsValid())
		{
			return Entry->GetSourceString();
		}
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
	// 查找变量元数据
	inline FString GetVariableMetaData(const FBPVariableDescription& Variable, const FName& Key)
	{
		if (Variable.HasMetaData(Key))
		{
			return Variable.GetMetaData(Key);
		}
		return FString();
	}

	// 设置变量元数据
	inline void SetVariableMetaData(FBPVariableDescription& Variable, const FName& Key, const FString& Value)
	{
		// const_cast 是为了适配 SetMetaData 非 const 签名 (虽然传入引用应该没问题，但为了保险)
		FBPVariableDescription& MutableVariable = const_cast<FBPVariableDescription&>(Variable);
		MutableVariable.SetMetaData(Key, Value);
	}

	// 获取变量 Tooltip（安全版本，避免崩溃）
	inline FString GetVariableTooltip(const FBPVariableDescription& Variable)
	{
		// 安全检查
		try
		{
			if (Variable.VarName.IsNone())
			{
				return FString();
			}
			
			return GetVariableMetaData(Variable, FBlueprintMetadata::MD_Tooltip);
		}
		catch (...)
		{
			// 如果访问失败，返回空字符串
			UE_LOG(LogTemp, Warning, TEXT("Failed to get variable tooltip for: %s"), *Variable.VarName.ToString());
			return FString();
		}
	}
	
	// 设置变量 Tooltip（安全版本）
	inline void SetVariableTooltip(FBPVariableDescription& Variable, const FString& Tooltip)
	{
		try
		{
			if (!Variable.VarName.IsNone())
			{
				SetVariableMetaData(Variable, FBlueprintMetadata::MD_Tooltip, Tooltip);
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
#include "AssetRegistry/IAssetRegistry.h"
// Include Version.h to ensure version macros are defined
#include "Runtime/Launch/Resources/Version.h"

namespace LanguageOneAssetDataHelper
{
	// 获取资产类名
	inline FString GetAssetClassName(const FAssetData& AssetData)
	{
		// UE 5.1+ 使用 AssetClassPath
#if (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1))
		return AssetData.AssetClassPath.ToString();
#else
		// UE 5.0 使用 AssetClass
		return AssetData.AssetClass.ToString();
#endif
	}

    // 检查类名是否包含特定字符串
    inline bool ClassNameContains(const FAssetData& AssetData, const TCHAR* Pattern)
    {
        return GetAssetClassName(AssetData).Contains(Pattern);
    }

	// 根据 ObjectPath 获取 AssetData
	inline FAssetData GetAssetByObjectPath(IAssetRegistry& AssetRegistry, const FSoftObjectPath& ObjectPath)
	{
#if (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1))
		return AssetRegistry.GetAssetByObjectPath(ObjectPath);
#else
		// UE 5.0 使用 FName
		return AssetRegistry.GetAssetByObjectPath(FName(*ObjectPath.ToString()));
#endif
	}
}

// ========== UMG 兼容 ==========
#include "Components/EditableText.h"
#include "Components/EditableTextBox.h"

namespace LanguageOneUMGHelper
{
	inline FText GetEditableTextHintText(UEditableText* EditableText)
	{
		return EditableText->GetHintText();
	}

	inline FText GetEditableTextBoxHintText(UEditableTextBox* EditableTextBox)
	{
		return EditableTextBox->GetHintText();
	}
}

// ========== Blueprint Function Metadata 兼容 ==========
namespace LanguageOneBlueprintMetadataHelper
{
	inline bool HasMetaData(const FKismetUserDeclaredFunctionMetadata& Metadata, const FName& Key)
	{
		return Metadata.HasMetaData(Key);
	}

	inline void SetMetaData(FKismetUserDeclaredFunctionMetadata& Metadata, const FName& Key, const FString& Value)
	{
		Metadata.SetMetaData(Key, Value);
	}
}
