#include "PhysAnimComponent.h"
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
	FPhysAnimStandingActivationPublicationTest,
	"PhysAnim.Component.StandingActivation.Publication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimStandingActivationPublicationTest::RunTest(const FString& Parameters)
{
	const FPhysAnimStandingPublicationDecision First =
		FPhysAnimStandingPublicationDecision::Build(false, true);
	const FPhysAnimStandingPublicationDecision Later =
		FPhysAnimStandingPublicationDecision::Build(true, true);
	TestTrue(TEXT("The first atomic publication commits movement types"), First.bWriteMovementTypes);
	TestFalse(TEXT("Movement types are never republished after commit"), Later.bWriteMovementTypes);
	TestTrue(TEXT("Gains are published on the activation tick"), First.bWriteControlGains);
	TestTrue(TEXT("Gains are republished after topology commit"), Later.bWriteControlGains);

	const FPhysAnimStandingActivationPlan Start =
		FPhysAnimStandingActivationPlan::BuildBlended(EPhysAnimStandingVariant::Normal, 0.0f, 1000.0f, 1.5f, 2.0f, 1.2f);
	const FPhysAnimStandingActivationPlan Mid =
		FPhysAnimStandingActivationPlan::BuildBlended(EPhysAnimStandingVariant::ZeroActions, 0.5f, 1000.0f, 1.5f, 2.0f, 1.2f);
	const FPhysAnimStandingActivationPlan End =
		FPhysAnimStandingActivationPlan::BuildBlended(EPhysAnimStandingVariant::DropControlDispatch, 1.0f, 1000.0f, 1.5f, 2.0f, 1.2f);
	TestEqual(TEXT("Strength begins at zero"), Start.Controls[0].AngularStrength, 0.0f);
	TestEqual(TEXT("Strength uses the shared midpoint alpha"), Mid.Controls[0].AngularStrength, 500.0f);
	TestEqual(TEXT("Strength reaches the configured endpoint"), End.Controls[0].AngularStrength, 1000.0f);
	TestEqual(TEXT("Configured damping ratio remains active at blend start"), Start.Controls[0].DampingRatio, 1.5f);
	TestEqual(TEXT("Configured damping ratio remains active at blend end"), End.Controls[0].DampingRatio, 1.5f);
	TestEqual(TEXT("Extra damping begins at bootstrap"), Start.Controls[0].ExtraDamping, 2.0f);
	TestEqual(TEXT("Extra damping reaches standing"), End.Controls[0].ExtraDamping, 1.2f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimStandingActivationOwnershipBoundaryTest,
	"PhysAnim.Component.StandingActivation.OwnershipBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimStandingActivationOwnershipBoundaryTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Preparation is owned by the unified standing path"), UPhysAnimComponent::IsStandingActivationRuntimeState(EPhysAnimRuntimeState::Standing_Preparation));
	TestTrue(TEXT("Atomic activation is owned by the unified standing path"), UPhysAnimComponent::IsStandingActivationRuntimeState(EPhysAnimRuntimeState::Standing_FullSimulationActivation));
	TestTrue(TEXT("Policy blend is owned by the unified standing path"), UPhysAnimComponent::IsStandingActivationRuntimeState(EPhysAnimRuntimeState::Standing_PolicyBlend));
	TestTrue(TEXT("Standing remains owned by the unified standing path"), UPhysAnimComponent::IsStandingActivationRuntimeState(EPhysAnimRuntimeState::BalanceActive_Standing));
	TestFalse(TEXT("Legacy phase states are not owned by the standing path"), UPhysAnimComponent::IsStandingActivationRuntimeState(EPhysAnimRuntimeState::BalanceEntry_RootOn));
	TestFalse(TEXT("Future locomotion remains outside standing ownership"), UPhysAnimComponent::IsStandingActivationRuntimeState(EPhysAnimRuntimeState::LocomotionActiveShell));
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
