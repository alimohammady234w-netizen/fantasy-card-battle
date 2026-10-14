#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FCBTypes.h"
#include "FCBWidgets.generated.h"
class AFCBPlayerController; class UFCBProgressionSubsystem; class UFCBBattleStateMachine;

/** Programmatically-built UMG widgets (no editor Widget Blueprints required). All buttons call real systems.
 *  Buttons are constructed ONCE in NativeConstruct and only their labels/state change afterwards (no per-round widget churn). */
UCLASS() class FANTASYCARDBATTLE_API UFCBMainMenuWidget : public UUserWidget { GENERATED_BODY()
public: void Init(AFCBPlayerController* InPC); virtual void NativeConstruct() override; void RefreshStatus();
private: UPROPERTY() TObjectPtr<AFCBPlayerController> PC; UPROPERTY() TObjectPtr<class UTextBlock> StatusText;
	UFUNCTION() void OnPlay(); UFUNCTION() void OnCollection(); UFUNCTION() void OnProgression(); UFUNCTION() void OnSettings(); UFUNCTION() void OnExit(); };

UCLASS() class FANTASYCARDBATTLE_API UFCBBattleWidget : public UUserWidget { GENERATED_BODY()
public: void Init(AFCBPlayerController* InPC); virtual void NativeConstruct() override;
	void PresentRound(UFCBBattleStateMachine* Battle, bool bReset); void PresentResult(UFCBBattleStateMachine* Battle, const FFCBPredictionChoice& Pick, int32 PredictorValue, int32 OtherValue); void PresentMatchSummary(UFCBBattleStateMachine* Battle, int32 XP, bool bLeveled);
private: UPROPERTY() TObjectPtr<AFCBPlayerController> PC;
	UPROPERTY() TObjectPtr<class UTextBlock> Header, Message, PlayerInfo, AIInfo;
	UPROPERTY() TArray<TObjectPtr<class UButton>> AttrBtns; UPROPERTY() TArray<TObjectPtr<class UTextBlock>> AttrLabels;
	UPROPERTY() TObjectPtr<class UButton> LockBtn, NextBtn;
	EFCBAttribute SelAttr = EFCBAttribute::Power; EFCBPrediction SelDir = EFCBPrediction::Greater; bool bDirSet = false, bAttrSet = false;
	UFUNCTION() void OnLock(); UFUNCTION() void OnNext(); UFUNCTION() void OnQuit();
	UFUNCTION() void OnAttr0(); UFUNCTION() void OnAttr1(); UFUNCTION() void OnAttr2(); UFUNCTION() void OnAttr3(); UFUNCTION() void OnAttr4(); UFUNCTION() void OnAttr5(); UFUNCTION() void OnAttr6(); UFUNCTION() void OnAttr7();
	UFUNCTION() void OnDirGreater(); UFUNCTION() void OnDirLess(); };

UCLASS() class FANTASYCARDBATTLE_API UFCBCollectionWidget : public UUserWidget { GENERATED_BODY()
public: void Init(AFCBPlayerController* InPC, UFCBProgressionSubsystem* InProg); virtual void NativeConstruct() override; void Refresh(const FString& Search = TEXT(""));
private: UPROPERTY() TObjectPtr<AFCBPlayerController> PC; UPROPERTY() TObjectPtr<UFCBProgressionSubsystem> Prog;
	UPROPERTY() TObjectPtr<class UTextBlock> Detail; FName Selected; int32 BrowseIndex = 0;
	UFUNCTION() void OnBack(); UFUNCTION() void OnUpgrade(); UFUNCTION() void OnPlayFromHere();
	UFUNCTION() void OnPrevCard(); UFUNCTION() void OnNextCard();
	UFUNCTION() void OnAddToDeck(); UFUNCTION() void OnRemoveFromDeck(); UFUNCTION() void OnNewDeck(); UFUNCTION() void OnValidateDeck(); };
