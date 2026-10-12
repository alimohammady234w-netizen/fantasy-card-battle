#include "FCBCardActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "TextureResource.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "FCBBattleRules.h"

AFCBCardActor::AFCBCardActor() {
	PrimaryActorTick.bCanEverTick = true; // only animates while FlipT/FeedbackT active; idle cost is two float checks
	CardMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CardMesh")); RootComponent = CardMesh;
	if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"))) { CardMesh->SetStaticMesh(Cube); CardMesh->SetWorldScale3D(FVector(2.15f, 0.06f, 3.15f)); }
}
UTexture* AFCBCardActor::RenderFrontTexture(const FFCBCardTableRow& Row, const FFCBAttributeSet& A) {
	// Draw name/rarity/attributes onto a transient texture via Canvas render target substitute:
	// (A full UCanvasRenderTarget2D pass is used so no editor-authored widget is required.)
	class UCanvasRenderTarget2D* RT = NewObject<UCanvasRenderTarget2D>(this, TEXT("CardRT")); RT->InitAutoFormat(512, 768); RT->UpdateResourceImmediate(true);
	UCanvas* Canvas; FVector2D Size; FDrawToRenderTargetContext Ctx;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, RT, Canvas, Size, Ctx);
	if (Canvas) {
		Canvas->K2_DrawBox(FVector2D(0, 0), Size, 768.f, FLinearColor(0.05f, 0.04f, 0.1f, 1.f));
		UTexture2D* Art = Row.Artwork.LoadSynchronous();
		if (Art) Canvas->K2_DrawTexture(Art, FVector2D(14, 64), FVector2D(484, 430), FVector2D(0, 0), FVector2D(1, 1), FLinearColor::White, EBlendMode::BLEND_Opaque, 0.f, FVector2D(0, 0));
		Canvas->K2_DrawText(nullptr, Row.DisplayName, FVector2D(24, 18), FVector2D(1.4f, 1.4f), FLinearColor(0.96f, 0.92f, 0.82f), 1.f, FLinearColor::Black, FVector2D(1, 1), true, false, true, FLinearColor::Black);
		int32 i = 0; for (EFCBAttribute At : FFCBAttributeSet::All()) { Canvas->K2_DrawText(nullptr, FString::Printf(TEXT("%s: %d"), *FFCBBattleRules::AttributeName(At), A.Get(At)), FVector2D(30 + (i % 2) * 245, 560 + (i / 2) * 48), FVector2D(1.1f, 1.1f), FLinearColor::White, 1.f, FLinearColor::Black, FVector2D(1, 1), false, false, true, FLinearColor::Black); ++i; }
	}
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Ctx);
	return RT; // UCanvasRenderTarget2D derives from UTexture; material samples it the same way
}
void AFCBCardActor::ShowCard(const FFCBCardTableRow& Row, const FFCBAttributeSet& Effective, bool bFaceUp) {
	FrontTexture = RenderFrontTexture(Row, Effective);
	if (UMaterialInstanceDynamic* MID = CardMesh->CreateAndSetMaterialInstanceDynamic(0)) { MID->SetTextureParameterValue(TEXT("CardTexture"), FrontTexture); if (FallbackMaterial) MID->Parent = FallbackMaterial; }
	BaseZ = GetActorLocation().Z; SetActorRotation(FRotator(0, bFaceUp ? 0.f : 180.f, 0)); bFlipping = false; FlipT = 1.f;
}
void AFCBCardActor::ShowBack() { BaseZ = GetActorLocation().Z; SetActorRotation(FRotator(0, 180.f, 0)); bFlipping = false; }
void AFCBCardActor::FlipToFront(float Duration) { FlipDuration = FMath::Max(0.05f, Duration); FlipT = 0.f; bFlipping = true; BaseZ = GetActorLocation().Z; }
void AFCBCardActor::SetHighlighted(bool bOn) { CardMesh->SetRenderCustomDepth(bOn); CardMesh->SetCustomDepthStencilValue(bOn ? 1 : 0); }
void AFCBCardActor::PlayResultFeedback(bool bWinner, bool bTie) { FeedbackT = 0.f; bFeedbackWinner = bWinner && !bTie; }
void AFCBCardActor::Tick(float Dt) {
	Super::Tick(Dt); if (!bFlipping && FeedbackT < 0.f) return;
	if (bFlipping) { FlipT = FMath::Min(1.f, FlipT + Dt / FlipDuration); const float A = FMath::InterpEaseOut(180.f, 0.f, FlipT, 2.f); SetActorRotation(FRotator(0, A, 0)); SetActorLocation(GetActorLocation() + FVector(0, 0, FMath::Sin(FlipT * PI) * Dt * 40.f)); if (FlipT >= 1.f) bFlipping = false; }
	if (FeedbackT >= 0.f) { FeedbackT += Dt; SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, BaseZ + (bFeedbackWinner ? FMath::Abs(FMath::Sin(FeedbackT * 7.f)) * 22.f : 0.f))); if (FeedbackT > 1.2f) { FeedbackT = -1.f; SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, BaseZ)); } }
}
