#include "PhysAnimEvidenceSummary.h"
#include "PhysAnimProofArtifactEmitter.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/AutomationTest.h"

namespace
{
	using namespace PhysAnimEvidenceSummary;

	FPhysAnimEvidenceSummaryCommandMetadata MakeCommandMetadata()
	{
		FPhysAnimEvidenceSummaryCommandMetadata Metadata;
		Metadata.CommandName = TEXT("PhysAnimBridgeTest");
		Metadata.CommandLine = TEXT("UEEditor.exe -ExecCmds=PhysAnimBridgeTest");
		Metadata.WorkingDirectory = TEXT("F:/NewEngine-AgentB/PhysAnimUE5");
		return Metadata;
	}

	FPhysAnimEvidenceSummarySegment MakeSegment(
		const FString& SegmentName,
		const EPhysAnimEvidenceBaselineSegmentState State,
		const int32 SampleCount,
		const double Confidence,
		const double Score)
	{
		FPhysAnimEvidenceSummarySegment Segment;
		Segment.SegmentName = SegmentName;
		Segment.State = State;
		Segment.Metrics.SampleCount = SampleCount;
		Segment.Metrics.Confidence = Confidence;
		Segment.Metrics.Score = Score;
		Segment.MissingRequiredFields = { TEXT("none") };
		Segment.DiagnosticNotes = { TEXT("stable") };
		Segment.SourceProvenance = { TEXT("EB-01"), TEXT("Stage2A") };
		return Segment;
	}

	FPhysAnimEvidenceSummary MakeCompleteSummary(const bool bIncludeCommandMetadata)
	{
		FPhysAnimEvidenceSummary Summary;
		Summary.SchemaVersion = 1;
		Summary.AttemptUuid = TEXT("attempt 001");
		Summary.TestName = TEXT("PhysAnim.EvidenceSummary.Serialization");
		Summary.MapName = TEXT("PhysAnim_TestMap");
		Summary.Timestamp = 1234.5;
		if (bIncludeCommandMetadata)
		{
			Summary.CommandMetadata = MakeCommandMetadata();
		}
		Summary.Segments.Add(MakeSegment(TEXT("PoseSearch"), EPhysAnimEvidenceBaselineSegmentState::Active, 8, 0.95, 0.90));
		Summary.Segments.Add(MakeSegment(TEXT("PhcPolicy"), EPhysAnimEvidenceBaselineSegmentState::ReachedButInactive, 4, 0.50, 0.40));
		Summary.QualityFlags.bAssistanceTruthClean = true;
		Summary.QualityFlags.bContinuityTruthClean = false;
		Summary.QualityFlags.bSupportTruthClean = true;
		Summary.QualityFlags.bSimulationTruthClean = false;
		Summary.QualityFlags.bTerminalFailure = true;
		Summary.QualityFlags.bArtifactLogContradiction = false;
		Summary.QualityFlags.bMissingEvidence = false;
		Summary.TerminalReason = EPhysAnimTerminalReason::ActivationSupportFailure;
		Summary.StrictVerdict = EPhysAnimEvidenceBaselineVerdict::Blocked;
		Summary.TerminalArtifactPath = PhysAnimProofArtifactEmitter::BuildTerminalArtifactJsonPath(Summary.AttemptUuid);
		return Summary;
	}

	FPhysAnimEvidenceBaselineResult MakeClassifierResultForWriterTest()
	{
		FPhysAnimEvidenceBaselineResult Result;
		Result.Segments.PoseSearch = EPhysAnimEvidenceBaselineSegmentState::Active;
		Result.Segments.PhcPolicy = EPhysAnimEvidenceBaselineSegmentState::Active;
		Result.Segments.PhysicsControl = EPhysAnimEvidenceBaselineSegmentState::Active;
		Result.Segments.Chaos = EPhysAnimEvidenceBaselineSegmentState::Active;
		Result.Segments.RendererFacingMotion = EPhysAnimEvidenceBaselineSegmentState::Active;
		Result.TruthFlags.bAssistanceTruthClean = true;
		Result.TruthFlags.bContinuityTruthClean = true;
		Result.TruthFlags.bSupportTruthClean = true;
		Result.TruthFlags.bSimulationTruthClean = true;
		Result.TruthFlags.bTerminalFailure = false;
		Result.TruthFlags.bArtifactLogContradiction = false;
		Result.TruthFlags.bMissingEvidence = false;
		Result.ProofSignals.TerminalProofJsonPassed = true;
		Result.ProofSignals.LogPass = true;
		Result.ProofSignals.ArtifactPass = true;
		Result.bHoldThresholdSatisfied = true;
		Result.Verdict = EPhysAnimEvidenceBaselineVerdict::ProductSuccessCandidate;
		return Result;
	}

	FPhysAnimEvidenceSummaryWriteInput MakeWriterInput(
		const FString& AttemptUuid,
		const FString& OutputPathOverride,
		const FPhysAnimEvidenceBaselineResult& ClassifierResult)
	{
		FPhysAnimEvidenceSummaryWriteInput Input;
		Input.Summary = MakeCompleteSummary(true);
		Input.Summary.AttemptUuid = AttemptUuid;
		Input.Summary.TestName = TEXT("PhysAnim.EvidenceSummary.Writer");
		Input.Summary.MapName = TEXT("PhysAnim_Writer_Map");
		Input.Summary.Timestamp = 7777.25;
		Input.Summary.QualityFlags.bAssistanceTruthClean = false;
		Input.Summary.QualityFlags.bContinuityTruthClean = false;
		Input.Summary.QualityFlags.bSupportTruthClean = false;
		Input.Summary.QualityFlags.bSimulationTruthClean = false;
		Input.Summary.StrictVerdict = EPhysAnimEvidenceBaselineVerdict::InsufficientEvidence;
		Input.Summary.TerminalReason = EPhysAnimTerminalReason::ActivationSupportFailure;
		Input.Summary.TerminalArtifactPath = PhysAnimProofArtifactEmitter::BuildTerminalArtifactJsonPath(AttemptUuid);
		Input.ClassifierResult = ClassifierResult;
		Input.OutputPathOverride = OutputPathOverride;
		return Input;
	}

	int32 CountFilesForAttempt(const FString& AttemptUuid)
	{
		TArray<FString> MatchingFiles;
		const FString SearchPattern = FPaths::Combine(
			FPaths::GetPath(BuildEvidenceSummaryJsonPath(AttemptUuid)),
			AttemptUuid + TEXT("*"));
		IFileManager::Get().FindFiles(MatchingFiles, *SearchPattern, true, false);
		return MatchingFiles.Num();
	}

	FPhysAnimProofArtifactEmitInput MakeProofEmitterInput(
		const FString& AttemptUuid,
		const int32 PoseSearchQueryCount = 0,
		const int32 PoseSearchValidResultCount = 0,
		const FString& PoseSearchSelectedAnimationName = TEXT(""),
		const double PoseSearchSelectedTime = 0.0,
		const int32 PoseSearchConsecutiveInvalidFrameCount = 0,
		const int32 RendererFacingMotionSampleCount = 0,
		const int32 RendererFacingMotionActiveSampleCount = 0,
		const double RendererFacingMotionMaxRootWorldPositionDriftCm = 0.0)
	{
		FPhysAnimProofArtifactEmitInput Input;
		Input.AttemptUuid = AttemptUuid;
		Input.StandingSeconds = 3.25;
		Input.RuntimeHitCount = 1;
		Input.MappedSupportHitCount = 1;

		FPhysAnimRunArtifactSnapshot& Artifact = Input.PipelineResult.StateApplyResult.State.TerminalArtifact;
		Artifact.AttemptUuid = AttemptUuid;
		Artifact.Timestamp = 8888.5;
		Artifact.BaselineId = TEXT("PhysAnim_Emitter_Map");
		Artifact.HoldDurationSec = 3.25;
		Artifact.TerminalReason = EPhysAnimTerminalReason::None;
		Artifact.bTerminalFrameArtifactCaptured = true;
		Artifact.PolicyInferenceSuccessCount = 1;
		Artifact.PolicyActionSampleCount = 1;
		Artifact.PolicyActionRawMeanAbsMax = 0.5;
		Artifact.PolicyActionConditionedMeanAbsMax = 0.5;
		Artifact.ControlAlpha = 1.0;
		Artifact.ControlTargetTotalWrites = 1;
		Artifact.ControlTargetNormalWrites = 1;
		Artifact.ControlTargetMaxDeltaDeg = 2.0;
		Artifact.PoseSearchQueryCount = PoseSearchQueryCount;
		Artifact.PoseSearchValidResultCount = PoseSearchValidResultCount;
		Artifact.PoseSearchSelectedAnimationName = PoseSearchSelectedAnimationName;
		Artifact.PoseSearchSelectedTime = PoseSearchSelectedTime;
		Artifact.PoseSearchConsecutiveInvalidFrameCount = PoseSearchConsecutiveInvalidFrameCount;
		Artifact.RendererFacingMotionSampleCount = RendererFacingMotionSampleCount;
		Artifact.RendererFacingMotionActiveSampleCount = RendererFacingMotionActiveSampleCount;
		Artifact.RendererFacingMotionMaxRootWorldPositionDriftCm = RendererFacingMotionMaxRootWorldPositionDriftCm;
		Artifact.RuntimeBodySampleCount = 1;
		Artifact.RuntimeSimulatingBodyCount = 1;
		Artifact.RuntimeMaxBodyLinearSpeedCmPerSecond = 12.0;
		Artifact.bPhysicalContinuityValidatorPassed = true;
		Artifact.SupportMode = EPhysAnimSupportMode::SingleFootSurvival;
		Artifact.ActiveSupportSideCount = 1;
		Artifact.SupportHullAreaCm2 = 100.0;
		Artifact.SupportGapTimerMs = 0.0;

		return Input;
	}

	const FPhysAnimEvidenceSummarySegment* FindSegment(
		const FPhysAnimEvidenceSummary& Summary,
		const FString& SegmentName)
	{
		for (const FPhysAnimEvidenceSummarySegment& Segment : Summary.Segments)
		{
			if (Segment.SegmentName == SegmentName)
			{
				return &Segment;
			}
		}

		return nullptr;
	}

	void AssertSummaryRoundTrip(
		FAutomationTestBase& Test,
		const FPhysAnimEvidenceSummary& Expected,
		const FPhysAnimEvidenceSummary& Actual)
	{
		Test.TestEqual(TEXT("schema_version round-trips"), Actual.SchemaVersion, Expected.SchemaVersion);
		Test.TestEqual(TEXT("attempt_uuid round-trips"), Actual.AttemptUuid, Expected.AttemptUuid);
		Test.TestEqual(TEXT("test_name round-trips"), Actual.TestName, Expected.TestName);
		Test.TestEqual(TEXT("map_name round-trips"), Actual.MapName, Expected.MapName);
		Test.TestEqual(TEXT("timestamp round-trips"), Actual.Timestamp, Expected.Timestamp);
		Test.TestEqual(TEXT("segment count round-trips"), Actual.Segments.Num(), Expected.Segments.Num());
		Test.TestEqual(TEXT("quality flag assistance round-trips"), Actual.QualityFlags.bAssistanceTruthClean, Expected.QualityFlags.bAssistanceTruthClean);
		Test.TestEqual(TEXT("quality flag continuity round-trips"), Actual.QualityFlags.bContinuityTruthClean, Expected.QualityFlags.bContinuityTruthClean);
		Test.TestEqual(TEXT("quality flag support round-trips"), Actual.QualityFlags.bSupportTruthClean, Expected.QualityFlags.bSupportTruthClean);
		Test.TestEqual(TEXT("quality flag simulation round-trips"), Actual.QualityFlags.bSimulationTruthClean, Expected.QualityFlags.bSimulationTruthClean);
		Test.TestEqual(TEXT("quality flag terminal_failure round-trips"), Actual.QualityFlags.bTerminalFailure, Expected.QualityFlags.bTerminalFailure);
		Test.TestEqual(TEXT("quality flag contradiction round-trips"), Actual.QualityFlags.bArtifactLogContradiction, Expected.QualityFlags.bArtifactLogContradiction);
		Test.TestEqual(TEXT("quality flag missing_evidence round-trips"), Actual.QualityFlags.bMissingEvidence, Expected.QualityFlags.bMissingEvidence);
		Test.TestEqual(TEXT("terminal_reason round-trips"), static_cast<uint8>(Actual.TerminalReason), static_cast<uint8>(Expected.TerminalReason));
		Test.TestEqual(TEXT("strict_verdict round-trips"), static_cast<uint8>(Actual.StrictVerdict), static_cast<uint8>(Expected.StrictVerdict));
		Test.TestEqual(TEXT("terminal artifact path round-trips"), Actual.TerminalArtifactPath, Expected.TerminalArtifactPath);
		Test.TestEqual(TEXT("command metadata presence round-trips"), Actual.CommandMetadata.IsSet(), Expected.CommandMetadata.IsSet());

		if (Expected.CommandMetadata.IsSet())
		{
			const FPhysAnimEvidenceSummaryCommandMetadata& ActualMetadata = Actual.CommandMetadata.GetValue();
			const FPhysAnimEvidenceSummaryCommandMetadata& ExpectedMetadata = Expected.CommandMetadata.GetValue();
			Test.TestEqual(TEXT("command name round-trips"), ActualMetadata.CommandName, ExpectedMetadata.CommandName);
			Test.TestEqual(TEXT("command line round-trips"), ActualMetadata.CommandLine, ExpectedMetadata.CommandLine);
			Test.TestEqual(TEXT("working directory round-trips"), ActualMetadata.WorkingDirectory, ExpectedMetadata.WorkingDirectory);
		}

		for (int32 Index = 0; Index < Expected.Segments.Num(); ++Index)
		{
			const FPhysAnimEvidenceSummarySegment& ExpectedSegment = Expected.Segments[Index];
			const FPhysAnimEvidenceSummarySegment& ActualSegment = Actual.Segments[Index];

			Test.TestEqual(TEXT("segment_name round-trips"), ActualSegment.SegmentName, ExpectedSegment.SegmentName);
			Test.TestEqual(TEXT("segment state round-trips"), static_cast<uint8>(ActualSegment.State), static_cast<uint8>(ExpectedSegment.State));
			Test.TestEqual(TEXT("segment sample_count round-trips"), ActualSegment.Metrics.SampleCount, ExpectedSegment.Metrics.SampleCount);
			Test.TestEqual(TEXT("segment confidence round-trips"), ActualSegment.Metrics.Confidence, ExpectedSegment.Metrics.Confidence);
			Test.TestEqual(TEXT("segment score round-trips"), ActualSegment.Metrics.Score, ExpectedSegment.Metrics.Score);
			Test.TestEqual(TEXT("segment missing_required_fields count round-trips"), ActualSegment.MissingRequiredFields.Num(), ExpectedSegment.MissingRequiredFields.Num());
			Test.TestEqual(TEXT("segment diagnostic_notes count round-trips"), ActualSegment.DiagnosticNotes.Num(), ExpectedSegment.DiagnosticNotes.Num());
			Test.TestEqual(TEXT("segment source_provenance count round-trips"), ActualSegment.SourceProvenance.Num(), ExpectedSegment.SourceProvenance.Num());

			for (int32 ValueIndex = 0; ValueIndex < ExpectedSegment.MissingRequiredFields.Num(); ++ValueIndex)
			{
				Test.TestEqual(TEXT("segment missing_required_fields value round-trips"), ActualSegment.MissingRequiredFields[ValueIndex], ExpectedSegment.MissingRequiredFields[ValueIndex]);
			}
			for (int32 ValueIndex = 0; ValueIndex < ExpectedSegment.DiagnosticNotes.Num(); ++ValueIndex)
			{
				Test.TestEqual(TEXT("segment diagnostic_notes value round-trips"), ActualSegment.DiagnosticNotes[ValueIndex], ExpectedSegment.DiagnosticNotes[ValueIndex]);
			}
			for (int32 ValueIndex = 0; ValueIndex < ExpectedSegment.SourceProvenance.Num(); ++ValueIndex)
			{
				Test.TestEqual(TEXT("segment source_provenance value round-trips"), ActualSegment.SourceProvenance[ValueIndex], ExpectedSegment.SourceProvenance[ValueIndex]);
			}
		}
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimEvidenceSummaryPoseSearchStateSerializationTest,
		"PhysAnim.EvidenceSummary.PoseSearchStateSerialization",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimEvidenceSummaryPoseSearchStateSerializationTest::RunTest(const FString& Parameters)
	{
		const struct FCase
		{
			const TCHAR* AttemptUuid;
			int32 PoseSearchQueryCount;
			int32 PoseSearchValidResultCount;
			const TCHAR* PoseSearchSelectedAnimationName;
			double PoseSearchSelectedTime;
			int32 PoseSearchConsecutiveInvalidFrameCount;
			EPhysAnimEvidenceBaselineSegmentState ExpectedState;
		} Cases[] =
		{
			{ TEXT("pose-search-0-0"), 0, 0, TEXT(""), 0.0, 0, EPhysAnimEvidenceBaselineSegmentState::NotReached },
			{ TEXT("pose-search-2-0"), 2, 0, TEXT(""), 0.0, 2, EPhysAnimEvidenceBaselineSegmentState::ReachedButInactive },
			{ TEXT("pose-search-2-1"), 2, 1, TEXT("Anim_Idle"), 1.25, 0, EPhysAnimEvidenceBaselineSegmentState::Active },
			{ TEXT("pose-search-4-2"), 4, 2, TEXT("Anim_Walk"), 3.5, 0, EPhysAnimEvidenceBaselineSegmentState::Active }
		};

		for (const FCase& Case : Cases)
		{
			const FString AttemptUuid = Case.AttemptUuid;
			const FString TerminalPath = PhysAnimProofArtifactEmitter::BuildTerminalArtifactJsonPath(AttemptUuid);
			const FString SummaryPath = BuildEvidenceSummaryJsonPath(AttemptUuid);
			IFileManager::Get().Delete(*TerminalPath);
			IFileManager::Get().Delete(*SummaryPath);

			const FPhysAnimProofArtifactEmitInput Input =
				MakeProofEmitterInput(
					AttemptUuid, 
					Case.PoseSearchQueryCount, 
					Case.PoseSearchValidResultCount, 
					Case.PoseSearchSelectedAnimationName,
					Case.PoseSearchSelectedTime,
					Case.PoseSearchConsecutiveInvalidFrameCount);
			const FPhysAnimProofArtifactEmitResult EmitResult =
				PhysAnimProofArtifactEmitter::EmitTerminalArtifactAndWriteJson(Input);
			TestTrue(TEXT("Terminal artifact writes successfully for PoseSearch summary case"), EmitResult.bJsonWritten);

			FString SummaryJson;
			TestTrue(TEXT("PoseSearch summary sidecar is readable"), FFileHelper::LoadFileToString(SummaryJson, *SummaryPath));

			FPhysAnimEvidenceSummary Parsed;
			TestTrue(TEXT("PoseSearch summary sidecar parses"), DeserializeFromJsonString(SummaryJson, Parsed));

			const FPhysAnimEvidenceSummarySegment* PoseSearchSegment = FindSegment(Parsed, TEXT("pose_search"));
			TestNotNull(TEXT("PoseSearch segment exists in summary"), PoseSearchSegment);
			if (!PoseSearchSegment)
			{
				IFileManager::Get().Delete(*TerminalPath);
				IFileManager::Get().Delete(*SummaryPath);
				return false;
			}

			TestEqual(
				TEXT("PoseSearch sample_count matches query count"),
				PoseSearchSegment->Metrics.SampleCount,
				Case.PoseSearchQueryCount);
			TestEqual(
				TEXT("PoseSearch state serializes from query/valid counts"),
				static_cast<uint8>(PoseSearchSegment->State),
				static_cast<uint8>(Case.ExpectedState));
			
			// We only expect the name to be populated when results are valid, although the struct copies it directly.
			// Let's assert based on what's configured.
			const FString ExpectedAnimName = Case.PoseSearchSelectedAnimationName;
			if (!ExpectedAnimName.IsEmpty())
			{
				const FString ActualAnimName = PoseSearchSegment->Metrics.SelectedSourceIdentity;
				TestEqual(
					TEXT("PoseSearch selected animation matches"),
					ActualAnimName,
					ExpectedAnimName);

				TestEqual(
					TEXT("PoseSearch selected time matches"),
					PoseSearchSegment->Metrics.SelectedSourceTime,
					Case.PoseSearchSelectedTime);
			}

			TestEqual(
				TEXT("PoseSearch consecutive invalid frame count matches"),
				PoseSearchSegment->Metrics.ConsecutiveInvalidSampleCount,
				Case.PoseSearchConsecutiveInvalidFrameCount);

			IFileManager::Get().Delete(*TerminalPath);
			IFileManager::Get().Delete(*SummaryPath);
		}

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimEvidenceSummaryRendererFacingMotionStateSerializationTest,
		"PhysAnim.EvidenceSummary.RendererFacingMotionStateSerialization",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimEvidenceSummaryRendererFacingMotionStateSerializationTest::RunTest(const FString& Parameters)
	{
		const struct FCase
		{
			const TCHAR* AttemptUuid;
			int32 RendererFacingMotionSampleCount;
			int32 RendererFacingMotionActiveSampleCount;
			double RendererFacingMotionMaxRootWorldPositionDriftCm;
			EPhysAnimEvidenceBaselineSegmentState ExpectedState;
		} Cases[] =
		{
			{ TEXT("renderer-motion-0-0"), 0, 0, 0.0, EPhysAnimEvidenceBaselineSegmentState::NotReached },
			{ TEXT("renderer-motion-2-0"), 2, 0, 7.5, EPhysAnimEvidenceBaselineSegmentState::ReachedButInactive },
			{ TEXT("renderer-motion-2-1"), 2, 1, 12.5, EPhysAnimEvidenceBaselineSegmentState::Active },
			{ TEXT("renderer-motion-4-2"), 4, 2, 16.25, EPhysAnimEvidenceBaselineSegmentState::Active }
		};

		for (const FCase& Case : Cases)
		{
			const FString AttemptUuid = Case.AttemptUuid;
			const FString TerminalPath = PhysAnimProofArtifactEmitter::BuildTerminalArtifactJsonPath(AttemptUuid);
			const FString SummaryPath = BuildEvidenceSummaryJsonPath(AttemptUuid);
			IFileManager::Get().Delete(*TerminalPath);
			IFileManager::Get().Delete(*SummaryPath);

			const FPhysAnimProofArtifactEmitInput Input = MakeProofEmitterInput(
				AttemptUuid,
				0,
				0,
				TEXT(""),
				0.0,
				Case.RendererFacingMotionSampleCount,
				Case.RendererFacingMotionActiveSampleCount,
				Case.RendererFacingMotionMaxRootWorldPositionDriftCm);
			const FPhysAnimProofArtifactEmitResult EmitResult =
				PhysAnimProofArtifactEmitter::EmitTerminalArtifactAndWriteJson(Input);
			TestTrue(TEXT("Terminal artifact writes successfully for RendererFacingMotion summary case"), EmitResult.bJsonWritten);

			FString SummaryJson;
			TestTrue(TEXT("RendererFacingMotion summary sidecar is readable"), FFileHelper::LoadFileToString(SummaryJson, *SummaryPath));

			FPhysAnimEvidenceSummary Parsed;
			TestTrue(TEXT("RendererFacingMotion summary sidecar parses"), DeserializeFromJsonString(SummaryJson, Parsed));

			const FPhysAnimEvidenceSummarySegment* RendererFacingMotionSegment = FindSegment(Parsed, TEXT("RendererFacingMotion"));
			TestNotNull(TEXT("RendererFacingMotion segment exists in summary"), RendererFacingMotionSegment);
			if (!RendererFacingMotionSegment)
			{
				IFileManager::Get().Delete(*TerminalPath);
				IFileManager::Get().Delete(*SummaryPath);
				return false;
			}

			TestEqual(
				TEXT("RendererFacingMotion sample_count matches sample count"),
				RendererFacingMotionSegment->Metrics.SampleCount,
				Case.RendererFacingMotionSampleCount);
			TestEqual(
				TEXT("RendererFacingMotion state serializes from sample counts"),
				static_cast<uint8>(RendererFacingMotionSegment->State),
				static_cast<uint8>(Case.ExpectedState));
			TestEqual(
				TEXT("RendererFacingMotion confidence uses the active/sample ratio"),
				RendererFacingMotionSegment->Metrics.Confidence,
				Case.RendererFacingMotionSampleCount > 0
					? static_cast<double>(Case.RendererFacingMotionActiveSampleCount) / static_cast<double>(Case.RendererFacingMotionSampleCount)
					: 0.0);
			TestEqual(
				TEXT("RendererFacingMotion score uses max root drift"),
				RendererFacingMotionSegment->Metrics.Score,
				Case.RendererFacingMotionMaxRootWorldPositionDriftCm);

			IFileManager::Get().Delete(*TerminalPath);
			IFileManager::Get().Delete(*SummaryPath);
		}

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimEvidenceSummarySchemaContractTest,
		"PhysAnim.EvidenceSummary.SchemaContract",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimEvidenceSummarySchemaContractTest::RunTest(const FString& Parameters)
	{
		const FPhysAnimEvidenceSummary Summary = MakeCompleteSummary(true);
		const FString Json = SerializeToJsonString(Summary);

		TestTrue(TEXT("schema_version field is present"), Json.Contains(TEXT("\"schema_version\":")));
		TestTrue(TEXT("attempt_uuid field is present"), Json.Contains(TEXT("\"attempt_uuid\":")));
		TestTrue(TEXT("test_name field is present"), Json.Contains(TEXT("\"test_name\":")));
		TestTrue(TEXT("map_name field is present"), Json.Contains(TEXT("\"map_name\":")));
		TestTrue(TEXT("timestamp field is present"), Json.Contains(TEXT("\"timestamp\":")));
		TestTrue(TEXT("command_metadata field is present"), Json.Contains(TEXT("\"command_metadata\":")));
		TestTrue(TEXT("segments field is present"), Json.Contains(TEXT("\"segments\":")));
		TestTrue(TEXT("quality_flags field is present"), Json.Contains(TEXT("\"quality_flags\":")));
		TestTrue(TEXT("terminal_reason field is present"), Json.Contains(TEXT("\"terminal_reason\":")));
		TestTrue(TEXT("strict_verdict field is present"), Json.Contains(TEXT("\"strict_verdict\":")));
		TestTrue(TEXT("terminal_artifact_path field is present"), Json.Contains(TEXT("\"terminal_artifact_path\":")));
		TestTrue(TEXT("segment_name field is present"), Json.Contains(TEXT("\"segment_name\":")));
		TestTrue(TEXT("state field is present"), Json.Contains(TEXT("\"state\":")));
		TestTrue(TEXT("metrics field is present"), Json.Contains(TEXT("\"metrics\":")));
		TestTrue(TEXT("missing_required_fields field is present"), Json.Contains(TEXT("\"missing_required_fields\":")));
		TestTrue(TEXT("diagnostic_notes field is present"), Json.Contains(TEXT("\"diagnostic_notes\":")));
		TestTrue(TEXT("source_provenance field is present"), Json.Contains(TEXT("\"source_provenance\":")));

		const FString ExpectedProofPath = PhysAnimProofArtifactEmitter::BuildTerminalArtifactJsonPath(Summary.AttemptUuid);
		const FString SummaryPath = BuildEvidenceSummaryJsonPath(Summary.AttemptUuid);
		TestTrue(TEXT("Evidence summary path differs from terminal proof path"), SummaryPath != ExpectedProofPath);
		TestTrue(TEXT("Evidence summary path uses the dedicated directory"), SummaryPath.Contains(TEXT("EvidenceSummaries")));
		TestEqual(
			TEXT("Evidence summary path uses the sanitized attempt token"),
			SummaryPath,
			FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("PhysAnim"),
				TEXT("EvidenceSummaries"),
				TEXT("attempt_001_evidence_summary.json")));

		FPhysAnimEvidenceSummary Parsed;
		TestTrue(TEXT("Serialized summary parses back into a contract object"), DeserializeFromJsonString(Json, Parsed));
		AssertSummaryRoundTrip(*this, Summary, Parsed);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimEvidenceSummaryMissingCommandMetadataTest,
		"PhysAnim.EvidenceSummary.MissingCommandMetadata",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimEvidenceSummaryMissingCommandMetadataTest::RunTest(const FString& Parameters)
	{
		const FPhysAnimEvidenceSummary Summary = MakeCompleteSummary(false);
		const FString Json = SerializeToJsonString(Summary);

		TestTrue(TEXT("Missing command metadata is emitted as null"), Json.Contains(TEXT("\"command_metadata\":null")));

		FPhysAnimEvidenceSummary Parsed;
		TestTrue(TEXT("Missing command metadata still parses"), DeserializeFromJsonString(Json, Parsed));
		TestFalse(TEXT("Missing command metadata stays unset after parsing"), Parsed.CommandMetadata.IsSet());
		AssertSummaryRoundTrip(*this, Summary, Parsed);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimEvidenceSummaryWriterSuccessTest,
		"PhysAnim.EvidenceSummary.WriterSuccess",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimEvidenceSummaryWriterSuccessTest::RunTest(const FString& Parameters)
	{
		const FString AttemptUuid = TEXT("writer-success-attempt");
		const FPhysAnimEvidenceBaselineResult ClassifierResult = MakeClassifierResultForWriterTest();
		const FPhysAnimEvidenceSummaryWriteInput Input =
			MakeWriterInput(AttemptUuid, FString(), ClassifierResult);
		const FString ExpectedPath = BuildEvidenceSummaryJsonPath(AttemptUuid);
		IFileManager::Get().Delete(*ExpectedPath);

		FPhysAnimEvidenceSummary ExpectedSummary = Input.Summary;
		ExpectedSummary.QualityFlags = ClassifierResult.TruthFlags;
		ExpectedSummary.StrictVerdict = ClassifierResult.Verdict;
		const FString ExpectedJson = SerializeToJsonString(ExpectedSummary);

		const FPhysAnimEvidenceSummaryWriteResult WriteResult = WriteEvidenceSummaryJson(Input);
		TestTrue(TEXT("Sidecar summary writes successfully"), WriteResult.bJsonWritten);
		TestFalse(TEXT("Sidecar summary write is not reported as capture failure"), WriteResult.bEvidenceCaptureFailure);
		TestEqual(TEXT("Sidecar summary path is built from the attempt UUID"), WriteResult.JsonPath, BuildEvidenceSummaryJsonPath(AttemptUuid));
		TestTrue(TEXT("Sidecar summary path differs from terminal proof path"), WriteResult.JsonPath != Input.Summary.TerminalArtifactPath);
		TestTrue(TEXT("Sidecar summary path uses the dedicated directory"), WriteResult.JsonPath.Contains(TEXT("EvidenceSummaries")));
		TestTrue(TEXT("Sidecar summary file exists after write"), IFileManager::Get().FileExists(*WriteResult.JsonPath));
		TestEqual(TEXT("Exactly one sidecar summary exists for the attempt"), CountFilesForAttempt(AttemptUuid), 1);

		FString FileContents;
		TestTrue(TEXT("Sidecar summary file is readable"), FFileHelper::LoadFileToString(FileContents, *WriteResult.JsonPath));
		TestEqual(TEXT("Sidecar summary uses the EB-02 serializer output"), FileContents, ExpectedJson);

		FPhysAnimEvidenceSummary Parsed;
		TestTrue(TEXT("Sidecar summary parses back"), DeserializeFromJsonString(FileContents, Parsed));
		TestEqual(TEXT("Classifier strict verdict is captured in the summary"), static_cast<uint8>(Parsed.StrictVerdict), static_cast<uint8>(ClassifierResult.Verdict));
		TestEqual(TEXT("Classifier assistance flag is captured in the summary"), Parsed.QualityFlags.bAssistanceTruthClean, ClassifierResult.TruthFlags.bAssistanceTruthClean);
		TestEqual(TEXT("Classifier continuity flag is captured in the summary"), Parsed.QualityFlags.bContinuityTruthClean, ClassifierResult.TruthFlags.bContinuityTruthClean);
		TestEqual(TEXT("Classifier support flag is captured in the summary"), Parsed.QualityFlags.bSupportTruthClean, ClassifierResult.TruthFlags.bSupportTruthClean);
		TestEqual(TEXT("Classifier simulation flag is captured in the summary"), Parsed.QualityFlags.bSimulationTruthClean, ClassifierResult.TruthFlags.bSimulationTruthClean);

		IFileManager::Get().Delete(*WriteResult.JsonPath);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimEvidenceSummaryWriterFailureTest,
		"PhysAnim.EvidenceSummary.WriterFailure",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimEvidenceSummaryWriterFailureTest::RunTest(const FString& Parameters)
	{
		const FString AttemptUuid = TEXT("writer-failure-attempt");
		const FPhysAnimEvidenceBaselineResult ClassifierResult = MakeClassifierResultForWriterTest();
		const FString OutputPathOverride = FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("PhysAnim"),
			TEXT("EvidenceSummaries"));
		IFileManager::Get().MakeDirectory(*OutputPathOverride, true);
		const FPhysAnimEvidenceSummaryWriteInput Input =
			MakeWriterInput(AttemptUuid, OutputPathOverride, ClassifierResult);

		const FPhysAnimEvidenceSummaryWriteResult WriteResult = WriteEvidenceSummaryJson(Input);
		TestFalse(TEXT("Writing to a directory path fails"), WriteResult.bJsonWritten);
		TestTrue(TEXT("Writing to a directory path is reported as evidence capture failure"), WriteResult.bEvidenceCaptureFailure);
		TestEqual(TEXT("Failure result reports the attempted path"), WriteResult.JsonPath, OutputPathOverride);
		TestFalse(TEXT("No file is created for the failure case"), IFileManager::Get().FileExists(*WriteResult.JsonPath));

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimEvidenceSummaryProofEmitterHookTest,
		"PhysAnim.EvidenceSummary.ProofEmitterHook",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimEvidenceSummaryProofEmitterHookTest::RunTest(const FString& Parameters)
	{
		const FString AttemptUuid = TEXT("proof-emitter-sidecar-attempt");
		const FString TerminalPath = PhysAnimProofArtifactEmitter::BuildTerminalArtifactJsonPath(AttemptUuid);
		const FString SummaryPath = BuildEvidenceSummaryJsonPath(AttemptUuid);
		IFileManager::Get().Delete(*TerminalPath);
		IFileManager::Get().Delete(*SummaryPath);

		const FPhysAnimProofArtifactEmitInput Input = MakeProofEmitterInput(AttemptUuid);
		const FPhysAnimProofArtifactEmitResult TerminalResult =
			PhysAnimProofArtifactEmitter::EmitTerminalArtifactAndWriteJson(Input);

		TestTrue(TEXT("Terminal proof artifact still writes successfully"), TerminalResult.bJsonWritten);
		TestEqual(TEXT("Terminal proof artifact path is unchanged"), TerminalResult.JsonPath, TerminalPath);
		TestTrue(TEXT("Terminal proof artifact exists"), IFileManager::Get().FileExists(*TerminalPath));
		TestTrue(TEXT("Evidence summary sidecar exists"), IFileManager::Get().FileExists(*SummaryPath));
		TestTrue(TEXT("Evidence summary sidecar path differs from terminal proof path"), SummaryPath != TerminalPath);
		TestEqual(TEXT("Proof attempt emits exactly one summary sidecar for the attempt"), CountFilesForAttempt(AttemptUuid), 1);

		FString SummaryJson;
		TestTrue(TEXT("Evidence summary sidecar is readable"), FFileHelper::LoadFileToString(SummaryJson, *SummaryPath));

		FPhysAnimEvidenceSummary Parsed;
		TestTrue(TEXT("Evidence summary sidecar parses"), DeserializeFromJsonString(SummaryJson, Parsed));
		TestEqual(TEXT("Sidecar preserves terminal artifact path reference"), Parsed.TerminalArtifactPath, TerminalPath);
		TestEqual(TEXT("Sidecar keeps strict verdict diagnostic until missing segment metrics exist"), static_cast<uint8>(Parsed.StrictVerdict), static_cast<uint8>(EPhysAnimEvidenceBaselineVerdict::Diagnostic));
		TestFalse(TEXT("Sidecar classifier output preserves durable terminal artifact evidence"), Parsed.QualityFlags.bMissingEvidence);

		FString TerminalJson;
		TestTrue(TEXT("Terminal proof artifact is readable"), FFileHelper::LoadFileToString(TerminalJson, *TerminalPath));
		TestTrue(TEXT("Terminal proof artifact captures renderer-facing motion sample count"), TerminalJson.Contains(TEXT("\"renderer_facing_motion_sample_count\":0")));
		TestTrue(TEXT("Terminal proof artifact captures renderer-facing motion active sample count"), TerminalJson.Contains(TEXT("\"renderer_facing_motion_active_sample_count\":0")));
		TestTrue(TEXT("Terminal proof artifact captures renderer-facing motion max root drift"), TerminalJson.Contains(TEXT("\"renderer_facing_motion_max_root_world_position_drift_cm\":0")));

		IFileManager::Get().Delete(*TerminalPath);
		IFileManager::Get().Delete(*SummaryPath);
		return true;
	}
}
