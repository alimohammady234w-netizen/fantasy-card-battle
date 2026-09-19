// Copyright (c) Fantasy Card Battle. All rights reserved.
//
// FCBMatchRules.h - the authoritative rules engine for a Top-Trumps-style fantasy card duel.
//
// Design notes (see also Docs/GameDesign.md and Docs/Balance.md):
//  * Pure C++, deterministic, no UObject / UMG / Engine includes. A match is fully described by
//    (config + seed + the ordered list of moves), which gives us replays, unit tests and AI rollouts
//    for free.
//  * The leader picks ANY card from hand plus one attribute to contest. The defender plays the top card
//    of their hand (classic Top Trumps), which is what keeps hidden information - and therefore the
//    card-counting AI - interesting.
//  * Ability text is data-driven; this file only implements the effect semantics.

#pragma once

#include "CoreMinimal.h"
#include "FCBTypes.h"
#include "FCBCardDatabase.h"

/** How a tie on the contested attribute is resolved. */
UENUM(BlueprintType)
enum class EFCBTieRule : uint8
{
	/** Classic "clash": both cards go to the pot, the next winner takes the whole pot. */
	PotClash			UMETA(DisplayName = "Clash / Pot"),
	/** The rarer card wins; if both have the same rarity it becomes a clash. */
	HigherRarityWins	UMETA(DisplayName = "Higher Rarity Wins"),
	/** The leading player wins ties (faster, more aggressive meta). */
	LeaderWinsTies		UMETA(DisplayName = "Leader Wins Ties"),
	/** No capture at all; both cards burn. Leader keeps the lead. */
	BurnNoCapture		UMETA(DisplayName = "Burn Both Cards"),
	/** Each player keeps their own card (honourable draw), leader keeps the lead. */
	SplitKeep			UMETA(DisplayName = "Split (Keep Cards)")
};

/** Result of one contested round. */
UENUM(BlueprintType)
enum class EFCBRoundOutcome : uint8
{
	Invalid			UMETA(DisplayName = "Invalid"),
	AttackerWins	UMETA(DisplayName = "Attacker Wins"),
	DefenderWins	UMETA(DisplayName = "Defender Wins"),
	Clash			UMETA(DisplayName = "Clash")
};

/** When a match ends. */
UENUM(BlueprintType)
enum class EFCBEndCondition : uint8
{
	/** Someone cannot field a card: the other seat wins (classic). */
	LastSeatStanding	UMETA(DisplayName = "Last Seat Standing"),
	/** Play up to MaxRounds, highest score wins. Used for timed / party modes. */
	RoundLimitScore		UMETA(DisplayName = "Round Limit, Highest Score"),
	/** First seat to reach TargetScore wins. */
	FirstToScore		UMETA(DisplayName = "First To Target Score")
};

/** Human-readable outcome, shared by the log, the HUD and the end-of-round toast. */
namespace FCBRoundUtil
{
	inline FString ToString(EFCBRoundOutcome InOutcome)
	{
		switch (InOutcome)
		{
		case EFCBRoundOutcome::AttackerWins:	return TEXT("Attacker wins");
		case EFCBRoundOutcome::DefenderWins:	return TEXT("Defender wins");
		case EFCBRoundOutcome::Clash:			return TEXT("Clash");
		case EFCBRoundOutcome::Invalid:
		default:								return TEXT("Invalid");
		}
	}
}

/** Seat identifiers. Two seats only in v1 (hot-seat human vs AI or human vs human). */
namespace FCBSeat
{
	inline constexpr int32 A = 0;
	inline constexpr int32 B = 1;
	inline constexpr int32 Count = 2;
	inline int32 OpponentOf(int32 Seat) { return Seat == FCBSeat::A ? FCBSeat::B : FCBSeat::A; }
}

/** Everything a designer / lobby can tune about a match. */
USTRUCT(BlueprintType)
struct FFCBMatchConfig
{
	GENERATED_BODY()

	/** Cards held per seat. 12 gives ~19 rounds per seat with a 110-card pool. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match", meta = (ClampMin = "3", ClampMax = "24"))
	int32 HandSize = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	EFCBTieRule TieRule = EFCBTieRule::PotClash;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	EFCBEndCondition EndCondition = EFCBEndCondition::LastSeatStanding;

	/** Hard stop for RoundLimitScore / safety net against pathological states. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match", meta = (ClampMin = "1"))
	int32 MaxRounds = 240;

	/** Only used by FirstToScore. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match", meta = (ClampMin = "0"))
	int32 TargetScore = 20;

	/** When true the defender contributes the top card of their hand (classic). When false the defender also picks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	bool bDefenderPlaysTopCard = true;

	/** Master switch for the keyword system (turn off for a pure "kids" Top Trumps ladder). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	bool bEnableAbilities = true;

	/** Disables the strongest trigger group (extra captures) without touching stat modifiers - used for Casual mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	bool bEnableExtraCaptures = true;

	/** Captured rarity weights the score. Off = pure card count (classic). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	bool bRarityScoreWeights = true;

	/** Once the pot holds this many cards, it is swallowed: cards are shuffled back into the deck. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match", meta = (ClampMin = "1", ClampMax = "40"))
	int32 PotCap = 12;

	/** Winner draws their refill card before the loser (small, intentional advantage). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	bool bWinnerDrawsFirst = true;

	/** Shuffle seed. Same seed + same card pool + same moves == identical match (replay/test guarantee). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	uint32 RngSeed = 1337u;

	/** Which slice of the card pool enters the match deck. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	FFCBDeckFilter DeckFilter;

	/** Swap cards between the two halves so total strength stays within tolerance (ranked fairness). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	bool bBalanceSeats = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	FString SeatAName = TEXT("Player One");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	FString SeatBName = TEXT("Player Two");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	bool bSeatAIsHuman = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	bool bSeatBIsHuman = false;

	/** Seat that leads round 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match", meta = (ClampMin = "0", ClampMax = "1"))
	int32 OpeningSeat = 0;

	/** If false and a seat has no cards, the match restarts that seat from the burn pile. Unused in v1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
	bool bAllowReanimateFromPile = false;

	bool IsHumanSeat(int32 Seat) const { return Seat == FCBSeat::A ? bSeatAIsHuman : bSeatBIsHuman; }
	void SetSeatHuman(int32 Seat, bool bHuman) { if (Seat == FCBSeat::A) { bSeatAIsHuman = bHuman; } else { bSeatBIsHuman = bHuman; } }
	const FString& GetSeatName(int32 Seat) const { return Seat == FCBSeat::A ? SeatAName : SeatBName; }
	void SetSeatName(int32 Seat, const FString& Name) { if (Seat == FCBSeat::A) { SeatAName = Name; } else { SeatBName = Name; } }

	FString ToString() const;
};

/** A legal (or being-proposed) action: "play hand card N on attribute X". */
USTRUCT(BlueprintType)
struct FFCBMove
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	int32 Seat = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	int32 HandIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	EFCBAttribute Attribute = EFCBAttribute::Power;

	bool IsValid() const { return Seat >= 0 && HandIndex >= 0 && Attribute != EFCBAttribute::Max; }

	static FFCBMove Make(int32 InSeat, int32 InHandIndex, EFCBAttribute InAttribute)
	{
		FFCBMove Move;
		Move.Seat = InSeat;
		Move.HandIndex = InHandIndex;
		Move.Attribute = InAttribute;
		return Move;
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("seat %d hand[%d] %s"), Seat, HandIndex, *FCBAttributeUtil::ToString(Attribute));
	}
};

/**
 * Everything the rules engine derives for a single round *before* state mutation.
 * Exposed as pure data so the UI can show a "what will happen" preview and the AI can evaluate
 * hypothetical moves without copying the match state.
 */
USTRUCT(BlueprintType)
struct FFCBRoundProjection
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	EFCBAttribute Attribute = EFCBAttribute::Power;

	/** Printed (doctrine-adjusted, stack-adjusted) values. */
	UPROPERTY(BlueprintReadOnly, Category = "Round")
	int32 AttackerPrinted = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	int32 DefenderPrinted = 0;

	/** Values after abilities, i.e. the numbers actually compared. */
	UPROPERTY(BlueprintReadOnly, Category = "Round")
	int32 AttackerEffective = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	int32 DefenderEffective = 0;

	/** Ability modifier totals, kept for the UI's "+12 Ember Fury" breakdown. */
	UPROPERTY(BlueprintReadOnly, Category = "Round")
	int32 AttackerModifier = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	int32 DefenderModifier = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	bool bAttackerNullified = false;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	bool bDefenderNullified = false;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	bool bAttackerWinsTies = false;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	bool bDefenderWinsTies = false;

	// Post-resolution triggers (already gated by config + remaining charges).
	UPROPERTY(BlueprintReadOnly, Category = "Round")
	bool bAttackerTakesDeckTop = false;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	bool bDefenderTakesDeckTop = false;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	bool bAttackerExtraCapture = false;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	bool bDefenderExtraCapture = false;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	bool bAttackerUndying = false;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	bool bDefenderUndying = false;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	int32 AttackerPotScoreBonus = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	int32 DefenderPotScoreBonus = 0;

	/**
	 * Resolved clutch windows in attribute points (-1 = no clutch). ResolveRoundModifiers converts the
	 * authored percent into points, so FinalizeOutcome stays scale-free and the AI sees the same number
	 * the engine will use. Only the defender can clutch: the leader's card is never in a defending
	 * position under v1 rules, so AttackerClutchMargin is informational for the UI.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Round")
	int32 AttackerClutchMargin = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	int32 DefenderClutchMargin = -1;

	/** Rarity indexes, used by the "Higher Rarity Wins" tie rule without needing the card objects. */
	UPROPERTY(BlueprintReadOnly, Category = "Round")
	int32 AttackerRarityIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	int32 DefenderRarityIndex = 0;

	/** Final outcome for this projection, after tie rules and clutch flips (filled by FFCBMatch::ProjectMove). */
	UPROPERTY(BlueprintReadOnly, Category = "Round")
	EFCBRoundOutcome Outcome = EFCBRoundOutcome::Invalid;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	TArray<FString> Notes;

	int32 GetMargin() const { return AttackerEffective - DefenderEffective; }

	bool IsClash() const { return Outcome == EFCBRoundOutcome::Clash; }
	int32 GetWinnerSeat(int32 LeaderSeat) const
	{
		if (Outcome == EFCBRoundOutcome::AttackerWins) { return LeaderSeat; }
		if (Outcome == EFCBRoundOutcome::DefenderWins) { return FCBSeat::OpponentOf(LeaderSeat); }
		return INDEX_NONE;
	}

	/** Winner from the stat comparison *including* tiebreaker abilities, before the configured tie rule. */
	EFCBRoundOutcome GetRawOutcome() const
	{
		const int32 Margin = GetMargin();
		if (Margin > 0) { return EFCBRoundOutcome::AttackerWins; }
		if (Margin < 0) { return EFCBRoundOutcome::DefenderWins; }
		if (bAttackerWinsTies && !bDefenderWinsTies) { return EFCBRoundOutcome::AttackerWins; }
		if (bDefenderWinsTies && !bAttackerWinsTies) { return EFCBRoundOutcome::DefenderWins; }
		return EFCBRoundOutcome::Clash;
	}
};

/** One resolved round, as recorded in history / replays. */
USTRUCT(BlueprintType)
struct FFCBRoundResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	int32 RoundNumber = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	FFCBMove Move;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	FName AttackerCardId;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	FName DefenderCardId;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	FString AttackerCardName;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	FString DefenderCardName;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	EFCBRoundOutcome Outcome = EFCBRoundOutcome::Invalid;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	FFCBRoundProjection Projection;

	/** Net cards moved to the winner's pile (including pot) minus cards the leader lost. */
	UPROPERTY(BlueprintReadOnly, Category = "Round")
	int32 WinnerCardsGained = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	int32 PotCardsAwarded = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	int32 PotCardsAdded = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	int32 PotCardsSwallowed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	int32 ScoreSwing = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	bool bLeaderChanged = false;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	FString Narration;
};

/** Per-seat public state. */
USTRUCT(BlueprintType)
struct FFCBSeatState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Seat")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Seat")
	bool bIsHuman = true;

	/** Face-down draw pile (public count, hidden contents). */
	UPROPERTY(BlueprintReadOnly, Category = "Seat")
	TArray<FFCBCardInstance> Hand;

	/** Face-up trophies. This is the "score pile". */
	UPROPERTY(BlueprintReadOnly, Category = "Seat")
	TArray<FFCBCardInstance> Pile;

	UPROPERTY(BlueprintReadOnly, Category = "Seat")
	int32 Score = 0;

	/** Non-pile score from abilities (Pot Score Bonus, ...). Total score = pile score + this. */
	UPROPERTY(BlueprintReadOnly, Category = "Seat")
	int32 BonusScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Seat")
	int32 RoundsWon = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Seat")
	int32 ClashCount = 0;

	/** Cards lost while holding a pot, tracked purely for the post-match screen. */
	UPROPERTY(BlueprintReadOnly, Category = "Seat")
	int32 PotLostCount = 0;

	/** Sum of CaptureScore() of the pile, cached. */
	int32 PeekPileScore() const
	{
		int32 Total = 0;
		for (const FFCBCardInstance& Card : Pile)
		{
			Total += FCBRarityUtil::CaptureScore(Card.Rarity);
		}
		return Total;
	}
};

/** Full match state. Cheap to copy, which is what lets the AI run rollouts. */
USTRUCT(BlueprintType)
struct FFCBMatchState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Match")
	TArray<FFCBSeatState> Seats;

	/** Shared draw pile. Both seats refill from here, so hand sizes can diverge. */
	UPROPERTY(BlueprintReadOnly, Category = "Match")
	TArray<FFCBCardInstance> Deck;

	/** Clash pile in the middle of the table. */
	UPROPERTY(BlueprintReadOnly, Category = "Match")
	TArray<FFCBCardInstance> Pot;

	/** Cards removed by the "Burn Both Cards" tie rule: out of the match, still public knowledge. */
	UPROPERTY(BlueprintReadOnly, Category = "Match")
	TArray<FFCBCardInstance> Burned;

	UPROPERTY(BlueprintReadOnly, Category = "Match")
	int32 LeaderSeat = FCBSeat::A;

	UPROPERTY(BlueprintReadOnly, Category = "Match")
	int32 RoundNumber = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Match")
	bool bMatchOver = false;

	/** INDEX_NONE while running; -1 == draw at the end. */
	UPROPERTY(BlueprintReadOnly, Category = "Match")
	int32 WinnerSeat = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Match")
	FString EndReason;

	UPROPERTY(BlueprintReadOnly, Category = "Match")
	TArray<FFCBRoundResult> History;

	/** Human readable, one line per event. Kept in state so headless runs / replays can dump it. */
	UPROPERTY(BlueprintReadOnly, Category = "Match")
	TArray<FString> Log;
};

/**
 * The match. Owns state, applies rules, exposes only legal operations to callers.
 */
class FANTASYCARDBATTLE_API FFCBMatch
{
public:
	FFCBMatch();

	/** Must be called before StartMatch. The database must outlive the match. */
	void Configure(const FFCBMatchConfig& InConfig, const FFCBCardDatabase* InDatabase);

	const FFCBMatchConfig& GetConfig() const { return Config; }
	const FFCBMatchState& GetState() const { return State; }
	const FFCBCardDatabase* GetDatabase() const { return Database; }

	/** Builds the deck from the config filter, shuffles deterministically, deals, balances seats. */
	bool StartMatch(FString& OutError);

	bool IsStarted() const { return State.Seats.Num() == FCBSeat::Count; }
	bool IsOver() const { return State.bMatchOver; }
	int32 GetLeaderSeat() const { return State.LeaderSeat; }
	int32 GetRoundNumber() const { return State.RoundNumber; }

	bool IsLegalMove(const FFCBMove& Move) const;
	void GetLegalMoves(int32 Seat, TArray<FFCBMove>& OutMoves) const;

	/** Non-mutating preview, also the AI's evaluation primitive. */
	bool ProjectMove(const FFCBMove& Move, FFCBRoundProjection& OutProjection) const;

	/** Which card the defender would contribute for a given attacker move (respects bDefenderPlaysTopCard). */
	bool GetDefenderCardForMove(const FFCBMove& Move, int32& OutHandIndex) const;

	/** Applies all rules for one round. Returns false and leaves state untouched if the move is illegal. */
	bool PlayMove(const FFCBMove& Move, FString& OutError);

	/** Convenience for "the leader has no choice" situations (empty hand edge cases). */
	bool PlayRandomMoveForLeader(FString& OutError);

	/**
	 * Card-counting view for the AI: multiset of def indexes that could still be in the deck or in the
	 * opponent's hand from `ForSeat`'s perspective.
	 */
	void GetUnknownPool(int32 ForSeat, TMap<int32, int32>& OutIndexToCount) const;

	/** Cards the seat may never legally hold (used by the AI to avoid illegal inferences in tests). */
	const TMap<int32, int32>& GetDealtCounts() const { return DealtCounts; }

	FString FormatState() const;
	FString FormatLog(int32 FromRound = 0) const;

	/** Total captures across both seats; the game is finite so this monotonically grows. */
	int32 GetTotalCaptured() const;

	/** Cards out of play due to the burn tie rule. */
	int32 GetBurnedCount() const { return State.Burned.Num(); }
	int32 GetPotCount() const { return State.Pot.Num(); }
	int32 GetDeckCount() const { return State.Deck.Num(); }

	/** Score value of a captured card under the current rules (rarity weighted or flat). */
	int32 GetCaptureValue(const FFCBCardInstance& Card) const { return ScoreOf(Card); }

	/** Test/tooling access to the shared RNG state (replay verification). */
	uint32 PeekRngState() const { return RngState; }

	/** Rules applied to a projection: the final outcome, given the tie rule and clutch triggers. */
	static EFCBRoundOutcome FinalizeOutcome(const FFCBMatchConfig& Config, const FFCBRoundProjection& Projection);

	/**
	 * Computes ability modifiers for a pair of cards. Public (static) so tests and the AI can call it directly.
	 *
	 * The two scores are required because some abilities are conditional on the match state rather than on the
	 * cards ("First Charge" only fires while the leader is ahead on captures). Passing them in keeps this
	 * function pure and replay-safe: every caller - PlayMove, the AI scan, the harness - has the same numbers.
	 */
	static void ResolveRoundModifiers(
		const FFCBMatchConfig& Config,
		const FFCBCardInstance& Attacker,
		const FFCBCardInstance& Defender,
		EFCBAttribute Attribute,
		int32 AttackerFactionPileCount,
		int32 DefenderFactionPileCount,
		int32 AttackerScore,
		int32 DefenderScore,
		FFCBRoundProjection& OutProjection);

	/** Score-blind convenience overload: treats the score as level, so lead-dependent abilities stay off. */
	static void ResolveRoundModifiers(
		const FFCBMatchConfig& Config,
		const FFCBCardInstance& Attacker,
		const FFCBCardInstance& Defender,
		EFCBAttribute Attribute,
		int32 AttackerFactionPileCount,
		int32 DefenderFactionPileCount,
		FFCBRoundProjection& OutProjection)
	{
		ResolveRoundModifiers(Config, Attacker, Defender, Attribute,
			AttackerFactionPileCount, DefenderFactionPileCount, 0, 0, OutProjection);
	}

private:
	/** Turns a database index into a mutable in-play copy (doctrine-adjusted stats, ability copies, charges). */
	bool MakeInstanceFromCardDef(int32 CardIndex, FFCBCardInstance& OutInstance) const;

	/** Snake draft over the two opening hands so neither seat starts with a materially stronger set. */
	void BalanceSeatHands(TArray<FFCBCardInstance>& InOutHandA, TArray<FFCBCardInstance>& InOutHandB);
	void DrawUp(int32 Seat, int32 Count);
	void CaptureToPile(int32 Seat, FFCBCardInstance&& Card, FFCBRoundResult& OutResult);
	int32 ScoreOf(const FFCBCardInstance& Card) const;
	void AppendLog(const FString& Line);
	void CheckMatchEnd();

	FFCBMatchConfig Config;
	FFCBMatchState State;

	/** Non-owning: the database lives on the GameInstance (or on the harness in headless runs). */
	const FFCBCardDatabase* Database = nullptr;

	/** Full deck composition (def index -> copies dealt), used for the unknown-pool inference. */
	TMap<int32, int32> DealtCounts;

	/** Copy of the RNG stream, so a rollout/AI simulation can never desync the real match. */
	FRandomStream Rng;

	/** Monotonic counter of RNG draws; replays assert on it to prove determinism. */
	uint32 RngState = 0u;

	bool bLogEnabled = true;
};
