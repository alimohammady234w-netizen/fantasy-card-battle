// Fantasy Card Battle - Card stats struct (pure data, Stage 2)

#pragma once

#include "CoreMinimal.h"
#include "CardStats.generated.h"

/**
 * Pure-data stats block shared by every card definition.
 *
 * - DATA ONLY: no comparison, battle, or gameplay logic lives here
 *   (battle logic belongs to a future stage).
 * - Editable inside Data Assets, Blueprint readable AND writable.
 * - Extend by APPENDING new UPROPERTYs; never reorder or remove existing
 *   ones (Data Asset serialization stores property values by name, but
 *   keeping the layout stable protects downstream tooling and docs).
 */
USTRUCT(BlueprintType)
struct CARDGAME_API FCardStats
{
	GENERATED_BODY()

	/** Offensive strength of the card. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CardStats", meta = (ClampMin = "0"))
	int32 Power = 0;

	/** Movement / reaction speed of the card. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CardStats", meta = (ClampMin = "0"))
	int32 Speed = 0;

	/** Height of the creature / unit (game units used by card comparisons). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CardStats", meta = (ClampMin = "0"))
	int32 Height = 0;

	/** Damage mitigation potential. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CardStats", meta = (ClampMin = "0"))
	int32 Defense = 0;

	/** Smartness / tactical awareness. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CardStats", meta = (ClampMin = "0"))
	int32 Intelligence = 0;

	/** How long the card can keep performing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CardStats", meta = (ClampMin = "0"))
	int32 Stamina = 0;

	/** Fortune factor used by future systems (still pure data). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CardStats", meta = (ClampMin = "0"))
	int32 Luck = 0;

	/** Age of the card's subject (years / era value). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CardStats", meta = (ClampMin = "0"))
	int32 Age = 0;
};
