#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"
#include "PhysAnimBalanceReadyTransitionPrivate.h"
#include "PhysAnimPhase1AutoCalibSubsystem.h"
#include "PhysAnimPhase1PelvisCouplingSearch.h"
#include "PhysAnimLogger.h"

#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
}

void UPhysAnimComponent::BeginPlay()
{
	Super::BeginPlay();

#if WITH_DEV_AUTOMATION_TESTS
	ApplyProductVariantFromCommandLineForTesting(FCommandLine::Get());
#endif

	PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnim] GStrictPhase1Certification = %d"), GStrictPhase1Certification);

	FString Error;
	if (!BeginStartupTPoseCapture(Error))
	{
		PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Error, 1.0f, TEXT("[PhysAnim] Startup blocked before live T-pose capture: %s"), *Error);
		FailStop(FString::Printf(TEXT("Startup blocked before live T-pose capture: %s"), *Error));
		UpdateBridgeStatusIndicator(5.0f);
	}
}

#if WITH_DEV_AUTOMATION_TESTS
void UPhysAnimComponent::ApplyProductVariantFromCommandLineForTesting(const TCHAR* CommandLine)
{
	bActionSemanticTraceEnabledForTesting = CommandLine &&
		FParse::Param(CommandLine, TEXT("PhysAnimActionSemanticTrace"));
	bMannyLocalFrameRoundtripTraceEnabledForTesting = CommandLine &&
		FParse::Param(CommandLine, TEXT("PhysAnimMannyLocalFrameRoundtripTrace"));
	bExperimentalComponentActionAxisEnabledForTesting = CommandLine &&
		FParse::Param(CommandLine, TEXT("PhysAnimExperimentalComponentActionAxis"));
	bExperimentalComponentActionAxisFromFirstPolicyEnabledForTesting = CommandLine &&
		FParse::Param(CommandLine, TEXT("PhysAnimExperimentalComponentActionAxisFromFirstPolicy"));
	bExperimentalBindNeutralFromFirstPolicyEnabledForTesting = CommandLine &&
		FParse::Param(CommandLine, TEXT("PhysAnimExperimentalBindNeutralFromFirstPolicy"));
	bExperimentalConstraintRangeRemapBypassFromFirstPolicyEnabledForTesting = CommandLine &&
		FParse::Param(CommandLine, TEXT("PhysAnimExperimentalConstraintRangeRemapBypassFromFirstPolicy"));
	bExperimentalCausalStandingSpineChestEnabledForTesting = false;
	bExperimentalCausalStandingDistalHandsEnabledForTesting = false;
	bExperimentalCausalStandingNeckHeadEnabledForTesting = false;
	bExperimentalCausalStandingNeckEnabledForTesting = false;
	bExperimentalCausalStandingHeadEnabledForTesting = false;
	bExperimentalCausalStandingHeadActiveOnlyEnabledForTesting = false;
	bExperimentalCausalStandingHeadAfterFirstPolicyEnabledForTesting = false;
	FString ExperimentalCausalStandingUpperBodyName;
	if (CommandLine &&
		FParse::Value(
			CommandLine,
			TEXT("PhysAnimExperimentalCausalStandingUpperBody="),
			ExperimentalCausalStandingUpperBodyName))
	{
		bExperimentalCausalStandingSpineChestEnabledForTesting =
			ExperimentalCausalStandingUpperBodyName == TEXT("SpineChest");
		bExperimentalCausalStandingDistalHandsEnabledForTesting =
			ExperimentalCausalStandingUpperBodyName == TEXT("DistalHands");
		bExperimentalCausalStandingNeckHeadEnabledForTesting =
			ExperimentalCausalStandingUpperBodyName == TEXT("NeckHead");
		bExperimentalCausalStandingNeckEnabledForTesting =
			ExperimentalCausalStandingUpperBodyName == TEXT("Neck") ||
			bExperimentalCausalStandingNeckHeadEnabledForTesting;
		bExperimentalCausalStandingHeadActiveOnlyEnabledForTesting =
			ExperimentalCausalStandingUpperBodyName == TEXT("HeadActiveOnly");
		bExperimentalCausalStandingHeadAfterFirstPolicyEnabledForTesting =
			ExperimentalCausalStandingUpperBodyName == TEXT("HeadAfterFirstPolicy");
		bExperimentalCausalStandingHeadEnabledForTesting =
			ExperimentalCausalStandingUpperBodyName == TEXT("Head") ||
			bExperimentalCausalStandingHeadActiveOnlyEnabledForTesting ||
			bExperimentalCausalStandingHeadAfterFirstPolicyEnabledForTesting ||
			bExperimentalCausalStandingNeckHeadEnabledForTesting;
	}
	bExperimentalPolicyActionBaselineResidualEnabledForTesting = CommandLine &&
		FParse::Param(CommandLine, TEXT("PhysAnimExperimentalPolicyActionBaselineResidual"));
	bExperimentalPolicyActionZeroUntilBaselineEnabledForTesting = CommandLine &&
		FParse::Param(CommandLine, TEXT("PhysAnimExperimentalPolicyActionZeroUntilBaseline"));
	bExperimentalPhysicsBodyObservationPositionsEnabledForTesting = CommandLine &&
		FParse::Param(CommandLine, TEXT("PhysAnimExperimentalPhysicsBodyObservationPositions"));
	ExperimentalActiveStrengthFactorForTesting = 1.0f;
	if (CommandLine)
	{
		FParse::Value(
			CommandLine,
			TEXT("PhysAnimExperimentalActiveStrengthFactor="),
			ExperimentalActiveStrengthFactorForTesting);
	}
	bExperimentalPolicyActionBaselineResidualStartedForTesting = false;
	bExperimentalCheckpointTorqueCeilingEnabledForTesting = CommandLine &&
		FParse::Param(CommandLine, TEXT("PhysAnimExperimentalCheckpointTorqueCeiling"));
	bExperimentalCheckpointForcePdEnabledForTesting = CommandLine &&
		FParse::Param(CommandLine, TEXT("PhysAnimExperimentalCheckpointForcePd"));
	ExperimentalActionFamilyMaskForTesting = EPhysAnimExperimentalActionFamilyMask::All;
	FString ExperimentalActionFamilyName;
	if (CommandLine &&
		FParse::Value(
			CommandLine,
			TEXT("PhysAnimExperimentalActionFamily="),
			ExperimentalActionFamilyName))
	{
		if (ExperimentalActionFamilyName == TEXT("Zero"))
		{
			ExperimentalActionFamilyMaskForTesting = EPhysAnimExperimentalActionFamilyMask::Zero;
		}
		else if (ExperimentalActionFamilyName == TEXT("LowerOnly"))
		{
			ExperimentalActionFamilyMaskForTesting = EPhysAnimExperimentalActionFamilyMask::LowerOnly;
		}
		else if (ExperimentalActionFamilyName == TEXT("AxialOnly"))
		{
			ExperimentalActionFamilyMaskForTesting = EPhysAnimExperimentalActionFamilyMask::AxialOnly;
		}
		else if (ExperimentalActionFamilyName == TEXT("ArmsOnly"))
		{
			ExperimentalActionFamilyMaskForTesting = EPhysAnimExperimentalActionFamilyMask::ArmsOnly;
		}
	}
	ExperimentalActionJointRangeStartForTesting = INDEX_NONE;
	ExperimentalActionJointRangeCountForTesting = 0;
	if (CommandLine)
	{
		int32 ParsedJointStart = INDEX_NONE;
		int32 ParsedJointCount = 0;
		if (FParse::Value(
				CommandLine,
				TEXT("PhysAnimExperimentalActionJointStart="),
				ParsedJointStart))
		{
			ExperimentalActionJointRangeStartForTesting = ParsedJointStart;
			FParse::Value(
				CommandLine,
				TEXT("PhysAnimExperimentalActionJointCount="),
				ParsedJointCount);
			ExperimentalActionJointRangeCountForTesting = FMath::Max(0, ParsedJointCount);
		}
	}
	bPolicyInputProvenanceTraceEnabledForTesting = CommandLine &&
		FParse::Param(CommandLine, TEXT("PhysAnimPolicyInputProvenanceTrace"));
	bStartupChronologyTraceEnabledForTesting = CommandLine &&
		FParse::Param(CommandLine, TEXT("PhysAnimStartupChronologyTrace"));

	FString VariantName;
	if (!CommandLine || !FParse::Value(CommandLine, TEXT("PhysAnimProductVariant="), VariantName))
	{
		return;
	}

	EPhysAnimStandingVariant ResolvedVariant = EPhysAnimStandingVariant::Normal;
	if (VariantName == TEXT("ZeroActions"))
	{
		ResolvedVariant = EPhysAnimStandingVariant::ZeroActions;
	}
	else if (VariantName == TEXT("DropControlDispatch"))
	{
		ResolvedVariant = EPhysAnimStandingVariant::DropControlDispatch;
	}
	else if (VariantName == TEXT("ControlsOff"))
	{
		ResolvedVariant = EPhysAnimStandingVariant::ControlsOff;
	}
	else if (VariantName == TEXT("DampingOnly"))
	{
		ResolvedVariant = EPhysAnimStandingVariant::DampingOnly;
	}
	else if (VariantName == TEXT("FixedNeutralTarget"))
	{
		ResolvedVariant = EPhysAnimStandingVariant::FixedNeutralTarget;
	}
	else if (VariantName == TEXT("RealOnnxPolicy"))
	{
		ResolvedVariant = EPhysAnimStandingVariant::RealOnnxPolicy;
	}
	else if (VariantName != TEXT("Normal") && VariantName != TEXT("ForcedSupportLoss"))
	{
		return;
	}

	StandingVariantForTesting = ResolvedVariant;
	bProductControlDispatchDroppedForTesting =
		ResolvedVariant == EPhysAnimStandingVariant::DropControlDispatch;
}
#endif


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
#if WITH_DEV_AUTOMATION_TESTS
		CaptureStartupChronologySampleForTesting(TEXT("pre_state_machine"));
#endif
		HandleInitialPoseSearchWait(DeltaTime, EffectiveSettings, TickError, SearchResult);
#if WITH_DEV_AUTOMATION_TESTS
		CaptureStartupChronologySampleForTesting(TEXT("post_state_machine"));
		CaptureStartupChronologySampleForTesting(TEXT("post_policy"));
#endif
		return;
	}

	if (bStandingFullSimulationCommitted && !CheckRuntimeInstability(DeltaTime, EffectiveSettings, TickError))
	{
		FailStop(TickError);
		return;
	}

#if WITH_DEV_AUTOMATION_TESTS
	CaptureStartupChronologySampleForTesting(TEXT("pre_state_machine"));
#endif
	TickRuntimeStateMachine(DeltaTime, EffectiveSettings);
#if WITH_DEV_AUTOMATION_TESTS
	CaptureStartupChronologySampleForTesting(TEXT("post_state_machine"));
#endif
	TickPolicyAndUpdateMetrics(DeltaTime, EffectiveSettings, TickError);
#if WITH_DEV_AUTOMATION_TESTS
	CaptureStartupChronologySampleForTesting(TEXT("post_policy"));
#endif
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
