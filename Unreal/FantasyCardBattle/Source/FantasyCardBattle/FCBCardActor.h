#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FCBTypes.h"
#include "FCBCardActor.generated.h"

/** Reusable 3D card presentation: front (art + attributes rendered to a dynamic texture), back, flip, hover/selection/winner states.
 *  One actor class serves every card definition - visuals are data-driven, no per-card classes. */
UCLASS() class FANTASYCARDBATTLE_API AFCBCardActor : public AActor {
	GENERATED_BODY()
public:
	AFCBCardActor();
	virtual void Tick(float DeltaSeconds) override;
	UFUNCTION(BlueprintCallable) void ShowCard(const FFCBCardTableRow& Row, const FFCBAttributeSet& Effective, bool bFaceUp);
	UFUNCTION(BlueprintCallable) void ShowBack();
	UFUNCTION(BlueprintCallable) void FlipToFront(float Duration = 0.6f);
	UFUNCTION(BlueprintCallable) void SetHighlighted(bool bOn);
	UFUNCTION(BlueprintCallable) void PlayResultFeedback(bool bWinner, bool bTie);
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UStaticMeshComponent> CardMesh;
	UPROPERTY(EditAnywhere, Category="Card") TObjectPtr<class UMaterialInterface> FallbackMaterial;
private:
	UPROPERTY() TObjectPtr<class UTexture> FrontTexture;
	float FlipT = 1.f, FlipDuration = 0.6f; bool bFlipping = false; float BaseZ = 0.f; float FeedbackT = -1.f; bool bFeedbackWinner = false;
	class UTexture* RenderFrontTexture(const FFCBCardTableRow& Row, const FFCBAttributeSet& Attrs);
};
