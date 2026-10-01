// Fantasy Card Battle - Player card collection subsystem (Stage 3)
//
// UCardCollectionSubsystem is a UGameInstanceSubsystem: it lives on the game
// instance (UCardGameGameInstance from Stage 1) and therefore survives level
// travel - the collection belongs to persistent player state, NOT to a battle
// session or a GameMode.
//
// Responsibilities:
//   - public (Blueprint) API for ownership / quantity / level / XP queries
//   - resolving CardID -> UCardDataAsset (Stage 2) without duplicating data
//   - save / load via UCardGameSaveGame (centralized slot + version)
//   - safe failure everywhere (logs via LogCardGame, never crashes)
//
// No Tick, no Actors, no UI - mobile friendly by design.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Collection/PlayerCardInstance.h"
#include "Cards/CardDataAsset.h"
#include "CardCollectionSubsystem.generated.h"

UCLASS()
class CARDGAME_API UCardCollectionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ------------------------------------------------------------------
	// USubsystem
	// ------------------------------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ------------------------------------------------------------------
	// Card ownership operations
	// ------------------------------------------------------------------

	/**
	 * Adds one copy of the given card (Level 1, XP 0, fresh unique id).
	 * Fails safely (false + log) when CardID is empty or the definition
	 * cannot be resolved - never crashes.
	 */
	UFUNCTION(BlueprintCallable, Category = "CardGame|Collection")
	bool AddCard(FName CardID);

	/**
	 * Removes up to Count copies of CardID (newest instance first).
	 * Returns how many instances were actually removed (0 when none owned).
	 */
	UFUNCTION(BlueprintCallable, Category = "CardGame|Collection")
	int32 RemoveCard(FName CardID, int32 Count = 1);

	/** Removes one specific instance by UniqueInstanceID. Returns false when not found. */
	UFUNCTION(BlueprintCallable, Category = "CardGame|Collection")
	bool RemoveCardInstance(FGuid UniqueInstanceID);

	// ------------------------------------------------------------------
	// Ownership queries (used by future Deck Builder / Shop / Packs / UI)
	// ------------------------------------------------------------------

	/** Does the player own at least one copy of this card? */
	UFUNCTION(BlueprintPure, Category = "CardGame|Collection")
	bool HasCard(FName CardID) const;

	/** Does this exact instance (unique copy) exist in the collection? */
	UFUNCTION(BlueprintPure, Category = "CardGame|Collection")
	bool HasCardInstance(FGuid UniqueInstanceID) const;

	/** How many copies of CardID does the player own? */
	UFUNCTION(BlueprintPure, Category = "CardGame|Collection")
	int32 GetCardQuantity(FName CardID) const;

	/** Retrieves one owned instance by its unique id. */
	UFUNCTION(BlueprintCallable, Category = "CardGame|Collection")
	bool GetCardInstance(FGuid UniqueInstanceID, FPlayerCardInstance& OutInstance) const;

	/** Retrieves every copy of a given CardID. */
	UFUNCTION(BlueprintCallable, Category = "CardGame|Collection")
	TArray<FPlayerCardInstance> GetCardInstances(FName CardID) const;

	/** Everything the player owns (data only - resolve definitions separately). */
	UFUNCTION(BlueprintCallable, Category = "CardGame|Collection")
	TArray<FPlayerCardInstance> GetAllOwnedCards() const;

	/** Owned cards whose static definition has the given category. */
	UFUNCTION(BlueprintCallable, Category = "CardGame|Collection")
	TArray<FPlayerCardInstance> GetCardsByCategory(ECardCategory Category) const;

	/** Owned cards whose static definition has the given rarity. */
	UFUNCTION(BlueprintCallable, Category = "CardGame|Collection")
	TArray<FPlayerCardInstance> GetCardsByRarity(ECardRarity Rarity) const;

	/** Removes every owned card (used by tests / debug / future reset flows). */
	UFUNCTION(BlueprintCallable, Category = "CardGame|Collection")
	void ClearCollection();

	// ------------------------------------------------------------------
	// Progression (data only - no upgrade costs, no auto formulas)
	// ------------------------------------------------------------------

	/**
	 * Sets the level of one instance. Rejects NewLevel < 1 and unknown ids.
	 * CardID and UniqueInstanceID are never modified.
	 */
	UFUNCTION(BlueprintCallable, Category = "CardGame|Collection")
	bool SetCardLevel(FGuid UniqueInstanceID, int32 NewLevel);

	/**
	 * Adds XP to one instance. Rejects negative amounts, saturates at MAX_int32.
	 * Deliberately does NOT auto-level-up (kept simple in this stage).
	 */
	UFUNCTION(BlueprintCallable, Category = "CardGame|Collection")
	bool AddCardXP(FGuid UniqueInstanceID, int32 Amount);

	// ------------------------------------------------------------------
	// Definition resolution (PlayerCardInstance -> CardID -> UCardDataAsset)
	// ------------------------------------------------------------------

	/**
	 * Resolves a CardID to its static Stage 2 definition.
	 * Lookup order: weak cache -> conventional path
	 * /Game/Data/Cards/<CardID>.<CardID> -> Asset Registry search (subfolders).
	 * Returns nullptr when the definition does not exist (fail-safe).
	 */
	UFUNCTION(BlueprintPure, Category = "CardGame|Collection")
	UCardDataAsset* GetCardDefinition(FName CardID) const;

	/** C++ variant of GetCardDefinition (same rules, same guarantees). */
	UCardDataAsset* ResolveCardDefinition(FName CardID) const;

	// ------------------------------------------------------------------
	// Save / Load
	// ------------------------------------------------------------------

	/** Serializes the collection into the CardGameSave slot. False on I/O failure. */
	UFUNCTION(BlueprintCallable, Category = "CardGame|Save")
	bool SaveCollection();

	/**
	 * Loads the collection from the CardGameSave slot and sanitizes it.
	 * Safe when: no save exists (starts empty), save is corrupt (starts
	 * empty + error log), save type mismatches, or data is malformed
	 * (invalid entries repaired/dropped, never crashes).
	 */
	UFUNCTION(BlueprintCallable, Category = "CardGame|Save")
	bool LoadCollection();

	// ------------------------------------------------------------------
	// Development helpers (remove/replace before production)
	// ------------------------------------------------------------------

	/**
	 * GRANTS DEVELOPMENT/TEST DATA ONLY - adds the Stage 2 sample cards with
	 * duplicates (Lion x3, T-Rex x4, Dragon x2, ...). Not a production
	 * starter bundle; replace it when a real reward flow exists.
	 */
	UFUNCTION(BlueprintCallable, Category = "CardGame|Development", meta = (DisplayName = "Grant Dev Test Collection"))
	void GrantDevTestCollection();

	/** Writes every owned instance to the log (LogCardGame) - manual test aid. */
	void DumpCollection() const;

private:
	/** Version migration hook for future save format changes (v1 = initial). */
	void MigrateSaveData(class UCardGameSaveGame& Save) const;

	/** Runtime collection - plain struct, no UObject churn. */
	FPlayerCardCollection CollectionData;

	/** Weak cache so definition lookups do not reload assets repeatedly. */
	mutable TMap<FName, TWeakObjectPtr<UCardDataAsset>> DefinitionCache;
};
