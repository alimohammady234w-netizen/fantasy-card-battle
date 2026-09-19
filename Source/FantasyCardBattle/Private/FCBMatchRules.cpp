// Copyright (c) Fantasy Card Battle. All rights reserved.
//
// FCBMatchRules.cpp - rules implementation. See FCBMatchRules.h for the contract.
//
// Order of operations for one round (this is the canonical order the tests assert on):
//   1. legality        - leader, hand index, defender has a card
//   2. projection      - printed values, ability modifiers, nullification, tiebreakers, triggers
//   3. outcome         - comparison -> tie rule -> clutch flip
//   4. capture         - loser card, then pot, then extra captures, then deck-top bonus
//   5. refill          - hands drawn back up to HandSize (winner first if configured)
//   6. score/history   - pile score, narration, end conditions

#include "FCBMatchRules.h"

namespace
{
	/** Integer percent with "half away from zero" rounding: identical in editor, shipping and headless builds. */
	int32 ApplyPercent(int32 Base, int32 Percent)
	{
		const int64 Scaled = static_cast<int64>(Base) * static_cast<int64>(Percent);
		const int64 Half = Scaled >= 0 ? 50 : -50;
		return static_cast<int32>((Scaled + Half) / 100);
	}

	const FFCBCardAbility* FindAbility(const FFCBCardInstance& Card, EFCBAbilityEffect Effect)
	{
		for (const FFCBCardAbility& Ability : Card.Abilities)
		{
			if (Ability.Effect == Effect && !Ability.IsNone())
			{
				return &Ability;
			}
		}
		return nullptr;
	}

	bool HasEffect(const FFCBCardInstance& Card, EFCBAbilityEffect Effect)
	{
		return FindAbility(Card, Effect) != nullptr;
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

	const TCHAR* OutcomeToString(EFCBRoundOutcome Outcome)
	{
		switch (Outcome)
		{
		case EFCBRoundOutcome::AttackerWins:	return TEXT("attacker wins");
		case EFCBRoundOutcome::DefenderWins:	return TEXT("defender wins");
		case EFCBRoundOutcome::Clash:			return TEXT("clash");
		default:								return TEXT("invalid");
		}
	}
}

FString FFCBMatchConfig::ToString() const
{
	const TCHAR* TieText = TEXT("Clash/Pot");
	switch (TieRule)
	{
	case EFCBTieRule::HigherRarityWins:	TieText = TEXT("HigherRarity"); break;
	case EFCBTieRule::LeaderWinsTies:	TieText = TEXT("LeaderWinsTies"); break;
	case EFCBTieRule::BurnNoCapture:	TieText = TEXT("Burn"); break;
	case EFCBTieRule::SplitKeep:		TieText = TEXT("SplitKeep"); break;
	default: break;
	}

	const TCHAR* EndText = TEXT("LastStanding");
	switch (EndCondition)
	{
	case EFCBEndCondition::RoundLimitScore:	EndText = TEXT("RoundLimit"); break;
	case EFCBEndCondition::FirstToScore:		EndText = TEXT("FirstToScore"); break;
	default: break;
	}

	return FString::Printf(
		TEXT("Hand=%d Tie=%s End=%s MaxRounds=%d Abilities=%s ExtraCap=%s RarityScore=%s PotCap=%d Seed=%u Seats='%s' vs '%s'"),
		HandSize, TieText, EndText, MaxRounds,
		bEnableAbilities ? TEXT("on") : TEXT("off"),
		bEnableExtraCaptures ? TEXT("on") : TEXT("off"),
		bRarityScoreWeights ? TEXT("on") : TEXT("off"),
		PotCap, RngSeed, *SeatAName, *SeatBName);
}

FFCBMatch::FFCBMatch()
{
	State.Seats.SetNum(FCBSeat::Count);
}

void FFCBMatch::Configure(const FFCBMatchConfig& InConfig, const FFCBCardDatabase* InDatabase)
{
	Config = InConfig;
	Database = InDatabase;

	State = FFCBMatchState();
	State.Seats.SetNum(FCBSeat::Count);
	for (int32 Seat = 0; Seat < FCBSeat::Count; ++Seat)
	{
		State.Seats[Seat].Name = Config.GetSeatName(Seat);
		State.Seats[Seat].bIsHuman = Config.IsHumanSeat(Seat);
	}

	DealtCounts.Reset();
	RngState = 0u;
	Rng = FRandomStream(static_cast<int32>(Config.RngSeed & 0x7fffffffu));
}

bool FFCBMatch::StartMatch(FString& OutError)
{
	if (!Database || !Database->IsReady())
	{
		OutError = TEXT("Card database is not loaded or reported fatal data issues.");
		return false;
	}
	if (Config.HandSize < 2)
	{
		OutError = TEXT("HandSize must be at least 2.");
		return false;
	}
	if (Config.PotCap < 1)
	{
		OutError = TEXT("PotCap must be at least 1.");
		return false;
	}
	if (!Config.bDefenderPlaysTopCard)
	{
		// v1 ships leader-only selection. The flag exists so the future netplay variant has a home.
		AppendLog(TEXT("Rule note: simultaneous defender selection is not implemented in v1; using classic top-card defence."));
	}

	TArray<int32> Pool;
	Database->CollectEligibleCards(Config.DeckFilter, Config.RngSeed, Pool);
	if (Pool.Num() < Config.HandSize * FCBSeat::Count)
	{
		OutError = FString::Printf(TEXT("Not enough eligible cards: found %d, need %d."),
			Pool.Num(), Config.HandSize * FCBSeat::Count);
		return false;
	}

	// Fisher-Yates over the seeded stream: identical ordering in editor, packaged and headless runs.
	for (int32 Index = Pool.Num() - 1; Index > 0; --Index)
	{
		const int32 SwapTo = Rng.RandRange(0, Index);
		Pool.Swap(Index, SwapTo);
		++RngState;
	}

	// Deal the opening hands from the front of the shuffled pool, balance *those* hands, then use the
	// remainder as the shared draw pile. Balancing after a full split would not work: each hand is only a
	// random slice of its seat's half, so the visible strength gap would stay around 15%.
	// StartMatch is re-entrant: "New Match" calls it again without Configure, so every per-match container
	// has to be cleared here (and the stream re-seeded, so the same seed replays the same match).
	State.Deck.Reset();
	DealtCounts.Reset();
	Rng = FRandomStream(static_cast<int32>(Config.RngSeed & 0x7fffffffu));
	RngState = 0u;

	TArray<FFCBCardInstance> OpeningHands[FCBSeat::Count];
	for (int32 Index = 0; Index < Pool.Num(); ++Index)
	{
		FFCBCardInstance Instance;
		if (!MakeInstanceFromCardDef(Pool[Index], Instance))
		{
			OutError = FString::Printf(TEXT("Card index %d could not be instantiated."), Pool[Index]);
			return false;
		}
		DealtCounts.FindOrAdd(Pool[Index]) += 1;

		if (Index < Config.HandSize * FCBSeat::Count)
		{
			OpeningHands[Index % FCBSeat::Count].Add(MoveTemp(Instance));
		}
		else
		{
			State.Deck.Add(MoveTemp(Instance));
		}
	}

	if (Config.bBalanceSeats)
	{
		BalanceSeatHands(OpeningHands[FCBSeat::A], OpeningHands[FCBSeat::B]);
	}

	for (int32 Seat = 0; Seat < FCBSeat::Count; ++Seat)
	{
		FFCBSeatState& SeatState = State.Seats[Seat];
		SeatState.Name = Config.GetSeatName(Seat);
		SeatState.bIsHuman = Config.IsHumanSeat(Seat);
		SeatState.Hand = MoveTemp(OpeningHands[Seat]);
		SeatState.Pile.Reset();
		SeatState.Score = 0;
		SeatState.BonusScore = 0;
		SeatState.RoundsWon = 0;
		SeatState.ClashCount = 0;
		SeatState.PotLostCount = 0;
	}

	State.Pot.Reset();
	State.Burned.Reset();
	State.History.Reset();
	State.Log.Reset();
	State.RoundNumber = 0;
	State.bMatchOver = false;
	State.WinnerSeat = INDEX_NONE;
	State.EndReason.Reset();
	State.LeaderSeat = FMath::Clamp(Config.OpeningSeat, 0, FCBSeat::Count - 1);

	AppendLog(FString::Printf(TEXT("Match start | %s"), *Config.ToString()));
	AppendLog(FString::Printf(TEXT("Match deck: %d cards (%d per hand, %d in the draw pile) | leader: %s"),
		Pool.Num(), Config.HandSize, State.Deck.Num(), *State.Seats[State.LeaderSeat].Name));

	return true;
}

bool FFCBMatch::MakeInstanceFromCardDef(int32 CardIndex, FFCBCardInstance& OutInstance) const
{
	return Database ? Database->MakeInstance(CardIndex, OutInstance) : false;
}

void FFCBMatch::BalanceSeatHands(TArray<FFCBCardInstance>& InOutHandA, TArray<FFCBCardInstance>& InOutHandB)
{
	if (!Database || InOutHandA.Num() == 0 || InOutHandB.Num() == 0)
	{
		return;
	}

	// Snake draft over the two opening hands: merge, order by strength, then deal A,B,B,A,A,B,B,A...
	// This is the standard way to equalise two halves and it beats greedy swapping (which stalls at the
	// first local optimum). The order *within* each hand is re-shuffled afterwards, because hand[0] is the
	// auto-defence card and it must not become predictable from the strength ordering.
	TArray<FFCBCardInstance> Combined;
	Combined.Reserve(InOutHandA.Num() + InOutHandB.Num());
	for (FFCBCardInstance& Card : InOutHandA)
	{
		Combined.Add(MoveTemp(Card));
	}
	for (FFCBCardInstance& Card : InOutHandB)
	{
		Combined.Add(MoveTemp(Card));
	}
	InOutHandA.Reset();
	InOutHandB.Reset();

	auto StrengthOf = [this](const FFCBCardInstance& Card) -> float
	{
		return Database->GetStrengthPercentile(Card.DefIndex);
	};

	// Selection sort, strongest first (24-ish items, so the cost is irrelevant and it stays dependency-free).
	for (int32 A = 0; A < Combined.Num(); ++A)
	{
		int32 Best = A;
		for (int32 B = A + 1; B < Combined.Num(); ++B)
		{
			if (StrengthOf(Combined[B]) > StrengthOf(Combined[Best]))
			{
				Best = B;
			}
		}
		Combined.Swap(A, Best);
	}

	for (int32 Index = 0; Index < Combined.Num(); ++Index)
	{
		const int32 Row = Index / FCBSeat::Count;
		const int32 PositionInRow = Index % FCBSeat::Count;
		const int32 Seat = (Row % 2 == 0) ? PositionInRow : (FCBSeat::Count - 1 - PositionInRow);
		(Seat == FCBSeat::A ? InOutHandA : InOutHandB).Add(MoveTemp(Combined[Index]));
	}

	// Shuffle each opening hand so the auto-defence order is not derived from the draft.
	auto ShuffleHand = [this](TArray<FFCBCardInstance>& Hand)
	{
		for (int32 Index = Hand.Num() - 1; Index > 0; --Index)
		{
			const int32 SwapTo = Rng.RandRange(0, Index);
			Hand.Swap(Index, SwapTo);
			++RngState;
		}
	};
	ShuffleHand(InOutHandA);
	ShuffleHand(InOutHandB);
}

bool FFCBMatch::GetDefenderCardForMove(const FFCBMove& Move, int32& OutHandIndex) const
{
	OutHandIndex = INDEX_NONE;
	if (!IsStarted() || !State.Seats.IsValidIndex(Move.Seat))
	{
		return false;
	}
	const int32 DefenderSeat = FCBSeat::OpponentOf(Move.Seat);
	if (!State.Seats.IsValidIndex(DefenderSeat) || State.Seats[DefenderSeat].Hand.Num() == 0)
	{
		return false;
	}
	// Classic defence: the top card of the hand. The UI shows the hand in order, so this stays readable.
	OutHandIndex = 0;
	return true;
}

bool FFCBMatch::IsLegalMove(const FFCBMove& Move) const
{
	if (!IsStarted() || State.bMatchOver || !Move.IsValid() || Move.Seat != State.LeaderSeat)
	{
		return false;
	}
	if (!State.Seats.IsValidIndex(Move.Seat) || !State.Seats[Move.Seat].Hand.IsValidIndex(Move.HandIndex))
	{
		return false;
	}
	int32 DefenderIndex = INDEX_NONE;
	return GetDefenderCardForMove(Move, DefenderIndex);
}

void FFCBMatch::GetLegalMoves(int32 Seat, TArray<FFCBMove>& OutMoves) const
{
	OutMoves.Reset();
	if (!IsStarted() || State.bMatchOver || Seat != State.LeaderSeat || !State.Seats.IsValidIndex(Seat))
	{
		return;
	}
	int32 DefenderIndex = INDEX_NONE;
	if (!GetDefenderCardForMove(FFCBMove::Make(Seat, 0, EFCBAttribute::Power), DefenderIndex))
	{
		return;
	}
	for (int32 HandIndex = 0; HandIndex < State.Seats[Seat].Hand.Num(); ++HandIndex)
	{
		for (int32 AttrIndex = 0; AttrIndex < static_cast<int32>(EFCBAttribute::Max); ++AttrIndex)
		{
			OutMoves.Add(FFCBMove::Make(Seat, HandIndex, static_cast<EFCBAttribute>(AttrIndex)));
		}
	}
}

void FFCBMatch::ResolveRoundModifiers(
	const FFCBMatchConfig& Config,
	const FFCBCardInstance& Attacker,
	const FFCBCardInstance& Defender,
	EFCBAttribute Attribute,
	int32 AttackerFactionPileCount,
	int32 DefenderFactionPileCount,
	int32 AttackerScore,
	int32 DefenderScore,
	FFCBRoundProjection& OutProjection)
{
	OutProjection = FFCBRoundProjection();
	OutProjection.Attribute = Attribute;
	OutProjection.AttackerPrinted = Attacker.Stats.Get(Attribute);
	OutProjection.DefenderPrinted = Defender.Stats.Get(Attribute);
	OutProjection.AttackerEffective = OutProjection.AttackerPrinted;
	OutProjection.DefenderEffective = OutProjection.DefenderPrinted;
	OutProjection.AttackerRarityIndex = FCBRarityUtil::ToIndex(Attacker.Rarity);
	OutProjection.DefenderRarityIndex = FCBRarityUtil::ToIndex(Defender.Rarity);

	if (!Config.bEnableAbilities)
	{
		// The "vanilla" ladder still needs a finalized outcome: callers must never have to know whether the
		// rules asset has abilities on or off.
		OutProjection.Outcome = FinalizeOutcome(Config, OutProjection);
		return;
	}

	// Nullification must be known before any modifier is applied.
	const bool bAttackerNullifies = HasEffect(Attacker, EFCBAbilityEffect::NullifyOpponentAbilities);
	const bool bDefenderNullifies = HasEffect(Defender, EFCBAbilityEffect::NullifyOpponentAbilities);
	OutProjection.bAttackerNullified = bDefenderNullifies;
	OutProjection.bDefenderNullified = bAttackerNullifies;
	if (bAttackerNullifies)
	{
		OutProjection.Notes.Add(FString::Printf(TEXT("%s voids %s's abilities this round"), *Attacker.Name, *Defender.Name));
	}
	if (bDefenderNullifies)
	{
		OutProjection.Notes.Add(FString::Printf(TEXT("%s voids %s's abilities this round"), *Defender.Name, *Attacker.Name));
	}

	auto ApplySide = [&OutProjection, &Config, &Attribute, AttackerScore, DefenderScore](
		const FFCBCardInstance& Card,
		bool bIsAttacker,
		int32 FactionPileCount,
		const FFCBCardInstance& Opponent,
		bool bNullified)
	{
		if (bNullified)
		{
			return;
		}

		int32& RunningModifier = bIsAttacker ? OutProjection.AttackerModifier : OutProjection.DefenderModifier;
		const int32 Printed = bIsAttacker ? OutProjection.AttackerPrinted : OutProjection.DefenderPrinted;
		const int32 OpponentValue = Opponent.Stats.Get(Attribute);

		for (const FFCBCardAbility& Ability : Card.Abilities)
		{
			switch (Ability.Effect)
			{
			case EFCBAbilityEffect::FlatBonusOnAttribute:
				if (Ability.Attribute == Attribute && Ability.ParamA != 0)
				{
					RunningModifier += Ability.ParamA;
					OutProjection.Notes.Add(FString::Printf(TEXT("%s %+d on %s [%s]"),
						*Card.Name, Ability.ParamA, *FCBAttributeUtil::ToString(Attribute), *Ability.DisplayName));
				}
				break;

			case EFCBAbilityEffect::PercentBonusOnAttribute:
				if (Ability.Attribute == Attribute)
				{
					const int32 Delta = ApplyPercent(Printed, Ability.ParamA);
					RunningModifier += Delta;
					OutProjection.Notes.Add(FString::Printf(TEXT("%s +%d%% of %d => %+d [%s]"),
						*Card.Name, Ability.ParamA, Printed, Delta, *Ability.DisplayName));
				}
				break;

			case EFCBAbilityEffect::BonusPerFactionInOwnPile:
				{
					const int32 MaxStacks = Ability.ParamB > 0 ? Ability.ParamB : 99;
					const int32 Stacks = FMath::Min(MaxStacks, FactionPileCount);
					if (Stacks > 0)
					{
						const int32 Delta = ApplyPercent(Printed, Ability.ParamA * Stacks);
						RunningModifier += Delta;
						OutProjection.Notes.Add(FString::Printf(TEXT("%s +%d%% x%d trophies => %+d [%s]"),
							*Card.Name, Ability.ParamA, Stacks, Delta, *Ability.DisplayName));
					}
				}
				break;

			case EFCBAbilityEffect::BonusAgainstFaction:
				if (Opponent.Faction == Ability.Faction)
				{
					const int32 Delta = ApplyPercent(Printed, Ability.ParamA);
					RunningModifier += Delta;
					OutProjection.Notes.Add(FString::Printf(TEXT("%s +%d%% bane vs %s => %+d [%s]"),
						*Card.Name, Ability.ParamA,
						*FCBFactionUtil::ToString(Ability.Faction), Delta, *Ability.DisplayName));
				}
				break;

			case EFCBAbilityEffect::BonusIfOpponentStatAbove:
				if (OpponentValue > Ability.ParamB)
				{
					const int32 Delta = ApplyPercent(Printed, Ability.ParamA);
					RunningModifier += Delta;
					OutProjection.Notes.Add(FString::Printf(TEXT("%s +%d%% (opponent %s %d > %d) => %+d [%s]"),
						*Card.Name, Ability.ParamA, *FCBAttributeUtil::ToString(Attribute),
						OpponentValue, Ability.ParamB, Delta, *Ability.DisplayName));
				}
				break;

			case EFCBAbilityEffect::PercentBonusWhenLeading:
				// "Leading" is measured on captures, so a player who is ahead pushes the advantage. This is
				// deliberate: it makes the endgame snowball, which the rule doc sells as the comeback window
				// for Fey Trickery on the other side.
				if (bIsAttacker && AttackerScore > DefenderScore)
				{
					const int32 Delta = ApplyPercent(Printed, Ability.ParamA);
					RunningModifier += Delta;
					OutProjection.Notes.Add(FString::Printf(TEXT("%s +%d%% on the lead => %+d [%s]"),
						*Card.Name, Ability.ParamA, Delta, *Ability.DisplayName));
				}
				break;

			case EFCBAbilityEffect::PercentBonusWhenDefending:
				if (!bIsAttacker)
				{
					const int32 Delta = ApplyPercent(Printed, Ability.ParamA);
					RunningModifier += Delta;
					OutProjection.Notes.Add(FString::Printf(TEXT("%s +%d%% while defending => %+d [%s]"),
						*Card.Name, Ability.ParamA, Delta, *Ability.DisplayName));
				}
				break;

			case EFCBAbilityEffect::AlwaysPenalty:
				{
					const int32 Delta = ApplyPercent(Printed, Ability.ParamA);   // ParamA is a negative percent
					RunningModifier += Delta;
					OutProjection.Notes.Add(FString::Printf(TEXT("%s %+d%% always => %+d [%s]"),
						*Card.Name, Ability.ParamA, Delta, *Ability.DisplayName));
				}
				break;

			case EFCBAbilityEffect::WinsTiesOnAttribute:
				if (Ability.Attribute == Attribute)
				{
					if (bIsAttacker) { OutProjection.bAttackerWinsTies = true; }
					else { OutProjection.bDefenderWinsTies = true; }
					OutProjection.Notes.Add(FString::Printf(TEXT("%s takes %s ties [%s]"),
						*Card.Name, *FCBAttributeUtil::ToString(Attribute), *Ability.DisplayName));
				}
				break;

			case EFCBAbilityEffect::WinIfMarginWithin:
				// Authored as a percent window; convert to points here so FinalizeOutcome stays scale-free.
				{
					const int32 Window = FMath::Max(1, ApplyPercent(FMath::Max(Printed, OpponentValue), Ability.ParamA));
					if (!bIsAttacker)
					{
						OutProjection.DefenderClutchMargin = Window;
					}
					else
					{
						OutProjection.AttackerClutchMargin = Window;
					}
				}
				break;

			case EFCBAbilityEffect::OnWinTakeDeckTop:
				if (bIsAttacker) { OutProjection.bAttackerTakesDeckTop = true; }
				else { OutProjection.bDefenderTakesDeckTop = true; }
				break;

			case EFCBAbilityEffect::OnWinForceExtraCapture:
				if (Config.bEnableExtraCaptures)
				{
					if (bIsAttacker) { OutProjection.bAttackerExtraCapture = true; }
					else { OutProjection.bDefenderExtraCapture = true; }
				}
				break;

			case EFCBAbilityEffect::PotScoreBonusOnWin:
				if (bIsAttacker) { OutProjection.AttackerPotScoreBonus += Ability.ParamA; }
				else { OutProjection.DefenderPotScoreBonus += Ability.ParamA; }
				break;

			case EFCBAbilityEffect::OnLoseReturnToHandOnce:
				{
					const bool bUsable = Card.RemainingCharges > 0;
					if (bIsAttacker) { OutProjection.bAttackerUndying = bUsable; }
					else { OutProjection.bDefenderUndying = bUsable; }
					if (!bUsable)
					{
						OutProjection.Notes.Add(FString::Printf(TEXT("%s: %s already spent this match"),
							*Card.Name, *Ability.DisplayName));
					}
				}
				break;

			default:
				break;
			}
		}
	};

	ApplySide(Attacker, true, AttackerFactionPileCount, Defender, OutProjection.bAttackerNullified);
	ApplySide(Defender, false, DefenderFactionPileCount, Attacker, OutProjection.bDefenderNullified);

	OutProjection.AttackerEffective = OutProjection.AttackerPrinted + OutProjection.AttackerModifier;
	OutProjection.DefenderEffective = OutProjection.DefenderPrinted + OutProjection.DefenderModifier;

	// Never let a penalty push a contested value below 1: "higher wins" must stay monotonic.
	OutProjection.AttackerEffective = FMath::Max(1, OutProjection.AttackerEffective);
	OutProjection.DefenderEffective = FMath::Max(1, OutProjection.DefenderEffective);

	// The outcome is part of the projection: every caller (PlayMove, the UI preview, the AI's 48-way move
	// scan) must see the tie rule and clutch flips applied. Doing it here instead of in each caller is what
	// keeps the AI from ever scoring a stale, unfinalized projection as "no winner".
	OutProjection.Outcome = FinalizeOutcome(Config, OutProjection);
}

EFCBRoundOutcome FFCBMatch::FinalizeOutcome(const FFCBMatchConfig& Config, const FFCBRoundProjection& Projection)
{
	EFCBRoundOutcome Outcome = Projection.GetRawOutcome();

	if (Outcome == EFCBRoundOutcome::Clash)
	{
		switch (Config.TieRule)
		{
		case EFCBTieRule::LeaderWinsTies:
			Outcome = EFCBRoundOutcome::AttackerWins;
			break;

		case EFCBTieRule::HigherRarityWins:
			if (Projection.AttackerRarityIndex != Projection.DefenderRarityIndex)
			{
				Outcome = Projection.AttackerRarityIndex > Projection.DefenderRarityIndex
					? EFCBRoundOutcome::AttackerWins
					: EFCBRoundOutcome::DefenderWins;
			}
			// Equal rarity stays a clash (documented in Docs/GameDesign.md).
			break;

		case EFCBTieRule::PotClash:
		case EFCBTieRule::BurnNoCapture:
		case EFCBTieRule::SplitKeep:
		default:
			// All three keep "no winner"; PlayMove decides where the cards physically go.
			Outcome = EFCBRoundOutcome::Clash;
			break;
		}
	}
	else if (Outcome == EFCBRoundOutcome::AttackerWins)
	{
		// Fey Trickery: a narrow defensive loss is flipped into a win.
		const int32 Margin = Projection.GetMargin();
		if (Projection.DefenderClutchMargin >= 0 && !Projection.bDefenderNullified
			&& Margin > 0 && Margin <= Projection.DefenderClutchMargin)
		{
			Outcome = EFCBRoundOutcome::DefenderWins;
		}
	}

	return Outcome;
}

bool FFCBMatch::ProjectMove(const FFCBMove& Move, FFCBRoundProjection& OutProjection) const
{
	OutProjection = FFCBRoundProjection();
	if (!IsStarted() || !State.Seats.IsValidIndex(Move.Seat))
	{
		return false;
	}
	const int32 DefenderSeat = FCBSeat::OpponentOf(Move.Seat);
	if (!State.Seats.IsValidIndex(DefenderSeat))
	{
		return false;
	}

	int32 DefenderHandIndex = INDEX_NONE;
	if (!GetDefenderCardForMove(Move, DefenderHandIndex) || !State.Seats[Move.Seat].Hand.IsValidIndex(Move.HandIndex))
	{
		return false;
	}

	const FFCBCardInstance& Attacker = State.Seats[Move.Seat].Hand[Move.HandIndex];
	const FFCBCardInstance& Defender = State.Seats[DefenderSeat].Hand[DefenderHandIndex];

	const int32 AttackerFactionPile = HasEffect(Attacker, EFCBAbilityEffect::BonusPerFactionInOwnPile)
		? CountFactionIn(State.Seats[Move.Seat].Pile, Attacker.Faction) : 0;
	const int32 DefenderFactionPile = HasEffect(Defender, EFCBAbilityEffect::BonusPerFactionInOwnPile)
		? CountFactionIn(State.Seats[DefenderSeat].Pile, Defender.Faction) : 0;

	ResolveRoundModifiers(Config, Attacker, Defender, Move.Attribute,
		AttackerFactionPile, DefenderFactionPile,
		State.Seats[Move.Seat].Score, State.Seats[DefenderSeat].Score, OutProjection);

	OutProjection.Outcome = FinalizeOutcome(Config, OutProjection);
	return true;
}

bool FFCBMatch::PlayMove(const FFCBMove& Move, FString& OutError)
{
	if (!IsStarted())
	{
		OutError = TEXT("Match not started.");
		return false;
	}
	if (State.bMatchOver)
	{
		OutError = TEXT("Match is already over.");
		return false;
	}
	if (!IsLegalMove(Move))
	{
		OutError = FString::Printf(TEXT("Illegal move: %s (leader is seat %d)"), *Move.ToString(), State.LeaderSeat);
		return false;
	}

	const int32 AttackerSeat = Move.Seat;
	const int32 DefenderSeat = FCBSeat::OpponentOf(AttackerSeat);
	int32 DefenderHandIndex = INDEX_NONE;
	GetDefenderCardForMove(Move, DefenderHandIndex);

	FFCBRoundProjection Projection;
	ProjectMove(Move, Projection);

	const FFCBCardInstance AttackerSnapshot = State.Seats[AttackerSeat].Hand[Move.HandIndex];
	const FFCBCardInstance DefenderSnapshot = State.Seats[DefenderSeat].Hand[DefenderHandIndex];
	const int32 ScoreBeforeA = State.Seats[FCBSeat::A].Score;
	const int32 ScoreBeforeB = State.Seats[FCBSeat::B].Score;
	const int32 PotBefore = State.Pot.Num();

	const EFCBRoundOutcome Outcome = Projection.Outcome;

	FFCBRoundResult Result;
	Result.RoundNumber = State.RoundNumber + 1;
	Result.Move = Move;
	Result.AttackerCardId = AttackerSnapshot.Id;
	Result.DefenderCardId = DefenderSnapshot.Id;
	Result.AttackerCardName = AttackerSnapshot.Name;
	Result.DefenderCardName = DefenderSnapshot.Name;
	Result.Outcome = Outcome;
	Result.Projection = Projection;

	TArray<FString>& Notes = Result.Projection.Notes;
	if (Outcome == EFCBRoundOutcome::AttackerWins && Projection.DefenderClutchMargin >= 0
		&& !Projection.bDefenderNullified
		&& Projection.GetMargin() > 0 && Projection.GetMargin() <= Projection.DefenderClutchMargin)
	{
		Notes.Add(FString::Printf(TEXT("%s claws back a %d-point loss (Win If Margin Within %d)"),
			*DefenderSnapshot.Name, Projection.GetMargin(), Projection.DefenderClutchMargin));
	}

	// --- remove both contested cards from their hands ------------------------------------------------
	TArray<FFCBCardInstance>& AttackerHand = State.Seats[AttackerSeat].Hand;
	FFCBCardInstance AttackerPlayed = MoveTemp(AttackerHand[Move.HandIndex]);
	AttackerHand.RemoveAt(Move.HandIndex);

	TArray<FFCBCardInstance>& DefenderHand = State.Seats[DefenderSeat].Hand;
	// The defender's index can only be 0 here (top-card defence), but re-validate defensively.
	const int32 SafeDefenderIndex = DefenderHand.IsValidIndex(DefenderHandIndex) ? DefenderHandIndex : 0;
	FFCBCardInstance DefenderPlayed = MoveTemp(DefenderHand[SafeDefenderIndex]);
	DefenderHand.RemoveAt(SafeDefenderIndex);

	if (Outcome == EFCBRoundOutcome::AttackerWins || Outcome == EFCBRoundOutcome::DefenderWins)
	{
		const bool bAttackerWon = (Outcome == EFCBRoundOutcome::AttackerWins);
		const int32 WinnerSeat = bAttackerWon ? AttackerSeat : DefenderSeat;
		const int32 LoserSeat = bAttackerWon ? DefenderSeat : AttackerSeat;
		FFCBCardInstance& WinnerCard = bAttackerWon ? AttackerPlayed : DefenderPlayed;
		FFCBCardInstance& LoserCard = bAttackerWon ? DefenderPlayed : AttackerPlayed;
		const FString WinnerCardName = WinnerCard.Name;
		TArray<FFCBCardInstance>& LoserHand = bAttackerWon ? DefenderHand : AttackerHand;

		const bool bWinnerTakesDeckTop = bAttackerWon ? Projection.bAttackerTakesDeckTop : Projection.bDefenderTakesDeckTop;
		const bool bWinnerExtraCapture = bAttackerWon ? Projection.bAttackerExtraCapture : Projection.bDefenderExtraCapture;
		const bool bLoserUndying = bAttackerWon ? Projection.bDefenderUndying : Projection.bAttackerUndying;
		const int32 WinnerPotBonus = bAttackerWon ? Projection.AttackerPotScoreBonus : Projection.DefenderPotScoreBonus;

		// Classic economy: the winner collects BOTH played cards. Their own card therefore never leaves the
		// match - on a win it lands in their pile (and scores), which is why losing a duel is strictly worse
		// than winning one even though both plays remove a card from the hand.
		CaptureToPile(WinnerSeat, MoveTemp(WinnerCard), Result);

		const FString LoserCardName = LoserCard.Name;
		if (bLoserUndying)
		{
			LoserCard.RemainingCharges -= 1;
			LoserCard.LastModifierNote = TEXT("Answered the call of its master");
			LoserHand.Add(MoveTemp(LoserCard));
			Notes.Add(FString::Printf(TEXT("%s escapes capture and returns to %s's hand"),
				*LoserCardName, *State.Seats[LoserSeat].Name));
		}
		else
		{
			CaptureToPile(WinnerSeat, MoveTemp(LoserCard), Result);
		}

		if (State.Pot.Num() > 0)
		{
			const int32 PotCards = State.Pot.Num();
			for (int32 Index = 0; Index < State.Pot.Num(); ++Index)
			{
				CaptureToPile(WinnerSeat, MoveTemp(State.Pot[Index]), Result);
			}
			State.Pot.Reset();
			Result.PotCardsAwarded = PotCards;
			Notes.Add(FString::Printf(TEXT("%s claims the pot of %d card(s)"),
				*State.Seats[WinnerSeat].Name, PotCards));
			if (WinnerPotBonus > 0)
			{
				State.Seats[WinnerSeat].BonusScore += WinnerPotBonus * PotCards;
				Notes.Add(FString::Printf(TEXT("+%d bonus score from pot greed"), WinnerPotBonus * PotCards));
			}
		}

		if (bWinnerExtraCapture && State.Seats[LoserSeat].Hand.Num() > 0)
		{
			int32 WeakestIndex = 0;
			int32 WeakestValue = TNumericLimits<int32>::Max();
			for (int32 Index = 0; Index < State.Seats[LoserSeat].Hand.Num(); ++Index)
			{
				const int32 Value = State.Seats[LoserSeat].Hand[Index].Stats.Get(Move.Attribute);
				if (Value < WeakestValue)
				{
					WeakestValue = Value;
					WeakestIndex = Index;
				}
			}
			const FString ExtraName = State.Seats[LoserSeat].Hand[WeakestIndex].Name;
			FFCBCardInstance Extra = MoveTemp(State.Seats[LoserSeat].Hand[WeakestIndex]);
			State.Seats[LoserSeat].Hand.RemoveAt(WeakestIndex);
			State.Seats[LoserSeat].PotLostCount += 1;
			CaptureToPile(WinnerSeat, MoveTemp(Extra), Result);
			Notes.Add(FString::Printf(TEXT("%s also seizes %s (weakest %s)"),
				*WinnerCardName, *ExtraName, *FCBAttributeUtil::ToString(Move.Attribute)));
		}

		if (bWinnerTakesDeckTop && State.Deck.Num() > 0)
		{
			const FString BonusName = State.Deck[0].Name;
			FFCBCardInstance Bonus = MoveTemp(State.Deck[0]);
			State.Deck.RemoveAt(0);
			State.Seats[WinnerSeat].Hand.Add(MoveTemp(Bonus));
			Notes.Add(FString::Printf(TEXT("%s snatches %s from the top of the deck"),
				*WinnerCardName, *BonusName));
		}

		State.Seats[WinnerSeat].RoundsWon += 1;
		Result.bLeaderChanged = (State.LeaderSeat != WinnerSeat);
		State.LeaderSeat = WinnerSeat;
	}
	else
	{
		// Clash: no winner. Where the cards go is decided by the tie rule.
		switch (Config.TieRule)
		{
		case EFCBTieRule::BurnNoCapture:
			State.Burned.Add(MoveTemp(AttackerPlayed));
			State.Burned.Add(MoveTemp(DefenderPlayed));
			Notes.Add(TEXT("Both cards burn: no capture"));
			break;

		case EFCBTieRule::SplitKeep:
			AttackerHand.Add(MoveTemp(AttackerPlayed));
			DefenderHand.Add(MoveTemp(DefenderPlayed));
			Notes.Add(TEXT("Honourable draw - both cards return to their hands"));
			break;

		case EFCBTieRule::PotClash:
		case EFCBTieRule::HigherRarityWins:
		case EFCBTieRule::LeaderWinsTies:
		default:
			{
				const FString AttackerName = AttackerPlayed.Name;
				const FString DefenderName = DefenderPlayed.Name;
				State.Pot.Add(MoveTemp(AttackerPlayed));
				State.Pot.Add(MoveTemp(DefenderPlayed));
				Result.PotCardsAdded = 2;
				State.Seats[AttackerSeat].ClashCount += 1;
				State.Seats[DefenderSeat].ClashCount += 1;
				Notes.Add(FString::Printf(TEXT("Clash! %s and %s go to the pot (%d card(s))"),
					*AttackerName, *DefenderName, State.Pot.Num()));

				if (State.Pot.Num() > Config.PotCap)
				{
					Result.PotCardsSwallowed = State.Pot.Num();
					for (int32 Index = 0; Index < State.Pot.Num(); ++Index)
					{
						State.Deck.Add(MoveTemp(State.Pot[Index]));
						State.Seats[AttackerSeat].PotLostCount += 1;
						State.Seats[DefenderSeat].PotLostCount += 1;
					}
					State.Pot.Reset();
					Notes.Add(FString::Printf(TEXT("Pot exceeded %d cards: %d card(s) are shuffled back into the deck"),
						Config.PotCap, Result.PotCardsSwallowed));
				}
			}
			break;
		}
		// The leader keeps the lead after a clash: they must choose again, with more information.
	}

	// --- refill, winner first when configured ---------------------------------------------------------
	const int32 FirstSeat = (Outcome != EFCBRoundOutcome::Clash && Config.bWinnerDrawsFirst)
		? State.LeaderSeat : AttackerSeat;
	DrawUp(FirstSeat, FMath::Max(0, Config.HandSize - State.Seats[FirstSeat].Hand.Num()));
	const int32 SecondSeat = FCBSeat::OpponentOf(FirstSeat);
	DrawUp(SecondSeat, FMath::Max(0, Config.HandSize - State.Seats[SecondSeat].Hand.Num()));

	// --- bookkeeping ---------------------------------------------------------------------------------
	// Score is accumulated incrementally in CaptureToPile (rarity weighted per config); swing = net change.
	Result.ScoreSwing = (State.Seats[FCBSeat::A].Score - ScoreBeforeA) + (ScoreBeforeB - State.Seats[FCBSeat::B].Score);

	Result.Narration = FString::Printf(TEXT("R%d %s leads \"%s\" on %s (%s vs %s): %s"),
		Result.RoundNumber,
		*State.Seats[AttackerSeat].Name,
		*AttackerSnapshot.Name,
		*FCBAttributeUtil::ToString(Move.Attribute),
		*FCBAttributeUtil::FormatValue(Move.Attribute, Projection.AttackerEffective),
		*FCBAttributeUtil::FormatValue(Move.Attribute, Projection.DefenderEffective),
		OutcomeToString(Outcome));
	if (Result.WinnerCardsGained > 0)
	{
		Result.Narration += FString::Printf(TEXT(" -> +%d collected"), Result.WinnerCardsGained);
		if (Result.PotCardsAwarded > 0)
		{
			Result.Narration += FString::Printf(TEXT(" (incl. %d from the pot)"), Result.PotCardsAwarded);
		}
	}
	if (PotBefore > 0 && Outcome != EFCBRoundOutcome::Clash && Result.PotCardsAwarded == 0)
	{
		Result.Narration += FString::Printf(TEXT(" [pot of %d still on the table]"), PotBefore);
	}

	State.RoundNumber += 1;
	State.History.Add(MoveTemp(Result));
	AppendLog(State.History.Last().Narration);
	for (const FString& Note : State.History.Last().Projection.Notes)
	{
		AppendLog(FString::Printf(TEXT("    - %s"), *Note));
	}

	CheckMatchEnd();
	return true;
}

void FFCBMatch::CaptureToPile(int32 Seat, FFCBCardInstance&& Card, FFCBRoundResult& OutResult)
{
	if (!State.Seats.IsValidIndex(Seat))
	{
		return;
	}
	State.Seats[Seat].Score += ScoreOf(Card);
	State.Seats[Seat].Pile.Add(MoveTemp(Card));
	OutResult.WinnerCardsGained += 1;
}

int32 FFCBMatch::ScoreOf(const FFCBCardInstance& Card) const
{
	return Config.bRarityScoreWeights ? FCBRarityUtil::CaptureScore(Card.Rarity) : 1;
}

void FFCBMatch::DrawUp(int32 Seat, int32 Count)
{
	if (!State.Seats.IsValidIndex(Seat))
	{
		return;
	}
	for (int32 Index = 0; Index < Count && State.Deck.Num() > 0; ++Index)
	{
		State.Seats[Seat].Hand.Add(MoveTemp(State.Deck[0]));
		State.Deck.RemoveAt(0);
	}
}

void FFCBMatch::CheckMatchEnd()
{
	if (State.bMatchOver)
	{
		return;
	}

	bool bEnd = false;
	FString Reason;
	const int32 HandA = State.Seats[FCBSeat::A].Hand.Num();
	const int32 HandB = State.Seats[FCBSeat::B].Hand.Num();
	const int32 DeckLeft = State.Deck.Num();
	const int32 ScoreA = State.Seats[FCBSeat::A].Score;
	const int32 ScoreB = State.Seats[FCBSeat::B].Score;

	if (Config.EndCondition == EFCBEndCondition::FirstToScore
		&& (ScoreA >= Config.TargetScore || ScoreB >= Config.TargetScore))
	{
		bEnd = true;
		Reason = FString::Printf(TEXT("First to %d points"), Config.TargetScore);
		State.WinnerSeat = ScoreA == ScoreB ? INDEX_NONE : (ScoreA > ScoreB ? FCBSeat::A : FCBSeat::B);
	}

	if (!bEnd && HandA == 0 && HandB == 0 && DeckLeft == 0)
	{
		bEnd = true;
		Reason = FString::Printf(TEXT("Both hands exhausted -> decided on score %d vs %d"), ScoreA, ScoreB);
		State.WinnerSeat = ScoreA == ScoreB ? INDEX_NONE : (ScoreA > ScoreB ? FCBSeat::A : FCBSeat::B);
	}
	else if (!bEnd && DeckLeft == 0 && (HandA == 0 || HandB == 0))
	{
		bEnd = true;
		State.WinnerSeat = HandA == 0 ? FCBSeat::B : FCBSeat::A;
		Reason = FString::Printf(TEXT("%s cannot field a card"), *State.Seats[HandA == 0 ? FCBSeat::A : FCBSeat::B].Name);
	}

	if (!bEnd && State.RoundNumber >= Config.MaxRounds)
	{
		bEnd = true;
		Reason = FString::Printf(TEXT("Round limit (%d) reached -> score %d vs %d"), Config.MaxRounds, ScoreA, ScoreB);
		State.WinnerSeat = ScoreA == ScoreB ? INDEX_NONE : (ScoreA > ScoreB ? FCBSeat::A : FCBSeat::B);
	}

	if (bEnd)
	{
		State.bMatchOver = true;
		State.EndReason = Reason;
		AppendLog(FString::Printf(TEXT("Match over - %s. Winner: %s (%d - %d score)"),
			*Reason,
			State.WinnerSeat == INDEX_NONE ? TEXT("Draw") : *State.Seats[State.WinnerSeat].Name,
			ScoreA, ScoreB));
	}
}

bool FFCBMatch::PlayRandomMoveForLeader(FString& OutError)
{
	TArray<FFCBMove> Moves;
	GetLegalMoves(State.LeaderSeat, Moves);
	if (Moves.Num() == 0)
	{
		OutError = TEXT("No legal moves available for the leader.");
		return false;
	}
	const FFCBMove& Move = Moves[Rng.RandRange(0, Moves.Num() - 1)];
	++RngState;
	return PlayMove(Move, OutError);
}

void FFCBMatch::GetUnknownPool(int32 ForSeat, TMap<int32, int32>& OutIndexToCount) const
{
	OutIndexToCount = DealtCounts;

	auto Subtract = [&OutIndexToCount](const TArray<FFCBCardInstance>& Cards)
	{
		for (const FFCBCardInstance& Card : Cards)
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
	};

	if (State.Seats.IsValidIndex(ForSeat))
	{
		Subtract(State.Seats[ForSeat].Hand);
	}
	for (int32 Seat = 0; Seat < State.Seats.Num(); ++Seat)
	{
		Subtract(State.Seats[Seat].Pile);
	}
	Subtract(State.Pot);
	Subtract(State.Burned);
}

int32 FFCBMatch::GetTotalCaptured() const
{
	int32 Total = 0;
	for (const FFCBSeatState& Seat : State.Seats)
	{
		Total += Seat.Pile.Num();
	}
	return Total;
}

FString FFCBMatch::FormatState() const
{
	FString Text;
	Text += FString::Printf(TEXT("Round %d | leader %s | deck %d | pot %d | burned %d\n"),
		State.RoundNumber,
		State.Seats.IsValidIndex(State.LeaderSeat) ? *State.Seats[State.LeaderSeat].Name : TEXT("-"),
		State.Deck.Num(), State.Pot.Num(), State.Burned.Num());
	for (int32 Seat = 0; Seat < State.Seats.Num(); ++Seat)
	{
		const FFCBSeatState& Info = State.Seats[Seat];
		Text += FString::Printf(TEXT("  [%d] %-16s hand %2d pile %2d score %3d won %2d clashes %2d\n"),
			Seat, *Info.Name, Info.Hand.Num(), Info.Pile.Num(), Info.Score, Info.RoundsWon, Info.ClashCount);
	}
	return Text;
}

FString FFCBMatch::FormatLog(int32 FromRound) const
{
	FString Text;
	for (const FFCBRoundResult& Result : State.History)
	{
		if (Result.RoundNumber < FromRound)
		{
			continue;
		}
		Text += Result.Narration;
		for (const FString& Note : Result.Projection.Notes)
		{
			Text += FString::Printf(TEXT("\n    - %s"), *Note);
		}
		Text += TEXT("\n");
	}
	return Text;
}

void FFCBMatch::AppendLog(const FString& Line)
{
	if (bLogEnabled)
	{
		State.Log.Add(Line);
	}
}
