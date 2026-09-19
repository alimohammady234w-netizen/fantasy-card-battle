// Copyright (c) Fantasy Card Battle. All rights reserved.

#include "FCBGameMode.h"
#include "FantasyCardBattle.h"
#include "FCBDataAssets.h"
#include "FCBGameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"

/** ------------------------------------------------------------------ layout ----------------------------------- */

namespace FCBHud
{
	/**
	 * A single place that decides where things are, so the click handler and the drawing code can never
	 * disagree. The bands are fractions of the viewport, which is what makes the same layout usable on a
	 * 1080p desktop and a landscape phone.
	 */
	struct FLayout
	{
		float Margin = 16.f;
		float CardHeight = 34.f;
		float CardWidth = 520.f;
		int32 VisibleCards = 12;

		FVector2D Viewport = FVector2D::ZeroVector;

		float PlayerTop() const { return Viewport.Y - Margin - CardHeight * static_cast<float>(VisibleCards); }
		float DetailLeft() const { return Margin + CardWidth + 24.f; }
		float OpponentTop() const { return Margin; }
		float TableBandTop() const { return Margin + CardHeight * 1.6f; }
	};

	static FLayout ComputeLayout(const FVector2D& InViewport, int32 InHandCount)
	{
		FLayout L;
		L.Viewport = InViewport;
		L.Margin = FMath::Max(8.f, InViewport.Y * 0.016f);
		L.CardHeight = FMath::Clamp(InViewport.Y * 0.052f, 20.f, 44.f);
		L.CardWidth = FMath::Clamp(InViewport.X * 0.34f, 320.f, 660.f);
		L.VisibleCards = FMath::Clamp(InHandCount, 1, 12);
		return L;
	}

	static FString TrimToWidth(const FString& In, int32 InMax)
	{
		if (In.Len() <= InMax)
		{
			return In;
		}
		return In.Left(InMax - 1) + TEXT("...");
	}
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

	// The debug HUD is mouse-friendly enough to want a cursor; UMG work keeps this.
	bShowMouseCursor = true;
	bEnableMouseOverEvents = true;
	bEnableClickEvents = true;
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
	Input->BindAction(TEXT("FCB_Confirm"), IE_Pressed, this, &AFCBPlayerController::Confirm);
	Input->BindAction(TEXT("FCB_Click"), IE_Pressed, this, &AFCBPlayerController::OnClick);
	Input->BindAction(TEXT("FCB_NewMatch"), IE_Pressed, this, &AFCBPlayerController::NewMatch);
	Input->BindAction(TEXT("FCB_RollSeed"), IE_Pressed, this, &AFCBPlayerController::RollSeed);
	Input->BindAction(TEXT("FCB_ToggleLog"), IE_Pressed, this, &AFCBPlayerController::ToggleLog);
	Input->BindAction(TEXT("FCB_ToggleDebug"), IE_Pressed, this, &AFCBPlayerController::ToggleDebug);
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

void AFCBPlayerController::SelectCardByIndex(int32 Index)
{
	if (UFCBGameInstance* Instance = GetFcbInstance())
	{
		Instance->SetSelectedHandIndex(Index);
	}
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
	if (UFCBGameInstance* Instance = GetFcbInstance()) { Instance->MoveSelection(-1); }
}

void AFCBPlayerController::SelectNextCard()
{
	if (UFCBGameInstance* Instance = GetFcbInstance()) { Instance->MoveSelection(1); }
}

void AFCBPlayerController::Confirm()
{
	if (UFCBGameInstance* Instance = GetFcbInstance())
	{
		Instance->ConfirmSelection();
	}
}

void AFCBPlayerController::OnClick()
{
	UFCBGameInstance* Instance = GetFcbInstance();
	const AFCBDebugHud* Hud = GetHUD<AFCBDebugHud>();
	if (!Instance || !Hud)
	{
		return;
	}

	float MouseX = 0.f, MouseY = 0.f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		return;
	}

	const int32 HandCount = Instance->GetHandCount(FCBSeat::A);
	for (int32 Index = 0; Index < HandCount; ++Index)
	{
		FVector2D TopLeft, BottomRight;
		if (!Hud->GetCardSlotRect(Index, TopLeft, BottomRight))
		{
			continue;
		}
		if (MouseX >= TopLeft.X && MouseX <= BottomRight.X && MouseY >= TopLeft.Y && MouseY <= BottomRight.Y)
		{
			Instance->SetSelectedHandIndex(Index);
			Instance->ConfirmSelection();
			return;
		}
	}
	// A click on empty table confirms the current selection, which is what a tap-anywhere mobile build does.
	Instance->ConfirmSelection();
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

bool AFCBDebugHud::GetCardSlotRect(int32 Index, FVector2D& OutTopLeft, FVector2D& OutBottomRight) const
{
	const APlayerController* Player = GetOwningPlayerController();
	UFCBGameInstance* Instance = (Player && Player->GetWorld())
		? Cast<UFCBGameInstance>(Player->GetWorld()->GetGameInstance()) : nullptr;
	if (!Instance || !Player)
	{
		return false;
	}

	FIntPoint ViewportSize;
	Player->GetViewportSize(ViewportSize); // APlayerController's out-param form; there is no value overload
	if (ViewportSize.X <= 0 || ViewportSize.Y <= 0)
	{
		return false;
	}

	const FCBHud::FLayout Layout = FCBHud::ComputeLayout(FVector2D(ViewportSize), Instance->GetHandCount(FCBSeat::A));
	if (Index < 0 || Index >= Layout.VisibleCards)
	{
		return false;
	}

	OutTopLeft = FVector2D(Layout.Margin, Layout.PlayerTop() + Layout.CardHeight * Index);
	OutBottomRight = OutTopLeft + FVector2D(Layout.CardWidth, Layout.CardHeight - 2.f);
	return true;
}

void AFCBDebugHud::FillRect(float X, float Y, float Width, float Height, const FLinearColor& Color)
{
	// AHUD::DrawRect is (color, x, y, w, h, alpha) with an FColor: ToFColor bakes alpha into the colour but the
	// blend mode ignores it, so the opacity is passed separately as well.
	DrawRect(Color.ToFColor(true), X, Y, Width, Height, Color.A);
}

float AFCBDebugHud::DrawLineAt(const FString& Text, float X, float Y, float Size, const FLinearColor& Color, bool bBold)
{
	UFont* Font = ResolveFont(bBold);
	if (!Font)
	{
		return Y + Size;
	}
	DrawText(FText::FromString(Text), Font, X, Y, Color.ToFColor(true));
	return Y + Size * 1.18f;
}

void AFCBDebugHud::DrawCardRow(const FFCBCardView& Card, float X, float Y, float Width, float Height, bool bSelected, bool bFaceDown)
{
	const float Alpha = bFaceDown ? 0.45f : 1.f;
	FLinearColor Frame = Card.FrameColor;
	Frame.A = Alpha;

	// Fill, then the selection border: cheaper than a widget brush and it reads fine at any scale.
	FillRect(X, Y, Width, Height, FLinearColor(0.06f, 0.06f, 0.08f, bSelected ? 0.92f : 0.7f));
	FillRect(X, Y, 5.f, Height, Frame);

	UFont* Font = ResolveFont(false);
	if (!Font)
	{
		return;
	}

	const FString Label = bFaceDown
		? FString::Printf(TEXT("%s  (face down)"), *Card.FactionName)
		: FString::Printf(TEXT("%s  [%s]"), *FCBHud::TrimToWidth(Card.DisplayName, 34), *Card.RarityName);
	DrawText(FText::FromString(Label), Font, X + 12.f, Y + Height * 0.10f,
		bSelected ? FColor::White : FColor(200, 200, 205));

	if (bFaceDown)
	{
		return;
	}

	const FString Stats = FString::Printf(TEXT("A %s   P %s   S %s   H %s"),
		*Card.AgeText, *Card.PowerText, *Card.SpeedText, *Card.HeightText);
	DrawText(FText::FromString(Stats), Font, X + 12.f, Y + Height * 0.55f, FColor(170, 190, 210));

	if (bSelected)
	{
		// AHUD has no line primitive of its own; the canvas does, and the HUD already owns the canvas here.
		if (Canvas)
		{
			Canvas->K2_DrawLine(FVector2D(X, Y), FVector2D(X + Width, Y), 1.5f, FLinearColor::White);
			Canvas->K2_DrawLine(FVector2D(X, Y + Height), FVector2D(X + Width, Y + Height), 1.5f, FLinearColor::White);
		}
	}
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

	FIntPoint ViewportSize;
	Player->GetViewportSize(ViewportSize); // APlayerController's out-param form; there is no value overload
	const FVector2D Viewport(ViewportSize.X, ViewportSize.Y);
	if (ViewportSize.X <= 0 || ViewportSize.Y <= 0)
	{
		return;
	}

	TArray<FFCBCardView> Hand;
	Instance->GetHandView(FCBSeat::A, Hand);
	const FCBHud::FLayout Layout = FCBHud::ComputeLayout(Viewport, Hand.Num());
	const float Size = Layout.CardHeight * 0.34f;
	const bool bDebug = FcbPlayer && FcbPlayer->IsDebugVisible();

	float Y = Layout.OpponentTop();
	const bool bPlayersTurn = Instance->IsPlayersTurn();

	// --- top band: the opponent and the shared piles ---------------------------------------------------------
	Y = DrawLineAt(FString::Printf(TEXT("%s  -  %d cards in hand, %d captured (score %d)"),
		*Instance->GetSeatName(FCBSeat::B),
		Instance->GetHandCount(FCBSeat::B),
		Instance->GetMatch().GetState().Seats[FCBSeat::B].Pile.Num(),
		Instance->GetScore(FCBSeat::B)),
		Layout.Margin, Y, Size * 1.2f, FLinearColor(0.95f, 0.85f, 0.6f), true);

	const FFCBCardView OpponentTop = Instance->GetOpponentTopCard();
	DrawCardRow(OpponentTop, Layout.Margin, Y, Layout.CardWidth * 0.62f, Layout.CardHeight, false, true);
	Y += Layout.CardHeight + 4.f;

	Y = DrawLineAt(FString::Printf(TEXT("Round %d   pot %d   deck %d   burned %d   |   your score %d"),
		Instance->GetRoundNumber(), Instance->GetPotCount(), Instance->GetDeckCount(),
		Instance->GetBurnedCount(), Instance->GetScore(FCBSeat::A)),
		Layout.Margin, Y, Size, FLinearColor(0.8f, 0.8f, 0.85f));

	if (!Instance->IsDataUsable())
	{
		DrawLineAt(Instance->GetDataNote(), Layout.Margin, Y, Size, FLinearColor(1.f, 0.45f, 0.35f), true);
		Y += Size * 1.4f;
	}

	// --- the table band: whose turn, and what will happen if we confirm -------------------------------------
	const float TableY = Layout.TableBandTop();
	if (Instance->IsMatchOver())
	{
		const AFCBGameMode* Mode = (FcbPlayer && FcbPlayer->GetWorld())
			? FcbPlayer->GetWorld()->GetAuthGameMode<AFCBGameMode>() : nullptr;
		DrawLineAt(TEXT("MATCH OVER"), Viewport.X * 0.5f - 120.f, TableY, Size * 1.8f, FLinearColor::White, true);
		if (Mode && !Mode->GetLastResultText().IsEmpty())
		{
			DrawLineAt(Mode->GetLastResultText(), Viewport.X * 0.5f - 120.f, TableY + Size * 0.1f, Size * 1.1f,
				FLinearColor(0.95f, 0.85f, 0.55f), true);
		}
		DrawLineAt(Instance->FormatSummary(), Viewport.X * 0.5f - 120.f, TableY + Size * 2.2f, Size,
			FLinearColor(0.9f, 0.9f, 0.9f));
		DrawLineAt(TEXT("press N for a new match, R to roll a fresh deal"),
			Viewport.X * 0.5f - 120.f, TableY + Size * 3.6f, Size, FLinearColor(0.6f, 0.85f, 0.6f));
	}
	else if (!bPlayersTurn)
	{
		DrawLineAt(FString::Printf(TEXT("%s is thinking..."), *Instance->GetSeatName(FCBSeat::B)),
			Viewport.X * 0.5f - 100.f, TableY, Size * 1.3f, FLinearColor(0.9f, 0.75f, 0.45f), true);
	}
	else
	{
		FFCBRoundView Preview;
		if (Instance->ProjectSelectedMove(Preview))
		{
			const FLinearColor PreviewColor = Preview.AttackerValue > Preview.DefenderValue
				? FLinearColor(0.55f, 0.9f, 0.55f)
				: (Preview.AttackerValue < Preview.DefenderValue ? FLinearColor(0.95f, 0.55f, 0.5f) : FLinearColor(0.95f, 0.85f, 0.4f));
			DrawLineAt(FString::Printf(TEXT("declare %s:  %d vs %d  ->  %s"),
				*Preview.AttributeName, Preview.AttackerValue, Preview.DefenderValue, *Preview.OutcomeText),
				Layout.DetailLeft(), TableY, Size * 1.25f, PreviewColor, true);
			for (int32 NoteIndex = 0; NoteIndex < Preview.ModifierNotes.Num(); ++NoteIndex)
			{
				DrawLineAt(FCBHud::TrimToWidth(Preview.ModifierNotes[NoteIndex], 96),
					Layout.DetailLeft(), TableY + Size * (1.4f + NoteIndex), Size * 0.92f,
					FLinearColor(0.75f, 0.75f, 0.85f));
			}
		}
	}

	// --- bottom band: the hand --------------------------------------------------------------------------------
	const float PlayerTop = Layout.PlayerTop();
	Y = PlayerTop - Size * 1.6f;
	DrawLineAt(FString::Printf(TEXT("YOUR HAND  -  A age   P power   S speed   H height   Enter confirms   (click a card to play it)")),
		Layout.Margin, Y, Size * 1.05f, FLinearColor(0.85f, 0.85f, 0.9f), true);

	for (int32 Index = 0; Index < Hand.Num() && Index < Layout.VisibleCards; ++Index)
	{
		DrawCardRow(Hand[Index], Layout.Margin, PlayerTop + Layout.CardHeight * Index, Layout.CardWidth,
			Layout.CardHeight, Index == Instance->GetSelectedHandIndex(), false);
	}

	// --- right column: the selected card in full, then the log ------------------------------------------------
	const FFCBCardView Selected = Instance->GetSelectedCard();
	float DetailY = Layout.DetailLeft() > 0.f ? TableY + Size * 3.2f : TableY;
	const float DetailX = Layout.DetailLeft();

	if (!Selected.DisplayName.IsEmpty())
	{
		DetailY = DrawLineAt(FString::Printf(TEXT("%s  (%s - %s)"), *Selected.DisplayName, *Selected.FactionName, *Selected.RarityName),
			DetailX, DetailY, Size * 1.15f, FLinearColor(0.95f, 0.9f, 0.75f), true) + 2.f;
		DetailY = DrawLineAt(FString::Printf(TEXT("Age %s   Power %s   Speed %s   Height %s"),
			*Selected.AgeText, *Selected.PowerText, *Selected.SpeedText, *Selected.HeightText),
			DetailX, DetailY, Size, FLinearColor(0.8f, 0.85f, 0.95f)) + 2.f;
		if (!Selected.Ability0Name.IsEmpty())
		{
			DetailY = DrawLineAt(FString::Printf(TEXT("%s: %s"), *Selected.Ability0Name, *FCBHud::TrimToWidth(Selected.Ability0Text, 110)),
				DetailX, DetailY, Size, FLinearColor(0.8f, 0.9f, 0.8f));
		}
		if (!Selected.Ability1Name.IsEmpty())
		{
			DetailY = DrawLineAt(FString::Printf(TEXT("%s: %s"), *Selected.Ability1Name, *FCBHud::TrimToWidth(Selected.Ability1Text, 110)),
				DetailX, DetailY, Size, FLinearColor(0.8f, 0.9f, 0.8f));
		}
		if (Selected.PowerStacks > 0 || Selected.RemainingCharges < 1)
		{
			DetailY = DrawLineAt(FString::Printf(TEXT("on the table: +%d power, %d charge(s) left"),
				Selected.PowerStacks, Selected.RemainingCharges),
				DetailX, DetailY, Size, FLinearColor(0.95f, 0.7f, 0.4f));
		}
		DetailY = DrawLineAt(FString::Printf(TEXT("strength %.0f%% of the pool"), Selected.StrengthPercentile * 100.f),
			DetailX, DetailY, Size * 0.9f, FLinearColor(0.6f, 0.6f, 0.68f));
	}

	if (FcbPlayer && FcbPlayer->IsLogVisible())
	{
		const FString Log = Instance->FormatLog(FMath::Max(0, Instance->GetRoundNumber() - 8));
		TArray<FString> Lines;
		Log.ParseIntoArrayLines(Lines, false);
		float LogY = Viewport.Y * 0.42f;
		for (int32 Index = FMath::Max(0, Lines.Num() - 12); Index < Lines.Num(); ++Index)
		{
			LogY = DrawLineAt(FCBHud::TrimToWidth(Lines[Index], 132), DetailX, LogY, Size * 0.86f,
				FLinearColor(0.65f, 0.68f, 0.72f));
		}
	}

	if (bDebug)
	{
		float DebugY = Viewport.Y - Size * 6.f;
		const FString Note = Instance->GetDataNote();
		DebugY = DrawLineAt(FString::Printf(TEXT("data: %s"), *FCBHud::TrimToWidth(Note, 120)),
			DetailX, DebugY, Size * 0.9f, FLinearColor(0.5f, 0.9f, 0.9f));
		DebugY = DrawLineAt(FString::Printf(TEXT("seed: %u   ai: %s"),
			Instance->GetMatch().GetConfig().RngSeed,
			*StaticEnum<EFCBAiDifficulty>()->GetNameStringByValue(static_cast<int32>(Instance->GetAiDifficulty()))),
			DetailX, DebugY, Size * 0.9f, FLinearColor(0.5f, 0.9f, 0.9f));
		DebugY = DrawLineAt(FString::Printf(TEXT("rng state: %u   ai: %s"),
			Instance->GetMatch().PeekRngState(),
			*FCBHud::TrimToWidth(Instance->GetLastAiExplanation(), 120)),
			DetailX, DebugY, Size * 0.9f, FLinearColor(0.5f, 0.9f, 0.9f));
		DrawLineAt(TEXT("F1 toggles this overlay - L the log"), DetailX, DebugY, Size * 0.9f, FLinearColor(0.45f, 0.5f, 0.55f));
	}
}
