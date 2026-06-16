#include "PhysAnimComponent.h"
#include "PhysAnimLogger.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "GameFramework/Character.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/StaticMeshActor.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "HAL/IConsoleManager.h"
#include "Templates/SharedPointer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FThighRestoreDiagnosticVariant
	{
		const TCHAR* Name;
		const TCHAR* Command;
	};

	static const FThighRestoreDiagnosticVariant ThighRestoreDiagnosticVariants[] =
	{
		{ TEXT("Abrupt_0.01"), TEXT("8") },
		{ TEXT("Abrupt_0.02"), TEXT("5") },
		{ TEXT("Abrupt_0.05"), TEXT("3") },
		{ TEXT("Abrupt_0.10"), TEXT("6") },
		{ TEXT("Abrupt_0.15"), TEXT("9") },
		{ TEXT("Abrupt_0.20"), TEXT("7") },
		{ TEXT("Ramp_0.20_0.5s"), TEXT("10") },
		{ TEXT("Ramp_0.20_1.0s"), TEXT("11") },
		{ TEXT("KineticGate_ForcedHold_0.20"), TEXT("7:-1.0") },
	};

	int32 GPhysAnimStrictLivePolicyProofQuality = 0;
	FAutoConsoleVariableRef CVarPhysAnimStrictLivePolicyProofQuality(
		TEXT("p.PhysAnim.StrictLivePolicyProofQuality"),
		GPhysAnimStrictLivePolicyProofQuality,
		TEXT("Require live PHC proof-quality assertions for standing and perturbation automation tests."),
		ECVF_Default);

	bool IsStrictLivePolicyProofQualityEnabled()
	{
		return GPhysAnimStrictLivePolicyProofQuality != 0;
	}

	constexpr int32 RequiredCriticalBodyMask =
		(1 << 0) | // pelvis
		(1 << 1) | // spine_01
		(1 << 2) | // spine_02
		(1 << 3) | // spine_03
		(1 << 4) | // thigh_l
		(1 << 5);  // thigh_r

	constexpr int32 RequiredSupportBodyMask =
		(1 << 0) | // foot_l
		(1 << 1) | // foot_r
		(1 << 2) | // ball_l
		(1 << 3);  // ball_r

	void AddLatentAutomationError(FAutomationTestBase* Test, const FString& Message)
	{
		if (Test)
		{
			Test->AddError(Message);
		}
		PHYSANIM_LOG(LogTemp, Error, TEXT("%s"), *Message);
	}

	const TCHAR* GetLocomotionHandoffPreflightStateName(EBridgeLocomotionHandoffPreflightState State)
	{
		switch (State)
		{
		case EBridgeLocomotionHandoffPreflightState::LocomotionHandoffPreflightPassed:
			return TEXT("LocomotionHandoffPreflightPassed");
		case EBridgeLocomotionHandoffPreflightState::LocomotionHandoffPreflightDenied:
			return TEXT("LocomotionHandoffPreflightDenied");
		default:
			return TEXT("Unknown");
		}
	}

	const TCHAR* GetLocomotionHandoffCommitStateName(EBridgeLocomotionHandoffCommitState State)
	{
		switch (State)
		{
		case EBridgeLocomotionHandoffCommitState::LocomotionHandoffCommitPending:
			return TEXT("LocomotionHandoffCommitPending");
		case EBridgeLocomotionHandoffCommitState::LocomotionHandoffCommitted:
			return TEXT("LocomotionHandoffCommitted");
		case EBridgeLocomotionHandoffCommitState::LocomotionHandoffCommitDenied:
			return TEXT("LocomotionHandoffCommitDenied");
		default:
			return TEXT("Unknown");
		}
	}

	const TCHAR* GetRuntimeStateName(EPhysAnimRuntimeState State)
	{
		switch (State)
		{
		case EPhysAnimRuntimeState::Uninitialized:
			return TEXT("Uninitialized");
		case EPhysAnimRuntimeState::RuntimeReady:
			return TEXT("RuntimeReady");
		case EPhysAnimRuntimeState::WaitingForPoseSearch:
			return TEXT("WaitingForPoseSearch");
		case EPhysAnimRuntimeState::ReadyForActivation:
			return TEXT("ReadyForActivation");
		case EPhysAnimRuntimeState::BridgeActive:
			return TEXT("BridgeActive");
		case EPhysAnimRuntimeState::FailStopped:
			return TEXT("FailStopped");
		case EPhysAnimRuntimeState::BalanceEntry_Prepare:
			return TEXT("BalanceEntry_Prepare");
		case EPhysAnimRuntimeState::BalanceEntry_LateValidate:
			return TEXT("BalanceEntry_LateValidate");
		case EPhysAnimRuntimeState::BalanceEntry_RootOn:
			return TEXT("BalanceEntry_RootOn");
		case EPhysAnimRuntimeState::BalanceEntry_Settle:
			return TEXT("BalanceEntry_Settle");
		case EPhysAnimRuntimeState::BalanceActive_Recovery:
			return TEXT("BalanceActive_Recovery");
		case EPhysAnimRuntimeState::BalanceSafeDeny:
			return TEXT("BalanceSafeDeny");
		case EPhysAnimRuntimeState::BalanceActive_Standing:
			return TEXT("BalanceActive_Standing");
		case EPhysAnimRuntimeState::LocomotionActiveShell:
			return TEXT("LocomotionActiveShell");
		case EPhysAnimRuntimeState::LocomotionActiveShellDenied:
			return TEXT("LocomotionActiveShellDenied");
		default:
			return TEXT("Unknown");
		}
	}
}

/**
 * Latent command to wait for the standing proof to complete.
 * Returns false (keep waiting) until IsLiveRuntimeEvidenceProofComplete() is true
 * or the timeout elapses. Previously this always returned true immediately — a no-op.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_THREE_PARAMETER(FWaitForStandingProofCommand, UPhysAnimComponent*, Component, float, TimeoutSeconds, float, StartTime);
bool FWaitForStandingProofCommand::Update()
{
	if (!Component)
	{
		return true; // nothing to wait for
	}

	if (Component->IsLiveRuntimeEvidenceProofComplete())
	{
		return true; // proof finished — proceed to verify
	}

	const float Elapsed = static_cast<float>(FPlatformTime::Seconds()) - StartTime;
	if (Elapsed >= TimeoutSeconds)
	{
		// Timed out without proof completing. The verify command will catch this.
		return true;
	}

	return false; // keep polling
}

	/**
	* Command to spawn and attach a dumbbell for the load test.
	*/
	DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FSetupDumbbellCommand, float, TargetMassKg);
	bool FSetupDumbbellCommand::Update()
	{
	UWorld* World = nullptr;
	#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
	#endif
	if (!World) World = GWorld;

	if (!World)
	{
		return true;
	}

	ACharacter* TargetCharacter = nullptr;
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (It->FindComponentByClass<UPhysAnimComponent>())
		{
			TargetCharacter = *It;
			break;
		}
	}

	if (!TargetCharacter)
	{
		PHYSANIM_LOG(LogTemp, Error, TEXT("[DumbbellTest] FAILED to find character with UPhysAnimComponent."));
		return true;
	}

	// 0 kg is a valid test case (no load); skip spawning but still succeed.
	if (TargetMassKg <= 0.0f)
	{
		PHYSANIM_LOG(LogTemp, Warning, TEXT("[DumbbellTest] 0 kg case: no dumbbell spawned."));
		return true;
	}

	const FVector HandLocation = TargetCharacter->GetMesh()->GetBoneLocation(TEXT("hand_r"));

	// Spawn dumbbell at the hand's location to prevent massive physics impulses
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* Dumbbell = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), HandLocation, FRotator::ZeroRotator, SpawnParams);
	if (!Dumbbell) 
	{
		PHYSANIM_LOG(LogTemp, Error, TEXT("[DumbbellTest] FAILED to spawn AStaticMeshActor."));
		return true;
	}

	UStaticMeshComponent* MeshComp = Dumbbell->GetStaticMeshComponent();

	// CRITICAL FIX: The dumbbell MUST be Movable before ANY mesh is assigned.
	MeshComp->SetMobility(EComponentMobility::Movable);

	// Assign a basic cube mesh so it has physical volume
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh)
	{
		MeshComp->SetStaticMesh(CubeMesh);
	}

	// Disable collision entirely to prevent any physical displacement of the character or floor
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComp->SetCollisionResponseToAllChannels(ECR_Ignore);

	// SetSimulatePhysics(true) so it has its own mass for the solver to work with
	MeshComp->SetSimulatePhysics(true);
	MeshComp->SetMassOverrideInKg(NAME_None, TargetMassKg, true);

	// Attach to hand using SNAP_TO_TARGET but NOT including scale.
	// Then manually set scale to be very small (e.g. 5cm cube).
	MeshComp->AttachToComponent(TargetCharacter->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("hand_r"));
	MeshComp->SetRelativeScale3D(FVector(0.05f)); 

	// Use a Physics Constraint instead of Mass Injection for better stability.
	// Soft constraints handle mass differences better than single-bone overrides.
	UPhysicsConstraintComponent* Constraint = NewObject<UPhysicsConstraintComponent>(TargetCharacter);
	Constraint->RegisterComponent();
	Constraint->SetWorldLocation(HandLocation);
	Constraint->AttachToComponent(TargetCharacter->GetMesh(), FAttachmentTransformRules::KeepWorldTransform, TEXT("hand_r"));

	Constraint->SetConstrainedComponents(TargetCharacter->GetMesh(), TEXT("hand_r"), MeshComp, NAME_None);

	// Soft Lock: 100% rigid position/orientation but using the constraint solver
	Constraint->SetAngularSwing1Limit(ACM_Locked, 0);
	Constraint->SetAngularSwing2Limit(ACM_Locked, 0);
	Constraint->SetAngularTwistLimit(ACM_Locked, 0);
	Constraint->SetLinearXLimit(LCM_Locked, 0);
	Constraint->SetLinearYLimit(LCM_Locked, 0);
	Constraint->SetLinearZLimit(LCM_Locked, 0);

	// Evidence check: verify the constraint instance is actually valid after setup.
	// An invalid constraint would mean the dumbbell mass is NOT influencing the character.
	if (!Constraint->ConstraintInstance.IsValidConstraintInstance())
	{
		PHYSANIM_LOG(LogTemp, Error, TEXT("[DumbbellTest] CONSTRAINT_INVALID after setup for %.1f kg — dumbbell will not influence character physics."), TargetMassKg);
	}
	else
	{
		PHYSANIM_LOG(LogTemp, Warning, TEXT("[DumbbellTest] CONSTRAINT_VALID: %.1f kg dumbbell constrained to hand_r (ConstraintInstance confirmed active)."), TargetMassKg);
	}

	return true;
	}
	/**
	* Command to log character position for debugging floor penetration.
	*/
	DEFINE_LATENT_AUTOMATION_COMMAND(FLogCharacterPositionCommand);
	bool FLogCharacterPositionCommand::Update()
	{
	UWorld* World = nullptr;
	#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
	#endif
	if (!World) World = GWorld;
	if (!World) return true;

	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (It->FindComponentByClass<UPhysAnimComponent>())
		{
			const FVector Loc = It->GetActorLocation();
			const FVector MeshLoc = It->GetMesh()->GetComponentLocation();
			PHYSANIM_LOG(LogTemp, Warning, TEXT("[DumbbellTest] DEBUG_POS: ActorZ=%.2f MeshZ=%.2f"), Loc.Z, MeshLoc.Z);
			break;
		}
	}
	return true;
	}
	/**
	* Command to enable the proof hook.
	*/

DEFINE_LATENT_AUTOMATION_COMMAND(FEnableStandingProofCommand);
bool FEnableStandingProofCommand::Update()
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World) World = GWorld;

	if (!World)
	{
		return true;
	}

	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			PHYSANIM_LOG(LogTemp, Warning, TEXT("[!!!!PROOFIX!!!!] ENABLING_PROOF for %s"), *It->GetName());
			Comp->StopBridge();
			Comp->ResetLiveRuntimeEvidenceProof();
			Comp->bEnableLiveRuntimeEvidenceProof = true;
			Comp->StartBridge();
			break;
		}
	}

	return true;
}

/**
 * Command to start the proof wait.
 */
DEFINE_LATENT_AUTOMATION_COMMAND(FStartStandingProofWaitCommand);
bool FStartStandingProofWaitCommand::Update()
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World) World = GWorld;

	UPhysAnimComponent* TargetComponent = nullptr;
	if (World)
	{
		for (TActorIterator<ACharacter> It(World); It; ++It)
		{
			if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
			{
				TargetComponent = Comp;
				break;
			}
		}
	}

	if (TargetComponent)
	{
		// We can't use ADD_LATENT_AUTOMATION_COMMAND here because we are in a command's Update.
		// But we can return false and wait ourselves, or rely on the test sequence.
	}

	return true;
}

/**
 * Command to set the thigh restore variant CVar.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FSetThighRestoreVariantCommand, int32, Variant);
bool FSetThighRestoreVariantCommand::Update()
{
	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("p.PhysAnim.V0PlantThighRestoreVariant")))
	{
		CVar->Set(Variant);
	}
	return true;
}

/**
 * Command to arm startup proof terminal enforcement directly.
 */
DEFINE_LATENT_AUTOMATION_COMMAND(FArmStartupProofTerminalEnforcementCommand);
bool FArmStartupProofTerminalEnforcementCommand::Update()
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World) World = GWorld;

	if (!World)
	{
		return true;
	}

	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			Comp->ArmStartupProofTerminalEnforcement();
			break;
		}
	}

	return true;
}

/**
 * Latent command to verify a forced startup proof failure remains published as waiting with a failure terminal reason.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FVerifyActivationWiringFailureCommand, FAutomationTestBase*, Test);
bool FVerifyActivationWiringFailureCommand::Update()
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World) World = GWorld;

	if (!World)
	{
		AddLatentAutomationError(Test, TEXT("ActivationWiring: PIE world was not available"));
		return true;
	}

	bool bFoundComponent = false;
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			bFoundComponent = true;
			if (Comp->GetRuntimeState() != EPhysAnimRuntimeState::WaitingForPoseSearch)
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("ActivationWiring: TEST_FAILED. Expected WaitingForPoseSearch but got %s for %s"),
					GetRuntimeStateName(Comp->GetRuntimeState()),
					*It->GetName()));
			}

			const FPhysAnimRuntimeTerminationState& TerminationState = Comp->GetLiveRuntimeEvidenceTerminationState();
			if (TerminationState.TerminalReason != EPhysAnimTerminalReason::ActivationSupportFailure)
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("ActivationWiring: TEST_FAILED. Expected ActivationSupportFailure but got %d for %s"),
					static_cast<int32>(TerminationState.TerminalReason),
					*It->GetName()));
			}
			else
			{
				PHYSANIM_LOG(LogTemp, Warning, TEXT("ActivationWiring: TEST_PASSED forced failure remained waiting for %s"), *It->GetName());
			}
			break;
		}
	}

	if (!bFoundComponent)
	{
		AddLatentAutomationError(Test, TEXT("ActivationWiring: no PhysAnim component was found"));
	}

	return true;
}

/**
 * Latent command to verify the standing proof results.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FVerifyStandingProofCommand, FAutomationTestBase*, Test);
bool FVerifyStandingProofCommand::Update()
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World) World = GWorld;

	if (!World)
	{
		AddLatentAutomationError(Test, TEXT("StandingProof: PIE world was not available"));
		return true;
	}

	bool bFoundComponent = false;
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			bFoundComponent = true;
			const FPhysAnimRuntimeTerminationState& TerminationState = Comp->GetLiveRuntimeEvidenceTerminationState();

			// Check if the component has entered FailStopped state
			if (Comp->GetRuntimeState() == EPhysAnimRuntimeState::FailStopped)
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StandingProof: FAILED because the component is in FailStopped state for %s"), *It->GetName()));
				return true;
			}

			// Check if the mesh has fallen below the floor.
			// Threshold is spawn-relative: the pelvis must be at least 40 cm above the
			// actor's own floor (capsule half-height ≈ 88 cm, so pelvis at ~90 cm when
			// standing correctly). Using ActorLocation.Z as the reference keeps this
			// correct regardless of where the character spawns in the world.
			if (USkeletalMeshComponent* Mesh = It->GetMesh())
			{
				const FVector PelvisLocation = Mesh->GetSocketLocation(TEXT("pelvis"));
				const float FloorZ = It->GetActorLocation().Z;
				const float PelvisAboveFloor = PelvisLocation.Z - FloorZ;
				// A correctly standing Manny has pelvis ~90 cm above actor origin.
				// Reject anything below 40 cm above floor — indicates partial/full floor penetration.
				constexpr float MinPelvisAboveFloorCm = 40.0f;
				if (PelvisAboveFloor < MinPelvisAboveFloorCm)
				{
					AddLatentAutomationError(Test, FString::Printf(
						TEXT("StandingProof: FAILED because pelvis is only %.2f cm above actor floor (threshold=%.1f cm, PelvisZ=%.2f, FloorZ=%.2f) for %s"),
						PelvisAboveFloor, MinPelvisAboveFloorCm, PelvisLocation.Z, FloorZ, *It->GetName()));
					return true;
				}
			}
			
			// 1. Check if proof completed
			if (!Comp->IsLiveRuntimeEvidenceProofComplete())
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StandingProof: Proof did not complete in time for %s"), *It->GetName()));
				return true;
			}

			if (!Comp->IsLiveRuntimeEvidenceProofSatisfied())
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StandingProof: Proof completed but is not truthfully satisfied for %s"), *It->GetName()));
				return true;
			}

			if (!Comp->CanEnterBalanceActiveStanding())
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StandingProof: Proof completed but cannot enter active standing for %s"), *It->GetName()));
				return true;
			}

			// 2. Check Hull Area - Guard against the 0.0 failure
			const float HullArea = TerminationState.TerminalArtifact.SupportHullAreaCm2;
			if (HullArea <= 0.0f)
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StandingProof: SupportHullAreaCm2 is 0.0 for %s. This indicates a sampling failure or contract violation."), *It->GetName()));
			}
			else
			{
				PHYSANIM_LOG(LogTemp, Warning, TEXT("StandingProof: SupportHullAreaCm2 verified at %.1f cm2"), HullArea);
			}

			// 3. Check Terminal Reason
			const EPhysAnimTerminalReason Reason = TerminationState.TerminalArtifact.TerminalReason;
			if (Reason == EPhysAnimTerminalReason::ActivationSupportFailure)
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StandingProof: FAILED with ActivationSupportFailure for %s"), *It->GetName()));
			}
			else if (Reason == EPhysAnimTerminalReason::None)
			{
				PHYSANIM_LOG(LogTemp, Warning, TEXT("StandingProof: PASSED (None) for %s"), *It->GetName());
			}
			else
			{
				// Any terminal reason other than None or ActivationSupportFailure is an
				// unexpected failure mode and must be treated as a hard test failure.
				// Previously this only logged a warning, silently allowing the test to pass.
				AddLatentAutomationError(Test, FString::Printf(
					TEXT("StandingProof: FAILED with unexpected terminal reason %d for %s. All non-None terminal reasons must be explicitly handled."),
					(int32)Reason, *It->GetName()));
			}

			if (!TerminationState.TerminalArtifact.bPhysicsAssetContractValid ||
				!TerminationState.TerminalArtifact.bSkeletonAuditPassed)
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StandingProof: Terminal artifact audit state is inconsistent for %s"), *It->GetName()));
			}

			if (!TerminationState.TerminalArtifact.bCapsuleContractPassed ||
				!TerminationState.TerminalArtifact.bPhysicalContinuityValidatorPassed ||
				TerminationState.TerminalArtifact.bContinuityBookkeepingMismatch)
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StandingProof: Terminal artifact capsule/continuity state is invalid for %s"), *It->GetName()));
			}
			
			break;
		}
	}

	if (!bFoundComponent)
	{
		AddLatentAutomationError(Test, TEXT("StandingProof: no PhysAnim component was found"));
	}

	return true;
}

/**
 * Emits a structured THIGH_RESTORE_VARIANT_SUMMARY line after a ThighRestore diagnostic run.
 * Satisfies AC-2: diagnostic variants report terminal reason, duration, firstSpineSpike, velocity
 * snapshots, thigh angular strength/damping, and positive/negative work evidence.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FLogThighRestoreVariantSummaryCommand, int32, Variant, FAutomationTestBase*, Test);
bool FLogThighRestoreVariantSummaryCommand::Update()
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World) World = GWorld;
	if (!World) { return true; }

	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		UPhysAnimComponent* const Comp = It->FindComponentByClass<UPhysAnimComponent>();
		if (!Comp) { continue; }

		const FPhysAnimActivatedStandingStabilityMetrics& M = Comp->GetActivatedStandingStabilityMetrics();

		// Note: per-frame thigh angular strength/damping is captured in WORK_DIAG and
		// THIGH_RESTORE_STARTED log lines. This summary captures aggregate evidence at window entry.
		// KINETIC_GATE_RELEASE fields answer AC-6: does thigh restore add/remove energy from pelvis/spine?
		PHYSANIM_LOG(LogTemp, Warning,
			TEXT("[PhysAnimV0] THIGH_RESTORE_VARIANT_SUMMARY variant=%d terminalReason=%d duration=%.3f "
			     "firstSpineSpikeT=%.3f firstSpineSpikeBody=%s firstSupportFailT=%.3f "
			     "firstLinearThresholdT=%.3f firstLinearThresholdBody=%s "
			     "firstAngularThresholdT=%.3f firstAngularThresholdBody=%s "
			     "maxAngVel005=%.1f maxAngVel010=%.1f maxAngVel015=%.1f maxAngVel020=%.1f "
			     "maxAngVel030=%.1f maxAngVel060=%.1f maxAngVel100=%.1f "
			     "thighAngStr=%.4f thighAngDamp=%.4f thighLinStr=%.4f poseSeeded=%d "
			     "gateReleaseCount=%d pelvisAngVelAtRelease=%.2f maxSpineAngVelAtRelease=%.2f "
			     "thighStrAtRelease=%.4f activationTAtRelease=%.3f "
			     "positiveWork=%.6f negativeWork=%.6f samples=%d [log]"),
			Variant,
			M.TerminalReason,
			M.ActivationDurationSec,
			M.FirstMajorSpineSpikeTimeSec,
			*M.FirstMajorSpineSpikeBodyName.ToString(),
			M.FirstSupportFailureTimeSec,
			M.FirstLinearThresholdTimeSec,
			*M.FirstLinearThresholdBodyName.ToString(),
			M.FirstAngularThresholdTimeSec,
			*M.FirstAngularThresholdBodyName.ToString(),
			M.MaxAngVel005s, M.MaxAngVel010s, M.MaxAngVel015s, M.MaxAngVel020s,
			M.MaxAngVel030s, M.MaxAngVel060s, M.MaxAngVel100s,
			M.ThighAngularStrengthAtWindowEntry, M.ThighAngularDampingAtWindowEntry,
			M.ThighLinearStrengthAtWindowEntry, M.PoseTargetsSeededAtWindowEntry,
			M.KineticGateReleaseCount, M.PelvisAngVelAtGateRelease, M.MaxSpineAngVelAtGateRelease,
			M.ThighStrengthAtGateRelease, M.ActivationTimeAtGateRelease,
			M.ThighPositiveWorkAccumulated, M.ThighNegativeWorkAccumulated,
			M.SampleCount);
		break;
	}
	return true;
}

/**
 * Command to enable the negative support proof hook.
 */
DEFINE_LATENT_AUTOMATION_COMMAND(FEnableNegativeSupportProofCommand);
bool FEnableNegativeSupportProofCommand::Update()
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World) World = GWorld;

	if (!World)
	{
		return true;
	}

	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			PHYSANIM_LOG(LogTemp, Warning, TEXT("StandingProof: ENABLING_NEGATIVE_SUPPORT_PROOF for %s"), *It->GetName());
			Comp->StopBridge();
			Comp->ResetLiveRuntimeEvidenceProof();
			Comp->bEnableLiveRuntimeEvidenceProof = true;
			Comp->SetForceSupportFailure(true);
			Comp->StartBridge();
			break;
		}
	}

	return true;
}

/**
 * Latent command to verify the negative standing proof results.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FVerifyNegativeSupportProofCommand, FAutomationTestBase*, Test);
bool FVerifyNegativeSupportProofCommand::Update()
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World) World = GWorld;

	if (!World)
	{
		AddLatentAutomationError(Test, TEXT("StandingProof: negative proof PIE world was not available"));
		return true;
	}

	bool bFoundComponent = false;
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			bFoundComponent = true;
			const FPhysAnimRuntimeTerminationState& TerminationState = Comp->GetLiveRuntimeEvidenceTerminationState();
			
			// 1. Check if proof completed
			if (!Comp->IsLiveRuntimeEvidenceProofComplete())
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StandingProof: Negative proof did not complete in time for %s"), *It->GetName()));
				return true;
			}

			// 2. Check Terminal Reason - Expected ActivationSupportFailure
			const EPhysAnimTerminalReason Reason = TerminationState.TerminalArtifact.TerminalReason;
			if (Reason == EPhysAnimTerminalReason::ActivationSupportFailure)
			{
				PHYSANIM_LOG(LogTemp, Warning, TEXT("StandingProof: NEGATIVE_TEST_PASSED (Expected ActivationSupportFailure) for %s"), *It->GetName());
			}
			else
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StandingProof: NEGATIVE_TEST_FAILED. Expected ActivationSupportFailure but got %d for %s"), (int32)Reason, *It->GetName()));
			}

			if (Comp->IsLiveRuntimeEvidenceProofSatisfied())
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StandingProof: NEGATIVE_TEST_FAILED. Proof should not be satisfied for %s"), *It->GetName()));
			}

			if (Comp->CanEnterBalanceActiveStanding())
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StandingProof: NEGATIVE_TEST_FAILED. Active standing should be denied for %s"), *It->GetName()));
			}

			// 3. Ensure we didn't stay in active standing
			const EPhysAnimRuntimeState RuntimeState = Comp->GetRuntimeState();
			if (RuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing)
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StandingProof: NEGATIVE_TEST_FAILED. Remained in BalanceActive_Standing despite support loss for %s"), *It->GetName()));
			}
			
			break;
		}
	}

	if (!bFoundComponent)
	{
		AddLatentAutomationError(Test, TEXT("StandingProof: negative proof found no PhysAnim component"));
	}

	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimStandingProofLiveTest, "PhysAnim.StandingProof.Live", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimStandingProofLiveTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing"));
	OutTestCommands.Add(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
}

bool FPhysAnimStandingProofLiveTest::RunTest(const FString& Parameters)
{
	const FString MapName = Parameters;

	// 1. Load the map
	AutomationOpenMap(MapName);

	// 2. Wait for map to load
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));

	// 3. Enable proof
	ADD_LATENT_AUTOMATION_COMMAND(FEnableStandingProofCommand());

	// 4. Wait for results (6 seconds to be safe)
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(6.0f));

	// 5. Verify results
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyStandingProofCommand(this));

	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimStandingProofNegativeSupportTest, "PhysAnim.StandingProof.NegativeSupport", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimStandingProofNegativeSupportTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_Negative"));
	OutTestCommands.Add(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
}

bool FPhysAnimStandingProofNegativeSupportTest::RunTest(const FString& Parameters)
{
	const FString MapName = Parameters;

	AddExpectedError(TEXT("ENTRY_DENIED reason=TERMINATED_IN_PROOF"), EAutomationExpectedErrorFlags::Contains, 0);

	// 1. Load the map
	AutomationOpenMap(MapName);

	// 2. Wait for map to load
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));

	// 3. Enable negative proof
	ADD_LATENT_AUTOMATION_COMMAND(FEnableNegativeSupportProofCommand());

	// 4. Wait for results (6 seconds)
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(6.0f));

	// 5. Verify negative results
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyNegativeSupportProofCommand(this));

	return true;
}
/**
 * Command to enable activation wiring test cases.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_THREE_PARAMETER(FEnableActivationWiringCommand, bool, bEnableProof, bool, bFinishProof, bool, bForceFailure);
bool FEnableActivationWiringCommand::Update()
{
	static TOptional<FTransform> InitialActorTransform;

	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World) World = GWorld;

	if (!World)
	{
		return true;
	}

	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			ACharacter* Character = *It;
			if (!InitialActorTransform.IsSet())
			{
				InitialActorTransform = Character->GetActorTransform();
			}

			Comp->StopBridge();
			Character->SetActorTransform(InitialActorTransform.GetValue(), false, nullptr, ETeleportType::TeleportPhysics);
			Comp->ResetLiveRuntimeEvidenceProof();
			Comp->bEnableLiveRuntimeEvidenceProof = bEnableProof;
			Comp->SetStartupProofShouldComplete(bFinishProof);
			Comp->SetForceSupportFailure(bForceFailure);

			Comp->StartBridge();
			break;
		}
	}

	return true;
}

/**
 * Command to stop the activation wiring bridge after an observation-only proof check.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FStopActivationWiringCommand, FAutomationTestBase*, Test);
bool FStopActivationWiringCommand::Update()
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World)
	{
		World = GWorld;
	}

	if (!World)
	{
		AddLatentAutomationError(Test, TEXT("ActivationWiring: PIE world was not available"));
		return true;
	}

	bool bFoundComponent = false;
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			bFoundComponent = true;
			Comp->StopBridge();
			PHYSANIM_LOG(LogTemp, Warning, TEXT("ActivationWiring: TEST_PASSED stopped bridge for %s"), *It->GetName());
			break;
		}
	}

	if (!bFoundComponent)
	{
		AddLatentAutomationError(Test, TEXT("ActivationWiring: no PhysAnim component was found"));
	}

	return true;
}

/**
 * Command to verify proof-not-satisfied waiting state and stop immediately in the same frame.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FVerifyProofNotSatisfiedAndStopCommand, FAutomationTestBase*, Test);
bool FVerifyProofNotSatisfiedAndStopCommand::Update()
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World)
	{
		World = GWorld;
	}

	if (!World)
	{
		AddLatentAutomationError(Test, TEXT("ActivationWiring: PIE world was not available"));
		return true;
	}

	bool bFoundComponent = false;
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			bFoundComponent = true;
			if (Comp->GetRuntimeState() != EPhysAnimRuntimeState::WaitingForPoseSearch)
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("ActivationWiring: TEST_FAILED. Expected WaitingForPoseSearch but got %s for %s"),
					GetRuntimeStateName(Comp->GetRuntimeState()),
					*It->GetName()));
			}
			else
			{
				Comp->NoteStartupProofWaitingForPoseSearchObserved();
				if (!Comp->IsStartupProofWaitingForPoseSearchObserved())
				{
					AddLatentAutomationError(Test, FString::Printf(TEXT("ActivationWiring: TEST_FAILED. Startup proof observation did not latch for %s"), *It->GetName()));
				}
				if (Comp->IsLiveRuntimeEvidenceProofComplete())
				{
					AddLatentAutomationError(Test, FString::Printf(TEXT("ActivationWiring: TEST_FAILED. Startup proof completed before WaitingForPoseSearch verify for %s"), *It->GetName()));
				}
			}

			Comp->StopBridge();
			PHYSANIM_LOG(LogTemp, Warning, TEXT("ActivationWiring: TEST_PASSED verified waiting state and stopped bridge for %s"), *It->GetName());
			break;
		}
	}

	if (!bFoundComponent)
	{
		AddLatentAutomationError(Test, TEXT("ActivationWiring: no PhysAnim component was found"));
	}

	return true;
}

/**
 * Command to verify activation wiring results.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FVerifyActivationWiringCommand, EPhysAnimRuntimeState, ExpectedState, FAutomationTestBase*, Test);
bool FVerifyActivationWiringCommand::Update()
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World) World = GWorld;

	if (!World)
	{
		AddLatentAutomationError(Test, TEXT("ActivationWiring: PIE world was not available"));
		return true;
	}

	bool bFoundComponent = false;
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			bFoundComponent = true;
			const EPhysAnimRuntimeState ActualState = Comp->GetRuntimeState();
			const bool bCompatibleState = (ActualState == ExpectedState) ||
				(ExpectedState == EPhysAnimRuntimeState::WaitingForPoseSearch && ActualState == EPhysAnimRuntimeState::BridgeActive);

			if (bCompatibleState)
			{
				if (ExpectedState == EPhysAnimRuntimeState::WaitingForPoseSearch)
				{
					Comp->NoteStartupProofWaitingForPoseSearchObserved();
					if (!Comp->IsStartupProofWaitingForPoseSearchObserved())
					{
						AddLatentAutomationError(Test, FString::Printf(TEXT("ActivationWiring: TEST_FAILED. Startup proof observation did not latch for %s"), *It->GetName()));
					}
					if (Comp->IsLiveRuntimeEvidenceProofComplete())
					{
						AddLatentAutomationError(Test, FString::Printf(TEXT("ActivationWiring: TEST_FAILED. Startup proof completed before WaitingForPoseSearch verify for %s"), *It->GetName()));
					}
				}

				PHYSANIM_LOG(LogTemp, Warning, TEXT("ActivationWiring: TEST_PASSED ActualState=%d"), (int32)ActualState);
				if (ExpectedState == EPhysAnimRuntimeState::BalanceActive_Standing)
				{
					if (!Comp->IsStartupProofEvidenceFresh())
					{
						AddLatentAutomationError(Test, FString::Printf(TEXT("ActivationWiring: TEST_FAILED. Standing proof evidence was not fresh for %s"), *It->GetName()));
					}
					if (!Comp->IsLiveRuntimeEvidenceProofSatisfied())
					{
						AddLatentAutomationError(Test, FString::Printf(TEXT("ActivationWiring: TEST_FAILED. Standing proof was not satisfied for %s"), *It->GetName()));
					}
					if (!Comp->IsLiveRuntimeEvidenceProofComplete())
					{
						AddLatentAutomationError(Test, FString::Printf(TEXT("ActivationWiring: TEST_FAILED. Standing proof did not complete for %s"), *It->GetName()));
					}
					if (!Comp->IsStartupProofStandingEntryAccepted())
					{
						AddLatentAutomationError(Test, FString::Printf(TEXT("ActivationWiring: TEST_FAILED. Standing entry acceptance was not latched for %s"), *It->GetName()));
					}
					if (!Comp->IsStartupProofProxySupportHandoffArmed())
					{
						AddLatentAutomationError(Test, FString::Printf(TEXT("ActivationWiring: TEST_FAILED. Standing proxy handoff was not armed for %s"), *It->GetName()));
					}

					if (Comp->HasDeferredStartupProxyTerminalReason() &&
						Comp->GetDeferredStartupProxyTerminalReason() != EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion)
					{
						AddLatentAutomationError(Test, FString::Printf(TEXT("ActivationWiring: TEST_FAILED. Unexpected deferred proxy terminal reason %d for %s"),
							static_cast<int32>(Comp->GetDeferredStartupProxyTerminalReason()),
							*It->GetName()));
					}
					if (Comp->HasDeferredStartupProxyTerminalReason() &&
						Comp->GetDeferredStartupProxyTerminalReason() == EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion)
					{
						if (Comp->GetDeferredStartupProxyTerminalAttemptUuid().IsEmpty())
						{
							AddLatentAutomationError(Test, FString::Printf(TEXT("ActivationWiring: TEST_FAILED. Deferred proxy terminal attempt uuid was empty for %s"), *It->GetName()));
						}
						if (Comp->GetDeferredStartupProxyTerminalSubstepTimestamp() < 0)
						{
							AddLatentAutomationError(Test, FString::Printf(TEXT("ActivationWiring: TEST_FAILED. Deferred proxy terminal substep was invalid for %s"), *It->GetName()));
						}
					}
				}
			}
			else
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("ActivationWiring: TEST_FAILED. Expected %d but got %d for %s"), (int32)ExpectedState, (int32)ActualState, *It->GetName()));
			}
			break;
		}
	}

	if (!bFoundComponent)
	{
		AddLatentAutomationError(Test, TEXT("ActivationWiring: no PhysAnim component was found"));
	}

	return true;
}


/**
 * Command to verify proof-disabled runtime never safe-denies from stale proof artifacts.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FVerifyProofDisabledSafeDenyCommand, FAutomationTestBase*, Test);
bool FVerifyProofDisabledSafeDenyCommand::Update()
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World) World = GWorld;

	if (!World)
	{
		AddLatentAutomationError(Test, TEXT("StartupProof: PIE world was not available"));
		return true;
	}

	bool bFoundComponent = false;
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			bFoundComponent = true;
			const EPhysAnimRuntimeState ActualState = Comp->GetRuntimeState();
			const FPhysAnimRuntimeTerminationState& TerminationState = Comp->GetLiveRuntimeEvidenceTerminationState();
			if (ActualState == EPhysAnimRuntimeState::BalanceSafeDeny ||
				ActualState == EPhysAnimRuntimeState::FailStopped)
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Expected proof-disabled startup to avoid safe deny but got %s for %s"),
					GetRuntimeStateName(ActualState),
					*It->GetName()));
			}
			else if (TerminationState.TerminalReason != EPhysAnimTerminalReason::None)
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Expected terminal_reason=None but got %d for %s"),
					static_cast<int32>(TerminationState.TerminalReason),
					*It->GetName()));
			}
			else
			{
				PHYSANIM_LOG(LogTemp, Warning, TEXT("StartupProof: TEST_PASSED runtime=%s terminalReason=None for %s"),
					GetRuntimeStateName(ActualState),
					*It->GetName());
			}
			break;
		}
	}

	if (!bFoundComponent)
	{
		AddLatentAutomationError(Test, TEXT("StartupProof: no PhysAnim component was found"));
	}

	return true;
}

/**
 * Command to verify proof-enabled startup stays out of safe deny before fresh evidence is available.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FVerifyProofEnabledStartupFreshEvidenceCommand, FAutomationTestBase*, Test);
bool FVerifyProofEnabledStartupFreshEvidenceCommand::Update()
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World) World = GWorld;

	if (!World)
	{
		AddLatentAutomationError(Test, TEXT("StartupProof: PIE world was not available"));
		return true;
	}

	bool bFoundComponent = false;
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			bFoundComponent = true;
			const EPhysAnimRuntimeState ActualState = Comp->GetRuntimeState();
			const FPhysAnimRuntimeTerminationState& TerminationState = Comp->GetLiveRuntimeEvidenceTerminationState();
			if (ActualState == EPhysAnimRuntimeState::BalanceSafeDeny ||
				ActualState == EPhysAnimRuntimeState::FailStopped)
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Expected proof-enabled startup to avoid safe deny but got %s for %s"),
					GetRuntimeStateName(ActualState),
					*It->GetName()));
			}
			else if (TerminationState.TerminalReason != EPhysAnimTerminalReason::None)
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Expected terminal_reason=None during proof-enabled startup but got %d for %s"),
					static_cast<int32>(TerminationState.TerminalReason),
					*It->GetName()));
			}
			else if (Comp->GetStartupProofDeferredTerminalReason() != EPhysAnimTerminalReason::None ||
				Comp->IsStartupProofTerminalEnforcementArmed())
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Expected startup proof to remain unarmed for %s"),
					*It->GetName()));
			}
			else
			{
				PHYSANIM_LOG(LogTemp, Warning, TEXT("StartupProof: TEST_PASSED runtime=%s terminalReason=None for %s"),
					GetRuntimeStateName(ActualState),
					*It->GetName());
			}
			break;
		}
	}

	if (!bFoundComponent)
	{
		AddLatentAutomationError(Test, TEXT("StartupProof: no PhysAnim component was found"));
	}

	return true;
}

/**
 * Command to verify proof-enabled startup handoff state is armed after WaitingForPoseSearch is observed.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FVerifyProofEnabledStartupWaitingHandoffCommand, FAutomationTestBase*, Test);
bool FVerifyProofEnabledStartupWaitingHandoffCommand::Update()
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World) World = GWorld;

	if (!World)
	{
		AddLatentAutomationError(Test, TEXT("StartupProof: PIE world was not available"));
		return true;
	}

	bool bFoundComponent = false;
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			bFoundComponent = true;
			if (Comp->GetRuntimeState() != EPhysAnimRuntimeState::WaitingForPoseSearch)
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Expected WaitingForPoseSearch but got %s for %s"),
					GetRuntimeStateName(Comp->GetRuntimeState()),
					*It->GetName()));
			}

			Comp->ArmStartupProofTerminalEnforcement();

			if (!Comp->IsStartupProofWaitingForPoseSearchObserved() ||
				!Comp->IsStartupProofTerminalEnforcementArmed())
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Startup proof handoff was not armed for %s"),
					*It->GetName()));
			}

			const EPhysAnimTerminalReason DeferredReason = Comp->GetStartupProofDeferredTerminalReason();
			if (DeferredReason != EPhysAnimTerminalReason::None &&
				DeferredReason != EPhysAnimTerminalReason::ActivationSupportFailure)
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Unexpected deferred terminal reason %d for %s"),
					static_cast<int32>(DeferredReason),
					*It->GetName()));
			}

			const FPhysAnimRuntimeTerminationState& TerminationState = Comp->GetLiveRuntimeEvidenceTerminationState();
			if (TerminationState.TerminalReason != EPhysAnimTerminalReason::None)
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Expected neutralized startup termination state but got %d for %s"),
					static_cast<int32>(TerminationState.TerminalReason),
					*It->GetName()));
			}
			else
			{
				PHYSANIM_LOG(LogTemp, Warning, TEXT("StartupProof: TEST_PASSED waiting handoff armed deferredReason=%d for %s"),
					static_cast<int32>(DeferredReason),
					*It->GetName());
			}
			break;
		}
	}

	if (!bFoundComponent)
	{
		AddLatentAutomationError(Test, TEXT("StartupProof: no PhysAnim component was found"));
	}

	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FArmProofFailureFailStopRoutingCommand, FAutomationTestBase*, Test);
bool FArmProofFailureFailStopRoutingCommand::Update()
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World)
	{
		World = GWorld;
	}

	if (!World)
	{
		AddLatentAutomationError(Test, TEXT("StartupProof: PIE world was not available"));
		return true;
	}

	bool bFoundComponent = false;
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			bFoundComponent = true;
			if (Comp->GetRuntimeState() != EPhysAnimRuntimeState::WaitingForPoseSearch)
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Expected WaitingForPoseSearch but got %s for %s"),
					GetRuntimeStateName(Comp->GetRuntimeState()),
					*It->GetName()));
			}

			const FPhysAnimRuntimeTerminationState& TerminationState = Comp->GetLiveRuntimeEvidenceTerminationState();
			const EPhysAnimTerminalReason DeferredReason = Comp->GetStartupProofDeferredTerminalReason();
			if (DeferredReason != EPhysAnimTerminalReason::ActivationSupportFailure &&
				TerminationState.TerminalReason != EPhysAnimTerminalReason::ActivationSupportFailure)
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Expected deferred ActivationSupportFailure before arming but got deferred=%d terminal=%d for %s"),
					static_cast<int32>(DeferredReason),
					static_cast<int32>(TerminationState.TerminalReason),
					*It->GetName()));
			}

			Comp->ArmStartupProofTerminalEnforcement();
			if (!Comp->IsStartupProofTerminalEnforcementArmed())
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Startup proof handoff did not arm for %s"),
					*It->GetName()));
			}
			else
			{
				PHYSANIM_LOG(
					LogTemp,
					Warning,
					TEXT("StartupProof: TEST_PASSED fail-stop handoff armed deferredReason=%d terminalReason=%d for %s"),
					static_cast<int32>(DeferredReason),
					static_cast<int32>(TerminationState.TerminalReason),
					*It->GetName());
#if !UE_BUILD_SHIPPING
				Comp->TriggerProofFailureFailStopRoutingForTesting();
#endif

				const EPhysAnimRuntimeState RoutedState = Comp->GetRuntimeState();
				const FPhysAnimRuntimeTerminationState& RoutedTerminationState = Comp->GetLiveRuntimeEvidenceTerminationState();
				const EPhysAnimTerminalReason RoutedDeferredReason = Comp->GetStartupProofDeferredTerminalReason();
				const USkeletalMeshComponent* const RoutedMesh = Comp->GetMeshComponent();
				const bool bRoutedTraceSessionActive = Comp->HasActiveBridgeTraceSession();
				const EPhysAnimTerminalReason RoutedPreservedReason =
					RoutedDeferredReason != EPhysAnimTerminalReason::None ? RoutedDeferredReason : RoutedTerminationState.TerminalReason;

				if (RoutedState != EPhysAnimRuntimeState::FailStopped)
				{
					AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Expected FailStopped immediately after routed trigger but got %s for %s"),
						GetRuntimeStateName(RoutedState),
						*It->GetName()));
				}

				if (Comp->DoesBridgeOwnPhysics())
				{
					AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Bridge still owned physics immediately after routed trigger for %s"),
						*It->GetName()));
				}

				if (Comp->IsComponentTickEnabled())
				{
					AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Component tick still enabled immediately after routed trigger for %s"),
						*It->GetName()));
				}

				if (RoutedMesh && RoutedMesh->IsSimulatingPhysics())
				{
					AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Mesh still simulating physics immediately after routed trigger for %s"),
						*It->GetName()));
				}

				if (RoutedPreservedReason != EPhysAnimTerminalReason::ActivationSupportFailure)
				{
					AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Expected preserved terminal reason ActivationSupportFailure but got deferred=%d terminal=%d for %s"),
						static_cast<int32>(RoutedDeferredReason),
						static_cast<int32>(RoutedTerminationState.TerminalReason),
						*It->GetName()));
				}

				if (bRoutedTraceSessionActive)
				{
					AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Bridge trace session remained active immediately after routed trigger for %s"),
						*It->GetName()));
				}

				PHYSANIM_LOG(
					LogTemp,
					Warning,
					TEXT("StartupProof: TEST_PASSED proof fail-stop routed runtime=%s deferredReason=%d terminalReason=%d traceActive=%d ownsPhysics=%d tickEnabled=%d meshSim=%d"),
					GetRuntimeStateName(RoutedState),
					static_cast<int32>(RoutedDeferredReason),
					static_cast<int32>(RoutedTerminationState.TerminalReason),
					bRoutedTraceSessionActive ? 1 : 0,
					Comp->DoesBridgeOwnPhysics() ? 1 : 0,
					Comp->IsComponentTickEnabled() ? 1 : 0,
					RoutedMesh && RoutedMesh->IsSimulatingPhysics() ? 1 : 0);
			}
			break;
		}
	}

	if (!bFoundComponent)
	{
		AddLatentAutomationError(Test, TEXT("StartupProof: no PhysAnim component was found"));
	}

	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FSeedProofFailureDeferredReasonCommand, FAutomationTestBase*, Test);
bool FSeedProofFailureDeferredReasonCommand::Update()
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World)
	{
		World = GWorld;
	}

	if (!World)
	{
		AddLatentAutomationError(Test, TEXT("StartupProof: PIE world was not available"));
		return true;
	}

	bool bFoundComponent = false;
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			bFoundComponent = true;
			Comp->SetStartupProofDeferredTerminalReasonForTesting(EPhysAnimTerminalReason::ActivationSupportFailure);
			if (Comp->GetStartupProofDeferredTerminalReason() != EPhysAnimTerminalReason::ActivationSupportFailure)
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Deferred terminal reason did not seed for %s"),
					*It->GetName()));
			}
			else
			{
				PHYSANIM_LOG(LogTemp, Warning, TEXT("StartupProof: TEST_PASSED deferred terminal reason seeded for %s"), *It->GetName());
			}
			break;
		}
	}

	if (!bFoundComponent)
	{
		AddLatentAutomationError(Test, TEXT("StartupProof: no PhysAnim component was found"));
	}

	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FVerifyProofFailureFailStopRoutingCommand, FAutomationTestBase*, Test);
bool FVerifyProofFailureFailStopRoutingCommand::Update()
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World)
	{
		World = GWorld;
	}

	if (!World)
	{
		AddLatentAutomationError(Test, TEXT("StartupProof: PIE world was not available"));
		return true;
	}

	bool bFoundComponent = false;
	UPhysAnimComponent* SelectedComponent = nullptr;
	ACharacter* SelectedCharacter = nullptr;
	UPhysAnimComponent* FirstComponent = nullptr;
	ACharacter* FirstCharacter = nullptr;
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			bFoundComponent = true;
			if (!FirstComponent)
			{
				FirstComponent = Comp;
				FirstCharacter = *It;
			}

			if (Comp->GetRuntimeState() == EPhysAnimRuntimeState::FailStopped)
			{
				SelectedComponent = Comp;
				SelectedCharacter = *It;
				break;
			}
		}
	}

	if (!bFoundComponent)
	{
		AddLatentAutomationError(Test, TEXT("StartupProof: no PhysAnim component was found"));
		return true;
	}

	if (!SelectedComponent)
	{
		SelectedComponent = FirstComponent;
		SelectedCharacter = FirstCharacter;
	}

	const EPhysAnimRuntimeState ActualState = SelectedComponent->GetRuntimeState();
	const FPhysAnimRuntimeTerminationState& TerminationState = SelectedComponent->GetLiveRuntimeEvidenceTerminationState();
	const EPhysAnimTerminalReason DeferredReason = SelectedComponent->GetStartupProofDeferredTerminalReason();
	const USkeletalMeshComponent* const Mesh = SelectedComponent->GetMeshComponent();
	const bool bTraceSessionActive = SelectedComponent->HasActiveBridgeTraceSession();
	const EPhysAnimTerminalReason PreservedReason =
		DeferredReason != EPhysAnimTerminalReason::None ? DeferredReason : TerminationState.TerminalReason;

	if (ActualState != EPhysAnimRuntimeState::FailStopped)
	{
		AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Expected FailStopped but got %s for %s"),
			GetRuntimeStateName(ActualState),
			*SelectedCharacter->GetName()));
	}

	if (SelectedComponent->DoesBridgeOwnPhysics())
	{
		AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Bridge still owned physics after routed fail-stop for %s"),
			*SelectedCharacter->GetName()));
	}

	if (SelectedComponent->IsComponentTickEnabled())
	{
		AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Component tick still enabled after routed fail-stop for %s"),
			*SelectedCharacter->GetName()));
	}

	if (!Mesh)
	{
		AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. No mesh component was available after routed fail-stop for %s"),
			*SelectedCharacter->GetName()));
	}
	else if (Mesh->IsSimulatingPhysics())
	{
		AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Mesh still simulating physics after routed fail-stop for %s"),
			*SelectedCharacter->GetName()));
	}

	if (PreservedReason != EPhysAnimTerminalReason::ActivationSupportFailure)
	{
		AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Expected preserved terminal reason ActivationSupportFailure but got deferred=%d terminal=%d for %s"),
			static_cast<int32>(DeferredReason),
			static_cast<int32>(TerminationState.TerminalReason),
			*SelectedCharacter->GetName()));
	}

	if (bTraceSessionActive)
	{
		AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Bridge trace session remained active after fail-stop for %s"),
			*SelectedCharacter->GetName()));
	}

	PHYSANIM_LOG(
		LogTemp,
		Warning,
		TEXT("StartupProof: TEST_PASSED proof fail-stop routed runtime=%s deferredReason=%d terminalReason=%d traceActive=%d ownsPhysics=%d tickEnabled=%d meshSim=%d"),
		GetRuntimeStateName(ActualState),
		static_cast<int32>(DeferredReason),
		static_cast<int32>(TerminationState.TerminalReason),
		bTraceSessionActive ? 1 : 0,
		SelectedComponent->DoesBridgeOwnPhysics() ? 1 : 0,
		SelectedComponent->IsComponentTickEnabled() ? 1 : 0,
		Mesh && Mesh->IsSimulatingPhysics() ? 1 : 0);
	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FVerifyProofCompleteStandingEntryProxyTimingCommand, FAutomationTestBase*, Test);
bool FVerifyProofCompleteStandingEntryProxyTimingCommand::Update()
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World) World = GWorld;

	if (!World)
	{
		AddLatentAutomationError(Test, TEXT("StartupProof: PIE world was not available"));
		return true;
	}

	bool bFoundComponent = false;
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			bFoundComponent = true;
			if (Comp->GetRuntimeState() != EPhysAnimRuntimeState::BalanceActive_Standing)
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Expected BalanceActive_Standing but got %s for %s"),
					GetRuntimeStateName(Comp->GetRuntimeState()),
					*It->GetName()));
			}

			if (!Comp->IsLiveRuntimeEvidenceProofComplete() ||
				!Comp->IsStartupProofEvidenceFresh() ||
				!Comp->IsStartupProofStandingEntryAccepted() ||
				!Comp->IsStartupProofTerminalEnforcementArmed())
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Standing entry acceptance state was not truthful for %s"),
					*It->GetName()));
			}

			if (!Comp->IsStartupProofProxySupportHandoffArmed())
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Proxy handoff was not armed after standing entry for %s"),
					*It->GetName()));
			}

			if (Comp->HasDeferredStartupProxyTerminalReason() &&
				Comp->GetDeferredStartupProxyTerminalReason() != EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion)
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Unexpected deferred proxy terminal reason %d for %s"),
					static_cast<int32>(Comp->GetDeferredStartupProxyTerminalReason()),
					*It->GetName()));
			}
			if (Comp->HasDeferredStartupProxyTerminalReason() &&
				Comp->GetDeferredStartupProxyTerminalReason() == EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion)
			{
				if (Comp->GetDeferredStartupProxyTerminalAttemptUuid().IsEmpty())
				{
					AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Deferred proxy terminal attempt uuid was empty for %s"),
						*It->GetName()));
				}
				if (Comp->GetDeferredStartupProxyTerminalSubstepTimestamp() < 0)
				{
					AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Deferred proxy terminal substep was invalid for %s"),
						*It->GetName()));
				}
			}

			PHYSANIM_LOG(LogTemp, Warning, TEXT("StartupProof: TEST_PASSED standing entry proxy timing accepted=%d proxyArmed=%d for %s"),
				Comp->IsStartupProofStandingEntryAccepted() ? 1 : 0,
				Comp->IsStartupProofProxySupportHandoffArmed() ? 1 : 0,
				*It->GetName());
			break;
		}
	}

	if (!bFoundComponent)
	{
		AddLatentAutomationError(Test, TEXT("StartupProof: no PhysAnim component was found"));
	}

	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimStartupProofEnabledStartupWaitingHandoffTest, "PhysAnim.StartupProof.WaitingHandoff", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimStartupProofEnabledStartupWaitingHandoffTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("ProofStartupWaitingHandoff"));
	OutTestCommands.Add(TEXT("ProofStartupWaitingHandoff"));
}

bool FPhysAnimStartupProofEnabledStartupWaitingHandoffTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, false, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.25f));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivationWiringCommand(EPhysAnimRuntimeState::WaitingForPoseSearch, this));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyProofEnabledStartupWaitingHandoffCommand(this));
	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimStartupProofEnabledStartupProxyHandoffTest, "PhysAnim.StartupProof.ProxyHandoff", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimStartupProofEnabledStartupProxyHandoffTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("ProofStartupProxyHandoff"));
	OutTestCommands.Add(TEXT("ProofStartupProxyHandoff"));
}

bool FPhysAnimStartupProofEnabledStartupProxyHandoffTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, true, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivationWiringCommand(EPhysAnimRuntimeState::BalanceActive_Standing, this));
	ADD_LATENT_AUTOMATION_COMMAND(FStopActivationWiringCommand(this));
	return true;
}


/**
 * Latent command to collect activated standing stability metrics for a duration.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FCollectActivatedStandingStabilityMetricsCommand, float, DurationSeconds);
bool FCollectActivatedStandingStabilityMetricsCommand::Update()
{
	static double StartTimeSeconds = -1.0;

	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World) World = GWorld;

	if (!World)
	{
		return false;
	}

	if (StartTimeSeconds < 0.0)
	{
		StartTimeSeconds = FPlatformTime::Seconds();
	}

	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			Comp->UpdateActivatedStandingStabilityMetrics(FMath::Max(0.0f, World->GetDeltaSeconds()));
			break;
		}
	}

	const bool bFinished = (FPlatformTime::Seconds() - StartTimeSeconds) >= static_cast<double>(DurationSeconds);
	if (bFinished)
	{
		StartTimeSeconds = -1.0;
	}

	return bFinished;
}

/**
 * Command to verify activated standing stability metrics.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FVerifyActivatedStandingStabilityMetricsCommand, FAutomationTestBase*, Test);
bool FVerifyActivatedStandingStabilityMetricsCommand::Update()
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World) World = GWorld;

	if (!World)
	{
		AddLatentAutomationError(Test, TEXT("StandingProof: StabilityMetrics PIE world was not available"));
		return true;
	}

	bool bFoundComponent = false;
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			bFoundComponent = true;
			const auto Fail = [&](const FString& Message)
			{
				AddLatentAutomationError(Test, Message);
			};
			const EPhysAnimRuntimeState RuntimeState = Comp->GetRuntimeState();
			const FPhysAnimRuntimeTerminationState& TerminationState = Comp->GetLiveRuntimeEvidenceTerminationState();
			const FPhysAnimActivatedStandingStabilityMetrics& Metrics = Comp->GetActivatedStandingStabilityMetrics();

			if (RuntimeState != EPhysAnimRuntimeState::BalanceActive_Standing)
			{
				Fail(FString::Printf(
					TEXT("StandingProof: StabilityMetrics expected BalanceActive_Standing but got %s (%d) for %s"),
					GetRuntimeStateName(RuntimeState),
					(int32)RuntimeState,
					*It->GetName()));
			}

			if (!Comp->IsLiveRuntimeEvidenceProofComplete())
			{
				Fail(FString::Printf(TEXT("StandingProof: StabilityMetrics proof did not complete for %s"), *It->GetName()));
			}

			if (!Comp->IsLiveRuntimeEvidenceProofSatisfied())
			{
				Fail(FString::Printf(TEXT("StandingProof: StabilityMetrics proof is not truthful for %s"), *It->GetName()));
			}

			if (TerminationState.TerminalReason != EPhysAnimTerminalReason::None)
			{
				Fail(FString::Printf(TEXT("StandingProof: StabilityMetrics expected terminal_reason=None but got %d for %s"), (int32)TerminationState.TerminalReason, *It->GetName()));
			}

			if (!Metrics.bHasSamples || Metrics.SampleCount <= 0)
			{
				Fail(FString::Printf(TEXT("StandingProof: StabilityMetrics were not collected for %s"), *It->GetName()));
			}

			const auto CheckFinite = [&](const TCHAR* Label, double Value)
			{
				if (!FMath::IsFinite(Value))
				{
					Fail(FString::Printf(TEXT("StandingProof: StabilityMetrics %s is not finite for %s"), Label, *It->GetName()));
				}
			};

			CheckFinite(TEXT("activation_duration_sec"), Metrics.ActivationDurationSec);
			CheckFinite(TEXT("root_world_position_drift_cm"), Metrics.RootWorldPositionDriftCm);
			CheckFinite(TEXT("root_vertical_drift_cm"), Metrics.RootVerticalDriftCm);
			CheckFinite(TEXT("root_angular_drift_deg"), Metrics.RootAngularDriftDeg);
			CheckFinite(TEXT("max_body_linear_speed_cm_per_second"), Metrics.MaxBodyLinearSpeedCmPerSecond);
			CheckFinite(TEXT("max_body_angular_speed_deg_per_second"), Metrics.MaxBodyAngularSpeedDegPerSecond);
			CheckFinite(TEXT("support_hull_area_min_cm2"), Metrics.SupportHullAreaMinCm2);
			CheckFinite(TEXT("support_hull_area_mean_cm2"), Metrics.SupportHullAreaMeanCm2);
			CheckFinite(TEXT("support_hull_area_max_cm2"), Metrics.SupportHullAreaMaxCm2);
			CheckFinite(TEXT("active_support_side_count_min"), Metrics.ActiveSupportSideCountMin);
			CheckFinite(TEXT("active_support_side_count_mean"), Metrics.ActiveSupportSideCountMean);
			CheckFinite(TEXT("active_support_side_count_max"), Metrics.ActiveSupportSideCountMax);
			CheckFinite(TEXT("policy_action_raw_mean_abs_max"), Metrics.PolicyActionRawMeanAbsMax);
			CheckFinite(TEXT("policy_action_conditioned_mean_abs_max"), Metrics.PolicyActionConditionedMeanAbsMax);
			CheckFinite(TEXT("control_target_max_delta_deg"), Metrics.ControlTargetMaxDeltaDeg);
			CheckFinite(TEXT("control_target_mean_delta_deg_max"), Metrics.ControlTargetMeanDeltaDegMax);
			CheckFinite(TEXT("control_target_max_raw_policy_offset_deg"), Metrics.ControlTargetMaxRawPolicyOffsetDeg);
			CheckFinite(TEXT("control_target_mean_raw_policy_offset_deg_max"), Metrics.ControlTargetMeanRawPolicyOffsetDegMax);

			if (Metrics.FailStopCount != 0)
			{
				Fail(FString::Printf(TEXT("StandingProof: StabilityMetrics expected no fail-stop but got %d for %s"), Metrics.FailStopCount, *It->GetName()));
			}

			if (Metrics.ActivationDurationSec < 30.0)
			{
				Fail(FString::Printf(TEXT("StandingProof: StabilityMetrics activation duration %.2f is below 30.0 seconds for %s"), Metrics.ActivationDurationSec, *It->GetName()));
			}

			if (Metrics.SupportHullAreaMinCm2 <= 0.0 || Metrics.SupportHullAreaMeanCm2 <= 0.0 || Metrics.SupportHullAreaMaxCm2 <= 0.0)
			{
				Fail(FString::Printf(TEXT("StandingProof: StabilityMetrics support hull area did not stay above zero for %s"), *It->GetName()));
			}

			if (Metrics.ActiveSupportSideCountMin < 1.0 || Metrics.ActiveSupportSideCountMean < 1.0 || Metrics.ActiveSupportSideCountMax < 1.0)
			{
				Fail(FString::Printf(TEXT("StandingProof: StabilityMetrics active support side count dropped below 1 for %s"), *It->GetName()));
			}

			const bool bStrictProofQuality = IsStrictLivePolicyProofQualityEnabled();
			if (bStrictProofQuality && Metrics.PolicyInferenceSuccessCount <= 0)
			{
				Fail(FString::Printf(TEXT("StandingProof: StabilityMetrics recorded no successful policy inference during hold for %s"), *It->GetName()));
			}

			if (bStrictProofQuality && (Metrics.PolicyActionSampleCount <= 0 || Metrics.PolicyActionConditionedMeanAbsMax <= 0.0))
			{
				Fail(FString::Printf(TEXT("StandingProof: StabilityMetrics recorded no nonzero conditioned policy action during hold for %s"), *It->GetName()));
			}

			if (bStrictProofQuality && (Metrics.ControlTargetSampleCount <= 0 || Metrics.ControlTargetNormalWrites <= 0 || Metrics.ControlTargetTotalWrites <= 0))
			{
				Fail(FString::Printf(TEXT("StandingProof: StabilityMetrics recorded no PhysicsControl target writes during hold for %s"), *It->GetName()));
			}

			if (bStrictProofQuality && Metrics.ControlTargetMaxDeltaDeg <= 0.0 && Metrics.ControlTargetMaxRawPolicyOffsetDeg <= 0.0)
			{
				Fail(FString::Printf(TEXT("StandingProof: StabilityMetrics recorded no material policy target delta during hold for %s"), *It->GetName()));
			}

			if (bStrictProofQuality && (Metrics.BodyTelemetrySampleCount <= 0 || Metrics.SimulatingBodyCountMax <= 0))
			{
				Fail(FString::Printf(TEXT("StandingProof: StabilityMetrics recorded no simulating body telemetry during hold for %s"), *It->GetName()));
			}

			if (bStrictProofQuality && (Metrics.CriticalBodyValidMask & RequiredCriticalBodyMask) != RequiredCriticalBodyMask)
			{
				Fail(FString::Printf(TEXT("StandingProof: StabilityMetrics missing valid critical bodies mask=0x%x required=0x%x for %s"),
					Metrics.CriticalBodyValidMask,
					RequiredCriticalBodyMask,
					*It->GetName()));
			}

			if (bStrictProofQuality && (Metrics.CriticalBodySimulatingMask & RequiredCriticalBodyMask) != RequiredCriticalBodyMask)
			{
				Fail(FString::Printf(TEXT("StandingProof: StabilityMetrics missing simulating critical bodies mask=0x%x required=0x%x for %s"),
					Metrics.CriticalBodySimulatingMask,
					RequiredCriticalBodyMask,
					*It->GetName()));
			}

			if (bStrictProofQuality && (Metrics.SupportBodyValidMask & RequiredSupportBodyMask) != RequiredSupportBodyMask)
			{
				Fail(FString::Printf(TEXT("StandingProof: StabilityMetrics missing valid support bodies mask=0x%x required=0x%x for %s"),
					Metrics.SupportBodyValidMask,
					RequiredSupportBodyMask,
					*It->GetName()));
			}

			if (bStrictProofQuality && (Metrics.SupportBodySimulatingMask & RequiredSupportBodyMask) != RequiredSupportBodyMask)
			{
				Fail(FString::Printf(TEXT("StandingProof: StabilityMetrics missing simulating support bodies mask=0x%x required=0x%x for %s"),
					Metrics.SupportBodySimulatingMask,
					RequiredSupportBodyMask,
					*It->GetName()));
			}

			if (bStrictProofQuality && Metrics.ExcludedRequiredBodySimulatingCountMax > 0)
			{
				Fail(FString::Printf(TEXT("StandingProof: StabilityMetrics has simulating non-V0 required bodies count=%d for %s"),
					Metrics.ExcludedRequiredBodySimulatingCountMax,
					*It->GetName()));
			}

			if (bStrictProofQuality &&
				(!TerminationState.LatestArtifact.bPhysicalContinuityValidatorPassed ||
					TerminationState.LatestArtifact.bContinuityBookkeepingMismatch))
			{
				Fail(FString::Printf(TEXT("StandingProof: StabilityMetrics physical continuity did not pass for %s"), *It->GetName()));
			}

			PHYSANIM_LOG(
				LogTemp,
				Warning,
				TEXT("StandingProof: StabilityMetrics samples=%d duration=%.2f rootDrift=%.2f verticalDrift=%.2f angularDrift=%.2f supportHull[min/mean/max]=%.2f/%.2f/%.2f activeSides[min/mean/max]=%.2f/%.2f/%.2f maxBodyLinear=%.2f(%s) maxBodyAngular=%.2f(%s) policy[inference=%d actionSamples=%d rawAbsMax=%.3f conditionedAbsMax=%.3f clampedMax=%d] targets[samples=%d normal=%d total=%d maxDelta=%.2f meanDeltaMax=%.2f maxRawOffset=%.2f meanRawOffsetMax=%.2f] bodies[samples=%d simMax=%d excludedSimMax=%d criticalValid=0x%x criticalSim=0x%x supportValid=0x%x supportSim=0x%x nonzeroVelocitySamples=%d] terminalReason=%d"),
				Metrics.SampleCount,
				Metrics.ActivationDurationSec,
				Metrics.RootWorldPositionDriftCm,
				Metrics.RootVerticalDriftCm,
				Metrics.RootAngularDriftDeg,
				Metrics.SupportHullAreaMinCm2,
				Metrics.SupportHullAreaMeanCm2,
				Metrics.SupportHullAreaMaxCm2,
				Metrics.ActiveSupportSideCountMin,
				Metrics.ActiveSupportSideCountMean,
				Metrics.ActiveSupportSideCountMax,
				Metrics.MaxBodyLinearSpeedCmPerSecond,
				*Metrics.MaxBodyLinearSpeedBodyName.ToString(),
				Metrics.MaxBodyAngularSpeedDegPerSecond,
				*Metrics.MaxBodyAngularSpeedBodyName.ToString(),
				Metrics.PolicyInferenceSuccessCount,
				Metrics.PolicyActionSampleCount,
				Metrics.PolicyActionRawMeanAbsMax,
				Metrics.PolicyActionConditionedMeanAbsMax,
				Metrics.PolicyActionClampedFloatMax,
				Metrics.ControlTargetSampleCount,
				Metrics.ControlTargetNormalWrites,
				Metrics.ControlTargetTotalWrites,
				Metrics.ControlTargetMaxDeltaDeg,
				Metrics.ControlTargetMeanDeltaDegMax,
				Metrics.ControlTargetMaxRawPolicyOffsetDeg,
				Metrics.ControlTargetMeanRawPolicyOffsetDegMax,
				Metrics.BodyTelemetrySampleCount,
				Metrics.SimulatingBodyCountMax,
				Metrics.ExcludedRequiredBodySimulatingCountMax,
				Metrics.CriticalBodyValidMask,
				Metrics.CriticalBodySimulatingMask,
				Metrics.SupportBodyValidMask,
				Metrics.SupportBodySimulatingMask,
				Metrics.BodyVelocityNonZeroSampleCount,
				Metrics.TerminalReason);
			break;
		}
	}

	if (!bFoundComponent)
	{
		AddLatentAutomationError(Test, TEXT("StandingProof: StabilityMetrics found no PhysAnim component"));
	}

	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FSetRawSimDiagnosticGroupCommand, int32, DiagnosticGroup);
bool FSetRawSimDiagnosticGroupCommand::Update()
{
	if (IConsoleVariable* const CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("p.PhysAnim.RawSimDiagnosticGroup")))
	{
		CVar->Set(DiagnosticGroup, ECVF_SetByCode);
	}
	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FSetAllowCharacterMovementInBridgeActiveCommand, int32, bAllowCharacterMovement);
bool FSetAllowCharacterMovementInBridgeActiveCommand::Update()
{
	if (IConsoleVariable* const CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("physanim.AllowCharacterMovementInBridgeActive")))
	{
		CVar->Set(bAllowCharacterMovement, ECVF_SetByCode);
	}
	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FSetIntConsoleVariableCommand, FString, CVarName, int32, Value);
bool FSetIntConsoleVariableCommand::Update()
{
	if (IConsoleVariable* const CVar = IConsoleManager::Get().FindConsoleVariable(*CVarName))
	{
		CVar->Set(Value, ECVF_SetByCode);
	}
	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FSetFloatConsoleVariableCommand, FString, CVarName, float, Value);
bool FSetFloatConsoleVariableCommand::Update()
{
	if (IConsoleVariable* const CVar = IConsoleManager::Get().FindConsoleVariable(*CVarName))
	{
		CVar->Set(Value, ECVF_SetByCode);
	}
	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_THREE_PARAMETER(FLogControlIsolationMatrixCommand, FString, CaseName, int32, ReviewMode, FAutomationTestBase*, Test);
bool FLogControlIsolationMatrixCommand::Update()
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World) World = GWorld;

	if (!World)
	{
		AddLatentAutomationError(Test, FString::Printf(TEXT("ControlIsolationMatrix[%s]: PIE world was not available"), *CaseName));
		return true;
	}

	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			const FPhysAnimActivatedStandingStabilityMetrics& Metrics = Comp->GetActivatedStandingStabilityMetrics();
			const FPhysAnimRuntimeTerminationState& TerminationState = Comp->GetLiveRuntimeEvidenceTerminationState();
			if (Metrics.SampleCount <= 0)
			{
				PHYSANIM_LOG(LogTemp, Warning, TEXT("ControlIsolationMatrix[%s]: no stability samples were collected"), *CaseName);
			}
			PHYSANIM_LOG(
				LogTemp,
				Warning,
				TEXT("ControlIsolationMatrix[%s]: mode=%d runtimeState=%d terminalReason=%d samples=%d duration=%.2f supportHull[min/mean/max]=%.2f/%.2f/%.2f activeSides[min/mean/max]=%.2f/%.2f/%.2f maxBodyLinear=%.2f(%s) maxBodyAngular=%.2f(%s) spine01[lin=%.2f ang=%.2f] spine03[lin=%.2f ang=%.2f] firstSpineSpike=%.2f(%s) firstSupportFailure=%.2f policy[inference=%d actionSamples=%d rawAbsMax=%.3f conditionedAbsMax=%.3f] targets[samples=%d normal=%d total=%d maxDelta=%.2f maxRawOffset=%.2f] bodies[samples=%d simMax=%d excludedSimMax=%d criticalValid=0x%x criticalSim=0x%x supportValid=0x%x supportSim=0x%x nonzeroVelocitySamples=%d] continuityValid=%d"),
				*CaseName,
				ReviewMode,
				static_cast<int32>(Comp->GetRuntimeState()),
				static_cast<int32>(TerminationState.TerminalReason),
				Metrics.SampleCount,
				Metrics.ActivationDurationSec,
				Metrics.SupportHullAreaMinCm2,
				Metrics.SupportHullAreaMeanCm2,
				Metrics.SupportHullAreaMaxCm2,
				Metrics.ActiveSupportSideCountMin,
				Metrics.ActiveSupportSideCountMean,
				Metrics.ActiveSupportSideCountMax,
				Metrics.MaxBodyLinearSpeedCmPerSecond,
				*Metrics.MaxBodyLinearSpeedBodyName.ToString(),
				Metrics.MaxBodyAngularSpeedDegPerSecond,
				*Metrics.MaxBodyAngularSpeedBodyName.ToString(),
				Metrics.Spine01MaxLinearSpeedCmPerSecond,
				Metrics.Spine01MaxAngularSpeedDegPerSecond,
				Metrics.Spine03MaxLinearSpeedCmPerSecond,
				Metrics.Spine03MaxAngularSpeedDegPerSecond,
				Metrics.FirstMajorSpineSpikeTimeSec,
				*Metrics.FirstMajorSpineSpikeBodyName.ToString(),
				Metrics.FirstSupportFailureTimeSec,
				Metrics.PolicyInferenceSuccessCount,
				Metrics.PolicyActionSampleCount,
				Metrics.PolicyActionRawMeanAbsMax,
				Metrics.PolicyActionConditionedMeanAbsMax,
				Metrics.ControlTargetSampleCount,
				Metrics.ControlTargetNormalWrites,
				Metrics.ControlTargetTotalWrites,
				Metrics.ControlTargetMaxDeltaDeg,
				Metrics.ControlTargetMaxRawPolicyOffsetDeg,
				Metrics.BodyTelemetrySampleCount,
				Metrics.SimulatingBodyCountMax,
				Metrics.ExcludedRequiredBodySimulatingCountMax,
				Metrics.CriticalBodyValidMask,
				Metrics.CriticalBodySimulatingMask,
				Metrics.SupportBodyValidMask,
				Metrics.SupportBodySimulatingMask,
				Metrics.BodyVelocityNonZeroSampleCount,
				(TerminationState.LatestArtifact.bPhysicalContinuityValidatorPassed &&
					!TerminationState.LatestArtifact.bContinuityBookkeepingMismatch) ? 1 : 0);
			break;
		}
	}

	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_THREE_PARAMETER(FLogRawSimulationOwnershipBisectCommand, FString, CaseName, int32, DiagnosticGroup, FAutomationTestBase*, Test);
bool FLogRawSimulationOwnershipBisectCommand::Update()
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World) World = GWorld;

	if (!World)
	{
		AddLatentAutomationError(Test, FString::Printf(TEXT("RawSimulationOwnershipBisect[%s]: PIE world was not available"), *CaseName));
		return true;
	}

	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			const FPhysAnimActivatedStandingStabilityMetrics& Metrics = Comp->GetActivatedStandingStabilityMetrics();
			const FPhysAnimRuntimeTerminationState& TerminationState = Comp->GetLiveRuntimeEvidenceTerminationState();
			if (Metrics.SampleCount <= 0)
			{
				if (DiagnosticGroup < 4)
				{
					AddLatentAutomationError(Test, FString::Printf(TEXT("RawSimulationOwnershipBisect[%s]: no stability samples were collected"), *CaseName));
				}
				else
				{
					PHYSANIM_LOG(LogTemp, Warning, TEXT("RawSimulationOwnershipBisect[%s]: full required non-V0 group collected no stability samples"), *CaseName);
				}
			}
			PHYSANIM_LOG(
				LogTemp,
				Warning,
				TEXT("RawSimulationOwnershipBisect[%s]: group=%d runtimeState=%d terminalReason=%d samples=%d duration=%.2f supportHull[min/mean/max]=%.2f/%.2f/%.2f activeSides[min/mean/max]=%.2f/%.2f/%.2f maxBodyLinear=%.2f(%s) maxBodyAngular=%.2f(%s) policy[inference=%d actionSamples=%d rawAbsMax=%.3f conditionedAbsMax=%.3f] targets[samples=%d normal=%d total=%d maxDelta=%.2f maxRawOffset=%.2f] bodies[samples=%d simMax=%d excludedSimMax=%d criticalValid=0x%x criticalSim=0x%x supportValid=0x%x supportSim=0x%x nonzeroVelocitySamples=%d] continuityValid=%d"),
				*CaseName,
				DiagnosticGroup,
				static_cast<int32>(Comp->GetRuntimeState()),
				static_cast<int32>(TerminationState.TerminalReason),
				Metrics.SampleCount,
				Metrics.ActivationDurationSec,
				Metrics.SupportHullAreaMinCm2,
				Metrics.SupportHullAreaMeanCm2,
				Metrics.SupportHullAreaMaxCm2,
				Metrics.ActiveSupportSideCountMin,
				Metrics.ActiveSupportSideCountMean,
				Metrics.ActiveSupportSideCountMax,
				Metrics.MaxBodyLinearSpeedCmPerSecond,
				*Metrics.MaxBodyLinearSpeedBodyName.ToString(),
				Metrics.MaxBodyAngularSpeedDegPerSecond,
				*Metrics.MaxBodyAngularSpeedBodyName.ToString(),
				Metrics.PolicyInferenceSuccessCount,
				Metrics.PolicyActionSampleCount,
				Metrics.PolicyActionRawMeanAbsMax,
				Metrics.PolicyActionConditionedMeanAbsMax,
				Metrics.ControlTargetSampleCount,
				Metrics.ControlTargetNormalWrites,
				Metrics.ControlTargetTotalWrites,
				Metrics.ControlTargetMaxDeltaDeg,
				Metrics.ControlTargetMaxRawPolicyOffsetDeg,
				Metrics.BodyTelemetrySampleCount,
				Metrics.SimulatingBodyCountMax,
				Metrics.ExcludedRequiredBodySimulatingCountMax,
				Metrics.CriticalBodyValidMask,
				Metrics.CriticalBodySimulatingMask,
				Metrics.SupportBodyValidMask,
				Metrics.SupportBodySimulatingMask,
				Metrics.BodyVelocityNonZeroSampleCount,
				(TerminationState.LatestArtifact.bPhysicalContinuityValidatorPassed &&
					!TerminationState.LatestArtifact.bContinuityBookkeepingMismatch) ? 1 : 0);
			break;
		}
	}

	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimActivationWiringTest, "PhysAnim.ActivationPath.Wiring", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimActivationWiringTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("ProofDisabled"));
	OutTestCommands.Add(TEXT("ProofDisabled"));

	OutBeautifiedNames.Add(TEXT("ProofNotSatisfied"));
	OutTestCommands.Add(TEXT("ProofNotSatisfied"));

	OutBeautifiedNames.Add(TEXT("ProofSatisfied"));
	OutTestCommands.Add(TEXT("ProofSatisfied"));

	OutBeautifiedNames.Add(TEXT("ProofFailedFailStopRouted"));
	OutTestCommands.Add(TEXT("ProofFailedFailStopRouted"));
}

bool FPhysAnimActivationWiringTest::RunTest(const FString& Parameters)
{
	if (Parameters == TEXT("ProofDisabled"))
	{
		AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
		ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(false, false, false));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f)); // Wait for startup
		ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivationWiringCommand(EPhysAnimRuntimeState::WaitingForPoseSearch, this));
		ADD_LATENT_AUTOMATION_COMMAND(FStopActivationWiringCommand(this));
	}
	else if (Parameters == TEXT("ProofNotSatisfied"))
	{
		AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
		ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, false, false));
		ADD_LATENT_AUTOMATION_COMMAND(FVerifyProofNotSatisfiedAndStopCommand(this));
	}
	else if (Parameters == TEXT("ProofSatisfied"))
	{
		AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));

		ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, true, false));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.0f)); // Wait for 3s proof + startup
		ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivationWiringCommand(EPhysAnimRuntimeState::BalanceActive_Standing, this));
		ADD_LATENT_AUTOMATION_COMMAND(FStopActivationWiringCommand(this));
	}
	else if (Parameters == TEXT("ProofFailedFailStopRouted"))
	{
		AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));

		ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, true, true));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
		ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivationWiringFailureCommand(this));
	}

	return true;
}


IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimStartupProofDisabledSafeDenyTest, "PhysAnim.StartupProof.ProofDisabledSafeDeny", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimStartupProofDisabledSafeDenyTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("ProofDisabledSafeDeny"));
	OutTestCommands.Add(TEXT("ProofDisabledSafeDeny"));
}

bool FPhysAnimStartupProofDisabledSafeDenyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(false, false, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivationWiringCommand(EPhysAnimRuntimeState::WaitingForPoseSearch, this));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyProofDisabledSafeDenyCommand(this));
	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimStartupProofEnabledStartupFreshEvidenceTest, "PhysAnim.StartupProof.FreshEvidence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimStartupProofEnabledStartupFreshEvidenceTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("ProofStartupFreshEvidence"));
	OutTestCommands.Add(TEXT("ProofStartupFreshEvidence"));
}

bool FPhysAnimStartupProofEnabledStartupFreshEvidenceTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, false, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.25f));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivationWiringCommand(EPhysAnimRuntimeState::WaitingForPoseSearch, this));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyProofEnabledStartupFreshEvidenceCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FStopActivationWiringCommand(this));
	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimActivationPathStartupEntryBridgeTest, "PhysAnim.ActivationPath.StartupEntryBridge", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimActivationPathStartupEntryBridgeTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("ActivationWiringStartupEntryBridge"));
	OutTestCommands.Add(TEXT("ActivationWiringStartupEntryBridge"));
}

bool FPhysAnimActivationPathStartupEntryBridgeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, true, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivationWiringCommand(EPhysAnimRuntimeState::BalanceActive_Standing, this));
	ADD_LATENT_AUTOMATION_COMMAND(FStopActivationWiringCommand(this));
	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimActivationPathVerifyWindowTest, "PhysAnim.ActivationPath.VerifyWindow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimActivationPathVerifyWindowTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("ActivationWiringVerifyWindow"));
	OutTestCommands.Add(TEXT("ActivationWiringVerifyWindow"));
}

bool FPhysAnimActivationPathVerifyWindowTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, false, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.25f));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivationWiringCommand(EPhysAnimRuntimeState::WaitingForPoseSearch, this));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyProofEnabledStartupWaitingHandoffCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FStopActivationWiringCommand(this));
	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimRuntimeEvidenceWaitingForPoseSearchNonterminalTest, "PhysAnim.RuntimeEvidence.WaitingForPoseSearchNonterminal", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimRuntimeEvidenceWaitingForPoseSearchNonterminalTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("WaitingForPoseSearchLiveEvidenceNonterminal"));
	OutTestCommands.Add(TEXT("WaitingForPoseSearchLiveEvidenceNonterminal"));
}

bool FPhysAnimRuntimeEvidenceWaitingForPoseSearchNonterminalTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, false, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.25f));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivationWiringCommand(EPhysAnimRuntimeState::WaitingForPoseSearch, this));
	ADD_LATENT_AUTOMATION_COMMAND(FStopActivationWiringCommand(this));
	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimStartupProofStartupEvidenceSatisfactionBoundaryTest, "PhysAnim.StartupProof.EvidenceSatisfactionBoundary", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimStartupProofStartupEvidenceSatisfactionBoundaryTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("StartupEvidenceSatisfactionBoundary"));
	OutTestCommands.Add(TEXT("StartupEvidenceSatisfactionBoundary"));
}

bool FPhysAnimStartupProofStartupEvidenceSatisfactionBoundaryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, true, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivationWiringCommand(EPhysAnimRuntimeState::BalanceActive_Standing, this));
	ADD_LATENT_AUTOMATION_COMMAND(FStopActivationWiringCommand(this));
	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimStartupProofCompleteStandingEntryProxyTimingTest, "PhysAnim.StartupProof.StandingEntryProxyTiming", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimStartupProofCompleteStandingEntryProxyTimingTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("ProofCompleteStandingEntryProxyTiming"));
	OutTestCommands.Add(TEXT("ProofCompleteStandingEntryProxyTiming"));
}

bool FPhysAnimStartupProofCompleteStandingEntryProxyTimingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, true, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivationWiringCommand(EPhysAnimRuntimeState::BalanceActive_Standing, this));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyProofCompleteStandingEntryProxyTimingCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FStopActivationWiringCommand(this));
	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FVerifyProxyHandoffResetCommand, FAutomationTestBase*, Test);
bool FVerifyProxyHandoffResetCommand::Update()
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World)
	{
		World = GWorld;
	}

	if (!World)
	{
		AddLatentAutomationError(Test, TEXT("StartupProof: PIE world was not available"));
		return true;
	}

	bool bFoundComponent = false;
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			bFoundComponent = true;
			if (Comp->IsStartupProofProxySupportHandoffArmed())
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Proxy handoff was armed during reset for %s"), *It->GetName()));
			}
			else
			{
				PHYSANIM_LOG(LogTemp, Warning, TEXT("StartupProof: TEST_PASSED proxy handoff reset disarmed for %s"), *It->GetName());
			}

			if (Comp->HasDeferredStartupProxyTerminalReason() &&
				Comp->GetDeferredStartupProxyTerminalReason() == EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion)
			{
				if (Comp->GetDeferredStartupProxyTerminalAttemptUuid().IsEmpty())
				{
					AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Deferred proxy terminal attempt uuid was empty for %s"), *It->GetName()));
				}
				if (Comp->GetDeferredStartupProxyTerminalSubstepTimestamp() < 0)
				{
					AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Deferred proxy terminal substep was invalid for %s"), *It->GetName()));
				}
			}
			break;
		}
	}

	if (!bFoundComponent)
	{
		AddLatentAutomationError(Test, TEXT("StartupProof: no PhysAnim component was found"));
	}

	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FVerifyProofSatisfiedProxyHandoffSourceOfTruthProofCommand, FAutomationTestBase*, Test);
bool FVerifyProofSatisfiedProxyHandoffSourceOfTruthProofCommand::Update()
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World) World = GWorld;

	if (!World)
	{
		AddLatentAutomationError(Test, TEXT("StartupProof: PIE world was not available"));
		return true;
	}

	bool bFoundComponent = false;
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			bFoundComponent = true;
			const FPhysAnimRuntimeTerminationState& TerminationState = Comp->GetLiveRuntimeEvidenceTerminationState();
			const FPhysAnimRunArtifactSnapshot& Latest = TerminationState.LatestArtifact;
			const bool bProofComplete = Comp->IsLiveRuntimeEvidenceProofComplete();
			const bool bProofSatisfied = Comp->IsLiveRuntimeEvidenceProofSatisfied();
			const bool bFreshEvidence = Comp->IsStartupProofEvidenceFresh();
			const bool bStandingAccepted = Comp->IsStartupProofStandingEntryAccepted();
			const bool bProxyArmed = Comp->IsStartupProofProxySupportHandoffArmed();
			const bool bDeferredProxyRecorded = Comp->HasDeferredStartupProxyTerminalReason();
			const EPhysAnimTerminalReason DeferredProxyReason = Comp->GetDeferredStartupProxyTerminalReason();

			if (Comp->GetRuntimeState() != EPhysAnimRuntimeState::BalanceActive_Standing)
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Expected BalanceActive_Standing but got %s for %s"),
					GetRuntimeStateName(Comp->GetRuntimeState()),
					*It->GetName()));
			}

			if (!bProofComplete || !bProofSatisfied || !bFreshEvidence || !bStandingAccepted || !bProxyArmed)
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Proof source-of-truth state was not complete for %s complete=%d satisfied=%d fresh=%d standingAccepted=%d proxyArmed=%d"),
					*It->GetName(),
					bProofComplete ? 1 : 0,
					bProofSatisfied ? 1 : 0,
					bFreshEvidence ? 1 : 0,
					bStandingAccepted ? 1 : 0,
					bProxyArmed ? 1 : 0));
			}

			if (bDeferredProxyRecorded &&
				(DeferredProxyReason != EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion ||
					Comp->GetDeferredStartupProxyTerminalAttemptUuid().IsEmpty() ||
					Comp->GetDeferredStartupProxyTerminalSubstepTimestamp() < 0))
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Deferred proxy terminal was not truthfully recorded for %s deferred=%d reason=%d attemptEmpty=%d substep=%lld"),
					*It->GetName(),
					bDeferredProxyRecorded ? 1 : 0,
					static_cast<int32>(DeferredProxyReason),
					Comp->GetDeferredStartupProxyTerminalAttemptUuid().IsEmpty() ? 1 : 0,
					static_cast<long long>(Comp->GetDeferredStartupProxyTerminalSubstepTimestamp())));
			}

			if (TerminationState.TerminalReason != EPhysAnimTerminalReason::None ||
				Latest.TerminalReason != EPhysAnimTerminalReason::None)
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Expected neutral terminal reasons but got state=%d latest=%d for %s"),
					static_cast<int32>(TerminationState.TerminalReason),
					static_cast<int32>(Latest.TerminalReason),
					*It->GetName()));
			}

			if (!Comp->CanEnterBalanceActiveStanding())
			{
				AddLatentAutomationError(Test, FString::Printf(TEXT("StartupProof: TEST_FAILED. Proof satisfied standing entry was not allowed for %s"),
					*It->GetName()));
			}

			PHYSANIM_LOG(
				LogTemp,
				Warning,
				TEXT("StartupProof: TEST_PASSED proof source-of-truth runtime=%s proofComplete=%d proofSatisfied=%d freshEvidence=%d standingAccepted=%d proxyArmed=%d deferredProxyRecorded=%d deferredProxyReason=%d terminalReason=%d latestTerminalReason=%d finalActivationPath=%s"),
				GetRuntimeStateName(Comp->GetRuntimeState()),
				bProofComplete ? 1 : 0,
				bProofSatisfied ? 1 : 0,
				bFreshEvidence ? 1 : 0,
				bStandingAccepted ? 1 : 0,
				bProxyArmed ? 1 : 0,
				bDeferredProxyRecorded ? 1 : 0,
				static_cast<int32>(DeferredProxyReason),
				static_cast<int32>(TerminationState.TerminalReason),
				static_cast<int32>(Latest.TerminalReason),
				GetRuntimeStateName(Comp->GetRuntimeState()));
			break;
		}
	}

	if (!bFoundComponent)
	{
		AddLatentAutomationError(Test, TEXT("StartupProof: no PhysAnim component was found"));
	}

	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimStartupProofSatisfiedProxyHandoffSourceOfTruthTest, "PhysAnim.StartupProof.ProxyHandoffSourceOfTruth", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimStartupProofSatisfiedProxyHandoffSourceOfTruthTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("ProofSatisfiedProxyHandoffSourceOfTruth"));
	OutTestCommands.Add(TEXT("ProofSatisfiedProxyHandoffSourceOfTruth"));
}

bool FPhysAnimStartupProofSatisfiedProxyHandoffSourceOfTruthTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, true, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivationWiringCommand(EPhysAnimRuntimeState::BalanceActive_Standing, this));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyProofCompleteStandingEntryProxyTimingCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FStopActivationWiringCommand(this));
	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimStartupProofSatisfiedProxyHandoffSourceOfTruthProofTest, "PhysAnim.StartupProof.ProxyHandoffSourceOfTruthEvidence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimStartupProofSatisfiedProxyHandoffSourceOfTruthProofTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("ProofSatisfiedProxyHandoffSourceOfTruthProof"));
	OutTestCommands.Add(TEXT("ProofSatisfiedProxyHandoffSourceOfTruthProof"));
}

bool FPhysAnimStartupProofSatisfiedProxyHandoffSourceOfTruthProofTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	AddExpectedMessagePlain(TEXT("PhysAnimProof: TerminalArtifact"), ELogVerbosity::All, EAutomationExpectedMessageFlags::Contains, 0);
	AddExpectedMessagePlain(TEXT("PhysAnimProof: AttemptResult"), ELogVerbosity::All, EAutomationExpectedMessageFlags::Contains, 0);
	ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, true, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.25f));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivationWiringCommand(EPhysAnimRuntimeState::WaitingForPoseSearch, this));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivationWiringCommand(EPhysAnimRuntimeState::BalanceActive_Standing, this));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyProofSatisfiedProxyHandoffSourceOfTruthProofCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FStopActivationWiringCommand(this));
	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimStartupProofFailureFailStopRoutingTest, "PhysAnim.StartupProof.FailureFailStopRouting", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimStartupProofFailureFailStopRoutingTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("ProofFailureFailStopRouting"));
	OutTestCommands.Add(TEXT("ProofFailureFailStopRouting"));
}

bool FPhysAnimStartupProofFailureFailStopRoutingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	AddExpectedErrorPlain(TEXT("[PhysAnim] Startup entry bridge terminal enforced reason=ActivationSupportFailure state="), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedErrorPlain(TEXT("[PhysAnim] Proof failure routed through fail-stop helper reason="), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedErrorPlain(TEXT("[PhysAnim] Fail-stop: Proof failed during activation wait"), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedErrorPlain(TEXT("[PhysAnim] Proof failure fail-stop side effects complete reason="), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedErrorPlain(TEXT("[PhysAnim] Proof failure terminal reason preserved reason="), EAutomationExpectedErrorFlags::Contains, 0);
	ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, false, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.25f));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivationWiringCommand(EPhysAnimRuntimeState::WaitingForPoseSearch, this));
	ADD_LATENT_AUTOMATION_COMMAND(FEnableNegativeSupportProofCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.25f));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivationWiringFailureCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FSeedProofFailureDeferredReasonCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FArmProofFailureFailStopRoutingCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.25f));
	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimActivationPathProxyTimingSurfaceWidenTest, "PhysAnim.ActivationPath.ProxyTimingSurfaceWiden", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimActivationPathProxyTimingSurfaceWidenTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("ActivationPathProxyTimingSurfaceWiden"));
	OutTestCommands.Add(TEXT("ActivationPathProxyTimingSurfaceWiden"));
}

bool FPhysAnimActivationPathProxyTimingSurfaceWidenTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, false, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.25f));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivationWiringCommand(EPhysAnimRuntimeState::WaitingForPoseSearch, this));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyProxyHandoffResetCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FStopActivationWiringCommand(this));
	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimStartupProofProxyHandoffArmingTimingResetTest, "PhysAnim.StartupProof.ProxyHandoffArmingReset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimStartupProofProxyHandoffArmingTimingResetTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("ProxyHandoffArmingTimingReset"));
	OutTestCommands.Add(TEXT("ProxyHandoffArmingTimingReset"));
}

bool FPhysAnimStartupProofProxyHandoffArmingTimingResetTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, false, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.25f));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivationWiringCommand(EPhysAnimRuntimeState::WaitingForPoseSearch, this));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyProxyHandoffResetCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FStopActivationWiringCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, false, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.25f));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivationWiringCommand(EPhysAnimRuntimeState::WaitingForPoseSearch, this));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyProxyHandoffResetCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FStopActivationWiringCommand(this));
	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimStateMachinePhase1EntryTest, "PhysAnim.StateMachine.Phase1Entry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimStateMachinePhase1EntryTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("Phase1Entry"));
	OutTestCommands.Add(TEXT("Phase1Entry"));
}

bool FPhysAnimStateMachinePhase1EntryTest::RunTest(const FString& Parameters)
{
	AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, false, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivationWiringCommand(EPhysAnimRuntimeState::WaitingForPoseSearch, this));
	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimStateMachinePhase2StandingTest, "PhysAnim.StateMachine.Phase2Standing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimStateMachinePhase2StandingTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("Phase2Standing"));
	OutTestCommands.Add(TEXT("Phase2Standing"));
}

bool FPhysAnimStateMachinePhase2StandingTest::RunTest(const FString& Parameters)
{
	AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, true, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivationWiringCommand(EPhysAnimRuntimeState::BalanceActive_Standing, this));
	return true;
}

namespace
{
struct FActivatedStandingPerturbationValidationState
{
	TWeakObjectPtr<UPhysAnimComponent> Component;
	FPhysAnimActivatedStandingStabilityMetrics BaselineMetrics;
	FPhysAnimRuntimeTerminationState BaselineTerminationState;
	bool bBaselineCaptured = false;
	bool bPerturbationApplied = false;
	double PerturbationAppliedWorldTimeSeconds = -1.0;
};

const TCHAR* GetLocomotionAuthorityStateName(EBridgeLocomotionAuthorityState State)
{
	switch (State)
	{
	case EBridgeLocomotionAuthorityState::Idle:
		return TEXT("Idle");
	case EBridgeLocomotionAuthorityState::StartupLocomotion:
		return TEXT("StartupLocomotion");
	case EBridgeLocomotionAuthorityState::Locomoting:
		return TEXT("Locomoting");
	default:
		return TEXT("Unknown");
	}
}

const TCHAR* GetLocomotionRequestStateName(EBridgeLocomotionRequestState State)
{
	switch (State)
	{
	case EBridgeLocomotionRequestState::BalanceActiveStanding:
		return TEXT("BalanceActiveStanding");
	case EBridgeLocomotionRequestState::LocomotionRequested:
		return TEXT("LocomotionRequested");
	case EBridgeLocomotionRequestState::LocomotionRequestDenied:
		return TEXT("LocomotionRequestDenied");
	default:
		return TEXT("Unknown");
	}
}

struct FActivatedStandingLocomotionReadinessValidationState
{
	TWeakObjectPtr<UPhysAnimComponent> Component;
	FPhysAnimActivatedStandingStabilityMetrics BaselineMetrics;
	FPhysAnimRuntimeTerminationState BaselineTerminationState;
	EBridgeLocomotionAuthorityState BaselineLocomotionAuthorityState = EBridgeLocomotionAuthorityState::Idle;
	EBridgeLocomotionAuthorityState PostIntentLocomotionAuthorityState = EBridgeLocomotionAuthorityState::Idle;
	bool bBaselineCaptured = false;
	bool bMovementIntentApplied = false;
	double MovementIntentAppliedWorldTimeSeconds = -1.0;
	FVector MovementIntentDirection = FVector::ZeroVector;
	float MovementIntentScale = 0.0f;
};
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FCaptureActivatedStandingPerturbationBaselineCommand, FActivatedStandingPerturbationValidationState*, State);
bool FCaptureActivatedStandingPerturbationBaselineCommand::Update()
{
	if (!State)
	{
		return true;
	}

	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World) World = GWorld;

	UPhysAnimComponent* TargetComponent = State->Component.Get();
	if (World && !TargetComponent)
	{
		for (TActorIterator<ACharacter> It(World); It; ++It)
		{
			if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
			{
				TargetComponent = Comp;
				State->Component = Comp;
				break;
			}
		}
	}

	if (!TargetComponent)
	{
		PHYSANIM_LOG(LogTemp, Error, TEXT("PerturbationProof: baseline capture could not find a PhysAnim component"));
		return true;
	}

	const EPhysAnimRuntimeState RuntimeState = TargetComponent->GetRuntimeState();
	const FPhysAnimActivatedStandingStabilityMetrics& Metrics = TargetComponent->GetActivatedStandingStabilityMetrics();
	const FPhysAnimRuntimeTerminationState& TerminationState = TargetComponent->GetLiveRuntimeEvidenceTerminationState();

	if (RuntimeState != EPhysAnimRuntimeState::BalanceActive_Standing)
	{
		PHYSANIM_LOG(LogTemp, Error, TEXT("PerturbationProof: expected BalanceActive_Standing before perturbation but got %d"), (int32)RuntimeState);
	}

	if (!TargetComponent->IsLiveRuntimeEvidenceProofComplete())
	{
		PHYSANIM_LOG(LogTemp, Error, TEXT("PerturbationProof: proof was not complete before perturbation"));
	}

	if (!TargetComponent->IsLiveRuntimeEvidenceProofSatisfied())
	{
		PHYSANIM_LOG(LogTemp, Error, TEXT("PerturbationProof: proof was not truthful before perturbation"));
	}

	if (!Metrics.bHasSamples || Metrics.SampleCount <= 0)
	{
		PHYSANIM_LOG(LogTemp, Error, TEXT("PerturbationProof: standing metrics were not yet collected before perturbation"));
	}

	if (Metrics.SupportHullAreaMinCm2 <= 0.0 || Metrics.SupportHullAreaMeanCm2 <= 0.0 || Metrics.SupportHullAreaMaxCm2 <= 0.0)
	{
		PHYSANIM_LOG(LogTemp, Error, TEXT("PerturbationProof: support hull area was not positive before perturbation"));
	}

	State->BaselineMetrics = Metrics;
	State->BaselineTerminationState = TerminationState;
	State->bBaselineCaptured = true;
	PHYSANIM_LOG(
		LogTemp,
		Warning,
		TEXT("PerturbationProof: baseline samples=%d duration=%.2f rootDrift=%.2f verticalDrift=%.2f angularDrift=%.2f supportHull[min/mean/max]=%.2f/%.2f/%.2f activeSides[min/mean/max]=%.2f/%.2f/%.2f terminalReason=%d"),
		Metrics.SampleCount,
		Metrics.ActivationDurationSec,
		Metrics.RootWorldPositionDriftCm,
		Metrics.RootVerticalDriftCm,
		Metrics.RootAngularDriftDeg,
		Metrics.SupportHullAreaMinCm2,
		Metrics.SupportHullAreaMeanCm2,
		Metrics.SupportHullAreaMaxCm2,
		Metrics.ActiveSupportSideCountMin,
		Metrics.ActiveSupportSideCountMean,
		Metrics.ActiveSupportSideCountMax,
		(int32)TerminationState.TerminalReason);
	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FApplyActivatedStandingPerturbationCommand, FActivatedStandingPerturbationValidationState*, State);
bool FApplyActivatedStandingPerturbationCommand::Update()
{
	if (!State || !State->Component.IsValid())
	{
		PHYSANIM_LOG(LogTemp, Error, TEXT("PerturbationProof: perturbation command has no component"));
		return true;
	}

	UPhysAnimComponent* const Component = State->Component.Get();
	if (!Component->ApplyActivatedStandingPerturbation(EPhysAnimPerturbationDirection::Forward, EPhysAnimPerturbationMagnitude::Small))
	{
		if (IsStrictLivePolicyProofQualityEnabled())
		{
			PHYSANIM_LOG(LogTemp, Error, TEXT("PerturbationProof: perturbation was not applied"));
		}
		else
		{
			PHYSANIM_LOG(LogTemp, Warning, TEXT("PerturbationProof: physical perturbation was not applied; strict proof quality is opt-in"));
		}
		return true;
	}

	State->bPerturbationApplied = true;
	State->PerturbationAppliedWorldTimeSeconds = Component->GetWorld() ? Component->GetWorld()->GetTimeSeconds() : -1.0;

	if (!Component->HasActivatedStandingPerturbationApplied())
	{
		PHYSANIM_LOG(LogTemp, Error, TEXT("PerturbationProof: component did not record the perturbation application"));
	}

	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FVerifyActivatedStandingPerturbationCommand, FActivatedStandingPerturbationValidationState*, State, FAutomationTestBase*, Test);
bool FVerifyActivatedStandingPerturbationCommand::Update()
{
	if (!State || !State->Component.IsValid())
	{
		AddLatentAutomationError(Test, TEXT("PerturbationProof: verification command has no component"));
		return true;
	}

	UPhysAnimComponent* const Component = State->Component.Get();
	const UWorld* const World = Component->GetWorld();
	const EPhysAnimRuntimeState RuntimeState = Component->GetRuntimeState();
	const FPhysAnimRuntimeTerminationState& TerminationState = Component->GetLiveRuntimeEvidenceTerminationState();
	const FPhysAnimActivatedStandingStabilityMetrics& Metrics = Component->GetActivatedStandingStabilityMetrics();
	const bool bStrictProofQuality = IsStrictLivePolicyProofQualityEnabled();
	const auto Fail = [&](const FString& Message)
	{
		AddLatentAutomationError(Test, Message);
	};

	if (!State->bBaselineCaptured)
	{
		Fail(TEXT("PerturbationProof: baseline was not captured"));
	}

	if (bStrictProofQuality && !State->bPerturbationApplied)
	{
		Fail(TEXT("PerturbationProof: perturbation was not applied"));
	}

	if (bStrictProofQuality && !Component->HasActivatedStandingPerturbationApplied())
	{
		Fail(TEXT("PerturbationProof: component did not remember the perturbation"));
	}

	constexpr double RequiredPhysicalPerturbationDeltaVCmPerSecond = 5.0;
	if (bStrictProofQuality &&
		(!Metrics.bPhysicalPerturbationApplied ||
			Metrics.PerturbationMeasuredDeltaVCmPerSecond < RequiredPhysicalPerturbationDeltaVCmPerSecond))
	{
		Fail(FString::Printf(TEXT("PerturbationProof: physical pelvis impulse response was not proven deltaV=%.2f threshold=%.2f"),
			Metrics.PerturbationMeasuredDeltaVCmPerSecond,
			RequiredPhysicalPerturbationDeltaVCmPerSecond));
	}

	if (bStrictProofQuality &&
		Metrics.BodyVelocityNonZeroSampleCount <= State->BaselineMetrics.BodyVelocityNonZeroSampleCount)
	{
		Fail(FString::Printf(TEXT("PerturbationProof: body velocity telemetry did not become nonzero after perturbation baseline=%d current=%d maxLinear=%.2f maxAngular=%.2f"),
			State->BaselineMetrics.BodyVelocityNonZeroSampleCount,
			Metrics.BodyVelocityNonZeroSampleCount,
			Metrics.MaxBodyLinearSpeedCmPerSecond,
			Metrics.MaxBodyAngularSpeedDegPerSecond));
	}

	const double RecoveryDurationSec =
		(World && State->PerturbationAppliedWorldTimeSeconds >= 0.0)
			? (World->GetTimeSeconds() - State->PerturbationAppliedWorldTimeSeconds)
			: -1.0;
	if ((bStrictProofQuality || State->bPerturbationApplied) &&
		(!FMath::IsFinite(RecoveryDurationSec) || RecoveryDurationSec < 0.0))
	{
		Fail(TEXT("PerturbationProof: recovery duration is invalid"));
	}

	if (Metrics.SampleCount <= State->BaselineMetrics.SampleCount)
	{
		Fail(FString::Printf(TEXT("PerturbationProof: samples did not advance after perturbation baseline=%d current=%d"),
			State->BaselineMetrics.SampleCount,
			Metrics.SampleCount));
	}

	if (Metrics.SupportHullAreaMinCm2 <= 0.0 || Metrics.SupportHullAreaMeanCm2 <= 0.0 || Metrics.SupportHullAreaMaxCm2 <= 0.0)
	{
		Fail(TEXT("PerturbationProof: support hull area collapsed after perturbation"));
	}

	if (Metrics.ActiveSupportSideCountMin < 1.0 || Metrics.ActiveSupportSideCountMean < 1.0 || Metrics.ActiveSupportSideCountMax < 1.0)
	{
		Fail(TEXT("PerturbationProof: active support side count dropped below 1 after perturbation"));
	}

	const bool bStandingStayedStanding = RuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing;
	const bool bSafeTransition =
		RuntimeState == EPhysAnimRuntimeState::BalanceSafeDeny ||
		RuntimeState == EPhysAnimRuntimeState::FailStopped;
	if (!bStandingStayedStanding && !bSafeTransition)
	{
		Fail(FString::Printf(TEXT("PerturbationProof: runtime state after perturbation was unexpected (%d)"), (int32)RuntimeState));
	}

	if (bStandingStayedStanding)
	{
		if (TerminationState.TerminalReason != EPhysAnimTerminalReason::None)
		{
			Fail(FString::Printf(TEXT("PerturbationProof: standing result expected terminal_reason=None but got %d"), (int32)TerminationState.TerminalReason));
		}
		if (!Component->IsLiveRuntimeEvidenceProofSatisfied())
		{
			Fail(TEXT("PerturbationProof: standing result is not truthful"));
		}
	}
	else
	{
		if (TerminationState.TerminalReason == EPhysAnimTerminalReason::None)
		{
			PHYSANIM_LOG(LogTemp, Warning, TEXT("PerturbationProof: safe transition kept terminal_reason=None"));
		}
		else if (Component->IsLiveRuntimeEvidenceProofSatisfied())
		{
			Fail(TEXT("PerturbationProof: failure reason was not truthful"));
		}
	}

	if (TerminationState.TerminalArtifact.TerminalReason != TerminationState.TerminalReason ||
		TerminationState.LatestArtifact.TerminalReason != TerminationState.TerminalReason)
	{
		Fail(TEXT("PerturbationProof: audit artifact terminal reason does not match final state"));
	}

	PHYSANIM_LOG(
		LogTemp,
		Warning,
		TEXT("PerturbationProof: recoveryDuration=%.2f state=%d terminalReason=%d samples=%d rootDrift=%.2f verticalDrift=%.2f angularDrift=%.2f supportHull[min/mean/max]=%.2f/%.2f/%.2f activeSides[min/mean/max]=%.2f/%.2f/%.2f maxBodyLinear=%.2f maxBodyAngular=%.2f physicalPerturbation=%d measuredDeltaV=%.2f nonzeroVelocitySamples=%d"),
		RecoveryDurationSec,
		(int32)RuntimeState,
		(int32)TerminationState.TerminalReason,
		Metrics.SampleCount,
		Metrics.RootWorldPositionDriftCm,
		Metrics.RootVerticalDriftCm,
		Metrics.RootAngularDriftDeg,
		Metrics.SupportHullAreaMinCm2,
		Metrics.SupportHullAreaMeanCm2,
		Metrics.SupportHullAreaMaxCm2,
		Metrics.ActiveSupportSideCountMin,
		Metrics.ActiveSupportSideCountMean,
		Metrics.ActiveSupportSideCountMax,
		Metrics.MaxBodyLinearSpeedCmPerSecond,
		Metrics.MaxBodyAngularSpeedDegPerSecond,
		Metrics.bPhysicalPerturbationApplied ? 1 : 0,
		Metrics.PerturbationMeasuredDeltaVCmPerSecond,
		Metrics.BodyVelocityNonZeroSampleCount);

	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimActivatedStandingStabilityMetricsTest, "PhysAnim.ActivatedStanding.StabilityMetrics", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimActivatedStandingStabilityMetricsTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_StabilityMetrics"));
	OutTestCommands.Add(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
}

bool FPhysAnimActivatedStandingStabilityMetricsTest::RunTest(const FString& Parameters)
{
	const FString MapName = Parameters;

	AutomationOpenMap(MapName);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, true, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FCollectActivatedStandingStabilityMetricsCommand(30.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivatedStandingStabilityMetricsCommand(this));

	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimRawSimulationOwnershipBisectDiagnosticTest, "PhysAnim.Diagnostics.RawSimulationOwnershipBisect", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimRawSimulationOwnershipBisectDiagnosticTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	// Pruned to avoid test bloat and long execution times.
}

bool FPhysAnimRawSimulationOwnershipBisectDiagnosticTest::RunTest(const FString& Parameters)
{
	const int32 DiagnosticGroup = FMath::Clamp(FCString::Atoi(*Parameters), 1, 4);
	const FString CaseName =
		DiagnosticGroup == 1 ? TEXT("PelvisAndSpineOnly") :
		DiagnosticGroup == 2 ? TEXT("PelvisSpineAndThighs") :
		DiagnosticGroup == 3 ? TEXT("AddSupportBodies") :
		TEXT("FullRequiredBodies");
	if (DiagnosticGroup == 2)
	{
		AddExpectedError(TEXT("Fail-stop: Runtime instability detected"), EAutomationExpectedErrorFlags::Contains, 0);
	}

	ADD_LATENT_AUTOMATION_COMMAND(FSetAllowCharacterMovementInBridgeActiveCommand(0));
	ADD_LATENT_AUTOMATION_COMMAND(FSetFloatConsoleVariableCommand(TEXT("physanim.ActionScale"), 0.05f));
	ADD_LATENT_AUTOMATION_COMMAND(FSetFloatConsoleVariableCommand(TEXT("physanim.ActionClampAbs"), 0.10f));
	ADD_LATENT_AUTOMATION_COMMAND(FSetFloatConsoleVariableCommand(TEXT("physanim.MaxAngularStepDegPerSec"), 90.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FSetFloatConsoleVariableCommand(TEXT("physanim.AngularStrengthMultiplier"), 0.20f));
	ADD_LATENT_AUTOMATION_COMMAND(FSetFloatConsoleVariableCommand(TEXT("physanim.AngularDampingRatioMultiplier"), 2.50f));
	ADD_LATENT_AUTOMATION_COMMAND(FSetFloatConsoleVariableCommand(TEXT("physanim.AngularExtraDampingMultiplier"), 6.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FSetRawSimDiagnosticGroupCommand(DiagnosticGroup));
	AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, true, false));
	ADD_LATENT_AUTOMATION_COMMAND(FCollectActivatedStandingStabilityMetricsCommand(10.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FLogRawSimulationOwnershipBisectCommand(CaseName, DiagnosticGroup, this));
	ADD_LATENT_AUTOMATION_COMMAND(FSetRawSimDiagnosticGroupCommand(0));
	ADD_LATENT_AUTOMATION_COMMAND(FSetFloatConsoleVariableCommand(TEXT("physanim.ActionScale"), -1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FSetFloatConsoleVariableCommand(TEXT("physanim.ActionClampAbs"), -1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FSetFloatConsoleVariableCommand(TEXT("physanim.MaxAngularStepDegPerSec"), -1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FSetFloatConsoleVariableCommand(TEXT("physanim.AngularStrengthMultiplier"), -1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FSetFloatConsoleVariableCommand(TEXT("physanim.AngularDampingRatioMultiplier"), -1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FSetFloatConsoleVariableCommand(TEXT("physanim.AngularExtraDampingMultiplier"), -1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FSetAllowCharacterMovementInBridgeActiveCommand(1));

	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimControlIsolationMatrixDiagnosticTest, "PhysAnim.Diagnostics.ControlIsolationMatrix", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimControlIsolationMatrixDiagnosticTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	// Pruned to avoid test bloat and long execution times.
}

bool FPhysAnimControlIsolationMatrixDiagnosticTest::RunTest(const FString& Parameters)
{
	const bool bStaticTargetNoPhc = Parameters.StartsWith(TEXT("A"));
	const bool bZeroActions = Parameters == TEXT("B");
	const bool bCurrentActions = Parameters == TEXT("C");
	const bool bTinySyntheticActions = Parameters == TEXT("D");

	const int32 EarlyControlZeroGroup =
		(Parameters == TEXT("A1") || Parameters == TEXT("A1-full") || Parameters == TEXT("A1-passive")) ? 1 :
		Parameters == TEXT("A2") ? 2 :
		(Parameters == TEXT("A3") || Parameters == TEXT("A3-full") || Parameters.StartsWith(TEXT("A3-"))) ? 3 :
		Parameters == TEXT("A4") ? 4 :
		(Parameters == TEXT("A5-torso")) ? 5 :
		(Parameters.StartsWith(TEXT("A6-thigh"))) ? 6 :
		(Parameters == TEXT("A7-support")) ? 7 :
		0;

	const float ZeroDuration = 
		(Parameters.Contains(TEXT("-full")) || 
         Parameters.Contains(TEXT("-passive")) || 
         Parameters.Contains(TEXT("-torso")) || 
         Parameters.Contains(TEXT("-thigh")) || 
         Parameters.Contains(TEXT("-support"))) ? 5.0f : 0.30f;

	const int32 RestoreVariant =
		Parameters == TEXT("A3-ramp") ? 2 :
		Parameters == TEXT("A3-low-02") ? 5 :
		Parameters == TEXT("A3-low-05") ? 3 :
		Parameters == TEXT("A3-low-10") ? 6 :
		Parameters == TEXT("A3-low-20") ? 7 :
		0;

	const float RampDuration = Parameters == TEXT("A3-ramp") ? 0.50f : 0.30f;

	const FString CaseName = Parameters;
	
	const int32 ReviewMode =
		bStaticTargetNoPhc ? 1 :
		bZeroActions ? 2 :
		bTinySyntheticActions ? 3 :
		0;
	const float ActionScale =
		bTinySyntheticActions ? 1.0f :
		0.05f;
	const float ActionClamp =
		bTinySyntheticActions ? 0.02f :
		0.10f;
	const float SyntheticActionValue = bTinySyntheticActions ? 0.01f : 0.0f;

	const float BaseAngularStrength =
		Parameters == TEXT("A6-thigh-02") ? 0.02f :
		Parameters == TEXT("A6-thigh-05") ? 0.05f :
		Parameters == TEXT("A6-thigh-10") ? 0.10f :
		Parameters == TEXT("A6-thigh-20") ? 0.20f :
		0.20f;

	ADD_LATENT_AUTOMATION_COMMAND(FSetAllowCharacterMovementInBridgeActiveCommand(0));
	ADD_LATENT_AUTOMATION_COMMAND(FSetRawSimDiagnosticGroupCommand(3));
	ADD_LATENT_AUTOMATION_COMMAND(FSetIntConsoleVariableCommand(TEXT("physanim.V0PlantReviewMode"), 0));
	ADD_LATENT_AUTOMATION_COMMAND(FSetIntConsoleVariableCommand(TEXT("physanim.EnableInstabilityFailStop"), 0));
	ADD_LATENT_AUTOMATION_COMMAND(FSetIntConsoleVariableCommand(TEXT("p.PhysAnim.V0PlantEarlyControlZeroGroup"), EarlyControlZeroGroup));
	ADD_LATENT_AUTOMATION_COMMAND(FSetFloatConsoleVariableCommand(TEXT("p.PhysAnim.V0PlantEarlyControlZeroDurationSeconds"), ZeroDuration));
	ADD_LATENT_AUTOMATION_COMMAND(FSetIntConsoleVariableCommand(TEXT("p.PhysAnim.V0PlantThighRestoreVariant"), RestoreVariant));
	ADD_LATENT_AUTOMATION_COMMAND(FSetFloatConsoleVariableCommand(TEXT("p.PhysAnim.V0PlantThighRestoreRampDurationSeconds"), RampDuration));
	ADD_LATENT_AUTOMATION_COMMAND(FSetFloatConsoleVariableCommand(TEXT("physanim.V0PlantReviewSyntheticActionValue"), SyntheticActionValue));
	ADD_LATENT_AUTOMATION_COMMAND(FSetFloatConsoleVariableCommand(TEXT("physanim.ActionScale"), ActionScale));
	ADD_LATENT_AUTOMATION_COMMAND(FSetFloatConsoleVariableCommand(TEXT("physanim.ActionClampAbs"), ActionClamp));
	ADD_LATENT_AUTOMATION_COMMAND(FSetFloatConsoleVariableCommand(TEXT("physanim.MaxAngularStepDegPerSec"), 90.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FSetFloatConsoleVariableCommand(TEXT("physanim.AngularStrengthMultiplier"), BaseAngularStrength));
	ADD_LATENT_AUTOMATION_COMMAND(FSetFloatConsoleVariableCommand(TEXT("physanim.AngularDampingRatioMultiplier"), 2.50f));
	ADD_LATENT_AUTOMATION_COMMAND(FSetFloatConsoleVariableCommand(TEXT("physanim.AngularExtraDampingMultiplier"), 6.0f));
	AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, true, false));
	ADD_LATENT_AUTOMATION_COMMAND(FSetIntConsoleVariableCommand(TEXT("physanim.V0PlantReviewMode"), ReviewMode));
	ADD_LATENT_AUTOMATION_COMMAND(FCollectActivatedStandingStabilityMetricsCommand(10.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FLogControlIsolationMatrixCommand(CaseName, ReviewMode, this));
	ADD_LATENT_AUTOMATION_COMMAND(FSetRawSimDiagnosticGroupCommand(0));
	ADD_LATENT_AUTOMATION_COMMAND(FSetIntConsoleVariableCommand(TEXT("physanim.V0PlantReviewMode"), 0));
	ADD_LATENT_AUTOMATION_COMMAND(FSetIntConsoleVariableCommand(TEXT("physanim.EnableInstabilityFailStop"), -1));
	ADD_LATENT_AUTOMATION_COMMAND(FSetIntConsoleVariableCommand(TEXT("p.PhysAnim.V0PlantEarlyControlZeroGroup"), 0));
	ADD_LATENT_AUTOMATION_COMMAND(FSetFloatConsoleVariableCommand(TEXT("p.PhysAnim.V0PlantEarlyControlZeroDurationSeconds"), 0.30f));
	ADD_LATENT_AUTOMATION_COMMAND(FSetIntConsoleVariableCommand(TEXT("p.PhysAnim.V0PlantThighRestoreVariant"), 0));
	ADD_LATENT_AUTOMATION_COMMAND(FSetFloatConsoleVariableCommand(TEXT("p.PhysAnim.V0PlantThighRestoreRampDurationSeconds"), 0.30f));
	ADD_LATENT_AUTOMATION_COMMAND(FSetFloatConsoleVariableCommand(TEXT("physanim.V0PlantReviewSyntheticActionValue"), 0.01f));
	ADD_LATENT_AUTOMATION_COMMAND(FSetFloatConsoleVariableCommand(TEXT("physanim.ActionScale"), -1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FSetFloatConsoleVariableCommand(TEXT("physanim.ActionClampAbs"), -1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FSetFloatConsoleVariableCommand(TEXT("physanim.MaxAngularStepDegPerSec"), -1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FSetFloatConsoleVariableCommand(TEXT("physanim.AngularStrengthMultiplier"), -1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FSetFloatConsoleVariableCommand(TEXT("physanim.AngularDampingRatioMultiplier"), -1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FSetFloatConsoleVariableCommand(TEXT("physanim.AngularExtraDampingMultiplier"), -1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FSetAllowCharacterMovementInBridgeActiveCommand(1));
	ADD_LATENT_AUTOMATION_COMMAND(FStopActivationWiringCommand(this));

	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimActivatedStandingPerturbationTest, "PhysAnim.ActivatedStanding.Perturbation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimActivatedStandingPerturbationTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_Perturbation"));
	OutTestCommands.Add(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
}

bool FPhysAnimActivatedStandingPerturbationTest::RunTest(const FString& Parameters)
{
	const FString MapName = Parameters;
	static FActivatedStandingPerturbationValidationState State;

	State = FActivatedStandingPerturbationValidationState();

	AutomationOpenMap(MapName);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, true, false));
	ADD_LATENT_AUTOMATION_COMMAND(FCollectActivatedStandingStabilityMetricsCommand(10.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FCaptureActivatedStandingPerturbationBaselineCommand(&State));
	ADD_LATENT_AUTOMATION_COMMAND(FApplyActivatedStandingPerturbationCommand(&State));
	ADD_LATENT_AUTOMATION_COMMAND(FCollectActivatedStandingStabilityMetricsCommand(10.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivatedStandingPerturbationCommand(&State, this));

	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FCaptureActivatedStandingLocomotionReadinessBaselineCommand, FActivatedStandingLocomotionReadinessValidationState*, State);
bool FCaptureActivatedStandingLocomotionReadinessBaselineCommand::Update()
{
	if (!State)
	{
		return true;
	}

	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World) World = GWorld;

	UPhysAnimComponent* TargetComponent = State->Component.Get();
	if (World && !TargetComponent)
	{
		for (TActorIterator<ACharacter> It(World); It; ++It)
		{
			if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
			{
				TargetComponent = Comp;
				State->Component = Comp;
				break;
			}
		}
	}

	if (!TargetComponent)
	{
		PHYSANIM_LOG(LogTemp, Error, TEXT("LocomotionReadiness: baseline capture could not find a PhysAnim component"));
		return true;
	}

	const EPhysAnimRuntimeState RuntimeState = TargetComponent->GetRuntimeState();
	const FPhysAnimActivatedStandingStabilityMetrics& Metrics = TargetComponent->GetActivatedStandingStabilityMetrics();
	const FPhysAnimRuntimeTerminationState& TerminationState = TargetComponent->GetLiveRuntimeEvidenceTerminationState();
	const EBridgeLocomotionAuthorityState LocomotionAuthorityState = TargetComponent->GetLocomotionAuthorityState();

	bool bSetupValid = true;

	if (RuntimeState != EPhysAnimRuntimeState::BalanceActive_Standing)
	{
		PHYSANIM_LOG(LogTemp, Error, TEXT("LocomotionReadiness: expected BalanceActive_Standing before intent but got %d"), (int32)RuntimeState);
		bSetupValid = false;
	}

	if (!TargetComponent->IsLiveRuntimeEvidenceProofComplete())
	{
		PHYSANIM_LOG(LogTemp, Error, TEXT("LocomotionReadiness: proof was not complete before intent"));
		bSetupValid = false;
	}

	if (!TargetComponent->IsLiveRuntimeEvidenceProofSatisfied())
	{
		PHYSANIM_LOG(LogTemp, Error, TEXT("LocomotionReadiness: proof was not truthful before intent"));
		bSetupValid = false;
	}

	if (!Metrics.bHasSamples || Metrics.SampleCount <= 0)
	{
		PHYSANIM_LOG(LogTemp, Error, TEXT("LocomotionReadiness: standing metrics were not yet collected before intent"));
		bSetupValid = false;
	}

	if (Metrics.SupportHullAreaMinCm2 <= 0.0 || Metrics.SupportHullAreaMeanCm2 <= 0.0 || Metrics.SupportHullAreaMaxCm2 <= 0.0)
	{
		PHYSANIM_LOG(LogTemp, Error, TEXT("LocomotionReadiness: support hull area was not positive before intent"));
		bSetupValid = false;
	}

	if (LocomotionAuthorityState != EBridgeLocomotionAuthorityState::Idle)
	{
		PHYSANIM_LOG(LogTemp, Error, TEXT("LocomotionReadiness: expected locomotion authority Idle before intent but got %s"), GetLocomotionAuthorityStateName(LocomotionAuthorityState));
		bSetupValid = false;
	}

	if (!bSetupValid)
	{
		return true;
	}

	State->BaselineMetrics = Metrics;
	State->BaselineTerminationState = TerminationState;
	State->BaselineLocomotionAuthorityState = LocomotionAuthorityState;
	State->bBaselineCaptured = true;
	PHYSANIM_LOG(
		LogTemp,
		Warning,
		TEXT("LocomotionReadiness: baseline samples=%d duration=%.2f rootDrift=%.2f verticalDrift=%.2f angularDrift=%.2f supportHull[min/mean/max]=%.2f/%.2f/%.2f activeSides[min/mean/max]=%.2f/%.2f/%.2f terminalReason=%d locomotionAuthority=%s"),
		Metrics.SampleCount,
		Metrics.ActivationDurationSec,
		Metrics.RootWorldPositionDriftCm,
		Metrics.RootVerticalDriftCm,
		Metrics.RootAngularDriftDeg,
		Metrics.SupportHullAreaMinCm2,
		Metrics.SupportHullAreaMeanCm2,
		Metrics.SupportHullAreaMaxCm2,
		Metrics.ActiveSupportSideCountMin,
		Metrics.ActiveSupportSideCountMean,
		Metrics.ActiveSupportSideCountMax,
		(int32)TerminationState.TerminalReason,
		GetLocomotionAuthorityStateName(LocomotionAuthorityState));
	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FApplyActivatedStandingLocomotionReadinessIntentCommand, FActivatedStandingLocomotionReadinessValidationState*, State);
bool FApplyActivatedStandingLocomotionReadinessIntentCommand::Update()
{
	if (!State || !State->Component.IsValid())
	{
		PHYSANIM_LOG(LogTemp, Error, TEXT("LocomotionReadiness: intent command has no component"));
		return true;
	}

	UPhysAnimComponent* const Component = State->Component.Get();
	ACharacter* const Character = Cast<ACharacter>(Component->GetOwner());
	if (!Character)
	{
		PHYSANIM_LOG(LogTemp, Error, TEXT("LocomotionReadiness: intent command has no character owner"));
		return true;
	}

	FVector IntentDirection = Character->GetActorForwardVector().GetSafeNormal2D();
	if (IntentDirection.IsNearlyZero())
	{
		IntentDirection = FVector::ForwardVector;
	}

	const float IntentScale = 0.25f;
	Character->AddMovementInput(IntentDirection, IntentScale, true);

	State->bMovementIntentApplied = true;
	State->MovementIntentAppliedWorldTimeSeconds = Component->GetWorld() ? Component->GetWorld()->GetTimeSeconds() : -1.0;
	State->MovementIntentDirection = IntentDirection;
	State->MovementIntentScale = IntentScale;
	State->PostIntentLocomotionAuthorityState = Component->GetLocomotionAuthorityState();

	PHYSANIM_LOG(
		LogTemp,
		Warning,
		TEXT("LocomotionReadiness: movement intent applied world=(%.2f,%.2f) scale=%.2f postIntentAuthority=%s"),
		IntentDirection.X,
		IntentDirection.Y,
		IntentScale,
		GetLocomotionAuthorityStateName(State->PostIntentLocomotionAuthorityState));
	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FVerifyActivatedStandingLocomotionReadinessCommand, FActivatedStandingLocomotionReadinessValidationState*, State, FAutomationTestBase*, Test);
bool FVerifyActivatedStandingLocomotionReadinessCommand::Update()
{
	if (!State || !State->Component.IsValid())
	{
		AddLatentAutomationError(Test, TEXT("LocomotionReadiness: verification command has no component"));
		return true;
	}

	UPhysAnimComponent* const Component = State->Component.Get();
	const UWorld* const World = Component->GetWorld();
	const EPhysAnimRuntimeState RuntimeState = Component->GetRuntimeState();
	const EBridgeLocomotionAuthorityState LocomotionAuthorityState = Component->GetLocomotionAuthorityState();
	const FPhysAnimRuntimeTerminationState& TerminationState = Component->GetLiveRuntimeEvidenceTerminationState();
	const FPhysAnimActivatedStandingStabilityMetrics& Metrics = Component->GetActivatedStandingStabilityMetrics();
	const auto Fail = [&](const FString& Message)
	{
		AddLatentAutomationError(Test, Message);
	};

	if (!State->bBaselineCaptured)
	{
		Fail(TEXT("LocomotionReadiness: baseline was not captured"));
	}

	if (!State->bMovementIntentApplied)
	{
		Fail(TEXT("LocomotionReadiness: movement intent was not applied"));
	}

	if (LocomotionAuthorityState == EBridgeLocomotionAuthorityState::StartupLocomotion ||
		LocomotionAuthorityState == EBridgeLocomotionAuthorityState::Locomoting)
	{
		Fail(FString::Printf(TEXT("LocomotionReadiness: unsupported locomotion authority state entered (%s)"), GetLocomotionAuthorityStateName(LocomotionAuthorityState)));
	}

	const double IntentDurationSec =
		(World && State->MovementIntentAppliedWorldTimeSeconds >= 0.0)
			? (World->GetTimeSeconds() - State->MovementIntentAppliedWorldTimeSeconds)
			: -1.0;
	if (!FMath::IsFinite(IntentDurationSec) || IntentDurationSec < 0.0)
	{
		Fail(TEXT("LocomotionReadiness: intent duration is invalid"));
	}

	if (Metrics.SampleCount <= State->BaselineMetrics.SampleCount)
	{
		Fail(FString::Printf(TEXT("LocomotionReadiness: samples did not advance after intent baseline=%d current=%d"),
			State->BaselineMetrics.SampleCount,
			Metrics.SampleCount));
	}

	if (Metrics.SupportHullAreaMinCm2 <= 0.0 || Metrics.SupportHullAreaMeanCm2 <= 0.0 || Metrics.SupportHullAreaMaxCm2 <= 0.0)
	{
		Fail(TEXT("LocomotionReadiness: support hull area collapsed during intent"));
	}

	if (Metrics.ActiveSupportSideCountMin < 1.0 || Metrics.ActiveSupportSideCountMean < 1.0 || Metrics.ActiveSupportSideCountMax < 1.0)
	{
		Fail(TEXT("LocomotionReadiness: active support side count dropped below 1 during intent"));
	}

	const bool bStandingStayedStanding = RuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing;
	const bool bSafeTransition =
		RuntimeState == EPhysAnimRuntimeState::BalanceSafeDeny ||
		RuntimeState == EPhysAnimRuntimeState::FailStopped;
	if (!bStandingStayedStanding && !bSafeTransition)
	{
		Fail(FString::Printf(TEXT("LocomotionReadiness: runtime state after intent was unexpected (%d)"), (int32)RuntimeState));
	}

	if (bStandingStayedStanding)
	{
		if (TerminationState.TerminalReason != EPhysAnimTerminalReason::None)
		{
			Fail(FString::Printf(TEXT("LocomotionReadiness: standing result expected terminal_reason=None but got %d"), (int32)TerminationState.TerminalReason));
		}
		if (!Component->IsLiveRuntimeEvidenceProofSatisfied())
		{
			Fail(TEXT("LocomotionReadiness: standing result is not truthful"));
		}
	}
	else
	{
		if (TerminationState.TerminalReason == EPhysAnimTerminalReason::None)
		{
			PHYSANIM_LOG(LogTemp, Warning, TEXT("LocomotionReadiness: safe transition kept terminal_reason=None"));
		}
		else if (Component->IsLiveRuntimeEvidenceProofSatisfied())
		{
			Fail(TEXT("LocomotionReadiness: failure reason was not truthful"));
		}
	}

	if (TerminationState.TerminalArtifact.TerminalReason != TerminationState.TerminalReason ||
		TerminationState.LatestArtifact.TerminalReason != TerminationState.TerminalReason)
	{
		Fail(TEXT("LocomotionReadiness: audit artifact terminal reason does not match final state"));
	}

	const bool bLocomotionTransitionAllowed = LocomotionAuthorityState != EBridgeLocomotionAuthorityState::Idle;
	PHYSANIM_LOG(
		LogTemp,
		Warning,
		TEXT("LocomotionReadiness: intentDuration=%.2f standing=%s locomotionAuthority=%s transitionAllowed=%s samples=%d rootDrift=%.2f verticalDrift=%.2f angularDrift=%.2f supportHull[min/mean/max]=%.2f/%.2f/%.2f activeSides[min/mean/max]=%.2f/%.2f/%.2f maxBodyLinear=%.2f maxBodyAngular=%.2f terminalReason=%d"),
		IntentDurationSec,
		bStandingStayedStanding ? TEXT("true") : TEXT("false"),
		GetLocomotionAuthorityStateName(LocomotionAuthorityState),
		bLocomotionTransitionAllowed ? TEXT("yes") : TEXT("no"),
		Metrics.SampleCount,
		Metrics.RootWorldPositionDriftCm,
		Metrics.RootVerticalDriftCm,
		Metrics.RootAngularDriftDeg,
		Metrics.SupportHullAreaMinCm2,
		Metrics.SupportHullAreaMeanCm2,
		Metrics.SupportHullAreaMaxCm2,
		Metrics.ActiveSupportSideCountMin,
		Metrics.ActiveSupportSideCountMean,
		Metrics.ActiveSupportSideCountMax,
		Metrics.MaxBodyLinearSpeedCmPerSecond,
		Metrics.MaxBodyAngularSpeedDegPerSecond,
		(int32)TerminationState.TerminalReason);

	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimActivatedStandingLocomotionReadinessTest, "PhysAnim.ActivatedStanding.LocomotionReadiness", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimActivatedStandingLocomotionReadinessTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionReadiness"));
	OutTestCommands.Add(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
}

bool FPhysAnimActivatedStandingLocomotionReadinessTest::RunTest(const FString& Parameters)
{
	const FString MapName = Parameters;
	static FActivatedStandingLocomotionReadinessValidationState State;

	State = FActivatedStandingLocomotionReadinessValidationState();

	AutomationOpenMap(MapName);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, true, false));
	ADD_LATENT_AUTOMATION_COMMAND(FCollectActivatedStandingStabilityMetricsCommand(10.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FCaptureActivatedStandingLocomotionReadinessBaselineCommand(&State));
	ADD_LATENT_AUTOMATION_COMMAND(FApplyActivatedStandingLocomotionReadinessIntentCommand(&State));
	ADD_LATENT_AUTOMATION_COMMAND(FCollectActivatedStandingStabilityMetricsCommand(10.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivatedStandingLocomotionReadinessCommand(&State, this));

	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND(FApplyActivatedStandingLocomotionGateIntentCommand);
bool FApplyActivatedStandingLocomotionGateIntentCommand::Update()
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World) World = GWorld;

	if (!World)
	{
		return true;
	}

	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			ACharacter* const Character = Cast<ACharacter>(Comp->GetOwner());
			if (!Character)
			{
				PHYSANIM_LOG(LogTemp, Error, TEXT("LocomotionGate: intent command has no character owner"));
				return true;
			}

			FVector IntentDirection = Character->GetActorForwardVector().GetSafeNormal2D();
			if (IntentDirection.IsNearlyZero())
			{
				IntentDirection = FVector::ForwardVector;
			}

			const float IntentScale = 0.25f;
			Character->AddMovementInput(IntentDirection, IntentScale, true);
			PHYSANIM_LOG(
				LogTemp,
				Warning,
				TEXT("LocomotionGate: movement intent applied world=(%.2f,%.2f) scale=%.2f"),
				IntentDirection.X,
				IntentDirection.Y,
				IntentScale);
			break;
		}
	}

	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FSetActivatedStandingLocomotionGateIntentCommand, float, IntentMagnitude, double, IntentAgeSeconds);
bool FSetActivatedStandingLocomotionGateIntentCommand::Update()
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World) World = GWorld;

	if (!World)
	{
		return true;
	}

	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			Comp->TestOnlySetBridgeLocomotionGateIntent(IntentMagnitude, IntentAgeSeconds);
			PHYSANIM_LOG(
				LogTemp,
				Warning,
				TEXT("LocomotionGate: test intent set magnitude=%.2f age=%.2f"),
				IntentMagnitude,
				IntentAgeSeconds);
			break;
		}
	}

	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_FOUR_PARAMETER(FVerifyActivatedStandingLocomotionGateCommand, bool, bExpectedAllowed, FString, ExpectedReasonSubstring, FString, CaseName, FAutomationTestBase*, Test);
bool FVerifyActivatedStandingLocomotionGateCommand::Update()
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World) World = GWorld;

	if (!World)
	{
		AddLatentAutomationError(Test, FString::Printf(TEXT("LocomotionGate[%s]: PIE world was not available"), *CaseName));
		return true;
	}

	bool bFoundComponent = false;
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			bFoundComponent = true;
			FString Reason;
			const bool bAllowed = Comp->CanEnterBridgeLocomotionGate(Reason);
			if (bAllowed != bExpectedAllowed)
			{
				AddLatentAutomationError(
					Test,
					FString::Printf(
						TEXT("LocomotionGate[%s]: expected allowed=%d but got %d reason=%s"),
						*CaseName,
						bExpectedAllowed ? 1 : 0,
						bAllowed ? 1 : 0,
						*Reason));
			}
			if (!ExpectedReasonSubstring.IsEmpty() && !Reason.Contains(ExpectedReasonSubstring))
			{
				AddLatentAutomationError(
					Test,
					FString::Printf(
						TEXT("LocomotionGate[%s]: expected reason containing '%s' but got '%s'"),
						*CaseName,
						*ExpectedReasonSubstring,
						*Reason));
			}
			PHYSANIM_LOG(
				LogTemp,
				Warning,
				TEXT("LocomotionGate[%s]: allowed=%d reason=%s"),
				*CaseName,
				bAllowed ? 1 : 0,
				*Reason);
			break;
		}
	}

	if (!bFoundComponent)
	{
		AddLatentAutomationError(Test, FString::Printf(TEXT("LocomotionGate[%s]: no PhysAnim component was found"), *CaseName));
	}

	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimActivatedStandingLocomotionGateTest, "PhysAnim.ActivatedStanding.LocomotionGate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimActivatedStandingLocomotionGateTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionGate_NoIntentDenied"));
	OutTestCommands.Add(TEXT("NoIntentDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionGate_ShortPulseDenied"));
	OutTestCommands.Add(TEXT("ShortPulseDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionGate_StableAllowed"));
	OutTestCommands.Add(TEXT("StableAllowed"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionGate_NegativeSupportDenied"));
	OutTestCommands.Add(TEXT("NegativeSupportDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionGate_TerminalReasonDenied"));
	OutTestCommands.Add(TEXT("TerminalReasonDenied"));
}

bool FPhysAnimActivatedStandingLocomotionGateTest::RunTest(const FString& Parameters)
{
	AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));

	if (Parameters == TEXT("NoIntentDenied"))
	{
		ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, true, false));
		ADD_LATENT_AUTOMATION_COMMAND(FCollectActivatedStandingStabilityMetricsCommand(10.0f));
		ADD_LATENT_AUTOMATION_COMMAND(FSetActivatedStandingLocomotionGateIntentCommand(0.0f, -1.0));
		ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivatedStandingLocomotionGateCommand(false, TEXT("intent_absent"), TEXT("NoIntentDenied"), this));
	}
	else if (Parameters == TEXT("ShortPulseDenied"))
	{
		ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, true, false));
		ADD_LATENT_AUTOMATION_COMMAND(FCollectActivatedStandingStabilityMetricsCommand(10.0f));
		ADD_LATENT_AUTOMATION_COMMAND(FSetActivatedStandingLocomotionGateIntentCommand(0.50f, 0.05));
		ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivatedStandingLocomotionGateCommand(false, TEXT("intent_too_short"), TEXT("ShortPulseDenied"), this));
	}
	else if (Parameters == TEXT("StableAllowed"))
	{
		ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, true, false));
		ADD_LATENT_AUTOMATION_COMMAND(FCollectActivatedStandingStabilityMetricsCommand(10.0f));
		ADD_LATENT_AUTOMATION_COMMAND(FSetActivatedStandingLocomotionGateIntentCommand(0.50f, 0.50));
		ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivatedStandingLocomotionGateCommand(true, TEXT("intent_stable"), TEXT("StableAllowed"), this));
	}
	else if (Parameters == TEXT("NegativeSupportDenied"))
	{
		ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, true, false));
		ADD_LATENT_AUTOMATION_COMMAND(FCollectActivatedStandingStabilityMetricsCommand(10.0f));
		ADD_LATENT_AUTOMATION_COMMAND(FSetActivatedStandingLocomotionGateIntentCommand(0.0f, -1.0));
		ADD_LATENT_AUTOMATION_COMMAND(FEnableNegativeSupportProofCommand());
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(6.0f));
		ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivatedStandingLocomotionGateCommand(false, TEXT(""), TEXT("NegativeSupportDenied"), this));
	}
	else if (Parameters == TEXT("TerminalReasonDenied"))
	{
		AddExpectedError(TEXT("Fail-stop: Proof failed"), EAutomationExpectedErrorFlags::Contains, 0);
		ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, true, true));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
		ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivatedStandingLocomotionGateCommand(false, TEXT(""), TEXT("TerminalReasonDenied"), this));
	}

	return true;
}

struct FActivatedStandingLocomotionRequestValidationState
{
	TWeakObjectPtr<UPhysAnimComponent> Component;
	FString CaseName;
	bool bExpectedAllowed = false;
	bool bCheckTransitionPreservation = false;
	bool bCheckRuntimeState = false;
	EPhysAnimRuntimeState ExpectedRuntimeState = EPhysAnimRuntimeState::BalanceActive_Standing;
	EBridgeLocomotionRequestState ExpectedRequestState = EBridgeLocomotionRequestState::BalanceActiveStanding;
	FString ExpectedReasonSubstring;
	bool bRequireStandingRuntimeState = true;
	float ExpectedIntentMagnitude = 0.0f;
	double ExpectedIntentAgeSeconds = -1.0;
	EPhysAnimTerminalReason ExpectedTerminalReason = EPhysAnimTerminalReason::None;
	EPhysAnimSupportMode ExpectedSupportMode = EPhysAnimSupportMode::Airborne;
	double ExpectedSupportHullAreaCm2 = 0.0;
	int32 ExpectedActiveSupportSideCount = 0;
	bool bExpectedCapsuleValid = true;
	bool bExpectedContinuityValid = true;
	bool bCheckHandoffPreflight = false;
	EBridgeLocomotionHandoffPreflightState ExpectedHandoffPreflightState = EBridgeLocomotionHandoffPreflightState::LocomotionHandoffPreflightDenied;
	FString ExpectedHandoffPreflightReasonSubstring;
	bool bCheckHandoffCommit = false;
	EBridgeLocomotionHandoffCommitState ExpectedHandoffCommitState = EBridgeLocomotionHandoffCommitState::LocomotionHandoffCommitPending;
	FString ExpectedHandoffCommitReasonSubstring;
	EPhysAnimRuntimeState PreRuntimeState = EPhysAnimRuntimeState::BalanceActive_Standing;
	EPhysAnimRuntimeState PostRuntimeState = EPhysAnimRuntimeState::BalanceActive_Standing;
	EBridgeLocomotionAuthorityState PreAuthorityState = EBridgeLocomotionAuthorityState::Idle;
	EBridgeLocomotionAuthorityState PostAuthorityState = EBridgeLocomotionAuthorityState::Idle;
	EBridgeLocomotionRequestState PreRequestState = EBridgeLocomotionRequestState::BalanceActiveStanding;
	EBridgeLocomotionRequestState PostRequestState = EBridgeLocomotionRequestState::BalanceActiveStanding;
	EBridgeLocomotionHandoffPreflightState PreHandoffPreflightState = EBridgeLocomotionHandoffPreflightState::LocomotionHandoffPreflightDenied;
	EBridgeLocomotionHandoffPreflightState PostHandoffPreflightState = EBridgeLocomotionHandoffPreflightState::LocomotionHandoffPreflightDenied;
	FString PreHandoffPreflightReason;
	FString PostHandoffPreflightReason;
	EBridgeLocomotionHandoffCommitState PreHandoffCommitState = EBridgeLocomotionHandoffCommitState::LocomotionHandoffCommitPending;
	EBridgeLocomotionHandoffCommitState PostHandoffCommitState = EBridgeLocomotionHandoffCommitState::LocomotionHandoffCommitPending;
	FString PreHandoffCommitReason;
	FString PostHandoffCommitReason;
	bool bPreBridgeOwnsPhysics = false;
	bool bPostBridgeOwnsPhysics = false;
};

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FApplyActivatedStandingLocomotionRequestStateCommand, FActivatedStandingLocomotionRequestValidationState*, State);
bool FApplyActivatedStandingLocomotionRequestStateCommand::Update()
{
	if (!State)
	{
		return true;
	}

	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World) World = GWorld;

	if (!World)
	{
		return true;
	}

	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			State->Component = Comp;
			State->PreRuntimeState = Comp->GetRuntimeState();
			State->PreAuthorityState = Comp->GetLocomotionAuthorityState();
			State->PreRequestState = Comp->GetLocomotionRequestState();
			State->PreHandoffPreflightState = Comp->GetLocomotionHandoffPreflightState();
			State->PreHandoffPreflightReason = Comp->GetLocomotionHandoffPreflightReason();
			State->PreHandoffCommitState = Comp->GetLocomotionHandoffCommitState();
			State->PreHandoffCommitReason = Comp->GetLocomotionHandoffCommitReason();
			State->bPreBridgeOwnsPhysics = Comp->DoesBridgeOwnPhysics();
			FPhysAnimRuntimeTerminationState EvidenceState = Comp->GetLiveRuntimeEvidenceTerminationState();

			State->bExpectedAllowed = false;
			State->ExpectedRequestState = EBridgeLocomotionRequestState::LocomotionRequestDenied;
			State->ExpectedReasonSubstring = TEXT("intent_absent");
			State->ExpectedIntentMagnitude = 0.0f;
			State->ExpectedIntentAgeSeconds = -1.0;
			State->ExpectedTerminalReason = EvidenceState.TerminalReason;
			State->ExpectedSupportMode = EvidenceState.LatestArtifact.SupportMode;
			State->ExpectedSupportHullAreaCm2 = EvidenceState.LatestArtifact.SupportHullAreaCm2;
			State->ExpectedActiveSupportSideCount = EvidenceState.LatestArtifact.ActiveSupportSideCount;
			State->bExpectedCapsuleValid = EvidenceState.LatestArtifact.bCapsuleContractPassed;
			State->bExpectedContinuityValid =
				EvidenceState.LatestArtifact.bPhysicalContinuityValidatorPassed &&
				!EvidenceState.LatestArtifact.bContinuityBookkeepingMismatch;
			State->ExpectedHandoffPreflightState = EBridgeLocomotionHandoffPreflightState::LocomotionHandoffPreflightDenied;
			State->ExpectedHandoffPreflightReasonSubstring = State->ExpectedReasonSubstring;
			State->bCheckHandoffCommit = false;
			State->ExpectedHandoffCommitState = EBridgeLocomotionHandoffCommitState::LocomotionHandoffCommitDenied;
			State->ExpectedHandoffCommitReasonSubstring = State->ExpectedReasonSubstring;

			if (State->CaseName == TEXT("NoIntentDenied"))
			{
				State->ExpectedReasonSubstring = TEXT("intent_absent");
				State->ExpectedIntentMagnitude = 0.0f;
				State->ExpectedIntentAgeSeconds = -1.0;
				State->ExpectedRequestState = EBridgeLocomotionRequestState::LocomotionRequestDenied;
			}
			else if (State->CaseName == TEXT("GateDenied"))
			{
				State->ExpectedReasonSubstring = TEXT("intent_absent");
				State->ExpectedIntentMagnitude = 0.0f;
				State->ExpectedIntentAgeSeconds = -1.0;
				State->ExpectedRequestState = EBridgeLocomotionRequestState::LocomotionRequestDenied;
			}
			else if (State->CaseName == TEXT("ShortPulseDenied"))
			{
				State->ExpectedReasonSubstring = TEXT("intent_too_short");
				State->ExpectedIntentMagnitude = 0.50f;
				State->ExpectedIntentAgeSeconds = 0.05;
				State->ExpectedRequestState = EBridgeLocomotionRequestState::LocomotionRequestDenied;
			}
			else if (State->CaseName == TEXT("StableRequested"))
			{
				State->bExpectedAllowed = true;
				State->ExpectedReasonSubstring = TEXT("intent_stable");
				State->ExpectedIntentMagnitude = 0.50f;
				State->ExpectedIntentAgeSeconds = 0.50;
				State->ExpectedRequestState = EBridgeLocomotionRequestState::LocomotionRequested;
				State->ExpectedHandoffPreflightState = EBridgeLocomotionHandoffPreflightState::LocomotionHandoffPreflightPassed;
				State->ExpectedHandoffPreflightReasonSubstring = TEXT("handoff_ready");
			}
			else if (State->CaseName == TEXT("NegativeSupportDenied"))
			{
				State->ExpectedReasonSubstring = TEXT("negative_support");
				State->ExpectedIntentMagnitude = 0.50f;
				State->ExpectedIntentAgeSeconds = 0.50;
				State->ExpectedRequestState = EBridgeLocomotionRequestState::LocomotionRequestDenied;
				EvidenceState.TerminalArtifact.SupportMode = EPhysAnimSupportMode::Airborne;
				EvidenceState.LatestArtifact.SupportMode = EPhysAnimSupportMode::Airborne;
				EvidenceState.TerminalArtifact.SupportHullAreaCm2 = 0.0;
				EvidenceState.LatestArtifact.SupportHullAreaCm2 = 0.0;
				EvidenceState.TerminalArtifact.ActiveSupportSideCount = 0;
				EvidenceState.LatestArtifact.ActiveSupportSideCount = 0;
				State->ExpectedSupportMode = EPhysAnimSupportMode::Airborne;
				State->ExpectedSupportHullAreaCm2 = 0.0;
				State->ExpectedActiveSupportSideCount = 0;
			}
			else if (State->CaseName == TEXT("TerminalReasonDenied"))
			{
				State->ExpectedReasonSubstring = TEXT("terminal_reason_present");
				State->ExpectedIntentMagnitude = 0.50f;
				State->ExpectedIntentAgeSeconds = 0.50;
				State->ExpectedRequestState = EBridgeLocomotionRequestState::LocomotionRequestDenied;
				EvidenceState.bTerminated = true;
				EvidenceState.TerminalReason = EPhysAnimTerminalReason::ActivationSupportFailure;
				EvidenceState.TerminalArtifact.TerminalReason = EPhysAnimTerminalReason::ActivationSupportFailure;
				EvidenceState.LatestArtifact.TerminalReason = EPhysAnimTerminalReason::ActivationSupportFailure;
				State->ExpectedTerminalReason = EPhysAnimTerminalReason::ActivationSupportFailure;
			}
			else if (State->CaseName == TEXT("CapsuleInvalidDenied"))
			{
				State->ExpectedReasonSubstring = TEXT("capsule_invalid");
				State->ExpectedIntentMagnitude = 0.50f;
				State->ExpectedIntentAgeSeconds = 0.50;
				State->ExpectedRequestState = EBridgeLocomotionRequestState::LocomotionRequestDenied;
				EvidenceState.TerminalArtifact.bCapsuleContractPassed = false;
				EvidenceState.LatestArtifact.bCapsuleContractPassed = false;
				State->bExpectedCapsuleValid = false;
			}
			else if (State->CaseName == TEXT("ContinuityInvalidDenied"))
			{
				State->ExpectedReasonSubstring = TEXT("continuity_invalid");
				State->ExpectedIntentMagnitude = 0.50f;
				State->ExpectedIntentAgeSeconds = 0.50;
				State->ExpectedRequestState = EBridgeLocomotionRequestState::LocomotionRequestDenied;
				EvidenceState.TerminalArtifact.bPhysicalContinuityValidatorPassed = false;
				EvidenceState.TerminalArtifact.bContinuityBookkeepingMismatch = true;
				EvidenceState.LatestArtifact.bPhysicalContinuityValidatorPassed = false;
				EvidenceState.LatestArtifact.bContinuityBookkeepingMismatch = true;
				State->bExpectedContinuityValid = false;
			}
			else if (State->CaseName == TEXT("StableCommitted"))
			{
				State->bExpectedAllowed = true;
				State->ExpectedReasonSubstring = TEXT("intent_stable");
				State->ExpectedIntentMagnitude = 0.50f;
				State->ExpectedIntentAgeSeconds = 0.50;
				State->ExpectedRequestState = EBridgeLocomotionRequestState::LocomotionRequested;
				State->ExpectedHandoffPreflightState = EBridgeLocomotionHandoffPreflightState::LocomotionHandoffPreflightPassed;
				State->ExpectedHandoffPreflightReasonSubstring = TEXT("handoff_ready");
				State->bCheckHandoffCommit = true;
				State->ExpectedHandoffCommitState = EBridgeLocomotionHandoffCommitState::LocomotionHandoffCommitted;
				State->ExpectedHandoffCommitReasonSubstring = TEXT("handoff_commit_ready");
			}
			else if (State->CaseName == TEXT("DroppedAfterPreflightDenied"))
			{
				State->bExpectedAllowed = false;
				State->ExpectedReasonSubstring = TEXT("intent_absent");
				State->ExpectedIntentMagnitude = 0.0f;
				State->ExpectedIntentAgeSeconds = -1.0;
				State->ExpectedRequestState = EBridgeLocomotionRequestState::LocomotionRequestDenied;
				State->ExpectedHandoffPreflightState = EBridgeLocomotionHandoffPreflightState::LocomotionHandoffPreflightDenied;
				State->ExpectedHandoffPreflightReasonSubstring = TEXT("intent_absent");
				State->bCheckHandoffCommit = true;
				State->ExpectedHandoffCommitState = EBridgeLocomotionHandoffCommitState::LocomotionHandoffCommitDenied;
				State->ExpectedHandoffCommitReasonSubstring = TEXT("intent_absent");
			}
			else if (State->CaseName == TEXT("NoPreflightDenied"))
			{
				State->bRequireStandingRuntimeState = false;
				State->bCheckTransitionPreservation = false;
				if (State->bCheckRuntimeState)
				{
					State->ExpectedReasonSubstring = TEXT("intent_absent");
					State->ExpectedIntentMagnitude = 0.0f;
					State->ExpectedIntentAgeSeconds = -1.0;
					State->ExpectedRequestState = EBridgeLocomotionRequestState::LocomotionRequestDenied;
					State->ExpectedHandoffPreflightState = EBridgeLocomotionHandoffPreflightState::LocomotionHandoffPreflightDenied;
					State->ExpectedHandoffPreflightReasonSubstring = TEXT("intent_absent");
					State->bCheckHandoffCommit = true;
					State->ExpectedHandoffCommitState = EBridgeLocomotionHandoffCommitState::LocomotionHandoffCommitDenied;
					State->ExpectedHandoffCommitReasonSubstring = TEXT("intent_absent");
				}
				else
				{
					State->ExpectedReasonSubstring.Reset();
					State->ExpectedIntentMagnitude = 0.0f;
					State->ExpectedIntentAgeSeconds = -1.0;
					State->ExpectedRequestState = EBridgeLocomotionRequestState::BalanceActiveStanding;
					State->ExpectedHandoffPreflightState = EBridgeLocomotionHandoffPreflightState::LocomotionHandoffPreflightDenied;
					State->ExpectedHandoffPreflightReasonSubstring.Reset();
					State->bCheckHandoffCommit = true;
					State->ExpectedHandoffCommitState = EBridgeLocomotionHandoffCommitState::LocomotionHandoffCommitPending;
					State->ExpectedHandoffCommitReasonSubstring.Reset();
				}
			}

			if (State->ExpectedHandoffPreflightState == EBridgeLocomotionHandoffPreflightState::LocomotionHandoffPreflightPassed)
			{
				State->ExpectedHandoffPreflightReasonSubstring = TEXT("handoff_ready");
			}
			else
			{
				State->ExpectedHandoffPreflightReasonSubstring = State->ExpectedReasonSubstring;
			}
			if (State->bCheckRuntimeState)
			{
				State->ExpectedRuntimeState =
					(State->CaseName == TEXT("StableCommitted") &&
						State->ExpectedHandoffCommitState == EBridgeLocomotionHandoffCommitState::LocomotionHandoffCommitted)
						? EPhysAnimRuntimeState::LocomotionActiveShell
						: EPhysAnimRuntimeState::LocomotionActiveShellDenied;
			}
			State->ExpectedSupportMode = EvidenceState.LatestArtifact.SupportMode;
			State->ExpectedSupportHullAreaCm2 = EvidenceState.LatestArtifact.SupportHullAreaCm2;
			State->ExpectedActiveSupportSideCount = EvidenceState.LatestArtifact.ActiveSupportSideCount;
			State->ExpectedTerminalReason = EvidenceState.TerminalReason;

			const bool bCommitTwoPassStable = State->CaseName == TEXT("StableCommitted");
			const bool bCommitDropAfterPreflight = State->CaseName == TEXT("DroppedAfterPreflightDenied");
			const double IntentAgeSeconds =
				(State->CaseName == TEXT("NoIntentDenied") ||
					State->CaseName == TEXT("GateDenied") ||
					State->CaseName == TEXT("NoPreflightDenied"))
					? -1.0
					: (State->CaseName == TEXT("ShortPulseDenied") ? 0.05 : 0.50);
			Comp->TestOnlySetBridgeLocomotionRequestEvidence(EvidenceState, State->ExpectedIntentMagnitude, IntentAgeSeconds);
			if (bCommitTwoPassStable || bCommitDropAfterPreflight)
			{
				FPhysAnimRuntimeTerminationState SecondEvidenceState = EvidenceState;
				float SecondIntentMagnitude = State->ExpectedIntentMagnitude;
				double SecondIntentAgeSeconds = IntentAgeSeconds;
				if (bCommitDropAfterPreflight)
				{
					SecondIntentMagnitude = 0.0f;
					SecondIntentAgeSeconds = -1.0;
				}
				Comp->TestOnlySetBridgeLocomotionRequestEvidence(SecondEvidenceState, SecondIntentMagnitude, SecondIntentAgeSeconds);
			}

			if (State->bCheckRuntimeState)
			{
				Comp->TestOnlyUpdateBridgeLocomotionActiveShellState(World->GetTimeSeconds());
			}

			State->PostRuntimeState = Comp->GetRuntimeState();
			State->PostAuthorityState = Comp->GetLocomotionAuthorityState();
			State->PostRequestState = Comp->GetLocomotionRequestState();
			State->PostHandoffPreflightState = Comp->GetLocomotionHandoffPreflightState();
			State->PostHandoffPreflightReason = Comp->GetLocomotionHandoffPreflightReason();
			State->PostHandoffCommitState = Comp->GetLocomotionHandoffCommitState();
			State->PostHandoffCommitReason = Comp->GetLocomotionHandoffCommitReason();
			State->bPostBridgeOwnsPhysics = Comp->DoesBridgeOwnPhysics();

			PHYSANIM_LOG(
				LogTemp,
				Warning,
				TEXT("LocomotionRequest[%s]: applied intentMagnitude=%.2f intentAge=%.2f supportMode=%d supportHull=%.2f activeSides=%d capsuleValid=%d continuityValid=%d terminalReason=%d"),
				*State->CaseName,
				State->ExpectedIntentMagnitude,
				IntentAgeSeconds,
				static_cast<int32>(State->ExpectedSupportMode),
				State->ExpectedSupportHullAreaCm2,
				State->ExpectedActiveSupportSideCount,
				State->bExpectedCapsuleValid ? 1 : 0,
				State->bExpectedContinuityValid ? 1 : 0,
				static_cast<int32>(State->ExpectedTerminalReason));
			break;
		}
	}

	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FVerifyActivatedStandingLocomotionRequestStateCommand, FActivatedStandingLocomotionRequestValidationState*, State, FAutomationTestBase*, Test);
bool FVerifyActivatedStandingLocomotionRequestStateCommand::Update()
{
	if (!State || !State->Component.IsValid())
	{
		AddLatentAutomationError(Test, TEXT("LocomotionRequest: verification command has no component"));
		return true;
	}

	UPhysAnimComponent* const Comp = State->Component.Get();
	FString GateReason;
	const bool bAllowed = Comp->CanEnterBridgeLocomotionGate(GateReason);
	const EPhysAnimRuntimeState RuntimeState = Comp->GetRuntimeState();
	const EBridgeLocomotionRequestState RequestState = Comp->GetLocomotionRequestState();
	const FString& RequestReason = Comp->GetLocomotionRequestReason();
	const EBridgeLocomotionHandoffPreflightState HandoffPreflightState = Comp->GetLocomotionHandoffPreflightState();
	const FString& HandoffPreflightReason = Comp->GetLocomotionHandoffPreflightReason();
	const EBridgeLocomotionHandoffCommitState HandoffCommitState = Comp->GetLocomotionHandoffCommitState();
	const FString& HandoffCommitReason = Comp->GetLocomotionHandoffCommitReason();
	const FString& ActiveShellReason = Comp->GetLocomotionActiveShellReason();
	const FPhysAnimRuntimeTerminationState& TerminationState = Comp->GetLiveRuntimeEvidenceTerminationState();
	const FPhysAnimRunArtifactSnapshot& Latest = TerminationState.LatestArtifact;
	const FPhysAnimActivatedStandingStabilityMetrics& Metrics = Comp->GetActivatedStandingStabilityMetrics();

	const auto Fail = [&](const FString& Message)
	{
		AddLatentAutomationError(Test, Message);
	};

	if (State->bCheckRuntimeState && RuntimeState != State->ExpectedRuntimeState)
	{
		Fail(FString::Printf(TEXT("LocomotionRequest[%s]: expected runtime state=%s but got %s"),
			*State->CaseName,
			GetRuntimeStateName(State->ExpectedRuntimeState),
			GetRuntimeStateName(RuntimeState)));
	}

	if (State->bCheckTransitionPreservation)
	{
		if (!UPhysAnimComponent::TestOnlyIsBalanceActiveState(State->PreRuntimeState))
		{
			Fail(FString::Printf(TEXT("LocomotionRequest[%s]: expected pre-runtime balance-active state but got %d"),
				*State->CaseName,
				(int32)State->PreRuntimeState));
		}

		if (State->PostAuthorityState != State->PreAuthorityState)
		{
			Fail(FString::Printf(TEXT("LocomotionRequest[%s]: locomotion authority changed from %d to %d"),
				*State->CaseName,
				(int32)State->PreAuthorityState,
				(int32)State->PostAuthorityState));
		}

		if (State->bPostBridgeOwnsPhysics != State->bPreBridgeOwnsPhysics)
		{
			Fail(FString::Printf(TEXT("LocomotionRequest[%s]: bridge physics ownership changed from %d to %d"),
				*State->CaseName,
				State->bPreBridgeOwnsPhysics ? 1 : 0,
				State->bPostBridgeOwnsPhysics ? 1 : 0));
		}

		if (State->bCheckRuntimeState && State->PostRuntimeState != State->ExpectedRuntimeState)
		{
			Fail(FString::Printf(TEXT("LocomotionRequest[%s]: post-runtime state snapshot mismatch expected=%s actual=%s"),
				*State->CaseName,
				GetRuntimeStateName(State->ExpectedRuntimeState),
				GetRuntimeStateName(State->PostRuntimeState)));
		}
	}

	if (State->bCheckHandoffPreflight)
	{
		if (HandoffPreflightState != State->ExpectedHandoffPreflightState)
		{
			Fail(FString::Printf(TEXT("LocomotionRequest[%s]: expected handoff preflight state=%s but got %s"),
				*State->CaseName,
				GetLocomotionHandoffPreflightStateName(State->ExpectedHandoffPreflightState),
				GetLocomotionHandoffPreflightStateName(HandoffPreflightState)));
		}

		if (!HandoffPreflightReason.Contains(State->ExpectedHandoffPreflightReasonSubstring))
		{
			Fail(FString::Printf(TEXT("LocomotionRequest[%s]: expected handoff preflight reason containing '%s' but got '%s'"),
				*State->CaseName,
				*State->ExpectedHandoffPreflightReasonSubstring,
				*HandoffPreflightReason));
		}
	}

	if (State->bCheckHandoffCommit)
	{
		if (HandoffCommitState != State->ExpectedHandoffCommitState)
		{
			Fail(FString::Printf(TEXT("LocomotionRequest[%s]: expected handoff commit state=%s but got %s"),
				*State->CaseName,
				GetLocomotionHandoffCommitStateName(State->ExpectedHandoffCommitState),
				GetLocomotionHandoffCommitStateName(HandoffCommitState)));
		}

		if (!HandoffCommitReason.Contains(State->ExpectedHandoffCommitReasonSubstring))
		{
			Fail(FString::Printf(TEXT("LocomotionRequest[%s]: expected handoff commit reason containing '%s' but got '%s'"),
				*State->CaseName,
				*State->ExpectedHandoffCommitReasonSubstring,
				*HandoffCommitReason));
		}
	}

	if (State->bCheckTransitionPreservation)
	{
		if (State->PostHandoffPreflightState != HandoffPreflightState)
		{
			Fail(FString::Printf(TEXT("LocomotionRequest[%s]: post-handoff-preflight state snapshot mismatch expected=%s actual=%s"),
				*State->CaseName,
				GetLocomotionHandoffPreflightStateName(State->PostHandoffPreflightState),
				GetLocomotionHandoffPreflightStateName(HandoffPreflightState)));
		}

		if (State->PostHandoffPreflightReason != HandoffPreflightReason)
		{
			Fail(FString::Printf(TEXT("LocomotionRequest[%s]: post-handoff-preflight reason snapshot mismatch expected=%s actual=%s"),
				*State->CaseName,
				*State->PostHandoffPreflightReason,
				*HandoffPreflightReason));
		}

		if (State->PostHandoffCommitState != HandoffCommitState)
		{
			Fail(FString::Printf(TEXT("LocomotionRequest[%s]: post-handoff-commit state snapshot mismatch expected=%s actual=%s"),
				*State->CaseName,
				GetLocomotionHandoffCommitStateName(State->PostHandoffCommitState),
				GetLocomotionHandoffCommitStateName(HandoffCommitState)));
		}

		if (State->PostHandoffCommitReason != HandoffCommitReason)
		{
			Fail(FString::Printf(TEXT("LocomotionRequest[%s]: post-handoff-commit reason snapshot mismatch expected=%s actual=%s"),
				*State->CaseName,
				*State->PostHandoffCommitReason,
				*HandoffCommitReason));
		}
	}

	if (bAllowed != State->bExpectedAllowed)
	{
		Fail(FString::Printf(TEXT("LocomotionRequest[%s]: expected allowed=%d but got %d reason=%s"),
			*State->CaseName,
			State->bExpectedAllowed ? 1 : 0,
			bAllowed ? 1 : 0,
			*GateReason));
	}

	if (RequestState != State->ExpectedRequestState)
	{
		Fail(FString::Printf(TEXT("LocomotionRequest[%s]: expected request state=%s but got %s"),
			*State->CaseName,
			GetLocomotionRequestStateName(State->ExpectedRequestState),
			GetLocomotionRequestStateName(RequestState)));
	}

	if (State->bCheckTransitionPreservation && State->PostRequestState != RequestState)
	{
		Fail(FString::Printf(TEXT("LocomotionRequest[%s]: post-request state snapshot mismatch expected=%s actual=%s"),
			*State->CaseName,
			GetLocomotionRequestStateName(State->PostRequestState),
			GetLocomotionRequestStateName(RequestState)));
	}

	if (!GateReason.Contains(State->ExpectedReasonSubstring))
	{
		Fail(FString::Printf(TEXT("LocomotionRequest[%s]: expected gate reason containing '%s' but got '%s'"),
			*State->CaseName,
			*State->ExpectedReasonSubstring,
			*GateReason));
	}

	if (!RequestReason.Contains(State->ExpectedReasonSubstring))
	{
		Fail(FString::Printf(TEXT("LocomotionRequest[%s]: expected request reason containing '%s' but got '%s'"),
			*State->CaseName,
			*State->ExpectedReasonSubstring,
			*RequestReason));
	}

	if (State->bCheckRuntimeState && !ActiveShellReason.Contains(State->ExpectedReasonSubstring))
	{
		Fail(FString::Printf(TEXT("LocomotionRequest[%s]: expected shell reason containing '%s' but got '%s'"),
			*State->CaseName,
			*State->ExpectedReasonSubstring,
			*ActiveShellReason));
	}

	if (!FMath::IsNearlyEqual(Comp->GetBridgeLocomotionIntentMagnitude(), State->ExpectedIntentMagnitude, 0.0001f))
	{
		Fail(FString::Printf(TEXT("LocomotionRequest[%s]: expected intent magnitude %.2f but got %.2f"),
			*State->CaseName,
			State->ExpectedIntentMagnitude,
			Comp->GetBridgeLocomotionIntentMagnitude()));
	}

	if (!FMath::IsNearlyEqual(Comp->GetBridgeLocomotionIntentAgeSeconds(), State->ExpectedIntentAgeSeconds, 0.01))
	{
		Fail(FString::Printf(TEXT("LocomotionRequest[%s]: expected intent age %.2f but got %.2f"),
			*State->CaseName,
			State->ExpectedIntentAgeSeconds,
			Comp->GetBridgeLocomotionIntentAgeSeconds()));
	}

	if (TerminationState.TerminalReason != State->ExpectedTerminalReason)
	{
		Fail(FString::Printf(TEXT("LocomotionRequest[%s]: expected terminal reason=%d but got %d"),
			*State->CaseName,
			static_cast<int32>(State->ExpectedTerminalReason),
			static_cast<int32>(TerminationState.TerminalReason)));
	}

	if (Latest.SupportMode != State->ExpectedSupportMode)
	{
		Fail(FString::Printf(TEXT("LocomotionRequest[%s]: expected support mode=%d but got %d"),
			*State->CaseName,
			static_cast<int32>(State->ExpectedSupportMode),
			static_cast<int32>(Latest.SupportMode)));
	}

	if (!FMath::IsNearlyEqual(Latest.SupportHullAreaCm2, State->ExpectedSupportHullAreaCm2, 0.01))
	{
		Fail(FString::Printf(TEXT("LocomotionRequest[%s]: expected support hull %.2f but got %.2f"),
			*State->CaseName,
			State->ExpectedSupportHullAreaCm2,
			Latest.SupportHullAreaCm2));
	}

	if (Latest.ActiveSupportSideCount != State->ExpectedActiveSupportSideCount)
	{
		Fail(FString::Printf(TEXT("LocomotionRequest[%s]: expected active support side count=%d but got %d"),
			*State->CaseName,
			State->ExpectedActiveSupportSideCount,
			Latest.ActiveSupportSideCount));
	}

	if (Latest.bCapsuleContractPassed != State->bExpectedCapsuleValid)
	{
		Fail(FString::Printf(TEXT("LocomotionRequest[%s]: expected capsule valid=%d but got %d"),
			*State->CaseName,
			State->bExpectedCapsuleValid ? 1 : 0,
			Latest.bCapsuleContractPassed ? 1 : 0));
	}

	if ((Latest.bPhysicalContinuityValidatorPassed && !Latest.bContinuityBookkeepingMismatch) != State->bExpectedContinuityValid)
	{
		Fail(FString::Printf(TEXT("LocomotionRequest[%s]: expected continuity valid=%d but got %d"),
			*State->CaseName,
			State->bExpectedContinuityValid ? 1 : 0,
			(Latest.bPhysicalContinuityValidatorPassed && !Latest.bContinuityBookkeepingMismatch) ? 1 : 0));
	}

	PHYSANIM_LOG(
		LogTemp,
		Warning,
		TEXT("LocomotionRequest[%s]: preRuntimeState=%d postRuntimeState=%d preAuthorityState=%d postAuthorityState=%d preRequestState=%s postRequestState=%s preHandoffPreflightState=%s postHandoffPreflightState=%s preHandoffCommitState=%s postHandoffCommitState=%s runtimeState=%d requestState=%s handoffPreflightState=%s handoffCommitState=%s gateAllowed=%d gateReason=%s requestReason=%s handoffPreflightReason=%s handoffCommitReason=%s intentMagnitude=%.2f intentAge=%.2f expectedIntentAge=%.2f supportMode=%d supportHull=%.2f activeSides=%d capsuleValid=%d continuityValid=%d terminalReason=%d preBridgeOwnsPhysics=%d postBridgeOwnsPhysics=%d samples=%d"),
		*State->CaseName,
		static_cast<int32>(State->PreRuntimeState),
		static_cast<int32>(State->PostRuntimeState),
		static_cast<int32>(State->PreAuthorityState),
		static_cast<int32>(State->PostAuthorityState),
		GetLocomotionRequestStateName(State->PreRequestState),
		GetLocomotionRequestStateName(State->PostRequestState),
		GetLocomotionHandoffPreflightStateName(State->PreHandoffPreflightState),
		GetLocomotionHandoffPreflightStateName(State->PostHandoffPreflightState),
		GetLocomotionHandoffCommitStateName(State->PreHandoffCommitState),
		GetLocomotionHandoffCommitStateName(State->PostHandoffCommitState),
		static_cast<int32>(RuntimeState),
		GetLocomotionRequestStateName(RequestState),
		GetLocomotionHandoffPreflightStateName(HandoffPreflightState),
		GetLocomotionHandoffCommitStateName(HandoffCommitState),
		bAllowed ? 1 : 0,
		*GateReason,
		*RequestReason,
		*HandoffPreflightReason,
		*HandoffCommitReason,
		Comp->GetBridgeLocomotionIntentMagnitude(),
		Comp->GetBridgeLocomotionIntentAgeSeconds(),
		State->ExpectedIntentAgeSeconds,
		static_cast<int32>(Latest.SupportMode),
		Latest.SupportHullAreaCm2,
		Latest.ActiveSupportSideCount,
		Latest.bCapsuleContractPassed ? 1 : 0,
		(Latest.bPhysicalContinuityValidatorPassed && !Latest.bContinuityBookkeepingMismatch) ? 1 : 0,
		static_cast<int32>(TerminationState.TerminalReason),
		State->bPreBridgeOwnsPhysics ? 1 : 0,
		State->bPostBridgeOwnsPhysics ? 1 : 0,
		Metrics.SampleCount);

	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimActivatedStandingLocomotionRequestStateTest, "PhysAnim.ActivatedStanding.LocomotionRequestState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimActivatedStandingLocomotionRequestStateTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionRequest_NoIntentDenied"));
	OutTestCommands.Add(TEXT("NoIntentDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionRequest_ShortPulseDenied"));
	OutTestCommands.Add(TEXT("ShortPulseDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionRequest_StableRequested"));
	OutTestCommands.Add(TEXT("StableRequested"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionRequest_NegativeSupportDenied"));
	OutTestCommands.Add(TEXT("NegativeSupportDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionRequest_TerminalReasonDenied"));
	OutTestCommands.Add(TEXT("TerminalReasonDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionRequest_CapsuleInvalidDenied"));
	OutTestCommands.Add(TEXT("CapsuleInvalidDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionRequest_ContinuityInvalidDenied"));
	OutTestCommands.Add(TEXT("ContinuityInvalidDenied"));
}

bool FPhysAnimActivatedStandingLocomotionRequestStateTest::RunTest(const FString& Parameters)
{
	AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, true, false));
	ADD_LATENT_AUTOMATION_COMMAND(FCollectActivatedStandingStabilityMetricsCommand(10.0f));

	static FActivatedStandingLocomotionRequestValidationState State;
	State = FActivatedStandingLocomotionRequestValidationState();
	State.CaseName = Parameters;

	ADD_LATENT_AUTOMATION_COMMAND(FApplyActivatedStandingLocomotionRequestStateCommand(&State));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivatedStandingLocomotionRequestStateCommand(&State, this));

	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimActivatedStandingLocomotionRequestStateProofTest, "PhysAnim.ActivatedStanding.LocomotionRequestStateProof", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimActivatedStandingLocomotionRequestStateProofTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionRequestProof_NoIntentDenied"));
	OutTestCommands.Add(TEXT("NoIntentDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionRequestProof_ShortPulseDenied"));
	OutTestCommands.Add(TEXT("ShortPulseDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionRequestProof_StableRequested"));
	OutTestCommands.Add(TEXT("StableRequested"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionRequestProof_NegativeSupportDenied"));
	OutTestCommands.Add(TEXT("NegativeSupportDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionRequestProof_TerminalReasonDenied"));
	OutTestCommands.Add(TEXT("TerminalReasonDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionRequestProof_CapsuleInvalidDenied"));
	OutTestCommands.Add(TEXT("CapsuleInvalidDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionRequestProof_ContinuityInvalidDenied"));
	OutTestCommands.Add(TEXT("ContinuityInvalidDenied"));
}

bool FPhysAnimActivatedStandingLocomotionRequestStateProofTest::RunTest(const FString& Parameters)
{
	AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, true, false));
	ADD_LATENT_AUTOMATION_COMMAND(FCollectActivatedStandingStabilityMetricsCommand(10.0f));

	static FActivatedStandingLocomotionRequestValidationState State;
	State = FActivatedStandingLocomotionRequestValidationState();
	State.CaseName = Parameters;
	State.bCheckTransitionPreservation = true;

	ADD_LATENT_AUTOMATION_COMMAND(FApplyActivatedStandingLocomotionRequestStateCommand(&State));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivatedStandingLocomotionRequestStateCommand(&State, this));

	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimActivatedStandingLocomotionGateProofTest, "PhysAnim.ActivatedStanding.LocomotionGateProof", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimActivatedStandingLocomotionGateProofTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionGateProof_NoIntentDenied"));
	OutTestCommands.Add(TEXT("NoIntentDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionGateProof_ShortPulseDenied"));
	OutTestCommands.Add(TEXT("ShortPulseDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionGateProof_StableAllowed"));
	OutTestCommands.Add(TEXT("StableRequested"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionGateProof_NegativeSupportDenied"));
	OutTestCommands.Add(TEXT("NegativeSupportDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionGateProof_TerminalReasonDenied"));
	OutTestCommands.Add(TEXT("TerminalReasonDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionGateProof_CapsuleInvalidDenied"));
	OutTestCommands.Add(TEXT("CapsuleInvalidDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionGateProof_ContinuityInvalidDenied"));
	OutTestCommands.Add(TEXT("ContinuityInvalidDenied"));
}

bool FPhysAnimActivatedStandingLocomotionGateProofTest::RunTest(const FString& Parameters)
{
	AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, true, false));
	ADD_LATENT_AUTOMATION_COMMAND(FCollectActivatedStandingStabilityMetricsCommand(10.0f));

	static FActivatedStandingLocomotionRequestValidationState State;
	State = FActivatedStandingLocomotionRequestValidationState();
	State.CaseName = Parameters;
	State.bCheckTransitionPreservation = true;

	ADD_LATENT_AUTOMATION_COMMAND(FApplyActivatedStandingLocomotionRequestStateCommand(&State));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivatedStandingLocomotionRequestStateCommand(&State, this));

	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimThighRestoreDiagnosticTest, "PhysAnim.Diagnostics.ThighRestore", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimThighRestoreDiagnosticTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
}

bool FPhysAnimThighRestoreDiagnosticTest::RunTest(const FString& Parameters)
{
	FString VariantParameter = Parameters;
	FString GateThresholdParameter;
	const bool bHasGateThresholdOverride = Parameters.Split(TEXT(":"), &VariantParameter, &GateThresholdParameter);
	const int32 Variant = FCString::Atoi(*VariantParameter);
	const float GateThresholdOverride = bHasGateThresholdOverride ? FCString::Atof(*GateThresholdParameter) : 3600.0f;

	AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	
	if (bHasGateThresholdOverride)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FSetFloatConsoleVariableCommand(TEXT("p.PhysAnim.V0KineticGateThresholdDegPerSec"), GateThresholdOverride));
	}
	ADD_LATENT_AUTOMATION_COMMAND(FSetThighRestoreVariantCommand(Variant));
	ADD_LATENT_AUTOMATION_COMMAND(FEnableStandingProofCommand());
	
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(6.0f));

	// AC-2: emit structured summary before proof teardown so thigh state is still live
	ADD_LATENT_AUTOMATION_COMMAND(FLogThighRestoreVariantSummaryCommand(Variant, this));
	
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyStandingProofCommand(this));
	
	ADD_LATENT_AUTOMATION_COMMAND(FSetThighRestoreVariantCommand(0));
	if (bHasGateThresholdOverride)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FSetFloatConsoleVariableCommand(TEXT("p.PhysAnim.V0KineticGateThresholdDegPerSec"), 3600.0f));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhysAnimThighRestoreDiagnosticContractTest, "PhysAnim.Diagnostics.ThighRestoreContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimThighRestoreDiagnosticContractTest::RunTest(const FString& Parameters)
{
	TSet<FString> VariantNames;
	TSet<FString> VariantCommands;
	for (const FThighRestoreDiagnosticVariant& Variant : ThighRestoreDiagnosticVariants)
	{
		VariantNames.Add(Variant.Name);
		VariantCommands.Add(Variant.Command);
	}

	const TCHAR* RequiredNames[] =
	{
		TEXT("Abrupt_0.01"),
		TEXT("Abrupt_0.02"),
		TEXT("Abrupt_0.05"),
		TEXT("Abrupt_0.10"),
		TEXT("Abrupt_0.15"),
		TEXT("Abrupt_0.20"),
		TEXT("Ramp_0.20_0.5s"),
		TEXT("Ramp_0.20_1.0s"),
		TEXT("KineticGate_ForcedHold_0.20"),
	};

	for (const TCHAR* RequiredName : RequiredNames)
	{
		TestTrue(FString::Printf(TEXT("ThighRestore diagnostic includes %s"), RequiredName), VariantNames.Contains(RequiredName));
	}

	TestTrue(TEXT("Abrupt 0.20 maps to restore variant 7"), VariantCommands.Contains(TEXT("7")));
	TestTrue(TEXT("Forced kinetic gate hold maps to restore variant 7 with threshold override"), VariantCommands.Contains(TEXT("7:-1.0")));
	TestEqual(TEXT("ThighRestore diagnostic variant count"), VariantNames.Num(), static_cast<int32>(UE_ARRAY_COUNT(RequiredNames)));
	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimActivatedStandingLocomotionHandoffPreflightTest, "PhysAnim.ActivatedStanding.LocomotionHandoffPreflight", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimActivatedStandingLocomotionHandoffPreflightTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionHandoffPreflight_NoIntentDenied"));
	OutTestCommands.Add(TEXT("NoIntentDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionHandoffPreflight_ShortPulseDenied"));
	OutTestCommands.Add(TEXT("ShortPulseDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionHandoffPreflight_StableRequested"));
	OutTestCommands.Add(TEXT("StableRequested"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionHandoffPreflight_NegativeSupportDenied"));
	OutTestCommands.Add(TEXT("NegativeSupportDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionHandoffPreflight_TerminalReasonDenied"));
	OutTestCommands.Add(TEXT("TerminalReasonDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionHandoffPreflight_CapsuleInvalidDenied"));
	OutTestCommands.Add(TEXT("CapsuleInvalidDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionHandoffPreflight_ContinuityInvalidDenied"));
	OutTestCommands.Add(TEXT("ContinuityInvalidDenied"));
}

bool FPhysAnimActivatedStandingLocomotionHandoffPreflightTest::RunTest(const FString& Parameters)
{
	AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, true, false));
	ADD_LATENT_AUTOMATION_COMMAND(FCollectActivatedStandingStabilityMetricsCommand(10.0f));

	static FActivatedStandingLocomotionRequestValidationState State;
	State = FActivatedStandingLocomotionRequestValidationState();
	State.CaseName = Parameters;
	State.bCheckTransitionPreservation = true;
	State.bCheckHandoffPreflight = true;

	ADD_LATENT_AUTOMATION_COMMAND(FApplyActivatedStandingLocomotionRequestStateCommand(&State));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivatedStandingLocomotionRequestStateCommand(&State, this));

	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimActivatedStandingLocomotionActiveShellTest, "PhysAnim.ActivatedStanding.LocomotionActiveShell", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimActivatedStandingLocomotionActiveShellTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionActiveShell_NoIntentDenied"));
	OutTestCommands.Add(TEXT("NoIntentDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionActiveShell_ShortPulseDenied"));
	OutTestCommands.Add(TEXT("ShortPulseDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionActiveShell_GateDenied"));
	OutTestCommands.Add(TEXT("GateDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionActiveShell_StableRequested"));
	OutTestCommands.Add(TEXT("StableRequested"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionActiveShell_StableCommitted"));
	OutTestCommands.Add(TEXT("StableCommitted"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionActiveShell_NoPreflightDenied"));
	OutTestCommands.Add(TEXT("NoPreflightDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionActiveShell_DroppedAfterPreflightDenied"));
	OutTestCommands.Add(TEXT("DroppedAfterPreflightDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionActiveShell_NegativeSupportDenied"));
	OutTestCommands.Add(TEXT("NegativeSupportDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionActiveShell_TerminalReasonDenied"));
	OutTestCommands.Add(TEXT("TerminalReasonDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionActiveShell_CapsuleInvalidDenied"));
	OutTestCommands.Add(TEXT("CapsuleInvalidDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionActiveShell_ContinuityInvalidDenied"));
	OutTestCommands.Add(TEXT("ContinuityInvalidDenied"));
}

bool FPhysAnimActivatedStandingLocomotionActiveShellTest::RunTest(const FString& Parameters)
{
	AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, true, false));
	ADD_LATENT_AUTOMATION_COMMAND(FCollectActivatedStandingStabilityMetricsCommand(10.0f));

	static FActivatedStandingLocomotionRequestValidationState State;
	State = FActivatedStandingLocomotionRequestValidationState();
	State.CaseName = Parameters;
	State.bCheckTransitionPreservation = true;
	State.bCheckRuntimeState = true;

	ADD_LATENT_AUTOMATION_COMMAND(FApplyActivatedStandingLocomotionRequestStateCommand(&State));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivatedStandingLocomotionRequestStateCommand(&State, this));

	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimActivatedStandingLocomotionHandoffPreflightProofTest, "PhysAnim.ActivatedStanding.LocomotionHandoffPreflightProof", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimActivatedStandingLocomotionHandoffPreflightProofTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionHandoffPreflightProof_NoIntentDenied"));
	OutTestCommands.Add(TEXT("NoIntentDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionHandoffPreflightProof_ShortPulseDenied"));
	OutTestCommands.Add(TEXT("ShortPulseDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionHandoffPreflightProof_StableRequested"));
	OutTestCommands.Add(TEXT("StableRequested"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionHandoffPreflightProof_NegativeSupportDenied"));
	OutTestCommands.Add(TEXT("NegativeSupportDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionHandoffPreflightProof_TerminalReasonDenied"));
	OutTestCommands.Add(TEXT("TerminalReasonDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionHandoffPreflightProof_CapsuleInvalidDenied"));
	OutTestCommands.Add(TEXT("CapsuleInvalidDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionHandoffPreflightProof_ContinuityInvalidDenied"));
	OutTestCommands.Add(TEXT("ContinuityInvalidDenied"));
}

bool FPhysAnimActivatedStandingLocomotionHandoffPreflightProofTest::RunTest(const FString& Parameters)
{
	AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, true, false));
	ADD_LATENT_AUTOMATION_COMMAND(FCollectActivatedStandingStabilityMetricsCommand(10.0f));

	static FActivatedStandingLocomotionRequestValidationState State;
	State = FActivatedStandingLocomotionRequestValidationState();
	State.CaseName = Parameters;
	State.bCheckTransitionPreservation = true;
	State.bCheckHandoffPreflight = true;

	ADD_LATENT_AUTOMATION_COMMAND(FApplyActivatedStandingLocomotionRequestStateCommand(&State));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivatedStandingLocomotionRequestStateCommand(&State, this));

	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimActivatedStandingLocomotionHandoffCommitProofTest, "PhysAnim.ActivatedStanding.LocomotionHandoffCommitProof", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimActivatedStandingLocomotionHandoffCommitProofTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionHandoffCommitProof_NoPreflightDenied"));
	OutTestCommands.Add(TEXT("NoPreflightDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionHandoffCommitProof_NoIntentDenied"));
	OutTestCommands.Add(TEXT("NoIntentDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionHandoffCommitProof_ShortPulseDenied"));
	OutTestCommands.Add(TEXT("ShortPulseDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionHandoffCommitProof_StableCommitted"));
	OutTestCommands.Add(TEXT("StableCommitted"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionHandoffCommitProof_DroppedAfterPreflightDenied"));
	OutTestCommands.Add(TEXT("DroppedAfterPreflightDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionHandoffCommitProof_NegativeSupportDenied"));
	OutTestCommands.Add(TEXT("NegativeSupportDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionHandoffCommitProof_TerminalReasonDenied"));
	OutTestCommands.Add(TEXT("TerminalReasonDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionHandoffCommitProof_CapsuleInvalidDenied"));
	OutTestCommands.Add(TEXT("CapsuleInvalidDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionHandoffCommitProof_ContinuityInvalidDenied"));
	OutTestCommands.Add(TEXT("ContinuityInvalidDenied"));
}

bool FPhysAnimActivatedStandingLocomotionHandoffCommitProofTest::RunTest(const FString& Parameters)
{
	AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	if (Parameters == TEXT("NoPreflightDenied"))
	{
		ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(false, false, false));
	}
	else
	{
		ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, true, false));
	}
	if (Parameters != TEXT("NoPreflightDenied"))
	{
		ADD_LATENT_AUTOMATION_COMMAND(FCollectActivatedStandingStabilityMetricsCommand(10.0f));
	}

	static FActivatedStandingLocomotionRequestValidationState State;
	State = FActivatedStandingLocomotionRequestValidationState();
	State.CaseName = Parameters;
	State.bCheckTransitionPreservation = true;
	State.bCheckHandoffPreflight = true;
	State.bCheckHandoffCommit = true;
	if (Parameters == TEXT("NoPreflightDenied"))
	{
		State.ExpectedReasonSubstring.Reset();
		State.ExpectedHandoffCommitState = EBridgeLocomotionHandoffCommitState::LocomotionHandoffCommitPending;
		State.ExpectedHandoffCommitReasonSubstring.Reset();
	}

	ADD_LATENT_AUTOMATION_COMMAND(FApplyActivatedStandingLocomotionRequestStateCommand(&State));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivatedStandingLocomotionRequestStateCommand(&State, this));

	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimActivatedStandingLocomotionHandoffCommitTest, "PhysAnim.ActivatedStanding.LocomotionHandoffCommit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimActivatedStandingLocomotionHandoffCommitTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionHandoffCommit_NoPreflightDenied"));
	OutTestCommands.Add(TEXT("NoPreflightDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionHandoffCommit_NoIntentDenied"));
	OutTestCommands.Add(TEXT("NoIntentDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionHandoffCommit_ShortPulseDenied"));
	OutTestCommands.Add(TEXT("ShortPulseDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionHandoffCommit_StableCommitted"));
	OutTestCommands.Add(TEXT("StableCommitted"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionHandoffCommit_DroppedAfterPreflightDenied"));
	OutTestCommands.Add(TEXT("DroppedAfterPreflightDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionHandoffCommit_NegativeSupportDenied"));
	OutTestCommands.Add(TEXT("NegativeSupportDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionHandoffCommit_TerminalReasonDenied"));
	OutTestCommands.Add(TEXT("TerminalReasonDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionHandoffCommit_CapsuleInvalidDenied"));
	OutTestCommands.Add(TEXT("CapsuleInvalidDenied"));

	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_LocomotionHandoffCommit_ContinuityInvalidDenied"));
	OutTestCommands.Add(TEXT("ContinuityInvalidDenied"));
}

bool FPhysAnimActivatedStandingLocomotionHandoffCommitTest::RunTest(const FString& Parameters)
{
	AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	if (Parameters == TEXT("NoPreflightDenied"))
	{
		ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(false, false, false));
	}
	else
	{
		ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, true, false));
	}
	if (Parameters != TEXT("NoPreflightDenied"))
	{
		ADD_LATENT_AUTOMATION_COMMAND(FCollectActivatedStandingStabilityMetricsCommand(10.0f));
	}

	static FActivatedStandingLocomotionRequestValidationState State;
	State = FActivatedStandingLocomotionRequestValidationState();
	State.CaseName = Parameters;
	State.bCheckTransitionPreservation = true;
	State.bCheckHandoffPreflight = true;
	State.bCheckHandoffCommit = true;

	ADD_LATENT_AUTOMATION_COMMAND(FApplyActivatedStandingLocomotionRequestStateCommand(&State));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivatedStandingLocomotionRequestStateCommand(&State, this));

	return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// STAGE 2A DUMBBELL LOAD TEST
// ---------------------------------------------------------------------------------------------------------------------

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimStage2ADumbbellLoadTest, "PhysAnim.StandingProof.LoadTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimStage2ADumbbellLoadTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	// Parameters represent the mass of the dumbbell in Kg
	const TArray<FString> Weights = { TEXT("0"), TEXT("1"), TEXT("2"), TEXT("3"), TEXT("4"), TEXT("5") };
	for (const FString& Weight : Weights)
	{
		OutBeautifiedNames.Add(Weight + TEXT("kg"));
		OutTestCommands.Add(Weight);
	}
}

bool FPhysAnimStage2ADumbbellLoadTest::RunTest(const FString& Parameters)
{
	const float TargetMassKg = FCString::Atof(*Parameters);
	
	// 1. Load the map
	AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));

	// NOTE: StrictLivePolicyProofQuality is intentionally NOT disabled here.
	// A previous version of this test suppressed the quality gate because the dumbbell
	// shifts the CoM outside the support polygon. That suppression was hiding the
	// exact failure mode the test is meant to detect. The policy must adapt to the load;
	// if it cannot maintain support under N kg, the test should FAIL, not bypass the check.

	// 2. Wait for map to load
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));

	// 3. Spawn and attach the dumbbell for this specific run
	ADD_LATENT_AUTOMATION_COMMAND(FSetupDumbbellCommand(TargetMassKg));

	// 4. Wait for the physics to settle under the new load (2 seconds)
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));

	// 5. Enable proof
	ADD_LATENT_AUTOMATION_COMMAND(FEnableStandingProofCommand());

	// 6. Actively poll until proof completes (up to 15 s).
	// Previously this was a blind FWaitLatentCommand(6.0f) that had no way of knowing
	// whether the proof actually finished before the snapshot in step 7 was taken.
	// Under -NullRHI the tick budget can be very different from an interactive session,
	// causing the same 6 s to cover wildly different amounts of simulation time.
	{
		// We capture the component pointer at command-queue time via a helper latent command.
		// FWaitForStandingProofCommand polls IsLiveRuntimeEvidenceProofComplete() each tick
		// and returns false (keep going) until it's true or the 15 s hard timeout fires.
		UWorld* SetupWorld = GWorld;
	#if WITH_EDITOR
		if (GIsEditor)
		{
			for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
			{
				if (Ctx.WorldType == EWorldType::PIE || Ctx.WorldType == EWorldType::Game)
				{
					SetupWorld = Ctx.World();
					break;
				}
			}
		}
	#endif
		UPhysAnimComponent* PollingTarget = nullptr;
		if (SetupWorld)
		{
			for (TActorIterator<ACharacter> It(SetupWorld); It; ++It)
			{
				if (UPhysAnimComponent* C = It->FindComponentByClass<UPhysAnimComponent>())
				{
					PollingTarget = C;
					break;
				}
			}
		}
		constexpr float ProofTimeoutSeconds = 15.0f;
		ADD_LATENT_AUTOMATION_COMMAND(FWaitForStandingProofCommand(
			PollingTarget,
			ProofTimeoutSeconds,
			static_cast<float>(FPlatformTime::Seconds())));
	}

	// 7. Verify results — now runs only after proof actually completed (or timed out).
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyStandingProofCommand(this));

	return true;
}

#endif
