#pragma once

#include "PhysAnimValidators.h"

class UCharacterMovementComponent;
class UCapsuleComponent;
class USkeletalMeshComponent;

struct FPhysAnimContinuitySnapshotCaptureInput
{
	USkeletalMeshComponent* SkeletalMeshComponent = nullptr;
	TArray<FName> CriticalBodyNames;
	FName PelvisBodyName = NAME_None;
	double PreviousPelvisSleepDurationMs = 0.0;
	double DeltaMs = 0.0;
	bool bBookkeepingReportsContinuity = true;
};

struct FPhysAnimCapsuleContractSnapshotCaptureInput
{
	UCapsuleComponent* CapsuleComponent = nullptr;
	USkeletalMeshComponent* SkeletalMeshComponent = nullptr;
	UCharacterMovementComponent* CharacterMovementComponent = nullptr;
	FVector RebaseOriginCm = FVector::ZeroVector;
};

struct FPhysAnimPlantContractSnapshotCaptureInput
{
	USkeletalMeshComponent* SkeletalMeshComponent = nullptr;
	FString ExpectedPhysicsAssetPath;
	bool bSkeletonAuditPassed = true;
	EPhysAnimPlantFailureClass PlantFailureClass = EPhysAnimPlantFailureClass::None;
	EPhysAnimPlantFailureField PlantFailureField = EPhysAnimPlantFailureField::None;
	double MassDriftTotalPct = 0.0;
};

namespace PhysAnimRuntimeAdapter
{
	FPhysAnimContinuitySnapshot CaptureContinuitySnapshot(const FPhysAnimContinuitySnapshotCaptureInput& Input);
	FPhysAnimCapsuleContractSnapshot CaptureCapsuleContractSnapshot(const FPhysAnimCapsuleContractSnapshotCaptureInput& Input);
	FPhysAnimPlantContractSnapshot CapturePlantContractSnapshot(const FPhysAnimPlantContractSnapshotCaptureInput& Input);
}
