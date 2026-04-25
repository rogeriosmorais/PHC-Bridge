#include "PhysAnimValidators.h"
#include "Misc/AutomationTest.h"

namespace
{
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimValidatorsContinuitySnapshotTest,
		"PhysAnim.Validators.ContinuitySnapshot",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimValidatorsContinuitySnapshotTest::RunTest(const FString& Parameters)
	{
		{
			const FPhysAnimContinuitySnapshot Snapshot;

			TestEqual(TEXT("Default topology_change_count is zero"), Snapshot.TopologyChangeCount, 0);
			TestTrue(TEXT("Default critical body instances are valid"), Snapshot.bAllCriticalBodiesValid);
			TestTrue(TEXT("Default critical body instances are simulating"), Snapshot.bAllCriticalBodiesSimulating);
			TestEqual(TEXT("Default pelvis_sleep_duration_ms is zero"), Snapshot.PelvisSleepDurationMs, 0.0);
			TestFalse(TEXT("Default continuity_bookkeeping_mismatch is false"), Snapshot.bContinuityBookkeepingMismatch);
		}

		{
			const FPhysAnimContinuityValidationResult Result;

			TestEqual(TEXT("Default result topology_change_count is zero"), Result.TopologyChangeCount, 0);
			TestFalse(TEXT("Default result continuity_bookkeeping_mismatch is false"), Result.bContinuityBookkeepingMismatch);
			TestEqual(TEXT("Default result pelvis_sleep_duration_ms is zero"), Result.PelvisSleepDurationMs, 0.0);
			TestTrue(TEXT("Default physical_continuity_validator_passed is true"), Result.bPhysicalContinuityValidatorPassed);
			TestEqual(TEXT("Default terminal_reason is None"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::None));
		}

		{
			FPhysAnimContinuitySnapshot Snapshot;
			Snapshot.TopologyChangeCount = 2;
			Snapshot.bAllCriticalBodiesValid = false;
			Snapshot.bAllCriticalBodiesSimulating = false;
			Snapshot.PelvisSleepDurationMs = 125.0;
			Snapshot.bContinuityBookkeepingMismatch = true;

			TestEqual(TEXT("Snapshot stores topology_change_count"), Snapshot.TopologyChangeCount, 2);
			TestFalse(TEXT("Snapshot stores invalid critical body state"), Snapshot.bAllCriticalBodiesValid);
			TestFalse(TEXT("Snapshot stores disabled simulation state"), Snapshot.bAllCriticalBodiesSimulating);
			TestEqual(TEXT("Snapshot stores pelvis_sleep_duration_ms"), Snapshot.PelvisSleepDurationMs, 125.0);
			TestTrue(TEXT("Snapshot stores continuity_bookkeeping_mismatch"), Snapshot.bContinuityBookkeepingMismatch);
		}

		{
			FPhysAnimContinuityValidationResult Result;
			Result.TopologyChangeCount = 1;
			Result.bContinuityBookkeepingMismatch = true;
			Result.PelvisSleepDurationMs = 125.0;
			Result.bPhysicalContinuityValidatorPassed = false;
			Result.TerminalReason = EPhysAnimTerminalReason::ActivationContinuousSimulationLost;

			TestEqual(TEXT("Result stores topology_change_count"), Result.TopologyChangeCount, 1);
			TestTrue(TEXT("Result stores continuity_bookkeeping_mismatch"), Result.bContinuityBookkeepingMismatch);
			TestEqual(TEXT("Result stores pelvis_sleep_duration_ms"), Result.PelvisSleepDurationMs, 125.0);
			TestFalse(TEXT("Result stores physical_continuity_validator_passed false"), Result.bPhysicalContinuityValidatorPassed);
			TestEqual(TEXT("Result stores terminal_reason"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationContinuousSimulationLost));
		}

		return true;
	}
}
