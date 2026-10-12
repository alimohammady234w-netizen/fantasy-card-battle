#include "FCBBattleStateMachine.h"
#include "FCBBattleRules.h"
#include "FCBAISystem.h"

void UFCBBattleStateMachine::StartMatch(const TArray<FName>& PlayerDeck, const TArray<FName>& AIDeck, int32 Seed) {
	Rng.Initialize(Seed); MatchId = FGuid::NewGuid().ToString();
	PlayerDraw = PlayerDeck; AIDraw = AIDeck;
	for (int32 i = PlayerDraw.Num() - 1; i > 0; --i) PlayerDraw.Swap(i, Rng.RandRange(0, i));
	for (int32 i = AIDraw.Num() - 1; i > 0; --i) AIDraw.Swap(i, Rng.RandRange(0, i));
	PlayerScore = AIScore = Ties = RoundNumber = 0; PublicHistory.Reset(); Phase = EFCBBattlePhase::Idle;
}
FFCBAttributeSet UFCBBattleStateMachine::AttrsOf(FName Id, bool bPlayer) const {
	FFCBCardTableRow Row; if (Lookup && Lookup(Id, Row)) return FFCBBattleRules::EffectiveAttributes(Row.Attributes, (bPlayer && LevelLookup) ? LevelLookup(Id) : 1);
	return FFCBAttributeSet();
}
bool UFCBBattleStateMachine::BeginNextRound(FName& OutPlayer, FName& OutAIHidden) {
	if (PlayerDraw.IsEmpty() || AIDraw.IsEmpty()) { Phase = EFCBBattlePhase::MatchOver; OnMatchFinished.Broadcast(); return false; }
	++RoundNumber; bResolved = false; bPlayerPredictor = (RoundNumber % 2 == 1);
	PlayerCardId = PlayerDraw.Pop(); AICardId = AIDraw.Pop(); OutPlayer = PlayerCardId; OutAIHidden = AICardId;
	Phase = EFCBBattlePhase::Choosing; return true;
}
bool UFCBBattleStateMachine::LockPlayerPrediction(const FFCBPredictionChoice& Pick) {
	if (Phase != EFCBBattlePhase::Choosing || !bPlayerPredictor) return false; LockedPick = Pick; Phase = EFCBBattlePhase::Revealing; return true;
}
bool UFCBBattleStateMachine::LockAIPrediction() {
	if (Phase != EFCBBattlePhase::Choosing || bPlayerPredictor) return false;
	LockedPick = FFCBAISystem::Choose(AttrsOf(AICardId, false), PublicHistory, Difficulty, Rng); // no hidden player data passed
	Phase = EFCBBattlePhase::Revealing; return true;
}
bool UFCBBattleStateMachine::RevealAndResolve(FFCBPredictionChoice& OutPick, int32& OutPredictorValue, int32& OutOtherValue) {
	if (Phase != EFCBBattlePhase::Revealing || bResolved) return false; // resolve exactly once
	bResolved = true; OutPick = LockedPick;
	const FFCBAttributeSet PA = AttrsOf(PlayerCardId, true), AA = AttrsOf(AICardId, false);
	OutPredictorValue = bPlayerPredictor ? PA.Get(LockedPick.Attribute) : AA.Get(LockedPick.Attribute);
	OutOtherValue = bPlayerPredictor ? AA.Get(LockedPick.Attribute) : PA.Get(LockedPick.Attribute);
	const EFCBRoundResult Result = FFCBBattleRules::Evaluate(OutPredictorValue, OutOtherValue, LockedPick.Direction);
	if (Result == EFCBRoundResult::Tie) ++Ties; else if ((Result == EFCBRoundResult::PredictorWin) == bPlayerPredictor) ++PlayerScore; else ++AIScore;
	for (EFCBAttribute A : FFCBAttributeSet::All()) PublicHistory.FindOrAdd(A).Add(PA.Get(A)); // player card becomes public only after reveal
	Phase = EFCBBattlePhase::Resolved; OnRoundResolved.Broadcast(Result, LockedPick); return true;
}
