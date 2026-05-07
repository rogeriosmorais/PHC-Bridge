#include "PhysAnimComponent.h"
#include "PhysAnimBalanceQuietHandoff.h"
#include "PhysAnimBalance.TestHelpers.h"
#if !UE_BUILD_SHIPPING
#include "PhysAnimPhase1AutoCalibSubsystem.h"
#endif
#include "PhysAnimComparisonSubsystem.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/UObjectIterator.h"
#include "Editor.h"
#include "EngineUtils.h"

namespace
{
	using namespace PhysAnimBridge;
	using namespace PhysAnimBalanceTestHelpers;

	const FString PhysAnimPieSmokeMap = TEXT("/Game/ThirdPerson/Lvl_ThirdPerson");

	const TCHAR* PhysAnimPieSmokePrefix = TEXT("[PhysAnimPieMapOpenSmoke]");
	const TCHAR* PhysAnimPieMovementSmokePrefix = TEXT("[PhysAnimPieMovementSmoke]");
	const TCHAR* PhysAnimPieMovementTraceSmokePrefix = TEXT("[PhysAnimPieMovementTraceSmoke]");
	const TCHAR* PhysAnimPieMovementSoakPrefix = TEXT("[PhysAnimPieMovementSoak]");
	const TCHAR* PhysAnimPieStandingProofPresentationPrefix = TEXT("[PhysAnimPieStandingProofPresentation]");
	const TCHAR* PhysAnimPiePhase1AutoCalibSmokePrefix = TEXT("[PhysAnimPiePhase1AutoCalibSmoke]");

	constexpr float PhysAnimPieSmokeStartTimeoutSeconds = 30.0f;
	constexpr float PhysAnimPieSmokeStopTimeoutSeconds = 30.0f;
	constexpr float PhysAnimPieSmokeDurationSeconds = 2.0f;
	constexpr float PhysAnimPieMovementSmokeDurationSeconds = 5.0f;
	constexpr float PhysAnimPieMovementSoakDurationSeconds = 30.0f;
	constexpr int32 PhysAnimPieMovementSoakLoopCount = 5;
	constexpr float PhysAnimPieStandingProofPresentationLeadInSeconds = 1.0f;
	constexpr float PhysAnimPieStandingProofPresentationDurationSeconds = 30.0f;
	constexpr float PhysAnimPieBalanceModeSmokeLeadInSeconds = 1.0f;
	constexpr float PhysAnimPieBalanceModeSmokeDurationSeconds = 15.0f;
	constexpr float PhysAnimPiePhase1AutoCalibBridgeActiveTimeoutSeconds = 15.0f;
	constexpr float PhysAnimPiePhase1AutoCalibTimeoutSeconds = 90.0f;

	UPhysAnimComponent* FindFirstPhysAnimComponent(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}

		for (FActorIterator It(World); It; ++It)
		{
			if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
			{
				return Comp;
			}
		}

		return nullptr;
	}

	DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FSetIntConsoleVariableCommand, FString, Name, int32, Value);
	bool FSetIntConsoleVariableCommand::Update()
	{
		if (IConsoleVariable* const ConsoleVariable = IConsoleManager::Get().FindConsoleVariable(*Name))
		{
			ConsoleVariable->Set(Value);
		}
		return true;
	}

	DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FExecPieConsoleCommand, FString, Command);
	bool FExecPieConsoleCommand::Update()
	{
		if (GEditor && GEditor->PlayWorld)
		{
			GEditor->PlayWorld->Exec(GEditor->PlayWorld, *Command);
		}
		return true;
	}

	DEFINE_LATENT_AUTOMATION_COMMAND(FStartStandingProofPresentationCommand);
	bool FStartStandingProofPresentationCommand::Update()
	{
		if (GEditor && GEditor->PlayWorld)
		{
			if (UPhysAnimComparisonSubsystem* Subsystem = GEditor->PlayWorld->GetSubsystem<UPhysAnimComparisonSubsystem>())
			{
				FString Error;
				Subsystem->StartPresentation(Error);
			}
		}
		return true;
	}

#if !UE_BUILD_SHIPPING
	DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FStartPhase1AutoCalibSmokeCommand, FAutomationTestBase*, Test);
	bool FStartPhase1AutoCalibSmokeCommand::Update()
	{
		if (!Test || !GEditor || !GEditor->PlayWorld)
		{
			return true;
		}

		UWorld* const World = GEditor->PlayWorld;
		UPhysAnimPhase1AutoCalibSubsystem* const AutoCalibSubsystem = World->GetSubsystem<UPhysAnimPhase1AutoCalibSubsystem>();
		Test->TestNotNull(TEXT("Phase1 auto-calibration subsystem exists before start"), AutoCalibSubsystem);
		if (!AutoCalibSubsystem)
		{
			return true;
		}

		UPhysAnimComponent* const TargetComponent = FindFirstPhysAnimComponent(World);

		Test->TestNotNull(TEXT("Phase1 auto-calibration smoke finds a candidate component"), TargetComponent);
		if (!TargetComponent || !TargetComponent->GetOwner())
		{
			return true;
		}

		FString Error;
		FPhase1AutoCalibRequest Request;
		Request.OwnerFilter = TargetComponent->GetOwner()->GetName();
		Request.Seed = 1337;
		Request.BudgetMode = EPhase1AutoCalibBudgetMode::Smoke;
		Request.OutputSubfolder = TEXT("automation_phase1_smoke");

		const bool bStarted = AutoCalibSubsystem->StartPhase1AutoCalib(Request, Error);
		Test->TestTrue(TEXT("Phase1 auto-calibration smoke starts successfully"), bStarted);
		if (!bStarted)
		{
			const FString FailureText = !Error.IsEmpty() ? Error : AutoCalibSubsystem->GetLastError();
			if (!FailureText.IsEmpty())
			{
				Test->AddError(FString::Printf(TEXT("%s Start failed: %s"), PhysAnimPiePhase1AutoCalibSmokePrefix, *FailureText));
			}
		}
		Test->TestTrue(TEXT("Phase1 auto-calibration smoke start reports no error"), Error.IsEmpty());
		return true;
	}

	DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FValidatePhase1AutoCalibSmokeArtifactsCommand, FAutomationTestBase*, Test);
	bool FValidatePhase1AutoCalibSmokeArtifactsCommand::Update()
	{
		if (!Test || !GEditor || !GEditor->PlayWorld)
		{
			return true;
		}

		UWorld* const World = GEditor->PlayWorld;
		UPhysAnimPhase1AutoCalibSubsystem* const AutoCalibSubsystem = World->GetSubsystem<UPhysAnimPhase1AutoCalibSubsystem>();
		Test->TestNotNull(TEXT("Phase1 auto-calibration subsystem exists"), AutoCalibSubsystem);
		if (!AutoCalibSubsystem)
		{
			return true;
		}

		const FPhase1AutoCalibReport& Report = AutoCalibSubsystem->GetLatestReport();
		Test->TestTrue(TEXT("Phase1 auto-calibration recorded at least two trials"), Report.Trials.Num() >= 2);
		if (Report.Trials.Num() == 0)
		{
			Test->AddError(FString::Printf(TEXT("Phase1 auto-calibration failed to capture any trials. Subsystem reached timeout while awaiting readiness. LastReason=%s"), *AutoCalibSubsystem->GetLastError()));
			return true;
		}

		Test->TestTrue(TEXT("Phase1 auto-calibration smoke emits preset-aware summaries"), Report.PresetSummaries.Num() > 0);
		Test->TestTrue(TEXT("Phase1 auto-calibration smoke writes summary.json"), IFileManager::Get().FileExists(*Report.SummaryPath));
		Test->TestTrue(TEXT("Phase1 auto-calibration smoke writes trials.csv"), IFileManager::Get().FileExists(*Report.TrialsCsvPath));
		Test->TestTrue(TEXT("Phase1 auto-calibration smoke writes pareto.json"), IFileManager::Get().FileExists(*Report.ParetoJsonPath));
		Test->TestTrue(TEXT("Phase1 auto-calibration smoke records timeout budget telemetry"), Report.Trials[0].TrialTimeoutBudgetSeconds > 0.0f);
		Test->TestTrue(TEXT("Phase1 auto-calibration smoke publishes a standing hold benchmark"), Report.RequiredBalanceActiveStandingHoldSeconds > 0.0f);
		if (Report.bHasBestCandidate)
		{
			Test->TestTrue(
				TEXT("Any reported best candidate satisfies the standing hold benchmark"),
				Report.BestCandidate.Score.BalanceActiveStandingHoldSeconds >= Report.RequiredBalanceActiveStandingHoldSeconds);
		}

		FString SummaryJson;
		Test->TestTrue(
			TEXT("Phase1 auto-calibration smoke can read summary.json"),
			FFileHelper::LoadFileToString(SummaryJson, *Report.SummaryPath));
		if (!SummaryJson.IsEmpty())
		{
			Test->TestTrue(
				TEXT("summary.json emits the standing hold benchmark field"),
				SummaryJson.Contains(TEXT("\"requiredBalanceActiveStandingHoldSeconds\":")));
			Test->TestTrue(
				TEXT("summary.json emits per-trial standing timing"),
				SummaryJson.Contains(TEXT("\"timeToBalanceActiveStandingSeconds\":")));
			Test->TestTrue(
				TEXT("summary.json emits per-trial standing reach flags"),
				SummaryJson.Contains(TEXT("\"reachedBalanceActiveStanding\":")));
			Test->TestTrue(
				TEXT("summary.json emits per-trial standing hold seconds"),
				SummaryJson.Contains(TEXT("\"balanceActiveStandingHoldSeconds\":")));
		}

		FString ParetoJson;
		Test->TestTrue(
			TEXT("Phase1 auto-calibration smoke can read pareto.json"),
			FFileHelper::LoadFileToString(ParetoJson, *Report.ParetoJsonPath));
		if (!ParetoJson.IsEmpty())
		{
			Test->TestTrue(
				TEXT("pareto.json emits per-trial standing timing"),
				ParetoJson.Contains(TEXT("\"timeToBalanceActiveStandingSeconds\":")));
			Test->TestTrue(
				TEXT("pareto.json emits per-trial standing reach flags"),
				ParetoJson.Contains(TEXT("\"reachedBalanceActiveStanding\":")));
			Test->TestTrue(
				TEXT("pareto.json emits per-trial standing hold seconds"),
				ParetoJson.Contains(TEXT("\"balanceActiveStandingHoldSeconds\":")));
		}

		FString TrialsCsv;
		Test->TestTrue(
			TEXT("Phase1 auto-calibration smoke can read trials.csv"),
			FFileHelper::LoadFileToString(TrialsCsv, *Report.TrialsCsvPath));
		if (!TrialsCsv.IsEmpty())
		{
			const int32 HeaderEndIndex = TrialsCsv.Find(TEXT("\n"));
			const FString Header = HeaderEndIndex >= 0 ? TrialsCsv.Left(HeaderEndIndex) : TrialsCsv;
			Test->TestTrue(
				TEXT("trials.csv header emits standing timing"),
				Header.Contains(TEXT("time_to_balance_active_standing_seconds")));
			Test->TestTrue(
				TEXT("trials.csv header emits standing reach flags"),
				Header.Contains(TEXT("reached_balance_active_standing")));
			Test->TestTrue(
				TEXT("trials.csv header emits standing hold seconds"),
				Header.Contains(TEXT("balance_active_standing_hold_seconds")));
		}

		TArray<FPhase1AutoCalibTrialResult> StageCTrials;
		for (const FPhase1AutoCalibTrialResult& Trial : Report.Trials)
		{
			if (Trial.StageName == TEXT("stage_c"))
			{
				StageCTrials.Add(Trial);
			}
		}

		if (StageCTrials.Num() >= 2)
		{
			Test->TestTrue(
				TEXT("Stage C reruns preserve the same terminal class from the restored baseline"),
				StageCTrials[0].TerminalClass == StageCTrials[1].TerminalClass);
			Test->TestTrue(
				TEXT("Stage C reruns preserve the same truthful blocker from the restored baseline"),
				StageCTrials[0].TruthfulBlocker == StageCTrials[1].TruthfulBlocker);
		}

		return true;
	}
#endif

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPieSmokeTest,
		"PhysAnim.PIE.MapOpenSmoke",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimPieSmokeTest::RunTest(const FString& Parameters)
	{
		if (!AutomationOpenMap(PhysAnimPieSmokeMap, true))
		{
			AddError(FString::Printf(TEXT("%s Failed to open map '%s'."), PhysAnimPieSmokePrefix, *PhysAnimPieSmokeMap));
			return false;
		}

		AddCommand(new FStartPIECommand(false));
		AddCommand(new FUntilCommand(
			[]() -> bool { return GEditor != nullptr && IsValid(GEditor->PlayWorld); },
			[this]() -> bool { AddError(TEXT("PIE did not start")); return true; },
			PhysAnimPieSmokeStartTimeoutSeconds));
		AddCommand(new FWaitLatentCommand(PhysAnimPieSmokeDurationSeconds));
		AddCommand(new FEndPlayMapCommand());
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPieMovementSmokeTest,
		"PhysAnim.PIE.MovementSmoke",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimPieMovementSmokeTest::RunTest(const FString& Parameters)
	{
		if (!AutomationOpenMap(PhysAnimPieSmokeMap, true))
		{
			AddError(FString::Printf(TEXT("%s Failed to open map '%s'."), PhysAnimPieMovementSmokePrefix, *PhysAnimPieSmokeMap));
			return false;
		}

		AddCommand(new FSetIntConsoleVariableCommand(TEXT("physanim.MovementSmokeMode"), 1));
		AddCommand(new FStartPIECommand(false));
		AddCommand(new FWaitLatentCommand(PhysAnimPieMovementSmokeDurationSeconds));
		AddCommand(new FEndPlayMapCommand());
		AddCommand(new FSetIntConsoleVariableCommand(TEXT("physanim.MovementSmokeMode"), 0));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPieBalanceModeSmokeTest,
		"PhysAnim.PIE.BalanceModeSmoke",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimPieBalanceModeSmokeTest::RunTest(const FString& Parameters)
	{
		if (!AutomationOpenMap(PhysAnimPieSmokeMap, true))
		{
			AddError(FString::Printf(TEXT("[PhysAnimPieBalanceSmoke] Failed to open map '%s'."), *PhysAnimPieSmokeMap));
			return false;
		}

		AddCommand(new FStartPIECommand(false));
		AddCommand(new FUntilCommand(
			[]() -> bool { return GEditor != nullptr && IsValid(GEditor->PlayWorld); },
			[this]() -> bool { AddError(TEXT("PIE did not start")); return true; },
			PhysAnimPieSmokeStartTimeoutSeconds));
		AddCommand(new FWaitLatentCommand(PhysAnimPieBalanceModeSmokeLeadInSeconds));
		AddCommand(new FWaitLatentCommand(PhysAnimPieBalanceModeSmokeDurationSeconds));
		AddCommand(new FValidateBalanceModeSmokeOutcomeCommand(this));
		AddCommand(new FEndPlayMapCommand());
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPieStandingProofPresentationTest,
		"PhysAnim.PIE.StandingProofPresentation",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimPieStandingProofPresentationTest::RunTest(const FString& Parameters)
	{
		if (!AutomationOpenMap(PhysAnimPieSmokeMap, true))
		{
			AddError(FString::Printf(TEXT("%s Failed to open map '%s'."), PhysAnimPieStandingProofPresentationPrefix, *PhysAnimPieSmokeMap));
			return false;
		}

		AddExpectedError(TEXT("PhysAnimProof: TerminalArtifact"), EAutomationExpectedErrorFlags::Contains, 0);
		AddExpectedError(TEXT("PhysAnimProof: AttemptResult"), EAutomationExpectedErrorFlags::Contains, 0);
		AddExpectedError(TEXT("Fail-stop: Proof failed"), EAutomationExpectedErrorFlags::Contains, 0);

		AddCommand(new FStartPIECommand(false));
		AddCommand(new FWaitLatentCommand(PhysAnimPieStandingProofPresentationLeadInSeconds));
		AddCommand(new FStartStandingProofPresentationCommand());
		AddCommand(new FWaitLatentCommand(PhysAnimPieStandingProofPresentationDurationSeconds));
		AddCommand(new FEndPlayMapCommand());
		return true;
	}
	
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPiePhase1AutoCalibSmokeTest,
		"PhysAnim.PIE.Phase1AutoCalibSmoke",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimPiePhase1AutoCalibSmokeTest::RunTest(const FString& Parameters)
	{
		if (!AutomationOpenMap(PhysAnimPieSmokeMap, true))
		{
			AddError(FString::Printf(TEXT("%s Failed to open map '%s'."), PhysAnimPiePhase1AutoCalibSmokePrefix, *PhysAnimPieSmokeMap));
			return false;
		}

		AddCommand(new FStartPIECommand(false));
		AddCommand(new FUntilCommand(
			[]() -> bool { return GEditor != nullptr && IsValid(GEditor->PlayWorld); },
			[this]() -> bool { AddError(TEXT("PIE did not start")); return true; },
			PhysAnimPieSmokeStartTimeoutSeconds));
		
		AddCommand(new FUntilCommand(
			[]() -> bool
			{
				if (!GEditor || !GEditor->PlayWorld)
				{
					return false;
				}

				if (UPhysAnimComponent* const Component = FindFirstPhysAnimComponent(GEditor->PlayWorld))
				{
					return Component->GetRuntimeState() == EPhysAnimRuntimeState::BridgeActive;
				}

				return false;
			},
			[this]() -> bool { AddError(TEXT("Phase1 auto-calibration smoke did not reach BridgeActive")); return true; },
			PhysAnimPiePhase1AutoCalibBridgeActiveTimeoutSeconds));
		
		AddCommand(new FStartPhase1AutoCalibSmokeCommand(this));
		
		// Wait for completion or timeout.
		AddCommand(new FUntilCommand(
			[this]() -> bool 
			{ 
				if (!GEditor || !GEditor->PlayWorld) return true;
				UPhysAnimPhase1AutoCalibSubsystem* Subsystem = GEditor->PlayWorld->GetSubsystem<UPhysAnimPhase1AutoCalibSubsystem>();
				return Subsystem && !Subsystem->IsPhase1AutoCalibActive();
			},
			[this]() -> bool { AddError(TEXT("Phase1 auto-calibration smoke timed out")); return true; },
			PhysAnimPiePhase1AutoCalibTimeoutSeconds));

		AddCommand(new FValidatePhase1AutoCalibSmokeArtifactsCommand(this));
		AddCommand(new FEndPlayMapCommand());
		return true;
	}

}
