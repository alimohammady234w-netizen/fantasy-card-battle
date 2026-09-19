// Copyright (c) Fantasy Card Battle. All rights reserved.
//
// FCBAiAgent.cpp - move scoring for the AI opponent. See FCBAiAgent.h for the tier descriptions.
//
// The model: every candidate move is scored as an expected value in "net cards" units against a
// distribution of possible defender cards. Expert/Legendary use the true remaining unknown multiset
// (card counting), Adept uses the printed pool, Novice blunders, TortureTest plays the worst move
// so the balance harness has a lower bound to compare against.

#include "FCBAiAgent.h"

namespace
{
	FFCBAiProfile MakePreset(EFCBAiDifficulty InDifficulty)
	{
		FFCBAiProfile Profile;
		Profile.Difficulty = InDifficulty;
		switch (InDifficulty)
		{
		case EFCBAiDifficulty::Novice:
			Profile.BlunderPercent = 100;
			Profile.RiskAversion = 0.f;
			Profile.CardQualityWeight = 0.f;
			Profile.PotValueWeight = 0.5f;
			Profile.Elo = 800;
			break;

		case EFCBAiDifficulty::Adept:
			Profile.BlunderPercent = 12;
			Profile.RolloutSamples = 0;
			Profile.RiskAversion = 0.2f;
			Profile.CardQualityWeight = 0.3f;
			Profile.Elo = 1200;
			break;

		case EFCBAiDifficulty::Expert:
			Profile.BlunderPercent = 0;
			Profile.RolloutSamples = 0;
			Profile.RiskAversion = 0.4f;
			Profile.CardQualityWeight = 0.6f;
			Profile.Elo = 1600;
			break;

		case EFCBAiDifficulty::Legendary:
			Profile.BlunderPercent = 0;
			Profile.RolloutSamples = 8;
			Profile.RiskAversion = 0.5f;
			Profile.CardQualityWeight = 0.7f;
			Profile.ReplyPenaltyWeight = 0.6f;
			Profile.FinisherBonus = 3.5f;
			Profile.Elo = 1900;
			break;

		case EFCBAiDifficulty::TortureTest:
		default:
			Profile.BlunderPercent = 0;
			Profile.RolloutSamples = 0;
			Profile.RiskAversion = 0.f;
			Profile.CardQualityWeight = 0.f;
			Profile.PotValueWeight = 1.f;
			Profile.PotReclaimChance = 1.f;
			Profile.Elo = 1;   // deliberately weak: gives the harness a floor to measure skill against
			break;
		}
		return Profile;
	}

	int32 CountFactionIn(const TArray<FFCBCardInstance>& Pile, EFCBFaction Faction)
	{
		int32 Count = 0;
		for (const FFCBCardInstance& Card : Pile)
		{
			if (Card.Faction == Faction)
			{
				++Count;
			}
		}
		return Count;
	}

	/**
	 * Which card a seat is likely to hold on top *after* the round. Deliberately approximate: the draw
	 * pile is hidden, so we mirror the refill rule (playing index 0 shifts the hand up by one).
	 */
	bool ProjectedTopCard(const FFCBMatchState& State, int32 InSeat, int32 PlayedHandIndex, const FFCBCardInstance*& OutCard)
	{
		OutCard = nullptr;
		if (!State.Seats.IsValidIndex(InSeat))
		{
			return false;
		}
		const TArray<FFCBCardInstance>& Hand = State.Seats[InSeat].Hand;
		const int32 Index = (PlayedHandIndex == 0 && Hand.Num() > 1) ? 1 : 0;
		if (!Hand.IsValidIndex(Index))
		{
			return false;
		}
		OutCard = &Hand[Index];
		return true;
	}
}

const FFCBAiProfile& FFCBAiProfile::GetPreset(EFCBAiDifficulty InDifficulty)
{
	static const FFCBAiProfile Presets[] = {
		MakePreset(EFCBAiDifficulty::Novice),
		MakePreset(EFCBAiDifficulty::Adept),
		MakePreset(EFCBAiDifficulty::Expert),
		MakePreset(EFCBAiDifficulty::Legendary),
		MakePreset(EFCBAiDifficulty::TortureTest)
	};
	const int32 Count = static_cast<int32>(sizeof(Presets) / sizeof(FFCBAiProfile));
	const int32 Index = FMath::Clamp(static_cast<int32>(InDifficulty), 0, Count - 1);
	return Presets[FMath::Min(Index, Count - 1)];
}

FString FFCBAiProfile::ToString() const
{
	const TCHAR* Name = TEXT("Adept");
	switch (Difficulty)
	{
	case EFCBAiDifficulty::Novice:		Name = TEXT("Novice"); break;
	case EFCBAiDifficulty::Expert:		Name = TEXT("Expert"); break;
	case EFCBAiDifficulty::Legendary:	Name = TEXT("Legendary"); break;
	case EFCBAiDifficulty::TortureTest:	Name = TEXT("TortureTest"); break;
	default: break;
	}
	return FString::Printf(TEXT("%s (blunder %d%%, rollouts %d, risk %.2f, Elo %d)"),
		Name, BlunderPercent, RolloutSamples, RiskAversion, Elo);
}

void FFCBAiAgent::Initialize(const FFCBCardDatabase* InDatabase, int32 InSeat, const FFCBAiProfile& InProfile, uint32 InSeed)
{
	Database = InDatabase;
	Seat = InSeat;
	Profile = InProfile;
	// Separate stream: thinking must never advance the match RNG (this is what keeps replays exact).
	Rng = FRandomStream(static_cast<int32>((InSeed ^ (0x9e3779b9u * static_cast<uint32>(InSeat + 1))) & 0x7fffffffu));
}

float FFCBAiAgent::ValueProjection(
	const FFCBMatchConfig& Config,
	const FFCBAiProfile& Profile,
	const FFCBRoundProjection& Projection,
	float MyCardQuality,
	float TheirCardQuality,
	int32 PotCards,
	int32 LoserHandSize,
	int32 DeckCards,
	int32 WinnerHandSize)
{
	const EFCBRoundOutcome Outcome = Projection.Outcome;
	const float PotFloat = static_cast<float>(FMath::Max(0, PotCards));

	float Value = 0.f;
	if (Outcome == EFCBRoundOutcome::AttackerWins)
	{
		// The winner collects both played cards, so a win is worth two pile cards: theirs (plus a bonus when
		// it is a strong card) and our own returning to our pile.
		Value = 2.f + Profile.CardQualityWeight * TheirCardQuality;
		Value += Profile.PotValueWeight * PotFloat;
		if (Projection.bAttackerExtraCapture && LoserHandSize > 0)
		{
			Value += Profile.ExtraCaptureValue;
		}
		if (Projection.bAttackerTakesDeckTop && DeckCards > 0)
		{
			Value += Profile.DeckTopValue;
		}
		if (Projection.AttackerPotScoreBonus > 0 && PotCards > 0)
		{
			Value += 0.02f * static_cast<float>(Projection.AttackerPotScoreBonus * PotCards);
		}
	}
	else if (Outcome == EFCBRoundOutcome::DefenderWins)
	{
		Value = -(2.f + Profile.CardQualityWeight * MyCardQuality) - Profile.PotValueWeight * PotFloat;
		if (Projection.bDefenderExtraCapture && WinnerHandSize > 0)
		{
			Value -= Profile.ExtraCaptureValue;
		}
		if (Projection.bAttackerUndying)
		{
			// Our card refuses to leave the hand, so the loss is much cheaper (we only hand over nothing).
			Value += 1.6f;
		}
	}
	else
	{
		// Clash: nothing is captured now; the pot (including our card) is a later coin flip.
		Value = Profile.PotReclaimChance * Profile.PotValueWeight * PotFloat - 0.05f;
	}

	// End-of-deck urgency: taking their last card with nothing left in the draw pile wins outright.
	if (Config.EndCondition != EFCBEndCondition::FirstToScore
		&& Outcome == EFCBRoundOutcome::AttackerWins
		&& DeckCards == 0
		&& LoserHandSize <= 1)
	{
		Value += Profile.FinisherBonus;
	}
	return Value;
}

void FFCBAiAgent::BuildUnknownPool(const FFCBMatch& Match, TMap<int32, int32>& OutIndexToCount) const
{
	OutIndexToCount = TMap<int32, int32>();

	const bool bCountCards = Profile.Difficulty == EFCBAiDifficulty::Expert
		|| Profile.Difficulty == EFCBAiDifficulty::Legendary
		|| Profile.Difficulty == EFCBAiDifficulty::TortureTest;

	if (bCountCards)
	{
		Match.GetUnknownPool(Seat, OutIndexToCount);
		if (OutIndexToCount.Num() > 0)
		{
			return;
		}
		// Draw pile and both hands empty: the defender's actual card is the only possibility.
	}

	if (bCountCards || !Database)
	{
		// Fallback: reason only about the card that is actually going to be played against us.
		const FFCBMatchState& State = Match.GetState();
		const int32 DefenderSeat = FCBSeat::OpponentOf(Seat);
		if (State.Seats.IsValidIndex(DefenderSeat) && State.Seats[DefenderSeat].Hand.Num() > 0)
		{
			OutIndexToCount.Add(State.Seats[DefenderSeat].Hand[0].DefIndex, 1);
		}
		return;
	}

	// Adept / Novice: the printed pool minus everything the agent can see. No counting.
	for (int32 Index = 0; Index < Database->Cards.Num(); ++Index)
	{
		OutIndexToCount.Add(Index, 1);
	}
	const FFCBMatchState& State = Match.GetState();
	if (State.Seats.IsValidIndex(Seat))
	{
		for (const FFCBCardInstance& Card : State.Seats[Seat].Hand)
		{
			if (int32* Count = OutIndexToCount.Find(Card.DefIndex))
			{
				*Count -= 1;
				if (*Count <= 0)
				{
					OutIndexToCount.Remove(Card.DefIndex);
				}
			}
		}
	}
}

float FFCBAiAgent::EvaluateMove(
	const FFCBMatch& Match,
	const FFCBAiProfile& Profile,
	const FFCBMove& Move,
	const TMap<int32, int32>& UnknownPool,
	float& OutWinChance,
	float& OutClashChance)
{
	OutWinChance = 0.f;
	OutClashChance = 0.f;

	const FFCBCardDatabase* Db = Match.GetDatabase();
	if (!Db)
	{
		return 0.f;
	}
	const FFCBMatchState& State = Match.GetState();
	const FFCBMatchConfig& Config = Match.GetConfig();
	const int32 DefenderSeat = FCBSeat::OpponentOf(Move.Seat);
	if (!State.Seats.IsValidIndex(Move.Seat) || !State.Seats.IsValidIndex(DefenderSeat)
		|| !State.Seats[Move.Seat].Hand.IsValidIndex(Move.HandIndex))
	{
		return 0.f;
	}
	const FFCBCardInstance& MyCard = State.Seats[Move.Seat].Hand[Move.HandIndex];

	// One instantiation of each possible defender card, reused for every attribute.
	TArray<FFCBCardInstance> Candidates;
	TArray<int32> Weights;
	for (const TPair<int32, int32>& Pair : UnknownPool)
	{
		if (Pair.Value > 0)
		{
			FFCBCardInstance Instance;
			if (Db->MakeInstance(Pair.Key, Instance))
			{
				Candidates.Add(MoveTemp(Instance));
				Weights.Add(Pair.Value);
			}
		}
	}
	if (Candidates.Num() == 0)
	{
		return 0.f;
	}

	const int32 MyFactionPile = CountFactionIn(State.Seats[Move.Seat].Pile, MyCard.Faction);
	const int32 PotCards = State.Pot.Num();
	const int32 DeckCards = State.Deck.Num();
	const int32 TheirHandSize = State.Seats[DefenderSeat].Hand.Num();
	const int32 MyHandAfter = FMath::Max(0, State.Seats[Move.Seat].Hand.Num() - 1);
	const float MyQuality = Db->GetStrengthPercentile(MyCard.DefIndex);

	float TotalWeight = 0.f;
	float SumValue = 0.f;
	float SumSquare = 0.f;
	int32 WinCount = 0;
	int32 ClashCount = 0;

	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		const FFCBCardInstance& TheirCard = Candidates[Index];
		const float Weight = static_cast<float>(Weights[Index]);
		const int32 TheirFactionPile = CountFactionIn(State.Seats[DefenderSeat].Pile, TheirCard.Faction);

		FFCBRoundProjection Projection;
		FFCBMatch::ResolveRoundModifiers(Config, MyCard, TheirCard, Move.Attribute,
			MyFactionPile, TheirFactionPile,
			State.Seats[Move.Seat].Score, State.Seats[DefenderSeat].Score, Projection);

		const float Value = ValueProjection(Config, Profile, Projection,
			MyQuality,
			Db->GetStrengthPercentile(TheirCard.DefIndex),
			PotCards, TheirHandSize, DeckCards, MyHandAfter);

		TotalWeight += Weight;
		SumValue += Weight * Value;
		SumSquare += Weight * Value * Value;
		if (Projection.Outcome == EFCBRoundOutcome::AttackerWins)
		{
			WinCount += Weights[Index];
		}
		else if (Projection.Outcome == EFCBRoundOutcome::Clash)
		{
			ClashCount += Weights[Index];
		}
	}

	if (TotalWeight <= 0.f)
	{
		return 0.f;
	}

	const float ExpectedValue = SumValue / TotalWeight;
	const float Variance = FMath::Max(0.f, SumSquare / TotalWeight - ExpectedValue * ExpectedValue);
	OutWinChance = static_cast<float>(WinCount) / TotalWeight;
	OutClashChance = static_cast<float>(ClashCount) / TotalWeight;

	return ExpectedValue - Profile.RiskAversion * FMath::Sqrt(Variance);
}

void FFCBAiAgent::SampleOpponentHand(
	const FFCBMatch& Match,
	const TMap<int32, int32>& UnknownPool,
	TArray<FFCBCardInstance>& OutHand)
{
	OutHand.Reset();
	if (!Database)
	{
		return;
	}
	const int32 OpponentSeat = FCBSeat::OpponentOf(Seat);
	const FFCBMatchState& State = Match.GetState();
	if (!State.Seats.IsValidIndex(OpponentSeat))
	{
		return;
	}
	const int32 Want = State.Seats[OpponentSeat].Hand.Num();

	// Sample without replacement from the unknown multiset.
	TMap<int32, int32> Remaining = UnknownPool;
	TArray<int32> Keys;
	int32 TotalRemaining = 0;
	for (const TPair<int32, int32>& Pair : Remaining)
	{
		if (Pair.Value > 0)
		{
			Keys.Add(Pair.Key);
			TotalRemaining += Pair.Value;
		}
	}

	for (int32 Draw = 0; Draw < Want && TotalRemaining > 0 && Keys.Num() > 0; ++Draw)
	{
		int32 Pick = Rng.RandRange(0, TotalRemaining - 1);
		int32 ChosenKeyIndex = Keys.Num() - 1;
		for (int32 KeyIndex = 0; KeyIndex < Keys.Num(); ++KeyIndex)
		{
			if (int32* Count = Remaining.Find(Keys[KeyIndex]))
			{
				if (Pick < *Count)
				{
					ChosenKeyIndex = KeyIndex;
					break;
				}
				Pick -= *Count;
			}
		}

		const int32 ChosenKey = Keys[ChosenKeyIndex];
		FFCBCardInstance Instance;
		if (Database->MakeInstance(ChosenKey, Instance))
		{
			OutHand.Add(MoveTemp(Instance));
		}
		if (int32* Count = Remaining.Find(ChosenKey))
		{
			--(*Count);
			--TotalRemaining;
			if (*Count <= 0)
			{
				Remaining.Remove(ChosenKey);
				Keys.RemoveAt(ChosenKeyIndex);
			}
		}
	}
}

float FFCBAiAgent::BestReplyValue(
	const FFCBMatch& Match,
	const FFCBAiProfile& InProfile,
	int32 OpponentSeat,
	const TArray<FFCBCardInstance>& SampledOpponentHand,
	const FFCBCardInstance& OurTopCardAfterMove) const
{
	if (SampledOpponentHand.Num() == 0 || !Database)
	{
		return 0.f;
	}
	const FFCBMatchState& State = Match.GetState();
	const FFCBMatchConfig& Config = Match.GetConfig();
	const int32 OurSeat = FCBSeat::OpponentOf(OpponentSeat);
	const int32 PotCards = State.Pot.Num();
	const int32 DeckCards = State.Deck.Num();
	const int32 OurHandAfter = FMath::Max(0, State.Seats.IsValidIndex(OurSeat) ? State.Seats[OurSeat].Hand.Num() - 1 : 0);
	const int32 TheirScore = State.Seats.IsValidIndex(OpponentSeat) ? State.Seats[OpponentSeat].Score : 0;
	const int32 OurScore = State.Seats.IsValidIndex(OurSeat) ? State.Seats[OurSeat].Score : 0;

	float Best = -TNumericLimits<float>::Max();
	for (const FFCBCardInstance& TheirCard : SampledOpponentHand)
	{
		const int32 TheirFactionPile = State.Seats.IsValidIndex(OpponentSeat)
			? CountFactionIn(State.Seats[OpponentSeat].Pile, TheirCard.Faction) : 0;

		for (int32 AttrIndex = 0; AttrIndex < static_cast<int32>(EFCBAttribute::Max); ++AttrIndex)
		{
			const EFCBAttribute Attribute = static_cast<EFCBAttribute>(AttrIndex);
			FFCBRoundProjection Projection;
			const int32 OurFactionPile = State.Seats.IsValidIndex(OurSeat)
				? CountFactionIn(State.Seats[OurSeat].Pile, OurTopCardAfterMove.Faction) : 0;
			FFCBMatch::ResolveRoundModifiers(Config, TheirCard, OurTopCardAfterMove, Attribute,
				TheirFactionPile, OurFactionPile, TheirScore, OurScore, Projection);

			// Sign-flipped: what is good for them is bad for us.
			const float Value = -ValueProjection(Config, InProfile, Projection,
				Database->GetStrengthPercentile(TheirCard.DefIndex),
				Database->GetStrengthPercentile(OurTopCardAfterMove.DefIndex),
				PotCards, OurHandAfter, DeckCards, FMath::Max(0, SampledOpponentHand.Num() - 1));

			Best = FMath::Max(Best, Value);
		}
	}
	return Best < TNumericLimits<float>::Max() ? Best : 0.f;
}

void FFCBAiAgent::ScoreAllMoves(const FFCBMatch& Match, TArray<FFCBAiScoredMove>& OutScored)
{
	OutScored.Reset();
	if (!Database || !Match.IsStarted())
	{
		return;
	}

	TArray<FFCBMove> Legal;
	Match.GetLegalMoves(Seat, Legal);
	if (Legal.Num() == 0)
	{
		return;
	}

	TMap<int32, int32> UnknownPool;
	BuildUnknownPool(Match, UnknownPool);

	const int32 CandidateCap = FMath::Min(Legal.Num(), FMath::Max(4, Profile.MaxCandidatesToScore));
	const FFCBMatchState& State = Match.GetState();
	for (int32 Index = 0; Index < CandidateCap; ++Index)
	{
		FFCBAiScoredMove Scored;
		Scored.Move = Legal[Index];
		Scored.ExpectedValue = EvaluateMove(Match, Profile, Legal[Index], UnknownPool, Scored.WinChance, Scored.ClashChance);

		if (State.Seats.IsValidIndex(Seat) && State.Seats[Seat].Hand.IsValidIndex(Legal[Index].HandIndex))
		{
			Scored.Explanation = FString::Printf(TEXT("%s on %s: win %.0f%% clash %.0f%% EV %+.2f"),
				*State.Seats[Seat].Hand[Legal[Index].HandIndex].Name,
				*FCBAttributeUtil::ToString(Legal[Index].Attribute),
				Scored.WinChance * 100.f, Scored.ClashChance * 100.f, Scored.ExpectedValue);
		}
		OutScored.Add(Scored);
	}

	// Descending by EV. Stable insertion sort: candidate counts are small (<= MaxCandidatesToScore).
	for (int32 A = 1; A < OutScored.Num(); ++A)
	{
		FFCBAiScoredMove Item = OutScored[A];
		int32 B = A - 1;
		while (B >= 0 && OutScored[B].ExpectedValue < Item.ExpectedValue)
		{
			OutScored[B + 1] = OutScored[B];
			--B;
		}
		OutScored[B + 1] = Item;
	}
}

bool FFCBAiAgent::ChooseMove(const FFCBMatch& Match, FFCBMove& OutMove, FString& OutExplanation)
{
	OutExplanation.Reset();
	if (!Database || !Match.IsStarted())
	{
		OutExplanation = TEXT("AI has no card database.");
		return false;
	}
	if (Match.GetLeaderSeat() != Seat)
	{
		OutExplanation = FString::Printf(TEXT("Seat %d is not the leader."), Seat);
		return false;
	}

	TArray<FFCBMove> Legal;
	Match.GetLegalMoves(Seat, Legal);
	if (Legal.Num() == 0)
	{
		OutExplanation = TEXT("No legal moves.");
		return false;
	}

	const int32 BlunderRoll = Rng.RandRange(0, 99);
	if (Profile.Difficulty != EFCBAiDifficulty::TortureTest && BlunderRoll < Profile.BlunderPercent)
	{
		OutMove = Legal[Rng.RandRange(0, Legal.Num() - 1)];
		OutExplanation = TEXT("blunder: random legal move");
		return true;
	}

	TArray<FFCBAiScoredMove> Scored;
	ScoreAllMoves(Match, Scored);
	if (Scored.Num() == 0)
	{
		OutMove = Legal[0];
		OutExplanation = TEXT("no scored candidates; falling back to the first legal move");
		return true;
	}

	int32 BestIndex = 0;
	if (Profile.Difficulty == EFCBAiDifficulty::TortureTest)
	{
		BestIndex = Scored.Num() - 1;   // the worst move for us, on purpose
	}

	if (Profile.Difficulty == EFCBAiDifficulty::Legendary && Profile.RolloutSamples > 0)
	{
		TMap<int32, int32> UnknownPool;
		BuildUnknownPool(Match, UnknownPool);

		const int32 OpponentSeat = FCBSeat::OpponentOf(Seat);
		const int32 TopK = FMath::Min(Scored.Num(), 6);
		float BestValue = -TNumericLimits<float>::Max();
		for (int32 Candidate = 0; Candidate < TopK; ++Candidate)
		{
			float Adjusted = Scored[Candidate].ExpectedValue;

			const FFCBCardInstance* OurNextTop = nullptr;
			if (ProjectedTopCard(Match.GetState(), Seat, Scored[Candidate].Move.HandIndex, OurNextTop) && OurNextTop)
			{
				float ReplySum = 0.f;
				int32 SamplesUsed = 0;
				for (int32 Sample = 0; Sample < Profile.RolloutSamples; ++Sample)
				{
					TArray<FFCBCardInstance> TheirHand;
					SampleOpponentHand(Match, UnknownPool, TheirHand);
					if (TheirHand.Num() == 0)
					{
						continue;
					}
					ReplySum += BestReplyValue(Match, Profile, OpponentSeat, TheirHand, *OurNextTop);
					++SamplesUsed;
				}
				if (SamplesUsed > 0)
				{
					Adjusted += Profile.ReplyPenaltyWeight * (ReplySum / static_cast<float>(SamplesUsed));
				}
			}

			if (Adjusted > BestValue)
			{
				BestValue = Adjusted;
				BestIndex = Candidate;
			}
		}
	}

	OutMove = Scored[BestIndex].Move;
	OutExplanation = Scored[BestIndex].Explanation;
	if (Profile.Difficulty == EFCBAiDifficulty::TortureTest)
	{
		OutExplanation += TEXT(" [torture test: worst available move]");
	}
	return true;
}
