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
#define UE_VERSION_OLDER_THAN(Major, Minor) (ENGINE_MAJOR_VERSION < Major || (ENGINE_MAJOR_VERSION == Major && ENGINE_MINOR_VERSION < Minor))
#define UE_VERSION_NEWER_THAN(Major, Minor) (ENGINE_MAJOR_VERSION > Major || (ENGINE_MAJOR_VERSION == Major && ENGINE_MINOR_VERSION >= Minor))

// ========== URL 编码兼容 ==========
// UE5.0+ 使用 FGenericPlatformHttp::UrlEncode
// UE4.26-4.27 使用 FPlatformHttp::UrlEncode
#if ENGINE_MAJOR_VERSION >= 5
	#include "GenericPlatform/GenericPlatformHttp.h"
	#define LANGUAGEONE_URL_ENCODE(Text) FGenericPlatformHttp::UrlEncode(Text)
#else
	#include "GenericPlatform/GenericPlatformHttp.h"
	#define LANGUAGEONE_URL_ENCODE(Text) FPlatformHttp::UrlEncode(Text)
#endif

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

// ========== 其他可能的兼容性问题预留 ==========
// 后续遇到版本差异可以在这里添加

