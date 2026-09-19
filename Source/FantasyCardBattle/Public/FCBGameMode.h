// Copyright (c) Fantasy Card Battle. All rights reserved.
//
// FCBGameMode.h - the playable, asset-free presentation layer.
//
// AFCBGameMode, AFCBPlayerController and AFCBDebugHud are all created from C++ defaults, so opening the
// project and pressing Play works with an empty Content folder. That is a deliberate constraint: a rules
// change must be testable by running the game, not by rebuilding widgets. A real UMG front-end (WBP_Card,
// WBP_Hand, a main menu) is the *next* layer on top of the same UFCBGameInstance API, and the debug HUD is
// written so it can be switched off with one setting (bShowDebugHud) instead of being deleted.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "FCBGameInstance.h"
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

protected:
	/** Seconds before a finished match redeals. 0 disables the auto-restart. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "FCB", meta = (ClampMin = "0", ClampMax = "600"))
	float AutoRestartSeconds = 0.f;

private:
	UFCBGameInstance* GetFcbInstance() const;

	FTimerHandle RestartTimer;

	/** Banner text for the end-of-match band; the game instance remains the source of truth for the result. */
	const FString& GetLastResultText() const { return LastResultText; }

private:
	FString LastResultText;
};

/** Keyboard/gamepad input. Legacy bindings from Config/DefaultInput.ini - no Input Action assets to create. */
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
	UFUNCTION() void Confirm();
	UFUNCTION() void NewMatch();
	UFUNCTION() void RollSeed();
	UFUNCTION() void ToggleLog();
	UFUNCTION() void ToggleDebug();

	/** Left mouse button: confirms the hovered card. Widget-based picking arrives with UMG. */
	UFUNCTION() void OnClick();

	bool IsLogVisible() const { return bLogVisible; }
	bool IsDebugVisible() const { return bDebugVisible; }

private:
	UFCBGameInstance* GetFcbInstance() const;

	void SelectCardByIndex(int32 Index);

	UPROPERTY()
	TObjectPtr<AFCBGameMode> FcbGameMode;

	bool bLogVisible = true;
	bool bDebugVisible = false;
};

/**
 * Everything on screen is drawn here, from the game instance's views. Layout is a simple 3-band grid
 * (opponent / table / player) computed from the viewport size, which keeps the code usable on desktop and on
 * the Android target without a resolution-specific widget.
 */
UCLASS(Config = Game)
class FANTASYCARDBATTLE_API AFCBDebugHud : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	/** Card slot rects, in viewport pixels, for the click handler. */
	bool GetCardSlotRect(int32 Index, FVector2D& OutTopLeft, FVector2D& OutBottomRight) const;

private:
	float DrawLineAt(const FString& Text, float X, float Y, float Size, const FLinearColor& Color, bool bBold = false);
	void DrawCardRow(const FFCBCardView& Card, float X, float Y, float Width, float Height, bool bSelected, bool bFaceDown);
	UFont* ResolveFont(bool bBold) const;
	void FillRect(float X, float Y, float Width, float Height, const FLinearColor& Color);
};
