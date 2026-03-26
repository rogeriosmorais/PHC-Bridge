#include "PhysAnimBalanceReadyTransitionPrivate.h"

namespace BalanceTransitionSets
{
	FVector ResolveBodyOrBoneLocationCm(const USkeletalMeshComponent* Mesh, FName BoneName)
	{
		if (!Mesh)
		{
			return FVector::ZeroVector;
		}

		if (const FBodyInstance* const BodyInstance = Mesh->GetBodyInstance(BoneName))
		{
			return BodyInstance->GetUnrealWorldTransform().GetLocation();
		}

		return Mesh->GetBoneTransform(Mesh->GetBoneIndex(BoneName)).GetLocation();
	}

	bool IsExpectedPhase2Topology(int32 SimCountPre, int32 SimCountPost, int32 DistalSimCountPre, int32 DistalSimCountPost)
	{
		return DistalSimCountPre >= 0 &&
			DistalSimCountPost == 0 &&
			(SimCountPost == SimCountPre || SimCountPost == SimCountPre + 1);
	}

	float ComputePelvisProximalConstraintErrorCm(
		const USkeletalMeshComponent* Mesh,
		const TArray<FName>& SimulatingBones,
		FVector& OutLiveChainCenterCm)
	{
		OutLiveChainCenterCm = FVector::ZeroVector;
		if (!Mesh)
		{
			return 0.0f;
		}

		const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
		const FVector PelvisLocation = ResolveBodyOrBoneLocationCm(Mesh, RootBoneName);
		const TSet<FName> SimulatingBoneSet(SimulatingBones);
		struct FConstraintLink
		{
			FName ParentBoneName;
			FName ChildBoneName;
			bool bRequireParentSimulating = false;
		};
		static const FConstraintLink PreservedProximalLinks[] =
		{
			{ RootBoneName, TEXT("thigh_l"), false },
			{ RootBoneName, TEXT("thigh_r"), false },
			{ RootBoneName, TEXT("spine_01"), false },
			{ TEXT("spine_01"), TEXT("spine_02"), true },
			{ TEXT("spine_02"), TEXT("spine_03"), true }
		};

		float MaxConstraintErrorCm = 0.0f;
		bool bHasConstraintEvidence = false;
		FVector WorstLinkParentAnchorWorldCm = PelvisLocation;
		FVector WorstLinkChildAnchorWorldCm = PelvisLocation;

		for (const FConstraintLink& Link : PreservedProximalLinks)
		{
			if (!SimulatingBoneSet.Contains(Link.ChildBoneName))
			{
				continue;
			}

			if (Link.bRequireParentSimulating && !SimulatingBoneSet.Contains(Link.ParentBoneName))
			{
				continue;
			}

			FDirectPelvisLinkForensicRecord LinkRecord;
			BuildDirectPelvisLinkForensicRecord(Mesh, Link.ParentBoneName, Link.ChildBoneName, LinkRecord);
			const float LinkErrorCm = LinkRecord.bConstraintFound ? LinkRecord.AnchorDistanceCm : LinkRecord.BodyOriginDistanceCm;
			if (!bHasConstraintEvidence || LinkErrorCm > MaxConstraintErrorCm)
			{
				bHasConstraintEvidence = true;
				MaxConstraintErrorCm = LinkErrorCm;
				WorstLinkParentAnchorWorldCm = LinkRecord.bConstraintFound
					? LinkRecord.EvaluatedParentAnchorWorldCm
					: ResolveBodyOrBoneLocationCm(Mesh, Link.ParentBoneName);
				WorstLinkChildAnchorWorldCm = LinkRecord.bConstraintFound
					? LinkRecord.EvaluatedChildAnchorWorldCm
					: ResolveBodyOrBoneLocationCm(Mesh, Link.ChildBoneName);
			}
		}

		if (bHasConstraintEvidence)
		{
			OutLiveChainCenterCm = (WorstLinkParentAnchorWorldCm + WorstLinkChildAnchorWorldCm) * 0.5f;
			return MaxConstraintErrorCm;
		}

		OutLiveChainCenterCm = PelvisLocation;
		return 0.0f;
	}

	FTransform BuildWarmStartPelvisTransform(
		const USkeletalMeshComponent* Mesh,
		const TArray<FName>& SimulatingBones)
	{
		if (!Mesh)
		{
			return FTransform::Identity;
		}

		FVector LiveChainCenterCm = FVector::ZeroVector;
		const float ErrorCm = ComputePelvisProximalConstraintErrorCm(Mesh, SimulatingBones, LiveChainCenterCm);
		const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
		FTransform PelvisTransform = Mesh->GetBoneTransform(Mesh->GetBoneIndex(RootBoneName));
		PelvisTransform.SetLocation(ResolveBodyOrBoneLocationCm(Mesh, RootBoneName));
		if (ErrorCm > KINDA_SMALL_NUMBER)
		{
			PelvisTransform.SetLocation(LiveChainCenterCm);
		}
		return PelvisTransform;
	}

	bool IsUpperOnlySafeDenyHandoff(int32 ProximalSimCount, int32 DistalSimCount, int32 UpperSimCount, bool bRootSimulating)
	{
		return !bRootSimulating && ProximalSimCount == 0 && DistalSimCount == 0 && UpperSimCount > 0;
	}

	bool IsRootCoupledReadyHandoff(int32 ProximalSimCount, int32 DistalSimCount, int32 UpperSimCount, bool bRootSimulating)
	{
		return !bRootSimulating && ProximalSimCount == 5 && DistalSimCount >= 0 && UpperSimCount >= 0;
	}

	const TCHAR* GetShellAuthorityModeName(EBalanceTransitionShellAuthorityMode Mode)
	{
		switch (Mode)
		{
		case EBalanceTransitionShellAuthorityMode::TransitionOwnedShellLocked:
			return TEXT("transition_owned_shell_locked");
		case EBalanceTransitionShellAuthorityMode::GameplayShellObservedOnly:
		default:
			return TEXT("gameplay_shell_observed_only");
		}
	}

	FString BuildCertifiedHandoffTopologyClass(bool bRootSimulating, int32 ProximalSimCount, int32 DistalSimCount, int32 UpperSimCount)
	{
		return FString::Printf(
			TEXT("root=%s proximal=%s distal=%s upper=%s"),
			bRootSimulating ? TEXT("sim") : TEXT("kin"),
			ProximalSimCount > 0 ? TEXT("sim") : TEXT("kin"),
			DistalSimCount > 0 ? TEXT("sim") : TEXT("kin"),
			UpperSimCount > 0 ? TEXT("sim") : TEXT("kin"));
	}
}
