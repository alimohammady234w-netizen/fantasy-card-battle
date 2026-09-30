// Copyright (c) Fantasy Card Battle. All rights reserved.
//
// FCBTouch.cpp - the layout, hit testing and gesture rules for touch input.
//
// See FCBTouch.h for why this file exists and Docs/Mobile.md for the numbers it is tuned to. Everything here
// is arithmetic: no engine, no actors, no Slate. That is what lets Tools/MockUE/SelfTest.cpp press every
// button and every value cell of a hand on a simulated phone.

#include "FCBTouch.h"

namespace FCBTouch
{
	namespace
	{
		/** UE has FMath::DivideAndRoundUp; this file stays shim-friendly without it. */
		int32 CeilDiv(int32 Numerator, int32 Denominator)
		{
			return Denominator <= 0 ? 0 : (Numerator + Denominator - 1) / Denominator;
		}

		/** Clamp a rect into a bounds rect, collapsing to a zero-size rect at the bounds edge when inverted. */
		FRect Sanitize(const FRect& Rect, const FRect& Bounds)
		{
			const FRect Clamped = Rect.ClampedTo(Bounds);
			if (!Clamped.IsEmpty())
			{
				return Clamped;
			}
			// An inside-out rect means "no space left"; pin it to the edge so callers get an empty, harmless box.
			const float X = FMath::Clamp(Rect.Left, Bounds.Left, Bounds.Right);
			const float Y = FMath::Clamp(Rect.Top, Bounds.Top, Bounds.Bottom);
			return FRect(X, Y, X, Y);
		}
	}

	int32 CellIndexFromAttribute(EFCBAttribute Attribute)
	{
		switch (Attribute)
		{
		case EFCBAttribute::Age:	return 0;
		case EFCBAttribute::Power:	return 1;
		case EFCBAttribute::Speed:	return 2;
		case EFCBAttribute::Height:	return 3;
		default:					return INDEX_NONE;
		}
	}

	EFCBAttribute AttributeFromCellIndex(int32 CellIndex)
	{
		switch (CellIndex)
		{
		case 0:		return EFCBAttribute::Age;
		case 1:		return EFCBAttribute::Power;
		case 2:		return EFCBAttribute::Speed;
		case 3:		return EFCBAttribute::Height;
		default:	return EFCBAttribute::Max;
		}
	}

	FSafeArea FallbackSafeArea(bool bTouchDevice)
	{
		if (!bTouchDevice)
		{
			return FSafeArea();
		}
		// Asymmetric on purpose: the top is a status bar, the bottom is the Android gesture bar, which is what
		// actually eats the last hand row on a phone (Docs/Mobile.md). The device value from Slate wins.
		FSafeArea Area;
		Area.Left = 0.03f;
		Area.Top = 0.02f;
		Area.Right = 0.03f;
		Area.Bottom = 0.05f;
		return Area;
	}

	namespace
	{
		/**
		 * The name strip gives way before the value cells do: a 30% name column on a two-column phone grid
		 * would leave cells narrower than a fingertip, and the full card name is on the detail panel anyway.
		 */
		float ResolveNameFraction(float ColumnWidth, const FTuning& Tuning)
		{
			const float MinStat = FMath::Max(1.f, Tuning.MinTarget * Tuning.MinStatCellWidthFraction);
			const float MaxNameFraction = 1.f - (4.f * MinStat) / FMath::Max(1.f, ColumnWidth);
			return FMath::Clamp(FMath::Min(Tuning.NameColumnFraction, MaxNameFraction), 0.18f, 0.45f);
		}

		float ResolveStatCellWidth(float ColumnWidth, const FTuning& Tuning)
		{
			return FMath::Max(1.f, ColumnWidth * (1.f - ResolveNameFraction(ColumnWidth, Tuning))) / 4.f;
		}
	}

	//~ -------------------------------------------------------------------------------- grid

	FHandGrid ComputeHandGrid(float RegionWidth, float RegionHeight, int32 HandCount, const FTuning& Tuning)
	{
		FHandGrid Grid;

		const int32 Count = FMath::Max(1, HandCount);
		const float Gap = FMath::Max(0.f, Tuning.CardGap);
		const float MinTarget = FMath::Max(16.f, Tuning.MinTarget);
		const int32 MaxColumns = FMath::Clamp(Tuning.MaxHandColumns, 1, 4);
		const float Width = FMath::Max(1.f, RegionWidth);
		const float Height = FMath::Max(1.f, RegionHeight);

		const auto ColumnWidthFor = [Width, Gap](int32 Columns) -> float
		{
			return (Width - Gap * static_cast<float>(Columns - 1)) / static_cast<float>(FMath::Max(1, Columns));
		};

		const auto RowHeightFor = [Height, Gap](int32 Rows) -> float
		{
			return (Height - Gap * static_cast<float>(Rows - 1)) / static_cast<float>(FMath::Max(1, Rows));
		};

		// Rows that fit at the target height: rows*Target + (rows-1)*Gap <= Height. The compact pass allows the
		// rows to shrink to CompactTolerance of the target, because on a phone keeping all 12 cards on one
		// screen at 44 dp beats a 48 dp grid that hides four of them behind a swipe.
		const float CompactTarget = MinTarget * FMath::Clamp(Tuning.CompactTolerance, 0.6f, 1.f);
		// The +1 is rounding slack: a fraction of a pixel must not flip a phone between "all twelve cards on
		// one screen at 44 dp" and "two pages of five", which is a visible difference from a 0.4 px remainder.
		const int32 FitRowsFull = FMath::Max(1, FMath::FloorToInt32((Height + Gap + 1.f) / (MinTarget + Gap)));
		const int32 FitRowsCompact = FMath::Max(1, FMath::FloorToInt32((Height + Gap + 1.f) / (CompactTarget + Gap)));

		// A column count is only allowed if its value cells stay tappable and its cards stay readable. Larger
		// counts are strictly worse, so the search stops at the first one that fails.
		int32 AllowedColumns = 1;
		for (int32 Columns = 2; Columns <= MaxColumns; ++Columns)
		{
			const float ColumnWidth = ColumnWidthFor(Columns);
			const bool bWideEnoughForCells = ResolveStatCellWidth(ColumnWidth, Tuning) >= MinTarget * Tuning.MinStatCellWidthFraction - 1.f;
			if (ColumnWidth >= Tuning.MinColumnWidth && bWideEnoughForCells)
			{
				AllowedColumns = Columns;
			}
			else
			{
				break;
			}
		}

		int32 Columns = 0;
		int32 Rows = 0;
		bool bBelowTarget = false;

		const auto TryFit = [&](int32 FitRows, bool bCompact) -> bool
		{
			for (int32 Candidate = 1; Candidate <= AllowedColumns; ++Candidate)
			{
				const int32 CandidateRows = CeilDiv(Count, Candidate);
				if (CandidateRows <= FitRows)
				{
					// Take the narrowest grid that shows the whole hand, so a desktop keeps its single column
					// and only a viewport that genuinely cannot fit 12 rows grows a second one.
					Columns = Candidate;
					Rows = CandidateRows;
					bBelowTarget = bCompact;
					return true;
				}
			}
			return false;
		};

		if (!TryFit(FitRowsFull, false) && !TryFit(FitRowsCompact, true))
		{
			// Neither at the target size nor compacted: page it, at the compact row height, and let the player
			// swipe. Every page still shows rows a finger can hit.
			Columns = AllowedColumns;
			Rows = FMath::Max(1, FMath::Min(FitRowsCompact, CeilDiv(Count, Columns)));
			bBelowTarget = true;
		}

		Grid.Columns = Columns;
		Grid.RowsPerPage = Rows;
		Grid.CardsPerPage = FMath::Max(1, Columns * Rows);
		Grid.PageCount = FMath::Max(1, CeilDiv(Count, Grid.CardsPerPage));
		Grid.bPaged = Grid.PageCount > 1;
		Grid.ColumnWidth = ColumnWidthFor(Columns);
		Grid.ColumnPitch = Grid.ColumnWidth + Gap;

		// A row never exceeds the pixels it was given, whatever the viewport does.
		const float Fitted = FMath::Max(1.f, FMath::Min(RowHeightFor(Rows), Height));
		Grid.CardHeight = FMath::Min(Fitted, FMath::Max(MinTarget, Tuning.MaxCardHeight));
		Grid.bBelowTarget = bBelowTarget || Grid.CardHeight < MinTarget;
		Grid.RowPitch = Grid.CardHeight + Gap;
		return Grid;
	}

	//~ -------------------------------------------------------------------------------- rects

	FRect FTableLayout::CardRect(int32 VisibleIndex) const
	{
		if (!IsVisible(VisibleIndex) || Grid.Columns <= 0)
		{
			return FRect();
		}
		const int32 Row = VisibleIndex / Grid.Columns;
		const int32 Column = VisibleIndex % Grid.Columns;
		const float X = HandRegion.Left + Grid.ColumnPitch * static_cast<float>(Column);
		const float Y = HandRegion.Top + Grid.RowPitch * static_cast<float>(Row);
		return FRect::FromSize(X, Y, Grid.ColumnWidth, Grid.CardHeight);
	}

	FRect FTableLayout::NameRect(int32 VisibleIndex) const
	{
		const FRect Row = CardRect(VisibleIndex);
		if (Row.IsEmpty())
		{
			return Row;
		}
		FRect Remainder;
		return Row.SplitLeft(FMath::Clamp(NameWidth, 0.f, Row.Width()), Remainder);
	}

	FRect FTableLayout::StatRect(int32 VisibleIndex, EFCBAttribute Attribute) const
	{
		const int32 Cell = CellIndexFromAttribute(Attribute);
		const FRect Row = CardRect(VisibleIndex);
		if (Row.IsEmpty() || Cell == INDEX_NONE)
		{
			return FRect();
		}

		const float Left = Row.Left + NameWidth + StatCellWidth * static_cast<float>(Cell);
		return FRect(Left, Row.Top, Left + StatCellWidth, Row.Bottom);
	}

	//~ -------------------------------------------------------------------------------- layout

	FTableLayout BuildLayout(const FVector2D& ViewportPixels, const FSafeArea& SafeArea, const FState& State, const FTuning& Tuning)
	{
		FTableLayout Layout;
		Layout.Viewport = ViewportPixels;
		Layout.bMatchOver = State.bMatchOver;
		Layout.bCanPlay = State.bCanPlay;

		const float ViewW = FMath::Max(1.f, ViewportPixels.X);
		const float ViewH = FMath::Max(1.f, ViewportPixels.Y);

		// Safe area: fractions of the viewport per edge. A device can report anything; keep it sane.
		FSafeArea Safe = SafeArea;
		Safe.Left = FMath::Clamp(Safe.Left, 0.f, 0.25f);
		Safe.Right = FMath::Clamp(Safe.Right, 0.f, 0.25f);
		Safe.Top = FMath::Clamp(Safe.Top, 0.f, 0.25f);
		Safe.Bottom = FMath::Clamp(Safe.Bottom, 0.f, 0.25f);
		Layout.Safe = Safe;
		Layout.SafeRect = FRect(ViewW * Safe.Left, ViewH * Safe.Top, ViewW * (1.f - Safe.Right), ViewH * (1.f - Safe.Bottom));

		const float Gap = FMath::Max(0.f, Tuning.CardGap);
		const float MinTarget = FMath::Max(16.f, Tuning.MinTarget);

		// --- buttons. They are the first thing laid out, because nothing may be allowed to push a button
		//     below the touch target: the top band grows to fit them instead.
		const float ButtonHeight = FMath::Clamp(MinTarget, 32.f, ViewH * 0.18f);
		const float ButtonWidth = FMath::Clamp(ViewW * Tuning.ButtonWidthFraction, MinTarget, ViewW * 0.28f);
		const float StatusHeight = FMath::Clamp(ViewH * Tuning.StatusHeightFraction, Tuning.MinStatusHeight, Tuning.MaxStatusHeight);
		const float TopBandHeight = FMath::Max(StatusHeight, ButtonHeight + 8.f);
		const float TopBandBottom = Layout.SafeRect.Top + TopBandHeight;

		const float ButtonY = Layout.SafeRect.Top + (TopBandHeight - ButtonHeight) * 0.5f;
		float ButtonRight = Layout.SafeRect.Right;
		Layout.LogButton = FRect(ButtonRight - ButtonWidth, ButtonY, ButtonRight, ButtonY + ButtonHeight);
		ButtonRight -= ButtonWidth + Gap;
		Layout.RollSeedButton = FRect(ButtonRight - ButtonWidth, ButtonY, ButtonRight, ButtonY + ButtonHeight);
		ButtonRight -= ButtonWidth + Gap;
		Layout.NewMatchButton = FRect(ButtonRight - ButtonWidth, ButtonY, ButtonRight, ButtonY + ButtonHeight);

		// Page buttons only exist when the hand had to be split; they sit at the left end of the top band.
		Layout.PagePrevButton = FRect(Layout.SafeRect.Left, ButtonY, Layout.SafeRect.Left + ButtonWidth * 0.6f, ButtonY + ButtonHeight);
		Layout.PageNextButton = FRect(Layout.PagePrevButton.Right + Gap, ButtonY,
			Layout.PagePrevButton.Right + Gap + ButtonWidth * 0.6f, ButtonY + ButtonHeight);
		Layout.NewMatchButton = Sanitize(Layout.NewMatchButton, Layout.SafeRect);
		Layout.RollSeedButton = Sanitize(Layout.RollSeedButton, Layout.SafeRect);
		Layout.LogButton = Sanitize(Layout.LogButton, Layout.SafeRect);

		// --- content band, between the top band and (on the end screen) the play-again bar.
		float ContentTop = FMath::Min(TopBandBottom + Gap, Layout.SafeRect.Bottom);
		float ContentBottom = Layout.SafeRect.Bottom;

		if (State.bMatchOver)
		{
			const float RestartHeight = FMath::Clamp(MinTarget * 1.2f, 44.f, ViewH * 0.18f);
			Layout.EndRestartButton = FRect(Layout.SafeRect.Left, Layout.SafeRect.Bottom - RestartHeight,
				Layout.SafeRect.Right, Layout.SafeRect.Bottom);
			ContentBottom = FMath::Max(ContentTop, Layout.EndRestartButton.Top - Gap);
		}

		const float ContentHeight = FMath::Max(1.f, ContentBottom - ContentTop);

		// --- how wide may the hand be? The detail column always keeps its share, so the preview, the ability
		//     text and PLAY are never squeezed out by a second column of cards.
		const float MinDetailWidth = FMath::Min(ViewW * Tuning.MinDetailWidthFraction, ViewW * 0.5f);
		const float AvailableWidth = FMath::Max(1.f, Layout.SafeRect.Width() - MinDetailWidth - Gap);
		const int32 HandCount = FMath::Max(0, State.HandCount);

		// One probe at the maximum width tells us how many columns the hand region can afford; the grid then
		// decides whether it needs them.
		const float HandWidthFraction = (HandCount > 0) ? Tuning.HandWidthFraction : Tuning.HandWidthFractionOneColumn;
		const float WantedHandWidth = FMath::Clamp(Layout.SafeRect.Width() * HandWidthFraction,
			FMath::Min(1.f, Tuning.MinColumnWidth), AvailableWidth);

		Layout.Grid = ComputeHandGrid(WantedHandWidth, ContentHeight, HandCount, Tuning);

		// With several columns the grid is wider than the wanted fraction: a card must stay usable, so grant
		// the hand the space the columns need, up to what the detail column leaves.
		const float NeededHandWidth = Layout.Grid.ColumnPitch * static_cast<float>(Layout.Grid.Columns) - Gap;
		const float HandWidth = FMath::Clamp(FMath::Max(WantedHandWidth, NeededHandWidth),
			1.f, AvailableWidth);

		Layout.HandRegion = FRect(Layout.SafeRect.Left, ContentTop, Layout.SafeRect.Left + HandWidth, ContentBottom);
		Layout.DetailPanel = FRect(Layout.HandRegion.Right + Gap, ContentTop, Layout.SafeRect.Right, ContentBottom);
		Layout.DetailPanel = Sanitize(Layout.DetailPanel, Layout.SafeRect);

		// --- paging state. The requested page is clamped, never trusted: a hand can lose three cards between
		//     frames and a stale page index must resolve to something drawable.
		Layout.PageIndex = FMath::Clamp(State.PageIndex, 0, Layout.Grid.PageCount - 1);
		Layout.FirstCard = FHandGrid::FirstCardOnPage(Layout.Grid, Layout.PageIndex);
		Layout.VisibleCards = FMath::Clamp(HandCount - Layout.FirstCard, 0, Layout.Grid.CardsPerPage);
		Layout.SelectedVisibleIndex = (State.SelectedCardIndex >= Layout.FirstCard
			&& State.SelectedCardIndex < Layout.FirstCard + Layout.VisibleCards)
			? (State.SelectedCardIndex - Layout.FirstCard) : INDEX_NONE;

		const bool bPaged = Layout.Grid.bPaged && Layout.Grid.PageCount > 1;
		if (!bPaged)
		{
			Layout.PagePrevButton = FRect();
			Layout.PageNextButton = FRect();
		}

		// --- detail column: four attribute chips, the log, then PLAY along the bottom edge (where a thumb is).
		const float PanelWidth = FMath::Max(1.f, Layout.DetailPanel.Width());
		const float PlayHeight = FMath::Clamp(MinTarget * 1.15f, 44.f, ViewH * 0.2f);
		Layout.PlayButton = FRect(Layout.DetailPanel.Left, Layout.DetailPanel.Bottom - PlayHeight,
			Layout.DetailPanel.Right, Layout.DetailPanel.Bottom);

		const float ChipGap = Gap;
		const float ChipWidth = FMath::Max(1.f, (PanelWidth - ChipGap * 3.f) / 4.f);
		const float ChipTop = FMath::Max(Layout.DetailPanel.Top, Layout.PlayButton.Top - Gap - ButtonHeight);
		for (int32 Chip = 0; Chip < 4; ++Chip)
		{
			const float X = Layout.DetailPanel.Left + (ChipWidth + ChipGap) * static_cast<float>(Chip);
			Layout.DetailAttrChips[Chip] = FRect(X, ChipTop, X + ChipWidth, ChipTop + ButtonHeight);
		}

		const float PreviewBottom = Layout.DetailPanel.Top + FMath::Clamp(ViewH * 0.30f, 90.f, ViewH * 0.5f);
		Layout.LogPanel = FRect(Layout.DetailPanel.Left, FMath::Min(PreviewBottom, ChipTop - Gap),
			Layout.DetailPanel.Right, FMath::Max(Layout.DetailPanel.Top, ChipTop - Gap));
		Layout.LogPanel = Sanitize(Layout.LogPanel, Layout.SafeRect);

		Layout.StatusBand = FRect(Layout.SafeRect.Left, Layout.SafeRect.Top, Layout.SafeRect.Right, TopBandBottom);

		// The opponent's face-down top card sits at the left of the status band, unless the hand is paged - then
		// the page buttons own that corner and the card steps aside, because two controls in one place is the
		// one bug a hit test cannot survive.
		const float OpponentCardWidth = FMath::Clamp(Layout.SafeRect.Width() * 0.22f, 120.f, Layout.SafeRect.Width() * 0.4f);
		const float OpponentLeft = bPaged ? Layout.PageNextButton.Right + Gap : Layout.SafeRect.Left;
		const float OpponentTop = Layout.SafeRect.Top + 4.f;
		Layout.OpponentCard = FRect(OpponentLeft, OpponentTop,
			OpponentLeft + OpponentCardWidth, FMath::Max(OpponentTop, TopBandBottom - 4.f));

		// Name column vs four value cells, resolved by the same helper the grid used to pick its column count.
		Layout.NameWidth = Layout.Grid.ColumnWidth * ResolveNameFraction(Layout.Grid.ColumnWidth, Tuning);
		Layout.StatCellWidth = ResolveStatCellWidth(Layout.Grid.ColumnWidth, Tuning);

		Layout.Note = FString::Printf(TEXT("%dx%d grid, %d cards/page, page %d/%d, row %.0fpx, cell %.0fpx%s%s"),
			Layout.Grid.Columns, Layout.Grid.RowsPerPage, Layout.Grid.CardsPerPage,
			Layout.PageIndex + 1, Layout.Grid.PageCount,
			Layout.Grid.CardHeight, Layout.StatCellWidth,
			Layout.Grid.bPaged ? TEXT(", paged") : TEXT(""),
			Layout.Grid.bBelowTarget ? TEXT(", below target") : TEXT(""));

		return Layout;
	}

	//~ -------------------------------------------------------------------------------- hit testing

	FHitTarget HitTest(const FTableLayout& Layout, const FVector2D& Point)
	{
		FHitTarget Hit;

		// 1. Buttons. They are on top of everything: a tap on PLAY must never be read as a card tap even if a
		//    future layout lets the two overlap.
		struct FButtonProbe
		{
			const FRect* Rect;
			ETarget Target;
		};
		const FButtonProbe Buttons[] =
		{
			{ &Layout.EndRestartButton, ETarget::TableBackground },
			{ &Layout.PlayButton, ETarget::DetailPlay },
			{ &Layout.NewMatchButton, ETarget::NewMatch },
			{ &Layout.RollSeedButton, ETarget::RollSeed },
			{ &Layout.LogButton, ETarget::ToggleLog },
			{ &Layout.PagePrevButton, ETarget::PagePrev },
			{ &Layout.PageNextButton, ETarget::PageNext },
		};
		for (const FButtonProbe& Button : Buttons)
		{
			if (!Button.Rect->IsEmpty() && Button.Rect->Contains(Point))
			{
				Hit.Target = Button.Target;
				return Hit;
			}
		}

		// 2. The hand. Value cells are tested before the row, so a tap on "Power" is never a plain selection.
		for (int32 VisibleIndex = 0; VisibleIndex < Layout.VisibleCards; ++VisibleIndex)
		{
			for (int32 Cell = 0; Cell < 4; ++Cell)
			{
				const EFCBAttribute Attribute = AttributeFromCellIndex(Cell);
				if (Layout.StatRect(VisibleIndex, Attribute).Contains(Point))
				{
					Hit.Target = ETarget::CardStat;
					Hit.VisibleIndex = VisibleIndex;
					Hit.HandIndex = Layout.HandIndexOf(VisibleIndex);
					Hit.Attribute = Attribute;
					return Hit;
				}
			}

			if (Layout.CardRect(VisibleIndex).Contains(Point))
			{
				Hit.Target = ETarget::CardBody;
				Hit.VisibleIndex = VisibleIndex;
				Hit.HandIndex = Layout.HandIndexOf(VisibleIndex);
				return Hit;
			}
		}

		// 3. The detail card's attribute chips: declare without playing, for the player who wants to see the
		//    projected outcome first.
		for (int32 Chip = 0; Chip < 4; ++Chip)
		{
			if (!Layout.DetailAttrChips[Chip].IsEmpty() && Layout.DetailAttrChips[Chip].Contains(Point))
			{
				Hit.Target = ETarget::DetailAttr;
				Hit.Attribute = AttributeFromCellIndex(Chip);
				Hit.HandIndex = Layout.SelectedVisibleIndex != INDEX_NONE
					? Layout.HandIndexOf(Layout.SelectedVisibleIndex) : INDEX_NONE;
				return Hit;
			}
		}

		// 4. Anywhere else on the table.
		if (Layout.SafeRect.Contains(Point))
		{
			Hit.Target = ETarget::TableBackground;
		}
		return Hit;
	}

	//~ -------------------------------------------------------------------------------- gestures

	EGesture ClassifyGesture(const FVector2D& Down, float DownTime, const FVector2D& Up, float UpTime,
		const FGestureConfig& Config, const FVector2D& ViewportPixels)
	{
		const float Held = FMath::Max(0.f, UpTime - DownTime);
		const FVector2D Delta = Up - Down;
		const FVector2D Abs = Delta.GetAbs();
		const float Distance = Delta.Size();

		if (Distance >= FGestureConfig::ResolveSwipeDistance(Config, ViewportPixels) && Held <= Config.SwipeMaxSeconds)
		{
			if (Abs.X >= Abs.Y * Config.AxisBias)
			{
				return Delta.X > 0.f ? EGesture::SwipeRight : EGesture::SwipeLeft;
			}
			if (Abs.Y >= Abs.X * Config.AxisBias)
			{
				return Delta.Y > 0.f ? EGesture::SwipeDown : EGesture::SwipeUp;
			}
			return EGesture::None; // a diagonal long drag is not a command
		}

		if (Distance <= Config.TapSlop)
		{
			// Deliberately no dead band between a tap and a long press: a slow tap is a tap. Holding past the
			// long-press time is how a card is inspected, and the PLAY button is always available for players
			// who cannot hit the value cells (Docs/Mobile.md, "Accessibility").
			return Held >= Config.LongPressSeconds ? EGesture::LongPress : EGesture::Tap;
		}

		return EGesture::None; // too far to be a tap, too short/slow to be a swipe
	}

	//~ -------------------------------------------------------------------------------- resolution

	FCommand ResolveTap(const FTableLayout& Layout, const FVector2D& Point, const FState& State, bool bOneTapPlay)
	{
		FCommand Command;
		const FHitTarget Hit = HitTest(Layout, Point);
		Command.Source = Hit.Target;

		// The overlay is modal: while a card is being inspected, a tap closes it and nothing else.
		if (State.bPeekActive)
		{
			Command.Action = EAction::ClosePeek;
			Command.Note = TEXT("tap closed the card overlay");
			return Command;
		}

		// Match-level buttons work at any time, including while the AI is thinking and after the match ends.
		switch (Hit.Target)
		{
		case ETarget::NewMatch:
			Command.Action = EAction::NewMatch;
			Command.Note = TEXT("new match");
			return Command;
		case ETarget::RollSeed:
			Command.Action = EAction::RollSeed;
			Command.Note = TEXT("new deal, fresh seed");
			return Command;
		case ETarget::ToggleLog:
			Command.Action = EAction::ToggleLog;
			Command.Note = TEXT("toggle the log");
			return Command;
		default:
			break;
		}

		// The end screen is one big play-again bar; nothing else on it is interactive.
		if (State.bMatchOver)
		{
			Command.Action = EAction::NewMatch;
			Command.Note = TEXT("match over: tap deals a new hand");
			return Command;
		}

		// Paging is presentation-only, so it stays available while the opponent is deciding.
		if (Hit.Target == ETarget::PagePrev || Hit.Target == ETarget::PageNext)
		{
			if (!Layout.Grid.bPaged || Layout.Grid.PageCount <= 1)
			{
				Command.Note = TEXT("the whole hand is on screen: nothing to page");
				return Command;
			}
			// Buttons wrap like the swipe does: a page turn that dead-ends at the last page reads as a dead
			// button, and wrapping is what every phone carousel does.
			const int32 Delta = Hit.Target == ETarget::PageNext ? 1 : -1;
			const int32 PageCount = FMath::Max(1, Layout.Grid.PageCount);
			Command.Action = Hit.Target == ETarget::PagePrev ? EAction::PrevPage : EAction::NextPage;
			Command.PageIndex = (Layout.PageIndex + Delta + PageCount) % PageCount;
			Command.Note = FString::Printf(TEXT("page %d of %d"), Command.PageIndex + 1, PageCount);
			return Command;
		}

		if (!State.bCanPlay)
		{
			Command.Action = EAction::None;
			Command.Note = TEXT("ignored: it is not a playable moment");
			return Command;
		}

		switch (Hit.Target)
		{
		case ETarget::CardStat:
			Command.HandIndex = Hit.HandIndex;
			Command.Attribute = Hit.Attribute;
			Command.Action = bOneTapPlay ? EAction::DeclareAndPlay : EAction::DeclareAttribute;
			Command.Note = FString::Printf(TEXT("%s %s on card %d"),
				bOneTapPlay ? TEXT("play") : TEXT("declare"),
				*FCBAttributeUtil::ToString(Hit.Attribute), Hit.HandIndex);
			return Command;

		case ETarget::CardBody:
			Command.Action = EAction::SelectCard;
			Command.HandIndex = Hit.HandIndex;
			Command.Note = FString::Printf(TEXT("select card %d"), Hit.HandIndex);
			return Command;

		case ETarget::DetailAttr:
			Command.Action = EAction::DeclareAttribute;
			Command.Attribute = Hit.Attribute;
			Command.HandIndex = Hit.HandIndex;
			Command.Note = FString::Printf(TEXT("declare %s"), *FCBAttributeUtil::ToString(Hit.Attribute));
			return Command;

		case ETarget::DetailPlay:
			Command.Action = EAction::Confirm;
			Command.HandIndex = Hit.HandIndex;
			Command.Attribute = State.SelectedAttribute;
			Command.Note = FString::Printf(TEXT("play %s"), *FCBAttributeUtil::ToString(State.SelectedAttribute));
			return Command;

		default:
			// Empty table. Deliberately a no-op: on a phone a stray tap in the middle of the table must not
			// spend a card. The keyboard keeps Enter as the confirm key and PLAY is always on screen.
			Command.Action = EAction::None;
			Command.Note = TEXT("tapped the table: nothing selected");
			return Command;
		}
	}

	FCommand ResolveGesture(const FTableLayout& Layout, const FVector2D& Down, float DownTime,
		const FVector2D& Up, float UpTime, const FState& State, const FGestureConfig& Config, bool bOneTapPlay)
	{
		FCommand Command;
		const EGesture Gesture = ClassifyGesture(Down, DownTime, Up, UpTime, Config, Layout.Viewport);

		switch (Gesture)
		{
		case EGesture::Tap:
			return ResolveTap(Layout, Up, State, bOneTapPlay);

		case EGesture::LongPress:
		{
			const FHitTarget Hit = HitTest(Layout, Down);
			if (Hit.Target == ETarget::CardStat || Hit.Target == ETarget::CardBody)
			{
				Command.Action = EAction::PeekCard;
				Command.HandIndex = Hit.HandIndex;
				Command.Source = Hit.Target;
				Command.Note = FString::Printf(TEXT("hold: inspect card %d"), Hit.HandIndex);
				return Command;
			}
			Command.Note = TEXT("hold: nothing to inspect here");
			return Command;
		}

		case EGesture::SwipeLeft:
		case EGesture::SwipeRight:
		{
			if (!Layout.HandRegion.Contains(Down))
			{
				Command.Note = TEXT("swipes only work over the hand");
				return Command;
			}
			if (State.bMatchOver)
			{
				Command.Note = TEXT("match over");
				return Command;
			}

			const bool bForward = (Gesture == EGesture::SwipeLeft);
			if (Layout.Grid.bPaged && Layout.Grid.PageCount > 1)
			{
				Command.Action = bForward ? EAction::NextPage : EAction::PrevPage;
				Command.PageIndex = Layout.PageIndex + (bForward ? 1 : -1);
				Command.Note = FString::Printf(TEXT("swipe: page %d of %d"),
					FMath::Clamp(Command.PageIndex, 0, Layout.Grid.PageCount - 1) + 1, Layout.Grid.PageCount);
				// Swiping past the end wraps, which is how every carousel on a phone behaves.
				Command.PageIndex = (Command.PageIndex + Layout.Grid.PageCount) % Layout.Grid.PageCount;
				return Command;
			}

			// Everything fits: a swipe walks the selection instead, the touch equivalent of the arrow keys.
			if (!bForward)
			{
				Command.Action = EAction::SelectPrevCard;
				Command.Note = TEXT("swipe: previous card");
			}
			else
			{
				Command.Action = EAction::SelectNextCard;
				Command.Note = TEXT("swipe: next card");
			}
			return Command;
		}

		default:
			Command.Note = TEXT("gesture ignored");
			return Command;
		}
	}

	//~ -------------------------------------------------------------------------------- text

	FString DescribeAction(EAction Action)
	{
		switch (Action)
		{
		case EAction::None:				return TEXT("None");
		case EAction::SelectCard:		return TEXT("SelectCard");
		case EAction::DeclareAndPlay:	return TEXT("DeclareAndPlay");
		case EAction::DeclareAttribute:	return TEXT("DeclareAttribute");
		case EAction::Confirm:			return TEXT("Confirm");
		case EAction::PrevPage:			return TEXT("PrevPage");
		case EAction::NextPage:			return TEXT("NextPage");
		case EAction::SelectPrevCard:	return TEXT("SelectPrevCard");
		case EAction::SelectNextCard:	return TEXT("SelectNextCard");
		case EAction::PeekCard:			return TEXT("PeekCard");
		case EAction::ClosePeek:		return TEXT("ClosePeek");
		case EAction::NewMatch:			return TEXT("NewMatch");
		case EAction::RollSeed:			return TEXT("RollSeed");
		case EAction::ToggleLog:		return TEXT("ToggleLog");
		default:						return TEXT("Unknown");
		}
	}

	FString DescribeTarget(ETarget Target)
	{
		switch (Target)
		{
		case ETarget::None:				return TEXT("None");
		case ETarget::CardBody:			return TEXT("CardBody");
		case ETarget::CardStat:			return TEXT("CardStat");
		case ETarget::DetailPlay:		return TEXT("DetailPlay");
		case ETarget::DetailAttr:		return TEXT("DetailAttr");
		case ETarget::PagePrev:			return TEXT("PagePrev");
		case ETarget::PageNext:			return TEXT("PageNext");
		case ETarget::NewMatch:			return TEXT("NewMatch");
		case ETarget::RollSeed:			return TEXT("RollSeed");
		case ETarget::ToggleLog:		return TEXT("ToggleLog");
		case ETarget::TableBackground:	return TEXT("TableBackground");
		default:						return TEXT("Unknown");
		}
	}
}
