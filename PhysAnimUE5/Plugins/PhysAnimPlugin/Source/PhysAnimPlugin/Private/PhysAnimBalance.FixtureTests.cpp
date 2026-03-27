#if WITH_DEV_AUTOMATION_TESTS

#include "PhysAnimBalanceReadyTransition.h"
#include "PhysAnimComponent.h"
#include "PhysAnimBalanceQuietHandoff.h"
#include "PhysAnimBalance.TestHelpers.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/UObjectIterator.h"
#endif

using namespace PhysAnimBalanceTestHelpers;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimBalanceFixtureTests,
	"PhysAnim.Balance.FixtureTests",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimBalanceFixtureTests::RunTest(const FString& Parameters)
{
	// PHASE: SKELETON (Unimplemented)
	// NOTE: This test is a placeholder for future complex fixtures.
	// It must fail until the implementation phase starts to avoid silent skips.
	
	const bool bIsImplemented = false; // Set to true when implementation begins
	if (!bIsImplemented)
	{
		AddError(TEXT("[PhysAnim.Balance.FixtureTests] Static fixture skeleton is currently unimplemented. Fail-by-design to prevent silent skips."));
		return false;
	}

	TestTrue(TEXT("Bridge Fixture Layer Initialized"), true);
	return true;
}

// --- Migrated Contract Tests (Component-Level Logic) ---

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimBalanceModeContractTest,
	"PhysAnim.Component.BalanceModeContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimBalanceModeContractTest::RunTest(const FString& Parameters)
{
	// 1. In BalancePerturbationMode, pelvis/root cached-target reset is always forbidden
	TestFalse(
		TEXT("Balance Mode forbids pelvis reset at zero alpha"),
		UPhysAnimComponent::ShouldResetBodyModifierToCachedBoneTransform(TEXT("pelvis"), EPhysAnimRuntimeState::BalanceActive_Recovery, false, true, true, true, true, 0.0f, false));
	TestFalse(
		TEXT("Balance Mode forbids pelvis reset at high alpha"),
		UPhysAnimComponent::ShouldResetBodyModifierToCachedBoneTransform(TEXT("pelvis"), EPhysAnimRuntimeState::BalanceActive_Recovery, false, true, true, true, true, 1.0f, false));

	// 2. In BridgeActive presentation perturbation, root reset behavior is preserved
	TestTrue(
		TEXT("BridgeActive allows root reset for presentation perturbation"),
		UPhysAnimComponent::ShouldResetBodyModifierToCachedBoneTransform(TEXT("pelvis"), EPhysAnimRuntimeState::BridgeActive, false, true, true, true, true, 0.0f, false));

	// 3. Balance Mode allows limb resets ONLY before policy begins
	TestTrue(
		TEXT("Balance Mode allows limb reset before policy alpha > 0"),
		UPhysAnimComponent::ShouldResetBodyModifierToCachedBoneTransform(TEXT("thigh_l"), EPhysAnimRuntimeState::BalanceActive_Recovery, false, true, true, false, false, 0.0f, false));
	TestFalse(
		TEXT("Balance Mode forbids limb reset once policy alpha > 0"),
		UPhysAnimComponent::ShouldResetBodyModifierToCachedBoneTransform(TEXT("thigh_l"), EPhysAnimRuntimeState::BalanceActive_Recovery, false, true, true, false, false, 0.01f, false));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimBalancePhase2RetryContractTest,
	"PhysAnim.Component.BalancePhase2RetryContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimBalancePhase2RetryContractTest::RunTest(const FString& Parameters)
{
	TestFalse(
		TEXT("Phase 2 root-on spike is not retryable"),
		FPhysAnimBalanceReadyTransition::IsFailureClassRetryable(TEXT("phase2_root_on_spike")));
	TestFalse(
		TEXT("Phase 2 root-on spike cannot be automatically retried without new quiet proof"),
		FPhysAnimBalanceReadyTransition::IsAutomaticRetryAllowed(
			TEXT("phase2_root_on_spike"),
			true,
			true,
			true,
			true,
			true));
	TestFalse(
		TEXT("Phase 2 policy leak is not retryable"),
		FPhysAnimBalanceReadyTransition::IsFailureClassRetryable(TEXT("phase2_policy_write_leak")));
	TestTrue(
		TEXT("Phase 2 topology preservation failure is retryable"),
		FPhysAnimBalanceReadyTransition::IsFailureClassRetryable(TEXT("phase2_topology_not_preserved")));
	TestFalse(
		TEXT("Retry is denied without a material recovery change and fresh quiet proof"),
		FPhysAnimBalanceReadyTransition::IsAutomaticRetryAllowed(
			TEXT("phase2_topology_not_preserved"),
			true,
			false,
			false,
			true,
			true));
	TestTrue(
		TEXT("Retry is allowed only when all retry prerequisites are satisfied"),
		FPhysAnimBalanceReadyTransition::IsAutomaticRetryAllowed(
			TEXT("phase2_topology_not_preserved"),
			true,
			true,
			true,
			true,
			true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimBalanceModeSmokeOutcomeContractTest,
	"PhysAnim.Component.BalanceModeSmokeOutcomeContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimBalanceModeSmokeOutcomeContractTest::RunTest(const FString& Parameters)
{
#if WITH_EDITOR
	FString Error;
	TestTrue(
		TEXT("Balance smoke passes only when balance mode is active"),
		EvaluateBalanceModeSmokeOutcome(
			EPhysAnimRuntimeState::BalanceActive_Recovery,
			false,
			EPhysAnimRuntimeState::Uninitialized,
			false,
			false,
			FString(),
			FString(),
			Error));
	TestTrue(TEXT("Active balance mode does not emit an error"), Error.IsEmpty());

	TestFalse(
		TEXT("Balance smoke fails on safe deny"),
		EvaluateBalanceModeSmokeOutcome(
			EPhysAnimRuntimeState::BalanceSafeDeny,
			false,
			EPhysAnimRuntimeState::BalanceSafeDeny,
			false,
			true,
			TEXT("phase2_root_on_spike"),
			FString(),
			Error));
	TestTrue(
		TEXT("Safe deny failure reports the denial reason"),
		Error.Contains(TEXT("Balance mode denied entry. reason=phase2_root_on_spike.")));

	TestFalse(
		TEXT("Balance smoke fails on unsafe failure paths"),
		EvaluateBalanceModeSmokeOutcome(
			EPhysAnimRuntimeState::FailStopped,
			false,
			EPhysAnimRuntimeState::FailStopped,
			true,
			false,
			FString(),
			TEXT("phase2_root_on_spike"),
			Error));
	TestTrue(
		TEXT("Unsafe failure reports failure details"),
		Error.Contains(TEXT("Unsafe failure path observed.")) && Error.Contains(TEXT("phase2_root_on_spike")));

	return true;
#else
	return true;
#endif
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimBalanceRecoveryReadinessContractTest,
	"PhysAnim.Component.BalanceRecoveryReadinessContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimBalanceRecoveryReadinessContractTest::RunTest(const FString& Parameters)
{
	FString Error;
	TestFalse(
		TEXT("Recovery readiness stays strict when pelvis raw simulation is off"),
		UPhysAnimComponent::EvaluateBalancePerturbationRuntimeReadiness(
			EPhysAnimRuntimeState::BalanceActive_Recovery,
			1,
			2,
			true,
			1.0f,
			0.95f,
			false,
			true,
			false,
			&Error));
	TestEqual(TEXT("Recovery readiness reports pelvisBodyNotSimulating"), Error, FString(TEXT("pelvisBodyNotSimulating")));

	TestTrue(
		TEXT("Recovery readiness passes when pelvis raw simulation is on and no resets are pending"),
		UPhysAnimComponent::EvaluateBalancePerturbationRuntimeReadiness(
			EPhysAnimRuntimeState::BalanceActive_Recovery,
			1,
			2,
			true,
			1.0f,
			0.95f,
			false,
			true,
			true,
			&Error));
	TestTrue(TEXT("Successful recovery readiness does not emit an error"), Error.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimBalanceQuietHandoffSuppressionTest,
	"PhysAnim.Component.BalanceQuietHandoffSuppression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimBalanceQuietHandoffSuppressionTest::RunTest(const FString& Parameters)
{
	FPhysAnimBalanceQuietHandoff QuietHandoff;
	QuietHandoff.Start(TEXT("test_start"), nullptr);
	TestTrue(TEXT("Quiet handoff suppresses policy while active"), QuietHandoff.ShouldSuppressPolicy());
	TestTrue(TEXT("Quiet handoff suppresses shell while active"), QuietHandoff.ShouldSuppressShell());
	TestTrue(TEXT("Quiet handoff suppresses move smoke while active"), QuietHandoff.ShouldSuppressMoveSmoke());
	QuietHandoff.Cancel();
	TestFalse(TEXT("Quiet handoff no longer suppresses policy after cancel"), QuietHandoff.ShouldSuppressPolicy());
	TestFalse(TEXT("Quiet handoff no longer suppresses shell after cancel"), QuietHandoff.ShouldSuppressShell());
	TestFalse(TEXT("Quiet handoff no longer suppresses move smoke after cancel"), QuietHandoff.ShouldSuppressMoveSmoke());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimSmplOrderContractTest,
	"PhysAnim.Component.SmplOrderContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimSmplOrderContractTest::RunTest(const FString& Parameters)
{
	const TArray<FName>& BoneNames = PhysAnimBridge::GetSmplObservationBoneNames();
	
	// Verified SMPL order (DFS traversal) from ProtoMotions smpl.yaml
	TestEqual(TEXT("SMPL observation count"), BoneNames.Num(), 24);
	if (BoneNames.Num() >= 24)
	{
		TestEqual(TEXT("SMPL 0: Pelvis"), BoneNames[0], TEXT("pelvis"));
		TestEqual(TEXT("SMPL 1: L_Hip"), BoneNames[1], TEXT("thigh_l"));
		TestEqual(TEXT("SMPL 2: L_Knee"), BoneNames[2], TEXT("calf_l"));
		TestEqual(TEXT("SMPL 3: L_Ankle"), BoneNames[3], TEXT("foot_l"));
		TestEqual(TEXT("SMPL 4: L_Toe"), BoneNames[4], TEXT("ball_l"));
		TestEqual(TEXT("SMPL 9: Torso"), BoneNames[9], TEXT("spine_01"));
		TestEqual(TEXT("SMPL 13: Head"), BoneNames[13], TEXT("head"));
		TestEqual(TEXT("SMPL 17: L_Wrist"), BoneNames[17], TEXT("hand_l"));
		TestEqual(TEXT("SMPL 18: L_Hand (collapsed)"), BoneNames[18], TEXT("hand_l"));
	}
	
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimActionToBoneMappingContractTest,
	"PhysAnim.Component.ActionToBoneMappingContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimActionToBoneMappingContractTest::RunTest(const FString& Parameters)
{
	using namespace PhysAnimBridge;

	TArray<float> Actions;
	Actions.Init(0.0f, NumActionFloats);
	
	// Test ExpMap (1.0, 0, 0) for joint 0 (thigh_l)
	// ExpMap (PI, 0, 0) because we multiply by PI in ConvertModelActionsToControlRotations
	Actions[0] = 1.0f; 
	
	TMap<FName, FQuat> Rotations;
	FString Error;
	TestTrue(TEXT("Convert actions"), ConvertModelActionsToControlRotations(Actions, Rotations, Error));
	
	const FQuat* ThighL = Rotations.Find(TEXT("thigh_l"));
	TestNotNull(TEXT("thigh_l in output"), ThighL);
	if (ThighL)
	{
		// Expected: rotation of PI around X axis in SMPL space
		const FQuat ExpectedSmpl = FQuat(FVector::ForwardVector, PI);
		const FQuat ExpectedUe = SmplQuaternionToUe(ExpectedSmpl);
		TestTrue(TEXT("thigh_l rotation matches ExpMap"), ThighL->Equals(ExpectedUe, 0.001f));
	}
	
	// Test distal hand collapse
	// hand_l indices: 16 (wrist) and 17 (hand)
	Actions.Init(0.0f, NumActionFloats);
	Actions[16 * 3 + 0] = 0.5f; // Wrist X-rot
	Actions[17 * 3 + 0] = 0.1f; // Hand X-rot
	TestTrue(TEXT("Convert distal actions"), ConvertModelActionsToControlRotations(Actions, Rotations, Error));
	
	const FQuat* HandL = Rotations.Find(TEXT("hand_l"));
	TestNotNull(TEXT("hand_l in output"), HandL);
	if (HandL)
	{
		const FQuat WristSmpl = FQuat(FVector::ForwardVector, 0.5f * PI);
		const FQuat HandSmpl = FQuat(FVector::ForwardVector, 0.1f * PI);
		const FQuat ExpectedUe = SmplQuaternionToUe(WristSmpl * HandSmpl);
		TestTrue(TEXT("hand_l combined rotation matches distal collapse logic"), HandL->Equals(ExpectedUe, 0.001f));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimSelfObservationContractTest,
	"PhysAnim.Component.SelfObservationContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimSelfObservationContractTest::RunTest(const FString& Parameters)
{
	using namespace PhysAnimBridge;
	
	TArray<FPhysAnimBodySample> Samples;
	Samples.SetNum(NumSmplBodies);
	for (int32 i = 0; i < NumSmplBodies; ++i)
	{
		Samples[i].Position = FVector(0, 0, 100);
		Samples[i].Rotation = FQuat::Identity;
	}
	
	TArray<float> Obs;
	FString Error;
	TestTrue(TEXT("Build self obs"), BuildSelfObservation(Samples, 0.0f, Obs, Error));
	TestEqual(TEXT("Obs size"), Obs.Num(), SelfObsSize);
	
	// Index 0: Root height
	TestEqual(TEXT("Root height (100cm)"), Obs[0], 100.0f);
	
	// Next 23*3: Local body positions (all 0 because they are same as root)
	for (int32 i = 1; i <= 23 * 3; ++i)
	{
		TestEqual(TEXT("Local position element"), Obs[i], 0.0f);
	}
	
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimMimicTargetPosesContractTest,
	"PhysAnim.Component.MimicTargetPosesContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimMimicTargetPosesContractTest::RunTest(const FString& Parameters)
{
	using namespace PhysAnimBridge;
	
	TArray<FPhysAnimBodySample> CurrentSamples;
	CurrentSamples.SetNum(NumSmplBodies);
	for (int32 i = 0; i < NumSmplBodies; ++i)
	{
		CurrentSamples[i].Position = FVector::ZeroVector;
		CurrentSamples[i].Rotation = FQuat::Identity;
	}

	TArray<FPhysAnimFuturePoseSample> FutureSamples;
	FutureSamples.SetNum(NumFutureSteps);
	for (int32 f = 0; f < NumFutureSteps; ++f)
	{
		FutureSamples[f].BodyTransforms.SetNum(NumSmplBodies);
		for (int32 i = 0; i < NumSmplBodies; ++i)
		{
			FutureSamples[f].BodyTransforms[i] = FTransform::Identity;
		}
		FutureSamples[f].FutureTimeSeconds = (f + 1) * FutureStepSeconds;
	}

	TArray<float> MimicData;
	FString Error;
	TestTrue(TEXT("Build mimic data"), BuildMimicTargetPoses(CurrentSamples, FutureSamples, MimicData, Error));
	TestEqual(TEXT("Mimic data size"), MimicData.Num(), MimicTargetPosesSize);
	
	// Verify first future step root-relative position (expected 0)
	// Each step has: relative_pos (24*3) + root_relative_pos (24*3) + rel_rot (24*6) + global_rot (24*6) + time (1)
	// Total per step: 72 + 72 + 144 + 144 + 1 = 433
	// NumFutureSteps = 15. 15 * 433 = 6495. Correct.
	
	TestEqual(TEXT("First step relative pos X"), MimicData[0], 0.0f);
	TestEqual(TEXT("First step time"), MimicData[432], FutureStepSeconds);
	
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimTerrainObservationContractTest,
	"PhysAnim.Component.TerrainObservationContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimTerrainObservationContractTest::RunTest(const FString& Parameters)
{
	using namespace PhysAnimBridge;
	
	TArray<float> GroundHeights;
	GroundHeights.Init(10.0f, TerrainSize);
	
	TArray<float> TerrainObs;
	FString Error;
	TestTrue(TEXT("Build terrain obs"), BuildTerrainObservation(100.0f, GroundHeights, TerrainObs, Error));
	TestEqual(TEXT("Terrain size"), TerrainObs.Num(), TerrainSize);
	
	// Value should be RootHeight - GroundHeight = 90.0
	for (int32 i = 0; i < TerrainSize; ++i)
	{
		TestEqual(TEXT("Terrain height delta"), TerrainObs[i], 90.0f);
	}
	
	return true;
}

#endif
