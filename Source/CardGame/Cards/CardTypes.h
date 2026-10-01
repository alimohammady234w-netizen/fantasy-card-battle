// Fantasy Card Battle - Card enums, native gameplay tags and type helpers (Stage 2)
//
// Architecture notes:
// - All card vocabulary (rarity / category / ability type) lives here as UENUMs.
// - NEVER reorder or remove existing enum entries: serialized Data Assets store
//   the numeric value. New entries must always be APPENDED at the end.
// - Native Gameplay Tags give cards an expandable free-form label system that
//   does not require touching the card architecture when new labels are needed.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CardTypes.generated.h"

// ============================================================================
// ECardRarity
// Supports future: frame changes, visual effects, pack probabilities,
// collection filtering, upgrade requirements (none of them exist yet).
// ============================================================================
UENUM(BlueprintType)
enum class ECardRarity : uint8
{
	Common,
	Uncommon,
	Rare,
	Epic,
	Legendary,
	Mythic
};

// ============================================================================
// ECardCategory
// Append new categories at the end only (serialized data stores the index).
// ============================================================================
UENUM(BlueprintType)
enum class ECardCategory : uint8
{
	Animals,
	Dinosaurs,
	Robots,
	AncientEgypt UMETA(DisplayName = "Ancient Egypt"),
	AncientPersia UMETA(DisplayName = "Ancient Persia"),
	AncientGreece UMETA(DisplayName = "Ancient Greece"),
	AncientRome UMETA(DisplayName = "Ancient Rome"),
	Heroes,
	Villains,
	Fantasy,
	Mythology,
	SciFi UMETA(DisplayName = "Sci-Fi"),
	Space,
	Monsters,
	Magic
};

// ============================================================================
// ECardAbilityType
// DATA ONLY: describes WHICH ability a card has. Ability execution/behavior
// is intentionally NOT implemented in Stage 2 (future stage).
// ============================================================================
UENUM(BlueprintType)
enum class ECardAbilityType : uint8
{
	None,
	Shield,
	Heal,
	Freeze,
	Poison,
	Curse,
	CriticalStrike UMETA(DisplayName = "Critical Strike"),
	Mirror,
	Copy,
	DoubleAttack UMETA(DisplayName = "Double Attack"),
	Counter,
	Revive,
	Boost,
	Silence,
	Dodge,
	Rage
};

// ============================================================================
// Native gameplay tags (foundation only - add more by appending DECLARE here
// and DEFINE in CardTypes.cpp; no need to touch any other system).
// ============================================================================
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Card_Element_Fire);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Card_Element_Water);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Card_Element_Earth);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Card_Element_Air);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Card_Trait_Ancient);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Card_Trait_Mechanical);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Card_Trait_Magical);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Card_Role_LegendaryCreature);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Card_Role_Warrior);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Card_Role_Monster);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Card_Role_Boss);

// ============================================================================
// UCardTypesLibrary - Blueprint access to the enum vocabulary.
// (Data-layer helpers only; no UI widgets are created in this stage.)
// ============================================================================
UCLASS()
class CARDGAME_API UCardTypesLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Localized display name of a rarity (e.g. "Legendary"). */
	UFUNCTION(BlueprintPure, Category = "CardGame|Cards")
	static FText GetRarityDisplayName(ECardRarity Rarity);

	/** Localized display name of a category (e.g. "Ancient Egypt"). */
	UFUNCTION(BlueprintPure, Category = "CardGame|Cards")
	static FText GetCategoryDisplayName(ECardCategory Category);

	/** Localized display name of an ability type (e.g. "Critical Strike"). */
	UFUNCTION(BlueprintPure, Category = "CardGame|Cards")
	static FText GetAbilityDisplayName(ECardAbilityType AbilityType);
};
