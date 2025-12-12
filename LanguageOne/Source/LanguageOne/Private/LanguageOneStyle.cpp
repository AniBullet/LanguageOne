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
#include "Styling/SlateBrush.h"

// UE5.1+ 需要 SVG Brush
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
	#include "Brushes/SlateVectorImageBrush.h"
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
const FVector2D Icon40x40(40.0f, 40.0f);

TSharedRef< FSlateStyleSet > FLanguageOneStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("LanguageOneStyle"));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin("LanguageOne")->GetBaseDir() / TEXT("Resources"));

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
	// UE 5.1+ 使用 SVG
	#define IMAGE_BRUSH_SVG(RelativePath, ...) FSlateVectorImageBrush(Style->RootToContentDir(RelativePath, TEXT(".svg")), __VA_ARGS__)
	Style->Set("LanguageOne.PluginAction", new IMAGE_BRUSH_SVG(TEXT("LanguageOneIcon"), Icon40x40));
	Style->Set("LanguageOne.TranslateCommentAction", new IMAGE_BRUSH_SVG(TEXT("CommentTranslateIcon"), Icon40x40));
	#undef IMAGE_BRUSH_SVG
	UE_LOG(LogTemp, Log, TEXT("LanguageOne Style - Using SVG icons (UE 5.1+)"));
#else
	// UE 4.26-5.0 使用 PNG (使用宏定义，在 Create 函数内 Style 已存在)
	#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
	Style->Set("LanguageOne.PluginAction", new IMAGE_BRUSH(TEXT("Icon128"), Icon40x40));
	Style->Set("LanguageOne.TranslateCommentAction", new IMAGE_BRUSH(TEXT("CommentTranslateIcon"), Icon40x40));
	#undef IMAGE_BRUSH
	UE_LOG(LogTemp, Log, TEXT("LanguageOne Style - Using PNG icons (UE 4.26-5.0)"));
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
