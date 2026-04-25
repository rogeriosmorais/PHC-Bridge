#pragma once

#include "PhysAnimValidators.h"

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

namespace PhysAnimRuntimeAdapter
{
	FPhysAnimContinuitySnapshot CaptureContinuitySnapshot(const FPhysAnimContinuitySnapshotCaptureInput& Input);
}
