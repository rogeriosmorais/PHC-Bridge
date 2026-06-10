#include "PhysAnimEvidenceSummary.h"
#include "PhysAnimProofArtifactEmitter.h"
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
}
