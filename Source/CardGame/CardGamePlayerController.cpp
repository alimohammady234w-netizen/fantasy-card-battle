// Fantasy Card Battle - Main PlayerController implementation

#include "CardGamePlayerController.h"

#include "CardGame.h"

ACardGamePlayerController::ACardGamePlayerController()
{
	// UI-driven game: the cursor/pointer must always be visible
	// and clickable/hoverable so UMG widgets work on desktop and Android touch.
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

void ACardGamePlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Let input reach both the world (cards in 3D/2D space) and UMG widgets.
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);

	UE_LOG(LogCardGame, Log, TEXT("ACardGamePlayerController::BeginPlay - player controller ready."));
}
