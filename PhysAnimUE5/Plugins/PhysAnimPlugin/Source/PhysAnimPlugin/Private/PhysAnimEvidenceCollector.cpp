#include "PhysAnimEvidenceCollector.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	FString PhysAnimEvidenceCollector_SanitizeFileToken(const FString& Value)
	{
		FString Sanitized = FPaths::MakeValidFileName(Value);
		Sanitized.ReplaceInline(TEXT(" "), TEXT("_"));
		return Sanitized.IsEmpty() ? TEXT("unknown") : Sanitized;
	}

	bool PhysAnimEvidenceCollector_TryReadTextFile(const FString& Path, FString& OutContents)
	{
		return FFileHelper::LoadFileToString(OutContents, *Path);
	}

	TArray<FString> PhysAnimEvidenceCollector_FindFiles(const FString& Directory, const TCHAR* Pattern)
	{
		TArray<FString> FilePaths;
		if (!Directory.IsEmpty())
		{
			IFileManager::Get().FindFilesRecursive(FilePaths, *Directory, Pattern, true, false);
		}

		return FilePaths;
	}

	bool PhysAnimEvidenceCollector_IsLaterFile(
		const FString& CandidatePath,
		const FString& BestPath)
	{
		const FDateTime CandidateTime = IFileManager::Get().GetTimeStamp(*CandidatePath);
		const FDateTime BestTime = IFileManager::Get().GetTimeStamp(*BestPath);

		if (CandidateTime == BestTime)
		{
			return CandidatePath > BestPath;
		}

		return CandidateTime > BestTime;
	}

	FString PhysAnimEvidenceCollector_FindLatestFile(const FString& Directory, const TCHAR* Pattern)
	{
		const TArray<FString> FilePaths = PhysAnimEvidenceCollector_FindFiles(Directory, Pattern);
		if (FilePaths.IsEmpty())
		{
			return FString();
		}

		FString BestPath = FilePaths[0];
		for (int32 Index = 1; Index < FilePaths.Num(); ++Index)
		{
			if (PhysAnimEvidenceCollector_IsLaterFile(FilePaths[Index], BestPath))
			{
				BestPath = FilePaths[Index];
			}
		}

		return BestPath;
	}

	TOptional<bool> PhysAnimEvidenceCollector_TryReadExplicitPassField(const TSharedPtr<FJsonObject>& JsonObject)
	{
		bool bPassed = false;
		if (JsonObject->TryGetBoolField(TEXT("bPassed"), bPassed))
		{
			return bPassed;
		}

		if (JsonObject->TryGetBoolField(TEXT("passed"), bPassed))
		{
			return bPassed;
		}

		return TOptional<bool>();
	}

	bool PhysAnimEvidenceCollector_TryParseTerminalArtifactJson(
		const FString& JsonString,
		FPhysAnimEvidenceCollectorTerminalArtifactResult& OutResult)
	{
		TSharedPtr<FJsonObject> JsonObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
		if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
		{
			return false;
		}

		OutResult = FPhysAnimEvidenceCollectorTerminalArtifactResult();
		OutResult.bFound = true;

		if (!JsonObject->TryGetStringField(TEXT("attempt_uuid"), OutResult.TerminalArtifact.AttemptUuid))
		{
			return false;
		}

		JsonObject->TryGetNumberField(TEXT("timestamp"), OutResult.TerminalArtifact.Timestamp);
		JsonObject->TryGetStringField(TEXT("baseline_id"), OutResult.TerminalArtifact.BaselineId);
		JsonObject->TryGetStringField(TEXT("standing_reference_id"), OutResult.TerminalArtifact.StandingReferenceId);

		double TerminalReasonValue = 0.0;
		if (JsonObject->TryGetNumberField(TEXT("terminal_reason"), TerminalReasonValue))
		{
			OutResult.TerminalArtifact.TerminalReason = static_cast<EPhysAnimTerminalReason>(static_cast<int32>(TerminalReasonValue));
		}

		FString TerminalReasonName;
		if (JsonObject->TryGetStringField(TEXT("terminal_reason_name"), TerminalReasonName))
		{
			OutResult.TerminalArtifact.TerminalReason =
				TerminalReasonName.Equals(TEXT("None"), ESearchCase::IgnoreCase)
					? EPhysAnimTerminalReason::None
					: OutResult.TerminalArtifact.TerminalReason;
		}

		OutResult.TerminalArtifact.bTerminalFrameArtifactCaptured = true;

		const TOptional<bool> ExplicitPass = PhysAnimEvidenceCollector_TryReadExplicitPassField(JsonObject);
		if (ExplicitPass.IsSet())
		{
			OutResult.bExplicitPassProvided = true;
			OutResult.bExplicitPass = ExplicitPass.GetValue();
		}
		else
		{
			OutResult.bExplicitPassProvided = false;
			OutResult.bExplicitPass = OutResult.TerminalArtifact.TerminalReason == EPhysAnimTerminalReason::None;
		}

		return true;
	}

	bool PhysAnimEvidenceCollector_TryParseEvidenceSummaryJson(
		const FString& JsonString,
		FPhysAnimEvidenceCollectorEvidenceSummaryResult& OutResult)
	{
		FPhysAnimEvidenceSummary ParsedSummary;
		if (!PhysAnimEvidenceSummary::DeserializeFromJsonString(JsonString, ParsedSummary))
		{
			return false;
		}

		OutResult.bFound = true;
		OutResult.Summary = MoveTemp(ParsedSummary);
		return true;
	}

	bool PhysAnimEvidenceCollector_TryParseLogSignal(
		const FString& LogContents,
		FPhysAnimEvidenceCollectorLogSignal& OutSignal)
	{
		OutSignal.bFound = true;
		OutSignal.LogContents = LogContents;
		OutSignal.bPass = LogContents.Contains(TEXT("verdict=PASS"));
		return true;
	}

	FString PhysAnimEvidenceCollector_BuildSegmentList(const TArray<FString>& SegmentNames)
	{
		return SegmentNames.IsEmpty() ? TEXT("none") : FString::Join(SegmentNames, TEXT(", "));
	}

	FString PhysAnimEvidenceCollector_BuildTerminalReasonString(const EPhysAnimTerminalReason Reason)
	{
		switch (Reason)
		{
		case EPhysAnimTerminalReason::None:
			return TEXT("None");
		case EPhysAnimTerminalReason::ActivationSupportFailure:
			return TEXT("ActivationSupportFailure");
		case EPhysAnimTerminalReason::ActivationContinuousSimulationLost:
			return TEXT("ActivationContinuousSimulationLost");
		case EPhysAnimTerminalReason::ActivationTargetDiscontinuity:
			return TEXT("ActivationTargetDiscontinuity");
		case EPhysAnimTerminalReason::ActivationInstabilityThresholdBreach:
			return TEXT("ActivationInstabilityThresholdBreach");
		default:
			return FString::Printf(TEXT("TerminalReason(%d)"), static_cast<int32>(Reason));
		}
	}

	FString PhysAnimEvidenceCollector_BuildVerdictString(const EPhysAnimEvidenceBaselineVerdict Verdict)
	{
		switch (Verdict)
		{
		case EPhysAnimEvidenceBaselineVerdict::ProductSuccessCandidate:
			return TEXT("ProductSuccessCandidate");
		case EPhysAnimEvidenceBaselineVerdict::Diagnostic:
			return TEXT("Diagnostic");
		case EPhysAnimEvidenceBaselineVerdict::Blocked:
			return TEXT("Blocked");
		case EPhysAnimEvidenceBaselineVerdict::Contradictory:
			return TEXT("Contradictory");
		case EPhysAnimEvidenceBaselineVerdict::InsufficientEvidence:
		default:
			return TEXT("InsufficientEvidence");
		}
	}

	FString PhysAnimEvidenceCollector_FindNextBlockingSegment(const FPhysAnimEvidenceSummary& Summary)
	{
		static const TCHAR* SegmentOrder[] =
		{
			TEXT("PoseSearch"),
			TEXT("PhcPolicy"),
			TEXT("PhysicsControl"),
			TEXT("Chaos"),
			TEXT("RendererFacingMotion")
		};

		for (const TCHAR* SegmentName : SegmentOrder)
		{
			const FPhysAnimEvidenceSummarySegment* Segment = nullptr;
			for (const FPhysAnimEvidenceSummarySegment& Candidate : Summary.Segments)
			{
				if (Candidate.SegmentName.Equals(SegmentName, ESearchCase::IgnoreCase))
				{
					Segment = &Candidate;
					break;
				}
			}

			if (!Segment || Segment->State != EPhysAnimEvidenceBaselineSegmentState::Active)
			{
				return SegmentName;
			}
		}

		return TEXT("None");
	}

	void PhysAnimEvidenceCollector_AppendLine(FString& Report, const FString& Line)
	{
		Report += Line;
		Report += LINE_TERMINATOR;
	}

	FString PhysAnimEvidenceCollector_BuildReport(const FPhysAnimEvidenceCollectorResult& Result)
	{
		FString Report;

		PhysAnimEvidenceCollector_AppendLine(Report, TEXT("Actual Evidence:"));
		PhysAnimEvidenceCollector_AppendLine(
			Report,
			FString::Printf(
				TEXT("- Terminal Artifact: %s"),
				Result.TerminalArtifact.bFound ? *Result.TerminalArtifact.TerminalArtifact.AttemptUuid : TEXT("missing")));
		PhysAnimEvidenceCollector_AppendLine(
			Report,
			FString::Printf(
				TEXT("- Evidence Summary: %s"),
				Result.EvidenceSummary.bFound ? *Result.EvidenceSummary.Summary.AttemptUuid : TEXT("missing")));
		PhysAnimEvidenceCollector_AppendLine(
			Report,
			FString::Printf(
				TEXT("- Test Name: %s"),
				Result.EvidenceSummary.bFound ? *Result.EvidenceSummary.Summary.TestName : TEXT("missing")));
		PhysAnimEvidenceCollector_AppendLine(
			Report,
			FString::Printf(
				TEXT("- Terminal Reason: %s"),
				Result.EvidenceSummary.bFound
					? *PhysAnimEvidenceCollector_BuildTerminalReasonString(Result.EvidenceSummary.Summary.TerminalReason)
					: TEXT("missing")));
		PhysAnimEvidenceCollector_AppendLine(
			Report,
			FString::Printf(
				TEXT("- Strict Verdict: %s"),
				Result.EvidenceSummary.bFound
					? *PhysAnimEvidenceCollector_BuildVerdictString(Result.EvidenceSummary.Summary.StrictVerdict)
					: TEXT("missing")));
		PhysAnimEvidenceCollector_AppendLine(
			Report,
			FString::Printf(
				TEXT("- Log Pass: %s"),
				Result.LogSignal.bFound ? (Result.LogSignal.bPass ? TEXT("true") : TEXT("false")) : TEXT("unset")));

		PhysAnimEvidenceCollector_AppendLine(Report, TEXT("Weak Evidence:"));
		if (Result.EvidenceSummary.bFound)
		{
			PhysAnimEvidenceCollector_AppendLine(Report, TEXT("- none"));
		}
		else
		{
			PhysAnimEvidenceCollector_AppendLine(Report, TEXT("- evidence summary missing"));
		}

		PhysAnimEvidenceCollector_AppendLine(Report, TEXT("Contradictions:"));
		TArray<FString> Contradictions;
		if (Result.ClassificationResult.Verdict == EPhysAnimEvidenceBaselineVerdict::Contradictory)
		{
			Contradictions.Add(TEXT("CONTRADICTORY"));
		}
		if (Result.LogSignal.bFound && Result.ClassificationInput.ProofSignals.TerminalProofJsonPassed.IsSet() &&
			Result.LogSignal.bPass != Result.ClassificationInput.ProofSignals.TerminalProofJsonPassed.GetValue())
		{
			Contradictions.Add(TEXT("log pass disagrees with terminal proof"));
		}
		PhysAnimEvidenceCollector_AppendLine(Report, FString::Printf(TEXT("- %s"), *PhysAnimEvidenceCollector_BuildSegmentList(Contradictions)));

		PhysAnimEvidenceCollector_AppendLine(Report, TEXT("Missing Evidence:"));
		TArray<FString> MissingEvidence;
		for (const FPhysAnimEvidenceSummarySegment& Segment : Result.EvidenceSummary.Summary.Segments)
		{
			if (Segment.State == EPhysAnimEvidenceBaselineSegmentState::NotReached)
			{
				MissingEvidence.Add(Segment.SegmentName);
			}
		}
		PhysAnimEvidenceCollector_AppendLine(Report, FString::Printf(TEXT("- %s"), *PhysAnimEvidenceCollector_BuildSegmentList(MissingEvidence)));

		PhysAnimEvidenceCollector_AppendLine(
			Report,
			FString::Printf(TEXT("Next Blocking Segment: %s"), *PhysAnimEvidenceCollector_FindNextBlockingSegment(Result.EvidenceSummary.Summary)));

		PhysAnimEvidenceCollector_AppendLine(
			Report,
			FString::Printf(
				TEXT("Verdict: %s"),
				*PhysAnimEvidenceCollector_BuildVerdictString(Result.ClassificationResult.Verdict)));

		return Report;
	}

	bool PhysAnimEvidenceCollector_LoadLatestTerminalArtifact(
		const FString& Directory,
		FPhysAnimEvidenceCollectorTerminalArtifactResult& OutResult)
	{
		const FString Path = PhysAnimEvidenceCollector_FindLatestFile(Directory, TEXT("*_terminal.json"));
		if (Path.IsEmpty())
		{
			return false;
		}

		FString Json;
		if (!PhysAnimEvidenceCollector_TryReadTextFile(Path, Json))
		{
			return false;
		}

		if (!PhysAnimEvidenceCollector_TryParseTerminalArtifactJson(Json, OutResult))
		{
			return false;
		}

		OutResult.JsonPath = Path;
		return true;
	}

	bool PhysAnimEvidenceCollector_LoadLatestEvidenceSummary(
		const FString& Directory,
		FPhysAnimEvidenceCollectorEvidenceSummaryResult& OutResult)
	{
		const FString Path = PhysAnimEvidenceCollector_FindLatestFile(Directory, TEXT("*_evidence_summary.json"));
		if (Path.IsEmpty())
		{
			return false;
		}

		FString Json;
		if (!PhysAnimEvidenceCollector_TryReadTextFile(Path, Json))
		{
			return false;
		}

		if (!PhysAnimEvidenceCollector_TryParseEvidenceSummaryJson(Json, OutResult))
		{
			return false;
		}

		OutResult.JsonPath = Path;
		return true;
	}

	bool PhysAnimEvidenceCollector_LoadLatestLogSignal(
		const FString& Directory,
		FPhysAnimEvidenceCollectorLogSignal& OutSignal)
	{
		const FString Path = PhysAnimEvidenceCollector_FindLatestFile(Directory, TEXT("*.log"));
		if (Path.IsEmpty())
		{
			return false;
		}

		FString LogContents;
		if (!PhysAnimEvidenceCollector_TryReadTextFile(Path, LogContents))
		{
			return false;
		}

		if (!PhysAnimEvidenceCollector_TryParseLogSignal(LogContents, OutSignal))
		{
			return false;
		}

		OutSignal.LogPath = Path;
		return true;
	}

	EPhysAnimEvidenceBaselineSegmentState PhysAnimEvidenceCollector_GetSummarySegmentState(
		const FPhysAnimEvidenceSummary& Summary,
		const TCHAR* SegmentName)
	{
		for (const FPhysAnimEvidenceSummarySegment& Segment : Summary.Segments)
		{
			if (Segment.SegmentName.Equals(SegmentName, ESearchCase::IgnoreCase))
			{
				return Segment.State;
			}
		}

		return EPhysAnimEvidenceBaselineSegmentState::NotReached;
	}

	FPhysAnimEvidenceBaselineInput PhysAnimEvidenceCollector_BuildClassificationInput(
		const FPhysAnimEvidenceCollectorTerminalArtifactResult& TerminalArtifact,
		const FPhysAnimEvidenceCollectorEvidenceSummaryResult& EvidenceSummary,
		const FPhysAnimEvidenceCollectorLogSignal& LogSignal)
	{
		FPhysAnimEvidenceBaselineInput Input;
		if (EvidenceSummary.bFound)
		{
			Input.Segments.PoseSearch = PhysAnimEvidenceCollector_GetSummarySegmentState(EvidenceSummary.Summary, TEXT("PoseSearch"));
			Input.Segments.PhcPolicy = PhysAnimEvidenceCollector_GetSummarySegmentState(EvidenceSummary.Summary, TEXT("PhcPolicy"));
			Input.Segments.PhysicsControl = PhysAnimEvidenceCollector_GetSummarySegmentState(EvidenceSummary.Summary, TEXT("PhysicsControl"));
			Input.Segments.Chaos = PhysAnimEvidenceCollector_GetSummarySegmentState(EvidenceSummary.Summary, TEXT("Chaos"));
			Input.Segments.RendererFacingMotion = PhysAnimEvidenceCollector_GetSummarySegmentState(EvidenceSummary.Summary, TEXT("RendererFacingMotion"));
		}

		if (EvidenceSummary.bFound)
		{
			Input.TruthFlags = EvidenceSummary.Summary.QualityFlags;
			Input.TruthFlags.bTerminalFailure = EvidenceSummary.Summary.TerminalReason != EPhysAnimTerminalReason::None;
			Input.TruthFlags.bMissingEvidence = false;
		}

		Input.ProofSignals.TerminalProofJsonPassed = TerminalArtifact.bFound
			? TOptional<bool>(TerminalArtifact.bExplicitPass)
			: TOptional<bool>();
		Input.ProofSignals.LogPass = LogSignal.bFound
			? TOptional<bool>(LogSignal.bPass)
			: TOptional<bool>();
		Input.ProofSignals.ArtifactPass = TerminalArtifact.bFound
			? TOptional<bool>(TerminalArtifact.bExplicitPass)
			: TOptional<bool>();

		if (EvidenceSummary.bFound)
		{
			Input.bHoldThresholdSatisfied = EvidenceSummary.Summary.StrictVerdict == EPhysAnimEvidenceBaselineVerdict::ProductSuccessCandidate;
		}

		return Input;
	}
}

namespace PhysAnimEvidenceCollector
{
	FPhysAnimEvidenceCollectorResult Collect(const FPhysAnimEvidenceCollectorInput& Input)
	{
		FPhysAnimEvidenceCollectorResult Result;
		PhysAnimEvidenceCollector_LoadLatestTerminalArtifact(Input.TerminalArtifactDirectory, Result.TerminalArtifact);
		PhysAnimEvidenceCollector_LoadLatestEvidenceSummary(Input.EvidenceSummaryDirectory, Result.EvidenceSummary);
		PhysAnimEvidenceCollector_LoadLatestLogSignal(Input.LogDirectory, Result.LogSignal);

		Result.ClassificationInput = PhysAnimEvidenceCollector_BuildClassificationInput(
			Result.TerminalArtifact,
			Result.EvidenceSummary,
			Result.LogSignal);
		Result.ClassificationResult = PhysAnimEvidenceClassifier::Classify(Result.ClassificationInput);
		Result.Report = PhysAnimEvidenceCollector_BuildReport(Result);
		return Result;
	}
}
