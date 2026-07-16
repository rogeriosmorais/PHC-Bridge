#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"
#include "PhysAnimBalanceReadyTransitionPrivate.h"

#include "PhysAnimLogger.h"
#include "Engine/OverlapResult.h"

namespace
{
	TAutoConsoleVariable<int32> CVarPhysAnimRawSimDiagnosticGroup(
		TEXT("p.PhysAnim.RawSimDiagnosticGroup"),
		0,
		TEXT("Diagnostic-only raw-sim body group for activated standing. 0=off, 1=pelvis+spine, 2=+thighs, 3=+support bodies, 4=full required set."),
		ECVF_Default);
	TAutoConsoleVariable<int32> CVarPhysAnimV0PlantEarlyControlZeroGroup(
		TEXT("p.PhysAnim.V0PlantEarlyControlZeroGroup"),
		0,
		TEXT("Diagnostic-only V0 Case A early control-zero group. 0=off, 1=all V0 controls, 2=torso, 3=thighs, 4=support feet/balls, 5=torso-only(zero thighs+support), 6=thigh-only(zero torso+support), 7=support-only(zero torso+thighs)."),
		ECVF_Default);
	TAutoConsoleVariable<float> CVarPhysAnimV0PlantEarlyControlZeroDurationSeconds(
		TEXT("p.PhysAnim.V0PlantEarlyControlZeroDurationSeconds"),
		0.3f,
		TEXT("Diagnostic-only V0 Case A duration for early control-zero variants."),
		ECVF_Default);
	TAutoConsoleVariable<int32> CVarPhysAnimV0PlantThighRestoreVariant(
		TEXT("p.PhysAnim.V0PlantThighRestoreVariant"),
		0,
		TEXT("Diagnostic-only V0 variant for restoring thigh controls after zero window. 0=off, 1=abrupt(0.20), 2=ramp(0.0-0.20), 3=abrupt(0.05), 4=zero_fixed, 5=abrupt(0.02), 6=abrupt(0.10), 7=abrupt(0.20_actual)."),
		ECVF_Default);
	TAutoConsoleVariable<float> CVarPhysAnimV0PlantThighRestoreRampDurationSeconds(
		TEXT("p.PhysAnim.V0PlantThighRestoreRampDurationSeconds"),
		0.3f,
		TEXT("Diagnostic-only V0 duration for thigh restore ramp (Variant 2)."),
		ECVF_Default);
	TAutoConsoleVariable<float> CVarPhysAnimV0KineticGateThresholdDegPerSec(
		TEXT("p.PhysAnim.V0KineticGateThresholdDegPerSec"),
		3600.0f,
		TEXT("Diagnostic-only V0 kinetic gate threshold (deg/sec) for holding thighs at zero strength when pelvis/spine spikes."),
		ECVF_Default);

	bool ShouldForceDiagnosticRawSimBody(FName BoneName, int32 DiagnosticGroup)
	{
		if (DiagnosticGroup <= 0)
		{
			return false;
		}

		const bool bPelvisOrSpine =
			BoneName == PhysAnimBridge::GetRootBoneName() ||
			BoneName == TEXT("spine_01") ||
			BoneName == TEXT("spine_02") ||
			BoneName == TEXT("spine_03");
		const bool bThigh =
			BoneName == TEXT("thigh_l") ||
			BoneName == TEXT("thigh_r");
		const bool bRequiredSupport =
			BoneName == TEXT("foot_l") ||
			BoneName == TEXT("foot_r") ||
			BoneName == TEXT("ball_l") ||
			BoneName == TEXT("ball_r");

		if (DiagnosticGroup >= 4)
		{
			return PhysAnimBridge::GetRequiredBodyModifierBoneNames().Contains(BoneName);
		}
		if (DiagnosticGroup >= 3)
		{
			return bPelvisOrSpine || bThigh || bRequiredSupport;
		}
		if (DiagnosticGroup >= 2)
		{
			return bPelvisOrSpine || bThigh;
		}
		return bPelvisOrSpine;
	}

	bool IsV0GroupCRawSimBody(FName BoneName)
	{
		return
			BoneName == PhysAnimBridge::GetRootBoneName() ||
			BoneName == TEXT("spine_01") ||
			BoneName == TEXT("spine_02") ||
			BoneName == TEXT("spine_03") ||
			BoneName == TEXT("thigh_l") ||
			BoneName == TEXT("thigh_r") ||
			BoneName == TEXT("foot_l") ||
			BoneName == TEXT("foot_r") ||
			BoneName == TEXT("ball_l") ||
			BoneName == TEXT("ball_r");
	}

	FString FormatTransformForV0RawSim(const FTransform& Transform)
	{
		const FVector Location = Transform.GetLocation();
		const FRotator Rotation = Transform.GetRotation().Rotator();
		return FString::Printf(
			TEXT("loc=(%.2f,%.2f,%.2f) rot=(%.2f,%.2f,%.2f)"),
			Location.X,
			Location.Y,
			Location.Z,
			Rotation.Pitch,
			Rotation.Yaw,
			Rotation.Roll);
	}

	FString FormatVectorForV0RawSim(const FVector& Vector)
	{
		return FString::Printf(TEXT("(%.2f,%.2f,%.2f)"), Vector.X, Vector.Y, Vector.Z);
	}

	FString JoinNamesForV0RawSim(const TArray<FString>& Names)
	{
		return Names.Num() > 0 ? FString::Join(Names, TEXT(",")) : TEXT("none");
	}

	struct FV0RawSimBodySnapshot
	{
		bool bValid = false;
		bool bSimulating = false;
		bool bAwake = false;
		ECollisionEnabled::Type BodyCollision = ECollisionEnabled::NoCollision;
		float BodyPhysicsBlendWeight = 0.0f;
		bool bBodyUpdateKinematicFromSimulation = false;
		float MassKg = 0.0f;
		FVector InertiaTensor = FVector::ZeroVector;
		FTransform BodyTransform = FTransform::Identity;
		FVector LinearVelocityCmPerSec = FVector::ZeroVector;
		FVector AngularVelocityRadPerSec = FVector::ZeroVector;
	};

	FV0RawSimBodySnapshot MakeV0RawSimBodySnapshot(const FBodyInstance* BodyInstance)
	{
		FV0RawSimBodySnapshot Snapshot;
		if (!BodyInstance || !BodyInstance->IsValidBodyInstance())
		{
			return Snapshot;
		}

		Snapshot.bValid = true;
		Snapshot.bSimulating = BodyInstance->IsInstanceSimulatingPhysics();
		Snapshot.bAwake = BodyInstance->IsInstanceAwake();
		Snapshot.BodyCollision = BodyInstance->GetCollisionEnabled();
		Snapshot.BodyPhysicsBlendWeight = BodyInstance->PhysicsBlendWeight;
		Snapshot.bBodyUpdateKinematicFromSimulation = BodyInstance->bUpdateKinematicFromSimulation != 0;
		Snapshot.MassKg = BodyInstance->GetBodyMass();
		Snapshot.InertiaTensor = BodyInstance->GetBodyInertiaTensor();
		Snapshot.BodyTransform = BodyInstance->GetUnrealWorldTransform();
		Snapshot.LinearVelocityCmPerSec = BodyInstance->GetUnrealWorldVelocity();
		Snapshot.AngularVelocityRadPerSec = BodyInstance->GetUnrealWorldAngularVelocityInRadians();
		return Snapshot;
	}

	struct FV0RawSimOverlapSummary
	{
		int32 WorldOverlapCount = 0;
		int32 CapsuleOverlapCount = 0;
		int32 SkeletalBodyOverlapCount = 0;
		TArray<FString> ContactBodies;
	};

	FV0RawSimOverlapSummary BuildV0RawSimOverlapSummary(
		const UPhysAnimComponent* OwnerComponent,
		USkeletalMeshComponent* SkeletalMesh,
		FName BoneName,
		const FBodyInstance* BodyInstance,
		const FTransform& BodyTransform)
	{
		FV0RawSimOverlapSummary Summary;
		if (!OwnerComponent || !SkeletalMesh || !BodyInstance || !BodyInstance->IsValidBodyInstance())
		{
			return Summary;
		}

		const UWorld* const World = OwnerComponent->GetWorld();
		if (World)
		{
			TArray<FOverlapResult> Overlaps;
			FComponentQueryParams QueryParams(TEXT("V0RawSimBodyEnable"), OwnerComponent->GetOwner());
			QueryParams.AddIgnoredComponent(SkeletalMesh);
			FCollisionResponseParams ResponseParams(BodyInstance->GetResponseToChannels());
			const FCollisionObjectQueryParams ObjectParams(FCollisionObjectQueryParams::InitType::AllObjects);
			BodyInstance->OverlapMulti(
				Overlaps,
				World,
				nullptr,
				BodyTransform.GetLocation(),
				BodyTransform.GetRotation(),
				BodyInstance->GetObjectType(),
				QueryParams,
				ResponseParams,
				ObjectParams);

			for (const FOverlapResult& Overlap : Overlaps)
			{
				UPrimitiveComponent* const OverlapComponent = Overlap.GetComponent();
				if (!OverlapComponent)
				{
					continue;
				}

				++Summary.WorldOverlapCount;
				Summary.ContactBodies.Add(FString::Printf(
					TEXT("world:%s:%s:%d"),
					Overlap.GetActor() ? *Overlap.GetActor()->GetName() : TEXT("none"),
					*OverlapComponent->GetName(),
					Overlap.GetItemIndex()));
			}
		}

		if (const ACharacter* const CharacterOwner = Cast<ACharacter>(OwnerComponent->GetOwner()))
		{
			if (const UCapsuleComponent* const Capsule = CharacterOwner->GetCapsuleComponent())
			{
				if (FBodyInstance* const CapsuleBody = Capsule->GetBodyInstance())
				{
					if (BodyInstance->OverlapTestForBody(BodyTransform.GetLocation(), BodyTransform.GetRotation(), CapsuleBody))
					{
						++Summary.CapsuleOverlapCount;
						Summary.ContactBodies.Add(TEXT("capsule"));
					}
				}
			}
		}

		for (const FName OtherBoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
		{
			if (OtherBoneName == BoneName)
			{
				continue;
			}

			if (FBodyInstance* const OtherBody = SkeletalMesh->GetBodyInstance(OtherBoneName))
			{
				if (BodyInstance->OverlapTestForBody(BodyTransform.GetLocation(), BodyTransform.GetRotation(), OtherBody))
				{
					++Summary.SkeletalBodyOverlapCount;
					Summary.ContactBodies.Add(FString::Printf(TEXT("skeletal:%s"), *OtherBoneName.ToString()));
				}
			}
		}

		return Summary;
	}

	void LogV0RawSimBodyEnable(
		const UPhysAnimComponent* OwnerComponent,
		const UPhysicsControlComponent* PhysicsControl,
		USkeletalMeshComponent* SkeletalMesh,
		FName BoneName,
		const FTransform& BoneWorldBefore,
		const FV0RawSimBodySnapshot& Before,
		const FV0RawSimBodySnapshot& After,
		EPhysicsMovementType PreviousModifierMovementType,
		EPhysicsMovementType NewModifierMovementType,
		ECollisionEnabled::Type NewCollisionType,
		float NewPhysicsBlendWeight,
		bool bNewUpdateKinematicFromSimulation)
	{
		const FPhysicsBodyModifierRecord* const Record =
			PhysicsControl
				? FPhysAnimPhysicsControlAccessor::GetModifierRecord(
					PhysicsControl,
					PhysAnimBridge::MakeBodyModifierName(BoneName))
				: nullptr;
		const FPhysicsControlModifierData* const ModifierData = Record ? &Record->BodyModifier.ModifierData : nullptr;
		const FV0RawSimOverlapSummary OverlapSummary = BuildV0RawSimOverlapSummary(
			OwnerComponent,
			SkeletalMesh,
			BoneName,
			SkeletalMesh ? SkeletalMesh->GetBodyInstance(BoneName) : nullptr,
			After.BodyTransform);

		PHYSANIM_LOG(
			LogPhysAnimBridge,
			Warning,
			TEXT("[PhysAnimV0] RAW_SIM_ENABLE bone=%s activationT=%.3f runtimeState=%s boneBefore{%s} bodyBefore{valid=%d sim=%d awake=%d collision=%d blend=%.2f updateKinFromSim=%d mass=%.3f inertia=%s xf=%s lin=%s angRad=%s} bodyAfter{valid=%d sim=%d awake=%d collision=%d blend=%.2f updateKinFromSim=%d mass=%.3f inertia=%s xf=%s lin=%s angRad=%s} modifier{prevMove=%s intendedMove=%s recordMove=%s intendedCollision=%d recordCollision=%d intendedBlend=%.2f recordBlend=%.2f intendedUpdateKinFromSim=%d recordUpdateKinFromSim=%d} penetration{world=%d capsule=%d skeletal=%d bodies=%s}"),
			*BoneName.ToString(),
			OwnerComponent ? OwnerComponent->GetActivatedStandingStabilityMetrics().ActivationDurationSec : -1.0,
			OwnerComponent ? OwnerComponent->GetRuntimeStateName(OwnerComponent->GetRuntimeState()) : TEXT("none"),
			*FormatTransformForV0RawSim(BoneWorldBefore),
			Before.bValid ? 1 : 0,
			Before.bSimulating ? 1 : 0,
			Before.bAwake ? 1 : 0,
			static_cast<int32>(Before.BodyCollision),
			Before.BodyPhysicsBlendWeight,
			Before.bBodyUpdateKinematicFromSimulation ? 1 : 0,
			Before.MassKg,
			*FormatVectorForV0RawSim(Before.InertiaTensor),
			*FormatTransformForV0RawSim(Before.BodyTransform),
			*FormatVectorForV0RawSim(Before.LinearVelocityCmPerSec),
			*FormatVectorForV0RawSim(Before.AngularVelocityRadPerSec),
			After.bValid ? 1 : 0,
			After.bSimulating ? 1 : 0,
			After.bAwake ? 1 : 0,
			static_cast<int32>(After.BodyCollision),
			After.BodyPhysicsBlendWeight,
			After.bBodyUpdateKinematicFromSimulation ? 1 : 0,
			After.MassKg,
			*FormatVectorForV0RawSim(After.InertiaTensor),
			*FormatTransformForV0RawSim(After.BodyTransform),
			*FormatVectorForV0RawSim(After.LinearVelocityCmPerSec),
			*FormatVectorForV0RawSim(After.AngularVelocityRadPerSec),
			UPhysAnimComponent::GetPhysicsMovementTypeName(PreviousModifierMovementType),
			UPhysAnimComponent::GetPhysicsMovementTypeName(NewModifierMovementType),
			ModifierData ? UPhysAnimComponent::GetPhysicsMovementTypeName(ModifierData->MovementType) : TEXT("none"),
			static_cast<int32>(NewCollisionType),
			ModifierData ? static_cast<int32>(ModifierData->CollisionType.GetValue()) : -1,
			NewPhysicsBlendWeight,
			ModifierData ? ModifierData->PhysicsBlendWeight : -1.0f,
			bNewUpdateKinematicFromSimulation ? 1 : 0,
			ModifierData && ModifierData->bUpdateKinematicFromSimulation ? 1 : 0,
			OverlapSummary.WorldOverlapCount,
			OverlapSummary.CapsuleOverlapCount,
			OverlapSummary.SkeletalBodyOverlapCount,
			*JoinNamesForV0RawSim(OverlapSummary.ContactBodies));
	}

	TMap<const UPhysAnimComponent*, TSet<FName>> EnabledBodiesByComponent;
	TMap<const UPhysAnimComponent*, TSet<FName>> LoggedBodiesByComponent;

	void LogV0RawSimGroupCCompleteIfReady(const UPhysAnimComponent* OwnerComponent, FName BoneName, bool bAfterRawSimulating)
	{
		if (!OwnerComponent || !bAfterRawSimulating || !IsV0GroupCRawSimBody(BoneName))
		{
			return;
		}

		TSet<FName>& EnabledBodies = EnabledBodiesByComponent.FindOrAdd(OwnerComponent);
		const int32 PreviousCount = EnabledBodies.Num();
		EnabledBodies.Add(BoneName);
		if (PreviousCount < 10 && EnabledBodies.Num() == 10)
		{
			PHYSANIM_LOG_RATE_LIMITED(
				LogPhysAnimBridge,
				Warning,
				1.0f,
				TEXT("[PhysAnimV0] RAW_SIM_GROUP_C_COMPLETE activationT=%.3f runtimeState=%s simBodies=10 excludedSimMax=%d bodies=pelvis,spine_01,spine_02,spine_03,thigh_l,thigh_r,foot_l,foot_r,ball_l,ball_r"),
				OwnerComponent->GetActivatedStandingStabilityMetrics().ActivationDurationSec,
				OwnerComponent->GetRuntimeStateName(OwnerComponent->GetRuntimeState()),
				OwnerComponent->GetActivatedStandingStabilityMetrics().ExcludedRequiredBodySimulatingCountMax);
		}
	}

	bool MarkV0RawSimBodyEnableLogged(const UPhysAnimComponent* OwnerComponent, FName BoneName)
	{
		if (!OwnerComponent || !IsV0GroupCRawSimBody(BoneName))
		{
			return false;
		}

		TSet<FName>& LoggedBodies = LoggedBodiesByComponent.FindOrAdd(OwnerComponent);
		if (LoggedBodies.Contains(BoneName))
		{
			return false;
		}

		LoggedBodies.Add(BoneName);
		return true;
	}

	bool IsHipQuarantineTraceBody(FName BoneName)
	{
		return
			BoneName == PhysAnimBridge::GetRootBoneName() ||
			BoneName == TEXT("thigh_l") ||
			BoneName == TEXT("thigh_r") ||
			BoneName == TEXT("spine_01") ||
			BoneName == TEXT("spine_02") ||
			BoneName == TEXT("spine_03") ||
			BoneName == TEXT("foot_l") ||
			BoneName == TEXT("foot_r") ||
			BoneName == TEXT("ball_l") ||
			BoneName == TEXT("ball_r");
	}

	bool IsV0ControlTraceBody(FName BoneName)
	{
		return
			BoneName == TEXT("thigh_l") ||
			BoneName == TEXT("thigh_r") ||
			BoneName == TEXT("foot_l") ||
			BoneName == TEXT("foot_r") ||
			BoneName == TEXT("ball_l") ||
			BoneName == TEXT("ball_r") ||
			BoneName == TEXT("spine_01") ||
			BoneName == TEXT("spine_02") ||
			BoneName == TEXT("spine_03");
	}

	bool ShouldZeroV0PlantEarlyControl(FName BoneName, int32 ZeroGroup)
	{
		if (ZeroGroup <= 0)
		{
			return false;
		}
		if (ZeroGroup == 1)
		{
			return IsV0ControlTraceBody(BoneName);
		}
		if (ZeroGroup == 2)
		{
			return
				BoneName == TEXT("spine_01") ||
				BoneName == TEXT("spine_02") ||
				BoneName == TEXT("spine_03");
		}
		if (ZeroGroup == 3)
		{
			return BoneName == TEXT("thigh_l") || BoneName == TEXT("thigh_r");
		}
		if (ZeroGroup == 4)
		{
			return
				BoneName == TEXT("foot_l") ||
				BoneName == TEXT("foot_r") ||
				BoneName == TEXT("ball_l") ||
				BoneName == TEXT("ball_r");
		}
		if (ZeroGroup == 5) // torso-only (zeros thighs + support)
		{
			return
				BoneName == TEXT("thigh_l") ||
				BoneName == TEXT("thigh_r") ||
				BoneName == TEXT("foot_l") ||
				BoneName == TEXT("foot_r") ||
				BoneName == TEXT("ball_l") ||
				BoneName == TEXT("ball_r");
		}
		if (ZeroGroup == 6) // thigh-only (zeros torso + support)
		{
			return
				BoneName == TEXT("spine_01") ||
				BoneName == TEXT("spine_02") ||
				BoneName == TEXT("spine_03") ||
				BoneName == TEXT("foot_l") ||
				BoneName == TEXT("foot_r") ||
				BoneName == TEXT("ball_l") ||
				BoneName == TEXT("ball_r");
		}
		if (ZeroGroup == 7) // support-only (zeros torso + thighs)
		{
			return
				BoneName == TEXT("spine_01") ||
				BoneName == TEXT("spine_02") ||
				BoneName == TEXT("spine_03") ||
				BoneName == TEXT("thigh_l") ||
				BoneName == TEXT("thigh_r");
		}
		return false;
	}

	const TCHAR* GetV0PlantEarlyControlZeroGroupName(int32 ZeroGroup)
	{
		switch (ZeroGroup)
		{
		case 1:
			return TEXT("all_v0");
		case 2:
			return TEXT("torso");
		case 3:
			return TEXT("thighs");
		case 4:
			return TEXT("support");
		case 5:
			return TEXT("torso-only");
		case 6:
			return TEXT("thigh-only");
		case 7:
			return TEXT("support-only");
		default:
			return TEXT("off");
		}
	}

	struct FHipQuarantineControlIntent
	{
		bool bValid = false;
		bool bEnabled = false;
		FPhysicsControlMultiplier Multiplier;
	};

	struct FHipQuarantineModifierSnapshot
	{
		bool bValid = false;
		EPhysicsMovementType MovementType = EPhysicsMovementType::Static;
		ECollisionEnabled::Type CollisionType = ECollisionEnabled::NoCollision;
		float PhysicsBlendWeight = 0.0f;
		bool bUpdateKinematicFromSimulation = false;
	};

	FHipQuarantineModifierSnapshot MakeHipQuarantineModifierSnapshot(
		const UPhysicsControlComponent* PhysicsControl,
		FName BoneName)
	{
		FHipQuarantineModifierSnapshot Snapshot;
		const FPhysicsBodyModifierRecord* const Record =
			PhysicsControl
				? FPhysAnimPhysicsControlAccessor::GetModifierRecord(
					PhysicsControl,
					PhysAnimBridge::MakeBodyModifierName(BoneName))
				: nullptr;
		if (!Record)
		{
			return Snapshot;
		}

		const FPhysicsControlModifierData& ModifierData = Record->BodyModifier.ModifierData;
		Snapshot.bValid = true;
		Snapshot.MovementType = ModifierData.MovementType;
		Snapshot.CollisionType = ModifierData.CollisionType;
		Snapshot.PhysicsBlendWeight = ModifierData.PhysicsBlendWeight;
		Snapshot.bUpdateKinematicFromSimulation = ModifierData.bUpdateKinematicFromSimulation;
		return Snapshot;
	}

	struct FHipQuarantineTraceSnapshot
	{
		FV0RawSimBodySnapshot Body;
		FHipQuarantineModifierSnapshot Modifier;
	};

	FString BuildHipQuarantineChangeSummary(
		const FHipQuarantineTraceSnapshot* Previous,
		const FHipQuarantineTraceSnapshot& Current)
	{
		if (!Previous)
		{
			return TEXT("baseline=1");
		}

		return FString::Printf(
			TEXT("bodySim=%d bodyAwake=%d bodyCollision=%d bodyBlend=%d bodyUpdateKin=%d modifierMove=%d modifierCollision=%d modifierBlend=%d modifierUpdateKin=%d"),
			Previous->Body.bSimulating != Current.Body.bSimulating ? 1 : 0,
			Previous->Body.bAwake != Current.Body.bAwake ? 1 : 0,
			Previous->Body.BodyCollision != Current.Body.BodyCollision ? 1 : 0,
			!FMath::IsNearlyEqual(Previous->Body.BodyPhysicsBlendWeight, Current.Body.BodyPhysicsBlendWeight) ? 1 : 0,
			Previous->Body.bBodyUpdateKinematicFromSimulation != Current.Body.bBodyUpdateKinematicFromSimulation ? 1 : 0,
			Previous->Modifier.MovementType != Current.Modifier.MovementType ? 1 : 0,
			Previous->Modifier.CollisionType != Current.Modifier.CollisionType ? 1 : 0,
			!FMath::IsNearlyEqual(Previous->Modifier.PhysicsBlendWeight, Current.Modifier.PhysicsBlendWeight) ? 1 : 0,
			Previous->Modifier.bUpdateKinematicFromSimulation != Current.Modifier.bUpdateKinematicFromSimulation ? 1 : 0);
	}

	struct FHipQuarantinePendingNextTickTrace
	{
		bool bPending = false;
		uint64 ReleaseFrame = 0;
		double ReleaseWorldTimeSeconds = -1.0;
		double ReleaseActivationTimeSeconds = -1.0;
	};

	TMap<const UPhysAnimComponent*, FHipQuarantinePendingNextTickTrace> HipQuarantinePendingNextTickTraces;
	TMap<const UPhysAnimComponent*, int32> CallCounts;
	TMap<const UPhysAnimComponent*, TMap<FName, FHipQuarantineTraceSnapshot>> PreviousSnapshotsByComponent;

	void LogHipQuarantineTraceFrame(
		const UPhysAnimComponent* OwnerComponent,
		const UPhysicsControlComponent* PhysicsControl,
		USkeletalMeshComponent* SkeletalMesh,
		const TCHAR* Phase,
		uint64 ReleaseFrame,
		double ReleaseWorldTimeSeconds,
		double ReleaseActivationTimeSeconds,
		int32 TicksBefore,
		int32 TicksAfter,
		bool bQuarantineActiveForTuning,
		const TMap<FName, FHipQuarantineControlIntent>& ControlIntents)
	{
		if (!OwnerComponent || !SkeletalMesh)
		{
			return;
		}

		const uint64 CurrentFrame = GFrameNumber;
		const double WorldTimeSeconds = OwnerComponent->GetWorld() ? OwnerComponent->GetWorld()->GetTimeSeconds() : -1.0;
		const double ActivationTimeSeconds = OwnerComponent->GetActivatedStandingStabilityMetrics().ActivationDurationSec;
		TMap<FName, FHipQuarantineTraceSnapshot>& PreviousSnapshots = PreviousSnapshotsByComponent.FindOrAdd(OwnerComponent);
		for (const FName BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
		{
			if (!IsHipQuarantineTraceBody(BoneName))
			{
				continue;
			}

			const FBodyInstance* const BodyInstance = SkeletalMesh->GetBodyInstance(BoneName);
			FHipQuarantineTraceSnapshot Current;
			Current.Body = MakeV0RawSimBodySnapshot(BodyInstance);
			Current.Modifier = MakeHipQuarantineModifierSnapshot(PhysicsControl, BoneName);
			const FHipQuarantineTraceSnapshot* const Previous = PreviousSnapshots.Find(BoneName);
			const FString ChangeSummary = BuildHipQuarantineChangeSummary(Previous, Current);
			PreviousSnapshots.FindOrAdd(BoneName) = Current;

			const FTransform BoneWorldTransform = SkeletalMesh->GetBoneTransform(BoneName, RTS_World);
			const FHipQuarantineControlIntent* const ControlIntent = ControlIntents.Find(BoneName);
			const FV0RawSimOverlapSummary OverlapSummary = BuildV0RawSimOverlapSummary(
				OwnerComponent,
				SkeletalMesh,
				BoneName,
				BodyInstance,
				Current.Body.BodyTransform);
			const double LinearSpeedCmPerSecond = Current.Body.LinearVelocityCmPerSec.Size();
			const double AngularSpeedDegPerSecond =
				FMath::RadiansToDegrees(Current.Body.AngularVelocityRadPerSec.Size());

			PHYSANIM_LOG_RATE_LIMITED(
				LogPhysAnimBridge,
				Warning,
				1.0f,
				TEXT("[PhysAnimV0] HIP_QUARANTINE_TRACE phase=%s frame=%llu worldT=%.3f activationT=%.3f releaseFrame=%llu releaseWorldT=%.3f releaseActivationT=%.3f ticksBefore=%d ticksAfter=%d activeForTuning=%d bone=%s boneWorld{%s} body{valid=%d sim=%d awake=%d collision=%d blend=%.2f updateKinFromSim=%d mass=%.3f inertia=%s xf=%s lin=%s linSpeed=%.2f angRad=%s angDeg=%.2f} modifier{valid=%d move=%s collision=%d blend=%.2f updateKinFromSim=%d} control{valid=%d enabled=%d angularStrength=%.4f angularDamping=%.4f angularExtraDamping=%.4f} penetration{world=%d capsule=%d skeletal=%d bodies=%s} changed{%s}"),
				Phase,
				CurrentFrame,
				WorldTimeSeconds,
				ActivationTimeSeconds,
				ReleaseFrame,
				ReleaseWorldTimeSeconds,
				ReleaseActivationTimeSeconds,
				TicksBefore,
				TicksAfter,
				bQuarantineActiveForTuning ? 1 : 0,
				*BoneName.ToString(),
				*FormatTransformForV0RawSim(BoneWorldTransform),
				Current.Body.bValid ? 1 : 0,
				Current.Body.bSimulating ? 1 : 0,
				Current.Body.bAwake ? 1 : 0,
				static_cast<int32>(Current.Body.BodyCollision),
				Current.Body.BodyPhysicsBlendWeight,
				Current.Body.bBodyUpdateKinematicFromSimulation ? 1 : 0,
				Current.Body.MassKg,
				*FormatVectorForV0RawSim(Current.Body.InertiaTensor),
				*FormatTransformForV0RawSim(Current.Body.BodyTransform),
				*FormatVectorForV0RawSim(Current.Body.LinearVelocityCmPerSec),
				LinearSpeedCmPerSecond,
				*FormatVectorForV0RawSim(Current.Body.AngularVelocityRadPerSec),
				AngularSpeedDegPerSecond,
				Current.Modifier.bValid ? 1 : 0,
				Current.Modifier.bValid ? UPhysAnimComponent::GetPhysicsMovementTypeName(Current.Modifier.MovementType) : TEXT("none"),
				Current.Modifier.bValid ? static_cast<int32>(Current.Modifier.CollisionType) : -1,
				Current.Modifier.bValid ? Current.Modifier.PhysicsBlendWeight : -1.0f,
				Current.Modifier.bValid && Current.Modifier.bUpdateKinematicFromSimulation ? 1 : 0,
				ControlIntent && ControlIntent->bValid ? 1 : 0,
				ControlIntent && ControlIntent->bEnabled ? 1 : 0,
				ControlIntent ? ControlIntent->Multiplier.AngularStrengthMultiplier : -1.0f,
				ControlIntent ? ControlIntent->Multiplier.AngularDampingRatioMultiplier : -1.0f,
				ControlIntent ? ControlIntent->Multiplier.AngularExtraDampingMultiplier : -1.0f,
				OverlapSummary.WorldOverlapCount,
				OverlapSummary.CapsuleOverlapCount,
				OverlapSummary.SkeletalBodyOverlapCount,
				*JoinNamesForV0RawSim(OverlapSummary.ContactBodies),
				*ChangeSummary);
		}
	}

	struct FPerComponentEarlyControlState
	{
		uint8 LoggedMilestoneMask = 0;
		bool bLoggedFirstLinearThreshold = false;
		bool bLoggedFirstAngularThreshold = false;
		bool bLoggedRestoreStarted = false;
	};

	TMap<const UPhysAnimComponent*, TMap<int32, FPerComponentEarlyControlState>> StatesByComponent;

	void LogV0PlantEarlyControlDiagnostics(
		const UPhysAnimComponent* OwnerComponent,
		const UPhysicsControlComponent* PhysicsControl,
		USkeletalMeshComponent* SkeletalMesh,
		int32 ZeroGroup,
		bool bZeroWindowActive,
		float ZeroDurationSeconds,
		const TMap<FName, FHipQuarantineControlIntent>& ControlIntents)
	{
		if (!OwnerComponent ||
			!SkeletalMesh ||
			ZeroGroup <= 0 ||
			OwnerComponent->GetRuntimeState() != EPhysAnimRuntimeState::BalanceActive_Standing)
		{
			return;
		}
		TMap<int32, FPerComponentEarlyControlState>& StatesByZeroGroup = StatesByComponent.FindOrAdd(OwnerComponent);
		FPerComponentEarlyControlState& State = StatesByZeroGroup.FindOrAdd(ZeroGroup);
		const double ActivationTimeSeconds = OwnerComponent->GetActivatedStandingStabilityMetrics().ActivationDurationSec;
		const FPhysAnimRuntimeTerminationState& TerminationState = OwnerComponent->GetLiveRuntimeEvidenceTerminationState();
		const double SupportHullAreaCm2 = TerminationState.LatestArtifact.SupportHullAreaCm2;
		const int32 ActiveSupportSides = TerminationState.LatestArtifact.ActiveSupportSideCount;
		const TMap<FName, FQuat>& PreviousTargets =
			OwnerComponent->GetPreviousControlTargetRotationsForDiagnostics();
		const TMap<FName, FQuat>& BlendStartTargets =
			OwnerComponent->GetPolicyBlendStartControlTargetRotationsForDiagnostics();
		const TArray<FName>& PendingCachedResets =
			OwnerComponent->GetPendingBodyModifierCachedResetNames();
		const int32 CurrentPoseTargetsSeeded =
			PreviousTargets.Num() > 0 &&
			BlendStartTargets.Num() > 0
				? 1
				: 0;
		const int32 PreviousTargetCount = PreviousTargets.Num();
		const int32 BlendStartTargetCount = BlendStartTargets.Num();
		const int32 PendingCachedResetCount = PendingCachedResets.Num();
		constexpr double LinearThresholdCmPerSecond = 1200.0;
		constexpr double AngularThresholdDegPerSecond = 3600.0;
		constexpr double Milestones[] = { 0.05, 0.10, 0.15, 0.20, 0.30, 0.45, 0.60 };

		const int32 ThighRestoreVariant = CVarPhysAnimV0PlantThighRestoreVariant.GetValueOnGameThread();

		uint8 MilestoneMaskToLog = 0;
		for (int32 MilestoneIndex = 0; MilestoneIndex < UE_ARRAY_COUNT(Milestones); ++MilestoneIndex)
		{
			const uint8 MilestoneBit = static_cast<uint8>(1u << MilestoneIndex);
			if ((State.LoggedMilestoneMask & MilestoneBit) == 0 && ActivationTimeSeconds >= Milestones[MilestoneIndex])
			{
				MilestoneMaskToLog |= MilestoneBit;
				State.LoggedMilestoneMask |= MilestoneBit;
			}
		}

		if (!State.bLoggedRestoreStarted && !bZeroWindowActive && ThighRestoreVariant > 0)
		{
			State.bLoggedRestoreStarted = true;
			// Forçar um log imediato no início do restore
			MilestoneMaskToLog |= 0xFF; // Gambiarra controlada para disparar log em todos os ossos relevantes neste frame
		}

		for (const FName BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
		{
			if (!IsHipQuarantineTraceBody(BoneName))
			{
				continue;
			}

			const FBodyInstance* const BodyInstance = SkeletalMesh->GetBodyInstance(BoneName);
			const FV0RawSimBodySnapshot Body = MakeV0RawSimBodySnapshot(BodyInstance);
			const FHipQuarantineModifierSnapshot Modifier = MakeHipQuarantineModifierSnapshot(PhysicsControl, BoneName);
			const FName ControlName = PhysAnimBridge::MakeControlName(BoneName);
			const FHipQuarantineControlIntent* const ControlIntent = ControlIntents.Find(BoneName);
			const bool bHasPreviousTarget = PreviousTargets.Contains(ControlName);
			const bool bHasBlendStartTarget = BlendStartTargets.Contains(ControlName);
			const bool bHasPendingCachedReset =
				PendingCachedResets.Contains(PhysAnimBridge::MakeBodyModifierName(BoneName));
			const FV0RawSimOverlapSummary OverlapSummary = BuildV0RawSimOverlapSummary(
				OwnerComponent,
				SkeletalMesh,
				BoneName,
				BodyInstance,
				Body.BodyTransform);
			const double LinearSpeedCmPerSecond = Body.LinearVelocityCmPerSec.Size();
			const double AngularSpeedDegPerSecond = FMath::RadiansToDegrees(Body.AngularVelocityRadPerSec.Size());

			double TargetOrientationDeltaDeg = -1.0;
			if (ControlIntent && ControlIntent->bValid)
			{
				FPhysicsControlTarget CurrentTarget;
				PhysicsControl->GetControlTarget(ControlName, CurrentTarget);
				if (const FQuat* PrevTarget = PreviousTargets.Find(ControlName))
				{
					TargetOrientationDeltaDeg = FMath::RadiansToDegrees(FQuat(CurrentTarget.TargetOrientation).AngularDistance(*PrevTarget));
				}
			}

			if (!State.bLoggedFirstLinearThreshold && LinearSpeedCmPerSecond >= LinearThresholdCmPerSecond)
			{
				State.bLoggedFirstLinearThreshold = true;
				PHYSANIM_LOG_RATE_LIMITED(
					LogPhysAnimBridge,
					Warning,
					1.0f,
					TEXT("[PhysAnimV0] EARLY_CONTROL_THRESHOLD variant=%s zeroGroup=%d restoreVariant=%d kind=linear activationT=%.3f bone=%s lin=%.2f angDeg=%.2f targetDeltaDeg=%.2f supportHull=%.2f activeSides=%d zeroActive=%d currentPoseTargetsSeeded=%d previousTargets=%d blendStartTargets=%d pendingCachedResets=%d control{valid=%d enabled=%d angularStrength=%.4f angularDamping=%.4f angularExtraDamping=%.4f} modifier{valid=%d move=%s collision=%d blend=%.2f updateKinFromSim=%d} penetration{world=%d capsule=%d skeletal=%d bodies=%s}"),
					GetV0PlantEarlyControlZeroGroupName(ZeroGroup),
					ZeroGroup,
					ThighRestoreVariant,
					ActivationTimeSeconds,
					*BoneName.ToString(),
					LinearSpeedCmPerSecond,
					AngularSpeedDegPerSecond,
					TargetOrientationDeltaDeg,
					SupportHullAreaCm2,
					ActiveSupportSides,
					bZeroWindowActive ? 1 : 0,
					CurrentPoseTargetsSeeded,
					PreviousTargetCount,
					BlendStartTargetCount,
					PendingCachedResetCount,
					ControlIntent && ControlIntent->bValid ? 1 : 0,
					ControlIntent && ControlIntent->bEnabled ? 1 : 0,
					ControlIntent ? ControlIntent->Multiplier.AngularStrengthMultiplier : -1.0f,
					ControlIntent ? ControlIntent->Multiplier.AngularDampingRatioMultiplier : -1.0f,
					ControlIntent ? ControlIntent->Multiplier.AngularExtraDampingMultiplier : -1.0f,
					Modifier.bValid ? 1 : 0,
					Modifier.bValid ? UPhysAnimComponent::GetPhysicsMovementTypeName(Modifier.MovementType) : TEXT("none"),
					Modifier.bValid ? static_cast<int32>(Modifier.CollisionType) : -1,
					Modifier.bValid ? Modifier.PhysicsBlendWeight : -1.0f,
					Modifier.bValid && Modifier.bUpdateKinematicFromSimulation ? 1 : 0,
					OverlapSummary.WorldOverlapCount,
					OverlapSummary.CapsuleOverlapCount,
					OverlapSummary.SkeletalBodyOverlapCount,
					*JoinNamesForV0RawSim(OverlapSummary.ContactBodies));
			}
			if (!State.bLoggedFirstAngularThreshold && AngularSpeedDegPerSecond >= AngularThresholdDegPerSecond)
			{
				State.bLoggedFirstAngularThreshold = true;
				PHYSANIM_LOG_RATE_LIMITED(
					LogPhysAnimBridge,
					Warning,
					1.0f,
					TEXT("[PhysAnimV0] EARLY_CONTROL_THRESHOLD variant=%s zeroGroup=%d restoreVariant=%d kind=angular activationT=%.3f bone=%s lin=%.2f angDeg=%.2f targetDeltaDeg=%.2f supportHull=%.2f activeSides=%d zeroActive=%d currentPoseTargetsSeeded=%d previousTargets=%d blendStartTargets=%d pendingCachedResets=%d control{valid=%d enabled=%d angularStrength=%.4f angularDamping=%.4f angularExtraDamping=%.4f} modifier{valid=%d move=%s collision=%d blend=%.2f updateKinFromSim=%d} penetration{world=%d capsule=%d skeletal=%d bodies=%s}"),
					GetV0PlantEarlyControlZeroGroupName(ZeroGroup),
					ZeroGroup,
					ThighRestoreVariant,
					ActivationTimeSeconds,
					*BoneName.ToString(),
					LinearSpeedCmPerSecond,
					AngularSpeedDegPerSecond,
					TargetOrientationDeltaDeg,
					SupportHullAreaCm2,
					ActiveSupportSides,
					bZeroWindowActive ? 1 : 0,
					CurrentPoseTargetsSeeded,
					PreviousTargetCount,
					BlendStartTargetCount,
					PendingCachedResetCount,
					ControlIntent && ControlIntent->bValid ? 1 : 0,
					ControlIntent && ControlIntent->bEnabled ? 1 : 0,
					ControlIntent ? ControlIntent->Multiplier.AngularStrengthMultiplier : -1.0f,
					ControlIntent ? ControlIntent->Multiplier.AngularDampingRatioMultiplier : -1.0f,
					ControlIntent ? ControlIntent->Multiplier.AngularExtraDampingMultiplier : -1.0f,
					Modifier.bValid ? 1 : 0,
					Modifier.bValid ? UPhysAnimComponent::GetPhysicsMovementTypeName(Modifier.MovementType) : TEXT("none"),
					Modifier.bValid ? static_cast<int32>(Modifier.CollisionType) : -1,
					Modifier.bValid ? Modifier.PhysicsBlendWeight : -1.0f,
					Modifier.bValid && Modifier.bUpdateKinematicFromSimulation ? 1 : 0,
					OverlapSummary.WorldOverlapCount,
					OverlapSummary.CapsuleOverlapCount,
					OverlapSummary.SkeletalBodyOverlapCount,
					*JoinNamesForV0RawSim(OverlapSummary.ContactBodies));
			}

			for (int32 MilestoneIndex = 0; MilestoneIndex < UE_ARRAY_COUNT(Milestones); ++MilestoneIndex)
			{
				const uint8 MilestoneBit = static_cast<uint8>(1u << MilestoneIndex);
				if ((MilestoneMaskToLog & MilestoneBit) == 0)
				{
					continue;
				}

				PHYSANIM_LOG_RATE_LIMITED(
					LogPhysAnimBridge,
					Warning,
					1.0f,
					TEXT("[PhysAnimV0] EARLY_CONTROL_SAMPLE variant=%s zeroGroup=%d restoreVariant=%d milestone=%.2f activationT=%.3f zeroDuration=%.2f zeroActive=%d bone=%s lin=%.2f angDeg=%.2f targetDeltaDeg=%.2f body{sim=%d awake=%d collision=%d blend=%.2f updateKinFromSim=%d} control{valid=%d enabled=%d angularStrength=%.4f angularDamping=%.4f angularExtraDamping=%.4f} modifier{valid=%d move=%s collision=%d blend=%.2f updateKinFromSim=%d} targets{currentPoseSeeded=%d hasPrevious=%d hasBlendStart=%d pendingCachedReset=%d previousCount=%d blendStartCount=%d pendingResetCount=%d} support{hull=%.2f activeSides=%d} penetration{world=%d capsule=%d skeletal=%d bodies=%s}"),
					GetV0PlantEarlyControlZeroGroupName(ZeroGroup),
					ZeroGroup,
					ThighRestoreVariant,
					Milestones[MilestoneIndex],
					ActivationTimeSeconds,
					ZeroDurationSeconds,
					bZeroWindowActive ? 1 : 0,
					*BoneName.ToString(),
					LinearSpeedCmPerSecond,
					AngularSpeedDegPerSecond,
					TargetOrientationDeltaDeg,
					Body.bSimulating ? 1 : 0,
					Body.bAwake ? 1 : 0,
					static_cast<int32>(Body.BodyCollision),
					Body.BodyPhysicsBlendWeight,
					Body.bBodyUpdateKinematicFromSimulation ? 1 : 0,
					ControlIntent && ControlIntent->bValid ? 1 : 0,
					ControlIntent && ControlIntent->bEnabled ? 1 : 0,
					ControlIntent ? ControlIntent->Multiplier.AngularStrengthMultiplier : -1.0f,
					ControlIntent ? ControlIntent->Multiplier.AngularDampingRatioMultiplier : -1.0f,
					ControlIntent ? ControlIntent->Multiplier.AngularExtraDampingMultiplier : -1.0f,
					Modifier.bValid ? 1 : 0,
					Modifier.bValid ? UPhysAnimComponent::GetPhysicsMovementTypeName(Modifier.MovementType) : TEXT("none"),
					Modifier.bValid ? static_cast<int32>(Modifier.CollisionType) : -1,
					Modifier.bValid ? Modifier.PhysicsBlendWeight : -1.0f,
					Modifier.bValid && Modifier.bUpdateKinematicFromSimulation ? 1 : 0,
					CurrentPoseTargetsSeeded,
					bHasPreviousTarget ? 1 : 0,
					bHasBlendStartTarget ? 1 : 0,
					bHasPendingCachedReset ? 1 : 0,
					PreviousTargetCount,
					BlendStartTargetCount,
					PendingCachedResetCount,
					SupportHullAreaCm2,
					ActiveSupportSides,
					OverlapSummary.WorldOverlapCount,
					OverlapSummary.CapsuleOverlapCount,
					OverlapSummary.SkeletalBodyOverlapCount,
					*JoinNamesForV0RawSim(OverlapSummary.ContactBodies));
			}
		}
	}
}

namespace PhysAnimComponentInternal
{
	void ClearPhysicsTuningDiagnosticCaches(const UPhysAnimComponent* Component)
	{
		EnabledBodiesByComponent.Remove(Component);
		LoggedBodiesByComponent.Remove(Component);
		PreviousSnapshotsByComponent.Remove(Component);
		StatesByComponent.Remove(Component);
		HipQuarantinePendingNextTickTraces.Remove(Component);
		CallCounts.Remove(Component);

		if (Component)
		{
			// Reset per-instance diagnostic tracking state
			const_cast<UPhysAnimComponent*>(Component)->bKineticGateActiveLastFrame = false;
		}
	}
}

bool UPhysAnimComponent::ActivateRuntimePhysicsControl(FString& OutError)
{
	UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	AActor* const OwnerActor = GetOwner();
	if (!PhysicsControl || !OwnerActor)
	{
		OutError = TEXT("Runtime Physics Control activation requires both the owning actor and Physics Control component.");
		return false;
	}

	DeactivateRuntimePhysicsControl(TEXT("ActivateRuntimePhysicsControl"));

	if (UPhysAnimStage1InitializerComponent* const Stage1Initializer = OwnerActor->FindComponentByClass<UPhysAnimStage1InitializerComponent>())
	{
		Stage1Initializer->CreateControls(PhysicsControl);
	}
	else if (UPhysicsControlInitializerComponent* const Initializer = OwnerActor->FindComponentByClass<UPhysicsControlInitializerComponent>())
	{
		Initializer->CreateControls(PhysicsControl);
	}
	else
	{
		OutError = TEXT("Owning actor is missing a runtime Physics Control initializer.");
		return false;
	}

	// Controls are created before targets can be addressed. Keep them disabled until
	// ActivateBridgeFromReadyState seeds every explicit parent-relative target and then
	// publishes runtime tuning, which is the sole enable point for this activation.
	PhysicsControl->SetControlsInSetEnabled(TEXT("All"), false);

	if (!ValidateRuntimePhysicsControl(OutError))
	{
		return false;
	}

	PHYSANIM_LOG(
		LogPhysAnimBridge,
		Log,
		TEXT("[PhysAnim] Runtime operator activation: controls=%d bodyModifiers=%d"),
		PhysicsControl->GetAllControlNames().Num(),
		PhysicsControl->GetAllBodyModifierNames().Num());
	return true;
}


void UPhysAnimComponent::DeactivateRuntimePhysicsControl(const TCHAR* Context)
{
	UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	if (!PhysicsControl)
	{
		return;
	}

	const TArray<FName> ControlNames = PhysicsControl->GetAllControlNames();
	const TArray<FName> BodyModifierNames = PhysicsControl->GetAllBodyModifierNames();
	if (ControlNames.Num() == 0 && BodyModifierNames.Num() == 0)
	{
		return;
	}

	PHYSANIM_LOG(
		LogPhysAnimBridge,
		Log,
		TEXT("[PhysAnim] Runtime operator deactivation[%s]: controls=%d bodyModifiers=%d"),
		Context,
		ControlNames.Num(),
		BodyModifierNames.Num());

	if (BodyModifierNames.Num() > 0)
	{
		PhysicsControl->ResetBodyModifiersToCachedBoneTransforms(BodyModifierNames);
		PhysicsControl->SetCachedBoneVelocitiesToZero();
		PhysicsControl->DestroyBodyModifiers(BodyModifierNames, true, false);
	}

	if (ControlNames.Num() > 0)
	{
		PhysicsControl->DestroyControls(ControlNames, true, false);
	}
}


void UPhysAnimComponent::ActivateBridgePhysicsState(const FPhysAnimStabilizationSettings& EffectiveSettings)
{
	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!SkeletalMesh)
	{
		return;
	}

	if (!bHasSavedMeshCollisionState)
	{
		OriginalMeshCollisionProfileName = SkeletalMesh->GetCollisionProfileName();
		OriginalMeshCollisionEnabled = SkeletalMesh->GetCollisionEnabled();
		OriginalMeshPawnResponse = SkeletalMesh->GetCollisionResponseToChannel(ECC_Pawn);
		bHasSavedMeshCollisionState = true;
	}

	SkeletalMesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	SkeletalMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	SkeletalMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SkeletalMesh->SetSimulatePhysics(true);

	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	if (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn)
	{
		(void)RootBoneName;
	}

	ApplyTrainingAlignedToeLimitPolicy(EffectiveSettings);
	ApplyTrainingAlignedSpineLimitPolicy(EffectiveSettings);
	SkeletalMesh->RecreatePhysicsState();
	SkeletalMesh->SetEnablePhysicsBlending(true);
	SkeletalMesh->WakeAllRigidBodies();
	ApplyTrainingAlignedMassScales(EffectiveSettings);

	const bool bPreserveGameplayShell = ShouldPreserveGameplayShellDuringBridgeActive(
		IsMovementSmokeModeEnabled(),
		PhysAnimComponentInternal::CVarPhysAnimAllowCharacterMovementInBridgeActive.GetValueOnGameThread() != 0);
	if (ACharacter* const CharacterOwner = Cast<ACharacter>(GetOwner()))
	{
		if (UCapsuleComponent* const CapsuleComponent = CharacterOwner->GetCapsuleComponent())
		{
			if (!bHasSavedCapsuleCollisionState)
			{
				OriginalCapsuleCollisionEnabled = CapsuleComponent->GetCollisionEnabled();
				bHasSavedCapsuleCollisionState = true;
			}

			if (!bPreserveGameplayShell)
			{
				CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}

		if (UCharacterMovementComponent* const CharacterMovement = CharacterOwner->GetCharacterMovement())
		{
			if (!bHasSavedCharacterMovementState)
			{
				OriginalCharacterMovementMode = CharacterMovement->MovementMode;
				OriginalCharacterCustomMovementMode = CharacterMovement->CustomMovementMode;
				bOriginalCharacterMovementActive = CharacterMovement->IsActive();
				bOriginalCharacterMovementTickEnabled = CharacterMovement->IsComponentTickEnabled();
				bHasSavedCharacterMovementState = true;
			}

			ApplyCharacterMovementBridgeOwnership(CharacterMovement, bPreserveGameplayShell);
		}
	}

	if (bPreserveGameplayShell)
	{
		PHYSANIM_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnim] BridgeActive preserving capsule collision and CharacterMovement during bridge ownership."));
	}
}


void UPhysAnimComponent::ReassertBridgeActiveStartupProofRawSimulation(const TCHAR* Context)
{
	if (!ShouldPreserveRawSimulationForBridgeActiveStartupProof(
		RuntimeState,
		bEnableLiveRuntimeEvidenceProof,
		bLiveRuntimeEvidenceProofComplete))
	{
		return;
	}

	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!SkeletalMesh)
	{
		return;
	}

	SkeletalMesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	SkeletalMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	SkeletalMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SkeletalMesh->SetSimulatePhysics(true);
	SkeletalMesh->SetEnablePhysicsBlending(true);
	SkeletalMesh->WakeAllRigidBodies();

	PHYSANIM_LOG(LogPhysAnimBridge, Verbose, TEXT("[PhysAnim] BridgeActive startup proof raw simulation reasserted after %s."), Context);
}


void UPhysAnimComponent::ApplyTrainingAlignedMassScales(const FPhysAnimStabilizationSettings& EffectiveSettings)
{
	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!SkeletalMesh)
	{
		return;
	}

	UPhysicsAsset* const PhysicsAsset = SkeletalMesh->GetPhysicsAsset();
	if (!PhysicsAsset)
	{
		return;
	}

	const bool bApplyMassPolicy = ShouldApplyTrainingAlignedMassScales(
		EffectiveSettings.bApplyTrainingAlignedMassScales,
		EffectiveSettings.TrainingAlignedMassScaleBlend);
	if (!bApplyMassPolicy)
	{
		return;
	}

	if (!bHasSavedBodyMassScales)
	{
		OriginalBodyMassScales.Reset();
		for (const USkeletalBodySetup* const BodySetup : PhysicsAsset->SkeletalBodySetups)
		{
			if (!BodySetup)
			{
				continue;
			}

			OriginalBodyMassScales.Add(BodySetup->BoneName, SkeletalMesh->GetMassScale(BodySetup->BoneName));
		}
		bHasSavedBodyMassScales = OriginalBodyMassScales.Num() > 0;
	}

	int32 NumAdjustedBodies = 0;
	for (const USkeletalBodySetup* const BodySetup : PhysicsAsset->SkeletalBodySetups)
	{
		if (!BodySetup)
		{
			continue;
		}

		const float MassScale =
			ResolveTrainingAlignedMassScaleForBone(
				BodySetup->BoneName,
				EffectiveSettings.TrainingAlignedMassScaleBlend);
		SkeletalMesh->SetMassScale(BodySetup->BoneName, MassScale);
		++NumAdjustedBodies;
	}

	PHYSANIM_LOG(
		LogPhysAnimBridge,
		Log,
		TEXT("[PhysAnim] Applied training-aligned Manny mass scales: bodies=%d blend=%.2f"),
		NumAdjustedBodies,
		EffectiveSettings.TrainingAlignedMassScaleBlend);
}


void UPhysAnimComponent::ResetTrainingAlignedMassScales()
{
	if (!bHasSavedBodyMassScales)
	{
		return;
	}

	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!SkeletalMesh)
	{
		OriginalBodyMassScales.Reset();
		bHasSavedBodyMassScales = false;
		return;
	}

	for (const TPair<FName, float>& Pair : OriginalBodyMassScales)
	{
		SkeletalMesh->SetMassScale(Pair.Key, Pair.Value);
	}

	OriginalBodyMassScales.Reset();
	bHasSavedBodyMassScales = false;
}


void UPhysAnimComponent::ApplyTrainingAlignedToeLimitPolicy(const FPhysAnimStabilizationSettings& EffectiveSettings)
{
	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!SkeletalMesh)
	{
		return;
	}

	UPhysicsAsset* const PhysicsAsset = SkeletalMesh->GetPhysicsAsset();
	if (!PhysicsAsset)
	{
		return;
	}

	if (!ShouldApplyTrainingAlignedToeLimitPolicy(
		EffectiveSettings.bApplyTrainingAlignedToeLimitPolicy,
		EffectiveSettings.TrainingAlignedToeLimitPolicyBlend))
	{
		return;
	}

	struct FToeConstraintTarget
	{
		FName ChildBoneName;
		FName ParentBoneName;
	};

	const TArray<FToeConstraintTarget> ToeConstraints =
	{
		{ TEXT("ball_l"), TEXT("foot_l") },
		{ TEXT("ball_r"), TEXT("foot_r") }
	};

	if (!bHasSavedToeConstraintLimits)
	{
		OriginalToeTwistMotions.Reset();
		OriginalToeSwing1Motions.Reset();
		OriginalToeSwing2Motions.Reset();
		OriginalToeTwistLimits.Reset();
		OriginalToeSwing1Limits.Reset();
		OriginalToeSwing2Limits.Reset();
	}

	int32 NumAdjustedToeConstraints = 0;
	const float ClampedBlendAlpha = FMath::Clamp(EffectiveSettings.TrainingAlignedToeLimitPolicyBlend, 0.0f, 1.0f);
	for (const FToeConstraintTarget& ToeConstraint : ToeConstraints)
	{
		const int32 ConstraintIndex = PhysicsAsset->FindConstraintIndex(ToeConstraint.ChildBoneName, ToeConstraint.ParentBoneName);
		if (ConstraintIndex == INDEX_NONE || !PhysicsAsset->ConstraintSetup.IsValidIndex(ConstraintIndex))
		{
			continue;
		}

		UPhysicsConstraintTemplate* const ConstraintTemplate = PhysicsAsset->ConstraintSetup[ConstraintIndex];
		FConstraintInstance* const ConstraintInstance = ConstraintTemplate ? &ConstraintTemplate->DefaultInstance : nullptr;
		if (!ConstraintInstance)
		{
			continue;
		}

		if (!bHasSavedToeConstraintLimits)
		{
			OriginalToeTwistMotions.Add(ToeConstraint.ChildBoneName, static_cast<uint8>(ConstraintInstance->GetAngularTwistMotion()));
			OriginalToeSwing1Motions.Add(ToeConstraint.ChildBoneName, static_cast<uint8>(ConstraintInstance->GetAngularSwing1Motion()));
			OriginalToeSwing2Motions.Add(ToeConstraint.ChildBoneName, static_cast<uint8>(ConstraintInstance->GetAngularSwing2Motion()));
			OriginalToeTwistLimits.Add(ToeConstraint.ChildBoneName, ConstraintInstance->GetAngularTwistLimit());
			OriginalToeSwing1Limits.Add(ToeConstraint.ChildBoneName, ConstraintInstance->GetAngularSwing1Limit());
			OriginalToeSwing2Limits.Add(ToeConstraint.ChildBoneName, ConstraintInstance->GetAngularSwing2Limit());
		}

		const float TargetTwistLimitDegrees = FMath::Lerp(ConstraintInstance->GetAngularTwistLimit(), 20.0f, ClampedBlendAlpha);
		const float TargetSwing1LimitDegrees = FMath::Lerp(ConstraintInstance->GetAngularSwing1Limit(), 20.0f, ClampedBlendAlpha);
		const float TargetSwing2LimitDegrees = FMath::Lerp(ConstraintInstance->GetAngularSwing2Limit(), 20.0f, ClampedBlendAlpha);

		ConstraintInstance->SetAngularTwistLimit(ACM_Limited, TargetTwistLimitDegrees);
		ConstraintInstance->SetAngularSwing1Limit(ACM_Limited, TargetSwing1LimitDegrees);
		ConstraintInstance->SetAngularSwing2Limit(ACM_Limited, TargetSwing2LimitDegrees);
		++NumAdjustedToeConstraints;
	}

	bHasSavedToeConstraintLimits = OriginalToeTwistMotions.Num() > 0;
	if (NumAdjustedToeConstraints > 0)
	{
		PHYSANIM_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnim] Applied training-aligned toe operating limits: constraints=%d blend=%.2f"),
			NumAdjustedToeConstraints,
			EffectiveSettings.TrainingAlignedToeLimitPolicyBlend);
	}
}

void UPhysAnimComponent::ApplyTrainingAlignedSpineLimitPolicy(const FPhysAnimStabilizationSettings& EffectiveSettings)
{
	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!SkeletalMesh)
	{
		return;
	}

	UPhysicsAsset* const PhysicsAsset = SkeletalMesh->GetPhysicsAsset();
	if (!PhysicsAsset)
	{
		return;
	}

	if (!ShouldApplyTrainingAlignedControlFamilyProfile(
		EffectiveSettings.bApplyTrainingAlignedControlFamilyProfile,
		EffectiveSettings.TrainingAlignedControlFamilyProfileBlend))
	{
		return;
	}

	struct FSpineConstraintTarget
	{
		FName ChildBoneName;
		FName ParentBoneName;
	};

	const TArray<FSpineConstraintTarget> SpineConstraints =
	{
		{ TEXT("spine_02"), TEXT("spine_01") },
		{ TEXT("spine_03"), TEXT("spine_02") }
	};

	if (!bHasSavedSpineConstraintLimits)
	{
		OriginalSpineTwistMotions.Reset();
		OriginalSpineSwing1Motions.Reset();
		OriginalSpineSwing2Motions.Reset();
		OriginalSpineTwistLimits.Reset();
		OriginalSpineSwing1Limits.Reset();
		OriginalSpineSwing2Limits.Reset();
	}

	int32 NumAdjustedSpineConstraints = 0;
	const float ClampedBlendAlpha = FMath::Clamp(EffectiveSettings.TrainingAlignedControlFamilyProfileBlend, 0.0f, 1.0f);
	for (const FSpineConstraintTarget& SpineConstraint : SpineConstraints)
	{
		const int32 ConstraintIndex = PhysicsAsset->FindConstraintIndex(SpineConstraint.ChildBoneName, SpineConstraint.ParentBoneName);
		if (ConstraintIndex == INDEX_NONE || !PhysicsAsset->ConstraintSetup.IsValidIndex(ConstraintIndex))
		{
			continue;
		}

		UPhysicsConstraintTemplate* const ConstraintTemplate = PhysicsAsset->ConstraintSetup[ConstraintIndex];
		FConstraintInstance* const ConstraintInstance = ConstraintTemplate ? &ConstraintTemplate->DefaultInstance : nullptr;
		if (!ConstraintInstance)
		{
			continue;
		}

		if (!bHasSavedSpineConstraintLimits)
		{
			OriginalSpineTwistMotions.Add(SpineConstraint.ChildBoneName, static_cast<uint8>(ConstraintInstance->GetAngularTwistMotion()));
			OriginalSpineSwing1Motions.Add(SpineConstraint.ChildBoneName, static_cast<uint8>(ConstraintInstance->GetAngularSwing1Motion()));
			OriginalSpineSwing2Motions.Add(SpineConstraint.ChildBoneName, static_cast<uint8>(ConstraintInstance->GetAngularSwing2Motion()));
			OriginalSpineTwistLimits.Add(SpineConstraint.ChildBoneName, ConstraintInstance->GetAngularTwistLimit());
			OriginalSpineSwing1Limits.Add(SpineConstraint.ChildBoneName, ConstraintInstance->GetAngularSwing1Limit());
			OriginalSpineSwing2Limits.Add(SpineConstraint.ChildBoneName, ConstraintInstance->GetAngularSwing2Limit());
		}

		const float TargetTwistLimitDegrees = FMath::Lerp(ConstraintInstance->GetAngularTwistLimit(), 25.0f, ClampedBlendAlpha);
		const float TargetSwing1LimitDegrees = FMath::Lerp(ConstraintInstance->GetAngularSwing1Limit(), 25.0f, ClampedBlendAlpha);
		const float TargetSwing2LimitDegrees = FMath::Lerp(ConstraintInstance->GetAngularSwing2Limit(), 25.0f, ClampedBlendAlpha);

		ConstraintInstance->SetAngularTwistLimit(ACM_Limited, TargetTwistLimitDegrees);
		ConstraintInstance->SetAngularSwing1Limit(ACM_Limited, TargetSwing1LimitDegrees);
		ConstraintInstance->SetAngularSwing2Limit(ACM_Limited, TargetSwing2LimitDegrees);
		++NumAdjustedSpineConstraints;
	}

	bHasSavedSpineConstraintLimits = OriginalSpineTwistMotions.Num() > 0;
	if (NumAdjustedSpineConstraints > 0)
	{
		PHYSANIM_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnim] Applied training-aligned spine operating limits: constraints=%d blend=%.2f"),
			NumAdjustedSpineConstraints,
			EffectiveSettings.TrainingAlignedControlFamilyProfileBlend);
	}
}


void UPhysAnimComponent::ResetTrainingAlignedToeLimitPolicy()
{
	if (!bHasSavedToeConstraintLimits)
	{
		return;
	}

	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	UPhysicsAsset* const PhysicsAsset = SkeletalMesh ? SkeletalMesh->GetPhysicsAsset() : nullptr;
	if (!PhysicsAsset)
	{
		OriginalToeTwistMotions.Reset();
		OriginalToeSwing1Motions.Reset();
		OriginalToeSwing2Motions.Reset();
		OriginalToeTwistLimits.Reset();
		OriginalToeSwing1Limits.Reset();
		OriginalToeSwing2Limits.Reset();
		bHasSavedToeConstraintLimits = false;
		return;
	}

	for (const TPair<FName, uint8>& Pair : OriginalToeTwistMotions)
	{
		const FName ChildBoneName = Pair.Key;
		const FName ParentBoneName = ChildBoneName == TEXT("ball_l") ? TEXT("foot_l") : TEXT("foot_r");
		const int32 ConstraintIndex = PhysicsAsset->FindConstraintIndex(ChildBoneName, ParentBoneName);
		if (ConstraintIndex == INDEX_NONE || !PhysicsAsset->ConstraintSetup.IsValidIndex(ConstraintIndex))
		{
			continue;
		}

		UPhysicsConstraintTemplate* const ConstraintTemplate = PhysicsAsset->ConstraintSetup[ConstraintIndex];
		FConstraintInstance* const ConstraintInstance = ConstraintTemplate ? &ConstraintTemplate->DefaultInstance : nullptr;
		if (!ConstraintInstance)
		{
			continue;
		}

		const uint8* const Swing1Motion = OriginalToeSwing1Motions.Find(ChildBoneName);
		const uint8* const Swing2Motion = OriginalToeSwing2Motions.Find(ChildBoneName);
		const float* const TwistLimit = OriginalToeTwistLimits.Find(ChildBoneName);
		const float* const Swing1Limit = OriginalToeSwing1Limits.Find(ChildBoneName);
		const float* const Swing2Limit = OriginalToeSwing2Limits.Find(ChildBoneName);
		if (!Swing1Motion || !Swing2Motion || !TwistLimit || !Swing1Limit || !Swing2Limit)
		{
			continue;
		}

		ConstraintInstance->SetAngularTwistLimit(static_cast<EAngularConstraintMotion>(Pair.Value), *TwistLimit);
		ConstraintInstance->SetAngularSwing1Limit(static_cast<EAngularConstraintMotion>(*Swing1Motion), *Swing1Limit);
		ConstraintInstance->SetAngularSwing2Limit(static_cast<EAngularConstraintMotion>(*Swing2Motion), *Swing2Limit);
	}

	OriginalToeTwistMotions.Reset();
	OriginalToeSwing1Motions.Reset();
	OriginalToeSwing2Motions.Reset();
	OriginalToeTwistLimits.Reset();
	OriginalToeSwing1Limits.Reset();
	OriginalToeSwing2Limits.Reset();
	bHasSavedToeConstraintLimits = false;
}

void UPhysAnimComponent::ResetTrainingAlignedSpineLimitPolicy()
{
	if (!bHasSavedSpineConstraintLimits)
	{
		return;
	}

	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	UPhysicsAsset* const PhysicsAsset = SkeletalMesh ? SkeletalMesh->GetPhysicsAsset() : nullptr;
	if (!PhysicsAsset)
	{
		OriginalSpineTwistMotions.Reset();
		OriginalSpineSwing1Motions.Reset();
		OriginalSpineSwing2Motions.Reset();
		OriginalSpineTwistLimits.Reset();
		OriginalSpineSwing1Limits.Reset();
		OriginalSpineSwing2Limits.Reset();
		bHasSavedSpineConstraintLimits = false;
		return;
	}

	for (const TPair<FName, uint8>& Pair : OriginalSpineTwistMotions)
	{
		const FName ChildBoneName = Pair.Key;
		const FName ParentBoneName = ChildBoneName == TEXT("spine_02") ? TEXT("spine_01") : TEXT("spine_02");
		const int32 ConstraintIndex = PhysicsAsset->FindConstraintIndex(ChildBoneName, ParentBoneName);
		if (ConstraintIndex == INDEX_NONE || !PhysicsAsset->ConstraintSetup.IsValidIndex(ConstraintIndex))
		{
			continue;
		}

		UPhysicsConstraintTemplate* const ConstraintTemplate = PhysicsAsset->ConstraintSetup[ConstraintIndex];
		FConstraintInstance* const ConstraintInstance = ConstraintTemplate ? &ConstraintTemplate->DefaultInstance : nullptr;
		if (!ConstraintInstance)
		{
			continue;
		}

		const uint8* const Swing1Motion = OriginalSpineSwing1Motions.Find(ChildBoneName);
		const uint8* const Swing2Motion = OriginalSpineSwing2Motions.Find(ChildBoneName);
		const float* const TwistLimit = OriginalSpineTwistLimits.Find(ChildBoneName);
		const float* const Swing1Limit = OriginalSpineSwing1Limits.Find(ChildBoneName);
		const float* const Swing2Limit = OriginalSpineSwing2Limits.Find(ChildBoneName);
		if (!Swing1Motion || !Swing2Motion || !TwistLimit || !Swing1Limit || !Swing2Limit)
		{
			continue;
		}

		ConstraintInstance->SetAngularTwistLimit(static_cast<EAngularConstraintMotion>(Pair.Value), *TwistLimit);
		ConstraintInstance->SetAngularSwing1Limit(static_cast<EAngularConstraintMotion>(*Swing1Motion), *Swing1Limit);
		ConstraintInstance->SetAngularSwing2Limit(static_cast<EAngularConstraintMotion>(*Swing2Motion), *Swing2Limit);
	}

	OriginalSpineTwistMotions.Reset();
	OriginalSpineSwing1Motions.Reset();
	OriginalSpineSwing2Motions.Reset();
	OriginalSpineTwistLimits.Reset();
	OriginalSpineSwing1Limits.Reset();
	OriginalSpineSwing2Limits.Reset();
	bHasSavedSpineConstraintLimits = false;
}


void UPhysAnimComponent::ResetBridgePhysicsState()
{
	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!SkeletalMesh)
	{
		bHasSavedMeshCollisionState = false;
		OriginalBodyMassScales.Reset();
		bHasSavedBodyMassScales = false;
		return;
	}

	ResetTrainingAlignedMassScales();
	ResetTrainingAlignedSpineLimitPolicy();
	ResetTrainingAlignedToeLimitPolicy();
	SkeletalMesh->SetSimulatePhysics(false);
	
	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	if (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn)
	{
		(void)RootBoneName;
	}

	SkeletalMesh->SetEnablePhysicsBlending(false);
	if (bHasSavedMeshCollisionState)
	{
		SkeletalMesh->SetCollisionProfileName(OriginalMeshCollisionProfileName);
		SkeletalMesh->SetCollisionEnabled(OriginalMeshCollisionEnabled);
		SkeletalMesh->SetCollisionResponseToChannel(ECC_Pawn, OriginalMeshPawnResponse);
		bHasSavedMeshCollisionState = false;
	}

	if (ACharacter* const CharacterOwner = Cast<ACharacter>(GetOwner()))
	{
		if (UCapsuleComponent* const CapsuleComponent = CharacterOwner->GetCapsuleComponent())
		{
			if (bHasSavedCapsuleCollisionState)
			{
				CapsuleComponent->SetCollisionEnabled(OriginalCapsuleCollisionEnabled);
				bHasSavedCapsuleCollisionState = false;
			}
		}

		if (UCharacterMovementComponent* const CharacterMovement = CharacterOwner->GetCharacterMovement())
		{
			if (bHasSavedCharacterMovementState)
			{
				CharacterMovement->SetActive(bOriginalCharacterMovementActive, true);
				CharacterMovement->SetComponentTickEnabled(bOriginalCharacterMovementTickEnabled);
				CharacterMovement->SetMovementMode(static_cast<EMovementMode>(OriginalCharacterMovementMode), OriginalCharacterCustomMovementMode);
				bHasSavedCharacterMovementState = false;
			}
		}
	}
}


bool UPhysAnimComponent::IsInstabilityPrecursorActive() const
{
	return TestOnlyShouldTreatInstabilityPrecursorAsTransitionBlocker(
		RuntimeState,
		RuntimeInstabilityState.UnstableAccumulatedSeconds);
}


void UPhysAnimComponent::GetSimulatingBodies(TArray<FName>& OutBones) const
{
	OutBones.Reset();
	const USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!SkeletalMesh)
	{
		return;
	}

	for (const FName BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
	{
		const FBodyInstance* const BodyInstance = SkeletalMesh->GetBodyInstance(BoneName);
		if (BodyInstance && BodyInstance->IsInstanceSimulatingPhysics())
		{
			OutBones.Add(BoneName);
		}
	}
}


void UPhysAnimComponent::ApplyRuntimeControlTuning(const FPhysAnimStabilizationSettings& EffectiveSettings)
{
	UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	const bool bPhase1Prepare = (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Prepare);
	const bool bPhase1LateValidate = (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_LateValidate);
	const bool bSimulationHandoffSettled = SimulationHandoffAlpha >= (1.0f - KINDA_SMALL_NUMBER);
	const bool bSimulationHandoffCompletedThisTick = bSimulationHandoffSettled && !bLastAppliedSimulationHandoffSettled;
	const bool bPresentationPerturbationOverrideActive = IsPresentationPerturbationOverrideActive();
	const bool bPolicyInfluenceActive = CalculateCurrentPolicyInfluenceAlpha(EffectiveSettings) > KINDA_SMALL_NUMBER;
	const bool bUseSkeletalAnimationTargetRepresentation =
		ShouldUseSkeletalAnimationTargetRepresentation(
			EffectiveSettings.bUseSkeletalAnimationTargets,
			bPolicyInfluenceActive);
	const bool bPreserveBridgeActiveStartupProofRawSimulation =
		ShouldPreserveRawSimulationForBridgeActiveStartupProof(
			RuntimeState,
			bEnableLiveRuntimeEvidenceProof,
			bLiveRuntimeEvidenceProofComplete);

	static uint64 LastFrameNumber = 0;
	const uint64 CurrentFrameNumber = GFrameNumber;
	if (LastFrameNumber != CurrentFrameNumber)
	{
		CallCounts.Empty();
		LastFrameNumber = CurrentFrameNumber;
	}
	int32& CallIndexRef = CallCounts.FindOrAdd(this);
	CallIndexRef++;
	const int32 CurrentCallIndex = CallIndexRef;

	if (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn || RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle)
	{
		if (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn)
		{
			BalanceEntryRootOnFrameCount++;
		}
		else if (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle)
		{
			BalanceEntrySettleFrameCount++;
		}

		if (CurrentCallIndex != 1)
		{
			PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Log, 1.0f, TEXT("[PhysAnimBalance] PHASE2_TUNING_CALL_AUDIT frame=%d callIndex=%d runtimeState=%s owner=%d actor=%s component=%s"),
				static_cast<int32>(CurrentFrameNumber),
				CurrentCallIndex,
				GetRuntimeStateName(RuntimeState),
				static_cast<int32>(FPhysAnimBalanceReadyTransition::ClassifyConditionOwner(BalanceReadyTransition.GetFailureReason())),
				*GetOwner()->GetName(),
				*GetName());
		}
	}

	const bool bAllowRootSim = ShouldAllowBalanceSimulation(EffectiveSettings);
	const bool bRootSimFlipFrame = bAllowRootSim && !bLastAppliedPresentationRootSimulationEnabled;
	const bool bPhase2RootOnGuardWindow = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn;
	if (bRootSimFlipFrame && !bPhase2RootOnGuardWindow)
	{
		HipQuarantineTicksRemaining = 10;
	}
	const bool bHipQuarantineActiveThisFrame = HipQuarantineTicksRemaining > 0;
	const bool bHipQuarantineWillReleaseThisFrame =
		bHipQuarantineActiveThisFrame &&
		HipQuarantineTicksRemaining == 1 &&
		RuntimeState != EPhysAnimRuntimeState::FailStopped &&
		!bKineticGateActiveLastFrame;
	const int32 V0PlantEarlyControlZeroGroup =
		CVarPhysAnimV0PlantEarlyControlZeroGroup.GetValueOnGameThread();
	const float V0PlantEarlyControlZeroDurationSeconds =
		FMath::Max(0.0f, CVarPhysAnimV0PlantEarlyControlZeroDurationSeconds.GetValueOnGameThread());
	const int32 V0PlantThighRestoreVariant =
		CVarPhysAnimV0PlantThighRestoreVariant.GetValueOnGameThread();
	const float V0PlantThighRestoreRampDurationSeconds =
		FMath::Max(0.01f, CVarPhysAnimV0PlantThighRestoreRampDurationSeconds.GetValueOnGameThread());
	const bool bV0PlantEarlyControlZeroWindowActive =
		V0PlantEarlyControlZeroGroup > 0 &&
		(IsBalanceActiveState(RuntimeState) || IsBalanceEntryState(RuntimeState)) &&
		GetActivatedStandingStabilityMetrics().ActivationDurationSec <= V0PlantEarlyControlZeroDurationSeconds;

	float ThighRestoreAlpha = 1.0f;
	if (V0PlantThighRestoreVariant > 0 && 
		(IsBalanceActiveState(RuntimeState) || IsBalanceEntryState(RuntimeState)) && 
		GetActivatedStandingStabilityMetrics().ActivationDurationSec > V0PlantEarlyControlZeroDurationSeconds)
	{
		const float TimeSinceZero = GetActivatedStandingStabilityMetrics().ActivationDurationSec - V0PlantEarlyControlZeroDurationSeconds;
		switch (V0PlantThighRestoreVariant)
		{
		case 1: // abrupt(0.20)
			ThighRestoreAlpha = 0.20f;
			break;
		case 2: // ramp(0.0-0.20)
			ThighRestoreAlpha = FMath::Min(0.20f, 0.20f * (TimeSinceZero / V0PlantThighRestoreRampDurationSeconds));
			break;
		case 3: // abrupt(0.05)
			ThighRestoreAlpha = 0.05f;
			break;
		case 4: // zero_fixed
			ThighRestoreAlpha = 0.0f;
			break;
		case 5: // abrupt(0.02)
			ThighRestoreAlpha = 0.02f;
			break;
		case 6: // abrupt(0.10)
			ThighRestoreAlpha = 0.10f;
			break;
		case 7: // abrupt(0.20)
			ThighRestoreAlpha = 0.20f;
			break;
		case 8: // abrupt(0.01)
			ThighRestoreAlpha = 0.01f;
			break;
		case 9: // abrupt(0.15)
			ThighRestoreAlpha = 0.15f;
			break;
		case 10: // ramp(0.0-0.20, 0.5s)
			ThighRestoreAlpha = FMath::Min(0.20f, 0.20f * (TimeSinceZero / 0.5f));
			break;
		case 11: // ramp(0.0-0.20, 1.0s)
			ThighRestoreAlpha = FMath::Min(0.20f, 0.20f * (TimeSinceZero / 1.0f));
			break;
		default:
			ThighRestoreAlpha = 1.0f;
			break;
		}
	}
	bool bHipQuarantineReleasedThisFrame = false;
	if (!PhysicsControl)
	{
		return;
	}

	if (RuntimeState == EPhysAnimRuntimeState::BalanceSafeDeny)
	{
		LastAppliedStabilizationSettings = EffectiveSettings;
		bLastAppliedSimulationHandoffSettled = bSimulationHandoffSettled;
		LastAppliedControlAuthorityAlpha = CalculateCurrentControlAuthorityAlpha(EffectiveSettings);
		return;
	}

	FHipQuarantinePendingNextTickTrace& HipQuarantinePendingNextTickTrace =
		HipQuarantinePendingNextTickTraces.FindOrAdd(this);
	const bool bTraceHipReleaseFrame =
		IsBalanceActiveState(RuntimeState) &&
		bHipQuarantineWillReleaseThisFrame;
	const bool bTraceHipReleaseNextTick =
		IsBalanceActiveState(RuntimeState) &&
		HipQuarantinePendingNextTickTrace.bPending &&
		HipQuarantinePendingNextTickTrace.ReleaseFrame != CurrentFrameNumber;
	TMap<FName, FHipQuarantineControlIntent> HipQuarantineControlIntents;
	if (bTraceHipReleaseFrame)
	{
		LogHipQuarantineTraceFrame(
			this,
			PhysicsControl,
			MeshComponent.Get(),
			TEXT("release_frame_pre_tuning"),
			CurrentFrameNumber,
			GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0,
			GetActivatedStandingStabilityMetrics().ActivationDurationSec,
			HipQuarantineTicksRemaining,
			HipQuarantineTicksRemaining,
			true,
			HipQuarantineControlIntents);
	}
	else if (bTraceHipReleaseNextTick)
	{
		LogHipQuarantineTraceFrame(
			this,
			PhysicsControl,
			MeshComponent.Get(),
			TEXT("next_tick_pre_tuning"),
			HipQuarantinePendingNextTickTrace.ReleaseFrame,
			HipQuarantinePendingNextTickTrace.ReleaseWorldTimeSeconds,
			HipQuarantinePendingNextTickTrace.ReleaseActivationTimeSeconds,
			HipQuarantineTicksRemaining,
			HipQuarantineTicksRemaining,
			false,
			HipQuarantineControlIntents);
	}

	const float OwnerPlanarSpeedCmPerSec = [this]() -> float
	{
		const AActor* const OwnerActor = GetOwner();
		if (!OwnerActor)
		{
			return 0.0f;
		}

		const FVector OwnerVelocity = OwnerActor->GetVelocity();
		return FVector(OwnerVelocity.X, OwnerVelocity.Y, 0.0f).Size();
	}();
	const bool bHasActiveMovementIntent = [this]() -> bool
	{
		const APawn* const OwnerPawn = Cast<APawn>(GetOwner());
		const FVector PendingInput = OwnerPawn ? OwnerPawn->GetPendingMovementInputVector() : FVector::ZeroVector;
		const FVector LastInput = OwnerPawn ? OwnerPawn->GetLastMovementInputVector() : FVector::ZeroVector;
		const FVector PendingPlanarInput(PendingInput.X, PendingInput.Y, 0.0f);
		const FVector LastPlanarInput(LastInput.X, LastInput.Y, 0.0f);

		const ACharacter* const CharacterOwner = Cast<ACharacter>(GetOwner());
		const UCharacterMovementComponent* const CharacterMovement = CharacterOwner ? CharacterOwner->GetCharacterMovement() : nullptr;
		const FVector CurrentAcceleration = CharacterMovement ? CharacterMovement->GetCurrentAcceleration() : FVector::ZeroVector;
		const FVector PlanarAcceleration(CurrentAcceleration.X, CurrentAcceleration.Y, 0.0f);

		return PendingPlanarInput.SizeSquared() > UE_KINDA_SMALL_NUMBER ||
			LastPlanarInput.SizeSquared() > UE_KINDA_SMALL_NUMBER ||
			PlanarAcceleration.SizeSquared() > UE_KINDA_SMALL_NUMBER;
	}();
	const float RuntimeDeltaTimeSeconds =
		GetWorld() ? FMath::Max(0.0f, GetWorld()->GetDeltaSeconds()) : 0.0f;
	if (bHasActiveMovementIntent)
	{
		DistalLocomotionCompositionTimeSinceActiveIntentSeconds = 0.0f;
	}
	else if (DistalLocomotionCompositionTimeSinceActiveIntentSeconds >= 0.0f)
	{
		DistalLocomotionCompositionTimeSinceActiveIntentSeconds += RuntimeDeltaTimeSeconds;
	}
	const bool bHasRecentMovementIntent =
		bHasActiveMovementIntent ||
		(DistalLocomotionCompositionTimeSinceActiveIntentSeconds >= 0.0f &&
		 DistalLocomotionCompositionTimeSinceActiveIntentSeconds <= EffectiveSettings.DistalLocomotionCompositionPolicyIntentGraceSeconds);
	const bool bHasLocomotionEntrySignal =
		bHasRecentMovementIntent ||
		BridgeTrajectoryState.DesiredVelocityCmPerSecond.Size2D() >= EffectiveSettings.BridgePoseSearchIdlePredictedSpeedCutoffCmPerSecond ||
		BridgeTrajectoryState.QueryVelocityCmPerSecond.Size2D() >= EffectiveSettings.BridgePoseSearchIdlePredictedSpeedCutoffCmPerSecond;
	if (EffectiveSettings.bApplyTrainingAlignedDistalLocomotionCompositionPolicy)
	{
		const bool bPreviousDistalLocomotionCompositionModeActive = bDistalLocomotionCompositionModeActive;
		bDistalLocomotionCompositionModeActive =
			UpdateBinarySpeedModeWithIntentLatch(
				bDistalLocomotionCompositionModeActive,
				OwnerPlanarSpeedCmPerSec,
				bHasLocomotionEntrySignal,
				EffectiveSettings.DistalLocomotionCompositionPolicyActivationSpeedCmPerSec,
				EffectiveSettings.DistalLocomotionCompositionPolicyExitSpeedCmPerSec,
				EffectiveSettings.DistalLocomotionCompositionPolicyEnterHoldSeconds,
				EffectiveSettings.DistalLocomotionCompositionPolicyExitHoldSeconds,
				RuntimeDeltaTimeSeconds,
				DistalLocomotionCompositionTimeAboveEnterSeconds,
				DistalLocomotionCompositionTimeBelowExitSeconds);
		if (bPreviousDistalLocomotionCompositionModeActive != bDistalLocomotionCompositionModeActive)
		{
			TRACE_BOOKMARK(
				TEXT("PhysAnim DistalCompositionMode %s speed=%.1f intent=%s"),
				bDistalLocomotionCompositionModeActive ? TEXT("On") : TEXT("Off"),
				OwnerPlanarSpeedCmPerSec,
				bHasActiveMovementIntent ? TEXT("true") : TEXT("false"));
		}
	}
	else
	{
		bDistalLocomotionCompositionModeActive = false;
		DistalLocomotionCompositionTimeAboveEnterSeconds = 0.0f;
		DistalLocomotionCompositionTimeBelowExitSeconds = 0.0f;
		DistalLocomotionCompositionTimeSinceActiveIntentSeconds = -1.0f;
	}

	PhysicsControl->SetControlsInSetEnabled(TEXT("All"), false);
	PhysicsControl->SetControlsInSetUseSkeletalAnimation(
		TEXT("All"),
		bUseSkeletalAnimationTargetRepresentation,
		0.0f,
		0.0f);

	for (const FName BoneName : PhysAnimBridge::GetControlledBoneNames())
	{
		const int32 BringUpGroupIndex = ResolveBringUpGroupIndex(BoneName);
		bool bBringUpGroupUnlocked = IsBringUpGroupUnlocked(BringUpGroupIndex);
		float ControlAuthorityAlpha =
			CalculateBringUpGroupControlAuthorityAlpha(BringUpGroupIndex, EffectiveSettings);

		if (bPhase1Prepare || bPhase1LateValidate)
		{
			const int32 FinalGroupIndex = GetBringUpGroupCount() - 1;
			if (BringUpGroupIndex == 0 || BringUpGroupIndex == 1)
			{
				bBringUpGroupUnlocked = true;
				ControlAuthorityAlpha = 1.0f;
			}
			else if (bPhase1LateValidate && BringUpGroupIndex == FinalGroupIndex)
			{
				// Allow final bring-up group to ramp during LateValidate
				bBringUpGroupUnlocked = IsBringUpGroupUnlocked(BringUpGroupIndex);
				ControlAuthorityAlpha = CalculateBringUpGroupControlAuthorityAlpha(BringUpGroupIndex, EffectiveSettings);
			}
			else
			{
				bBringUpGroupUnlocked = false;
				ControlAuthorityAlpha = 0.0f;
			}
		}
		const bool bApplyTrainingAlignedControlProfile =
			ShouldApplyTrainingAlignedControlFamilyProfile(
				EffectiveSettings.bApplyTrainingAlignedControlFamilyProfile,
				EffectiveSettings.TrainingAlignedControlFamilyProfileBlend);
		const float FamilyStrengthScale =
			bApplyTrainingAlignedControlProfile
				? ResolveTrainingAlignedControlStrengthScaleForBone(
					BoneName,
					EffectiveSettings.TrainingAlignedControlFamilyProfileBlend)
				: 1.0f;
		const float FamilyExtraDampingScale =
			bApplyTrainingAlignedControlProfile
				? ResolveTrainingAlignedControlExtraDampingScaleForBone(
					BoneName,
					EffectiveSettings.TrainingAlignedControlFamilyProfileBlend)
				: 1.0f;
		const bool bLocomotionLowerLimbResponseModeActive =
			ShouldUseLocomotionLowerLimbResponseMode(
				RuntimeState,
				bDistalLocomotionCompositionModeActive);
		const bool bApplyTrainingAlignedLocomotionLowerLimbResponseProfile =
			ShouldApplyTrainingAlignedLocomotionLowerLimbResponsePolicy(
				EffectiveSettings.bApplyTrainingAlignedLocomotionLowerLimbResponsePolicy,
				EffectiveSettings.TrainingAlignedLocomotionLowerLimbResponsePolicyBlend,
				bLocomotionLowerLimbResponseModeActive);
		const bool bApplyPhase1TransitionLowerLimbResponseProfile =
			(bPhase1Prepare || bPhase1LateValidate) &&
			EffectiveSettings.bApplyTrainingAlignedLocomotionLowerLimbResponsePolicy &&
			EffectiveSettings.TrainingAlignedLocomotionLowerLimbResponsePolicyBlend > UE_SMALL_NUMBER;
		const bool bApplySharedLowerLimbResponseProfile =
			bApplyTrainingAlignedLocomotionLowerLimbResponseProfile ||
			bApplyPhase1TransitionLowerLimbResponseProfile;
		const float LocomotionLowerLimbDampingRatioScale =
			bApplySharedLowerLimbResponseProfile
				? ResolveTrainingAlignedLocomotionLowerLimbDampingRatioScaleForBone(
					BoneName,
					EffectiveSettings.TrainingAlignedLocomotionLowerLimbResponsePolicyBlend)
				: 1.0f;
		const float LocomotionLowerLimbExtraDampingScale =
			bApplySharedLowerLimbResponseProfile
				? ResolveTrainingAlignedLocomotionLowerLimbExtraDampingScaleForBone(
					BoneName,
					EffectiveSettings.TrainingAlignedLocomotionLowerLimbResponsePolicyBlend)
				: 1.0f;
		FPhysicsControlMultiplier ControlMultiplier;
		const float HandoverEasing = 1.0f;

		ControlMultiplier.AngularStrengthMultiplier =
			EffectiveSettings.AngularStrengthMultiplier * FamilyStrengthScale * ControlAuthorityAlpha * HandoverEasing;
		ControlMultiplier.AngularStrengthMultiplier *= BalanceReadyTransition.GetProximalControlSoftAlpha(BoneName);
		ControlMultiplier.AngularDampingRatioMultiplier =
			EffectiveSettings.AngularDampingRatioMultiplier * LocomotionLowerLimbDampingRatioScale *
			BalanceReadyTransition.GetTransitionDampingRatioMultiplier(BoneName, EffectiveSettings);
		ControlMultiplier.AngularExtraDampingMultiplier =
			EffectiveSettings.AngularExtraDampingMultiplier * FamilyExtraDampingScale * LocomotionLowerLimbExtraDampingScale *
			BalanceReadyTransition.GetTransitionExtraDampingMultiplier(BoneName, EffectiveSettings);
		if (bHipQuarantineActiveThisFrame &&
			!bPhase2RootOnGuardWindow &&
			(BoneName == "thigh_l" || BoneName == "thigh_r"))
		{
			ControlMultiplier.AngularStrengthMultiplier = 0.0f;
		}
		if (bV0PlantEarlyControlZeroWindowActive &&
			ShouldZeroV0PlantEarlyControl(BoneName, V0PlantEarlyControlZeroGroup))
		{
			ControlMultiplier.AngularStrengthMultiplier = 0.0f;
		}
		else if (V0PlantThighRestoreVariant > 0 && (BoneName == "thigh_l" || BoneName == "thigh_r"))
		{
			ControlMultiplier.AngularStrengthMultiplier *= ThighRestoreAlpha;
		}

		// Kinetic Gating (V0 Diagnostic)
		// Holds thigh angular strength at zero while pelvis/spine angular velocity exceeds threshold.
		// Also detects the gate release frame to snapshot pelvis/spine ω for AC-6 energy answer.
		if ((IsBalanceActiveState(RuntimeState) || IsBalanceEntryState(RuntimeState)) && (BoneName == "thigh_l" || BoneName == "thigh_r"))
		{
			if (const USkeletalMeshComponent* const Mesh = MeshComponent.Get())
			{
				const FName PelvisName = PhysAnimBridge::GetRootBoneName();
				const FBodyInstance* PelvisBody = Mesh->GetBodyInstance(PelvisName);
				const FBodyInstance* Spine01Body = Mesh->GetBodyInstance(TEXT("spine_01"));
				const FBodyInstance* Spine02Body = Mesh->GetBodyInstance(TEXT("spine_02"));
				const FBodyInstance* Spine03Body = Mesh->GetBodyInstance(TEXT("spine_03"));
				
				const auto GetAngVelDeg = [](const FBodyInstance* BI) -> double
				{
					return BI ? FMath::RadiansToDegrees(BI->GetUnrealWorldAngularVelocityInRadians().Size()) : 0.0;
				};

				const double PelvisAngVel = GetAngVelDeg(PelvisBody);
				const double Spine01AngVel = GetAngVelDeg(Spine01Body);
				const double Spine02AngVel = GetAngVelDeg(Spine02Body);
				const double Spine03AngVel = GetAngVelDeg(Spine03Body);

				const float GateThreshold = CVarPhysAnimV0KineticGateThresholdDegPerSec.GetValueOnGameThread();
				const bool bGateActiveNow =
					PelvisAngVel > GateThreshold || Spine01AngVel > GateThreshold ||
					Spine02AngVel > GateThreshold || Spine03AngVel > GateThreshold;

				// Per-component gate state tracking (release edge detection)
				const bool bGateWasActive = bKineticGateActiveLastFrame;

				// Only update the "last frame" state from thigh_l to avoid double-write per tick
				if (BoneName == TEXT("thigh_l"))
				{
					bKineticGateReleasedThisFrame = bGateWasActive && !bGateActiveNow;
					bKineticGateActiveLastFrame = bGateActiveNow;
				}

				if (bGateActiveNow)
				{
					ControlMultiplier.AngularStrengthMultiplier = 0.0f;
					PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f,
						TEXT("[PhysAnimV0] KINETIC_GATE_HOLD bone=%s P=%.1f S1=%.1f S2=%.1f S3=%.1f thresh=%.1f activationT=%.3f"),
						*BoneName.ToString(), PelvisAngVel, Spine01AngVel, Spine02AngVel, Spine03AngVel, GateThreshold,
						GetActivatedStandingStabilityMetrics().ActivationDurationSec);
				}
				else if (bGateWasActive && !bGateActiveNow && BoneName == TEXT("thigh_l"))
				{
					// Gate just released this frame — snapshot pelvis/spine ω and restore strength.
					// This is the definitive AC-6 measurement: does the restored thigh strength
					// add or remove energy from the pelvis/spine chain?
					const double MaxSpineAngVel = FMath::Max3(Spine01AngVel, Spine02AngVel, Spine03AngVel);
					const float EffectiveRestoreStrength = ControlMultiplier.AngularStrengthMultiplier;
					const double ActivationT = GetActivatedStandingStabilityMetrics().ActivationDurationSec;

					++ActivatedStandingStabilityMetrics.KineticGateReleaseCount;
					if (ActivatedStandingStabilityMetrics.PelvisAngVelAtGateRelease < 0.0)
					{
						// First release only — subsequent releases tracked in log only
						ActivatedStandingStabilityMetrics.PelvisAngVelAtGateRelease = PelvisAngVel;
						ActivatedStandingStabilityMetrics.MaxSpineAngVelAtGateRelease = MaxSpineAngVel;
						ActivatedStandingStabilityMetrics.ThighStrengthAtGateRelease = EffectiveRestoreStrength;
						ActivatedStandingStabilityMetrics.ActivationTimeAtGateRelease = ActivationT;
						
						// Synchronize Hip Quarantine release with Kinetic Gate release
						HipQuarantineTicksRemaining = 0;
					}

					PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f,
						TEXT("[PhysAnimV0] KINETIC_GATE_RELEASE pelvisAngVel=%.2f maxSpineAngVel=%.2f "
						     "thighRestoreStrength=%.4f activationT=%.3f releaseN=%d variant=%d thresh=%.1f"),
						PelvisAngVel, MaxSpineAngVel, EffectiveRestoreStrength, ActivationT,
						ActivatedStandingStabilityMetrics.KineticGateReleaseCount,
						CVarPhysAnimV0PlantThighRestoreVariant.GetValueOnGameThread(),
						GateThreshold);
				}
			}
		}

		const FName ControlName = PhysAnimBridge::MakeControlName(BoneName);
		if (bDistalLocomotionCompositionModeActive &&
			ShouldForceExplicitOnlyDistalLocomotionTargetMode(BoneName))
		{
			PhysicsControl->SetControlUseSkeletalAnimation(
				ControlName,
				false,
				0.0f,
				0.0f,
				true,
				false);
		}
		PhysicsControl->SetControlMultiplier(
			ControlName,
			ControlMultiplier,
			bBringUpGroupUnlocked && !EffectiveSettings.bForceZeroActions,
			true,
			false);
		if (((bTraceHipReleaseFrame || bTraceHipReleaseNextTick) && IsHipQuarantineTraceBody(BoneName)) ||
			(V0PlantEarlyControlZeroGroup > 0 && IsV0ControlTraceBody(BoneName)))
		{
			FHipQuarantineControlIntent& ControlIntent = HipQuarantineControlIntents.FindOrAdd(BoneName);
			ControlIntent.bValid = true;
			ControlIntent.bEnabled = bBringUpGroupUnlocked && !EffectiveSettings.bForceZeroActions;
			ControlIntent.Multiplier = ControlMultiplier;
		}

		// One-shot trace capture for preserved spine
		if (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn && (BoneName == "pelvis" || BoneName == "spine_01" || BoneName == "spine_02" || BoneName == "spine_03" || BoneName == "thigh_l" || BoneName == "thigh_r"))
		{
			if (BalanceEntryRootOnFrameCount == 1 || BalanceEntryRootOnFrameCount == 2)
			{
				const FName ModifierName = PhysAnimBridge::MakeBodyModifierName(BoneName);
				const FPhysicsBodyModifierRecord* Record = FPhysAnimPhysicsControlAccessor::GetModifierRecord(PhysicsControl, ModifierName);
				const FBodyInstance* BodyInst = MeshComponent->GetBodyInstance(BoneName);
				
				if (Record && BodyInst)
				{
					const float LinSpeed = BodyInst->GetUnrealWorldVelocity().Size();
					const float AngSpeed = BodyInst->GetUnrealWorldAngularVelocityInRadians().Size();
					const float ExtraDamping = BalanceReadyTransition.GetTransitionExtraDampingMultiplier(BoneName, EffectiveSettings);
					
					if (BalanceEntryRootOnFrameCount == 1 && (BoneName == "spine_01" || BoneName == "spine_02" || BoneName == "spine_03"))
					{
						PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimBalance] PHASE2_PRESERVED_SPINE_EFFECTIVE_ROUTING bone=%s softAlpha=%.4f extraDampingMultiplier=%.2f"),
							*BoneName.ToString(), ControlMultiplier.AngularStrengthMultiplier, ExtraDamping);
					}

					if (BalanceEntryRootOnFrameCount == 1 && (BoneName == "thigh_l" || BoneName == "thigh_r"))
					{
						PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimBalance] PHASE2_PRESERVED_THIGH_EFFECTIVE_ROUTING bone=%s softAlpha=%.4f extraDampingMultiplier=%.2f"),
							*BoneName.ToString(), ControlMultiplier.AngularStrengthMultiplier, ExtraDamping);
					}

					const TCHAR* StateLogName = (BoneName == "thigh_l" || BoneName == "thigh_r") ? TEXT("PHASE2_PRESERVED_THIGH_STATE") : TEXT("PHASE2_PRESERVED_SPINE_STATE");
					PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimBalance] %s source=%s bone=%s rawSim=%d modifier=%s linSpeed=%.2f angSpeed=%.2f controlAlpha=%.4f extraDampingMultiplier=%.2f"),
						StateLogName,
						BalanceEntryRootOnFrameCount == 1 ? TEXT("post_tuning_tick1") : TEXT("pre_updatecontrols_tick2"),
						*BoneName.ToString(),
						BodyInst->IsInstanceSimulatingPhysics() ? 1 : 0,
						UPhysAnimComponent::GetPhysicsMovementTypeName(Record->BodyModifier.ModifierData.MovementType),
						LinSpeed,
						AngSpeed,
						ControlMultiplier.AngularStrengthMultiplier,
						ExtraDamping);
				}
			}
		}

		// Deep diagnostics for Balance Mode Final Ramp Enable
		if (IsBalanceActiveState(RuntimeState) && BringUpGroupIndex == (GetBringUpGroupCount() - 1))
		{
			const double WorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;
			const bool bRampJustStarted = (BringUpGroupControlRampStartTimeSeconds.IsValidIndex(BringUpGroupIndex) && 
										  BringUpGroupControlRampStartTimeSeconds[BringUpGroupIndex] == WorldTime);
			
			if (bRampJustStarted)
			{
				const FTransform BoneTransform = MeshComponent->GetBoneTransform(MeshComponent->GetBoneIndex(BoneName));
				
				PHYSANIM_LOG_RATE_LIMITED(
					LogPhysAnimBridge,
					Log,
					1.0f,
					TEXT("[PhysAnimBalance] FINAL RAMP ENABLE DIAG: bone=%s alpha=%.4f easing=%.4f strength=%.2f useSkelAnim=%s loc=(%.1f, %.1f, %.1f) rot=(%.2f, %.2f, %.2f, %.2f)"),
					*BoneName.ToString(),
					ControlAuthorityAlpha,
					HandoverEasing,
					ControlMultiplier.AngularStrengthMultiplier,
					bUseSkeletalAnimationTargetRepresentation ? TEXT("true") : TEXT("false"),
					BoneTransform.GetLocation().X, BoneTransform.GetLocation().Y, BoneTransform.GetLocation().Z,
					BoneTransform.GetRotation().X, BoneTransform.GetRotation().Y, BoneTransform.GetRotation().Z, BoneTransform.GetRotation().W);
			}
		}
	}


	if (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn || RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle)
	{
		const FName PelvisName = PhysAnimBridge::GetRootBoneName();
		USkeletalMeshComponent* const Mesh = GetMeshComponent();
		if (Mesh)
		{
			if (const FBodyInstance* const PelvisBody = Mesh->GetBodyInstance(PelvisName))
			{
				const bool bActualSimulating = PelvisBody->IsInstanceSimulatingPhysics();
				if (!bActualSimulating)
				{
					const bool bIsFirstFailureTrigger = BalanceReadyTransition.GetFailureReason().IsEmpty();
					if (bIsFirstFailureTrigger && RuntimeState != EPhysAnimRuntimeState::BalanceEntry_RootOn)
					{
						PHYSANIM_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PELVIS_SIM_CHECK_FAIL bone=%s pointer=%d [log]"), *PelvisName.ToString(), bActualSimulating ? 1 : 0);
					}
				}
			}
		}
	}

	// After the control loop, if the ramp just started, log the AFTER state.
	const int32 FinalGroupIndex = GetBringUpGroupCount() - 1;
	const double WorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;
	if (IsBalanceActiveState(RuntimeState) &&
		BringUpGroupControlRampStartTimeSeconds.IsValidIndex(FinalGroupIndex) && 
		BringUpGroupControlRampStartTimeSeconds[FinalGroupIndex] == WorldTime &&
		WorldTime >= 0.0)
	{
		const float PolicyAlpha = CalculateCurrentPolicyInfluenceAlpha(EffectiveSettings);
		PHYSANIM_LOG_RATE_LIMITED(
			LogPhysAnimBridge,
			Log,
			1.0f,
			TEXT("[PhysAnimBalance] STATE FLIP - AFTER FINAL RAMP: time=%.4f policyAlpha=%.4f useSkelAnim=%s"),
			WorldTime,
			PolicyAlpha,
			bUseSkeletalAnimationTargetRepresentation ? TEXT("true") : TEXT("false"));
	}

	const bool bUseAuthoritativePerBoneBodyModifierSync =
		ShouldUseAuthoritativePerBoneBodyModifierSync(RuntimeState, 
			BalanceReadyTransition.IsDistalKinematicAccepted() || 
			(RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Prepare && EffectiveSettings.bPhase1DistalKinematicExperiment));
	if (bUseAuthoritativePerBoneBodyModifierSync)
	{
		static bool bLoggedAuthoritativeWrite = false;
		if (!bLoggedAuthoritativeWrite)
		{
			PHYSANIM_LOG(LogPhysAnimBridge, Log, TEXT("PHASE1_AUTHORITATIVE_PER_BONE_WRITE active=1 broadSetWriteBypassedForCriticalBones=1"));
			bLoggedAuthoritativeWrite = true;
		}

		// Broad sets for non-movement records are still fine
		PhysicsControl->SetBodyModifiersInSetPhysicsBlendWeight(TEXT("All"), 0.0f);
		PhysicsControl->SetBodyModifiersInSetCollisionType(TEXT("All"), ECollisionEnabled::NoCollision);
		PhysicsControl->SetBodyModifiersInSetUpdateKinematicFromSimulation(TEXT("All"), false);

		// But for movement type, we perform explicit per-bone writes to ensure the modifier record updates reliably.
		// We update the live body only in balance-entry states. BridgeActive startup proof
		// ownership must preserve raw simulation that ActivateBridgePhysicsState just enabled.
		const bool bUpdateBodyOnAuthoritativeKinematicWrite =
			ShouldUpdateBodyOnAuthoritativePerBoneKinematicWrite(RuntimeState);
		for (const FName BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
		{
			if (!IsStage1() && BalanceTransitionSets::IsUpperBody(BoneName))
			{
				continue;
			}

			// Skip simulated carry-through bones during entry/settle so the later per-bone
			// sync remains the first live movement-type write they see this tick.
			if ((bPhase1Prepare || bPhase1LateValidate || RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle) &&
				ResolveBringUpGroupIndex(BoneName) == 0 &&
				!BalanceReadyTransition.ShouldKeepBoneKinematic(BoneName, EffectiveSettings))
			{
				continue;
			}

			// Do not drop the root in RootOn or Settle here; the later per-bone resolution
			// owns the authoritative pelvis movement write for both phases.
			if ((RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn ||
				 RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle) &&
				BoneName == PhysAnimBridge::GetRootBoneName())
			{
				continue;
			}

			const FName ModifierName = PhysAnimBridge::MakeBodyModifierName(BoneName);
			TrackDistalModifierWrite(BoneName, EPhysicsMovementType::Kinematic, bUpdateBodyOnAuthoritativeKinematicWrite, TEXT("ApplyRuntimeControlTuning_AuthoritativeDistalKin"));
			PhysicsControl->SetBodyModifierMovementType(ModifierName, EPhysicsMovementType::Kinematic, false, bUpdateBodyOnAuthoritativeKinematicWrite);
		}
	}
	else
	{
		PhysicsControl->SetBodyModifiersInSetMovementType(TEXT("All"), EPhysicsMovementType::Kinematic);
		PhysicsControl->SetBodyModifiersInSetPhysicsBlendWeight(TEXT("All"), 0.0f);
		PhysicsControl->SetBodyModifiersInSetCollisionType(TEXT("All"), ECollisionEnabled::NoCollision);
		PhysicsControl->SetBodyModifiersInSetUpdateKinematicFromSimulation(TEXT("All"), false);
	}

	TrackDistalBoneOwnershipChange(TEXT("calf_r"), EPhysicsMovementType::Kinematic, TEXT("ApplyRuntimeControlTuning_SetAllKinematic"));
	TrackDistalBoneOwnershipChange(TEXT("foot_r"), EPhysicsMovementType::Kinematic, TEXT("ApplyRuntimeControlTuning_SetAllKinematic"));
	TrackDistalBoneOwnershipChange(TEXT("ball_r"), EPhysicsMovementType::Kinematic, TEXT("ApplyRuntimeControlTuning_SetAllKinematic"));

	// Use the pre-calculated value from the top of the function
	const bool bAllowRootBodyModifierSimulationInBalanceMode = bAllowRootSim;
	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	USkeletalMeshComponent* const MeshComponentPtr = GetMeshComponent();
	for (const FName BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
	{
		const FName ModifierName = PhysAnimBridge::MakeBodyModifierName(BoneName);
		if (!PhysicsControl->GetBodyModifierExists(ModifierName))
		{
			continue;
		}

		const FName RootBoneNameInternal = PhysAnimBridge::GetRootBoneName();
		const bool bIsRootBodyModifier = BoneName == RootBoneNameInternal;
		FTransform PelvisTransformPre = FTransform::Identity;
		if (bIsRootBodyModifier)
		{
			USkeletalMeshComponent* const Mesh = GetMeshComponent();
			PelvisTransformPre = Mesh ? Mesh->GetBoneTransform(Mesh->GetBoneIndex(RootBoneNameInternal)) : FTransform::Identity;
		}

		const int32 BringUpGroupIndex = ResolveBringUpGroupIndex(BoneName);
		const bool bTransitionKeepsBoneKinematic =
			(RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Prepare ||
				RuntimeState == EPhysAnimRuntimeState::BalanceEntry_LateValidate ||
				RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn ||
				RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle ||
				RuntimeState == EPhysAnimRuntimeState::BalanceSafeDeny) &&
			BalanceReadyTransition.ShouldKeepBoneKinematic(BoneName, EffectiveSettings) &&
			(IsStage1() || !BalanceTransitionSets::IsUpperBody(BoneName));
		const bool bTransitionOwnsRootOnThisTick =
			bIsRootBodyModifier &&
			bPhase2RootOnGuardWindow &&
			!bTransitionKeepsBoneKinematic &&
			RuntimeState != EPhysAnimRuntimeState::BalanceSafeDeny;
		const bool bPhase2RootAuthorityQuarantined =
			bIsRootBodyModifier &&
			bPhase2RootOnGuardWindow &&
			BalanceReadyTransition.IsPhase2RootAuthorityQuarantined();
		const bool bIsCertifiedRootOnPreservedBone =
			BoneName == TEXT("thigh_l") || BoneName == TEXT("thigh_r") ||
			BoneName == TEXT("spine_01") || BoneName == TEXT("spine_02") || BoneName == TEXT("spine_03");
		const int32 RawSimDiagnosticGroup = CVarPhysAnimRawSimDiagnosticGroup.GetValueOnGameThread();
		const bool bDiagnosticRawSimBody =
			RuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing &&
			!EffectiveSettings.bForceZeroActions &&
			ShouldForceDiagnosticRawSimBody(BoneName, RawSimDiagnosticGroup);

		// During entry transition the component path keeps the root body modifier kinematic
		// until the transition explicitly enters Phase 2 root-on. Once Phase 2 owns the guard
		// window, later per-tick bring-up resolution must not demote the pelvis back off.
		const bool bAllowRootBodyModifierSimulation =
			bIsRootBodyModifier &&
			(bAllowRootBodyModifierSimulationInBalanceMode || bTransitionOwnsRootOnThisTick) &&
			RuntimeState != EPhysAnimRuntimeState::BalanceSafeDeny;
		
		if (bIsRootBodyModifier && bAllowRootBodyModifierSimulation)
		{
			// Resetting bLastAppliedPresentationRootSimulationEnabled happens at the END of the loop
		}

		bool bBringUpGroupUnlocked =
			bIsRootBodyModifier ? bAllowRootBodyModifierSimulation : IsBringUpGroupUnlocked(BringUpGroupIndex);

		if (bTransitionKeepsBoneKinematic)
		{
			bBringUpGroupUnlocked = false;
		}
		const bool bBodyModifierActivatedThisTick =
			(!bIsRootBodyModifier && bSimulationHandoffCompletedThisTick) ||
			(bIsRootBodyModifier && bAllowRootBodyModifierSimulation && !bLastAppliedPresentationRootSimulationEnabled);
		EPhysicsMovementType BodyModifierMovementType = EPhysicsMovementType::Kinematic;
		float BodyModifierPhysicsBlendWeight = 0.0f;
		bool bUpdateKinematicFromSimulation = false;
		ECollisionEnabled::Type BodyModifierCollisionType =
			ResolveBodyModifierCollisionType(
				RuntimeState,
				EffectiveSettings.bForceZeroActions,
				bSimulationHandoffSettled,
				bBringUpGroupUnlocked,
				bIsRootBodyModifier,
				bAllowRootBodyModifierSimulation);
		ResolveBodyModifierRuntimeMode(
			RuntimeState,
			EffectiveSettings.bForceZeroActions,
			bSimulationHandoffSettled,
			bBringUpGroupUnlocked,
			bIsRootBodyModifier,
			bAllowRootBodyModifierSimulation,
			BodyModifierMovementType,
			BodyModifierPhysicsBlendWeight,
			bUpdateKinematicFromSimulation);

		if (bDiagnosticRawSimBody)
		{
			BodyModifierMovementType = EPhysicsMovementType::Simulated;
			BodyModifierPhysicsBlendWeight = 1.0f;
			BodyModifierCollisionType = ECollisionEnabled::QueryAndPhysics;
			bUpdateKinematicFromSimulation = false;
			bBringUpGroupUnlocked = true;
		}
		if (bPreserveBridgeActiveStartupProofRawSimulation &&
			!bTransitionKeepsBoneKinematic &&
			!EffectiveSettings.bForceZeroActions)
		{
			BodyModifierMovementType = EPhysicsMovementType::Simulated;
			BodyModifierPhysicsBlendWeight = 1.0f;
			BodyModifierCollisionType = ECollisionEnabled::QueryAndPhysics;
			bUpdateKinematicFromSimulation = false;
			bBringUpGroupUnlocked = true;
		}

		if (bPhase1Prepare || bPhase1LateValidate)
		{
			// Enforce Phase 1 topology (root=kin, proximal=sim, distal=sim/kin based on experiment, upper=kin)
			if (bIsRootBodyModifier || bTransitionKeepsBoneKinematic)
			{
				BodyModifierMovementType = EPhysicsMovementType::Kinematic;
				BodyModifierCollisionType = ECollisionEnabled::NoCollision;
				BodyModifierPhysicsBlendWeight = 0.0f;
				bUpdateKinematicFromSimulation = bIsRootBodyModifier;
			}
			else if (BringUpGroupIndex == 0 || BringUpGroupIndex == 1)
			{
				// Group 0 = Proximal (thighs + spine), Group 1 = Distal (calves + feet + balls)
				BodyModifierMovementType = EPhysicsMovementType::Simulated;
				BodyModifierCollisionType = ECollisionEnabled::QueryAndPhysics;
				BodyModifierPhysicsBlendWeight = 1.0f;
			}
			else
			{
				// Upper body (Groups 2, 3, 4)
				if (IsStage1())
				{
					BodyModifierMovementType = EPhysicsMovementType::Kinematic;
					BodyModifierCollisionType = ECollisionEnabled::NoCollision;
					BodyModifierPhysicsBlendWeight = 0.0f;
				}
				else
				{
					// If Stage 1 is stripped, upper body simulates
					BodyModifierMovementType = EPhysicsMovementType::Simulated;
					BodyModifierCollisionType = ECollisionEnabled::QueryAndPhysics;
					BodyModifierPhysicsBlendWeight = 1.0f;
				}
			}
		}
		if (bPhase2RootAuthorityQuarantined && !bTransitionOwnsRootOnThisTick && !bLastAppliedPresentationRootSimulationEnabled)
		{
			BodyModifierMovementType = EPhysicsMovementType::Kinematic;
			BodyModifierPhysicsBlendWeight = 0.0f;
			BodyModifierCollisionType = ECollisionEnabled::NoCollision;
			if (!bIsRootBodyModifier)
			{
				bUpdateKinematicFromSimulation = false;
			}
		}

		const bool bRootOnApplicationTick =
			RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn &&
			!bTransitionKeepsBoneKinematic &&
			(bTransitionOwnsRootOnThisTick || (bIsCertifiedRootOnPreservedBone && bAllowRootBodyModifierSimulation));
		if (bRootOnApplicationTick && (bIsRootBodyModifier || bIsCertifiedRootOnPreservedBone))
		{
			BodyModifierMovementType = EPhysicsMovementType::Simulated;
			BodyModifierPhysicsBlendWeight = 1.0f;
			BodyModifierCollisionType = ECollisionEnabled::QueryAndPhysics;
			bUpdateKinematicFromSimulation = false;
		}

		if (bIsRootBodyModifier)
		{
			const float RootSoftSimAlpha = BalanceReadyTransition.GetRootBodyModifierSoftSimAlpha();

			// NARROW GUARD: During the Phase 2 guard window, if the transition owns the root-on commitment,
			// or we are still in the quarantine window, we skip the soft-sim/collision suppression 
			// to ensure stable simulation bring-up.
			if (!bTransitionOwnsRootOnThisTick && !BalanceReadyTransition.IsPhase2RootAuthorityQuarantined())
			{
				BodyModifierPhysicsBlendWeight *= RootSoftSimAlpha;
				if (bPhase2RootOnGuardWindow && RootSoftSimAlpha < 1.0f)
				{
					BodyModifierCollisionType = ECollisionEnabled::NoCollision;
				}
			}
		}

		if (bIsRootBodyModifier && RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn)
		{
			if (BodyModifierMovementType != EPhysicsMovementType::Simulated)
			{
				PHYSANIM_LOG(LogPhysAnimBridge, Error, TEXT("[PhysAnimBalance] PELVIS_BODYMOD_SIM_ACTIVATION_FAIL bone=%s allowRootSim=%d handoffSettled=%d bringUpUnlocked=%d keepsKinematic=%d quarantined=%d"),
					*BoneName.ToString(),
					bAllowRootBodyModifierSimulation ? 1 : 0,
					bSimulationHandoffSettled ? 1 : 0,
					bBringUpGroupUnlocked ? 1 : 0,
					bTransitionKeepsBoneKinematic ? 1 : 0,
					BalanceReadyTransition.IsPhase2RootAuthorityQuarantined() ? 1 : 0);
			}
		}

		// Phase 2 Root Promotion Audit (One-Shot per Frame)
		const bool bIsRootTraceTargetState = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn || RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle;
		if (bIsRootBodyModifier && bIsRootTraceTargetState)
		{
			const int32 CurrentFrame = static_cast<int32>(GFrameNumber);
			
			// DROP CULPRIT: Trace if the pelvis was simulating but we are about to write it as kinematic
			if (bLastAppliedPresentationRootSimulationEnabled && BodyModifierMovementType == EPhysicsMovementType::Kinematic)
			{
				PHYSANIM_LOG(LogPhysAnimBridge, Error, TEXT("[PhysAnimBalance] PHASE2_ROOT_DROP_CULPRIT frame=%d bone=%s previousSim=1 requestedSim=%d quarantined=%d keepsKin=%d source=ApplyRuntimeControlTuning"),
					CurrentFrame, *BoneName.ToString(), 
					bAllowRootBodyModifierSimulation ? 1 : 0, 
					bPhase2RootAuthorityQuarantined ? 1 : 0,
					bTransitionKeepsBoneKinematic ? 1 : 0);
			}

			const int32 RawReadbackValue = -1;

			// Calculate TotalSimCount for the probe
			int32 TotalSimCount = 0;
			if (USkeletalMeshComponent* const Mesh = GetMeshComponent())
			{
				for (const FName& SimBone : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
				{
					if (const FBodyInstance* const BI = Mesh->GetBodyInstance(SimBone))
					{
						if (BI->IsInstanceSimulatingPhysics())
						{
							TotalSimCount++;
						}
					}
				}
			}

			static EPhysAnimRuntimeState LastAuditState = EPhysAnimRuntimeState::Uninitialized;
			const bool bRuntimeStateChanged = (RuntimeState != LastAuditState);

			static TMap<UPhysAnimComponent*, EPhysicsMovementType> LastRootMovementTypes;
			static TMap<UPhysAnimComponent*, bool> LastRootQuarantinedStates;
			static TMap<UPhysAnimComponent*, int32> LastRootRawReadbacks;

			const EPhysicsMovementType LastRootMovementType = LastRootMovementTypes.Contains(this) ? LastRootMovementTypes[this] : EPhysicsMovementType::Static;
			const bool LastRootQuarantined = LastRootQuarantinedStates.Contains(this) ? LastRootQuarantinedStates[this] : false;
			const int32 LastRootRawReadback = LastRootRawReadbacks.Contains(this) ? LastRootRawReadbacks[this] : -2;

			const bool bStateChanged = (BodyModifierMovementType != LastRootMovementType) ||
				(bPhase2RootAuthorityQuarantined != LastRootQuarantined) ||
				(RawReadbackValue != LastRootRawReadback);

			const bool bIsRootOn = (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn);
			const bool bShouldEmitRootOnAudit = bIsRootOn && (BalanceEntryRootOnFrameCount <= 4 || bStateChanged);

			if (bShouldEmitRootOnAudit)
			{
				LastRootMovementTypes.FindOrAdd(this) = BodyModifierMovementType;
				LastRootQuarantinedStates.FindOrAdd(this) = bPhase2RootAuthorityQuarantined;
				LastRootRawReadbacks.FindOrAdd(this) = RawReadbackValue;
			}
		}

		if ((bPhase1Prepare || bPhase1LateValidate || RuntimeState == EPhysAnimRuntimeState::BridgeActive) &&
			(BoneName == TEXT("calf_r") || BoneName == TEXT("foot_r") || BoneName == TEXT("ball_r") ||
			 BoneName == TEXT("calf_l") || BoneName == TEXT("foot_l") || BoneName == TEXT("ball_l")) &&
			BalanceReadyTransition.IsDistalKinematicAccepted() &&
			BodyModifierMovementType == EPhysicsMovementType::Simulated)
		{
			// Explicit precedence rule: distal experiment wins
			BodyModifierMovementType = EPhysicsMovementType::Kinematic;
			BodyModifierCollisionType = ECollisionEnabled::NoCollision;
			BodyModifierPhysicsBlendWeight = 0.0f;
			bUpdateKinematicFromSimulation = false;
			
			// Telemetry when this safety override catches a conflicting re-promotion attempt
			if (!BalanceReadyTransition.LoggedSuppressedDistalBones.Contains(BoneName))
			{
				if (GVerbosePhase2Forensics != 0)
				{
					PHYSANIM_LOG(LogPhysAnimBridge, Log, TEXT("DISTAL_SYNC_REPROMOTION_SUPPRESSED bone=%s phase=%s reason=PerBone_BodyModSync blockedBy=DistalOwnershipRule"), 
						*BoneName.ToString(), 
						GetRuntimeStateName(RuntimeState));
				}
				BalanceReadyTransition.LoggedSuppressedDistalBones.Add(BoneName);
			}

			if (bPhase1Prepare || bPhase1LateValidate)
			{
				if (GVerbosePhase1Forensics != 0)
				{
					PHYSANIM_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] DISTAL_MODIFIER_SYNC_CORRECTED bone=%s phase=%s previousModifier=Simulated correctedModifier=Kinematic reason=AcceptedPhase1Topology"), 
						*BoneName.ToString(), 
						bPhase1Prepare ? TEXT("BalanceEntry_Prepare") : TEXT("BalanceEntry_LateValidate"));
				}
			}
		}

		const bool bRootBodyModLogStateChanged =
			bAllowRootBodyModifierSimulation != bLastAppliedPresentationRootSimulationEnabled ||
			bTransitionOwnsRootOnThisTick ||
			bTransitionKeepsBoneKinematic ||
			bBringUpGroupUnlocked ||
			bBodyModifierActivatedThisTick;
		if (bIsRootBodyModifier && bRootBodyModLogStateChanged)
		{
			if (GVerbosePhase2Forensics != 0 || RuntimeState != EPhysAnimRuntimeState::BalanceEntry_RootOn)
			{
				PHYSANIM_LOG(
					LogPhysAnimBridge,
					Verbose,
					TEXT("[PhysAnimBalance] PELVIS_BODYMOD tickPhase=%d allowRootSim=%d transitionOwnsRootOn=%d transitionKeepKinematic=%d bringUpUnlocked=%d simHandoffSettled=%d movementType=%d collisionType=%d updateKinematicFromSimulation=%d bodyActivatedThisTick=%d lastAppliedRootSim=%d pendingResets=%d"),
					static_cast<int32>(RuntimeState),
					bAllowRootBodyModifierSimulation ? 1 : 0,
					bTransitionOwnsRootOnThisTick ? 1 : 0,
					bTransitionKeepsBoneKinematic ? 1 : 0,
					bBringUpGroupUnlocked ? 1 : 0,
					bSimulationHandoffSettled ? 1 : 0,
					static_cast<int32>(BodyModifierMovementType),
					static_cast<int32>(BodyModifierCollisionType),
					bUpdateKinematicFromSimulation ? 1 : 0,
					bBodyModifierActivatedThisTick ? 1 : 0,
					bLastAppliedPresentationRootSimulationEnabled ? 1 : 0,
					PendingBodyModifierCachedResetNames.Num());
			}
		}

		const bool bUpdateBodyOnPerBoneSync =
			ShouldUpdateBodyOnPerBoneBodyModifierSync(RuntimeState) ||
			bDiagnosticRawSimBody;
		const FPhysicsBodyModifierRecord* const PreviousModifierRecord =
			FPhysAnimPhysicsControlAccessor::GetModifierRecord(PhysicsControl, ModifierName);
		const EPhysicsMovementType PreviousModifierMovementType = PreviousModifierRecord
			? PreviousModifierRecord->BodyModifier.ModifierData.MovementType
			: EPhysicsMovementType::Static;
		const FTransform BoneWorldBeforeRawSim = MeshComponentPtr
			? MeshComponentPtr->GetBoneTransform(BoneName, RTS_World)
			: FTransform::Identity;
		const FV0RawSimBodySnapshot BodyBeforeRawSim = MakeV0RawSimBodySnapshot(
			MeshComponentPtr ? MeshComponentPtr->GetBodyInstance(BoneName) : nullptr);
		PhysicsControl->SetBodyModifierUpdateKinematicFromSimulation(
			ModifierName,
			bUpdateKinematicFromSimulation,
			false,
			false);
		TrackDistalModifierWrite(BoneName, BodyModifierMovementType, bUpdateBodyOnPerBoneSync, TEXT("ApplyRuntimeControlTuning_PerBone_BodyModSync"));
		PhysicsControl->SetBodyModifierMovementType(ModifierName, BodyModifierMovementType, false, bUpdateBodyOnPerBoneSync);
		

		if (bPhase1Prepare || bPhase1LateValidate)
		{
			if (BoneName == TEXT("spine_01") || BoneName == TEXT("calf_r"))
			{
				if (GVerbosePhase1Forensics != 0)
				{
					const USkeletalMeshComponent* const Mesh = GetMeshComponent();
					const FBodyInstance* const TargetBody = Mesh ? Mesh->GetBodyInstance(BoneName) : nullptr;
					const bool bRawSimulating = TargetBody ? TargetBody->IsInstanceSimulatingPhysics() : false;
					PHYSANIM_LOG(LogPhysAnimBridge, Verbose, TEXT("[PhysAnimBalance] PHASE1_MODIFIER_SYNC bone=%s movement=%s updateBody=1 rawSim=%d"),
						*BoneName.ToString(),
						UPhysAnimComponent::GetPhysicsMovementTypeName(BodyModifierMovementType),
						bRawSimulating ? 1 : 0);
				}
			}
		}

		TrackDistalBoneOwnershipChange(BoneName, BodyModifierMovementType, TEXT("ApplyRuntimeControlTuning_PerBone_BodyModSync"));
		PhysicsControl->SetBodyModifierMovementType(ModifierName, BodyModifierMovementType, false, bUpdateBodyOnPerBoneSync);
		PhysicsControl->SetBodyModifierPhysicsBlendWeight(ModifierName, BodyModifierPhysicsBlendWeight, false, false);
		PhysicsControl->SetBodyModifierCollisionType(ModifierName, BodyModifierCollisionType, false, false);
		if (bIsRootBodyModifier && (bPhase1Prepare || bPhase1LateValidate))
		{
			ForceBodyModifierRecordState(
				PhysicsControl,
				ModifierName,
				BodyModifierMovementType,
				BodyModifierPhysicsBlendWeight,
				BodyModifierCollisionType,
				bUpdateKinematicFromSimulation);
		}
		if (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn &&
			(bIsRootBodyModifier || bIsCertifiedRootOnPreservedBone))
		{
			ForceBodyModifierRecordState(
				PhysicsControl,
				ModifierName,
				BodyModifierMovementType,
				BodyModifierPhysicsBlendWeight,
				BodyModifierCollisionType,
				bUpdateKinematicFromSimulation);
		}
		if (bDiagnosticRawSimBody)
		{
			ForceBodyModifierRecordState(
				PhysicsControl,
				ModifierName,
				BodyModifierMovementType,
				BodyModifierPhysicsBlendWeight,
				BodyModifierCollisionType,
				bUpdateKinematicFromSimulation);
			if (FBodyInstance* const BodyInstance = MeshComponentPtr ? MeshComponentPtr->GetBodyInstance(BoneName) : nullptr)
			{
				BodyInstance->WakeInstance();
			}
		}
		if (bDiagnosticRawSimBody)
		{
			const FV0RawSimBodySnapshot BodyAfterRawSim = MakeV0RawSimBodySnapshot(
				MeshComponentPtr ? MeshComponentPtr->GetBodyInstance(BoneName) : nullptr);
			if (BodyAfterRawSim.bSimulating)
			{
				if (MarkV0RawSimBodyEnableLogged(this, BoneName))
				{
					LogV0RawSimBodyEnable(
						this,
						PhysicsControl,
						MeshComponentPtr,
						BoneName,
						BoneWorldBeforeRawSim,
						BodyBeforeRawSim,
						BodyAfterRawSim,
						PreviousModifierMovementType,
						BodyModifierMovementType,
						BodyModifierCollisionType,
						BodyModifierPhysicsBlendWeight,
						bUpdateKinematicFromSimulation);
				}
				LogV0RawSimGroupCCompleteIfReady(this, BoneName, true);
			}
		}
		if (bIsRootBodyModifier && bRootOnApplicationTick && BodyModifierMovementType == EPhysicsMovementType::Simulated)
		{
			PhysicsControl->SetBodyModifierUpdateKinematicFromSimulation(ModifierName, false, false, false);
			PhysicsControl->SetBodyModifierMovementType(ModifierName, EPhysicsMovementType::Simulated, false, true);
			ForceBodyModifierRecordState(
				PhysicsControl,
				ModifierName,
				EPhysicsMovementType::Simulated,
				1.0f,
				ECollisionEnabled::QueryAndPhysics,
				false);

			if (FBodyInstance* const BodyInstance = MeshComponentPtr ? MeshComponentPtr->GetBodyInstance(BoneName) : nullptr)
			{
				BodyInstance->WakeInstance();
			}
		}

		const float CurrentPolicyAlpha = CalculateCurrentPolicyInfluenceAlpha(EffectiveSettings);

		if (bIsRootBodyModifier)
		{
			bLastAppliedPresentationRootSimulationEnabled = bAllowRootBodyModifierSimulation;
		}

		const bool bShouldResetThisBone = ShouldResetBodyModifierToCachedBoneTransform(
				BoneName,
				RuntimeState,
				EffectiveSettings.bForceZeroActions,
				bBodyModifierActivatedThisTick,
				bBringUpGroupUnlocked,
				bIsRootBodyModifier,
				bAllowRootBodyModifierSimulation,
				CurrentPolicyAlpha,
				BalanceReadyTransition.IsDistalKinematicAccepted());

		if (bShouldResetThisBone &&
			!PendingBodyModifierCachedResetNames.Contains(ModifierName))
		{
			if (IsBalanceActiveState(RuntimeState))
			{
				if (bIsRootBodyModifier)
				{
					PHYSANIM_LOG(
						LogPhysAnimBridge,
						Error,
						TEXT("[PhysAnimBalance] STATE MACHINE VIOLATION: Cached-target reset for pelvis/root '%s' requested in Balance Mode. Failing and stopping mode. reason=pelvisResetRequestedDuringBalance"),
						*BoneName.ToString());
					FinalizeBalanceScenario(false, TEXT("pelvisResetRequestedDuringBalance"));
					StopBalancePerturbationMode();
				}
				else if (CurrentPolicyAlpha > 0.0f)
				{
					const FString ViolationReason = FString::Printf(TEXT("bodyResetViolation:%s"), *BoneName.ToString());
					PHYSANIM_LOG(
						LogPhysAnimBridge,
						Error,
						TEXT("[PhysAnimBalance] STATE MACHINE VIOLATION: Cached-target reset for '%s' requested after policy influence has begun (Alpha=%.2f). Failing and stopping mode."),
						*BoneName.ToString(), CurrentPolicyAlpha);
					FinalizeBalanceScenario(false, ViolationReason);
					StopBalancePerturbationMode();
				}
				else
				{
					if (BalanceTransitionSets::IsUpperBody(BoneName) && 
						(RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Prepare || RuntimeState == EPhysAnimRuntimeState::BalanceEntry_LateValidate) &&
						BalanceReadyTransition.IsUpperBodyKinematicHoldActive())
					{
						if (GVerbosePhase1Forensics != 0)
						{
							PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Log, 1.0f, TEXT("PHASE1_UPPER_BODY_RESET_READD_SUPPRESSED bone=%s source=recovery"), *BoneName.ToString());
						}
					}
					else
					{
						PendingBodyModifierCachedResetNames.Add(ModifierName);
					}
				}
			}
			else
			{
				if (BalanceTransitionSets::IsUpperBody(BoneName) && 
					(RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Prepare || RuntimeState == EPhysAnimRuntimeState::BalanceEntry_LateValidate) &&
					BalanceReadyTransition.IsUpperBodyKinematicHoldActive())
				{
					if (GVerbosePhase1Forensics != 0)
					{
						PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Log, 1.0f, TEXT("PHASE1_UPPER_BODY_RESET_READD_SUPPRESSED bone=%s source=applyTuning"), *BoneName.ToString());
					}
				}
				else
				{
					PendingBodyModifierCachedResetNames.Add(ModifierName);
				}
			}
		}
	}

	if (bTraceHipReleaseFrame)
	{
		LogHipQuarantineTraceFrame(
			this,
			PhysicsControl,
			MeshComponent.Get(),
			TEXT("release_frame_post_tuning_pre_release"),
			CurrentFrameNumber,
			GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0,
			GetActivatedStandingStabilityMetrics().ActivationDurationSec,
			HipQuarantineTicksRemaining,
			HipQuarantineTicksRemaining,
			true,
			HipQuarantineControlIntents);
	}
	else if (bTraceHipReleaseNextTick)
	{
		LogHipQuarantineTraceFrame(
			this,
			PhysicsControl,
			MeshComponent.Get(),
			TEXT("next_tick_post_tuning"),
			HipQuarantinePendingNextTickTrace.ReleaseFrame,
			HipQuarantinePendingNextTickTrace.ReleaseWorldTimeSeconds,
			HipQuarantinePendingNextTickTrace.ReleaseActivationTimeSeconds,
			HipQuarantineTicksRemaining,
			HipQuarantineTicksRemaining,
			false,
			HipQuarantineControlIntents);
		HipQuarantinePendingNextTickTrace.bPending = false;
	}
	if (V0PlantEarlyControlZeroGroup > 0)
	{
		LogV0PlantEarlyControlDiagnostics(
			this,
			PhysicsControl,
			MeshComponent.Get(),
			V0PlantEarlyControlZeroGroup,
			bV0PlantEarlyControlZeroWindowActive,
			V0PlantEarlyControlZeroDurationSeconds,
			HipQuarantineControlIntents);
	}

	if (bHipQuarantineActiveThisFrame)
	{
		const int32 HipQuarantineTicksBeforeDecrement = HipQuarantineTicksRemaining;
		if (HipQuarantineTicksRemaining > 0 && RuntimeState != EPhysAnimRuntimeState::FailStopped && !bKineticGateActiveLastFrame)
		{
			--HipQuarantineTicksRemaining;
			bHipQuarantineReleasedThisFrame = (HipQuarantineTicksRemaining == 0);
		}

		if (bHipQuarantineReleasedThisFrame)
		{
			if (IsBalanceActiveState(RuntimeState) || IsBalanceEntryState(RuntimeState))
			{
				PHYSANIM_LOG_RATE_LIMITED(
					LogPhysAnimBridge,
					Warning,
					1.0f,
					TEXT("[PhysAnimBalance] HIP_QUARANTINE_RELEASED frame=%llu worldT=%.3f activationT=%.3f ticksBefore=%d ticksAfter=%d nextTickTraceArmed=1"),
					CurrentFrameNumber,
					GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0,
					GetActivatedStandingStabilityMetrics().ActivationDurationSec,
					HipQuarantineTicksBeforeDecrement,
					HipQuarantineTicksRemaining);
				HipQuarantinePendingNextTickTrace.bPending = true;
				HipQuarantinePendingNextTickTrace.ReleaseFrame = CurrentFrameNumber;
				HipQuarantinePendingNextTickTrace.ReleaseWorldTimeSeconds =
					GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;
				HipQuarantinePendingNextTickTrace.ReleaseActivationTimeSeconds =
					GetActivatedStandingStabilityMetrics().ActivationDurationSec;
				LogHipQuarantineTraceFrame(
					this,
					PhysicsControl,
					MeshComponent.Get(),
					TEXT("release_frame_post_decrement"),
					CurrentFrameNumber,
					HipQuarantinePendingNextTickTrace.ReleaseWorldTimeSeconds,
					HipQuarantinePendingNextTickTrace.ReleaseActivationTimeSeconds,
					HipQuarantineTicksBeforeDecrement,
					HipQuarantineTicksRemaining,
					true,
					HipQuarantineControlIntents);
			}
		}
	}


	LastAppliedStabilizationSettings = EffectiveSettings;
	bLastAppliedSimulationHandoffSettled = bSimulationHandoffSettled;
	LastAppliedControlAuthorityAlpha = CalculateCurrentControlAuthorityAlpha(EffectiveSettings);
	// bLastAppliedPresentationRootSimulationEnabled is now updated inside the loop for the root bone
}


void UPhysAnimComponent::ReconcilePhase1DistalModifierRecords(const FPhysAnimStabilizationSettings& EffectiveSettings)
{
	UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	USkeletalMeshComponent* const Mesh = MeshComponent.Get();
	if (!PhysicsControl || !Mesh)
	{
		return;
	}

	const FName DistalBones[] = { TEXT("calf_l"), TEXT("calf_r"), TEXT("foot_l"), TEXT("foot_r"), TEXT("ball_l"), TEXT("ball_r") };
	for (const FName BoneName : DistalBones)
	{
		if (BalanceReadyTransition.IsDistalKinematicAccepted())
		{
			const FName ModifierName = PhysAnimBridge::MakeBodyModifierName(BoneName);
			EPhysicsMovementType ModifierMovementType = EPhysicsMovementType::Simulated;
			
			if (const FPhysicsBodyModifierRecord* Record = FPhysAnimPhysicsControlAccessor::GetModifierRecord(PhysicsControl, ModifierName))
			{
				ModifierMovementType = Record->BodyModifier.ModifierData.MovementType;
			}

			const FBodyInstance* BodyInst = Mesh->GetBodyInstance(BoneName);
			const bool bRawSimulating = BodyInst && BodyInst->IsValidBodyInstance() ? BodyInst->IsInstanceSimulatingPhysics() : false;

			if (ModifierMovementType != EPhysicsMovementType::Kinematic)
			{
				if (GVerbosePhase1Forensics != 0)
				{
					PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("PHASE1_DISTAL_RECORD_REPAIRED bone=%s prevModifier=%s rawBody=%s"),
						*BoneName.ToString(),
						GetPhysicsMovementTypeName(ModifierMovementType),
						bRawSimulating ? TEXT("Simulated") : TEXT("Kinematic"));
				}
				
				PhysicsControl->SetBodyModifierMovementType(ModifierName, EPhysicsMovementType::Kinematic);
				TrackDistalModifierWrite(BoneName, EPhysicsMovementType::Kinematic, false, TEXT("ReconcilePhase1DistalModifierRecords"));
			}
			else
			{
				if (GVerbosePhase1Forensics != 0)
				{
					PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("PHASE1_DISTAL_RECORD_ENTRY_STATE bone=%s modifier=%s rawBody=%s"),
						*BoneName.ToString(),
						GetPhysicsMovementTypeName(ModifierMovementType),
						bRawSimulating ? TEXT("Simulated") : TEXT("Kinematic"));
				}
			}
		}
	}
}

UE::NNE::IModelInstanceRunSync* UPhysAnimComponent::GetModelInstanceRunSync() const
{
	if (ModelInstanceGPU.IsValid())
	{
		return ModelInstanceGPU.Get();
	}

	if (ModelInstanceCPU.IsValid())
	{
		return ModelInstanceCPU.Get();
	}

	return nullptr;
}

TConstArrayView<UE::NNE::FTensorDesc> UPhysAnimComponent::GetInputTensorDescs() const
{
	if (const UE::NNE::IModelInstanceRunSync* const ModelInstance = GetModelInstanceRunSync())
	{
		return ModelInstance->GetInputTensorDescs();
	}

	return {};
}

TConstArrayView<UE::NNE::FTensorDesc> UPhysAnimComponent::GetOutputTensorDescs() const
{
	if (const UE::NNE::IModelInstanceRunSync* const ModelInstance = GetModelInstanceRunSync())
	{
		return ModelInstance->GetOutputTensorDescs();
	}

	return {};
}
