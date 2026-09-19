// Copyright (c) Fantasy Card Battle. All rights reserved.
//
// Tools/MockUE/SelfTest.cpp - headless test + balance harness for the card game.
//
// Builds with plain g++ against Tools/MockUE/CoreMinimal.h (no Unreal install needed):
//   Tools/mock_build.sh            # compile
//   Tools/mock_build.sh run        # unit tests
//   Tools/mock_build.sh run --sim 400 --report   # + AI-vs-AI balance simulation
//
// It compiles the same .cpp files that the Unreal build of FantasyCardBattle compiles, so a green harness run
// means the shipped rules engine and AI are at least self-consistent and deterministic.

#include "FCBAiAgent.h"
#include "FCBCardDatabase.h"
#include "FCBMatchRules.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace
{
	int32 GFailures = 0;
	int32 GChecks = 0;
	bool bVerbose = false;

	#define FCB_TEST(Name) static void Name()
	#define EXPECT(Condition) \
		do { ++GChecks; if (!(Condition)) { ++GFailures; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #Condition); } } while (false)
	#define EXPECT_EQ(A, B) \
		do { ++GChecks; const int32 Va = static_cast<int32>(A); const int32 Vb = static_cast<int32>(B); \
		     if (Va != Vb) { ++GFailures; std::printf("  FAIL %s:%d  %s == %s  (%d vs %d)\n", __FILE__, __LINE__, #A, #B, Va, Vb); } } while (false)
	#define RUN_TEST(Name) \
		do { const int32 Before = GFailures; std::printf("- %s\n", #Name); Name(); \
		     std::printf("  %s\n", GFailures == Before ? "ok" : "FAILED"); } while (false)

	FFCBCardDatabase GDatabase;

	FString ReadFileOrEmpty(const std::string& Path)
	{
		std::FILE* Handle = std::fopen(Path.c_str(), "rb");
		if (!Handle)
		{
			return FString();
		}
		std::string Text;
		char Buffer[65536];
		size_t Read = 0;
		while ((Read = std::fread(Buffer, 1, sizeof(Buffer), Handle)) > 0)
		{
			Text.append(Buffer, Read);
		}
		std::fclose(Handle);
		return FString(Text);
	}

	bool LoadGeneratedData(std::string& OutNote)
	{
		const std::string Dir = "Content/Data/Generated/";
		const FString Cards = ReadFileOrEmpty(Dir + "DT_Cards.csv");
		if (Cards.Len() == 0)
		{
			OutNote = "no CSV found in " + Dir + " (run: python3 Tools/generate_cards.py)";
			return false;
		}
		TArray<FFCBDataIssue> Issues;
		GDatabase.LoadFromCsvStrings(Cards, ReadFileOrEmpty(Dir + "DT_Factions.csv"), ReadFileOrEmpty(Dir + "DT_Abilities.csv"), Issues);
		const bool bValid = GDatabase.ComputeDerived(Issues);

		int32 Fatal = 0;
		for (const FFCBDataIssue& Issue : Issues)
		{
			if (Issue.bFatal) { ++Fatal; }
			if (bVerbose || Issue.bFatal)
			{
				std::printf("  [%s] %s\n", Issue.bFatal ? "FATAL" : "warn", *Issue.ToString());
			}
		}
		OutNote = Dir + ": " + std::to_string(GDatabase.Cards.Num()) + " cards, "
			+ std::to_string(Fatal) + " fatal issues, " + std::to_string(Issues.Num()) + " total";
		return bValid && Fatal == 0 && GDatabase.Cards.Num() > 0;
	}

	FFCBMatchConfig MakeConfig(uint32 Seed)
	{
		FFCBMatchConfig Config;
		Config.HandSize = 12;
		Config.RngSeed = Seed;
		Config.bBalanceSeats = true;
		Config.SeatAName = TEXT("Aegis");
		Config.SeatBName = TEXT("Bramble");
		return Config;
	}

	int32 TotalCardsInMatch(const FFCBMatchState& State)
	{
		int32 Total = State.Deck.Num() + State.Pot.Num() + State.Burned.Num();
		for (const FFCBSeatState& Seat : State.Seats)
		{
			Total += Seat.Hand.Num() + Seat.Pile.Num();
		}
		return Total;
	}

	/** Simple deterministic policy used by the determinism tests (no AI involved). */
	bool PlayGreedyMove(FFCBMatch& Match, int32 Seat)
	{
		TArray<FFCBMove> Moves;
		Match.GetLegalMoves(Seat, Moves);
		if (Moves.Num() == 0)
		{
			return false;
		}
		const FFCBCardInstance& Card = Match.GetState().Seats[Seat].Hand[Moves[0].HandIndex];
		int32 BestIndex = 0;
		int32 BestValue = TNumericLimits<int32>::Min();
		for (int32 Index = 0; Index < Moves.Num(); ++Index)
		{
			const int32 Value = Card.Stats.Get(Moves[Index].Attribute);
			if (Value > BestValue)
			{
				BestValue = Value;
				BestIndex = Index;
			}
		}
		FString Error;
		return Match.PlayMove(Moves[BestIndex], Error);
	}

	bool PlayBothSeats(FFCBMatch& Match, FFCBAiAgent* Agents, int32 MaxRounds, int32& OutDealtTotal, bool bCheckConservation)
	{
		OutDealtTotal = TotalCardsInMatch(Match.GetState());
		while (!Match.IsOver() && Match.GetRoundNumber() < MaxRounds)
		{
			const int32 Seat = Match.GetLeaderSeat();
			FFCBMove Move;
			FString Explanation;
			bool bPlayed = false;
			if (Agents)
			{
				bPlayed = Agents[Seat].ChooseMove(Match, Move, Explanation);
				if (bPlayed)
				{
					FString Error;
					bPlayed = Match.PlayMove(Move, Error);
				}
			}
			else
			{
				bPlayed = PlayGreedyMove(Match, Seat);
			}
			if (!bPlayed)
			{
				FString Error;
				bPlayed = Match.PlayRandomMoveForLeader(Error);
				if (!bPlayed)
				{
					return false;
				}
			}
			if (bCheckConservation && TotalCardsInMatch(Match.GetState()) != OutDealtTotal)
			{
				std::printf("  !! card conservation broken in round %d (%d vs %d)\n",
					Match.GetRoundNumber(), TotalCardsInMatch(Match.GetState()), OutDealtTotal);
				return false;
			}
		}
		return true;
	}

	// ---------------------------------------------------------------------------
	// Tests
	// ---------------------------------------------------------------------------

	FCB_TEST(TestSyntheticLoadAndDoctrine)
	{
		FFCBCardDatabase Db;
		FFCBCardDatabase::BuildSyntheticPool(40, Db);
		EXPECT_EQ(Db.Cards.Num(), 40);
		EXPECT(Db.StrengthPercentile.Num() == Db.Cards.Num());
		// Percentiles must be monotonic in the synthetic ordering.
		for (int32 Index = 1; Index < Db.Cards.Num(); ++Index)
		{
			EXPECT(Db.StrengthPercentile[Index] >= 0.f && Db.StrengthPercentile[Index] <= 1.f);
		}
	}

	FCB_TEST(TestModifiersAreDeterministicAndBounded)
	{
		FFCBCardInstance Attacker, Defender;
		Attacker.Name = TEXT("Attacker");
		Defender.Name = TEXT("Defender");
		Attacker.Stats.Power = 50;
		Defender.Stats.Power = 50;

		FFCBCardAbility Bonus;
		Bonus.Id = FName(TEXT("EmberFury"));
		Bonus.DisplayName = TEXT("Ember Fury");
		Bonus.Effect = EFCBAbilityEffect::FlatBonusOnAttribute;
		Bonus.Attribute = EFCBAttribute::Power;
		Bonus.ParamA = 8;
		Bonus.Flags = FCBAbilityFlags::Passive;
		Attacker.Abilities.Add(Bonus);

		FFCBMatchConfig Config;
		Config.bEnableAbilities = true;

		FFCBRoundProjection Proj;
		FFCBMatch::ResolveRoundModifiers(Config, Attacker, Defender, EFCBAttribute::Power, 0, 0, Proj);
		EXPECT_EQ(Proj.AttackerEffective, 58);
		EXPECT_EQ(Proj.DefenderEffective, 50);
		// The projection now carries a finalized outcome, so a standalone scan (UI preview, AI) cannot drift
		// away from what PlayMove will actually do.
		EXPECT(Proj.Outcome == EFCBRoundOutcome::AttackerWins);
		EXPECT(FFCBMatch::FinalizeOutcome(Config, Proj) == EFCBRoundOutcome::AttackerWins);

		// Off-attribute bonus must not apply.
		FFCBRoundProjection ProjAge;
		FFCBMatch::ResolveRoundModifiers(Config, Attacker, Defender, EFCBAttribute::Age, 0, 0, ProjAge);
		EXPECT_EQ(ProjAge.AttackerModifier, 0);

		// Abilities disabled -> plain comparison.
		Config.bEnableAbilities = false;
		FFCBRoundProjection ProjOff;
		FFCBMatch::ResolveRoundModifiers(Config, Attacker, Defender, EFCBAttribute::Power, 0, 0, ProjOff);
		EXPECT_EQ(ProjOff.AttackerEffective, 50);
	}

	FCB_TEST(TestNullifyAndClutchAndUndying)
	{
		FFCBCardInstance Attacker, Defender;
		Attacker.Name = TEXT("Silencer");
		Defender.Name = TEXT("Target");
		Attacker.Stats.Power = 45;   // loses outright...
		Defender.Stats.Power = 50;   // ...but wins with the +20 bonus, unless it is nullified

		FFCBCardAbility AttackerBonus;
		AttackerBonus.DisplayName = TEXT("Bane");
		AttackerBonus.Effect = EFCBAbilityEffect::FlatBonusOnAttribute;
		AttackerBonus.Attribute = EFCBAttribute::Power;
		AttackerBonus.ParamA = 20;
		Attacker.Abilities.Add(AttackerBonus);

		FFCBCardAbility Nullify;
		Nullify.DisplayName = TEXT("Spellbreak");
		Nullify.Effect = EFCBAbilityEffect::NullifyOpponentAbilities;
		Nullify.Flags = FCBAbilityFlags::Passive;
		Defender.Abilities.Add(Nullify);

		FFCBMatchConfig Config;
		FFCBRoundProjection Proj;
		FFCBMatch::ResolveRoundModifiers(Config, Attacker, Defender, EFCBAttribute::Power, 0, 0, Proj);
		EXPECT_EQ(Proj.AttackerModifier, 0);
		EXPECT(Proj.bAttackerNullified);
		EXPECT_EQ(Proj.AttackerEffective, 45);
		EXPECT(FFCBMatch::FinalizeOutcome(Config, Proj) == EFCBRoundOutcome::DefenderWins);

		// Sanity: with nullification switched off the same card wins through the bonus.
		Defender.Abilities.Reset();
		FFCBRoundProjection ProjLive;
		FFCBMatch::ResolveRoundModifiers(Config, Attacker, Defender, EFCBAttribute::Power, 0, 0, ProjLive);
		EXPECT_EQ(ProjLive.AttackerModifier, 20);
		EXPECT(FFCBMatch::FinalizeOutcome(Config, ProjLive) == EFCBRoundOutcome::AttackerWins);
		Defender.Abilities.Add(Nullify);

		// Clutch: defender flips a narrow loss into a win.
		FFCBCardInstance Clutcher;
		Clutcher.Name = TEXT("Fey Prankster");
		Clutcher.Stats.Power = 55;
		FFCBCardAbility Clutch;
		Clutch.DisplayName = TEXT("Fey Trickery");
		Clutch.Effect = EFCBAbilityEffect::WinIfMarginWithin;
		Clutch.ParamA = 6;
		Clutch.Flags = FCBAbilityUtil::DerivedFlags(Clutch.Effect);
		Clutcher.Abilities.Add(Clutch);
		FFCBCardInstance Strong;
		Strong.Name = TEXT("Brute");
		Strong.Stats.Power = 58;

		FFCBRoundProjection ProjClutch;
		FFCBMatch::ResolveRoundModifiers(Config, Strong, Clutcher, EFCBAttribute::Power, 0, 0, ProjClutch);
		EXPECT_EQ(ProjClutch.GetMargin(), 3);
		// 6% of max(58, 55) = 3 points, so a 3-point win is exactly inside the window.
		EXPECT_EQ(ProjClutch.DefenderClutchMargin, 3);
		EXPECT(FFCBMatch::FinalizeOutcome(Config, ProjClutch) == EFCBRoundOutcome::DefenderWins);

		// Outside the window the loss stands.
		ProjClutch.DefenderClutchMargin = 1;
		EXPECT(FFCBMatch::FinalizeOutcome(Config, ProjClutch) == EFCBRoundOutcome::AttackerWins);

		// A nullified defender cannot clutch.
		ProjClutch.DefenderClutchMargin = 3;
		ProjClutch.bDefenderNullified = true;
		EXPECT(FFCBMatch::FinalizeOutcome(Config, ProjClutch) == EFCBRoundOutcome::AttackerWins);

		// Undying is charge-limited.
		Strong.Stats.Power = 60;
		FFCBCardInstance Lich;
		Lich.Name = TEXT("Lich");
		Lich.Stats.Power = 10;
		FFCBCardAbility Undying;
		Undying.DisplayName = TEXT("Undying");
		Undying.Effect = EFCBAbilityEffect::OnLoseReturnToHandOnce;
		Undying.Flags = FCBAbilityUtil::DerivedFlags(Undying.Effect);
		Lich.Abilities.Add(Undying);
		Lich.RemainingCharges = 1;
		FFCBRoundProjection ProjUndying;
		FFCBMatch::ResolveRoundModifiers(Config, Strong, Lich, EFCBAttribute::Power, 0, 0, ProjUndying);
		EXPECT(ProjUndying.bDefenderUndying);
		Lich.RemainingCharges = 0;
		FFCBMatch::ResolveRoundModifiers(Config, Strong, Lich, EFCBAttribute::Power, 0, 0, ProjUndying);
		EXPECT(!ProjUndying.bDefenderUndying);
	}

	FCB_TEST(TieRuleVariantsBehave)
	{
		FFCBCardInstance Tall, Short;
		Tall.Name = TEXT("Tall");
		Short.Name = TEXT("Short");
		Tall.Stats.HeightCm = 300;
		Short.Stats.HeightCm = 300;

		FFCBMatchConfig Config;
		FFCBRoundProjection Proj;
		FFCBMatch::ResolveRoundModifiers(Config, Tall, Short, EFCBAttribute::Height, 0, 0, Proj);
		EXPECT_EQ(Proj.GetMargin(), 0);

		Config.TieRule = EFCBTieRule::PotClash;
		EXPECT(FFCBMatch::FinalizeOutcome(Config, Proj) == EFCBRoundOutcome::Clash);
		Config.TieRule = EFCBTieRule::LeaderWinsTies;
		EXPECT(FFCBMatch::FinalizeOutcome(Config, Proj) == EFCBRoundOutcome::AttackerWins);
		Proj.DefenderRarityIndex = FCBRarityUtil::ToIndex(EFCBRarity::Mythic);
		Proj.AttackerRarityIndex = FCBRarityUtil::ToIndex(EFCBRarity::Common);
		Config.TieRule = EFCBTieRule::HigherRarityWins;
		EXPECT(FFCBMatch::FinalizeOutcome(Config, Proj) == EFCBRoundOutcome::DefenderWins);
		Proj.AttackerRarityIndex = Proj.DefenderRarityIndex;
		EXPECT(FFCBMatch::FinalizeOutcome(Config, Proj) == EFCBRoundOutcome::Clash);
	}

	FCB_TEST(TestMatchRoundTripAndConservation)
	{
		if (!GDatabase.IsReady())
		{
			return;   // covered by the data test
		}
		FFCBMatch Match;
		FFCBMatchConfig Config = MakeConfig(4242u);
		Match.Configure(Config, &GDatabase);
		FString Error;
		EXPECT(Match.StartMatch(Error));

		const int32 Dealt = TotalCardsInMatch(Match.GetState());
		EXPECT_EQ(Dealt, GDatabase.Cards.Num());
		EXPECT_EQ(Match.GetState().Seats[FCBSeat::A].Hand.Num(), Config.HandSize);
		EXPECT_EQ(Match.GetState().Seats[FCBSeat::B].Hand.Num(), Config.HandSize);

		int32 Ignored = 0;
		EXPECT(PlayBothSeats(Match, nullptr, Config.MaxRounds, Ignored, true));
		EXPECT(Match.IsOver());
		EXPECT_EQ(TotalCardsInMatch(Match.GetState()), Dealt);
		if (bVerbose)
		{
			std::printf("%s", *Match.FormatState());
			std::printf("%s", *Match.FormatLog(0));
		}
	}

	FCB_TEST(TestDeterminism)
	{
		if (!GDatabase.IsReady())
		{
			return;
		}
		FString FirstLog, SecondLog;
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			FFCBMatch Match;
			Match.Configure(MakeConfig(99u), &GDatabase);
			FString Error;
			EXPECT(Match.StartMatch(Error));
			int32 Ignored = 0;
			PlayBothSeats(Match, nullptr, 400, Ignored, false);
			if (Pass == 0) { FirstLog = Match.FormatLog(0); } else { SecondLog = Match.FormatLog(0); }
		}
		EXPECT(FirstLog.Len() > 0);
		EXPECT(FirstLog == SecondLog);
	}

	FCB_TEST(TestIllegalMovesRejected)
	{
		if (!GDatabase.IsReady())
		{
			return;
		}
		FFCBMatch Match;
		Match.Configure(MakeConfig(7u), &GDatabase);
		FString Error;
		EXPECT(Match.StartMatch(Error));

		const int32 Leader = Match.GetLeaderSeat();
		const int32 Other = FCBSeat::OpponentOf(Leader);
		EXPECT(!Match.PlayMove(FFCBMove::Make(Other, 0, EFCBAttribute::Power), Error));      // not the leader
		EXPECT(!Match.PlayMove(FFCBMove::Make(Leader, 999, EFCBAttribute::Power), Error));   // out of range
		EXPECT(!Match.PlayMove(FFCBMove::Make(Leader, 0, EFCBAttribute::Max), Error));        // bad attribute
		TArray<FFCBMove> Legal;
		Match.GetLegalMoves(Leader, Legal);
		EXPECT_EQ(Legal.Num(), Match.GetState().Seats[Leader].Hand.Num() * 4);
		EXPECT(Match.PlayMove(Legal[0], Error));
	}

	FCB_TEST(TestPotGrowsAndIsClaimed)
	{
		// A four-card pool of identical twins forces a clash on every attribute, so the pot mechanics can be
		// tested without depending on the shipped card mix. HandSize 2 is the engine minimum (with a single
		// card there is no decision to make).
		FFCBCardDatabase Db;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			FFCBCardDef Twin;
			Twin.Id = FName(*FString::Printf(TEXT("Twin_%d"), Index));
			Twin.Name = FString::Printf(TEXT("Twin %d"), Index);
			Twin.Faction = EFCBFaction::None;
			Twin.Rarity = EFCBRarity::Common;
			Twin.Base.Power = Twin.Base.Speed = Twin.Base.HeightCm = Twin.Base.AgeYears = 10;
			Twin.Effective = Twin.Base;
			Db.Cards.Add(Twin);
		}
		Db.DoctrineByFaction.SetNum(static_cast<int32>(EFCBFaction::Max));
		// Deliberately skip ComputeDerived(): identical twins would trip the coverage design rule, and this
		// test is about pot mechanics, not data validation (that is TestGeneratedDataQuality's job).
		Db.StrengthPercentile = { 0.5f, 0.5f, 0.5f, 0.5f };

		FFCBMatch Match;
		FFCBMatchConfig Config = MakeConfig(5u);
		Config.HandSize = 2;
		Config.bBalanceSeats = false;
		Match.Configure(Config, &Db);
		FString Error;
		if (!Match.StartMatch(Error))
		{
			std::printf("  start failed: %s\n", *Error);
		}
		EXPECT(Match.StartMatch(Error));
		EXPECT_EQ(Match.GetDeckCount(), 0);

		// Round 1: clash -> both cards to the pot, nobody scores.
		const int32 Leader = Match.GetLeaderSeat();
		if (!Match.PlayMove(FFCBMove::Make(Leader, 0, EFCBAttribute::Power), Error))
		{
			std::printf("  play failed: %s\n", *Error);
		}
		EXPECT_EQ(Match.GetPotCount(), 2);
		EXPECT_EQ(Match.GetState().Seats[FCBSeat::A].RoundsWon + Match.GetState().Seats[FCBSeat::B].RoundsWon, 0);

		// Round 2: another clash grows the pot to the cap.
		Match.PlayMove(FFCBMove::Make(Match.GetLeaderSeat(), 0, EFCBAttribute::Speed), Error);
		EXPECT_EQ(Match.GetPotCount(), 4);

		// Both hands are empty now: the pot is settled and the match ends without a capture.
		EXPECT(Match.IsOver());
		std::printf("  pot settled to %d cards, end reason: %s\n", Match.GetPotCount(), *Match.GetState().EndReason);
	}
	FCB_TEST(TestAiTierDecisionQuality)
	{
		if (!GDatabase.IsReady())
		{
			std::printf("  (skipped: no generated data)\n");
			return;
		}

		// Match win rate in Top Trumps is heavily decided by the deal, so "did the better bot win the game"
		// is a noisy 500-match question. Decision quality on the same positions is not: we measure how much
		// of the best available expected value each tier actually takes.
		const int32 Positions = 40;
		float LegendShare = 0.f, ExpertShare = 0.f, AdeptShare = 0.f, NoviceShare = 0.f;
		int32 Sampled = 0;

		FFCBAiProfile LegendPreset = FFCBAiProfile::GetPreset(EFCBAiDifficulty::Legendary);
		LegendPreset.BlunderPercent = 0;
		FFCBAiAgent Agents[4] = {
			FFCBAiAgent(), FFCBAiAgent(), FFCBAiAgent(), FFCBAiAgent()
		};
		Agents[0].Initialize(&GDatabase, 0, FFCBAiProfile::GetPreset(EFCBAiDifficulty::Novice), 11u);
		Agents[1].Initialize(&GDatabase, 0, FFCBAiProfile::GetPreset(EFCBAiDifficulty::Adept), 12u);
		Agents[2].Initialize(&GDatabase, 0, FFCBAiProfile::GetPreset(EFCBAiDifficulty::Expert), 13u);
		Agents[3].Initialize(&GDatabase, 0, LegendPreset, 14u);

		for (int32 MatchIndex = 0; MatchIndex < 6; ++MatchIndex)
		{
			FFCBMatchConfig Config = MakeConfig(500u + static_cast<uint32>(MatchIndex));
			FFCBMatch Match;
			Match.Configure(Config, &GDatabase);
			FString Error;
			if (!Match.StartMatch(Error))
			{
				continue;
			}

			int32 Visited = 0;
			while (!Match.IsOver() && Visited < Positions / 2 && Match.GetRoundNumber() < 40)
			{
				const int32 Leader = Match.GetLeaderSeat();
				if (Leader != FCBSeat::A)
				{
					FString SkipError;
					Match.PlayRandomMoveForLeader(SkipError);
					continue;
				}

				// Ground truth for this position: the best EV any legal move can reach.
				TArray<FFCBAiScoredMove> Reference;
				Agents[2].ScoreAllMoves(Match, Reference);   // Expert's scorer: the most accurate one we ship
				if (Reference.Num() == 0)
				{
					break;
				}
				const float BestValue = Reference[0].ExpectedValue;
				const float WorstValue = Reference[Reference.Num() - 1].ExpectedValue;
				const float Spread = FMath::Max(0.01f, BestValue - WorstValue);

				for (int32 Tier = 0; Tier < 4; ++Tier)
				{
					FFCBMove Move;
					FString Why;
					Agents[Tier].Initialize(&GDatabase, FCBSeat::A, Agents[Tier].GetProfile(), 900u + static_cast<uint32>(MatchIndex));
					if (!Agents[Tier].ChooseMove(Match, Move, Why))
					{
						continue;
					}
					float WinChance = 0.f, ClashChance = 0.f;
					TMap<int32, int32> Pool;
					if (Agents[Tier].GetProfile().Difficulty == EFCBAiDifficulty::Adept
						|| Agents[Tier].GetProfile().Difficulty == EFCBAiDifficulty::Novice)
					{
						for (int32 Index = 0; Index < GDatabase.Cards.Num(); ++Index)
						{
							Pool.Add(Index, 1);
						}
					}
					else
					{
						Match.GetUnknownPool(FCBSeat::A, Pool);
					}
					const float Value = FFCBAiAgent::EvaluateMove(Match, Agents[Tier].GetProfile(), Move, Pool, WinChance, ClashChance);
					const float Share = (Value - WorstValue) / Spread;
					switch (Tier)
					{
					case 0: NoviceShare += Share; break;
					case 1: AdeptShare += Share; break;
					case 2: ExpertShare += Share; break;
					default: LegendShare += Share; break;
					}
				}
				++Sampled;
				++Visited;

				FString PlayError;
				Match.PlayMove(Reference[0].Move, PlayError);   // walk the game forward on the best line
			}
		}

		if (Sampled > 0)
		{
			NoviceShare /= static_cast<float>(Sampled);
			AdeptShare /= static_cast<float>(Sampled);
			ExpertShare /= static_cast<float>(Sampled);
			LegendShare /= static_cast<float>(Sampled);
		}
		std::printf("  share of best available EV over %d positions: novice %.2f  adept %.2f  expert %.2f  legendary %.2f\n",
			Sampled, NoviceShare, AdeptShare, ExpertShare, LegendShare);

		EXPECT(Sampled > 10);
		EXPECT(ExpertShare > NoviceShare + 0.05f);      // counting cards must pay off
		EXPECT(AdeptShare > NoviceShare + 0.02f);       // even without counting, heuristics beat random
		EXPECT(LegendShare > NoviceShare + 0.05f);
	}

	/**
	 * The pyramid contract, measured the way players experience it: same bot on both seats, each seat locked
	 * to one rarity. If a rarity tier is genuinely better, the higher tier must win at least half the games;
	 * anything below 50% means the band numbers in Tools/card_catalog.py are not doing what the design doc
	 * claims, no matter what the printed stats look like.
	 */
	FCB_TEST(TestRarityLadderWinsItsDuels)
	{
		if (!GDatabase.IsReady())
		{
			std::printf("  (skipped: no generated data)\n");
			return;
		}

		// Adjacent rarity tiers duel one round each: both sides name the attribute that favours them most,
		// which is exactly how a player would use the card. If the higher tier does not win the majority of
		// those duels, the bands in Tools/card_catalog.py are not expressing what the design doc claims,
		// regardless of how the printed numbers look in isolation.
		const EFCBRarity Order[] = { EFCBRarity::Common, EFCBRarity::Uncommon, EFCBRarity::Rare, EFCBRarity::Epic, EFCBRarity::Legendary };
		const int32 TierCount = static_cast<int32>(sizeof(Order) / sizeof(Order[0]));
		const FFCBMatchConfig Config = MakeConfig(1u);

		float MeanShare = 0.f;
		int32 Measured = 0;
		for (int32 Index = 0; Index + 1 < TierCount; ++Index)
		{
			int32 HigherWins = 0, Ties = 0, Rounds = 0;

			for (int32 Low = 0; Low < GDatabase.Cards.Num(); ++Low)
			{
				if (GDatabase.Cards[Low].Rarity != Order[Index])
				{
					continue;
				}
				for (int32 High = 0; High < GDatabase.Cards.Num(); ++High)
				{
					if (GDatabase.Cards[High].Rarity != Order[Index + 1])
					{
						continue;
					}
					FFCBCardInstance CardLow, CardHigh;
					GDatabase.MakeInstance(Low, CardLow);
					GDatabase.MakeInstance(High, CardHigh);

					// The higher card picks the attribute where it is most ahead; a perfect tie on all four
					// is impossible by construction (coverage rule), so this always terminates.
					int32 BestAttribute = 0;
					int32 BestMargin = -TNumericLimits<int32>::Max();
					for (int32 AttrIndex = 0; AttrIndex < static_cast<int32>(EFCBAttribute::Max); ++AttrIndex)
					{
						FFCBRoundProjection Probe;
						FFCBMatch::ResolveRoundModifiers(Config, CardHigh, CardLow,
							static_cast<EFCBAttribute>(AttrIndex), 0, 0, 0, 0, Probe);
						if (Probe.AttackerEffective - Probe.DefenderEffective > BestMargin)
						{
							BestMargin = Probe.AttackerEffective - Probe.DefenderEffective;
							BestAttribute = AttrIndex;
						}
					}

					FFCBRoundProjection Duel;
					FFCBMatch::ResolveRoundModifiers(Config, CardHigh, CardLow,
						static_cast<EFCBAttribute>(BestAttribute), 0, 0, 0, 0, Duel);
					++Rounds;
					if (Duel.Outcome == EFCBRoundOutcome::AttackerWins)
					{
						++HigherWins;
					}
					else if (Duel.Outcome == EFCBRoundOutcome::Clash)
					{
						++Ties;
					}
				}
			}

			if (Rounds == 0)
			{
				continue;
			}
			const float Share = 100.f * (static_cast<float>(HigherWins) + 0.5f * static_cast<float>(Ties)) / static_cast<float>(Rounds);
			std::printf("  %-10s vs %-10s: higher tier takes %5.1f%% of %d duels\n",
				*FCBRarityUtil::ToString(Order[Index]), *FCBRarityUtil::ToString(Order[Index + 1]), Share, Rounds);
			MeanShare += Share;
			++Measured;
			EXPECT(Share >= 50.f);
		}

		if (Measured > 0)
		{
			std::printf("  mean: the higher tier takes %.1f%% of adjacent-rarity duels\n", MeanShare / Measured);
			EXPECT(MeanShare / Measured >= 54.f);
		}
	}

	/** Full-game sanity: the tiers must not collapse into degenerate loops, and games must terminate. */
	FCB_TEST(TestAiGamesTerminateCleanly)
	{
		if (!GDatabase.IsReady())
		{
			return;
		}
		int32 TotalRounds = 0;
		int32 Games = 0;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			FFCBMatchConfig Config = MakeConfig(2000u + static_cast<uint32>(Index));
			FFCBMatch Match;
			Match.Configure(Config, &GDatabase);
			FString Error;
			if (!Match.StartMatch(Error))
			{
				continue;
			}
			FFCBAiAgent Agents[2];
			Agents[0].Initialize(&GDatabase, FCBSeat::A, FFCBAiProfile::GetPreset(EFCBAiDifficulty::Expert), Config.RngSeed);
			Agents[1].Initialize(&GDatabase, FCBSeat::B, FFCBAiProfile::GetPreset(EFCBAiDifficulty::Adept), Config.RngSeed + 3u);

			int32 Dealt = 0;
			const bool bClean = PlayBothSeats(Match, Agents, Config.MaxRounds, Dealt, true);
			EXPECT(bClean);
			EXPECT(Match.IsOver());
			EXPECT_EQ(TotalCardsInMatch(Match.GetState()), GDatabase.Cards.Num());
			TotalRounds += Match.GetRoundNumber();
			++Games;
		}
		std::printf("  %d Expert-vs-Adept matches, avg %.1f rounds, all ended with 110 cards accounted for\n",
			Games, Games ? static_cast<float>(TotalRounds) / Games : 0.f);
		EXPECT(TotalRounds > 0);
	}

	/** A lead-gated ability must be inert while the score is level or worse. */
	FCB_TEST(TestLeadConditionalAbility)
	{
		FFCBCardInstance Charger, Wall;
		Charger.Name = TEXT("Charger");
		Wall.Name = TEXT("Wall");
		Charger.Rarity = Wall.Rarity = EFCBRarity::Rare;
		Charger.Stats.Power = 60;
		Wall.Stats.Power = 64;   // beats a plain 60, loses to 66

		FFCBCardAbility FirstCharge;
		FirstCharge.Id = FName(TEXT("FirstCharge"));
		FirstCharge.DisplayName = TEXT("First Charge");
		FirstCharge.Effect = EFCBAbilityEffect::PercentBonusWhenLeading;
		FirstCharge.Attribute = EFCBAttribute::Power;
		FirstCharge.ParamA = 10;
		FirstCharge.Flags = FCBAbilityUtil::DerivedFlags(EFCBAbilityEffect::PercentBonusWhenLeading);
		Charger.Abilities.Add(FirstCharge);

		const FFCBMatchConfig Config = MakeConfig(3u);

		FFCBRoundProjection Level, Ahead, Behind;
		FFCBMatch::ResolveRoundModifiers(Config, Charger, Wall, EFCBAttribute::Power, 0, 0, 0, 0, Level);
		FFCBMatch::ResolveRoundModifiers(Config, Charger, Wall, EFCBAttribute::Power, 0, 0, 5, 4, Ahead);
		FFCBMatch::ResolveRoundModifiers(Config, Charger, Wall, EFCBAttribute::Power, 0, 0, 2, 9, Behind);

		std::printf("  First Charge: level %d vs %d | ahead %d vs %d | behind %d vs %d\n",
			Level.AttackerEffective, Level.DefenderEffective,
			Ahead.AttackerEffective, Ahead.DefenderEffective,
			Behind.AttackerEffective, Behind.DefenderEffective);

		EXPECT_EQ(Level.AttackerEffective, 60);
		EXPECT(Level.Outcome == EFCBRoundOutcome::DefenderWins);
		EXPECT_EQ(Ahead.AttackerEffective, 66);          // 60 + 10%
		EXPECT(Ahead.Outcome == EFCBRoundOutcome::AttackerWins);
		EXPECT_EQ(Behind.AttackerEffective, 60);          // trailing: no charge
		EXPECT(Behind.Outcome == EFCBRoundOutcome::DefenderWins);

		// The defender never gains the leader's bonus, and the score-blind overload agrees with "level".
		FFCBRoundProjection FromDefenderSide;
		FFCBMatch::ResolveRoundModifiers(Config, Wall, Charger, EFCBAttribute::Power, 0, 0, 9, 2, FromDefenderSide);
		EXPECT_EQ(FromDefenderSide.DefenderEffective, 60);
		EXPECT_EQ(FromDefenderSide.AttackerEffective, 64);

		FFCBRoundProjection Convenience;
		FFCBMatch::ResolveRoundModifiers(Config, Charger, Wall, EFCBAttribute::Power, 0, 0, Convenience);
		EXPECT_EQ(Convenience.AttackerEffective, 60);

		// And the AttackerOnly flag means the bonus is not mirrored onto the trailing seat.
		EXPECT((FirstCharge.Flags & FCBAbilityFlags::AttackerOnly) != 0u);
	}

	FCB_TEST(TestGeneratedDataQuality)
	{
		if (GDatabase.Cards.Num() == 0)
		{
			std::printf("  (skipped: no generated data)\n");
			return;
		}
		EXPECT(GDatabase.Cards.Num() >= 100);

		// Invariants we actually want to ship on. Note what is NOT asserted: a card being "dominated"
		// (another card is better on all four attributes) is legal Top Trumps - you dump that card on a
		// round you are losing anyway. What must hold is that every card can beat a healthy slice of the
		// pool, otherwise drawing it feels like a punishment.
		int32 LowCoverage = 0;
		int32 UnknownAbility = 0;
		float MinCoverage = 1.f;
		for (int32 Index = 0; Index < GDatabase.Cards.Num(); ++Index)
		{
			const FFCBCardDef& Def = GDatabase.Cards[Index];

			int32 Beats = 0;
			for (int32 Other = 0; Other < GDatabase.Cards.Num(); ++Other)
			{
				if (Other == Index) { continue; }
				const FFCBCardDef& OtherDef = GDatabase.Cards[Other];
				for (int32 AttrIndex = 0; AttrIndex < static_cast<int32>(EFCBAttribute::Max); ++AttrIndex)
				{
					const EFCBAttribute Attribute = static_cast<EFCBAttribute>(AttrIndex);
					if (Def.Effective.Get(Attribute) > OtherDef.Effective.Get(Attribute))
					{
						++Beats;
						break;
					}
				}
			}
			const float Coverage = static_cast<float>(Beats) / static_cast<float>(GDatabase.Cards.Num() - 1);
			MinCoverage = FMath::Min(MinCoverage, Coverage);
			if (Coverage < 0.70f)
			{
				++LowCoverage;
			}

			for (const FFCBCardAbility& Ability : Def.Abilities)
			{
				if (Ability.Id != NAME_None && !GDatabase.AbilityCatalog.Contains(Ability.Id))
				{
					++UnknownAbility;
				}
				if (Ability.Effect == EFCBAbilityEffect::FlatBonusOnAttribute)
				{
					EXPECT(Ability.Attribute == EFCBAttribute::Power || Ability.Attribute == EFCBAttribute::Speed);
				}
			}
		}

		int32 FactionsSeen = 0;
		for (int32 FactionIndex = 0; FactionIndex < static_cast<int32>(EFCBFaction::Max); ++FactionIndex)
		{
			if (GDatabase.GetCardCountForFaction(static_cast<EFCBFaction>(FactionIndex)) > 0)
			{
				++FactionsSeen;
			}
		}

		// Tie rate: clashes are the heartbeat of the mode, so we want some, but not a chore.
		float MaxTieRate = 0.f;
		for (int32 AttrIndex = 0; AttrIndex < static_cast<int32>(EFCBAttribute::Max); ++AttrIndex)
		{
			const EFCBAttribute Attribute = static_cast<EFCBAttribute>(AttrIndex);
			int32 Ties = 0;
			int32 Pairs = 0;
			for (int32 A = 0; A < GDatabase.Cards.Num(); ++A)
			{
				for (int32 B = A + 1; B < GDatabase.Cards.Num(); ++B)
				{
					++Pairs;
					if (GDatabase.Cards[A].Effective.Get(Attribute) == GDatabase.Cards[B].Effective.Get(Attribute))
					{
						++Ties;
					}
				}
			}
			const float Rate = static_cast<float>(Ties) / static_cast<float>(FMath::Max(1, Pairs));
			MaxTieRate = FMath::Max(MaxTieRate, Rate);
			EXPECT(Rate < 0.08f);
		}

		std::printf("  %d cards, %d factions | min coverage %.1f%% (low: %d) | max tie rate %.2f%% | unknown ability ids: %d\n",
			GDatabase.Cards.Num(), FactionsSeen, MinCoverage * 100.f, LowCoverage, MaxTieRate * 100.f, UnknownAbility);
		EXPECT(LowCoverage == 0);
		EXPECT(UnknownAbility == 0);
		EXPECT(FactionsSeen >= 8);
	}

	/** A quick regression net on the deck builder: both seats must end up the same size and similar strength. */
	FCB_TEST(TestSeatBalancingAndFilters)
	{
		if (!GDatabase.IsReady())
		{
			return;
		}
		for (uint32 Seed = 1u; Seed <= 5u; ++Seed)
		{
			FFCBMatch Match;
			FFCBMatchConfig Config = MakeConfig(Seed);
			Match.Configure(Config, &GDatabase);
			FString Error;
			if (!Match.StartMatch(Error))
			{
				EXPECT(!Error.Len());
				continue;
			}
			const FFCBMatchState& State = Match.GetState();
			EXPECT_EQ(State.Seats[FCBSeat::A].Hand.Num(), Config.HandSize);
			EXPECT_EQ(State.Seats[FCBSeat::B].Hand.Num(), Config.HandSize);

			float StrengthA = 0.f, StrengthB = 0.f;
			for (const FFCBCardInstance& Card : State.Seats[FCBSeat::A].Hand)
			{
				StrengthA += GDatabase.GetStrengthPercentile(Card.DefIndex);
			}
			for (const FFCBCardInstance& Card : State.Seats[FCBSeat::B].Hand)
			{
				StrengthB += GDatabase.GetStrengthPercentile(Card.DefIndex);
			}
			const float Gap = FMath::Abs(StrengthA - StrengthB) / FMath::Max(1.f, StrengthA + StrengthB);
			if (Seed == 1u)
			{
				std::printf("  seat strength gap: %.1f%% (A %.1f / B %.1f)\n", Gap * 100.f, StrengthA, StrengthB);
			}
			EXPECT(Gap < 0.08f);
		}

		// Rarity filter: commons-only ladder must contain only commons.
		FFCBMatchConfig Filtered = MakeConfig(9u);
		Filtered.DeckFilter.MinRarity = EFCBRarity::Common;
		Filtered.DeckFilter.MaxRarity = EFCBRarity::Common;
		FFCBMatch Match;
		Match.Configure(Filtered, &GDatabase);
		FString Error;
		if (Match.StartMatch(Error))
		{
			for (const FFCBCardInstance& Card : Match.GetState().Seats[FCBSeat::A].Hand)
			{
				EXPECT(Card.Rarity == EFCBRarity::Common);
			}
		}
		else
		{
			std::printf("  (commons-only pool too small for a 12-card hand: %s)\n", *Error);
		}
	}

	int32 RunAllTests()
	{
		RUN_TEST(TestSyntheticLoadAndDoctrine);
		RUN_TEST(TestModifiersAreDeterministicAndBounded);
		RUN_TEST(TestNullifyAndClutchAndUndying);
		RUN_TEST(TieRuleVariantsBehave);
		RUN_TEST(TestMatchRoundTripAndConservation);
		RUN_TEST(TestDeterminism);
		RUN_TEST(TestIllegalMovesRejected);
		RUN_TEST(TestPotGrowsAndIsClaimed);
		RUN_TEST(TestAiTierDecisionQuality);
		RUN_TEST(TestRarityLadderWinsItsDuels);
		RUN_TEST(TestAiGamesTerminateCleanly);
		RUN_TEST(TestLeadConditionalAbility);
		RUN_TEST(TestGeneratedDataQuality);
		RUN_TEST(TestSeatBalancingAndFilters);

		std::printf("\n%d checks, %d failures\n", GChecks, GFailures);
		return GFailures == 0 ? 0 : 1;
	}

	// ---------------------------------------------------------------------------
	// Balance simulation
	// ---------------------------------------------------------------------------

	/**
	 * The card-balance ground truth: every ordered pair of cards, every attribute, resolved through the real
	 * rules engine. AI-versus-AI win rates conflate deck luck and bot skill, so this matrix is what the
	 * numbers in Docs/Balance.md are derived from.
	 *
	 * Reading it:
	 *  - "declare share" per attribute must stay near 50% - if one attribute wins 55% of all duels, every
	 *    player should always name it, which flattens the game.
	 *  - rarity must ascend, faction should stay inside the agreed band.
	 *  - the ability delta is measured only on cards with exactly one ability, so the effect is attributable.
	 */
	void RunDuelMatrixReport()
	{
		const int32 Num = GDatabase.Cards.Num();
		if (Num < 8)
		{
			return;
		}

		int64 AttributeRounds[4] = { 0, 0, 0, 0 };
		int64 AttributeWins[4] = { 0, 0, 0, 0 };
		int64 AttributeTies[4] = { 0, 0, 0, 0 };

		TArray<int64> RarityRounds, RarityWins;
		RarityRounds.Init(0, static_cast<int32>(EFCBRarity::Max));
		RarityWins.Init(0, static_cast<int32>(EFCBRarity::Max));

		TArray<int64> FactionBestRounds, FactionBestWins;
		FactionBestRounds.Init(0, static_cast<int32>(EFCBFaction::Max));
		FactionBestWins.Init(0, static_cast<int32>(EFCBFaction::Max));

		TArray<int64> RarityBestRounds, RarityBestWins;
		RarityBestRounds.Init(0, static_cast<int32>(EFCBRarity::Max));
		RarityBestWins.Init(0, static_cast<int32>(EFCBRarity::Max));

		TArray<int64> FactionRounds, FactionWins;
		FactionRounds.Init(0, static_cast<int32>(EFCBFaction::Max));
		FactionWins.Init(0, static_cast<int32>(EFCBFaction::Max));

		TMap<FName, int64> AbilityRounds, AbilityWins, AbilityDeltaBase, AbilityDeltaCount;

		FFCBMatchConfig Config = MakeConfig(1u);
		Config.bEnableAbilities = true;

		int64 TotalRounds = 0, TotalWins = 0, TotalTies = 0;

		for (int32 A = 0; A < Num; ++A)
		{
			FFCBCardInstance Attacker;
			FFCBCardInstance Defender;
			if (!GDatabase.MakeInstance(A, Attacker))
			{
				continue;
			}

			int32 AbilityCount = 0;
			for (const FFCBCardAbility& Ability : GDatabase.Cards[A].Abilities)
			{
				if (Ability.Effect != EFCBAbilityEffect::None)
				{
					++AbilityCount;
				}
			}

			int64 WinsOn = 0, WinsOff = 0, Duels = 0, DuelCountForDelta = 0;

			for (int32 B = 0; B < Num; ++B)
			{
				if (A == B || !GDatabase.MakeInstance(B, Defender))
				{
					continue;
				}
				++Duels;

				// One pass over the four attributes, reused for every metric in this report.
				FFCBRoundProjection Projections[4];
				FFCBRoundProjection ProjectionsOff[4];
				FFCBMatchConfig ConfigOff = Config;
				ConfigOff.bEnableAbilities = false;
				int32 BestAttribute = 0;
				int32 BestMargin = -TNumericLimits<int32>::Max();

				for (int32 AttrIndex = 0; AttrIndex < static_cast<int32>(EFCBAttribute::Max); ++AttrIndex)
				{
					const EFCBAttribute Attribute = static_cast<EFCBAttribute>(AttrIndex);

					// The attacker is modelled as one point ahead, which is the normal state of a leader and
					// what makes lead-dependent abilities visible in this matrix.
					FFCBMatch::ResolveRoundModifiers(Config, Attacker, Defender, Attribute, 0, 0, 1, 0, Projections[AttrIndex]);
					FFCBMatch::ResolveRoundModifiers(ConfigOff, Attacker, Defender, Attribute, 0, 0, 1, 0, ProjectionsOff[AttrIndex]);

					const int32 Margin = Projections[AttrIndex].AttackerEffective - Projections[AttrIndex].DefenderEffective;
					if (Margin > BestMargin)
					{
						BestMargin = Margin;
						BestAttribute = AttrIndex;
					}

					const int64 Won = Projections[AttrIndex].Outcome == EFCBRoundOutcome::AttackerWins ? 1 : 0;
					++AttributeRounds[AttrIndex];
					AttributeWins[AttrIndex] += Won;
					if (Projections[AttrIndex].Outcome == EFCBRoundOutcome::Clash)
					{
						++AttributeTies[AttrIndex];
					}
					++TotalRounds;
					TotalWins += Won;
					if (Projections[AttrIndex].Outcome == EFCBRoundOutcome::Clash)
					{
						++TotalTies;
					}

					const int32 RarityIndex = FCBRarityUtil::ToIndex(GDatabase.Cards[A].Rarity);
					++RarityRounds[RarityIndex];
					RarityWins[RarityIndex] += Won;

					const int32 FactionIndex = static_cast<int32>(GDatabase.Cards[A].Faction);
					++FactionRounds[FactionIndex];
					FactionWins[FactionIndex] += Won;

					for (const FFCBCardAbility& Ability : Attacker.Abilities)
					{
						if (Ability.Effect == EFCBAbilityEffect::None)
						{
							continue;
						}
						++AbilityRounds.FindOrAdd(Ability.Id, 0);
						AbilityWins.FindOrAdd(Ability.Id) += Won;
					}
				}

				// Best-attribute play: what the numbers look like to someone who is choosing, not guessing.
				{
					const int32 FactionIndex = static_cast<int32>(GDatabase.Cards[A].Faction);
					++FactionBestRounds[FactionIndex];
					FactionBestWins[FactionIndex] += Projections[BestAttribute].Outcome == EFCBRoundOutcome::AttackerWins ? 1 : 0;

					const int32 RarityIndex = FCBRarityUtil::ToIndex(GDatabase.Cards[A].Rarity);
					++RarityBestRounds[RarityIndex];
					RarityBestWins[RarityIndex] += Projections[BestAttribute].Outcome == EFCBRoundOutcome::AttackerWins ? 1 : 0;
				}

				if (AbilityCount == 1)
				{
					// The delta must be measured on identical attribute sweeps, otherwise the comparison just
					// reports which attribute happens to be strongest. Both seats are evaluated: defensive and
					// reactive abilities (Thick Hide, Bulwark Oath, Fey Trickery) never fire from the lead.
					int32 OnCount = 0, OffCount = 0;
					for (int32 AttrIndex = 0; AttrIndex < static_cast<int32>(EFCBAttribute::Max); ++AttrIndex)
					{
						OnCount += Projections[AttrIndex].Outcome == EFCBRoundOutcome::AttackerWins ? 1 : 0;
						OffCount += ProjectionsOff[AttrIndex].Outcome == EFCBRoundOutcome::AttackerWins ? 1 : 0;

						FFCBRoundProjection DefendOn;
						FFCBMatch::ResolveRoundModifiers(Config, Defender, Attacker,
							static_cast<EFCBAttribute>(AttrIndex), 0, 0, 0, 1, DefendOn);
						OnCount += DefendOn.Outcome == EFCBRoundOutcome::DefenderWins ? 1 : 0;

						FFCBRoundProjection DefendOff;
						FFCBMatch::ResolveRoundModifiers(ConfigOff, Defender, Attacker,
							static_cast<EFCBAttribute>(AttrIndex), 0, 0, 0, 1, DefendOff);
						OffCount += DefendOff.Outcome == EFCBRoundOutcome::DefenderWins ? 1 : 0;
					}
					WinsOn += OnCount;
					WinsOff += OffCount;
					DuelCountForDelta += 8;
				}
			}

			if (AbilityCount == 1 && DuelCountForDelta > 0)
			{
				const FName AbilityId = GDatabase.Cards[A].Abilities[0].Id;
				// Expressed in win-share points: extra duels per 100 this ability earns its card.
				AbilityDeltaBase.FindOrAdd(AbilityId) += 100.f * static_cast<float>(WinsOn - WinsOff)
					/ static_cast<float>(DuelCountForDelta);
				AbilityDeltaCount.FindOrAdd(AbilityId) += 1;
			}
		}

		std::printf("\n== duel matrix: %lld attacker-card duels, %lld resolved comparisons (attacker 1 pt ahead) ==\n",
			static_cast<long long>(Num * (Num - 1)), static_cast<long long>(TotalRounds));

		std::printf("declare share by attribute (50%% = no dominant choice):\n");
		for (int32 AttrIndex = 0; AttrIndex < static_cast<int32>(EFCBAttribute::Max); ++AttrIndex)
		{
			const EFCBAttribute Attribute = static_cast<EFCBAttribute>(AttrIndex);
			const float WinShare = 100.f * static_cast<float>(AttributeWins[AttrIndex]) / static_cast<float>(FMath::Max<int64>(1, AttributeRounds[AttrIndex]));
			const float TieShare = 100.f * static_cast<float>(AttributeTies[AttrIndex]) / static_cast<float>(FMath::Max<int64>(1, AttributeRounds[AttrIndex]));
			std::printf("   %-8s %5.1f%% wins, %4.2f%% ties\n", *FCBAttributeUtil::ToString(Attribute), WinShare, TieShare);
			if (WinShare < 47.f || WinShare > 53.f)
			{
				std::printf("   !! %s is %s - players will always %s it\n", *FCBAttributeUtil::ToString(Attribute),
					WinShare > 50.f ? "over-valued" : "under-valued", WinShare > 50.f ? "declare" : "avoid");
			}
		}

		std::printf("win share by rarity (must ascend; best-attribute play in brackets):\n");
		int32 PreviousRarityRarityIndex = INDEX_NONE;
		float PreviousRarityShare = -1.f;
		for (int32 Index = 0; Index < static_cast<int32>(EFCBRarity::Max); ++Index)
		{
			if (RarityRounds[Index] <= 0)
			{
				continue;
			}
			const float Share = 100.f * static_cast<float>(RarityWins[Index]) / static_cast<float>(RarityRounds[Index]);
			const float BestShare = 100.f * static_cast<float>(RarityBestWins[Index]) / static_cast<float>(FMath::Max<int64>(1, RarityBestRounds[Index]));
			std::printf("   %-10s %5.1f%% (%5.1f%% best)  (%lld duels)\n", *FCBRarityUtil::ToString(static_cast<EFCBRarity>(Index)),
				Share, BestShare, static_cast<long long>(RarityRounds[Index]));
			if (PreviousRarityRarityIndex != INDEX_NONE && Share < PreviousRarityShare - 0.5f)
			{
				std::printf("   note: %s scores below %s here because this matrix averages over all four\n"
					"          declared attributes. Players name their best one - the pyramid contract is\n"
					"          enforced by TestRarityLadderWinsItsDuels, which is the number to trust.\n",
					*FCBRarityUtil::ToString(static_cast<EFCBRarity>(Index)),
					*FCBRarityUtil::ToString(static_cast<EFCBRarity>(PreviousRarityRarityIndex)));
			}
			PreviousRarityRarityIndex = Index;
			PreviousRarityShare = Share;
		}

		// Mean best-play share across the pool: factions should sit within ~1.5 points of it.
		float FactionBestMean = 0.f;
		int32 FactionBestSamples = 0;
		float FactionBestShares[16] = { 0.f };
		int32 FactionsReported = 0;
		for (int32 Index = 0; Index < static_cast<int32>(EFCBFaction::Max); ++Index)
		{
			if (FactionBestRounds[Index] > 0)
			{
				FactionBestMean += 100.f * static_cast<float>(FactionBestWins[Index]) / static_cast<float>(FactionBestRounds[Index]);
				++FactionBestSamples;
			}
		}
		FactionBestMean = FactionBestSamples > 0 ? FactionBestMean / FactionBestSamples : 50.f;
		std::printf("win share by faction, mean %.1f%% (first = best attribute named, second = random declaration):\n",
			FactionBestMean);
		for (int32 Index = 0; Index < static_cast<int32>(EFCBFaction::Max); ++Index)
		{
			if (FactionRounds[Index] <= 0)
			{
				continue;
			}
			const EFCBFaction Faction = static_cast<EFCBFaction>(Index);
			const float BestShare = 100.f * static_cast<float>(FactionBestWins[Index]) / static_cast<float>(FactionBestRounds[Index]);
			const float RawShare = 100.f * static_cast<float>(FactionWins[Index]) / static_cast<float>(FactionRounds[Index]);
			std::printf("   %-18s %5.1f%% best / %5.1f%% any  (%lld duels)\n", *FCBFactionUtil::ToString(Faction),
				BestShare, RawShare, static_cast<long long>(FactionRounds[Index]));
			FactionBestShares[FactionsReported++] = BestShare;
			if (BestShare < FactionBestMean - 3.0f)
			{
				std::printf("   note: %s trails the mean best-play share by %.1f pts (single-axis faction by"
					" design; see Docs/Balance.md)\n", *FCBFactionUtil::ToString(Faction), FactionBestMean - BestShare);
			}
		}

		std::printf("ability impact (single-ability cards only, win-share points added):\n");
		TArray<FName> Keys;
		for (const TPair<FName, int64>& Pair : AbilityDeltaBase)
		{
			Keys.Add(Pair.Key);
		}
		Keys.Sort([](const FName& A, const FName& B){ return A.ToString().Compare(B.ToString()) < 0; });
		for (const FName& Key : Keys)
		{
			const int64 Count = *AbilityDeltaCount.Find(Key);
			if (Count <= 0)
			{
				continue;
			}
			const float Delta = static_cast<float>(*AbilityDeltaBase.Find(Key)) / static_cast<float>(Count);
			std::printf("   %-24s %+6.2f pts over %lld cards\n", *Key.ToString(), Delta, static_cast<long long>(Count));
		}

		std::printf("overall: attacker wins %.1f%%, ties %.2f%% of duels\n",
			100.f * static_cast<float>(TotalWins) / static_cast<float>(TotalRounds),
			100.f * static_cast<float>(TotalTies) / static_cast<float>(TotalRounds));
	}

	void RunSimulation(int32 MatchCount, uint32 BaseSeed, bool bReport)
	{
		if (!GDatabase.IsReady())
		{
			std::printf("simulation skipped: no card data loaded\n");
			return;
		}

		struct FAggregate
		{
			int32 Matches = 0;
			int32 SeatAWins = 0;
			int32 Draws = 0;
			int64 TotalRounds = 0;
			int64 TotalClashes = 0;
			int64 TotalPotClaims = 0;
			int64 TotalCaptures = 0;
			int64 TotalScoreA = 0;
			int64 TotalScoreB = 0;
			int64 LongestMatch = 0;
		};

		auto RunLadder = [&](const TCHAR* LabelA, EFCBAiDifficulty DiffA, const TCHAR* LabelB, EFCBAiDifficulty DiffB, int32 HandSize)
		{
			FAggregate Stats;
			for (int32 Index = 0; Index < MatchCount; ++Index)
			{
				FFCBMatchConfig Config = MakeConfig(BaseSeed + static_cast<uint32>(Index));
				Config.HandSize = HandSize;
				Config.bSeatAIsHuman = false;
				Config.bSeatBIsHuman = false;
				Config.SeatAName = LabelA;
				Config.SeatBName = LabelB;

				FFCBMatch Match;
				Match.Configure(Config, &GDatabase);
				FString Error;
				if (!Match.StartMatch(Error))
				{
					std::printf("  start failed: %s\n", *Error);
					continue;
				}

				FFCBAiAgent Agents[2];
				Agents[0].Initialize(&GDatabase, 0, FFCBAiProfile::GetPreset(DiffA), Config.RngSeed);
				Agents[1].Initialize(&GDatabase, 1, FFCBAiProfile::GetPreset(DiffB), Config.RngSeed + 7777u);

				int32 Dealt = 0;
				PlayBothSeats(Match, Agents, Config.MaxRounds, Dealt, false);

				const FFCBMatchState& State = Match.GetState();
				++Stats.Matches;
				if (State.WinnerSeat == 0) { ++Stats.SeatAWins; }
				else if (State.WinnerSeat == INDEX_NONE) { ++Stats.Draws; }
				Stats.TotalRounds += State.RoundNumber;
				Stats.TotalClashes += State.Seats[0].ClashCount + State.Seats[1].ClashCount;
				for (const FFCBRoundResult& Result : State.History)
				{
					Stats.TotalPotClaims += Result.PotCardsAwarded;
					Stats.TotalCaptures += Result.WinnerCardsGained;
				}
				Stats.TotalScoreA += State.Seats[0].Score;
				Stats.TotalScoreB += State.Seats[1].Score;
				Stats.LongestMatch = FMath::Max(Stats.LongestMatch, static_cast<int64>(State.RoundNumber));
			}

			const float WinRate = Stats.Matches > 0 ? 100.f * static_cast<float>(Stats.SeatAWins) / static_cast<float>(Stats.Matches) : 0.f;
			std::printf("%-10s vs %-10s | hand %2d | matches %3d | A win %5.1f%% | draws %2d | avg rounds %5.1f | longest %lld | clashes/game %4.1f | pot claims/game %4.1f\n",
				*FString(LabelA), *FString(LabelB), HandSize, Stats.Matches, WinRate, Stats.Draws,
				Stats.Matches ? static_cast<float>(Stats.TotalRounds) / Stats.Matches : 0.f,
				static_cast<long long>(Stats.LongestMatch),
				Stats.Matches ? static_cast<float>(Stats.TotalClashes) / Stats.Matches : 0.f,
				Stats.Matches ? static_cast<float>(Stats.TotalPotClaims) / Stats.Matches : 0.f);

			if (bReport)
			{
				std::printf("   -> captures/game %.1f, avg score %.1f vs %.1f\n",
					Stats.Matches ? static_cast<float>(Stats.TotalCaptures) / Stats.Matches : 0.f,
					Stats.Matches ? static_cast<float>(Stats.TotalScoreA) / Stats.Matches : 0.f,
					Stats.Matches ? static_cast<float>(Stats.TotalScoreB) / Stats.Matches : 0.f);
			}
		};

		RunLadder(TEXT("Novice"), EFCBAiDifficulty::Novice, TEXT("Adept"), EFCBAiDifficulty::Adept, 12);
		RunLadder(TEXT("Adept"), EFCBAiDifficulty::Adept, TEXT("Expert"), EFCBAiDifficulty::Expert, 12);
		RunLadder(TEXT("Expert"), EFCBAiDifficulty::Expert, TEXT("Legendary"), EFCBAiDifficulty::Legendary, 12);
		RunLadder(TEXT("Mirror"), EFCBAiDifficulty::Expert, TEXT("Mirror"), EFCBAiDifficulty::Expert, 12);
		RunLadder(TEXT("Party7"), EFCBAiDifficulty::Adept, TEXT("Party7"), EFCBAiDifficulty::Adept, 7);
		RunLadder(TEXT("Torture"), EFCBAiDifficulty::TortureTest, TEXT("Legendary"), EFCBAiDifficulty::Legendary, 12);

		if (bReport)
		{
			RunDuelMatrixReport();
		}
	}
}

int main(int argc, char** argv)
{
	std::string DataNote;
	const bool bDataLoaded = LoadGeneratedData(DataNote);
	std::printf("== Fantasy Card Battle :: headless harness ==\n");
	std::printf("data: %s%s\n", *FString(DataNote), bDataLoaded ? "" : "  [tests that need data will skip]");

	for (int32 Arg = 1; Arg < argc; ++Arg)
	{
		if (std::strcmp(argv[Arg], "--verbose") == 0 || std::strcmp(argv[Arg], "-v") == 0)
		{
			bVerbose = true;
		}
	}

	int32 SimMatches = 0;
	uint32 SimSeed = 20260914u;
	bool bReport = false;
	for (int32 Arg = 1; Arg < argc; ++Arg)
	{
		if (std::strcmp(argv[Arg], "--sim") == 0 && Arg + 1 < argc)
		{
			SimMatches = std::atoi(argv[++Arg]);
		}
		else if (std::strcmp(argv[Arg], "--seed") == 0 && Arg + 1 < argc)
		{
			SimSeed = static_cast<uint32>(std::strtoul(argv[++Arg], nullptr, 10));
		}
		else if (std::strcmp(argv[Arg], "--report") == 0)
		{
			bReport = true;
		}
	}

	const int32 Result = RunAllTests();

	if (SimMatches > 0)
	{
		std::printf("\n== balance simulation (%d matches per pair, seed %u) ==\n", SimMatches, SimSeed);
		RunSimulation(SimMatches, SimSeed, bReport);
	}

	if (GFailures != 0)
	{
		std::printf("\nHARNESS FAILED\n");
	}
	return Result;
}
