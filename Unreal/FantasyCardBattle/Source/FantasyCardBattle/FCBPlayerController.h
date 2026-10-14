#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "FCBTypes.h"
#include "FCBPlayerController.generated.h"
UCLASS() class FANTASYCARDBATTLE_API AFCBPlayerController : public APlayerController {
	GENERATED_BODY()
public:
	virtual void BeginPlay() override;
	UFUNCTION(BlueprintCallable) void StartMatchFromActiveDeck(FString& OutError);
	UFUNCTION(BlueprintCallable) void SubmitPrediction(EFCBAttribute Attribute, EFCBPrediction Direction);
	UFUNCTION(BlueprintCallable) void ContinueAfterResult();
	class UFCBBattleStateMachine* Battle = nullptr;
	class UFCBMainMenuWidget* MainMenu = nullptr; class UFCBBattleWidget* BattleWidget = nullptr; class UFCBCollectionWidget* CollectionWidget = nullptr;
private:
	void ShowWidget(class UUserWidget* W);
	class UFCBProgressionSubsystem* Prog();
	void OnRoundResolved(EFCBRoundResult Result, const FFCBPredictionChoice& Pick);
	void OnMatchFinished();
	FString CurrentMatchId; FName CurrentPlayerCard;
};
