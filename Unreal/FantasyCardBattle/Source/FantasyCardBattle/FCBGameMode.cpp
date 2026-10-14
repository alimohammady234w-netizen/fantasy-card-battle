#include "FCBGameMode.h"
#include "FCBPlayerController.h"
#include "FCBCardActor.h"
#include "Camera/CameraActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

AFCBGameMode::AFCBGameMode() { PlayerControllerClass = AFCBPlayerController::StaticClass(); bStartPlayersAsSpectators = false; }
static void Box(UWorld* W, const FVector& Loc, const FVector& Scale, const FLinearColor& Color, float Metallic = 0.2f, float Rough = 0.55f) {
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* A = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Loc, FRotator::ZeroRotator, SP); if (!A) return;
	UStaticMeshComponent* C = A->GetStaticMeshComponent(); C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	C->SetWorldScale3D(Scale); C->SetMobility(EComponentMobility::Movable);
	if (UMaterialInstanceDynamic* M = C->CreateAndSetMaterialInstanceDynamic(0)) { M->SetVectorParameterValue(TEXT("Color"), Color); M->SetScalarParameterValue(TEXT("Metallic"), Metallic); M->SetScalarParameterValue(TEXT("Roughness"), Rough); }
}
void AFCBGameMode::BeginPlay() { Super::BeginPlay(); BuildBattlefield(); }
void AFCBGameMode::BuildBattlefield() {
	UWorld* W = GetWorld(); if (!W) return;
	Box(W, FVector(0, 0, -30), FVector(16, 12, 0.4f), FLinearColor(0.10f, 0.07f, 0.20f), 0.35f, 0.45f);         // table slab (top at z=-10)
	Box(W, FVector(0, 0, -300), FVector(6, 6, 5), FLinearColor(0.05f, 0.03f, 0.10f));                              // pedestal
	Box(W, FVector(-560, -230, -5), FVector(1.5f, 2.15f, 0.1f), FLinearColor(0.07f, 0.05f, 0.16f), 0.6f, 0.35f);          // AI deck zone
	Box(W, FVector(560, 230, -5), FVector(1.5f, 2.15f, 0.1f), FLinearColor(0.07f, 0.05f, 0.16f), 0.6f, 0.35f);            // player deck zone
	Box(W, FVector(-560, 230, -5), FVector(1.5f, 2.15f, 0.1f), FLinearColor(0.16f, 0.05f, 0.10f), 0.6f, 0.35f);           // player discard
	Box(W, FVector(560, -230, -5), FVector(1.5f, 2.15f, 0.1f), FLinearColor(0.16f, 0.05f, 0.10f), 0.6f, 0.35f);          // AI discard
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	PlayerCardActor = W->SpawnActor<AFCBCardActor>(AFCBCardActor::StaticClass(), FVector(-235, 0, 147), FRotator::ZeroRotator, SP);
	AICardActor = W->SpawnActor<AFCBCardActor>(AFCBCardActor::StaticClass(), FVector(235, 0, 147), FRotator(0, 180, 0), SP);
	ACameraActor* Cam = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FVector(0, -780, 620), FRotator(-38, 90, 0), SP);
	if (Cam && W->GetFirstPlayerController()) W->GetFirstPlayerController()->SetViewTarget(Cam); // controlled, readable framing
	if (APointLight* Key = W->SpawnActor<APointLight>(FVector(-420, -260, 520), FRotator::ZeroRotator, SP)) { Key->SetLightColor(FLinearColor(1.f, 0.72f, 0.4f)); Key->SetIntensity(90000.f); Key->SetAttenuationRadius(4000.f); }
	if (APointLight* Rim = W->SpawnActor<APointLight>(FVector(480, 320, 420), FRotator::ZeroRotator, SP)) { Rim->SetLightColor(FLinearColor(0.4f, 0.65f, 1.f)); Rim->SetIntensity(70000.f); Rim->SetAttenuationRadius(4000.f); }
	if (UNiagaraSystem* FX = AmbientParticles.LoadSynchronous()) UNiagaraFunctionLibrary::SpawnSystemAtLocation(W, FX, FVector(0, 0, 260)); // optional, guarded
}
