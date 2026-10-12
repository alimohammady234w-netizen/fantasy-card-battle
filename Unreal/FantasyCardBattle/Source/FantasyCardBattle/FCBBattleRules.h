#pragma once
#include "CoreMinimal.h"
#include "FCBTypes.h"

/** Pure, presentation-independent battle/deck/progression rules. Mirrors js/rules.js (covered by Tests). */
struct FANTASYCARDBATTLE_API FFCBBattleRules {
	static constexpr int32 DefaultDeckSize = 30;
	static constexpr int32 MaxCardLevel = 10;
	static constexpr int32 BonusPerLevel = 2;
	/** Greater = predictor's value strictly greater; Less = strictly lower; equal is always a Tie (no points). */
	static EFCBRoundResult Evaluate(int32 PredictorValue, int32 OtherValue, EFCBPrediction Prediction);
	static FFCBAttributeSet EffectiveAttributes(const FFCBAttributeSet& Base, int32 CardLevel); // definition never mutated
	static bool ValidateDeck(const TArray<FName>& Deck, const TMap<FName, int32>& OwnedCopies, int32 RequiredSize, TArray<FString>& OutErrors);
	static int32 XPForNextLevel(int32 Level) { return FMath::RoundToInt(100.f * FMath::Pow((float)FMath::Max(1, Level), 1.5f)); }
	static int32 ApplyXP(int32& InOutLevel, int32& InOutXP, int32 Gained); // returns levels gained
	static FString AttributeName(EFCBAttribute A);
};
