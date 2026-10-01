// Fantasy Card Battle - Player-owned card data (Stage 3)
//
// Architecture:
//   UCardDataAsset   = STATIC card definition (Stage 2, shared by everyone)
//   FPlayerCardInstance = one card copy OWNED by this player (per-copy progression)
//   FPlayerCardCollection = container of all owned instances (pure data)
//
// Duplicate policy (Stage 3 decision): Option A - ONE INSTANCE PER COPY.
//   Lion x3 = three FPlayerCardInstance entries, each with its own
//   UniqueInstanceID, Level and XP. This keeps future per-copy upgrades
//   (different levels for different copies) fully possible.
//   The Quantity field (min required by spec) records how many copies an
//   instance represents; in the current architecture it is always 1, and
//   GetQuantity() SUMS it across instances of the same CardID.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "PlayerCardInstance.generated.h"

/**
 * A single owned copy of a card, as held by the player.
 *
 * DATA ONLY - it stores identity + progression state and references the static
 * definition by CardID. It never duplicates Name/Description/Stats/Ability
 * (those live in UCardDataAsset).
 */
USTRUCT(BlueprintType)
struct CARDGAME_API FPlayerCardInstance
{
	GENERATED_BODY()

	/** Globally unique id of THIS copy (player may own several copies of one CardID). */
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "CardGame|Collection")
	FGuid UniqueInstanceID;

	/** References the static definition (UCardDataAsset / Primary Asset Id name). */
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "CardGame|Collection")
	FName CardID = NAME_None;

	/** Current level of this copy. Minimum 1. */
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "CardGame|Collection", meta = (ClampMin = "1"))
	int32 Level = 1;

	/** Current XP of this copy. Minimum 0. */
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "CardGame|Collection", meta = (ClampMin = "0"))
	int32 XP = 0;

	/** Copies this instance represents. Always 1 in the one-instance-per-copy architecture (reserved for future stack semantics). */
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "CardGame|Collection", meta = (ClampMin = "1"))
	int32 Quantity = 1;

	/** True when identity is complete (valid GUID + non-empty CardID). */
	bool IsValid() const
	{
		return UniqueInstanceID.IsValid() && !CardID.IsNone();
	}

	/** Factory: fresh GUID, Level 1, XP 0, Quantity 1. */
	static FPlayerCardInstance MakeNew(FName InCardID);
};

/**
 * Pure-data container of every card the player owns.
 *
 * All operations are safe: invalid input returns false/0 and never crashes.
 * Blueprint code does not talk to this struct directly - it goes through
 * UCardCollectionSubsystem, which adds definition resolution, logging and save/load.
 */
USTRUCT(BlueprintType)
struct CARDGAME_API FPlayerCardCollection
{
	GENERATED_BODY()

	/** Owned card instances, in add-order. Read-only from Blueprint on purpose. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "CardGame|Collection")
	TArray<FPlayerCardInstance> OwnedCards;

	// ------------------------------------------------------------------
	// Mutating operations (fail-safe)
	// ------------------------------------------------------------------

	/** Adds an instance. Rejects invalid instances and duplicate UniqueInstanceIDs. */
	bool AddCard(const FPlayerCardInstance& Instance);

	/** Removes ONE instance by its unique id. Returns false when not found. */
	bool RemoveInstance(const FGuid& UniqueInstanceID);

	/** Removes up to Count copies of CardID (newest first). Returns how many were actually removed. */
	int32 RemoveCards(FName CardID, int32 Count = 1);

	/** Removes every instance. Returns how many were removed. */
	int32 Clear();

	/**
	 * Sets the level of one instance. Rejects NewLevel < 1 and unknown ids.
	 * Never touches CardID / UniqueInstanceID (identity is preserved).
	 */
	bool SetLevel(const FGuid& UniqueInstanceID, int32 NewLevel);

	/**
	 * Adds XP to one instance. Rejects negative amounts and unknown ids.
	 * Saturates at MAX_int32 (large values are safe). No auto level-up.
	 */
	bool AddXP(const FGuid& UniqueInstanceID, int32 Amount);

	// ------------------------------------------------------------------
	// Queries
	// ------------------------------------------------------------------

	FPlayerCardInstance* FindInstance(const FGuid& UniqueInstanceID);
	const FPlayerCardInstance* FindInstance(const FGuid& UniqueInstanceID) const;

	/** All copies of a CardID (copies of the internal array). */
	TArray<FPlayerCardInstance> GetCardsOf(FName CardID) const;

	bool HasCard(FName CardID) const;
	bool HasInstance(const FGuid& UniqueInstanceID) const;

	/** Number of owned copies of CardID (sum of Quantity across instances). */
	int32 GetQuantity(FName CardID) const;

	/** Total number of instances in the collection. */
	int32 Num() const { return OwnedCards.Num(); }

	// ------------------------------------------------------------------
	// Validation (used after loading possibly-malformed saves)
	// ------------------------------------------------------------------

	/**
	 * Repairs invalid collection data in place:
	 *  - drops instances with empty CardID or Quantity < 1
	 *  - regenerates invalid / duplicate UniqueInstanceIDs
	 *  - clamps Level to >= 1 and XP to >= 0
	 * Returns the number of issues fixed. Never crashes.
	 */
	int32 Sanitize();
};
