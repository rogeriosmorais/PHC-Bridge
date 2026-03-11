#pragma once

#include "Subsystems/WorldSubsystem.h"

#include "PhysAnimComparisonSubsystem.generated.h"

class ACharacter;
class APlayerController;
class ACameraActor;
class AActor;

UCLASS()
class PHYSANIMPLUGIN_API UPhysAnimComparisonSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;

	bool StartSideBySide(FString& OutError);
	bool StartPresentation(FString& OutError);
	void StopSideBySide();
	bool IsSideBySideActive() const;

	static FString ResolveComparisonRoleLabel(bool bPhysicsDriven);
	static FVector ResolveComparisonSpawnOffset(bool bPhysicsDriven, float LateralSeparationCm);
	static FVector ResolveMirroredWorldInput(const FVector& PendingWorldInput, const FVector& LastWorldInput);
	static FVector ResolvePresentationLocalIntent(float ElapsedSeconds);
	static float ResolvePresentationInputScale(float ElapsedSeconds);
	static FName ResolvePresentationPhaseName(float ElapsedSeconds);
	static float GetPresentationDurationSeconds();
	static FVector ResolvePresentationCameraOffsetCm();

private:
	bool StartComparison(bool bEnablePresentationMode, FString& OutError);
	bool ConfigureSourcePhysicsCharacter(ACharacter& Character, FString& OutError);
	bool ConfigureKinematicBaselineCharacter(ACharacter& Character, FString& OutError);
	bool ActivatePresentationMode(FString& OutError);
	void TickManualComparison(ACharacter& SourceCharacter, ACharacter& KinematicCharacter);
	void TickPresentationComparison(ACharacter& SourceCharacter, ACharacter& KinematicCharacter);
	void TickComparisonLabels() const;
	void UpdatePresentationCamera();
	void RestoreSourcePhysicsCharacter();
	void RestorePresentationMode();

	TWeakObjectPtr<ACharacter> SourcePhysicsCharacter;
	TWeakObjectPtr<ACharacter> SpawnedKinematicCharacter;
	TWeakObjectPtr<APlayerController> PresentationPlayerController;
	TWeakObjectPtr<AActor> OriginalViewTarget;
	TWeakObjectPtr<ACameraActor> ComparisonCameraActor;
	FTransform SourceOriginalTransform = FTransform::Identity;
	bool bHasSavedSourceOriginalTransform = false;
	TEnumAsByte<ECollisionResponse> SourceOriginalCapsulePawnResponse = ECollisionResponse::ECR_Block;
	TEnumAsByte<ECollisionResponse> SourceOriginalMeshPawnResponse = ECollisionResponse::ECR_Block;
	FRotator ComparisonAnchorRotation = FRotator::ZeroRotator;
	double PresentationStartTimeSeconds = -1.0;
	bool bPresentationModeActive = false;
	bool bHasSavedControllerInputState = false;
	bool bOriginalIgnoreMoveInput = false;
	bool bOriginalIgnoreLookInput = false;
	FName LastPresentationPhaseName = NAME_None;
	bool bComparisonActive = false;
	float LateralSeparationCm = 250.0f;
};
