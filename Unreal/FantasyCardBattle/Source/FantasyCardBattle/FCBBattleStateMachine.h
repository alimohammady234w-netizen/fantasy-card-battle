#pragma once
#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "FCBTypes.h"
#include "FCBBattleStateMachine.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFCBRoundResolved, EFCBRoundResult, Result, const FFCBPredictionChoice&, Pick);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFCBMatchFinished);

/** Owns one match: shuffled 30-card decks, alternating predictor, hidden opponent card, resolve-exactly-once guard.
 *  Presentation (actors/widgets) observes delegates; it never resolves rounds itself. Testable without a world. */
UCLASS(BlueprintType) class FANTASYCARDBATTLE_API UFCBBattleStateMachine : public UObject {
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable) void StartMatch(const TArray<FName>& PlayerDeck, const TArray<FName>& AIDeck, int32 Seed);
	UFUNCTION(BlueprintCallable) bool BeginNextRound(FName& OutPlayerCard, FName& OutAICardHidden); // AI card returned only as ID for actor back-face; attributes stay hidden until reveal
	/** Player locks a prediction. Only valid in Choosing phase; second calls are rejected. */
	UFUNCTION(BlueprintCallable) bool LockPlayerPrediction(const FFCBPredictionChoice& Pick);
	UFUNCTION(BlueprintCallable) bool LockAIPrediction(); // AI picks using ONLY its own attrs + public history
	UFUNCTION(BlueprintCallable) bool RevealAndResolve(FFCBPredictionChoice& OutPick, int32& OutPredictorValue, int32& OutOtherValue);
	UFUNCTION(BlueprintPure) EFCBBattlePhase GetPhase() const { return Phase; }
	UFUNCTION(BlueprintPure) bool IsPlayerPredictor() const { return bPlayerPredictor; }
	UFUNCTION(BlueprintPure) FName GetPlayerCardId() const { return PlayerCardId; }
	/** Valid only after reveal - the UI/actor layer must not query AI attributes before Lock. */
	UFUNCTION(BlueprintPure) bool GetRevealedAICard(FName& OutId) const { if (Phase == EFCBBattlePhase::Resolved || Phase == EFCBBattlePhase::MatchOver) { OutId = AICardId; return true; } return false; }
	void SetCardLookup(TFunction<bool(FName, FFCBCardTableRow&)> InLookup) { Lookup = MoveTemp(InLookup); }
	void SetPlayerLevelLookup(TFunction<int32(FName)> InLookup) { LevelLookup = MoveTemp(InLookup); }
	UPROPERTY(BlueprintReadOnly) int32 PlayerScore = 0, AIScore = 0, Ties = 0, RoundNumber = 0;
	UPROPERTY(BlueprintReadOnly) FString MatchId;
	UPROPERTY(BlueprintAssignable) FOnFCBRoundResolved OnRoundResolved;
	UPROPERTY(BlueprintAssignable) FOnFCBMatchFinished OnMatchFinished;
	EFCBDifficulty Difficulty = EFCBDifficulty::Normal;
private:
	EFCBBattlePhase Phase = EFCBBattlePhase::Idle;
	TArray<FName> PlayerDraw, AIDraw; FName PlayerCardId, AICardId; bool bPlayerPredictor = true, bResolved = false;
	FFCBPredictionChoice LockedPick; FRandomStream Rng; TMap<EFCBAttribute, TArray<int32>> PublicHistory;
	TFunction<bool(FName, FFCBCardTableRow&)> Lookup; TFunction<int32(FName)> LevelLookup;
	FFCBAttributeSet AttrsOf(FName Id, bool bPlayer) const;
};
