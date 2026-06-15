#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"
#include "PhysAnimBalanceReadyTransitionPrivate.h"
#include "PhysAnimPhase1AutoCalibSubsystem.h"
#include "PhysAnimPhase1PelvisCouplingSearch.h"
#include "PhysAnimLogger.h"

namespace
{
}

void UPhysAnimComponent::BeginPlay()
{
	Super::BeginPlay();

	PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnim] GStrictPhase1Certification = %d"), GStrictPhase1Certification);

	FString Error;
	if (!BeginStartupTPoseCapture(Error))
	{
		PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Error, 1.0f, TEXT("[PhysAnim] Startup blocked before live T-pose capture: %s"), *Error);
		FailStop(FString::Printf(TEXT("Startup blocked before live T-pose capture: %s"), *Error));
		UpdateBridgeStatusIndicator(5.0f);
	}
}


void UPhysAnimComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopBridge();
	PhysAnimComponentInternal::ClearPhysicsTuningDiagnosticCaches(this);
	Super::EndPlay(EndPlayReason);
}

void UPhysAnimComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPhysAnimComponent_TickComponent);

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	UAnimInstance* const LocalAnimInstance = this->AnimInstance.Get();

	if (!PhysicsControl || !SkeletalMesh || !LocalAnimInstance)
	{
		FailStop(TEXT("Runtime context became invalid after startup."));
		return;
	}

	UpdateStabilizationStressTestState(ResolveEffectiveStabilizationSettings());
	const FPhysAnimStabilizationSettings EffectiveSettings = ResolveEffectiveStabilizationSettings();
	
	UpdateStartupMovementLockState(EffectiveSettings);

	FString TickError;

	if (bPendingStartupRestPoseCapture)
	{
		if (!FinalizeStartupTPoseCaptureAndStartBridge(TickError))
		{
			FailStop(FString::Printf(TEXT("Failed to finalize startup T-pose capture: %s"), *TickError));
			return;
		}
	}

	if (RuntimeState == EPhysAnimRuntimeState::Uninitialized)
	{
		return;
	}

	FPoseSearchBlueprintResult SearchResult;

	if (RuntimeState == EPhysAnimRuntimeState::WaitingForPoseSearch)
	{
		HandleInitialPoseSearchWait(DeltaTime, EffectiveSettings, TickError, SearchResult);
		return;
	}

	if (!CheckRuntimeInstability(DeltaTime, EffectiveSettings, TickError))
	{
		FailStop(TickError);
		return;
	}

	AdvanceBringUpState(DeltaTime, EffectiveSettings);

	TickRuntimeStateMachine(DeltaTime, EffectiveSettings);
	TickPolicyAndUpdateMetrics(DeltaTime, EffectiveSettings, TickError);
	ProcessPendingDistalOwnershipChecks();

	if (bEnableLiveRuntimeEvidenceProof)
	{
		TickLiveRuntimeEvidenceProof(DeltaTime);
	}

	if (!TickError.IsEmpty())
	{
		FailStop(TickError);
		return;
	}
}
