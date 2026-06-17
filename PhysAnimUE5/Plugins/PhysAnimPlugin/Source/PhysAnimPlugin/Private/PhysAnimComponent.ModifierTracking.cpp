#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"
#include "PhysAnimLogger.h"

void UPhysAnimComponent::ResetPendingBodyModifiersToCachedTargets()
{
	UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	const bool bPhase1Prepare = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Prepare;
	const bool bPhase1LateValidate = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_LateValidate;
	const bool bPhase1RootOn = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn;
	const bool bPhase1Settle = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle;
	const FName RootModifierName = PhysAnimBridge::MakeBodyModifierName(PhysAnimBridge::GetRootBoneName());
	if (!PhysicsControl ||
		PendingBodyModifierCachedResetNames.IsEmpty() ||
		((bPhase1Prepare || bPhase1LateValidate || bPhase1RootOn || bPhase1Settle) && BalanceReadyTransition.ShouldSuppressResets()))
	{
		return;
	}

	TArray<FName> ModifierNamesToReset;
	ModifierNamesToReset.Reserve(PendingBodyModifierCachedResetNames.Num());
	for (const FName ModifierName : PendingBodyModifierCachedResetNames)
	{
		if ((bPhase1Prepare || bPhase1LateValidate || bPhase1RootOn || bPhase1Settle) && BalanceReadyTransition.ShouldSuppressResets())
		{
			if (ModifierName != RootModifierName)
			{
				continue;
			}
		}

		if (bPhase1Prepare || bPhase1LateValidate)
		{
			const FName BoneName = PhysAnimBridge::GetBoneNameFromBodyModifierName(ModifierName);
			
			// Guard to prevent accepted distal-kinematic bones from being reset to simulated (Task: Fix stale modifier record)
			if (BalanceReadyTransition.IsDistalKinematicAccepted() &&
				(BoneName == TEXT("calf_l") || BoneName == TEXT("foot_l") || BoneName == TEXT("ball_l") ||
				 BoneName == TEXT("calf_r") || BoneName == TEXT("foot_r") || BoneName == TEXT("ball_r")))
			{
				continue;
			}

			const int32 GroupIndex = ResolveBringUpGroupIndex(BoneName);
			if (GroupIndex == 0 || GroupIndex == 1)
			{
				// Do not process deferred resets for critical Phase 1 sim bodies in transition.
				continue;
			}
		}

		if (PhysicsControl->GetBodyModifierExists(ModifierName))
		{
			ModifierNamesToReset.Add(ModifierName);
			const FName BoneName = PhysAnimBridge::GetBoneNameFromBodyModifierName(ModifierName);
			TrackDistalModifierWrite(BoneName, EPhysicsMovementType::Simulated, false, TEXT("ResetPendingBodyModifiersToCachedTargets_PreReset"));
		}
	}

	if (ModifierNamesToReset.IsEmpty())
	{
		PendingBodyModifierCachedResetNames.Reset();
		return;
	}

	if (ModifierNamesToReset.Contains(RootModifierName) && RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn)
	{
	}

	PhysicsControl->ResetBodyModifiersToCachedBoneTransforms(
		ModifierNamesToReset,
		EResetToCachedTargetBehavior::ResetDuringUpdateControls,
		true,
		false);

	for (const FName ModifierName : ModifierNamesToReset)
	{
		const FName BoneName = PhysAnimBridge::GetBoneNameFromBodyModifierName(ModifierName);
		if (const FPhysicsBodyModifierRecord* Record = FPhysAnimPhysicsControlAccessor::GetModifierRecord(PhysicsControl, BoneName))
		{
			TrackDistalModifierWrite(BoneName, Record->BodyModifier.ModifierData.MovementType, false, TEXT("ResetPendingBodyModifiersToCachedTargets_PostReset"));
		}
	}

	for (const FName ModifierName : ModifierNamesToReset)
	{
		const FName BoneName = PhysAnimBridge::GetBoneNameFromBodyModifierName(ModifierName);
		if (const EPhysicsMovementType* PrevOwnership = PreviousDistalBoneIntendedOwnership.Find(BoneName))
		{
			TrackDistalBoneOwnershipChange(BoneName, *PrevOwnership, TEXT("ResetPendingBodyModifiersToCachedTargets"));
		}
	}

	for (const FName ModifierName : ModifierNamesToReset)
	{
		if (ModifierName == RootModifierName)
		{
			bPelvisResetAppliedThisTick = true;
			break;
		}
	}

	TArray<FString> BoneNamesToReset;
	for (const FName ModifierName : ModifierNamesToReset)
	{
		BoneNamesToReset.Add(PhysAnimBridge::GetBoneNameFromBodyModifierName(ModifierName).ToString());
	}

	PHYSANIM_LOG(
		LogPhysAnimBridge,
		Log,
		TEXT("[PhysAnim] Scheduled deferred cached-target reset for %d promoted body modifiers: [%s]"),
		ModifierNamesToReset.Num(),
		*FString::Join(BoneNamesToReset, TEXT(", ")));
	PendingBodyModifierCachedResetNames.Reset();
}


void UPhysAnimComponent::ConsumeUpperBodyPendingResets()
{
	if (PendingBodyModifierCachedResetNames.IsEmpty())
	{
		return;
	}

	TArray<FName> UpperBodyResetsToClear;
	for (int32 i = PendingBodyModifierCachedResetNames.Num() - 1; i >= 0; --i)
	{
		FName ModifierName = PendingBodyModifierCachedResetNames[i];
		FName BoneName = PhysAnimBridge::GetBoneNameFromBodyModifierName(ModifierName);
		if (BalanceTransitionSets::IsUpperBody(BoneName))
		{
			UpperBodyResetsToClear.Add(BoneName);
			PendingBodyModifierCachedResetNames.RemoveAt(i);
		}
	}

	if (UpperBodyResetsToClear.Num() > 0)
	{
		TArray<FString> BoneNames;
		for (FName BoneName : UpperBodyResetsToClear)
		{
			BoneNames.Add(BoneName.ToString());
		}
		PHYSANIM_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_UPPER_BODY_PENDING_RESETS_CLEARED bones=[%s]"), *FString::Join(BoneNames, TEXT(", ")));
	}
}


void UPhysAnimComponent::ApplyControlTargets(
	float PolicyStepDeltaTime,
	const FPhysAnimStabilizationSettings& EffectiveSettings,
	bool bApplyNewPolicyStepThisTick,
	FString& OutError)
{
	// Evaluate cached targets reset before updating controls so we don't clobber the frame's true targets.
	ResetPendingBodyModifiersToCachedTargets();

	// Consecutive active frame tracking is now handled at the end of ApplyControlTargets to ensure it only increments on successful writes.

	TRACE_CPUPROFILER_EVENT_SCOPE(UPhysAnimComponent_ApplyControlTargets);

	UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	if (!PhysicsControl)
	{
		OutError = TEXT("Physics Control component was not resolved.");
		return;
	}

	USkeletalMeshComponent* const Mesh = GetMeshComponent();
	const FBodyInstance* const PelvisBodyScope = Mesh ? Mesh->GetBodyInstance(PhysAnimBridge::GetRootBoneName()) : nullptr;

	const double CurrentTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : BridgeStartTimeSeconds;
	const bool bPhase1Prepare = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Prepare;
	const bool bPhase1LateValidate = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_LateValidate;
	const bool bPhase1RootOn = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn;
	const bool bPhase1Settle = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle;
	const bool bApplyPhase1HoldPoseThisFrame = bPhase1Prepare || bPhase1LateValidate;
	const bool bPolicyInfluenceActive =
		PolicyInfluenceRampStartTimeSeconds >= 0.0 &&
		(
			RuntimeState == EPhysAnimRuntimeState::BridgeActive ||
			IsBalanceActiveState(RuntimeState) ||
			bPhase1RootOn ||
			bPhase1Settle);
	FPhysAnimControlTargetDiagnostics ControlTargetDiagnostics;
	ControlTargetDiagnostics.bPolicyInfluenceActive = bPolicyInfluenceActive;
	ControlTargetDiagnostics.bFirstPolicyEnabledFrame = bPolicyInfluenceActive && !bPolicyTargetsAppliedLastFrame;

	if (ControlTargetDiagnostics.bFirstPolicyEnabledFrame && PolicyInfluenceRampStartTimeSeconds >= 0.0)
	{
		// Preserve the original ramp start so readiness accounting keeps the full settled window,
		// but remember that the first policy-enabled frame was explicitly re-based for readiness.
		bPolicyInfluenceRampReanchoredOnFirstPolicyEnabledFrame = true;
	}

	const float PolicyInfluenceAlpha = CalculateCurrentPolicyInfluenceAlpha(EffectiveSettings);

	if (EffectiveSettings.bForceZeroActions)
	{
		PreviousControlTargetRotations.Reset();
		PolicyBlendStartControlTargetRotations.Reset();
		LastControlTargetDiagnostics = {};
		bPolicyTargetsAppliedLastFrame = false;
		return;
	}

	if (!bPolicyInfluenceActive && !bApplyPhase1HoldPoseThisFrame)
	{
		PolicyBlendStartControlTargetRotations.Reset();
		LastControlTargetDiagnostics = ControlTargetDiagnostics;
		bPolicyTargetsAppliedLastFrame = false;
		return;
	}

	if (!bApplyNewPolicyStepThisTick && !bApplyPhase1HoldPoseThisFrame)
	{
		ControlTargetDiagnostics.bPolicyInfluenceActive = bPolicyInfluenceActive;
		ControlTargetDiagnostics.bFirstPolicyEnabledFrame = false;
		LastControlTargetDiagnostics = ControlTargetDiagnostics;
		return;
	}

	TMap<FName, FQuat> ControlRotations;
	if (!PhysAnimBridge::ConvertModelActionsToControlRotations(ConditionedActionBuffer, ControlRotations, OutError))
	{
		return;
	}

	const float MaxAngularStepDegrees =
		FMath::Max(0.0f, EffectiveSettings.MaxAngularStepDegreesPerSecond) * PolicyStepDeltaTime;
	const bool bUseSkeletalAnimationTargetRepresentation =
		ShouldUseSkeletalAnimationTargetRepresentation(
			EffectiveSettings.bUseSkeletalAnimationTargets,
			bPolicyInfluenceActive);
	UPhysicsAsset* const PhysicsAsset = Mesh ? Mesh->GetPhysicsAsset() : nullptr;


	const bool bAllowRootSim = ShouldAllowBalanceSimulation(EffectiveSettings);
	const bool bRootSimFlipFrame = bAllowRootSim && !bLastAppliedPresentationRootSimulationEnabled;
	const bool bHipQuarantineActiveThisFrame = HipQuarantineTicksRemaining > 0;
	float ThighLDeltaPre = 0.0f;
	float ThighRDeltaPre = 0.0f;
	const float OwnerPlanarSpeedCmPerSec = [this]() -> float
	{
		const AActor* const OwnerActor = GetOwner();
		if (!OwnerActor)
		{
			return 0.0f;
		}

		const FVector OwnerVelocity = OwnerActor->GetVelocity();
		return FVector(OwnerVelocity.X, OwnerVelocity.Y, 0.0f).Size();
	}();

	const bool bIsPhase1PolicyLoopSuppressed = (bPhase1Prepare || bPhase1LateValidate) && !bEnableLiveRuntimeEvidenceProof;
	bool bSuppressPostShellPolicy = false;

	const float TargetWriteDeltaTime =
		ResolvePolicyTargetWriteDeltaTime(
			bUseSkeletalAnimationTargetRepresentation,
			ControlTargetDiagnostics.bFirstPolicyEnabledFrame,
			PolicyStepDeltaTime);
	const bool bRebaseControlTargetHistoryThisFrame =
		ControlTargetDiagnostics.bFirstPolicyEnabledFrame ||
		ShouldResetAllControlOffsetsForPolicyTargetRepresentationSwitch(
			bUseSkeletalAnimationTargetRepresentation,
			ControlTargetDiagnostics.bFirstPolicyEnabledFrame);

	if (ShouldResetAllControlOffsetsForPolicyTargetRepresentationSwitch(
		bUseSkeletalAnimationTargetRepresentation,
		ControlTargetDiagnostics.bFirstPolicyEnabledFrame) && !bApplyPhase1HoldPoseThisFrame)
	{
		PhysicsControl->SetControlTargetOrientationsInSet(TEXT("All"), FRotator::ZeroRotator, 0.0f, true, false);
	}

	if (bApplyPhase1HoldPoseThisFrame)
	{
		const bool bIsPhase1EntryState = bPhase1Prepare || bPhase1LateValidate;
		auto IsPhase1KinematicAllowlistedBone = [&EffectiveSettings](const FName BoneName)
		{
			bool bIsAllowlisted = BoneName == "pelvis" ||
				BoneName == "neck_01" || BoneName == "head" ||
				BoneName == "clavicle_l" || BoneName == "upperarm_l" || BoneName == "lowerarm_l" || BoneName == "hand_l" ||
				BoneName == "clavicle_r" || BoneName == "upperarm_r" || BoneName == "lowerarm_r" || BoneName == "hand_r";
			
			if (EffectiveSettings.bPhase1DistalKinematicExperiment &&
				(BoneName == "calf_l" || BoneName == "calf_r" ||
				 BoneName == "foot_l" || BoneName == "foot_r" ||
				 BoneName == "ball_l" || BoneName == "ball_r"))
			{
				bIsAllowlisted = true;
			}
			return bIsAllowlisted;
		};

		for (const TPair<FName, FQuat>& HoldPair : BalanceReadyTransition.GetEntryHoldRotations())
		{
			const FName BoneName = HoldPair.Key;

			// Phase 1 Ownership: Enforce strict positive allowlist for kinematic bones.
			// Only intended hold-eligible kinematic bones receive held targets during Prepare/LateValidate.
			if (bIsPhase1EntryState && !IsPhase1KinematicAllowlistedBone(BoneName))
			{
				continue;
			}

			const FName ControlName = PhysAnimBridge::MakeControlName(BoneName);
			if (!PhysicsControl->GetControlExists(ControlName))
			{
				continue;
			}

			const FQuat HoldRot = HoldPair.Value;
			const FQuat* PreviousRotation = PreviousControlTargetRotations.Find(ControlName);
			if (PreviousRotation)
			{
				const float Delta = CalculateControlTargetDeltaDegrees(*PreviousRotation, HoldRot);
				if (Delta > ControlTargetDiagnostics.MaxTargetDeltaDegrees)
				{
					ControlTargetDiagnostics.MaxTargetDeltaDegrees = Delta;
					ControlTargetDiagnostics.MaxTargetDeltaBoneName = BoneName;
				}
				ControlTargetDiagnostics.MeanTargetDeltaDegrees += Delta;
			}

			PhysicsControl->SetControlTargetOrientation(
				ControlName,
				HoldRot.Rotator(),
				0.0f,
				true,
				false,
				true,
				false);

			PreviousControlTargetRotations.Add(ControlName, HoldRot);
			++ControlTargetDiagnostics.NumHeldTargetsWritten;
		}
	}

	if (bApplyNewPolicyStepThisTick)
	{
		// Phase 1 Transition Rule: Normally no normal policy writes during Prepare/LateValidate.
		// HOWEVER, if bEnableLiveRuntimeEvidenceProof is active, we ALLOW these writes to provide 
		// authentic telemetry of the policy's intent during the validation hold.


		bool bPolicyTargetsAppliedThisTick = true;
		const int32 RootRawSimPost = (PelvisBodyScope && PelvisBodyScope->IsInstanceSimulatingPhysics()) ? 1 : 0;
		const int32 Phase3KineticTick = BalanceReadyTransition.GetPhase3KineticGateReleaseTickCount();
		const bool bIsPhase3SuppressionWindow = (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle && Phase3KineticTick > 0 && Phase3KineticTick <= 20);

		bSuppressPostShellPolicy =
			(RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn &&
			BalanceEntryRootOnFrameCount >= 4 &&
			RootRawSimPost == 1 &&
			LastRuntimeInstabilityDiagnostics.NumSimulatingBodies >= 6 &&
			BalanceReadyTransition.GetDiagnostics().bShellMaterialGuardSuppressed) ||
			bIsPhase3SuppressionWindow;

		float Phase3HandoverAlpha = 1.0f;
		if (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle && Phase3KineticTick > 20 && Phase3KineticTick <= 40)
		{
			Phase3HandoverAlpha = FMath::Clamp((Phase3KineticTick - 20) / 20.0f, 0.0f, 1.0f);
		}

		const bool bInSettlementWindow = Phase3KineticTick > 0 && Phase3KineticTick <= 20;
		const bool bIsPhase3HandoverBlend = (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle && Phase3KineticTick > 20 && Phase3KineticTick <= 40);

		// §EPIC-13.2 - Kinetic Gate Warm-Start Rebase
		// If the gate just released, OR we are in the settlement window, rebase the targets to 
		// the current physical pose to prevent "snap-back" energy spikes.
		// Continuous rebase during settlement provides "Active Damping" (Strength*0 + Damping*Vel).
		if ((bKineticGateReleasedThisFrame || bInSettlementWindow) && Mesh)
		{
			for (const FName& BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
			{
				const FName ControlName = PhysAnimBridge::MakeControlName(BoneName);
				
				const FQuat ChildWorldRot = Mesh->GetBoneQuaternion(BoneName, EBoneSpaces::WorldSpace);
				const FName ParentBoneName = Mesh->GetParentBone(BoneName);
				const FQuat ParentWorldRot = (ParentBoneName != NAME_None) ? 
					Mesh->GetBoneQuaternion(ParentBoneName, EBoneSpaces::WorldSpace) : 
					FQuat::Identity;

				const FQuat LocalRot = (ParentWorldRot.Inverse() * ChildWorldRot).GetNormalized();
				PreviousControlTargetRotations.Add(ControlName, LocalRot);

				// During settlement, we keep updating the blend start so it carries the final settled pose 
				// into the handover phase at Tick 21.
				PolicyBlendStartControlTargetRotations.Add(ControlName, LocalRot);
			}
		}

		if (Phase3HandoverAlpha <= 0.0f)
		{
			bSuppressPostShellPolicy = true;
		}

		if (bSuppressPostShellPolicy && bApplyNewPolicyStepThisTick)
		{
			const int32 SuppressionTick = (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle) ? Phase3KineticTick : static_cast<int32>(BalanceEntryRootOnFrameCount);
			const TCHAR* SuppressionPhaseStr = (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle) ? TEXT("PHASE3") : TEXT("PHASE2");

			if (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn)
			{
				PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimBalance] %s_POST_SHELL_POLICY_SUPPRESSED frame=%d tick=%d normalWrites=%d heldWrites=%d rootRawSim=%d simCount=%d policyInfluenceAlpha=%.2f owner=%d actor=%s component=%s"),
					SuppressionPhaseStr,
					static_cast<int32>(GFrameNumber),
					SuppressionTick,
					0,
					ControlTargetDiagnostics.NumHeldTargetsWritten,
					RootRawSimPost,
					LastRuntimeInstabilityDiagnostics.NumSimulatingBodies,
					PolicyInfluenceAlpha,
					static_cast<int32>(FPhysAnimBalanceReadyTransition::ClassifyConditionOwner(BalanceReadyTransition.GetFailureReason())),
					*GetOwner()->GetName(),
					*GetName());

				ControlTargetDiagnostics.NumNormalPolicyTargetsWritten = 0;
				bPolicyTargetsAppliedThisTick = false;
				return;
			}
		}

		bool bNormalWritesBlockedByRootOn = false;
		if (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn)
		{
			ControlTargetDiagnostics.NumNormalPolicyTargetsWritten = 0;
			bPolicyTargetsAppliedThisTick = false;
			bNormalWritesBlockedByRootOn = !bIsPhase1PolicyLoopSuppressed && !bSuppressPostShellPolicy;
		}
		const bool bIsPhase3FlowActive = (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle);
		if (!bIsPhase1PolicyLoopSuppressed && (!bSuppressPostShellPolicy || bIsPhase3FlowActive) && !bNormalWritesBlockedByRootOn)
		{
			float EffectiveHandoverAlpha = Phase3HandoverAlpha;
			if (bIsPhase3SuppressionWindow) EffectiveHandoverAlpha = 0.0f;
			bPolicyTargetsAppliedLastFrame = true;

			for (const TPair<FName, FQuat>& Pair : ControlRotations)
			{
				if (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn && 
					BalanceReadyTransition.GetPhase2GuardTickCount() == 1)
				{
					static const TSet<FName> ProximalSet = { PhysAnimBridge::GetRootBoneName(), TEXT("thigh_l"), TEXT("thigh_r"), TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03") };
					if (ProximalSet.Contains(Pair.Key))
					{
						bool bPelvisSimNow = false;
						int32 TotalSimNow = 0;
						if (Mesh)
						{
							if (FBodyInstance* const RootBI = Mesh->GetBodyInstance(PhysAnimBridge::GetRootBoneName()))
							{
								bPelvisSimNow = RootBI->IsInstanceSimulatingPhysics();
							}
							for (const FName& SimBone : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
							{
								if (FBodyInstance* const BI = Mesh->GetBodyInstance(SimBone))
								{
									if (BI->IsInstanceSimulatingPhysics())
									{
										TotalSimNow++;
									}
								}
							}
						}

						if (bPelvisSimNow && TotalSimNow >= 6)
						{
							PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnim] PHASE2_ENTRY_PROXIMAL_POLICY_SUPPRESSED bone=%s frame=%llu actor=%s component=%s"), *Pair.Key.ToString(), GFrameCounter, *GetOwner()->GetName(), *GetName());
							continue;
						}
					}
				}

				if (!ShouldApplyPolicyTargetToBone(Pair.Key, bPolicyInfluenceActive))
				{
					continue;
				}

				const FName ControlName = PhysAnimBridge::MakeControlName(Pair.Key);
				if (!PhysicsControl->GetControlExists(ControlName))
				{
					OutError = FString::Printf(TEXT("Missing required control '%s' during target write."), *ControlName.ToString());
					return;
				}

				if (bRebaseControlTargetHistoryThisFrame)
				{
					if (!PreviousControlTargetRotations.Contains(ControlName))
					{
						PreviousControlTargetRotations.Add(ControlName, Pair.Value);
					}
					if (!PolicyBlendStartControlTargetRotations.Contains(ControlName))
					{
						PolicyBlendStartControlTargetRotations.Add(ControlName, Pair.Value);
					}
				}

				// §EPIC-13.2 - Phase 3 Soft Handover
				FQuat BasePolicyRotation = Pair.Value;
				if (bIsPhase3HandoverBlend && EffectiveHandoverAlpha < 1.0f - KINDA_SMALL_NUMBER)
				{
					// Blend from the physically settled pose (captured at Tick 20) to the current policy target
					const FQuat* const StartRotPtr = PolicyBlendStartControlTargetRotations.Find(ControlName);
					if (StartRotPtr)
					{
						BasePolicyRotation = FQuat::Slerp(*StartRotPtr, Pair.Value, EffectiveHandoverAlpha).GetNormalized();
					}
				}
				else if (bInSettlementWindow)
				{
					// During settlement, strictly follow the rebased physical pose
					const FQuat* const SettledRotPtr = PreviousControlTargetRotations.Find(ControlName);
					if (SettledRotPtr)
					{
						BasePolicyRotation = *SettledRotPtr;
					}
				}

				const FQuat* const PreviousRotation = PreviousControlTargetRotations.Find(ControlName);
				const FQuat* const BlendStartRotation = PolicyBlendStartControlTargetRotations.Find(ControlName);

				const bool bApplyTrainingAlignedLowerLimbTargetRangePolicy =
				ShouldApplyTrainingAlignedLowerLimbTargetRangePolicy(
					EffectiveSettings.bApplyTrainingAlignedLowerLimbTargetRangePolicy,
					EffectiveSettings.TrainingAlignedLowerLimbTargetRangePolicyBlend);
			const float LowerLimbTargetRangeScale = bApplyTrainingAlignedLowerLimbTargetRangePolicy
				? ResolveTrainingAlignedLowerLimbTargetRangeScaleForBone(
					Pair.Key,
					EffectiveSettings.TrainingAlignedLowerLimbTargetRangePolicyBlend)
				: 1.0f;
			const bool bApplyTrainingAlignedDistalLocomotionTargetPolicy =
				ShouldApplyTrainingAlignedDistalLocomotionTargetPolicy(
					EffectiveSettings.bApplyTrainingAlignedDistalLocomotionTargetPolicy,
					EffectiveSettings.TrainingAlignedDistalLocomotionTargetPolicyBlend,
					OwnerPlanarSpeedCmPerSec,
					EffectiveSettings.DistalLocomotionTargetPolicyActivationSpeedCmPerSec);
			const float DistalLocomotionTargetScale = bApplyTrainingAlignedDistalLocomotionTargetPolicy
				? ResolveTrainingAlignedDistalLocomotionTargetScaleForBone(
					Pair.Key,
					EffectiveSettings.TrainingAlignedDistalLocomotionTargetPolicyBlend)
				: 1.0f;
			const float RawPolicyOffsetDegrees = BlendStartRotation
				? CalculateControlTargetDeltaDegrees(*BlendStartRotation, BasePolicyRotation)
				: 0.0f;
			const FQuat RangeAlignedPolicyRotation = BlendStartRotation
				? BlendPolicyTargetRotation(*BlendStartRotation, BasePolicyRotation, LowerLimbTargetRangeScale)
				: BasePolicyRotation;

			const FQuat DistalLocomotionAlignedPolicyRotation = BlendStartRotation
				? BlendPolicyTargetRotation(*BlendStartRotation, RangeAlignedPolicyRotation, DistalLocomotionTargetScale)
				: RangeAlignedPolicyRotation;
			const float RangeAlignedPolicyOffsetDegrees = BlendStartRotation
				? CalculateControlTargetDeltaDegrees(*BlendStartRotation, DistalLocomotionAlignedPolicyRotation)
				: 0.0f;
			const FQuat BlendedPolicyRotation = BlendStartRotation
				? BlendPolicyTargetRotation(*BlendStartRotation, DistalLocomotionAlignedPolicyRotation, PolicyInfluenceAlpha)
				: DistalLocomotionAlignedPolicyRotation;
			const float TargetDeltaDegrees = PreviousRotation
				? CalculateControlTargetDeltaDegrees(*PreviousRotation, BlendedPolicyRotation)
				: 0.0f;
			FQuat LimitedRotation = PreviousRotation
				? LimitTargetRotationStep(*PreviousRotation, BlendedPolicyRotation, MaxAngularStepDegrees)
				: BlendedPolicyRotation;

			if (bHipQuarantineActiveThisFrame && (Pair.Key == "thigh_l" || Pair.Key == "thigh_r") && Mesh)
			{
				FBodyInstance* BI = Mesh->GetBodyInstance(Pair.Key);
				if (BI)
				{
					const FQuat CurPhysRot = BI->GetUnrealWorldTransform().GetRotation();
					const FQuat CurSkelRot = Mesh->GetBoneQuaternion(Pair.Key, EBoneSpaces::WorldSpace);
					const FQuat DeltaPre = CurSkelRot.Inverse() * CurPhysRot;
					const float Deg = FMath::RadiansToDegrees(DeltaPre.GetAngle());

					if (Pair.Key == "thigh_l")
					{
						ThighLDeltaPre = Deg;
						LastHipQuarantineLeftPreDeltaDegrees = Deg;
					}
					else
					{
						ThighRDeltaPre = Deg;
						LastHipQuarantineRightPreDeltaDegrees = Deg;
					}

					LimitedRotation = DeltaPre;
				}
			}

			++ControlTargetDiagnostics.NumNormalPolicyTargetsWritten;
			ControlTargetDiagnostics.MeanTargetDeltaDegrees += TargetDeltaDegrees;
			ControlTargetDiagnostics.MeanRawPolicyOffsetDegrees += RawPolicyOffsetDegrees;
			if (TargetDeltaDegrees > ControlTargetDiagnostics.MaxTargetDeltaDegrees)
			{
					ControlTargetDiagnostics.MaxTargetDeltaDegrees = TargetDeltaDegrees;
					ControlTargetDiagnostics.MaxTargetDeltaBoneName = Pair.Key;
			}
			if (RawPolicyOffsetDegrees > ControlTargetDiagnostics.MaxRawPolicyOffsetDegrees)
			{
					ControlTargetDiagnostics.MaxRawPolicyOffsetDegrees = RawPolicyOffsetDegrees;
					ControlTargetDiagnostics.MaxRawPolicyOffsetBoneName = Pair.Key;
			}
			const float LowerLimbLimitProxyDegrees =
				PhysAnimComponentInternal::ResolveLowerLimbConstraintLimitProxyDegrees(PhysicsAsset, Pair.Key);
			if (LowerLimbLimitProxyDegrees > UE_SMALL_NUMBER)
			{
				const float LowerLimbLimitOccupancy = RangeAlignedPolicyOffsetDegrees / LowerLimbLimitProxyDegrees;
				++ControlTargetDiagnostics.NumLowerLimbTargetsConsidered;
				ControlTargetDiagnostics.MeanLowerLimbLimitOccupancy += LowerLimbLimitOccupancy;
				if (LowerLimbLimitOccupancy > ControlTargetDiagnostics.MaxLowerLimbLimitOccupancy)
				{
					ControlTargetDiagnostics.MaxLowerLimbLimitOccupancy = LowerLimbLimitOccupancy;
					ControlTargetDiagnostics.MaxLowerLimbLimitOccupancyBoneName = Pair.Key;
					ControlTargetDiagnostics.MaxLowerLimbLimitProxyDegrees = LowerLimbLimitProxyDegrees;
				}
			}

			if (bRootSimFlipFrame)
			{
				continue;
			}

			PreviousControlTargetRotations.Add(ControlName, LimitedRotation);
			const float TargetAngularVelocityDeltaTime =
				ResolvePolicyTargetAngularVelocityDeltaTime(
					Pair.Key,
					bUseSkeletalAnimationTargetRepresentation,
					ControlTargetDiagnostics.bFirstPolicyEnabledFrame,
					bDistalLocomotionCompositionModeActive,
					TargetWriteDeltaTime);

			PhysicsControl->SetControlTargetOrientation(
				ControlName,
				LimitedRotation.Rotator(),
				TargetAngularVelocityDeltaTime,
				true,
				false,
				true,
				false);
			}
		}
	}

	if (ControlTargetDiagnostics.NumNormalPolicyTargetsWritten > 0)
	{
		ControlTargetDiagnostics.MeanTargetDeltaDegrees /=
			static_cast<float>(ControlTargetDiagnostics.NumNormalPolicyTargetsWritten);
		ControlTargetDiagnostics.MeanRawPolicyOffsetDegrees /=
			static_cast<float>(ControlTargetDiagnostics.NumNormalPolicyTargetsWritten);
	}
	ControlTargetDiagnostics.NumTotalTargetsWritten =
		ControlTargetDiagnostics.NumNormalPolicyTargetsWritten + ControlTargetDiagnostics.NumHeldTargetsWritten;
	if (ControlTargetDiagnostics.NumLowerLimbTargetsConsidered > 0)
	{
		ControlTargetDiagnostics.MeanLowerLimbLimitOccupancy /=
			static_cast<float>(ControlTargetDiagnostics.NumLowerLimbTargetsConsidered);
	}

	LastControlTargetDiagnostics = ControlTargetDiagnostics;
	bPolicyTargetsAppliedLastFrame = bPolicyInfluenceActive && !bRootSimFlipFrame;
	if (RuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing)
	{
		++ActivatedStandingStabilityMetrics.ControlTargetSampleCount;
		ActivatedStandingStabilityMetrics.ControlTargetNormalWrites += ControlTargetDiagnostics.NumNormalPolicyTargetsWritten;
		ActivatedStandingStabilityMetrics.ControlTargetTotalWrites += ControlTargetDiagnostics.NumTotalTargetsWritten;
		ActivatedStandingStabilityMetrics.ControlTargetMaxDeltaDeg = FMath::Max(
			ActivatedStandingStabilityMetrics.ControlTargetMaxDeltaDeg,
			static_cast<double>(ControlTargetDiagnostics.MaxTargetDeltaDegrees));
		ActivatedStandingStabilityMetrics.ControlTargetMeanDeltaDegMax = FMath::Max(
			ActivatedStandingStabilityMetrics.ControlTargetMeanDeltaDegMax,
			static_cast<double>(ControlTargetDiagnostics.MeanTargetDeltaDegrees));
		ActivatedStandingStabilityMetrics.ControlTargetMaxRawPolicyOffsetDeg = FMath::Max(
			ActivatedStandingStabilityMetrics.ControlTargetMaxRawPolicyOffsetDeg,
			static_cast<double>(ControlTargetDiagnostics.MaxRawPolicyOffsetDegrees));
		ActivatedStandingStabilityMetrics.ControlTargetMeanRawPolicyOffsetDegMax = FMath::Max(
			ActivatedStandingStabilityMetrics.ControlTargetMeanRawPolicyOffsetDegMax,
			static_cast<double>(ControlTargetDiagnostics.MeanRawPolicyOffsetDegrees));
	}

	const bool bInferenceSucceeded = bApplyNewPolicyStepThisTick && OutError.IsEmpty();
	const bool bTargetsWritten = ControlTargetDiagnostics.NumNormalPolicyTargetsWritten > 0;

	if (bInferenceSucceeded && bTargetsWritten)
	{
		LastPolicyControlUpdateTimeSeconds = GetPhysAnimClockTime();
		++Stage2AConsecutivePolicyActiveFrames;
	}
	else if (!IsStage2APolicyOutputActive())
	{
		// Only reset if the watchdog has actually timed out, or if we explicitly failed inference
		if (!bInferenceSucceeded)
		{
			Stage2AConsecutivePolicyActiveFrames = 0;
		}
	}

	if (bRootSimFlipFrame)
	{
		PHYSANIM_LOG_RATE_LIMITED(
			LogPhysAnimBridge,
			Log,
			1.0f,
			TEXT("[PhysAnimBalance] POLICY_TARGETS_SUPPRESSED_ON_SIM_FLIP: normal=%d hold=%d total=%d"),
			ControlTargetDiagnostics.NumNormalPolicyTargetsWritten,
			ControlTargetDiagnostics.NumHeldTargetsWritten,
			ControlTargetDiagnostics.NumTotalTargetsWritten);
	}
	if (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn)
	{
		const FBodyInstance* const PelvisBodyProbe = GetMeshComponent() ? GetMeshComponent()->GetBodyInstance(PhysAnimBridge::GetRootBoneName()) : nullptr;
		const int32 RootRawSim = (PelvisBodyProbe && PelvisBodyProbe->IsInstanceSimulatingPhysics()) ? 1 : 0;
		const bool bNormalWritesBlocked = !bIsPhase1PolicyLoopSuppressed && !bSuppressPostShellPolicy;
		const bool bHasPolicyWrites = (ControlTargetDiagnostics.NumNormalPolicyTargetsWritten > 0);

		if (bApplyNewPolicyStepThisTick && bNormalWritesBlocked && bHasPolicyWrites)
		{
			PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Log, 1.0f, TEXT("[PhysAnimBalance] PHASE2_ROOTON_POLICY_SUPPRESSED frame=%d tick=%d normalWritesBlocked=%d heldWrites=%d rootRawSim=%d simCount=%d policyInfluenceAlpha=%.2f owner=%d actor=%s component=%s"),
				static_cast<int32>(GFrameNumber),
				static_cast<int32>(BalanceEntryRootOnFrameCount),
				1,
				ControlTargetDiagnostics.NumHeldTargetsWritten,
				RootRawSim,
				LastRuntimeInstabilityDiagnostics.NumSimulatingBodies,
				PolicyInfluenceAlpha,
				static_cast<int32>(FPhysAnimBalanceReadyTransition::ClassifyConditionOwner(BalanceReadyTransition.GetFailureReason())),
				*GetOwner()->GetName(),
				*GetName());
		}
	}

	if (ControlTargetDiagnostics.bFirstPolicyEnabledFrame && RuntimeState != EPhysAnimRuntimeState::BalanceEntry_RootOn)
	{
		PHYSANIM_LOG_RATE_LIMITED(
			LogPhysAnimBridge,
			Log,
			1.0f,
			TEXT("[PhysAnim] First policy-enabled frame: normal=%d hold=%d total=%d maxTargetDelta=%s:%.1fdeg meanTargetDelta=%.1fdeg maxRawPolicyOffset=%s:%.1fdeg meanRawPolicyOffset=%.1fdeg lowerLimbLimitOccupancy=%s:%.2fx proxy=%.1fdeg mean=%.2fx"),
			ControlTargetDiagnostics.NumNormalPolicyTargetsWritten,
			ControlTargetDiagnostics.NumHeldTargetsWritten,
			ControlTargetDiagnostics.NumTotalTargetsWritten,
			*ControlTargetDiagnostics.MaxTargetDeltaBoneName.ToString(),
			ControlTargetDiagnostics.MaxTargetDeltaDegrees,
			ControlTargetDiagnostics.MeanTargetDeltaDegrees,
			*ControlTargetDiagnostics.MaxRawPolicyOffsetBoneName.ToString(),
			ControlTargetDiagnostics.MaxRawPolicyOffsetDegrees,
			ControlTargetDiagnostics.MeanRawPolicyOffsetDegrees,
			*ControlTargetDiagnostics.MaxLowerLimbLimitOccupancyBoneName.ToString(),
			ControlTargetDiagnostics.MaxLowerLimbLimitOccupancy,
			ControlTargetDiagnostics.MaxLowerLimbLimitProxyDegrees,
			ControlTargetDiagnostics.MeanLowerLimbLimitOccupancy);
	}

	if (bRootSimFlipFrame)
	{
		PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Log, 1.0f, TEXT("[PhysAnimBalance] THIGH_RESEED_ON_SIM_FLIP: leftDeltaPre=%.2f rightDeltaPre=%.2f"), ThighLDeltaPre, ThighRDeltaPre);
	}
}


FQuat UPhysAnimComponent::BuildCurrentPoseControlTargetOrientation(
	const FQuat& ParentWorldRotation,
	const FQuat& ChildWorldRotation)
{
	return (ParentWorldRotation.Inverse() * ChildWorldRotation).GetNormalized();
}


void UPhysAnimComponent::ResolveBodyModifierRuntimeMode(
	EPhysAnimRuntimeState RuntimeState,
	bool bForceZeroActions,
	bool bSimulationHandoffSettled,
	bool bBringUpGroupUnlocked,
	bool bIsRootBodyModifier,
	bool bAllowRootBodyModifierSimulation,
	EPhysicsMovementType& OutMovementType,
	float& OutPhysicsBlendWeight,
	bool& bOutUpdateKinematicFromSimulation)
{
	// Balance mode should honor the per-scenario root-simulation gate so the quiet-window and pre-impact settle phases are real.
	const bool bModeAllowsRootSimulation = bAllowRootBodyModifierSimulation;

	if (bForceZeroActions || !bSimulationHandoffSettled || !bBringUpGroupUnlocked || (bIsRootBodyModifier && !bModeAllowsRootSimulation))
	{
		OutMovementType = EPhysicsMovementType::Kinematic;
		OutPhysicsBlendWeight = 0.0f;
		bOutUpdateKinematicFromSimulation = false;
		return;
	}

	OutMovementType = EPhysicsMovementType::Simulated;
	OutPhysicsBlendWeight = 1.0f;
	bOutUpdateKinematicFromSimulation = false;
}

ECollisionEnabled::Type UPhysAnimComponent::ResolveBodyModifierCollisionType(
	EPhysAnimRuntimeState CurrentRuntimeState,
	bool bForceZeroActions,
	bool bSimulationHandoffSettled,
	bool bBringUpGroupUnlocked,
	bool bIsRootBodyModifier,
	bool bAllowRootBodyModifierSimulation)
{
	// Balance mode should honor the per-scenario root-simulation gate so collision/simulation stay aligned.
	const bool bModeAllowsRootSimulation = bAllowRootBodyModifierSimulation;

	if (bForceZeroActions || !bSimulationHandoffSettled || !bBringUpGroupUnlocked || (bIsRootBodyModifier && !bModeAllowsRootSimulation))
	{
		return ECollisionEnabled::NoCollision;
	}

	return ECollisionEnabled::QueryAndPhysics;
}


bool UPhysAnimComponent::ShouldUseAuthoritativePerBoneBodyModifierSync(
	EPhysAnimRuntimeState RuntimeState,
	bool bDistalKinematicAccepted)
{
	return RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle ||
		(bDistalKinematicAccepted &&
			(RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Prepare ||
			 RuntimeState == EPhysAnimRuntimeState::BalanceEntry_LateValidate ||
			 RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn ||
			 RuntimeState == EPhysAnimRuntimeState::BridgeActive));
}


bool UPhysAnimComponent::ShouldUpdateBodyOnAuthoritativePerBoneKinematicWrite(EPhysAnimRuntimeState RuntimeState)
{
	return RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Prepare ||
		RuntimeState == EPhysAnimRuntimeState::BalanceEntry_LateValidate ||
		RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn ||
		RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle;
}


bool UPhysAnimComponent::ShouldUpdateBodyOnPerBoneBodyModifierSync(EPhysAnimRuntimeState RuntimeState)
{
	return RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Prepare ||
		RuntimeState == EPhysAnimRuntimeState::BalanceEntry_LateValidate ||
		RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn ||
		RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle;
}


bool UPhysAnimComponent::ShouldPreserveRawSimulationForBridgeActiveStartupProof(
	EPhysAnimRuntimeState RuntimeState,
	bool bLiveProofEnabled,
	bool bProofComplete)
{
	return RuntimeState == EPhysAnimRuntimeState::BridgeActive &&
		bLiveProofEnabled &&
		!bProofComplete;
}


bool UPhysAnimComponent::ShouldUseRawSimComProxyForStartupProof(
	EPhysAnimRuntimeState RuntimeState,
	bool bLiveProofEnabled,
	bool bProofComplete)
{
	return RuntimeState == EPhysAnimRuntimeState::BridgeActive &&
		bLiveProofEnabled &&
		!bProofComplete;
}


bool UPhysAnimComponent::ShouldDeferStartupProxyTerminalForProof(
	EPhysAnimRuntimeState RuntimeState,
	bool bLiveProofEnabled,
	bool bProofComplete,
	bool bProxySupportHandoffArmed,
	EPhysAnimTerminalReason TerminalReason)
{
	const bool bStartupProofRuntime =
		RuntimeState == EPhysAnimRuntimeState::WaitingForPoseSearch ||
		RuntimeState == EPhysAnimRuntimeState::ReadyForActivation ||
		(RuntimeState == EPhysAnimRuntimeState::BridgeActive && bLiveProofEnabled && !bProofComplete);

	return bStartupProofRuntime &&
		!bProxySupportHandoffArmed &&
		TerminalReason == EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion;
}


bool UPhysAnimComponent::ShouldStartBridgeActivePolicyRampAfterStartupProof(
	EPhysAnimRuntimeState RuntimeState,
	bool bLiveProofComplete,
	bool bPolicyRampAlreadyStarted,
	bool bForceZeroActions,
	bool bCoreBringUpGroupUnlocked,
	bool bCoreBringUpGroupRampActive,
	bool bStartupBringUpFrozenByBalanceEntry)
{
	const bool bValidState = RuntimeState == EPhysAnimRuntimeState::BridgeActive ||
	                         RuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing;

	return bValidState &&
		bLiveProofComplete &&
		!bPolicyRampAlreadyStarted &&
		!bForceZeroActions &&
		bCoreBringUpGroupUnlocked &&
		bCoreBringUpGroupRampActive &&
		!bStartupBringUpFrozenByBalanceEntry;
}


bool UPhysAnimComponent::ShouldResetBodyModifierToCachedBoneTransform(
	FName BoneName,
	EPhysAnimRuntimeState InRuntimeState,
	bool bForceZeroActions,
	bool bBodyModifierActivatedThisTick,
	bool bBringUpGroupUnlocked,
	bool bIsRootBodyModifier,
	bool bAllowRootBodyModifierSimulation,
	float PolicyAlpha,
	bool bIsDistalKinematicAccepted)
{
	auto LogReturn = [&](bool bResult)
	{
		// Removed temporary debugging logs.
		return bResult;
	};

	// Guard to prevent upper-body reset re-adds.
	if (BalanceTransitionSets::IsUpperBody(BoneName) && (InRuntimeState == EPhysAnimRuntimeState::BalanceEntry_Prepare || InRuntimeState == EPhysAnimRuntimeState::BalanceEntry_LateValidate))
	{
		return LogReturn(false);
	}

	// Guard to prevent accepted distal-kinematic bones from being reset to simulated (Task: Fix stale modifier record)
	if (bIsDistalKinematicAccepted &&
		(BoneName == TEXT("calf_l") || BoneName == TEXT("foot_l") || BoneName == TEXT("ball_l") ||
		 BoneName == TEXT("calf_r") || BoneName == TEXT("foot_r") || BoneName == TEXT("ball_r")))
	{
		return LogReturn(false);
	}

	if (bForceZeroActions)
	{
		return LogReturn(false);
	}

	if (IsBalanceActiveState(InRuntimeState))
	{
		if (bIsRootBodyModifier)
		{
			// Hard design constraint: No pelvis/root resets allowed at all in Balance Perturbation Mode.
			return LogReturn(false);
		}

		if (PolicyAlpha > 0.0f)
		{
			// Hard design constraint: No limb resets allowed once the neural policy has begun influencing the body.
			return LogReturn(false);
		}
	}

	// EXPERIMENT: DO NOT schedule/apply proximal cached-target reset if we are in transition handoff
	if (InRuntimeState == EPhysAnimRuntimeState::BalanceEntry_Prepare ||
		InRuntimeState == EPhysAnimRuntimeState::BalanceEntry_LateValidate)
	{
		const FString BoneStr = BoneName.ToString().ToLower();
		if (bIsRootBodyModifier ||
			BoneStr.Contains(TEXT("spine")) ||
			BoneStr.Contains(TEXT("thigh")) ||
			BoneStr.Contains(TEXT("clavicle")) ||
			BoneStr.Contains(TEXT("upperarm")) ||
			BoneStr.Contains(TEXT("lowerarm")) ||
			BoneStr.Contains(TEXT("hand")) ||
			BoneStr.Contains(TEXT("neck")) ||
			BoneStr.Contains(TEXT("head")))
		{
			return LogReturn(false);
		}
	}

	if (bIsRootBodyModifier)
	{
		if (InRuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn)
		{
			// Phase 2 guard-window ownership requires the pelvis/root to come on without any
			// follow-up cached reset request on the same tick.
			return LogReturn(false);
		}

		// We allow exactly one reset for the root when it transitions to simulation to ensure 
		// its physical state is precisely aligned with the visual kinematic state before it begins moving.
		return LogReturn(bBodyModifierActivatedThisTick && bAllowRootBodyModifierSimulation);
	}

	return LogReturn(bBodyModifierActivatedThisTick && bBringUpGroupUnlocked);
}


void UPhysAnimComponent::TrackDistalModifierWrite(FName BoneName, EPhysicsMovementType NewMovementType, bool bUpdateBody, const FString& CallSiteReason)
{
	if (BoneName != TEXT("calf_r") && BoneName != TEXT("foot_r") && BoneName != TEXT("ball_r") &&
		BoneName != TEXT("calf_l") && BoneName != TEXT("foot_l") && BoneName != TEXT("ball_l"))
	{
		return;
	}

	EPhysicsMovementType PreviousMovementType = EPhysicsMovementType::Simulated; 
	bool bHadPrevious = false;
	if (const EPhysicsMovementType* Prev = PreviousDistalBoneModifierOwnership.Find(BoneName))
	{
		PreviousMovementType = *Prev;
		bHadPrevious = true;
	}

	if (!bHadPrevious || PreviousMovementType != NewMovementType)
	{
		if (GVerbosePhase1Forensics != 0)
		{
			PHYSANIM_LOG_RATE_LIMITED(
				LogPhysAnimBridge,
				Log,
				1.0f,
				TEXT("[PhysAnim] DISTAL_MODIFIER_WRITE: bone=%s prevModifier=%s newModifier=%s bUpdateBody=%d runtimeState=%s phase=%d reason=%s"),
				*BoneName.ToString(),
				bHadPrevious ? GetPhysicsMovementTypeName(PreviousMovementType) : TEXT("None"),
				GetPhysicsMovementTypeName(NewMovementType),
				bUpdateBody ? 1 : 0,
				GetRuntimeStateName(RuntimeState),
				(int32)BalanceReadyTransition.GetPhase(),
				*CallSiteReason);
		}

		PreviousDistalBoneModifierOwnership.Add(BoneName, NewMovementType);
	}
}


void UPhysAnimComponent::TrackDistalBoneOwnershipChange(FName BoneName, EPhysicsMovementType NewOwnership, const FString& CallSiteReason)
{
	if (BoneName != TEXT("calf_r") && BoneName != TEXT("foot_r") && BoneName != TEXT("ball_r"))
	{
		return;
	}

	EPhysicsMovementType PreviousOwnership = EPhysicsMovementType::Simulated; 
	bool bHadPrevious = false;
	if (const EPhysicsMovementType* Prev = PreviousDistalBoneIntendedOwnership.Find(BoneName))
	{
		PreviousOwnership = *Prev;
		bHadPrevious = true;
	}

	if (!bHadPrevious || PreviousOwnership != NewOwnership)
	{
		USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
		bool bRawSimulating = false;
		if (SkeletalMesh)
		{
			if (FBodyInstance* const BodyInstance = SkeletalMesh->GetBodyInstance(BoneName))
			{
				bRawSimulating = BodyInstance->IsInstanceSimulatingPhysics();
			}
		}

		if (GVerbosePhase1Forensics != 0)
		{
			PHYSANIM_LOG_RATE_LIMITED(
				LogPhysAnimBridge,
				Warning,
				1.0f,
				TEXT("[PhysAnim] DISTAL_OWNERSHIP_CHANGE: bone=%s prevIntended=%s newIntended=%s rawSimulate=%s runtimeState=%s phase=%d reason=%s"),
				*BoneName.ToString(),
				bHadPrevious ? GetPhysicsMovementTypeName(PreviousOwnership) : TEXT("None"),
				GetPhysicsMovementTypeName(NewOwnership),
				bRawSimulating ? TEXT("Simulated") : TEXT("Kinematic"),
				GetRuntimeStateName(RuntimeState),
				(int32)BalanceReadyTransition.GetPhase(),
				*CallSiteReason);
		}

		PreviousDistalBoneIntendedOwnership.Add(BoneName, NewOwnership);
	}

	// Note: Phase 1 topology intent and raw body sim state are not guaranteed to
	// be frame-synchronous inside the same component tick. We evaluate ownership
	// violations on the subsequent frame in TickComponent.
	if (IsBalanceEntryState(RuntimeState) || RuntimeState == EPhysAnimRuntimeState::BalanceSafeDeny)
	{
		const bool bShouldKeepKinematic = BalanceReadyTransition.ShouldKeepBoneKinematic(BoneName, ResolveEffectiveStabilizationSettings());
		
		USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
		bool bRawSimulating = false;
		if (SkeletalMesh)
		{
			if (FBodyInstance* const BodyInstance = SkeletalMesh->GetBodyInstance(BoneName))
			{
				bRawSimulating = BodyInstance->IsInstanceSimulatingPhysics();
			}
		}

		if (bShouldKeepKinematic)
		{
			if (bRawSimulating)
			{
				static TSet<FName> LoggedBones;
				if (BoneName == TEXT("calf_r") && !LoggedBones.Contains(BoneName))
				{
					PHYSANIM_LOG_RATE_LIMITED(
						LogPhysAnimBridge,
						Warning,
						1.0f,
						TEXT("[PhysAnim] CALF_R_DISTAL_GATE_FORENSIC: shouldKeepKinematic=%d rawSimulating=%d runtimeState=%s phase=%d reason=%s"),
						bShouldKeepKinematic ? 1 : 0,
						bRawSimulating ? 1 : 0,
						GetRuntimeStateName(RuntimeState),
						(int32)BalanceReadyTransition.GetPhase(),
						*CallSiteReason);
					LoggedBones.Add(BoneName);
				}

				PHYSANIM_LOG_RATE_LIMITED(
					LogPhysAnimBridge,
					Warning,
					1.0f,
					TEXT("[PhysAnim] DISTAL_EXPERIMENT_PENDING_OWNERSHIP_MISMATCH: bone=%s ShouldKeepBoneKinematic=true but raw BodyInstance is SIMULATING! reason=%s"),
					*BoneName.ToString(), *CallSiteReason);
			}

			FPhysAnimPendingDistalOwnershipCheck& PendingCheck = PendingDistalOwnershipChecks.FindOrAdd(BoneName);
			PendingCheck.bActive = true;
			PendingCheck.IntendedOwnership = EPhysicsMovementType::Kinematic;
			PendingCheck.ModifierMovementType = NewOwnership;
			PendingCheck.bRawBodySimulatingAtWrite = bRawSimulating;
			PendingCheck.RuntimeState = RuntimeState;
			PendingCheck.TransitionPhase = static_cast<int32>(BalanceReadyTransition.GetPhase());
			PendingCheck.CallSiteReason = CallSiteReason;
		}
	}
}

