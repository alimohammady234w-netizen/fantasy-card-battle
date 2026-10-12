#pragma once
#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "FCBTypes.generated.h"

UENUM(BlueprintType) enum class EFCBAttribute : uint8 { Power, Speed, Height, Defense, Intelligence, Stamina, Luck, Age };
UENUM(BlueprintType) enum class EFCBRarity : uint8 { Common, Uncommon, Rare, Epic, Legendary, Mythic };
UENUM(BlueprintType) enum class EFCBPrediction : uint8 { Greater, Less };
UENUM(BlueprintType) enum class EFCBRoundResult : uint8 { PredictorWin, PredictorLoss, Tie };
UENUM(BlueprintType) enum class EFCBDifficulty : uint8 { Easy, Normal, Hard, Expert };
UENUM(BlueprintType) enum class EFCBBattlePhase : uint8 { Idle, Choosing, Revealing, Resolved, MatchOver };

USTRUCT(BlueprintType) struct FFCBAttributeSet {
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Card", meta=(ClampMin="1", ClampMax="100")) int32 Power = 50;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Card", meta=(ClampMin="1", ClampMax="100")) int32 Speed = 50;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Card", meta=(ClampMin="1", ClampMax="100")) int32 Height = 50;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Card", meta=(ClampMin="1", ClampMax="100")) int32 Defense = 50;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Card", meta=(ClampMin="1", ClampMax="100")) int32 Intelligence = 50;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Card", meta=(ClampMin="1", ClampMax="100")) int32 Stamina = 50;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Card", meta=(ClampMin="1", ClampMax="100")) int32 Luck = 50;
	/** Age is a normal comparable numeric attribute (1-100 scale); never silently reversed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Card", meta=(ClampMin="1", ClampMax="100")) int32 Age = 50;
	int32 Get(EFCBAttribute A) const { switch (A) { case EFCBAttribute::Power: return Power; case EFCBAttribute::Speed: return Speed; case EFCBAttribute::Height: return Height; case EFCBAttribute::Defense: return Defense; case EFCBAttribute::Intelligence: return Intelligence; case EFCBAttribute::Stamina: return Stamina; case EFCBAttribute::Luck: return Luck; default: return Age; } }
	static TArray<EFCBAttribute> All() { return { EFCBAttribute::Power, EFCBAttribute::Speed, EFCBAttribute::Height, EFCBAttribute::Defense, EFCBAttribute::Intelligence, EFCBAttribute::Stamina, EFCBAttribute::Luck, EFCBAttribute::Age }; }
	bool IsValid() const { for (EFCBAttribute A : All()) { const int32 V = Get(A); if (V < 1 || V > 100) return false; } return true; }
};

/** Row of DT_Cards imported from Data/cards.csv (repo root). Add cards by adding rows - no new C++ classes. */
USTRUCT(BlueprintType) struct FFCBCardTableRow : public FTableRowBase {
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CardId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Category;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EFCBRarity Rarity = EFCBRarity::Common;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Ability;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Flavor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FFCBAttributeSet Attributes;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString ArtworkFile;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<class UTexture2D> Artwork;
};

USTRUCT(BlueprintType) struct FFCBOwnedCard { // mutable per-player progression; definitions stay immutable
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, SaveGame) FName DefinitionId;
	UPROPERTY(BlueprintReadWrite, SaveGame) int32 Copies = 1;
	UPROPERTY(BlueprintReadWrite, SaveGame) int32 Level = 1;
	UPROPERTY(BlueprintReadWrite, SaveGame) int32 XP = 0;
};
USTRUCT(BlueprintType) struct FFCBDeck {
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, SaveGame) FGuid DeckId;
	UPROPERTY(BlueprintReadWrite, SaveGame) FString Name;
	UPROPERTY(BlueprintReadWrite, SaveGame) TArray<FName> CardIds;
};
USTRUCT(BlueprintType) struct FFCBPredictionChoice {
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite) EFCBAttribute Attribute = EFCBAttribute::Power;
	UPROPERTY(BlueprintReadWrite) EFCBPrediction Direction = EFCBPrediction::Greater;
};
