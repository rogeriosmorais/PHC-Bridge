#include "PhysAnimComponent.h"
#include "PhysAnimBridge.h"

#include "Components/CapsuleComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "PhysicsControlComponent.h"
#include "PhysicsControlData.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FStandingPlantSpeedExtrema
	{
		void Observe(
			FName BodyName,
			double LinearSpeedCmPerSec,
			double AngularSpeedDegPerSec,
			double MassKg,
			const FVector& InertiaTensorKgCmSq)
		{
			const double MinInertiaKgCmSq = FMath::Min3(
				static_cast<double>(InertiaTensorKgCmSq.X),
				static_cast<double>(InertiaTensorKgCmSq.Y),
				static_cast<double>(InertiaTensorKgCmSq.Z));
			if (LinearSpeedCmPerSec > MaxLinearSpeedCmPerSec)
			{
				MaxLinearBody = BodyName;
				MaxLinearSpeedCmPerSec = LinearSpeedCmPerSec;
				MaxLinearBodyMassKg = MassKg;
				MaxLinearBodyMinInertiaKgCmSq = MinInertiaKgCmSq;
			}
			if (AngularSpeedDegPerSec > MaxAngularSpeedDegPerSec)
			{
				MaxAngularBody = BodyName;
				MaxAngularSpeedDegPerSec = AngularSpeedDegPerSec;
				MaxAngularBodyMassKg = MassKg;
				MaxAngularBodyMinInertiaKgCmSq = MinInertiaKgCmSq;
			}
		}

		FName MaxLinearBody = NAME_None;
		double MaxLinearSpeedCmPerSec = 0.0;
		double MaxLinearBodyMassKg = 0.0;
		double MaxLinearBodyMinInertiaKgCmSq = 0.0;
		FName MaxAngularBody = NAME_None;
		double MaxAngularSpeedDegPerSec = 0.0;
		double MaxAngularBodyMassKg = 0.0;
		double MaxAngularBodyMinInertiaKgCmSq = 0.0;
	};

	struct FStandingPlantPoseErrorSummary
	{
		void Observe(FName BoneName, double ErrorDegrees, bool bLowerLimb)
		{
			SumSquares += ErrorDegrees * ErrorDegrees;
			++Count;
			if (bLowerLimb)
			{
				LowerLimbSumSquares += ErrorDegrees * ErrorDegrees;
				++LowerLimbCount;
			}
			if (ErrorDegrees > MaxErrorDegrees)
			{
				MaxErrorDegrees = ErrorDegrees;
				MaxErrorBone = BoneName;
			}
		}

		double RmsDegrees() const
		{
			return Count > 0 ? FMath::Sqrt(SumSquares / static_cast<double>(Count)) : 180.0;
		}

		double LowerLimbRmsDegrees() const
		{
			return LowerLimbCount > 0
				? FMath::Sqrt(LowerLimbSumSquares / static_cast<double>(LowerLimbCount))
				: 180.0;
		}

		double SumSquares = 0.0;
		double LowerLimbSumSquares = 0.0;
		double MaxErrorDegrees = 0.0;
		int32 Count = 0;
		int32 LowerLimbCount = 0;
		FName MaxErrorBone = NAME_None;
	};

	struct FStandingPlantMassExtrema
	{
		void Observe(FName BodyName, double MassKg, const FVector& InertiaTensorKgCmSq)
		{
			if (MassKg < MinMassKg)
			{
				MinMassBody = BodyName;
				MinMassKg = MassKg;
			}
			const double MinInertia = FMath::Min3(
				static_cast<double>(InertiaTensorKgCmSq.X),
				static_cast<double>(InertiaTensorKgCmSq.Y),
				static_cast<double>(InertiaTensorKgCmSq.Z));
			if (MinInertia < MinInertiaKgCmSq)
			{
				MinInertiaBody = BodyName;
				MinInertiaKgCmSq = MinInertia;
			}
		}

		FName MinMassBody = NAME_None;
		double MinMassKg = TNumericLimits<double>::Max();
		FName MinInertiaBody = NAME_None;
		double MinInertiaKgCmSq = TNumericLimits<double>::Max();
	};

	TOptional<double> ResolveCausalStandingSampleTime(double ActualTimeSeconds, double CaptureWindowSeconds);
	double GetCausalStandingFixedDeltaTimeSeconds();
	double AdvanceCausalStandingSupportGapMs(double CurrentGapMs, bool bHasSupportContact, double DeltaTimeSeconds);
	TArray<TSharedPtr<FJsonValue>> BuildPolicyActionJsonArray(const TArray<float>& Actions)
	{
		TArray<TSharedPtr<FJsonValue>> JsonActions;
		JsonActions.Reserve(Actions.Num());
		for (const float Action : Actions)
		{
			JsonActions.Add(MakeShared<FJsonValueNumber>(static_cast<double>(Action)));
		}
		return JsonActions;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimProductHarnessDropDispatchSwitchTest,
	"PhysAnim.ProductHarness.DropDispatchSwitch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimProductHarnessDropDispatchSwitchTest::RunTest(const FString& Parameters)
{
	FStandingPlantSpeedExtrema Extrema;
	Extrema.Observe(TEXT("pelvis"), 10.0, 20.0, 10.0, FVector(20.0, 30.0, 40.0));
	Extrema.Observe(TEXT("calf_l"), 15.0, 5.0, 5.0, FVector(10.0, 15.0, 20.0));
	Extrema.Observe(TEXT("foot_l"), 15.0, 30.0, 2.0, FVector(1.0, 2.0, 3.0));
	TestEqual(TEXT("Linear speed diagnostic keeps first maximum body"), Extrema.MaxLinearBody, FName(TEXT("calf_l")));
	TestEqual(TEXT("Linear speed diagnostic records the maximum"), Extrema.MaxLinearSpeedCmPerSec, 15.0);
	TestEqual(TEXT("Linear speed diagnostic records the matching body mass"), Extrema.MaxLinearBodyMassKg, 5.0);
	TestEqual(TEXT("Linear speed diagnostic records the matching minimum inertia"), Extrema.MaxLinearBodyMinInertiaKgCmSq, 10.0);
	TestEqual(TEXT("Angular speed diagnostic records its body independently"), Extrema.MaxAngularBody, FName(TEXT("foot_l")));
	TestEqual(TEXT("Angular speed diagnostic records the maximum"), Extrema.MaxAngularSpeedDegPerSec, 30.0);
	TestEqual(TEXT("Angular speed diagnostic records the matching body mass"), Extrema.MaxAngularBodyMassKg, 2.0);
	TestEqual(TEXT("Angular speed diagnostic records the matching minimum inertia"), Extrema.MaxAngularBodyMinInertiaKgCmSq, 1.0);
	FStandingPlantPoseErrorSummary PoseErrors;
	PoseErrors.Observe(TEXT("spine_01"), 3.0, false);
	PoseErrors.Observe(TEXT("thigh_l"), 4.0, true);
	TestEqual(TEXT("Pose error diagnostic records the maximum body"), PoseErrors.MaxErrorBone, FName(TEXT("thigh_l")));
	TestEqual(TEXT("Pose error diagnostic records whole-body RMS"), PoseErrors.RmsDegrees(), FMath::Sqrt(12.5));
	TestEqual(TEXT("Pose error diagnostic records lower-limb RMS"), PoseErrors.LowerLimbRmsDegrees(), 4.0);
	FStandingPlantMassExtrema MassExtrema;
	MassExtrema.Observe(TEXT("pelvis"), 10.0, FVector(20.0, 30.0, 40.0));
	MassExtrema.Observe(TEXT("ball_r"), 0.1, FVector(0.5, 0.25, 0.75));
	TestEqual(TEXT("Mass diagnostic records the lightest body"), MassExtrema.MinMassBody, FName(TEXT("ball_r")));
	TestEqual(TEXT("Mass diagnostic records the minimum mass"), MassExtrema.MinMassKg, 0.1);
	TestEqual(TEXT("Inertia diagnostic records the lowest-inertia body"), MassExtrema.MinInertiaBody, FName(TEXT("ball_r")));
	TestEqual(TEXT("Inertia diagnostic records the minimum tensor component"), MassExtrema.MinInertiaKgCmSq, 0.25);

	TArray<float> ZeroActions;
	ZeroActions.Init(0.0f, 69);
	const TArray<TSharedPtr<FJsonValue>> ZeroActionJson = BuildPolicyActionJsonArray(ZeroActions);
	TestEqual(TEXT("Policy evidence preserves the 69-action width"), ZeroActionJson.Num(), 69);
	for (int32 ActionIndex = 0; ActionIndex < ZeroActionJson.Num(); ++ActionIndex)
	{
		TestEqual(
			FString::Printf(TEXT("Zero policy action %d remains exactly zero"), ActionIndex),
			ZeroActionJson[ActionIndex]->AsNumber(),
			0.0);
	}

	TArray<float> IndexedActions;
	IndexedActions.Init(0.0f, 69);
	IndexedActions[0] = -0.25f;
	IndexedActions[34] = 0.5f;
	IndexedActions[68] = 0.75f;
	const TArray<TSharedPtr<FJsonValue>> IndexedActionJson = BuildPolicyActionJsonArray(IndexedActions);
	TestEqual(TEXT("Policy evidence preserves action index 0"), IndexedActionJson[0]->AsNumber(), -0.25);
	TestEqual(TEXT("Policy evidence preserves action index 34"), IndexedActionJson[34]->AsNumber(), 0.5);
	TestEqual(TEXT("Policy evidence preserves action index 68"), IndexedActionJson[68]->AsNumber(), 0.75);

	UPhysAnimComponent* const Component = NewObject<UPhysAnimComponent>();
	TestNotNull(TEXT("Transient product harness component"), Component);
	if (!Component)
	{
		return false;
	}
	const FPhysAnimStabilizationSettings DefaultSettings;
	TestEqual(TEXT("Standing gain ramp preserves the seeded plant"), DefaultSettings.StartupRampSeconds, 0.25f);
	TestEqual(TEXT("Standing publishes the configured eight-hertz control authority"), DefaultSettings.AngularStrengthMultiplier, 1.0f);
	TestTrue(
		TEXT("Causal standing uses a deterministic sixty-hertz runtime step"),
		FMath::IsNearlyEqual(GetCausalStandingFixedDeltaTimeSeconds(), 1.0 / 60.0, 1.0e-9));
	TestEqual(
		TEXT("An in-window raw sample keeps its observed time"),
		ResolveCausalStandingSampleTime(0.495, 0.5).GetValue(),
		0.495);
	TestEqual(
		TEXT("Floating-point endpoint noise is normalized to the locked window"),
		ResolveCausalStandingSampleTime(0.50000001, 0.5).GetValue(),
		0.5);
	TestFalse(
		TEXT("A post-window runtime observation cannot be relabeled as endpoint evidence"),
		ResolveCausalStandingSampleTime(0.89, 0.5).IsSet());
	TestTrue(
		TEXT("Missing support advances the product support-gap timer at the raw fixed step"),
		FMath::IsNearlyEqual(
			AdvanceCausalStandingSupportGapMs(50.0, false, 1.0 / 60.0),
			50.0 + 1000.0 / 60.0,
			1.0e-9));
	TestEqual(
		TEXT("Observed support resets the product support-gap timer"),
		AdvanceCausalStandingSupportGapMs(50.0, true, 1.0 / 60.0),
		0.0);

	TestFalse(TEXT("Dispatch fault is disabled by default"), Component->IsProductControlDispatchDroppedForTesting());
	Component->SetProductControlDispatchDroppedForTesting(true);
	TestTrue(TEXT("Dispatch fault can be enabled for a destructive control"), Component->IsProductControlDispatchDroppedForTesting());
	Component->SetProductControlDispatchDroppedForTesting(false);
	TestFalse(TEXT("Dispatch fault can be restored"), Component->IsProductControlDispatchDroppedForTesting());
	TestEqual(TEXT("Standing variant defaults to Normal"), Component->GetStandingVariantForTesting(), EPhysAnimStandingVariant::Normal);
	Component->SetStandingVariantForTesting(EPhysAnimStandingVariant::DampingOnly);
	TestEqual(TEXT("Plant harness can select damping-only"), Component->GetStandingVariantForTesting(), EPhysAnimStandingVariant::DampingOnly);
	Component->SetStandingVariantForTesting(EPhysAnimStandingVariant::Normal);
	Component->ApplyProductVariantFromCommandLineForTesting(TEXT("-PhysAnimProductVariant=ControlsOff"));
	TestEqual(TEXT("Command line selects controls-off before startup"), Component->GetStandingVariantForTesting(), EPhysAnimStandingVariant::ControlsOff);
	TestFalse(TEXT("Controls-off does not drop the already-disabled dispatch path"), Component->IsProductControlDispatchDroppedForTesting());
	TestFalse(TEXT("Plant-only variants do not start product evidence capture"), Component->bEnableLiveRuntimeEvidenceProof);
	Component->ApplyProductVariantFromCommandLineForTesting(
		TEXT("-ExecCmds=\"Automation RunTests PhysAnim.Development.StandingPlant.ZeroActions\" -PhysAnimProductVariant=ZeroActions"));
	TestFalse(TEXT("The plant ZeroActions layer remains outside product evidence capture"), Component->bEnableLiveRuntimeEvidenceProof);
	Component->ApplyProductVariantFromCommandLineForTesting(TEXT("-PhysAnimProductVariant=DropControlDispatch"));
	TestEqual(TEXT("Command line selects dropped-dispatch activation"), Component->GetStandingVariantForTesting(), EPhysAnimStandingVariant::DropControlDispatch);
	TestTrue(TEXT("Dropped-dispatch command line arms the destructive control"), Component->IsProductControlDispatchDroppedForTesting());
	TestFalse(TEXT("Product capture remains independent of the legacy proof lifecycle"), Component->bEnableLiveRuntimeEvidenceProof);
	Component->ApplyProductVariantFromCommandLineForTesting(TEXT("-PhysAnimProductVariant=Normal"));
	TestEqual(TEXT("Normal command line restores normal activation"), Component->GetStandingVariantForTesting(), EPhysAnimStandingVariant::Normal);
	TestFalse(TEXT("Normal command line clears dropped dispatch"), Component->IsProductControlDispatchDroppedForTesting());
	TestFalse(
		TEXT("Action semantic tracing is disabled without its explicit development flag"),
		Component->IsActionSemanticTraceEnabledForTesting());
	Component->ApplyProductVariantFromCommandLineForTesting(
		TEXT("-PhysAnimProductVariant=RealOnnxPolicy -PhysAnimActionSemanticTrace"));
	TestTrue(
		TEXT("The explicit development flag enables action semantic tracing"),
		Component->IsActionSemanticTraceEnabledForTesting());
	Component->ApplyProductVariantFromCommandLineForTesting(TEXT("-PhysAnimProductVariant=RealOnnxPolicy"));
	TestFalse(
		TEXT("Removing the development flag restores the trace-off default"),
		Component->IsActionSemanticTraceEnabledForTesting());
	TestFalse(
		TEXT("Policy-input provenance tracing is disabled without its explicit development flag"),
		Component->IsPolicyInputProvenanceTraceEnabledForTesting());
	Component->ApplyProductVariantFromCommandLineForTesting(
		TEXT("-PhysAnimProductVariant=RealOnnxPolicy -PhysAnimPolicyInputProvenanceTrace"));
	TestTrue(
		TEXT("The explicit development flag enables policy-input provenance tracing"),
		Component->IsPolicyInputProvenanceTraceEnabledForTesting());
	Component->ApplyProductVariantFromCommandLineForTesting(TEXT("-PhysAnimProductVariant=RealOnnxPolicy"));
	TestFalse(
		TEXT("Removing the development flag restores the provenance trace-off default"),
		Component->IsPolicyInputProvenanceTraceEnabledForTesting());
	TestEqual(
		TEXT("First-policy delay defaults to zero ticks"),
		Component->GetExperimentalFirstPolicyDelayTicksForTesting(),
		0);
	Component->ApplyProductVariantFromCommandLineForTesting(
		TEXT("-PhysAnimProductVariant=RealOnnxPolicy -PhysAnimExperimentalFirstPolicyDelayTicks=3"));
	TestEqual(
		TEXT("An in-range first-policy delay is owned by the development command line"),
		Component->GetExperimentalFirstPolicyDelayTicksForTesting(),
		3);
	TestEqual(
		TEXT("Parsing a delay resets its consumed-tick diagnostic"),
		Component->GetExperimentalFirstPolicyDelayTicksConsumedForTesting(),
		0);
	Component->ApplyProductVariantFromCommandLineForTesting(
		TEXT("-PhysAnimProductVariant=RealOnnxPolicy -PhysAnimExperimentalFirstPolicyDelayTicks=-1"));
	TestEqual(
		TEXT("Negative first-policy delays are rejected to the zero-delay default"),
		Component->GetExperimentalFirstPolicyDelayTicksForTesting(),
		0);
	const FString AboveRangeDelay = FString::Printf(
		TEXT("-PhysAnimProductVariant=RealOnnxPolicy -PhysAnimExperimentalFirstPolicyDelayTicks=%d"),
		UPhysAnimComponent::GetMaximumExperimentalFirstPolicyDelayTicksForTesting() + 1);
	Component->ApplyProductVariantFromCommandLineForTesting(*AboveRangeDelay);
	TestEqual(
		TEXT("Above-range first-policy delays are rejected to the zero-delay default"),
		Component->GetExperimentalFirstPolicyDelayTicksForTesting(),
		0);
	TestFalse(
		TEXT("Constraint-range remap bypass is disabled without its explicit development flag"),
		Component->IsConstraintRangeRemapBypassEnabledForTesting());
	Component->ApplyProductVariantFromCommandLineForTesting(
		TEXT("-PhysAnimProductVariant=RealOnnxPolicy -PhysAnimExperimentalConstraintRangeRemapBypass"));
	TestTrue(
		TEXT("The explicit development flag enables the constraint-range remap experiment"),
		Component->IsConstraintRangeRemapBypassEnabledForTesting());
	TestTrue(
		TEXT("The remap bypass is owned only by active-standing RealOnnxPolicy"),
		Component->ShouldBypassConstraintRangeRemapForTesting(
			EPhysAnimRuntimeState::BalanceActive_Standing,
			EPhysAnimStandingVariant::RealOnnxPolicy));
	TestFalse(
		TEXT("The remap bypass cannot alter standing preparation"),
		Component->ShouldBypassConstraintRangeRemapForTesting(
			EPhysAnimRuntimeState::Standing_Preparation,
			EPhysAnimStandingVariant::RealOnnxPolicy));
	TestFalse(
		TEXT("The remap bypass cannot alter a non-policy plant variant"),
		Component->ShouldBypassConstraintRangeRemapForTesting(
			EPhysAnimRuntimeState::BalanceActive_Standing,
			EPhysAnimStandingVariant::Normal));
	Component->ApplyProductVariantFromCommandLineForTesting(TEXT("-PhysAnimProductVariant=RealOnnxPolicy"));
	TestFalse(
		TEXT("Removing the development flag restores the remap-on default"),
		Component->IsConstraintRangeRemapBypassEnabledForTesting());
	float NeutralCalibratedTiltDeg = 0.0f;
	TestFalse(
		TEXT("Neutral-calibrated tilt requires a live owner, mesh, pelvis body, and startup calibration"),
		Component->TryMeasureNeutralCalibratedPelvisTiltDegrees(NeutralCalibratedTiltDeg));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimStandingShellIndependenceDefaultsTest,
	"PhysAnim.ProductHarness.StandingShellIndependenceDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimStandingShellIndependenceDefaultsTest::RunTest(const FString& Parameters)
{
	const FPhysAnimStabilizationSettings Settings;
	TestFalse(TEXT("Standing does not restore CharacterMovement after startup"), Settings.bRestoreCharacterMovementAfterStartupReady);
	TestFalse(TEXT("Standing does not translate the gameplay shell through a bridge helper"), Settings.bEnableBridgeOwnedMovementWhileCharacterMovementLocked);
	TestFalse(TEXT("Standing does not preserve the gameplay shell by default"), UPhysAnimComponent::ShouldPreserveGameplayShellDuringBridgeActive(false, false));
	TestTrue(TEXT("The explicit movement smoke can opt into the gameplay shell"), UPhysAnimComponent::ShouldPreserveGameplayShellDuringBridgeActive(true, false));

	UCharacterMovementComponent* Movement = NewObject<UCharacterMovementComponent>();
	Movement->Activate(true);
	Movement->SetComponentTickEnabled(true);
	UPhysAnimComponent::ApplyCharacterMovementBridgeOwnership(Movement, false);
	TestFalse(TEXT("Bridge ownership deactivates CharacterMovement"), Movement->IsActive());
	TestFalse(TEXT("Bridge ownership disables CharacterMovement ticking"), Movement->IsComponentTickEnabled());
	UPhysAnimComponent::ApplyCharacterMovementBridgeOwnership(Movement, true);
	TestTrue(TEXT("Explicit gameplay-shell ownership reactivates CharacterMovement"), Movement->IsActive());
	return true;
}

namespace
{
	constexpr double StartupTimeoutSeconds = 20.0;
	constexpr double StandingWindowSeconds = 10.0;
	constexpr double PerturbationTimeSeconds = 2.0;
	constexpr float PerturbationDeltaVCmPerSecond = 15.0f;
	constexpr float MannyReferencePelvisHeightCm = PhysAnimBridge::MannyRootHeightMeters * 100.0f;
	constexpr double CausalStandingFixedDeltaTimeSeconds = 1.0 / 60.0;
	constexpr double CaptureEndpointToleranceSeconds = 1.0e-6;

	TOptional<double> ResolveCausalStandingSampleTime(
		double ActualTimeSeconds,
		double CaptureWindowSeconds)
	{
		if (ActualTimeSeconds > CaptureWindowSeconds + CaptureEndpointToleranceSeconds)
		{
			return {};
		}
		return FMath::Min(ActualTimeSeconds, CaptureWindowSeconds);
	}

	double GetCausalStandingFixedDeltaTimeSeconds()
	{
		return CausalStandingFixedDeltaTimeSeconds;
	}

	double AdvanceCausalStandingSupportGapMs(
		double CurrentGapMs,
		bool bHasSupportContact,
		double DeltaTimeSeconds)
	{
		return bHasSupportContact
			? 0.0
			: CurrentGapMs + FMath::Max(0.0, DeltaTimeSeconds) * 1000.0;
	}

	struct FCausalStandingRunConfig
	{
		FString RunRoot;
		FString RunId;
		FString Variant;
		FString SourceCommit;
		FString ModelOnnxSha256;
		int32 Repetition = 0;
		bool bSourceTreeDirty = false;
		bool bPlantRun = false;
		double CaptureWindowSeconds = StandingWindowSeconds;
		bool bApplyPerturbation = true;
		FString ProtocolRelativePath = TEXT("../product-gates/causal-standing.v1.json");
		FString FixtureAuthority = TEXT("PRODUCT_RUN");
		FString RunSchemaVersion = TEXT("physanim-product-run/v1");
		EPhysAnimStandingVariant StandingVariant = EPhysAnimStandingVariant::Normal;

		bool ReadFromCommandLine(FString& OutError)
		{
			const TCHAR* CommandLine = FCommandLine::Get();
			int32 SourceTreeDirty = 0;
			FParse::Value(CommandLine, TEXT("PhysAnimProductRunRoot="), RunRoot);
			FParse::Value(CommandLine, TEXT("PhysAnimProductRunId="), RunId);
			FParse::Value(CommandLine, TEXT("PhysAnimProductVariant="), Variant);
			FParse::Value(CommandLine, TEXT("PhysAnimProductRepetition="), Repetition);
			FParse::Value(CommandLine, TEXT("PhysAnimSourceCommit="), SourceCommit);
			FParse::Value(CommandLine, TEXT("PhysAnimModelOnnxSha256="), ModelOnnxSha256);
			FParse::Value(CommandLine, TEXT("PhysAnimSourceTreeDirty="), SourceTreeDirty);
			bSourceTreeDirty = SourceTreeDirty != 0;

			if (RunRoot.IsEmpty() || RunId.IsEmpty() || Variant.IsEmpty() || Repetition < 1 ||
				SourceCommit.Len() != 40 || ModelOnnxSha256.Len() != 64)
			{
				OutError = TEXT("Product-run command line is missing required run identity fields");
				return false;
			}
			RunRoot = FPaths::ConvertRelativePathToFull(RunRoot);
			return true;
		}

		bool ConfigurePlantLayer(FString& OutError)
		{
			bPlantRun = true;
			bApplyPerturbation = false;
			ProtocolRelativePath = TEXT("../product-gates/standing-plant-ladder.v2.json");
			FixtureAuthority = TEXT("DEVELOPMENT_GATE_RUN");
			RunSchemaVersion = TEXT("physanim-development-run/v1");
			if (Variant == TEXT("ControlsOff"))
			{
				StandingVariant = EPhysAnimStandingVariant::ControlsOff;
				CaptureWindowSeconds = 0.25;
			}
			else if (Variant == TEXT("DampingOnly"))
			{
				StandingVariant = EPhysAnimStandingVariant::DampingOnly;
				CaptureWindowSeconds = 0.5;
			}
			else if (Variant == TEXT("FixedNeutralTarget"))
			{
				StandingVariant = EPhysAnimStandingVariant::FixedNeutralTarget;
				CaptureWindowSeconds = 10.0;
			}
			else if (Variant == TEXT("ZeroActions"))
			{
				StandingVariant = EPhysAnimStandingVariant::ZeroActions;
				CaptureWindowSeconds = 10.0;
			}
			else if (Variant == TEXT("RealOnnxPolicy"))
			{
				StandingVariant = EPhysAnimStandingVariant::RealOnnxPolicy;
				CaptureWindowSeconds = 10.0;
			}
			else
			{
				OutError = FString::Printf(TEXT("Unknown standing plant layer '%s'"), *Variant);
				return false;
			}
			return true;
		}
	};

	UWorld* FindProductWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				return Context.World();
			}
		}
		return nullptr;
	}

	UPhysAnimComponent* FindProductComponent(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		for (TActorIterator<ACharacter> It(World); It; ++It)
		{
			if (UPhysAnimComponent* Component = It->FindComponentByClass<UPhysAnimComponent>())
			{
				return Component;
			}
		}
		return nullptr;
	}

	FString RuntimeStateName(EPhysAnimRuntimeState State)
	{
		if (const UEnum* Enum = StaticEnum<EPhysAnimRuntimeState>())
		{
			return Enum->GetNameStringByValue(static_cast<int64>(State));
		}
		return TEXT("Unknown");
	}

	double L2Norm(const TArray<float>& Values)
	{
		double SumSquares = 0.0;
		for (const float Value : Values)
		{
			SumSquares += static_cast<double>(Value) * static_cast<double>(Value);
		}
		return FMath::Sqrt(SumSquares);
	}

	void ReadBodyMasks(USkeletalMeshComponent* Mesh, const TArray<FName>& Bones, int32& OutValidMask, int32& OutSimulatingMask)
	{
		OutValidMask = 0;
		OutSimulatingMask = 0;
		if (!Mesh)
		{
			return;
		}
		for (int32 Index = 0; Index < Bones.Num(); ++Index)
		{
			FBodyInstance* Body = Mesh->GetBodyInstance(Bones[Index]);
			if (Body && Body->IsValidBodyInstance())
			{
				OutValidMask |= 1 << Index;
				if (Body->IsInstanceSimulatingPhysics())
				{
					OutSimulatingMask |= 1 << Index;
				}
			}
		}
	}

	double MeasurePelvisHeight(UWorld* World, ACharacter* Character, FBodyInstance* PelvisBody)
	{
		if (!World || !Character || !PelvisBody || !PelvisBody->IsValidBodyInstance())
		{
			return 0.0;
		}
		const FVector PelvisLocation = PelvisBody->GetUnrealWorldTransform().GetLocation();
		FCollisionQueryParams Params(SCENE_QUERY_STAT(CausalStandingPelvisHeight), false, Character);
		const FCollisionObjectQueryParams Objects(FCollisionObjectQueryParams::InitType::AllStaticObjects);
		FHitResult Hit;
		if (!World->LineTraceSingleByObjectType(
			Hit,
			PelvisLocation + FVector(0.0f, 0.0f, 100.0f),
			PelvisLocation - FVector(0.0f, 0.0f, 500.0f),
			Objects,
			Params))
		{
			return 0.0;
		}
		return PelvisLocation.Z - Hit.ImpactPoint.Z;
	}

	double MeasureRootTilt(UPhysAnimComponent* Component)
	{
		float TiltDegrees = 180.0f;
		return Component && Component->TryMeasureNeutralCalibratedPelvisTiltDegrees(TiltDegrees)
			? static_cast<double>(TiltDegrees)
			: 180.0;
	}

	FStandingPlantPoseErrorSummary MeasurePoseErrors(
		USkeletalMeshComponent* Mesh,
		UPhysicsControlComponent* PhysicsControl,
		const TMap<FName, FQuat>& IntendedTargets)
	{
		FStandingPlantPoseErrorSummary Summary;
		if (!Mesh || !PhysicsControl || !Mesh->GetSkeletalMeshAsset())
		{
			return Summary;
		}
		const FReferenceSkeleton& ReferenceSkeleton = Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
		static const TSet<FName> LowerLimbBones = {
			TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"), TEXT("ball_l"),
			TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r"), TEXT("ball_r") };
		for (const TPair<FName, FQuat>& Pair : IntendedTargets)
		{
			const FName BoneName = PhysAnimBridge::GetBoneNameFromControlName(Pair.Key);
			FBodyInstance* ChildBody = Mesh->GetBodyInstance(BoneName);
			FPhysicsControlTarget Target;
			if (!ChildBody || !ChildBody->IsValidBodyInstance() || !PhysicsControl->GetControlTarget(Pair.Key, Target))
			{
				continue;
			}
			const int32 BoneIndex = ReferenceSkeleton.FindBoneIndex(BoneName);
			const int32 ParentIndex = BoneIndex != INDEX_NONE ? ReferenceSkeleton.GetParentIndex(BoneIndex) : INDEX_NONE;
			const FName ParentBoneName = ParentIndex != INDEX_NONE ? ReferenceSkeleton.GetBoneName(ParentIndex) : NAME_None;
			FBodyInstance* ParentBody = ParentBoneName.IsNone() ? nullptr : Mesh->GetBodyInstance(ParentBoneName);
			const FQuat ParentRotation = ParentBody && ParentBody->IsValidBodyInstance()
				? ParentBody->GetUnrealWorldTransform().GetRotation()
				: Mesh->GetComponentQuat();
			const FQuat ActualRelative = ParentRotation.Inverse() * ChildBody->GetUnrealWorldTransform().GetRotation();
			const double ErrorDegrees = FMath::RadiansToDegrees(ActualRelative.AngularDistance(Target.TargetOrientation.Quaternion()));
			Summary.Observe(BoneName, ErrorDegrees, LowerLimbBones.Contains(BoneName));
		}
		return Summary;
	}

	void MeasureTargetReadback(
		UPhysicsControlComponent* PhysicsControl,
		const TMap<FName, FQuat>& IntendedTargets,
		int32& OutMatches,
		double& OutMaxErrorDegrees)
	{
		OutMatches = 0;
		OutMaxErrorDegrees = 180.0;
		if (!PhysicsControl || IntendedTargets.IsEmpty())
		{
			return;
		}
		OutMaxErrorDegrees = 0.0;
		for (const TPair<FName, FQuat>& Pair : IntendedTargets)
		{
			FPhysicsControlTarget Target;
			if (!PhysicsControl->GetControlTarget(Pair.Key, Target))
			{
				OutMaxErrorDegrees = 180.0;
				continue;
			}
			const double ErrorDegrees = FMath::RadiansToDegrees(Pair.Value.AngularDistance(Target.TargetOrientation.Quaternion()));
			OutMaxErrorDegrees = FMath::Max(OutMaxErrorDegrees, ErrorDegrees);
			if (ErrorDegrees <= 0.5)
			{
				++OutMatches;
			}
		}
	}

	FString SerializeJson(const TSharedRef<FJsonObject>& Object)
	{
		FString Output;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output);
		FJsonSerializer::Serialize(Object, Writer);
		return Output;
	}

	TArray<TSharedPtr<FJsonValue>> BuildVectorJsonArray(const FVector& Value)
	{
		return {
			MakeShared<FJsonValueNumber>(Value.X),
			MakeShared<FJsonValueNumber>(Value.Y),
			MakeShared<FJsonValueNumber>(Value.Z) };
	}

	TArray<TSharedPtr<FJsonValue>> BuildQuaternionJsonArray(const FQuat& Value)
	{
		return {
			MakeShared<FJsonValueNumber>(Value.X),
			MakeShared<FJsonValueNumber>(Value.Y),
			MakeShared<FJsonValueNumber>(Value.Z),
			MakeShared<FJsonValueNumber>(Value.W) };
	}

	TArray<TSharedPtr<FJsonValue>> BuildIntegerJsonArray(const TArray<int32>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> Result;
		Result.Reserve(Values.Num());
		for (const int32 Value : Values)
		{
			Result.Add(MakeShared<FJsonValueNumber>(Value));
		}
		return Result;
	}

	TSharedRef<FJsonObject> BuildActionSemanticTraceJson(
		const PhysAnimBridge::FPhysAnimActionSemanticTrace& Trace,
		const bool bEnabled)
	{
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("schema_version"), TEXT("physanim-action-semantic-trace/v1"));
		Root->SetStringField(TEXT("authority"), TEXT("DEVELOPMENT_DIAGNOSTIC_ONLY"));
		Root->SetBoolField(TEXT("enabled"), bEnabled);
		Root->SetBoolField(TEXT("captured"), Trace.bCaptured);
		Root->SetStringField(TEXT("capture_scope"), Trace.CaptureScope);
		Root->SetStringField(TEXT("capture_error"), Trace.CaptureError);
		Root->SetNumberField(TEXT("policy_step_delta_time"), Trace.PolicyStepDeltaTime);
		Root->SetNumberField(TEXT("policy_influence_alpha"), Trace.PolicyInfluenceAlpha);
		Root->SetNumberField(TEXT("max_angular_step_deg"), Trace.MaxAngularStepDegrees);
		Root->SetBoolField(TEXT("constraint_adapter_enabled"), Trace.bConstraintAdapterEnabled);

		TArray<TSharedPtr<FJsonValue>> JointValues;
		JointValues.Reserve(Trace.ActionJoints.Num());
		for (const PhysAnimBridge::FPhysAnimActionJointSemanticTrace& Entry : Trace.ActionJoints)
		{
			const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
			Object->SetNumberField(TEXT("proto_joint_index"), Entry.ProtoJointIndex);
			Object->SetStringField(TEXT("proto_joint_name"), Entry.ProtoJointName.ToString());
			Object->SetStringField(TEXT("manny_bone_name"), Entry.MannyBoneName.ToString());
			Object->SetBoolField(TEXT("shares_mapped_control"), Entry.bSharesMappedControl);
			Object->SetArrayField(TEXT("raw_action"), BuildVectorJsonArray(Entry.RawAction));
			Object->SetArrayField(TEXT("conditioned_action"), BuildVectorJsonArray(Entry.ConditionedAction));
			Object->SetArrayField(TEXT("raw_decoded_rotation_ue_xyzw"), BuildQuaternionJsonArray(Entry.RawDecodedRotationUe));
			Object->SetArrayField(TEXT("conditioned_decoded_rotation_ue_xyzw"), BuildQuaternionJsonArray(Entry.ConditionedDecodedRotationUe));
			JointValues.Add(MakeShared<FJsonValueObject>(Object));
		}
		Root->SetArrayField(TEXT("action_joints"), JointValues);

		TArray<TSharedPtr<FJsonValue>> ControlValues;
		ControlValues.Reserve(Trace.ControlTargets.Num());
		for (const PhysAnimBridge::FPhysAnimControlTargetSemanticTrace& Entry : Trace.ControlTargets)
		{
			const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
			Object->SetStringField(TEXT("manny_bone_name"), Entry.MannyBoneName.ToString());
			Object->SetStringField(TEXT("control_name"), Entry.ControlName.ToString());
			Object->SetArrayField(TEXT("source_proto_joint_indices"), BuildIntegerJsonArray(Entry.SourceProtoJointIndices));
			Object->SetArrayField(TEXT("combined_decoded_rotation_ue_xyzw"), BuildQuaternionJsonArray(Entry.CombinedDecodedRotationUe));
			Object->SetArrayField(TEXT("manny_neutral_rotation_xyzw"), BuildQuaternionJsonArray(Entry.MannyNeutralRotation));
			Object->SetArrayField(TEXT("bind_composed_rotation_xyzw"), BuildQuaternionJsonArray(Entry.BindComposedRotation));
			Object->SetArrayField(TEXT("range_scaled_rotation_xyzw"), BuildQuaternionJsonArray(Entry.RangeScaledRotation));
			Object->SetArrayField(TEXT("distal_scaled_rotation_xyzw"), BuildQuaternionJsonArray(Entry.DistalScaledRotation));
			Object->SetArrayField(TEXT("constraint_range_mapped_rotation_xyzw"), BuildQuaternionJsonArray(Entry.ConstraintRangeMappedRotation));
			Object->SetArrayField(TEXT("constraint_adapted_rotation_xyzw"), BuildQuaternionJsonArray(Entry.ConstraintAdaptedRotation));
			Object->SetArrayField(TEXT("blended_rotation_xyzw"), BuildQuaternionJsonArray(Entry.BlendedRotation));
			Object->SetArrayField(TEXT("published_rotation_xyzw"), BuildQuaternionJsonArray(Entry.PublishedRotation));
			Object->SetArrayField(TEXT("readback_rotation_xyzw"), BuildQuaternionJsonArray(Entry.ReadbackRotation));
			Object->SetBoolField(TEXT("has_constraint_profile"), Entry.bHasConstraintProfile);
			Object->SetBoolField(TEXT("target_written"), Entry.bTargetWritten);
			Object->SetBoolField(TEXT("readback_succeeded"), Entry.bReadbackSucceeded);
			Object->SetNumberField(TEXT("twist_motion"), Entry.TwistMotion);
			Object->SetNumberField(TEXT("swing1_motion"), Entry.Swing1Motion);
			Object->SetNumberField(TEXT("swing2_motion"), Entry.Swing2Motion);
			Object->SetNumberField(TEXT("twist_limit_deg"), Entry.TwistLimitDegrees);
			Object->SetNumberField(TEXT("swing1_limit_deg"), Entry.Swing1LimitDegrees);
			Object->SetNumberField(TEXT("swing2_limit_deg"), Entry.Swing2LimitDegrees);
			Object->SetNumberField(TEXT("lower_limb_range_scale"), Entry.LowerLimbRangeScale);
			Object->SetNumberField(TEXT("distal_range_scale"), Entry.DistalRangeScale);
			Object->SetNumberField(TEXT("raw_policy_offset_deg"), Entry.RawPolicyOffsetDegrees);
			Object->SetNumberField(TEXT("range_scale_delta_deg"), Entry.RangeScaleDeltaDegrees);
			Object->SetNumberField(TEXT("distal_scale_delta_deg"), Entry.DistalScaleDeltaDegrees);
			Object->SetNumberField(TEXT("constraint_range_mapping_delta_deg"), Entry.ConstraintRangeMappingDeltaDegrees);
			Object->SetNumberField(TEXT("constraint_projection_delta_deg"), Entry.ConstraintProjectionDeltaDegrees);
			Object->SetNumberField(TEXT("adapted_to_published_delta_deg"), Entry.AdaptedToPublishedDeltaDegrees);
			Object->SetNumberField(TEXT("readback_error_deg"), Entry.ReadbackErrorDegrees);
			ControlValues.Add(MakeShared<FJsonValueObject>(Object));
		}
		Root->SetArrayField(TEXT("control_targets"), ControlValues);
		return Root;
	}


	TSharedRef<FJsonObject> BuildPolicyInputProvenanceTransformJson(const FTransform& Transform)
	{
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetArrayField(TEXT("translation_xyz"), BuildVectorJsonArray(Transform.GetLocation()));
		Object->SetArrayField(TEXT("rotation_xyzw"), BuildQuaternionJsonArray(Transform.GetRotation()));
		Object->SetArrayField(TEXT("scale_xyz"), BuildVectorJsonArray(Transform.GetScale3D()));
		return Object;
	}

	TArray<TSharedPtr<FJsonValue>> BuildPolicyInputProvenanceBodySamplesJson(
		TConstArrayView<FPhysAnimBodySample> Samples)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		Values.Reserve(Samples.Num());
		for (const FPhysAnimBodySample& Sample : Samples)
		{
			const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
			Object->SetArrayField(TEXT("position_xyz"), BuildVectorJsonArray(Sample.Position));
			Object->SetArrayField(TEXT("rotation_xyzw"), BuildQuaternionJsonArray(Sample.Rotation));
			Object->SetArrayField(TEXT("linear_velocity_xyz"), BuildVectorJsonArray(Sample.LinearVelocity));
			Object->SetArrayField(TEXT("angular_velocity_xyz"), BuildVectorJsonArray(Sample.AngularVelocity));
			Values.Add(MakeShared<FJsonValueObject>(Object));
		}
		return Values;
	}

	TArray<TSharedPtr<FJsonValue>> BuildPolicyInputProvenanceFutureSamplesJson(
		TConstArrayView<FPhysAnimFuturePoseSample> Samples)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		Values.Reserve(Samples.Num());
		for (const FPhysAnimFuturePoseSample& Sample : Samples)
		{
			const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
			Object->SetNumberField(TEXT("future_time_seconds"), Sample.FutureTimeSeconds);
			TArray<TSharedPtr<FJsonValue>> BodyTransforms;
			BodyTransforms.Reserve(Sample.BodyTransforms.Num());
			for (const FTransform& BodyTransform : Sample.BodyTransforms)
			{
				BodyTransforms.Add(MakeShared<FJsonValueObject>(BuildPolicyInputProvenanceTransformJson(BodyTransform)));
			}
			Object->SetArrayField(TEXT("body_transforms"), BodyTransforms);
			Values.Add(MakeShared<FJsonValueObject>(Object));
		}
		return Values;
	}

	TSharedRef<FJsonObject> BuildPolicyInputProvenanceJson(
		const PhysAnimBridge::FPhysAnimPolicyInputProvenanceSnapshot& Snapshot,
		const bool bEnabled,
		const int32 ExperimentalFirstPolicyDelayTicks,
		const int32 ExperimentalFirstPolicyDelayTicksConsumed)
	{
		FString ValidationError;
		const bool bValid = bEnabled &&
			PhysAnimBridge::ValidatePolicyInputProvenanceSnapshot(Snapshot, ValidationError);
		if (!bEnabled)
		{
			ValidationError.Reset();
		}

		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("schema_version"), TEXT("physanim-policy-input-provenance/v1"));
		Root->SetStringField(TEXT("authority"), TEXT("DEVELOPMENT_DIAGNOSTIC_ONLY"));
		Root->SetBoolField(TEXT("enabled"), bEnabled);
		Root->SetBoolField(TEXT("captured"), Snapshot.bCaptured);
		Root->SetBoolField(TEXT("valid"), bValid);
		Root->SetStringField(TEXT("validation_error"), ValidationError);
		Root->SetNumberField(TEXT("experimental_first_policy_delay_ticks"), ExperimentalFirstPolicyDelayTicks);
		Root->SetNumberField(TEXT("experimental_first_policy_delay_ticks_consumed"), ExperimentalFirstPolicyDelayTicksConsumed);
		Root->SetStringField(TEXT("capture_scope"), Snapshot.CaptureScope);
		Root->SetStringField(TEXT("runtime_state"), Snapshot.RuntimeState);
		Root->SetNumberField(TEXT("world_time_seconds"), Snapshot.WorldTimeSeconds);
		Root->SetNumberField(TEXT("policy_control_tick"), Snapshot.PolicyControlTick);
		Root->SetStringField(TEXT("pose_search_animation"), Snapshot.PoseSearchAnimation);
		Root->SetNumberField(TEXT("pose_search_selected_time"), Snapshot.PoseSearchSelectedTime);
		Root->SetBoolField(TEXT("pose_search_mirrored"), Snapshot.bPoseSearchMirrored);
		Root->SetObjectField(TEXT("owner_actor_world_transform"), BuildPolicyInputProvenanceTransformJson(Snapshot.OwnerActorWorldTransform));
		Root->SetObjectField(TEXT("mesh_world_transform"), BuildPolicyInputProvenanceTransformJson(Snapshot.MeshWorldTransform));
		Root->SetObjectField(TEXT("root_bone_world_transform"), BuildPolicyInputProvenanceTransformJson(Snapshot.RootBoneWorldTransform));
		Root->SetObjectField(TEXT("mimic_target_reference_world_root"), BuildPolicyInputProvenanceTransformJson(Snapshot.MimicTargetReferenceWorldRoot));
		Root->SetObjectField(TEXT("mimic_target_reference_data_root"), BuildPolicyInputProvenanceTransformJson(Snapshot.MimicTargetReferenceDataRoot));
		Root->SetNumberField(TEXT("self_observation_ground_height"), Snapshot.SelfObservationGroundHeight);
		Root->SetArrayField(TEXT("manny_body_samples"), BuildPolicyInputProvenanceBodySamplesJson(Snapshot.MannyBodySamples));
		Root->SetArrayField(TEXT("canonical_body_samples"), BuildPolicyInputProvenanceBodySamplesJson(Snapshot.CanonicalBodySamples));
		Root->SetArrayField(TEXT("mimic_reference_body_samples"), BuildPolicyInputProvenanceBodySamplesJson(Snapshot.MimicReferenceBodySamples));
		Root->SetArrayField(TEXT("manny_future_pose_samples"), BuildPolicyInputProvenanceFutureSamplesJson(Snapshot.MannyFuturePoseSamples));
		Root->SetArrayField(TEXT("canonical_future_pose_samples"), BuildPolicyInputProvenanceFutureSamplesJson(Snapshot.CanonicalFuturePoseSamples));
		Root->SetArrayField(TEXT("terrain_ground_heights"), BuildPolicyActionJsonArray(Snapshot.TerrainGroundHeights));
		Root->SetArrayField(TEXT("previous_actions"), BuildPolicyActionJsonArray(Snapshot.PreviousActions));
		return Root;
	}


	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPolicyInputProvenancePublicationContractTest,
		"PhysAnim.ProductHarness.PolicyInputProvenancePublicationContract",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimPolicyInputProvenancePublicationContractTest::RunTest(const FString& Parameters)
	{
		const PhysAnimBridge::FPhysAnimPolicyInputProvenanceSnapshot EmptySnapshot;
		const TSharedRef<FJsonObject> DisabledJson = BuildPolicyInputProvenanceJson(EmptySnapshot, false, 0, 0);
		TestFalse(TEXT("Provenance publication reports the default-off state"), DisabledJson->GetBoolField(TEXT("enabled")));
		TestFalse(TEXT("Disabled provenance publication is uncaptured"), DisabledJson->GetBoolField(TEXT("captured")));
		TestFalse(TEXT("Disabled provenance publication is not validated"), DisabledJson->GetBoolField(TEXT("valid")));
		TestTrue(
			TEXT("Published delay configuration is nonnegative"),
			DisabledJson->GetIntegerField(TEXT("experimental_first_policy_delay_ticks")) >= 0);
		TestTrue(
			TEXT("Published consumed delay cannot exceed its configuration"),
			DisabledJson->GetIntegerField(TEXT("experimental_first_policy_delay_ticks_consumed")) <=
				DisabledJson->GetIntegerField(TEXT("experimental_first_policy_delay_ticks")));

		TArray<FPhysAnimBodySample> BodySamples;
		BodySamples.SetNum(PhysAnimBridge::NumSmplBodies);
		TArray<FPhysAnimFuturePoseSample> FutureSamples;
		FutureSamples.SetNum(PhysAnimBridge::NumFutureSteps);
		for (FPhysAnimFuturePoseSample& FutureSample : FutureSamples)
		{
			FutureSample.BodyTransforms.Init(FTransform::Identity, PhysAnimBridge::NumSmplBodies);
		}
		TArray<float> TerrainGroundHeights;
		TerrainGroundHeights.Init(0.0f, PhysAnimBridge::TerrainSize);
		TArray<float> PreviousActions;
		PreviousActions.Init(0.0f, PhysAnimBridge::NumActionFloats);

		PhysAnimBridge::FPhysAnimPolicyInputProvenanceSnapshot Snapshot;
		TestTrue(
			TEXT("Publication fixture captures complete provenance"),
			Snapshot.CaptureFirstIf(
				true,
				TEXT("Standing_Preparation"),
				1.0,
				1,
				TEXT("Idle"),
				0.0f,
				false,
				FTransform::Identity,
				FTransform::Identity,
				FTransform::Identity,
				FTransform::Identity,
				FTransform::Identity,
				0.0f,
				BodySamples,
				BodySamples,
				BodySamples,
				FutureSamples,
				FutureSamples,
				TerrainGroundHeights,
				PreviousActions));

		const TSharedRef<FJsonObject> Json = BuildPolicyInputProvenanceJson(Snapshot, true, 3, 2);
		TestEqual(
			TEXT("Provenance publication schema is versioned"),
			Json->GetStringField(TEXT("schema_version")),
			FString(TEXT("physanim-policy-input-provenance/v1")));
		TestTrue(TEXT("Provenance publication reports explicit enablement"), Json->GetBoolField(TEXT("enabled")));
		TestTrue(TEXT("Provenance publication reports capture"), Json->GetBoolField(TEXT("captured")));
		TestTrue(TEXT("Provenance publication reports contract validity"), Json->GetBoolField(TEXT("valid")));
		TestEqual(
			TEXT("Provenance publishes the applied experimental delay configuration"),
			Json->GetIntegerField(TEXT("experimental_first_policy_delay_ticks")),
			3);
		TestEqual(
			TEXT("Provenance publishes the consumed experimental delay ticks"),
			Json->GetIntegerField(TEXT("experimental_first_policy_delay_ticks_consumed")),
			2);
		TestEqual(
			TEXT("Canonical body sources publish in SMPL order"),
			Json->GetArrayField(TEXT("canonical_body_samples")).Num(),
			PhysAnimBridge::NumSmplBodies);
		TestEqual(
			TEXT("Mimic reference body sources publish in SMPL order"),
			Json->GetArrayField(TEXT("mimic_reference_body_samples")).Num(),
			PhysAnimBridge::NumSmplBodies);
		const TArray<TSharedPtr<FJsonValue>>& CanonicalFuture = Json->GetArrayField(TEXT("canonical_future_pose_samples"));
		TestEqual(
			TEXT("Canonical future sources publish the configured horizon"),
			CanonicalFuture.Num(),
			PhysAnimBridge::NumFutureSteps);
		TestEqual(
			TEXT("Each future source publishes every SMPL body"),
			CanonicalFuture[0]->AsObject()->GetArrayField(TEXT("body_transforms")).Num(),
			PhysAnimBridge::NumSmplBodies);
		TestEqual(
			TEXT("Terrain source publication preserves the sample grid"),
			Json->GetArrayField(TEXT("terrain_ground_heights")).Num(),
			PhysAnimBridge::TerrainSize);
		TestEqual(
			TEXT("Previous-action publication preserves checkpoint width"),
			Json->GetArrayField(TEXT("previous_actions")).Num(),
			PhysAnimBridge::NumActionFloats);
		return true;
	}

	class FCausalStandingCaptureCommand final : public IAutomationLatentCommand
	{
	public:
		FCausalStandingCaptureCommand(FAutomationTestBase* InTest, const FCausalStandingRunConfig& InConfig)
			: Test(InTest), Config(InConfig), StartupRealTime(FPlatformTime::Seconds())
		{
		}

		virtual ~FCausalStandingCaptureCommand() override
		{
			RestoreFixedTimeStep();
		}

		virtual bool Update() override
		{
			ConfigureFixedTimeStep();
			UWorld* World = FindProductWorld();
			if (!World)
			{
				if (FPlatformTime::Seconds() - StartupRealTime >= StartupTimeoutSeconds)
				{
					Test->AddError(TEXT("PIE world was unavailable for the causal-standing product run"));
					RestoreFixedTimeStep();
					return true;
				}
				return false;
			}

			UPhysAnimComponent* Component = FindProductComponent(World);
			if (Component && !bVariantApplied)
			{
				if (Config.Variant == TEXT("DropControlDispatch"))
				{
					Component->SetProductControlDispatchDroppedForTesting(true);
				}
				Component->SetStandingVariantForTesting(Config.StandingVariant);
				bVariantApplied = true;
			}

			if (!bObservationStarted)
			{
				const bool bPlantRequiresStandingHold = Config.bPlantRun &&
					FPhysAnimStandingActivationPlan::RequiresStandingHold(Config.StandingVariant);
				const bool bPlantReady = Component && (bPlantRequiresStandingHold
					? (Component->GetRuntimeState() == EPhysAnimRuntimeState::BalanceActive_Standing ||
						Component->GetRuntimeState() == EPhysAnimRuntimeState::FailStopped)
					: (Component->GetStandingActivationStatus().bFullSimulationCommitted ||
						Component->GetRuntimeState() == EPhysAnimRuntimeState::FailStopped));
				const bool bReady = Component && (Config.bPlantRun
					? bPlantReady
					: Component->GetRuntimeState() == EPhysAnimRuntimeState::BalanceActive_Standing);
				if (!bReady && FPlatformTime::Seconds() - StartupRealTime < StartupTimeoutSeconds)
				{
					return false;
				}
				bObservationStarted = true;
				ObservationStartWorldTime = World->GetTimeSeconds();
				if (Component)
				{
					const FPhysAnimActivatedStandingStabilityMetrics& Metrics = Component->GetActivatedStandingStabilityMetrics();
					LastPolicyActionSampleCount = Metrics.PolicyActionSampleCount;
					LastPoseSearchQueryCount = Metrics.PoseSearchQueryCount;
					LastPoseSearchValidCount = Metrics.PoseSearchValidResultCount;
					LastInferenceAttemptCount = Metrics.PolicyInferenceAttemptCount;
					LastInferenceSuccessCount = Metrics.PolicyInferenceSuccessCount;
				}
			}

			const double TimeSeconds = World->GetTimeSeconds() - ObservationStartWorldTime;
			const TOptional<double> SampleTimeSeconds = ResolveCausalStandingSampleTime(
				TimeSeconds,
				Config.CaptureWindowSeconds);
			if (SampleTimeSeconds.IsSet() && SampleTimeSeconds.GetValue() > LastPhysicsTimeSeconds)
			{
				LastPhysicsTimeSeconds = SampleTimeSeconds.GetValue();
				CapturePhysicsSample(World, Component, SampleTimeSeconds.GetValue());
				CapturePolicySample(Component, SampleTimeSeconds.GetValue());
			}

			if (Config.bApplyPerturbation && !bPerturbationApplied && TimeSeconds >= PerturbationTimeSeconds)
			{
				ApplyPerturbation(Component);
				bPerturbationApplied = true;
			}

			if (TimeSeconds < Config.CaptureWindowSeconds)
			{
				return false;
			}

			RestoreVariant(Component);
			if (!WriteEvidence(World, Component))
			{
				Test->AddError(TEXT("Failed to write causal-standing product evidence"));
			}
			RestoreFixedTimeStep();
			return true;
		}

	private:
		void ConfigureFixedTimeStep()
		{
			if (bFixedTimeStepConfigured)
			{
				return;
			}
			bPreviousUseFixedTimeStep = FApp::UseFixedTimeStep();
			PreviousFixedDeltaTimeSeconds = FApp::GetFixedDeltaTime();
			FApp::SetFixedDeltaTime(GetCausalStandingFixedDeltaTimeSeconds());
			FApp::SetUseFixedTimeStep(true);
			bFixedTimeStepConfigured = true;
		}

		void RestoreFixedTimeStep()
		{
			if (!bFixedTimeStepConfigured)
			{
				return;
			}
			FApp::SetUseFixedTimeStep(bPreviousUseFixedTimeStep);
			FApp::SetFixedDeltaTime(PreviousFixedDeltaTimeSeconds);
			bFixedTimeStepConfigured = false;
		}

		void CapturePhysicsSample(UWorld* World, UPhysAnimComponent* Component, double TimeSeconds)
		{
			ProductSupportGapTimerMs = AdvanceCausalStandingSupportGapMs(
				ProductSupportGapTimerMs,
				Component && Component->HasProductSupportContactForTesting(),
				CausalStandingFixedDeltaTimeSeconds);
			ACharacter* Character = Component ? Cast<ACharacter>(Component->GetOwner()) : nullptr;
			USkeletalMeshComponent* Mesh = Component ? Component->GetMeshComponent() : nullptr;
			UPhysicsControlComponent* PhysicsControl = Character ? Character->FindComponentByClass<UPhysicsControlComponent>() : nullptr;
			FBodyInstance* PelvisBody = Mesh ? Mesh->GetBodyInstance(TEXT("pelvis")) : nullptr;
			static const TArray<FName> CriticalBones = { TEXT("pelvis"), TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"), TEXT("thigh_l"), TEXT("thigh_r") };
			static const TArray<FName> SupportBones = { TEXT("foot_l"), TEXT("foot_r"), TEXT("ball_l"), TEXT("ball_r") };
			int32 CriticalValidMask = 0;
			int32 CriticalSimulatingMask = 0;
			int32 SupportValidMask = 0;
			int32 SupportSimulatingMask = 0;
			ReadBodyMasks(Mesh, CriticalBones, CriticalValidMask, CriticalSimulatingMask);
			ReadBodyMasks(Mesh, SupportBones, SupportValidMask, SupportSimulatingMask);
			int32 BodyValidCount = 0;
			int32 BodySimulatingCount = 0;
			FStandingPlantSpeedExtrema SpeedExtrema;
			FStandingPlantMassExtrema MassExtrema;
			if (Mesh)
			{
				for (const FName BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
				{
					const FBodyInstance* const Body = Mesh->GetBodyInstance(BoneName);
					if (!Body || !Body->IsValidBodyInstance())
					{
						continue;
					}
					++BodyValidCount;
					BodySimulatingCount += Body->IsInstanceSimulatingPhysics() ? 1 : 0;
					SpeedExtrema.Observe(
						BoneName,
						static_cast<double>(Body->GetUnrealWorldVelocity().Size()),
						static_cast<double>(FMath::RadiansToDegrees(Body->GetUnrealWorldAngularVelocityInRadians().Size())),
						Body->GetBodyMass(),
						Body->GetBodyInertiaTensor());
					MassExtrema.Observe(BoneName, Body->GetBodyMass(), Body->GetBodyInertiaTensor());
				}
			}
			const double RootLinearSpeedCmPerSec = PelvisBody
				? PelvisBody->GetUnrealWorldVelocity().Size()
				: 0.0;
			const double RootAngularSpeedDegPerSec = PelvisBody
				? FMath::RadiansToDegrees(PelvisBody->GetUnrealWorldAngularVelocityInRadians().Size())
				: 0.0;

			const FPhysAnimRunArtifactSnapshot* Artifact = Component
				? &Component->GetLiveRuntimeEvidenceTerminationState().LatestArtifact
				: nullptr;
			const FPhysAnimStandingActivationStatus StandingStatus = Component
				? Component->GetStandingActivationStatus()
				: FPhysAnimStandingActivationStatus{};
			FPhysicsControlData GainProbeData;
			FPhysicsControlMultiplier GainProbeMultiplier;
			const FName GainProbeControl = PhysAnimBridge::MakeControlName(TEXT("thigh_l"));
			const bool bHasGainProbe = PhysicsControl &&
				PhysicsControl->GetControlData(GainProbeControl, GainProbeData) &&
				PhysicsControl->GetControlMultiplier(GainProbeControl, GainProbeMultiplier);
			const FStandingPlantPoseErrorSummary PoseErrors = Component
				? MeasurePoseErrors(Mesh, PhysicsControl, Component->GetPreviousControlTargetRotationsForDiagnostics())
				: FStandingPlantPoseErrorSummary{};
			UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
			UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
			const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetNumberField(TEXT("sequence"), PhysicsSequence++);
			Row->SetNumberField(TEXT("time_sec"), TimeSeconds);
			Row->SetStringField(TEXT("runtime_state"), Component ? RuntimeStateName(Component->GetRuntimeState()) : TEXT("Uninitialized"));
			Row->SetNumberField(TEXT("pelvis_height_cm"), MeasurePelvisHeight(World, Character, PelvisBody));
			Row->SetNumberField(TEXT("root_tilt_deg"), MeasureRootTilt(Component));
			Row->SetNumberField(TEXT("max_penetration_cm"), Artifact ? Artifact->MaxPenetrationCm : 0.0);
			Row->SetNumberField(TEXT("support_gap_ms"), ProductSupportGapTimerMs);
			Row->SetNumberField(TEXT("critical_body_valid_mask"), CriticalValidMask);
			Row->SetNumberField(TEXT("critical_body_simulating_mask"), CriticalSimulatingMask);
			Row->SetNumberField(TEXT("support_body_valid_mask"), SupportValidMask);
			Row->SetNumberField(TEXT("support_body_simulating_mask"), SupportSimulatingMask);
			Row->SetNumberField(TEXT("body_valid_count"), BodyValidCount);
			Row->SetNumberField(TEXT("body_simulating_count"), BodySimulatingCount);
			Row->SetNumberField(TEXT("control_gain_match_count"), StandingStatus.ControlGainMatchCount);
			Row->SetNumberField(TEXT("passive_constraint_velocity_drive_match_count"), StandingStatus.PassiveConstraintVelocityDriveMatchCount);
			Row->SetNumberField(TEXT("control_base_angular_strength_hz"), bHasGainProbe ? GainProbeData.AngularStrength : 0.0);
			Row->SetNumberField(TEXT("control_angular_strength_multiplier"), bHasGainProbe ? GainProbeMultiplier.AngularStrengthMultiplier : 0.0);
			Row->SetNumberField(TEXT("control_angular_damping_ratio_multiplier"), bHasGainProbe ? GainProbeMultiplier.AngularDampingRatioMultiplier : 0.0);
			Row->SetNumberField(TEXT("control_angular_extra_damping_multiplier"), bHasGainProbe ? GainProbeMultiplier.AngularExtraDampingMultiplier : 0.0);
			Row->SetBoolField(TEXT("full_simulation_committed"), StandingStatus.bFullSimulationCommitted);
			Row->SetNumberField(TEXT("root_linear_speed_cm_per_sec"), RootLinearSpeedCmPerSec);
			Row->SetNumberField(TEXT("root_angular_speed_deg_per_sec"), RootAngularSpeedDegPerSec);
			Row->SetNumberField(TEXT("max_body_linear_speed_cm_per_sec"), SpeedExtrema.MaxLinearSpeedCmPerSec);
			Row->SetStringField(TEXT("max_body_linear_speed_bone"), SpeedExtrema.MaxLinearBody.ToString());
			Row->SetNumberField(TEXT("max_body_linear_speed_body_mass_kg"), SpeedExtrema.MaxLinearBodyMassKg);
			Row->SetNumberField(TEXT("max_body_linear_speed_body_min_inertia_kg_cm_sq"), SpeedExtrema.MaxLinearBodyMinInertiaKgCmSq);
			Row->SetNumberField(TEXT("max_body_angular_speed_deg_per_sec"), SpeedExtrema.MaxAngularSpeedDegPerSec);
			Row->SetStringField(TEXT("max_body_angular_speed_bone"), SpeedExtrema.MaxAngularBody.ToString());
			Row->SetNumberField(TEXT("max_body_angular_speed_body_mass_kg"), SpeedExtrema.MaxAngularBodyMassKg);
			Row->SetNumberField(TEXT("max_body_angular_speed_body_min_inertia_kg_cm_sq"), SpeedExtrema.MaxAngularBodyMinInertiaKgCmSq);
			Row->SetNumberField(TEXT("minimum_body_mass_kg"), MassExtrema.MinMassKg);
			Row->SetStringField(TEXT("minimum_body_mass_bone"), MassExtrema.MinMassBody.ToString());
			Row->SetNumberField(TEXT("minimum_body_inertia_kg_cm_sq"), MassExtrema.MinInertiaKgCmSq);
			Row->SetStringField(TEXT("minimum_body_inertia_bone"), MassExtrema.MinInertiaBody.ToString());
			Row->SetBoolField(TEXT("root_is_simulating"), PelvisBody && PelvisBody->IsValidBodyInstance() && PelvisBody->IsInstanceSimulatingPhysics());
			Row->SetBoolField(TEXT("cmc_active"), Movement && Movement->IsActive());
			Row->SetBoolField(TEXT("cmc_tick_enabled"), Movement && Movement->IsComponentTickEnabled());
			Row->SetBoolField(TEXT("cmc_updated_component_is_null"), !Movement || Movement->UpdatedComponent == nullptr);
			Row->SetNumberField(TEXT("capsule_collision_enabled"), Capsule ? static_cast<int32>(Capsule->GetCollisionEnabled()) : 0);
			Row->SetNumberField(TEXT("movement_reclaim_count"), Artifact ? Artifact->MovementReclaimCount : 0);
			Row->SetNumberField(TEXT("shell_helper_used_count"), Artifact ? Artifact->ShellHelperUsedCount : 0);
			Row->SetNumberField(TEXT("topology_change_count"), Artifact ? Artifact->TopologyChangeCount : 0);
			Row->SetNumberField(TEXT("pose_rms_error_deg"), Component ? PoseErrors.RmsDegrees() : 180.0);
			Row->SetNumberField(TEXT("lower_limb_pose_rms_error_deg"), Component ? PoseErrors.LowerLimbRmsDegrees() : 180.0);
			Row->SetNumberField(TEXT("max_pose_error_deg"), Component ? PoseErrors.MaxErrorDegrees : 180.0);
			Row->SetStringField(TEXT("max_pose_error_bone"), PoseErrors.MaxErrorBone.ToString());
			PhysicsRows.Add(SerializeJson(Row));
		}

		void CapturePolicySample(UPhysAnimComponent* Component, double TimeSeconds)
		{
			if (!Component)
			{
				return;
			}
			const FPhysAnimActivatedStandingStabilityMetrics& Metrics = Component->GetActivatedStandingStabilityMetrics();
			if (Metrics.PolicyActionSampleCount <= LastPolicyActionSampleCount)
			{
				return;
			}
			ACharacter* Character = Cast<ACharacter>(Component->GetOwner());
			UPhysicsControlComponent* PhysicsControl = Character ? Character->FindComponentByClass<UPhysicsControlComponent>() : nullptr;
			const TMap<FName, FQuat>& IntendedTargets = Component->GetPreviousControlTargetRotationsForDiagnostics();
			const FPhysAnimControlTargetDiagnostics& Diagnostics = Component->GetLastControlTargetDiagnostics();
			int32 ReadbackMatches = 0;
			double MaxReadbackErrorDegrees = 0.0;
			if (Diagnostics.NumTotalTargetsWritten > 0)
			{
				MaxReadbackErrorDegrees = 180.0;
				MeasureTargetReadback(PhysicsControl, IntendedTargets, ReadbackMatches, MaxReadbackErrorDegrees);
			}

			const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetNumberField(TEXT("sequence"), PolicySequence++);
			Row->SetNumberField(TEXT("time_sec"), TimeSeconds);
			Row->SetBoolField(TEXT("pose_search_valid"), Metrics.PoseSearchQueryCount > LastPoseSearchQueryCount && Metrics.PoseSearchValidResultCount > LastPoseSearchValidCount);
			Row->SetStringField(TEXT("selected_animation"), Metrics.PoseSearchSelectedAnimationName);
			Row->SetBoolField(TEXT("inference_attempted"), Metrics.PolicyInferenceAttemptCount > LastInferenceAttemptCount);
			Row->SetBoolField(TEXT("inference_succeeded"), Metrics.PolicyInferenceSuccessCount > LastInferenceSuccessCount);
			Row->SetNumberField(TEXT("raw_action_l2"), L2Norm(Component->GetRawPolicyActionsForDiagnostics()));
			Row->SetNumberField(TEXT("conditioned_action_l2"), L2Norm(Component->GetConditionedPolicyActionsForDiagnostics()));
			Row->SetArrayField(TEXT("raw_actions"), BuildPolicyActionJsonArray(Component->GetRawPolicyActionsForDiagnostics()));
			Row->SetArrayField(TEXT("conditioned_actions"), BuildPolicyActionJsonArray(Component->GetConditionedPolicyActionsForDiagnostics()));
			Row->SetNumberField(TEXT("target_write_attempt_count"), Diagnostics.NumTotalTargetsWritten);
			Row->SetNumberField(TEXT("target_readback_match_count"), ReadbackMatches);
			Row->SetNumberField(TEXT("target_readback_max_error_deg"), MaxReadbackErrorDegrees);
			PolicyRows.Add(SerializeJson(Row));

			LastPolicyActionSampleCount = Metrics.PolicyActionSampleCount;
			LastPoseSearchQueryCount = Metrics.PoseSearchQueryCount;
			LastPoseSearchValidCount = Metrics.PoseSearchValidResultCount;
			LastInferenceAttemptCount = Metrics.PolicyInferenceAttemptCount;
			LastInferenceSuccessCount = Metrics.PolicyInferenceSuccessCount;
		}

		void ApplyPerturbation(UPhysAnimComponent* Component)
		{
			if (!Component)
			{
				return;
			}
			if (Config.Variant == TEXT("ForcedSupportLoss"))
			{
				Component->SetForceSupportFailure(true);
				return;
			}
			USkeletalMeshComponent* Mesh = Component->GetMeshComponent();
			FBodyInstance* PelvisBody = Mesh ? Mesh->GetBodyInstance(TEXT("pelvis")) : nullptr;
			if (PelvisBody && PelvisBody->IsValidBodyInstance())
			{
				const FVector Forward = Component->GetOwner()->GetActorForwardVector().GetSafeNormal();
				PelvisBody->AddImpulse(Forward * PelvisBody->GetBodyMass() * PerturbationDeltaVCmPerSecond, false);
			}
		}

		void RestoreVariant(UPhysAnimComponent* Component)
		{
			if (!Component)
			{
				return;
			}
			Component->SetProductControlDispatchDroppedForTesting(false);
			Component->SetStandingVariantForTesting(EPhysAnimStandingVariant::Normal);
			Component->SetForceSupportFailure(false);
		}

		int32 CaptureRender(UWorld* World, UPhysAnimComponent* Component, const FString& OutputPath)
		{
			if (!World)
			{
				return 0;
			}
			ASceneCapture2D* CaptureActor = World->SpawnActor<ASceneCapture2D>();
			if (!CaptureActor)
			{
				return 0;
			}
			const FVector Target = Component && Component->GetOwner()
				? Component->GetOwner()->GetActorLocation() + FVector(0.0f, 0.0f, 90.0f)
				: FVector::ZeroVector;
			const FVector CameraLocation = Target + FVector(-300.0f, 300.0f, 120.0f);
			CaptureActor->SetActorLocationAndRotation(CameraLocation, (Target - CameraLocation).Rotation());
			UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(CaptureActor);
			RenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
			RenderTarget->InitAutoFormat(640, 480);
			RenderTarget->UpdateResourceImmediate(true);
			USceneCaptureComponent2D* CaptureComponent = CaptureActor->GetCaptureComponent2D();
			CaptureComponent->TextureTarget = RenderTarget;
			CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
			CaptureComponent->bCaptureEveryFrame = false;
			CaptureComponent->CaptureScene();

			TArray<FColor> Pixels;
			const bool bRead = RenderTarget->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
			int32 NonblankPixels = 0;
			if (bRead && Pixels.Num() == 640 * 480)
			{
				const int32 ReferenceLuminance = (static_cast<int32>(Pixels[0].R) + Pixels[0].G + Pixels[0].B) / 3;
				for (const FColor& Pixel : Pixels)
				{
					const int32 Luminance = (static_cast<int32>(Pixel.R) + Pixel.G + Pixel.B) / 3;
					if (FMath::Abs(Luminance - ReferenceLuminance) > 4)
					{
						++NonblankPixels;
					}
				}
				TArray64<uint8> Compressed;
				FImageUtils::PNGCompressImageArray(640, 480, Pixels, Compressed);
				FFileHelper::SaveArrayToFile(Compressed, *OutputPath);
			}
			CaptureActor->Destroy();
			return NonblankPixels;
		}

		bool WriteEvidence(UWorld* World, UPhysAnimComponent* Component)
		{
			IFileManager::Get().MakeDirectory(*Config.RunRoot, true);
			const FString PhysicsPath = FPaths::Combine(Config.RunRoot, TEXT("physics.jsonl"));
			const FString PolicyPath = FPaths::Combine(Config.RunRoot, TEXT("policy.jsonl"));
			const FString PolicyInputSnapshotPath = FPaths::Combine(Config.RunRoot, TEXT("policy-input-snapshot.json"));
			const FString RenderPath = FPaths::Combine(Config.RunRoot, TEXT("render.png"));
			const FString ManifestPath = FPaths::Combine(Config.RunRoot, TEXT("manifest.json"));
			const bool bPhysicsWritten = FFileHelper::SaveStringToFile(FString::Join(PhysicsRows, TEXT("\n")) + TEXT("\n"), *PhysicsPath);
			const FString PolicyContents = PolicyRows.IsEmpty() ? FString() : FString::Join(PolicyRows, TEXT("\n")) + TEXT("\n");
			const bool bPolicyWritten = FFileHelper::SaveStringToFile(PolicyContents, *PolicyPath);
			const PhysAnimBridge::FPhysAnimPolicyInferenceSnapshot EmptySnapshot;
			const PhysAnimBridge::FPhysAnimPolicyInferenceSnapshot& Snapshot = Component
				? Component->GetFirstPolicyInferenceSnapshotForDiagnostics()
				: EmptySnapshot;
			const TSharedRef<FJsonObject> SnapshotJson = MakeShared<FJsonObject>();
			SnapshotJson->SetStringField(TEXT("schema_version"), TEXT("physanim-policy-input-snapshot/v1"));
			SnapshotJson->SetBoolField(TEXT("captured"), Snapshot.bCaptured);
			SnapshotJson->SetNumberField(TEXT("self_observation_width"), Snapshot.SelfObservation.Num());
			SnapshotJson->SetNumberField(TEXT("mimic_target_poses_width"), Snapshot.MimicTargetPoses.Num());
			SnapshotJson->SetNumberField(TEXT("terrain_width"), Snapshot.Terrain.Num());
			SnapshotJson->SetNumberField(TEXT("action_width"), Snapshot.Actions.Num());
			SnapshotJson->SetArrayField(TEXT("self_observation"), BuildPolicyActionJsonArray(Snapshot.SelfObservation));
			SnapshotJson->SetArrayField(TEXT("mimic_target_poses"), BuildPolicyActionJsonArray(Snapshot.MimicTargetPoses));
			SnapshotJson->SetArrayField(TEXT("terrain"), BuildPolicyActionJsonArray(Snapshot.Terrain));
			SnapshotJson->SetArrayField(TEXT("actions"), BuildPolicyActionJsonArray(Snapshot.Actions));
			const bool bPolicyInputSnapshotWritten = FFileHelper::SaveStringToFile(
				SerializeJson(SnapshotJson) + TEXT("\n"),
				*PolicyInputSnapshotPath);
			bool bActiveStandingPolicyInputSnapshotWritten = true;
			bool bActionSemanticTraceWritten = true;
			bool bPolicyInputProvenanceWritten = true;
			if (Config.bPlantRun)
			{
				const FString ActiveStandingSnapshotPath = FPaths::Combine(
					Config.RunRoot,
					TEXT("active-standing-policy-input-snapshot.json"));
				const PhysAnimBridge::FPhysAnimPolicyInferenceSnapshot& ActiveStandingSnapshot = Component
					? Component->GetFirstActiveStandingPolicyInferenceSnapshotForDiagnostics()
					: EmptySnapshot;
				const TSharedRef<FJsonObject> ActiveStandingSnapshotJson = MakeShared<FJsonObject>();
				ActiveStandingSnapshotJson->SetStringField(
					TEXT("schema_version"),
					TEXT("physanim-policy-input-snapshot/v1"));
				ActiveStandingSnapshotJson->SetStringField(
					TEXT("capture_scope"),
					TEXT("first_active_standing_inference"));
				ActiveStandingSnapshotJson->SetBoolField(TEXT("captured"), ActiveStandingSnapshot.bCaptured);
				ActiveStandingSnapshotJson->SetNumberField(TEXT("self_observation_width"), ActiveStandingSnapshot.SelfObservation.Num());
				ActiveStandingSnapshotJson->SetNumberField(TEXT("mimic_target_poses_width"), ActiveStandingSnapshot.MimicTargetPoses.Num());
				ActiveStandingSnapshotJson->SetNumberField(TEXT("terrain_width"), ActiveStandingSnapshot.Terrain.Num());
				ActiveStandingSnapshotJson->SetNumberField(TEXT("action_width"), ActiveStandingSnapshot.Actions.Num());
				ActiveStandingSnapshotJson->SetArrayField(TEXT("self_observation"), BuildPolicyActionJsonArray(ActiveStandingSnapshot.SelfObservation));
				ActiveStandingSnapshotJson->SetArrayField(TEXT("mimic_target_poses"), BuildPolicyActionJsonArray(ActiveStandingSnapshot.MimicTargetPoses));
				ActiveStandingSnapshotJson->SetArrayField(TEXT("terrain"), BuildPolicyActionJsonArray(ActiveStandingSnapshot.Terrain));
				ActiveStandingSnapshotJson->SetArrayField(TEXT("actions"), BuildPolicyActionJsonArray(ActiveStandingSnapshot.Actions));
				bActiveStandingPolicyInputSnapshotWritten = FFileHelper::SaveStringToFile(
					SerializeJson(ActiveStandingSnapshotJson) + TEXT("\n"),
					*ActiveStandingSnapshotPath);

				const FString ActionSemanticTracePath = FPaths::Combine(
					Config.RunRoot,
					TEXT("action-semantic-trace.json"));
				const PhysAnimBridge::FPhysAnimActionSemanticTrace EmptyActionSemanticTrace;
				const PhysAnimBridge::FPhysAnimActionSemanticTrace& ActionSemanticTrace = Component
					? Component->GetActionSemanticTraceForTesting()
					: EmptyActionSemanticTrace;
				const bool bActionSemanticTraceEnabled =
					Component && Component->IsActionSemanticTraceEnabledForTesting();
				bActionSemanticTraceWritten = FFileHelper::SaveStringToFile(
					SerializeJson(BuildActionSemanticTraceJson(
						ActionSemanticTrace,
						bActionSemanticTraceEnabled)) + TEXT("\n"),
					*ActionSemanticTracePath);

				const bool bPolicyInputProvenanceEnabled =
					Component && Component->IsPolicyInputProvenanceTraceEnabledForTesting();
				if (bPolicyInputProvenanceEnabled)
				{
					const FString PolicyInputProvenancePath = FPaths::Combine(
						Config.RunRoot,
						TEXT("policy-input-provenance.json"));
					bPolicyInputProvenanceWritten = FFileHelper::SaveStringToFile(
						SerializeJson(BuildPolicyInputProvenanceJson(
							Component->GetPolicyInputProvenanceSnapshotForTesting(),
							true,
							Component->GetExperimentalFirstPolicyDelayTicksForTesting(),
							Component->GetExperimentalFirstPolicyDelayTicksConsumedForTesting())) + TEXT("\n"),
						*PolicyInputProvenancePath);
				}
			}
			const int32 NonblankPixels = CaptureRender(World, Component, RenderPath);

			const FString ProtocolPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), Config.ProtocolRelativePath));
			const TSharedRef<FJsonObject> Manifest = MakeShared<FJsonObject>();
			Manifest->SetStringField(TEXT("schema_version"), Config.RunSchemaVersion);
			Manifest->SetStringField(TEXT("fixture_authority"), Config.FixtureAuthority);
			Manifest->SetStringField(TEXT("run_id"), Config.RunId);
			Manifest->SetStringField(TEXT("protocol_path"), ProtocolPath);
			Manifest->SetStringField(TEXT("variant"), Config.Variant);
			Manifest->SetNumberField(TEXT("repetition"), Config.Repetition);
			Manifest->SetStringField(TEXT("source_commit"), Config.SourceCommit);
			Manifest->SetBoolField(TEXT("source_tree_dirty"), Config.bSourceTreeDirty);
			Manifest->SetStringField(TEXT("model_onnx_sha256"), Config.ModelOnnxSha256);
			Manifest->SetNumberField(TEXT("reference_pelvis_height_cm"), MannyReferencePelvisHeightCm);
			Manifest->SetNumberField(TEXT("standing_window_start_sec"), 0.0);
			Manifest->SetNumberField(TEXT("capture_window_sec"), Config.CaptureWindowSeconds);
			Manifest->SetNumberField(TEXT("perturbation_time_sec"), Config.bApplyPerturbation ? PerturbationTimeSeconds : -1.0);
			Manifest->SetStringField(TEXT("physics_samples"), TEXT("physics.jsonl"));
			Manifest->SetStringField(TEXT("policy_samples"), TEXT("policy.jsonl"));
			Manifest->SetStringField(TEXT("policy_input_snapshot"), TEXT("policy-input-snapshot.json"));
			Manifest->SetStringField(TEXT("render_capture"), TEXT("render.png"));
			Manifest->SetNumberField(TEXT("render_nonblank_pixel_count"), NonblankPixels);
			const bool bManifestWritten = FFileHelper::SaveStringToFile(SerializeJson(Manifest) + TEXT("\n"), *ManifestPath);
			return bPhysicsWritten &&
				bPolicyWritten &&
				bPolicyInputSnapshotWritten &&
				bActiveStandingPolicyInputSnapshotWritten &&
				bActionSemanticTraceWritten &&
				bPolicyInputProvenanceWritten &&
				bManifestWritten;
		}

		FAutomationTestBase* Test = nullptr;
		FCausalStandingRunConfig Config;
		double StartupRealTime = 0.0;
		double ObservationStartWorldTime = 0.0;
		double LastPhysicsTimeSeconds = -1.0;
		double ProductSupportGapTimerMs = 0.0;
		bool bObservationStarted = false;
		bool bVariantApplied = false;
		bool bPerturbationApplied = false;
		bool bFixedTimeStepConfigured = false;
		bool bPreviousUseFixedTimeStep = false;
		double PreviousFixedDeltaTimeSeconds = 0.0;
		int32 PhysicsSequence = 0;
		int32 PolicySequence = 0;
		int32 LastPolicyActionSampleCount = 0;
		int32 LastPoseSearchQueryCount = 0;
		int32 LastPoseSearchValidCount = 0;
		int32 LastInferenceAttemptCount = 0;
		int32 LastInferenceSuccessCount = 0;
		TArray<FString> PhysicsRows;
		TArray<FString> PolicyRows;
	};

	class FResetCausalStandingVariantCommand final : public IAutomationLatentCommand
	{
	public:
		virtual bool Update() override
		{
			if (IConsoleVariable* ReviewMode = IConsoleManager::Get().FindConsoleVariable(TEXT("physanim.V0PlantReviewMode")))
			{
				ReviewMode->Set(0, ECVF_SetByCode);
			}
			return true;
		}
	};
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
	FPhysAnimCausalStandingProductTest,
	"PhysAnim.Product.CausalStanding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimCausalStandingProductTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	for (const TCHAR* Variant : { TEXT("Normal"), TEXT("ZeroActions"), TEXT("DropControlDispatch"), TEXT("ForcedSupportLoss") })
	{
		OutBeautifiedNames.Add(Variant);
		OutTestCommands.Add(Variant);
	}
}

bool FPhysAnimCausalStandingProductTest::RunTest(const FString& Parameters)
{
	FCausalStandingRunConfig Config;
	FString ConfigError;
	if (!Config.ReadFromCommandLine(ConfigError))
	{
		AddError(ConfigError);
		return false;
	}
	if (Config.Variant != Parameters)
	{
		AddError(FString::Printf(TEXT("Requested variant '%s' does not match test '%s'"), *Config.Variant, *Parameters));
		return false;
	}
	if (IConsoleVariable* ReviewMode = IConsoleManager::Get().FindConsoleVariable(TEXT("physanim.V0PlantReviewMode")))
	{
		ReviewMode->Set(Config.Variant == TEXT("ZeroActions") ? 2 : 0, ECVF_SetByCode);
	}
	if (!AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"), true))
	{
		AddError(TEXT("Unable to open the causal-standing product map"));
		return false;
	}
	AddCommand(new FStartPIECommand(false));
	AddCommand(new FCausalStandingCaptureCommand(this, Config));
	AddCommand(new FEndPlayMapCommand());
	AddCommand(new FResetCausalStandingVariantCommand());
	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
	FPhysAnimStandingPlantDevelopmentTest,
	"PhysAnim.Development.StandingPlant",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimStandingPlantDevelopmentTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	for (const TCHAR* Layer : { TEXT("ControlsOff"), TEXT("DampingOnly"), TEXT("FixedNeutralTarget"), TEXT("ZeroActions"), TEXT("RealOnnxPolicy") })
	{
		OutBeautifiedNames.Add(Layer);
		OutTestCommands.Add(Layer);
	}
}

bool FPhysAnimStandingPlantDevelopmentTest::RunTest(const FString& Parameters)
{
	FCausalStandingRunConfig Config;
	FString ConfigError;
	if (!Config.ReadFromCommandLine(ConfigError) || !Config.ConfigurePlantLayer(ConfigError))
	{
		AddError(ConfigError);
		return false;
	}
	if (Config.Variant != Parameters)
	{
		AddError(FString::Printf(TEXT("Requested plant layer '%s' does not match test '%s'"), *Config.Variant, *Parameters));
		return false;
	}
	if (!AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"), true))
	{
		AddError(TEXT("Unable to open the standing-plant development map"));
		return false;
	}
	AddCommand(new FStartPIECommand(false));
	AddCommand(new FCausalStandingCaptureCommand(this, Config));
	AddCommand(new FEndPlayMapCommand());
	AddCommand(new FResetCausalStandingVariantCommand());
	return true;
}

#endif
