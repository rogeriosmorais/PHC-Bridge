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
			Snapshot.bIsBridgeActive = true;
			Snapshot.bAllCriticalBodiesSimulating = false;

			const FPhysAnimContinuityValidationResult Result = PhysAnimValidators::ValidateContinuity(Snapshot);

			TestFalse(TEXT("VALID-01A physical_continuity_validator_passed is false"), Result.bPhysicalContinuityValidatorPassed);
			TestEqual(TEXT("VALID-01A terminal_reason is activation_continuous_simulation_lost"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationContinuousSimulationLost));
		}

		{
			// VALID-01B: Pelvis sleep limit exceeded.
			FPhysAnimContinuitySnapshot Snapshot;
			Snapshot.bIsBridgeActive = true;
			Snapshot.PelvisSleepDurationMs = 100.1;

			const FPhysAnimContinuityValidationResult Result = PhysAnimValidators::ValidateContinuity(Snapshot);

			TestTrue(TEXT("VALID-01B pelvis_sleep_duration_ms exceeds limit"), Result.PelvisSleepDurationMs > 100.0);
			TestFalse(TEXT("VALID-01B physical_continuity_validator_passed is false"), Result.bPhysicalContinuityValidatorPassed);
			TestEqual(TEXT("VALID-01B terminal_reason is activation_continuous_simulation_lost"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationContinuousSimulationLost));
		}

		{
			// VALID-01C: Body instance loss.
			FPhysAnimContinuitySnapshot Snapshot;
			Snapshot.bIsBridgeActive = true;
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
			Snapshot.bIsBridgeActive = true;
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
			Snapshot.bIsBridgeActive = true;

			const FPhysAnimCapsuleContractValidationResult Result = PhysAnimValidators::ValidateCapsule(Snapshot);

			TestTrue(TEXT("VALID-02A capsule_lock_delta_cm exceeds threshold"), Result.CapsuleLockDeltaCm > 0.01);
			TestFalse(TEXT("VALID-02A capsule contract fails"), Result.bCapsuleContractPassed);
			TestEqual(TEXT("VALID-02A terminal_reason is activation_capsule_contract_violation"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationCapsuleContractViolation));
		}

		{
			// VALID-02B: Capsule collision active.
			FPhysAnimCapsuleContractSnapshot Snapshot;
			Snapshot.CapsuleCollisionEnabled = EPhysAnimCapsuleCollisionState::CollisionEnabled;
			Snapshot.bIsBridgeActive = true;

			const FPhysAnimCapsuleContractValidationResult Result = PhysAnimValidators::ValidateCapsule(Snapshot);

			TestEqual(TEXT("VALID-02B capsule_collision_enabled is not NoCollision"), static_cast<uint8>(Result.CapsuleCollisionEnabled), static_cast<uint8>(EPhysAnimCapsuleCollisionState::CollisionEnabled));
			TestFalse(TEXT("VALID-02B capsule contract fails"), Result.bCapsuleContractPassed);
			TestEqual(TEXT("VALID-02B terminal_reason is activation_capsule_contract_violation"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationCapsuleContractViolation));
		}

		{
			// VALID-02C: CMC active/ticking.
			FPhysAnimCapsuleContractSnapshot ActiveSnapshot;
			ActiveSnapshot.bCmcIsActive = true;
			ActiveSnapshot.bIsBridgeActive = true;

			const FPhysAnimCapsuleContractValidationResult ActiveResult = PhysAnimValidators::ValidateCapsule(ActiveSnapshot);

			TestTrue(TEXT("VALID-02C cmc_is_active is true"), ActiveResult.bCmcIsActive);
			TestFalse(TEXT("VALID-02C active CMC fails capsule contract"), ActiveResult.bCapsuleContractPassed);
			TestEqual(TEXT("VALID-02C active CMC terminal_reason is activation_capsule_contract_violation"), static_cast<uint8>(ActiveResult.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationCapsuleContractViolation));

			FPhysAnimCapsuleContractSnapshot TickingSnapshot;
			TickingSnapshot.bCmcTickEnabled = true;
			TickingSnapshot.bIsBridgeActive = true;

			const FPhysAnimCapsuleContractValidationResult TickingResult = PhysAnimValidators::ValidateCapsule(TickingSnapshot);

			TestTrue(TEXT("VALID-02C cmc_tick_enabled is true"), TickingResult.bCmcTickEnabled);
			TestFalse(TEXT("VALID-02C ticking CMC fails capsule contract"), TickingResult.bCapsuleContractPassed);
			TestEqual(TEXT("VALID-02C ticking CMC terminal_reason is activation_capsule_contract_violation"), static_cast<uint8>(TickingResult.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationCapsuleContractViolation));
		}

		{
			// VALID-02D: UpdatedComponent still owned.
			FPhysAnimCapsuleContractSnapshot Snapshot;
			Snapshot.bCmcUpdatedComponentIsNull = false;
			Snapshot.bIsBridgeActive = true;

			const FPhysAnimCapsuleContractValidationResult Result = PhysAnimValidators::ValidateCapsule(Snapshot);

			TestFalse(TEXT("VALID-02D cmc_updated_component_is_null is false"), Result.bCmcUpdatedComponentIsNull);
			TestFalse(TEXT("VALID-02D capsule contract fails"), Result.bCapsuleContractPassed);
			TestEqual(TEXT("VALID-02D terminal_reason is activation_capsule_contract_violation"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationCapsuleContractViolation));
		}

		{
			FPhysAnimCapsuleContractSnapshot Snapshot;
			Snapshot.bCapsuleGenerateOverlapEvents = true;
			Snapshot.bIsBridgeActive = true;

			const FPhysAnimCapsuleContractValidationResult Result = PhysAnimValidators::ValidateCapsule(Snapshot);

			TestTrue(TEXT("Overlap generation is represented in capsule result"), Result.bCapsuleGenerateOverlapEvents);
			TestFalse(TEXT("Overlap generation fails capsule contract"), Result.bCapsuleContractPassed);
			TestEqual(TEXT("Overlap generation terminal_reason is activation_capsule_contract_violation"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationCapsuleContractViolation));
		}

		{
			FPhysAnimCapsuleContractSnapshot Snapshot;
			Snapshot.bMeshUsesAbsoluteLocation = false;
			Snapshot.bIsBridgeActive = true;

			const FPhysAnimCapsuleContractValidationResult Result = PhysAnimValidators::ValidateCapsule(Snapshot);

			TestFalse(TEXT("Mesh absolute transform violation is represented"), Result.bMeshUsesAbsoluteLocation);
			TestFalse(TEXT("Mesh absolute transform violation fails capsule contract"), Result.bCapsuleContractPassed);
			TestEqual(TEXT("Mesh absolute transform terminal_reason is activation_capsule_contract_violation"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationCapsuleContractViolation));
		}

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimValidatorsPlantTest,
		"PhysAnim.Validators.Plant.ValidatePlant",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimValidatorsPlantTest::RunTest(const FString& Parameters)
	{
		{
			const FPhysAnimPlantContractSnapshot Snapshot;
			const FPhysAnimPlantContractValidationResult Result = PhysAnimValidators::ValidatePlant(Snapshot);

			TestTrue(TEXT("Default physics_asset_contract_valid is true"), Result.bPhysicsAssetContractValid);
			TestEqual(TEXT("Default plant_failure_class is None"), static_cast<uint8>(Result.PlantFailureClass), static_cast<uint8>(EPhysAnimPlantFailureClass::None));
			TestEqual(TEXT("Default terminal_reason is None"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::None));
		}

		{
			// VALID-03A: Skeleton mismatch.
			FPhysAnimPlantContractSnapshot Snapshot;
			Snapshot.bSkeletonAuditPassed = false;
			Snapshot.PlantFailureClass = EPhysAnimPlantFailureClass::StaticStructural;
			Snapshot.PlantFailureField = EPhysAnimPlantFailureField::Skeleton;

			const FPhysAnimPlantContractValidationResult Result = PhysAnimValidators::ValidatePlant(Snapshot);

			TestEqual(TEXT("VALID-03A plant_failure_class is StaticStructural"), static_cast<uint8>(Result.PlantFailureClass), static_cast<uint8>(EPhysAnimPlantFailureClass::StaticStructural));
			TestEqual(TEXT("VALID-03A terminal_reason is activation_physics_asset_contract_violation"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationPhysicsAssetContractViolation));
		}

		{
			// VALID-03B: Segment length drift.
			FPhysAnimPlantContractSnapshot Snapshot;
			Snapshot.bPhysicsAssetContractValid = false;
			Snapshot.PlantFailureClass = EPhysAnimPlantFailureClass::StaticStructural;
			Snapshot.PlantFailureField = EPhysAnimPlantFailureField::SegmentLength;

			const FPhysAnimPlantContractValidationResult Result = PhysAnimValidators::ValidatePlant(Snapshot);

			TestFalse(TEXT("VALID-03B physics_asset_contract_valid is false"), Result.bPhysicsAssetContractValid);
			TestEqual(TEXT("VALID-03B plant_failure_class is StaticStructural"), static_cast<uint8>(Result.PlantFailureClass), static_cast<uint8>(EPhysAnimPlantFailureClass::StaticStructural));
			TestEqual(TEXT("VALID-03B plant_failure_field is segment_length"), static_cast<uint8>(Result.PlantFailureField), static_cast<uint8>(EPhysAnimPlantFailureField::SegmentLength));
			TestEqual(TEXT("VALID-03B terminal_reason is activation_physics_asset_contract_violation"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationPhysicsAssetContractViolation));

			Snapshot.PlantFailureField = EPhysAnimPlantFailureField::AxisAlignment;

			const FPhysAnimPlantContractValidationResult AxisResult = PhysAnimValidators::ValidatePlant(Snapshot);

			TestEqual(TEXT("VALID-03B plant_failure_field can be axis_alignment"), static_cast<uint8>(AxisResult.PlantFailureField), static_cast<uint8>(EPhysAnimPlantFailureField::AxisAlignment));
			TestEqual(TEXT("VALID-03B axis terminal_reason is activation_physics_asset_contract_violation"), static_cast<uint8>(AxisResult.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationPhysicsAssetContractViolation));
		}

		{
			// VALID-03C: Mass mutation.
			FPhysAnimPlantContractSnapshot Snapshot;
			Snapshot.PlantFailureClass = EPhysAnimPlantFailureClass::Mutation;
			Snapshot.PlantFailureField = EPhysAnimPlantFailureField::Mass;
			Snapshot.MassDriftTotalPct = 3.0;

			const FPhysAnimPlantContractValidationResult Result = PhysAnimValidators::ValidatePlant(Snapshot);

			TestEqual(TEXT("VALID-03C plant_failure_class is Mutation"), static_cast<uint8>(Result.PlantFailureClass), static_cast<uint8>(EPhysAnimPlantFailureClass::Mutation));
			TestEqual(TEXT("VALID-03C plant_failure_field is mass"), static_cast<uint8>(Result.PlantFailureField), static_cast<uint8>(EPhysAnimPlantFailureField::Mass));
			TestEqual(TEXT("VALID-03C mass drift is preserved"), Result.MassDriftTotalPct, 3.0);
			TestEqual(TEXT("VALID-03C terminal_reason is activation_physics_asset_contract_violation"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationPhysicsAssetContractViolation));
		}

		{
			// VALID-03D: Physics asset swap.
			FPhysAnimPlantContractSnapshot Snapshot;
			Snapshot.bPhysicsAssetContractValid = false;
			Snapshot.PlantFailureClass = EPhysAnimPlantFailureClass::Mutation;
			Snapshot.PlantFailureField = EPhysAnimPlantFailureField::PhysicsAssetIdentity;

			const FPhysAnimPlantContractValidationResult Result = PhysAnimValidators::ValidatePlant(Snapshot);

			TestFalse(TEXT("VALID-03D physics_asset_contract_valid is false"), Result.bPhysicsAssetContractValid);
			TestEqual(TEXT("VALID-03D plant_failure_class is Mutation"), static_cast<uint8>(Result.PlantFailureClass), static_cast<uint8>(EPhysAnimPlantFailureClass::Mutation));
			TestEqual(TEXT("VALID-03D plant_failure_field is physics_asset_identity"), static_cast<uint8>(Result.PlantFailureField), static_cast<uint8>(EPhysAnimPlantFailureField::PhysicsAssetIdentity));
			TestEqual(TEXT("VALID-03D terminal_reason is activation_physics_asset_contract_violation"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationPhysicsAssetContractViolation));
		}

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimValidatorsAuthorityTest,
		"PhysAnim.Validators.Authority.ValidateAuthority",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimValidatorsAuthorityTest::RunTest(const FString& Parameters)
	{
		{
			const FPhysAnimAuthoritySnapshot Snapshot;
			const FPhysAnimAuthorityValidationResult Result = PhysAnimValidators::ValidateAuthority(Snapshot);

			TestTrue(TEXT("Default authority snapshot passes"), Result.bAuthorityPassed);
			TestEqual(TEXT("Default contamination_class is None"), static_cast<uint8>(Result.ContaminationClass), static_cast<uint8>(EPhysAnimContaminationClass::None));
			TestEqual(TEXT("Default terminal_reason is None"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::None));
		}

		{
			// VALID-04A: Mesh-wide assist.
			FPhysAnimAuthoritySnapshot Snapshot;
			Snapshot.AuthorityConflictCount = 1;
			Snapshot.ContaminationClass = EPhysAnimContaminationClass::MeshWideAssist;
			Snapshot.ContaminationSourceSubsystem = TEXT("ExternalBlend");
			Snapshot.bMeshWideAssistDetected = true;

			const FPhysAnimAuthorityValidationResult Result = PhysAnimValidators::ValidateAuthority(Snapshot);

			TestEqual(TEXT("VALID-04A contamination_class is mesh_wide_assist"), static_cast<uint8>(Result.ContaminationClass), static_cast<uint8>(EPhysAnimContaminationClass::MeshWideAssist));
			TestEqual(TEXT("VALID-04A terminal_reason is activation_authority_conflict"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationAuthorityConflict));
		}

		{
			// VALID-04B: Non-critical body assist.
			FPhysAnimAuthoritySnapshot Snapshot;
			Snapshot.AuthorityConflictCount = 1;
			Snapshot.ContaminationClass = EPhysAnimContaminationClass::NonCriticalBodyAssist;
			Snapshot.ContaminationSourceBody = TEXT("calf_l");
			Snapshot.ContaminationSourceSubsystem = TEXT("ExternalAssist");

			const FPhysAnimAuthorityValidationResult Result = PhysAnimValidators::ValidateAuthority(Snapshot);

			TestEqual(TEXT("VALID-04B contamination_class is non_critical_body_assist"), static_cast<uint8>(Result.ContaminationClass), static_cast<uint8>(EPhysAnimContaminationClass::NonCriticalBodyAssist));
			TestEqual(TEXT("VALID-04B terminal_reason is activation_authority_conflict"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationAuthorityConflict));
		}

		{
			// VALID-04C: Calf or excluded world brace.
			FPhysAnimAuthoritySnapshot Snapshot;
			Snapshot.AuthorityConflictCount = 1;
			Snapshot.ContaminationClass = EPhysAnimContaminationClass::ExcludedBodyWorldBrace;
			Snapshot.ContaminationSourceBody = TEXT("calf_r");
			Snapshot.ContaminationSourceSubsystem = TEXT("WorldContact");

			const FPhysAnimAuthorityValidationResult Result = PhysAnimValidators::ValidateAuthority(Snapshot);

			TestEqual(TEXT("VALID-04C contamination_class is excluded_body_world_brace"), static_cast<uint8>(Result.ContaminationClass), static_cast<uint8>(EPhysAnimContaminationClass::ExcludedBodyWorldBrace));
			TestEqual(TEXT("VALID-04C terminal_reason is activation_authority_conflict"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationAuthorityConflict));
		}

		{
			// VALID-04D: Global blend/kinematic assist.
			FPhysAnimAuthoritySnapshot Snapshot;
			Snapshot.AuthorityConflictCount = 1;
			Snapshot.ContaminationClass = EPhysAnimContaminationClass::GlobalBlendOrKinematicAssist;
			Snapshot.ContaminationSourceSubsystem = TEXT("KinematicAssist");

			const FPhysAnimAuthorityValidationResult Result = PhysAnimValidators::ValidateAuthority(Snapshot);

			TestEqual(TEXT("VALID-04D contamination_class is global_blend_or_kinematic_assist"), static_cast<uint8>(Result.ContaminationClass), static_cast<uint8>(EPhysAnimContaminationClass::GlobalBlendOrKinematicAssist));
			TestEqual(TEXT("VALID-04D terminal_reason is activation_authority_conflict"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationAuthorityConflict));
		}

		{
			// VALID-06A: CMC correction path runs.
			FPhysAnimMovementReclaimSnapshot Snapshot;
			Snapshot.MovementReclaimCount = 1;

			const FPhysAnimMovementReclaimValidationResult Result = PhysAnimValidators::ValidateMovementReclaim(Snapshot);

			TestTrue(TEXT("VALID-06A movement_reclaim_count is greater than zero"), Result.MovementReclaimCount > 0);
			TestFalse(TEXT("VALID-06A movement reclaim validation fails"), Result.bMovementReclaimPassed);
			TestEqual(TEXT("VALID-06A terminal_reason is activation_movement_reclaim"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationMovementReclaim));
		}

		{
			// VALID-06B: Shell helper writes during activation.
			FPhysAnimShellHelperSnapshot Snapshot;
			Snapshot.ShellHelperUsedCount = 1;

			const FPhysAnimShellHelperValidationResult Result = PhysAnimValidators::ValidateShellHelper(Snapshot);

			TestTrue(TEXT("VALID-06B shell_helper_used_count is greater than zero"), Result.ShellHelperUsedCount > 0);
			TestFalse(TEXT("VALID-06B shell helper validation fails"), Result.bShellHelperPassed);
			TestEqual(TEXT("VALID-06B terminal_reason is activation_shell_helper_violation"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationShellHelperViolation));
		}

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimValidatorsControllerStabilityTest,
		"PhysAnim.Validators.ControllerStability.ValidateControllerStability",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimValidatorsControllerStabilityTest::RunTest(const FString& Parameters)
	{
		{
			const FPhysAnimControllerStabilitySnapshot Snapshot;
			const FPhysAnimControllerStabilityValidationResult Result = PhysAnimValidators::ValidateControllerStability(Snapshot);

			TestTrue(TEXT("Default controller stability passes"), Result.bControllerStabilityPassed);
			TestEqual(TEXT("Default failure field is None"), static_cast<uint8>(Result.FailureField), static_cast<uint8>(EPhysAnimControllerStabilityFailureField::None));
			TestEqual(TEXT("Default terminal_reason is None"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::None));
		}

		{
			// VALID-05A: Target jump at blend start.
			FPhysAnimControllerStabilitySnapshot Snapshot;
			Snapshot.TargetDiscontinuityDeg = 15.1;
			Snapshot.TargetDiscontinuityPhase = EPhysAnimTargetDiscontinuityPhase::BlendStart;

			const FPhysAnimControllerStabilityValidationResult Result = PhysAnimValidators::ValidateControllerStability(Snapshot);

			TestTrue(TEXT("VALID-05A target_discontinuity_deg exceeds limit"), Result.TargetDiscontinuityDeg > 15.0);
			TestEqual(TEXT("VALID-05A target_discontinuity_phase is BlendStart"), static_cast<uint8>(Result.TargetDiscontinuityPhase), static_cast<uint8>(EPhysAnimTargetDiscontinuityPhase::BlendStart));
			TestEqual(TEXT("VALID-05A terminal_reason is activation_target_discontinuity"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationTargetDiscontinuity));
		}

		{
			// VALID-05B: Controller gain breach.
			FPhysAnimControllerStabilitySnapshot Snapshot;
			Snapshot.ControllerGainScale = 1.1;

			const FPhysAnimControllerStabilityValidationResult Result = PhysAnimValidators::ValidateControllerStability(Snapshot);

			TestFalse(TEXT("VALID-05B controller_gain_damping_valid is false"), Result.bControllerGainDampingValid);
			TestTrue(TEXT("VALID-05B controller_gain_scale exceeds max"), Result.ControllerGainScale > 1.0);
			TestEqual(TEXT("VALID-05B failure field is controller_gain_scale"), static_cast<uint8>(Result.FailureField), static_cast<uint8>(EPhysAnimControllerStabilityFailureField::ControllerGainScale));
			TestEqual(TEXT("VALID-05B terminal_reason is activation_unstable_gain_or_damping"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationUnstableGainOrDamping));
		}

		{
			// VALID-05C: Controller damping breach.
			FPhysAnimControllerStabilitySnapshot Snapshot;
			Snapshot.ControllerDampingRatio = 0.9;

			const FPhysAnimControllerStabilityValidationResult Result = PhysAnimValidators::ValidateControllerStability(Snapshot);

			TestFalse(TEXT("VALID-05C controller_gain_damping_valid is false"), Result.bControllerGainDampingValid);
			TestTrue(TEXT("VALID-05C controller_damping_ratio below min"), Result.ControllerDampingRatio < 1.0);
			TestEqual(TEXT("VALID-05C failure field is controller_damping_ratio"), static_cast<uint8>(Result.FailureField), static_cast<uint8>(EPhysAnimControllerStabilityFailureField::ControllerDampingRatio));
			TestEqual(TEXT("VALID-05C terminal_reason is activation_unstable_gain_or_damping"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationUnstableGainOrDamping));
		}

		{
			// VALID-05D: Root tilt breach.
			FPhysAnimControllerStabilitySnapshot Snapshot;
			Snapshot.MaxRootTiltDeg = 20.1;

			const FPhysAnimControllerStabilityValidationResult Result = PhysAnimValidators::ValidateControllerStability(Snapshot);

			TestTrue(TEXT("VALID-05D max_root_tilt_deg exceeds limit"), Result.MaxRootTiltDeg > 20.0);
			TestEqual(TEXT("VALID-05D failure field is max_root_tilt_deg"), static_cast<uint8>(Result.FailureField), static_cast<uint8>(EPhysAnimControllerStabilityFailureField::MaxRootTiltDeg));
			TestEqual(TEXT("VALID-05D terminal_reason is activation_instability_threshold_breach"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationInstabilityThresholdBreach));
		}

		{
			// VALID-05E: Angular speed breach.
			FPhysAnimControllerStabilitySnapshot Snapshot;
			Snapshot.PeakAngularSpeedDegPerSec = 720.1;

			const FPhysAnimControllerStabilityValidationResult Result = PhysAnimValidators::ValidateControllerStability(Snapshot);

			TestTrue(TEXT("VALID-05E peak_angular_speed exceeds limit"), Result.PeakAngularSpeedDegPerSec > 720.0);
			TestEqual(TEXT("VALID-05E failure field is peak_angular_speed"), static_cast<uint8>(Result.FailureField), static_cast<uint8>(EPhysAnimControllerStabilityFailureField::PeakAngularSpeed));
			TestEqual(TEXT("VALID-05E terminal_reason is activation_instability_threshold_breach"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationInstabilityThresholdBreach));
		}

		{
			// VALID-05F: Max-body mismatch over grace.
			FPhysAnimControllerStabilitySnapshot Snapshot;
			Snapshot.MaxBodyMismatchDeg = 25.1;
			Snapshot.MismatchDurationMs = 200.1;

			const FPhysAnimControllerStabilityValidationResult Result = PhysAnimValidators::ValidateControllerStability(Snapshot);

			TestTrue(TEXT("VALID-05F max_body_mismatch_deg exceeds limit"), Result.MaxBodyMismatchDeg > 25.0);
			TestTrue(TEXT("VALID-05F mismatch_duration_ms exceeds grace"), Result.MismatchDurationMs > 200.0);
			TestEqual(TEXT("VALID-05F failure field is max_body_mismatch_deg"), static_cast<uint8>(Result.FailureField), static_cast<uint8>(EPhysAnimControllerStabilityFailureField::MaxBodyMismatchDeg));
			TestEqual(TEXT("VALID-05F terminal_reason is activation_pose_reference_mismatch"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationPoseReferenceMismatch));
		}

		{
			// VALID-05G: RMS-chain mismatch over grace.
			FPhysAnimControllerStabilitySnapshot Snapshot;
			Snapshot.RmsMismatchDeg = 15.1;
			Snapshot.MismatchDurationMs = 200.1;

			const FPhysAnimControllerStabilityValidationResult Result = PhysAnimValidators::ValidateControllerStability(Snapshot);

			TestTrue(TEXT("VALID-05G rms_mismatch_deg exceeds limit"), Result.RmsMismatchDeg > 15.0);
			TestTrue(TEXT("VALID-05G mismatch_duration_ms exceeds grace"), Result.MismatchDurationMs > 200.0);
			TestEqual(TEXT("VALID-05G failure field is rms_mismatch_deg"), static_cast<uint8>(Result.FailureField), static_cast<uint8>(EPhysAnimControllerStabilityFailureField::RmsMismatchDeg));
			TestEqual(TEXT("VALID-05G terminal_reason is activation_pose_reference_mismatch"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationPoseReferenceMismatch));
		}

		{
			// VALID-05H: Standing validation timeout.
			FPhysAnimControllerStabilitySnapshot Snapshot;
			Snapshot.HoldDurationSec = 2.9;
			Snapshot.bStandingValidationTimedOut = true;

			const FPhysAnimControllerStabilityValidationResult Result = PhysAnimValidators::ValidateControllerStability(Snapshot);

			TestTrue(TEXT("VALID-05H hold_duration_sec is below success threshold"), Result.HoldDurationSec < 3.0);
			TestTrue(TEXT("VALID-05H standing_validation_timed_out is true"), Result.bStandingValidationTimedOut);
			TestEqual(TEXT("VALID-05H terminal_reason is activation_standing_validation_timeout"), static_cast<uint8>(Result.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationStandingValidationTimeout));
		}

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimValidatorsArtifactSnapshotTest,
		"PhysAnim.Validators.ArtifactSnapshot.BuildRunArtifactSnapshot",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimValidatorsArtifactSnapshotTest::RunTest(const FString& Parameters)
	{
		{
			FPhysAnimRunArtifactSnapshotInput Input;
			Input.Values.AttemptUuid = TEXT("attempt-001");
			Input.Values.Timestamp = 12.5;
			Input.Values.BaselineId = TEXT("baseline-001");
			Input.Values.StandingReferenceId = TEXT("stand-001");
			Input.Values.ProxyInsideHull = TOptional<bool>();
			Input.Values.ProxyOutsideHullDurationMs = TOptional<double>();
			Input.Values.PolicyInferenceSuccessCount = 7;
			Input.Values.PolicyActionSampleCount = 6;
			Input.Values.PolicyActionRawMeanAbsMax = 0.35;
			Input.Values.PolicyActionConditionedMeanAbsMax = 0.21;
			Input.Values.PolicyActionClampedFloatMax = 2;
			Input.Values.ControlTargetSampleCount = 5;
			Input.Values.ControlTargetNormalWrites = 84;
			Input.Values.ControlTargetTotalWrites = 89;
			Input.Values.ControlTargetMaxDeltaDeg = 4.5;
			Input.Values.ControlTargetMeanDeltaDegMax = 1.25;
			Input.Values.ControlTargetMaxRawPolicyOffsetDeg = 9.5;
			Input.Values.ControlTargetMeanRawPolicyOffsetDegMax = 2.75;
			Input.Values.RuntimeBodySampleCount = 22;
			Input.Values.RuntimeSimulatingBodyCount = 14;
			Input.Values.RuntimeMaxBodyLinearSpeedCmPerSecond = 33.0;
			Input.Values.RuntimeMaxBodyAngularSpeedDegPerSecond = 44.0;
			Input.Values.RendererFacingMotionSampleCount = 3;
			Input.Values.RendererFacingMotionActiveSampleCount = 1;
			Input.Values.RendererFacingMotionMaxRootWorldPositionDriftCm = 12.5;
			Input.Values.bPhysicalPerturbationApplied = true;
			Input.Values.PerturbationMeasuredDeltaVCmPerSecond = 12.0;

			const FPhysAnimRunArtifactSnapshot Snapshot = PhysAnimValidators::BuildRunArtifactSnapshot(Input);

			TestEqual(TEXT("Artifact preserves attempt_uuid"), Snapshot.AttemptUuid, FString(TEXT("attempt-001")));
			TestEqual(TEXT("Artifact preserves timestamp"), Snapshot.Timestamp, 12.5);
			TestEqual(TEXT("Artifact preserves baseline_id"), Snapshot.BaselineId, FString(TEXT("baseline-001")));
			TestEqual(TEXT("Artifact preserves standing_reference_id"), Snapshot.StandingReferenceId, FString(TEXT("stand-001")));
			TestFalse(TEXT("Artifact preserves nullable proxy_inside_hull"), Snapshot.ProxyInsideHull.IsSet());
			TestFalse(TEXT("Artifact preserves nullable proxy_outside_hull_duration_ms"), Snapshot.ProxyOutsideHullDurationMs.IsSet());
			TestEqual(TEXT("Artifact preserves policy_inference_success_count"), Snapshot.PolicyInferenceSuccessCount, 7);
			TestEqual(TEXT("Artifact preserves policy_action_sample_count"), Snapshot.PolicyActionSampleCount, 6);
			TestEqual(TEXT("Artifact preserves policy_action_raw_mean_abs_max"), Snapshot.PolicyActionRawMeanAbsMax, 0.35);
			TestEqual(TEXT("Artifact preserves policy_action_conditioned_mean_abs_max"), Snapshot.PolicyActionConditionedMeanAbsMax, 0.21);
			TestEqual(TEXT("Artifact preserves policy_action_clamped_float_max"), Snapshot.PolicyActionClampedFloatMax, 2);
			TestEqual(TEXT("Artifact preserves control_target_sample_count"), Snapshot.ControlTargetSampleCount, 5);
			TestEqual(TEXT("Artifact preserves control_target_normal_writes"), Snapshot.ControlTargetNormalWrites, 84);
			TestEqual(TEXT("Artifact preserves control_target_total_writes"), Snapshot.ControlTargetTotalWrites, 89);
			TestEqual(TEXT("Artifact preserves control_target_max_delta_deg"), Snapshot.ControlTargetMaxDeltaDeg, 4.5);
			TestEqual(TEXT("Artifact preserves control_target_mean_delta_deg_max"), Snapshot.ControlTargetMeanDeltaDegMax, 1.25);
			TestEqual(TEXT("Artifact preserves control_target_max_raw_policy_offset_deg"), Snapshot.ControlTargetMaxRawPolicyOffsetDeg, 9.5);
			TestEqual(TEXT("Artifact preserves control_target_mean_raw_policy_offset_deg_max"), Snapshot.ControlTargetMeanRawPolicyOffsetDegMax, 2.75);
			TestEqual(TEXT("Artifact preserves runtime_body_sample_count"), Snapshot.RuntimeBodySampleCount, 22);
			TestEqual(TEXT("Artifact preserves runtime_simulating_body_count"), Snapshot.RuntimeSimulatingBodyCount, 14);
			TestEqual(TEXT("Artifact preserves runtime_max_body_linear_speed_cm_per_second"), Snapshot.RuntimeMaxBodyLinearSpeedCmPerSecond, 33.0);
			TestEqual(TEXT("Artifact preserves runtime_max_body_angular_speed_deg_per_second"), Snapshot.RuntimeMaxBodyAngularSpeedDegPerSecond, 44.0);
			TestEqual(TEXT("Artifact preserves renderer_facing_motion_sample_count"), Snapshot.RendererFacingMotionSampleCount, 3);
			TestEqual(TEXT("Artifact preserves renderer_facing_motion_active_sample_count"), Snapshot.RendererFacingMotionActiveSampleCount, 1);
			TestEqual(TEXT("Artifact preserves renderer_facing_motion_max_root_world_position_drift_cm"), Snapshot.RendererFacingMotionMaxRootWorldPositionDriftCm, 12.5);
			TestTrue(TEXT("Artifact preserves physical_perturbation_applied"), Snapshot.bPhysicalPerturbationApplied);
			TestEqual(TEXT("Artifact preserves perturbation_measured_delta_v_cm_per_second"), Snapshot.PerturbationMeasuredDeltaVCmPerSecond, 12.0);
			TestEqual(TEXT("Artifact default terminal_reason is None/null"), static_cast<uint8>(Snapshot.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::None));
		}

		{
			FPhysAnimRunArtifactSnapshotInput Input;
			Input.Plant.bPhysicsAssetContractValid = false;
			Input.Plant.bSkeletonAuditPassed = false;
			Input.Plant.PlantFailureClass = EPhysAnimPlantFailureClass::StaticStructural;
			Input.Plant.PlantFailureField = EPhysAnimPlantFailureField::Skeleton;
			Input.Plant.MassDriftTotalPct = 3.0;
			Input.Plant.TerminalReason = EPhysAnimTerminalReason::ActivationPhysicsAssetContractViolation;

			Input.Capsule.CapsuleCollisionEnabled = EPhysAnimCapsuleCollisionState::CollisionEnabled;
			Input.Capsule.bCmcIsActive = true;
			Input.Capsule.bCmcUpdatedComponentIsNull = false;
			Input.Capsule.TerminalReason = EPhysAnimTerminalReason::ActivationCapsuleContractViolation;

			Input.Continuity.TopologyChangeCount = 2;
			Input.Continuity.bContinuityBookkeepingMismatch = true;
			Input.Continuity.PelvisSleepDurationMs = 125.0;
			Input.Continuity.bPhysicalContinuityValidatorPassed = false;
			Input.Continuity.TerminalReason = EPhysAnimTerminalReason::ActivationTopologyChange;

			Input.Authority.AuthorityConflictCount = 1;
			Input.Authority.ContaminationClass = EPhysAnimContaminationClass::ExcludedBodyWorldBrace;
			Input.Authority.ContaminationSourceBody = TEXT("calf_l");
			Input.Authority.ContaminationSourceSubsystem = TEXT("WorldContact");
			Input.Authority.TerminalReason = EPhysAnimTerminalReason::ActivationAuthorityConflict;

			Input.MovementReclaim.MovementReclaimCount = 1;
			Input.MovementReclaim.TerminalReason = EPhysAnimTerminalReason::ActivationMovementReclaim;
			Input.ShellHelper.ShellHelperUsedCount = 1;
			Input.ShellHelper.TerminalReason = EPhysAnimTerminalReason::ActivationShellHelperViolation;

			Input.ControllerStability.HoldDurationSec = 2.5;
			Input.ControllerStability.MaxRootTiltDeg = 21.0;
			Input.ControllerStability.PeakAngularSpeedDegPerSec = 721.0;
			Input.ControllerStability.RmsMismatchDeg = 16.0;
			Input.ControllerStability.MaxBodyMismatchDeg = 26.0;
			Input.ControllerStability.TargetDiscontinuityDeg = 16.0;
			Input.ControllerStability.TargetDiscontinuityPhase = EPhysAnimTargetDiscontinuityPhase::BlendStart;
			Input.ControllerStability.MismatchDurationMs = 201.0;
			Input.ControllerStability.ControllerGainScale = 1.2;
			Input.ControllerStability.ControllerDampingRatio = 0.8;
			Input.ControllerStability.bControllerGainDampingValid = false;
			Input.ControllerStability.FailureField = EPhysAnimControllerStabilityFailureField::ControllerGainScale;
			Input.ControllerStability.bStandingValidationTimedOut = true;
			Input.ControllerStability.TerminalReason = EPhysAnimTerminalReason::ActivationUnstableGainOrDamping;

			Input.Support.ProxyInsideHull = false;
			Input.Support.ProxyOutsideHullDurationMs = 101.0;
			Input.Values.SupportMode = EPhysAnimSupportMode::SingleFootSurvival;
			Input.Values.SupportChurnCount = 3;
			Input.Values.SupportChurnHz = 9.0;
			Input.Values.RendererFacingMotionSampleCount = 4;
			Input.Values.RendererFacingMotionActiveSampleCount = 2;
			Input.Values.RendererFacingMotionMaxRootWorldPositionDriftCm = 9.75;

			const FPhysAnimRunArtifactSnapshot Snapshot = PhysAnimValidators::BuildRunArtifactSnapshot(Input);

			TestFalse(TEXT("Artifact maps physics_asset_contract_valid"), Snapshot.bPhysicsAssetContractValid);
			TestFalse(TEXT("Artifact maps skeleton_audit_passed"), Snapshot.bSkeletonAuditPassed);
			TestEqual(TEXT("Artifact maps plant_failure_class"), static_cast<uint8>(Snapshot.PlantFailureClass), static_cast<uint8>(EPhysAnimPlantFailureClass::StaticStructural));
			TestEqual(TEXT("Artifact maps plant_failure_field"), static_cast<uint8>(Snapshot.PlantFailureField), static_cast<uint8>(EPhysAnimPlantFailureField::Skeleton));
			TestEqual(TEXT("Artifact maps capsule_collision_enabled"), static_cast<uint8>(Snapshot.CapsuleCollisionEnabled), static_cast<uint8>(EPhysAnimCapsuleCollisionState::CollisionEnabled));
			TestTrue(TEXT("Artifact maps cmc_is_active"), Snapshot.bCmcIsActive);
			TestFalse(TEXT("Artifact maps cmc_updated_component_is_null"), Snapshot.bCmcUpdatedComponentIsNull);
			TestEqual(TEXT("Artifact maps topology_change_count"), Snapshot.TopologyChangeCount, 2);
			TestTrue(TEXT("Artifact maps continuity_bookkeeping_mismatch"), Snapshot.bContinuityBookkeepingMismatch);
			TestFalse(TEXT("Artifact maps physical_continuity_validator_passed"), Snapshot.bPhysicalContinuityValidatorPassed);
			TestEqual(TEXT("Artifact maps contamination_class"), static_cast<uint8>(Snapshot.ContaminationClass), static_cast<uint8>(EPhysAnimContaminationClass::ExcludedBodyWorldBrace));
			TestEqual(TEXT("Artifact maps contamination_source_body"), Snapshot.ContaminationSourceBody, FName(TEXT("calf_l")));
			TestEqual(TEXT("Artifact maps excluded_body_world_contact_source"), Snapshot.ExcludedBodyWorldContactSource, FName(TEXT("calf_l")));
			TestEqual(TEXT("Artifact maps movement_reclaim_count"), Snapshot.MovementReclaimCount, 1);
			TestEqual(TEXT("Artifact maps shell_helper_used_count"), Snapshot.ShellHelperUsedCount, 1);
			TestEqual(TEXT("Artifact maps controller_stability_failure_field"), static_cast<uint8>(Snapshot.ControllerStabilityFailureField), static_cast<uint8>(EPhysAnimControllerStabilityFailureField::ControllerGainScale));
			TestEqual(TEXT("Artifact maps renderer_facing_motion_sample_count"), Snapshot.RendererFacingMotionSampleCount, 4);
			TestEqual(TEXT("Artifact maps renderer_facing_motion_active_sample_count"), Snapshot.RendererFacingMotionActiveSampleCount, 2);
			TestEqual(TEXT("Artifact maps renderer_facing_motion_max_root_world_position_drift_cm"), Snapshot.RendererFacingMotionMaxRootWorldPositionDriftCm, 9.75);
			TestTrue(TEXT("Artifact preserves proxy_inside_hull false"), Snapshot.ProxyInsideHull.IsSet() && !Snapshot.ProxyInsideHull.GetValue());
			TestEqual(TEXT("Artifact preserves proxy_outside_hull_duration_ms"), Snapshot.ProxyOutsideHullDurationMs.GetValue(), 101.0);
			TestEqual(TEXT("Artifact primary terminal_reason uses validator order"), static_cast<uint8>(Snapshot.TerminalReason), static_cast<uint8>(EPhysAnimTerminalReason::ActivationPhysicsAssetContractViolation));
			TestEqual(TEXT("Artifact records co_terminal_reasons"), Snapshot.CoTerminalReasons.Num(), 6);
		}

		return true;
	}
}
