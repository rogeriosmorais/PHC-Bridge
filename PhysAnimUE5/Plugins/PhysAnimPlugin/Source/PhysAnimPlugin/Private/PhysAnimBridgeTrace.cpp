#include "PhysAnimBridgeTrace.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogPhysAnimBridgeTrace, Log, All);

namespace
{
	constexpr int32 TraceVersion = 1;
	const TCHAR* SessionMetadataFileName = TEXT("session.json");
	const TCHAR* EventsFileName = TEXT("events.jsonl");
	const TCHAR* FramesFileName = TEXT("frames.csv");

	FString EscapeTraceJsonString(const FString& Value)
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

	FString EscapeCsvField(const FString& Value)
	{
		FString Escaped = Value;
		Escaped.ReplaceInline(TEXT("\""), TEXT("\"\""));
		return FString::Printf(TEXT("\"%s\""), *Escaped);
	}

	FString BoolToCsv(const bool bValue)
	{
		return bValue ? TEXT("true") : TEXT("false");
	}

	FString BoolToJson(const bool bValue)
	{
		return bValue ? TEXT("true") : TEXT("false");
	}

	FString FormatFloat(const float Value)
	{
		return FString::Printf(TEXT("%.3f"), Value);
	}

	FString FormatDouble(const double Value)
	{
		return FString::Printf(TEXT("%.6f"), Value);
	}

	void AppendJsonLine(FString& Json, const FString& Key, const FString& Value, const bool bAddComma)
	{
		Json += FString::Printf(TEXT("  \"%s\": \"%s\"%s\n"), *Key, *EscapeTraceJsonString(Value), bAddComma ? TEXT(",") : TEXT(""));
	}

	void AppendJsonLine(FString& Json, const FString& Key, const int32 Value, const bool bAddComma)
	{
		Json += FString::Printf(TEXT("  \"%s\": %d%s\n"), *Key, Value, bAddComma ? TEXT(",") : TEXT(""));
	}

	void AppendJsonLine(FString& Json, const FString& Key, const int64 Value, const bool bAddComma)
	{
		Json += FString::Printf(TEXT("  \"%s\": %lld%s\n"), *Key, Value, bAddComma ? TEXT(",") : TEXT(""));
	}

	void AppendJsonLine(FString& Json, const FString& Key, const float Value, const bool bAddComma)
	{
		Json += FString::Printf(TEXT("  \"%s\": %s%s\n"), *Key, *FormatFloat(Value), bAddComma ? TEXT(",") : TEXT(""));
	}

	void AppendJsonLine(FString& Json, const FString& Key, const double Value, const bool bAddComma)
	{
		Json += FString::Printf(TEXT("  \"%s\": %s%s\n"), *Key, *FormatDouble(Value), bAddComma ? TEXT(",") : TEXT(""));
	}

	void AppendJsonLine(FString& Json, const FString& Key, const bool bValue, const bool bAddComma)
	{
		Json += FString::Printf(TEXT("  \"%s\": %s%s\n"), *Key, *BoolToJson(bValue), bAddComma ? TEXT(",") : TEXT(""));
	}

	void AppendOptionalJsonString(FString& Json, const FString& Key, const FString& Value, bool& bFirstField)
	{
		if (Value.IsEmpty())
		{
			return;
		}

		if (!bFirstField)
		{
			Json += TEXT(",");
		}

		Json += FString::Printf(TEXT("\"%s\":\"%s\""), *Key, *EscapeTraceJsonString(Value));
		bFirstField = false;
	}

	void AppendOptionalJsonDouble(FString& Json, const FString& Key, const double Value, const bool bInclude, bool& bFirstField)
	{
		if (!bInclude)
		{
			return;
		}

		if (!bFirstField)
		{
			Json += TEXT(",");
		}

		Json += FString::Printf(TEXT("\"%s\":%s"), *Key, *FormatDouble(Value));
		bFirstField = false;
	}

	void AppendCsvField(TArray<FString>& Fields, const FString& Value)
	{
		Fields.Add(EscapeCsvField(Value));
	}

	void AppendCsvField(TArray<FString>& Fields, const TCHAR* Value)
	{
		Fields.Add(EscapeCsvField(Value ? FString(Value) : FString()));
	}

	void AppendCsvField(TArray<FString>& Fields, const bool bValue)
	{
		Fields.Add(BoolToCsv(bValue));
	}

	void AppendCsvField(TArray<FString>& Fields, const int32 Value)
	{
		Fields.Add(FString::FromInt(Value));
	}

	void AppendCsvField(TArray<FString>& Fields, const int64 Value)
	{
		Fields.Add(FString::Printf(TEXT("%lld"), Value));
	}

	void AppendCsvField(TArray<FString>& Fields, const float Value)
	{
		Fields.Add(FormatFloat(Value));
	}

	void AppendCsvField(TArray<FString>& Fields, const double Value)
	{
		Fields.Add(FormatDouble(Value));
	}
}

FPhysAnimBridgeTraceWriter::FPhysAnimBridgeTraceWriter(const EPhysAnimBridgeTraceMode InMode)
	: Mode(InMode)
{
}

bool FPhysAnimBridgeTraceWriter::StartSession(
	const FString& TraceRootPath,
	const FPhysAnimBridgeTraceSessionMetadata& Metadata,
	FString& OutError)
{
	OutError.Reset();

	if (Mode == EPhysAnimBridgeTraceMode::Off)
	{
		return true;
	}

	SessionId = Metadata.SessionId;
	SessionFolderPath = FPaths::Combine(TraceRootPath, Metadata.SessionId);

	IFileManager::Get().MakeDirectory(*SessionFolderPath, true);
	if (!IFileManager::Get().DirectoryExists(*SessionFolderPath))
	{
		OutError = FString::Printf(TEXT("Failed to create trace session directory '%s'."), *SessionFolderPath);
		bWriteFailed = true;
		return false;
	}

	const FString SessionMetadataPath = FPaths::Combine(SessionFolderPath, SessionMetadataFileName);
	if (!WriteWholeFile(SessionMetadataPath, BuildSessionMetadataJson(Metadata)))
	{
		OutError = FString::Printf(TEXT("Failed to write trace session metadata '%s'."), *SessionMetadataPath);
		HandleWriteFailure(OutError);
		return false;
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	const FString EventsPath = FPaths::Combine(SessionFolderPath, EventsFileName);
	EventsFileHandle.Reset(PlatformFile.OpenWrite(*EventsPath, false, true));
	if (!EventsFileHandle.IsValid())
	{
		OutError = FString::Printf(TEXT("Failed to open trace event file '%s'."), *EventsPath);
		HandleWriteFailure(OutError);
		return false;
	}

	if (Mode == EPhysAnimBridgeTraceMode::Full)
	{
		const FString FramesPath = FPaths::Combine(SessionFolderPath, FramesFileName);
		FramesFileHandle.Reset(PlatformFile.OpenWrite(*FramesPath, false, true));
		if (!FramesFileHandle.IsValid())
		{
			OutError = FString::Printf(TEXT("Failed to open trace frame file '%s'."), *FramesPath);
			HandleWriteFailure(OutError);
			return false;
		}

		const FString HeaderLine = BuildFrameCsvHeader() + LINE_TERMINATOR;
		if (!WriteBytes(*FramesFileHandle, HeaderLine))
		{
			OutError = FString::Printf(TEXT("Failed to write trace frame header '%s'."), *FramesPath);
			HandleWriteFailure(OutError);
			return false;
		}
	}

	bSessionStarted = true;
	return true;
}

void FPhysAnimBridgeTraceWriter::AppendEvent(const FPhysAnimBridgeTraceEvent& Event)
{
	if (!IsEnabled())
	{
		return;
	}

	EventsBuffer += BuildEventJsonLine(Event);
	EventsBuffer += LINE_TERMINATOR;
}

void FPhysAnimBridgeTraceWriter::AppendFrame(const FPhysAnimBridgeTraceFrame& Frame)
{
	if (!CanWriteFrames())
	{
		return;
	}

	FramesBuffer += BuildFrameCsvRow(Frame);
	FramesBuffer += LINE_TERMINATOR;
}

void FPhysAnimBridgeTraceWriter::Flush(const bool bForce)
{
	(void)bForce;

	if (!bSessionStarted || bWriteFailed)
	{
		return;
	}

	if (!EventsBuffer.IsEmpty())
	{
		if (!EventsFileHandle.IsValid() || !WriteBytes(*EventsFileHandle, EventsBuffer))
		{
			HandleWriteFailure(TEXT("Failed to flush buffered trace events."));
			return;
		}
		EventsBuffer.Reset();
	}

	if (!FramesBuffer.IsEmpty())
	{
		if (!FramesFileHandle.IsValid() || !WriteBytes(*FramesFileHandle, FramesBuffer))
		{
			HandleWriteFailure(TEXT("Failed to flush buffered trace frames."));
			return;
		}
		FramesBuffer.Reset();
	}

	if (EventsFileHandle.IsValid())
	{
		EventsFileHandle->Flush();
	}
	if (FramesFileHandle.IsValid())
	{
		FramesFileHandle->Flush();
	}
}

void FPhysAnimBridgeTraceWriter::Shutdown()
{
	if (!bSessionStarted)
	{
		return;
	}

	Flush(true);
	EventsFileHandle.Reset();
	FramesFileHandle.Reset();
	bSessionStarted = false;
}

bool FPhysAnimBridgeTraceWriter::IsEnabled() const
{
	return Mode != EPhysAnimBridgeTraceMode::Off && bSessionStarted && !bWriteFailed;
}

bool FPhysAnimBridgeTraceWriter::CanWriteFrames() const
{
	return Mode == EPhysAnimBridgeTraceMode::Full && IsEnabled();
}

void FPhysAnimBridgeTraceWriter::SetFailWritesAfterCountForTesting(const int32 InFailWritesAfterCount)
{
	FailWritesAfterCount = InFailWritesAfterCount;
}

FString FPhysAnimBridgeTraceWriter::BuildSessionMetadataJson(const FPhysAnimBridgeTraceSessionMetadata& Metadata)
{
	FString Json = TEXT("{\n");
	AppendJsonLine(Json, TEXT("trace_version"), Metadata.TraceVersion, true);
	AppendJsonLine(Json, TEXT("session_id"), Metadata.SessionId, true);
	AppendJsonLine(Json, TEXT("timestamp_utc"), Metadata.TimestampUtc, true);
	AppendJsonLine(Json, TEXT("map_name"), Metadata.MapName, true);
	AppendJsonLine(Json, TEXT("actor_name"), Metadata.ActorName, true);
	AppendJsonLine(Json, TEXT("runtime_state"), Metadata.RuntimeState, true);
	AppendJsonLine(Json, TEXT("nne_runtime_name"), Metadata.NneRuntimeName, true);
	AppendJsonLine(Json, TEXT("model_asset_path"), Metadata.ModelAssetPath, true);
	AppendJsonLine(Json, TEXT("pose_search_database_path"), Metadata.PoseSearchDatabasePath, true);
	AppendJsonLine(Json, TEXT("trace_mode"), Metadata.TraceMode, true);
	AppendJsonLine(Json, TEXT("sample_every_nth_frame"), Metadata.SampleEveryNthFrame, true);
	AppendJsonLine(Json, TEXT("flush_interval_seconds"), Metadata.FlushIntervalSeconds, true);
	AppendJsonLine(Json, TEXT("force_zero_actions"), Metadata.bForceZeroActions, true);
	AppendJsonLine(Json, TEXT("action_scale"), Metadata.ActionScale, true);
	AppendJsonLine(Json, TEXT("action_clamp_abs"), Metadata.ActionClampAbs, true);
	AppendJsonLine(Json, TEXT("action_smoothing_alpha"), Metadata.ActionSmoothingAlpha, true);
	AppendJsonLine(Json, TEXT("startup_ramp_seconds"), Metadata.StartupRampSeconds, true);
	AppendJsonLine(Json, TEXT("policy_control_rate_hz"), Metadata.PolicyControlRateHz, true);
	AppendJsonLine(Json, TEXT("max_angular_step_degrees_per_second"), Metadata.MaxAngularStepDegreesPerSecond, true);
	AppendJsonLine(Json, TEXT("angular_strength_multiplier"), Metadata.AngularStrengthMultiplier, true);
	AppendJsonLine(Json, TEXT("angular_damping_ratio_multiplier"), Metadata.AngularDampingRatioMultiplier, true);
	AppendJsonLine(Json, TEXT("angular_extra_damping_multiplier"), Metadata.AngularExtraDampingMultiplier, true);
	AppendJsonLine(Json, TEXT("enable_instability_fail_stop"), Metadata.bEnableInstabilityFailStop, true);
	AppendJsonLine(Json, TEXT("max_root_height_delta_cm"), Metadata.MaxRootHeightDeltaCm, true);
	AppendJsonLine(Json, TEXT("max_root_linear_speed_cm_per_second"), Metadata.MaxRootLinearSpeedCmPerSecond, true);
	AppendJsonLine(Json, TEXT("max_root_angular_speed_deg_per_second"), Metadata.MaxRootAngularSpeedDegPerSecond, true);
	AppendJsonLine(Json, TEXT("instability_grace_period_seconds"), Metadata.InstabilityGracePeriodSeconds, true);
	AppendJsonLine(Json, TEXT("stabilization_summary"), Metadata.StabilizationSummary, false);
	Json += TEXT("}");
	return Json;
}

FString FPhysAnimBridgeTraceWriter::BuildEventJsonLine(const FPhysAnimBridgeTraceEvent& Event)
{
	FString Json = TEXT("{");
	bool bFirstField = true;

	AppendOptionalJsonString(Json, TEXT("session_id"), Event.SessionId, bFirstField);
	AppendOptionalJsonDouble(Json, TEXT("event_time_seconds"), Event.EventTimeSeconds, true, bFirstField);
	AppendOptionalJsonString(Json, TEXT("event_type"), Event.EventType, bFirstField);
	AppendOptionalJsonString(Json, TEXT("runtime_state"), Event.RuntimeState, bFirstField);
	AppendOptionalJsonString(Json, TEXT("message"), Event.Message, bFirstField);
	AppendOptionalJsonString(Json, TEXT("error"), Event.Error, bFirstField);
	AppendOptionalJsonString(Json, TEXT("previous_runtime_state"), Event.PreviousRuntimeState, bFirstField);
	AppendOptionalJsonString(Json, TEXT("new_runtime_state"), Event.NewRuntimeState, bFirstField);
	AppendOptionalJsonString(Json, TEXT("nne_runtime_name"), Event.NneRuntimeName, bFirstField);
	AppendOptionalJsonString(Json, TEXT("model_asset_path"), Event.ModelAssetPath, bFirstField);
	AppendOptionalJsonString(Json, TEXT("map_name"), Event.MapName, bFirstField);
	AppendOptionalJsonString(Json, TEXT("actor_name"), Event.ActorName, bFirstField);
	Json += TEXT("}");
	return Json;
}

FString FPhysAnimBridgeTraceWriter::BuildFrameCsvHeader()
{
	return TEXT("session_id,trace_version,frame_index,world_time_seconds,delta_time_seconds,sampled_policy_step,runtime_state,nne_runtime_name,pose_search_valid,run_sync_succeeded,update_controls_succeeded,policy_influence_active,first_policy_enabled_frame,num_policy_targets_written,resolve_context_ms,pose_search_query_ms,future_pose_sample_ms,body_sample_ms,observation_pack_ms,inference_ms,action_condition_ms,control_target_ms,update_controls_ms,instability_check_ms,bridge_tick_total_ms,self_observation_root_height,self_observation_mean_abs,mimic_target_poses_mean_abs,mimic_target_poses_min_future_time_seconds,mimic_target_poses_max_future_time_seconds,terrain_mean,terrain_min,terrain_max,terrain_center,movement_smoke_phase,distal_locomotion_composition_mode_active,raw_action_min,raw_action_max,raw_action_mean_abs,conditioned_action_mean_abs,num_clamped_action_floats,max_target_delta_bone,max_target_delta_degrees,mean_target_delta_degrees,max_raw_policy_offset_bone,max_raw_policy_offset_degrees,mean_raw_policy_offset_degrees,max_lower_limb_limit_occupancy_bone,max_lower_limb_limit_occupancy,max_lower_limb_limit_proxy_degrees,mean_lower_limb_limit_occupancy,num_lower_limb_targets_considered,root_height_delta_cm,root_linear_speed_cm_per_second,root_angular_speed_deg_per_second,height_exceeded,linear_speed_exceeded,angular_speed_exceeded,unstable_accumulated_seconds,num_bodies_considered,num_simulating_bodies,max_body_linear_speed_bone,max_body_linear_speed_cm_per_second,max_body_angular_speed_bone,max_body_angular_speed_deg_per_second,max_body_height_delta_bone,max_body_height_delta_cm");
}

FString FPhysAnimBridgeTraceWriter::BuildFrameCsvRow(const FPhysAnimBridgeTraceFrame& Frame)
{
	TArray<FString> Fields;
	Fields.Reserve(68);

	AppendCsvField(Fields, Frame.SessionId);
	AppendCsvField(Fields, Frame.TraceVersion);
	AppendCsvField(Fields, Frame.FrameIndex);
	AppendCsvField(Fields, Frame.WorldTimeSeconds);
	AppendCsvField(Fields, Frame.DeltaTimeSeconds);
	AppendCsvField(Fields, Frame.bSampledPolicyStep);
	AppendCsvField(Fields, Frame.RuntimeState);
	AppendCsvField(Fields, Frame.NneRuntimeName);
	AppendCsvField(Fields, Frame.bPoseSearchValid);
	AppendCsvField(Fields, Frame.bRunSyncSucceeded);
	AppendCsvField(Fields, Frame.bUpdateControlsSucceeded);
	AppendCsvField(Fields, Frame.bPolicyInfluenceActive);
	AppendCsvField(Fields, Frame.bFirstPolicyEnabledFrame);
	AppendCsvField(Fields, Frame.NumPolicyTargetsWritten);
	AppendCsvField(Fields, Frame.ResolveContextMs);
	AppendCsvField(Fields, Frame.PoseSearchQueryMs);
	AppendCsvField(Fields, Frame.FuturePoseSampleMs);
	AppendCsvField(Fields, Frame.BodySampleMs);
	AppendCsvField(Fields, Frame.ObservationPackMs);
	AppendCsvField(Fields, Frame.InferenceMs);
	AppendCsvField(Fields, Frame.ActionConditionMs);
	AppendCsvField(Fields, Frame.ControlTargetMs);
	AppendCsvField(Fields, Frame.UpdateControlsMs);
	AppendCsvField(Fields, Frame.InstabilityCheckMs);
	AppendCsvField(Fields, Frame.BridgeTickTotalMs);
	AppendCsvField(Fields, Frame.SelfObservationRootHeight);
	AppendCsvField(Fields, Frame.SelfObservationMeanAbs);
	AppendCsvField(Fields, Frame.MimicTargetPosesMeanAbs);
	AppendCsvField(Fields, Frame.MimicTargetPosesMinFutureTimeSeconds);
	AppendCsvField(Fields, Frame.MimicTargetPosesMaxFutureTimeSeconds);
	AppendCsvField(Fields, Frame.TerrainMean);
	AppendCsvField(Fields, Frame.TerrainMin);
	AppendCsvField(Fields, Frame.TerrainMax);
	AppendCsvField(Fields, Frame.TerrainCenter);
	AppendCsvField(Fields, Frame.MovementSmokePhaseName);
	AppendCsvField(Fields, Frame.bDistalLocomotionCompositionModeActive);
	AppendCsvField(Fields, Frame.ActionDiagnostics.RawMin);
	AppendCsvField(Fields, Frame.ActionDiagnostics.RawMax);
	AppendCsvField(Fields, Frame.ActionDiagnostics.RawMeanAbs);
	AppendCsvField(Fields, Frame.ActionDiagnostics.ConditionedMeanAbs);
	AppendCsvField(Fields, Frame.ActionDiagnostics.NumClampedActionFloats);
	AppendCsvField(Fields, Frame.ControlTargetDiagnostics.MaxTargetDeltaBoneName.ToString());
	AppendCsvField(Fields, Frame.ControlTargetDiagnostics.MaxTargetDeltaDegrees);
	AppendCsvField(Fields, Frame.ControlTargetDiagnostics.MeanTargetDeltaDegrees);
	AppendCsvField(Fields, Frame.ControlTargetDiagnostics.MaxRawPolicyOffsetBoneName.ToString());
	AppendCsvField(Fields, Frame.ControlTargetDiagnostics.MaxRawPolicyOffsetDegrees);
	AppendCsvField(Fields, Frame.ControlTargetDiagnostics.MeanRawPolicyOffsetDegrees);
	AppendCsvField(Fields, Frame.ControlTargetDiagnostics.MaxLowerLimbLimitOccupancyBoneName.ToString());
	AppendCsvField(Fields, Frame.ControlTargetDiagnostics.MaxLowerLimbLimitOccupancy);
	AppendCsvField(Fields, Frame.ControlTargetDiagnostics.MaxLowerLimbLimitProxyDegrees);
	AppendCsvField(Fields, Frame.ControlTargetDiagnostics.MeanLowerLimbLimitOccupancy);
	AppendCsvField(Fields, Frame.ControlTargetDiagnostics.NumLowerLimbTargetsConsidered);
	AppendCsvField(Fields, Frame.InstabilityDiagnostics.RootHeightDeltaCm);
	AppendCsvField(Fields, Frame.InstabilityDiagnostics.RootLinearSpeedCmPerSecond);
	AppendCsvField(Fields, Frame.InstabilityDiagnostics.RootAngularSpeedDegPerSecond);
	AppendCsvField(Fields, Frame.InstabilityDiagnostics.bHeightExceeded);
	AppendCsvField(Fields, Frame.InstabilityDiagnostics.bLinearSpeedExceeded);
	AppendCsvField(Fields, Frame.InstabilityDiagnostics.bAngularSpeedExceeded);
	AppendCsvField(Fields, Frame.InstabilityDiagnostics.UnstableAccumulatedSeconds);
	AppendCsvField(Fields, Frame.InstabilityDiagnostics.NumBodiesConsidered);
	AppendCsvField(Fields, Frame.InstabilityDiagnostics.NumSimulatingBodies);
	AppendCsvField(Fields, Frame.InstabilityDiagnostics.MaxLinearSpeedBoneName.ToString());
	AppendCsvField(Fields, Frame.InstabilityDiagnostics.MaxBodyLinearSpeedCmPerSecond);
	AppendCsvField(Fields, Frame.InstabilityDiagnostics.MaxAngularSpeedBoneName.ToString());
	AppendCsvField(Fields, Frame.InstabilityDiagnostics.MaxBodyAngularSpeedDegPerSecond);
	AppendCsvField(Fields, Frame.InstabilityDiagnostics.MaxHeightDeltaBoneName.ToString());
	AppendCsvField(Fields, Frame.InstabilityDiagnostics.MaxBodyHeightDeltaCm);

	return FString::Join(Fields, TEXT(","));
}

bool FPhysAnimBridgeTraceWriter::WriteBytes(IFileHandle& Handle, const FString& Text)
{
	if (FailWritesAfterCount >= 0 && WriteCount >= FailWritesAfterCount)
	{
		return false;
	}

	FTCHARToUTF8 Converter(*Text);
	const bool bWriteSucceeded = Handle.Write(reinterpret_cast<const uint8*>(Converter.Get()), Converter.Length());
	if (bWriteSucceeded)
	{
		++WriteCount;
	}

	return bWriteSucceeded;
}

bool FPhysAnimBridgeTraceWriter::WriteWholeFile(const FString& FilePath, const FString& Text)
{
	if (FailWritesAfterCount >= 0 && WriteCount >= FailWritesAfterCount)
	{
		return false;
	}

	const bool bWriteSucceeded = FFileHelper::SaveStringToFile(Text, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	if (bWriteSucceeded)
	{
		++WriteCount;
	}

	return bWriteSucceeded;
}

void FPhysAnimBridgeTraceWriter::HandleWriteFailure(const FString& Context)
{
	UE_LOG(LogPhysAnimBridgeTrace, Warning, TEXT("[PhysAnimTrace] %s"), *Context);
	bWriteFailed = true;
	EventsBuffer.Reset();
	FramesBuffer.Reset();
	EventsFileHandle.Reset();
	FramesFileHandle.Reset();
}
