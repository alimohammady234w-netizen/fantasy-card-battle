#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FCBTypes.h"
#include "FCBCardDefinition.generated.h"

/** Optional Primary Data Asset authoring path for a card. The shipped 50 cards live in DT_Cards (from Data/cards.csv);
 *  either source registers into the same runtime registry, so adding cards never requires a new C++ class. */
UCLASS(BlueprintType) class FANTASYCARDBATTLE_API UFCBCardDefinition : public UPrimaryDataAsset {
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Card") FName CardId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Card") FString DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Card") FString Category;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Card") FString Flavor;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Card") FString Ability;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Card") EFCBRarity Rarity = EFCBRarity::Common;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Card") FFCBAttributeSet Attributes;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Card") TSoftObjectPtr<UTexture2D> Artwork;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Card") TSoftObjectPtr<class USoundBase> AbilitySound;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("Card"), CardId); }
	bool IsValidDefinition(FString& OutError) const;
};
