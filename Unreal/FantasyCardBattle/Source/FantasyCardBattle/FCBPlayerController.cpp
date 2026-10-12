#include "FCBPlayerController.h"
#include "FCBProgressionSubsystem.h"
#include "FCBBattleStateMachine.h"
#include "FCBBattleRules.h"
#include "FCBGameMode.h"
#include "FCBCardActor.h"
#include "UI/FCBWidgets.h"
#include "Blueprint/UserWidget.h"

UFCBProgressionSubsystem* AFCBPlayerController::Prog() { return GetGameInstance()->GetSubsystem<UFCBProgressionSubsystem>(); }
void AFCBPlayerController::ShowWidget(UUserWidget* W) {
	for (UUserWidget* X : { (UUserWidget*)MainMenu, (UUserWidget*)BattleWidget, (UUserWidget*)CollectionWidget }) if (X) X->SetVisibility(X == W ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	bShowMouseCursor = true; SetInputMode(FInputModeUIOnly());
}
void AFCBPlayerController::BeginPlay() {
	Super::BeginPlay();
	MainMenu = CreateWidget<UFCBMainMenuWidget>(this, UFCBMainMenuWidget::StaticClass()); if (MainMenu) MainMenu->AddToViewport(10);
	BattleWidget = CreateWidget<UFCBBattleWidget>(this, UFCBBattleWidget::StaticClass()); if (BattleWidget) BattleWidget->AddToViewport(9);
	CollectionWidget = CreateWidget<UFCBCollectionWidget>(this, UFCBCollectionWidget::StaticClass()); if (CollectionWidget) CollectionWidget->AddToViewport(9);
	if (MainMenu) { MainMenu->Init(this); } if (BattleWidget) BattleWidget->Init(this); if (CollectionWidget) CollectionWidget->Init(this, Prog());
	Battle = NewObject<UFCBBattleStateMachine>(this);
	Battle->SetCardLookup([this](FName Id, FFCBCardTableRow& Row) { return Prog()->GetDefinition(Id, Row); });
	Battle->SetPlayerLevelLookup([this](FName Id) { return Prog()->GetOwned(Id).Level; });
	Battle->OnRoundResolved.AddDynamic(this, &AFCBPlayerController::OnRoundResolved);
	Battle->OnMatchFinished.AddDynamic(this, &AFCBPlayerController::OnMatchFinished);
	ShowWidget(MainMenu);
}
void AFCBPlayerController::StartMatchFromActiveDeck(FString& OutError) {
	TArray<FString> Errors; if (!Prog()->ValidateActiveDeck(Errors)) { OutError = Errors.IsEmpty() ? TEXT("Invalid deck") : Errors[0]; return; }
	// Use the first deck that validates against owned copies (the active deck is validated above).
	TArray<FName> PlayerIds; for (const FFCBDeck& D : Prog()->GetDecks()) { TArray<FString> E; if (FFCBBattleRules::ValidateDeck(D.CardIds, Prog()->OwnedCopiesMap(), FFCBBattleRules::DefaultDeckSize, E)) { PlayerIds = D.CardIds; break; } }
	if (PlayerIds.IsEmpty()) { OutError = TEXT("No valid deck found."); return; }
	TArray<FName> All = Prog()->GetAllCardIds(); FRandomStream Rng((int32)(FDateTime::Now().GetTicks() % INT_MAX));
	for (int32 i = All.Num() - 1; i > 0; --i) All.Swap(i, Rng.RandRange(0, i));
	All.SetNum(FMath::Min(FFCBBattleRules::DefaultDeckSize, All.Num()));
	Battle->Difficulty = Prog()->Difficulty; Battle->StartMatch(PlayerIds, All, Rng.RandRange(0, INT_MAX)); CurrentMatchId = Battle->MatchId;
	FName PC, Hidden; if (Battle->BeginNextRound(PC, Hidden)) { CurrentPlayerCard = PC; ShowWidget(BattleWidget); BattleWidget->PresentRound(Battle, /*bReset=*/true); if (AFCBGameMode* GM = Cast<AFCBGameMode>(GetWorld()->GetAuthGameMode())) { FFCBCardTableRow Row; if (Prog()->GetDefinition(PC, Row) && GM->PlayerCardActor) GM->PlayerCardActor->ShowCard(Row, FFCBBattleRules::EffectiveAttributes(Row.Attributes, Prog()->GetOwned(PC).Level), true); if (GM->AICardActor) GM->AICardActor->ShowBack(); } }
}
void AFCBPlayerController::SubmitPrediction(EFCBAttribute Attribute, EFCBPrediction Direction) {
	if (!Battle) return;
	if (Battle->IsPlayerPredictor()) { FFCBPredictionChoice P; P.Attribute = Attribute; P.Direction = Direction; if (!Battle->LockPlayerPrediction(P)) return; }
	else if (!Battle->LockAIPrediction()) return;
	FFCBPredictionChoice Pick; int32 PV = 0, OV = 0;
	// Reveal the AI card actor at the same moment the state machine resolves (never before lock)
	if (AFCBGameMode* GM = Cast<AFCBGameMode>(GetWorld()->GetAuthGameMode())) { FName AIId; /* id known to controller only after resolve below */ (void)AIId; (void)GM; }
	if (Battle->RevealAndResolve(Pick, PV, OV) && BattleWidget) {
		FName AIId; Battle->GetRevealedAICard(AIId);
		if (AFCBGameMode* GM = Cast<AFCBGameMode>(GetWorld()->GetAuthGameMode())) { FFCBCardTableRow Row; if (Prog()->GetDefinition(AIId, Row) && GM->AICardActor) { GM->AICardActor->ShowCard(Row, Row.Attributes, true); GM->AICardActor->FlipToFront(0.6f); } }
		BattleWidget->PresentResult(Battle, Pick, PV, OV);
	}
}
void AFCBPlayerController::OnRoundResolved(EFCBRoundResult Result, const FFCBPredictionChoice& Pick) {
	Prog()->GrantCardXP(CurrentPlayerCard, Result == EFCBRoundResult::Tie ? 12 : 8); // winner bonus handled in widget summary text; XP saved at match end + here
}
void AFCBPlayerController::OnMatchFinished() {
	if (!Battle) return; int32 XP = 0; bool bUp = false;
	const bool bWon = Battle->PlayerScore > Battle->AIScore, bDraw = Battle->PlayerScore == Battle->AIScore;
	Prog()->AwardMatchRewardsOnce(CurrentMatchId, bWon, bDraw, XP, bUp); // saved inside; idempotent per MatchId
	if (BattleWidget) BattleWidget->PresentMatchSummary(Battle, XP, bUp);
}
void AFCBPlayerController::ContinueAfterResult() {
	if (!Battle) return; FName PC, Hidden;
	if (Battle->BeginNextRound(PC, Hidden)) { CurrentPlayerCard = PC; if (BattleWidget) BattleWidget->PresentRound(Battle, false);
		if (AFCBGameMode* GM = Cast<AFCBGameMode>(GetWorld()->GetAuthGameMode())) { FFCBCardTableRow Row; if (Prog()->GetDefinition(PC, Row) && GM->PlayerCardActor) GM->PlayerCardActor->ShowCard(Row, FFCBBattleRules::EffectiveAttributes(Row.Attributes, Prog()->GetOwned(PC).Level), true); if (GM->AICardActor) GM->AICardActor->ShowBack(); } }
}
