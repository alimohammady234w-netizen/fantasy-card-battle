// Copyright (c) Fantasy Card Battle. All rights reserved.
//
// FCBGameMode.h - the playable, asset-free presentation layer.
//
// AFCBGameMode, AFCBPlayerController and AFCBDebugHud are all created from C++ defaults, so opening the
// project and pressing Play works with an empty Content folder. That is a deliberate constraint: a rules
// change must be testable by running the game, not by rebuilding widgets. A real UMG front-end (WBP_Card,
// WBP_Hand, a main menu) is the *next* layer on top of the same UFCBGameInstance API, and the debug HUD is
// written so it can be switched off with one setting (bShowDebugHud) instead of being deleted.
//
// Touch and mouse are not a second code path: FCBTouch::BuildLayout decides where everything is, the HUD
// draws it and the controller hit tests it, so a phone tap, a mouse click and a keypress all end in the same
// AFCBPlayerController::ExecuteCommand. See Docs/Mobile.md.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "FCBGameInstance.h"
#include "FCBTouch.h"
#include "FCBGameMode.generated.h"

class UFCBGameInstance;

/** Owns nothing but the session's flow: start a match, offer "new match", react to the result. */
UCLASS(Config = Game)
class FANTASYCARDBATTLE_API AFCBGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AFCBGameMode();

	virtual void StartPlay() override;

	/** Bound to the game instance so the mode can react without the instance knowing about actors. */
	UFUNCTION()
	void HandleMatchFinished(int32 WinnerSeat, const FString& Reason);

	/** Keybind N and the "Play again" action on the end screen. */
	UFUNCTION(BlueprintCallable, Category = "FCB")
	void RequestNewMatch();

	/** Banner text for the end-of-match band; the game instance remains the source of truth for the result. */
	const FString& GetLastResultText() const { return LastResultText; }

protected:
	/** Seconds before a finished match redeals. 0 disables the auto-restart. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "FCB", meta = (ClampMin = "0", ClampMax = "600"))
	float AutoRestartSeconds = 0.f;

private:
	UFCBGameInstance* GetFcbInstance() const;

	FTimerHandle RestartTimer;

	FString LastResultText;
};

/**
 * Keyboard, mouse and touch input.
 *
 * Keys come from Config/DefaultInput.ini (legacy bindings, no Input Action assets to create) and touch comes
 * from BindTouch press/release pairs, because a gesture only needs the two endpoints plus the time between
 * them - no touch-move stream, and therefore no per-frame cost on the render thread.
 */
UCLASS(Config = Game)
class FANTASYCARDBATTLE_API AFCBPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	// --- attribute declarations
	UFUNCTION() void DeclareAge();
	UFUNCTION() void DeclarePower();
	UFUNCTION() void DeclareSpeed();
	UFUNCTION() void DeclareHeight();

	// --- hand selection
	UFUNCTION() void SelectCard1();
	UFUNCTION() void SelectCard2();
	UFUNCTION() void SelectCard3();
	UFUNCTION() void SelectCard4();
	UFUNCTION() void SelectCard5();
	UFUNCTION() void SelectCard6();
	UFUNCTION() void SelectCard7();
	UFUNCTION() void SelectCard8();
	UFUNCTION() void SelectCard9();
	UFUNCTION() void SelectCard10();
	UFUNCTION() void SelectCard11();
	UFUNCTION() void SelectCard12();
	UFUNCTION() void SelectPreviousCard();
	UFUNCTION() void SelectNextCard();
	UFUNCTION() void SelectPreviousPage();
	UFUNCTION() void SelectNextPage();
	UFUNCTION() void Confirm();
	UFUNCTION() void NewMatch();
	UFUNCTION() void RollSeed();
	UFUNCTION() void ToggleLog();
	UFUNCTION() void ToggleDebug();

	/** Left mouse button. Resolved through the same layout and the same commands as a touch. */
	UFUNCTION() void OnClick();

	/** Touch press: remembers the finger and where/when it landed. */
	void TouchPressed(ETouchIndex::Type FingerIndex, FVector Location);

	/** Touch release: classifies the gesture and executes the resulting command. */
	void TouchReleased(ETouchIndex::Type FingerIndex, FVector Location);

	/** The one place input becomes game calls, whichever device produced it. */
	void ExecuteCommand(const FCBTouch::FCommand& Command);

	bool IsLogVisible() const { return bLogVisible; }
	bool IsDebugVisible() const { return bDebugVisible; }

private:
	UFCBGameInstance* GetFcbInstance() const;

	void SelectCardByIndex(int32 Index);

	/** Shared by the keyboard, the mouse and the touch paths: keeps the visible page on the selection. */
	void SelectCardAndFollow(int32 Index);

	/** Paging through the same code path the touch page buttons use. */
	void TurnPage(int32 Delta);

	UPROPERTY()
	TObjectPtr<AFCBGameMode> FcbGameMode;

	bool bLogVisible = true;
	bool bDebugVisible = false;

	/** Touch state: one finger at a time, which is all this game needs (no pinch, no multi-touch commands). */
	FVector2D TouchDownPosition = FVector2D::ZeroVector;
	float TouchDownTimeSeconds = 0.f;
	int32 TouchDownFinger = INDEX_NONE;
};

/**
 * Everything on screen is drawn here, from the game instance's views, using the layout FCBTouch computed.
 * One layout means the drawing code and the hit testing code cannot disagree, which is the bug that makes a
 * touch build feel broken ("I tapped it and nothing happened").
 */
UCLASS(Config = Game)
class FANTASYCARDBATTLE_API AFCBDebugHud : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	/**
	 * The current layout, in viewport pixels: the HUD's DrawHUD and the controller's hit testing both call
	 * this. False when there is no viewport or no game instance yet.
	 */
	bool BuildTableLayout(FCBTouch::FTableLayout& OutLayout) const;

	/** The current state the layout needs (hand size, selection, page, playability). */
	bool BuildTableState(FCBTouch::FState& OutState) const;

	int32 GetHandPage() const { return HandPage; }
	void SetHandPage(int32 InPage);

	/** INDEX_NONE when the full-card overlay is closed. */
	int32 GetPeekCardIndex() const { return PeekCardIndex; }

	/**
	 * Opens the full card face for a hand slot. The card's id is stored next to the slot, because abilities and
	 * captures move cards around the hand: "the card I was reading" has to follow the card, like the selection.
	 */
	void SetPeekCard(int32 InHandIndex);

	/** Turns the page to the one holding a card, used when the selection moves off the visible page. */
	void FollowSelection(int32 InHandIndex);

	/**
	 * The last command note - including the commands that did nothing ("ignored: it is not a playable
	 * moment"). On a phone a tap with no visible response reads as a broken game, so it is drawn for a moment.
	 */
	void SetInputNote(const FString& InNote);
	const FString& GetInputNote() const { return InputNote; }
	float GetInputNoteAgeSeconds() const;

private:
	//~ drawing helpers
	float DrawLineAt(const FString& Text, float X, float Y, float PixelHeight, const FLinearColor& Color, bool bBold = false);
	void DrawButton(const FCBTouch::FRect& Rect, const FString& Label, const FLinearColor& Color, bool bEnabled);
	void DrawStatCells(const FFCBCardView& Card, const FCBTouch::FRect& Row, const FCBTouch::FTableLayout& Layout,
		int32 VisibleIndex, bool bSelected, EFCBAttribute DeclaredAttribute);
	void DrawCardRow(const FFCBCardView& Card, const FCBTouch::FRect& Row, const FCBTouch::FTableLayout& Layout,
		int32 VisibleIndex, bool bSelected, bool bFaceDown, EFCBAttribute DeclaredAttribute);
	void DrawStatusBand(const UFCBGameInstance& Instance, const FCBTouch::FTableLayout& Layout, float StatusSize);
	void DrawHand(const UFCBGameInstance& Instance, const TArray<FFCBCardView>& Hand, const FCBTouch::FTableLayout& Layout);
	void DrawDetailPanel(const UFCBGameInstance& Instance, const FFCBCardView& Card, const FCBTouch::FTableLayout& Layout, EFCBAttribute Declared);
	void DrawPeekOverlay(const FFCBCardView& CardView, const FCBTouch::FTableLayout& Layout);
	void DrawEndScreen(const UFCBGameInstance& Instance, const FCBTouch::FTableLayout& Layout);
	void DrawDebugOverlay(const UFCBGameInstance& Instance, const FCBTouch::FTableLayout& Layout, float StatusSize);
	void FillRect(float X, float Y, float Width, float Height, const FLinearColor& Color);
	void FrameRect(const FCBTouch::FRect& Rect, const FLinearColor& Color, float Thickness);
	UFont* ResolveFont(bool bBold) const;

	/** The page the hand is showing. Clamped by the layout on every build, so a stale value is harmless. */
	int32 HandPage = 0;

	/** Long-press overlay: the full face of one card, modal until it is dismissed. */
	int32 PeekCardIndex = INDEX_NONE;
	FName PeekCardId;

	/** Watches the selection so the page follows the keyboard and the rules without fighting manual paging. */
	int32 LastSelectedIndex = INDEX_NONE;

	/** Last command note and when it was set, in world seconds. */
	FString InputNote;
	float InputNoteTimeSeconds = -1000.f;
};
