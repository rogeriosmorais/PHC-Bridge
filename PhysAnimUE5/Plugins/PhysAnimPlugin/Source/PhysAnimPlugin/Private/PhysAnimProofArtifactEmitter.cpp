#include "PhysAnimProofArtifactEmitter.h"
#include "PhysAnimComponentPrivate.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	FString PhysAnimProof_SanitizeFileToken(const FString& Value)
	{
		FString Sanitized = FPaths::MakeValidFileName(Value);
		Sanitized.ReplaceInline(TEXT(" "), TEXT("_"));
		return Sanitized.IsEmpty() ? TEXT("unknown") : Sanitized;
	}

	void PhysAnimProof_SetOptionalBool(
		const TSharedRef<FJsonObject>& Json,
		const TCHAR* FieldName,
		const TOptional<bool>& Value)
	{
		if (Value.IsSet())
		{
			Json->SetBoolField(FieldName, Value.GetValue());
		}
		else
		{
			Json->SetField(FieldName, MakeShared<FJsonValueNull>());
		}
	}

	void PhysAnimProof_SetOptionalNumber(
		const TSharedRef<FJsonObject>& Json,
		const TCHAR* FieldName,
		const TOptional<double>& Value)
	{
		if (Value.IsSet())
		{
			Json->SetNumberField(FieldName, Value.GetValue());
		}
		else
		{
			Json->SetField(FieldName, MakeShared<FJsonValueNull>());
		}
	}

	TArray<TSharedPtr<FJsonValue>> PhysAnimProof_Vector2DArrayToJson(const TArray<FVector2D>& Points)
	{
		TArray<TSharedPtr<FJsonValue>> Result;
		Result.Reserve(Points.Num());

		for (const FVector2D& Point : Points)
		{
			const TSharedRef<FJsonObject> PointJson = MakeShared<FJsonObject>();
			PointJson->SetNumberField(TEXT("x"), Point.X);
			PointJson->SetNumberField(TEXT("y"), Point.Y);
			Result.Add(MakeShared<FJsonValueObject>(PointJson));
		}

		return Result;
	}

	TArray<TSharedPtr<FJsonValue>> PhysAnimProof_TerminalReasonArrayToJson(const TArray<EPhysAnimTerminalReason>& Reasons)
	{
		TArray<TSharedPtr<FJsonValue>> Result;
		Result.Reserve(Reasons.Num());

		for (const EPhysAnimTerminalReason Reason : Reasons)
		{
			const TSharedRef<FJsonObject> ReasonJson = MakeShared<FJsonObject>();
			ReasonJson->SetNumberField(TEXT("value"), static_cast<int32>(Reason));
			ReasonJson->SetStringField(TEXT("name"), PhysAnimProofArtifactEmitter::ToTerminalReasonString(Reason));
			Result.Add(MakeShared<FJsonValueObject>(ReasonJson));
		}

		return Result;
	}

	TSharedRef<FJsonObject> PhysAnimProof_ArtifactToJson(
		const FPhysAnimRunArtifactSnapshot& Artifact,
		const FPhysAnimProofArtifactEmitInput& Input)
	{
		const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();

		Json->SetStringField(TEXT("attempt_uuid"), Artifact.AttemptUuid);
		Json->SetStringField(TEXT("emitter_attempt_uuid"), Input.AttemptUuid);
		Json->SetNumberField(TEXT("timestamp"), Artifact.Timestamp);
		Json->SetStringField(TEXT("baseline_id"), Artifact.BaselineId);
		Json->SetStringField(TEXT("standing_reference_id"), Artifact.StandingReferenceId);

		Json->SetNumberField(TEXT("standing_seconds_at_emit"), Input.StandingSeconds);
		Json->SetNumberField(TEXT("runtime_hit_count"), Input.RuntimeHitCount);
		Json->SetNumberField(TEXT("mapped_support_hit_count"), Input.MappedSupportHitCount);

		Json->SetBoolField(TEXT("physics_asset_contract_valid"), Artifact.bPhysicsAssetContractValid);
		Json->SetBoolField(TEXT("skeleton_audit_passed"), Artifact.bSkeletonAuditPassed);
		Json->SetNumberField(TEXT("plant_failure_class"), static_cast<int32>(Artifact.PlantFailureClass));
		Json->SetNumberField(TEXT("plant_failure_field"), static_cast<int32>(Artifact.PlantFailureField));
		Json->SetNumberField(TEXT("mass_drift_total_pct"), Artifact.MassDriftTotalPct);

		Json->SetNumberField(TEXT("capsule_collision_enabled"), static_cast<int32>(Artifact.CapsuleCollisionEnabled));
		Json->SetBoolField(TEXT("capsule_generate_overlap_events"), Artifact.bCapsuleGenerateOverlapEvents);
		Json->SetNumberField(TEXT("capsule_world_pos_x"), Artifact.CapsuleWorldPosCm.X);
		Json->SetNumberField(TEXT("capsule_world_pos_y"), Artifact.CapsuleWorldPosCm.Y);
		Json->SetNumberField(TEXT("capsule_world_pos_z"), Artifact.CapsuleWorldPosCm.Z);
		Json->SetNumberField(TEXT("capsule_lock_delta_cm"), Artifact.CapsuleLockDeltaCm);
		Json->SetBoolField(TEXT("mesh_uses_absolute_location"), Artifact.bMeshUsesAbsoluteLocation);
		Json->SetBoolField(TEXT("mesh_uses_absolute_rotation"), Artifact.bMeshUsesAbsoluteRotation);
		Json->SetBoolField(TEXT("mesh_uses_absolute_scale"), Artifact.bMeshUsesAbsoluteScale);
		Json->SetBoolField(TEXT("cmc_is_active"), Artifact.bCmcIsActive);
		Json->SetBoolField(TEXT("cmc_tick_enabled"), Artifact.bCmcTickEnabled);
		Json->SetStringField(TEXT("cmc_movement_mode"), Artifact.CmcMovementMode.ToString());
		Json->SetBoolField(TEXT("cmc_updated_component_is_null"), Artifact.bCmcUpdatedComponentIsNull);

		Json->SetNumberField(TEXT("hold_duration_sec"), Artifact.HoldDurationSec);
		Json->SetNumberField(TEXT("support_uptime_sec"), Artifact.SupportUptimeSec);
		Json->SetNumberField(TEXT("max_root_tilt_deg"), Artifact.MaxRootTiltDeg);
		Json->SetNumberField(TEXT("peak_angular_speed_deg_per_sec"), Artifact.PeakAngularSpeedDegPerSec);
		Json->SetNumberField(TEXT("rms_mismatch_deg"), Artifact.RmsMismatchDeg);
		Json->SetNumberField(TEXT("max_body_mismatch_deg"), Artifact.MaxBodyMismatchDeg);
		Json->SetNumberField(TEXT("target_discontinuity_deg"), Artifact.TargetDiscontinuityDeg);
		Json->SetNumberField(TEXT("target_discontinuity_phase"), static_cast<int32>(Artifact.TargetDiscontinuityPhase));
		Json->SetNumberField(TEXT("mismatch_duration_ms"), Artifact.MismatchDurationMs);
		Json->SetNumberField(TEXT("controller_gain_scale"), Artifact.ControllerGainScale);
		Json->SetNumberField(TEXT("controller_damping_ratio"), Artifact.ControllerDampingRatio);
		Json->SetBoolField(TEXT("controller_gain_damping_valid"), Artifact.bControllerGainDampingValid);
		Json->SetNumberField(TEXT("controller_stability_failure_field"), static_cast<int32>(Artifact.ControllerStabilityFailureField));
		Json->SetNumberField(TEXT("standing_validation_timeout_sec"), Artifact.StandingValidationTimeoutSec);
		Json->SetBoolField(TEXT("standing_validation_timed_out"), Artifact.bStandingValidationTimedOut);

		Json->SetBoolField(TEXT("support_state_l"), Artifact.bSupportStateL);
		Json->SetBoolField(TEXT("support_state_r"), Artifact.bSupportStateR);
		Json->SetNumberField(TEXT("support_mode"), static_cast<int32>(Artifact.SupportMode));
		Json->SetStringField(TEXT("support_mode_name"), PhysAnimProofArtifactEmitter::ToSupportModeString(Artifact.SupportMode));
		Json->SetNumberField(TEXT("support_gap_timer_ms"), Artifact.SupportGapTimerMs);
		PhysAnimProof_SetOptionalBool(Json, TEXT("proxy_inside_hull"), Artifact.ProxyInsideHull);
		PhysAnimProof_SetOptionalNumber(Json, TEXT("proxy_outside_hull_duration_ms"), Artifact.ProxyOutsideHullDurationMs);
		Json->SetNumberField(TEXT("active_support_side_count"), Artifact.ActiveSupportSideCount);
		Json->SetNumberField(TEXT("support_hull_area_cm2"), Artifact.SupportHullAreaCm2);
		Json->SetNumberField(TEXT("support_patch_area_l_cm2"), Artifact.SupportPatchAreaLCm2);
		Json->SetNumberField(TEXT("support_patch_area_r_cm2"), Artifact.SupportPatchAreaRCm2);
		Json->SetArrayField(TEXT("support_hull_points_cm"), PhysAnimProof_Vector2DArrayToJson(Artifact.SupportHullPointsCm));
		Json->SetNumberField(TEXT("com_proxy_pos_x"), Artifact.ComProxyPosCm.X);
		Json->SetNumberField(TEXT("com_proxy_pos_y"), Artifact.ComProxyPosCm.Y);
		Json->SetNumberField(TEXT("max_penetration_cm"), Artifact.MaxPenetrationCm);
		Json->SetNumberField(TEXT("support_churn_count"), Artifact.SupportChurnCount);
		Json->SetNumberField(TEXT("support_churn_hz"), Artifact.SupportChurnHz);
		Json->SetBoolField(TEXT("calf_world_contact_l"), Artifact.bCalfWorldContactL);
		Json->SetBoolField(TEXT("calf_world_contact_r"), Artifact.bCalfWorldContactR);
		Json->SetBoolField(TEXT("calf_contact_terminal"), Artifact.bCalfContactTerminal);

		Json->SetNumberField(TEXT("control_alpha"), Artifact.ControlAlpha);
		Json->SetNumberField(TEXT("policy_inference_success_count"), Artifact.PolicyInferenceSuccessCount);
		Json->SetNumberField(TEXT("policy_action_sample_count"), Artifact.PolicyActionSampleCount);
		Json->SetNumberField(TEXT("policy_action_raw_mean_abs_max"), Artifact.PolicyActionRawMeanAbsMax);
		Json->SetNumberField(TEXT("policy_action_conditioned_mean_abs_max"), Artifact.PolicyActionConditionedMeanAbsMax);
		Json->SetNumberField(TEXT("policy_action_clamped_float_max"), Artifact.PolicyActionClampedFloatMax);
		Json->SetNumberField(TEXT("control_target_sample_count"), Artifact.ControlTargetSampleCount);
		Json->SetNumberField(TEXT("control_target_normal_writes"), Artifact.ControlTargetNormalWrites);
		Json->SetNumberField(TEXT("control_target_total_writes"), Artifact.ControlTargetTotalWrites);
		Json->SetNumberField(TEXT("control_target_max_delta_deg"), Artifact.ControlTargetMaxDeltaDeg);
		Json->SetNumberField(TEXT("control_target_mean_delta_deg_max"), Artifact.ControlTargetMeanDeltaDegMax);
		Json->SetNumberField(TEXT("control_target_max_raw_policy_offset_deg"), Artifact.ControlTargetMaxRawPolicyOffsetDeg);
		Json->SetNumberField(TEXT("control_target_mean_raw_policy_offset_deg_max"), Artifact.ControlTargetMeanRawPolicyOffsetDegMax);
		Json->SetNumberField(TEXT("runtime_body_sample_count"), Artifact.RuntimeBodySampleCount);
		Json->SetNumberField(TEXT("runtime_simulating_body_count"), Artifact.RuntimeSimulatingBodyCount);
		Json->SetNumberField(TEXT("runtime_max_body_linear_speed_cm_per_second"), Artifact.RuntimeMaxBodyLinearSpeedCmPerSecond);
		Json->SetNumberField(TEXT("runtime_max_body_angular_speed_deg_per_second"), Artifact.RuntimeMaxBodyAngularSpeedDegPerSecond);
		Json->SetBoolField(TEXT("physical_perturbation_applied"), Artifact.bPhysicalPerturbationApplied);
		Json->SetNumberField(TEXT("perturbation_measured_delta_v_cm_per_second"), Artifact.PerturbationMeasuredDeltaVCmPerSecond);
		Json->SetStringField(TEXT("shell_bookkeeping_state"), Artifact.ShellBookkeepingState);
		Json->SetNumberField(TEXT("shell_influence_materiality"), Artifact.ShellInfluenceMateriality);
		Json->SetNumberField(TEXT("topology_change_count"), Artifact.TopologyChangeCount);
		Json->SetNumberField(TEXT("authority_conflict_count"), Artifact.AuthorityConflictCount);
		Json->SetNumberField(TEXT("shell_helper_used_count"), Artifact.ShellHelperUsedCount);
		Json->SetNumberField(TEXT("movement_reclaim_count"), Artifact.MovementReclaimCount);
		Json->SetBoolField(TEXT("continuity_bookkeeping_mismatch"), Artifact.bContinuityBookkeepingMismatch);
		Json->SetNumberField(TEXT("pelvis_sleep_duration_ms"), Artifact.PelvisSleepDurationMs);
		Json->SetBoolField(TEXT("physical_continuity_validator_passed"), Artifact.bPhysicalContinuityValidatorPassed);

		Json->SetNumberField(TEXT("contamination_class"), static_cast<int32>(Artifact.ContaminationClass));
		Json->SetStringField(TEXT("contamination_source_body"), Artifact.ContaminationSourceBody.ToString());
		Json->SetStringField(TEXT("contamination_source_subsystem"), Artifact.ContaminationSourceSubsystem.ToString());
		Json->SetBoolField(TEXT("mesh_wide_assist_detected"), Artifact.bMeshWideAssistDetected);
		Json->SetBoolField(TEXT("non_critical_body_assist_detected"), Artifact.bNonCriticalBodyAssistDetected);
		Json->SetStringField(TEXT("excluded_body_world_contact_source"), Artifact.ExcludedBodyWorldContactSource.ToString());
		Json->SetNumberField(TEXT("global_blend_weight"), Artifact.GlobalBlendWeight);
		Json->SetBoolField(TEXT("mesh_update_when_kinematic_enabled"), Artifact.bMeshUpdateWhenKinematicEnabled);

		Json->SetNumberField(TEXT("terminal_reason"), static_cast<int32>(Artifact.TerminalReason));
		Json->SetStringField(TEXT("terminal_reason_name"), PhysAnimProofArtifactEmitter::ToTerminalReasonString(Artifact.TerminalReason));
		Json->SetArrayField(TEXT("co_terminal_reasons"), PhysAnimProof_TerminalReasonArrayToJson(Artifact.CoTerminalReasons));
		Json->SetNumberField(TEXT("terminal_substep_timestamp"), static_cast<double>(Artifact.TerminalSubstepTimestamp));
		Json->SetBoolField(TEXT("terminal_frame_artifact_captured"), Artifact.bTerminalFrameArtifactCaptured);

		return Json;
	}
}

namespace PhysAnimProofArtifactEmitter
{
	FString ToTerminalReasonString(const EPhysAnimTerminalReason Reason)
	{
		switch (Reason)
		{
		case EPhysAnimTerminalReason::None:
			return TEXT("None");
		case EPhysAnimTerminalReason::ActivationPhysicsAssetContractViolation:
			return TEXT("ActivationPhysicsAssetContractViolation");
		case EPhysAnimTerminalReason::ActivationCapsuleContractViolation:
			return TEXT("ActivationCapsuleContractViolation");
		case EPhysAnimTerminalReason::ActivationTopologyChange:
			return TEXT("ActivationTopologyChange");
		case EPhysAnimTerminalReason::ActivationContinuousSimulationLost:
			return TEXT("ActivationContinuousSimulationLost");
		case EPhysAnimTerminalReason::ActivationPhysicsNotStarted:
			return TEXT("ActivationPhysicsNotStarted");
		case EPhysAnimTerminalReason::ActivationSupportFailure:
			return TEXT("ActivationSupportFailure");
		case EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion:
			return TEXT("ActivationProxyOutsideSupportRegion");
		case EPhysAnimTerminalReason::ActivationTargetDiscontinuity:
			return TEXT("ActivationTargetDiscontinuity");
		case EPhysAnimTerminalReason::ActivationUnstableGainOrDamping:
			return TEXT("ActivationUnstableGainOrDamping");
		case EPhysAnimTerminalReason::ActivationInstabilityThresholdBreach:
			return TEXT("ActivationInstabilityThresholdBreach");
		case EPhysAnimTerminalReason::ActivationPoseReferenceMismatch:
			return TEXT("ActivationPoseReferenceMismatch");
		case EPhysAnimTerminalReason::ActivationMovementReclaim:
			return TEXT("ActivationMovementReclaim");
		case EPhysAnimTerminalReason::ActivationShellHelperViolation:
			return TEXT("ActivationShellHelperViolation");
		case EPhysAnimTerminalReason::ActivationAuthorityConflict:
			return TEXT("ActivationAuthorityConflict");
		case EPhysAnimTerminalReason::ActivationStandingValidationTimeout:
			return TEXT("ActivationStandingValidationTimeout");
		default:
			return TEXT("Unknown");
		}
	}

	FString ToSupportModeString(const EPhysAnimSupportMode Mode)
	{
		switch (Mode)
		{
		case EPhysAnimSupportMode::TwoFootStable:
			return TEXT("TwoFootStable");
		case EPhysAnimSupportMode::SingleFootSurvival:
			return TEXT("SingleFootSurvival");
		case EPhysAnimSupportMode::TransientRecovery:
			return TEXT("TransientRecovery");
		case EPhysAnimSupportMode::Airborne:
			return TEXT("Airborne");
		default:
			return TEXT("Unknown");
		}
	}

	void LogAttemptStart(const FString& AttemptUuid)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("PhysAnimProof: AttemptStart uuid=%s"), *AttemptUuid);
	}

	void LogRuntimeEvidence(
		const FString& AttemptUuid,
		const int32 RuntimeHitCount,
		const int32 MappedSupportHitCount,
		const FPhysAnimRunArtifactSnapshot& Artifact)
	{
		UE_LOG(
			LogPhysAnimBridge,
			Verbose,
			TEXT("PhysAnimProof: RuntimeEvidence uuid=%s hits=%d mapped=%d support_mode=%s active_sides=%d hull_area=%.3f inference=%d action_samples=%d action_abs=%.3f control_samples=%d control_writes=%d control_delta=%.3f sim_bodies=%d max_body_lin=%.3f max_body_ang=%.3f"),
			*AttemptUuid,
			RuntimeHitCount,
			MappedSupportHitCount,
			*ToSupportModeString(Artifact.SupportMode),
			Artifact.ActiveSupportSideCount,
			Artifact.SupportHullAreaCm2,
			Artifact.PolicyInferenceSuccessCount,
			Artifact.PolicyActionSampleCount,
			Artifact.PolicyActionConditionedMeanAbsMax,
			Artifact.ControlTargetSampleCount,
			Artifact.ControlTargetTotalWrites,
			Artifact.ControlTargetMaxDeltaDeg,
			Artifact.RuntimeSimulatingBodyCount,
			Artifact.RuntimeMaxBodyLinearSpeedCmPerSecond,
			Artifact.RuntimeMaxBodyAngularSpeedDegPerSecond);
	}

	void LogStandingProgress(
		const FString& AttemptUuid,
		const double StandingSeconds,
		const EPhysAnimTerminalReason TerminalReason,
		const int32 RuntimeHitCount,
		const int32 MappedSupportHitCount,
		const FPhysAnimRunArtifactSnapshot& Artifact)
	{
		UE_LOG(
			LogPhysAnimBridge,
			Warning,
			TEXT("PhysAnimProof: StandingProgress uuid=%s t=%.3f terminal_reason=%s hits=%d mapped=%d support_mode=%s active_sides=%d hull_area=%.3f"),
			*AttemptUuid,
			StandingSeconds,
			*ToTerminalReasonString(TerminalReason),
			RuntimeHitCount,
			MappedSupportHitCount,
			*ToSupportModeString(Artifact.SupportMode),
			Artifact.ActiveSupportSideCount,
			Artifact.SupportHullAreaCm2);
	}

	FString BuildTerminalArtifactJsonPath(const FString& AttemptUuid)
	{
		const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PhysAnim"), TEXT("ProofArtifacts"));
		const FString FileName = FString::Printf(TEXT("%s_terminal.json"), *PhysAnimProof_SanitizeFileToken(AttemptUuid));
		return FPaths::Combine(Directory, FileName);
	}

	bool WriteTerminalArtifactJson(const FString& OutputPath, const FPhysAnimProofArtifactEmitInput& Input)
	{
		const FPhysAnimRunArtifactSnapshot& Artifact = Input.PipelineResult.StateApplyResult.State.TerminalArtifact;
		const TSharedRef<FJsonObject> Json = PhysAnimProof_ArtifactToJson(Artifact, Input);

		FString OutputString;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
		if (!FJsonSerializer::Serialize(Json, Writer))
		{
			return false;
		}

		const FString Directory = FPaths::GetPath(OutputPath);
		if (!IFileManager::Get().MakeDirectory(*Directory, true))
		{
			return false;
		}

		return FFileHelper::SaveStringToFile(OutputString, *OutputPath);
	}

	FPhysAnimProofArtifactEmitResult EmitTerminalArtifactAndWriteJson(const FPhysAnimProofArtifactEmitInput& Input)
	{
		FPhysAnimProofArtifactEmitResult Result;

		const FPhysAnimRunArtifactSnapshot& Artifact = Input.PipelineResult.StateApplyResult.State.TerminalArtifact;

		const FString ProxyInsideText =
			Artifact.ProxyInsideHull.IsSet()
				? (Artifact.ProxyInsideHull.GetValue() ? TEXT("true") : TEXT("false"))
				: TEXT("unset");

		const FString ProxyOutsideDurationText =
			Artifact.ProxyOutsideHullDurationMs.IsSet()
				? FString::Printf(TEXT("%.3f"), Artifact.ProxyOutsideHullDurationMs.GetValue())
				: TEXT("unset");

		Result.JsonPath = BuildTerminalArtifactJsonPath(Input.AttemptUuid);
		Result.bJsonWritten = WriteTerminalArtifactJson(Result.JsonPath, Input);

		UE_LOG(
			LogPhysAnimBridge,
			Warning,
			TEXT("PhysAnimProof: TerminalArtifact uuid=%s terminal_reason=%s timestamp=%lld support_mode=%s active_sides=%d hull_area=%.3f support_gap=%.3f proxy_inside=%s proxy_outside_duration=%s terminal_frame_captured=%d coterminal_count=%d artifact_json=%s artifact_json_written=%d"),
			*Input.AttemptUuid,
			*ToTerminalReasonString(Artifact.TerminalReason),
			static_cast<long long>(Artifact.TerminalSubstepTimestamp),
			*ToSupportModeString(Artifact.SupportMode),
			Artifact.ActiveSupportSideCount,
			Artifact.SupportHullAreaCm2,
			Artifact.SupportGapTimerMs,
			*ProxyInsideText,
			*ProxyOutsideDurationText,
			Artifact.bTerminalFrameArtifactCaptured ? 1 : 0,
			Artifact.CoTerminalReasons.Num(),
			*Result.JsonPath,
			Result.bJsonWritten ? 1 : 0);

		return Result;
	}

	void LogAttemptResult(
		const FString& AttemptUuid,
		const bool bPassed,
		const double StandingSeconds,
		const EPhysAnimTerminalReason TerminalReason)
	{
		if (bPassed)
		{
			UE_LOG(
				LogPhysAnimBridge,
				Warning,
				TEXT("PhysAnimProof: AttemptResult uuid=%s verdict=PASS duration=%.3f terminal_reason=%s"),
				*AttemptUuid,
				StandingSeconds,
				*ToTerminalReasonString(TerminalReason));
		}
		else
		{
			UE_LOG(
				LogPhysAnimBridge,
				Warning,
				TEXT("PhysAnimProof: AttemptResult uuid=%s verdict=FAIL duration=%.3f terminal_reason=%s"),
				*AttemptUuid,
				StandingSeconds,
				*ToTerminalReasonString(TerminalReason));
		}
	}
}
