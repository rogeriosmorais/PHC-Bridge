#include "PhysAnimEvidenceSummary.h"
#include "PhysAnimProofArtifactEmitter.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/AutomationTest.h"
#include "Math/UnrealMathUtility.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

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
		Segment.Metrics.SelectedSourceIdentity = TEXT("identity");
		Segment.Metrics.SelectedSourceTime = 1.25;
		Segment.Metrics.ConsecutiveInvalidSampleCount = 0;
		Segment.Metrics.InferenceAttemptCount = 1;
		Segment.Metrics.InferenceFailureCount = 0;
		Segment.Metrics.InferenceLatencyMsMax = 5.0;
		Segment.Metrics.bModelLoaded = true;
		Segment.Metrics.RuntimeName = TEXT("ORT_DML");
		Segment.Metrics.bInputBuffersFinite = true;
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
		const int32 PolicyInferenceSuccessCount = 1,
		const int32 PolicyInferenceAttemptCount = 1,
		const int32 PolicyInferenceFailureCount = 0,
		const double PolicyInferenceLatencyMsMax = 5.0,
		const bool bPolicyModelLoaded = true,
		const FString& PolicyRuntimeName = TEXT("ORT_DML"),
		const FString& PolicyModelName = TEXT("phc_policy"),
		const bool bPolicyInputBuffersFinite = true,
		const int32 RendererFacingMotionSampleCount = 0,
		const int32 RendererFacingMotionActiveSampleCount = 0,
		const double RendererFacingMotionMaxRootWorldPositionDriftCm = 0.0,
		const int32 ControlTargetSampleCount = 1,
		const int32 ControlTargetNormalWrites = 1,
		const int32 ControlTargetTotalWrites = 1,
		const double ControlTargetMaxDeltaDeg = 2.0,
		const double ControlTargetMaxRawPolicyOffsetDeg = 0.0,
		const double ControlTargetMeanRawPolicyOffsetDegMax = 0.0)
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
		Artifact.PolicyInferenceSuccessCount = PolicyInferenceSuccessCount;
		Artifact.PolicyInferenceAttemptCount = PolicyInferenceAttemptCount;
		Artifact.PolicyInferenceFailureCount = PolicyInferenceFailureCount;
		Artifact.PolicyInferenceLatencyMsMax = PolicyInferenceLatencyMsMax;
		Artifact.bPolicyModelLoaded = bPolicyModelLoaded;
		Artifact.PolicyRuntimeName = PolicyRuntimeName;
		Artifact.PolicyModelName = PolicyModelName;
		Artifact.bPolicyInputBuffersFinite = bPolicyInputBuffersFinite;
		Artifact.PolicyActionSampleCount = 1;
		Artifact.PolicyActionRawMeanAbsMax = 0.5;
		Artifact.PolicyActionConditionedMeanAbsMax = 0.5;
		Artifact.ControlAlpha = 1.0;
		Artifact.ControlTargetSampleCount = ControlTargetSampleCount;
		Artifact.ControlTargetNormalWrites = ControlTargetNormalWrites;
		Artifact.ControlTargetTotalWrites = ControlTargetTotalWrites;
		Artifact.ControlTargetMaxDeltaDeg = ControlTargetMaxDeltaDeg;
		Artifact.ControlTargetMaxRawPolicyOffsetDeg = ControlTargetMaxRawPolicyOffsetDeg;
		Artifact.ControlTargetMeanRawPolicyOffsetDegMax = ControlTargetMeanRawPolicyOffsetDegMax;
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

			const FPhysAnimEvidenceSummarySegment* PoseSearchSegment = FindSegment(Parsed, TEXT("PoseSearch"));
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
			{ TEXT("renderer-motion-4-2"), 4, 2, 16.25, EPhysAnimEvidenceBaselineSegmentState::Active },
			{ TEXT("renderer-motion-4-2-1875"), 4, 2, 18.75, EPhysAnimEvidenceBaselineSegmentState::Active }
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
				0,
				1,
				1,
				0,
				5.0,
				true,
				TEXT("ORT_DML"),
				TEXT("phc_policy"),
				true,
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
		FPhysAnimEvidenceSummaryPhcPolicyStateSerializationTest,
		"PhysAnim.EvidenceSummary.PhcPolicyStateSerialization",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimEvidenceSummaryPhcPolicyStateSerializationTest::RunTest(const FString& Parameters)
	{
		const struct FCase
		{
			const TCHAR* AttemptUuid;
			int32 PolicyInferenceAttemptCount;
			int32 PolicyInferenceSuccessCount;
			int32 PolicyInferenceFailureCount;
			const TCHAR* PolicyModelName;
			const TCHAR* PolicyRuntimeName;
			double MaxLatencyMs;
			bool bModelLoaded;
			bool bBuffersFinite;
			EPhysAnimEvidenceBaselineSegmentState ExpectedState;
		} Cases[] =
		{
			{ TEXT("policy-0-0"), 0, 0, 0, TEXT(""), TEXT(""), 0.0, false, false, EPhysAnimEvidenceBaselineSegmentState::NotReached },
			{ TEXT("policy-2-0"), 2, 0, 2, TEXT("phc_policy"), TEXT("ORT_CPU"), 15.0, true, false, EPhysAnimEvidenceBaselineSegmentState::ReachedButInactive },
			{ TEXT("policy-2-2"), 2, 2, 0, TEXT("phc_policy"), TEXT("ORT_DML"), 4.5, true, true, EPhysAnimEvidenceBaselineSegmentState::Active }
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
				0, 0, TEXT(""), 0.0, 0, // PoseSearch defaults
				Case.PolicyInferenceSuccessCount,
				Case.PolicyInferenceAttemptCount,
				Case.PolicyInferenceFailureCount,
				Case.MaxLatencyMs,
				Case.bModelLoaded,
				Case.PolicyRuntimeName,
				Case.PolicyModelName,
				Case.bBuffersFinite);

			const FPhysAnimProofArtifactEmitResult EmitResult =
				PhysAnimProofArtifactEmitter::EmitTerminalArtifactAndWriteJson(Input);
			TestTrue(TEXT("Terminal artifact writes successfully for PhcPolicy summary case"), EmitResult.bJsonWritten);

			FString SummaryJson;
			TestTrue(TEXT("PhcPolicy summary sidecar is readable"), FFileHelper::LoadFileToString(SummaryJson, *SummaryPath));

			FPhysAnimEvidenceSummary Parsed;
			TestTrue(TEXT("PhcPolicy summary sidecar parses"), DeserializeFromJsonString(SummaryJson, Parsed));

			const FPhysAnimEvidenceSummarySegment* PolicySegment = FindSegment(Parsed, TEXT("PhcPolicy"));
			TestNotNull(TEXT("PhcPolicy segment exists in summary"), PolicySegment);
			if (!PolicySegment)
			{
				IFileManager::Get().Delete(*TerminalPath);
				IFileManager::Get().Delete(*SummaryPath);
				return false;
			}

			TestEqual(
				TEXT("PhcPolicy attempt_count matches"),
				PolicySegment->Metrics.InferenceAttemptCount,
				Case.PolicyInferenceAttemptCount);

			TestEqual(
				TEXT("PhcPolicy state serializes correctly"),
				static_cast<uint8>(PolicySegment->State),
				static_cast<uint8>(Case.ExpectedState));

			TestEqual(
				TEXT("PhcPolicy model name matches"),
				PolicySegment->Metrics.SelectedSourceIdentity,
				FString(Case.PolicyModelName));

			TestEqual(
				TEXT("PhcPolicy runtime name matches"),
				PolicySegment->Metrics.RuntimeName,
				FString(Case.PolicyRuntimeName));

			TestEqual(
				TEXT("PhcPolicy latency matches"),
				PolicySegment->Metrics.InferenceLatencyMsMax,
				Case.MaxLatencyMs);

			IFileManager::Get().Delete(*TerminalPath);
			IFileManager::Get().Delete(*SummaryPath);
		}

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimEvidenceSummaryPhysicsControlStateSerializationTest,
		"PhysAnim.EvidenceSummary.PhysicsControlStateSerialization",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimEvidenceSummaryPhysicsControlStateSerializationTest::RunTest(const FString& Parameters)
	{
		const struct FCase
		{
			const TCHAR* AttemptUuid;
			bool bPhysicsControlComponentAvailable;
			int32 ControlTargetSampleCount;
			int32 ControlTargetNormalWrites;
			int32 ControlTargetTotalWrites;
			EPhysAnimEvidenceBaselineSegmentState ExpectedState;
			bool bExpectTerminalJsonField;
		} Cases[] =
		{
			{ TEXT("physics-control-0-4"), false, 4, 3, 7, EPhysAnimEvidenceBaselineSegmentState::NotReached, false },
			{ TEXT("physics-control-2-0"), true, 2, 0, 2, EPhysAnimEvidenceBaselineSegmentState::ReachedButInactive, true },
			{ TEXT("physics-control-2-2"), true, 2, 2, 2, EPhysAnimEvidenceBaselineSegmentState::Active, true }
		};

		for (const FCase& Case : Cases)
		{
			const FString AttemptUuid = Case.AttemptUuid;
			const FString TerminalPath = PhysAnimProofArtifactEmitter::BuildTerminalArtifactJsonPath(AttemptUuid);
			const FString SummaryPath = BuildEvidenceSummaryJsonPath(AttemptUuid);
			IFileManager::Get().Delete(*TerminalPath);
			IFileManager::Get().Delete(*SummaryPath);

			FPhysAnimProofArtifactEmitInput Input = MakeProofEmitterInput(AttemptUuid);
			FPhysAnimRunArtifactSnapshot& Artifact = Input.PipelineResult.StateApplyResult.State.TerminalArtifact;
			Artifact.bPhysicsControlComponentAvailable = Case.bPhysicsControlComponentAvailable;
			Artifact.ControlTargetSampleCount = Case.ControlTargetSampleCount;
			Artifact.ControlTargetNormalWrites = Case.ControlTargetNormalWrites;
			Artifact.ControlTargetTotalWrites = Case.ControlTargetTotalWrites;
			Artifact.ControlledBodyCount = 11;

			const FPhysAnimProofArtifactEmitResult EmitResult =
				PhysAnimProofArtifactEmitter::EmitTerminalArtifactAndWriteJson(Input);
			TestTrue(TEXT("Terminal artifact writes successfully for PhysicsControl summary case"), EmitResult.bJsonWritten);

			FString TerminalJson;
			TestTrue(TEXT("PhysicsControl terminal artifact is readable"), FFileHelper::LoadFileToString(TerminalJson, *TerminalPath));
			if (Case.bExpectTerminalJsonField)
			{
				TSharedPtr<FJsonObject> TerminalJsonObject;
				TestTrue(
					TEXT("PhysicsControl terminal artifact parses"),
					FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TerminalJson), TerminalJsonObject));
				bool bPhysicsControlComponentAvailable = false;
				double ControlledBodyCount = 0.0;
				TestTrue(
					TEXT("PhysicsControl terminal artifact includes physics_control_component_available true"),
					TerminalJsonObject.IsValid() &&
						TerminalJsonObject->TryGetBoolField(TEXT("physics_control_component_available"), bPhysicsControlComponentAvailable) &&
						bPhysicsControlComponentAvailable);
				TestTrue(
					TEXT("PhysicsControl terminal artifact includes controlled_body_count 11"),
					TerminalJsonObject.IsValid() &&
						TerminalJsonObject->TryGetNumberField(TEXT("controlled_body_count"), ControlledBodyCount) &&
						ControlledBodyCount == 11.0);
			}

			FString SummaryJson;
			TestTrue(TEXT("PhysicsControl summary sidecar is readable"), FFileHelper::LoadFileToString(SummaryJson, *SummaryPath));

			FPhysAnimEvidenceSummary Parsed;
			TestTrue(TEXT("PhysicsControl summary sidecar parses"), DeserializeFromJsonString(SummaryJson, Parsed));

			const FPhysAnimEvidenceSummarySegment* PhysicsControlSegment = FindSegment(Parsed, TEXT("PhysicsControl"));
			TestNotNull(TEXT("PhysicsControl segment exists in summary"), PhysicsControlSegment);
			if (!PhysicsControlSegment)
			{
				IFileManager::Get().Delete(*TerminalPath);
				IFileManager::Get().Delete(*SummaryPath);
				return false;
			}

			TestEqual(
				TEXT("PhysicsControl sample_count matches target samples"),
				PhysicsControlSegment->Metrics.SampleCount,
				Case.ControlTargetSampleCount);
			TestEqual(
				TEXT("PhysicsControl state serializes from component availability and writes"),
				static_cast<uint8>(PhysicsControlSegment->State),
				static_cast<uint8>(Case.ExpectedState));
			TestEqual(
				TEXT("PhysicsControl selected_source_identity exposes controlled body count"),
				PhysicsControlSegment->Metrics.SelectedSourceIdentity,
				FString(TEXT("11_bodies")));

			IFileManager::Get().Delete(*TerminalPath);
			IFileManager::Get().Delete(*SummaryPath);
		}

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimEvidenceSummaryPhysicsControlNotReachedEvidenceTest,
		"PhysAnim.EvidenceSummary.PhysicsControlNotReachedEvidence",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimEvidenceSummaryPhysicsControlNotReachedEvidenceTest::RunTest(const FString& Parameters)
	{
		const struct FCase
		{
			const TCHAR* AttemptUuid;
			bool bPhysicsControlComponentAvailable;
			int32 ControlTargetSampleCount;
			int32 ControlTargetNormalWrites;
		} Cases[] =
		{
			{ TEXT("physics-control-not-reached-0-4"), false, 4, 3 },
			{ TEXT("physics-control-not-reached-0-0"), true, 0, 0 }
		};

		for (const FCase& Case : Cases)
		{
			const FString AttemptUuid = Case.AttemptUuid;
			const FString TerminalPath = PhysAnimProofArtifactEmitter::BuildTerminalArtifactJsonPath(AttemptUuid);
			const FString SummaryPath = BuildEvidenceSummaryJsonPath(AttemptUuid);
			IFileManager::Get().Delete(*TerminalPath);
			IFileManager::Get().Delete(*SummaryPath);

			FPhysAnimProofArtifactEmitInput Input = MakeProofEmitterInput(AttemptUuid);
			FPhysAnimRunArtifactSnapshot& Artifact = Input.PipelineResult.StateApplyResult.State.TerminalArtifact;
			Artifact.bPhysicsControlComponentAvailable = Case.bPhysicsControlComponentAvailable;
			Artifact.ControlTargetSampleCount = Case.ControlTargetSampleCount;
			Artifact.ControlTargetNormalWrites = Case.ControlTargetNormalWrites;
			Artifact.ControlTargetTotalWrites = Case.ControlTargetSampleCount + Case.ControlTargetNormalWrites;
			Artifact.ControlledBodyCount = 11;

			const FPhysAnimProofArtifactEmitResult EmitResult =
				PhysAnimProofArtifactEmitter::EmitTerminalArtifactAndWriteJson(Input);
			TestTrue(TEXT("Terminal artifact writes successfully for PhysicsControl NotReached evidence"), EmitResult.bJsonWritten);

			FString TerminalJson;
			TestTrue(TEXT("PhysicsControl terminal artifact is readable"), FFileHelper::LoadFileToString(TerminalJson, *TerminalPath));
			TSharedPtr<FJsonObject> TerminalJsonObject;
			TestTrue(
				TEXT("PhysicsControl terminal artifact parses"),
				FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TerminalJson), TerminalJsonObject));

			bool bPhysicsControlComponentAvailable = true;
			double ControlTargetSampleCount = -1.0;
			TestTrue(
				TEXT("PhysicsControl terminal artifact preserves physics_control_component_available"),
				TerminalJsonObject.IsValid() &&
					TerminalJsonObject->TryGetBoolField(TEXT("physics_control_component_available"), bPhysicsControlComponentAvailable) &&
					bPhysicsControlComponentAvailable == Case.bPhysicsControlComponentAvailable);
			TestTrue(
				TEXT("PhysicsControl terminal artifact preserves control_target_sample_count"),
				TerminalJsonObject.IsValid() &&
					TerminalJsonObject->TryGetNumberField(TEXT("control_target_sample_count"), ControlTargetSampleCount) &&
					ControlTargetSampleCount == static_cast<double>(Case.ControlTargetSampleCount));

			FString SummaryJson;
			TestTrue(TEXT("PhysicsControl summary sidecar is readable"), FFileHelper::LoadFileToString(SummaryJson, *SummaryPath));

			FPhysAnimEvidenceSummary Parsed;
			TestTrue(TEXT("PhysicsControl summary sidecar parses"), DeserializeFromJsonString(SummaryJson, Parsed));

			const FPhysAnimEvidenceSummarySegment* PhysicsControlSegment = FindSegment(Parsed, TEXT("PhysicsControl"));
			TestNotNull(TEXT("PhysicsControl segment exists in summary"), PhysicsControlSegment);
			if (!PhysicsControlSegment)
			{
				IFileManager::Get().Delete(*TerminalPath);
				IFileManager::Get().Delete(*SummaryPath);
				return false;
			}

			TestEqual(
				TEXT("PhysicsControl sample_count remains tied to the classifier inputs"),
				PhysicsControlSegment->Metrics.SampleCount,
				Case.ControlTargetSampleCount);
			TestEqual(
				TEXT("PhysicsControl state is NotReached"),
				static_cast<uint8>(PhysicsControlSegment->State),
				static_cast<uint8>(EPhysAnimEvidenceBaselineSegmentState::NotReached));

			IFileManager::Get().Delete(*TerminalPath);
			IFileManager::Get().Delete(*SummaryPath);
		}

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimEvidenceSummaryPhysicsControlSampleCountEvidenceTest,
		"PhysAnim.EvidenceSummary.PhysicsControlSampleCountEvidence",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimEvidenceSummaryPhysicsControlSampleCountEvidenceTest::RunTest(const FString& Parameters)
	{
		const FString AttemptUuid = TEXT("physics-control-sample-count-evidence");
		const FString TerminalPath = PhysAnimProofArtifactEmitter::BuildTerminalArtifactJsonPath(AttemptUuid);
		const FString SummaryPath = BuildEvidenceSummaryJsonPath(AttemptUuid);
		IFileManager::Get().Delete(*TerminalPath);
		IFileManager::Get().Delete(*SummaryPath);

		FPhysAnimProofArtifactEmitInput Input = MakeProofEmitterInput(AttemptUuid);
		FPhysAnimRunArtifactSnapshot& Artifact = Input.PipelineResult.StateApplyResult.State.TerminalArtifact;
		Artifact.bPhysicsControlComponentAvailable = true;
		Artifact.ControlTargetSampleCount = 17;
		Artifact.ControlTargetNormalWrites = 13;
		Artifact.ControlTargetTotalWrites = 17;
		Artifact.ControlledBodyCount = 11;

		const FPhysAnimProofArtifactEmitResult EmitResult =
			PhysAnimProofArtifactEmitter::EmitTerminalArtifactAndWriteJson(Input);
		TestTrue(TEXT("Terminal artifact writes successfully for PhysicsControl sample count evidence"), EmitResult.bJsonWritten);

		FString TerminalJson;
		TestTrue(TEXT("PhysicsControl terminal artifact is readable"), FFileHelper::LoadFileToString(TerminalJson, *TerminalPath));
		TSharedPtr<FJsonObject> TerminalJsonObject;
		TestTrue(
			TEXT("PhysicsControl terminal artifact parses"),
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TerminalJson), TerminalJsonObject));

		double ControlTargetSampleCount = 0.0;
		TestTrue(
			TEXT("PhysicsControl terminal artifact includes control_target_sample_count 17"),
			TerminalJsonObject.IsValid() &&
				TerminalJsonObject->TryGetNumberField(TEXT("control_target_sample_count"), ControlTargetSampleCount) &&
				ControlTargetSampleCount == 17.0);

		FString SummaryJson;
		TestTrue(TEXT("PhysicsControl summary sidecar is readable"), FFileHelper::LoadFileToString(SummaryJson, *SummaryPath));

		FPhysAnimEvidenceSummary Parsed;
		TestTrue(TEXT("PhysicsControl summary sidecar parses"), DeserializeFromJsonString(SummaryJson, Parsed));

		const FPhysAnimEvidenceSummarySegment* PhysicsControlSegment = FindSegment(Parsed, TEXT("PhysicsControl"));
		TestNotNull(TEXT("PhysicsControl segment exists in summary"), PhysicsControlSegment);
		if (!PhysicsControlSegment)
		{
			IFileManager::Get().Delete(*TerminalPath);
			IFileManager::Get().Delete(*SummaryPath);
			return false;
		}

		TestEqual(
			TEXT("PhysicsControl sample_count matches control target sample count"),
			PhysicsControlSegment->Metrics.SampleCount,
			17);

		IFileManager::Get().Delete(*TerminalPath);
		IFileManager::Get().Delete(*SummaryPath);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimEvidenceSummaryPhysicsControlNormalWriteEvidenceTest,
		"PhysAnim.EvidenceSummary.PhysicsControlNormalWriteEvidence",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimEvidenceSummaryPhysicsControlNormalWriteEvidenceTest::RunTest(const FString& Parameters)
	{
		const FString AttemptUuid = TEXT("physics-control-normal-write-evidence");
		const FString TerminalPath = PhysAnimProofArtifactEmitter::BuildTerminalArtifactJsonPath(AttemptUuid);
		const FString SummaryPath = BuildEvidenceSummaryJsonPath(AttemptUuid);
		IFileManager::Get().Delete(*TerminalPath);
		IFileManager::Get().Delete(*SummaryPath);

		FPhysAnimProofArtifactEmitInput Input = MakeProofEmitterInput(AttemptUuid);
		FPhysAnimRunArtifactSnapshot& Artifact = Input.PipelineResult.StateApplyResult.State.TerminalArtifact;
		Artifact.bPhysicsControlComponentAvailable = true;
		Artifact.ControlTargetSampleCount = 17;
		Artifact.ControlTargetNormalWrites = 13;
		Artifact.ControlTargetTotalWrites = 17;
		Artifact.ControlledBodyCount = 11;

		const FPhysAnimProofArtifactEmitResult EmitResult =
			PhysAnimProofArtifactEmitter::EmitTerminalArtifactAndWriteJson(Input);
		TestTrue(TEXT("Terminal artifact writes successfully for PhysicsControl normal write evidence"), EmitResult.bJsonWritten);

		FString TerminalJson;
		TestTrue(TEXT("PhysicsControl terminal artifact is readable"), FFileHelper::LoadFileToString(TerminalJson, *TerminalPath));
		TSharedPtr<FJsonObject> TerminalJsonObject;
		TestTrue(
			TEXT("PhysicsControl terminal artifact parses"),
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TerminalJson), TerminalJsonObject));

		double ControlTargetNormalWrites = 0.0;
		TestTrue(
			TEXT("PhysicsControl terminal artifact includes control_target_normal_writes 13"),
			TerminalJsonObject.IsValid() &&
				TerminalJsonObject->TryGetNumberField(TEXT("control_target_normal_writes"), ControlTargetNormalWrites) &&
				ControlTargetNormalWrites == 13.0);

		FString SummaryJson;
		TestTrue(TEXT("PhysicsControl summary sidecar is readable"), FFileHelper::LoadFileToString(SummaryJson, *SummaryPath));

		FPhysAnimEvidenceSummary Parsed;
		TestTrue(TEXT("PhysicsControl summary sidecar parses"), DeserializeFromJsonString(SummaryJson, Parsed));

		const FPhysAnimEvidenceSummarySegment* PhysicsControlSegment = FindSegment(Parsed, TEXT("PhysicsControl"));
		TestNotNull(TEXT("PhysicsControl segment exists in summary"), PhysicsControlSegment);
		if (!PhysicsControlSegment)
		{
			IFileManager::Get().Delete(*TerminalPath);
			IFileManager::Get().Delete(*SummaryPath);
			return false;
		}

		const double ExpectedConfidence = 13.0 / 17.0;
		TestEqual(
			TEXT("PhysicsControl sample_count matches control target sample count"),
			PhysicsControlSegment->Metrics.SampleCount,
			17);
		TestTrue(
			TEXT("PhysicsControl confidence uses normal writes over sample count"),
			FMath::Abs(PhysicsControlSegment->Metrics.Confidence - ExpectedConfidence) <= 1.0e-6);

		IFileManager::Get().Delete(*TerminalPath);
		IFileManager::Get().Delete(*SummaryPath);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimEvidenceSummaryPhysicsControlTotalWriteEvidenceTest,
		"PhysAnim.EvidenceSummary.PhysicsControlTotalWriteEvidence",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimEvidenceSummaryPhysicsControlTotalWriteEvidenceTest::RunTest(const FString& Parameters)
	{
		const FString AttemptUuid = TEXT("physics-control-total-write-evidence");
		const FString TerminalPath = PhysAnimProofArtifactEmitter::BuildTerminalArtifactJsonPath(AttemptUuid);
		const FString SummaryPath = BuildEvidenceSummaryJsonPath(AttemptUuid);
		IFileManager::Get().Delete(*TerminalPath);
		IFileManager::Get().Delete(*SummaryPath);

		FPhysAnimProofArtifactEmitInput Input = MakeProofEmitterInput(AttemptUuid);
		FPhysAnimRunArtifactSnapshot& Artifact = Input.PipelineResult.StateApplyResult.State.TerminalArtifact;
		Artifact.bPhysicsControlComponentAvailable = true;
		Artifact.ControlTargetSampleCount = 17;
		Artifact.ControlTargetNormalWrites = 13;
		Artifact.ControlTargetTotalWrites = 19;
		Artifact.ControlledBodyCount = 11;

		const FPhysAnimProofArtifactEmitResult EmitResult =
			PhysAnimProofArtifactEmitter::EmitTerminalArtifactAndWriteJson(Input);
		TestTrue(TEXT("Terminal artifact writes successfully for PhysicsControl total write evidence"), EmitResult.bJsonWritten);

		FString TerminalJson;
		TestTrue(TEXT("PhysicsControl terminal artifact is readable"), FFileHelper::LoadFileToString(TerminalJson, *TerminalPath));
		TSharedPtr<FJsonObject> TerminalJsonObject;
		TestTrue(
			TEXT("PhysicsControl terminal artifact parses"),
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TerminalJson), TerminalJsonObject));

		double ControlTargetTotalWrites = 0.0;
		TestTrue(
			TEXT("PhysicsControl terminal artifact includes control_target_total_writes 19"),
			TerminalJsonObject.IsValid() &&
				TerminalJsonObject->TryGetNumberField(TEXT("control_target_total_writes"), ControlTargetTotalWrites) &&
				ControlTargetTotalWrites == 19.0);

		FString SummaryJson;
		TestTrue(TEXT("PhysicsControl summary sidecar is readable"), FFileHelper::LoadFileToString(SummaryJson, *SummaryPath));

		FPhysAnimEvidenceSummary Parsed;
		TestTrue(TEXT("PhysicsControl summary sidecar parses"), DeserializeFromJsonString(SummaryJson, Parsed));

		const FPhysAnimEvidenceSummarySegment* PhysicsControlSegment = FindSegment(Parsed, TEXT("PhysicsControl"));
		TestNotNull(TEXT("PhysicsControl segment exists in summary"), PhysicsControlSegment);
		if (!PhysicsControlSegment)
		{
			IFileManager::Get().Delete(*TerminalPath);
			IFileManager::Get().Delete(*SummaryPath);
			return false;
		}

		const double ExpectedConfidence = 13.0 / 17.0;
		TestEqual(
			TEXT("PhysicsControl sample_count remains tied to control target sample count"),
			PhysicsControlSegment->Metrics.SampleCount,
			17);
		TestTrue(
			TEXT("PhysicsControl confidence remains tied to normal-write ratio"),
			FMath::Abs(PhysicsControlSegment->Metrics.Confidence - ExpectedConfidence) <= 1.0e-6);

		IFileManager::Get().Delete(*TerminalPath);
		IFileManager::Get().Delete(*SummaryPath);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimEvidenceSummaryPhysicsControlTargetDeltaEvidenceTest,
		"PhysAnim.EvidenceSummary.PhysicsControlTargetDeltaEvidence",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimEvidenceSummaryPhysicsControlTargetDeltaEvidenceTest::RunTest(const FString& Parameters)
	{
		const FString AttemptUuid = TEXT("physics-control-target-delta-evidence");
		const FString TerminalPath = PhysAnimProofArtifactEmitter::BuildTerminalArtifactJsonPath(AttemptUuid);
		const FString SummaryPath = BuildEvidenceSummaryJsonPath(AttemptUuid);
		IFileManager::Get().Delete(*TerminalPath);
		IFileManager::Get().Delete(*SummaryPath);

		FPhysAnimProofArtifactEmitInput Input = MakeProofEmitterInput(AttemptUuid);
		FPhysAnimRunArtifactSnapshot& Artifact = Input.PipelineResult.StateApplyResult.State.TerminalArtifact;
		Artifact.bPhysicsControlComponentAvailable = true;
		Artifact.ControlTargetSampleCount = 17;
		Artifact.ControlTargetNormalWrites = 13;
		Artifact.ControlTargetTotalWrites = 17;
		Artifact.ControlTargetMaxDeltaDeg = 6.5;
		Artifact.ControlTargetMeanDeltaDegMax = 2.25;
		Artifact.ControlledBodyCount = 11;

		const FPhysAnimProofArtifactEmitResult EmitResult =
			PhysAnimProofArtifactEmitter::EmitTerminalArtifactAndWriteJson(Input);
		TestTrue(TEXT("Terminal artifact writes successfully for PhysicsControl target delta evidence"), EmitResult.bJsonWritten);

		FString TerminalJson;
		TestTrue(TEXT("PhysicsControl terminal artifact is readable"), FFileHelper::LoadFileToString(TerminalJson, *TerminalPath));
		TSharedPtr<FJsonObject> TerminalJsonObject;
		TestTrue(
			TEXT("PhysicsControl terminal artifact parses"),
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TerminalJson), TerminalJsonObject));

		double ControlTargetMaxDeltaDeg = 0.0;
		double ControlTargetMeanDeltaDegMax = 0.0;
		TestTrue(
			TEXT("PhysicsControl terminal artifact includes control_target_max_delta_deg 6.5"),
			TerminalJsonObject.IsValid() &&
				TerminalJsonObject->TryGetNumberField(TEXT("control_target_max_delta_deg"), ControlTargetMaxDeltaDeg) &&
				ControlTargetMaxDeltaDeg == 6.5);
		TestTrue(
			TEXT("PhysicsControl terminal artifact includes control_target_mean_delta_deg_max 2.25"),
			TerminalJsonObject.IsValid() &&
				TerminalJsonObject->TryGetNumberField(TEXT("control_target_mean_delta_deg_max"), ControlTargetMeanDeltaDegMax) &&
				ControlTargetMeanDeltaDegMax == 2.25);

		FString SummaryJson;
		TestTrue(TEXT("PhysicsControl summary sidecar is readable"), FFileHelper::LoadFileToString(SummaryJson, *SummaryPath));

		FPhysAnimEvidenceSummary Parsed;
		TestTrue(TEXT("PhysicsControl summary sidecar parses"), DeserializeFromJsonString(SummaryJson, Parsed));

		const FPhysAnimEvidenceSummarySegment* PhysicsControlSegment = FindSegment(Parsed, TEXT("PhysicsControl"));
		TestNotNull(TEXT("PhysicsControl segment exists in summary"), PhysicsControlSegment);
		if (!PhysicsControlSegment)
		{
			IFileManager::Get().Delete(*TerminalPath);
			IFileManager::Get().Delete(*SummaryPath);
			return false;
		}

		TestEqual(
			TEXT("PhysicsControl sample_count remains active at 17"),
			PhysicsControlSegment->Metrics.SampleCount,
			17);
		TestEqual(
			TEXT("PhysicsControl state remains Active"),
			static_cast<uint8>(PhysicsControlSegment->State),
			static_cast<uint8>(EPhysAnimEvidenceBaselineSegmentState::Active));
		TestTrue(
			TEXT("PhysicsControl confidence remains 13/17"),
			FMath::Abs(PhysicsControlSegment->Metrics.Confidence - (13.0 / 17.0)) <= 1.0e-6);

		IFileManager::Get().Delete(*TerminalPath);
		IFileManager::Get().Delete(*SummaryPath);
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
		TestTrue(TEXT("sample_count field is present"), Json.Contains(TEXT("\"sample_count\":")));
		TestTrue(TEXT("confidence field is present"), Json.Contains(TEXT("\"confidence\":")));
		TestTrue(TEXT("score field is present"), Json.Contains(TEXT("\"score\":")));
		TestTrue(TEXT("selected_source_identity field is present"), Json.Contains(TEXT("\"selected_source_identity\":")));
		TestTrue(TEXT("selected_source_time field is present"), Json.Contains(TEXT("\"selected_source_time\":")));
		TestTrue(TEXT("consecutive_invalid_sample_count field is present"), Json.Contains(TEXT("\"consecutive_invalid_sample_count\":")));
		TestTrue(TEXT("inference_attempt_count field is present"), Json.Contains(TEXT("\"inference_attempt_count\":")));
		TestTrue(TEXT("inference_failure_count field is present"), Json.Contains(TEXT("\"inference_failure_count\":")));
		TestTrue(TEXT("inference_latency_ms_max field is present"), Json.Contains(TEXT("\"inference_latency_ms_max\":")));
		TestTrue(TEXT("model_loaded field is present"), Json.Contains(TEXT("\"model_loaded\":")));
		TestTrue(TEXT("runtime_name field is present"), Json.Contains(TEXT("\"runtime_name\":")));
		TestTrue(TEXT("input_buffers_finite field is present"), Json.Contains(TEXT("\"input_buffers_finite\":")));
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
		FPhysAnimEvidenceSummaryPhysicsControlRawPolicyOffsetEvidenceTest,
		"PhysAnim.EvidenceSummary.PhysicsControlRawPolicyOffsetEvidence",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimEvidenceSummaryPhysicsControlRawPolicyOffsetEvidenceTest::RunTest(const FString& Parameters)
	{
		const FString AttemptUuid = TEXT("physics-control-raw-policy-offset-attempt");
		const FString TerminalPath = PhysAnimProofArtifactEmitter::BuildTerminalArtifactJsonPath(AttemptUuid);
		const FString SummaryPath = BuildEvidenceSummaryJsonPath(AttemptUuid);
		IFileManager::Get().Delete(*TerminalPath);
		IFileManager::Get().Delete(*SummaryPath);

		FPhysAnimProofArtifactEmitInput Input = MakeProofEmitterInput(AttemptUuid);
		Input.PipelineResult.StateApplyResult.State.TerminalArtifact.bPhysicsControlComponentAvailable = true;
		Input.PipelineResult.StateApplyResult.State.TerminalArtifact.ControlTargetSampleCount = 17;
		Input.PipelineResult.StateApplyResult.State.TerminalArtifact.ControlTargetNormalWrites = 13;
		Input.PipelineResult.StateApplyResult.State.TerminalArtifact.ControlTargetTotalWrites = 17;
		Input.PipelineResult.StateApplyResult.State.TerminalArtifact.ControlTargetMaxDeltaDeg = 6.5;
		Input.PipelineResult.StateApplyResult.State.TerminalArtifact.ControlTargetMaxRawPolicyOffsetDeg = 8.75;
		Input.PipelineResult.StateApplyResult.State.TerminalArtifact.ControlTargetMeanRawPolicyOffsetDegMax = 3.25;
		Input.PipelineResult.StateApplyResult.State.TerminalArtifact.ControlledBodyCount = 11;
		const FPhysAnimProofArtifactEmitResult TerminalResult =
			PhysAnimProofArtifactEmitter::EmitTerminalArtifactAndWriteJson(Input);

		TestTrue(TEXT("Terminal proof artifact writes successfully"), TerminalResult.bJsonWritten);
		TestEqual(TEXT("Terminal proof artifact path is unchanged"), TerminalResult.JsonPath, TerminalPath);

		FString TerminalJson;
		TestTrue(TEXT("Terminal proof artifact is readable"), FFileHelper::LoadFileToString(TerminalJson, *TerminalPath));
		TSharedPtr<FJsonObject> TerminalJsonObject;
		TestTrue(
			TEXT("Terminal proof artifact parses"),
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TerminalJson), TerminalJsonObject));

		double ControlTargetMaxRawPolicyOffsetDeg = 0.0;
		double ControlTargetMeanRawPolicyOffsetDegMax = 0.0;
		TestTrue(
			TEXT("Terminal JSON includes control_target_max_raw_policy_offset_deg"),
			TerminalJsonObject.IsValid() &&
				TerminalJsonObject->TryGetNumberField(TEXT("control_target_max_raw_policy_offset_deg"), ControlTargetMaxRawPolicyOffsetDeg) &&
				FMath::IsNearlyEqual(ControlTargetMaxRawPolicyOffsetDeg, 8.75));
		TestTrue(
			TEXT("Terminal JSON includes control_target_mean_raw_policy_offset_deg_max"),
			TerminalJsonObject.IsValid() &&
				TerminalJsonObject->TryGetNumberField(TEXT("control_target_mean_raw_policy_offset_deg_max"), ControlTargetMeanRawPolicyOffsetDegMax) &&
				FMath::IsNearlyEqual(ControlTargetMeanRawPolicyOffsetDegMax, 3.25));

		FString SummaryJson;
		TestTrue(TEXT("PhysicsControl summary sidecar is readable"), FFileHelper::LoadFileToString(SummaryJson, *SummaryPath));

		FPhysAnimEvidenceSummary Parsed;
		TestTrue(TEXT("PhysicsControl summary sidecar parses"), DeserializeFromJsonString(SummaryJson, Parsed));

		const FPhysAnimEvidenceSummarySegment* PhysicsControlSegment = FindSegment(Parsed, TEXT("PhysicsControl"));
		TestNotNull(TEXT("PhysicsControl segment exists in summary"), PhysicsControlSegment);
		if (!PhysicsControlSegment)
		{
			IFileManager::Get().Delete(*TerminalPath);
			IFileManager::Get().Delete(*SummaryPath);
			return false;
		}

		TestEqual(TEXT("PhysicsControl summary remains Active"), static_cast<uint8>(PhysicsControlSegment->State), static_cast<uint8>(EPhysAnimEvidenceBaselineSegmentState::Active));
		TestEqual(TEXT("PhysicsControl summary sample_count remains 17"), PhysicsControlSegment->Metrics.SampleCount, 17);
		TestTrue(
			TEXT("PhysicsControl summary confidence remains 13/17"),
			FMath::Abs(PhysicsControlSegment->Metrics.Confidence - (13.0 / 17.0)) <= 1.0e-6);
		TestEqual(TEXT("PhysicsControl summary score remains 6.5"), PhysicsControlSegment->Metrics.Score, 6.5);

		IFileManager::Get().Delete(*TerminalPath);
		IFileManager::Get().Delete(*SummaryPath);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimEvidenceSummaryPhysicsControlReachedButInactiveEvidenceTest,
		"PhysAnim.EvidenceSummary.PhysicsControlReachedButInactiveEvidence",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimEvidenceSummaryPhysicsControlReachedButInactiveEvidenceTest::RunTest(const FString& Parameters)
	{
		const FString AttemptUuid = TEXT("physics-control-reached-but-inactive-evidence");
		const FString TerminalPath = PhysAnimProofArtifactEmitter::BuildTerminalArtifactJsonPath(AttemptUuid);
		const FString SummaryPath = BuildEvidenceSummaryJsonPath(AttemptUuid);
		IFileManager::Get().Delete(*TerminalPath);
		IFileManager::Get().Delete(*SummaryPath);

		FPhysAnimProofArtifactEmitInput Input = MakeProofEmitterInput(AttemptUuid);
		Input.PipelineResult.StateApplyResult.State.TerminalArtifact.bPhysicsControlComponentAvailable = true;
		Input.PipelineResult.StateApplyResult.State.TerminalArtifact.ControlTargetSampleCount = 5;
		Input.PipelineResult.StateApplyResult.State.TerminalArtifact.ControlTargetNormalWrites = 0;
		Input.PipelineResult.StateApplyResult.State.TerminalArtifact.ControlTargetTotalWrites = 5;
		Input.PipelineResult.StateApplyResult.State.TerminalArtifact.ControlledBodyCount = 11;

		const FPhysAnimProofArtifactEmitResult EmitResult =
			PhysAnimProofArtifactEmitter::EmitTerminalArtifactAndWriteJson(Input);
		TestTrue(TEXT("Terminal artifact writes successfully for PhysicsControl reached-but-inactive evidence"), EmitResult.bJsonWritten);

		FString TerminalJson;
		TestTrue(TEXT("PhysicsControl terminal artifact is readable"), FFileHelper::LoadFileToString(TerminalJson, *TerminalPath));
		TSharedPtr<FJsonObject> TerminalJsonObject;
		TestTrue(
			TEXT("PhysicsControl terminal artifact parses"),
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TerminalJson), TerminalJsonObject));

		bool bPhysicsControlComponentAvailable = false;
		double ControlTargetSampleCount = 0.0;
		double ControlTargetNormalWrites = 0.0;
		double ControlTargetTotalWrites = 0.0;
		TestTrue(
			TEXT("Terminal JSON preserves physics_control_component_available=true"),
			TerminalJsonObject.IsValid() &&
				TerminalJsonObject->TryGetBoolField(TEXT("physics_control_component_available"), bPhysicsControlComponentAvailable) &&
				bPhysicsControlComponentAvailable);
		TestTrue(
			TEXT("Terminal JSON preserves control_target_sample_count=5"),
			TerminalJsonObject.IsValid() &&
				TerminalJsonObject->TryGetNumberField(TEXT("control_target_sample_count"), ControlTargetSampleCount) &&
				ControlTargetSampleCount == 5.0);
		TestTrue(
			TEXT("Terminal JSON preserves control_target_normal_writes=0"),
			TerminalJsonObject.IsValid() &&
				TerminalJsonObject->TryGetNumberField(TEXT("control_target_normal_writes"), ControlTargetNormalWrites) &&
				ControlTargetNormalWrites == 0.0);
		TestTrue(
			TEXT("Terminal JSON preserves control_target_total_writes=5"),
			TerminalJsonObject.IsValid() &&
				TerminalJsonObject->TryGetNumberField(TEXT("control_target_total_writes"), ControlTargetTotalWrites) &&
				ControlTargetTotalWrites == 5.0);

		FString SummaryJson;
		TestTrue(TEXT("PhysicsControl summary sidecar is readable"), FFileHelper::LoadFileToString(SummaryJson, *SummaryPath));

		FPhysAnimEvidenceSummary Parsed;
		TestTrue(TEXT("PhysicsControl summary sidecar parses"), DeserializeFromJsonString(SummaryJson, Parsed));

		const FPhysAnimEvidenceSummarySegment* PhysicsControlSegment = FindSegment(Parsed, TEXT("PhysicsControl"));
		TestNotNull(TEXT("PhysicsControl segment exists in summary"), PhysicsControlSegment);
		if (!PhysicsControlSegment)
		{
			IFileManager::Get().Delete(*TerminalPath);
			IFileManager::Get().Delete(*SummaryPath);
			return false;
		}

		TestEqual(
			TEXT("PhysicsControl summary sample_count is 5"),
			PhysicsControlSegment->Metrics.SampleCount,
			5);
		TestEqual(
			TEXT("PhysicsControl state serializes as ReachedButInactive"),
			static_cast<uint8>(PhysicsControlSegment->State),
			static_cast<uint8>(EPhysAnimEvidenceBaselineSegmentState::ReachedButInactive));
		TestTrue(
			TEXT("PhysicsControl summary confidence is 0.0"),
			FMath::IsNearlyEqual(PhysicsControlSegment->Metrics.Confidence, 0.0));

		IFileManager::Get().Delete(*TerminalPath);
		IFileManager::Get().Delete(*SummaryPath);
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
		TestTrue(TEXT("Sidecar captures runtime command metadata when command context is available"), Parsed.CommandMetadata.IsSet());
		if (Parsed.CommandMetadata.IsSet())
		{
			const FPhysAnimEvidenceSummaryCommandMetadata& Metadata = Parsed.CommandMetadata.GetValue();
			TestEqual(TEXT("Sidecar command metadata names the runtime evidence command"), Metadata.CommandName, FString(TEXT("PhysAnim.LiveRuntimeEvidence")));
			TestEqual(TEXT("Sidecar command metadata mirrors the UE command line"), Metadata.CommandLine, FString(FCommandLine::Get()));
			TestEqual(TEXT("Sidecar command metadata captures the working directory"), Metadata.WorkingDirectory, FPaths::LaunchDir());
		}

		FString TerminalJson;
		TestTrue(TEXT("Terminal proof artifact is readable"), FFileHelper::LoadFileToString(TerminalJson, *TerminalPath));
		TestFalse(TEXT("Terminal proof artifact preserves backward-compatible JSON shape"), TerminalJson.Contains(TEXT("\"command_metadata\"")));
		TestTrue(TEXT("Terminal proof artifact captures renderer-facing motion sample count"), TerminalJson.Contains(TEXT("\"renderer_facing_motion_sample_count\":")));
		TestTrue(TEXT("Terminal proof artifact captures renderer-facing motion active sample count"), TerminalJson.Contains(TEXT("\"renderer_facing_motion_active_sample_count\":")));
		TestTrue(TEXT("Terminal proof artifact captures renderer-facing motion max root drift"), TerminalJson.Contains(TEXT("\"renderer_facing_motion_max_root_world_position_drift_cm\":")));

		IFileManager::Get().Delete(*TerminalPath);
		IFileManager::Get().Delete(*SummaryPath);
		return true;
	}
}
