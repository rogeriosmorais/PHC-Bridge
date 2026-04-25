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

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimValidatorsContinuityTest,
		"PhysAnim.Validators.Continuity.ValidateContinuity",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimValidatorsContinuityTest::RunTest(const FString& Parameters)
	{
		{
			// VALID-01A: Physics disabled.
			FPhysAnimContinuitySnapshot Snapshot;
			Snapshot.bAllCriticalBodiesSimulating = false;

			const FPhysAnimContinuityValidationResult Result = PhysAnimValidators::ValidateContinuity(Snapshot);

			TestFalse(TEXT("VALID-01A physical_continuity_validator_passed is false"), Result.bPhysicalContinuityValidatorPassed);
			TestEqual(TEXT("VALID-01A terminal_reason is activation_continuous_simulation_lost"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationContinuousSimulationLost));
		}

		{
			// VALID-01B: Pelvis sleep limit exceeded.
			FPhysAnimContinuitySnapshot Snapshot;
			Snapshot.PelvisSleepDurationMs = 100.1;

			const FPhysAnimContinuityValidationResult Result = PhysAnimValidators::ValidateContinuity(Snapshot);

			TestTrue(TEXT("VALID-01B pelvis_sleep_duration_ms exceeds limit"), Result.PelvisSleepDurationMs > 100.0);
			TestFalse(TEXT("VALID-01B physical_continuity_validator_passed is false"), Result.bPhysicalContinuityValidatorPassed);
			TestEqual(TEXT("VALID-01B terminal_reason is activation_continuous_simulation_lost"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationContinuousSimulationLost));
		}

		{
			// VALID-01C: Body instance loss.
			FPhysAnimContinuitySnapshot Snapshot;
			Snapshot.TopologyChangeCount = 1;

			const FPhysAnimContinuityValidationResult Result = PhysAnimValidators::ValidateContinuity(Snapshot);

			TestEqual(TEXT("VALID-01C terminal_reason is activation_topology_change"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationTopologyChange));
			TestFalse(TEXT("VALID-01C physical_continuity_validator_passed is false"), Result.bPhysicalContinuityValidatorPassed);
		}

		{
			// VALID-01D: Bookkeeping delta only.
			FPhysAnimContinuitySnapshot Snapshot;
			Snapshot.bContinuityBookkeepingMismatch = true;

			const FPhysAnimContinuityValidationResult Result = PhysAnimValidators::ValidateContinuity(Snapshot);

			TestEqual(TEXT("VALID-01D terminal_reason is nullptr/None"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::None));
			TestTrue(TEXT("VALID-01D continuity_bookkeeping_mismatch is true"), Result.bContinuityBookkeepingMismatch);
			TestTrue(TEXT("VALID-01D physical_continuity_validator_passed remains true"), Result.bPhysicalContinuityValidatorPassed);
		}

		{
			// Raw continuity loss remains authoritative over bookkeeping mismatch.
			FPhysAnimContinuitySnapshot Snapshot;
			Snapshot.bAllCriticalBodiesSimulating = false;
			Snapshot.bContinuityBookkeepingMismatch = true;

			const FPhysAnimContinuityValidationResult Result = PhysAnimValidators::ValidateContinuity(Snapshot);

			TestTrue(TEXT("Bookkeeping mismatch diagnostic is preserved"), Result.bContinuityBookkeepingMismatch);
			TestEqual(TEXT("Raw simulation loss wins over bookkeeping mismatch"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationContinuousSimulationLost));
			TestFalse(TEXT("Raw simulation loss fails continuity"), Result.bPhysicalContinuityValidatorPassed);
		}

		return true;
	}
}
