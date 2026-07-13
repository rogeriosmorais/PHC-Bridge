#include "PhysAnimStandingActivation.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FPhysAnimStandingActivationReadback MakeValidReadback()
{
	FPhysAnimStandingActivationReadback Readback;
	Readback.bFullSimulationCommitted = true;
	Readback.ModifierSimulationMatchCount = FPhysAnimStandingActivationPlan::RequiredBodyCount;
	Readback.RawSimulationMatchCount = FPhysAnimStandingActivationPlan::RequiredBodyCount;
	Readback.ControlGainMatchCount = FPhysAnimStandingActivationPlan::RequiredControlCount;
	return Readback;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimStandingActivationSequenceTest,
	"PhysAnim.Component.StandingActivation.Sequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimStandingActivationSequenceTest::RunTest(const FString& Parameters)
{
	FPhysAnimStandingActivation Activation;
	Activation.Start();
	TestEqual(TEXT("Preparation begins immediately"), Activation.GetStatus().RuntimeState, EPhysAnimRuntimeState::Standing_Preparation);

	Activation.CompletePreparation(true, TEXT(""));
	TestEqual(TEXT("Preparation advances directly to atomic activation"), Activation.GetStatus().RuntimeState, EPhysAnimRuntimeState::Standing_FullSimulationActivation);

	Activation.CompleteFullSimulationActivation(MakeValidReadback(), TEXT(""));
	TestEqual(TEXT("Valid atomic activation advances directly to policy blend"), Activation.GetStatus().RuntimeState, EPhysAnimRuntimeState::Standing_PolicyBlend);

	Activation.TickPolicyBlend(0.0f, 2.0f, MakeValidReadback(), TEXT(""));
	TestEqual(TEXT("Blend starts at zero"), Activation.GetStatus().LinearBlendAlpha, 0.0f);
	TestEqual(TEXT("Target alpha shares the linear alpha"), Activation.GetTargetBlendAlpha(), Activation.GetGainBlendAlpha());
	Activation.TickPolicyBlend(1.0f, 2.0f, MakeValidReadback(), TEXT(""));
	TestEqual(TEXT("Blend midpoint is linear"), Activation.GetStatus().LinearBlendAlpha, 0.5f);
	Activation.TickPolicyBlend(2.0f, 2.0f, MakeValidReadback(), TEXT(""));
	TestEqual(TEXT("Blend reaches standing at alpha one"), Activation.GetStatus().LinearBlendAlpha, 1.0f);
	TestEqual(TEXT("Standing is the only successful terminal activation state"), Activation.GetStatus().RuntimeState, EPhysAnimRuntimeState::BalanceActive_Standing);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimStandingActivationUniformPlanTest,
	"PhysAnim.Component.StandingActivation.UniformPlan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimStandingActivationUniformPlanTest::RunTest(const FString& Parameters)
{
	const FPhysAnimStandingActivationPlan Normal = FPhysAnimStandingActivationPlan::Build(EPhysAnimStandingVariant::Normal, 1000.0f, 1.0f, 50.0f);
	const FPhysAnimStandingActivationPlan Zero = FPhysAnimStandingActivationPlan::Build(EPhysAnimStandingVariant::ZeroActions, 1000.0f, 1.0f, 50.0f);
	const FPhysAnimStandingActivationPlan Dropped = FPhysAnimStandingActivationPlan::Build(EPhysAnimStandingVariant::DropControlDispatch, 1000.0f, 1.0f, 50.0f);

	TestEqual(TEXT("Every variant plans all Manny bodies"), Normal.Bodies.Num(), FPhysAnimStandingActivationPlan::RequiredBodyCount);
	TestEqual(TEXT("Every variant plans all controlled joints"), Normal.Controls.Num(), FPhysAnimStandingActivationPlan::RequiredControlCount);
	TestTrue(TEXT("ZeroActions activation matches Normal"), Normal == Zero);
	TestTrue(TEXT("Dropped dispatch activation matches Normal"), Normal == Dropped);
	for (const FPhysAnimStandingBodyPlan& Body : Normal.Bodies)
	{
		TestTrue(TEXT("Every body is simulated"), Body.bSimulated);
		TestEqual(TEXT("Every body has full physics blend"), Body.PhysicsBlendWeight, 1.0f);
		TestEqual(TEXT("Every body has query and physics collision"), Body.CollisionEnabled, ECollisionEnabled::QueryAndPhysics);
		TestFalse(TEXT("No body updates kinematic pose from simulation"), Body.bUpdateKinematicFromSimulation);
	}
	for (const FPhysAnimStandingControlPlan& Control : Normal.Controls)
	{
		TestTrue(TEXT("Every control is enabled for every variant"), Control.bEnabled);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimStandingActivationFailStopTest,
	"PhysAnim.Component.StandingActivation.FailStop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimStandingActivationFailStopTest::RunTest(const FString& Parameters)
{
	FPhysAnimStandingActivation WithEvidence;
	WithEvidence.SetObservationalEvidenceFlags(true, true);
	WithEvidence.Start();
	WithEvidence.CompletePreparation(true, TEXT(""));

	FPhysAnimStandingActivationReadback Mismatch = MakeValidReadback();
	Mismatch.ControlGainMatchCount = FPhysAnimStandingActivationPlan::RequiredControlCount - 1;
	WithEvidence.CompleteFullSimulationActivation(Mismatch, TEXT("gain_readback_mismatch"));
	const FPhysAnimStandingActivationStatus Status = WithEvidence.GetStatus();
	TestEqual(TEXT("Readback mismatch fail-stops"), Status.RuntimeState, EPhysAnimRuntimeState::FailStopped);
	TestEqual(TEXT("Failure reason is explicit"), Status.FailureReason, FString(TEXT("gain_readback_mismatch")));
	TestFalse(TEXT("Failure does not retry"), Status.bRetryRequested);
	TestFalse(TEXT("Failure does not demote topology"), Status.bKinematicDemotionRequested);
	TestFalse(TEXT("Failure does not request a cached reset"), Status.bResetRequested);
	TestFalse(TEXT("Failure does not publish BalanceSafeDeny"), Status.bBalanceSafeDenyPublished);

	FPhysAnimStandingActivation WithoutEvidence;
	WithoutEvidence.SetObservationalEvidenceFlags(false, false);
	WithoutEvidence.Start();
	WithoutEvidence.CompletePreparation(true, TEXT(""));
	WithoutEvidence.CompleteFullSimulationActivation(Mismatch, TEXT("gain_readback_mismatch"));
	TestEqual(TEXT("Evidence flags do not alter activation decisions"), WithoutEvidence.GetStatus().RuntimeState, Status.RuntimeState);
	return true;
}

#endif
