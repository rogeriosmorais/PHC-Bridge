#include "PhysAnimRuntimeTerminationPipeline.h"
#include "Misc/AutomationTest.h"

namespace
{
	FPhysAnimFailureCandidate RuntimeTerminationPipeline_MakeFailureCandidate(
		const EPhysAnimTerminalReason Reason,
		const int64 TerminalSubstepTimestamp)
	{
		FPhysAnimFailureCandidate Candidate;
		Candidate.TerminalReason = Reason;
		Candidate.TerminalSubstepTimestamp = TerminalSubstepTimestamp;
		return Candidate;
	}

	FPhysAnimSupportObservationResult RuntimeTerminationPipeline_MakeCleanSupportObservation()
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

	FPhysAnimSupportObservationResult RuntimeTerminationPipeline_MakeSupportFailureObservation()
	{
		FPhysAnimSupportObservationResult Observation = RuntimeTerminationPipeline_MakeCleanSupportObservation();

		Observation.Snapshot.bSupportStateL = false;
		Observation.Snapshot.SupportMode = EPhysAnimSupportMode::Airborne;
		Observation.Snapshot.ActiveSupportSideCount = 0;
		Observation.Snapshot.SupportHullAreaCm2 = 0.0;
		Observation.Snapshot.SupportPatchAreaLCm2 = 0.0;
		Observation.Snapshot.SupportGapTimerMs = 120.0;
		Observation.Snapshot.ProxyInsideHull.Reset();

		Observation.Validation.bSupportStateL = false;
		Observation.Validation.SupportMode = EPhysAnimSupportMode::Airborne;
		Observation.Validation.ActiveSupportSideCount = 0;
		Observation.Validation.SupportHullAreaCm2 = 0.0;
		Observation.Validation.SupportPatchAreaLCm2 = 0.0;
		Observation.Validation.SupportGapTimerMs = 120.0;
		Observation.Validation.ProxyInsideHull.Reset();
		Observation.Validation.bSupportContractPassed = false;
		Observation.Validation.TerminalReason = EPhysAnimTerminalReason::ActivationSupportFailure;
		Observation.bObservationValid = false;

		return Observation;
	}

	FPhysAnimRuntimeSubstepInput RuntimeTerminationPipeline_MakeCleanSubstepInput(
		const int64 TerminalSubstepTimestamp,
		const TCHAR* AttemptUuid)
	{
		FPhysAnimRuntimeSubstepInput Input;

		Input.Values.Timestamp = static_cast<double>(TerminalSubstepTimestamp);
		Input.Values.TerminalSubstepTimestamp = TerminalSubstepTimestamp;
		Input.Values.AttemptUuid = AttemptUuid;
		Input.Values.ActiveSupportSideCount = 1;
		Input.Values.SupportHullAreaCm2 = 100.0;
		Input.Values.SupportGapTimerMs = 0.0;

		Input.SupportObservation = RuntimeTerminationPipeline_MakeCleanSupportObservation();

		Input.Authority.bAuthorityPassed = true;
		Input.MovementReclaim.bMovementReclaimPassed = true;
		Input.ShellHelper.bShellHelperPassed = true;
		Input.ControllerStability.bControllerStabilityPassed = true;
		Input.Continuity.bPhysicalContinuityValidatorPassed = true;

		return Input;
	}

	FPhysAnimRuntimeSubstepInput RuntimeTerminationPipeline_MakeSupportFailureSubstepInput(
		const int64 TerminalSubstepTimestamp,
		const TCHAR* AttemptUuid)
	{
		FPhysAnimRuntimeSubstepInput Input = RuntimeTerminationPipeline_MakeCleanSubstepInput(TerminalSubstepTimestamp, AttemptUuid);

		Input.Values.ActiveSupportSideCount = 0;
		Input.Values.SupportHullAreaCm2 = 0.0;
		Input.Values.SupportGapTimerMs = 120.0;

		Input.SupportObservation = RuntimeTerminationPipeline_MakeSupportFailureObservation();

		return Input;
	}

	FPhysAnimRuntimeSubstepInput RuntimeTerminationPipeline_MakePlantFailureSubstepInput(
		const int64 TerminalSubstepTimestamp,
		const TCHAR* AttemptUuid)
	{
		FPhysAnimRuntimeSubstepInput Input = RuntimeTerminationPipeline_MakeCleanSubstepInput(TerminalSubstepTimestamp, AttemptUuid);

		Input.Plant.bPhysicsAssetContractValid = false;
		Input.Plant.bSkeletonAuditPassed = false;
		Input.Plant.PlantFailureClass = EPhysAnimPlantFailureClass::StaticStructural;
		Input.Plant.PlantFailureField = EPhysAnimPlantFailureField::Skeleton;
		Input.Plant.TerminalReason = EPhysAnimTerminalReason::ActivationPhysicsAssetContractViolation;

		return Input;
	}

	FPhysAnimRuntimeSubstepInput RuntimeTerminationPipeline_MakeEarlierAdditionalFailureSubstepInput(
		const int64 CurrentSubstepTimestamp,
		const int64 EarlierFailureTimestamp,
		const TCHAR* AttemptUuid)
	{
		FPhysAnimRuntimeSubstepInput Input = RuntimeTerminationPipeline_MakeSupportFailureSubstepInput(CurrentSubstepTimestamp, AttemptUuid);
		Input.AdditionalFailureCandidates.Add(RuntimeTerminationPipeline_MakeFailureCandidate(
			EPhysAnimTerminalReason::ActivationAuthorityConflict,
			EarlierFailureTimestamp));
		return Input;
	}

	FPhysAnimRuntimeProofFailureFailStopRoutingInput RuntimeTerminationPipeline_MakeProofFailureRoutingInput(
		const int64 TerminalSubstepTimestamp,
		const TCHAR* AttemptUuid,
		const EPhysAnimTerminalReason Reason)
	{
		FPhysAnimRuntimeProofFailureFailStopRoutingInput Input;
		FPhysAnimRuntimeSubstepInput SeedSubstepInput =
			RuntimeTerminationPipeline_MakeCleanSubstepInput(TerminalSubstepTimestamp, AttemptUuid);
		Input.PreviousState.bTerminated = true;
		Input.PreviousState.TerminalReason = Reason;
		Input.PreviousState.TerminalSubstepTimestamp = TerminalSubstepTimestamp;
		Input.PreviousState.bTerminalFrameArtifactCaptured = true;
		Input.PreviousState.TerminalArtifact = SeedSubstepInput.Values;
		Input.PreviousState.TerminalArtifact.TerminalReason = Reason;
		Input.PreviousState.TerminalArtifact.TerminalSubstepTimestamp = TerminalSubstepTimestamp;
		Input.PreviousState.TerminalArtifact.bTerminalFrameArtifactCaptured = true;
		Input.Artifact = Input.PreviousState.TerminalArtifact;
		Input.TerminalReason = Reason;
		Input.TerminalSubstepTimestamp = TerminalSubstepTimestamp;
		return Input;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimRuntimeTerminationPipelineTest,
		"PhysAnim.RuntimeTermination.Pipeline",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimRuntimeTerminationPipelineTest::RunTest(const FString& Parameters)
	{
		{
			FPhysAnimRuntimeTerminationPipelineInput Input_A;
			Input_A.SubstepInput = RuntimeTerminationPipeline_MakeCleanSubstepInput(100, TEXT("pipeline-clean"));

			const FPhysAnimRuntimeTerminationPipelineResult Result_A =
				PhysAnimRuntimeTerminationPipeline::EvaluateTerminationPipeline(Input_A);

			TestFalse(TEXT("TERMINATION-PIPELINE-01 clean substep does not terminate command"), Result_A.Command.bTerminate);
			TestFalse(TEXT("TERMINATION-PIPELINE-01 clean substep does not terminate state"), Result_A.StateApplyResult.State.bTerminated);
			TestFalse(TEXT("TERMINATION-PIPELINE-01 clean substep not applied as termination"), Result_A.StateApplyResult.bAppliedTermination);
			TestEqual(TEXT("TERMINATION-PIPELINE-01 latest artifact retained"), Result_A.StateApplyResult.State.LatestArtifact.AttemptUuid, FString(TEXT("pipeline-clean")));
			TestEqual(TEXT("TERMINATION-PIPELINE-01 latest artifact timestamp copied"), Result_A.StateApplyResult.State.LatestArtifact.TerminalSubstepTimestamp, static_cast<int64>(100));
		}

		{
			FPhysAnimRuntimeTerminationPipelineInput Input_B;
			Input_B.SubstepInput = RuntimeTerminationPipeline_MakeSupportFailureSubstepInput(200, TEXT("pipeline-support-failure"));

			const FPhysAnimRuntimeTerminationPipelineResult Result_B =
				PhysAnimRuntimeTerminationPipeline::EvaluateTerminationPipeline(Input_B);

			TestTrue(TEXT("TERMINATION-PIPELINE-02 support failure terminates command"), Result_B.Command.bTerminate);
			TestTrue(TEXT("TERMINATION-PIPELINE-02 support failure terminates state"), Result_B.StateApplyResult.State.bTerminated);
			TestTrue(TEXT("TERMINATION-PIPELINE-02 support failure applied as termination"), Result_B.StateApplyResult.bAppliedTermination);
			TestEqual(
				TEXT("TERMINATION-PIPELINE-02 terminal reason is support failure"),
				static_cast<uint8>(Result_B.StateApplyResult.State.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationSupportFailure));
			TestEqual(TEXT("TERMINATION-PIPELINE-02 terminal timestamp copied"), Result_B.StateApplyResult.State.TerminalSubstepTimestamp, static_cast<int64>(200));
		}

		{
			FPhysAnimRuntimeTerminationPipelineInput Input_C;
			Input_C.SubstepInput = RuntimeTerminationPipeline_MakeSupportFailureSubstepInput(300, TEXT("pipeline-disabled-command"));
			Input_C.bEnableTerminationCommand = false;

			const FPhysAnimRuntimeTerminationPipelineResult Result_C =
				PhysAnimRuntimeTerminationPipeline::EvaluateTerminationPipeline(Input_C);

			TestEqual(
				TEXT("TERMINATION-PIPELINE-03 orchestrator still reports terminal reason"),
				static_cast<uint8>(Result_C.SubstepResult.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationSupportFailure));
			TestFalse(TEXT("TERMINATION-PIPELINE-03 disabled command does not terminate command"), Result_C.Command.bTerminate);
			TestFalse(TEXT("TERMINATION-PIPELINE-03 disabled command does not terminate state"), Result_C.StateApplyResult.State.bTerminated);
			TestEqual(TEXT("TERMINATION-PIPELINE-03 latest artifact retained"), Result_C.StateApplyResult.State.LatestArtifact.AttemptUuid, FString(TEXT("pipeline-disabled-command")));
			TestEqual(TEXT("TERMINATION-PIPELINE-03 latest artifact terminal timestamp retained"), Result_C.StateApplyResult.State.LatestArtifact.TerminalSubstepTimestamp, static_cast<int64>(300));
		}

		{
			FPhysAnimRuntimeTerminationPipelineInput Input_D;
			Input_D.SubstepInput = RuntimeTerminationPipeline_MakeSupportFailureSubstepInput(400, TEXT("pipeline-reclaim-disallowed"));
			Input_D.bAllowMovementReclaimOnTermination = false;

			const FPhysAnimRuntimeTerminationPipelineResult Result_D =
				PhysAnimRuntimeTerminationPipeline::EvaluateTerminationPipeline(Input_D);

			TestTrue(TEXT("TERMINATION-PIPELINE-04 command still terminates"), Result_D.Command.bTerminate);
			TestFalse(TEXT("TERMINATION-PIPELINE-04 command suppresses movement reclaim"), Result_D.Command.bRequestMovementReclaim);
			TestFalse(TEXT("TERMINATION-PIPELINE-04 state suppresses movement reclaim"), Result_D.StateApplyResult.State.bMovementReclaimRequested);
		}

		{
			FPhysAnimRuntimeTerminationPipelineInput FirstInput_E;
			FirstInput_E.SubstepInput = RuntimeTerminationPipeline_MakeSupportFailureSubstepInput(500, TEXT("pipeline-first-terminal"));

			const FPhysAnimRuntimeTerminationPipelineResult FirstResult_E =
				PhysAnimRuntimeTerminationPipeline::EvaluateTerminationPipeline(FirstInput_E);

			FPhysAnimRuntimeTerminationPipelineInput SecondInput_E;
			SecondInput_E.PreviousState = FirstResult_E.StateApplyResult.State;
			SecondInput_E.SubstepInput = RuntimeTerminationPipeline_MakePlantFailureSubstepInput(501, TEXT("pipeline-second-terminal"));

			const FPhysAnimRuntimeTerminationPipelineResult SecondResult_E =
				PhysAnimRuntimeTerminationPipeline::EvaluateTerminationPipeline(SecondInput_E);

			TestTrue(TEXT("TERMINATION-PIPELINE-05 later terminal ignored by state"), SecondResult_E.StateApplyResult.bIgnoredBecauseAlreadyTerminated);
			TestEqual(TEXT("TERMINATION-PIPELINE-05 first terminal timestamp preserved"), SecondResult_E.StateApplyResult.State.TerminalSubstepTimestamp, static_cast<int64>(500));
			TestEqual(TEXT("TERMINATION-PIPELINE-05 first terminal artifact preserved"), SecondResult_E.StateApplyResult.State.TerminalArtifact.AttemptUuid, FString(TEXT("pipeline-first-terminal")));
		}

		{
			FPhysAnimRuntimeTerminationPipelineInput Input_F;
			Input_F.SubstepInput = RuntimeTerminationPipeline_MakeCleanSubstepInput(600, TEXT("pipeline-artifact-fields"));
			Input_F.SubstepInput.SupportObservation.Validation.SupportChurnCount = 9;
			Input_F.SubstepInput.SupportObservation.Validation.SupportChurnHz = 4.5;
			Input_F.SubstepInput.SupportObservation.Validation.SupportHullAreaCm2 = 321.0;
			Input_F.SubstepInput.SupportObservation.Validation.ActiveSupportSideCount = 1;

			const FPhysAnimRuntimeTerminationPipelineResult Result_F =
				PhysAnimRuntimeTerminationPipeline::EvaluateTerminationPipeline(Input_F);

			TestEqual(TEXT("TERMINATION-PIPELINE-06 artifact support hull area copied"), Result_F.SubstepResult.Artifact.SupportHullAreaCm2, 321.0);
			TestEqual(TEXT("TERMINATION-PIPELINE-06 artifact support churn count copied"), Result_F.SubstepResult.Artifact.SupportChurnCount, 9);
			TestEqual(TEXT("TERMINATION-PIPELINE-06 artifact support churn hz copied"), Result_F.SubstepResult.Artifact.SupportChurnHz, 4.5);
			TestEqual(TEXT("TERMINATION-PIPELINE-06 latest artifact sees same support hull area"), Result_F.StateApplyResult.State.LatestArtifact.SupportHullAreaCm2, 321.0);
			TestEqual(TEXT("TERMINATION-PIPELINE-06 latest artifact timestamp copied"), Result_F.StateApplyResult.State.LatestArtifact.TerminalSubstepTimestamp, static_cast<int64>(600));
		}

		{
			FPhysAnimRuntimeTerminationPipelineInput Input_G;
			Input_G.SubstepInput = RuntimeTerminationPipeline_MakeSupportFailureSubstepInput(700, TEXT("pipeline-consistency"));

			const FPhysAnimRuntimeTerminationPipelineResult Result_G =
				PhysAnimRuntimeTerminationPipeline::EvaluateTerminationPipeline(Input_G);

			TestEqual(
				TEXT("TERMINATION-PIPELINE-07 command reason matches substep reason"),
				static_cast<uint8>(Result_G.Command.TerminalReason),
				static_cast<uint8>(Result_G.SubstepResult.TerminalReason));
			TestEqual(TEXT("TERMINATION-PIPELINE-07 command timestamp matches substep timestamp"), Result_G.Command.TerminalSubstepTimestamp, Result_G.SubstepResult.TerminalSubstepTimestamp);
			TestEqual(
				TEXT("TERMINATION-PIPELINE-07 state reason matches command reason"),
				static_cast<uint8>(Result_G.StateApplyResult.State.TerminalReason),
				static_cast<uint8>(Result_G.Command.TerminalReason));
			TestEqual(TEXT("TERMINATION-PIPELINE-07 state timestamp matches command timestamp"), Result_G.StateApplyResult.State.TerminalSubstepTimestamp, Result_G.Command.TerminalSubstepTimestamp);
		}

		{
			FPhysAnimRuntimeTerminationPipelineInput Input_H;
			Input_H.SubstepInput = RuntimeTerminationPipeline_MakePlantFailureSubstepInput(800, TEXT("pipeline-plant-failure"));

			const FPhysAnimRuntimeTerminationPipelineResult Result_H =
				PhysAnimRuntimeTerminationPipeline::EvaluateTerminationPipeline(Input_H);

			TestTrue(TEXT("TERMINATION-PIPELINE-08 plant failure terminates command"), Result_H.Command.bTerminate);
			TestTrue(TEXT("TERMINATION-PIPELINE-08 plant failure terminates state"), Result_H.StateApplyResult.State.bTerminated);
			TestEqual(
				TEXT("TERMINATION-PIPELINE-08 terminal reason is plant contract violation"),
				static_cast<uint8>(Result_H.StateApplyResult.State.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationPhysicsAssetContractViolation));
			TestFalse(TEXT("TERMINATION-PIPELINE-08 plant contract invalid field copied"), Result_H.StateApplyResult.State.TerminalArtifact.bPhysicsAssetContractValid);
			TestFalse(TEXT("TERMINATION-PIPELINE-08 skeleton audit copied"), Result_H.StateApplyResult.State.TerminalArtifact.bSkeletonAuditPassed);
			TestEqual(TEXT("TERMINATION-PIPELINE-08 plant terminal timestamp copied"), Result_H.StateApplyResult.State.TerminalSubstepTimestamp, static_cast<int64>(800));
		}

		{
			FPhysAnimRuntimeTerminationPipelineInput Input_I;
			Input_I.SubstepInput = RuntimeTerminationPipeline_MakeEarlierAdditionalFailureSubstepInput(
				900,
				899,
				TEXT("pipeline-earlier-additional"));

			const FPhysAnimRuntimeTerminationPipelineResult Result_I =
				PhysAnimRuntimeTerminationPipeline::EvaluateTerminationPipeline(Input_I);

			TestTrue(TEXT("TERMINATION-PIPELINE-09 earlier additional failure terminates command"), Result_I.Command.bTerminate);
			TestEqual(
				TEXT("TERMINATION-PIPELINE-09 earlier authority wins by temporal precedence"),
				static_cast<uint8>(Result_I.Command.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationAuthorityConflict));
			TestEqual(TEXT("TERMINATION-PIPELINE-09 earlier terminal timestamp copied"), Result_I.Command.TerminalSubstepTimestamp, static_cast<int64>(899));
		}

		{
			constexpr int64 ProofFailureTimestamp = 901;
			const FPhysAnimRuntimeProofFailureFailStopRoutingInput Input_J =
				RuntimeTerminationPipeline_MakeProofFailureRoutingInput(
					ProofFailureTimestamp,
					TEXT("pipeline-proof-fail-stop"),
					EPhysAnimTerminalReason::ActivationSupportFailure);

			const FPhysAnimRuntimeTerminationPipelineResult Result_J =
				PhysAnimRuntimeTerminationPipeline::EvaluateProofFailureFailStopRouting(Input_J);

			TestTrue(TEXT("TERMINATION-PIPELINE-10 proof failure command terminates"), Result_J.Command.bTerminate);
			TestTrue(TEXT("TERMINATION-PIPELINE-10 proof failure command requests physics fail-stop"), Result_J.Command.bRequestPhysicsFailStop);
			TestTrue(TEXT("TERMINATION-PIPELINE-10 proof failure state remains terminated"), Result_J.StateApplyResult.State.bTerminated);
			TestTrue(TEXT("TERMINATION-PIPELINE-10 proof failure route does not reapply terminal state"), Result_J.StateApplyResult.bIgnoredBecauseAlreadyTerminated);
			TestEqual(
				TEXT("TERMINATION-PIPELINE-10 proof failure terminal reason preserved"),
				static_cast<uint8>(Result_J.StateApplyResult.State.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationSupportFailure));
			TestEqual(TEXT("TERMINATION-PIPELINE-10 proof failure terminal timestamp preserved"), Result_J.StateApplyResult.State.TerminalSubstepTimestamp, ProofFailureTimestamp);
		}

		return true;
	}
}
