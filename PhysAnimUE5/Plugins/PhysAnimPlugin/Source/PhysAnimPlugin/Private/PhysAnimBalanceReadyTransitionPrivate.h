#pragma once

#include "PhysAnimBalanceReadyTransition.h"
#include "PhysAnimComponentPrivate.h"

namespace BalanceTransitionSets
{
	inline constexpr float Phase2TopologySettleGraceSeconds = 1.0f / 30.0f;
	inline constexpr float Phase2AuthorityRampSeconds = 0.10f;
	inline constexpr float Phase2MaxPelvisProximalConstraintErrorCm = 15.0f;

	float ComputePelvisProximalConstraintErrorCm(
		const USkeletalMeshComponent* Mesh,
		const TArray<FName>& SimulatingBones,
		FVector& OutLiveChainCenterCm);

	FTransform BuildWarmStartPelvisTransform(
		const USkeletalMeshComponent* Mesh,
		const TArray<FName>& SimulatingBones);

	bool IsExpectedPhase2Topology(int32 SimCountPre, int32 SimCountPost, int32 DistalSimCountPre, int32 DistalSimCountPost);
	bool IsUpperOnlySafeDenyHandoff(int32 ProximalSimCount, int32 DistalSimCount, int32 UpperSimCount, bool bRootSimulating);
	bool IsRootCoupledReadyHandoff(int32 ProximalSimCount, int32 DistalSimCount, int32 UpperSimCount, bool bRootSimulating);
	const TCHAR* GetShellAuthorityModeName(EBalanceTransitionShellAuthorityMode Mode);
	FString BuildCertifiedHandoffTopologyClass(bool bRootSimulating, int32 ProximalSimCount, int32 DistalSimCount, int32 UpperSimCount);
}
