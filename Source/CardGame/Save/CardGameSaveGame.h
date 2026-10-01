// Fantasy Card Battle - SaveGame for the player collection (Stage 3)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Collection/PlayerCardInstance.h"
#include "CardGameSaveGame.generated.h"

/**
 * Persistent storage for the player's card collection.
 *
 * Centralized save configuration (slot name / user index / version) lives
 * here so no other class hard-codes save details.
 *
 * Versioning: SaveVersion records the format that wrote the file. Future
 * stages (decks, coins, achievements...) bump CurrentSaveVersion and add
 * migration steps in UCardCollectionSubsystem::MigrateSaveData().
 */
UCLASS()
class CARDGAME_API UCardGameSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** Save slot used by the project (single local profile for now). */
	static const FString DefaultSaveSlotName;

	/** User index of the save slot. */
	static constexpr int32 DefaultSaveUserIndex = 0;

	/** Current data format written by this build. Bump when the layout changes. */
	static constexpr int32 CurrentSaveVersion = 1;

	/** Format version of THIS save file (written by the saving build). */
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "CardGame|Save")
	int32 SaveVersion = CurrentSaveVersion;

	/** Full player card collection (instances, levels, XP, quantities). */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "CardGame|Save")
	FPlayerCardCollection Collection;
};
