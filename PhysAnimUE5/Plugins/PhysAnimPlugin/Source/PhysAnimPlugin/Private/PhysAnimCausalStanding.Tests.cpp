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

	UPhysAnimComponent* const Component = NewObject<UPhysAnimComponent>();
	TestNotNull(TEXT("Transient product harness component"), Component);
	if (!Component)
	{
		return false;
	}
	const FPhysAnimStabilizationSettings DefaultSettings;
	TestEqual(TEXT("Standing gain ramp preserves the seeded plant"), DefaultSettings.StartupRampSeconds, 0.25f);
	TestEqual(TEXT("Standing uses bounded startup control authority"), DefaultSettings.AngularStrengthMultiplier, 0.35f);

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
	Component->ApplyProductVariantFromCommandLineForTesting(TEXT("-PhysAnimProductVariant=DropControlDispatch"));
	TestEqual(TEXT("Command line selects dropped-dispatch activation"), Component->GetStandingVariantForTesting(), EPhysAnimStandingVariant::DropControlDispatch);
	TestTrue(TEXT("Dropped-dispatch command line arms the destructive control"), Component->IsProductControlDispatchDroppedForTesting());
	Component->ApplyProductVariantFromCommandLineForTesting(TEXT("-PhysAnimProductVariant=Normal"));
	TestEqual(TEXT("Normal command line restores normal activation"), Component->GetStandingVariantForTesting(), EPhysAnimStandingVariant::Normal);
	TestFalse(TEXT("Normal command line clears dropped dispatch"), Component->IsProductControlDispatchDroppedForTesting());
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

	class FCausalStandingCaptureCommand final : public IAutomationLatentCommand
	{
	public:
		FCausalStandingCaptureCommand(FAutomationTestBase* InTest, const FCausalStandingRunConfig& InConfig)
			: Test(InTest), Config(InConfig), StartupRealTime(FPlatformTime::Seconds())
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
			const double SampleTimeSeconds = FMath::Min(TimeSeconds, Config.CaptureWindowSeconds);
			if (SampleTimeSeconds <= LastPhysicsTimeSeconds)
			{
				return false;
			}
			LastPhysicsTimeSeconds = SampleTimeSeconds;
			CapturePhysicsSample(World, Component, SampleTimeSeconds);
			CapturePolicySample(Component, SampleTimeSeconds);

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
			return true;
		}

	private:
		void CapturePhysicsSample(UWorld* World, UPhysAnimComponent* Component, double TimeSeconds)
		{
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
			Row->SetNumberField(TEXT("support_gap_ms"), Artifact ? Artifact->SupportGapTimerMs : 0.0);
			Row->SetNumberField(TEXT("critical_body_valid_mask"), CriticalValidMask);
			Row->SetNumberField(TEXT("critical_body_simulating_mask"), CriticalSimulatingMask);
			Row->SetNumberField(TEXT("support_body_valid_mask"), SupportValidMask);
			Row->SetNumberField(TEXT("support_body_simulating_mask"), SupportSimulatingMask);
			Row->SetNumberField(TEXT("body_valid_count"), BodyValidCount);
			Row->SetNumberField(TEXT("body_simulating_count"), BodySimulatingCount);
			Row->SetNumberField(TEXT("control_gain_match_count"), StandingStatus.ControlGainMatchCount);
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
			const FString RenderPath = FPaths::Combine(Config.RunRoot, TEXT("render.png"));
			const FString ManifestPath = FPaths::Combine(Config.RunRoot, TEXT("manifest.json"));
			const bool bPhysicsWritten = FFileHelper::SaveStringToFile(FString::Join(PhysicsRows, TEXT("\n")) + TEXT("\n"), *PhysicsPath);
			const FString PolicyContents = PolicyRows.IsEmpty() ? FString() : FString::Join(PolicyRows, TEXT("\n")) + TEXT("\n");
			const bool bPolicyWritten = FFileHelper::SaveStringToFile(PolicyContents, *PolicyPath);
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
			Manifest->SetStringField(TEXT("render_capture"), TEXT("render.png"));
			Manifest->SetNumberField(TEXT("render_nonblank_pixel_count"), NonblankPixels);
			const bool bManifestWritten = FFileHelper::SaveStringToFile(SerializeJson(Manifest) + TEXT("\n"), *ManifestPath);
			return bPhysicsWritten && bPolicyWritten && bManifestWritten;
		}

		FAutomationTestBase* Test = nullptr;
		FCausalStandingRunConfig Config;
		double StartupRealTime = 0.0;
		double ObservationStartWorldTime = 0.0;
		double LastPhysicsTimeSeconds = -1.0;
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
