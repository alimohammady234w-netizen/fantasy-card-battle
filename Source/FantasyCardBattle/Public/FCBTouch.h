// Copyright (c) Fantasy Card Battle. All rights reserved.
//
// FCBTouch.h - the mobile layer: where everything is, what a finger hit, and what that means.
//
// WHY THIS IS A SEPARATE, ENGINE-FREE MODULE
//   A touch build lives or dies on two questions that are pure geometry: is every command reachable with
//   a thumb, and is a tap ever ambiguous? Neither needs an engine. So the layout, the hit testing and the
//   gesture rules live here as arithmetic over FVector2D, and Tools/MockUE/SelfTest.cpp compiles this exact
//   file and checks it against the shipped tuning (see Docs/Mobile.md).
//
// THE ONE-LAYOUT RULE
//   AFCBDebugHud draws from FTableLayout and AFCBPlayerController resolves input from the *same* struct.
//   Keyboard, mouse and touch all end up in one FFCBCommand, which the controller executes. There is no
//   second code path that decides where a card is, and no rule that exists only for touch.
//
// WHAT PORTRAIT VS LANDSCAPE MEANS HERE
//   Nothing in this file branches on orientation. The grid is chosen from the space that is actually
//   available: 12 rows fit on a desktop window, so desktop gets one column; a 1080 px phone cannot fit 12
//   rows of a 48 dp target, so it gets two columns of six. Landscape is the supported orientation
//   (Config/DefaultEngine.ini) because that is what the available space assumes.

#pragma once

#include "CoreMinimal.h"
#include "FCBTypes.h"

namespace FCBTouch
{
	/** Minimal float rectangle in viewport pixels. Unreal has no core float rect; FBox2D is heavier than this needs. */
	struct FRect
	{
		float Left = 0.f;
		float Top = 0.f;
		float Right = 0.f;
		float Bottom = 0.f;

		FRect() = default;
		FRect(float InLeft, float InTop, float InRight, float InBottom)
			: Left(InLeft), Top(InTop), Right(InRight), Bottom(InBottom) {}

		static FRect FromSize(float X, float Y, float Width, float Height)
		{
			return FRect(X, Y, X + FMath::Max(0.f, Width), Y + FMath::Max(0.f, Height));
		}

		float Width() const { return Right - Left; }
		float Height() const { return Bottom - Top; }
		float Area() const { return FMath::Max(0.f, Width()) * FMath::Max(0.f, Height()); }

		/** A rect that is inside-out (or zero-sized) is empty, which is how "this button is not drawn" reads. */
		bool IsEmpty() const { return Right - Left <= 0.f || Bottom - Top <= 0.f; }

		bool Contains(const FVector2D& Point) const
		{
			return Point.X >= Left && Point.X <= Right && Point.Y >= Top && Point.Y <= Bottom;
		}

		bool Intersects(const FRect& Other) const
		{
			return !(Other.Left >= Right || Other.Right <= Left || Other.Top >= Bottom || Other.Bottom <= Top);
		}

		FVector2D Center() const { return FVector2D((Left + Right) * 0.5f, (Top + Bottom) * 0.5f); }
		FVector2D Min() const { return FVector2D(Left, Top); }
		FVector2D Max() const { return FVector2D(Right, Bottom); }

		FRect Shrunk(float Amount) const
		{
			return FRect(Left + Amount, Top + Amount, Right - Amount, Bottom - Amount);
		}

		FRect Expanded(float Amount) const { return Shrunk(-Amount); }

		FRect Translated(float DX, float DY) const
		{
			return FRect(Left + DX, Top + DY, Right + DX, Bottom + DY);
		}

		/** Same size, moved so its top-left is at X/Y - used to place rows from a pitch. */
		FRect WithTopLeft(float X, float Y) const
		{
			return FRect(X, Y, X + Width(), Y + Height());
		}

		/** Splits off a left slice, leaving the remainder; used for the name + four stat cells of a row. */
		FRect SplitLeft(float Amount, FRect& OutRemainder) const
		{
			const float Cut = FMath::Clamp(Amount, 0.f, Width());
			OutRemainder = FRect(Left + Cut, Top, Right, Bottom);
			return FRect(Left, Top, Left + Cut, Bottom);
		}

		FRect ClampedTo(const FRect& Bounds) const
		{
			return FRect(
				FMath::Max(Left, Bounds.Left), FMath::Max(Top, Bounds.Top),
				FMath::Min(Right, Bounds.Right), FMath::Min(Bottom, Bounds.Bottom));
		}
	};

	/** Safe-area insets as a fraction of the viewport (0.04 = 4%). Resolved from the device in the game layer. */
	struct FSafeArea
	{
		float Left = 0.f;
		float Top = 0.f;
		float Right = 0.f;
		float Bottom = 0.f;

		bool IsZero() const { return Left == 0.f && Top == 0.f && Right == 0.f && Bottom == 0.f; }

		/** A single uniform inset on all four edges - the "no idea what the notch does" default. */
		static FSafeArea Uniform(float Inset) { return FSafeArea{ Inset, Inset, Inset, Inset }; }

		FString ToString() const
		{
			return FString::Printf(TEXT("L%.1f%% T%.1f%% R%.1f%% B%.1f%%"),
				Left * 100.f, Top * 100.f, Right * 100.f, Bottom * 100.f);
		}
	};

	/** Which control a point landed on. */
	enum class ETarget : uint8
	{
		None,
		CardBody,			// the name/rarity part of a hand row: selects, never plays
		CardStat,			// one of the four value cells: the primary mobile command
		DetailPlay,			// the big PLAY button on the detail card
		DetailAttr,			// one of the four chips on the detail card: declares without playing
		PagePrev,
		PageNext,
		NewMatch,
		RollSeed,
		ToggleLog,
		TableBackground,	// anything else on the table
	};

	/** What the player asked for. Executed by AFCBPlayerController against UFCBGameInstance. */
	enum class EAction : uint8
	{
		None,
		SelectCard,
		DeclareAndPlay,		// select + declare + confirm, the one-tap mobile command
		DeclareAttribute,	// declare only; PLAY confirms (and the only path when one-tap play is off)
		Confirm,
		PrevPage,
		NextPage,
		SelectPrevCard,
		SelectNextCard,
		PeekCard,			// long press: show the full card face, play nothing
		ClosePeek,
		NewMatch,
		RollSeed,
		ToggleLog,
	};

	enum class EGesture : uint8
	{
		None,
		Tap,
		LongPress,
		SwipeLeft,
		SwipeRight,
		SwipeUp,
		SwipeDown,
	};

	/** Tunables that come from UFCBSettings. Plain numbers so the harness can sweep them. */
	struct FTuning
	{
		/** Smallest tappable square in viewport pixels. 48 dp x device DPI scale on a phone. */
		float MinTarget = 96.f;

		/**
		 * A value cell is narrower than it is tall and is still hittable: this is the floor on its width as a
		 * fraction of MinTarget. Below it the grid drops a column rather than shipping a cell a thumb misses.
		 */
		float MinStatCellWidthFraction = 0.75f;

		/**
		 * How far rows may shrink below MinTarget to keep the whole hand on one screen, as a fraction
		 * (0.92 = 44 dp rows for a 48 dp target). Below this the grid pages instead of squeezing: a phone
		 * that shows all 12 cards at 44 dp is a better game than one that hides four behind a swipe to reach
		 * 48 dp. Tests assert the floor and Docs/Mobile.md explains the trade.
		 */
		float CompactTolerance = 0.92f;

		/** Never grow a row past this, or two cards would eat a tablet screen. */
		float MaxCardHeight = 190.f;

		float CardGap = 6.f;

		/** The grid will use at most this many columns before it starts paging. */
		int32 MaxHandColumns = 2;

		/** Fraction of the full width the hand grid may take (the rest is the detail column). One column gets more. */
		float HandWidthFraction = 0.62f;
		float HandWidthFractionOneColumn = 0.58f;

		/** Fraction of the free width reserved for the detail column, so it can never be squeezed to nothing. */
		float MinDetailWidthFraction = 0.30f;

		float MinColumnWidth = 220.f;

		/** Fraction of a card row given to the name + rarity before the four value cells start. */
		float NameColumnFraction = 0.30f;

		/** Button bar height as a fraction of the viewport height, floored at MinTarget. */
		float ButtonHeightFraction = 0.075f;

		float ButtonWidthFraction = 0.15f;

		/** Height of the status strip (round / pot / deck / opponent) as a fraction of the viewport height. */
		float StatusHeightFraction = 0.075f;

		float MinStatusHeight = 30.f;
		float MaxStatusHeight = 96.f;
	};

	/** Gesture recognition thresholds. Times are seconds; distances are viewport pixels. */
	struct FGestureConfig
	{
		/** A finger on the glass longer than this is a long press, not a tap. */
		float TapMaxSeconds = 0.35f;
		float LongPressSeconds = 0.45f;

		/** A tap may drift this far; past it the input is a drag. */
		float TapSlop = 24.f;

		/** 0 = derive from the viewport (max(72, 5% of the short side)). */
		float SwipeMinDistance = 0.f;
		float SwipeMaxSeconds = 0.8f;

		/** How much the dominant axis must beat the other one before it counts as a horizontal or vertical swipe. */
		float AxisBias = 1.4f;

		/** Resolved swipe distance for a viewport: what the game layer and the tests both use. */
		static float ResolveSwipeDistance(const FGestureConfig& Config, const FVector2D& Viewport)
		{
			if (Config.SwipeMinDistance > 0.f)
			{
				return Config.SwipeMinDistance;
			}
			return FMath::Max(72.f, 0.05f * FMath::Min(Viewport.X, Viewport.Y));
		}
	};

	/** The hand grid: how many columns, how many rows fit, and whether paging is needed. */
	struct FHandGrid
	{
		int32 Columns = 1;
		int32 RowsPerPage = 12;			// rows per *page*, counting all columns
		int32 CardsPerPage = 12;		// Columns * RowsPerPage
		int32 PageCount = 1;
		bool bPaged = false;			// true when one page cannot show the whole hand

		float CardHeight = 0.f;			// row height in pixels (>= MinTarget unless the viewport is absurd)
		float RowPitch = 0.f;			// CardHeight + gap
		float ColumnWidth = 0.f;
		float ColumnPitch = 0.f;		// ColumnWidth + gap

		/** True when the grid had to shrink a row below the target to keep every card on one page. */
		bool bBelowTarget = false;

		static int32 PageForCard(const FHandGrid& Grid, int32 HandIndex)
		{
			if (Grid.CardsPerPage <= 0 || HandIndex < 0)
			{
				return 0;
			}
			return HandIndex / Grid.CardsPerPage;
		}

		static int32 FirstCardOnPage(const FHandGrid& Grid, int32 PageIndex)
		{
			return FMath::Max(0, PageIndex) * FMath::Max(1, Grid.CardsPerPage);
		}
	};

	/**
	 * Everything the layout needs to know about the match right now. Kept as plain data so the resolution
	 * rules can be unit tested without a match, an engine or a viewport.
	 */
	struct FState
	{
		int32 HandCount = 0;
		int32 SelectedCardIndex = 0;
		EFCBAttribute SelectedAttribute = EFCBAttribute::Power;
		int32 PageIndex = 0;
		bool bMatchOver = false;

		/** The player may only play on their turn, with usable data and a live match. */
		bool bCanPlay = true;

		/** The full-card overlay is up: every tap closes it instead of reaching the table. */
		bool bPeekActive = false;
	};

	/** The whole screen, resolved. Built by BuildLayout, drawn by the HUD, hit tested by HitTest. */
	struct FTableLayout
	{
		FVector2D Viewport = FVector2D::ZeroVector;
		FSafeArea Safe;
		FRect SafeRect;			// the viewport minus the safe-area insets
		FRect StatusBand;		// round / pot / deck / opponent line
		FRect OpponentCard;		// the face-down top card of the opponent
		FRect HandRegion;		// where the grid lives
		FRect DetailPanel;		// selected card, preview, log, PLAY
		FRect LogPanel;			// inside DetailPanel, below the preview
		FRect PlayButton;
		FRect PagePrevButton;
		FRect PageNextButton;
		FRect NewMatchButton;
		FRect RollSeedButton;
		FRect LogButton;
		FRect EndRestartButton;	// end-of-match: a full-width "play again" bar over the table

		/** The four chips on the detail panel that declare an attribute without playing. */
		FRect DetailAttrChips[4];

		FHandGrid Grid;
		int32 PageIndex = 0;			// clamped to the grid
		int32 FirstCard = 0;			// hand index of the first card on this page

		/**
		 * Row internals: the name/rarity strip and one value cell, resolved once here so that drawing, hit
		 * testing and the tests all read the same numbers. StatCellWidth is what guarantees the "a value cell
		 * is wide enough to hit" property (see MinStatCellWidthFraction).
		 */
		float NameWidth = 0.f;
		float StatCellWidth = 0.f;
		int32 VisibleCards = 0;
		int32 SelectedVisibleIndex = INDEX_NONE;	// row of the selected card on this page, or INDEX_NONE
		bool bMatchOver = false;
		bool bCanPlay = true;

		/** Human readable summary of what the layout decided - shown in the debug overlay and asserted in tests. */
		FString Note;

		bool IsVisible(int32 VisibleIndex) const { return VisibleIndex >= 0 && VisibleIndex < VisibleCards; }
		int32 HandIndexOf(int32 VisibleIndex) const { return FirstCard + VisibleIndex; }

		/** Row rect for a card on the current page. Empty when the index is off page. */
		FRect CardRect(int32 VisibleIndex) const;

		/** One of the four value cells of a row. Empty when off page. */
		FRect StatRect(int32 VisibleIndex, EFCBAttribute Attribute) const;

		/** The name/rarity strip of a row (the "select, do not play" target). */
		FRect NameRect(int32 VisibleIndex) const;
	};

	/** Result of a hit test. Target + which card + which attribute. */
	struct FHitTarget
	{
		ETarget Target = ETarget::None;
		int32 VisibleIndex = INDEX_NONE;
		int32 HandIndex = INDEX_NONE;
		EFCBAttribute Attribute = EFCBAttribute::Power;
		int32 PageDelta = 0;
	};

	/** What the player asked for, ready to execute. One struct, whichever input produced it. */
	struct FCommand
	{
		EAction Action = EAction::None;
		int32 HandIndex = INDEX_NONE;
		EFCBAttribute Attribute = EFCBAttribute::Power;
		int32 PageIndex = INDEX_NONE;
		ETarget Source = ETarget::None;

		/** One line for the log / debug overlay, e.g. "tap Age on card 3 of the hand". */
		FString Note;

		bool IsValid() const { return Action != EAction::None; }
	};

	//~ --------------------------------------------------------------------- layout

	/**
	 * Picks the grid for a hand region.
	 *
	 * The rule, in order:
	 *   1. Fewest columns that fit the whole hand at MinTarget (so a desktop keeps its single column).
	 *   2. Failing that, the widest allowed grid, paged, each page still at MinTarget.
	 *   3. Failing that (a hand taller than the screen allows even with MaxHandColumns), shrink rows to fit
	 *      one page and flag bBelowTarget - better one crowded page than a control the player cannot reach.
	 */
	FHandGrid ComputeHandGrid(float RegionWidth, float RegionHeight, int32 HandCount, const FTuning& Tuning);

	/** Builds the full screen. PageIndex in State is clamped, never trusted. */
	FTableLayout BuildLayout(const FVector2D& ViewportPixels, const FSafeArea& Safe, const FState& State, const FTuning& Tuning);

	//~ --------------------------------------------------------------------- input

	/** Topmost control under a point. Buttons win over cells, cells win over the table background. */
	FHitTarget HitTest(const FTableLayout& Layout, const FVector2D& Point);

	/** What a completed touch means. Down/Up are viewport pixels, times are seconds on any clock. */
	EGesture ClassifyGesture(const FVector2D& Down, float DownTime, const FVector2D& Up, float UpTime,
		const FGestureConfig& Config, const FVector2D& ViewportPixels);

	/**
	 * The one place a tap becomes an action. Returns EAction::None (with a Note) whenever the tap must be
	 * ignored, which is what keeps a double tap or a tap during the AI's turn from doing damage.
	 *
	 * @param bOneTapPlay can be turned off in settings: a value cell then only declares the attribute and
	 *                    PLAY confirms, which is the keyboard-like flow.
	 */
	FCommand ResolveTap(const FTableLayout& Layout, const FVector2D& Point, const FState& State, bool bOneTapPlay);

	/** Gesture -> action. Swipes page the hand (or walk the selection when everything fits), long press peeks. */
	FCommand ResolveGesture(const FTableLayout& Layout, const FVector2D& Down, float DownTime,
		const FVector2D& Up, float UpTime, const FState& State, const FGestureConfig& Config, bool bOneTapPlay);

	/** Stable name for the log and the tests. */
	FString DescribeAction(EAction Action);

	/** Stable name for a target, used by the notes and the debug overlay. */
	FString DescribeTarget(ETarget Target);

	//~ --------------------------------------------------------------------- helpers the game layer shares

	/** The attribute a value cell index refers to (0..3 -> Age/Power/Speed/Height). */
	EFCBAttribute AttributeFromCellIndex(int32 CellIndex);

	/** Inverse of AttributeFromCellIndex; EFCBAttribute::Max for anything unknown. */
	int32 CellIndexFromAttribute(EFCBAttribute Attribute);

	/**
	 * Auto safe-area inset for a viewport: 4% of the short side on touch builds, floored at 8 px. The device
	 * reports its real cutout through FSlateApplication and that always wins; this is the fallback for
	 * platform builds that report nothing (see Docs/Mobile.md).
	 */
	FSafeArea FallbackSafeArea(bool bTouchDevice);
}
