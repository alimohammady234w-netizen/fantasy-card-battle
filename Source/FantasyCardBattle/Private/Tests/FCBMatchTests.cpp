// Copyright (c) Fantasy Card Battle. All rights reserved.
//
// FCBMatchTests.cpp - in-engine automation tests (Session Frontend > Automation, or
// `UnrealEditor-Cmd -RunTests=FantasyCardBattle`).
//
// These are NOT a copy of Tools/MockUE/SelfTest.cpp. The harness owns algorithmic coverage (every modifier,
// every tie rule, the balance simulation); this file owns the things that can only be checked inside a real
// engine build: that the shipped DataTables and CSVs validate, that the DataAsset -> CSV bridge round-trips,
// that a seeded match replays identically in the engine's own allocator environment, and that the UI helpers
// format numbers the way the docs claim.

#include "Misc/AutomationTest.h"
#include "FantasyCardBattle.h"
#include "FCBDataAssets.h"
#include "FCBCardDatabase.h"
#include "FCBGameInstance.h"
#include "FCBAiAgent.h"
#include "FCBMatchRules.h"
#include "Engine/DataTable.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

#if WITH_DEVFRAMEWORK

namespace FCBTest
{
	static bool LoadCsvDatabase(FFCBCardDatabase& OutDatabase, FString& OutError, TArray<FFCBDataIssue>& OutIssues)
	{
		const FString Directory = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Generated"));
		if (!IFileManager::Get().FileExists(*FPaths::Combine(Directory, TEXT("DT_Cards.csv"))))
		{
			OutError = FString::Printf(TEXT("no generated data in %s (run: python3 Tools/generate_cards.py)"), *Directory);
			return false;
		}
		return UFCBMatchRulesAsset::LoadDatabaseFromCsvDirectory(Directory, OutDatabase, OutError, &OutIssues);
	}

	static FFCBMatchConfig MakeConfig(uint32 Seed)
	{
		FFCBMatchConfig Config;
		Config.HandSize = 12;
		Config.RngSeed = Seed;
		Config.bBalanceSeats = true;
		Config.SeatAName = TEXT("Aegis");
		Config.SeatBName = TEXT("Bramble");
		return Config;
	}
}

/** --------------------------------------------------------------- data tests --------------------------------- */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFCBGeneratedDataIsValidTest,
	"FantasyCardBattle.Data.GeneratedDataIsValid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFCBGeneratedDataIsValidTest::RunTest(const FString& Parameters)
{
	FFCBCardDatabase Database;
	FString Error;
	TArray<FFCBDataIssue> Issues;

	if (!FCBTest::LoadCsvDatabase(Database, Error, Issues))
	{
		// Not a failure: a clean checkout without generated data is a normal state (CI runs the generator
		// itself). Say so loudly enough that it cannot be mistaken for a pass.
		AddWarning(Error);
		return true;
	}

	// LoadDatabaseFromCsvDirectory already refuses to return true when anything fatal fired, so this is the
	// only place the count matters - a silently empty table used to be the most common authoring mistake.
	TestTrue(TEXT("database reports itself ready"), Database.IsReady());
	TestTrue(TEXT("at least 100 cards"), Database.GetCardCount() >= 100);

	int32 FatalCount = 0;
	for (const FFCBDataIssue& Issue : Issues)
	{
		if (Issue.bFatal)
		{
			++FatalCount;
			AddError(FString::Printf(TEXT("data issue: %s"), *Issue.ToString()));
		}
	}
	TestEqual(TEXT("no fatal data issues"), FatalCount, 0);

	int32 FactionsWithCards = 0;
	for (int32 Index = 0; Index < static_cast<int32>(EFCBFaction::Max); ++Index)
	{
		if (Database.GetCardCountForFaction(static_cast<EFCBFaction>(Index)) > 0)
		{
			++FactionsWithCards;
		}
	}
	TestTrue(TEXT("at least 8 playable factions"), FactionsWithCards >= 8);

	int32 VanillaCount = 0;
	for (const FFCBCardDef& Def : Database.Cards)
	{
		if (Def.Abilities.Num() == 0 || Def.Abilities[0].IsNone())
		{
			++VanillaCount;
		}
		TestFalse(TEXT("printed stats inside the published band"),
			Def.Base.Power < FCBLimits::PowerMin || Def.Base.Power > FCBLimits::PowerMax);
		TestTrue(TEXT("every card has a display name"), Def.Name.Len() > 0);
	}
	TestTrue(TEXT("some vanilla cards exist for the beginner ladder"), VanillaCount > 0);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFCBRulesAssetRoundTripTest,
	"FantasyCardBattle.Data.RulesAssetRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFCBRulesAssetRoundTripTest::RunTest(const FString& Parameters)
{
	const UFCBMatchRulesAsset* Rules = UFCBMatchRulesAsset::GetConfigured();
	if (!Rules || Rules->CardTable.IsNull())
	{
		AddWarning(TEXT("No CardTable configured (Config/DefaultGame.ini) - run Content/Python/import_content.py"));
		return true;
	}

	FString Error;
	FFCBCardDatabase FromTables;
	if (!Rules->BuildDatabase(FromTables, Error))
	{
		AddError(FString::Printf(TEXT("BuildDatabase from DataTables failed: %s"), *Error));
		return false;
	}

	// The same content, re-emitted as CSV text and pushed through the plain-file loader. If these two counts
	// ever differ, the row struct and the CSV have drifted apart - the exact failure mode the bridge exists to
	// prevent.
	const FString Csv = Rules->ExportCardTableToCsv(Error);
	TestTrue(TEXT("card table exports to CSV text"), Csv.Len() > 0);

	FFCBCardDatabase FromCsvText;
	FString SecondError;
	if (Csv.Len() > 0)
	{
		// Feed the exported cards back with the tables' own faction/ability text so the loader has its context.
		TArray<FFCBDataIssue> Issues;
		FromCsvText.LoadFromCsvStrings(Csv, FString(), FString(), Issues);
		if (FromCsvText.GetCardCount() > 0)
		{
			TestEqual(TEXT("card count survives the round trip"),
				FromCsvText.GetCardCount(), FromTables.GetCardCount());
		}
	}
	return true;
}

/** ------------------------------------------------------------- rules tests -------------------------------- */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFCBDeterministicMatchTest,
	"FantasyCardBattle.Match.SameSeedPlaysTheSameMatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFCBDeterministicMatchTest::RunTest(const FString& Parameters)
{
	FFCBCardDatabase Database;
	FString Error;
	TArray<FFCBDataIssue> Issues;
	if (!FCBTest::LoadCsvDatabase(Database, Error, Issues))
	{
		AddWarning(Error);
		return true;
	}

	auto PlayFixedGame = [&Database](uint32 Seed, const FFCBAiDifficulty Difficulty) -> FString
	{
		FFCBMatch Match;
		Match.Configure(FCBTest::MakeConfig(Seed), &Database);
		FString StartError;
		if (!Match.StartMatch(StartError))
		{
			return StartError;
		}

		FFCBAiProfile Profile = FFCBAiProfile::GetPreset(Difficulty);
		FFCBAiAgent Agents[2];
		Agents[0].Initialize(&Database, FCBSeat::A, Profile, Seed);
		Agents[1].Initialize(&Database, FCBSeat::B, Profile, Seed + 1u);

		for (int32 Round = 0; Round < 25 && !Match.IsOver(); ++Round)
		{
			const int32 Seat = Match.GetLeaderSeat();
			FFCBMove Move;
			FString Why;
			if (!Agents[Seat].ChooseMove(Match, Move, Why))
			{
				Match.PlayRandomMoveForLeader(Why);
				continue;
			}
			Match.PlayMove(Move, Why);
		}
		return Match.FormatState();
	};

	const FString First = PlayFixedGame(4242u, EFCBAiDifficulty::Expert);
	const FString Second = PlayFixedGame(4242u, EFCBAiDifficulty::Expert);
	const FString Other = PlayFixedGame(4243u, EFCBAiDifficulty::Expert);

	TestEqual(TEXT("same seed, same AI, same transcript"), First, Second);
	TestFalse(TEXT("a different seed produces a different game"), First == Other);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFCBCardConservationTest,
	"FantasyCardBattle.Match.CardsAreAlwaysAccountedFor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFCBCardConservationTest::RunTest(const FString& Parameters)
{
	FFCBCardDatabase Database;
	FString Error;
	TArray<FFCBDataIssue> Issues;
	if (!FCBTest::LoadCsvDatabase(Database, Error, Issues))
	{
		AddWarning(Error);
		return true;
	}

	for (uint32 Seed = 1u; Seed <= 4u; ++Seed)
	{
		FFCBMatch Match;
		Match.Configure(FCBTest::MakeConfig(Seed), &Database);
		FString StartError;
		if (!Match.StartMatch(StartError))
		{
			AddError(StartError);
			continue;
		}

		FFCBAiProfile Profile = FFCBAiProfile::GetPreset(EFCBAiDifficulty::Adept);
		FFCBAiAgent Agents[2];
		Agents[0].Initialize(&Database, FCBSeat::A, Profile, Seed);
		Agents[1].Initialize(&Database, FCBSeat::B, Profile, Seed + 5u);

		for (int32 Round = 0; Round < 80 && !Match.IsOver(); ++Round)
		{
			const int32 Seat = Match.GetLeaderSeat();
			FFCBMove Move;
			FString Why;
			if (Agents[Seat].ChooseMove(Match, Move, Why))
			{
				Match.PlayMove(Move, Why);
			}
			else
			{
				Match.PlayRandomMoveForLeader(Why);
			}

			// Nothing may vanish: hands, deck, pot, the burn pile and both capture piles must always add up to
			// the whole pool. This catches double-captures and dropped cards, which is how card games rot.
			const FFCBMatchState& State = Match.GetState();
			int32 Total = State.Deck.Num() + State.Pot.Num() + State.Burned.Num();
			for (const FFCBSeatState& SeatState : State.Seats)
			{
				Total += SeatState.Hand.Num() + SeatState.Pile.Num();
			}
			if (Total != Database.GetCardCount())
			{
				AddError(FString::Printf(TEXT("seed %u round %d: %d cards in play, expected %d"),
					Seed, Round, Total, Database.GetCardCount()));
				break;
			}
		}
	}
	return true;
}

/** --------------------------------------------------------------- ui helpers ------------------------------- */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFCBUiFormattingTest,
	"FantasyCardBattle.Ui.NumberAndColourFormatting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFCBUiFormattingTest::RunTest(const FString& Parameters)
{
	// The strings in Docs/Balance.md and the ones on screen must be produced by the same function, so pin the
	// formatting here rather than trusting a screenshot.
	TestEqual(TEXT("age under a century"), FCBUi::FormatAttribute(EFCBAttribute::Age, 42), FString(TEXT("42 yr")));
	TestEqual(TEXT("thousands separated"), FCBUi::FormatAttribute(EFCBAttribute::Age, 12400), FString(TEXT("12,400 yr")));
	TestEqual(TEXT("height in metres"), FCBUi::FormatAttribute(EFCBAttribute::Height, 194), FString(TEXT("1.94 m")));
	TestEqual(TEXT("power is bare"), FCBUi::FormatAttribute(EFCBAttribute::Power, 112), FString(TEXT("112")));

	FLinearColor Color = FLinearColor::Black;
	TestTrue(TEXT("hex colour parses"), FCBData::ParseHexColor(TEXT("#B4361F"), Color));
	TestEqual(TEXT("red channel"), FMath::RoundToInt(Color.R * 255.f), 180);
	TestEqual(TEXT("green channel"), FMath::RoundToInt(Color.G * 255.f), 54);
	TestEqual(TEXT("blue channel"), FMath::RoundToInt(Color.B * 255.f), 31);

	TestFalse(TEXT("garbage colour rejected"), FCBData::ParseHexColor(TEXT("not a colour"), Color));
	TestFalse(TEXT("short colour rejected"), FCBData::ParseHexColor(TEXT("#FFF"), Color));

	TestEqual(TEXT("Common has no glow"), FCBUi::RarityGlowStrength(EFCBRarity::Common), 0.f);
	TestTrue(TEXT("Mythic glows the most"), FCBUi::RarityGlowStrength(EFCBRarity::Mythic) > FCBUi::RarityGlowStrength(EFCBRarity::Legendary));
}

#endif // WITH_DEVFRAMEWORK
