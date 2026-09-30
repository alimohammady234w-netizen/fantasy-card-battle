// Fantasy Card Battle - Main PlayerController (infrastructure only)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CardGamePlayerController.generated.h"

/**
 * Main PlayerController of the project.
 *
 * Phase 1 responsibilities (infrastructure only):
 * - Mouse/touch cursor always visible (menus and cards are UI-driven).
 * - Input mode set to Game+UI so both world taps and UMG widgets receive input.
 *
 * Gameplay input bindings (card select/drag/drop...) will be added in later phases
 * via SetupInputComponent / Enhanced Input, kept separate from widget code.
 */
UCLASS()
class CARDGAME_API ACardGamePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACardGamePlayerController();

protected:
	virtual void BeginPlay() override;
};
