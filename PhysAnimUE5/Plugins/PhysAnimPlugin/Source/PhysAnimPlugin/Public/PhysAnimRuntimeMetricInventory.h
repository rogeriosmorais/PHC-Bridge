#pragma once

#include "CoreMinimal.h"
#include "PhysAnimEvidenceClassifier.h"

struct FPhysAnimRuntimeMetricFieldMapping
{
	FString ArtifactFieldName;
	TArray<FString> SourceProvenance;
	TArray<FString> TestCoverageNames;
};

struct FPhysAnimRuntimeMetricSegmentInventory
{
	EPhysAnimEvidenceBaselineSegment Segment = EPhysAnimEvidenceBaselineSegment::PoseSearch;
	FString SegmentName;
	bool bRequiresNewTelemetry = false;
	TArray<FPhysAnimRuntimeMetricFieldMapping> ExistingCounterMappings;
};

struct FPhysAnimRuntimeMetricInventory
{
	bool bGameplayBehaviorChanges = false;
	TArray<FPhysAnimRuntimeMetricSegmentInventory> Segments;
};

namespace PhysAnimRuntimeMetricInventory
{
	FPhysAnimRuntimeMetricInventory BuildExistingCounterInventory();
}
