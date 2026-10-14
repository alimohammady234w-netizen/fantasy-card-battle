#pragma once
#include "CoreMinimal.h"
#include "FCBTypes.h"
/** AI decision-making. Inputs are ONLY: its own card's (effective) attributes, publicly revealed opponent
 *  attribute history, difficulty, and an RNG stream. Hidden/future cards are never passed in - it cannot cheat. */
struct FANTASYCARDBATTLE_API FFCBAISystem {
	static FFCBPredictionChoice Choose(const FFCBAttributeSet& Own, const TMap<EFCBAttribute, TArray<int32>>& PublicOpponentHistory, EFCBDifficulty Difficulty, FRandomStream& Rng);
};
