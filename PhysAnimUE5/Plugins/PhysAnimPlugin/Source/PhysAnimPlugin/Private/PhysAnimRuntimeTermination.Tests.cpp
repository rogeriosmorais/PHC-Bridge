#include "PhysAnimRuntimeTermination.h"
#include "Misc/AutomationTest.h"

namespace
{
	FPhysAnimRuntimeSubstepResult RuntimeTermination_MakeCleanSubstepResult()
	{
		FPhysAnimRuntimeSubstepResult Result;
		Result.Artifact.AttemptUuid = TEXT("runtime-termination-clean");
		Result.Artifact.TerminalReason = EPhysAnimTerminalReason::None;
		Result.Artifact.TerminalSubstepTimestamp = 100;
		Result.Artifact.ActiveSupportSideCount = 1;
		Result.Artifact.SupportHullAreaCm2 = 120.0;
		Result.TerminalReason = EPhysAnimTerminalReason::None;
		Result.TerminalSubstepTimestamp = 100;
		Result.bShouldTerminate = false;
		Result.bTerminalFrameArtifactCaptured = false;
		return Result;
	}

	FPhysAnimRuntimeSubstepResult RuntimeTermination_MakeTerminalSubstepResult()
	{
		FPhysAnimRuntimeSubstepResult Result;
		Result.Artifact.AttemptUuid = TEXT("runtime-termination-terminal");
		Result.Artifact.TerminalReason = EPhysAnimTerminalReason::ActivationSupportFailure;
		Result.Artifact.TerminalSubstepTimestamp = 200;
		Result.Artifact.ActiveSupportSideCount = 0;
		Result.Artifact.SupportHullAreaCm2 = 0.0;
		Result.Artifact.SupportGapTimerMs = 120.0;
		Result.Artifact.bTerminalFrameArtifactCaptured = true;
		Result.TerminalReason = EPhysAnimTerminalReason::ActivationSupportFailure;
		Result.TerminalSubstepTimestamp = 200;
		Result.bShouldTerminate = true;
		Result.bTerminalFrameArtifactCaptured = true;
		return Result;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimRuntimeTerminationCommandTest,
		"PhysAnim.RuntimeTermination.Command",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimRuntimeTerminationCommandTest::RunTest(const FString& Parameters)
	{
		{
			FPhysAnimRuntimeTerminationCommandInput Input;
			Input.SubstepResult = RuntimeTermination_MakeCleanSubstepResult();

			const FPhysAnimRuntimeTerminationCommand Command = PhysAnimRuntimeTermination::BuildTerminationCommand(Input);

			TestFalse(TEXT("TERMINATION-COMMAND-01 clean result does not terminate"), Command.bTerminate);
			TestFalse(TEXT("TERMINATION-COMMAND-01 clean result does not capture terminal artifact"), Command.bCaptureTerminalArtifact);
			TestFalse(TEXT("TERMINATION-COMMAND-01 clean result does not disable policy"), Command.bDisablePolicyEvaluation);
			TestFalse(TEXT("TERMINATION-COMMAND-01 clean result does not freeze bridge output"), Command.bFreezeBridgeOutput);
			TestEqual(
				TEXT("TERMINATION-COMMAND-01 terminal reason none"),
				static_cast<uint8>(Command.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::None));
		}

		{
			FPhysAnimRuntimeTerminationCommandInput Input;
			Input.SubstepResult = RuntimeTermination_MakeTerminalSubstepResult();

			const FPhysAnimRuntimeTerminationCommand Command = PhysAnimRuntimeTermination::BuildTerminationCommand(Input);

			TestTrue(TEXT("TERMINATION-COMMAND-02 terminal result terminates"), Command.bTerminate);
			TestTrue(TEXT("TERMINATION-COMMAND-02 terminal artifact captured"), Command.bCaptureTerminalArtifact);
			TestEqual(
				TEXT("TERMINATION-COMMAND-02 terminal reason copied"),
				static_cast<uint8>(Command.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationSupportFailure));
			TestEqual(TEXT("TERMINATION-COMMAND-02 terminal timestamp copied"), Command.TerminalSubstepTimestamp, static_cast<int64>(200));
			TestTrue(TEXT("TERMINATION-COMMAND-02 artifact terminal flag copied"), Command.Artifact.bTerminalFrameArtifactCaptured);
		}

		{
			FPhysAnimRuntimeTerminationCommandInput Input;
			Input.SubstepResult = RuntimeTermination_MakeTerminalSubstepResult();
			Input.bEnableTerminationCommand = false;

			const FPhysAnimRuntimeTerminationCommand Command = PhysAnimRuntimeTermination::BuildTerminationCommand(Input);

			TestFalse(TEXT("TERMINATION-COMMAND-03 disabled command does not terminate"), Command.bTerminate);
			TestFalse(TEXT("TERMINATION-COMMAND-03 disabled command does not capture terminal artifact"), Command.bCaptureTerminalArtifact);
			TestFalse(TEXT("TERMINATION-COMMAND-03 disabled command does not disable policy"), Command.bDisablePolicyEvaluation);
			TestFalse(TEXT("TERMINATION-COMMAND-03 disabled command does not freeze bridge output"), Command.bFreezeBridgeOutput);
			TestEqual(
				TEXT("TERMINATION-COMMAND-03 disabled command still preserves artifact terminal reason"),
				static_cast<uint8>(Command.Artifact.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationSupportFailure));
			TestFalse(TEXT("TERMINATION-COMMAND-03 disabled command clears artifact terminal capture flag"), Command.Artifact.bTerminalFrameArtifactCaptured);
		}

		{
			FPhysAnimRuntimeTerminationCommandInput Input;
			Input.SubstepResult = RuntimeTermination_MakeTerminalSubstepResult();

			const FPhysAnimRuntimeTerminationCommand Command = PhysAnimRuntimeTermination::BuildTerminationCommand(Input);

			TestTrue(TEXT("TERMINATION-COMMAND-04 terminal command disables policy"), Command.bDisablePolicyEvaluation);
			TestTrue(TEXT("TERMINATION-COMMAND-04 terminal command freezes bridge output"), Command.bFreezeBridgeOutput);
		}

		{
			FPhysAnimRuntimeTerminationCommandInput Input;
			Input.SubstepResult = RuntimeTermination_MakeTerminalSubstepResult();

			const FPhysAnimRuntimeTerminationCommand Command = PhysAnimRuntimeTermination::BuildTerminationCommand(Input);

			TestTrue(TEXT("TERMINATION-COMMAND-05 terminal command requests physics fail-stop"), Command.bRequestPhysicsFailStop);
		}

		{
			FPhysAnimRuntimeTerminationCommandInput Input_A;
			Input_A.SubstepResult = RuntimeTermination_MakeTerminalSubstepResult();
			Input_A.bAllowMovementReclaimOnTermination = true;

			const FPhysAnimRuntimeTerminationCommand Command_A = PhysAnimRuntimeTermination::BuildTerminationCommand(Input_A);

			TestTrue(TEXT("TERMINATION-COMMAND-06 movement reclaim requested when allowed"), Command_A.bRequestMovementReclaim);

			FPhysAnimRuntimeTerminationCommandInput Input_B;
			Input_B.SubstepResult = RuntimeTermination_MakeTerminalSubstepResult();
			Input_B.bAllowMovementReclaimOnTermination = false;

			const FPhysAnimRuntimeTerminationCommand Command_B = PhysAnimRuntimeTermination::BuildTerminationCommand(Input_B);

			TestFalse(TEXT("TERMINATION-COMMAND-06 movement reclaim suppressed when disallowed"), Command_B.bRequestMovementReclaim);
		}

		{
			FPhysAnimRuntimeTerminationCommandInput Input;
			Input.SubstepResult = RuntimeTermination_MakeTerminalSubstepResult();

			const FPhysAnimRuntimeTerminationCommand Command = PhysAnimRuntimeTermination::BuildTerminationCommand(Input);

			TestEqual(TEXT("TERMINATION-COMMAND-07 artifact attempt uuid preserved"), Command.Artifact.AttemptUuid, FString(TEXT("runtime-termination-terminal")));
			TestEqual(TEXT("TERMINATION-COMMAND-07 artifact support side count preserved"), Command.Artifact.ActiveSupportSideCount, 0);
			TestEqual(TEXT("TERMINATION-COMMAND-07 artifact support gap preserved"), Command.Artifact.SupportGapTimerMs, 120.0);
			TestEqual(TEXT("TERMINATION-COMMAND-07 artifact timestamp preserved"), Command.Artifact.TerminalSubstepTimestamp, static_cast<int64>(200));
		}

		return true;
	}
}
