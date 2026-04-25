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

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimValidatorsCapsuleTest,
		"PhysAnim.Validators.Capsule.ValidateCapsule",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimValidatorsCapsuleTest::RunTest(const FString& Parameters)
	{
		{
			const FPhysAnimCapsuleContractSnapshot Snapshot;
			const FPhysAnimCapsuleContractValidationResult Result = PhysAnimValidators::ValidateCapsule(Snapshot);

			TestTrue(TEXT("Default capsule contract passes"), Result.bCapsuleContractPassed);
			TestEqual(TEXT("Default capsule terminal_reason is None"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::None));
		}

		{
			// VALID-02A: Actor moved.
			FPhysAnimCapsuleContractSnapshot Snapshot;
			Snapshot.CapsuleLockDeltaCm = 0.02;

			const FPhysAnimCapsuleContractValidationResult Result = PhysAnimValidators::ValidateCapsule(Snapshot);

			TestTrue(TEXT("VALID-02A capsule_lock_delta_cm exceeds threshold"), Result.CapsuleLockDeltaCm > 0.01);
			TestFalse(TEXT("VALID-02A capsule contract fails"), Result.bCapsuleContractPassed);
			TestEqual(TEXT("VALID-02A terminal_reason is activation_capsule_contract_violation"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationCapsuleContractViolation));
		}

		{
			// VALID-02B: Capsule collision active.
			FPhysAnimCapsuleContractSnapshot Snapshot;
			Snapshot.CapsuleCollisionEnabled = EPhysAnimCapsuleCollisionState::CollisionEnabled;

			const FPhysAnimCapsuleContractValidationResult Result = PhysAnimValidators::ValidateCapsule(Snapshot);

			TestEqual(TEXT("VALID-02B capsule_collision_enabled is not NoCollision"), static_cast<uint8>(Result.CapsuleCollisionEnabled), static_cast<uint8>(EPhysAnimCapsuleCollisionState::CollisionEnabled));
			TestFalse(TEXT("VALID-02B capsule contract fails"), Result.bCapsuleContractPassed);
			TestEqual(TEXT("VALID-02B terminal_reason is activation_capsule_contract_violation"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationCapsuleContractViolation));
		}

		{
			// VALID-02C: CMC active/ticking.
			FPhysAnimCapsuleContractSnapshot ActiveSnapshot;
			ActiveSnapshot.bCmcIsActive = true;

			const FPhysAnimCapsuleContractValidationResult ActiveResult = PhysAnimValidators::ValidateCapsule(ActiveSnapshot);

			TestTrue(TEXT("VALID-02C cmc_is_active is true"), ActiveResult.bCmcIsActive);
			TestFalse(TEXT("VALID-02C active CMC fails capsule contract"), ActiveResult.bCapsuleContractPassed);
			TestEqual(TEXT("VALID-02C active CMC terminal_reason is activation_capsule_contract_violation"), static_cast<uint8>(ActiveResult.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationCapsuleContractViolation));

			FPhysAnimCapsuleContractSnapshot TickingSnapshot;
			TickingSnapshot.bCmcTickEnabled = true;

			const FPhysAnimCapsuleContractValidationResult TickingResult = PhysAnimValidators::ValidateCapsule(TickingSnapshot);

			TestTrue(TEXT("VALID-02C cmc_tick_enabled is true"), TickingResult.bCmcTickEnabled);
			TestFalse(TEXT("VALID-02C ticking CMC fails capsule contract"), TickingResult.bCapsuleContractPassed);
			TestEqual(TEXT("VALID-02C ticking CMC terminal_reason is activation_capsule_contract_violation"), static_cast<uint8>(TickingResult.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationCapsuleContractViolation));
		}

		{
			// VALID-02D: UpdatedComponent still owned.
			FPhysAnimCapsuleContractSnapshot Snapshot;
			Snapshot.bCmcUpdatedComponentIsNull = false;

			const FPhysAnimCapsuleContractValidationResult Result = PhysAnimValidators::ValidateCapsule(Snapshot);

			TestFalse(TEXT("VALID-02D cmc_updated_component_is_null is false"), Result.bCmcUpdatedComponentIsNull);
			TestFalse(TEXT("VALID-02D capsule contract fails"), Result.bCapsuleContractPassed);
			TestEqual(TEXT("VALID-02D terminal_reason is activation_capsule_contract_violation"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationCapsuleContractViolation));
		}

		{
			FPhysAnimCapsuleContractSnapshot Snapshot;
			Snapshot.bCapsuleGenerateOverlapEvents = true;

			const FPhysAnimCapsuleContractValidationResult Result = PhysAnimValidators::ValidateCapsule(Snapshot);

			TestTrue(TEXT("Overlap generation is represented in capsule result"), Result.bCapsuleGenerateOverlapEvents);
			TestFalse(TEXT("Overlap generation fails capsule contract"), Result.bCapsuleContractPassed);
			TestEqual(TEXT("Overlap generation terminal_reason is activation_capsule_contract_violation"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationCapsuleContractViolation));
		}

		{
			FPhysAnimCapsuleContractSnapshot Snapshot;
			Snapshot.bMeshUsesAbsoluteLocation = false;

			const FPhysAnimCapsuleContractValidationResult Result = PhysAnimValidators::ValidateCapsule(Snapshot);

			TestFalse(TEXT("Mesh absolute transform violation is represented"), Result.bMeshUsesAbsoluteLocation);
			TestFalse(TEXT("Mesh absolute transform violation fails capsule contract"), Result.bCapsuleContractPassed);
			TestEqual(TEXT("Mesh absolute transform terminal_reason is activation_capsule_contract_violation"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationCapsuleContractViolation));
		}

		return true;
	}
}
