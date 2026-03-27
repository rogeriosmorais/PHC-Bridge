#if WITH_DEV_AUTOMATION_TESTS

#include "PhysAnimBalanceReadyTransition.h"
#include "PhysAnimComponent.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimBalanceReadinessMathTest,
	"PhysAnim.Balance.ReadinessMath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimBalanceReadinessMathTest::RunTest(const FString& Parameters)
{
	FPhysAnimStabilizationSettings Settings;
	Settings.MaxRootLinearSpeedCmPerSecond = 100.0f;
	Settings.MaxRootAngularSpeedDegPerSecond = 45.0f;
	Settings.BalanceEntryMaxGroundDistanceCm = 15.0f;

	FString Reason;

	auto GetDefaultSettings = []()
	{
		FPhysAnimStabilizationSettings S;
		S.MaxRootLinearSpeedCmPerSecond = 100.0f;
		S.MaxRootAngularSpeedDegPerSecond = 45.0f;
		S.BalanceEntryMaxGroundDistanceCm = 15.0f;
		S.BalancePhase2EntryMaxRootTiltDeg = 20.0f;
		S.BalancePhase2EntryMaxShellOffsetDelta = 5.0f;
		S.BalancePhase2EntryMaxShellVelocityDelta = 10.0f;
		S.BalancePhase2EntryMaxTargetDeltaDeg = 15.0f;
		return S;
	};

	auto GetStableDomain = []()
	{
		FPhysAnimStabilizationDomain D;
		D.bRootSimulating = true;
		D.RootLinearSpeed = 10.0f;
		D.RootAngularSpeed = 5.0f;
		D.RootGroundDistance = 5.0f;
		D.RootTiltDeg = 2.0f;
		D.ShellPlanarOffsetCm = 1.0f;
		D.ShellPlanarVelocityCmPerSec = 2.0f;
		D.MaxTargetDeltaDegrees = 3.0f;
		D.MeanTargetDeltaDegrees = 1.0f;
		return D;
	};

	// --- 1. Baseline Success ---
	{
		TestTrue(TEXT("Baseline stable domain should be accepted"), 
			FPhysAnimBalanceReadyTransition::IsSnapshotReady(GetStableDomain(), GetDefaultSettings(), Reason));
		TestEqual(TEXT("Reason should be ready"), Reason, BalanceReadinessReasons::Ready);
	}

	// --- 2. Adversarial Coverage: Physics Continuity ---
	{
		FPhysAnimStabilizationDomain D = GetStableDomain();
		D.bRootSimulating = false;
		TestFalse(TEXT("Dropped simulation must be rejected"), FPhysAnimBalanceReadyTransition::IsSnapshotReady(D, GetDefaultSettings(), Reason));
		TestEqual(TEXT("Reason should be root_simulation_dropped"), Reason, BalanceReadinessReasons::RootSimulationDropped);
	}

	// --- 3. Adversarial Coverage: Linear Speed ---
	{
		FPhysAnimStabilizationDomain D = GetStableDomain();
		D.RootLinearSpeed = 150.0f;
		TestFalse(TEXT("Excessive linear speed must be rejected"), FPhysAnimBalanceReadyTransition::IsSnapshotReady(D, GetDefaultSettings(), Reason));
		TestEqual(TEXT("Reason should be fail_stop_precursor"), Reason, BalanceReadinessReasons::FailStopPrecursor);
	}

	// --- 4. Adversarial Coverage: Angular Speed ---
	{
		FPhysAnimStabilizationDomain D = GetStableDomain();
		D.RootAngularSpeed = 60.0f;
		TestFalse(TEXT("Excessive angular speed must be rejected"), FPhysAnimBalanceReadyTransition::IsSnapshotReady(D, GetDefaultSettings(), Reason));
		TestEqual(TEXT("Reason should be fail_stop_precursor"), Reason, BalanceReadinessReasons::FailStopPrecursor);
	}

	// --- 5. Adversarial Coverage: Ground Distance ---
	{
		FPhysAnimStabilizationDomain D = GetStableDomain();
		D.RootGroundDistance = 25.0f;
		TestFalse(TEXT("Excessive ground distance must be rejected"), FPhysAnimBalanceReadyTransition::IsSnapshotReady(D, GetDefaultSettings(), Reason));
		TestEqual(TEXT("Reason should be root_too_far_from_ground"), Reason, BalanceReadinessReasons::RootTooFarFromGround);
	}

	// --- 6. Adversarial Coverage: Root Tilt ---
	{
		FPhysAnimStabilizationDomain D = GetStableDomain();
		D.RootTiltDeg = 35.0f;
		TestFalse(TEXT("Excessive tilt must be rejected"), FPhysAnimBalanceReadyTransition::IsSnapshotReady(D, GetDefaultSettings(), Reason));
		TestEqual(TEXT("Reason should be root_tilt_too_high"), Reason, BalanceReadinessReasons::RootTiltTooHigh);
	}

	// --- 7. Adversarial Coverage: Shell Offset ---
	{
		FPhysAnimStabilizationDomain D = GetStableDomain();
		D.ShellPlanarOffsetCm = 10.0f;
		TestFalse(TEXT("Excessive shell offset must be rejected"), FPhysAnimBalanceReadyTransition::IsSnapshotReady(D, GetDefaultSettings(), Reason));
		TestEqual(TEXT("Reason should be shell_offset_too_high"), Reason, BalanceReadinessReasons::ShellOffsetTooHigh);
	}

	// --- 8. Adversarial Coverage: Shell Velocity ---
	{
		FPhysAnimStabilizationDomain D = GetStableDomain();
		D.ShellPlanarVelocityCmPerSec = 25.0f;
		TestFalse(TEXT("Excessive shell velocity must be rejected"), FPhysAnimBalanceReadyTransition::IsSnapshotReady(D, GetDefaultSettings(), Reason));
		TestEqual(TEXT("Reason should be shell_velocity_too_high"), Reason, BalanceReadinessReasons::ShellVelocityTooHigh);
	}

	// --- 9. Adversarial Coverage: Target Discontinuity ---
	{
		FPhysAnimStabilizationDomain D = GetStableDomain();
		D.MaxTargetDeltaDegrees = 30.0f;
		TestFalse(TEXT("Excessive target discontinuity must be rejected"), FPhysAnimBalanceReadyTransition::IsSnapshotReady(D, GetDefaultSettings(), Reason));
		TestEqual(TEXT("Reason should be target_discontinuity_too_high"), Reason, BalanceReadinessReasons::TargetDiscontinuityTooHigh);
	}

	// --- 10. Phase Progression Mocking: From Unstable to Ready ---
	{
		FPhysAnimStabilizationSettings S = GetDefaultSettings();
		FString ProgressionReason;

		// Frame 1: High Linear Speed (Prepare)
		FPhysAnimStabilizationDomain F1 = GetStableDomain();
		F1.RootLinearSpeed = 150.0f;
		TestFalse(TEXT("F1 (High Speed) should not be ready"), FPhysAnimBalanceReadyTransition::IsSnapshotReady(F1, S, ProgressionReason));
		TestEqual(TEXT("F1 Reason"), ProgressionReason, BalanceReadinessReasons::FailStopPrecursor);

		// Frame 2: Settled Speed but High Tilt (LateValidate)
		FPhysAnimStabilizationDomain F2 = GetStableDomain();
		F2.RootTiltDeg = 30.0f;
		TestFalse(TEXT("F2 (High Tilt) should not be ready"), FPhysAnimBalanceReadyTransition::IsSnapshotReady(F2, S, ProgressionReason));
		TestEqual(TEXT("F2 Reason"), ProgressionReason, BalanceReadinessReasons::RootTiltTooHigh);

		// Frame 3: All metrics stable (Ready)
		FPhysAnimStabilizationDomain F3 = GetStableDomain();
		TestTrue(TEXT("F3 (Stable) should be ready"), FPhysAnimBalanceReadyTransition::IsSnapshotReady(F3, S, ProgressionReason));
		TestEqual(TEXT("F3 Reason"), ProgressionReason, BalanceReadinessReasons::Ready);
	}

	// --- 11. Adversarial Coverage: Topology Preservation (Phase 2) ---
	{
		FPhysAnimStabilizationDomain D = GetStableDomain();
		D.CurrentPhase = EBalanceReadyTransitionPhase::BRT_Phase1_LateValidate;
		D.CertifiedSimCount = 20;
		D.SimCount = 19; // Missing a bone
		D.CertifiedDistalSimCount = 4;
		D.DistalSimCount = 4;

		TestFalse(TEXT("Topology mismatch in Phase 2 must be rejected"), FPhysAnimBalanceReadyTransition::IsSnapshotReady(D, GetDefaultSettings(), Reason));
		TestEqual(TEXT("Reason should be topology_mismatch"), Reason, BalanceReadinessReasons::TopologyMismatch);
	}

	// --- 12. Adversarial Coverage: Phase 3 Stability (Relaxed) ---
	{
		FPhysAnimStabilizationDomain D = GetStableDomain();
		D.CurrentPhase = EBalanceReadyTransitionPhase::BRT_Phase3_Settle;
		D.RootLinearSpeed = 150.0f; // Above base 100.0, but below 2.5x (250.0)

		TestTrue(TEXT("Phase 3 should accept 1.5x speed (relaxed)"), FPhysAnimBalanceReadyTransition::IsSnapshotReady(D, GetDefaultSettings(), Reason));
		TestEqual(TEXT("Reason should be ready"), Reason, BalanceReadinessReasons::Ready);

		D.RootLinearSpeed = 300.0f; // Above 2.5x threshold
		TestFalse(TEXT("Phase 3 should reject 3.0x speed spike"), FPhysAnimBalanceReadyTransition::IsSnapshotReady(D, GetDefaultSettings(), Reason));
		TestEqual(TEXT("Reason should be phase3_post_root_on_instability"), Reason, BalanceReadinessReasons::Phase3InstabilitySpike);
	}

	return true;
}

#endif
