#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "FCBGameMode.generated.h"
/** Builds the 3D battlefield procedurally from engine primitives/materials (no marketplace assets):
 *  rune-inlaid table, card zones, deck/discard stacks, cinematic lights, ambient particles (Niagara if assigned). */
UCLASS() class FANTASYCARDBATTLE_API AFCBGameMode : public AGameModeBase {
	GENERATED_BODY()
public:
	AFCBGameMode();
	virtual void BeginPlay() override;
	UPROPERTY(EditAnywhere, Category="Battlefield") TSoftObjectPtr<class UNiagaraSystem> AmbientParticles;
	class AFCBCardActor* PlayerCardActor = nullptr; class AFCBCardActor* AICardActor = nullptr;
private:
	void BuildBattlefield();
};
