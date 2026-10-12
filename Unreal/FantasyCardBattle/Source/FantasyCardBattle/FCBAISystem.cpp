#include "FCBAISystem.h"
FFCBPredictionChoice FFCBAISystem::Choose(const FFCBAttributeSet& Own, const TMap<EFCBAttribute, TArray<int32>>& History, EFCBDifficulty Difficulty, FRandomStream& Rng) {
	FFCBPredictionChoice Best; float BestP = -1.f;
	if (Difficulty == EFCBDifficulty::Easy) { Best.Attribute = FFCBAttributeSet::All()[Rng.RandRange(0, 7)]; Best.Direction = Rng.FRand() < 0.5f ? EFCBPrediction::Greater : EFCBPrediction::Less; return Best; }
	for (EFCBAttribute A : FFCBAttributeSet::All()) {
		const int32 V = Own.Get(A); const TArray<int32>* Seen = History.Find(A); float PG;
		if (!Seen || Seen->IsEmpty()) PG = V / 100.f;
		else if (Difficulty == EFCBDifficulty::Normal) { float Sum = 0; for (int32 S : *Seen) Sum += S; PG = V > (Sum / Seen->Num()) ? 0.72f : 0.28f; }
		else { int32 Beat = 0; for (int32 S : *Seen) if (V > S) ++Beat; PG = (float)Beat / Seen->Num(); if (Difficulty == EFCBDifficulty::Expert && Seen->Num() >= 4) PG = (Beat + V / 100.f) / (Seen->Num() + 1); }
		const float P = FMath::Max(PG, 1.f - PG);
		if (P > BestP) { BestP = P; Best.Attribute = A; Best.Direction = PG >= 0.5f ? EFCBPrediction::Greater : EFCBPrediction::Less; }
	}
	if ((Difficulty == EFCBDifficulty::Normal && Rng.FRand() < 0.18f) || (Difficulty == EFCBDifficulty::Hard && Rng.FRand() < 0.07f))
		Best.Direction = Best.Direction == EFCBPrediction::Greater ? EFCBPrediction::Less : EFCBPrediction::Greater; // calibrated human-like error
	return Best;
}
