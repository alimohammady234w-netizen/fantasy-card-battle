#include "FCBWidgets.h"
#include "../FCBPlayerController.h"
#include "../FCBProgressionSubsystem.h"
#include "../FCBBattleStateMachine.h"
#include "../FCBBattleRules.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UObject/Class.h" // UEnum::GetValueAsString

static UTextBlock* BtnLabel(UButton* B) { return B && B->GetChildrenCount() ? Cast<UTextBlock>(B->GetChildAt(0)) : nullptr; }
static UButton* MakeBtn(UUserWidget* W, UVerticalBox* Box, const FString& Label, FName Name) {
	UButton* B = W->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
	UTextBlock* T = W->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass()); T->SetText(FText::FromString(Label)); B->AddChild(T); Box->AddChild(B); return B;
}
static UTextBlock* MakeText(UUserWidget* W, UVerticalBox* Box, float Size = 20.f) {
	UTextBlock* T = W->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass()); T->SetAutoWrapText(true);
	FSlateFontInfo F = T->GetFont(); F.Size = Size; T->SetFont(F); Box->AddChild(T); return T;
}
static FString AttrLine(const FFCBAttributeSet& A) { return FString::Printf(TEXT("Power %d  Speed %d  Height %d  Defense %d\nIntelligence %d  Stamina %d  Luck %d  Age %d"), A.Power, A.Speed, A.Height, A.Defense, A.Intelligence, A.Stamina, A.Luck, A.Age); }

// ---------------- Main Menu (Play / Collection+DeckBuilder / Progression / Settings / Exit - all functional) ----------------
void UFCBMainMenuWidget::Init(AFCBPlayerController* InPC) { PC = InPC; RefreshStatus(); }
void UFCBMainMenuWidget::NativeConstruct() {
	Super::NativeConstruct(); auto* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root")); WidgetTree->RootWidget = Box;
	MakeText(this, Box, 44)->SetText(FText::FromString(TEXT("FANTASY CARD BATTLE")));
	StatusText = MakeText(this, Box, 16);
	MakeBtn(this, Box, TEXT("Play"), TEXT("BtnPlay"))->OnClicked.AddDynamic(this, &UFCBMainMenuWidget::OnPlay);
	MakeBtn(this, Box, TEXT("Card Collection & Deck Builder"), TEXT("BtnColl"))->OnClicked.AddDynamic(this, &UFCBMainMenuWidget::OnCollection);
	MakeBtn(this, Box, TEXT("Player Progression"), TEXT("BtnProg"))->OnClicked.AddDynamic(this, &UFCBMainMenuWidget::OnProgression);
	MakeBtn(this, Box, TEXT("Settings: Cycle AI Difficulty"), TEXT("BtnSet"))->OnClicked.AddDynamic(this, &UFCBMainMenuWidget::OnSettings);
	MakeBtn(this, Box, TEXT("Exit"), TEXT("BtnExit"))->OnClicked.AddDynamic(this, &UFCBMainMenuWidget::OnExit);
	RefreshStatus();
}
void UFCBMainMenuWidget::RefreshStatus() {
	if (!StatusText || !PC) return; auto* P = PC->GetGameInstance()->GetSubsystem<UFCBProgressionSubsystem>(); if (!P) return;
	StatusText->SetText(FText::FromString(FString::Printf(TEXT("Player Level %d  -  %d cards registered  -  AI: %s  -  saves automatically, fully offline"), P->PlayerLevel, P->GetAllCardIds().Num(), *UEnum::GetValueAsString(P->Difficulty))));
}
void UFCBMainMenuWidget::OnPlay() { FString Err; PC->StartMatchFromActiveDeck(Err); if (!Err.IsEmpty() && StatusText) StatusText->SetText(FText::FromString(TEXT("Cannot start: ") + Err)); }
void UFCBMainMenuWidget::OnCollection() { if (PC && PC->CollectionWidget) { PC->CollectionWidget->Refresh(); PC->CollectionWidget->SetVisibility(ESlateVisibility::Visible); SetVisibility(ESlateVisibility::Collapsed); } }
void UFCBMainMenuWidget::OnProgression() {
	auto* P = PC->GetGameInstance()->GetSubsystem<UFCBProgressionSubsystem>();
	StatusText->SetText(FText::FromString(FString::Printf(TEXT("PROGRESSION - Player Level %d, XP %d/%d. Earn card XP by playing cards in matches, then upgrade them in the Collection screen. Match wins also award player XP and sometimes a bonus card copy. Progress is saved locally after every change."), P->PlayerLevel, P->PlayerXP, FFCBBattleRules::XPForNextLevel(P->PlayerLevel))));
}
void UFCBMainMenuWidget::OnSettings() { // cycles difficulty and persists immediately (volume/effects fields live on the subsystem for a future options screen)
	auto* P = PC->GetGameInstance()->GetSubsystem<UFCBProgressionSubsystem>();
	P->Difficulty = (EFCBDifficulty)(((int32)P->Difficulty + 1) % 4); P->SaveNow(); RefreshStatus();
}
void UFCBMainMenuWidget::OnExit() { UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false); }

// ---------------- Battle ----------------
void UFCBBattleWidget::Init(AFCBPlayerController* InPC) { PC = InPC; }
void UFCBBattleWidget::NativeConstruct() {
	Super::NativeConstruct(); auto* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root")); WidgetTree->RootWidget = Box;
	Header = MakeText(this, Box, 26); PlayerInfo = MakeText(this, Box, 17); AIInfo = MakeText(this, Box, 17); Message = MakeText(this, Box, 19);
	for (int32 i = 0; i < 8; ++i) { UButton* B = MakeBtn(this, Box, TEXT("Attribute"), FName(*FString::Printf(TEXT("AttrBtn%d"), i))); AttrBtns.Add(B); AttrLabels.Add(BtnLabel(B)); }
	AttrBtns[0]->OnClicked.AddDynamic(this, &UFCBBattleWidget::OnAttr0); AttrBtns[1]->OnClicked.AddDynamic(this, &UFCBBattleWidget::OnAttr1);
	AttrBtns[2]->OnClicked.AddDynamic(this, &UFCBBattleWidget::OnAttr2); AttrBtns[3]->OnClicked.AddDynamic(this, &UFCBBattleWidget::OnAttr3);
	AttrBtns[4]->OnClicked.AddDynamic(this, &UFCBBattleWidget::OnAttr4); AttrBtns[5]->OnClicked.AddDynamic(this, &UFCBBattleWidget::OnAttr5);
	AttrBtns[6]->OnClicked.AddDynamic(this, &UFCBBattleWidget::OnAttr6); AttrBtns[7]->OnClicked.AddDynamic(this, &UFCBBattleWidget::OnAttr7);
	MakeBtn(this, Box, TEXT("Direction: GREATER"), TEXT("DirG"))->OnClicked.AddDynamic(this, &UFCBBattleWidget::OnDirGreater);
	MakeBtn(this, Box, TEXT("Direction: LESS"), TEXT("DirL"))->OnClicked.AddDynamic(this, &UFCBBattleWidget::OnDirLess);
	LockBtn = MakeBtn(this, Box, TEXT("Lock In & Reveal"), TEXT("LockBtn")); LockBtn->OnClicked.AddDynamic(this, &UFCBBattleWidget::OnLock);
	NextBtn = MakeBtn(this, Box, TEXT("Next Round"), TEXT("NextBtn")); NextBtn->OnClicked.AddDynamic(this, &UFCBBattleWidget::OnNext);
	MakeBtn(this, Box, TEXT("Abandon Match"), TEXT("QuitBtn"))->OnClicked.AddDynamic(this, &UFCBBattleWidget::OnQuit);
}
void UFCBBattleWidget::OnAttr0() { SelAttr = EFCBAttribute::Power; bAttrSet = true; } void UFCBBattleWidget::OnAttr1() { SelAttr = EFCBAttribute::Speed; bAttrSet = true; }
void UFCBBattleWidget::OnAttr2() { SelAttr = EFCBAttribute::Height; bAttrSet = true; } void UFCBBattleWidget::OnAttr3() { SelAttr = EFCBAttribute::Defense; bAttrSet = true; }
void UFCBBattleWidget::OnAttr4() { SelAttr = EFCBAttribute::Intelligence; bAttrSet = true; } void UFCBBattleWidget::OnAttr5() { SelAttr = EFCBAttribute::Stamina; bAttrSet = true; }
void UFCBBattleWidget::OnAttr6() { SelAttr = EFCBAttribute::Luck; bAttrSet = true; } void UFCBBattleWidget::OnAttr7() { SelAttr = EFCBAttribute::Age; bAttrSet = true; }
void UFCBBattleWidget::OnDirGreater() { SelDir = EFCBPrediction::Greater; bDirSet = true; } void UFCBBattleWidget::OnDirLess() { SelDir = EFCBPrediction::Less; bDirSet = true; }
void UFCBBattleWidget::PresentRound(UFCBBattleStateMachine* Battle, bool bReset) {
	if (!Battle) return; bAttrSet = false; bDirSet = false;
	auto* Prog = PC->GetGameInstance()->GetSubsystem<UFCBProgressionSubsystem>(); FFCBCardTableRow Row;
	Header->SetText(FText::FromString(FString::Printf(TEXT("Round %d   -   You %d : %d AI   -   Ties %d"), Battle->RoundNumber, Battle->PlayerScore, Battle->AIScore, Battle->Ties)));
	if (Prog->GetDefinition(Battle->GetPlayerCardId(), Row)) { const auto Eff = FFCBBattleRules::EffectiveAttributes(Row.Attributes, Prog->GetOwned(Row.CardId).Level);
		PlayerInfo->SetText(FText::FromString(FString::Printf(TEXT("YOUR CARD: %s (%s, Lv %d)\n%s"), *Row.DisplayName, *Row.Category, Prog->GetOwned(Row.CardId).Level, *AttrLine(Eff))));
		int32 i = 0; for (EFCBAttribute A : FFCBAttributeSet::All()) { if (AttrLabels.IsValidIndex(i)) AttrLabels[i]->SetText(FText::FromString(FString::Printf(TEXT("%s: %d"), *FFCBBattleRules::AttributeName(A), Eff.Get(A)))); AttrBtns[i]->SetVisibility(Battle->IsPlayerPredictor() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); ++i; } }
	AIInfo->SetText(FText::FromString(TEXT("AI CARD: hidden until the prediction is locked.")));
	Message->SetText(FText::FromString(Battle->IsPlayerPredictor() ? TEXT("Your prediction: tap an attribute, tap GREATER or LESS, then Lock In & Reveal.") : TEXT("AI predicts this round (its own card + public history only). Press Lock In & Reveal to reveal its card.")));
}
void UFCBBattleWidget::PresentResult(UFCBBattleStateMachine* Battle, const FFCBPredictionChoice& Pick, int32 PV, int32 OV) {
	auto* Prog = PC->GetGameInstance()->GetSubsystem<UFCBProgressionSubsystem>(); FName AIId; Battle->GetRevealedAICard(AIId); FFCBCardTableRow Row;
	if (Prog->GetDefinition(AIId, Row)) AIInfo->SetText(FText::FromString(FString::Printf(TEXT("AI CARD REVEALED: %s\n%s"), *Row.DisplayName, *AttrLine(Row.Attributes))));
	const EFCBRoundResult R = FFCBBattleRules::Evaluate(PV, OV, Pick.Direction);
	Message->SetText(FText::FromString(R == EFCBRoundResult::Tie ? FString::Printf(TEXT("TIE on %s (%d vs %d) - no point awarded. Press Next Round."), *FFCBBattleRules::AttributeName(Pick.Attribute), PV, OV)
		: FString::Printf(TEXT("%s wins the point on %s (%d vs %d, predicted %s). Press Next Round."), (R == EFCBRoundResult::PredictorWin) == Battle->IsPlayerPredictor() ? TEXT("YOU") : TEXT("AI"), *FFCBBattleRules::AttributeName(Pick.Attribute), PV, OV, Pick.Direction == EFCBPrediction::Greater ? TEXT("Greater") : TEXT("Less"))));
	Header->SetText(FText::FromString(FString::Printf(TEXT("Round %d   -   You %d : %d AI   -   Ties %d"), Battle->RoundNumber, Battle->PlayerScore, Battle->AIScore, Battle->Ties)));
}
void UFCBBattleWidget::PresentMatchSummary(UFCBBattleStateMachine* Battle, int32 XP, bool bLeveled) {
	const bool bWon = Battle->PlayerScore > Battle->AIScore, bDraw = Battle->PlayerScore == Battle->AIScore;
	Message->SetText(FText::FromString(FString::Printf(TEXT("%s  Final: You %d - %d AI, %d ties. +%d player XP%s. Abandon returns to the menu; start again from Play."), bWon ? TEXT("VICTORY!") : bDraw ? TEXT("DRAW") : TEXT("DEFEAT"), Battle->PlayerScore, Battle->AIScore, Battle->Ties, XP, bLeveled ? TEXT(" - LEVEL UP!") : TEXT(""))));
}
void UFCBBattleWidget::OnLock() { if (PC) PC->SubmitPrediction(SelAttr, SelDir); }
void UFCBBattleWidget::OnNext() { if (PC) PC->ContinueAfterResult(); }
void UFCBBattleWidget::OnQuit() { SetVisibility(ESlateVisibility::Collapsed); if (PC && PC->MainMenu) { PC->MainMenu->SetVisibility(ESlateVisibility::Visible); PC->MainMenu->RefreshStatus(); } }

// ---------------- Collection + Deck Builder ----------------
void UFCBCollectionWidget::Init(AFCBPlayerController* InPC, UFCBProgressionSubsystem* InProg) { PC = InPC; Prog = InProg; }
void UFCBCollectionWidget::NativeConstruct() {
	Super::NativeConstruct(); auto* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root")); WidgetTree->RootWidget = Box;
	Detail = MakeText(this, Box, 17);
	MakeBtn(this, Box, TEXT("< Prev Card"), TEXT("PrevBtn"))->OnClicked.AddDynamic(this, &UFCBCollectionWidget::OnPrevCard);
	MakeBtn(this, Box, TEXT("Next Card >"), TEXT("NextBtn"))->OnClicked.AddDynamic(this, &UFCBCollectionWidget::OnNextCard);
	MakeBtn(this, Box, TEXT("Upgrade Selected Card (spends card XP)"), TEXT("UpBtn"))->OnClicked.AddDynamic(this, &UFCBCollectionWidget::OnUpgrade);
	MakeBtn(this, Box, TEXT("Add Selected to Active Deck"), TEXT("AddBtn"))->OnClicked.AddDynamic(this, &UFCBCollectionWidget::OnAddToDeck);
	MakeBtn(this, Box, TEXT("Remove Selected from Active Deck"), TEXT("RmBtn"))->OnClicked.AddDynamic(this, &UFCBCollectionWidget::OnRemoveFromDeck);
	MakeBtn(this, Box, TEXT("Create New Deck"), TEXT("NewDeckBtn"))->OnClicked.AddDynamic(this, &UFCBCollectionWidget::OnNewDeck);
	MakeBtn(this, Box, TEXT("Validate Active Deck"), TEXT("ValBtn"))->OnClicked.AddDynamic(this, &UFCBCollectionWidget::OnValidateDeck);
	MakeBtn(this, Box, TEXT("Play with Active Deck"), TEXT("PlayBtn"))->OnClicked.AddDynamic(this, &UFCBCollectionWidget::OnPlayFromHere);
	MakeBtn(this, Box, TEXT("Back to Menu"), TEXT("BackBtn"))->OnClicked.AddDynamic(this, &UFCBCollectionWidget::OnBack);
	if (Prog) Refresh();
}
void UFCBCollectionWidget::Refresh(const FString& Search) {
	if (!Prog || !Detail) return; const TArray<FName> Ids = Prog->GetAllCardIds(); if (Ids.IsEmpty()) { Detail->SetText(FText::FromString(TEXT("No cards registered - import DT_Cards (see Docs/INSTALL_UE58.md)."))); return; }
	BrowseIndex = FMath::Clamp(BrowseIndex, 0, Ids.Num() - 1); Selected = Ids[BrowseIndex];
	FFCBCardTableRow Row; if (!Prog->GetDefinition(Selected, Row)) return; const FFCBOwnedCard O = Prog->GetOwned(Selected); const auto Eff = FFCBBattleRules::EffectiveAttributes(Row.Attributes, O.Level);
	FFCBDeck Deck; const int32 InDeck = Prog->GetActiveDeck(Deck) ? Deck.CardIds.Num() : 0;
	Detail->SetText(FText::FromString(FString::Printf(TEXT("Card %d / %d\n%s (%s, %s, Lv %d, XP %d, %d copies)\n%s\nAbility: %s\n%s\nActive deck: %d / %d cards"), BrowseIndex + 1, Ids.Num(), *Row.DisplayName, *Row.Category, *UEnum::GetValueAsString(Row.Rarity), O.Level, O.XP, O.Copies, *Row.Flavor, *Row.Ability, *AttrLine(Eff), InDeck, FFCBBattleRules::DefaultDeckSize)));
}
void UFCBCollectionWidget::OnPrevCard() { --BrowseIndex; Refresh(); }
void UFCBCollectionWidget::OnNextCard() { ++BrowseIndex; Refresh(); }
void UFCBCollectionWidget::OnUpgrade() { FString Err; if (Prog && !Selected.IsNone() && !Prog->TryUpgradeCard(Selected, Err)) UE_LOG(LogTemp, Warning, TEXT("[FCB] Upgrade: %s"), *Err); Refresh(); }
void UFCBCollectionWidget::OnAddToDeck() { FString Err; if (Prog && !Prog->AddCardToActiveDeck(Selected, Err)) UE_LOG(LogTemp, Warning, TEXT("[FCB] Deck: %s"), *Err); Refresh(); }
void UFCBCollectionWidget::OnRemoveFromDeck() { FString Err; if (Prog && !Prog->RemoveCardFromActiveDeck(Selected, Err)) UE_LOG(LogTemp, Warning, TEXT("[FCB] Deck: %s"), *Err); Refresh(); }
void UFCBCollectionWidget::OnNewDeck() { if (Prog) { const FGuid Id = Prog->CreateDeck(FString::Printf(TEXT("Deck %d"), Prog->GetDecks().Num() + 1)); Prog->SetActiveDeck(Id); } Refresh(); }
void UFCBCollectionWidget::OnValidateDeck() { TArray<FString> Err; const bool bOk = Prog && Prog->ValidateActiveDeck(Err); if (Detail) Detail->SetText(FText::FromString(bOk ? TEXT("Active deck is VALID (30 cards, copies owned) - ready to play.") : FString::Printf(TEXT("Deck invalid: %s"), Err.IsEmpty() ? TEXT("unknown") : *Err[0]))); }
void UFCBCollectionWidget::OnPlayFromHere() { SetVisibility(ESlateVisibility::Collapsed); FString Err; if (PC) PC->StartMatchFromActiveDeck(Err); }
void UFCBCollectionWidget::OnBack() { SetVisibility(ESlateVisibility::Collapsed); if (PC && PC->MainMenu) PC->MainMenu->SetVisibility(ESlateVisibility::Visible); }
