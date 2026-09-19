// Copyright (c) Fantasy Card Battle. All rights reserved.
//
// FantasyCardBattle.h - module surface: log categories and the one include every UE-side file needs.
//
// The categories are deliberately split, because the three subsystems fail in very different ways:
//   LogFCB     gameplay flow (match started, round resolved, game over) - what a designer watches while tuning
//   LogFCBData card/database loading and validation - the only place a *fatal* data error is reported
//   LogFCBAI   opponent decisions and their explanations - spammy, so it is Verbose by default
//
// Enable them in console:  Log LogFCBAI Verbose

#pragma once

#include "CoreMinimal.h"
#include "FCBTypes.h"

DECLARE_LOG_CATEGORY_EXTERN(LogFCB, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogFCBData, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogFCBAI, Verbose, All);

/** Small helpers shared by the UE layer. Kept here so widget code does not need FCBTypes.h. */
namespace FCBUi
{
	/** "12,400 yr", "1.94 m", "72" - the same formatting the headless log uses, so both agree on screen. */
	FString FormatAttribute(EFCBAttribute InAttribute, int32 InValue);

	/** 0..1 alpha used for the rarity glow on card widgets; authored so Mythic is visibly special. */
	float RarityGlowStrength(EFCBRarity InRarity);
}
