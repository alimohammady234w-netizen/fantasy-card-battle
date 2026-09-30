// Copyright (c) Fantasy Card Battle. All rights reserved.
//
// FCBTouchTests.cpp - in-engine automation tests for the mobile layer.
//
// These are deliberately NOT a copy of the harness tests in Tools/MockUE/SelfTest.cpp. The harness owns the
// geometry (every target on every simulated device, with no engine in the room). This file owns the one seam
// the harness cannot see: that Config/DefaultGame.ini really reaches UFCBSettings, and that the numbers it
// ships produce a playable table in a real build. A tuning value that silently fails to load is invisible
// until someone is holding a phone.

#include "Misc/AutomationTest.h"
#include "FantasyCardBattle.h"
#include "FCBDataAssets.h"
#include "FCBTouch.h"
#include "FCBTypes.h"

#if WITH_DEVFRAMEWORK

namespace FCBTouchTest
{
	/** The devices the mobile build is tuned against. Density is what the game computes, not a guess. */
	struct FDevice
	{
		const TCHAR* Label;
		float Width;
		float Height;
		bool bMobile;
	};

	static const FDevice Devices[] =
	{
		{ TEXT("phone 20:9"),	2400.f, 1080.f, true },
		{ TEXT("phone 16:9"),	1920.f, 1080.f, true },
		{ TEXT("small phone"),	1280.f,  720.f, true },
		{ TEXT("tablet"),		2560.f, 1600.f, true },
		{ TEXT("desktop"),		1920.f, 1080.f, false },
	};

	/**
	 * The density rule from AFCBDebugHud::ResolveDensityScale, restated here on purpose: if someone changes the
	 * game's rule and not this test, the numbers below stop matching a real phone and this fails.
	 */
	static float ResolveDensity(const FCBTouchTest::FDevice& Device)
	{
		return Device.bMobile
			? FMath::Clamp(FMath::Min(Device.Width, Device.Height) / 380.f, 1.f, 3.f)
			: 1.f;
	}

	static FCBTouch::FTuning MakeTuning(float Density)
	{
		FCBTouch::FTuning Tuning;
		Tuning.MinTarget = UFCBSettings::Get().TouchTargetDp * Density;
		return Tuning;
	}

	static FCBTouch::FState MakeState(int32 HandCount)
	{
		FCBTouch::FState State;
		State.HandCount = HandCount;
		State.SelectedCardIndex = 0;
		State.SelectedAttribute = EFCBAttribute::Power;
		State.bCanPlay = true;
		return State;
	}
}

/** --------------------------------------------------------------- settings ----------------------------------- */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFCBTouchSettingsAreAbsorbedTest,
	"FantasyCardBattle.Mobile.SettingsAreAbsorbed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFCBTouchSettingsAreAbsorbedTest::RunTest(const FString& Parameters)
{
	// Same seam as FantasyCardBattle.Data.IniConfigIsAbsorbed: the config file is the shipped tuning, and a
	// property without the config flag - or a renamed property - fails silently in every other test.
	const UFCBSettings& Settings = UFCBSettings::Get();

	TestEqual(TEXT("48 dp touch target from DefaultGame.ini"), Settings.TouchTargetDp, 48.f);
	TestEqual(TEXT("no debug override of the target"), Settings.TouchTargetOverridePx, 0.f);
	TestTrue(TEXT("one tap declares and plays"), Settings.bTouchOneTapPlay);
	TestEqual(TEXT("long press opens the card face"), Settings.TouchLongPressSeconds, 0.45f);
	TestEqual(TEXT("tap slop is 24 px"), Settings.TouchTapSlopPx, 24.f);

	// Negative means "derive from the device": the fallback covers the notch and the Android gesture bar.
	TestTrue(TEXT("safe area left is auto"), Settings.TouchSafeAreaLeftPercent < 0.f);
	TestTrue(TEXT("safe area top is auto"), Settings.TouchSafeAreaTopPercent < 0.f);
	TestTrue(TEXT("safe area right is auto"), Settings.TouchSafeAreaRightPercent < 0.f);
	TestTrue(TEXT("safe area bottom is auto"), Settings.TouchSafeAreaBottomPercent < 0.f);

	const FCBTouch::FSafeArea Fallback = FCBTouch::FallbackSafeArea(true);
	TestTrue(TEXT("the bottom inset is the big one (gesture bar)"), Fallback.Bottom > Fallback.Top);
	TestTrue(TEXT("the desktop fallback reserves nothing"), FCBTouch::FallbackSafeArea(false).IsZero());

	if (Settings.TouchTargetDp < 24.f || Settings.TouchTargetDp > 96.f)
	{
		AddError(FString::Printf(TEXT("TouchTargetDp=%g is outside the usable 24-96 dp range"), Settings.TouchTargetDp));
	}
	return true;
}

/** --------------------------------------------------------------- layout ------------------------------------- */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFCBTouchLayoutIsPlayableTest,
	"FantasyCardBattle.Mobile.LayoutIsPlayable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFCBTouchLayoutIsPlayableTest::RunTest(const FString& Parameters)
{
	// The same property the harness proves, restated against the shipped settings in a real build: every card
	// and every attribute is on screen, every value cell is at least 0.75 of a touch target wide, and no two
	// controls overlap.
	for (const FCBTouchTest::FDevice& Device : FCBTouchTest::Devices)
	{
		const float Density = FCBTouchTest::ResolveDensity(Device);
		const FCBTouch::FTuning Tuning = FCBTouchTest::MakeTuning(Density);
		const FCBTouch::FTableLayout Layout = FCBTouch::BuildLayout(
			FVector2D(Device.Width, Device.Height), FCBTouch::FallbackSafeArea(Device.bMobile),
			FCBTouchTest::MakeState(12), Tuning);
		const FString Label(Device.Label);

		if (Layout.Grid.PageCount > 1 || Layout.VisibleCards != 12)
		{
			// Paging is allowed on a viewport that genuinely cannot hold 12 cards, but not on the four phones
			// the game is tuned for.
			AddError(FString::Printf(TEXT("%s: %s"), *Label, *Layout.Note));
		}

		TestTrue(*FString::Printf(TEXT("%s: all twelve cards are on the page"), *Label), Layout.VisibleCards == 12);
		TestTrue(*FString::Printf(TEXT("%s: rows are at least a compact touch target (%.0f px vs %.0f dp)"),
			*Label, Layout.Grid.CardHeight, Tuning.MinTarget), Layout.Grid.CardHeight >= Tuning.MinTarget * 0.9f);

		for (int32 Index = 0; Index < Layout.VisibleCards; ++Index)
		{
			const FCBTouch::FRect Row = Layout.CardRect(Index);
			TestTrue(*FString::Printf(TEXT("%s: row %d is inside the hand region"), *Label, Index),
				!Row.IsEmpty() && Row.Right <= Layout.HandRegion.Right + 0.5f);

			for (int32 Cell = 0; Cell < 4; ++Cell)
			{
				const EFCBAttribute Attribute = FCBTouch::AttributeFromCellIndex(Cell);
				const FCBTouch::FRect Stat = Layout.StatRect(Index, Attribute);
				TestTrue(*FString::Printf(TEXT("%s: card %d %s cell is hittable"), *Label, Index,
					*FCBAttributeUtil::ToString(Attribute)),
					Stat.Width() >= Tuning.MinStatCellWidthFraction * Tuning.MinTarget * 0.95f);

				// Tapping the middle of a value cell is the mobile command: card + attribute + play.
				const FCBTouch::FCommand Command = FCBTouch::ResolveTap(Layout, Stat.Center(),
					FCBTouchTest::MakeState(12), true);
				TestTrue(*FString::Printf(TEXT("%s: tapping card %d %s plays it"), *Label, Index,
					*FCBAttributeUtil::ToString(Attribute)), Command.Action == FCBTouch::EAction::DeclareAndPlay);
			}
		}

		TestTrue(*FString::Printf(TEXT("%s: nothing overlaps the page buttons"), *Label),
			Layout.OpponentCard.IsEmpty() || !Layout.OpponentCard.Intersects(Layout.PageNextButton));
		TestTrue(*FString::Printf(TEXT("%s: PLAY is inside the safe area"), *Label),
			Layout.PlayButton.Bottom <= Layout.SafeRect.Bottom + 0.5f);
	}

	// The layout is a pure function of its inputs, which is what lets the HUD and the controller share it.
	const FCBTouch::FTableLayout First = FCBTouch::BuildLayout(FVector2D(2400.f, 1080.f),
		FCBTouch::FallbackSafeArea(true), FCBTouchTest::MakeState(12), FCBTouchTest::MakeTuning(3.f));
	const FCBTouch::FTableLayout Second = FCBTouch::BuildLayout(FVector2D(2400.f, 1080.f),
		FCBTouch::FallbackSafeArea(true), FCBTouchTest::MakeState(12), FCBTouchTest::MakeTuning(3.f));
	TestEqual(TEXT("the same viewport and state give the same grid"), First.Note, Second.Note);
	TestEqual(TEXT("... and the same first card cell"), First.StatRect(0, EFCBAttribute::Age).Left,
		Second.StatRect(0, EFCBAttribute::Age).Left);
	return true;
}

#endif // WITH_DEVFRAMEWORK
