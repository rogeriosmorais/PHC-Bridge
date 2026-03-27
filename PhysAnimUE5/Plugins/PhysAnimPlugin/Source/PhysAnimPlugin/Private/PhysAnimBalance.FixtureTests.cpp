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

#endif
