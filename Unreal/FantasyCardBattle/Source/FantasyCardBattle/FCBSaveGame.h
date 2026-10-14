#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "FCBTypes.h"
#include "FCBSaveGame.generated.h"

/** Local, offline save. Stored by UE in the project's Saved/SaveGames folder (FCB_Save.sav, plus FCB_Save_Backup.sav). */
UCLASS() class FANTASYCARDBATTLE_API UFCBSaveGame : public USaveGame {
	GENERATED_BODY()
public:
	static constexpr int32 CurrentSchema = 1;
	UPROPERTY(SaveGame) int32 SchemaVersion = CurrentSchema;
	UPROPERTY(SaveGame) int32 PlayerLevel = 1;
	UPROPERTY(SaveGame) int32 PlayerXP = 0;
	UPROPERTY(SaveGame) int32 RoundWins = 0; UPROPERTY(SaveGame) int32 RoundLosses = 0; UPROPERTY(SaveGame) int32 RoundTies = 0;
	UPROPERTY(SaveGame) int32 MatchesPlayed = 0; UPROPERTY(SaveGame) int32 MatchesWon = 0;
	UPROPERTY(SaveGame) TArray<FFCBOwnedCard> OwnedCards;
	UPROPERTY(SaveGame) TArray<FFCBDeck> Decks;
	UPROPERTY(SaveGame) FGuid ActiveDeckId;
	UPROPERTY(SaveGame) EFCBDifficulty Difficulty = EFCBDifficulty::Normal;
	UPROPERTY(SaveGame) float SoundVolume = 0.7f;
	UPROPERTY(SaveGame) bool bEffectsEnabled = true;
	UPROPERTY(SaveGame) TArray<FString> RewardedMatchIds; // idempotent match rewards
};
