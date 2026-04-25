#include "PhysAnimRuntimeAdapter.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"

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
}
