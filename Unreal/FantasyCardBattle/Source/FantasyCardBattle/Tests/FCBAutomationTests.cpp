#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../FCBBattleRules.h"
#include "../FCBAISystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFCBRulesTest, "FantasyCardBattle.Rules.Comparison", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFCBRulesTest::RunTest(const FString& Params) {
	TestEqual(TEXT("Greater wins when higher"), FFCBBattleRules::Evaluate(80, 50, EFCBPrediction::Greater), EFCBRoundResult::PredictorWin);
	TestEqual(TEXT("Greater fails when lower"), FFCBBattleRules::Evaluate(40, 50, EFCBPrediction::Greater), EFCBRoundResult::PredictorLoss);
	TestEqual(TEXT("Less wins when lower"), FFCBBattleRules::Evaluate(40, 50, EFCBPrediction::Less), EFCBRoundResult::PredictorWin);
	TestEqual(TEXT("Less fails when higher"), FFCBBattleRules::Evaluate(80, 50, EFCBPrediction::Less), EFCBRoundResult::PredictorLoss);
	TestEqual(TEXT("Equal ties (Greater)"), FFCBBattleRules::Evaluate(50, 50, EFCBPrediction::Greater), EFCBRoundResult::Tie);
	TestEqual(TEXT("Equal ties (Less)"), FFCBBattleRules::Evaluate(50, 50, EFCBPrediction::Less), EFCBRoundResult::Tie);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFCBDeckTest, "FantasyCardBattle.Rules.DeckValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFCBDeckTest::RunTest(const FString& Params) {
	TArray<FName> Deck; TMap<FName, int32> Owned; TArray<FString> Err;
	for (int32 i = 0; i < 31; ++i) { FName Id(*FString::Printf(TEXT("FCB-%03d"), i + 1)); Owned.Add(Id, 1); if (i < 30) Deck.Add(Id); }
	TestTrue(TEXT("30 owned cards valid"), FFCBBattleRules::ValidateDeck(Deck, Owned, 30, Err));
	Deck.Pop(); TestFalse(TEXT("29 cards invalid"), FFCBBattleRules::ValidateDeck(Deck, Owned, 30, Err));
	Deck.Add(Deck[0]); Deck.Add(FName(TEXT("FCB-031"))); TestFalse(TEXT("duplicate beyond copies invalid"), FFCBBattleRules::ValidateDeck(Deck, Owned, 30, Err));
	int32 L = 1, X = 0; TestEqual(TEXT("level up at curve"), FFCBBattleRules::ApplyXP(L, X, FFCBBattleRules::XPForNextLevel(1) + 5), 1); TestEqual(TEXT("remainder banked"), X, 5);
	const FFCBAttributeSet Base; const FFCBAttributeSet Up = FFCBBattleRules::EffectiveAttributes(Base, 3);
	TestEqual(TEXT("upgrade +4, base unchanged"), Up.Power, 54); TestEqual(TEXT("base intact"), Base.Power, 50);
	FRandomStream Rng(42); FFCBAttributeSet Strong; Strong.Power = 95;
	TMap<EFCBAttribute, TArray<int32>> Hist; Hist.Add(EFCBAttribute::Power, { 10, 20, 30 });
	const FFCBPredictionChoice Pick = FFCBAISystem::Choose(Strong, Hist, EFCBDifficulty::Expert, Rng); // AI sees own card + public history only
	TestTrue(TEXT("expert picks its strength"), Pick.Attribute == EFCBAttribute::Power && Pick.Direction == EFCBPrediction::Greater);
	return true;
}
#endif
