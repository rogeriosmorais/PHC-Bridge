#if WITH_DEV_AUTOMATION_TESTS

#include "PhysAnimRuntimeMetricInventory.h"
#include "Misc/AutomationTest.h"

namespace
{
	using namespace PhysAnimRuntimeMetricInventory;

	const FPhysAnimRuntimeMetricSegmentInventory* FindSegment(
		const FPhysAnimRuntimeMetricInventory& Inventory,
		const EPhysAnimEvidenceBaselineSegment Segment)
	{
		for (const FPhysAnimRuntimeMetricSegmentInventory& SegmentInventory : Inventory.Segments)
		{
			if (SegmentInventory.Segment == Segment)
			{
				return &SegmentInventory;
			}
		}

		return nullptr;
	}

	const FPhysAnimRuntimeMetricFieldMapping* FindMapping(
		const FPhysAnimRuntimeMetricSegmentInventory& SegmentInventory,
		const FString& ArtifactFieldName)
	{
		for (const FPhysAnimRuntimeMetricFieldMapping& Mapping : SegmentInventory.ExistingCounterMappings)
		{
			if (Mapping.ArtifactFieldName == ArtifactFieldName)
			{
				return &Mapping;
			}
		}

		return nullptr;
	}

	bool AssertNonEmptyStringList(
		FAutomationTestBase& Test,
		const TCHAR* Context,
		const TArray<FString>& Values)
	{
		Test.TestTrue(Context, Values.Num() > 0);
		for (const FString& Value : Values)
		{
			Test.TestFalse(Context, Value.IsEmpty());
		}
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimRuntimeMetricInventoryArchitectureTest,
		"PhysAnim.RuntimeMetricInventory.ArchitectureSegments",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimRuntimeMetricInventoryArchitectureTest::RunTest(const FString& Parameters)
	{
		const FPhysAnimRuntimeMetricInventory Inventory = BuildExistingCounterInventory();

		TestFalse(TEXT("Inventory declares no gameplay behavior changes"), Inventory.bGameplayBehaviorChanges);
		TestEqual(TEXT("Inventory declares exactly five EB-01 architecture segments"), Inventory.Segments.Num(), 5);

		const EPhysAnimEvidenceBaselineSegment ExpectedSegments[] =
		{
			EPhysAnimEvidenceBaselineSegment::PoseSearch,
			EPhysAnimEvidenceBaselineSegment::PhcPolicy,
			EPhysAnimEvidenceBaselineSegment::PhysicsControl,
			EPhysAnimEvidenceBaselineSegment::Chaos,
			EPhysAnimEvidenceBaselineSegment::RendererFacingMotion
		};

		const TCHAR* ExpectedSegmentNames[] =
		{
			TEXT("PoseSearch"),
			TEXT("PhcPolicy"),
			TEXT("PhysicsControl"),
			TEXT("Chaos"),
			TEXT("RendererFacingMotion")
		};

		for (int32 Index = 0; Index < UE_ARRAY_COUNT(ExpectedSegments); ++Index)
		{
			TestEqual(
				TEXT("Inventory preserves the EB-01 architecture segment order"),
				static_cast<uint8>(Inventory.Segments[Index].Segment),
				static_cast<uint8>(ExpectedSegments[Index]));
			TestEqual(
				TEXT("Inventory preserves the EB-01 segment names"),
				Inventory.Segments[Index].SegmentName,
				FString(ExpectedSegmentNames[Index]));
		}

		const FPhysAnimRuntimeMetricSegmentInventory* PoseSearch = FindSegment(Inventory, EPhysAnimEvidenceBaselineSegment::PoseSearch);
		const FPhysAnimRuntimeMetricSegmentInventory* PhcPolicy = FindSegment(Inventory, EPhysAnimEvidenceBaselineSegment::PhcPolicy);
		const FPhysAnimRuntimeMetricSegmentInventory* PhysicsControl = FindSegment(Inventory, EPhysAnimEvidenceBaselineSegment::PhysicsControl);
		const FPhysAnimRuntimeMetricSegmentInventory* Chaos = FindSegment(Inventory, EPhysAnimEvidenceBaselineSegment::Chaos);
		const FPhysAnimRuntimeMetricSegmentInventory* RendererFacingMotion = FindSegment(Inventory, EPhysAnimEvidenceBaselineSegment::RendererFacingMotion);

		TestNotNull(TEXT("PoseSearch segment exists"), PoseSearch);
		TestNotNull(TEXT("PhcPolicy segment exists"), PhcPolicy);
		TestNotNull(TEXT("PhysicsControl segment exists"), PhysicsControl);
		TestNotNull(TEXT("Chaos segment exists"), Chaos);
		TestNotNull(TEXT("RendererFacingMotion segment exists"), RendererFacingMotion);

		if (!PoseSearch || !PhcPolicy || !PhysicsControl || !Chaos || !RendererFacingMotion)
		{
			return false;
		}

		TestFalse(TEXT("PoseSearch reuses existing counters"), PoseSearch->bRequiresNewTelemetry);
		TestFalse(TEXT("RendererFacingMotion reuses existing counters"), RendererFacingMotion->bRequiresNewTelemetry);
		TestFalse(TEXT("PhcPolicy reuses existing counters"), PhcPolicy->bRequiresNewTelemetry);
		TestFalse(TEXT("PhysicsControl reuses existing counters"), PhysicsControl->bRequiresNewTelemetry);
		TestFalse(TEXT("Chaos reuses existing counters"), Chaos->bRequiresNewTelemetry);

		TestEqual(TEXT("PoseSearch has five reused counter mappings"), PoseSearch->ExistingCounterMappings.Num(), 5);
		TestEqual(TEXT("RendererFacingMotion has three reused counter mappings"), RendererFacingMotion->ExistingCounterMappings.Num(), 3);
		TestEqual(TEXT("PhcPolicy has twelve reused counter mappings"), PhcPolicy->ExistingCounterMappings.Num(), 12);
		TestEqual(TEXT("PhysicsControl has nine reused counter mappings"), PhysicsControl->ExistingCounterMappings.Num(), 9);
		TestEqual(TEXT("Chaos has ten reused counter mappings"), Chaos->ExistingCounterMappings.Num(), 10);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimRuntimeMetricInventoryFieldMappingTest,
		"PhysAnim.RuntimeMetricInventory.FieldMappings",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimRuntimeMetricInventoryFieldMappingTest::RunTest(const FString& Parameters)
	{
		const FPhysAnimRuntimeMetricInventory Inventory = BuildExistingCounterInventory();

		const FPhysAnimRuntimeMetricSegmentInventory* PoseSearch = FindSegment(Inventory, EPhysAnimEvidenceBaselineSegment::PoseSearch);
		const FPhysAnimRuntimeMetricSegmentInventory* PhcPolicy = FindSegment(Inventory, EPhysAnimEvidenceBaselineSegment::PhcPolicy);
		const FPhysAnimRuntimeMetricSegmentInventory* PhysicsControl = FindSegment(Inventory, EPhysAnimEvidenceBaselineSegment::PhysicsControl);
		const FPhysAnimRuntimeMetricSegmentInventory* Chaos = FindSegment(Inventory, EPhysAnimEvidenceBaselineSegment::Chaos);
		const FPhysAnimRuntimeMetricSegmentInventory* RendererFacingMotion = FindSegment(Inventory, EPhysAnimEvidenceBaselineSegment::RendererFacingMotion);

		TestNotNull(TEXT("PoseSearch segment exists"), PoseSearch);
		TestNotNull(TEXT("PhcPolicy segment exists"), PhcPolicy);
		TestNotNull(TEXT("PhysicsControl segment exists"), PhysicsControl);
		TestNotNull(TEXT("Chaos segment exists"), Chaos);
		TestNotNull(TEXT("RendererFacingMotion segment exists"), RendererFacingMotion);

		if (!PoseSearch || !PhcPolicy || !PhysicsControl || !Chaos || !RendererFacingMotion)
		{
			return false;
		}

		TestNotNull(TEXT("PoseSearchQueryCount mapping exists"), FindMapping(*PoseSearch, TEXT("PoseSearchQueryCount")));
		TestNotNull(TEXT("PoseSearchValidResultCount mapping exists"), FindMapping(*PoseSearch, TEXT("PoseSearchValidResultCount")));
		TestNotNull(TEXT("PoseSearchSelectedAnimationName mapping exists"), FindMapping(*PoseSearch, TEXT("PoseSearchSelectedAnimationName")));
		TestNotNull(TEXT("PoseSearchSelectedTime mapping exists"), FindMapping(*PoseSearch, TEXT("PoseSearchSelectedTime")));
		TestNotNull(TEXT("PoseSearchConsecutiveInvalidFrameCount mapping exists"), FindMapping(*PoseSearch, TEXT("PoseSearchConsecutiveInvalidFrameCount")));
		TestNotNull(TEXT("PolicyInferenceSuccessCount mapping exists"), FindMapping(*PhcPolicy, TEXT("PolicyInferenceSuccessCount")));
		TestNotNull(TEXT("PolicyInferenceAttemptCount mapping exists"), FindMapping(*PhcPolicy, TEXT("PolicyInferenceAttemptCount")));
		TestNotNull(TEXT("PolicyInferenceFailureCount mapping exists"), FindMapping(*PhcPolicy, TEXT("PolicyInferenceFailureCount")));
		TestNotNull(TEXT("PolicyInferenceLatencyMsMax mapping exists"), FindMapping(*PhcPolicy, TEXT("PolicyInferenceLatencyMsMax")));
		TestNotNull(TEXT("bPolicyModelLoaded mapping exists"), FindMapping(*PhcPolicy, TEXT("bPolicyModelLoaded")));
		TestNotNull(TEXT("PolicyRuntimeName mapping exists"), FindMapping(*PhcPolicy, TEXT("PolicyRuntimeName")));
		TestNotNull(TEXT("PolicyModelName mapping exists"), FindMapping(*PhcPolicy, TEXT("PolicyModelName")));
		TestNotNull(TEXT("bPolicyInputBuffersFinite mapping exists"), FindMapping(*PhcPolicy, TEXT("bPolicyInputBuffersFinite")));
		TestNotNull(TEXT("PolicyActionSampleCount mapping exists"), FindMapping(*PhcPolicy, TEXT("PolicyActionSampleCount")));
		TestNotNull(TEXT("PolicyActionRawMeanAbsMax mapping exists"), FindMapping(*PhcPolicy, TEXT("PolicyActionRawMeanAbsMax")));
		TestNotNull(TEXT("PolicyActionConditionedMeanAbsMax mapping exists"), FindMapping(*PhcPolicy, TEXT("PolicyActionConditionedMeanAbsMax")));
		TestNotNull(TEXT("PolicyActionClampedFloatMax mapping exists"), FindMapping(*PhcPolicy, TEXT("PolicyActionClampedFloatMax")));
		const FPhysAnimRuntimeMetricFieldMapping* ControlTargetTotalWritesMapping = FindMapping(*PhysicsControl, TEXT("ControlTargetTotalWrites"));
		TestNotNull(TEXT("ControlTargetTotalWrites mapping exists"), ControlTargetTotalWritesMapping);
		TestNotNull(TEXT("ControlTargetNormalWrites mapping exists"), FindMapping(*PhysicsControl, TEXT("ControlTargetNormalWrites")));
		const FPhysAnimRuntimeMetricFieldMapping* ControlTargetSampleCountMapping = FindMapping(*PhysicsControl, TEXT("ControlTargetSampleCount"));
		TestNotNull(TEXT("ControlTargetSampleCount mapping exists"), ControlTargetSampleCountMapping);
		const FPhysAnimRuntimeMetricFieldMapping* ControlTargetMaxDeltaDegMapping = FindMapping(*PhysicsControl, TEXT("ControlTargetMaxDeltaDeg"));
		TestNotNull(TEXT("ControlTargetMaxDeltaDeg mapping exists"), ControlTargetMaxDeltaDegMapping);
		const FPhysAnimRuntimeMetricFieldMapping* ControlTargetMeanDeltaDegMaxMapping = FindMapping(*PhysicsControl, TEXT("ControlTargetMeanDeltaDegMax"));
		TestNotNull(TEXT("ControlTargetMeanDeltaDegMax mapping exists"), ControlTargetMeanDeltaDegMaxMapping);
		const FPhysAnimRuntimeMetricFieldMapping* ControlTargetMaxRawPolicyOffsetDegMapping = FindMapping(*PhysicsControl, TEXT("ControlTargetMaxRawPolicyOffsetDeg"));
		TestNotNull(TEXT("ControlTargetMaxRawPolicyOffsetDeg mapping exists"), ControlTargetMaxRawPolicyOffsetDegMapping);
		const FPhysAnimRuntimeMetricFieldMapping* ControlTargetMeanRawPolicyOffsetDegMaxMapping = FindMapping(*PhysicsControl, TEXT("ControlTargetMeanRawPolicyOffsetDegMax"));
		TestNotNull(TEXT("ControlTargetMeanRawPolicyOffsetDegMax mapping exists"), ControlTargetMeanRawPolicyOffsetDegMaxMapping);
		const FPhysAnimRuntimeMetricFieldMapping* ControlTargetNormalWritesMapping = FindMapping(*PhysicsControl, TEXT("ControlTargetNormalWrites"));
		TestNotNull(TEXT("ControlTargetNormalWrites mapping exists"), ControlTargetNormalWritesMapping);
		TestFalse(TEXT("PhysicsControl reuses existing telemetry"), PhysicsControl->bRequiresNewTelemetry);
		TestNotNull(TEXT("bPhysicsControlComponentAvailable mapping exists"), FindMapping(*PhysicsControl, TEXT("bPhysicsControlComponentAvailable")));
		const FPhysAnimRuntimeMetricFieldMapping* ControlledBodyCountMapping = FindMapping(*PhysicsControl, TEXT("ControlledBodyCount"));
		TestNotNull(TEXT("ControlledBodyCount mapping exists"), ControlledBodyCountMapping);
		if (!ControlledBodyCountMapping)
		{
			return false;
		}

		TestFalse(TEXT("ControlledBodyCount reuses existing telemetry"), PhysicsControl->bRequiresNewTelemetry);
		TestTrue(
			TEXT("ControlTargetTotalWrites mapping is sourced from reused telemetry"),
			ControlTargetTotalWritesMapping != nullptr &&
				ControlTargetTotalWritesMapping->SourceProvenance.Contains(TEXT("PhysAnimComponent.ModifierTracking.cpp::UPhysAnimComponent::ApplyControlTargets")) &&
				ControlTargetTotalWritesMapping->SourceProvenance.Contains(TEXT("PhysAnimComponent.cpp::UPhysAnimComponent::TickLiveRuntimeEvidenceProof")));
		TestTrue(
			TEXT("ControlTargetNormalWrites mapping is sourced from reused telemetry"),
			ControlTargetNormalWritesMapping != nullptr &&
				ControlTargetNormalWritesMapping->SourceProvenance.Contains(TEXT("PhysAnimComponent.ModifierTracking.cpp::UPhysAnimComponent::ApplyControlTargets")) &&
				ControlTargetNormalWritesMapping->SourceProvenance.Contains(TEXT("PhysAnimComponent.cpp::UPhysAnimComponent::TickLiveRuntimeEvidenceProof")));
		TestTrue(
			TEXT("ControlTargetSampleCount mapping is sourced from runtime capture and snapshot reuse"),
			ControlTargetSampleCountMapping != nullptr &&
				ControlTargetSampleCountMapping->SourceProvenance.Contains(TEXT("PhysAnimComponent.cpp::UPhysAnimComponent::UpdateActivatedStandingStabilityMetrics")) &&
				ControlTargetSampleCountMapping->SourceProvenance.Contains(TEXT("PhysAnimValidators.cpp::BuildRunArtifactSnapshot")));
		TestTrue(
			TEXT("ControlTargetMaxDeltaDeg mapping is sourced from the control-target application path"),
			ControlTargetMaxDeltaDegMapping != nullptr &&
				ControlTargetMaxDeltaDegMapping->SourceProvenance.Contains(TEXT("PhysAnimComponent.ModifierTracking.cpp::UPhysAnimComponent::ApplyControlTargets")));
		TestTrue(
			TEXT("ControlTargetMaxDeltaDeg mapping cites focused target-delta tests"),
			ControlTargetMaxDeltaDegMapping != nullptr &&
				ControlTargetMaxDeltaDegMapping->TestCoverageNames.Contains(TEXT("PhysAnim.Validators.ArtifactSnapshot.ControlTargetDeltaPreservation")) &&
				ControlTargetMaxDeltaDegMapping->TestCoverageNames.Contains(TEXT("PhysAnim.EvidenceSummary.PhysicsControlTargetDeltaEvidence")));
		TestTrue(
			TEXT("ControlTargetMeanDeltaDegMax mapping is sourced from the control-target application path"),
			ControlTargetMeanDeltaDegMaxMapping != nullptr &&
				ControlTargetMeanDeltaDegMaxMapping->SourceProvenance.Contains(TEXT("PhysAnimComponent.ModifierTracking.cpp::UPhysAnimComponent::ApplyControlTargets")));
		TestTrue(
			TEXT("ControlTargetMeanDeltaDegMax mapping cites focused target-delta tests"),
			ControlTargetMeanDeltaDegMaxMapping != nullptr &&
				ControlTargetMeanDeltaDegMaxMapping->TestCoverageNames.Contains(TEXT("PhysAnim.Validators.ArtifactSnapshot.ControlTargetDeltaPreservation")) &&
				ControlTargetMeanDeltaDegMaxMapping->TestCoverageNames.Contains(TEXT("PhysAnim.EvidenceSummary.PhysicsControlTargetDeltaEvidence")));
		TestTrue(
			TEXT("ControlTargetMaxRawPolicyOffsetDeg mapping is sourced from the control-target application path"),
			ControlTargetMaxRawPolicyOffsetDegMapping != nullptr &&
				ControlTargetMaxRawPolicyOffsetDegMapping->SourceProvenance.Contains(TEXT("PhysAnimComponent.ModifierTracking.cpp::UPhysAnimComponent::ApplyControlTargets")));
		TestTrue(
			TEXT("ControlTargetMaxRawPolicyOffsetDeg mapping cites focused raw-offset tests"),
			ControlTargetMaxRawPolicyOffsetDegMapping != nullptr &&
				ControlTargetMaxRawPolicyOffsetDegMapping->TestCoverageNames.Contains(TEXT("PhysAnim.Validators.ArtifactSnapshot.RawPolicyOffsetPreservation")) &&
				ControlTargetMaxRawPolicyOffsetDegMapping->TestCoverageNames.Contains(TEXT("PhysAnim.EvidenceSummary.PhysicsControlRawPolicyOffsetEvidence")));
		TestTrue(
			TEXT("ControlTargetMeanRawPolicyOffsetDegMax mapping is sourced from the control-target application path"),
			ControlTargetMeanRawPolicyOffsetDegMaxMapping != nullptr &&
				ControlTargetMeanRawPolicyOffsetDegMaxMapping->SourceProvenance.Contains(TEXT("PhysAnimComponent.ModifierTracking.cpp::UPhysAnimComponent::ApplyControlTargets")));
		TestTrue(
			TEXT("ControlTargetMeanRawPolicyOffsetDegMax mapping cites focused raw-offset tests"),
			ControlTargetMeanRawPolicyOffsetDegMaxMapping != nullptr &&
				ControlTargetMeanRawPolicyOffsetDegMaxMapping->TestCoverageNames.Contains(TEXT("PhysAnim.Validators.ArtifactSnapshot.RawPolicyOffsetPreservation")) &&
				ControlTargetMeanRawPolicyOffsetDegMaxMapping->TestCoverageNames.Contains(TEXT("PhysAnim.EvidenceSummary.PhysicsControlRawPolicyOffsetEvidence")));
		TestTrue(
			TEXT("ControlledBodyCount mapping is sourced from runtime capture and snapshot reuse"),
			ControlledBodyCountMapping->SourceProvenance.Contains(TEXT("PhysAnimComponent.cpp::UPhysAnimComponent::UpdateActivatedStandingStabilityMetrics")) &&
				ControlledBodyCountMapping->SourceProvenance.Contains(TEXT("PhysAnimValidators.cpp::BuildRunArtifactSnapshot")));

		const TCHAR* ExpectedChaosFields[] =
		{
			TEXT("RuntimeBodySampleCount"),
			TEXT("RuntimeSimulatingBodyCount"),
			TEXT("bPhysicalContinuityValidatorPassed"),
			TEXT("bContinuityBookkeepingMismatch"),
			TEXT("SupportMode"),
			TEXT("ActiveSupportSideCount"),
			TEXT("SupportHullAreaCm2"),
			TEXT("SupportGapTimerMs"),
			TEXT("ProxyInsideHull"),
			TEXT("bCalfContactTerminal")
		};

		for (const TCHAR* ExpectedField : ExpectedChaosFields)
		{
			TestNotNull(TEXT("Expected Chaos field mapping exists"), FindMapping(*Chaos, ExpectedField));
		}

		const TCHAR* ExpectedRendererFacingMotionFields[] =
		{
			TEXT("RendererFacingMotionSampleCount"),
			TEXT("RendererFacingMotionActiveSampleCount"),
			TEXT("RendererFacingMotionMaxRootWorldPositionDriftCm")
		};

		for (const TCHAR* ExpectedField : ExpectedRendererFacingMotionFields)
		{
			TestNotNull(TEXT("Expected RendererFacingMotion field mapping exists"), FindMapping(*RendererFacingMotion, ExpectedField));
		}

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimRuntimeMetricInventoryProvenanceTest,
		"PhysAnim.RuntimeMetricInventory.Provenance",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimRuntimeMetricInventoryProvenanceTest::RunTest(const FString& Parameters)
	{
		const FPhysAnimRuntimeMetricInventory Inventory = BuildExistingCounterInventory();

		for (const FPhysAnimRuntimeMetricSegmentInventory& Segment : Inventory.Segments)
		{
			for (const FPhysAnimRuntimeMetricFieldMapping& Mapping : Segment.ExistingCounterMappings)
			{
				const FString Context = FString::Printf(TEXT("Mapping provenance and coverage are populated for %s"), *Mapping.ArtifactFieldName);
				TestTrue(*Context, !Mapping.ArtifactFieldName.IsEmpty());
				AssertNonEmptyStringList(*this, TEXT("Source provenance list is populated"), Mapping.SourceProvenance);
				AssertNonEmptyStringList(*this, TEXT("Test coverage list is populated"), Mapping.TestCoverageNames);
			}
		}

		return true;
	}
}

#endif
