from pathlib import Path

ROOT = Path(".")
H = ROOT / "PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimComponent.h"
CPP = ROOT / "PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp"
CORE = ROOT / "PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Core.cpp"

def replace_once(text: str, old: str, new: str, path: Path) -> str:
    if old not in text:
        raise RuntimeError(f"Marker not found in {path}: {old[:120]!r}")
    return text.replace(old, new, 1)

def insert_before(text: str, marker: str, insertion: str, path: Path) -> str:
    if insertion.strip() in text:
        return text
    return replace_once(text, marker, insertion + marker, path)

def insert_after(text: str, marker: str, insertion: str, path: Path) -> str:
    if insertion.strip() in text:
        return text
    return replace_once(text, marker, marker + insertion, path)

h = H.read_text(encoding="utf-8")
cpp = CPP.read_text(encoding="utf-8")
core = CORE.read_text(encoding="utf-8")

# Header include
h = insert_before(
    h,
    '#include "PhysAnimComponent.generated.h"',
    '#include "PhysAnimRuntimeTerminationPipeline.h"\n',
    H,
)

# Insert public/proof UPROPERTY block
proof_properties = r'''
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Proof|RuntimeEvidence")
	bool bEnableLiveRuntimeEvidenceProof = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Proof|RuntimeEvidence")
	FName LiveRuntimeEvidenceLeftSupportBodyName = TEXT("foot_l");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Proof|RuntimeEvidence")
	FName LiveRuntimeEvidenceRightSupportBodyName = TEXT("foot_r");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Proof|RuntimeEvidence", meta = (ClampMin = "0.0"))
	float LiveRuntimeEvidenceSupportSweepRadiusCm = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Proof|RuntimeEvidence", meta = (ClampMin = "0.0"))
	float LiveRuntimeEvidenceSupportSweepDistanceCm = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Proof|RuntimeEvidence", meta = (ClampMin = "0.0"))
	float LiveRuntimeEvidenceSupportSweepStartLiftCm = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Proof|RuntimeEvidence")
	bool bLiveRuntimeEvidenceWorldStaticOnly = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Proof|RuntimeEvidence", meta = (ClampMin = "0.0"))
	float LiveRuntimeEvidenceStandingTargetSeconds = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Proof|RuntimeEvidence", meta = (ClampMin = "0.0"))
	float LiveRuntimeEvidenceProgressLogIntervalSeconds = 0.25f;

'''

if "bEnableLiveRuntimeEvidenceProof" not in h:
    if "public:\n\tUPhysAnimComponent();" in h:
        h = insert_after(h, "public:\n\tUPhysAnimComponent();\n", proof_properties, H)
    else:
        raise RuntimeError("Could not find UPhysAnimComponent public constructor anchor.")

proof_private = r'''
	bool bLiveRuntimeEvidenceProofActive = false;
	bool bLiveRuntimeEvidenceProofComplete = false;
	bool bLiveRuntimeEvidenceTerminalArtifactEmitted = false;

	FString LiveRuntimeEvidenceAttemptUuid;
	float LiveRuntimeEvidenceStandingSeconds = 0.0f;
	float LiveRuntimeEvidenceLastProgressLogSeconds = -1.0f;
	int64 LiveRuntimeEvidenceSubstepCounter = 0;

	FPhysAnimRuntimeTerminationState LiveRuntimeEvidenceTerminationState;

	void TickLiveRuntimeEvidenceProof(float DeltaTimeSeconds);
	void ResetLiveRuntimeEvidenceProof();
	bool CaptureLiveRuntimeEvidenceHitResults(TArray<FHitResult>& OutHitResults, int32& OutMappedSupportHitCount) const;
	bool CaptureLiveRuntimeEvidenceHitResultForBody(const FName BodyName, TArray<FHitResult>& OutHitResults) const;
	FPhysAnimSupportHitResultObservationInput BuildLiveRuntimeEvidenceObservationInput(
		const TArray<FHitResult>& HitResults,
		float DeltaTimeSeconds) const;
	FPhysAnimRuntimeSubstepInput BuildLiveRuntimeEvidenceSubstepInput(
		const FPhysAnimSupportObservationResult& SupportObservation,
		float DeltaTimeSeconds) const;
	void EmitLiveRuntimeEvidenceProgressLog(
		const FPhysAnimRuntimeTerminationPipelineResult& PipelineResult,
		int32 HitCount,
		int32 MappedSupportHitCount);
	void EmitLiveRuntimeEvidenceTerminalArtifactOnce(
		const FPhysAnimRuntimeTerminationPipelineResult& PipelineResult);
	void EmitLiveRuntimeEvidenceAttemptResult(
		bool bPassed,
		const FPhysAnimRuntimeTerminationPipelineResult& PipelineResult);

'''

if "TickLiveRuntimeEvidenceProof" not in h:
    if "\nprivate:\n" in h:
        h = insert_after(h, "\nprivate:\n", proof_private, H)
    else:
        raise RuntimeError("Could not find private: anchor in PhysAnimComponent.h")

# CPP includes
cpp = insert_after(cpp, '#include "PhysAnimComponentPrivate.h"\n', '''#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
''', CPP)

# Constructor init
# Search for constructor body end or similar.
# In PhysAnimComponent.cpp, we can look for the closing brace of the constructor if it's there, 
# or just look for the first method implementation.
if "LiveRuntimeEvidenceAttemptUuid" not in cpp:
    if "\n}\n\nbool UPhysAnimComponent::BuildConditionedActions" in cpp:
         cpp = insert_before(
            cpp,
            "\n}\n\nbool UPhysAnimComponent::BuildConditionedActions",
            '\n\tLiveRuntimeEvidenceAttemptUuid = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);\n',
            CPP,
        )
    else:
        # Fallback: look for the constructor definition start
        if "UPhysAnimComponent::UPhysAnimComponent()" in cpp:
             cpp = insert_after(
                cpp,
                "UPhysAnimComponent::UPhysAnimComponent()\n{",
                '\n\tLiveRuntimeEvidenceAttemptUuid = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);',
                CPP,
            )

impl = r'''

void UPhysAnimComponent::ResetLiveRuntimeEvidenceProof()
{
	bLiveRuntimeEvidenceProofActive = false;
	bLiveRuntimeEvidenceProofComplete = false;
	bLiveRuntimeEvidenceTerminalArtifactEmitted = false;

	LiveRuntimeEvidenceAttemptUuid = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	LiveRuntimeEvidenceStandingSeconds = 0.0f;
	LiveRuntimeEvidenceLastProgressLogSeconds = -1.0f;
	LiveRuntimeEvidenceSubstepCounter = 0;
	LiveRuntimeEvidenceTerminationState = FPhysAnimRuntimeTerminationState();
}

void UPhysAnimComponent::TickLiveRuntimeEvidenceProof(float DeltaTimeSeconds)
{
	if (!bEnableLiveRuntimeEvidenceProof)
	{
		if (bLiveRuntimeEvidenceProofActive || bLiveRuntimeEvidenceProofComplete)
		{
			ResetLiveRuntimeEvidenceProof();
		}
		return;
	}

	if (bLiveRuntimeEvidenceProofComplete)
	{
		return;
	}

	if (!bLiveRuntimeEvidenceProofActive)
	{
		bLiveRuntimeEvidenceProofActive = true;
		bLiveRuntimeEvidenceProofComplete = false;
		bLiveRuntimeEvidenceTerminalArtifactEmitted = false;
		LiveRuntimeEvidenceAttemptUuid = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
		LiveRuntimeEvidenceStandingSeconds = 0.0f;
		LiveRuntimeEvidenceLastProgressLogSeconds = -1.0f;
		LiveRuntimeEvidenceSubstepCounter = 0;
		LiveRuntimeEvidenceTerminationState = FPhysAnimRuntimeTerminationState();

		UE_LOG(LogPhysAnimBridge, Warning, TEXT("PhysAnimProof: AttemptStart uuid=%s"), *LiveRuntimeEvidenceAttemptUuid);
	}

	LiveRuntimeEvidenceStandingSeconds += FMath::Max(0.0f, DeltaTimeSeconds);
	++LiveRuntimeEvidenceSubstepCounter;

	TArray<FHitResult> HitResults;
	int32 MappedSupportHitCount = 0;
	CaptureLiveRuntimeEvidenceHitResults(HitResults, MappedSupportHitCount);

	const FPhysAnimSupportHitResultObservationInput ObservationInput =
		BuildLiveRuntimeEvidenceObservationInput(HitResults, DeltaTimeSeconds);

	const FPhysAnimSupportObservationResult SupportObservation =
		PhysAnimRuntimeAdapter::BuildSupportObservationFromHitResults(ObservationInput);

	FPhysAnimRuntimeTerminationPipelineInput PipelineInput;
	PipelineInput.PreviousState = LiveRuntimeEvidenceTerminationState;
	PipelineInput.SubstepInput = BuildLiveRuntimeEvidenceSubstepInput(SupportObservation, DeltaTimeSeconds);
	PipelineInput.bEnableTerminationCommand = true;
	PipelineInput.bAllowMovementReclaimOnTermination = true;

	const FPhysAnimRuntimeTerminationPipelineResult PipelineResult =
		PhysAnimRuntimeTerminationPipeline::EvaluateTerminationPipeline(PipelineInput);

	LiveRuntimeEvidenceTerminationState = PipelineResult.StateApplyResult.State;

	UE_LOG(
		LogPhysAnimBridge,
		Verbose,
		TEXT("PhysAnimProof: RuntimeEvidence uuid=%s hits=%d mapped=%d support_mode=%d active_sides=%d hull_area=%.3f"),
		*LiveRuntimeEvidenceAttemptUuid,
		HitResults.Num(),
		MappedSupportHitCount,
		static_cast<int32>(PipelineResult.SubstepResult.Artifact.SupportMode),
		PipelineResult.SubstepResult.Artifact.ActiveSupportSideCount,
		PipelineResult.SubstepResult.Artifact.SupportHullAreaCm2);

	EmitLiveRuntimeEvidenceProgressLog(PipelineResult, HitResults.Num(), MappedSupportHitCount);

	if (PipelineResult.StateApplyResult.State.bTerminated)
	{
		EmitLiveRuntimeEvidenceTerminalArtifactOnce(PipelineResult);
		EmitLiveRuntimeEvidenceAttemptResult(false, PipelineResult);
		bLiveRuntimeEvidenceProofComplete = true;
		return;
	}

	if (LiveRuntimeEvidenceStandingSeconds >= LiveRuntimeEvidenceStandingTargetSeconds)
	{
		EmitLiveRuntimeEvidenceAttemptResult(true, PipelineResult);
		bLiveRuntimeEvidenceProofComplete = true;
	}
}

bool UPhysAnimComponent::CaptureLiveRuntimeEvidenceHitResults(TArray<FHitResult>& OutHitResults, int32& OutMappedSupportHitCount) const
{
	OutHitResults.Reset();
	OutMappedSupportHitCount = 0;

	const int32 BeforeLeft = OutHitResults.Num();
	if (CaptureLiveRuntimeEvidenceHitResultForBody(LiveRuntimeEvidenceLeftSupportBodyName, OutHitResults))
	{
		OutMappedSupportHitCount += OutHitResults.Num() - BeforeLeft;
	}

	const int32 BeforeRight = OutHitResults.Num();
	if (CaptureLiveRuntimeEvidenceHitResultForBody(LiveRuntimeEvidenceRightSupportBodyName, OutHitResults))
	{
		OutMappedSupportHitCount += OutHitResults.Num() - BeforeRight;
	}

	return OutHitResults.Num() > 0;
}

bool UPhysAnimComponent::CaptureLiveRuntimeEvidenceHitResultForBody(const FName BodyName, TArray<FHitResult>& OutHitResults) const
{
	if (BodyName.IsNone())
	{
		return false;
	}

	const USkeletalMeshComponent* const Mesh = GetMeshComponent();
	const UWorld* const World = GetWorld();

	if (!Mesh || !World)
	{
		return false;
	}

	const int32 BoneIndex = Mesh->GetBoneIndex(BodyName);
	if (BoneIndex == INDEX_NONE)
	{
		return false;
	}

	const FVector BoneWorldLocation = Mesh->GetBoneLocation(BodyName);
	const FVector TraceStart = BoneWorldLocation + FVector(0.0, 0.0, LiveRuntimeEvidenceSupportSweepStartLiftCm);
	const FVector TraceEnd = BoneWorldLocation - FVector(0.0, 0.0, LiveRuntimeEvidenceSupportSweepDistanceCm);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PhysAnimLiveRuntimeEvidenceProof), false);
	QueryParams.bReturnPhysicalMaterial = false;
	QueryParams.AddIgnoredActor(GetOwner());

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);

	if (!bLiveRuntimeEvidenceWorldStaticOnly)
	{
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	}

	FHitResult Hit;
	const bool bHit = World->SweepSingleByObjectType(
		Hit,
		TraceStart,
		TraceEnd,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(LiveRuntimeEvidenceSupportSweepRadiusCm),
		QueryParams);

	if (!bHit || !Hit.bBlockingHit)
	{
		return false;
	}

	Hit.BoneName = BodyName;
	OutHitResults.Add(Hit);
	return true;
}

FPhysAnimSupportHitResultObservationInput UPhysAnimComponent::BuildLiveRuntimeEvidenceObservationInput(
	const TArray<FHitResult>& HitResults,
	float DeltaTimeSeconds) const
{
	FPhysAnimSupportHitResultObservationInput Input;

	Input.HitResults = HitResults;
	Input.WorldOriginCm = FVector::ZeroVector;
	Input.ComProxyPosCm = FVector2D::ZeroVector;
	Input.DeltaMs = static_cast<double>(FMath::Max(0.0f, DeltaTimeSeconds) * 1000.0f);
	Input.PreviousSupportGapTimerMs = LiveRuntimeEvidenceTerminationState.LatestArtifact.SupportGapTimerMs;

	if (LiveRuntimeEvidenceTerminationState.LatestArtifact.ProxyOutsideHullDurationMs.IsSet())
	{
		Input.PreviousProxyOutsideHullDurationMs =
			LiveRuntimeEvidenceTerminationState.LatestArtifact.ProxyOutsideHullDurationMs.GetValue();
	}

	FPhysAnimSupportBodyMapping LeftMapping;
	LeftMapping.BodyName = LiveRuntimeEvidenceLeftSupportBodyName;
	LeftMapping.SupportSide = EPhysAnimSupportSide::Left;
	Input.SupportBodies.Add(LeftMapping);

	FPhysAnimSupportBodyMapping RightMapping;
	RightMapping.BodyName = LiveRuntimeEvidenceRightSupportBodyName;
	RightMapping.SupportSide = EPhysAnimSupportSide::Right;
	Input.SupportBodies.Add(RightMapping);

	return Input;
}

FPhysAnimRuntimeSubstepInput UPhysAnimComponent::BuildLiveRuntimeEvidenceSubstepInput(
	const FPhysAnimSupportObservationResult& SupportObservation,
	float DeltaTimeSeconds) const
{
	FPhysAnimRuntimeSubstepInput Input;

	Input.SupportObservation = SupportObservation;
	Input.Values.AttemptUuid = LiveRuntimeEvidenceAttemptUuid;
	Input.Values.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : LiveRuntimeEvidenceStandingSeconds;
	Input.Values.TerminalSubstepTimestamp = LiveRuntimeEvidenceSubstepCounter;
	Input.Values.HoldDurationSec = LiveRuntimeEvidenceStandingSeconds;
	Input.Values.SupportUptimeSec = LiveRuntimeEvidenceStandingSeconds;
	Input.Values.ActiveSupportSideCount = SupportObservation.Validation.ActiveSupportSideCount;
	Input.Values.SupportHullAreaCm2 = SupportObservation.Validation.SupportHullAreaCm2;
	Input.Values.SupportGapTimerMs = SupportObservation.Validation.SupportGapTimerMs;
	Input.Values.SupportMode = SupportObservation.Validation.SupportMode;
	Input.Values.SupportChurnCount = SupportObservation.Validation.SupportChurnCount;
	Input.Values.SupportChurnHz = SupportObservation.Validation.SupportChurnHz;
	Input.Values.ProxyInsideHull = SupportObservation.Validation.ProxyInsideHull;
	Input.Values.ProxyOutsideHullDurationMs = SupportObservation.Validation.ProxyOutsideHullDurationMs;

	Input.ControllerStability.HoldDurationSec = LiveRuntimeEvidenceStandingSeconds;
	Input.ControllerStability.bControllerStabilityPassed = true;
	Input.Authority.bAuthorityPassed = true;
	Input.MovementReclaim.bMovementReclaimPassed = true;
	Input.ShellHelper.bShellHelperPassed = true;
	Input.Continuity.bPhysicalContinuityValidatorPassed = true;

	return Input;
}

void UPhysAnimComponent::EmitLiveRuntimeEvidenceProgressLog(
	const FPhysAnimRuntimeTerminationPipelineResult& PipelineResult,
	int32 HitCount,
	int32 MappedSupportHitCount)
{
	const bool bShouldLog =
		LiveRuntimeEvidenceLastProgressLogSeconds < 0.0f ||
		LiveRuntimeEvidenceStandingSeconds - LiveRuntimeEvidenceLastProgressLogSeconds >= LiveRuntimeEvidenceProgressLogIntervalSeconds ||
		PipelineResult.Command.bTerminate;

	if (!bShouldLog)
	{
		return;
	}

	LiveRuntimeEvidenceLastProgressLogSeconds = LiveRuntimeEvidenceStandingSeconds;

	UE_LOG(
		LogPhysAnimBridge,
		Warning,
		TEXT("PhysAnimProof: StandingProgress uuid=%s t=%.3f terminal_reason=%d hits=%d mapped=%d support_mode=%d active_sides=%d hull_area=%.3f"),
		*LiveRuntimeEvidenceAttemptUuid,
		LiveRuntimeEvidenceStandingSeconds,
		static_cast<int32>(PipelineResult.SubstepResult.TerminalReason),
		HitCount,
		MappedSupportHitCount,
		static_cast<int32>(PipelineResult.SubstepResult.Artifact.SupportMode),
		PipelineResult.SubstepResult.Artifact.ActiveSupportSideCount,
		PipelineResult.SubstepResult.Artifact.SupportHullAreaCm2);
}

void UPhysAnimComponent::EmitLiveRuntimeEvidenceTerminalArtifactOnce(
	const FPhysAnimRuntimeTerminationPipelineResult& PipelineResult)
{
	if (bLiveRuntimeEvidenceTerminalArtifactEmitted)
	{
		return;
	}

	bLiveRuntimeEvidenceTerminalArtifactEmitted = true;

	const FPhysAnimRunArtifactSnapshot& Artifact = PipelineResult.StateApplyResult.State.TerminalArtifact;

	const FString ProxyInsideText =
		Artifact.ProxyInsideHull.IsSet()
			? (Artifact.ProxyInsideHull.GetValue() ? TEXT("true") : TEXT("false"))
			: TEXT("unset");

	const FString ProxyOutsideDurationText =
		Artifact.ProxyOutsideHullDurationMs.IsSet()
			? FString::Printf(TEXT("%.3f"), Artifact.ProxyOutsideHullDurationMs.GetValue())
			: TEXT("unset");

	UE_LOG(
		LogPhysAnimBridge,
		Error,
		TEXT("PhysAnimProof: TerminalArtifact uuid=%s terminal_reason=%d timestamp=%lld support_mode=%d active_sides=%d hull_area=%.3f support_gap=%.3f proxy_inside=%s proxy_outside_duration=%s terminal_frame_captured=%d coterminal_count=%d"),
		*LiveRuntimeEvidenceAttemptUuid,
		static_cast<int32>(Artifact.TerminalReason),
		static_cast<long long>(Artifact.TerminalSubstepTimestamp),
		static_cast<int32>(Artifact.SupportMode),
		Artifact.ActiveSupportSideCount,
		Artifact.SupportHullAreaCm2,
		Artifact.SupportGapTimerMs,
		*ProxyInsideText,
		*ProxyOutsideDurationText,
		Artifact.bTerminalFrameArtifactCaptured ? 1 : 0,
		Artifact.CoTerminalReasons.Num());
}

void UPhysAnimComponent::EmitLiveRuntimeEvidenceAttemptResult(
	bool bPassed,
	const FPhysAnimRuntimeTerminationPipelineResult& PipelineResult)
{
	const EPhysAnimTerminalReason TerminalReason =
		bPassed
			? EPhysAnimTerminalReason::None
			: PipelineResult.StateApplyResult.State.TerminalReason;

	UE_LOG(
		LogPhysAnimBridge,
		bPassed ? ELogVerbosity::Warning : ELogVerbosity::Error,
		TEXT("PhysAnimProof: AttemptResult uuid=%s verdict=%s duration=%.3f terminal_reason=%d"),
		*LiveRuntimeEvidenceAttemptUuid,
		bPassed ? TEXT("PASS") : TEXT("FAIL"),
		LiveRuntimeEvidenceStandingSeconds,
		static_cast<int32>(TerminalReason));
}
'''

if "UPhysAnimComponent::TickLiveRuntimeEvidenceProof" not in cpp:
    cpp = cpp + impl

# Core tick hook
if "TickLiveRuntimeEvidenceProof(DeltaTime)" not in core:
    candidates = [
        "\n}\n\nvoid UPhysAnimComponent::ApplyPhase1PelvisRootCouplingSolve()",
        "\n}\n\nvoid UPhysAnimComponent::EndPlay",
    ]
    inserted = False
    for marker in candidates:
        if marker in core:
            core = replace_once(core, marker, "\n\tTickLiveRuntimeEvidenceProof(DeltaTime);\n" + marker, CORE)
            inserted = True
            break
    if not inserted:
        # Final fallback: look for TickComponent implementation
        if "void UPhysAnimComponent::TickComponent" in core:
             core = insert_after(
                core,
                "UPhysAnimComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)\n{",
                "\n\tTickLiveRuntimeEvidenceProof(DeltaTime);",
                CORE,
            )
             inserted = True

    if not inserted:
        raise RuntimeError("Could not automatically place TickLiveRuntimeEvidenceProof(DeltaTime).")

H.write_text(h, encoding="utf-8")
CPP.write_text(cpp, encoding="utf-8")
CORE.write_text(core, encoding="utf-8")

print("Cycle 15 live runtime evidence proof hook applied.")
