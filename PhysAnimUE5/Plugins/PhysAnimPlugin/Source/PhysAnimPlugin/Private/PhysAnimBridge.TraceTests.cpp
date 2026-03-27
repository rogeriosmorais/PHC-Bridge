#if WITH_DEV_AUTOMATION_TESTS

#include "PhysAnimBridgeTrace.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/Csv/CsvParser.h"

namespace
{
	using namespace PhysAnimBridge;

	FString CreateTraceTestRootPath()
	{
		const FString RootPath = FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation"),
			TEXT("PhysAnimTraceTests"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits));
		IFileManager::Get().MakeDirectory(*RootPath, true);
		return RootPath;
	}

	void DeleteTraceTestRootPath(const FString& RootPath)
	{
		IFileManager::Get().DeleteDirectory(*RootPath, false, true);
	}

	int32 CountDelimitedFields(const FString& Value)
	{
		const FCsvParser Parser(Value);
		const FCsvParser::FRows& Rows = Parser.GetRows();
		return Rows.Num() > 0 ? Rows[0].Num() : 0;
	}

	FPhysAnimBridgeTraceSessionMetadata MakeTraceSessionMetadata(const FString& SessionId)
	{
		FPhysAnimBridgeTraceSessionMetadata Metadata;
		Metadata.TraceVersion = 1;
		Metadata.SessionId = SessionId;
		Metadata.TimestampUtc = TEXT("2026-03-12T12:34:56Z");
		Metadata.MapName = TEXT("TraceMap");
		Metadata.ActorName = TEXT("TraceActor");
		Metadata.RuntimeState = TEXT("BridgeActive");
		Metadata.NneRuntimeName = TEXT("NNERuntimeORTDml");
		Metadata.ModelAssetPath = TEXT("/Game/NNEModels/phc_policy");
		Metadata.PoseSearchDatabasePath = TEXT("/Game/PoseSearch/Databases/PSDB_Stage1_Locomotion");
		Metadata.TraceMode = 2;
		Metadata.SampleEveryNthFrame = 1;
		Metadata.FlushIntervalSeconds = 0.5f;
		Metadata.bForceZeroActions = false;
		Metadata.ActionScale = 0.1f;
		Metadata.ActionClampAbs = 0.2f;
		Metadata.ActionSmoothingAlpha = 0.25f;
		Metadata.StartupRampSeconds = 1.0f;
		Metadata.PolicyControlRateHz = 30.0f;
		Metadata.MaxAngularStepDegreesPerSecond = 180.0f;
		Metadata.AngularStrengthMultiplier = 0.35f;
		Metadata.AngularDampingRatioMultiplier = 1.5f;
		Metadata.AngularExtraDampingMultiplier = 2.0f;
		Metadata.bEnableInstabilityFailStop = true;
		Metadata.MaxRootHeightDeltaCm = 120.0f;
		Metadata.MaxRootLinearSpeedCmPerSecond = 1200.0f;
		Metadata.MaxRootAngularSpeedDegPerSecond = 720.0f;
		Metadata.InstabilityGracePeriodSeconds = 0.25f;
		Metadata.StabilizationSummary = TEXT("Trace summary");
		return Metadata;
	}

	FPhysAnimBridgeTraceEvent MakeTraceEvent(const FString& SessionId)
	{
		FPhysAnimBridgeTraceEvent Event;
		Event.SessionId = SessionId;
		Event.EventTimeSeconds = 12.5;
		Event.EventType = TEXT("runtime_state_transition");
		Event.RuntimeState = TEXT("BridgeActive");
		Event.Message = TEXT("Runtime state: ReadyForActivation -> BridgeActive");
		Event.Error = TEXT("None");
		Event.PreviousRuntimeState = TEXT("ReadyForActivation");
		Event.NewRuntimeState = TEXT("BridgeActive");
		Event.NneRuntimeName = TEXT("NNERuntimeORTDml");
		Event.ModelAssetPath = TEXT("/Game/NNEModels/phc_policy");
		Event.MapName = TEXT("TraceMap");
		Event.ActorName = TEXT("TraceActor");
		return Event;
	}

	FPhysAnimBridgeTraceFrame MakeTraceFrame(const FString& SessionId)
	{
		FPhysAnimBridgeTraceFrame Frame;
		Frame.SessionId = SessionId;
		Frame.TraceVersion = 1;
		Frame.FrameIndex = 42;
		Frame.WorldTimeSeconds = 1.25;
		Frame.DeltaTimeSeconds = 1.0f / 60.0f;
		Frame.bSampledPolicyStep = true;
		Frame.RuntimeState = TEXT("BridgeActive");
		Frame.NneRuntimeName = TEXT("NNERuntimeORTDml");
		Frame.bPoseSearchValid = true;
		Frame.bRunSyncSucceeded = true;
		Frame.bUpdateControlsSucceeded = true;
		Frame.bPolicyInfluenceActive = true;
		Frame.bFirstPolicyEnabledFrame = false;
		Frame.NumNormalPolicyTargetsWritten = 21;
		Frame.NumHeldTargetsWritten = 0;
		Frame.NumTotalTargetsWritten = 21;
		Frame.ResolveContextMs = 0.0f;
		Frame.PoseSearchQueryMs = 0.4f;
		Frame.FuturePoseSampleMs = 0.3f;
		Frame.BodySampleMs = 0.2f;
		Frame.ObservationPackMs = 0.6f;
		Frame.InferenceMs = 0.5f;
		Frame.ActionConditionMs = 0.1f;
		Frame.ControlTargetMs = 0.7f;
		Frame.UpdateControlsMs = 0.2f;
		Frame.InstabilityCheckMs = 0.1f;
		Frame.BridgeTickTotalMs = 3.1f;
		Frame.SelfObservationRootHeight = 88.0f;
		Frame.SelfObservationMeanAbs = 12.5f;
		Frame.MimicTargetPosesMeanAbs = 6.2f;
		Frame.MimicTargetPosesMinFutureTimeSeconds = 1.0f / 30.0f;
		Frame.MimicTargetPosesMaxFutureTimeSeconds = 0.5f;
		Frame.TerrainMean = 2.0f;
		Frame.TerrainMin = -1.0f;
		Frame.TerrainMax = 4.0f;
		Frame.TerrainCenter = 1.5f;
		Frame.MovementSmokePhaseName = TEXT("Forward");
		Frame.bDistalLocomotionCompositionModeActive = true;
		Frame.ActionDiagnostics.RawMin = -0.4f;
		Frame.ActionDiagnostics.RawMax = 0.6f;
		Frame.ActionDiagnostics.RawMeanAbs = 0.2f;
		Frame.ActionDiagnostics.ConditionedMeanAbs = 0.1f;
		Frame.ActionDiagnostics.NumClampedActionFloats = 2;
		Frame.ControlTargetDiagnostics.MaxTargetDeltaBoneName = TEXT("foot_l");
		Frame.ControlTargetDiagnostics.MaxTargetDeltaDegrees = 12.0f;
		Frame.ControlTargetDiagnostics.MeanTargetDeltaDegrees = 3.5f;
		Frame.ControlTargetDiagnostics.MaxRawPolicyOffsetBoneName = TEXT("thigh_r");
		Frame.ControlTargetDiagnostics.MaxRawPolicyOffsetDegrees = 15.0f;
		Frame.ControlTargetDiagnostics.MeanRawPolicyOffsetDegrees = 4.0f;
		Frame.ControlTargetDiagnostics.MaxLowerLimbLimitOccupancyBoneName = TEXT("foot_r");
		Frame.ControlTargetDiagnostics.MaxLowerLimbLimitOccupancy = 0.7f;
		Frame.ControlTargetDiagnostics.MaxLowerLimbLimitProxyDegrees = 20.0f;
		Frame.ControlTargetDiagnostics.MeanLowerLimbLimitOccupancy = 0.3f;
		Frame.ControlTargetDiagnostics.NumLowerLimbTargetsConsidered = 8;
		Frame.InstabilityDiagnostics.RootHeightDeltaCm = 2.5f;
		Frame.InstabilityDiagnostics.RootLinearSpeedCmPerSecond = 120.0f;
		Frame.InstabilityDiagnostics.RootAngularSpeedDegPerSecond = 45.0f;
		Frame.InstabilityDiagnostics.bHeightExceeded = false;
		Frame.InstabilityDiagnostics.bLinearSpeedExceeded = false;
		Frame.InstabilityDiagnostics.bAngularSpeedExceeded = false;
		Frame.InstabilityDiagnostics.UnstableAccumulatedSeconds = 0.0f;
		Frame.InstabilityDiagnostics.NumBodiesConsidered = 22;
		Frame.InstabilityDiagnostics.NumSimulatingBodies = 21;
		Frame.InstabilityDiagnostics.MaxLinearSpeedBoneName = TEXT("foot_l");
		Frame.InstabilityDiagnostics.MaxBodyLinearSpeedCmPerSecond = 220.0f;
		Frame.InstabilityDiagnostics.MaxAngularSpeedBoneName = TEXT("spine_01");
		Frame.InstabilityDiagnostics.MaxBodyAngularSpeedDegPerSecond = 180.0f;
		Frame.InstabilityDiagnostics.MaxHeightDeltaBoneName = TEXT("head");
		Frame.InstabilityDiagnostics.MaxBodyHeightDeltaCm = 15.0f;
		return Frame;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimTraceSessionMetadataSerializationTest,
		"PhysAnim.Trace.SessionMetadataSerialization",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimTraceSessionMetadataSerializationTest::RunTest(const FString& Parameters)
	{
		const FString Json = FPhysAnimBridgeTraceWriter::BuildSessionMetadataJson(MakeTraceSessionMetadata(TEXT("trace-session")));
		TestTrue(TEXT("Session metadata includes session id"), Json.Contains(TEXT("\"session_id\": \"trace-session\"")));
		TestTrue(TEXT("Session metadata includes runtime name"), Json.Contains(TEXT("\"nne_runtime_name\": \"NNERuntimeORTDml\"")));
		TestTrue(TEXT("Session metadata includes stabilization summary"), Json.Contains(TEXT("\"stabilization_summary\": \"Trace summary\"")));
		TestTrue(TEXT("Session metadata preserves schema version"), Json.Contains(TEXT("\"trace_version\": 1")));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimTraceFrameCsvHeaderTest,
		"PhysAnim.Trace.FrameCsvHeader",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimTraceFrameCsvHeaderTest::RunTest(const FString& Parameters)
	{
		TestEqual(
			TEXT("Frame CSV header order stays stable"),
			FPhysAnimBridgeTraceWriter::BuildFrameCsvHeader(),
			FString(TEXT("session_id,trace_version,frame_index,world_time_seconds,delta_time_seconds,sampled_policy_step,runtime_state,nne_runtime_name,pose_search_valid,run_sync_succeeded,update_controls_succeeded,policy_influence_active,first_policy_enabled_frame,num_normal_policy_targets_written,num_hold_targets_written,num_total_targets_written,resolve_context_ms,pose_search_query_ms,future_pose_sample_ms,body_sample_ms,observation_pack_ms,inference_ms,action_condition_ms,control_target_ms,update_controls_ms,instability_check_ms,bridge_tick_total_ms,self_observation_root_height,self_observation_mean_abs,mimic_target_poses_mean_abs,mimic_target_poses_min_future_time_seconds,mimic_target_poses_max_future_time_seconds,terrain_mean,terrain_min,terrain_max,terrain_center,movement_smoke_phase,distal_locomotion_composition_mode_active,raw_action_min,raw_action_max,raw_action_mean_abs,conditioned_action_mean_abs,num_clamped_action_floats,max_target_delta_bone,max_target_delta_degrees,mean_target_delta_degrees,max_raw_policy_offset_bone,max_raw_policy_offset_degrees,mean_raw_policy_offset_degrees,max_lower_limb_limit_occupancy_bone,max_lower_limb_limit_occupancy,max_lower_limb_limit_proxy_degrees,mean_lower_limb_limit_occupancy,num_lower_limb_targets_considered,root_height_delta_cm,root_linear_speed_cm_per_second,root_angular_speed_deg_per_second,height_exceeded,linear_speed_exceeded,angular_speed_exceeded,unstable_accumulated_seconds,num_bodies_considered,num_simulating_bodies,max_body_linear_speed_bone,max_body_linear_speed_cm_per_second,max_body_angular_speed_bone,max_body_angular_speed_deg_per_second,max_body_height_delta_bone,max_body_height_delta_cm")));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimTraceFrameCsvRowSerializationTest,
		"PhysAnim.Trace.FrameCsvRowSerialization",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimTraceFrameCsvRowSerializationTest::RunTest(const FString& Parameters)
	{
		const FString Header = FPhysAnimBridgeTraceWriter::BuildFrameCsvHeader();
		const FString Row = FPhysAnimBridgeTraceWriter::BuildFrameCsvRow(MakeTraceFrame(TEXT("trace-session")));
		TestEqual(TEXT("Serialized row field count matches the header"), CountDelimitedFields(Row), CountDelimitedFields(Header));
		TestTrue(TEXT("Serialized row contains the runtime state"), Row.Contains(TEXT("\"BridgeActive\"")));
		TestTrue(TEXT("Serialized row contains the movement smoke phase"), Row.Contains(TEXT("\"Forward\"")));
		TestTrue(TEXT("Serialized row contains the max target delta bone"), Row.Contains(TEXT("\"foot_l\"")));
		TestTrue(TEXT("Serialized row contains the max body height delta bone"), Row.Contains(TEXT("\"head\"")));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimTraceEventJsonSerializationTest,
		"PhysAnim.Trace.EventJsonSerialization",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimTraceEventJsonSerializationTest::RunTest(const FString& Parameters)
	{
		TestEqual(
			TEXT("Event JSON line stays stable"),
			FPhysAnimBridgeTraceWriter::BuildEventJsonLine(MakeTraceEvent(TEXT("trace-session"))),
			FString(TEXT("{\"session_id\":\"trace-session\",\"event_time_seconds\":12.500000,\"event_type\":\"runtime_state_transition\",\"runtime_state\":\"BridgeActive\",\"message\":\"Runtime state: ReadyForActivation -> BridgeActive\",\"error\":\"None\",\"previous_runtime_state\":\"ReadyForActivation\",\"new_runtime_state\":\"BridgeActive\",\"nne_runtime_name\":\"NNERuntimeORTDml\",\"model_asset_path\":\"/Game/NNEModels/phc_policy\",\"map_name\":\"TraceMap\",\"actor_name\":\"TraceActor\"}")));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimTraceModeGatingTest,
		"PhysAnim.Trace.ModeGating",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimTraceModeGatingTest::RunTest(const FString& Parameters)
	{
		const FString RootPath = CreateTraceTestRootPath();

		FString Error;
		FPhysAnimBridgeTraceWriter DisabledWriter(EPhysAnimBridgeTraceMode::Off);
		TestTrue(TEXT("Disabled trace session start succeeds as a no-op"), DisabledWriter.StartSession(RootPath, MakeTraceSessionMetadata(TEXT("off-session")), Error));
		TestFalse(
			TEXT("Disabled trace mode creates no session folder"),
			IFileManager::Get().DirectoryExists(*FPaths::Combine(RootPath, TEXT("off-session"))));

		FPhysAnimBridgeTraceWriter MetadataWriter(EPhysAnimBridgeTraceMode::MetadataAndEvents);
		TestTrue(TEXT("Metadata/events trace session starts"), MetadataWriter.StartSession(RootPath, MakeTraceSessionMetadata(TEXT("events-session")), Error));
		MetadataWriter.AppendEvent(MakeTraceEvent(TEXT("events-session")));
		MetadataWriter.AppendFrame(MakeTraceFrame(TEXT("events-session")));
		MetadataWriter.Flush(true);
		MetadataWriter.Shutdown();
		TestTrue(
			TEXT("Metadata/events mode writes session metadata"),
			IFileManager::Get().FileExists(*FPaths::Combine(RootPath, TEXT("events-session"), TEXT("session.json"))));
		TestTrue(
			TEXT("Metadata/events mode writes the event log"),
			IFileManager::Get().FileExists(*FPaths::Combine(RootPath, TEXT("events-session"), TEXT("events.jsonl"))));
		TestFalse(
			TEXT("Metadata/events mode skips the frame CSV"),
			IFileManager::Get().FileExists(*FPaths::Combine(RootPath, TEXT("events-session"), TEXT("frames.csv"))));

		FPhysAnimBridgeTraceWriter FullWriter(EPhysAnimBridgeTraceMode::Full);
		TestTrue(TEXT("Full trace session starts"), FullWriter.StartSession(RootPath, MakeTraceSessionMetadata(TEXT("full-session")), Error));
		FullWriter.AppendEvent(MakeTraceEvent(TEXT("full-session")));
		FullWriter.AppendFrame(MakeTraceFrame(TEXT("full-session")));
		FullWriter.Flush(true);
		FullWriter.Shutdown();
		TestTrue(
			TEXT("Full trace mode writes the frame CSV"),
			IFileManager::Get().FileExists(*FPaths::Combine(RootPath, TEXT("full-session"), TEXT("frames.csv"))));

		DeleteTraceTestRootPath(RootPath);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimTraceWriterFailureDisablesFurtherWritesTest,
		"PhysAnim.Trace.WriterFailureDisablesFurtherWrites",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimTraceWriterFailureDisablesFurtherWritesTest::RunTest(const FString& Parameters)
	{
		const FString RootPath = CreateTraceTestRootPath();
		const FString SessionId = TEXT("failure-session");
		const FString FramesPath = FPaths::Combine(RootPath, SessionId, TEXT("frames.csv"));
		const FString EventsPath = FPaths::Combine(RootPath, SessionId, TEXT("events.jsonl"));

		FString Error;
		FPhysAnimBridgeTraceWriter Writer(EPhysAnimBridgeTraceMode::Full);
		Writer.SetFailWritesAfterCountForTesting(2);
		TestTrue(TEXT("Failure test trace session starts before the injected write failure"), Writer.StartSession(RootPath, MakeTraceSessionMetadata(SessionId), Error));
		Writer.AppendEvent(MakeTraceEvent(SessionId));
		Writer.Flush(true);
		TestTrue(TEXT("A flush write failure disables the writer"), Writer.HasWriteFailed());
		Writer.AppendFrame(MakeTraceFrame(SessionId));
		Writer.AppendEvent(MakeTraceEvent(SessionId));
		Writer.Flush(true);
		Writer.Shutdown();

		FString FramesText;
		TestTrue(TEXT("Frame CSV still exists after the failure"), FFileHelper::LoadFileToString(FramesText, *FramesPath));
		TestEqual(TEXT("Events JSONL stays empty after the injected failure"), IFileManager::Get().FileSize(*EventsPath), 0LL);
		TestEqual(TEXT("Only the frame header survives the injected failure"), FramesText, FPhysAnimBridgeTraceWriter::BuildFrameCsvHeader() + LINE_TERMINATOR);

		DeleteTraceTestRootPath(RootPath);
		return true;
	}
}

#endif
