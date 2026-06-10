#include "PhysAnimEvidenceSummary.h"

#include "PhysAnimProofArtifactEmitter.h"

#include "Dom/JsonObject.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	FString PhysAnimEvidenceSummary_SanitizeFileToken(const FString& Value)
	{
		FString Sanitized = FPaths::MakeValidFileName(Value);
		Sanitized.ReplaceInline(TEXT(" "), TEXT("_"));
		return Sanitized.IsEmpty() ? TEXT("unknown") : Sanitized;
	}

	FString PhysAnimEvidenceSummary_EscapeJsonString(const FString& Value)
	{
		FString Escaped;
		Escaped.Reserve(Value.Len() + 8);

		for (const TCHAR Character : Value)
		{
			switch (Character)
			{
			case TEXT('\\'):
				Escaped += TEXT("\\\\");
				break;
			case TEXT('"'):
				Escaped += TEXT("\\\"");
				break;
			case TEXT('\b'):
				Escaped += TEXT("\\b");
				break;
			case TEXT('\f'):
				Escaped += TEXT("\\f");
				break;
			case TEXT('\n'):
				Escaped += TEXT("\\n");
				break;
			case TEXT('\r'):
				Escaped += TEXT("\\r");
				break;
			case TEXT('\t'):
				Escaped += TEXT("\\t");
				break;
			default:
				Escaped.AppendChar(Character);
				break;
			}
		}

		return Escaped;
	}

	void PhysAnimEvidenceSummary_AppendCommaIfNeeded(FString& Json, bool& bFirstField)
	{
		if (!bFirstField)
		{
			Json += TEXT(",");
		}

		bFirstField = false;
	}

	void PhysAnimEvidenceSummary_AppendStringField(FString& Json, bool& bFirstField, const TCHAR* FieldName, const FString& Value)
	{
		PhysAnimEvidenceSummary_AppendCommaIfNeeded(Json, bFirstField);
		Json += FString::Printf(TEXT("\"%s\":\"%s\""), FieldName, *PhysAnimEvidenceSummary_EscapeJsonString(Value));
	}

	void PhysAnimEvidenceSummary_AppendIntField(FString& Json, bool& bFirstField, const TCHAR* FieldName, const int32 Value)
	{
		PhysAnimEvidenceSummary_AppendCommaIfNeeded(Json, bFirstField);
		Json += FString::Printf(TEXT("\"%s\":%d"), FieldName, Value);
	}

	void PhysAnimEvidenceSummary_AppendDoubleField(FString& Json, bool& bFirstField, const TCHAR* FieldName, const double Value)
	{
		PhysAnimEvidenceSummary_AppendCommaIfNeeded(Json, bFirstField);
		Json += FString::Printf(TEXT("\"%s\":%.6f"), FieldName, Value);
	}

	void PhysAnimEvidenceSummary_AppendBoolField(FString& Json, bool& bFirstField, const TCHAR* FieldName, const bool bValue)
	{
		PhysAnimEvidenceSummary_AppendCommaIfNeeded(Json, bFirstField);
		Json += FString::Printf(TEXT("\"%s\":%s"), FieldName, bValue ? TEXT("true") : TEXT("false"));
	}

	void PhysAnimEvidenceSummary_AppendNullField(FString& Json, bool& bFirstField, const TCHAR* FieldName)
	{
		PhysAnimEvidenceSummary_AppendCommaIfNeeded(Json, bFirstField);
		Json += FString::Printf(TEXT("\"%s\":null"), FieldName);
	}

	FString PhysAnimEvidenceSummary_BuildStringArrayJson(const TArray<FString>& Values)
	{
		FString Json = TEXT("[");
		for (int32 Index = 0; Index < Values.Num(); ++Index)
		{
			if (Index > 0)
			{
				Json += TEXT(",");
			}

			Json += FString::Printf(TEXT("\"%s\""), *PhysAnimEvidenceSummary_EscapeJsonString(Values[Index]));
		}

		Json += TEXT("]");
		return Json;
	}

	FString PhysAnimEvidenceSummary_BuildSegmentMetricsJson(const FPhysAnimEvidenceSummarySegmentMetrics& Metrics)
	{
		FString Json = TEXT("{");
		bool bFirstField = true;

		PhysAnimEvidenceSummary_AppendIntField(Json, bFirstField, TEXT("sample_count"), Metrics.SampleCount);
		PhysAnimEvidenceSummary_AppendDoubleField(Json, bFirstField, TEXT("confidence"), Metrics.Confidence);
		PhysAnimEvidenceSummary_AppendDoubleField(Json, bFirstField, TEXT("score"), Metrics.Score);

		Json += TEXT("}");
		return Json;
	}

	FString PhysAnimEvidenceSummary_BuildCommandMetadataJson(const FPhysAnimEvidenceSummaryCommandMetadata& Metadata)
	{
		FString Json = TEXT("{");
		bool bFirstField = true;

		PhysAnimEvidenceSummary_AppendStringField(Json, bFirstField, TEXT("command_name"), Metadata.CommandName);
		PhysAnimEvidenceSummary_AppendStringField(Json, bFirstField, TEXT("command_line"), Metadata.CommandLine);
		PhysAnimEvidenceSummary_AppendStringField(Json, bFirstField, TEXT("working_directory"), Metadata.WorkingDirectory);

		Json += TEXT("}");
		return Json;
	}

	FString PhysAnimEvidenceSummary_BuildQualityFlagsJson(const FPhysAnimEvidenceBaselineTruthFlags& Flags)
	{
		FString Json = TEXT("{");
		bool bFirstField = true;

		PhysAnimEvidenceSummary_AppendBoolField(Json, bFirstField, TEXT("assistance_truth_clean"), Flags.bAssistanceTruthClean);
		PhysAnimEvidenceSummary_AppendBoolField(Json, bFirstField, TEXT("continuity_truth_clean"), Flags.bContinuityTruthClean);
		PhysAnimEvidenceSummary_AppendBoolField(Json, bFirstField, TEXT("support_truth_clean"), Flags.bSupportTruthClean);
		PhysAnimEvidenceSummary_AppendBoolField(Json, bFirstField, TEXT("simulation_truth_clean"), Flags.bSimulationTruthClean);
		PhysAnimEvidenceSummary_AppendBoolField(Json, bFirstField, TEXT("terminal_failure"), Flags.bTerminalFailure);
		PhysAnimEvidenceSummary_AppendBoolField(Json, bFirstField, TEXT("artifact_log_contradiction"), Flags.bArtifactLogContradiction);
		PhysAnimEvidenceSummary_AppendBoolField(Json, bFirstField, TEXT("missing_evidence"), Flags.bMissingEvidence);

		Json += TEXT("}");
		return Json;
	}

	FString PhysAnimEvidenceSummary_BuildSegmentStateString(const EPhysAnimEvidenceBaselineSegmentState State)
	{
		switch (State)
		{
		case EPhysAnimEvidenceBaselineSegmentState::NotReached:
			return TEXT("NotReached");
		case EPhysAnimEvidenceBaselineSegmentState::ReachedButInactive:
			return TEXT("ReachedButInactive");
		case EPhysAnimEvidenceBaselineSegmentState::Active:
			return TEXT("Active");
		default:
			return TEXT("NotReached");
		}
	}

	bool PhysAnimEvidenceSummary_TryParseSegmentState(const FString& Value, EPhysAnimEvidenceBaselineSegmentState& OutState)
	{
		if (Value.Equals(TEXT("NotReached"), ESearchCase::IgnoreCase))
		{
			OutState = EPhysAnimEvidenceBaselineSegmentState::NotReached;
			return true;
		}

		if (Value.Equals(TEXT("ReachedButInactive"), ESearchCase::IgnoreCase))
		{
			OutState = EPhysAnimEvidenceBaselineSegmentState::ReachedButInactive;
			return true;
		}

		if (Value.Equals(TEXT("Active"), ESearchCase::IgnoreCase))
		{
			OutState = EPhysAnimEvidenceBaselineSegmentState::Active;
			return true;
		}

		return false;
	}

	FString PhysAnimEvidenceSummary_BuildStrictVerdictString(const EPhysAnimEvidenceBaselineVerdict Verdict)
	{
		switch (Verdict)
		{
		case EPhysAnimEvidenceBaselineVerdict::ProductSuccessCandidate:
			return TEXT("PRODUCT_SUCCESS_CANDIDATE");
		case EPhysAnimEvidenceBaselineVerdict::Diagnostic:
			return TEXT("DIAGNOSTIC");
		case EPhysAnimEvidenceBaselineVerdict::Blocked:
			return TEXT("BLOCKED");
		case EPhysAnimEvidenceBaselineVerdict::Contradictory:
			return TEXT("CONTRADICTORY");
		case EPhysAnimEvidenceBaselineVerdict::InsufficientEvidence:
			return TEXT("INSUFFICIENT_EVIDENCE");
		default:
			return TEXT("INSUFFICIENT_EVIDENCE");
		}
	}

	bool PhysAnimEvidenceSummary_TryParseStrictVerdict(const FString& Value, EPhysAnimEvidenceBaselineVerdict& OutVerdict)
	{
		if (Value.Equals(TEXT("PRODUCT_SUCCESS_CANDIDATE"), ESearchCase::IgnoreCase) ||
			Value.Equals(TEXT("ProductSuccessCandidate"), ESearchCase::IgnoreCase))
		{
			OutVerdict = EPhysAnimEvidenceBaselineVerdict::ProductSuccessCandidate;
			return true;
		}

		if (Value.Equals(TEXT("DIAGNOSTIC"), ESearchCase::IgnoreCase) ||
			Value.Equals(TEXT("Diagnostic"), ESearchCase::IgnoreCase))
		{
			OutVerdict = EPhysAnimEvidenceBaselineVerdict::Diagnostic;
			return true;
		}

		if (Value.Equals(TEXT("BLOCKED"), ESearchCase::IgnoreCase) ||
			Value.Equals(TEXT("Blocked"), ESearchCase::IgnoreCase))
		{
			OutVerdict = EPhysAnimEvidenceBaselineVerdict::Blocked;
			return true;
		}

		if (Value.Equals(TEXT("CONTRADICTORY"), ESearchCase::IgnoreCase) ||
			Value.Equals(TEXT("Contradictory"), ESearchCase::IgnoreCase))
		{
			OutVerdict = EPhysAnimEvidenceBaselineVerdict::Contradictory;
			return true;
		}

		if (Value.Equals(TEXT("INSUFFICIENT_EVIDENCE"), ESearchCase::IgnoreCase) ||
			Value.Equals(TEXT("InsufficientEvidence"), ESearchCase::IgnoreCase))
		{
			OutVerdict = EPhysAnimEvidenceBaselineVerdict::InsufficientEvidence;
			return true;
		}

		return false;
	}

	bool PhysAnimEvidenceSummary_TryParseTerminalReason(const FString& Value, EPhysAnimTerminalReason& OutReason)
	{
		if (Value.Equals(TEXT("None"), ESearchCase::IgnoreCase))
		{
			OutReason = EPhysAnimTerminalReason::None;
			return true;
		}
		if (Value.Equals(TEXT("ActivationPhysicsAssetContractViolation"), ESearchCase::IgnoreCase))
		{
			OutReason = EPhysAnimTerminalReason::ActivationPhysicsAssetContractViolation;
			return true;
		}
		if (Value.Equals(TEXT("ActivationCapsuleContractViolation"), ESearchCase::IgnoreCase))
		{
			OutReason = EPhysAnimTerminalReason::ActivationCapsuleContractViolation;
			return true;
		}
		if (Value.Equals(TEXT("ActivationTopologyChange"), ESearchCase::IgnoreCase))
		{
			OutReason = EPhysAnimTerminalReason::ActivationTopologyChange;
			return true;
		}
		if (Value.Equals(TEXT("ActivationContinuousSimulationLost"), ESearchCase::IgnoreCase))
		{
			OutReason = EPhysAnimTerminalReason::ActivationContinuousSimulationLost;
			return true;
		}
		if (Value.Equals(TEXT("ActivationSupportFailure"), ESearchCase::IgnoreCase))
		{
			OutReason = EPhysAnimTerminalReason::ActivationSupportFailure;
			return true;
		}
		if (Value.Equals(TEXT("ActivationProxyOutsideSupportRegion"), ESearchCase::IgnoreCase))
		{
			OutReason = EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion;
			return true;
		}
		if (Value.Equals(TEXT("ActivationTargetDiscontinuity"), ESearchCase::IgnoreCase))
		{
			OutReason = EPhysAnimTerminalReason::ActivationTargetDiscontinuity;
			return true;
		}
		if (Value.Equals(TEXT("ActivationUnstableGainOrDamping"), ESearchCase::IgnoreCase))
		{
			OutReason = EPhysAnimTerminalReason::ActivationUnstableGainOrDamping;
			return true;
		}
		if (Value.Equals(TEXT("ActivationInstabilityThresholdBreach"), ESearchCase::IgnoreCase))
		{
			OutReason = EPhysAnimTerminalReason::ActivationInstabilityThresholdBreach;
			return true;
		}
		if (Value.Equals(TEXT("ActivationPoseReferenceMismatch"), ESearchCase::IgnoreCase))
		{
			OutReason = EPhysAnimTerminalReason::ActivationPoseReferenceMismatch;
			return true;
		}
		if (Value.Equals(TEXT("ActivationMovementReclaim"), ESearchCase::IgnoreCase))
		{
			OutReason = EPhysAnimTerminalReason::ActivationMovementReclaim;
			return true;
		}
		if (Value.Equals(TEXT("ActivationShellHelperViolation"), ESearchCase::IgnoreCase))
		{
			OutReason = EPhysAnimTerminalReason::ActivationShellHelperViolation;
			return true;
		}
		if (Value.Equals(TEXT("ActivationAuthorityConflict"), ESearchCase::IgnoreCase))
		{
			OutReason = EPhysAnimTerminalReason::ActivationAuthorityConflict;
			return true;
		}
		if (Value.Equals(TEXT("ActivationStandingValidationTimeout"), ESearchCase::IgnoreCase))
		{
			OutReason = EPhysAnimTerminalReason::ActivationStandingValidationTimeout;
			return true;
		}

		return false;
	}

	FString PhysAnimEvidenceSummary_BuildSegmentJson(const FPhysAnimEvidenceSummarySegment& Segment)
	{
		FString Json = TEXT("{");
		bool bFirstField = true;

		PhysAnimEvidenceSummary_AppendStringField(Json, bFirstField, TEXT("segment_name"), Segment.SegmentName);
		PhysAnimEvidenceSummary_AppendStringField(Json, bFirstField, TEXT("state"), PhysAnimEvidenceSummary_BuildSegmentStateString(Segment.State));

		PhysAnimEvidenceSummary_AppendCommaIfNeeded(Json, bFirstField);
		Json += FString::Printf(TEXT("\"metrics\":%s"), *PhysAnimEvidenceSummary_BuildSegmentMetricsJson(Segment.Metrics));

		PhysAnimEvidenceSummary_AppendCommaIfNeeded(Json, bFirstField);
		Json += FString::Printf(TEXT("\"missing_required_fields\":%s"), *PhysAnimEvidenceSummary_BuildStringArrayJson(Segment.MissingRequiredFields));

		PhysAnimEvidenceSummary_AppendCommaIfNeeded(Json, bFirstField);
		Json += FString::Printf(TEXT("\"diagnostic_notes\":%s"), *PhysAnimEvidenceSummary_BuildStringArrayJson(Segment.DiagnosticNotes));

		PhysAnimEvidenceSummary_AppendCommaIfNeeded(Json, bFirstField);
		Json += FString::Printf(TEXT("\"source_provenance\":%s"), *PhysAnimEvidenceSummary_BuildStringArrayJson(Segment.SourceProvenance));

		Json += TEXT("}");
		return Json;
	}

	bool PhysAnimEvidenceSummary_ReadStringArrayField(
		const TSharedPtr<FJsonObject>& JsonObject,
		const TCHAR* FieldName,
		TArray<FString>& OutValues)
	{
		const TArray<TSharedPtr<FJsonValue>>* ArrayValues = nullptr;
		if (!JsonObject->TryGetArrayField(FieldName, ArrayValues))
		{
			return false;
		}

		OutValues.Reset();
		OutValues.Reserve(ArrayValues->Num());
		for (const TSharedPtr<FJsonValue>& Value : *ArrayValues)
		{
			if (!Value.IsValid() || Value->Type != EJson::String)
			{
				return false;
			}

			OutValues.Add(Value->AsString());
		}

		return true;
	}

	bool PhysAnimEvidenceSummary_ReadSegmentMetricsField(
		const TSharedPtr<FJsonObject>& JsonObject,
		FPhysAnimEvidenceSummarySegmentMetrics& OutMetrics)
	{
		double SampleCount = 0.0;
		if (!JsonObject->TryGetNumberField(TEXT("sample_count"), SampleCount) ||
			!JsonObject->TryGetNumberField(TEXT("confidence"), OutMetrics.Confidence) ||
			!JsonObject->TryGetNumberField(TEXT("score"), OutMetrics.Score))
		{
			return false;
		}

		OutMetrics.SampleCount = static_cast<int32>(SampleCount);
		return true;
	}

	bool PhysAnimEvidenceSummary_ReadCommandMetadataField(
		const TSharedPtr<FJsonObject>& JsonObject,
		const TCHAR* FieldName,
		TOptional<FPhysAnimEvidenceSummaryCommandMetadata>& OutCommandMetadata)
	{
		const TSharedPtr<FJsonObject>* CommandMetadataObject = nullptr;
		if (!JsonObject->TryGetObjectField(FieldName, CommandMetadataObject) || !CommandMetadataObject || !CommandMetadataObject->IsValid())
		{
			OutCommandMetadata.Reset();
			return true;
		}

		FPhysAnimEvidenceSummaryCommandMetadata CommandMetadata;
		if (!(*CommandMetadataObject)->TryGetStringField(TEXT("command_name"), CommandMetadata.CommandName) ||
			!(*CommandMetadataObject)->TryGetStringField(TEXT("command_line"), CommandMetadata.CommandLine) ||
			!(*CommandMetadataObject)->TryGetStringField(TEXT("working_directory"), CommandMetadata.WorkingDirectory))
		{
			return false;
		}

		OutCommandMetadata = CommandMetadata;
		return true;
	}
}

namespace PhysAnimEvidenceSummary
{
	FString BuildEvidenceSummaryJsonPath(const FString& AttemptUuid)
	{
		const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PhysAnim"), TEXT("EvidenceSummaries"));
		const FString FileName = FString::Printf(TEXT("%s_evidence_summary.json"), *PhysAnimEvidenceSummary_SanitizeFileToken(AttemptUuid));
		return FPaths::Combine(Directory, FileName);
	}

	FString SerializeToJsonString(const FPhysAnimEvidenceSummary& Summary)
	{
		FString Json = TEXT("{");
		bool bFirstField = true;

		PhysAnimEvidenceSummary_AppendIntField(Json, bFirstField, TEXT("schema_version"), Summary.SchemaVersion);
		PhysAnimEvidenceSummary_AppendStringField(Json, bFirstField, TEXT("attempt_uuid"), Summary.AttemptUuid);
		PhysAnimEvidenceSummary_AppendStringField(Json, bFirstField, TEXT("test_name"), Summary.TestName);
		PhysAnimEvidenceSummary_AppendStringField(Json, bFirstField, TEXT("map_name"), Summary.MapName);
		PhysAnimEvidenceSummary_AppendDoubleField(Json, bFirstField, TEXT("timestamp"), Summary.Timestamp);

		if (Summary.CommandMetadata.IsSet())
		{
			PhysAnimEvidenceSummary_AppendCommaIfNeeded(Json, bFirstField);
			Json += FString::Printf(TEXT("\"command_metadata\":%s"), *PhysAnimEvidenceSummary_BuildCommandMetadataJson(Summary.CommandMetadata.GetValue()));
		}
		else
		{
			PhysAnimEvidenceSummary_AppendNullField(Json, bFirstField, TEXT("command_metadata"));
		}

		PhysAnimEvidenceSummary_AppendCommaIfNeeded(Json, bFirstField);
		Json += TEXT("\"segments\":[");
		for (int32 Index = 0; Index < Summary.Segments.Num(); ++Index)
		{
			if (Index > 0)
			{
				Json += TEXT(",");
			}

			Json += PhysAnimEvidenceSummary_BuildSegmentJson(Summary.Segments[Index]);
		}
		Json += TEXT("]");

		PhysAnimEvidenceSummary_AppendCommaIfNeeded(Json, bFirstField);
		Json += FString::Printf(TEXT("\"quality_flags\":%s"), *PhysAnimEvidenceSummary_BuildQualityFlagsJson(Summary.QualityFlags));

		PhysAnimEvidenceSummary_AppendCommaIfNeeded(Json, bFirstField);
		Json += FString::Printf(TEXT("\"terminal_reason\":\"%s\""), *PhysAnimProofArtifactEmitter::ToTerminalReasonString(Summary.TerminalReason));

		PhysAnimEvidenceSummary_AppendCommaIfNeeded(Json, bFirstField);
		Json += FString::Printf(TEXT("\"strict_verdict\":\"%s\""), *PhysAnimEvidenceSummary_BuildStrictVerdictString(Summary.StrictVerdict));

		PhysAnimEvidenceSummary_AppendCommaIfNeeded(Json, bFirstField);
		Json += FString::Printf(TEXT("\"terminal_artifact_path\":\"%s\""), *PhysAnimEvidenceSummary_EscapeJsonString(Summary.TerminalArtifactPath));

		Json += TEXT("}");
		return Json;
	}

	bool DeserializeFromJsonString(const FString& JsonString, FPhysAnimEvidenceSummary& OutSummary)
	{
		TSharedPtr<FJsonObject> JsonObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
		if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
		{
			return false;
		}

		double SchemaVersion = 0.0;
		if (!JsonObject->TryGetNumberField(TEXT("schema_version"), SchemaVersion) ||
			!JsonObject->TryGetStringField(TEXT("attempt_uuid"), OutSummary.AttemptUuid) ||
			!JsonObject->TryGetStringField(TEXT("test_name"), OutSummary.TestName) ||
			!JsonObject->TryGetStringField(TEXT("map_name"), OutSummary.MapName) ||
			!JsonObject->TryGetNumberField(TEXT("timestamp"), OutSummary.Timestamp))
		{
			return false;
		}

		OutSummary.SchemaVersion = static_cast<int32>(SchemaVersion);

		if (!PhysAnimEvidenceSummary_ReadCommandMetadataField(JsonObject, TEXT("command_metadata"), OutSummary.CommandMetadata))
		{
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* SegmentArray = nullptr;
		if (!JsonObject->TryGetArrayField(TEXT("segments"), SegmentArray))
		{
			return false;
		}

		OutSummary.Segments.Reset();
		OutSummary.Segments.Reserve(SegmentArray->Num());
		for (const TSharedPtr<FJsonValue>& SegmentValue : *SegmentArray)
		{
			const TSharedPtr<FJsonObject> SegmentObject = SegmentValue.IsValid() ? SegmentValue->AsObject() : nullptr;
			if (!SegmentObject.IsValid())
			{
				return false;
			}

			FPhysAnimEvidenceSummarySegment Segment;
			if (!SegmentObject->TryGetStringField(TEXT("segment_name"), Segment.SegmentName))
			{
				return false;
			}

			FString StateString;
			if (!SegmentObject->TryGetStringField(TEXT("state"), StateString) ||
				!PhysAnimEvidenceSummary_TryParseSegmentState(StateString, Segment.State))
			{
				return false;
			}

			const TSharedPtr<FJsonObject>* MetricsObject = nullptr;
			if (!SegmentObject->TryGetObjectField(TEXT("metrics"), MetricsObject) || !MetricsObject || !MetricsObject->IsValid())
			{
				return false;
			}
			if (!PhysAnimEvidenceSummary_ReadSegmentMetricsField(*MetricsObject, Segment.Metrics))
			{
				return false;
			}

			if (!PhysAnimEvidenceSummary_ReadStringArrayField(SegmentObject, TEXT("missing_required_fields"), Segment.MissingRequiredFields) ||
				!PhysAnimEvidenceSummary_ReadStringArrayField(SegmentObject, TEXT("diagnostic_notes"), Segment.DiagnosticNotes) ||
				!PhysAnimEvidenceSummary_ReadStringArrayField(SegmentObject, TEXT("source_provenance"), Segment.SourceProvenance))
			{
				return false;
			}

			OutSummary.Segments.Add(MoveTemp(Segment));
		}

		const TSharedPtr<FJsonObject>* QualityFlagsObject = nullptr;
		if (!JsonObject->TryGetObjectField(TEXT("quality_flags"), QualityFlagsObject) || !QualityFlagsObject || !QualityFlagsObject->IsValid())
		{
			return false;
		}

		if (!(*QualityFlagsObject)->TryGetBoolField(TEXT("assistance_truth_clean"), OutSummary.QualityFlags.bAssistanceTruthClean) ||
			!(*QualityFlagsObject)->TryGetBoolField(TEXT("continuity_truth_clean"), OutSummary.QualityFlags.bContinuityTruthClean) ||
			!(*QualityFlagsObject)->TryGetBoolField(TEXT("support_truth_clean"), OutSummary.QualityFlags.bSupportTruthClean) ||
			!(*QualityFlagsObject)->TryGetBoolField(TEXT("simulation_truth_clean"), OutSummary.QualityFlags.bSimulationTruthClean) ||
			!(*QualityFlagsObject)->TryGetBoolField(TEXT("terminal_failure"), OutSummary.QualityFlags.bTerminalFailure) ||
			!(*QualityFlagsObject)->TryGetBoolField(TEXT("artifact_log_contradiction"), OutSummary.QualityFlags.bArtifactLogContradiction) ||
			!(*QualityFlagsObject)->TryGetBoolField(TEXT("missing_evidence"), OutSummary.QualityFlags.bMissingEvidence))
		{
			return false;
		}

		FString TerminalReasonString;
		if (!JsonObject->TryGetStringField(TEXT("terminal_reason"), TerminalReasonString) ||
			!PhysAnimEvidenceSummary_TryParseTerminalReason(TerminalReasonString, OutSummary.TerminalReason))
		{
			return false;
		}

		FString StrictVerdictString;
		if (!JsonObject->TryGetStringField(TEXT("strict_verdict"), StrictVerdictString) ||
			!PhysAnimEvidenceSummary_TryParseStrictVerdict(StrictVerdictString, OutSummary.StrictVerdict))
		{
			return false;
		}

		if (!JsonObject->TryGetStringField(TEXT("terminal_artifact_path"), OutSummary.TerminalArtifactPath))
		{
			return false;
		}

		return true;
	}
}
