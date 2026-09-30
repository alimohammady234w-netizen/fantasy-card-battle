// Copyright (c) Fantasy Card Battle. All rights reserved.
//
// FCBGameMode.cpp - game mode, player controller and the code-only HUD.
//
// The interesting part of this file is the boundary: everything that decides *where* a control is lives in
// FCBTouch (engine-free, tested by Tools/MockUE/SelfTest.cpp), and everything here only draws those rects and
// executes the commands they produce. Keyboard, mouse and touch all end in AFCBPlayerController::ExecuteCommand.

#include "FCBGameMode.h"
#include "FantasyCardBattle.h"
#include "FCBDataAssets.h"
#include "FCBGameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/Class.h"
#include "TimerManager.h"

/** ------------------------------------------------------------ presentation tuning ------------------------------- */

namespace FCBHud
{
	/** Truncate text to a pixel budget. The debug HUD has no text metrics, so this is a character estimate. */
	static FString TrimToPixels(const FString& In, float WidthPx, float TextPx)
	{
		const int32 MaxChars = FMath::Max(3, FMath::FloorToInt32(WidthPx / FMath::Max(4.f, TextPx * 0.56f)));
		if (In.Len() <= MaxChars)
		{
			return In;
		}
		return In.Left(FMath::Max(1, MaxChars - 1)) + TEXT("~");
	}

	/** Fixed-width truncation, for the log lines which have their own column. */
	static FString TrimToWidth(const FString& In, int32 InMax)
	{
		if (In.Len() <= InMax)
		{
			return In;
		}
		return In.Left(FMath::Max(1, InMax - 1)) + TEXT("...");
	}

	/**
	 * Viewport pixels per device-independent pixel.
	 *
	 * A phone's short side is 360-420 dp, so the short side over 380 is the density. It is capped at 3 so a
	 * tablet, whose short side is ~800 dp, does not inflate its own touch targets, and it only applies to the
	 * mobile build: a mouse hits a 48 px row perfectly well, and a desktop window must not turn into a
	 * two-column phone hand because it happens to be small.
	 */
	static float ResolveDensityScale(const FVector2D& Viewport)
	{
#if FCB_MOBILE_BUILD
		return FMath::Clamp(FMath::Min(Viewport.X, Viewport.Y) / 380.f, 1.f, 3.f);
#else
		return 1.f;
#endif
	}

	/** The layout tunables, from project settings (Docs/Mobile.md). */
	static FCBTouch::FTuning MakeTuning(const FVector2D& Viewport)
	{
		const UFCBSettings& Settings = UFCBSettings::Get();
		FCBTouch::FTuning Tuning;
		Tuning.MinTarget = Settings.TouchTargetOverridePx > 0.f
			? Settings.TouchTargetOverridePx
			: Settings.TouchTargetDp * ResolveDensityScale(Viewport);
		return Tuning;
	}

	/** Safe-area insets: the settings win, otherwise the notch/gesture-bar fallback in FCBTouch. */
	static FCBTouch::FSafeArea MakeSafeArea(bool bTouchDevice)
	{
		const UFCBSettings& Settings = UFCBSettings::Get();
		const FCBTouch::FSafeArea Fallback = FCBTouch::FallbackSafeArea(bTouchDevice);
		FCBTouch::FSafeArea Area;
		Area.Left = Settings.TouchSafeAreaLeftPercent >= 0.f ? Settings.TouchSafeAreaLeftPercent * 0.01f : Fallback.Left;
		Area.Top = Settings.TouchSafeAreaTopPercent >= 0.f ? Settings.TouchSafeAreaTopPercent * 0.01f : Fallback.Top;
		Area.Right = Settings.TouchSafeAreaRightPercent >= 0.f ? Settings.TouchSafeAreaRightPercent * 0.01f : Fallback.Right;
		Area.Bottom = Settings.TouchSafeAreaBottomPercent >= 0.f ? Settings.TouchSafeAreaBottomPercent * 0.01f : Fallback.Bottom;
		return Area;
	}

	static FCBTouch::FGestureConfig MakeGestureConfig()
	{
		const UFCBSettings& Settings = UFCBSettings::Get();
		FCBTouch::FGestureConfig Config;
		Config.LongPressSeconds = Settings.TouchLongPressSeconds;
		Config.TapSlop = Settings.TouchTapSlopPx;
		return Config;
	}

	/** Touch-sized rows get short labels ("NEW", "PWR"); a mouse-sized layout has room for words. */
	static bool IsTouchSized(const FCBTouch::FTableLayout& Layout)
	{
		return Layout.Grid.CardHeight > 64.f;
	}

	/** "1.2M yr" / "12k yr" / "1,219": a value cell is about 120 px wide on a phone. */
	static FString CompactValue(EFCBAttribute Attribute, int32 Value)
	{
		switch (Attribute)
		{
		case EFCBAttribute::Age:
			if (Value >= 1000000) { return FString::Printf(TEXT("%d.%dM"), Value / 1000000, (Value / 100000) % 10); }
			if (Value >= 10000) { return FString::Printf(TEXT("%dk"), Value / 1000); }
			return FCBAttributeUtil::WithThousandsSeparator(Value);
		case EFCBAttribute::Height:
			return FString::Printf(TEXT("%d.%02d m"), Value / 100, Value % 100);
		default:
			return FString::FromInt(Value);
		}
	}

	static int32 AttributeValue(EFCBAttribute Attribute, const FFCBCardView& Card)
	{
		switch (Attribute)
		{
		case EFCBAttribute::Age:	return Card.AgeYears;
		case EFCBAttribute::Power:	return Card.Power;
		case EFCBAttribute::Speed:	return Card.Speed;
		case EFCBAttribute::Height:	return Card.HeightCm;
		default:					return 0;
		}
	}

	/** Three letters in a value cell, the full word in the detail panel. */
	static const TCHAR* AttributeLabel(EFCBAttribute Attribute)
	{
		switch (Attribute)
		{
		case EFCBAttribute::Age:	return TEXT("AGE");
		case EFCBAttribute::Power:	return TEXT("PWR");
		case EFCBAttribute::Speed:	return TEXT("SPD");
		case EFCBAttribute::Height:	return TEXT("HGT");
		default:					return TEXT("?");
		}
	}

	static const TCHAR* AttributeLongLabel(EFCBAttribute Attribute)
	{
		switch (Attribute)
		{
		case EFCBAttribute::Age:	return TEXT("AGE");
		case EFCBAttribute::Power:	return TEXT("POWER");
		case EFCBAttribute::Speed:	return TEXT("SPEED");
		case EFCBAttribute::Height:	return TEXT("HEIGHT");
		default:					return TEXT("?");
		}
	}

	static const FLinearColor Ink(0.86f, 0.88f, 0.92f, 1.f);
	static const FLinearColor InkDim(0.58f, 0.61f, 0.67f, 1.f);
	static const FLinearColor InkFaint(0.38f, 0.40f, 0.46f, 1.f);
	static const FLinearColor Accent(0.98f, 0.80f, 0.35f, 1.f);
	static const FLinearColor Good(0.55f, 0.90f, 0.55f, 1.f);
	static const FLinearColor Bad(0.95f, 0.55f, 0.50f, 1.f);
	static const FLinearColor Panel(0.06f, 0.06f, 0.08f, 0.78f);
	static const FLinearColor PanelStrong(0.10f, 0.10f, 0.13f, 0.94f);
}

/** ---------------------------------------------------------------- game mode ---------------------------------- */

AFCBGameMode::AFCBGameMode()
{
	// Set here rather than in project settings so an empty project still plays: no asset to create, no
	// setting to lose in a merge.
	PlayerControllerClass = AFCBPlayerController::StaticClass();
	HUDClass = AFCBDebugHud::StaticClass();

	// DefaultPawnClass stays as AGameModeBase's default: with no pawn the engine logs a possession warning on
	// every connect, and an empty world costs nothing to render.
	bDelayedStart = false;
}

UFCBGameInstance* AFCBGameMode::GetFcbInstance() const
{
	// Cast rather than GetGameInstance<UFCBGameInstance>(): AActor's accessor is not templated, and a null
	// world or a stale instance is a legitimate state during shutdown.
	return GetWorld() ? Cast<UFCBGameInstance>(GetWorld()->GetGameInstance()) : nullptr;
}

void AFCBGameMode::StartPlay()
{
	Super::StartPlay();

	if (UFCBGameInstance* Instance = GetFcbInstance())
	{
		Instance->OnMatchFinished.AddDynamic(this, &AFCBGameMode::HandleMatchFinished);
		if (!Instance->IsMatchStarted())
		{
			Instance->StartNewMatch();
		}
	}
}

void AFCBGameMode::HandleMatchFinished(int32 WinnerSeat, const FString& Reason)
{
	UFCBGameInstance* Instance = GetFcbInstance();
	const FString Winner = WinnerSeat == INDEX_NONE
		? TEXT("Nobody - the deck ran out evenly")
		: (Instance && Instance->GetMatch().GetState().Seats.IsValidIndex(WinnerSeat)
			? Instance->GetMatch().GetState().Seats[WinnerSeat].Name : TEXT("Winner"));

	LastResultText = FString::Printf(TEXT("%s | %s"), *Winner, *Reason);
	UE_LOG(LogFCB, Log, TEXT("Result: %s"), *LastResultText);

	if (AutoRestartSeconds > 0.f && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(RestartTimer,
			FTimerDelegate::CreateUObject(this, &AFCBGameMode::RequestNewMatch), AutoRestartSeconds, false);
	}
}

void AFCBGameMode::RequestNewMatch()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(RestartTimer);
	}
	LastResultText.Reset();

	if (UFCBGameInstance* Instance = GetFcbInstance())
	{
		Instance->StartNewMatch();
	}
}

/** ------------------------------------------------------------ player controller ------------------------------- */

UFCBGameInstance* AFCBPlayerController::GetFcbInstance() const
{
	return GetWorld() ? Cast<UFCBGameInstance>(GetWorld()->GetGameInstance()) : nullptr;
}

void AFCBPlayerController::BeginPlay()
{
	Super::BeginPlay();

	FcbGameMode = Cast<AFCBGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr);

#if FCB_MOBILE_BUILD
	// A phone has no cursor and no hover, and leaving these on makes Slate run a mouse hit test for every
	// touch and can leave a stray pointer drawn on some Android builds.
	bShowMouseCursor = false;
	bEnableMouseOverEvents = false;
	bEnableClickEvents = false;
#else
	// The debug HUD is mouse-friendly enough to want a cursor; UMG work keeps this.
	bShowMouseCursor = true;
	bEnableMouseOverEvents = true;
	bEnableClickEvents = true;
#endif
}

void AFCBPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UInputComponent* Input = InputComponent;
	if (!Input)
	{
		UE_LOG(LogFCB, Warning, TEXT("No input component: the FCB_* bindings in DefaultInput.ini are inactive"));
		return;
	}

	// Names must match Config/DefaultInput.ini. They are hard-coded on purpose: a typo in a legacy binding is
	// silent at runtime, and this way grep finds both sides.
	Input->BindAction(TEXT("FCB_Attr_Age"), IE_Pressed, this, &AFCBPlayerController::DeclareAge);
	Input->BindAction(TEXT("FCB_Attr_Power"), IE_Pressed, this, &AFCBPlayerController::DeclarePower);
	Input->BindAction(TEXT("FCB_Attr_Speed"), IE_Pressed, this, &AFCBPlayerController::DeclareSpeed);
	Input->BindAction(TEXT("FCB_Attr_Height"), IE_Pressed, this, &AFCBPlayerController::DeclareHeight);

	Input->BindAction(TEXT("FCB_Card_1"), IE_Pressed, this, &AFCBPlayerController::SelectCard1);
	Input->BindAction(TEXT("FCB_Card_2"), IE_Pressed, this, &AFCBPlayerController::SelectCard2);
	Input->BindAction(TEXT("FCB_Card_3"), IE_Pressed, this, &AFCBPlayerController::SelectCard3);
	Input->BindAction(TEXT("FCB_Card_4"), IE_Pressed, this, &AFCBPlayerController::SelectCard4);
	Input->BindAction(TEXT("FCB_Card_5"), IE_Pressed, this, &AFCBPlayerController::SelectCard5);
	Input->BindAction(TEXT("FCB_Card_6"), IE_Pressed, this, &AFCBPlayerController::SelectCard6);
	Input->BindAction(TEXT("FCB_Card_7"), IE_Pressed, this, &AFCBPlayerController::SelectCard7);
	Input->BindAction(TEXT("FCB_Card_8"), IE_Pressed, this, &AFCBPlayerController::SelectCard8);
	Input->BindAction(TEXT("FCB_Card_9"), IE_Pressed, this, &AFCBPlayerController::SelectCard9);
	Input->BindAction(TEXT("FCB_Card_10"), IE_Pressed, this, &AFCBPlayerController::SelectCard10);
	Input->BindAction(TEXT("FCB_Card_11"), IE_Pressed, this, &AFCBPlayerController::SelectCard11);
	Input->BindAction(TEXT("FCB_Card_12"), IE_Pressed, this, &AFCBPlayerController::SelectCard12);

	Input->BindAction(TEXT("FCB_Card_Prev"), IE_Pressed, this, &AFCBPlayerController::SelectPreviousCard);
	Input->BindAction(TEXT("FCB_Card_Next"), IE_Pressed, this, &AFCBPlayerController::SelectNextCard);
	Input->BindAction(TEXT("FCB_Page_Prev"), IE_Pressed, this, &AFCBPlayerController::SelectPreviousPage);
	Input->BindAction(TEXT("FCB_Page_Next"), IE_Pressed, this, &AFCBPlayerController::SelectNextPage);
	Input->BindAction(TEXT("FCB_Confirm"), IE_Pressed, this, &AFCBPlayerController::Confirm);
	Input->BindAction(TEXT("FCB_Click"), IE_Pressed, this, &AFCBPlayerController::OnClick);
	Input->BindAction(TEXT("FCB_NewMatch"), IE_Pressed, this, &AFCBPlayerController::NewMatch);
	Input->BindAction(TEXT("FCB_RollSeed"), IE_Pressed, this, &AFCBPlayerController::RollSeed);
	Input->BindAction(TEXT("FCB_ToggleLog"), IE_Pressed, this, &AFCBPlayerController::ToggleLog);
	Input->BindAction(TEXT("FCB_ToggleDebug"), IE_Pressed, this, &AFCBPlayerController::ToggleDebug);

	// Touch: two bindings, not a stream. A gesture is defined by where the finger went down, where it came up
	// and how long it stayed, so there is nothing a per-frame move handler would add.
	Input->BindTouch(IE_Pressed, this, &AFCBPlayerController::TouchPressed);
	Input->BindTouch(IE_Released, this, &AFCBPlayerController::TouchReleased);
}

void AFCBPlayerController::DeclareAge()
{
	if (UFCBGameInstance* Instance = GetFcbInstance()) { Instance->SetSelectedAttribute(EFCBAttribute::Age); }
}

void AFCBPlayerController::DeclarePower()
{
	if (UFCBGameInstance* Instance = GetFcbInstance()) { Instance->SetSelectedAttribute(EFCBAttribute::Power); }
}

void AFCBPlayerController::DeclareSpeed()
{
	if (UFCBGameInstance* Instance = GetFcbInstance()) { Instance->SetSelectedAttribute(EFCBAttribute::Speed); }
}

void AFCBPlayerController::DeclareHeight()
{
	if (UFCBGameInstance* Instance = GetFcbInstance()) { Instance->SetSelectedAttribute(EFCBAttribute::Height); }
}

void AFCBPlayerController::SelectCardAndFollow(int32 Index)
{
	if (UFCBGameInstance* Instance = GetFcbInstance())
	{
		Instance->SetSelectedHandIndex(Index);
		if (AFCBDebugHud* Hud = Cast<AFCBDebugHud>(GetHUD()))
		{
			Hud->FollowSelection(Instance->GetSelectedHandIndex());
		}
	}
}

void AFCBPlayerController::SelectCardByIndex(int32 Index)
{
	SelectCardAndFollow(Index);
}

void AFCBPlayerController::SelectCard1() { SelectCardByIndex(0); }
void AFCBPlayerController::SelectCard2() { SelectCardByIndex(1); }
void AFCBPlayerController::SelectCard3() { SelectCardByIndex(2); }
void AFCBPlayerController::SelectCard4() { SelectCardByIndex(3); }
void AFCBPlayerController::SelectCard5() { SelectCardByIndex(4); }
void AFCBPlayerController::SelectCard6() { SelectCardByIndex(5); }
void AFCBPlayerController::SelectCard7() { SelectCardByIndex(6); }
void AFCBPlayerController::SelectCard8() { SelectCardByIndex(7); }
void AFCBPlayerController::SelectCard9() { SelectCardByIndex(8); }
void AFCBPlayerController::SelectCard10() { SelectCardByIndex(9); }
void AFCBPlayerController::SelectCard11() { SelectCardByIndex(10); }
void AFCBPlayerController::SelectCard12() { SelectCardByIndex(11); }

void AFCBPlayerController::SelectPreviousCard()
{
	if (UFCBGameInstance* Instance = GetFcbInstance())
	{
		Instance->MoveSelection(-1);
		SelectCardAndFollow(Instance->GetSelectedHandIndex());
	}
}

void AFCBPlayerController::SelectNextCard()
{
	if (UFCBGameInstance* Instance = GetFcbInstance())
	{
		Instance->MoveSelection(1);
		SelectCardAndFollow(Instance->GetSelectedHandIndex());
	}
}

void AFCBPlayerController::TurnPage(int32 Delta)
{
	if (AFCBDebugHud* Hud = Cast<AFCBDebugHud>(GetHUD()))
	{
		Hud->SetHandPage(Hud->GetHandPage() + Delta);
	}
}

void AFCBPlayerController::SelectPreviousPage() { TurnPage(-1); }
void AFCBPlayerController::SelectNextPage() { TurnPage(1); }

void AFCBPlayerController::Confirm()
{
	if (UFCBGameInstance* Instance = GetFcbInstance())
	{
		Instance->ConfirmSelection();
	}
}

void AFCBPlayerController::OnClick()
{
	AFCBDebugHud* Hud = Cast<AFCBDebugHud>(GetHUD());
	if (!Hud)
	{
		return;
	}

	float MouseX = 0.f, MouseY = 0.f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		return;
	}

	// A click is a tap with a stationary cursor: same layout, same resolution rules, same command.
	FCBTouch::FTableLayout Layout;
	FCBTouch::FState State;
	if (Hud->BuildTableLayout(Layout) && Hud->BuildTableState(State))
	{
		ExecuteCommand(FCBTouch::ResolveTap(Layout, FVector2D(MouseX, MouseY), State,
			UFCBSettings::Get().bTouchOneTapPlay));
	}
}

void AFCBPlayerController::TouchPressed(ETouchIndex::Type FingerIndex, FVector Location)
{
	// One finger at a time. A second finger landing mid-gesture must not retarget or cancel the first.
	if (TouchDownFinger != INDEX_NONE)
	{
		return;
	}
	TouchDownFinger = static_cast<int32>(FingerIndex);
	TouchDownPosition = FVector2D(Location.X, Location.Y);
	TouchDownTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

void AFCBPlayerController::TouchReleased(ETouchIndex::Type FingerIndex, FVector Location)
{
	if (TouchDownFinger != static_cast<int32>(FingerIndex))
	{
		return;
	}

	const float UpTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const FVector2D UpPosition(Location.X, Location.Y);
	const FVector2D DownPosition = TouchDownPosition;
	const float DownTimeSeconds = TouchDownTimeSeconds;
	TouchDownFinger = INDEX_NONE;

	AFCBDebugHud* Hud = Cast<AFCBDebugHud>(GetHUD());
	FCBTouch::FTableLayout Layout;
	FCBTouch::FState State;
	if (!Hud || !Hud->BuildTableLayout(Layout) || !Hud->BuildTableState(State))
	{
		return;
	}

	const FCBTouch::FCommand Command = FCBTouch::ResolveGesture(Layout, DownPosition, DownTimeSeconds,
		UpPosition, UpTimeSeconds, State, FCBHud::MakeGestureConfig(), UFCBSettings::Get().bTouchOneTapPlay);
	ExecuteCommand(Command);
}

void AFCBPlayerController::ExecuteCommand(const FCBTouch::FCommand& Command)
{
	UFCBGameInstance* Instance = GetFcbInstance();
	AFCBDebugHud* Hud = Cast<AFCBDebugHud>(GetHUD());
	if (!Instance)
	{
		return;
	}

	switch (Command.Action)
	{
	case FCBTouch::EAction::SelectCard:
		SelectCardAndFollow(Command.HandIndex);
		break;

	case FCBTouch::EAction::DeclareAndPlay:
		// One gesture: pick the card, name the attribute, play it. This is the whole point of the phone build,
		// so it stays one command rather than three that a frame can interrupt.
		Instance->SetSelectedHandIndex(Command.HandIndex);
		Instance->SetSelectedAttribute(Command.Attribute);
		Instance->ConfirmSelection();
		break;

	case FCBTouch::EAction::DeclareAttribute:
		Instance->SetSelectedHandIndex(Command.HandIndex);
		Instance->SetSelectedAttribute(Command.Attribute);
		break;

	case FCBTouch::EAction::Confirm:
		Instance->ConfirmSelection();
		break;

	case FCBTouch::EAction::PrevPage:
	case FCBTouch::EAction::NextPage:
		if (Hud)
		{
			Hud->SetHandPage(Command.PageIndex);
		}
		break;

	case FCBTouch::EAction::SelectPrevCard:
		Instance->MoveSelection(-1);
		SelectCardAndFollow(Instance->GetSelectedHandIndex());
		break;

	case FCBTouch::EAction::SelectNextCard:
		Instance->MoveSelection(1);
		SelectCardAndFollow(Instance->GetSelectedHandIndex());
		break;

	case FCBTouch::EAction::PeekCard:
		if (Hud)
		{
			Hud->SetPeekCard(Command.HandIndex);
		}
		break;

	case FCBTouch::EAction::ClosePeek:
		if (Hud)
		{
			Hud->SetPeekCard(INDEX_NONE);
		}
		break;

	case FCBTouch::EAction::NewMatch:
		NewMatch();
		break;

	case FCBTouch::EAction::RollSeed:
		RollSeed();
		break;

	case FCBTouch::EAction::ToggleLog:
		ToggleLog();
		break;

	case FCBTouch::EAction::None:
	default:
		break;
	}

	// Every command says what it meant, including the ones that did nothing. On a phone an ignored tap with no
	// feedback reads as a broken game.
	if (Hud && !Command.Note.IsEmpty())
	{
		Hud->SetInputNote(Command.Note);
	}
}

void AFCBPlayerController::NewMatch()
{
	if (AFCBGameMode* Mode = FcbGameMode.Get())
	{
		Mode->RequestNewMatch();
	}
	else if (UFCBGameInstance* Instance = GetFcbInstance())
	{
		Instance->StartNewMatch();
	}
}

void AFCBPlayerController::RollSeed()
{
	if (UFCBGameInstance* Instance = GetFcbInstance())
	{
		Instance->RollSeedAndRestart();
	}
}

void AFCBPlayerController::ToggleLog()
{
	bLogVisible = !bLogVisible;
}

void AFCBPlayerController::ToggleDebug()
{
	bDebugVisible = !bDebugVisible;
}

/** -------------------------------------------------------------------- hud ---------------------------------- */

UFont* AFCBDebugHud::ResolveFont(bool bBold) const
{
	if (!GEngine)
	{
		return nullptr;
	}
	// There is no bold engine-bundled font to reach for; the size difference is what the debug HUD uses for
	// emphasis, and the real typography arrives with the UMG pass.
	return bBold ? GEngine->GetMediumFont() : GEngine->GetSmallFont();
}

void AFCBDebugHud::FillRect(float X, float Y, float Width, float Height, const FLinearColor& Color)
{
	if (Width <= 0.f || Height <= 0.f)
	{
		return;
	}
	// AHUD::DrawRect is (color, x, y, w, h) and the colour carries its own alpha ("can be translucent"), which
	// is why every colour used here is an FLinearColor with the opacity already folded in.
	DrawRect(Color, X, Y, Width, Height);
}

void AFCBDebugHud::FrameRect(const FCBTouch::FRect& Rect, const FLinearColor& Color, float Thickness)
{
	if (Rect.IsEmpty())
	{
		return;
	}
	FillRect(Rect.Left, Rect.Top, Rect.Width(), Thickness, Color);
	FillRect(Rect.Left, Rect.Bottom - Thickness, Rect.Width(), Thickness, Color);
	FillRect(Rect.Left, Rect.Top, Thickness, Rect.Height(), Color);
	FillRect(Rect.Right - Thickness, Rect.Top, Thickness, Rect.Height(), Color);
}

float AFCBDebugHud::DrawLineAt(const FString& Text, float X, float Y, float PixelHeight, const FLinearColor& Color, bool bBold)
{
	UFont* Font = ResolveFont(bBold);
	if (!Font)
	{
		return Y + PixelHeight * 1.2f;
	}

	// The engine fonts are 12-16 px tall: on a phone every line has to be scaled up, and this division is what
	// makes the requested pixel height the actual pixel height. GetMaxCharHeight is the font's own line height.
	const float Scale = PixelHeight / FMath::Max(1.f, Font->GetMaxCharHeight());
	DrawText(Text, Color, X, Y, Font, Scale, false);
	return Y + PixelHeight * 1.2f;
}

bool AFCBDebugHud::BuildTableState(FCBTouch::FState& OutState) const
{
	const APlayerController* Player = GetOwningPlayerController();
	UFCBGameInstance* Instance = (Player && Player->GetWorld())
		? Cast<UFCBGameInstance>(Player->GetWorld()->GetGameInstance()) : nullptr;
	if (!Instance)
	{
		return false;
	}

	OutState.HandCount = Instance->GetHandCount(FCBSeat::A);
	OutState.SelectedCardIndex = Instance->GetSelectedHandIndex();
	OutState.SelectedAttribute = Instance->GetSelectedAttribute();
	OutState.PageIndex = HandPage;
	OutState.bMatchOver = Instance->IsMatchOver();
	OutState.bPeekActive = PeekCardIndex != INDEX_NONE;
	OutState.bCanPlay = Instance->IsPlayersTurn() && Instance->IsDataUsable() && !Instance->IsMatchOver();
	return true;
}

bool AFCBDebugHud::BuildTableLayout(FCBTouch::FTableLayout& OutLayout) const
{
	const APlayerController* Player = GetOwningPlayerController();
	if (!Player)
	{
		return false;
	}

	FIntPoint ViewportSize;
	Player->GetViewportSize(ViewportSize); // APlayerController's out-param form; there is no value overload
	if (ViewportSize.X <= 0 || ViewportSize.Y <= 0)
	{
		return false;
	}

	FCBTouch::FState State;
	if (!BuildTableState(State))
	{
		return false;
	}

	const FVector2D Viewport(ViewportSize.X, ViewportSize.Y);
#if FCB_MOBILE_BUILD
	// The platform, not the aspect ratio, is what says "there is a finger and not a mouse": a portrait window
	// on a desktop still gets mouse-sized targets.
	const bool bTouchDevice = true;
#else
	const bool bTouchDevice = false;
#endif
	OutLayout = FCBTouch::BuildLayout(Viewport, FCBHud::MakeSafeArea(bTouchDevice), State, FCBHud::MakeTuning(Viewport));
	return true;
}

void AFCBDebugHud::SetHandPage(int32 InPage)
{
	HandPage = FMath::Max(0, InPage);
}

void AFCBDebugHud::SetPeekCard(int32 InHandIndex)
{
	PeekCardIndex = InHandIndex;
	PeekCardId = NAME_None;

	const APlayerController* Player = GetOwningPlayerController();
	UFCBGameInstance* Instance = (Player && Player->GetWorld())
		? Cast<UFCBGameInstance>(Player->GetWorld()->GetGameInstance()) : nullptr;
	if (!Instance || InHandIndex == INDEX_NONE)
	{
		return;
	}

	TArray<FFCBCardView> Hand;
	Instance->GetHandView(FCBSeat::A, Hand);
	if (Hand.IsValidIndex(InHandIndex))
	{
		PeekCardId = Hand[InHandIndex].Id;
	}
}

void AFCBDebugHud::SetInputNote(const FString& InNote)
{
	InputNote = InNote;
	InputNoteTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

float AFCBDebugHud::GetInputNoteAgeSeconds() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() - InputNoteTimeSeconds : 1000.f;
}

void AFCBDebugHud::FollowSelection(int32 InHandIndex)
{
	// The page follows the selection only when the selection moved off it: paging away from the selected card
	// must not immediately yank the player back.
	FCBTouch::FTableLayout Layout;
	if (!BuildTableLayout(Layout))
	{
		return;
	}
	if (Layout.SelectedVisibleIndex == INDEX_NONE && InHandIndex != INDEX_NONE && Layout.Grid.CardsPerPage > 0)
	{
		HandPage = FCBTouch::FHandGrid::PageForCard(Layout.Grid, InHandIndex);
	}
}

void AFCBDebugHud::DrawButton(const FCBTouch::FRect& Rect, const FString& Label, const FLinearColor& Color, bool bEnabled)
{
	if (Rect.IsEmpty())
	{
		return;
	}

	const float Alpha = bEnabled ? 1.f : 0.35f;
	FLinearColor Fill = Color;
	Fill.A = 0.20f * Alpha;
	FillRect(Rect.Left, Rect.Top, Rect.Width(), Rect.Height(), Fill);

	FLinearColor Border = Color;
	Border.A = Alpha;
	FrameRect(Rect, Border, 2.f);

	const float Text = FMath::Clamp(Rect.Height() * 0.34f, 9.f, 26.f);
	DrawLineAt(Label, Rect.Left + 8.f, Rect.Center().Y - Text * 0.62f, Text, Border, true);
}

void AFCBDebugHud::DrawStatCells(const FFCBCardView& Card, const FCBTouch::FRect& Row, const FCBTouch::FTableLayout& Layout,
	int32 VisibleIndex, bool bSelected, EFCBAttribute DeclaredAttribute)
{
	const float LabelSize = FMath::Clamp(Row.Height() * 0.20f, 8.f, 20.f);
	const float ValueSize = FMath::Clamp(Row.Height() * 0.30f, 10.f, 30.f);

	for (int32 Cell = 0; Cell < 4; ++Cell)
	{
		const EFCBAttribute Attribute = FCBTouch::AttributeFromCellIndex(Cell);
		const FCBTouch::FRect CellRect = Layout.StatRect(VisibleIndex, Attribute);
		if (CellRect.IsEmpty())
		{
			continue;
		}

		const bool bDeclared = bSelected && Attribute == DeclaredAttribute;
		if (bDeclared)
		{
			// The declared attribute is the one that will be compared: it has to be findable at a glance.
			FLinearColor Highlight = FCBHud::Accent;
			Highlight.A = 0.18f;
			FillRect(CellRect.Left + 1.f, CellRect.Top + 1.f, CellRect.Width() - 2.f, CellRect.Height() - 2.f, Highlight);
			FrameRect(CellRect, FCBHud::Accent, 2.f);
		}
		else if (Cell > 0)
		{
			// A hairline between cells: the four hit rects touch, and the eye should not have to guess where one
			// ends.
			FLinearColor Divider = FCBHud::InkFaint;
			Divider.A = 0.5f;
			FillRect(CellRect.Left, CellRect.Top + 3.f, 1.f, CellRect.Height() - 6.f, Divider);
		}

		DrawLineAt(FCBHud::AttributeLabel(Attribute), CellRect.Left + 6.f, CellRect.Top + Row.Height() * 0.10f,
			LabelSize, bDeclared ? FCBHud::Accent : FCBHud::InkFaint, false);
		DrawLineAt(FCBHud::CompactValue(Attribute, FCBHud::AttributeValue(Attribute, Card)),
			CellRect.Left + 6.f, CellRect.Top + Row.Height() * 0.42f, ValueSize,
			bDeclared ? FCBHud::Accent : FCBHud::Ink, bDeclared);
	}
}

void AFCBDebugHud::DrawCardRow(const FFCBCardView& Card, const FCBTouch::FRect& Row, const FCBTouch::FTableLayout& Layout,
	int32 VisibleIndex, bool bSelected, bool bFaceDown, EFCBAttribute DeclaredAttribute)
{
	if (Row.IsEmpty())
	{
		return;
	}

	const float Alpha = bFaceDown ? 0.55f : 1.f;
	FLinearColor Frame = Card.FrameColor;
	Frame.A = Alpha;

	// Fill, then the faction bar: cheaper than a widget brush and it reads fine at any scale.
	FLinearColor Fill = bSelected ? FCBHud::PanelStrong : FCBHud::Panel;
	Fill.A *= Alpha;
	FillRect(Row.Left, Row.Top, Row.Width(), Row.Height(), Fill);
	FillRect(Row.Left, Row.Top, bSelected ? 6.f : 4.f, Row.Height(), Frame);

	// The name strip is a real part of the layout (tapping it selects without playing), so its width comes from
	// the layout rather than from a guess.
	const float NameWidth = FMath::Max(40.f, Layout.NameWidth > 0.f && VisibleIndex != INDEX_NONE ? Layout.NameWidth : Row.Width());
	const float NameSize = FMath::Clamp(Row.Height() * 0.26f, 9.f, 24.f);
	const FString Name = bFaceDown
		? TEXT("face down")
		: FCBHud::TrimToPixels(Card.DisplayName, NameWidth - 16.f, NameSize);
	DrawLineAt(Name, Row.Left + 12.f, Row.Top + Row.Height() * 0.14f, NameSize,
		bSelected ? FLinearColor::White : FCBHud::Ink, bSelected);

	if (!bFaceDown)
	{
		// Rarity, and the "+n" stacks or "spent" marker, on the strip's second line so the value cells stay clean.
		FString Second = Card.RarityName;
		if (Card.PowerStacks > 0)
		{
			Second += FString::Printf(TEXT("  +%d"), Card.PowerStacks);
		}
		else if (Card.RemainingCharges < 1)
		{
			Second += TEXT("  spent");
		}
		DrawLineAt(FCBHud::TrimToPixels(Second, NameWidth - 16.f, NameSize * 0.85f),
			Row.Left + 12.f, Row.Top + Row.Height() * 0.62f, NameSize * 0.85f, FCBHud::InkDim);

		if (VisibleIndex != INDEX_NONE)
		{
			DrawStatCells(Card, Row, Layout, VisibleIndex, bSelected, DeclaredAttribute);
		}
	}

	if (bSelected)
	{
		FrameRect(Row, FLinearColor::White, 2.f);
	}
}

void AFCBDebugHud::DrawStatusBand(const UFCBGameInstance& Instance, const FCBTouch::FTableLayout& Layout, float StatusSize)
{
	const FFCBCardView OpponentTop = Instance.GetOpponentTopCard();
	DrawCardRow(OpponentTop, Layout.OpponentCard, Layout, INDEX_NONE, false, true, EFCBAttribute::Max);

	// The status lines stop before the buttons: text under a button is a control the player cannot see.
	const float StatusX = Layout.OpponentCard.Right + 12.f;
	const float StatusWidth = FMath::Max(80.f, Layout.NewMatchButton.Left - StatusX - 8.f);
	float StatusY = Layout.StatusBand.Top + 6.f;
	StatusY = DrawLineAt(FCBHud::TrimToPixels(FString::Printf(TEXT("Round %d   pot %d   deck %d   burned %d"),
		Instance.GetRoundNumber(), Instance.GetPotCount(), Instance.GetDeckCount(), Instance.GetBurnedCount()),
		StatusWidth, StatusSize), StatusX, StatusY, StatusSize, FCBHud::Ink);
	DrawLineAt(FCBHud::TrimToPixels(FString::Printf(TEXT("%s: %d cards, score %d      your score %d"),
		*Instance.GetSeatName(FCBSeat::B), Instance.GetHandCount(FCBSeat::B), Instance.GetScore(FCBSeat::B),
		Instance.GetScore(FCBSeat::A)), StatusWidth, StatusSize),
		StatusX, StatusY, StatusSize, FCBHud::InkDim);

	if (!Instance.IsDataUsable())
	{
		DrawLineAt(FCBHud::TrimToPixels(Instance.GetDataNote(), Layout.StatusBand.Width() * 0.5f, StatusSize),
			StatusX, StatusY + StatusSize * 1.3f, StatusSize, FCBHud::Bad, true);
	}
}

void AFCBDebugHud::DrawHand(const UFCBGameInstance& Instance, const TArray<FFCBCardView>& Hand, const FCBTouch::FTableLayout& Layout)
{
	const int32 SelectedIndex = Instance.GetSelectedHandIndex();
	const EFCBAttribute Declared = Instance.GetSelectedAttribute();

	for (int32 VisibleIndex = 0; VisibleIndex < Layout.VisibleCards; ++VisibleIndex)
	{
		const int32 HandIndex = Layout.HandIndexOf(VisibleIndex);
		if (!Hand.IsValidIndex(HandIndex))
		{
			continue;
		}
		DrawCardRow(Hand[HandIndex], Layout.CardRect(VisibleIndex), Layout, VisibleIndex,
			HandIndex == SelectedIndex, false, Declared);
	}
}

void AFCBDebugHud::DrawDetailPanel(const UFCBGameInstance& Instance, const FFCBCardView& Card, const FCBTouch::FTableLayout& Layout, EFCBAttribute Declared)
{
	const FCBTouch::FRect Panel = Layout.DetailPanel;
	if (Panel.IsEmpty())
	{
		return;
	}

	FillRect(Panel.Left, Panel.Top, Panel.Width(), Panel.Height(), FCBHud::Panel);
	FrameRect(Panel, FCBHud::InkFaint, 1.f);

	const float TitleSize = FMath::Clamp(Panel.Height() * 0.055f, 12.f, 30.f);
	const float X = Panel.Left + 10.f;
	const float TextWidth = Panel.Width() - 20.f;
	float Y = Panel.Top + 6.f;

	if (Card.DisplayName.IsEmpty())
	{
		DrawLineAt(TEXT("no card selected"), X, Y, TitleSize, FCBHud::InkDim);
		return;
	}

	Y = DrawLineAt(FCBHud::TrimToPixels(Card.DisplayName, TextWidth, TitleSize * 1.15f), X, Y, TitleSize * 1.15f,
		FCBHud::Accent, true);
	Y = DrawLineAt(FString::Printf(TEXT("%s  -  %s"), *Card.FactionName, *Card.RarityName), X, Y, TitleSize * 0.85f,
		FCBHud::InkDim);

	// Two stats per line: on a phone the panel is a few hundred pixels wide and four would collide.
	const float StatSize = TitleSize * 0.95f;
	Y = DrawLineAt(FString::Printf(TEXT("Age %s    Power %s"), *Card.AgeText, *Card.PowerText), X, Y, StatSize, FCBHud::Ink);
	Y = DrawLineAt(FString::Printf(TEXT("Speed %s    Height %s"), *Card.SpeedText, *Card.HeightText), X, Y, StatSize, FCBHud::Ink);

	if (!Card.Ability0Name.IsEmpty())
	{
		Y = DrawLineAt(FCBHud::TrimToPixels(FString::Printf(TEXT("%s: %s"), *Card.Ability0Name, *Card.Ability0Text), TextWidth, StatSize * 0.8f),
			X, Y, StatSize * 0.8f, FCBHud::Good);
	}
	if (!Card.Ability1Name.IsEmpty())
	{
		Y = DrawLineAt(FCBHud::TrimToPixels(FString::Printf(TEXT("%s: %s"), *Card.Ability1Name, *Card.Ability1Text), TextWidth, StatSize * 0.8f),
			X, Y, StatSize * 0.8f, FCBHud::Good);
	}
	Y = DrawLineAt(FString::Printf(TEXT("strength %.0f%% of the pool"), Card.StrengthPercentile * 100.f), X, Y,
		StatSize * 0.8f, FCBHud::InkFaint);

	// The preview: what confirming right now would do, from the same call the keyboard path uses.
	FFCBRoundView Preview;
	if (Instance.ProjectSelectedMove(Preview))
	{
		const FLinearColor Color = Preview.AttackerValue > Preview.DefenderValue ? FCBHud::Good
			: (Preview.AttackerValue < Preview.DefenderValue ? FCBHud::Bad : FCBHud::Accent);
		Y = DrawLineAt(FCBHud::TrimToPixels(FString::Printf(TEXT("declare %s:  %d vs %d  ->  %s"),
			*Preview.AttributeName, Preview.AttackerValue, Preview.DefenderValue, *Preview.OutcomeText), TextWidth, StatSize),
			X, Y, StatSize, Color, true);
		for (int32 NoteIndex = 0; NoteIndex < Preview.ModifierNotes.Num() && NoteIndex < 2; ++NoteIndex)
		{
			Y = DrawLineAt(FCBHud::TrimToPixels(Preview.ModifierNotes[NoteIndex], TextWidth, StatSize * 0.75f),
				X, Y, StatSize * 0.75f, FCBHud::InkDim);
		}
	}

	// Four chips that declare an attribute without playing, then PLAY along the bottom edge where a thumb is.
	for (int32 Chip = 0; Chip < 4; ++Chip)
	{
		const EFCBAttribute Attribute = FCBTouch::AttributeFromCellIndex(Chip);
		const bool bDeclared = Attribute == Declared;
		DrawButton(Layout.DetailAttrChips[Chip], FCBHud::AttributeLongLabel(Attribute),
			bDeclared ? FCBHud::Accent : FCBHud::InkDim, true);
	}

	// The layout carries the same flag the input resolution used: the button and the rule cannot disagree.
	const bool bCanPlay = Layout.bCanPlay;
	DrawButton(Layout.PlayButton,
		bCanPlay ? FString::Printf(TEXT("PLAY %s"), FCBHud::AttributeLongLabel(Declared)) : TEXT("WAITING..."),
		bCanPlay ? FCBHud::Good : FCBHud::InkFaint, bCanPlay);
}

void AFCBDebugHud::DrawPeekOverlay(const FFCBCardView& CardView, const FCBTouch::FTableLayout& Layout)
{
	const FCBTouch::FRect Safe = Layout.SafeRect;
	FillRect(Safe.Left, Safe.Top, Safe.Width(), Safe.Height(), FLinearColor(0.02f, 0.02f, 0.03f, 0.93f));

	const float Width = Safe.Width() * 0.74f;
	const float Height = Safe.Height() * 0.74f;
	const float CenterX = Layout.Viewport.X * 0.5f;
	const float CenterY = Layout.Viewport.Y * 0.5f;
	const FCBTouch::FRect Card(CenterX - Width * 0.5f, CenterY - Height * 0.5f, CenterX + Width * 0.5f, CenterY + Height * 0.5f);

	FillRect(Card.Left, Card.Top, Card.Width(), Card.Height(), FCBHud::PanelStrong);
	FLinearColor Frame = CardView.FrameColor;
	Frame.A = 1.f;
	FrameRect(Card, Frame, 4.f);

	const float Title = FMath::Clamp(Card.Height() * 0.11f, 14.f, 36.f);
	const float Body = Title * 0.78f;
	const float X = Card.Left + 20.f;
	const float TextWidth = Card.Width() - 40.f;
	float Y = Card.Top + 16.f;

	Y = DrawLineAt(FCBHud::TrimToPixels(CardView.DisplayName, TextWidth, Title), X, Y, Title, FCBHud::Accent, true);
	Y = DrawLineAt(FString::Printf(TEXT("%s  -  %s"), *CardView.FactionName, *CardView.RarityName), X, Y, Body, FCBHud::InkDim);
	Y += Body * 0.4f;
	Y = DrawLineAt(FString::Printf(TEXT("Age %s"), *CardView.AgeText), X, Y, Body, FCBHud::Ink);
	Y = DrawLineAt(FString::Printf(TEXT("Power %s"), *CardView.PowerText), X, Y, Body, FCBHud::Ink);
	Y = DrawLineAt(FString::Printf(TEXT("Speed %s"), *CardView.SpeedText), X, Y, Body, FCBHud::Ink);
	Y = DrawLineAt(FString::Printf(TEXT("Height %s"), *CardView.HeightText), X, Y, Body, FCBHud::Ink);
	Y += Body * 0.4f;

	if (!CardView.Ability0Name.IsEmpty())
	{
		Y = DrawLineAt(FCBHud::TrimToPixels(FString::Printf(TEXT("%s: %s"), *CardView.Ability0Name, *CardView.Ability0Text), TextWidth, Body * 0.9f),
			X, Y, Body * 0.9f, FCBHud::Good);
	}
	if (!CardView.Ability1Name.IsEmpty())
	{
		Y = DrawLineAt(FCBHud::TrimToPixels(FString::Printf(TEXT("%s: %s"), *CardView.Ability1Name, *CardView.Ability1Text), TextWidth, Body * 0.9f),
			X, Y, Body * 0.9f, FCBHud::Good);
	}
	if (CardView.PowerStacks > 0 || CardView.RemainingCharges < 1)
	{
		Y = DrawLineAt(FString::Printf(TEXT("on the table: +%d power, %d charge(s) left"), CardView.PowerStacks, CardView.RemainingCharges),
			X, Y, Body * 0.9f, FCBHud::Accent);
	}
	Y = DrawLineAt(FString::Printf(TEXT("strength %.0f%% of the pool"), CardView.StrengthPercentile * 100.f), X, Y, Body * 0.9f, FCBHud::InkFaint);

	DrawLineAt(TEXT("tap anywhere to close"), X, Card.Bottom - Body * 1.6f, Body, FCBHud::InkDim);
}

void AFCBDebugHud::DrawEndScreen(const UFCBGameInstance& Instance, const FCBTouch::FTableLayout& Layout)
{
	const FCBTouch::FRect Safe = Layout.SafeRect;
	FillRect(Safe.Left, Safe.Top, Safe.Width(), Safe.Height(), FLinearColor(0.02f, 0.02f, 0.03f, 0.78f));

	const float Title = FMath::Clamp(Layout.Viewport.Y * 0.042f, 14.f, 40.f);
	const float X = Safe.Left + 24.f;
	float Y = Safe.Top + Safe.Height() * 0.12f;

	Y = DrawLineAt(TEXT("MATCH OVER"), X, Y, Title * 1.3f, FLinearColor::White, true);

	if (const APlayerController* Player = GetOwningPlayerController())
	{
		if (const AFCBGameMode* Mode = Player->GetWorld() ? Player->GetWorld()->GetAuthGameMode<AFCBGameMode>() : nullptr)
		{
			Y = DrawLineAt(FCBHud::TrimToPixels(Mode->GetLastResultText(), Safe.Width() - 48.f, Title), X, Y, Title,
				FCBHud::Accent, true);
		}
	}

	TArray<FString> SummaryLines;
	Instance.FormatSummary().ParseIntoArrayLines(SummaryLines, false);
	for (int32 Index = 0; Index < SummaryLines.Num() && Index < 4; ++Index)
	{
		Y = DrawLineAt(FCBHud::TrimToPixels(SummaryLines[Index], Safe.Width() - 48.f, Title * 0.8f), X, Y, Title * 0.8f,
			FCBHud::Ink);
	}

	DrawButton(Layout.EndRestartButton,
		FCBHud::IsTouchSized(Layout) ? TEXT("TAP TO PLAY AGAIN") : TEXT("PLAY AGAIN  (N)"), FCBHud::Good, true);
}

void AFCBDebugHud::DrawDebugOverlay(const UFCBGameInstance& Instance, const FCBTouch::FTableLayout& Layout, float StatusSize)
{
	const FLinearColor DebugInk(0.40f, 0.85f, 0.95f, 1.f);
	const float Size = FMath::Max(8.f, StatusSize * 0.9f);
	float Y = Layout.HandRegion.Bottom - Size * 5.f;
	const float X = Layout.HandRegion.Left + 6.f;
	const float Width = Layout.HandRegion.Width() - 12.f;

	Y = DrawLineAt(FCBHud::TrimToPixels(FString::Printf(TEXT("layout: %s"), *Layout.Note), Width, Size), X, Y, Size, DebugInk);
	Y = DrawLineAt(FString::Printf(TEXT("safe %s   target %.0fpx   page %d/%d"),
		*Layout.Safe.ToString(), FCBHud::MakeTuning(Layout.Viewport).MinTarget, Layout.PageIndex + 1, Layout.Grid.PageCount),
		X, Y, Size, DebugInk);
	Y = DrawLineAt(FString::Printf(TEXT("seed %u   rng %u   ai %s"),
		Instance.GetMatch().GetConfig().RngSeed, Instance.GetMatch().PeekRngState(),
		*StaticEnum<EFCBAiDifficulty>()->GetNameStringByValue(static_cast<int32>(Instance.GetAiDifficulty()))),
		X, Y, Size, DebugInk);
	Y = DrawLineAt(FCBHud::TrimToPixels(Instance.GetLastAiExplanation(), Width, Size), X, Y, Size, DebugInk);
}

void AFCBDebugHud::DrawHUD()
{
	Super::DrawHUD();

	const APlayerController* Player = GetOwningPlayerController();
	const AFCBPlayerController* FcbPlayer = Player ? Cast<AFCBPlayerController>(const_cast<APlayerController*>(Player)) : nullptr;
	UFCBGameInstance* Instance = (Player && Player->GetWorld())
		? Cast<UFCBGameInstance>(Player->GetWorld()->GetGameInstance()) : nullptr;
	if (!Instance)
	{
		return;
	}

	if (!UFCBSettings::Get().bShowDebugHud)
	{
		// UMG is in charge; the debug layer draws nothing at all so both can never fight over the screen.
		return;
	}

	// The page follows the selection when it changes under the player's feet (a card left the hand, the keyboard
	// moved the cursor), but never while they are paging by hand.
	const int32 SelectedIndex = Instance->GetSelectedHandIndex();
	if (SelectedIndex != LastSelectedIndex)
	{
		FollowSelection(SelectedIndex);
		LastSelectedIndex = SelectedIndex;
	}

	FCBTouch::FTableLayout Layout;
	if (!BuildTableLayout(Layout))
	{
		return;
	}

	TArray<FFCBCardView> Hand;
	Instance->GetHandView(FCBSeat::A, Hand);

	const bool bTouchSized = FCBHud::IsTouchSized(Layout);
	const float StatusSize = FMath::Clamp(Layout.Viewport.Y * 0.021f, 10.f, 26.f);

	// --- status band, hand, detail --------------------------------------------------------------------------
	DrawStatusBand(*Instance, Layout, StatusSize);

	DrawButton(Layout.NewMatchButton, bTouchSized ? TEXT("NEW") : TEXT("NEW (N)"), FCBHud::InkDim, true);
	DrawButton(Layout.RollSeedButton, bTouchSized ? TEXT("DEAL") : TEXT("DEAL (R)"), FCBHud::InkDim, true);
	DrawButton(Layout.LogButton, bTouchSized ? TEXT("LOG") : TEXT("LOG (L)"),
		(FcbPlayer && FcbPlayer->IsLogVisible()) ? FCBHud::Accent : FCBHud::InkFaint, true);

	if (Layout.Grid.bPaged && Layout.Grid.PageCount > 1)
	{
		const float PageText = FMath::Clamp(Layout.PagePrevButton.Height() * 0.32f, 9.f, 22.f);
		DrawButton(Layout.PagePrevButton, TEXT("<"), FCBHud::InkDim, true);
		DrawButton(Layout.PageNextButton, TEXT(">"), FCBHud::InkDim, true);
		DrawLineAt(FString::Printf(TEXT("hand %d/%d"), Layout.PageIndex + 1, Layout.Grid.PageCount),
			Layout.PageNextButton.Right + 8.f, Layout.PageNextButton.Center().Y - PageText * 0.6f, PageText, FCBHud::InkDim);
	}

	DrawHand(*Instance, Hand, Layout);
	DrawDetailPanel(*Instance, Instance->GetSelectedCard(), Layout, Instance->GetSelectedAttribute());

	if (FcbPlayer && FcbPlayer->IsLogVisible() && !Layout.LogPanel.IsEmpty())
	{
		const float LogSize = FMath::Clamp(Layout.Viewport.Y * 0.0165f, 9.f, 20.f);
		const int32 MaxLines = FMath::Max(1, FMath::FloorToInt32(Layout.LogPanel.Height() / (LogSize * 1.2f)));
		const FString Log = Instance->FormatLog(FMath::Max(0, Instance->GetRoundNumber() - MaxLines));
		TArray<FString> Lines;
		Log.ParseIntoArrayLines(Lines, false);
		float LogY = Layout.LogPanel.Top;
		for (int32 Index = FMath::Max(0, Lines.Num() - MaxLines); Index < Lines.Num(); ++Index)
		{
			LogY = DrawLineAt(FCBHud::TrimToPixels(Lines[Index], Layout.LogPanel.Width() - 8.f, LogSize),
				Layout.LogPanel.Left, LogY, LogSize, FCBHud::InkDim);
		}
	}

	// --- input feedback: what the last tap meant, including "nothing, because it is not your turn" ----------
	if (!GetInputNote().IsEmpty() && GetInputNoteAgeSeconds() < 2.5f)
	{
		const float NoteSize = FMath::Clamp(Layout.Viewport.Y * 0.018f, 9.f, 22.f);
		DrawLineAt(FCBHud::TrimToPixels(GetInputNote(), Layout.SafeRect.Width() - 24.f, NoteSize),
			Layout.SafeRect.Left + 12.f, Layout.SafeRect.Bottom - NoteSize * 1.6f, NoteSize, FCBHud::Accent);
	}

	// --- overlays -------------------------------------------------------------------------------------------
	if (Instance->IsMatchOver())
	{
		DrawEndScreen(*Instance, Layout);
	}
	else if (PeekCardIndex != INDEX_NONE)
	{
		// Follow the card, not the slot: a capture or an ability can move the hand under the overlay.
		int32 PeekIndex = PeekCardIndex;
		for (int32 Index = 0; Index < Hand.Num(); ++Index)
		{
			if (Hand[Index].Id == PeekCardId)
			{
				PeekIndex = Index;
				break;
			}
		}

		if (Hand.IsValidIndex(PeekIndex) && Hand[PeekIndex].Id == PeekCardId)
		{
			DrawPeekOverlay(Hand[PeekIndex], Layout);
		}
		else
		{
			// The card left the hand while the overlay was open: close it rather than showing the wrong card.
			PeekCardIndex = INDEX_NONE;
			PeekCardId = NAME_None;
		}
	}

	if (FcbPlayer && FcbPlayer->IsDebugVisible())
	{
		DrawDebugOverlay(*Instance, Layout, StatusSize);
	}
}
