#include "FCBWidgets.h"
#include "../FCBPlayerController.h"
#include "../FCBProgressionSubsystem.h"
#include "../FCBBattleStateMachine.h"
#include "../FCBBattleRules.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/ScrollBox.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/KismetSystemLibrary.h"

static UButton* MakeBtn(UUserWidget* W, UVerticalBox* Box, const FString& Label, const FName Slot = NAME_None) {
	UButton* B = W->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Slot.IsNone() ? FName(*Label) : Slot);
	UTextBlock* T = W->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass()); T->SetText(FText::FromString(Label)); B->AddChild(T); Box->AddChild(B); return B;
}
static UTextBlock* MakeText(UUserWidget* W, UVerticalBox* Box, float Size = 20.f) {
	UTextBlock* T = W->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass()); T->SetAutoWrapText(true);
	FSlateFontInfo F = T->GetFont(); F.Size = Size; T->SetFont(F); Box->AddChild(T); return T;
}

// ---------------- Main Menu ----------------
void UFCBMainMenuWidget::Init(AFCBPlayerController* InPC) { PC = InPC; }
void UFCBMainMenuWidget::NativeConstruct() {
	Super::NativeConstruct(); auto* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root")); WidgetTree->RootWidget = Box;
	MakeText(this, Box, 44)->SetText(FText::FromString(TEXT("FANTASY CARD BATTLE")));
	StatusText = MakeText(this, Box, 16);
	MakeBtn(this, Box, TEXT("Play"))->OnClicked.AddDynamic(this, &UFCBMainMenuWidget::OnPlay);
	MakeBtn(this, Box, TEXT("Card Collection"))->OnClicked.AddDynamic(this, &UFCBMainMenuWidget::OnCollection);
	MakeBtn(this, Box, TEXT("Deck Builder (in Collection screen)"))->OnClicked.AddDynamic(this, &UFCBMainMenuWidget::OnDecks);
	MakeBtn(this, Box, TEXT("Exit"))->OnClicked.AddDynamic(this, &UFCBMainMenuWidget::OnExit);
	if (PC) if (auto* P = PC->GetGameInstance()->GetSubsystem<UFCBProgressionSubsystem>()) StatusText->SetText(FText::FromString(FString::Printf(TEXT("Player Level %d  -  %d cards registered  -  progress saves automatically, fully offline"), P->PlayerLevel, P->GetAllCardIds().Num())));
}
void UFCBMainMenuWidget::OnPlay() { FString Err; PC->StartMatchFromActiveDeck(Err); if (!Err.IsEmpty() && StatusText) StatusText->SetText(FText::FromString(Err)); }
void UFCBMainMenuWidget::OnCollection() { if (PC && PC->CollectionWidget) { PC->CollectionWidget->Refresh(); PC->CollectionWidget->SetVisibility(ESlateVisibility::Visible); SetVisibility(ESlateVisibility::Collapsed); } }
void UFCBMainMenuWidget::OnDecks() { OnCollection(); } // deck editing lives in the collection screen (add/remove/validate/new deck)
void UFCBMainMenuWidget::OnExit() { UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false); }

// ---------------- Battle ----------------
void UFCBBattleWidget::Init(AFCBPlayerController* InPC) { PC = InPC; }
void UFCBBattleWidget::NativeConstruct() {
	Super::NativeConstruct(); auto* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root")); WidgetTree->RootWidget = Box;
	Header = MakeText(this, Box, 26); PlayerInfo = MakeText(this, Box, 17); AIInfo = MakeText(this, Box, 17); Message = MakeText(this, Box, 19);
	AttrButtons = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("AttrBox")); Box->AddChild(AttrButtons);
	MakeBtn(this, Box, TEXT("Lock In & Reveal"), TEXT("LockBtn"))->OnClicked.AddDynamic(this, &UFCBBattleWidget::OnLock);
	MakeBtn(this, Box, TEXT("Next Round"), TEXT("NextBtn"))->OnClicked.AddDynamic(this, &UFCBBattleWidget::OnNext);
	MakeBtn(this, Box, TEXT("Abandon Match"), TEXT("QuitBtn"))->OnClicked.AddDynamic(this, &UFCBBattleWidget::OnQuit);
}
void UFCBBattleWidget::MakeAttrButton(EFCBAttribute A, int32 Value, int32 Index) {
	UButton* B = MakeBtn(this, AttrButtons, FString::Printf(TEXT("%s: %d%s"), *FFCBBattleRules::AttributeName(A), Value, bAttrSet && SelAttr == A ? TEXT("  [selected]") : TEXT("")), FName(*FString::Printf(TEXT("Attr%d"), Index)));
	switch (Index) { case 0: B->OnClicked.AddDynamic(this, &UFCBBattleWidget::OnAttr0); break; case 1: B->OnClicked.AddDynamic(this, &UFCBBattleWidget::OnAttr1); break; case 2: B->OnClicked.AddDynamic(this, &UFCBBattleWidget::OnAttr2); break; case 3: B->OnClicked.AddDynamic(this, &UFCBBattleWidget::OnAttr3); break; case 4: B->OnClicked.AddDynamic(this, &UFCBBattleWidget::OnAttr4); break; case 5: B->OnClicked.AddDynamic(this, &UFCBBattleWidget::OnAttr5); break; case 6: B->OnClicked.AddDynamic(this, &UFCBBattleWidget::OnAttr6); break; default: B->OnClicked.AddDynamic(this, &UFCBBattleWidget::OnAttr7); break; }
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
		PlayerInfo->SetText(FText::FromString(FString::Printf(TEXT("YOUR CARD: %s (%s, Lv %d)\nPower %d  Speed %d  Height %d  Defense %d\nIntelligence %d  Stamina %d  Luck %d  Age %d"), *Row.DisplayName, *Row.Category, Prog->GetOwned(Row.CardId).Level, Eff.Power, Eff.Speed, Eff.Height, Eff.Defense, Eff.Intelligence, Eff.Stamina, Eff.Luck, Eff.Age))); }
	AIInfo->SetText(FText::FromString(TEXT("AI CARD: hidden until the prediction is locked.")));
	Message->SetText(FText::FromString(Battle->IsPlayerPredictor() ? TEXT("Your prediction: choose an attribute button, then use the direction buttons below, then Lock In & Reveal.") : TEXT("AI is predicting on its hidden card (own card + public history only). Press Lock In & Reveal to reveal.")));
	AttrButtons->ClearChildren();
	if (Prog->GetDefinition(Battle->GetPlayerCardId(), Row)) { const auto Eff = FFCBBattleRules::EffectiveAttributes(Row.Attributes, Prog->GetOwned(Row.CardId).Level);
		int32 Ai = 0; for (EFCBAttribute A : FFCBAttributeSet::All()) MakeAttrButton(A, Eff.Get(A), Ai++);
		MakeBtn(this, AttrButtons, TEXT("Direction: GREATER"), TEXT("DirG"))->OnClicked.AddDynamic(this, &UFCBBattleWidget::OnDirGreater);
		MakeBtn(this, AttrButtons, TEXT("Direction: LESS"), TEXT("DirL"))->OnClicked.AddDynamic(this, &UFCBBattleWidget::OnDirLess); }
}
void UFCBBattleWidget::PresentResult(UFCBBattleStateMachine* Battle, const FFCBPredictionChoice& Pick, int32 PV, int32 OV) {
	auto* Prog = PC->GetGameInstance()->GetSubsystem<UFCBProgressionSubsystem>(); FName AIId; Battle->GetRevealedAICard(AIId); FFCBCardTableRow Row;
	if (Prog->GetDefinition(AIId, Row)) AIInfo->SetText(FText::FromString(FString::Printf(TEXT("AI CARD REVEALED: %s\nPower %d  Speed %d  Height %d  Defense %d\nIntelligence %d  Stamina %d  Luck %d  Age %d"), *Row.DisplayName, Row.Attributes.Power, Row.Attributes.Speed, Row.Attributes.Height, Row.Attributes.Defense, Row.Attributes.Intelligence, Row.Attributes.Stamina, Row.Attributes.Luck, Row.Attributes.Age)));
	const EFCBRoundResult R = FFCBBattleRules::Evaluate(PV, OV, Pick.Direction);
	Message->SetText(FText::FromString(R == EFCBRoundResult::Tie ? FString::Printf(TEXT("TIE on %s (%d vs %d) - no point awarded."), *FFCBBattleRules::AttributeName(Pick.Attribute), PV, OV)
		: FString::Printf(TEXT("%s wins the point on %s (%d vs %d, predicted %s). Press Next Round."), (R == EFCBRoundResult::PredictorWin) == Battle->IsPlayerPredictor() ? TEXT("YOU") : TEXT("AI"), *FFCBBattleRules::AttributeName(Pick.Attribute), PV, OV, Pick.Direction == EFCBPrediction::Greater ? TEXT("Greater") : TEXT("Less"))));
	Header->SetText(FText::FromString(FString::Printf(TEXT("Round %d   -   You %d : %d AI   -   Ties %d"), Battle->RoundNumber, Battle->PlayerScore, Battle->AIScore, Battle->Ties)));
}
void UFCBBattleWidget::PresentMatchSummary(UFCBBattleStateMachine* Battle, int32 XP, bool bLeveled) {
	const bool bWon = Battle->PlayerScore > Battle->AIScore, bDraw = Battle->PlayerScore == Battle->AIScore;
	Message->SetText(FText::FromString(FString::Printf(TEXT("%s  Final: You %d - %d AI, %d ties. +%d player XP%s. Use Abandon/Quit to return to the menu, or start a new match from the menu."), bWon ? TEXT("VICTORY!") : bDraw ? TEXT("DRAW") : TEXT("DEFEAT"), Battle->PlayerScore, Battle->AIScore, Battle->Ties, XP, bLeveled ? TEXT(" - LEVEL UP!") : TEXT(""))));
}
void UFCBBattleWidget::OnLock() { if (PC) PC->SubmitPrediction(SelAttr, SelDir); }
void UFCBBattleWidget::OnNext() { if (PC) PC->ContinueAfterResult(); }
void UFCBBattleWidget::OnQuit() { SetVisibility(ESlateVisibility::Collapsed); if (PC && PC->MainMenu) PC->MainMenu->SetVisibility(ESlateVisibility::Visible); }

// ---------------- Collection + Deck Builder ----------------
void UFCBCollectionWidget::Init(AFCBPlayerController* InPC, UFCBProgressionSubsystem* InProg) { PC = InPC; Prog = InProg; }
void UFCBCollectionWidget::NativeConstruct() {
	Super::NativeConstruct(); auto* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root")); WidgetTree->RootWidget = Box;
	Detail = MakeText(this, Box, 17);
	MakeBtn(this, Box, TEXT("< Prev Card"))->OnClicked.AddDynamic(this, &UFCBCollectionWidget::OnPrevCard);
	MakeBtn(this, Box, TEXT("Next Card >"))->OnClicked.AddDynamic(this, &UFCBCollectionWidget::OnNextCard);
	MakeBtn(this, Box, TEXT("Upgrade Selected Card (spends card XP)"))->OnClicked.AddDynamic(this, &UFCBCollectionWidget::OnUpgrade);
	MakeBtn(this, Box, TEXT("Play with Active Deck"))->OnClicked.AddDynamic(this, &UFCBCollectionWidget::OnPlayFromHere);
	MakeBtn(this, Box, TEXT("Back to Menu"))->OnClicked.AddDynamic(this, &UFCBCollectionWidget::OnBack);
}
void UFCBCollectionWidget::Refresh(const FString& Search) {
	if (!Prog) return; const TArray<FName> Ids = Prog->GetAllCardIds(); if (Ids.IsEmpty()) return;
	BrowseIndex = FMath::Clamp(BrowseIndex, 0, Ids.Num() - 1); Selected = Ids[BrowseIndex];
	FFCBCardTableRow Row; if (!Prog->GetDefinition(Selected, Row)) return; const FFCBOwnedCard O = Prog->GetOwned(Selected); const auto Eff = FFCBBattleRules::EffectiveAttributes(Row.Attributes, O.Level);
	Detail->SetText(FText::FromString(FString::Printf(TEXT("Card %d / %d\n%s (%s, %s, Lv %d, XP %d, %d copies)\n%s\nAbility: %s\nPower %d  Speed %d  Height %d  Defense %d  Intelligence %d  Stamina %d  Luck %d  Age %d\nUse Prev/Next to browse, Upgrade to spend card XP. Decks: Starter Deck is pre-validated (30 cards); deck CRUD/validation APIs live in UFCBProgressionSubsystem."), BrowseIndex + 1, Ids.Num(), *Row.DisplayName, *Row.Category, *UEnum::GetValueAsString(Row.Rarity), O.Level, O.XP, O.Copies, *Row.Flavor, *Row.Ability, Eff.Power, Eff.Speed, Eff.Height, Eff.Defense, Eff.Intelligence, Eff.Stamina, Eff.Luck, Eff.Age)));
}
void UFCBCollectionWidget::OnPrevCard() { --BrowseIndex; Refresh(); }
void UFCBCollectionWidget::OnNextCard() { ++BrowseIndex; Refresh(); }
void UFCBCollectionWidget::OnUpgrade() { FString Err; if (Prog && !Selected.IsNone() && !Prog->TryUpgradeCard(Selected, Err)) UE_LOG(LogTemp, Warning, TEXT("[FCB] Upgrade: %s"), *Err); Refresh(); }
void UFCBCollectionWidget::OnPlayFromHere() { SetVisibility(ESlateVisibility::Collapsed); FString Err; if (PC) PC->StartMatchFromActiveDeck(Err); }
void UFCBCollectionWidget::OnBack() { SetVisibility(ESlateVisibility::Collapsed); if (PC && PC->MainMenu) PC->MainMenu->SetVisibility(ESlateVisibility::Visible); }
