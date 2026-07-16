#pragma once

#include "CoreMinimal.h"

namespace PhysAnimScriptedLocomotion
{
	struct FPhaseRange
	{
		double StartSeconds = 0.0;
		double EndSeconds = 0.0;
	};

	struct FIntentRampPhase : FPhaseRange
	{
		double IntentStart = 0.0;
		double IntentEnd = 0.0;
	};

	struct FConstantIntentPhase : FPhaseRange
	{
		double Intent = 0.0;
	};

	struct FMovingTurnPhase : FConstantIntentPhase
	{
		double TotalYawDegrees = 0.0;
	};

	struct FAcceptanceThresholds
	{
		double StartupTimeoutSeconds = 0.0;
		double MinimumShellPathLengthCm = 0.0;
		double MinimumNetPlanarDisplacementCm = 0.0;
		double MinimumAbsYawDeltaDegrees = 0.0;
		double MaximumAbsYawDeltaDegrees = 0.0;
		double MinimumPelvisHeightRatio = 0.0;
		double MaximumRootTiltDegrees = 0.0;
		double MaximumPenetrationCm = 0.0;
		double MaximumSupportGapMs = 0.0;
		double MinimumPolicyStepCoverage = 0.0;
		double MaximumTargetReadbackErrorDegrees = 0.0;
		double MinimumTargetReadbackMatchRatio = 0.0;
		double SettleWindowStartSeconds = 0.0;
		double MaximumSettlePlanarSpeedCmPerSecond = 0.0;
		FString RequiredFinalRuntimeState;
		double NormalToZeroStabilityCostRatioMax = 0.0;
		bool bRequireAllScriptPhases = false;
		bool bRequireZeroInferenceFailures = false;
		bool bRequireCmcInactive = false;
		bool bRequireNoSimRoot = false;
		bool bRequireNonblankRenderCapture = false;
	};

	struct FStabilityCostContract
	{
		double WindowStartSeconds = 0.0;
		double WindowEndSeconds = 0.0;
		FString Formula;
		FString Integration;
		bool bLowerIsBetter = false;
	};

	struct FStep
	{
		FString Phase = TEXT("StandingHold");
		float IntentMagnitude = 0.0f;
		float YawDeltaDegrees = 0.0f;
		bool bMove = false;
		bool bStop = false;
	};

	struct FProtocol
	{
		FString SourcePath;
		FString Sha256;
		FString SchemaVersion;
		FString ProtocolId;
		int32 Version = 0;
		FString Status;
		FString Map;
		FString ActorClass;
		FString Skeleton;
		FString ModelAsset;
		FString RootAuthority;
		FString MotionSource;
		bool bHumanInput = true;
		FString TestFamily;
		FString RendererMode;
		TArray<FString> Variants;
		TMap<FString, int32> Repetitions;

		double CaptureWindowSeconds = 0.0;
		double FixedDeltaTimeSeconds = 0.0;
		double NominalSpeedCmPerSecond = 0.0;
		FPhaseRange StandingHold;
		FIntentRampPhase Acceleration;
		FConstantIntentPhase Cruise;
		FMovingTurnPhase MovingTurn;
		FIntentRampPhase Deceleration;
		FPhaseRange Settle;

		int32 PhysicsMinimumSamples = 0;
		int32 PolicyMinimumSamples = 0;
		FAcceptanceThresholds Acceptance;
		FStabilityCostContract StabilityCost;

		bool Validate(FString& OutError) const;
		FStep ResolveStep(double TimeSeconds) const;
	};

	bool LoadProtocolFromFile(const FString& ProtocolPath, FProtocol& OutProtocol, FString& OutError);
	bool LoadProtocolFromJsonText(
		const FString& JsonText,
		const FString& SourcePath,
		FProtocol& OutProtocol,
		FString& OutError);
}
