#include "PhysAnimRuntimeAdapter.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysAnimSupportTruth.h"

namespace
{
	bool IsUsableBodyInstance(const FBodyInstance* BodyInstance)
	{
		return BodyInstance && BodyInstance->IsValidBodyInstance();
	}
}

namespace PhysAnimRuntimeAdapter
{
	FPhysAnimContinuitySnapshot CaptureContinuitySnapshot(const FPhysAnimContinuitySnapshotCaptureInput& Input)
	{
		FPhysAnimContinuitySnapshot Snapshot;

		USkeletalMeshComponent* const Mesh = Input.SkeletalMeshComponent;
		bool bRawBodiesValid = Mesh != nullptr;
		bool bRawBodiesSimulating = Mesh != nullptr;
		int32 TopologyChangeCount = Mesh ? 0 : 1;

		for (const FName& BodyName : Input.CriticalBodyNames)
		{
			FBodyInstance* const BodyInstance = Mesh && !BodyName.IsNone() ? Mesh->GetBodyInstance(BodyName) : nullptr;
			if (!IsUsableBodyInstance(BodyInstance))
			{
				bRawBodiesValid = false;
				bRawBodiesSimulating = false;
				++TopologyChangeCount;
				continue;
			}

			if (!BodyInstance->IsInstanceSimulatingPhysics())
			{
				bRawBodiesSimulating = false;
			}
		}

		if (Mesh && !Input.PelvisBodyName.IsNone())
		{
			FBodyInstance* const PelvisBody = Mesh->GetBodyInstance(Input.PelvisBodyName);
			if (!IsUsableBodyInstance(PelvisBody))
			{
				if (!Input.CriticalBodyNames.Contains(Input.PelvisBodyName))
				{
					++TopologyChangeCount;
				}
				bRawBodiesValid = false;
				bRawBodiesSimulating = false;
			}
			else if (!PelvisBody->IsInstanceSimulatingPhysics())
			{
				bRawBodiesSimulating = false;
				Snapshot.PelvisSleepDurationMs = 0.0;
			}
			else
			{
				Snapshot.PelvisSleepDurationMs = PelvisBody->IsInstanceAwake()
					? 0.0
					: Input.PreviousPelvisSleepDurationMs + FMath::Max(0.0, Input.DeltaMs);
			}
		}

		const bool bRawContinuityValid = bRawBodiesValid && bRawBodiesSimulating;
		Snapshot.TopologyChangeCount = TopologyChangeCount;
		Snapshot.bAllCriticalBodiesValid = bRawBodiesValid;
		Snapshot.bAllCriticalBodiesSimulating = bRawBodiesSimulating;
		Snapshot.bContinuityBookkeepingMismatch = Input.bBookkeepingReportsContinuity != bRawContinuityValid;

		return Snapshot;
	}

	FPhysAnimCapsuleContractSnapshot CaptureCapsuleContractSnapshot(const FPhysAnimCapsuleContractSnapshotCaptureInput& Input)
	{
		FPhysAnimCapsuleContractSnapshot Snapshot;

		if (const UCapsuleComponent* const Capsule = Input.CapsuleComponent)
		{
			Snapshot.CapsuleCollisionEnabled = Capsule->GetCollisionEnabled() == ECollisionEnabled::NoCollision
				? EPhysAnimCapsuleCollisionState::NoCollision
				: EPhysAnimCapsuleCollisionState::CollisionEnabled;
			Snapshot.bCapsuleGenerateOverlapEvents = Capsule->GetGenerateOverlapEvents();
			Snapshot.CapsuleLockDeltaCm = FVector::Dist(Capsule->GetComponentLocation(), Input.RebaseOriginCm);
		}
		else
		{
			Snapshot.CapsuleCollisionEnabled = EPhysAnimCapsuleCollisionState::CollisionEnabled;
		}

		if (const USkeletalMeshComponent* const Mesh = Input.SkeletalMeshComponent)
		{
			Snapshot.bMeshUsesAbsoluteLocation = Mesh->IsUsingAbsoluteLocation();
			Snapshot.bMeshUsesAbsoluteRotation = Mesh->IsUsingAbsoluteRotation();
			Snapshot.bMeshUsesAbsoluteScale = Mesh->IsUsingAbsoluteScale();
		}
		else
		{
			Snapshot.bMeshUsesAbsoluteLocation = false;
			Snapshot.bMeshUsesAbsoluteRotation = false;
			Snapshot.bMeshUsesAbsoluteScale = false;
		}

		if (const UCharacterMovementComponent* const CharacterMovement = Input.CharacterMovementComponent)
		{
			Snapshot.bCmcIsActive = CharacterMovement->IsActive();
			Snapshot.bCmcTickEnabled = CharacterMovement->IsComponentTickEnabled();
			Snapshot.bCmcUpdatedComponentIsNull = CharacterMovement->UpdatedComponent == nullptr;
		}

		return Snapshot;
	}

	FPhysAnimPlantContractSnapshot CapturePlantContractSnapshot(const FPhysAnimPlantContractSnapshotCaptureInput& Input)
	{
		FPhysAnimPlantContractSnapshot Snapshot;
		Snapshot.bSkeletonAuditPassed = Input.bSkeletonAuditPassed;
		Snapshot.PlantFailureClass = Input.PlantFailureClass;
		Snapshot.PlantFailureField = Input.PlantFailureField;
		Snapshot.MassDriftTotalPct = Input.MassDriftTotalPct;

		const UPhysicsAsset* const PhysicsAsset = Input.SkeletalMeshComponent
			? Input.SkeletalMeshComponent->GetPhysicsAsset()
			: nullptr;
		const FString PhysicsAssetPath = PhysicsAsset ? PhysicsAsset->GetPathName() : FString();

		if (!Input.ExpectedPhysicsAssetPath.IsEmpty() && PhysicsAssetPath != Input.ExpectedPhysicsAssetPath)
		{
			Snapshot.bPhysicsAssetContractValid = false;
			Snapshot.PlantFailureClass = EPhysAnimPlantFailureClass::Mutation;
			Snapshot.PlantFailureField = EPhysAnimPlantFailureField::PhysicsAssetIdentity;
		}

		if (!Input.bSkeletonAuditPassed)
		{
			Snapshot.bPhysicsAssetContractValid = false;
			if (Snapshot.PlantFailureClass == EPhysAnimPlantFailureClass::None)
			{
				Snapshot.PlantFailureClass = EPhysAnimPlantFailureClass::StaticStructural;
			}
			if (Snapshot.PlantFailureField == EPhysAnimPlantFailureField::None)
			{
				Snapshot.PlantFailureField = EPhysAnimPlantFailureField::Skeleton;
			}
		}

		if (Input.PlantFailureClass != EPhysAnimPlantFailureClass::None ||
			Input.PlantFailureField != EPhysAnimPlantFailureField::None)
		{
			Snapshot.bPhysicsAssetContractValid = false;
		}

		return Snapshot;
	}

	FPhysAnimSupportContractSnapshot CaptureSupportSnapshot(const FPhysAnimSupportSnapshotCaptureInput& Input)
	{
		FPhysAnimSupportContractSnapshot Snapshot;
		Snapshot.bSupportStateL = Input.bSupportStateL;
		Snapshot.bSupportStateR = Input.bSupportStateR;
		Snapshot.SupportMode = Input.SupportMode;
		Snapshot.SupportGapTimerMs = Input.SupportGapTimerMs;
		Snapshot.SupportGapMaxMs = Input.SupportGapMaxMs;
		Snapshot.ActiveSupportSideCount = Input.ActiveSupportSideCount;
		Snapshot.SupportHullAreaCm2 = Input.SupportHullAreaCm2;
		Snapshot.SupportAreaMinCm2 = Input.SupportAreaMinCm2;
		Snapshot.SupportPatchAreaLCm2 = Input.SupportPatchAreaLCm2;
		Snapshot.SupportPatchAreaRCm2 = Input.SupportPatchAreaRCm2;
		Snapshot.SupportHullPointsCm = Input.SupportHullPointsCm;
		Snapshot.ComProxyPosCm = Input.ComProxyPosCm;
		Snapshot.MaxPenetrationCm = Input.MaxPenetrationCm;
		Snapshot.SupportChurnCount = Input.SupportChurnCount;
		Snapshot.SupportChurnHz = Input.SupportChurnHz;
		Snapshot.ProxyInsideHull = Input.ProxyInsideHull;
		Snapshot.ProxyOutsideHullDurationMs = Input.ProxyOutsideHullDurationMs;
		Snapshot.ProxyTerminalReason = Input.ProxyTerminalReason;
		return Snapshot;
	}

	FPhysAnimSupportContractSnapshot CaptureSupportSnapshotFromContacts(const FPhysAnimSupportContactsSnapshotCaptureInput& Input)
	{
		TMap<TTuple<FName, EPhysAnimSupportSide>, TArray<FPhysAnimSupportPoint2D>> GroupedPoints;

		for (const FPhysAnimSupportContactSample& Sample : Input.Contacts)
		{
			if (!Sample.bIsValidSupportContact)
			{
				continue;
			}

			FPhysAnimSupportPoint2D Point;
			Point.PositionCm = Sample.PositionCm;
			Point.BodyName = Sample.BodyName;
			Point.SupportSide = Sample.SupportSide;

			GroupedPoints.FindOrAdd({Sample.BodyName, Sample.SupportSide}).Add(Point);
		}

		TArray<FPhysAnimSupportPatch> Patches;
		double PatchAreaL = 0.0;
		double PatchAreaR = 0.0;

		for (auto& Elem : GroupedPoints)
		{
			const FPhysAnimSupportPatch Patch = PhysAnimSupportTruth::ExtractPatchHull(Elem.Value);
			if (Patch.bValidInput && !Patch.HullPointsCm.IsEmpty())
			{
				Patches.Add(Patch);
				if (Patch.SupportSide == EPhysAnimSupportSide::Left)
				{
					PatchAreaL += Patch.PatchAreaCm2;
				}
				else
				{
					PatchAreaR += Patch.PatchAreaCm2;
				}
			}
		}

		const FPhysAnimFrameHull FrameHull = PhysAnimSupportTruth::BuildFrameHull(Patches);

		const bool bActiveL = PatchAreaL > UE_SMALL_NUMBER;
		const bool bActiveR = PatchAreaR > UE_SMALL_NUMBER;

		FPhysAnimSupportSnapshotCaptureInput CaptureInput;
		CaptureInput.bSupportStateL = bActiveL;
		CaptureInput.bSupportStateR = bActiveR;
		CaptureInput.SupportGapMaxMs = Input.SupportGapMaxMs;
		CaptureInput.SupportAreaMinCm2 = Input.SupportAreaMinCm2;
		CaptureInput.SupportGapTimerMs = (bActiveL || bActiveR) ? 0.0 : Input.PreviousSupportGapTimerMs + FMath::Max(0.0, Input.DeltaMs);
		CaptureInput.SupportMode = PhysAnimSupportTruth::ClassifySupportMode(bActiveL, bActiveR, CaptureInput.SupportGapTimerMs, CaptureInput.SupportGapMaxMs);

		FPhysAnimProxyAdjudicationInput ProxyInput;
		ProxyInput.ProxyPositionCm = Input.ComProxyPosCm;
		ProxyInput.HullPointsCm = FrameHull.HullPointsCm;
		ProxyInput.ActiveSupportSideCount = FrameHull.ActiveSupportSideCount;
		ProxyInput.PreviousProxyOutsideHullDurationMs = Input.PreviousProxyOutsideHullDurationMs;
		ProxyInput.DeltaMs = Input.DeltaMs;
		ProxyInput.ProxyDriftLimitMs = Input.ProxyDriftLimitMs;

		const FPhysAnimProxyAdjudicationResult ProxyResult = PhysAnimSupportTruth::AdjudicateProxy(ProxyInput);

		CaptureInput.ActiveSupportSideCount = FrameHull.ActiveSupportSideCount;
		CaptureInput.SupportHullAreaCm2 = FrameHull.SupportHullAreaCm2;
		CaptureInput.SupportPatchAreaLCm2 = PatchAreaL;
		CaptureInput.SupportPatchAreaRCm2 = PatchAreaR;
		CaptureInput.SupportHullPointsCm = FrameHull.HullPointsCm;
		CaptureInput.ComProxyPosCm = Input.ComProxyPosCm;
		CaptureInput.SupportChurnCount = Input.SupportChurnCount;
		CaptureInput.SupportChurnHz = Input.SupportChurnHz;
		CaptureInput.ProxyInsideHull = ProxyResult.ProxyInsideHull;
		CaptureInput.ProxyOutsideHullDurationMs = ProxyResult.ProxyOutsideHullDurationMs;
		CaptureInput.ProxyTerminalReason = ProxyResult.TerminalReason;

		return CaptureSupportSnapshot(CaptureInput);
	}
}
