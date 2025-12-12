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

// 版本兼容的头文件
#if ENGINE_MAJOR_VERSION >= 5
	#include "Styling/SlateStyleMacros.h"
#else
	#include "Styling/SlateBrush.h"
	#include "Styling/SlateStyle.h"
#endif

TSharedPtr<FSlateStyleSet> FLanguageOneStyle::StyleInstance = NULL;

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

#if ENGINE_MAJOR_VERSION >= 5
	// UE 5.0+ 使用 SVG（更清晰）
	#define RootToContentDir Style->RootToContentDir
	Style->Set("LanguageOne.PluginAction", new IMAGE_BRUSH_SVG(TEXT("LanguageOneIcon"), Icon64x64));
	Style->Set("LanguageOne.TranslateCommentAction", new IMAGE_BRUSH_SVG(TEXT("CommentTranslateIcon"), Icon64x64));
	#undef RootToContentDir
	UE_LOG(LogTemp, Log, TEXT("LanguageOne Style - Using SVG icons (UE 5.0+)"));
#else
	// UE 4.x 使用 PNG
	#define IMAGE_BRUSH_PNG(RelativePath, ...) FSlateImageBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
	Style->Set("LanguageOne.PluginAction", new IMAGE_BRUSH_PNG(TEXT("Icon128"), Icon64x64));
	Style->Set("LanguageOne.TranslateCommentAction", new IMAGE_BRUSH_PNG(TEXT("CommentTranslateIcon"), Icon64x64));
	#undef IMAGE_BRUSH_PNG
	UE_LOG(LogTemp, Log, TEXT("LanguageOne Style - Using PNG icons (UE 4.x)"));
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
