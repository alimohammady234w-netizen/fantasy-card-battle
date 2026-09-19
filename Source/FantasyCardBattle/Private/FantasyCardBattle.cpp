// Copyright (c) Fantasy Card Battle. All rights reserved.

#include "FantasyCardBattle.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogFCB);
DEFINE_LOG_CATEGORY(LogFCBData);
DEFINE_LOG_CATEGORY(LogFCBAI);

/**
 * The primary game module. Nothing is done at module load time on purpose: the rules engine, the card
 * database and the AI are plain C++ (see Source/FantasyCardBattle/Public/FCBCardDatabase.h), so a data error
 * must surface when a match is *started*, not while the module is still loading - that is what keeps the
 * headless harness and the editor on exactly the same code path.
 */
IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, FantasyCardBattle, "FantasyCardBattle");

namespace FCBUi
{
	FString FormatAttribute(EFCBAttribute InAttribute, int32 InValue)
	{
		// Delegated to the engine-side util so the number format cannot drift between the HUD, the log file
		// and Docs/Balance.md samples.
		return FCBAttributeUtil::FormatValue(InAttribute, InValue);
	}

	float RarityGlowStrength(EFCBRarity InRarity)
	{
		switch (InRarity)
		{
		case EFCBRarity::Common:		return 0.00f;
		case EFCBRarity::Uncommon:	return 0.12f;
		case EFCBRarity::Rare:			return 0.28f;
		case EFCBRarity::Epic:			return 0.48f;
		case EFCBRarity::Legendary:	return 0.72f;
		case EFCBRarity::Mythic:		return 1.00f;
		default:							return 0.00f;
		}
	}
}
