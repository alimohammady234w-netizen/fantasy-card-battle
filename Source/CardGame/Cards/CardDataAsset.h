// Fantasy Card Battle - Card Data Asset definition (Stage 2)

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/PrimaryAssetId.h"
#include "GameplayTagContainer.h"
#include "CardTypes.h"
#include "CardStats.h"
#include "CardDataAsset.generated.h"

class UTexture2D;
class USoundBase;
class UAnimationAsset;

/**
 * Static, data-driven definition of a single card.
 *
 * This is the SINGLE SOURCE OF TRUTH for card information. Widgets, GameMode,
 * Deck, Collection, Battle and AI must reference this asset instead of
 * hard-coding card data anywhere.
 *
 * Architecture contract for future stages:
 * - UCardDataAsset  = immutable "CardDefinition" (what the card IS).
 * - A future PlayerOwnedCardData (struct/save-game) = per-player instance
 *   (UniqueInstanceID, current Level/XP/UpgradeState) referencing this asset
 *   by Primary Asset Id. Player progression NEVER writes into this asset.
 *
 * Mobile notes:
 * - All large assets (images, sounds, animations) are SOFT references so a
 *   10,000-card database can be scanned/loaded without pulling textures or
 *   audio into memory.
 * - The object itself is a lightweight UObject with no Tick and no Actors.
 *
 * Primary Asset Id:  Card:<CardID>   (e.g. Card:CARD_ANIMAL_LION_001)
 * Compatible with a future UAssetManager-based card database.
 */
UCLASS(BlueprintType)
class CARDGAME_API UCardDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ---------------------------------------------------------------
	// Identification
	// ---------------------------------------------------------------

	/** Unique card identifier, e.g. CARD_ANIMAL_LION_001. Used as Primary Asset Id name. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identification", AssetRegistrySearchable, meta = (DisplayName = "Card ID"))
	FName CardID = NAME_None;

	/** Display name of the card (localized). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identification", meta = (MultiLine = true))
	FText Name;

	/** Flavor / rules description of the card (localized). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identification", meta = (MultiLine = true))
	FText Description;

	// ---------------------------------------------------------------
	// Classification
	// ---------------------------------------------------------------

	/** Category this card belongs to (Animals, Dinosaurs, Robots, ...). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Classification")
	ECardCategory Category = ECardCategory::Animals;

	/** Rarity of the card (Common ... Mythic). Drives future frames/packs/filtering. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Classification")
	ECardRarity Rarity = ECardRarity::Common;

	// ---------------------------------------------------------------
	// Stats
	// ---------------------------------------------------------------

	/** Power / Speed / Height / Defense / Intelligence / Stamina / Luck / Age. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	FCardStats Stats;

	// ---------------------------------------------------------------
	// Base progression (initial values only - no upgrade system yet)
	// ---------------------------------------------------------------

	/** Level this card definition starts at. Must be >= 1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression", meta = (ClampMin = "1", DisplayName = "Base Level"))
	int32 BaseLevel = 1;

	/** XP this card definition starts at. Must be >= 0. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression", meta = (ClampMin = "0", DisplayName = "Base XP"))
	int32 BaseXP = 0;

	// ---------------------------------------------------------------
	// Ability (data only - no execution in this stage)
	// ---------------------------------------------------------------

	/** Which ability the card has (None = no ability). Behavior comes in a future stage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
	ECardAbilityType AbilityType = ECardAbilityType::None;

	/** Numeric magnitude of the ability (e.g. 10 = +10%). Meaning depends on AbilityType. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability", meta = (ClampMin = "0"))
	float AbilityValue = 0.f;

	/** Human-readable description of what the ability does (localized). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability", meta = (MultiLine = true))
	FText AbilityDescription;

	// ---------------------------------------------------------------
	// Visual (soft references - loaded on demand, mobile friendly)
	// ---------------------------------------------------------------

	/** Main artwork of the card. Soft reference: NOT loaded until requested. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TSoftObjectPtr<UTexture2D> CardImage;

	/** Frame/border texture for the card (rarity frames later). Soft reference. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TSoftObjectPtr<UTexture2D> CardFrame;

	// ---------------------------------------------------------------
	// Audio (soft references)
	// ---------------------------------------------------------------

	/** Sound played when the card is revealed/used (no Sound Manager yet). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TSoftObjectPtr<USoundBase> CardSound;

	/** Voice-over clip of the card. Soft reference. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TSoftObjectPtr<USoundBase> CardVoice;

	// ---------------------------------------------------------------
	// Animation (soft reference)
	// ---------------------------------------------------------------

	/** Future card animation (flip/attack/etc). No animation logic in this stage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimationAsset> CardAnimation;

	// ---------------------------------------------------------------
	// Tags (expandable gameplay-tag labels)
	// ---------------------------------------------------------------

	/** Free-form labels (Card.Element.*, Card.Trait.*, Card.Role.*). Expandable without code changes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tags", meta = (Categories = "Card"))
	FGameplayTagContainer CardTags;

	// ---------------------------------------------------------------
	// API
	// ---------------------------------------------------------------

	/**
	 * Unique Primary Asset Id for this card:  Card:<CardID>
	 * (falls back to the asset name while CardID is unset).
	 */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/** Blueprint access to the Primary Asset Id (future UAssetManager lookups). */
	UFUNCTION(BlueprintPure, Category = "CardGame|Card")
	FPrimaryAssetId GetCardPrimaryAssetId() const;

	/**
	 * Validates this card definition. Returns true when there are no ERRORS
	 * (warnings are allowed). Messages are human readable and prefixed with
	 * "Card Data Validation ...". Never throws/crashes - invalid editor data
	 * only produces messages.
	 */
	UFUNCTION(BlueprintCallable, Category = "CardGame|Validation")
	bool ValidateCardData(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const;

	/**
	 * Editor convenience: runs ValidateCardData and writes every message to the
	 * LogCardGame category. Exposed as a "Validate Card Data Now" button in the
	 * asset's Details panel (CallInEditor).
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "CardGame|Validation")
	void ValidateCardDataNow();
};
