#include "PhysAnimRuntimeOrchestrator.h"
#include "Misc/AutomationTest.h"

namespace
{
	FPhysAnimFailureCandidate RuntimeOrchestrator_MakeFailureCandidate(EPhysAnimTerminalReason Reason, int64 TerminalSubstepTimestamp)
	{
		FPhysAnimFailureCandidate Candidate;
		Candidate.TerminalReason = Reason;
		Candidate.TerminalSubstepTimestamp = TerminalSubstepTimestamp;
		return Candidate;
	}

	FPhysAnimSupportObservationResult RuntimeOrchestrator_MakeCleanSupportObservation()
	{
		FPhysAnimSupportObservationResult Observation;
		Observation.Snapshot.bSupportStateL = true;
		Observation.Snapshot.SupportMode = EPhysAnimSupportMode::SingleFootSurvival;
		Observation.Snapshot.ActiveSupportSideCount = 1;
		Observation.Snapshot.SupportHullAreaCm2 = 100.0;
		Observation.Snapshot.SupportPatchAreaLCm2 = 100.0;
		Observation.Snapshot.ComProxyPosCm = FVector2D(5.0, 5.0);
		Observation.Snapshot.ProxyInsideHull = true;

		Observation.Validation.bSupportStateL = true;
		Observation.Validation.SupportMode = EPhysAnimSupportMode::SingleFootSurvival;
		Observation.Validation.ActiveSupportSideCount = 1;
		Observation.Validation.SupportHullAreaCm2 = 100.0;
		Observation.Validation.SupportPatchAreaLCm2 = 100.0;
		Observation.Validation.ComProxyPosCm = FVector2D(5.0, 5.0);
		Observation.Validation.ProxyInsideHull = true;
		Observation.Validation.bSupportContractPassed = true;
		Observation.Validation.TerminalReason = EPhysAnimTerminalReason::None;
		Observation.bObservationValid = true;

		return Observation;
	}

	FPhysAnimSupportObservationResult RuntimeOrchestrator_MakeSupportFailureObservation()
	{
		FPhysAnimSupportObservationResult Observation = RuntimeOrchestrator_MakeCleanSupportObservation();
		Observation.Snapshot.SupportMode = EPhysAnimSupportMode::Airborne;
		Observation.Snapshot.ActiveSupportSideCount = 0;
		Observation.Snapshot.SupportHullAreaCm2 = 0.0;
		Observation.Snapshot.SupportGapTimerMs = 120.0;

		Observation.Validation.SupportMode = EPhysAnimSupportMode::Airborne;
		Observation.Validation.ActiveSupportSideCount = 0;
		Observation.Validation.SupportHullAreaCm2 = 0.0;
		Observation.Validation.SupportGapTimerMs = 120.0;
		Observation.Validation.bSupportContractPassed = false;
		Observation.Validation.TerminalReason = EPhysAnimTerminalReason::ActivationSupportFailure;
		Observation.bObservationValid = false;

		return Observation;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimRuntimeSubstepOrchestratorTest,
		"PhysAnim.RuntimeOrchestrator.Substep",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimRuntimeSubstepOrchestratorTest::RunTest(const FString& Parameters)
	{
		{
			FPhysAnimRuntimeSubstepInput Input;
			Input.Values.TerminalSubstepTimestamp = 100;
			Input.SupportObservation = RuntimeOrchestrator_MakeCleanSupportObservation();

			const FPhysAnimRuntimeSubstepResult Result = PhysAnimRuntimeOrchestrator::EvaluateRuntimeSubstep(Input);

			TestFalse(TEXT("ORCH-01 clean substep does not terminate"), Result.bShouldTerminate);
			TestFalse(TEXT("ORCH-01 terminal artifact not captured"), Result.bTerminalFrameArtifactCaptured);
			TestEqual(
				TEXT("ORCH-01 terminal reason is None"),
				static_cast<uint8>(Result.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::None));
			TestFalse(TEXT("ORCH-01 artifact terminal frame flag false"), Result.Artifact.bTerminalFrameArtifactCaptured);
		}

		{
			FPhysAnimRuntimeSubstepInput Input;
			Input.Values.TerminalSubstepTimestamp = 200;
			Input.SupportObservation = RuntimeOrchestrator_MakeSupportFailureObservation();

			const FPhysAnimRuntimeSubstepResult Result = PhysAnimRuntimeOrchestrator::EvaluateRuntimeSubstep(Input);

			TestTrue(TEXT("ORCH-02 support failure terminates"), Result.bShouldTerminate);
			TestTrue(TEXT("ORCH-02 terminal artifact captured"), Result.bTerminalFrameArtifactCaptured);
			TestEqual(
				TEXT("ORCH-02 terminal reason is support failure"),
				static_cast<uint8>(Result.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationSupportFailure));
			TestEqual(TEXT("ORCH-02 timestamp preserved"), Result.TerminalSubstepTimestamp, static_cast<int64>(200));
			TestTrue(TEXT("ORCH-02 artifact terminal frame flag true"), Result.Artifact.bTerminalFrameArtifactCaptured);
		}

		{
			FPhysAnimRuntimeSubstepInput Input;
			Input.Values.TerminalSubstepTimestamp = 300;
			Input.SupportObservation = RuntimeOrchestrator_MakeSupportFailureObservation();
			Input.Plant.TerminalReason = EPhysAnimTerminalReason::ActivationPhysicsAssetContractViolation;
			Input.Plant.bPhysicsAssetContractValid = false;

			const FPhysAnimRuntimeSubstepResult Result = PhysAnimRuntimeOrchestrator::EvaluateRuntimeSubstep(Input);

			TestTrue(TEXT("ORCH-03 same-substep plant/support failure terminates"), Result.bShouldTerminate);
			TestEqual(
				TEXT("ORCH-03 plant outranks support at same timestamp"),
				static_cast<uint8>(Result.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationPhysicsAssetContractViolation));
			TestTrue(
				TEXT("ORCH-03 support is co-terminal"),
				Result.Artifact.CoTerminalReasons.Contains(EPhysAnimTerminalReason::ActivationSupportFailure));
		}

		{
			FPhysAnimRuntimeSubstepInput Input;
			Input.Values.TerminalSubstepTimestamp = 400;
			Input.SupportObservation = RuntimeOrchestrator_MakeSupportFailureObservation();
			Input.AdditionalFailureCandidates.Add(
				RuntimeOrchestrator_MakeFailureCandidate(EPhysAnimTerminalReason::ActivationAuthorityConflict, 399));

			const FPhysAnimRuntimeSubstepResult Result = PhysAnimRuntimeOrchestrator::EvaluateRuntimeSubstep(Input);

			TestTrue(TEXT("ORCH-04 earlier additional candidate terminates"), Result.bShouldTerminate);
			TestEqual(
				TEXT("ORCH-04 earlier authority wins by temporal precedence"),
				static_cast<uint8>(Result.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationAuthorityConflict));
			TestEqual(TEXT("ORCH-04 earlier timestamp wins"), Result.TerminalSubstepTimestamp, static_cast<int64>(399));
		}

		{
			FPhysAnimRuntimeSubstepInput Input;
			Input.Values.TerminalSubstepTimestamp = 500;
			Input.SupportObservation = RuntimeOrchestrator_MakeSupportFailureObservation();
			Input.bEnableTermination = false;

			const FPhysAnimRuntimeSubstepResult Result = PhysAnimRuntimeOrchestrator::EvaluateRuntimeSubstep(Input);

			TestFalse(TEXT("ORCH-05 disabled termination does not terminate"), Result.bShouldTerminate);
			TestFalse(TEXT("ORCH-05 terminal artifact not captured when disabled"), Result.bTerminalFrameArtifactCaptured);
			TestEqual(
				TEXT("ORCH-05 artifact still preserves terminal reason"),
				static_cast<uint8>(Result.Artifact.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationSupportFailure));
			TestFalse(TEXT("ORCH-05 artifact terminal frame flag false when disabled"), Result.Artifact.bTerminalFrameArtifactCaptured);
		}

		{
			FPhysAnimRuntimeSubstepInput Input;
			Input.Values.TerminalSubstepTimestamp = 600;
			Input.SupportObservation = RuntimeOrchestrator_MakeCleanSupportObservation();
			Input.SupportObservation.Validation.SupportChurnCount = 7;
			Input.SupportObservation.Validation.SupportChurnHz = 3.5;
			Input.SupportObservation.Validation.SupportHullAreaCm2 = 123.0;
			Input.SupportObservation.Validation.ActiveSupportSideCount = 1;

			const FPhysAnimRuntimeSubstepResult Result = PhysAnimRuntimeOrchestrator::EvaluateRuntimeSubstep(Input);

			TestEqual(TEXT("ORCH-06 support active side copied to artifact"), Result.Artifact.ActiveSupportSideCount, 1);
			TestEqual(TEXT("ORCH-06 support hull area copied to artifact"), Result.Artifact.SupportHullAreaCm2, 123.0);
			TestEqual(TEXT("ORCH-06 support churn count copied to artifact"), Result.Artifact.SupportChurnCount, 7);
			TestEqual(TEXT("ORCH-06 support churn hz copied to artifact"), Result.Artifact.SupportChurnHz, 3.5);
		}

		return true;
	}
}
