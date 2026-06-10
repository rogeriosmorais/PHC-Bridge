#include "PhysAnimEvidenceCollector.h"
#include "PhysAnimProofArtifactEmitter.h"

#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	struct FPhysAnimEvidenceCollectorFixturePaths
	{
		FString BaseDir;
		FString TerminalDir;
		FString SummaryDir;
		FString LogDir;
	};

	FPhysAnimEvidenceSummarySegment MakeSegment(
		const FString& SegmentName,
		const EPhysAnimEvidenceBaselineSegmentState State)
	{
		FPhysAnimEvidenceSummarySegment Segment;
		Segment.SegmentName = SegmentName;
		Segment.State = State;
		Segment.Metrics.SampleCount = 1;
		Segment.Metrics.Confidence = State == EPhysAnimEvidenceBaselineSegmentState::Active ? 1.0 : 0.5;
		Segment.Metrics.Score = State == EPhysAnimEvidenceBaselineSegmentState::Active ? 1.0 : 0.0;
		return Segment;
	}

	FPhysAnimEvidenceSummary MakeSummary(
		const FString& AttemptUuid,
		const EPhysAnimEvidenceBaselineVerdict StrictVerdict,
		const EPhysAnimTerminalReason TerminalReason,
		const EPhysAnimEvidenceBaselineSegmentState PoseSearch,
		const EPhysAnimEvidenceBaselineSegmentState PhcPolicy,
		const EPhysAnimEvidenceBaselineSegmentState PhysicsControl,
		const EPhysAnimEvidenceBaselineSegmentState Chaos,
		const EPhysAnimEvidenceBaselineSegmentState RendererFacingMotion)
	{
		FPhysAnimEvidenceSummary Summary;
		Summary.AttemptUuid = AttemptUuid;
		Summary.TestName = TEXT("PhysAnim.EvidenceCollector.Contract");
		Summary.MapName = TEXT("PhysAnim_TestMap");
		Summary.Timestamp = 2.0;
		Summary.Segments =
		{
			MakeSegment(TEXT("PoseSearch"), PoseSearch),
			MakeSegment(TEXT("PhcPolicy"), PhcPolicy),
			MakeSegment(TEXT("PhysicsControl"), PhysicsControl),
			MakeSegment(TEXT("Chaos"), Chaos),
			MakeSegment(TEXT("RendererFacingMotion"), RendererFacingMotion)
		};
		Summary.TerminalReason = TerminalReason;
		Summary.StrictVerdict = StrictVerdict;
		Summary.TerminalArtifactPath = PhysAnimProofArtifactEmitter::BuildTerminalArtifactJsonPath(AttemptUuid);
		return Summary;
	}

	bool WriteTextFile(const FString& Path, const FString& Contents)
	{
		const FString Directory = FPaths::GetPath(Path);
		if (!Directory.IsEmpty() && !IFileManager::Get().MakeDirectory(*Directory, true))
		{
			return false;
		}

		return FFileHelper::SaveStringToFile(Contents, *Path);
	}

	void SetTimestamp(const FString& Path, const FDateTime& Timestamp)
	{
		FPlatformFileManager::Get().GetPlatformFile().SetTimeStamp(*Path, Timestamp);
	}

	FString BuildTerminalArtifactJson(
		const FString& AttemptUuid,
		const EPhysAnimTerminalReason TerminalReason,
		const bool bPassed)
	{
		const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("attempt_uuid"), AttemptUuid);
		Json->SetNumberField(TEXT("timestamp"), 1.0);
		Json->SetNumberField(TEXT("terminal_reason"), static_cast<int32>(TerminalReason));
		Json->SetStringField(TEXT("terminal_reason_name"), PhysAnimProofArtifactEmitter::ToTerminalReasonString(TerminalReason));
		Json->SetBoolField(TEXT("terminal_frame_artifact_captured"), true);
		Json->SetBoolField(TEXT("bPassed"), bPassed);

		FString JsonString;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
		FJsonSerializer::Serialize(Json, Writer);
		return JsonString;
	}

	FPhysAnimEvidenceCollectorFixturePaths MakeFixturePaths(const FString& CaseName)
	{
		FPhysAnimEvidenceCollectorFixturePaths Paths;
		Paths.BaseDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PhysAnim"), TEXT("CollectorTests"), CaseName);
		Paths.TerminalDir = FPaths::Combine(Paths.BaseDir, TEXT("ProofArtifacts"));
		Paths.SummaryDir = FPaths::Combine(Paths.BaseDir, TEXT("EvidenceSummaries"));
		Paths.LogDir = FPaths::Combine(Paths.BaseDir, TEXT("Logs"));
		IFileManager::Get().DeleteDirectory(*Paths.BaseDir, false, true);
		IFileManager::Get().MakeDirectory(*Paths.TerminalDir, true);
		IFileManager::Get().MakeDirectory(*Paths.SummaryDir, true);
		IFileManager::Get().MakeDirectory(*Paths.LogDir, true);
		return Paths;
	}

	void CleanupFixturePaths(const FPhysAnimEvidenceCollectorFixturePaths& Paths)
	{
		IFileManager::Get().DeleteDirectory(*Paths.BaseDir, false, true);
	}

	void WriteAttemptArtifacts(
		FAutomationTestBase& Test,
		const FPhysAnimEvidenceCollectorFixturePaths& Paths,
		const FString& AttemptUuid,
		const FString& LogName,
		const FString& LogContents,
		const FDateTime& Timestamp,
		const bool bPassed,
		const EPhysAnimTerminalReason TerminalReason,
		const EPhysAnimEvidenceBaselineVerdict StrictVerdict,
		const EPhysAnimEvidenceBaselineSegmentState PoseSearch,
		const EPhysAnimEvidenceBaselineSegmentState PhcPolicy,
		const EPhysAnimEvidenceBaselineSegmentState PhysicsControl,
		const EPhysAnimEvidenceBaselineSegmentState Chaos,
		const EPhysAnimEvidenceBaselineSegmentState RendererFacingMotion)
	{
		const FString TerminalPath = FPaths::Combine(Paths.TerminalDir, AttemptUuid + TEXT("_terminal.json"));
		const FString SummaryPath = FPaths::Combine(Paths.SummaryDir, AttemptUuid + TEXT("_evidence_summary.json"));
		const FString CollectorLogPath = FPaths::Combine(Paths.LogDir, LogName);

		Test.TestTrue(TEXT("terminal artifact fixture writes"), WriteTextFile(TerminalPath, BuildTerminalArtifactJson(AttemptUuid, TerminalReason, bPassed)));
		Test.TestTrue(TEXT("summary fixture writes"), WriteTextFile(SummaryPath, PhysAnimEvidenceSummary::SerializeToJsonString(MakeSummary(
			AttemptUuid,
			StrictVerdict,
			TerminalReason,
			PoseSearch,
			PhcPolicy,
			PhysicsControl,
			Chaos,
			RendererFacingMotion))));
		Test.TestTrue(TEXT("log fixture writes"), WriteTextFile(CollectorLogPath, LogContents));

		SetTimestamp(TerminalPath, Timestamp);
		SetTimestamp(SummaryPath, Timestamp);
		SetTimestamp(CollectorLogPath, Timestamp);
	}

	bool CollectAndAssertLatestSelection(
		FAutomationTestBase& Test,
		const FPhysAnimEvidenceCollectorFixturePaths& Paths,
		const FString& ExpectedTerminalAttemptUuid,
		const FString& ExpectedSummaryAttemptUuid)
	{
		const FPhysAnimEvidenceCollectorInput Input
		{
			Paths.TerminalDir,
			Paths.SummaryDir,
			Paths.LogDir
		};

		const FPhysAnimEvidenceCollectorResult Result = PhysAnimEvidenceCollector::Collect(Input);
		Test.TestEqual(TEXT("latest terminal artifact is selected"), Result.TerminalArtifact.TerminalArtifact.AttemptUuid, ExpectedTerminalAttemptUuid);
		Test.TestEqual(TEXT("latest evidence summary is selected"), Result.EvidenceSummary.Summary.AttemptUuid, ExpectedSummaryAttemptUuid);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimEvidenceCollectorContractTest,
		"PhysAnim.EvidenceCollector.Contract",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimEvidenceCollectorContractTest::RunTest(const FString& Parameters)
	{
		{
			const FPhysAnimEvidenceCollectorFixturePaths Paths = MakeFixturePaths(TEXT("latest-selection"));
			WriteAttemptArtifacts(
				*this,
				Paths,
				TEXT("attempt-old"),
				TEXT("attempt-old.log"),
				TEXT("PhysAnimProof: AttemptResult uuid=attempt-old verdict=FAIL"),
				FDateTime(2026, 6, 10, 10, 0, 0),
				false,
				EPhysAnimTerminalReason::ActivationSupportFailure,
				EPhysAnimEvidenceBaselineVerdict::Blocked,
				EPhysAnimEvidenceBaselineSegmentState::Active,
				EPhysAnimEvidenceBaselineSegmentState::ReachedButInactive,
				EPhysAnimEvidenceBaselineSegmentState::NotReached,
				EPhysAnimEvidenceBaselineSegmentState::NotReached,
				EPhysAnimEvidenceBaselineSegmentState::NotReached);
			WriteAttemptArtifacts(
				*this,
				Paths,
				TEXT("attempt-new"),
				TEXT("attempt-new.log"),
				TEXT("PhysAnimProof: AttemptResult uuid=attempt-new verdict=PASS"),
				FDateTime(2026, 6, 10, 11, 0, 0),
				true,
				EPhysAnimTerminalReason::None,
				EPhysAnimEvidenceBaselineVerdict::Diagnostic,
				EPhysAnimEvidenceBaselineSegmentState::Active,
				EPhysAnimEvidenceBaselineSegmentState::Active,
				EPhysAnimEvidenceBaselineSegmentState::Active,
				EPhysAnimEvidenceBaselineSegmentState::Active,
				EPhysAnimEvidenceBaselineSegmentState::Active);

			const FString SummaryNewPath = FPaths::Combine(Paths.SummaryDir, TEXT("summary-new_evidence_summary.json"));
			TestTrue(TEXT("independent latest summary fixture writes"), WriteTextFile(
				SummaryNewPath,
				PhysAnimEvidenceSummary::SerializeToJsonString(MakeSummary(
					TEXT("summary-new"),
					EPhysAnimEvidenceBaselineVerdict::Blocked,
					EPhysAnimTerminalReason::ActivationSupportFailure,
					EPhysAnimEvidenceBaselineSegmentState::Active,
					EPhysAnimEvidenceBaselineSegmentState::ReachedButInactive,
					EPhysAnimEvidenceBaselineSegmentState::NotReached,
					EPhysAnimEvidenceBaselineSegmentState::NotReached,
					EPhysAnimEvidenceBaselineSegmentState::NotReached))));
			SetTimestamp(SummaryNewPath, FDateTime(2026, 6, 10, 11, 30, 0));

			TestTrue(TEXT("latest selection collector result is valid"), CollectAndAssertLatestSelection(*this, Paths, TEXT("attempt-new"), TEXT("summary-new")));
			CleanupFixturePaths(Paths);
		}

		{
			const FPhysAnimEvidenceCollectorFixturePaths Paths = MakeFixturePaths(TEXT("artifact-log-contradiction"));
			WriteAttemptArtifacts(
				*this,
				Paths,
				TEXT("attempt-contradict"),
				TEXT("attempt-contradict.log"),
				TEXT("PhysAnimProof: AttemptResult uuid=attempt-contradict verdict=PASS"),
				FDateTime(2026, 6, 10, 12, 0, 0),
				false,
				EPhysAnimTerminalReason::ActivationSupportFailure,
				EPhysAnimEvidenceBaselineVerdict::Blocked,
				EPhysAnimEvidenceBaselineSegmentState::Active,
				EPhysAnimEvidenceBaselineSegmentState::ReachedButInactive,
				EPhysAnimEvidenceBaselineSegmentState::NotReached,
				EPhysAnimEvidenceBaselineSegmentState::NotReached,
				EPhysAnimEvidenceBaselineSegmentState::NotReached);

			const FPhysAnimEvidenceCollectorInput Input
			{
				Paths.TerminalDir,
				Paths.SummaryDir,
				Paths.LogDir
			};
			const FPhysAnimEvidenceCollectorResult Result = PhysAnimEvidenceCollector::Collect(Input);

			TestTrue(TEXT("terminal proof JSON signal is present"), Result.ClassificationInput.ProofSignals.TerminalProofJsonPassed.IsSet());
			TestFalse(TEXT("terminal proof JSON false is preserved"), Result.ClassificationInput.ProofSignals.TerminalProofJsonPassed.GetValue());
			TestTrue(TEXT("log pass is preserved"), Result.ClassificationInput.ProofSignals.LogPass.IsSet());
			TestTrue(TEXT("log pass is true"), Result.ClassificationInput.ProofSignals.LogPass.GetValue());
			TestNotEqual(
				TEXT("contradiction does not become product success candidate"),
				static_cast<uint8>(Result.ClassificationResult.Verdict),
				static_cast<uint8>(EPhysAnimEvidenceBaselineVerdict::ProductSuccessCandidate));
			CleanupFixturePaths(Paths);
		}

		{
			const FPhysAnimEvidenceCollectorFixturePaths Paths = MakeFixturePaths(TEXT("report-sections"));
			WriteAttemptArtifacts(
				*this,
				Paths,
				TEXT("attempt-report"),
				TEXT("attempt-report.log"),
				TEXT("PhysAnimProof: AttemptResult uuid=attempt-report verdict=FAIL"),
				FDateTime(2026, 6, 10, 13, 0, 0),
				false,
				EPhysAnimTerminalReason::ActivationSupportFailure,
				EPhysAnimEvidenceBaselineVerdict::Blocked,
				EPhysAnimEvidenceBaselineSegmentState::Active,
				EPhysAnimEvidenceBaselineSegmentState::ReachedButInactive,
				EPhysAnimEvidenceBaselineSegmentState::NotReached,
				EPhysAnimEvidenceBaselineSegmentState::NotReached,
				EPhysAnimEvidenceBaselineSegmentState::NotReached);

			const FPhysAnimEvidenceCollectorInput Input
			{
				Paths.TerminalDir,
				Paths.SummaryDir,
				Paths.LogDir
			};
			const FPhysAnimEvidenceCollectorResult Result = PhysAnimEvidenceCollector::Collect(Input);

			TestTrue(TEXT("report includes Actual Evidence"), Result.Report.Contains(TEXT("Actual Evidence")));
			TestTrue(TEXT("report includes Weak Evidence"), Result.Report.Contains(TEXT("Weak Evidence")));
			TestTrue(TEXT("report includes Contradictions"), Result.Report.Contains(TEXT("Contradictions")));
			TestTrue(TEXT("report includes Missing Evidence"), Result.Report.Contains(TEXT("Missing Evidence")));
			TestTrue(TEXT("report includes Next Blocking Segment"), Result.Report.Contains(TEXT("Next Blocking Segment")));
			TestTrue(TEXT("report identifies PhcPolicy as the next blocking segment"), Result.Report.Contains(TEXT("Next Blocking Segment: PhcPolicy")));
			CleanupFixturePaths(Paths);
		}

		{
			const FPhysAnimEvidenceCollectorFixturePaths Paths = MakeFixturePaths(TEXT("contradictory-report"));
			WriteAttemptArtifacts(
				*this,
				Paths,
				TEXT("attempt-fail"),
				TEXT("attempt-fail.log"),
				TEXT("PhysAnimProof: AttemptResult uuid=attempt-fail verdict=PASS"),
				FDateTime(2026, 6, 10, 14, 0, 0),
				false,
				EPhysAnimTerminalReason::ActivationSupportFailure,
				EPhysAnimEvidenceBaselineVerdict::Blocked,
				EPhysAnimEvidenceBaselineSegmentState::Active,
				EPhysAnimEvidenceBaselineSegmentState::ReachedButInactive,
				EPhysAnimEvidenceBaselineSegmentState::NotReached,
				EPhysAnimEvidenceBaselineSegmentState::NotReached,
				EPhysAnimEvidenceBaselineSegmentState::NotReached);

			const FPhysAnimEvidenceCollectorInput Input
			{
				Paths.TerminalDir,
				Paths.SummaryDir,
				Paths.LogDir
			};
			const FPhysAnimEvidenceCollectorResult Result = PhysAnimEvidenceCollector::Collect(Input);

			TestTrue(TEXT("contradictions section includes uppercase marker"), Result.Report.Contains(TEXT("CONTRADICTORY")));
			TestFalse(TEXT("report does not claim product success candidate"), Result.Report.Contains(TEXT("PRODUCT_SUCCESS_CANDIDATE")));
			CleanupFixturePaths(Paths);
		}

		return true;
	}
}
