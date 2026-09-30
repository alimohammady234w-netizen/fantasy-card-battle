// Fantasy Card Battle - Main GameMode implementation

#include "CardGameGameMode.h"

#include "CardGame.h"
#include "CardGamePlayerController.h"
#include "GameFramework/SpectatorPawn.h"

ACardGameGameMode::ACardGameGameMode()
{
	// Every player in the project uses the project PlayerController.
	PlayerControllerClass = ACardGamePlayerController::StaticClass();

	// The game is UI-driven (menus + card battles without pawn movement),
	// so players spawn as spectators. Maps only need a PlayerStart.
	DefaultPawnClass = ASpectatorPawn::StaticClass();
}

void ACardGameGameMode::BeginPlay()
{
	Super::BeginPlay();

	const UWorld* World = GetWorld();
	UE_LOG(LogCardGame, Log,
		TEXT("ACardGameGameMode::BeginPlay - map '%s' started."),
		World ? *World->GetName() : TEXT("<no world>"));
}
