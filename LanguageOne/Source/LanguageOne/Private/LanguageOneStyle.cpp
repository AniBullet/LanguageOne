/*
 * @Description: 
 * @Author: Bullet.S
 * @Date: 2025-01-19 00:07:44
 * @LastEditors: Bullet.S
 * @LastEditTime: 2025-12-12 19:48:15
 * @Email: animator.bullet@foxmail.com
 */
// Copyright Epic Games, Inc. All Rights Reserved.

#include "LanguageOneStyle.h"
#include "LanguageOne.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/SlateStyleRegistry.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Brushes/SlateImageBrush.h"

// UE 5.1+ 才有 SlateStyleMacros.h
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
	#include "Styling/SlateStyleMacros.h"
	#define RootToContentDir Style->RootToContentDir
#else
	// UE 4.26-5.0 使用传统方式，SVG 回退到同名 PNG
	#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
	#define IMAGE_BRUSH_SVG(RelativePath, ...) IMAGE_BRUSH(RelativePath, __VA_ARGS__)
#endif

TSharedPtr<FSlateStyleSet> FLanguageOneStyle::StyleInstance = nullptr;

void FLanguageOneStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FLanguageOneStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FLanguageOneStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("LanguageOneStyle"));
	return StyleSetName;
}


const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon64x64(64.0f, 64.0f);

TSharedRef< FSlateStyleSet > FLanguageOneStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("LanguageOneStyle"));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin("LanguageOne")->GetBaseDir() / TEXT("Resources"));

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
	// UE 5.1+ 使用 SVG
	Style->Set("LanguageOne.PluginAction", new IMAGE_BRUSH_SVG(TEXT("LanguageOneIcon"), Icon64x64));
	Style->Set("LanguageOne.TranslateCommentAction", new IMAGE_BRUSH_SVG(TEXT("CommentTranslateIcon"), Icon64x64));
#else
	// UE 4.26-5.0 使用 PNG
	Style->Set("LanguageOne.PluginAction", new IMAGE_BRUSH(TEXT("Icon128"), Icon64x64));
	Style->Set("LanguageOne.TranslateCommentAction", new IMAGE_BRUSH(TEXT("CommentTranslateIcon"), Icon64x64));
#endif
	
	return Style;
}

void FLanguageOneStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FLanguageOneStyle::Get()
{
	return *StyleInstance;
}
