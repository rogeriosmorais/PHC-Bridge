#pragma once

#include "CoreMinimal.h"

enum class EPhysAnimSupportSide : uint8
{
	Left,
	Right
};

enum class EPhysAnimSupportMode : uint8
{
	TwoFootStable,
	SingleFootSurvival,
	TransientRecovery,
	Airborne
};

enum class EPhysAnimTerminalReason : uint8
{
	None,
	ActivationPhysicsAssetContractViolation,
	ActivationCapsuleContractViolation,
	ActivationTopologyChange,
	ActivationContinuousSimulationLost,
	ActivationSupportFailure,
	ActivationProxyOutsideSupportRegion,
	ActivationTargetDiscontinuity,
	ActivationUnstableGainOrDamping,
	ActivationInstabilityThresholdBreach,
	ActivationPoseReferenceMismatch,
	ActivationMovementReclaim,
	ActivationShellHelperViolation,
	ActivationAuthorityConflict,
	ActivationStandingValidationTimeout
};
