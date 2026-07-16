#include "PhysAnimComponent.h"
#include "PhysAnimBridge.h"

#include "Animation/AnimSequence.h"
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
#include "PhysicsEngine/ConstraintInstance.h"
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
	void ConfigureCausalStandingFixedTimeStep(
		bool& bInOutConfigured,
		bool& bOutPreviousUseFixedTimeStep,
		double& OutPreviousFixedDeltaTimeSeconds);
	void RestoreCausalStandingFixedTimeStep(
		bool& bInOutConfigured,
		bool bPreviousUseFixedTimeStep,
		double PreviousFixedDeltaTimeSeconds);
	double AdvanceCausalStandingSupportGapMs(double CurrentGapMs, bool bHasSupportContact, double DeltaTimeSeconds);
	double MeasureRootShellTrackingErrorCm(
		const FTransform& ActorTransform,
		const FVector& InitialActorLocalRootOffset,
		const FVector& PhysicalRootWorldLocation);
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
	FPhysAnimPoseSearchLocomotionAssetDirectionAuditTest,
	"PhysAnim.Development.PoseSearchAssetDirections",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimPoseSearchLocomotionAssetDirectionAuditTest::RunTest(const FString& Parameters)
{
	struct FAssetDirectionExpectation
	{
		const TCHAR* AssetName;
		FVector ExpectedDirection;
	};
	static const FAssetDirectionExpectation Expectations[] =
	{
		{ TEXT("MF_Unarmed_Walk_Fwd"), FVector(0.0f, 1.0f, 0.0f) },
		{ TEXT("MF_Unarmed_Walk_Fwd_Left"), FVector(1.0f, 1.0f, 0.0f).GetSafeNormal() },
		{ TEXT("MF_Unarmed_Walk_Left"), FVector(1.0f, 0.0f, 0.0f) },
		{ TEXT("MF_Unarmed_Walk_Bwd_Left"), FVector(1.0f, -1.0f, 0.0f).GetSafeNormal() },
		{ TEXT("MF_Unarmed_Walk_Bwd"), FVector(0.0f, -1.0f, 0.0f) },
		{ TEXT("MF_Unarmed_Walk_Bwd_Right"), FVector(-1.0f, -1.0f, 0.0f).GetSafeNormal() },
		{ TEXT("MF_Unarmed_Walk_Right"), FVector(-1.0f, 0.0f, 0.0f) },
		{ TEXT("MF_Unarmed_Walk_Fwd_Right"), FVector(-1.0f, 1.0f, 0.0f).GetSafeNormal() },
	};
	for (const FAssetDirectionExpectation& Expectation : Expectations)
	{
		const FString AssetPath = FString::Printf(
			TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Walk/%s.%s"),
			Expectation.AssetName,
			Expectation.AssetName);
		UAnimSequence* const Sequence = LoadObject<UAnimSequence>(nullptr, *AssetPath);
		TestNotNull(FString::Printf(TEXT("Load %s"), Expectation.AssetName), Sequence);
		if (!Sequence)
		{
			continue;
		}
		const double SampleEndTime = FMath::Min(0.5, Sequence->GetPlayLength());
		const FAnimExtractContext ExtractionContext;
		const FVector RootDelta = Sequence
			->ExtractRootMotionFromRange(0.0, SampleEndTime, ExtractionContext)
			.GetTranslation();
		TestTrue(
			FString::Printf(TEXT("%s has nonzero planar root motion"), Expectation.AssetName),
			RootDelta.SizeSquared2D() > 1.0);
		TestTrue(
			FString::Printf(TEXT("%s root motion matches its locked database axis"), Expectation.AssetName),
			FVector::DotProduct(RootDelta.GetSafeNormal2D(), Expectation.ExpectedDirection) > 0.999f);
		TestTrue(
			FString::Printf(TEXT("%s half-second root travel remains a walk-scale displacement"), Expectation.AssetName),
			RootDelta.Size2D() > 140.0f && RootDelta.Size2D() < 160.0f);
	}

	const FQuat ActorFacingYaw0 = FQuat::Identity;
	const FQuat MeshFacingYawMinus90(FVector::UpVector, FMath::DegreesToRadians(-90.0f));
	const FQuat QueryFacingYawMinus90 = UPhysAnimComponent::TestOnlyResolveBridgePoseSearchCurrentFacing(
		ActorFacingYaw0,
		MeshFacingYawMinus90,
		true);
	TestTrue(
		TEXT("E77 current Pose Search facing follows the skeletal mesh world basis"),
		QueryFacingYawMinus90.Equals(MeshFacingYawMinus90, 1.0e-6f));
	const FVector WorldForwardVelocity(160.0f, 0.0f, 0.0f);
	TestTrue(
		TEXT("E77 unchanged world-forward velocity localizes to authored database-forward +Y"),
		QueryFacingYawMinus90.Inverse().RotateVector(WorldForwardVelocity).Equals(
			FVector(0.0f, 160.0f, 0.0f),
			1.0e-4f));
	TestTrue(
		TEXT("E77 no-mesh current facing falls back to actor facing"),
		UPhysAnimComponent::TestOnlyResolveBridgePoseSearchCurrentFacing(
			ActorFacingYaw0,
			MeshFacingYawMinus90,
			false).Equals(ActorFacingYaw0, 1.0e-6f));

	const FQuat ActorFacingYaw30(FVector::UpVector, FMath::DegreesToRadians(30.0f));
	const FQuat MeshFacingYawMinus60(FVector::UpVector, FMath::DegreesToRadians(-60.0f));
	const FVector TurnedActorForward = ActorFacingYaw30.RotateVector(WorldForwardVelocity);
	const FQuat TurnedQueryFacing = UPhysAnimComponent::TestOnlyResolveBridgePoseSearchCurrentFacing(
		ActorFacingYaw30,
		MeshFacingYawMinus60,
		true);
	TestTrue(
		TEXT("E77 turned mesh facing preserves authored local +Y for actor-forward velocity"),
		TurnedQueryFacing.Inverse().RotateVector(TurnedActorForward).Equals(
			FVector(0.0f, 160.0f, 0.0f),
			1.0e-4f));
	const FQuat DesiredQueryFacing = UPhysAnimComponent::TestOnlyResolveBridgePoseSearchDesiredFacing(
		ActorFacingYaw30,
		ActorFacingYaw30,
		MeshFacingYawMinus60,
		true);
	TestTrue(
		TEXT("E77 desired actor yaw 30 retains the current actor-to-mesh minus-90 offset"),
		DesiredQueryFacing.Equals(MeshFacingYawMinus60, 1.0e-6f));
	TestTrue(
		TEXT("E77 no-mesh desired facing remains actor desired facing"),
		UPhysAnimComponent::TestOnlyResolveBridgePoseSearchDesiredFacing(
			ActorFacingYaw30,
			ActorFacingYaw0,
			MeshFacingYawMinus90,
			false).Equals(ActorFacingYaw30, 1.0e-6f));
	TestTrue(
		TEXT("E77 facing-basis change leaves exact zero velocity unchanged"),
		TurnedQueryFacing.Inverse().RotateVector(FVector::ZeroVector).Equals(FVector::ZeroVector, 0.0f));
	return true;
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
	bool bFixedStepLifecycleConfigured = false;
	bool bPreviousUseFixedTimeStep = false;
	double PreviousFixedDeltaTimeSeconds = 0.0;
	ConfigureCausalStandingFixedTimeStep(
		bFixedStepLifecycleConfigured,
		bPreviousUseFixedTimeStep,
		PreviousFixedDeltaTimeSeconds);
	TestTrue(TEXT("E42 fixed-step lifecycle records configured state"), bFixedStepLifecycleConfigured);
	TestTrue(TEXT("E42 fixed-step lifecycle enables fixed timestep before PIE"), FApp::UseFixedTimeStep());
	TestTrue(
		TEXT("E42 fixed-step lifecycle publishes the locked delta before PIE"),
		FMath::IsNearlyEqual(FApp::GetFixedDeltaTime(), GetCausalStandingFixedDeltaTimeSeconds(), 1.0e-12));
	RestoreCausalStandingFixedTimeStep(
		bFixedStepLifecycleConfigured,
		bPreviousUseFixedTimeStep,
		PreviousFixedDeltaTimeSeconds);
	TestFalse(TEXT("E42 fixed-step lifecycle clears configured state on restore"), bFixedStepLifecycleConfigured);
	TestEqual(TEXT("E42 fixed-step lifecycle restores prior fixed-step mode"), FApp::UseFixedTimeStep(), bPreviousUseFixedTimeStep);
	TestTrue(
		TEXT("E42 fixed-step lifecycle restores prior delta"),
		FMath::IsNearlyEqual(FApp::GetFixedDeltaTime(), PreviousFixedDeltaTimeSeconds, 1.0e-12));
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

	const FVector InitialActorLocalRootOffset(12.0f, -7.0f, 91.2f);
	const FTransform InitialActorTransform(FRotator(0.0f, 0.0f, 0.0f), FVector(100.0f, 200.0f, 300.0f));
	const FVector InitialExpectedRootWorld = InitialActorTransform.TransformPosition(InitialActorLocalRootOffset);
	TestEqual(
		TEXT("E73 root-shell tracking error is zero at the preserved actor-local offset"),
		MeasureRootShellTrackingErrorCm(
			InitialActorTransform,
			InitialActorLocalRootOffset,
			InitialExpectedRootWorld),
		0.0);
	const FTransform MovedAndTurnedActorTransform(
		FRotator(0.0f, 73.0f, 0.0f),
		FVector(-425.0f, 810.0f, 300.0f));
	const FVector MovedExpectedRootWorld = MovedAndTurnedActorTransform.TransformPosition(InitialActorLocalRootOffset);
	TestEqual(
		TEXT("E73 root-shell tracking is invariant to shell translation and yaw"),
		MeasureRootShellTrackingErrorCm(
			MovedAndTurnedActorTransform,
			InitialActorLocalRootOffset,
			MovedExpectedRootWorld),
		0.0);
	TestEqual(
		TEXT("E73 five-centimeter planar root displacement reports five-centimeter tracking error"),
		MeasureRootShellTrackingErrorCm(
			MovedAndTurnedActorTransform,
			InitialActorLocalRootOffset,
			MovedExpectedRootWorld + FVector(3.0f, 4.0f, 25.0f)),
		5.0);

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
		TEXT("Manny local-frame round-trip tracing is disabled without its explicit development flag"),
		Component->IsMannyLocalFrameRoundtripTraceEnabledForTesting());
	Component->ApplyProductVariantFromCommandLineForTesting(
		TEXT("-PhysAnimProductVariant=RealOnnxPolicy -PhysAnimMannyLocalFrameRoundtripTrace"));
	TestTrue(
		TEXT("The explicit development flag enables Manny local-frame round-trip tracing"),
		Component->IsMannyLocalFrameRoundtripTraceEnabledForTesting());
	Component->ApplyProductVariantFromCommandLineForTesting(TEXT("-PhysAnimProductVariant=RealOnnxPolicy"));
	TestFalse(
		TEXT("Removing the development flag restores the Manny round-trip trace-off default"),
		Component->IsMannyLocalFrameRoundtripTraceEnabledForTesting());
	TestFalse(
		TEXT("Experimental component action axis is disabled by default"),
		Component->IsExperimentalComponentActionAxisEnabledForTesting());
	Component->ApplyProductVariantFromCommandLineForTesting(
		TEXT("-PhysAnimProductVariant=RealOnnxPolicy -PhysAnimExperimentalComponentActionAxis"));
	TestTrue(
		TEXT("Explicit development flag enables the component-space action axis"),
		Component->IsExperimentalComponentActionAxisEnabledForTesting());
	TestFalse(
		TEXT("Configured component-space action axis remains inactive during standing preparation"),
		UPhysAnimComponent::ShouldUseExperimentalComponentActionAxisForRuntimeStateForTesting(
			true,
			EPhysAnimRuntimeState::Standing_Preparation));
	TestFalse(
		TEXT("Configured component-space action axis remains inactive during full simulation activation"),
		UPhysAnimComponent::ShouldUseExperimentalComponentActionAxisForRuntimeStateForTesting(
			true,
			EPhysAnimRuntimeState::Standing_FullSimulationActivation));
	TestFalse(
		TEXT("Configured component-space action axis remains inactive during policy blend"),
		UPhysAnimComponent::ShouldUseExperimentalComponentActionAxisForRuntimeStateForTesting(
			true,
			EPhysAnimRuntimeState::Standing_PolicyBlend));
	TestTrue(
		TEXT("Configured component-space action axis becomes effective only in active standing"),
		UPhysAnimComponent::ShouldUseExperimentalComponentActionAxisForRuntimeStateForTesting(
			true,
			EPhysAnimRuntimeState::BalanceActive_Standing));
	TestFalse(
		TEXT("Unconfigured component-space action axis remains inactive in active standing"),
		UPhysAnimComponent::ShouldUseExperimentalComponentActionAxisForRuntimeStateForTesting(
			false,
			EPhysAnimRuntimeState::BalanceActive_Standing));
	Component->ApplyProductVariantFromCommandLineForTesting(TEXT("-PhysAnimProductVariant=RealOnnxPolicy"));
	TestFalse(
		TEXT("Omitting the development flag restores the cached-world action-axis path"),
		Component->IsExperimentalComponentActionAxisEnabledForTesting());
	TestFalse(
		TEXT("First-policy component-axis override is disabled by default"),
		Component->IsExperimentalComponentActionAxisFromFirstPolicyEnabledForTesting());
	Component->ApplyProductVariantFromCommandLineForTesting(
		TEXT("-PhysAnimProductVariant=RealOnnxPolicy -PhysAnimExperimentalComponentActionAxisFromFirstPolicy"));
	TestTrue(
		TEXT("Explicit development flag enables component-axis composition from first policy"),
		Component->IsExperimentalComponentActionAxisFromFirstPolicyEnabledForTesting());
	TestFalse(
		TEXT("First-policy component-axis composition remains inactive before standing activation"),
		UPhysAnimComponent::ShouldUseExperimentalComponentActionAxisFromFirstPolicyForRuntimeStateForTesting(
			true,
			EPhysAnimRuntimeState::RuntimeReady));
	TestTrue(
		TEXT("First-policy component-axis composition is active during standing preparation"),
		UPhysAnimComponent::ShouldUseExperimentalComponentActionAxisFromFirstPolicyForRuntimeStateForTesting(
			true,
			EPhysAnimRuntimeState::Standing_Preparation));
	TestTrue(
		TEXT("First-policy component-axis composition remains active during policy blend"),
		UPhysAnimComponent::ShouldUseExperimentalComponentActionAxisFromFirstPolicyForRuntimeStateForTesting(
			true,
			EPhysAnimRuntimeState::Standing_PolicyBlend));
	TestTrue(
		TEXT("First-policy component-axis composition remains active in standing"),
		UPhysAnimComponent::ShouldUseExperimentalComponentActionAxisFromFirstPolicyForRuntimeStateForTesting(
			true,
			EPhysAnimRuntimeState::BalanceActive_Standing));
	TestFalse(
		TEXT("Unconfigured first-policy component-axis composition remains inactive"),
		UPhysAnimComponent::ShouldUseExperimentalComponentActionAxisFromFirstPolicyForRuntimeStateForTesting(
			false,
			EPhysAnimRuntimeState::Standing_Preparation));
	TestFalse(
		TEXT("First-policy bind-neutral override is disabled by default after flag reset"),
		Component->IsExperimentalBindNeutralFromFirstPolicyEnabledForTesting());
	Component->ApplyProductVariantFromCommandLineForTesting(
		TEXT("-PhysAnimProductVariant=RealOnnxPolicy -PhysAnimExperimentalBindNeutralFromFirstPolicy"));
	TestTrue(
		TEXT("Explicit development flag enables bind neutral from first policy"),
		Component->IsExperimentalBindNeutralFromFirstPolicyEnabledForTesting());
	TestFalse(
		TEXT("Bind neutral remains inactive before standing activation"),
		UPhysAnimComponent::ShouldUseExperimentalBindNeutralFromFirstPolicyForRuntimeStateForTesting(
			true,
			EPhysAnimRuntimeState::RuntimeReady));
	TestTrue(
		TEXT("Bind neutral is active during standing preparation"),
		UPhysAnimComponent::ShouldUseExperimentalBindNeutralFromFirstPolicyForRuntimeStateForTesting(
			true,
			EPhysAnimRuntimeState::Standing_Preparation));
	TestTrue(
		TEXT("Bind neutral remains active during policy blend"),
		UPhysAnimComponent::ShouldUseExperimentalBindNeutralFromFirstPolicyForRuntimeStateForTesting(
			true,
			EPhysAnimRuntimeState::Standing_PolicyBlend));
	TestTrue(
		TEXT("Bind neutral remains active in standing"),
		UPhysAnimComponent::ShouldUseExperimentalBindNeutralFromFirstPolicyForRuntimeStateForTesting(
			true,
			EPhysAnimRuntimeState::BalanceActive_Standing));
	TestFalse(
		TEXT("Constraint-range-remap bypass is disabled by default after flag reset"),
		Component->IsExperimentalConstraintRangeRemapBypassFromFirstPolicyEnabledForTesting());
	Component->ApplyProductVariantFromCommandLineForTesting(
		TEXT("-PhysAnimProductVariant=RealOnnxPolicy -PhysAnimExperimentalConstraintRangeRemapBypassFromFirstPolicy"));
	TestTrue(
		TEXT("Explicit development flag enables constraint-range-remap bypass from first policy"),
		Component->IsExperimentalConstraintRangeRemapBypassFromFirstPolicyEnabledForTesting());
	TestFalse(
		TEXT("Constraint-range-remap bypass remains inactive before standing activation"),
		UPhysAnimComponent::ShouldBypassExperimentalConstraintRangeRemapFromFirstPolicyForRuntimeStateForTesting(
			true,
			EPhysAnimRuntimeState::RuntimeReady));
	TestTrue(
		TEXT("Constraint-range-remap bypass is active during standing preparation"),
		UPhysAnimComponent::ShouldBypassExperimentalConstraintRangeRemapFromFirstPolicyForRuntimeStateForTesting(
			true,
			EPhysAnimRuntimeState::Standing_Preparation));
	TestTrue(
		TEXT("Constraint-range-remap bypass remains active in standing"),
		UPhysAnimComponent::ShouldBypassExperimentalConstraintRangeRemapFromFirstPolicyForRuntimeStateForTesting(
			true,
			EPhysAnimRuntimeState::BalanceActive_Standing));
	TestFalse(
		TEXT("Causal standing neck/head restoration is disabled by default after flag reset"),
		Component->IsExperimentalCausalStandingNeckHeadEnabledForTesting());
	Component->ApplyProductVariantFromCommandLineForTesting(
		TEXT("-PhysAnimProductVariant=RealOnnxPolicy -PhysAnimExperimentalCausalStandingUpperBody=NeckHead"));
	TestTrue(
		TEXT("Explicit development option enables causal standing neck/head restoration"),
		Component->IsExperimentalCausalStandingNeckHeadEnabledForTesting());
	Component->ApplyProductVariantFromCommandLineForTesting(
		TEXT("-PhysAnimProductVariant=RealOnnxPolicy -PhysAnimExperimentalCausalStandingUpperBody=Neck"));
	TestTrue(
		TEXT("E35 development option enables neck restoration"),
		Component->IsExperimentalCausalStandingNeckEnabledForTesting());
	TestFalse(
		TEXT("E35 neck option keeps head restoration disabled"),
		Component->IsExperimentalCausalStandingHeadEnabledForTesting());
	Component->ApplyProductVariantFromCommandLineForTesting(
		TEXT("-PhysAnimProductVariant=RealOnnxPolicy -PhysAnimExperimentalCausalStandingUpperBody=Head"));
	TestFalse(
		TEXT("E36 head option keeps neck restoration disabled"),
		Component->IsExperimentalCausalStandingNeckEnabledForTesting());
	TestTrue(
		TEXT("E36 development option enables head restoration"),
		Component->IsExperimentalCausalStandingHeadEnabledForTesting());
	Component->ApplyProductVariantFromCommandLineForTesting(
		TEXT("-PhysAnimProductVariant=RealOnnxPolicy -PhysAnimExperimentalCausalStandingUpperBody=HeadActiveOnly"));
	TestTrue(
		TEXT("E37 active-only option enables head restoration"),
		Component->IsExperimentalCausalStandingHeadEnabledForTesting());
	TestTrue(
		TEXT("E37 active-only option records delayed activation"),
		Component->IsExperimentalCausalStandingHeadActiveOnlyEnabledForTesting());
	TestFalse(
		TEXT("E37 keeps head masked during standing preparation"),
		UPhysAnimComponent::ShouldRestoreExperimentalCausalStandingHeadForRuntimeStateForTesting(
			true,
			true,
			EPhysAnimRuntimeState::Standing_Preparation));
	TestFalse(
		TEXT("E37 keeps head masked during full simulation activation"),
		UPhysAnimComponent::ShouldRestoreExperimentalCausalStandingHeadForRuntimeStateForTesting(
			true,
			true,
			EPhysAnimRuntimeState::Standing_FullSimulationActivation));
	TestFalse(
		TEXT("E37 keeps head masked during policy blend"),
		UPhysAnimComponent::ShouldRestoreExperimentalCausalStandingHeadForRuntimeStateForTesting(
			true,
			true,
			EPhysAnimRuntimeState::Standing_PolicyBlend));
	TestTrue(
		TEXT("E37 restores head only in active standing"),
		UPhysAnimComponent::ShouldRestoreExperimentalCausalStandingHeadForRuntimeStateForTesting(
			true,
			true,
			EPhysAnimRuntimeState::BalanceActive_Standing));
	TestTrue(
		TEXT("Non-delayed head mode remains active throughout standing activation"),
		UPhysAnimComponent::ShouldRestoreExperimentalCausalStandingHeadForRuntimeStateForTesting(
			true,
			false,
			EPhysAnimRuntimeState::Standing_Preparation));
	TestFalse(
		TEXT("Disabled head restoration remains inactive"),
		UPhysAnimComponent::ShouldRestoreExperimentalCausalStandingHeadForRuntimeStateForTesting(
			false,
			false,
			EPhysAnimRuntimeState::BalanceActive_Standing));
	Component->ApplyProductVariantFromCommandLineForTesting(
		TEXT("-PhysAnimProductVariant=RealOnnxPolicy -PhysAnimExperimentalCausalStandingUpperBody=HeadAfterFirstPolicy"));
	TestTrue(
		TEXT("E38 option enables head restoration"),
		Component->IsExperimentalCausalStandingHeadEnabledForTesting());
	TestTrue(
		TEXT("E38 option records post-snapshot activation"),
		Component->IsExperimentalCausalStandingHeadAfterFirstPolicyEnabledForTesting());
	TestFalse(
		TEXT("E38 keeps head masked on the first active-policy inference"),
		UPhysAnimComponent::ShouldRestoreExperimentalCausalStandingHeadAfterFirstPolicyForTesting(
			true,
			false,
			EPhysAnimRuntimeState::BalanceActive_Standing));
	TestTrue(
		TEXT("E38 restores head after the first active-policy snapshot existed before inference"),
		UPhysAnimComponent::ShouldRestoreExperimentalCausalStandingHeadAfterFirstPolicyForTesting(
			true,
			true,
			EPhysAnimRuntimeState::BalanceActive_Standing));
	TestFalse(
		TEXT("E38 remains inactive outside active standing after capture"),
		UPhysAnimComponent::ShouldRestoreExperimentalCausalStandingHeadAfterFirstPolicyForTesting(
			true,
			true,
			EPhysAnimRuntimeState::Standing_PolicyBlend));
	TestFalse(
		TEXT("E47 production Head remains masked on the first active-policy inference"),
		UPhysAnimComponent::ShouldRestoreCausalStandingHeadAfterFirstPolicy(
			false,
			EPhysAnimRuntimeState::BalanceActive_Standing));
	TestTrue(
		TEXT("E47 production Head restores after an earlier active-policy inference"),
		UPhysAnimComponent::ShouldRestoreCausalStandingHeadAfterFirstPolicy(
			true,
			EPhysAnimRuntimeState::BalanceActive_Standing));
	TestFalse(
		TEXT("E47 production Head remains masked outside active standing"),
		UPhysAnimComponent::ShouldRestoreCausalStandingHeadAfterFirstPolicy(
			true,
			EPhysAnimRuntimeState::Standing_PolicyBlend));
	TestEqual(
		TEXT("E51 production Neck remains masked on the first active-policy inference"),
		UPhysAnimComponent::ResolveCausalStandingNeckScaleAfterFirstPolicy(
			false,
			EPhysAnimRuntimeState::BalanceActive_Standing),
		0.0f);
	TestEqual(
		TEXT("E51 production Neck restores at the validated quarter scale after an earlier active-policy inference"),
		UPhysAnimComponent::ResolveCausalStandingNeckScaleAfterFirstPolicy(
			true,
			EPhysAnimRuntimeState::BalanceActive_Standing),
		0.25f);
	TestEqual(
		TEXT("E51 production Neck remains masked outside active standing"),
		UPhysAnimComponent::ResolveCausalStandingNeckScaleAfterFirstPolicy(
			true,
			EPhysAnimRuntimeState::Standing_PolicyBlend),
		0.0f);
	TestEqual(
		TEXT("E53 production left proximal remains masked on the first active-policy inference"),
		UPhysAnimComponent::ResolveCausalStandingLeftProximalScaleAfterFirstPolicy(
			false,
			EPhysAnimRuntimeState::BalanceActive_Standing),
		0.0f);
	TestEqual(
		TEXT("E53 production left proximal restores at the validated quarter scale after an earlier active-policy inference"),
		UPhysAnimComponent::ResolveCausalStandingLeftProximalScaleAfterFirstPolicy(
			true,
			EPhysAnimRuntimeState::BalanceActive_Standing),
		0.25f);
	TestEqual(
		TEXT("E53 production left proximal remains masked outside active standing"),
		UPhysAnimComponent::ResolveCausalStandingLeftProximalScaleAfterFirstPolicy(
			true,
			EPhysAnimRuntimeState::Standing_PolicyBlend),
		0.0f);
	TestEqual(
		TEXT("E55 production right proximal remains masked on the first active-policy inference"),
		UPhysAnimComponent::ResolveCausalStandingRightProximalScaleAfterFirstPolicy(
			false,
			EPhysAnimRuntimeState::BalanceActive_Standing),
		0.0f);
	TestEqual(
		TEXT("E55 production right proximal restores at the validated scale after an earlier active-policy inference"),
		UPhysAnimComponent::ResolveCausalStandingRightProximalScaleAfterFirstPolicy(
			true,
			EPhysAnimRuntimeState::BalanceActive_Standing),
		0.03125f);
	TestEqual(
		TEXT("E55 production right proximal remains masked outside active standing"),
		UPhysAnimComponent::ResolveCausalStandingRightProximalScaleAfterFirstPolicy(
			true,
			EPhysAnimRuntimeState::Standing_PolicyBlend),
		0.0f);
	TestEqual(
		TEXT("E58 production Torso remains masked on the first active-policy inference"),
		UPhysAnimComponent::ResolveCausalStandingTorsoScaleAfterFirstPolicy(
			false,
			EPhysAnimRuntimeState::BalanceActive_Standing),
		0.0f);
	TestEqual(
		TEXT("E58 production Torso restores at the validated tiny scale after an earlier active-policy inference"),
		UPhysAnimComponent::ResolveCausalStandingTorsoScaleAfterFirstPolicy(
			true,
			EPhysAnimRuntimeState::BalanceActive_Standing),
		0.0000152587890625f);
	TestEqual(
		TEXT("E58 production Torso remains masked outside active standing"),
		UPhysAnimComponent::ResolveCausalStandingTorsoScaleAfterFirstPolicy(
			true,
			EPhysAnimRuntimeState::Standing_PolicyBlend),
		0.0f);
	Component->ApplyProductVariantFromCommandLineForTesting(
		TEXT("-PhysAnimProductVariant=RealOnnxPolicy -PhysAnimExperimentalCausalStandingUpperBody=SpineChest"));
	TestTrue(
		TEXT("E39 development option enables spine/chest restoration"),
		Component->IsExperimentalCausalStandingSpineChestEnabledForTesting());
	TestFalse(
		TEXT("E39 spine/chest option keeps neck restoration disabled"),
		Component->IsExperimentalCausalStandingNeckEnabledForTesting());
	TestFalse(
		TEXT("E39 spine/chest option keeps head restoration disabled"),
		Component->IsExperimentalCausalStandingHeadEnabledForTesting());
	Component->ApplyProductVariantFromCommandLineForTesting(
		TEXT("-PhysAnimProductVariant=RealOnnxPolicy -PhysAnimExperimentalCausalStandingUpperBody=DistalHands"));
	TestTrue(
		TEXT("E41 development option enables distal hand restoration"),
		Component->IsExperimentalCausalStandingDistalHandsEnabledForTesting());
	TestFalse(
		TEXT("E41 distal hand option keeps neck restoration disabled"),
		Component->IsExperimentalCausalStandingNeckEnabledForTesting());
	TestFalse(
		TEXT("E41 distal hand option keeps head restoration disabled"),
		Component->IsExperimentalCausalStandingHeadEnabledForTesting());
	Component->ApplyProductVariantFromCommandLineForTesting(
		TEXT("-PhysAnimProductVariant=RealOnnxPolicy -PhysAnimExperimentalCausalStandingScaledRegion=Neck -PhysAnimExperimentalCausalStandingScaledScale=0.5"));
	TestEqual(
		TEXT("E48 parser selects the scaled Neck region"),
		Component->GetExperimentalCausalStandingScaledRegionForTesting(),
		EPhysAnimExperimentalCausalStandingScaledRegion::Neck);
	TestTrue(
		TEXT("E48 parser preserves the requested region scale"),
		FMath::IsNearlyEqual(Component->GetExperimentalCausalStandingScaledScaleForTesting(), 0.5f));
	TArray<float> FirstActiveCaptureSource;
	FirstActiveCaptureSource.SetNumUninitialized(PhysAnimBridge::NumActionFloats);
	for (int32 Index = 0; Index < FirstActiveCaptureSource.Num(); ++Index)
	{
		FirstActiveCaptureSource[Index] = static_cast<float>(Index + 1);
	}
	bool bFirstActiveCaptureRecorded = false;
	TArray<float> FirstActiveCapturedActions;
	TestFalse(
		TEXT("First active conditioned-action evidence ignores non-active samples"),
		UPhysAnimComponent::CaptureFirstActiveStandingConditionedActionsForTesting(
			false,
			FirstActiveCaptureSource,
			bFirstActiveCaptureRecorded,
			FirstActiveCapturedActions));
	TestTrue(
		TEXT("First active conditioned-action evidence captures the first active sample"),
		UPhysAnimComponent::CaptureFirstActiveStandingConditionedActionsForTesting(
			true,
			FirstActiveCaptureSource,
			bFirstActiveCaptureRecorded,
			FirstActiveCapturedActions));
	TestTrue(TEXT("First active conditioned-action evidence records capture state"), bFirstActiveCaptureRecorded);
	TestEqual(TEXT("First active conditioned-action evidence preserves action width"), FirstActiveCapturedActions.Num(), FirstActiveCaptureSource.Num());
	TestEqual(TEXT("First active conditioned-action evidence preserves Head X"), FirstActiveCapturedActions[36], FirstActiveCaptureSource[36]);
	TArray<float> ReplacementActions;
	ReplacementActions.Init(-1.0f, PhysAnimBridge::NumActionFloats);
	TestFalse(
		TEXT("First active conditioned-action evidence does not overwrite the first sample"),
		UPhysAnimComponent::CaptureFirstActiveStandingConditionedActionsForTesting(
			true,
			ReplacementActions,
			bFirstActiveCaptureRecorded,
			FirstActiveCapturedActions));
	TestEqual(TEXT("First active conditioned-action evidence remains immutable"), FirstActiveCapturedActions[36], FirstActiveCaptureSource[36]);
	Component->ApplyProductVariantFromCommandLineForTesting(
		TEXT("-PhysAnimProductVariant=RealOnnxPolicy -PhysAnimExperimentalActionFamily=LowerOnly"));
	TestEqual(
		TEXT("Command line selects the lower-body-only action mask"),
		Component->GetExperimentalActionFamilyMaskForTesting(),
		EPhysAnimExperimentalActionFamilyMask::LowerOnly);
	TArray<float> FamilyActions;
	FamilyActions.SetNumUninitialized(PhysAnimBridge::NumActionFloats);
	for (int32 Index = 0; Index < FamilyActions.Num(); ++Index)
	{
		FamilyActions[Index] = static_cast<float>(Index + 1);
	}
	TArray<float> ProductionCompatibleActions = FamilyActions;
	UPhysAnimComponent::ApplyCausalStandingPolicyActionCompatibility(
		true,
		ProductionCompatibleActions);
	for (int32 Index = 0; Index < ProductionCompatibleActions.Num(); ++Index)
	{
		const int32 JointIndex = Index / 3;
		const bool bExpectedRetained =
			JointIndex < 8 ||
			(JointIndex >= 9 && JointIndex < 11) ||
			(JointIndex >= 16 && JointIndex < 18) ||
			(JointIndex >= 21 && JointIndex < 23);
		TestEqual(
			*FString::Printf(TEXT("Production standing-policy action scalar %d matches promoted Spine+Chest+DistalHands mask"), Index),
			ProductionCompatibleActions[Index],
			bExpectedRetained ? FamilyActions[Index] : 0.0f);
	}
	TArray<float> NeckHeadRestoredActions = FamilyActions;
	UPhysAnimComponent::ApplyCausalStandingPolicyActionCompatibility(
		true,
		true,
		NeckHeadRestoredActions);
	for (int32 Index = 0; Index < NeckHeadRestoredActions.Num(); ++Index)
	{
		const int32 JointIndex = Index / 3;
		const bool bExpectedRetained = JointIndex < 8 || (JointIndex >= 11 && JointIndex < 13);
		TestEqual(
			*FString::Printf(TEXT("E34 neck/head candidate action scalar %d matches preregistered mask"), Index),
			NeckHeadRestoredActions[Index],
			bExpectedRetained ? FamilyActions[Index] : 0.0f);
	}
	TArray<float> NeckRestoredActions = FamilyActions;
	UPhysAnimComponent::ApplyCausalStandingPolicyActionCompatibility(
		true,
		true,
		false,
		NeckRestoredActions);
	for (int32 Index = 0; Index < NeckRestoredActions.Num(); ++Index)
	{
		const int32 JointIndex = Index / 3;
		const bool bExpectedRetained = JointIndex < 8 || JointIndex == 11;
		TestEqual(
			*FString::Printf(TEXT("E35 neck candidate action scalar %d matches preregistered mask"), Index),
			NeckRestoredActions[Index],
			bExpectedRetained ? FamilyActions[Index] : 0.0f);
	}
	TArray<float> HeadRestoredActions = FamilyActions;
	UPhysAnimComponent::ApplyCausalStandingPolicyActionCompatibility(
		true,
		false,
		true,
		HeadRestoredActions);
	for (int32 Index = 0; Index < HeadRestoredActions.Num(); ++Index)
	{
		const int32 JointIndex = Index / 3;
		const bool bExpectedRetained = JointIndex < 8 || JointIndex == 12;
		TestEqual(
			*FString::Printf(TEXT("E36 head candidate action scalar %d matches preregistered mask"), Index),
			HeadRestoredActions[Index],
			bExpectedRetained ? FamilyActions[Index] : 0.0f);
	}
	TArray<float> SpineChestRestoredActions = FamilyActions;
	UPhysAnimComponent::ApplyCausalStandingPolicyActionCompatibility(
		true,
		true,
		false,
		false,
		SpineChestRestoredActions);
	for (int32 Index = 0; Index < SpineChestRestoredActions.Num(); ++Index)
	{
		const int32 JointIndex = Index / 3;
		const bool bExpectedRetained = JointIndex < 8 || (JointIndex >= 9 && JointIndex < 11);
		TestEqual(
			*FString::Printf(TEXT("E39 spine/chest candidate action scalar %d matches preregistered mask"), Index),
			SpineChestRestoredActions[Index],
			bExpectedRetained ? FamilyActions[Index] : 0.0f);
	}
	TArray<float> DistalHandsRestoredActions = FamilyActions;
	UPhysAnimComponent::ApplyCausalStandingPolicyActionCompatibility(
		true,
		true,
		false,
		false,
		true,
		DistalHandsRestoredActions);
	for (int32 Index = 0; Index < DistalHandsRestoredActions.Num(); ++Index)
	{
		const int32 JointIndex = Index / 3;
		const bool bExpectedRetained =
			JointIndex < 8 ||
			(JointIndex >= 9 && JointIndex < 11) ||
			(JointIndex >= 16 && JointIndex < 18) ||
			(JointIndex >= 21 && JointIndex < 23);
		TestEqual(
			*FString::Printf(TEXT("E41 distal hand candidate action scalar %d matches preregistered mask"), Index),
			DistalHandsRestoredActions[Index],
			bExpectedRetained ? FamilyActions[Index] : 0.0f);
	}
	TArray<float> ScaledRegionActions = FamilyActions;
	UPhysAnimComponent::ApplyCausalStandingPolicyActionScales(
		true,
		0.25f,
		1.0f,
		0.5f,
		1.0f,
		0.75f,
		1.0f,
		0.6f,
		1.0f,
		ScaledRegionActions);
	for (int32 Index = 0; Index < ScaledRegionActions.Num(); ++Index)
	{
		const int32 JointIndex = Index / 3;
		float ExpectedScale = 1.0f;
		if (JointIndex == 8)
		{
			ExpectedScale = 0.25f;
		}
		else if (JointIndex >= 9 && JointIndex < 11)
		{
			ExpectedScale = 1.0f;
		}
		else if (JointIndex == 11)
		{
			ExpectedScale = 0.5f;
		}
		else if (JointIndex == 12)
		{
			ExpectedScale = 1.0f;
		}
		else if (JointIndex >= 13 && JointIndex < 16)
		{
			ExpectedScale = 0.75f;
		}
		else if (JointIndex >= 16 && JointIndex < 18)
		{
			ExpectedScale = 1.0f;
		}
		else if (JointIndex >= 18 && JointIndex < 21)
		{
			ExpectedScale = 0.6f;
		}
		else if (JointIndex >= 21)
		{
			ExpectedScale = 1.0f;
		}
		TestEqual(
			*FString::Printf(TEXT("E48 scaled action scalar %d matches its isolated region scale"), Index),
			ScaledRegionActions[Index],
			FamilyActions[Index] * ExpectedScale);
	}
	TArray<float> NonPolicyCompatibleActions = FamilyActions;
	UPhysAnimComponent::ApplyCausalStandingPolicyActionCompatibility(
		false,
		NonPolicyCompatibleActions);
	for (int32 Index = 0; Index < NonPolicyCompatibleActions.Num(); ++Index)
	{
		TestEqual(
			*FString::Printf(TEXT("Non-policy action scalar %d is preserved"), Index),
			NonPolicyCompatibleActions[Index],
			FamilyActions[Index]);
	}
	TestTrue(
		TEXT("E63 active standing remains a causal policy-control state"),
		UPhysAnimComponent::IsCausalPolicyControlRuntimeState(
			EPhysAnimRuntimeState::BalanceActive_Standing));
	TestTrue(
		TEXT("E63 active shell locomotion continues the causal policy-control state"),
		UPhysAnimComponent::IsCausalPolicyControlRuntimeState(
			EPhysAnimRuntimeState::LocomotionActiveShell));
	TestFalse(
		TEXT("E63 denied locomotion is not a causal policy-control state"),
		UPhysAnimComponent::IsCausalPolicyControlRuntimeState(
			EPhysAnimRuntimeState::LocomotionActiveShellDenied));
	TestTrue(
		TEXT("E64 standing uses the locked idle Pose Search path"),
		UPhysAnimComponent::ShouldUseBalanceIdlePoseSearchState(
			EPhysAnimRuntimeState::BalanceActive_Standing));
	TestFalse(
		TEXT("E64 active locomotion uses trajectory-driven Pose Search"),
		UPhysAnimComponent::ShouldUseBalanceIdlePoseSearchState(
			EPhysAnimRuntimeState::LocomotionActiveShell));
	TestFalse(
		TEXT("E64 denied locomotion does not masquerade as standing idle Pose Search"),
		UPhysAnimComponent::ShouldUseBalanceIdlePoseSearchState(
			EPhysAnimRuntimeState::LocomotionActiveShellDenied));
	TestTrue(
		TEXT("E65 active locomotion routes to bridge-trajectory Pose Search"),
		UPhysAnimComponent::ShouldUseBridgeTrajectoryPoseSearchState(
			EPhysAnimRuntimeState::LocomotionActiveShell));
	TestFalse(
		TEXT("E65 standing does not route to bridge-trajectory Pose Search"),
		UPhysAnimComponent::ShouldUseBridgeTrajectoryPoseSearchState(
			EPhysAnimRuntimeState::BalanceActive_Standing));
	TestFalse(
		TEXT("E65 denied locomotion does not route to bridge-trajectory Pose Search"),
		UPhysAnimComponent::ShouldUseBridgeTrajectoryPoseSearchState(
			EPhysAnimRuntimeState::LocomotionActiveShellDenied));
	TestEqual(
		TEXT("E63 production locomotion retains proven policy authority"),
		UPhysAnimComponent::ResolveCausalStandingPolicyStrengthFactor(
			true,
			true,
			EPhysAnimRuntimeState::LocomotionActiveShell),
		1.5f);
	TestTrue(
		TEXT("E63 production component action axis remains active during locomotion"),
		UPhysAnimComponent::ShouldUseCausalStandingComponentActionAxis(
			EPhysAnimRuntimeState::LocomotionActiveShell));
	TestTrue(
		TEXT("E63 delayed Head remains restored during locomotion"),
		UPhysAnimComponent::ShouldRestoreCausalStandingHeadAfterFirstPolicy(
			true,
			EPhysAnimRuntimeState::LocomotionActiveShell));
	TestEqual(
		TEXT("E63 validated Neck scale remains active during locomotion"),
		UPhysAnimComponent::ResolveCausalStandingNeckScaleAfterFirstPolicy(
			true,
			EPhysAnimRuntimeState::LocomotionActiveShell),
		0.25f);
	TestEqual(
		TEXT("E63 validated right proximal scale remains active during locomotion"),
		UPhysAnimComponent::ResolveCausalStandingRightProximalScaleAfterFirstPolicy(
			true,
			EPhysAnimRuntimeState::LocomotionActiveShell),
		0.03125f);
	TestEqual(
		TEXT("E63 validated Torso scale remains active during locomotion"),
		UPhysAnimComponent::ResolveCausalStandingTorsoScaleAfterFirstPolicy(
			true,
			EPhysAnimRuntimeState::LocomotionActiveShell),
		0.0000152587890625f);
	TestEqual(
		TEXT("Production policy authority is identity before active capture"),
		UPhysAnimComponent::ResolveCausalStandingPolicyStrengthFactor(
			true,
			false,
			EPhysAnimRuntimeState::BalanceActive_Standing),
		1.0f);
	TestEqual(
		TEXT("Production policy authority is identity outside active standing"),
		UPhysAnimComponent::ResolveCausalStandingPolicyStrengthFactor(
			true,
			true,
			EPhysAnimRuntimeState::Standing_PolicyBlend),
		1.0f);
	TestEqual(
		TEXT("Production nonzero standing policy receives proven authority"),
		UPhysAnimComponent::ResolveCausalStandingPolicyStrengthFactor(
			true,
			true,
			EPhysAnimRuntimeState::BalanceActive_Standing),
		1.5f);
	TestEqual(
		TEXT("Zero or passive standing mode retains identity authority"),
		UPhysAnimComponent::ResolveCausalStandingPolicyStrengthFactor(
			false,
			true,
			EPhysAnimRuntimeState::BalanceActive_Standing),
		1.0f);
	TestFalse(
		TEXT("Production component action axis is inactive before standing activation"),
		UPhysAnimComponent::ShouldUseCausalStandingComponentActionAxis(
			EPhysAnimRuntimeState::RuntimeReady));
	TestTrue(
		TEXT("Production component action axis is active during standing preparation"),
		UPhysAnimComponent::ShouldUseCausalStandingComponentActionAxis(
			EPhysAnimRuntimeState::Standing_Preparation));
	TestTrue(
		TEXT("Production component action axis remains active during full simulation activation"),
		UPhysAnimComponent::ShouldUseCausalStandingComponentActionAxis(
			EPhysAnimRuntimeState::Standing_FullSimulationActivation));
	TestTrue(
		TEXT("Production component action axis remains active during policy blend"),
		UPhysAnimComponent::ShouldUseCausalStandingComponentActionAxis(
			EPhysAnimRuntimeState::Standing_PolicyBlend));
	TestTrue(
		TEXT("Production component action axis remains active during active standing"),
		UPhysAnimComponent::ShouldUseCausalStandingComponentActionAxis(
			EPhysAnimRuntimeState::BalanceActive_Standing));
	const FQuat ActionBindComponentWorldRotation(
		FVector::UpVector,
		FMath::DegreesToRadians(23.0f));
	const FQuat CachedWorldActionAxisRotation(
		FVector::RightVector,
		FMath::DegreesToRadians(-31.0f));
	const FQuat ProductionComponentActionAxis =
		UPhysAnimComponent::ExpressCachedWorldActionAxisInMeshComponent(
			ActionBindComponentWorldRotation,
			CachedWorldActionAxisRotation);
	const FQuat ValidatedComponentActionAxis =
		UPhysAnimComponent::ExpressCachedWorldActionAxisInMeshComponentForTesting(
			ActionBindComponentWorldRotation,
			CachedWorldActionAxisRotation);
	TestTrue(
		TEXT("Production action-axis transform matches the validated E18 adapter"),
		ProductionComponentActionAxis.Equals(ValidatedComponentActionAxis, 1.0e-6f));
	const FQuat MannyNeutralParentRelativeRotation(
		FVector::ForwardVector,
		FMath::DegreesToRadians(17.0f));
	const FQuat ProtoPolicyRotationUe(
		FVector(1.0f, 1.0f, 0.5f).GetSafeNormal(),
		FMath::DegreesToRadians(11.0f));
	const FQuat ProductionPolicyTarget =
		UPhysAnimComponent::ComposeProtoPolicyTargetAroundMannyNeutralWithActionAxis(
			ProductionComponentActionAxis,
			MannyNeutralParentRelativeRotation,
			ProtoPolicyRotationUe);
	const FQuat ValidatedPolicyTarget =
		UPhysAnimComponent::ComposeProtoPolicyTargetAroundMannyNeutralWithActionAxisForTesting(
			ValidatedComponentActionAxis,
			MannyNeutralParentRelativeRotation,
			ProtoPolicyRotationUe);
	TestTrue(
		TEXT("Production target composition matches the validated E18 adapter"),
		ProductionPolicyTarget.Equals(ValidatedPolicyTarget, 1.0e-6f));
	TArray<float> LowerOnlyActions = FamilyActions;
	UPhysAnimComponent::ApplyExperimentalActionFamilyMaskForTesting(
		EPhysAnimExperimentalActionFamilyMask::LowerOnly,
		LowerOnlyActions);
	for (int32 Index = 0; Index < LowerOnlyActions.Num(); ++Index)
	{
		TestEqual(
			*FString::Printf(TEXT("Lower-only action scalar %d matches its anatomical mask"), Index),
			LowerOnlyActions[Index],
			Index < 24 ? FamilyActions[Index] : 0.0f);
	}
	TArray<float> AxialOnlyActions = FamilyActions;
	UPhysAnimComponent::ApplyExperimentalActionFamilyMaskForTesting(
		EPhysAnimExperimentalActionFamilyMask::AxialOnly,
		AxialOnlyActions);
	for (int32 Index = 0; Index < AxialOnlyActions.Num(); ++Index)
	{
		TestEqual(
			*FString::Printf(TEXT("Axial-only action scalar %d matches its anatomical mask"), Index),
			AxialOnlyActions[Index],
			(Index >= 24 && Index < 39) ? FamilyActions[Index] : 0.0f);
	}
	TArray<float> ArmsOnlyActions = FamilyActions;
	UPhysAnimComponent::ApplyExperimentalActionFamilyMaskForTesting(
		EPhysAnimExperimentalActionFamilyMask::ArmsOnly,
		ArmsOnlyActions);
	for (int32 Index = 0; Index < ArmsOnlyActions.Num(); ++Index)
	{
		TestEqual(
			*FString::Printf(TEXT("Arms-only action scalar %d matches its anatomical mask"), Index),
			ArmsOnlyActions[Index],
			Index >= 39 ? FamilyActions[Index] : 0.0f);
	}
	TArray<float> ZeroFamilyActions = FamilyActions;
	UPhysAnimComponent::ApplyExperimentalActionFamilyMaskForTesting(
		EPhysAnimExperimentalActionFamilyMask::Zero,
		ZeroFamilyActions);
	for (int32 Index = 0; Index < ZeroFamilyActions.Num(); ++Index)
	{
		TestEqual(
			*FString::Printf(TEXT("Zero-family action scalar %d is cleared"), Index),
			ZeroFamilyActions[Index],
			0.0f);
	}
	Component->ApplyProductVariantFromCommandLineForTesting(
		TEXT("-PhysAnimProductVariant=RealOnnxPolicy -PhysAnimExperimentalActionJointStart=11 -PhysAnimExperimentalActionJointCount=2"));
	TestEqual(
		TEXT("Command line selects the requested action-joint range start"),
		Component->GetExperimentalActionJointRangeStartForTesting(),
		11);
	TestEqual(
		TEXT("Command line selects the requested action-joint range count"),
		Component->GetExperimentalActionJointRangeCountForTesting(),
		2);
	TArray<float> NeckHeadRangeActions = FamilyActions;
	UPhysAnimComponent::ApplyExperimentalActionJointRangeForTesting(
		11,
		2,
		NeckHeadRangeActions);
	for (int32 Index = 0; Index < NeckHeadRangeActions.Num(); ++Index)
	{
		TestEqual(
			*FString::Printf(TEXT("Neck/head range scalar %d matches the requested joint interval"), Index),
			NeckHeadRangeActions[Index],
			(Index >= 33 && Index < 39) ? FamilyActions[Index] : 0.0f);
	}
	TArray<float> EmptyJointRangeActions = FamilyActions;
	UPhysAnimComponent::ApplyExperimentalActionJointRangeForTesting(
		0,
		0,
		EmptyJointRangeActions);
	for (int32 Index = 0; Index < EmptyJointRangeActions.Num(); ++Index)
	{
		TestEqual(
			*FString::Printf(TEXT("Empty action-joint range scalar %d is cleared"), Index),
			EmptyJointRangeActions[Index],
			0.0f);
	}
	TestFalse(
		TEXT("Policy action baseline residual is disabled by default"),
		Component->IsExperimentalPolicyActionBaselineResidualEnabledForTesting());
	Component->ApplyProductVariantFromCommandLineForTesting(
		TEXT("-PhysAnimProductVariant=RealOnnxPolicy -PhysAnimExperimentalPolicyActionBaselineResidual"));
	TestTrue(
		TEXT("Explicit development flag enables policy action baseline residual"),
		Component->IsExperimentalPolicyActionBaselineResidualEnabledForTesting());
	const TArray<float> BaselinePolicyActions = { 0.25f, -0.50f, 0.75f };
	TArray<float> ResidualPolicyActions = { 0.50f, -0.25f, 0.25f };
	TestTrue(
		TEXT("Matching policy baseline is applied when configured"),
		UPhysAnimComponent::ApplyExperimentalPolicyActionBaselineResidualForTesting(
			true,
			false,
			BaselinePolicyActions,
			ResidualPolicyActions));
	TestEqual(TEXT("Residual action zero subtracts baseline"), ResidualPolicyActions[0], 0.25f);
	TestEqual(TEXT("Residual action one subtracts baseline"), ResidualPolicyActions[1], 0.25f);
	TestEqual(TEXT("Residual action two subtracts baseline"), ResidualPolicyActions[2], -0.50f);
	TArray<float> DisabledResidualActions = { 0.50f, -0.25f, 0.25f };
	TestFalse(
		TEXT("Disabled policy baseline residual preserves actions"),
		UPhysAnimComponent::ApplyExperimentalPolicyActionBaselineResidualForTesting(
			false,
			false,
			BaselinePolicyActions,
			DisabledResidualActions));
	TestEqual(TEXT("Disabled residual preserves first action"), DisabledResidualActions[0], 0.50f);
	TArray<float> ZeroVariantActions = { 0.0f, 0.0f, 0.0f };
	TestFalse(
		TEXT("Forced-zero variant bypasses policy baseline residual"),
		UPhysAnimComponent::ApplyExperimentalPolicyActionBaselineResidualForTesting(
			true,
			true,
			BaselinePolicyActions,
			ZeroVariantActions));
	for (const float Value : ZeroVariantActions)
	{
		TestEqual(TEXT("Forced-zero action remains zero"), Value, 0.0f);
	}
	TArray<float> MismatchedResidualActions = { 0.50f, -0.25f };
	TestFalse(
		TEXT("Mismatched policy baseline size is rejected without mutation"),
		UPhysAnimComponent::ApplyExperimentalPolicyActionBaselineResidualForTesting(
			true,
			false,
			BaselinePolicyActions,
			MismatchedResidualActions));
	TestEqual(TEXT("Mismatched residual preserves action"), MismatchedResidualActions[0], 0.50f);
	TestFalse(
		TEXT("Policy action zero-until-baseline is disabled by default"),
		Component->IsExperimentalPolicyActionZeroUntilBaselineEnabledForTesting());
	Component->ApplyProductVariantFromCommandLineForTesting(
		TEXT("-PhysAnimProductVariant=RealOnnxPolicy -PhysAnimExperimentalPolicyActionZeroUntilBaseline"));
	TestTrue(
		TEXT("Explicit development flag enables policy action zero-until-baseline"),
		Component->IsExperimentalPolicyActionZeroUntilBaselineEnabledForTesting());
	TArray<float> PreBaselineActions = { 0.50f, -0.25f, 0.75f };
	TestTrue(
		TEXT("Configured pre-baseline policy actions are zeroed"),
		UPhysAnimComponent::ApplyExperimentalPolicyActionZeroUntilBaselineForTesting(
			true,
			false,
			false,
			PreBaselineActions));
	for (const float Value : PreBaselineActions)
	{
		TestEqual(TEXT("Pre-baseline action is exactly zero"), Value, 0.0f);
	}
	TArray<float> BaselineAvailableActions = { 0.50f, -0.25f, 0.75f };
	TestFalse(
		TEXT("Available baseline bypasses pre-baseline zeroing"),
		UPhysAnimComponent::ApplyExperimentalPolicyActionZeroUntilBaselineForTesting(
			true,
			true,
			false,
			BaselineAvailableActions));
	TestEqual(TEXT("Available-baseline action is preserved"), BaselineAvailableActions[0], 0.50f);
	TArray<float> ExplicitZeroActions = { 0.0f, 0.0f, 0.0f };
	TestFalse(
		TEXT("Explicit ZeroActions bypasses pre-baseline zeroing"),
		UPhysAnimComponent::ApplyExperimentalPolicyActionZeroUntilBaselineForTesting(
			true,
			false,
			true,
			ExplicitZeroActions));
	TestFalse(
		TEXT("Physics-body observation positions are disabled by default"),
		Component->IsExperimentalPhysicsBodyObservationPositionsEnabledForTesting());
	Component->ApplyProductVariantFromCommandLineForTesting(
		TEXT("-PhysAnimProductVariant=RealOnnxPolicy -PhysAnimExperimentalPhysicsBodyObservationPositions"));
	TestTrue(
		TEXT("Explicit development flag enables physics-body observation positions"),
		Component->IsExperimentalPhysicsBodyObservationPositionsEnabledForTesting());
	const FVector BoneObservationPosition(1.0, 2.0, 3.0);
	const FVector PhysicsBodyObservationPosition(4.0, 5.0, 6.0);
	TestTrue(
		TEXT("Disabled observation-position override preserves bone origin"),
		UPhysAnimComponent::SelectObservationWorldPositionForTesting(
			false,
			BoneObservationPosition,
			PhysicsBodyObservationPosition).Equals(BoneObservationPosition));
	TestTrue(
		TEXT("Enabled observation-position override selects physics-body origin"),
		UPhysAnimComponent::SelectObservationWorldPositionForTesting(
			true,
			BoneObservationPosition,
			PhysicsBodyObservationPosition).Equals(PhysicsBodyObservationPosition));
	TestEqual(
		TEXT("Experimental active strength factor defaults to identity"),
		Component->GetExperimentalActiveStrengthFactorForTesting(),
		1.0f);
	Component->ApplyProductVariantFromCommandLineForTesting(
		TEXT("-PhysAnimProductVariant=RealOnnxPolicy -PhysAnimExperimentalActiveStrengthFactor=1.5"));
	TestEqual(
		TEXT("Command line selects the experimental active strength factor"),
		Component->GetExperimentalActiveStrengthFactorForTesting(),
		1.5f);
	TestEqual(
		TEXT("Strength factor remains identity before active-standing capture"),
		UPhysAnimComponent::ResolveExperimentalActiveStrengthFactorForTesting(
			1.5f,
			false,
			EPhysAnimRuntimeState::BalanceActive_Standing),
		1.0f);
	TestEqual(
		TEXT("Strength factor remains identity outside active standing"),
		UPhysAnimComponent::ResolveExperimentalActiveStrengthFactorForTesting(
			1.5f,
			true,
			EPhysAnimRuntimeState::Standing_PolicyBlend),
		1.0f);
	TestEqual(
		TEXT("Configured strength factor activates after active-standing capture"),
		UPhysAnimComponent::ResolveExperimentalActiveStrengthFactorForTesting(
			1.5f,
			true,
			EPhysAnimRuntimeState::BalanceActive_Standing),
		1.5f);
	TestEqual(
		TEXT("Sub-identity configured strength factor is clamped to zero rather than inverted"),
		UPhysAnimComponent::ResolveExperimentalActiveStrengthFactorForTesting(
			-2.0f,
			true,
			EPhysAnimRuntimeState::BalanceActive_Standing),
		0.0f);
	TestFalse(
		TEXT("Checkpoint torque ceiling is disabled by default"),
		Component->IsExperimentalCheckpointTorqueCeilingEnabledForTesting());
	Component->ApplyProductVariantFromCommandLineForTesting(
		TEXT("-PhysAnimProductVariant=RealOnnxPolicy -PhysAnimExperimentalCheckpointTorqueCeiling"));
	TestTrue(
		TEXT("Explicit development flag enables the checkpoint torque ceiling"),
		Component->IsExperimentalCheckpointTorqueCeilingEnabledForTesting());
	TestFalse(
		TEXT("Checkpoint torque ceiling remains inactive before standing activation"),
		UPhysAnimComponent::ShouldUseExperimentalCheckpointTorqueCeilingForRuntimeStateForTesting(
			true,
			true,
			EPhysAnimRuntimeState::RuntimeReady));
	TestFalse(
		TEXT("Checkpoint torque ceiling remains inactive during standing preparation"),
		UPhysAnimComponent::ShouldUseExperimentalCheckpointTorqueCeilingForRuntimeStateForTesting(
			true,
			true,
			EPhysAnimRuntimeState::Standing_Preparation));
	TestFalse(
		TEXT("Checkpoint torque ceiling remains inactive before the first active-standing policy capture"),
		UPhysAnimComponent::ShouldUseExperimentalCheckpointTorqueCeilingForRuntimeStateForTesting(
			true,
			false,
			EPhysAnimRuntimeState::BalanceActive_Standing));
	TestTrue(
		TEXT("Checkpoint torque ceiling activates after the first active-standing policy capture"),
		UPhysAnimComponent::ShouldUseExperimentalCheckpointTorqueCeilingForRuntimeStateForTesting(
			true,
			true,
			EPhysAnimRuntimeState::BalanceActive_Standing));
	TestFalse(
		TEXT("Unconfigured checkpoint torque ceiling remains inactive"),
		UPhysAnimComponent::ShouldUseExperimentalCheckpointTorqueCeilingForRuntimeStateForTesting(
			false,
			true,
			EPhysAnimRuntimeState::BalanceActive_Standing));
	TestFalse(
		TEXT("Checkpoint force PD is disabled by default"),
		Component->IsExperimentalCheckpointForcePdEnabledForTesting());
	Component->ApplyProductVariantFromCommandLineForTesting(
		TEXT("-PhysAnimProductVariant=RealOnnxPolicy -PhysAnimExperimentalCheckpointForcePd"));
	TestTrue(
		TEXT("Explicit development flag enables checkpoint force PD"),
		Component->IsExperimentalCheckpointForcePdEnabledForTesting());
	TestFalse(
		TEXT("Checkpoint force PD remains inactive before first active-standing policy capture"),
		UPhysAnimComponent::ShouldUseExperimentalCheckpointForcePdForRuntimeStateForTesting(
			true,
			false,
			EPhysAnimRuntimeState::BalanceActive_Standing));
	TestTrue(
		TEXT("Checkpoint force PD activates after first active-standing policy capture"),
		UPhysAnimComponent::ShouldUseExperimentalCheckpointForcePdForRuntimeStateForTesting(
			true,
			true,
			EPhysAnimRuntimeState::BalanceActive_Standing));
	TestFalse(
		TEXT("Checkpoint force PD remains inactive outside active standing"),
		UPhysAnimComponent::ShouldUseExperimentalCheckpointForcePdForRuntimeStateForTesting(
			true,
			true,
			EPhysAnimRuntimeState::Standing_PolicyBlend));
	FPhysicsControlData BaselineCheckpointData;
	BaselineCheckpointData.LinearStrength = 17.0f;
	BaselineCheckpointData.AngularTargetVelocityMultiplier = 0.25f;
	auto ValidateCheckpointProfile = [this, &BaselineCheckpointData](
		const FName BoneName,
		const float ExpectedKpNmPerRad,
		const float ExpectedKdNmSecPerRad)
	{
		FPhysicsControlData ProfileData;
		TestTrue(
			*FString::Printf(TEXT("Checkpoint force-PD profile resolves for %s"), *BoneName.ToString()),
			UPhysAnimComponent::TryBuildCheckpointForcePdControlDataForTesting(
				BoneName,
				BaselineCheckpointData,
				ProfileData));
		const float PublishedKpEngine = FMath::Square(ProfileData.AngularStrength * 2.0f * PI);
		TestTrue(
			*FString::Printf(TEXT("Checkpoint Kp matches for %s"), *BoneName.ToString()),
			FMath::IsNearlyEqual(PublishedKpEngine, ExpectedKpNmPerRad * 10000.0f, 2.0f));
		TestEqual(
			*FString::Printf(TEXT("Checkpoint damping ratio is zero for %s"), *BoneName.ToString()),
			ProfileData.AngularDampingRatio,
			0.0f);
		TestEqual(
			*FString::Printf(TEXT("Checkpoint Kd matches for %s"), *BoneName.ToString()),
			ProfileData.AngularExtraDamping,
			ExpectedKdNmSecPerRad * 10000.0f);
		TestEqual(
			*FString::Printf(TEXT("Checkpoint effort matches for %s"), *BoneName.ToString()),
			ProfileData.MaxTorque,
			5000000.0f);
		TestEqual(
			*FString::Printf(TEXT("Unrelated linear strength is preserved for %s"), *BoneName.ToString()),
			ProfileData.LinearStrength,
			BaselineCheckpointData.LinearStrength);
		TestEqual(
			*FString::Printf(TEXT("Target velocity multiplier is preserved for %s"), *BoneName.ToString()),
			ProfileData.AngularTargetVelocityMultiplier,
			BaselineCheckpointData.AngularTargetVelocityMultiplier);
	};
	ValidateCheckpointProfile(TEXT("thigh_l"), 800.0f, 80.0f);
	ValidateCheckpointProfile(TEXT("ball_l"), 500.0f, 50.0f);
	ValidateCheckpointProfile(TEXT("spine_02"), 1000.0f, 100.0f);
	ValidateCheckpointProfile(TEXT("upperarm_r"), 500.0f, 50.0f);
	ValidateCheckpointProfile(TEXT("hand_r"), 300.0f, 30.0f);
	FPhysicsControlData UnknownCheckpointData;
	TestFalse(
		TEXT("Unknown bones do not receive an invented checkpoint profile"),
		UPhysAnimComponent::TryBuildCheckpointForcePdControlDataForTesting(
			TEXT("unknown_bone"),
			BaselineCheckpointData,
			UnknownCheckpointData));
	Component->ApplyProductVariantFromCommandLineForTesting(TEXT("-PhysAnimProductVariant=RealOnnxPolicy"));
	TestFalse(
		TEXT("Omitting experimental flags restores captured neutral, cached-world axis, range mapping, all actions, and authored actuation"),
		Component->IsExperimentalComponentActionAxisFromFirstPolicyEnabledForTesting() ||
			Component->IsExperimentalBindNeutralFromFirstPolicyEnabledForTesting() ||
			Component->IsExperimentalConstraintRangeRemapBypassFromFirstPolicyEnabledForTesting() ||
			Component->GetExperimentalActionFamilyMaskForTesting() !=
				EPhysAnimExperimentalActionFamilyMask::All ||
			Component->GetExperimentalActionJointRangeStartForTesting() != INDEX_NONE ||
			Component->GetExperimentalActionJointRangeCountForTesting() != 0 ||
			Component->IsExperimentalPolicyActionBaselineResidualEnabledForTesting() ||
			Component->IsExperimentalPolicyActionZeroUntilBaselineEnabledForTesting() ||
			Component->IsExperimentalPhysicsBodyObservationPositionsEnabledForTesting() ||
			!FMath::IsNearlyEqual(Component->GetExperimentalActiveStrengthFactorForTesting(), 1.0f) ||
			Component->IsExperimentalCheckpointTorqueCeilingEnabledForTesting() ||
			Component->IsExperimentalCheckpointForcePdEnabledForTesting());
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
	TestFalse(
		TEXT("Startup chronology tracing is disabled without its explicit development flag"),
		Component->IsStartupChronologyTraceEnabledForTesting());
	Component->ApplyProductVariantFromCommandLineForTesting(
		TEXT("-PhysAnimProductVariant=RealOnnxPolicy -PhysAnimStartupChronologyTrace"));
	TestTrue(
		TEXT("The explicit development flag enables startup chronology tracing"),
		Component->IsStartupChronologyTraceEnabledForTesting());
	Component->ApplyProductVariantFromCommandLineForTesting(TEXT("-PhysAnimProductVariant=RealOnnxPolicy"));
	TestFalse(
		TEXT("Removing the development flag restores the startup chronology trace-off default"),
		Component->IsStartupChronologyTraceEnabledForTesting());
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

	void ConfigureCausalStandingFixedTimeStep(
		bool& bInOutConfigured,
		bool& bOutPreviousUseFixedTimeStep,
		double& OutPreviousFixedDeltaTimeSeconds)
	{
		if (bInOutConfigured)
		{
			return;
		}

		bOutPreviousUseFixedTimeStep = FApp::UseFixedTimeStep();
		OutPreviousFixedDeltaTimeSeconds = FApp::GetFixedDeltaTime();
		FApp::SetFixedDeltaTime(GetCausalStandingFixedDeltaTimeSeconds());
		FApp::SetUseFixedTimeStep(true);
		bInOutConfigured = true;
	}

	void RestoreCausalStandingFixedTimeStep(
		bool& bInOutConfigured,
		bool bPreviousUseFixedTimeStep,
		double PreviousFixedDeltaTimeSeconds)
	{
		if (!bInOutConfigured)
		{
			return;
		}

		FApp::SetUseFixedTimeStep(bPreviousUseFixedTimeStep);
		FApp::SetFixedDeltaTime(PreviousFixedDeltaTimeSeconds);
		bInOutConfigured = false;
	}

	struct FCausalStandingFixedTimeStepState
	{
		bool bConfigured = false;
		bool bPreviousUseFixedTimeStep = false;
		double PreviousFixedDeltaTimeSeconds = 0.0;

		void Configure()
		{
			ConfigureCausalStandingFixedTimeStep(
				bConfigured,
				bPreviousUseFixedTimeStep,
				PreviousFixedDeltaTimeSeconds);
		}

		void Restore()
		{
			RestoreCausalStandingFixedTimeStep(
				bConfigured,
				bPreviousUseFixedTimeStep,
				PreviousFixedDeltaTimeSeconds);
		}
	};

	class FConfigureCausalStandingFixedTimeStepCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FConfigureCausalStandingFixedTimeStepCommand(
			const TSharedRef<FCausalStandingFixedTimeStepState>& InState)
			: State(InState)
		{
		}

		virtual bool Update() override
		{
			State->Configure();
			return true;
		}

	private:
		TSharedRef<FCausalStandingFixedTimeStepState> State;
	};

	class FRestoreCausalStandingFixedTimeStepCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FRestoreCausalStandingFixedTimeStepCommand(
			const TSharedRef<FCausalStandingFixedTimeStepState>& InState)
			: State(InState)
		{
		}

		virtual bool Update() override
		{
			State->Restore();
			return true;
		}

	private:
		TSharedRef<FCausalStandingFixedTimeStepState> State;
	};

	double AdvanceCausalStandingSupportGapMs(
		double CurrentGapMs,
		bool bHasSupportContact,
		double DeltaTimeSeconds)
	{
		return bHasSupportContact
			? 0.0
			: CurrentGapMs + FMath::Max(0.0, DeltaTimeSeconds) * 1000.0;
	}

	struct FScriptedLocomotionStep
	{
		FString Phase = TEXT("StandingHold");
		float IntentMagnitude = 0.0f;
		float YawDeltaDegrees = 0.0f;
		bool bMove = false;
		bool bStop = false;
	};

	FScriptedLocomotionStep ResolveScriptedLocomotionStep(double TimeSeconds)
	{
		static constexpr double StandingEndSeconds = 1.0;
		static constexpr double AccelerationEndSeconds = 1.6;
		static constexpr double CruiseEndSeconds = 2.1;
		static constexpr double MovingTurnEndSeconds = 2.4;
		static constexpr double DecelerationEndSeconds = 3.0;
		static constexpr double AccelerationDurationSeconds =
			AccelerationEndSeconds - StandingEndSeconds;
		static constexpr double MovingTurnDurationSeconds =
			MovingTurnEndSeconds - CruiseEndSeconds;
		static constexpr double DecelerationDurationSeconds =
			DecelerationEndSeconds - MovingTurnEndSeconds;

		FScriptedLocomotionStep Step;
		if (TimeSeconds < StandingEndSeconds)
		{
			return Step;
		}
		if (TimeSeconds < AccelerationEndSeconds)
		{
			Step.Phase = TEXT("Acceleration");
			const float PhaseAlpha = static_cast<float>(
				(TimeSeconds - StandingEndSeconds) / AccelerationDurationSeconds);
			Step.IntentMagnitude = FMath::Lerp(0.1f, 1.0f, PhaseAlpha);
			Step.bMove = true;
			return Step;
		}
		if (TimeSeconds < CruiseEndSeconds)
		{
			Step.Phase = TEXT("Cruise");
			Step.IntentMagnitude = 1.0f;
			Step.bMove = true;
			return Step;
		}
		if (TimeSeconds < MovingTurnEndSeconds)
		{
			Step.Phase = TEXT("MovingTurn");
			Step.IntentMagnitude = 0.8f;
			Step.YawDeltaDegrees =
				30.0f * static_cast<float>(CausalStandingFixedDeltaTimeSeconds / MovingTurnDurationSeconds);
			Step.bMove = true;
			return Step;
		}
		if (TimeSeconds < DecelerationEndSeconds)
		{
			Step.Phase = TEXT("Deceleration");
			const float PhaseAlpha = static_cast<float>(
				(TimeSeconds - MovingTurnEndSeconds) / DecelerationDurationSeconds);
			Step.IntentMagnitude = FMath::Lerp(0.8f, 0.05f, PhaseAlpha);
			Step.bMove = true;
			return Step;
		}
		Step.Phase = TEXT("Settle");
		Step.bStop = true;
		return Step;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimScriptedLocomotionScheduleContractTest,
		"PhysAnim.Locomotion.ScriptedSchedule",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimScriptedLocomotionScheduleContractTest::RunTest(const FString& Parameters)
	{
		auto TestPhase = [this](double TimeSeconds, const TCHAR* ExpectedPhase)
		{
			TestEqual(
				FString::Printf(TEXT("E69 phase at %.3f seconds"), TimeSeconds),
				ResolveScriptedLocomotionStep(TimeSeconds).Phase,
				FString(ExpectedPhase));
		};

		TestPhase(0.999, TEXT("StandingHold"));
		TestPhase(1.000, TEXT("Acceleration"));
		TestPhase(1.599, TEXT("Acceleration"));
		TestPhase(1.600, TEXT("Cruise"));
		TestPhase(2.099, TEXT("Cruise"));
		TestPhase(2.100, TEXT("MovingTurn"));
		TestPhase(2.399, TEXT("MovingTurn"));
		TestPhase(2.400, TEXT("Deceleration"));
		TestPhase(2.999, TEXT("Deceleration"));
		TestPhase(3.000, TEXT("Settle"));

		double IntegratedYawDegrees = 0.0;
		double PredictedPathCm = 0.0;
		TSet<FString> ObservedPhases;
		for (int32 StepIndex = 0; StepIndex < 300; ++StepIndex)
		{
			const double TimeSeconds = static_cast<double>(StepIndex) * CausalStandingFixedDeltaTimeSeconds;
			const FScriptedLocomotionStep Step = ResolveScriptedLocomotionStep(TimeSeconds);
			ObservedPhases.Add(Step.Phase);
			IntegratedYawDegrees += Step.YawDeltaDegrees;
			PredictedPathCm += static_cast<double>(Step.IntentMagnitude) * 160.0 * CausalStandingFixedDeltaTimeSeconds;
		}

		for (const FString& RequiredPhase : {
			FString(TEXT("StandingHold")),
			FString(TEXT("Acceleration")),
			FString(TEXT("Cruise")),
			FString(TEXT("MovingTurn")),
			FString(TEXT("Deceleration")),
			FString(TEXT("Settle")) })
		{
			TestTrue(FString::Printf(TEXT("E69 observes phase %s"), *RequiredPhase), ObservedPhases.Contains(RequiredPhase));
		}
		TestEqual(TEXT("E69 moving turn integrates to 30 degrees"), IntegratedYawDegrees, 30.0, 1.0e-4);
		TestTrue(TEXT("E69 predicted path is inside the locked 180-260 cm band"), PredictedPathCm >= 180.0 && PredictedPathCm <= 260.0);
		return true;
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
		bool bScriptedLocomotionRun = false;
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

		bool ConfigureScriptedLocomotion(FString& OutError)
		{
			bScriptedLocomotionRun = true;
			bApplyPerturbation = false;
			CaptureWindowSeconds = 10.0;
			ProtocolRelativePath = TEXT("../product-gates/scripted-locomotion.v1.json");
			FixtureAuthority = TEXT("PRODUCT_RUN");
			RunSchemaVersion = TEXT("physanim-scripted-locomotion-run/v1");
			StandingVariant = Variant == TEXT("ZeroActions")
				? EPhysAnimStandingVariant::ZeroActions
				: EPhysAnimStandingVariant::Normal;
			if (Variant != TEXT("Normal") &&
				Variant != TEXT("ZeroActions") &&
				Variant != TEXT("DropTrajectoryConditioning") &&
				Variant != TEXT("SuppressStopTransition"))
			{
				OutError = FString::Printf(TEXT("Unknown scripted-locomotion variant '%s'"), *Variant);
				return false;
			}
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

	double MeasureRootShellTrackingErrorCm(
		const FTransform& ActorTransform,
		const FVector& InitialActorLocalRootOffset,
		const FVector& PhysicalRootWorldLocation)
	{
		const FVector ExpectedRootWorldLocation =
			ActorTransform.TransformPosition(InitialActorLocalRootOffset);
		return FVector::Dist2D(ExpectedRootWorldLocation, PhysicalRootWorldLocation);
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

	TArray<TSharedPtr<FJsonValue>> BuildNameJsonArray(const TArray<FName>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> Result;
		Result.Reserve(Values.Num());
		for (const FName Value : Values)
		{
			Result.Add(MakeShared<FJsonValueString>(Value.ToString()));
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

	TSharedRef<FJsonObject> BuildMannyLocalFrameRoundtripJson(
		const PhysAnimBridge::FPhysAnimMannyLocalFrameRoundtripTrace& Trace,
		const bool bEnabled)
	{
		FString ValidationError;
		const bool bValid = bEnabled &&
			PhysAnimBridge::ValidateMannyLocalFrameRoundtripTrace(Trace, ValidationError);
		if (!bEnabled)
		{
			ValidationError.Reset();
		}

		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(
			TEXT("schema_version"),
			TEXT("physanim-manny-local-frame-roundtrip/v2"));
		Root->SetStringField(TEXT("authority"), TEXT("DEVELOPMENT_DIAGNOSTIC_ONLY"));
		Root->SetBoolField(TEXT("product_success"), false);
		Root->SetBoolField(TEXT("enabled"), bEnabled);
		Root->SetBoolField(TEXT("captured"), Trace.bCaptured);
		Root->SetBoolField(TEXT("valid"), bValid);
		Root->SetStringField(TEXT("validation_error"), ValidationError);
		Root->SetStringField(TEXT("capture_scope"), Trace.CaptureScope);
		Root->SetStringField(TEXT("capture_error"), Trace.CaptureError);
		Root->SetStringField(
			TEXT("configured_action_axis_mode"),
			Trace.ConfiguredActionAxisMode);
		Root->SetStringField(
			TEXT("effective_action_axis_mode"),
			Trace.EffectiveActionAxisMode);
		Root->SetNumberField(TEXT("axis_probe_degrees"), Trace.AxisProbeDegrees);
		Root->SetStringField(TEXT("quaternion_layout"), TEXT("xyzw"));
		Root->SetStringField(
			TEXT("quaternion_multiplication"),
			PhysAnimBridge::MannyLocalFrameRoundtripQuaternionMultiplication);
		Root->SetStringField(
			TEXT("action_composition_order"),
			PhysAnimBridge::MannyLocalFrameRoundtripActionCompositionOrder);
		Root->SetStringField(
			TEXT("observation_recovery_order"),
			PhysAnimBridge::MannyLocalFrameRoundtripObservationRecoveryOrder);
		Root->SetStringField(
			TEXT("roundtrip_observation_body_selection"),
			PhysAnimBridge::MannyLocalFrameRoundtripObservationBodySelection);
		Root->SetStringField(
			TEXT("cached_action_axis_reference_frame"),
			PhysAnimBridge::MannyLocalFrameRoundtripActionAxisFrame);
		Root->SetStringField(
			TEXT("action_bind_component_world_rotation_frame"),
			PhysAnimBridge::MannyLocalFrameRoundtripActionBindComponentWorldFrame);
		Root->SetStringField(
			TEXT("observation_bind_rotation_frame"),
			PhysAnimBridge::MannyLocalFrameRoundtripObservationBindFrame);

		TArray<TSharedPtr<FJsonValue>> ControlValues;
		ControlValues.Reserve(Trace.Controls.Num());
		for (const PhysAnimBridge::FPhysAnimMannyLocalFrameRoundtripControl& Entry : Trace.Controls)
		{
			const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
			Object->SetNumberField(TEXT("control_index"), Entry.ControlIndex);
			Object->SetStringField(TEXT("manny_bone_name"), Entry.MannyBoneName.ToString());
			Object->SetStringField(TEXT("control_name"), Entry.ControlName.ToString());
			Object->SetStringField(
				TEXT("initial_control_child_bone_name"),
				Entry.InitialControlChildBoneName.ToString());
			Object->SetStringField(
				TEXT("initial_control_parent_bone_name"),
				Entry.InitialControlParentBoneName.ToString());
			Object->SetArrayField(
				TEXT("source_proto_joint_indices"),
				BuildIntegerJsonArray(Entry.SourceProtoJointIndices));
			Object->SetArrayField(
				TEXT("source_proto_joint_names"),
				BuildNameJsonArray(Entry.SourceProtoJointNames));
			Object->SetArrayField(
				TEXT("observation_body_indices"),
				BuildIntegerJsonArray(Entry.ObservationBodyIndices));
			Object->SetArrayField(
				TEXT("observation_body_names"),
				BuildNameJsonArray(Entry.ObservationBodyNames));
			Object->SetNumberField(
				TEXT("roundtrip_observation_body_index"),
				Entry.RoundtripObservationBodyIndex);
			Object->SetStringField(
				TEXT("roundtrip_observation_body_name"),
				Entry.RoundtripObservationBodyName.ToString());
			Object->SetNumberField(
				TEXT("observation_parent_body_index"),
				Entry.ObservationParentBodyIndex);
			Object->SetStringField(
				TEXT("observation_parent_body_name"),
				Entry.ObservationParentBodyName.ToString());
			Object->SetBoolField(TEXT("decisive_one_to_one"), Entry.bDecisiveOneToOne);
			Object->SetBoolField(TEXT("ownership_complete"), Entry.bOwnershipComplete);
			Object->SetStringField(
				TEXT("effective_action_axis_mode"),
				Entry.EffectiveActionAxisMode);
			Object->SetArrayField(
				TEXT("cached_action_axis_reference_rotation_xyzw"),
				BuildQuaternionJsonArray(Entry.CachedActionAxisReferenceRotation));
			Object->SetArrayField(
				TEXT("cached_action_axis_world_rotation_xyzw"),
				BuildQuaternionJsonArray(Entry.CachedActionAxisReferenceRotation));
			Object->SetArrayField(
				TEXT("action_bind_component_world_rotation_xyzw"),
				BuildQuaternionJsonArray(Entry.ActionBindComponentWorldRotation));
			Object->SetArrayField(
				TEXT("component_corrected_action_axis_rotation_xyzw"),
				BuildQuaternionJsonArray(Entry.ComponentCorrectedActionAxisRotation));
			Object->SetArrayField(
				TEXT("effective_action_axis_rotation_xyzw"),
				BuildQuaternionJsonArray(Entry.EffectiveActionAxisRotation));
			Object->SetArrayField(
				TEXT("action_bind_parent_relative_rotation_xyzw"),
				BuildQuaternionJsonArray(Entry.ActionBindParentRelativeRotation));
			Object->SetArrayField(
				TEXT("policy_neutral_parent_relative_rotation_xyzw"),
				BuildQuaternionJsonArray(Entry.PolicyNeutralParentRelativeRotation));
			Object->SetArrayField(
				TEXT("observation_parent_bind_component_rotation_xyzw"),
				BuildQuaternionJsonArray(Entry.ObservationParentBindComponentRotation));
			Object->SetArrayField(
				TEXT("observation_body_bind_component_rotation_xyzw"),
				BuildQuaternionJsonArray(Entry.ObservationBodyBindComponentRotation));
			Object->SetArrayField(
				TEXT("observation_bind_parent_relative_rotation_xyzw"),
				BuildQuaternionJsonArray(Entry.ObservationBindParentRelativeRotation));
			Object->SetArrayField(
				TEXT("actual_decoded_rotation_ue_xyzw"),
				BuildQuaternionJsonArray(Entry.ActualDecodedRotationUe));
			Object->SetArrayField(
				TEXT("actual_manny_pre_range_target_parent_relative_xyzw"),
				BuildQuaternionJsonArray(Entry.ActualMannyPreRangeTargetParentRelative));
			Object->SetNumberField(
				TEXT("action_axis_vs_observation_parent_bind_angular_delta_degrees"),
				Entry.ActionAxisVsObservationParentBindAngularDeltaDegrees);
			Object->SetNumberField(
				TEXT("effective_action_axis_vs_observation_parent_bind_component_angular_delta_degrees"),
				Entry.EffectiveActionAxisVsObservationParentBindComponentAngularDeltaDegrees);
			Object->SetNumberField(
				TEXT("action_bind_vs_observation_bind_parent_relative_angular_delta_degrees"),
				Entry.ActionBindVsObservationBindParentRelativeAngularDeltaDegrees);
			Object->SetNumberField(
				TEXT("policy_neutral_vs_action_bind_parent_relative_angular_delta_degrees"),
				Entry.PolicyNeutralVsActionBindParentRelativeAngularDeltaDegrees);
			Object->SetNumberField(
				TEXT("policy_neutral_vs_observation_bind_parent_relative_angular_delta_degrees"),
				Entry.PolicyNeutralVsObservationBindParentRelativeAngularDeltaDegrees);

			TArray<TSharedPtr<FJsonValue>> CaseValues;
			CaseValues.Reserve(Entry.RoundtripCases.Num());
			for (const PhysAnimBridge::FPhysAnimMannyLocalFrameRoundtripCase& Case : Entry.RoundtripCases)
			{
				const TSharedRef<FJsonObject> CaseObject = MakeShared<FJsonObject>();
				CaseObject->SetStringField(TEXT("label"), Case.Label.ToString());
				CaseObject->SetArrayField(
					TEXT("input_canonical_rotation_ue_xyzw"),
					BuildQuaternionJsonArray(Case.InputCanonicalRotationUe));
				CaseObject->SetArrayField(
					TEXT("manny_pre_range_target_parent_relative_xyzw"),
					BuildQuaternionJsonArray(Case.MannyPreRangeTargetParentRelative));
				CaseObject->SetArrayField(
					TEXT("recovered_canonical_rotation_ue_xyzw"),
					BuildQuaternionJsonArray(Case.RecoveredCanonicalRotationUe));
				CaseObject->SetNumberField(
					TEXT("angular_error_degrees"),
					Case.AngularErrorDegrees);
				CaseValues.Add(MakeShared<FJsonValueObject>(CaseObject));
			}
			Object->SetArrayField(TEXT("roundtrip_cases"), CaseValues);
			ControlValues.Add(MakeShared<FJsonValueObject>(Object));
		}
		Root->SetArrayField(TEXT("controls"), ControlValues);
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
		const bool bEnabled)
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

	TSharedRef<FJsonObject> BuildStartupChronologyJson(
		const PhysAnimBridge::FPhysAnimStartupChronologyTrace& Trace,
		const bool bEnabled)
	{
		FString ValidationError;
		const bool bValid = bEnabled &&
			PhysAnimBridge::ValidateStartupChronologyTrace(Trace, ValidationError);
		if (!bEnabled)
		{
			ValidationError.Reset();
		}

		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("schema_version"), TEXT("physanim-startup-chronology/v1"));
		Root->SetStringField(TEXT("authority"), TEXT("DEVELOPMENT_DIAGNOSTIC_ONLY"));
		Root->SetBoolField(TEXT("enabled"), bEnabled);
		Root->SetBoolField(TEXT("complete"), Trace.bComplete);
		Root->SetBoolField(TEXT("valid"), bValid);
		Root->SetStringField(TEXT("capture_error"), Trace.CaptureError);
		Root->SetStringField(TEXT("validation_error"), ValidationError);
		TArray<TSharedPtr<FJsonValue>> Samples;
		Samples.Reserve(Trace.Samples.Num());
		for (const PhysAnimBridge::FPhysAnimStartupChronologySample& Sample : Trace.Samples)
		{
			const TSharedRef<FJsonObject> SampleJson = MakeShared<FJsonObject>();
			SampleJson->SetNumberField(TEXT("sequence"), Sample.Sequence);
			SampleJson->SetStringField(TEXT("stage"), Sample.Stage);
			SampleJson->SetNumberField(TEXT("world_time_seconds"), Sample.WorldTimeSeconds);
			SampleJson->SetStringField(TEXT("runtime_state"), Sample.RuntimeState);
			SampleJson->SetNumberField(TEXT("policy_update_accumulator_seconds"), Sample.PolicyUpdateAccumulatorSeconds);
			SampleJson->SetNumberField(TEXT("last_policy_elapsed_steps"), Sample.LastPolicyElapsedSteps);
			SampleJson->SetNumberField(TEXT("policy_control_ticks_executed"), Sample.PolicyControlTicksExecuted);
			SampleJson->SetBoolField(TEXT("first_policy_input_captured"), Sample.bFirstPolicyInputCaptured);
			SampleJson->SetObjectField(TEXT("owner_actor_world_transform"), BuildPolicyInputProvenanceTransformJson(Sample.OwnerActorWorldTransform));
			SampleJson->SetObjectField(TEXT("mesh_world_transform"), BuildPolicyInputProvenanceTransformJson(Sample.MeshWorldTransform));
			SampleJson->SetObjectField(TEXT("root_bone_world_transform"), BuildPolicyInputProvenanceTransformJson(Sample.RootBoneWorldTransform));
			SampleJson->SetArrayField(TEXT("body_samples"), BuildPolicyInputProvenanceBodySamplesJson(Sample.BodySamples));
			Samples.Add(MakeShared<FJsonValueObject>(SampleJson));
		}
		Root->SetArrayField(TEXT("samples"), Samples);
		return Root;
	}

	TSharedRef<FJsonObject> BuildFirstPolicyBodySourceRecordJson(
		const PhysAnimBridge::FPhysAnimFirstPolicyBodySourceRecord& Record)
	{
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetBoolField(TEXT("recorded"), Record.bRecorded);
		Root->SetStringField(TEXT("stage"), Record.Stage);
		Root->SetNumberField(TEXT("world_time_seconds"), Record.WorldTimeSeconds);
		Root->SetStringField(TEXT("runtime_state"), Record.RuntimeState);
		Root->SetNumberField(TEXT("policy_control_tick"), Record.PolicyControlTick);
		Root->SetNumberField(TEXT("body_sample_count"), Record.BodySampleCount);
		Root->SetStringField(TEXT("fingerprint_algorithm"), Record.FingerprintAlgorithm);
		Root->SetStringField(TEXT("fingerprint"), Record.Fingerprint);
		Root->SetArrayField(TEXT("body_samples"), BuildPolicyInputProvenanceBodySamplesJson(Record.BodySamples));
		return Root;
	}

	TSharedRef<FJsonObject> BuildFirstPolicyBodySourceJson(
		const PhysAnimBridge::FPhysAnimFirstPolicyBodySourceTrace& Trace)
	{
		FString ValidationError;
		const bool bValid = PhysAnimBridge::ValidateFirstPolicyBodySourceTrace(Trace, ValidationError);
		const bool bComplete = Trace.bFirstInferenceRecorded &&
			Trace.Prior.bRecorded &&
			Trace.Live.bRecorded &&
			Trace.Effective.bRecorded;

		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("schema_version"), TEXT("physanim-first-policy-body-source/v1"));
		Root->SetStringField(TEXT("authority"), TEXT("DEVELOPMENT_DIAGNOSTIC_ONLY"));
		Root->SetBoolField(TEXT("product_success_authority"), false);
		Root->SetBoolField(TEXT("complete"), bComplete);
		Root->SetBoolField(TEXT("valid"), bValid);
		Root->SetStringField(TEXT("error"), ValidationError);
		Root->SetObjectField(TEXT("prior"), BuildFirstPolicyBodySourceRecordJson(Trace.Prior));
		Root->SetObjectField(TEXT("live"), BuildFirstPolicyBodySourceRecordJson(Trace.Live));
		Root->SetObjectField(TEXT("effective"), BuildFirstPolicyBodySourceRecordJson(Trace.Effective));
		return Root;
	}

	TSharedRef<FJsonObject> BuildFirstPolicyGroundReferenceRecordJson(
		const PhysAnimBridge::FPhysAnimFirstPolicyGroundReferenceRecord& Record)
	{
		const PhysAnimBridge::FPhysAnimSelfObservationGroundReferenceValues& Values = Record.Values;
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetBoolField(TEXT("recorded"), Record.bRecorded);
		Root->SetStringField(TEXT("stage"), Record.Stage);
		Root->SetNumberField(TEXT("world_time_seconds"), Record.WorldTimeSeconds);
		Root->SetStringField(TEXT("runtime_state"), Record.RuntimeState);
		Root->SetNumberField(TEXT("policy_control_tick"), Record.PolicyControlTick);
		Root->SetNumberField(TEXT("body_root_proto_z_m"), Values.BodyRootProtoZM);
		Root->SetNumberField(TEXT("root_bone_world_z_cm"), Values.RootBoneWorldZCm);
		Root->SetBoolField(TEXT("static_trace_attempted"), Values.bStaticTraceAttempted);
		Root->SetBoolField(TEXT("static_trace_succeeded"), Values.bStaticTraceSucceeded);
		Root->SetNumberField(TEXT("static_trace_impact_z_cm"), Values.StaticTraceImpactZCm);
		Root->SetBoolField(TEXT("walkable_floor"), Values.bHasWalkableFloor);
		Root->SetBoolField(TEXT("blocking_floor_hit"), Values.bHasBlockingFloorHit);
		Root->SetNumberField(TEXT("floor_impact_z_cm"), Values.FloorImpactZCm);
		Root->SetBoolField(TEXT("capsule_available"), Values.bCapsuleAvailable);
		Root->SetNumberField(TEXT("capsule_center_z_cm"), Values.CapsuleCenterZCm);
		Root->SetNumberField(TEXT("capsule_half_height_cm"), Values.CapsuleHalfHeightCm);
		Root->SetNumberField(TEXT("floor_distance_cm"), Values.FloorDistanceCm);
		Root->SetNumberField(TEXT("fallback_ground_world_z_cm"), Values.FallbackGroundWorldZCm);
		Root->SetNumberField(TEXT("resolved_ground_world_z_cm"), Values.GroundWorldZCm);
		Root->SetNumberField(TEXT("synthetic_ground_height_m"), Values.SyntheticGroundHeightM);
		Root->SetNumberField(TEXT("final_root_height_m"), Values.FinalRootHeightM);
		return Root;
	}

	TSharedRef<FJsonObject> BuildFirstPolicyGroundReferenceJson(
		const PhysAnimBridge::FPhysAnimFirstPolicyGroundReferenceTrace& Trace)
	{
		FString ValidationError;
		const bool bValid = PhysAnimBridge::ValidateFirstPolicyGroundReferenceTrace(Trace, ValidationError);
		const bool bComplete = Trace.bFirstPolicyRecorded &&
			Trace.Prior.bRecorded &&
			Trace.Live.bRecorded;

		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("schema_version"), TEXT("physanim-first-policy-ground-reference/v1"));
		Root->SetStringField(TEXT("authority"), TEXT("DEVELOPMENT_DIAGNOSTIC_ONLY"));
		Root->SetBoolField(TEXT("product_success_authority"), false);
		Root->SetBoolField(TEXT("complete"), bComplete);
		Root->SetBoolField(TEXT("valid"), bValid);
		Root->SetStringField(TEXT("error"), ValidationError);
		Root->SetObjectField(TEXT("prior"), BuildFirstPolicyGroundReferenceRecordJson(Trace.Prior));
		Root->SetObjectField(TEXT("live"), BuildFirstPolicyGroundReferenceRecordJson(Trace.Live));
		return Root;
	}


	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPolicyInputProvenancePublicationContractTest,
		"PhysAnim.ProductHarness.PolicyInputProvenancePublicationContract",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimPolicyInputProvenancePublicationContractTest::RunTest(const FString& Parameters)
	{
		const PhysAnimBridge::FPhysAnimPolicyInputProvenanceSnapshot EmptySnapshot;
		const TSharedRef<FJsonObject> DisabledJson = BuildPolicyInputProvenanceJson(EmptySnapshot, false);
		TestFalse(TEXT("Provenance publication reports the default-off state"), DisabledJson->GetBoolField(TEXT("enabled")));
		TestFalse(TEXT("Disabled provenance publication is uncaptured"), DisabledJson->GetBoolField(TEXT("captured")));
		TestFalse(TEXT("Disabled provenance publication is not validated"), DisabledJson->GetBoolField(TEXT("valid")));

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

		const TSharedRef<FJsonObject> Json = BuildPolicyInputProvenanceJson(Snapshot, true);
		TestEqual(
			TEXT("Provenance publication schema is versioned"),
			Json->GetStringField(TEXT("schema_version")),
			FString(TEXT("physanim-policy-input-provenance/v1")));
		TestTrue(TEXT("Provenance publication reports explicit enablement"), Json->GetBoolField(TEXT("enabled")));
		TestTrue(TEXT("Provenance publication reports capture"), Json->GetBoolField(TEXT("captured")));
		TestTrue(TEXT("Provenance publication reports contract validity"), Json->GetBoolField(TEXT("valid")));
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

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimStartupChronologyPublicationContractTest,
		"PhysAnim.ProductHarness.StartupChronologyPublicationContract",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimStartupChronologyPublicationContractTest::RunTest(const FString& Parameters)
	{
		const PhysAnimBridge::FPhysAnimStartupChronologyTrace EmptyTrace;
		const TSharedRef<FJsonObject> DisabledJson = BuildStartupChronologyJson(EmptyTrace, false);
		TestFalse(TEXT("Startup chronology publication reports default-off"), DisabledJson->GetBoolField(TEXT("enabled")));
		TestFalse(TEXT("Disabled startup chronology is incomplete"), DisabledJson->GetBoolField(TEXT("complete")));
		TestFalse(TEXT("Disabled startup chronology is not validated"), DisabledJson->GetBoolField(TEXT("valid")));

		TArray<FPhysAnimBodySample> BodySamples;
		BodySamples.SetNum(PhysAnimBridge::NumSmplBodies);
		PhysAnimBridge::FPhysAnimStartupChronologyTrace Trace;
		for (int32 StageIndex = 0; StageIndex < 3; ++StageIndex)
		{
			const TCHAR* Stage = StageIndex == 0
				? TEXT("pre_state_machine")
				: (StageIndex == 1 ? TEXT("post_state_machine") : TEXT("post_policy"));
			TestTrue(
				FString::Printf(TEXT("Publication fixture captures chronology stage %d"), StageIndex),
				Trace.CaptureIf(
					true,
					Stage,
					1.0,
					TEXT("Standing_Preparation"),
					0.0f,
					StageIndex == 2 ? 1 : 0,
					StageIndex == 2 ? 1 : 0,
					StageIndex == 2,
					FTransform::Identity,
					FTransform::Identity,
					FTransform::Identity,
					BodySamples));
		}

		const TSharedRef<FJsonObject> Json = BuildStartupChronologyJson(Trace, true);
		TestEqual(
			TEXT("Startup chronology schema is versioned"),
			Json->GetStringField(TEXT("schema_version")),
			FString(TEXT("physanim-startup-chronology/v1")));
		TestTrue(TEXT("Startup chronology reports explicit enablement"), Json->GetBoolField(TEXT("enabled")));
		TestTrue(TEXT("Startup chronology reports completion"), Json->GetBoolField(TEXT("complete")));
		TestTrue(TEXT("Startup chronology reports contract validity"), Json->GetBoolField(TEXT("valid")));
		const TArray<TSharedPtr<FJsonValue>>& Samples = Json->GetArrayField(TEXT("samples"));
		TestEqual(TEXT("Startup chronology publishes the ordered triplet"), Samples.Num(), 3);
		TestEqual(
			TEXT("Each chronology stage publishes all SMPL bodies"),
			Samples[0]->AsObject()->GetArrayField(TEXT("body_samples")).Num(),
			PhysAnimBridge::NumSmplBodies);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimFirstPolicyBodySourcePublicationContractTest,
		"PhysAnim.ProductHarness.FirstPolicyBodySourcePublicationContract",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimFirstPolicyBodySourcePublicationContractTest::RunTest(const FString& Parameters)
	{
		TArray<FPhysAnimBodySample> PriorBodySamples;
		TArray<FPhysAnimBodySample> LiveBodySamples;
		PriorBodySamples.SetNum(PhysAnimBridge::NumSmplBodies);
		PriorBodySamples[0].Position = FVector(0.25, -0.5, 100.0);
		LiveBodySamples = PriorBodySamples;
		LiveBodySamples[0].Position += FVector(1.0, 2.0, 3.0);

		PhysAnimBridge::FPhysAnimFirstPolicyBodySourceTrace Trace;
		TestTrue(
			TEXT("Publication fixture captures the prior body source"),
			Trace.CapturePriorIf(
				true,
				TEXT("pre_state_machine"),
				1.0,
				TEXT("WaitingForPoseSearch"),
				0,
				PriorBodySamples));
		FString RecordError;
		TestTrue(
			TEXT("Publication fixture records the live first-policy source"),
			Trace.RecordFirstPolicySourceIf(
				true,
				TEXT("first_policy_pre_adapter"),
				2.0,
				TEXT("BridgeActive"),
				1,
				LiveBodySamples,
				RecordError));
		TestTrue(TEXT("Publication fixture records without error"), RecordError.IsEmpty());

		const TSharedRef<FJsonObject> Json = BuildFirstPolicyBodySourceJson(Trace);
		TestEqual(
			TEXT("First-policy body-source schema is versioned"),
			Json->GetStringField(TEXT("schema_version")),
			FString(TEXT("physanim-first-policy-body-source/v1")));
		TestEqual(
			TEXT("First-policy body-source publication is development-only"),
			Json->GetStringField(TEXT("authority")),
			FString(TEXT("DEVELOPMENT_DIAGNOSTIC_ONLY")));
		TestFalse(
			TEXT("First-policy body-source publication cannot establish product success"),
			Json->GetBoolField(TEXT("product_success_authority")));
		TestTrue(TEXT("First-policy body-source publication reports completion"), Json->GetBoolField(TEXT("complete")));
		TestTrue(TEXT("First-policy body-source publication reports validity"), Json->GetBoolField(TEXT("valid")));
		TestTrue(TEXT("Valid first-policy body-source publication has no error"), Json->GetStringField(TEXT("error")).IsEmpty());
		TestFalse(TEXT("Instrumentation publication has no configured behavior field"), Json->HasField(TEXT("configured")));
		TestFalse(TEXT("Instrumentation publication has no consumed behavior field"), Json->HasField(TEXT("consumed")));
		TestFalse(TEXT("Instrumentation publication has no replay behavior field"), Json->HasField(TEXT("replay")));

		const TSharedPtr<FJsonObject> PriorJson = Json->GetObjectField(TEXT("prior"));
		const TSharedPtr<FJsonObject> LiveJson = Json->GetObjectField(TEXT("live"));
		const TSharedPtr<FJsonObject> EffectiveJson = Json->GetObjectField(TEXT("effective"));
		TestEqual(TEXT("Prior metadata publishes its stage"), PriorJson->GetStringField(TEXT("stage")), FString(TEXT("pre_state_machine")));
		TestEqual(TEXT("Prior metadata publishes its runtime state"), PriorJson->GetStringField(TEXT("runtime_state")), FString(TEXT("WaitingForPoseSearch")));
		TestEqual(TEXT("Prior metadata publishes policy tick zero"), static_cast<int32>(PriorJson->GetNumberField(TEXT("policy_control_tick"))), 0);
		TestEqual(TEXT("Live metadata publishes its stage"), LiveJson->GetStringField(TEXT("stage")), FString(TEXT("first_policy_pre_adapter")));
		TestEqual(TEXT("Live metadata publishes policy tick one"), static_cast<int32>(LiveJson->GetNumberField(TEXT("policy_control_tick"))), 1);
		TestEqual(
			TEXT("Every source publishes the SMPL body count"),
			static_cast<int32>(PriorJson->GetNumberField(TEXT("body_sample_count"))),
			PhysAnimBridge::NumSmplBodies);
		TestEqual(
			TEXT("Every source publishes the locked fingerprint algorithm"),
			LiveJson->GetStringField(TEXT("fingerprint_algorithm")),
			FString(PhysAnimBridge::FirstPolicyBodySourceFingerprintAlgorithm));
		TestFalse(TEXT("Live source publishes a fingerprint"), LiveJson->GetStringField(TEXT("fingerprint")).IsEmpty());
		TestEqual(
			TEXT("Every source publishes raw body samples"),
			PriorJson->GetArrayField(TEXT("body_samples")).Num(),
			PhysAnimBridge::NumSmplBodies);
		const TSharedPtr<FJsonObject> FirstPriorBody = PriorJson->GetArrayField(TEXT("body_samples"))[0]->AsObject();
		TestEqual(
			TEXT("Raw body-sample position is preserved"),
			FirstPriorBody->GetArrayField(TEXT("position_xyz"))[0]->AsNumber(),
			0.25);
		TestEqual(
			TEXT("Instrumentation-only effective source is byte-for-byte JSON-identical to live"),
			SerializeJson(EffectiveJson.ToSharedRef()),
			SerializeJson(LiveJson.ToSharedRef()));

		Trace.Effective.BodySamples[0].Position.X += 1.0;
		FString FingerprintError;
		TestTrue(
			TEXT("Divergent effective-source fixture remains internally fingerprint-consistent"),
			PhysAnimBridge::BuildFirstPolicyBodySourceFingerprint(
				Trace.Effective.BodySamples,
				Trace.Effective.Fingerprint,
				FingerprintError));
		const TSharedRef<FJsonObject> InvalidJson = BuildFirstPolicyBodySourceJson(Trace);
		TestFalse(
			TEXT("Instrumentation rejects an effective source that differs from live"),
			InvalidJson->GetBoolField(TEXT("valid")));
		TestTrue(
			TEXT("Effective/live divergence is explained"),
			InvalidJson->GetStringField(TEXT("error")).Contains(TEXT("did not preserve the exact live source")));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimFirstPolicyGroundReferencePublicationContractTest,
		"PhysAnim.ProductHarness.FirstPolicyGroundReferencePublicationContract",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimFirstPolicyGroundReferencePublicationContractTest::RunTest(const FString& Parameters)
	{
		auto MakeValues = [](const double BodyRootProtoZM, const double RootBoneWorldZCm)
		{
			PhysAnimBridge::FPhysAnimSelfObservationGroundReferenceValues Values;
			Values.BodyRootProtoZM = BodyRootProtoZM;
			Values.RootBoneWorldZCm = RootBoneWorldZCm;
			Values.bStaticTraceAttempted = true;
			Values.bStaticTraceSucceeded = true;
			Values.StaticTraceImpactZCm = 0.0;
			Values.bCapsuleAvailable = true;
			Values.CapsuleCenterZCm = RootBoneWorldZCm;
			Values.CapsuleHalfHeightCm = 50.0;
			const float GroundWorldZCm = static_cast<float>(Values.StaticTraceImpactZCm);
			const float DesiredRootHeightM =
				(static_cast<float>(RootBoneWorldZCm) - GroundWorldZCm) * PhysAnimBridge::CmToMeters;
			const volatile float ObservationFrameRootZM = static_cast<float>(BodyRootProtoZM);
			const float SyntheticGroundHeightM =
				ObservationFrameRootZM - DesiredRootHeightM;
			Values.GroundWorldZCm = GroundWorldZCm;
			Values.SyntheticGroundHeightM = SyntheticGroundHeightM;
			Values.FinalRootHeightM = static_cast<float>(
				BodyRootProtoZM - static_cast<double>(SyntheticGroundHeightM));
			return Values;
		};

		const PhysAnimBridge::FPhysAnimFirstPolicyGroundReferenceTrace EmptyTrace;
		const TSharedRef<FJsonObject> EmptyJson = BuildFirstPolicyGroundReferenceJson(EmptyTrace);
		TestFalse(TEXT("An empty ground-reference publication is incomplete"), EmptyJson->GetBoolField(TEXT("complete")));
		TestFalse(TEXT("An empty ground-reference publication is invalid"), EmptyJson->GetBoolField(TEXT("valid")));

		PhysAnimBridge::FPhysAnimFirstPolicyGroundReferenceTrace Trace;
		TestTrue(
			TEXT("Publication fixture captures the prior ground reference"),
			Trace.CapturePriorIf(
				true,
				TEXT("pre_state_machine"),
				1.0,
				TEXT("WaitingForPoseSearch"),
				0,
				MakeValues(1.0, 100.0)));
		FString RecordError;
		TestTrue(
			*FString::Printf(
				TEXT("Publication fixture captures the first-policy ground reference: %s"),
				*RecordError),
			Trace.RecordFirstPolicyIf(
				true,
				TEXT("first_policy_self_observation"),
				2.0,
				TEXT("Standing_Preparation"),
				1,
				MakeValues(1.2, 120.0),
				RecordError));

		const TSharedRef<FJsonObject> Json = BuildFirstPolicyGroundReferenceJson(Trace);
		TestEqual(
			TEXT("First-policy ground-reference schema is versioned"),
			Json->GetStringField(TEXT("schema_version")),
			FString(TEXT("physanim-first-policy-ground-reference/v1")));
		TestEqual(
			TEXT("First-policy ground-reference publication is development-only"),
			Json->GetStringField(TEXT("authority")),
			FString(TEXT("DEVELOPMENT_DIAGNOSTIC_ONLY")));
		TestFalse(
			TEXT("Ground-reference diagnostics cannot establish product success"),
			Json->GetBoolField(TEXT("product_success_authority")));
		TestTrue(TEXT("Ground-reference publication reports completion"), Json->GetBoolField(TEXT("complete")));
		TestTrue(TEXT("Ground-reference publication reports validity"), Json->GetBoolField(TEXT("valid")));
		TestTrue(TEXT("Valid ground-reference publication has no error"), Json->GetStringField(TEXT("error")).IsEmpty());
		TestTrue(TEXT("The prior record is published"), Json->HasField(TEXT("prior")));
		TestTrue(TEXT("The live record is published"), Json->HasField(TEXT("live")));
		TestFalse(TEXT("The two-record diagnostic has no synthetic effective record"), Json->HasField(TEXT("effective")));

		const TSharedPtr<FJsonObject> PriorJson = Json->GetObjectField(TEXT("prior"));
		const TSharedPtr<FJsonObject> LiveJson = Json->GetObjectField(TEXT("live"));
		TestEqual(TEXT("Prior metadata owns policy tick zero"), static_cast<int32>(PriorJson->GetNumberField(TEXT("policy_control_tick"))), 0);
		TestEqual(TEXT("Live metadata owns policy tick one"), static_cast<int32>(LiveJson->GetNumberField(TEXT("policy_control_tick"))), 1);
		for (const TCHAR* RequiredField : {
			TEXT("body_root_proto_z_m"),
			TEXT("root_bone_world_z_cm"),
			TEXT("static_trace_attempted"),
			TEXT("static_trace_succeeded"),
			TEXT("static_trace_impact_z_cm"),
			TEXT("walkable_floor"),
			TEXT("blocking_floor_hit"),
			TEXT("floor_impact_z_cm"),
			TEXT("capsule_available"),
			TEXT("capsule_center_z_cm"),
			TEXT("capsule_half_height_cm"),
			TEXT("floor_distance_cm"),
			TEXT("fallback_ground_world_z_cm"),
			TEXT("resolved_ground_world_z_cm"),
			TEXT("synthetic_ground_height_m"),
			TEXT("final_root_height_m")})
		{
			TestTrue(
				FString::Printf(TEXT("Both records publish required field '%s'"), RequiredField),
				PriorJson->HasField(RequiredField) && LiveJson->HasField(RequiredField));
		}
		return true;
	}

	class FCausalStandingCaptureCommand final : public IAutomationLatentCommand
	{
	public:
		FCausalStandingCaptureCommand(
			FAutomationTestBase* InTest,
			const FCausalStandingRunConfig& InConfig,
			const TSharedRef<FCausalStandingFixedTimeStepState>& InFixedTimeStepState)
			: Test(InTest),
			  Config(InConfig),
			  FixedTimeStepState(InFixedTimeStepState),
			  StartupRealTime(FPlatformTime::Seconds())
		{
		}

		virtual bool Update() override
		{
			UWorld* World = FindProductWorld();
			if (!World)
			{
				if (FPlatformTime::Seconds() - StartupRealTime >= StartupTimeoutSeconds)
				{
					Test->AddError(TEXT("PIE world was unavailable for the causal-standing product run"));
					FixedTimeStepState->Restore();
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
					if (Config.bScriptedLocomotionRun && Component->GetOwner())
					{
						ScenarioStartLocation = Component->GetOwner()->GetActorLocation();
						LastScenarioActorLocation = ScenarioStartLocation;
						ScenarioStartYawDegrees = Component->GetOwner()->GetActorRotation().Yaw;
						bScenarioTransformInitialized = true;
					}
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

			if (Config.bScriptedLocomotionRun)
			{
				DriveScriptedLocomotion(Component, TimeSeconds);
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
			return true;
		}

	private:
		void DriveScriptedLocomotion(UPhysAnimComponent* Component, double TimeSeconds)
		{
			if (!Component)
			{
				++ScriptStepFailureCount;
				return;
			}
			const FScriptedLocomotionStep Step = ResolveScriptedLocomotionStep(TimeSeconds);
			CurrentScriptPhase = Step.Phase;
			CurrentScriptIntentMagnitude = Step.IntentMagnitude;
			ObservedScriptPhases.Add(Step.Phase);
			if (Step.bMove)
			{
				if (!bLocomotionGateOpened)
				{
					bLocomotionGateOpened = Component->TestOnlyTryOpenStage2ALocomotionRequestGate(TEXT("E60ScriptedScenario"));
					if (!bLocomotionGateOpened)
					{
						++ScriptStepFailureCount;
						return;
					}
				}
				const bool bPublishTrajectory = Config.Variant != TEXT("DropTrajectoryConditioning");
				bTrajectoryConditioningPublished = bPublishTrajectory;
				if (!Component->TestOnlyTryActivateStage2AScriptedLocomotionIntent(
					CausalStandingFixedDeltaTimeSeconds,
					Step.IntentMagnitude,
					Step.YawDeltaDegrees,
					bPublishTrajectory))
				{
					++ScriptStepFailureCount;
				}
			}
			else if (Step.bStop && !bStopDecisionMade)
			{
				bStopDecisionMade = true;
				if (Config.Variant == TEXT("SuppressStopTransition"))
				{
					bStopTransitionSuppressed = true;
				}
				else
				{
					bStopIssued = Component->TestOnlyStopStage2AScriptedLocomotionAndReturnToStanding();
					if (!bStopIssued)
					{
						++ScriptStepFailureCount;
					}
				}
			}
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
			FConstraintInstance* const GainProbeConstraint = Mesh
				? Mesh->FindConstraintInstance(TEXT("thigh_l"))
				: nullptr;
			int32 CheckpointForcePdProfileMatchCount = 0;
			int32 CheckpointForcePdForceModeCount = 0;
			if (PhysicsControl && Mesh)
			{
				for (const FName BoneName : PhysAnimBridge::GetControlledBoneNames())
				{
					FPhysicsControlData ActualControlData;
					FPhysicsControlData ExpectedCheckpointData;
					const FName ControlName = PhysAnimBridge::MakeControlName(BoneName);
					if (PhysicsControl->GetControlData(ControlName, ActualControlData) &&
						UPhysAnimComponent::TryBuildCheckpointForcePdControlDataForTesting(
							BoneName,
							ActualControlData,
							ExpectedCheckpointData) &&
						FMath::IsNearlyEqual(
							ActualControlData.AngularStrength,
							ExpectedCheckpointData.AngularStrength,
							1.0e-3f) &&
						FMath::IsNearlyEqual(
							ActualControlData.AngularDampingRatio,
							ExpectedCheckpointData.AngularDampingRatio,
							1.0e-6f) &&
						FMath::IsNearlyEqual(
							ActualControlData.AngularExtraDamping,
							ExpectedCheckpointData.AngularExtraDamping,
							1.0f) &&
						FMath::IsNearlyEqual(
							ActualControlData.MaxTorque,
							ExpectedCheckpointData.MaxTorque,
							1.0f))
					{
						++CheckpointForcePdProfileMatchCount;
					}
					if (FConstraintInstance* const Constraint = Mesh->FindConstraintInstance(BoneName))
					{
						CheckpointForcePdForceModeCount +=
							Constraint->ProfileInstance.AngularDrive.GetAccelerationMode() ? 0 : 1;
					}
				}
			}
			const FStandingPlantPoseErrorSummary PoseErrors = Component
				? MeasurePoseErrors(Mesh, PhysicsControl, Component->GetPreviousControlTargetRotationsForDiagnostics())
				: FStandingPlantPoseErrorSummary{};
			UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
			UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
			const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
			const AActor* const OwnerActor = Component ? Component->GetOwner() : nullptr;
			const FVector ActorLocation = OwnerActor ? OwnerActor->GetActorLocation() : FVector::ZeroVector;
			const bool bPhysicalRootValid = PelvisBody && PelvisBody->IsValidBodyInstance();
			const FVector PhysicalRootLocation = bPhysicalRootValid
				? PelvisBody->GetUnrealWorldTransform().GetLocation()
				: FVector::ZeroVector;
			double RootShellTrackingErrorCm = 0.0;
			if (Config.bScriptedLocomotionRun && bScenarioTransformInitialized)
			{
				ShellPathLengthCm += FVector::Dist2D(LastScenarioActorLocation, ActorLocation);
				LastScenarioActorLocation = ActorLocation;
				if (OwnerActor && bPhysicalRootValid)
				{
					if (!bPhysicalRootTrackingInitialized)
					{
						InitialActorLocalRootOffset =
							OwnerActor->GetActorTransform().InverseTransformPosition(PhysicalRootLocation);
						ScenarioStartPhysicalRootLocation = PhysicalRootLocation;
						LastPhysicalRootLocation = PhysicalRootLocation;
						bPhysicalRootTrackingInitialized = true;
					}
					else
					{
						PhysicalRootPathLengthCm +=
							FVector::Dist2D(LastPhysicalRootLocation, PhysicalRootLocation);
						LastPhysicalRootLocation = PhysicalRootLocation;
					}
					RootShellTrackingErrorCm = MeasureRootShellTrackingErrorCm(
						OwnerActor->GetActorTransform(),
						InitialActorLocalRootOffset,
						PhysicalRootLocation);
					MaxRootShellTrackingErrorCm =
						FMath::Max(MaxRootShellTrackingErrorCm, RootShellTrackingErrorCm);
				}
			}
			const FScriptedLocomotionStep ScriptStep = ResolveScriptedLocomotionStep(TimeSeconds);
			const FBridgeShellState& ScriptShell = Component
				? Component->GetBridgeShellStateForTesting()
				: EmptyBridgeShellState;
			Row->SetNumberField(TEXT("sequence"), PhysicsSequence++);
			Row->SetNumberField(TEXT("time_sec"), TimeSeconds);
			Row->SetStringField(TEXT("script_phase"), Config.bScriptedLocomotionRun ? ScriptStep.Phase : TEXT("Standing"));
			Row->SetNumberField(TEXT("actor_location_x_cm"), ActorLocation.X);
			Row->SetNumberField(TEXT("actor_location_y_cm"), ActorLocation.Y);
			Row->SetNumberField(TEXT("actor_location_z_cm"), ActorLocation.Z);
			Row->SetNumberField(TEXT("actor_yaw_deg"), OwnerActor ? OwnerActor->GetActorRotation().Yaw : 0.0);
			Row->SetNumberField(TEXT("script_intent_magnitude"), Config.bScriptedLocomotionRun ? ScriptStep.IntentMagnitude : 0.0);
			Row->SetBoolField(TEXT("trajectory_conditioning_published"), Config.bScriptedLocomotionRun && Config.Variant != TEXT("DropTrajectoryConditioning") && ScriptStep.bMove);
			Row->SetNumberField(TEXT("shell_accepted_speed_cm_per_sec"), ScriptShell.AcceptedPlanarVelocityCmPerSecond.Size2D());
			Row->SetNumberField(TEXT("shell_path_length_cm"), ShellPathLengthCm);
			Row->SetNumberField(TEXT("physical_root_location_x_cm"), PhysicalRootLocation.X);
			Row->SetNumberField(TEXT("physical_root_location_y_cm"), PhysicalRootLocation.Y);
			Row->SetNumberField(TEXT("physical_root_location_z_cm"), PhysicalRootLocation.Z);
			Row->SetNumberField(TEXT("root_shell_tracking_error_cm"), RootShellTrackingErrorCm);
			Row->SetNumberField(TEXT("physical_root_path_length_cm"), PhysicalRootPathLengthCm);
			Row->SetNumberField(TEXT("target_readback_match_ratio"), LatestTargetReadbackMatchRatio);
			Row->SetNumberField(TEXT("script_step_failure_count"), ScriptStepFailureCount);
			Row->SetBoolField(TEXT("human_input"), false);
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
			Row->SetNumberField(TEXT("control_base_angular_damping_ratio"), bHasGainProbe ? GainProbeData.AngularDampingRatio : 0.0);
			Row->SetNumberField(TEXT("control_base_angular_extra_damping"), bHasGainProbe ? GainProbeData.AngularExtraDamping : 0.0);
			Row->SetNumberField(TEXT("control_base_max_torque_engine_units"), bHasGainProbe ? GainProbeData.MaxTorque : 0.0);
			Row->SetBoolField(
				TEXT("control_angular_acceleration_mode"),
				GainProbeConstraint ? GainProbeConstraint->ProfileInstance.AngularDrive.GetAccelerationMode() : true);
			Row->SetNumberField(TEXT("checkpoint_force_pd_profile_match_count"), CheckpointForcePdProfileMatchCount);
			Row->SetNumberField(TEXT("checkpoint_force_pd_force_mode_count"), CheckpointForcePdForceModeCount);
			Row->SetNumberField(TEXT("control_angular_strength_multiplier"), bHasGainProbe ? GainProbeMultiplier.AngularStrengthMultiplier : 0.0);
			Row->SetNumberField(TEXT("control_angular_damping_ratio_multiplier"), bHasGainProbe ? GainProbeMultiplier.AngularDampingRatioMultiplier : 0.0);
			Row->SetNumberField(TEXT("control_angular_extra_damping_multiplier"), bHasGainProbe ? GainProbeMultiplier.AngularExtraDampingMultiplier : 0.0);
			Row->SetNumberField(TEXT("control_max_torque_multiplier"), bHasGainProbe ? GainProbeMultiplier.MaxTorqueMultiplier : 0.0);
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
			LatestTargetReadbackMatchRatio = Diagnostics.NumTotalTargetsWritten > 0
				? static_cast<double>(ReadbackMatches) / static_cast<double>(Diagnostics.NumTotalTargetsWritten)
				: 0.0;
			const FScriptedLocomotionStep ScriptStep = ResolveScriptedLocomotionStep(TimeSeconds);
			Row->SetNumberField(TEXT("sequence"), PolicySequence++);
			Row->SetNumberField(TEXT("time_sec"), TimeSeconds);
			Row->SetStringField(TEXT("script_phase"), Config.bScriptedLocomotionRun ? ScriptStep.Phase : TEXT("Standing"));
			Row->SetBoolField(TEXT("trajectory_conditioning_published"), Config.bScriptedLocomotionRun && Config.Variant != TEXT("DropTrajectoryConditioning") && ScriptStep.bMove);
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
			const FString ScenarioSummaryPath = FPaths::Combine(Config.RunRoot, TEXT("scenario-summary.json"));
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
			bool bFirstActiveStandingConditionedActionsWritten = true;
			bool bActionSemanticTraceWritten = true;
			bool bMannyLocalFrameRoundtripTraceWritten = true;
			bool bPolicyInputProvenanceWritten = true;
			bool bStartupChronologyWritten = true;
			bool bFirstPolicyBodySourceWritten = true;
			bool bFirstPolicyGroundReferenceWritten = true;
			bool bObservationPositionTraceWritten = true;
			const FString FirstActiveConditionedActionsPath = FPaths::Combine(
				Config.RunRoot,
				TEXT("first-active-conditioned-actions.json"));
			const TArray<float> EmptyConditionedActions;
			const TArray<float>& FirstActiveConditionedActions = Component
				? Component->GetFirstActiveStandingConditionedActionsForTesting()
				: EmptyConditionedActions;
			TArray<float> FirstActiveHeadTriplet;
			for (int32 ScalarIndex = 36;
				ScalarIndex < FMath::Min(39, FirstActiveConditionedActions.Num());
				++ScalarIndex)
			{
				FirstActiveHeadTriplet.Add(FirstActiveConditionedActions[ScalarIndex]);
			}
			const TSharedRef<FJsonObject> FirstActiveConditionedActionsJson = MakeShared<FJsonObject>();
			FirstActiveConditionedActionsJson->SetStringField(
				TEXT("schema_version"),
				TEXT("physanim-first-active-conditioned-actions/v1"));
			FirstActiveConditionedActionsJson->SetStringField(
				TEXT("capture_scope"),
				TEXT("first_active_standing_conditioning"));
			FirstActiveConditionedActionsJson->SetBoolField(
				TEXT("captured"),
				Component && Component->HasFirstActiveStandingConditionedActionsForTesting());
			FirstActiveConditionedActionsJson->SetNumberField(
				TEXT("action_width"),
				FirstActiveConditionedActions.Num());
			FirstActiveConditionedActionsJson->SetArrayField(
				TEXT("conditioned_actions"),
				BuildPolicyActionJsonArray(FirstActiveConditionedActions));
			FirstActiveConditionedActionsJson->SetArrayField(
				TEXT("head_triplet"),
				BuildPolicyActionJsonArray(FirstActiveHeadTriplet));
			bFirstActiveStandingConditionedActionsWritten = FFileHelper::SaveStringToFile(
				SerializeJson(FirstActiveConditionedActionsJson) + TEXT("\n"),
				*FirstActiveConditionedActionsPath);

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

				const FString MannyLocalFrameRoundtripTracePath = FPaths::Combine(
					Config.RunRoot,
					TEXT("manny-local-frame-roundtrip.json"));
				const PhysAnimBridge::FPhysAnimMannyLocalFrameRoundtripTrace EmptyMannyLocalFrameRoundtripTrace;
				const PhysAnimBridge::FPhysAnimMannyLocalFrameRoundtripTrace& MannyLocalFrameRoundtripTrace = Component
					? Component->GetMannyLocalFrameRoundtripTraceForTesting()
					: EmptyMannyLocalFrameRoundtripTrace;
				const bool bMannyLocalFrameRoundtripTraceEnabled =
					Component && Component->IsMannyLocalFrameRoundtripTraceEnabledForTesting();
				bMannyLocalFrameRoundtripTraceWritten = FFileHelper::SaveStringToFile(
					SerializeJson(BuildMannyLocalFrameRoundtripJson(
						MannyLocalFrameRoundtripTrace,
						bMannyLocalFrameRoundtripTraceEnabled)) + TEXT("\n"),
					*MannyLocalFrameRoundtripTracePath);

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
							true)) + TEXT("\n"),
						*PolicyInputProvenancePath);
				}

				const bool bStartupChronologyEnabled =
					Component && Component->IsStartupChronologyTraceEnabledForTesting();
				if (bStartupChronologyEnabled)
				{
					const FString StartupChronologyPath = FPaths::Combine(
						Config.RunRoot,
						TEXT("startup-chronology.json"));
					bStartupChronologyWritten = FFileHelper::SaveStringToFile(
						SerializeJson(BuildStartupChronologyJson(
							Component->GetStartupChronologyTraceForTesting(),
							true)) + TEXT("\n"),
						*StartupChronologyPath);

					const FString FirstPolicyBodySourcePath = FPaths::Combine(
						Config.RunRoot,
						TEXT("first-policy-body-source.json"));
					bFirstPolicyBodySourceWritten = FFileHelper::SaveStringToFile(
						SerializeJson(BuildFirstPolicyBodySourceJson(
							Component->GetFirstPolicyBodySourceTraceForTesting())) + TEXT("\n"),
						*FirstPolicyBodySourcePath);

					const FString FirstPolicyGroundReferencePath = FPaths::Combine(
						Config.RunRoot,
						TEXT("first-policy-ground-reference.json"));
					bFirstPolicyGroundReferenceWritten = FFileHelper::SaveStringToFile(
						SerializeJson(BuildFirstPolicyGroundReferenceJson(
							Component->GetFirstPolicyGroundReferenceTraceForTesting())) + TEXT("\n"),
						*FirstPolicyGroundReferencePath);
				}

				const FString ObservationPositionTracePath = FPaths::Combine(
					Config.RunRoot,
					TEXT("rigid-body-position-observation.json"));
				const TSharedRef<FJsonObject> ObservationPositionTrace = MakeShared<FJsonObject>();
				ObservationPositionTrace->SetStringField(
					TEXT("schema_version"),
					TEXT("physanim-rigid-body-position-observation/v1"));
				ObservationPositionTrace->SetBoolField(
					TEXT("physics_body_positions_selected"),
					Component && Component->IsExperimentalPhysicsBodyObservationPositionsEnabledForTesting());
				TArray<TSharedPtr<FJsonValue>> PositionEntries;
				if (Component)
				{
					const TArray<FName>& ObservationBones = PhysAnimBridge::GetSmplObservationBoneNames();
					const TArray<FVector>& BonePositions = Component->GetObservationBoneWorldPositionsForTesting();
					const TArray<FVector>& BodyPositions = Component->GetObservationPhysicsBodyWorldPositionsForTesting();
					const int32 EntryCount = FMath::Min3(ObservationBones.Num(), BonePositions.Num(), BodyPositions.Num());
					for (int32 EntryIndex = 0; EntryIndex < EntryCount; ++EntryIndex)
					{
						const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
						Entry->SetNumberField(TEXT("index"), EntryIndex);
						Entry->SetStringField(TEXT("bone_name"), ObservationBones[EntryIndex].ToString());
						Entry->SetArrayField(TEXT("bone_world_position_cm"), BuildVectorJsonArray(BonePositions[EntryIndex]));
						Entry->SetArrayField(TEXT("physics_body_world_position_cm"), BuildVectorJsonArray(BodyPositions[EntryIndex]));
						Entry->SetArrayField(TEXT("body_minus_bone_cm"), BuildVectorJsonArray(BodyPositions[EntryIndex] - BonePositions[EntryIndex]));
						Entry->SetNumberField(TEXT("delta_magnitude_cm"), FVector::Distance(BonePositions[EntryIndex], BodyPositions[EntryIndex]));
						PositionEntries.Add(MakeShared<FJsonValueObject>(Entry));
					}
				}
				ObservationPositionTrace->SetNumberField(TEXT("entry_count"), PositionEntries.Num());
				ObservationPositionTrace->SetArrayField(TEXT("bodies"), PositionEntries);
				bObservationPositionTraceWritten = FFileHelper::SaveStringToFile(
					SerializeJson(ObservationPositionTrace) + TEXT("\n"),
					*ObservationPositionTracePath);
			}
			if (!Config.bPlantRun)
			{
				const FString ObservationPositionTracePath = FPaths::Combine(
					Config.RunRoot,
					TEXT("rigid-body-position-observation.json"));
				const TSharedRef<FJsonObject> ObservationPositionTrace = MakeShared<FJsonObject>();
				ObservationPositionTrace->SetStringField(
					TEXT("schema_version"),
					TEXT("physanim-rigid-body-position-observation/v1"));
				ObservationPositionTrace->SetBoolField(
					TEXT("physics_body_positions_selected"),
					Component && Component->IsExperimentalPhysicsBodyObservationPositionsEnabledForTesting());
				TArray<TSharedPtr<FJsonValue>> PositionEntries;
				if (Component)
				{
					const TArray<FName>& ObservationBones = PhysAnimBridge::GetSmplObservationBoneNames();
					const TArray<FVector>& BonePositions = Component->GetObservationBoneWorldPositionsForTesting();
					const TArray<FVector>& BodyPositions = Component->GetObservationPhysicsBodyWorldPositionsForTesting();
					const int32 EntryCount = FMath::Min3(ObservationBones.Num(), BonePositions.Num(), BodyPositions.Num());
					for (int32 EntryIndex = 0; EntryIndex < EntryCount; ++EntryIndex)
					{
						const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
						Entry->SetNumberField(TEXT("index"), EntryIndex);
						Entry->SetStringField(TEXT("bone_name"), ObservationBones[EntryIndex].ToString());
						Entry->SetArrayField(TEXT("bone_world_position_cm"), BuildVectorJsonArray(BonePositions[EntryIndex]));
						Entry->SetArrayField(TEXT("physics_body_world_position_cm"), BuildVectorJsonArray(BodyPositions[EntryIndex]));
						Entry->SetArrayField(TEXT("body_minus_bone_cm"), BuildVectorJsonArray(BodyPositions[EntryIndex] - BonePositions[EntryIndex]));
						Entry->SetNumberField(TEXT("delta_magnitude_cm"), FVector::Distance(BonePositions[EntryIndex], BodyPositions[EntryIndex]));
						PositionEntries.Add(MakeShared<FJsonValueObject>(Entry));
					}
				}
				ObservationPositionTrace->SetNumberField(TEXT("entry_count"), PositionEntries.Num());
				ObservationPositionTrace->SetArrayField(TEXT("bodies"), PositionEntries);
				bObservationPositionTraceWritten = FFileHelper::SaveStringToFile(
					SerializeJson(ObservationPositionTrace) + TEXT("\n"),
					*ObservationPositionTracePath);
			}
			const int32 NonblankPixels = CaptureRender(World, Component, RenderPath);
			bool bScenarioSummaryWritten = true;
			if (Config.bScriptedLocomotionRun)
			{
				const AActor* const OwnerActor = Component ? Component->GetOwner() : nullptr;
				const FVector EndLocation = OwnerActor ? OwnerActor->GetActorLocation() : FVector::ZeroVector;
				const float EndYawDegrees = OwnerActor ? OwnerActor->GetActorRotation().Yaw : 0.0f;
				const FVector PlanarDelta(EndLocation.X - ScenarioStartLocation.X, EndLocation.Y - ScenarioStartLocation.Y, 0.0f);
				FBodyInstance* const EndPelvisBody = Component && Component->GetMeshComponent()
					? Component->GetMeshComponent()->GetBodyInstance(TEXT("pelvis"))
					: nullptr;
				const bool bEndPhysicalRootValid = EndPelvisBody && EndPelvisBody->IsValidBodyInstance();
				const FVector EndPhysicalRootLocation = bEndPhysicalRootValid
					? EndPelvisBody->GetUnrealWorldTransform().GetLocation()
					: LastPhysicalRootLocation;
				const FVector PhysicalRootPlanarDelta = bPhysicalRootTrackingInitialized
					? FVector(
						EndPhysicalRootLocation.X - ScenarioStartPhysicalRootLocation.X,
						EndPhysicalRootLocation.Y - ScenarioStartPhysicalRootLocation.Y,
						0.0f)
					: FVector::ZeroVector;
				const double FinalRootShellTrackingErrorCm =
					OwnerActor && bPhysicalRootTrackingInitialized && bEndPhysicalRootValid
						? MeasureRootShellTrackingErrorCm(
							OwnerActor->GetActorTransform(),
							InitialActorLocalRootOffset,
							EndPhysicalRootLocation)
						: 0.0;
				const TSharedRef<FJsonObject> Summary = MakeShared<FJsonObject>();
				Summary->SetStringField(TEXT("schema_version"), TEXT("physanim-scripted-locomotion-summary/v1"));
				Summary->SetStringField(TEXT("variant"), Config.Variant);
				Summary->SetBoolField(TEXT("human_input"), false);
				Summary->SetBoolField(TEXT("locomotion_gate_opened"), bLocomotionGateOpened);
				Summary->SetBoolField(TEXT("stop_issued"), bStopIssued);
				Summary->SetBoolField(TEXT("stop_transition_suppressed"), bStopTransitionSuppressed);
				Summary->SetBoolField(TEXT("trajectory_conditioning_expected"), Config.Variant != TEXT("DropTrajectoryConditioning"));
				Summary->SetBoolField(TEXT("trajectory_conditioning_ever_published"), bTrajectoryConditioningPublished);
				Summary->SetNumberField(TEXT("script_step_failure_count"), ScriptStepFailureCount);
				Summary->SetNumberField(TEXT("shell_path_length_cm"), ShellPathLengthCm);
				Summary->SetNumberField(TEXT("net_planar_displacement_cm"), PlanarDelta.Size2D());
				Summary->SetNumberField(TEXT("physical_root_path_length_cm"), PhysicalRootPathLengthCm);
				Summary->SetNumberField(TEXT("physical_root_net_planar_displacement_cm"), PhysicalRootPlanarDelta.Size2D());
				Summary->SetNumberField(TEXT("final_root_shell_tracking_error_cm"), FinalRootShellTrackingErrorCm);
				Summary->SetNumberField(TEXT("max_root_shell_tracking_error_cm"), MaxRootShellTrackingErrorCm);
				Summary->SetNumberField(TEXT("yaw_delta_deg"), FMath::FindDeltaAngleDegrees(ScenarioStartYawDegrees, EndYawDegrees));
				Summary->SetStringField(TEXT("final_runtime_state"), Component ? RuntimeStateName(Component->GetRuntimeState()) : TEXT("Uninitialized"));
				Summary->SetNumberField(TEXT("final_shell_speed_cm_per_sec"), Component ? Component->GetBridgeShellStateForTesting().AcceptedPlanarVelocityCmPerSecond.Size2D() : 0.0);
				TArray<TSharedPtr<FJsonValue>> PhaseValues;
				for (const FString& Phase : { FString(TEXT("StandingHold")), FString(TEXT("Acceleration")), FString(TEXT("Cruise")), FString(TEXT("MovingTurn")), FString(TEXT("Deceleration")), FString(TEXT("Settle")) })
				{
					const TSharedRef<FJsonObject> PhaseJson = MakeShared<FJsonObject>();
					PhaseJson->SetStringField(TEXT("phase"), Phase);
					PhaseJson->SetBoolField(TEXT("observed"), ObservedScriptPhases.Contains(Phase));
					PhaseValues.Add(MakeShared<FJsonValueObject>(PhaseJson));
				}
				Summary->SetArrayField(TEXT("phases"), PhaseValues);
				bScenarioSummaryWritten = FFileHelper::SaveStringToFile(SerializeJson(Summary) + TEXT("\n"), *ScenarioSummaryPath);
			}

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
			Manifest->SetBoolField(TEXT("scripted_locomotion_run"), Config.bScriptedLocomotionRun);
			Manifest->SetBoolField(TEXT("human_input"), false);
			if (Config.bScriptedLocomotionRun)
			{
				Manifest->SetStringField(TEXT("scenario_summary"), TEXT("scenario-summary.json"));
				Manifest->SetStringField(TEXT("root_authority"), TEXT("Stage1_KinematicRoot"));
				Manifest->SetStringField(TEXT("motion_source"), TEXT("Stage2A_KinematicShell"));
			}
			Manifest->SetStringField(TEXT("policy_input_snapshot"), TEXT("policy-input-snapshot.json"));
			Manifest->SetStringField(
				TEXT("first_active_conditioned_actions"),
				TEXT("first-active-conditioned-actions.json"));
			Manifest->SetStringField(TEXT("render_capture"), TEXT("render.png"));
			Manifest->SetNumberField(TEXT("render_nonblank_pixel_count"), NonblankPixels);
			const bool bManifestWritten = FFileHelper::SaveStringToFile(SerializeJson(Manifest) + TEXT("\n"), *ManifestPath);
			return bPhysicsWritten &&
				bPolicyWritten &&
				bPolicyInputSnapshotWritten &&
				bActiveStandingPolicyInputSnapshotWritten &&
				bFirstActiveStandingConditionedActionsWritten &&
				bActionSemanticTraceWritten &&
				bMannyLocalFrameRoundtripTraceWritten &&
				bPolicyInputProvenanceWritten &&
				bStartupChronologyWritten &&
				bFirstPolicyBodySourceWritten &&
				bFirstPolicyGroundReferenceWritten &&
				bObservationPositionTraceWritten &&
				bScenarioSummaryWritten &&
				bManifestWritten;
		}

		FAutomationTestBase* Test = nullptr;
		FCausalStandingRunConfig Config;
		TSharedRef<FCausalStandingFixedTimeStepState> FixedTimeStepState;
		double StartupRealTime = 0.0;
		double ObservationStartWorldTime = 0.0;
		double LastPhysicsTimeSeconds = -1.0;
		double ProductSupportGapTimerMs = 0.0;
		double LatestTargetReadbackMatchRatio = 0.0;
		double ShellPathLengthCm = 0.0;
		double PhysicalRootPathLengthCm = 0.0;
		double MaxRootShellTrackingErrorCm = 0.0;
		FVector ScenarioStartLocation = FVector::ZeroVector;
		FVector LastScenarioActorLocation = FVector::ZeroVector;
		FVector InitialActorLocalRootOffset = FVector::ZeroVector;
		FVector ScenarioStartPhysicalRootLocation = FVector::ZeroVector;
		FVector LastPhysicalRootLocation = FVector::ZeroVector;
		float ScenarioStartYawDegrees = 0.0f;
		FString CurrentScriptPhase = TEXT("StandingHold");
		float CurrentScriptIntentMagnitude = 0.0f;
		TSet<FString> ObservedScriptPhases;
		FBridgeShellState EmptyBridgeShellState;
		bool bScenarioTransformInitialized = false;
		bool bPhysicalRootTrackingInitialized = false;
		bool bLocomotionGateOpened = false;
		bool bStopDecisionMade = false;
		bool bStopIssued = false;
		bool bStopTransitionSuppressed = false;
		bool bTrajectoryConditioningPublished = false;
		int32 ScriptStepFailureCount = 0;
		bool bObservationStarted = false;
		bool bVariantApplied = false;
		bool bPerturbationApplied = false;
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
	const TSharedRef<FCausalStandingFixedTimeStepState> FixedTimeStepState =
		MakeShared<FCausalStandingFixedTimeStepState>();
	AddCommand(new FConfigureCausalStandingFixedTimeStepCommand(FixedTimeStepState));
	AddCommand(new FStartPIECommand(false));
	AddCommand(new FCausalStandingCaptureCommand(this, Config, FixedTimeStepState));
	AddCommand(new FEndPlayMapCommand());
	AddCommand(new FRestoreCausalStandingFixedTimeStepCommand(FixedTimeStepState));
	AddCommand(new FResetCausalStandingVariantCommand());
	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
	FPhysAnimScriptedLocomotionProductTest,
	"PhysAnim.Product.ScriptedLocomotion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimScriptedLocomotionProductTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	for (const TCHAR* Variant : { TEXT("Normal"), TEXT("ZeroActions"), TEXT("DropTrajectoryConditioning"), TEXT("SuppressStopTransition") })
	{
		OutBeautifiedNames.Add(Variant);
		OutTestCommands.Add(Variant);
	}
}

bool FPhysAnimScriptedLocomotionProductTest::RunTest(const FString& Parameters)
{
	FCausalStandingRunConfig Config;
	FString ConfigError;
	if (!Config.ReadFromCommandLine(ConfigError) || !Config.ConfigureScriptedLocomotion(ConfigError))
	{
		AddError(ConfigError);
		return false;
	}
	if (Config.Variant != Parameters)
	{
		AddError(FString::Printf(TEXT("Requested scripted-locomotion variant '%s' does not match test '%s'"), *Config.Variant, *Parameters));
		return false;
	}
	if (IConsoleVariable* ReviewMode = IConsoleManager::Get().FindConsoleVariable(TEXT("physanim.V0PlantReviewMode")))
	{
		ReviewMode->Set(Config.Variant == TEXT("ZeroActions") ? 2 : 0, ECVF_SetByCode);
	}
	if (!AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"), true))
	{
		AddError(TEXT("Unable to open the scripted-locomotion product map"));
		return false;
	}
	const TSharedRef<FCausalStandingFixedTimeStepState> FixedTimeStepState =
		MakeShared<FCausalStandingFixedTimeStepState>();
	AddCommand(new FConfigureCausalStandingFixedTimeStepCommand(FixedTimeStepState));
	AddCommand(new FStartPIECommand(false));
	AddCommand(new FCausalStandingCaptureCommand(this, Config, FixedTimeStepState));
	AddCommand(new FEndPlayMapCommand());
	AddCommand(new FRestoreCausalStandingFixedTimeStepCommand(FixedTimeStepState));
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
	const TSharedRef<FCausalStandingFixedTimeStepState> FixedTimeStepState =
		MakeShared<FCausalStandingFixedTimeStepState>();
	AddCommand(new FConfigureCausalStandingFixedTimeStepCommand(FixedTimeStepState));
	AddCommand(new FStartPIECommand(false));
	AddCommand(new FCausalStandingCaptureCommand(this, Config, FixedTimeStepState));
	AddCommand(new FEndPlayMapCommand());
	AddCommand(new FRestoreCausalStandingFixedTimeStepCommand(FixedTimeStepState));
	AddCommand(new FResetCausalStandingVariantCommand());
	return true;
}

#endif
