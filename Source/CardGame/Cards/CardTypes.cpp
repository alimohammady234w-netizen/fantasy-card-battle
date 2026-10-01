// Fantasy Card Battle - Card enums, native gameplay tags and type helpers (Stage 2)

#include "CardTypes.h"

#include "UObject/Class.h"

// ---------------------------------------------------------------------------
// Native gameplay tags (declared in CardTypes.h).
// These register automatically at module startup and show up in the editor's
// Gameplay Tag picker under the Card.* hierarchy.
// ---------------------------------------------------------------------------
UE_DEFINE_GAMEPLAY_TAG(TAG_Card_Element_Fire, "Card.Element.Fire");
UE_DEFINE_GAMEPLAY_TAG(TAG_Card_Element_Water, "Card.Element.Water");
UE_DEFINE_GAMEPLAY_TAG(TAG_Card_Element_Earth, "Card.Element.Earth");
UE_DEFINE_GAMEPLAY_TAG(TAG_Card_Element_Air, "Card.Element.Air");
UE_DEFINE_GAMEPLAY_TAG(TAG_Card_Trait_Ancient, "Card.Trait.Ancient");
UE_DEFINE_GAMEPLAY_TAG(TAG_Card_Trait_Mechanical, "Card.Trait.Mechanical");
UE_DEFINE_GAMEPLAY_TAG(TAG_Card_Trait_Magical, "Card.Trait.Magical");
UE_DEFINE_GAMEPLAY_TAG(TAG_Card_Role_LegendaryCreature, "Card.Role.LegendaryCreature");
UE_DEFINE_GAMEPLAY_TAG(TAG_Card_Role_Warrior, "Card.Role.Warrior");
UE_DEFINE_GAMEPLAY_TAG(TAG_Card_Role_Monster, "Card.Role.Monster");
UE_DEFINE_GAMEPLAY_TAG(TAG_Card_Role_Boss, "Card.Role.Boss");

// ---------------------------------------------------------------------------
// Display-name helpers
// ---------------------------------------------------------------------------
FText UCardTypesLibrary::GetRarityDisplayName(ECardRarity Rarity)
{
	if (const UEnum* EnumPtr = StaticEnum<ECardRarity>())
	{
		return EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(Rarity));
	}
	return FText::GetEmpty();
}

FText UCardTypesLibrary::GetCategoryDisplayName(ECardCategory Category)
{
	if (const UEnum* EnumPtr = StaticEnum<ECardCategory>())
	{
		return EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(Category));
	}
	return FText::GetEmpty();
}

FText UCardTypesLibrary::GetAbilityDisplayName(ECardAbilityType AbilityType)
{
	if (const UEnum* EnumPtr = StaticEnum<ECardAbilityType>())
	{
		return EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(AbilityType));
	}
	return FText::GetEmpty();
}
