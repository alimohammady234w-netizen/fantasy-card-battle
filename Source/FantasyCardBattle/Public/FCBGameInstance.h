// Copyright (c) Fantasy Card Battle. All rights reserved.
//
// FCBGameInstance.h - the only object that owns game state.
//
// Why a GameInstance and not an Actor: the card database, the match and the AI are plain C++ and outlive any
// single level, so a match survives a travel (and a debug "restart level" keeps the same deck). Widgets and
// the code-only HUD read everything through the view structs below and never touch FFCBMatch, which keeps the
// presentation layer away from the rules engine.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "FCBCardDatabase.h"
#include "FCBDataAssets.h"
#include "FCBAiAgent.h"
#include "FCBMatchRules.h"
#include "FCBGameInstance.generated.h"

USTRUCT(BlueprintType)
struct FFCBCardView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Card")
	FName Id;

	UPROPERTY(BlueprintReadOnly, Category = "Card")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Card")
	FString FactionName;

	UPROPERTY(BlueprintReadOnly, Category = "Card")
	FString RarityName;

	UPROPERTY(BlueprintReadOnly, Category = "Card")
	FLinearColor FrameColor = FLinearColor::Gray;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 AgeYears = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 Power = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 Speed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 HeightCm = 0;

	/** Pre-formatted ("12,400 yr", "1.94 m"), so widget blueprints never re-implement the number rules. */
	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	FString AgeText;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	FString PowerText;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	FString SpeedText;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	FString HeightText;

	UPROPERTY(BlueprintReadOnly, Category = "Ability")
	FString Ability0Name;

	UPROPERTY(BlueprintReadOnly, Category = "Ability")
	FString Ability0Text;

	UPROPERTY(BlueprintReadOnly, Category = "Ability")
	FString Ability1Name;

	UPROPERTY(BlueprintReadOnly, Category = "Ability")
	FString Ability1Text;

	/** 0..1 across the whole pool - the "how good is this, really" bar. */
	UPROPERTY(BlueprintReadOnly, Category = "Card")
	float StrengthPercentile = 0.5f;

	/** Hand slot this copy sits in (INDEX_NONE once it has left the hand). */
	UPROPERTY(BlueprintReadOnly, Category = "Card")
	int32 HandIndex = INDEX_NONE;

	/** Top of the hand: the card that will be used automatically if it is the opponent's turn. */
	UPROPERTY(BlueprintReadOnly, Category = "Card")
	bool bIsTopOfHand = false;

	/** Remaining "once per match" charges (Undying and friends). */
	UPROPERTY(BlueprintReadOnly, Category = "Card")
	int32 RemainingCharges = 1;

	/** Persisted self-buff stacks, shown as "+n" on the card face. */
	UPROPERTY(BlueprintReadOnly, Category = "Card")
	int32 PowerStacks = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Card")
	FString PortraitPath;
};

USTRUCT(BlueprintType)
struct FFCBRoundView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	int32 RoundNumber = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	FString AttackerName;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	FString DefenderName;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	FString AttributeName;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	FString OutcomeText;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	FString Narration;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	TArray<FString> ModifierNotes;

	/** Printed values, before ability modifiers. */
	UPROPERTY(BlueprintReadOnly, Category = "Round")
	int32 AttackerPrinted = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	int32 DefenderPrinted = 0;

	/** Values the engine actually compared. */
	UPROPERTY(BlueprintReadOnly, Category = "Round")
	int32 AttackerValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	int32 DefenderValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	int32 WinnerCardsGained = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	int32 PotCardsAdded = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	int32 PotCardsAwarded = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	int32 ScoreA = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	int32 ScoreB = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFcbMatchUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFcbRoundResolved, const FFCBRoundView&, Round);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFcbMatchFinished, int32, WinnerSeat, const FString&, Reason);

UCLASS(BlueprintType)
class FANTASYCARDBATTLE_API UFCBGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	//~ Begin UObject / UGameInstance interface
	virtual void Init() override;
	virtual void Shutdown() override;
	//~ End UObject / UGameInstance interface

	/** Seats, for Blueprint code that wants to avoid hard-coding 0/1. */
	UFUNCTION(BlueprintPure, Category = "FCB")
	static int32 HumanSeat() { return FCBSeat::A; }

	UFUNCTION(BlueprintPure, Category = "FCB")
	static int32 OpponentSeat() { return FCBSeat::B; }

	/** (Re)load the card data: rules asset, else Content/Data/Generated, else a debug pool. */
	UFUNCTION(BlueprintCallable, Category = "FCB|Data")
	bool ReloadCardData();

	UFUNCTION(BlueprintPure, Category = "FCB|Data")
	FString GetDataNote() const { return DataNote; }

	UFUNCTION(BlueprintPure, Category = "FCB|Data")
	int32 GetCardCount() const { return Database.GetCardCount(); }

	/** True when the data passed validation, i.e. a match can be started. */
	UFUNCTION(BlueprintPure, Category = "FCB|Data")
	bool IsDataUsable() const { return bDataUsable; }

	/** @param InSeedOverride negative keeps the current seed, useful for a pure rematch. */
	UFUNCTION(BlueprintCallable, Category = "FCB|Match")
	bool StartNewMatch(int32 InSeedOverride = -1);

	/** Fresh random seed, then a new match. The seed is printed in the HUD so the game stays replayable. */
	UFUNCTION(BlueprintCallable, Category = "FCB|Match")
	void RollSeedAndRestart();

	UFUNCTION(BlueprintCallable, Category = "FCB|Match")
	bool IsMatchStarted() const { return Match.IsStarted(); }

	UFUNCTION(BlueprintPure, Category = "FCB|Match")
	bool IsMatchOver() const { return Match.IsOver(); }

	UFUNCTION(BlueprintPure, Category = "FCB|Match")
	int32 GetRoundNumber() const { return Match.GetRoundNumber(); }

	UFUNCTION(BlueprintPure, Category = "FCB|Match")
	int32 GetLeaderSeat() const { return Match.GetLeaderSeat(); }

	UFUNCTION(BlueprintPure, Category = "FCB|Match")
	FString GetSeatName(int32 Seat) const { return Match.GetConfig().GetSeatName(Seat); }

	UFUNCTION(BlueprintPure, Category = "FCB|Match")
	int32 GetScore(int32 Seat) const;

	UFUNCTION(BlueprintPure, Category = "FCB|Match")
	int32 GetPotCount() const { return Match.GetPotCount(); }

	UFUNCTION(BlueprintPure, Category = "FCB|Match")
	int32 GetDeckCount() const { return Match.GetDeckCount(); }

	UFUNCTION(BlueprintPure, Category = "FCB|Match")
	int32 GetBurnedCount() const { return Match.GetBurnedCount(); }

	UFUNCTION(BlueprintPure, Category = "FCB|Match")
	bool IsPlayersTurn() const { return !Match.IsOver() && Match.GetLeaderSeat() == FCBSeat::A; }

	// ------------------------------------------------------------- selection / input -----------------------------

	UFUNCTION(BlueprintCallable, Category = "FCB|Input")
	void SetSelectedHandIndex(int32 InIndex);

	UFUNCTION(BlueprintPure, Category = "FCB|Input")
	int32 GetSelectedHandIndex() const { return SelectedHandIndex; }

	UFUNCTION(BlueprintCallable, Category = "FCB|Input")
	void MoveSelection(int32 Delta);

	UFUNCTION(BlueprintCallable, Category = "FCB|Input")
	void SetSelectedAttribute(EFCBAttribute InAttribute);

	UFUNCTION(BlueprintPure, Category = "FCB|Input")
	EFCBAttribute GetSelectedAttribute() const { return SelectedAttribute; }

	/** The card the player is holding. Returns an empty view when the hand is empty. */
	UFUNCTION(BlueprintPure, Category = "FCB|Input")
	FFCBCardView GetSelectedCard() const;

	/** What would happen if the player confirmed right now - the HUD's "preview" numbers. */
	UFUNCTION(BlueprintCallable, Category = "FCB|Input")
	bool ProjectSelectedMove(FFCBRoundView& OutProjection) const;

	UFUNCTION(BlueprintCallable, Category = "FCB|Match")
	bool ConfirmSelection();

	/** Play on behalf of the AI seat. Public so a widget or a replay driver can step the game by hand. */
	UFUNCTION(BlueprintCallable, Category = "FCB|Match")
	bool PlayAiTurn();

	// -------------------------------------------------------------------- views -------------------------------------

	UFUNCTION(BlueprintPure, Category = "FCB|View")
	void GetHandView(int32 Seat, TArray<FFCBCardView>& OutCards) const;

	UFUNCTION(BlueprintPure, Category = "FCB|View")
	void GetPileView(int32 Seat, int32 MaxCards, TArray<FFCBCardView>& OutCards) const;

	/** The opponent's face-down top card: the player can see the frame, the rest is hidden by design. */
	UFUNCTION(BlueprintPure, Category = "FCB|View")
	FFCBCardView GetOpponentTopCard() const;

	UFUNCTION(BlueprintPure, Category = "FCB|View")
	int32 GetHandCount(int32 Seat) const;

	/** Plain-text match summary for the debug HUD, the log file and (later) the end-of-game screen. */
	UFUNCTION(BlueprintCallable, Category = "FCB|View")
	FString FormatSummary() const;

	UFUNCTION(BlueprintCallable, Category = "FCB|View")
	FString FormatLog(int32 FromRound = 0) const { return Match.FormatLog(FromRound); }

	/** Why the AI just did what it did - empty unless the debug overlay is on. */
	UFUNCTION(BlueprintPure, Category = "FCB|View")
	FString GetLastAiExplanation() const { return LastAiExplanation; }

	UFUNCTION(BlueprintPure, Category = "FCB|View")
	EFCBAiDifficulty GetAiDifficulty() const { return AiDifficulty; }

	UPROPERTY(BlueprintAssignable, Category = "FCB")
	FOnFcbMatchUpdated OnMatchUpdated;

	UPROPERTY(BlueprintAssignable, Category = "FCB")
	FOnFcbRoundResolved OnRoundResolved;

	UPROPERTY(BlueprintAssignable, Category = "FCB")
	FOnFcbMatchFinished OnMatchFinished;

	/** Direct access for the code-only HUD and for tests; widgets should use the views above. */
	const FFCBMatch& GetMatch() const { return Match; }

	const FFCBCardDatabase& GetDatabase() const { return Database; }

private:
	void ApplySettingsToConfig(FFCBMatchConfig& InOutConfig) const;
	void ClampSelectionToHand();
	void BuildCardView(const FFCBCardInstance& InCard, int32 InHandIndex, FFCBCardView& OutView) const;
	int32 FindDefIndex(FName InId) const;
	void BroadcastMatchUpdated();
	void BroadcastRound(const FFCBRoundResult& Result);
	void ScheduleAiTurn();
	void HandleDataError(const FString& InError);

	/** Timer trampoline: FTimerDelegate wants a void signature, PlayAiTurn reports whether it acted. */
	void HandleAiThinkExpired();

	UPROPERTY(EditAnywhere, Category = "FCB", meta = (ClampMin = "0", ClampMax = "4"))
	EFCBAiDifficulty AiDifficulty = EFCBAiDifficulty::Expert;

	/** Optional override for a "tournament" style fixed-seed session; -1 means "from the config". */
	UPROPERTY(EditAnywhere, Category = "FCB")
	int32 FixedSeedOverride = -1;

	/** Consecutive AI moves in one chain. Bounded so a runaway AI can never lock the game loop. */
	UPROPERTY(EditAnywhere, Category = "FCB", meta = (ClampMin = "1", ClampMax = "200"))
	int32 MaxConsecutiveAiTurns = 40;

	int32 AiChainLength = 0;
	int32 SelectedHandIndex = 0;

	/**
	 * The card the cursor is on, tracked by id as well as by index. Abilities move cards around the hand
	 * (Undying returns a card, an extra capture takes one), and "my card" should follow the card, not the slot.
	 */
	FName SelectedCardId;

	EFCBAttribute SelectedAttribute = EFCBAttribute::Power;
	FString DataNote;
	FString LastAiExplanation;
	bool bDataUsable = false;

	FFCBCardDatabase Database;
	FFCBMatch Match;
	FFCBAiAgent AiAgent;
	FFCBAiProfile AiProfile;

	FTimerHandle AiThinkTimer;
};
