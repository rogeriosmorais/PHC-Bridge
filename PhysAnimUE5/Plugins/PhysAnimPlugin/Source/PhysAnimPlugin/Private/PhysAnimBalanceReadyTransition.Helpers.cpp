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

		FVector Sum = FVector::ZeroVector;
		int32 Count = 0;
		for (const FName BoneName : SimulatingBones)
		{
			if (!IsProximal(BoneName) && !IsUpperBody(BoneName))
			{
				continue;
			}

			Sum += ResolveBodyOrBoneLocationCm(Mesh, BoneName);
			++Count;
		}

		if (Count > 0)
		{
			OutLiveChainCenterCm = Sum / static_cast<float>(Count);
		}
		else
		{
			OutLiveChainCenterCm = PelvisLocation;
		}

		return FVector::Dist(PelvisLocation, OutLiveChainCenterCm);
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
