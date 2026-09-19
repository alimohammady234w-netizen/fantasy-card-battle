// Copyright (c) Fantasy Card Battle. All rights reserved.
//
// FCBAiAgent.h - the opponent AI. Four difficulty tiers, all deterministic for a given seed:
//
//   Novice    - uniformly random legal move.
//   Adept     - evaluates moves against the *whole printed card pool* (no card counting).
//   Expert    - evaluates moves against the *remaining unknown multiset* (proper card counting),
//               including the opponent's likely ability modifiers.
//   Legendary - Expert plus a 1-ply Monte-Carlo rollout of the opponent's best reply to our lead,
//               which is what makes it stop feeding pots at the end of the deck.
//
// The agent never mutates the match: everything is projected through FFCBMatch::ResolveRoundModifiers,
// so thinking is side-effect free and reproducible in tests.

#pragma once

#include "CoreMinimal.h"
#include "FCBMatchRules.h"

/** Difficulty tiers, mirrored by UFCBAiProfileAsset in the editor. */
UENUM(BlueprintType)
enum class EFCBAiDifficulty : uint8
{
	Novice		UMETA(DisplayName = "Novice"),
	Adept		UMETA(DisplayName = "Adept"),
	Expert		UMETA(DisplayName = "Expert"),
	Legendary	UMETA(DisplayName = "Legendary"),
	/** Anti-heuristic used by the balance harness: picks the move that is best *for the opponent*. */
	TortureTest	UMETA(DisplayName = "Torture Test (dev)")
};

/** Tunables. Authored in DT_AiProfiles / UFCBAiProfileAsset so balance does not need a code change. */
USTRUCT(BlueprintType)
struct FFCBAiProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	EFCBAiDifficulty Difficulty = EFCBAiDifficulty::Adept;

	/** Chance (0-100) of playing a random legal move instead of the scored best one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0", ClampMax = "100"))
	int32 BlunderPercent = 0;

	/** Monte-Carlo hands sampled for the Legendary reply model. 0 disables rollouts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0", ClampMax = "64"))
	int32 RolloutSamples = 0;

	/** Upper bound on candidate (card, attribute) pairs scored per decision. Keeps frame time bounded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "4"))
	int32 MaxCandidatesToScore = 400;

	/** How much losing a strong card hurts relative to losing a weak one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0", ClampMax = "4"))
	float CardQualityWeight = 0.6f;

	/** Value of one pot card, in "cards captured" units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0", ClampMax = "4"))
	float PotValueWeight = 0.85f;

	/** Fraction of the pot we expect to get back after a clash. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0", ClampMax = "1"))
	float PotReclaimChance = 0.45f;

	/** Bonus for an extra capture (opponent's weakest card). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0", ClampMax = "2"))
	float ExtraCaptureValue = 0.55f;

	/** Bonus for taking the top deck card on a win. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0", ClampMax = "2"))
	float DeckTopValue = 0.3f;

	/** Subtract sqrt of EV variance; >0 makes the agent prefer safe lines. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0", ClampMax = "2"))
	float RiskAversion = 0.35f;

	/** Extra credit for moves that can end the match immediately. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0", ClampMax = "8"))
	float FinisherBonus = 2.5f;

	/** Weight of the opponent's best reply (Legendary). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0", ClampMax = "2"))
	float ReplyPenaltyWeight = 0.5f;

	/** Approximate player rating shown in the lobby. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	int32 Elo = 1200;

	/**
	 * Soft frame budget for one decision, in milliseconds. The engine never measures itself against wall
	 * clock (that would make replays and the balance harness non-deterministic); the number exists so the UI
	 * can pace the AI's move and so the shipped tuning in Config/DefaultGame.ini has somewhere to live.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0"))
	int32 TimeBudgetMs = 0;

	static const FFCBAiProfile& GetPreset(EFCBAiDifficulty InDifficulty);
	FString ToString() const;
};

/** One scored candidate, kept for the "why did the AI do that?" debug overlay and the coach UI. */
USTRUCT(BlueprintType)
struct FFCBAiScoredMove
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AI")
	FFCBMove Move;

	UPROPERTY(BlueprintReadOnly, Category = "AI")
	float ExpectedValue = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "AI")
	float WinChance = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "AI")
	float ClashChance = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "AI")
	FString Explanation;

	bool operator<(const FFCBAiScoredMove& Other) const { return ExpectedValue > Other.ExpectedValue; }
};

class FANTASYCARDBATTLE_API FFCBAiAgent
{
public:
	/** Seed drives blunders and rollout sampling; pass the match seed for fully reproducible games. */
	void Initialize(const FFCBCardDatabase* InDatabase, int32 InSeat, const FFCBAiProfile& InProfile, uint32 InSeed);

	int32 GetSeat() const { return Seat; }
	const FFCBAiProfile& GetProfile() const { return Profile; }

	bool ChooseMove(const FFCBMatch& Match, FFCBMove& OutMove, FString& OutExplanation);

	/** Ranked list of candidate moves. The UI uses the top entry for the "coach" hint. */
	void ScoreAllMoves(const FFCBMatch& Match, TArray<FFCBAiScoredMove>& OutScored);

	/**
	 * Core evaluation. Public static so unit tests can pin down the model and the UI can show the
	 * expected value of the move a human is about to make.
	 */
	static float EvaluateMove(
		const FFCBMatch& Match,
		const FFCBAiProfile& Profile,
		const FFCBMove& Move,
		const TMap<int32, int32>& UnknownPool,
		float& OutWinChance,
		float& OutClashChance);

	/** Value of a single round given its projection, in "net cards" units. */
	static float ValueProjection(
		const FFCBMatchConfig& Config,
		const FFCBAiProfile& Profile,
		const FFCBRoundProjection& Projection,
		float MyCardQuality,
		float TheirCardQuality,
		int32 PotCards,
		int32 LoserHandSize,
		int32 DeckCards,
		int32 WinnerHandSize);

private:
	const FFCBCardDatabase* Database = nullptr;
	int32 Seat = FCBSeat::A;
	FFCBAiProfile Profile;
	FRandomStream Rng;

	/** The distribution the agent reasons about: the unseen multiset for Expert+, the printed pool for Adept. */
	void BuildUnknownPool(const FFCBMatch& Match, TMap<int32, int32>& OutIndexToCount) const;

	/** Sample a plausible opponent hand from the unknown pool (Legendary rollout input). */
	void SampleOpponentHand(
		const FFCBMatch& Match,
		const TMap<int32, int32>& UnknownPool,
		TArray<FFCBCardInstance>& OutHand);

	float BestReplyValue(
		const FFCBMatch& Match,
		const FFCBAiProfile& Profile,
		int32 OpponentSeat,
		const TArray<FFCBCardInstance>& SampledOpponentHand,
		const FFCBCardInstance& OurTopCardAfterMove) const;
};
