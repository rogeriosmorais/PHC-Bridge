#pragma once

#include "PhysAnimBalanceReadyTransition.h"
#include "PhysAnimComponentPrivate.h"

namespace BalanceTransitionSets
{
	inline constexpr float Phase2TopologySettleGraceSeconds = 1.0f / 30.0f;
	inline constexpr float Phase2AuthorityRampSeconds = 0.25f;
	inline constexpr float Phase2MaxPelvisProximalConstraintErrorCm = 15.0f;
	inline constexpr float Phase2MaxDirectPelvisLinkErrorCm = 8.0f;

	struct FDirectPelvisLinkForensicRecord
	{
		FName ParentBoneName = NAME_None;
		FName ChildBoneName = NAME_None;
		FString LinkName;
		FString PhysicsAssetPath;
		FString ParentSpaceLabel;
		FString ChildSpaceLabel;
		FVector AuthoredParentAnchorLocalCm = FVector::ZeroVector;
		FVector AuthoredChildAnchorLocalCm = FVector::ZeroVector;
		FVector EvaluatedParentAnchorWorldCm = FVector::ZeroVector;
		FVector EvaluatedChildAnchorWorldCm = FVector::ZeroVector;
		float AnchorDistanceCm = 0.0f;
		float BodyOriginDistanceCm = 0.0f;
		bool bConstraintFound = false;
		bool bParentUsedBodyInstance = false;
		bool bChildUsedBodyInstance = false;
	};

	float ComputePelvisProximalConstraintErrorCm(
		const USkeletalMeshComponent* Mesh,
		const TArray<FName>& SimulatingBones,
		FVector& OutLiveChainCenterCm);
	FVector ResolveBodyOrBoneLocationCm(const USkeletalMeshComponent* Mesh, FName BoneName);
	bool BuildDirectPelvisLinkForensicRecord(
		const USkeletalMeshComponent* Mesh,
		FName ParentBoneName,
		FName ChildBoneName,
		FDirectPelvisLinkForensicRecord& OutRecord);
	void LogDirectPelvisLinkForensicRecords(
		const TArray<FDirectPelvisLinkForensicRecord>& Records,
		const TCHAR* ContextTag,
		bool bEmitMissingConstraintErrors);

	FTransform BuildWarmStartPelvisTransform(
		const USkeletalMeshComponent* Mesh,
		const TArray<FName>& SimulatingBones);

	bool IsExpectedPhase2Topology(int32 SimCountPre, int32 SimCountPost, int32 DistalSimCountPre, int32 DistalSimCountPost);
	bool IsUpperOnlySafeDenyHandoff(int32 ProximalSimCount, int32 DistalSimCount, int32 UpperSimCount, bool bRootSimulating);
	bool IsRootCoupledReadyHandoff(int32 ProximalSimCount, int32 DistalSimCount, int32 UpperSimCount, bool bRootSimulating);
	const TCHAR* GetShellAuthorityModeName(EBalanceTransitionShellAuthorityMode Mode);
	FString BuildCertifiedHandoffTopologyClass(bool bRootSimulating, int32 ProximalSimCount, int32 DistalSimCount, int32 UpperSimCount);
}
