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

	{
		FPhase1AcceptedConvergenceSnapshot StableSnapshot;
		StableSnapshot.FrameIndex = 1;
		StableSnapshot.RootLinearSpeed = 10.0f;
		StableSnapshot.RootAngularSpeed = 5.0f;
		StableSnapshot.RootGroundDistance = 5.0f;
		TestTrue(TEXT("Stable snapshot should be accepted"), FPhysAnimBalanceReadyTransition::IsRootStable(StableSnapshot, Settings, Reason));
	}

	{
		FPhase1AcceptedConvergenceSnapshot FastSnapshot;
		FastSnapshot.FrameIndex = 1;
		FastSnapshot.RootLinearSpeed = 150.0f; // Above 100
		FastSnapshot.RootAngularSpeed = 5.0f;
		FastSnapshot.RootGroundDistance = 5.0f;
		TestFalse(TEXT("Fast snapshot should be rejected"), FPhysAnimBalanceReadyTransition::IsRootStable(FastSnapshot, Settings, Reason));
		TestEqual(TEXT("Reason should be fail_stop_precursor"), Reason, TEXT("fail_stop_precursor"));
	}

	{
		FPhase1AcceptedConvergenceSnapshot HighSnapshot;
		HighSnapshot.FrameIndex = 1;
		HighSnapshot.RootLinearSpeed = 10.0f;
		HighSnapshot.RootAngularSpeed = 5.0f;
		HighSnapshot.RootGroundDistance = 50.0f; // Above 15.0f
		TestFalse(TEXT("High snapshot should be rejected"), FPhysAnimBalanceReadyTransition::IsRootStable(HighSnapshot, Settings, Reason));
		TestEqual(TEXT("Reason should be root_too_far_from_ground"), Reason, TEXT("root_too_far_from_ground"));
	}

	return true;
}

#endif
