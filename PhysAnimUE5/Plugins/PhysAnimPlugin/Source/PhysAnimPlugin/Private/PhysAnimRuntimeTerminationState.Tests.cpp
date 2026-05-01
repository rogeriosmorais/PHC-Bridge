#include "PhysAnimRuntimeTerminationState.h"
#include "Misc/AutomationTest.h"

namespace
{
	FPhysAnimRuntimeTerminationCommand RuntimeTerminationState_MakeCleanCommand()
	{
		FPhysAnimRuntimeTerminationCommand Command;
		Command.bTerminate = false;
		Command.TerminalReason = EPhysAnimTerminalReason::None;
		Command.TerminalSubstepTimestamp = 100;
		Command.Artifact.AttemptUuid = TEXT("runtime-termination-state-clean");
		Command.Artifact.TerminalReason = EPhysAnimTerminalReason::None;
		Command.Artifact.TerminalSubstepTimestamp = 100;
		Command.Artifact.ActiveSupportSideCount = 1;
		Command.Artifact.SupportHullAreaCm2 = 120.0;
		return Command;
	}

	FPhysAnimRuntimeTerminationCommand RuntimeTerminationState_MakeTerminalCommand(
		const EPhysAnimTerminalReason Reason = EPhysAnimTerminalReason::ActivationSupportFailure,
		const int64 Timestamp = 200,
		const TCHAR* AttemptUuid = TEXT("runtime-termination-state-terminal"))
	{
		FPhysAnimRuntimeTerminationCommand Command;
		Command.bTerminate = true;
		Command.TerminalReason = Reason;
		Command.TerminalSubstepTimestamp = Timestamp;
		Command.bCaptureTerminalArtifact = true;
		Command.bDisablePolicyEvaluation = true;
		Command.bFreezeBridgeOutput = true;
		Command.bRequestPhysicsFailStop = true;
		Command.bRequestMovementReclaim = true;
		Command.Artifact.AttemptUuid = AttemptUuid;
		Command.Artifact.TerminalReason = Reason;
		Command.Artifact.TerminalSubstepTimestamp = Timestamp;
		Command.Artifact.ActiveSupportSideCount = 0;
		Command.Artifact.SupportHullAreaCm2 = 0.0;
		Command.Artifact.SupportGapTimerMs = 120.0;
		Command.Artifact.bTerminalFrameArtifactCaptured = true;
		return Command;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimRuntimeTerminationStateTest,
		"PhysAnim.RuntimeTermination.State",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimRuntimeTerminationStateTest::RunTest(const FString& Parameters)
	{
		{
			FPhysAnimRuntimeTerminationStateApplyInput Input_A;
			Input_A.Command = RuntimeTerminationState_MakeCleanCommand();

			const FPhysAnimRuntimeTerminationStateApplyResult Result_A =
				PhysAnimRuntimeTerminationState::ApplyTerminationCommand(Input_A);

			TestFalse(TEXT("TERMINATION-STATE-01 clean command does not terminate"), Result_A.State.bTerminated);
			TestFalse(TEXT("TERMINATION-STATE-01 clean command not applied as termination"), Result_A.bAppliedTermination);
			TestTrue(TEXT("TERMINATION-STATE-01 policy remains enabled"), Result_A.State.bPolicyEvaluationEnabled);
			TestFalse(TEXT("TERMINATION-STATE-01 bridge output not frozen"), Result_A.State.bBridgeOutputFrozen);
			TestFalse(TEXT("TERMINATION-STATE-01 no deferred startup proxy reason"), Result_A.State.bHasDeferredStartupProxyTerminalReason);
			TestEqual(TEXT("TERMINATION-STATE-01 latest artifact copied"), Result_A.State.LatestArtifact.AttemptUuid, FString(TEXT("runtime-termination-state-clean")));
		}

		{
			FPhysAnimRuntimeTerminationStateApplyInput Input_B;
			Input_B.Command = RuntimeTerminationState_MakeTerminalCommand();

			const FPhysAnimRuntimeTerminationStateApplyResult Result_B =
				PhysAnimRuntimeTerminationState::ApplyTerminationCommand(Input_B);

			TestTrue(TEXT("TERMINATION-STATE-02 terminal command terminates"), Result_B.State.bTerminated);
			TestTrue(TEXT("TERMINATION-STATE-02 terminal command applied"), Result_B.bAppliedTermination);
			TestEqual(
				TEXT("TERMINATION-STATE-02 terminal reason copied"),
				static_cast<uint8>(Result_B.State.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationSupportFailure));
			TestEqual(TEXT("TERMINATION-STATE-02 terminal timestamp copied"), Result_B.State.TerminalSubstepTimestamp, static_cast<int64>(200));
			TestTrue(TEXT("TERMINATION-STATE-02 terminal frame captured"), Result_B.State.bTerminalFrameArtifactCaptured);
			TestFalse(TEXT("TERMINATION-STATE-02 no deferred startup proxy reason"), Result_B.State.bHasDeferredStartupProxyTerminalReason);
		}

		{
			FPhysAnimRuntimeTerminationStateApplyInput Input_C;
			Input_C.Command = RuntimeTerminationState_MakeTerminalCommand();

			const FPhysAnimRuntimeTerminationStateApplyResult Result_C =
				PhysAnimRuntimeTerminationState::ApplyTerminationCommand(Input_C);

			TestFalse(TEXT("TERMINATION-STATE-03 terminal command disables policy"), Result_C.State.bPolicyEvaluationEnabled);
		}

		{
			FPhysAnimRuntimeTerminationStateApplyInput Input_D;
			Input_D.Command = RuntimeTerminationState_MakeTerminalCommand();

			const FPhysAnimRuntimeTerminationStateApplyResult Result_D =
				PhysAnimRuntimeTerminationState::ApplyTerminationCommand(Input_D);

			TestTrue(TEXT("TERMINATION-STATE-04 terminal command freezes bridge output"), Result_D.State.bBridgeOutputFrozen);
		}

		{
			FPhysAnimRuntimeTerminationStateApplyInput Input_E;
			Input_E.Command = RuntimeTerminationState_MakeTerminalCommand();

			const FPhysAnimRuntimeTerminationStateApplyResult Result_E =
				PhysAnimRuntimeTerminationState::ApplyTerminationCommand(Input_E);

			TestTrue(TEXT("TERMINATION-STATE-05 terminal command requests physics fail-stop"), Result_E.State.bPhysicsFailStopRequested);
		}

		{
			FPhysAnimRuntimeTerminationCommand Command_F = RuntimeTerminationState_MakeTerminalCommand();
			Command_F.bRequestMovementReclaim = true;

			FPhysAnimRuntimeTerminationStateApplyInput Input_F;
			Input_F.Command = Command_F;

			const FPhysAnimRuntimeTerminationStateApplyResult Result_F =
				PhysAnimRuntimeTerminationState::ApplyTerminationCommand(Input_F);

			TestTrue(TEXT("TERMINATION-STATE-06 movement reclaim follows true command flag"), Result_F.State.bMovementReclaimRequested);

			FPhysAnimRuntimeTerminationCommand Command_G = RuntimeTerminationState_MakeTerminalCommand();
			Command_G.bRequestMovementReclaim = false;

			FPhysAnimRuntimeTerminationStateApplyInput Input_G;
			Input_G.Command = Command_G;

			const FPhysAnimRuntimeTerminationStateApplyResult Result_G =
				PhysAnimRuntimeTerminationState::ApplyTerminationCommand(Input_G);

			TestFalse(TEXT("TERMINATION-STATE-06 movement reclaim follows false command flag"), Result_G.State.bMovementReclaimRequested);
		}

		{
			FPhysAnimRuntimeTerminationStateApplyInput FirstInput_H;
			FirstInput_H.Command = RuntimeTerminationState_MakeTerminalCommand(
				EPhysAnimTerminalReason::ActivationSupportFailure,
				300,
				TEXT("first-terminal"));

			const FPhysAnimRuntimeTerminationStateApplyResult FirstResult_H =
				PhysAnimRuntimeTerminationState::ApplyTerminationCommand(FirstInput_H);

			FPhysAnimRuntimeTerminationStateApplyInput SecondInput_H;
			SecondInput_H.PreviousState = FirstResult_H.State;
			SecondInput_H.Command = RuntimeTerminationState_MakeTerminalCommand(
				EPhysAnimTerminalReason::ActivationAuthorityConflict,
				301,
				TEXT("second-terminal"));

			const FPhysAnimRuntimeTerminationStateApplyResult SecondResult_H =
				PhysAnimRuntimeTerminationState::ApplyTerminationCommand(SecondInput_H);

			TestTrue(TEXT("TERMINATION-STATE-07 later terminal ignored after already terminated"), SecondResult_H.bIgnoredBecauseAlreadyTerminated);
			TestEqual(
				TEXT("TERMINATION-STATE-07 first terminal reason preserved"),
				static_cast<uint8>(SecondResult_H.State.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationSupportFailure));
			TestEqual(TEXT("TERMINATION-STATE-07 first terminal timestamp preserved"), SecondResult_H.State.TerminalSubstepTimestamp, static_cast<int64>(300));
			TestEqual(TEXT("TERMINATION-STATE-07 first terminal artifact preserved"), SecondResult_H.State.TerminalArtifact.AttemptUuid, FString(TEXT("first-terminal")));
		}

		{
			FPhysAnimRuntimeTerminationStateApplyInput Input_I;
			Input_I.Command = RuntimeTerminationState_MakeCleanCommand();

			const FPhysAnimRuntimeTerminationStateApplyResult Result_I =
				PhysAnimRuntimeTerminationState::ApplyTerminationCommand(Input_I);

			TestEqual(TEXT("TERMINATION-STATE-08 latest non-terminal artifact retained"), Result_I.State.LatestArtifact.AttemptUuid, FString(TEXT("runtime-termination-state-clean")));
			TestFalse(TEXT("TERMINATION-STATE-08 no terminal artifact captured"), Result_I.State.bTerminalFrameArtifactCaptured);
		}

		{
			FPhysAnimRuntimeTerminationStateApplyInput Input_J;
			Input_J.PreviousState.bHasDeferredStartupProxyTerminalReason = true;
			Input_J.PreviousState.DeferredStartupProxyTerminalReason = EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion;
			Input_J.PreviousState.DeferredStartupProxyTerminalAttemptUuid = TEXT("deferred-startup-proxy");
			Input_J.PreviousState.DeferredStartupProxyTerminalSubstepTimestamp = 1234;
			Input_J.Command = RuntimeTerminationState_MakeCleanCommand();

			const FPhysAnimRuntimeTerminationStateApplyResult Result_J =
				PhysAnimRuntimeTerminationState::ApplyTerminationCommand(Input_J);

			TestTrue(TEXT("TERMINATION-STATE-09 deferred startup proxy reason preserved"), Result_J.State.bHasDeferredStartupProxyTerminalReason);
			TestEqual(
				TEXT("TERMINATION-STATE-09 deferred startup proxy reason copied"),
				static_cast<uint8>(Result_J.State.DeferredStartupProxyTerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion));
			TestEqual(TEXT("TERMINATION-STATE-09 deferred startup proxy attempt copied"), Result_J.State.DeferredStartupProxyTerminalAttemptUuid, FString(TEXT("deferred-startup-proxy")));
			TestEqual(TEXT("TERMINATION-STATE-09 deferred startup proxy substep copied"), Result_J.State.DeferredStartupProxyTerminalSubstepTimestamp, static_cast<int64>(1234));
		}

		{
			FPhysAnimRuntimeTerminationStateApplyInput Input_K;
			Input_K.Command = RuntimeTerminationState_MakeTerminalCommand(
				EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion,
				400,
				TEXT("terminal-artifact-preserved"));

			const FPhysAnimRuntimeTerminationStateApplyResult Result_K =
				PhysAnimRuntimeTerminationState::ApplyTerminationCommand(Input_K);

			TestEqual(TEXT("TERMINATION-STATE-10 terminal artifact uuid preserved"), Result_K.State.TerminalArtifact.AttemptUuid, FString(TEXT("terminal-artifact-preserved")));
			TestEqual(
				TEXT("TERMINATION-STATE-10 terminal artifact reason preserved"),
				static_cast<uint8>(Result_K.State.TerminalArtifact.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion));
			TestEqual(TEXT("TERMINATION-STATE-10 terminal artifact timestamp preserved"), Result_K.State.TerminalArtifact.TerminalSubstepTimestamp, static_cast<int64>(400));
		}

		return true;
	}
}
