#include "FCBBattleRules.h"

EFCBRoundResult FFCBBattleRules::Evaluate(int32 PredictorValue, int32 OtherValue, EFCBPrediction Prediction) {
	if (PredictorValue == OtherValue) return EFCBRoundResult::Tie;
	const bool bGreater = PredictorValue > OtherValue;
	return ((Prediction == EFCBPrediction::Greater) == bGreater) ? EFCBRoundResult::PredictorWin : EFCBRoundResult::PredictorLoss;
}
FFCBAttributeSet FFCBBattleRules::EffectiveAttributes(const FFCBAttributeSet& Base, int32 CardLevel) {
	FFCBAttributeSet Out = Base; const int32 Bonus = (FMath::Clamp(CardLevel, 1, MaxCardLevel) - 1) * BonusPerLevel;
	Out.Power = FMath::Min(100, Out.Power + Bonus); Out.Speed = FMath::Min(100, Out.Speed + Bonus);
	Out.Height = FMath::Min(100, Out.Height + Bonus); Out.Defense = FMath::Min(100, Out.Defense + Bonus);
	Out.Intelligence = FMath::Min(100, Out.Intelligence + Bonus); Out.Stamina = FMath::Min(100, Out.Stamina + Bonus);
	Out.Luck = FMath::Min(100, Out.Luck + Bonus); Out.Age = FMath::Min(100, Out.Age + Bonus); return Out;
}
bool FFCBBattleRules::ValidateDeck(const TArray<FName>& Deck, const TMap<FName, int32>& OwnedCopies, int32 RequiredSize, TArray<FString>& OutErrors) {
	OutErrors.Reset();
	if (Deck.Num() != RequiredSize) OutErrors.Add(FString::Printf(TEXT("Deck must contain exactly %d cards (has %d)."), RequiredSize, Deck.Num()));
	TMap<FName, int32> Counts; for (FName Id : Deck) Counts.FindOrAdd(Id)++;
	for (const auto& Pair : Counts) { const int32 Owned = OwnedCopies.FindRef(Pair.Key); if (Pair.Value > Owned) OutErrors.Add(FString::Printf(TEXT("Not enough owned copies of %s (uses %d, owns %d)."), *Pair.Key.ToString(), Pair.Value, Owned)); }
	return OutErrors.IsEmpty();
}
int32 FFCBBattleRules::ApplyXP(int32& Level, int32& XP, int32 Gained) {
	int32 Levels = 0; XP = FMath::Max(0, XP + Gained);
	while (Level < 99 && XP >= XPForNextLevel(Level)) { XP -= XPForNextLevel(Level); ++Level; ++Levels; } return Levels;
}
FString FFCBBattleRules::AttributeName(EFCBAttribute A) {
	switch (A) { case EFCBAttribute::Power: return TEXT("Power"); case EFCBAttribute::Speed: return TEXT("Speed"); case EFCBAttribute::Height: return TEXT("Height"); case EFCBAttribute::Defense: return TEXT("Defense"); case EFCBAttribute::Intelligence: return TEXT("Intelligence"); case EFCBAttribute::Stamina: return TEXT("Stamina"); case EFCBAttribute::Luck: return TEXT("Luck"); default: return TEXT("Age"); }
}
