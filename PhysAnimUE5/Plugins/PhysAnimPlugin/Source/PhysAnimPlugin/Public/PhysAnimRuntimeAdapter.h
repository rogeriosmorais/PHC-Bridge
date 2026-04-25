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

struct FPhysAnimSupportSnapshotCaptureInput
{
	bool bSupportStateL = false;
	bool bSupportStateR = false;
	EPhysAnimSupportMode SupportMode = EPhysAnimSupportMode::Airborne;
	double SupportGapTimerMs = 0.0;
	double SupportGapMaxMs = 100.0;
	int32 ActiveSupportSideCount = 0;
	double SupportHullAreaCm2 = 0.0;
	double SupportAreaMinCm2 = 50.0;
	double SupportPatchAreaLCm2 = 0.0;
	double SupportPatchAreaRCm2 = 0.0;
	TArray<FVector2D> SupportHullPointsCm;
	FVector2D ComProxyPosCm = FVector2D::ZeroVector;
	double MaxPenetrationCm = 0.0;
	int32 SupportChurnCount = 0;
	double SupportChurnHz = 0.0;
	TOptional<bool> ProxyInsideHull;
	TOptional<double> ProxyOutsideHullDurationMs;
	EPhysAnimTerminalReason ProxyTerminalReason = EPhysAnimTerminalReason::None;
};

namespace PhysAnimRuntimeAdapter
{
	FPhysAnimContinuitySnapshot CaptureContinuitySnapshot(const FPhysAnimContinuitySnapshotCaptureInput& Input);
	FPhysAnimCapsuleContractSnapshot CaptureCapsuleContractSnapshot(const FPhysAnimCapsuleContractSnapshotCaptureInput& Input);
	FPhysAnimPlantContractSnapshot CapturePlantContractSnapshot(const FPhysAnimPlantContractSnapshotCaptureInput& Input);
	FPhysAnimSupportContractSnapshot CaptureSupportSnapshot(const FPhysAnimSupportSnapshotCaptureInput& Input);
}
