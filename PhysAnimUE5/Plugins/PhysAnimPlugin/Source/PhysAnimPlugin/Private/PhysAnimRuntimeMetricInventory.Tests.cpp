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

		TestEqual(TEXT("PoseSearch has two reused counter mappings"), PoseSearch->ExistingCounterMappings.Num(), 2);
		TestEqual(TEXT("RendererFacingMotion has three reused counter mappings"), RendererFacingMotion->ExistingCounterMappings.Num(), 3);
		TestEqual(TEXT("PhcPolicy has two reused counter mappings"), PhcPolicy->ExistingCounterMappings.Num(), 2);
		TestEqual(TEXT("PhysicsControl has two reused counter mappings"), PhysicsControl->ExistingCounterMappings.Num(), 2);
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
		TestNotNull(TEXT("PolicyInferenceSuccessCount mapping exists"), FindMapping(*PhcPolicy, TEXT("PolicyInferenceSuccessCount")));
		TestNotNull(TEXT("PolicyActionSampleCount mapping exists"), FindMapping(*PhcPolicy, TEXT("PolicyActionSampleCount")));
		TestNotNull(TEXT("ControlTargetTotalWrites mapping exists"), FindMapping(*PhysicsControl, TEXT("ControlTargetTotalWrites")));
		TestNotNull(TEXT("ControlTargetNormalWrites mapping exists"), FindMapping(*PhysicsControl, TEXT("ControlTargetNormalWrites")));

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
