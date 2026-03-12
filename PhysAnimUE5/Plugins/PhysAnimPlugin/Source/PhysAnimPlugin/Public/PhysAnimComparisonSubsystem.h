#pragma once

#include "Subsystems/WorldSubsystem.h"

#include "PhysAnimComparisonSubsystem.generated.h"

class ACharacter;
class APlayerController;
class ACameraActor;
class AActor;
class UBoxComponent;

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
	static bool ShouldApplyPresentationPerturbation(float ElapsedSeconds);
	static float GetPresentationDurationSeconds();
	static FVector ResolvePresentationCameraOffsetCm(bool bPerturbationPhase = false);
	static FVector ResolvePresentationPusherHalfExtentCm();
	static FVector ResolvePresentationPusherStartOffsetCm();
	static float ResolvePresentationPusherTravelDistanceCm();
	static float ResolvePresentationPusherTravelSeconds();
	static FVector ResolvePresentationShellPushForce();
	static float ResolvePresentationStabilizationOverrideSeconds();
	static float ResolvePresentationStrengthRelaxationMultiplier();
	static float ResolvePresentationDampingRatioRelaxationMultiplier();
	static float ResolvePresentationExtraDampingRelaxationMultiplier();
	static FName ResolvePresentationRootBoneName();

private:
	bool StartComparison(bool bEnablePresentationMode, FString& OutError);
	bool ConfigureSourcePhysicsCharacter(ACharacter& Character, FString& OutError);
	bool ConfigureKinematicBaselineCharacter(ACharacter& Character, FString& OutError);
	bool ActivatePresentationMode(FString& OutError);
	bool CreatePresentationPushers(FString& OutError);
	void TickManualComparison(ACharacter& SourceCharacter, ACharacter& KinematicCharacter);
	void TickPresentationComparison(ACharacter& SourceCharacter, ACharacter& KinematicCharacter);
	void TickComparisonLabels() const;
	void UpdatePresentationCamera();
	void UpdatePerturbationPushers(ACharacter& SourceCharacter, ACharacter& KinematicCharacter, float ElapsedSeconds);
	void SetPresentationSourceShellSuppressed(ACharacter& SourceCharacter, bool bSuppressed);
	void UpdatePresentationSourceRootFollow(ACharacter& SourceCharacter);
	void CapturePerturbationBaseline(const ACharacter& SourceCharacter, const ACharacter& KinematicCharacter);
	void MaybeLogPerturbationTelemetry(const ACharacter& SourceCharacter, const ACharacter& KinematicCharacter);
	void RestoreSourcePhysicsCharacter();
	void RestorePresentationMode();

	TWeakObjectPtr<ACharacter> SourcePhysicsCharacter;
	TWeakObjectPtr<ACharacter> SpawnedKinematicCharacter;
	TWeakObjectPtr<APlayerController> PresentationPlayerController;
	TWeakObjectPtr<AActor> OriginalViewTarget;
	TWeakObjectPtr<ACameraActor> ComparisonCameraActor;
	TWeakObjectPtr<AActor> SourcePerturbationPusherActor;
	TWeakObjectPtr<AActor> KinematicPerturbationPusherActor;
	TWeakObjectPtr<UBoxComponent> SourcePerturbationPusherBox;
	TWeakObjectPtr<UBoxComponent> KinematicPerturbationPusherBox;
	FTransform SourceOriginalTransform = FTransform::Identity;
	bool bHasSavedSourceOriginalTransform = false;
	TEnumAsByte<ECollisionResponse> SourceOriginalCapsulePawnResponse = ECollisionResponse::ECR_Block;
	TEnumAsByte<ECollisionResponse> SourceOriginalMeshPawnResponse = ECollisionResponse::ECR_Block;
	TEnumAsByte<ECollisionResponse> SourceOriginalCapsuleWorldDynamicResponse = ECollisionResponse::ECR_Block;
	TEnumAsByte<ECollisionResponse> SourceOriginalMeshWorldDynamicResponse = ECollisionResponse::ECR_Block;
	FRotator ComparisonAnchorRotation = FRotator::ZeroRotator;
	double PresentationStartTimeSeconds = -1.0;
	bool bPresentationPerturbationApplied = false;
	bool bPresentationModeActive = false;
	bool bHasSavedControllerInputState = false;
	bool bOriginalIgnoreMoveInput = false;
	bool bOriginalIgnoreLookInput = false;
	FName LastPresentationPhaseName = NAME_None;
	bool bComparisonActive = false;
	float LateralSeparationCm = 250.0f;
	double PresentationPerturbationAppliedTimeSeconds = -1.0;
	double LastPerturbationTelemetryLogTimeSeconds = -1.0;
	FVector SourcePerturbationBaselineLocation = FVector::ZeroVector;
	FVector KinematicPerturbationBaselineLocation = FVector::ZeroVector;
	FVector SourcePerturbationBaselineRootLocation = FVector::ZeroVector;
	FVector SourcePerturbationBaselineSpineLocation = FVector::ZeroVector;
	FVector KinematicPerturbationBaselineSpineLocation = FVector::ZeroVector;
	FVector SourcePerturbationBaselineHeadLocation = FVector::ZeroVector;
	FVector KinematicPerturbationBaselineHeadLocation = FVector::ZeroVector;
	FVector SourcePerturbationBaselineLeftFootLocation = FVector::ZeroVector;
	FVector KinematicPerturbationBaselineLeftFootLocation = FVector::ZeroVector;
	FVector SourcePerturbationBaselineRightFootLocation = FVector::ZeroVector;
	FVector KinematicPerturbationBaselineRightFootLocation = FVector::ZeroVector;
	FVector SourcePerturbationBaselineRootLocalOffset = FVector::ZeroVector;
	FVector SourcePerturbationBaselineSpineLocalOffset = FVector::ZeroVector;
	FVector KinematicPerturbationBaselineSpineLocalOffset = FVector::ZeroVector;
	FVector SourcePerturbationBaselineHeadLocalOffset = FVector::ZeroVector;
	FVector KinematicPerturbationBaselineHeadLocalOffset = FVector::ZeroVector;
	FVector SourcePerturbationBaselineLeftFootLocalOffset = FVector::ZeroVector;
	FVector KinematicPerturbationBaselineLeftFootLocalOffset = FVector::ZeroVector;
	FVector SourcePerturbationBaselineRightFootLocalOffset = FVector::ZeroVector;
	FVector KinematicPerturbationBaselineRightFootLocalOffset = FVector::ZeroVector;
	bool bPresentationSourceShellSuppressed = false;
	bool bHasSavedPresentationSourceMovementState = false;
	bool bSavedPresentationSourceMovementTickEnabled = false;
	uint8 SavedPresentationSourceMovementMode = 0;
	uint8 SavedPresentationSourceCustomMovementMode = 0;
	TEnumAsByte<ECollisionEnabled::Type> SavedPresentationSourceCapsuleCollisionEnabled = ECollisionEnabled::QueryAndPhysics;
};
