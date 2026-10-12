#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FCBTypes.h"
#include "FCBProgressionSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFCBProgressChanged);

/** Card registry + collection + decks + progression + automatic local save. UI never writes save data directly;
 *  it calls this subsystem, which validates, mutates, and persists at meaningful checkpoints (never per-frame). */
UCLASS() class FANTASYCARDBATTLE_API UFCBProgressionSubsystem : public UGameInstanceSubsystem {
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	// Registry (immutable definitions)
	UFUNCTION(BlueprintCallable) bool LoadCardRegistry(FString& OutReport); // DT_Cards + Card data assets; validates duplicates/ranges/art
	UFUNCTION(BlueprintPure) TArray<FName> GetAllCardIds() const { TArray<FName> K; Registry.GetKeys(K); return K; }
	UFUNCTION(BlueprintPure) bool GetDefinition(FName CardId, FFCBCardTableRow& OutRow) const { if (const FFCBCardTableRow* R = Registry.Find(CardId)) { OutRow = *R; return true; } return false; }
	// Collection / progression
	UFUNCTION(BlueprintPure) FFCBOwnedCard GetOwned(FName CardId) const;
	UFUNCTION(BlueprintCallable) bool TryUpgradeCard(FName CardId, FString& OutError); // spends that card's XP bank
	UFUNCTION(BlueprintCallable) void GrantCardXP(FName CardId, int32 XP);
	// Decks
	UFUNCTION(BlueprintPure) TArray<FFCBDeck> GetDecks() const { return Decks; }
	UFUNCTION(BlueprintCallable) FGuid CreateDeck(const FString& Name);
	UFUNCTION(BlueprintCallable) bool DeleteDeck(FGuid DeckId);
	UFUNCTION(BlueprintCallable) bool SetActiveDeck(FGuid DeckId);
	UFUNCTION(BlueprintCallable) bool ValidateActiveDeck(TArray<FString>& OutErrors) const;
	// Save
	UFUNCTION(BlueprintCallable) void SaveNow();
	UFUNCTION(BlueprintCallable) void ResetProgress(); // deletes slot + backup, re-initializes a new game
	void LoadOrInitialize();
	UPROPERTY(BlueprintAssignable) FOnFCBProgressChanged OnProgressChanged;
	UPROPERTY(BlueprintReadOnly) int32 PlayerLevel = 1; UPROPERTY(BlueprintReadOnly) int32 PlayerXP = 0;
	EFCBDifficulty Difficulty = EFCBDifficulty::Normal; float SoundVolume = 0.7f; bool bEffectsEnabled = true;
	bool AwardMatchRewardsOnce(const FString& MatchId, bool bWon, bool bDraw, int32& OutPlayerXP, bool& bLeveledUp);
	TMap<FName, int32> OwnedCopiesMap() const;
private:
	TMap<FName, FFCBCardTableRow> Registry;
	TMap<FName, FFCBOwnedCard> Owned;
	TArray<FFCBDeck> Decks; FGuid ActiveDeckId;
	int32 RoundWins = 0, RoundLosses = 0, RoundTies = 0, MatchesPlayed = 0, MatchesWon = 0;
	TArray<FString> RewardedMatchIds;
	void InitNewGame();
};
