#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"
#include "PhysAnimBalanceReadyTransitionPrivate.h"

void UPhysAnimComponent::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnim] GStrictPhase1Certification = %d"), GStrictPhase1Certification);

	FString Error;
	if (!BeginStartupTPoseCapture(Error))
	{
		UE_LOG(LogPhysAnimBridge, Error, TEXT("[PhysAnim] Startup blocked before live T-pose capture: %s"), *Error);
		SetComponentTickEnabled(false);
		TransitionRuntimeState(EPhysAnimRuntimeState::FailStopped);
		UpdateBridgeStatusIndicator(5.0f);
	}
}


void UPhysAnimComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopBridge();
	Super::EndPlay(EndPlayReason);
}

void UPhysAnimComponent::ApplyPhase1PelvisRootCouplingSolve()
{
	const bool bPhase1Prepare = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Prepare;
	const bool bPhase1LateValidate = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_LateValidate;
	if (!bPhase1Prepare && !bPhase1LateValidate)
	{
		bPhase1PelvisCouplingSkipLogged = false;
		return;
	}

	USkeletalMeshComponent* const Mesh = GetMeshComponent();
	if (!Mesh)
	{
		if (!bPhase1PelvisCouplingSkipLogged)
		{
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_PELVIS_COUPLING_SKIPPED reason=noMesh state=%s"), GetRuntimeStateName(RuntimeState));
			bPhase1PelvisCouplingSkipLogged = true;
		}
		return;
	}

	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	FBodyInstance* const PelvisBody = Mesh->GetBodyInstance(RootBoneName);
	if (!PelvisBody || !PelvisBody->IsValidBodyInstance())
	{
		if (!bPhase1PelvisCouplingSkipLogged)
		{
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_PELVIS_COUPLING_SKIPPED reason=noPelvisBody state=%s"), GetRuntimeStateName(RuntimeState));
			bPhase1PelvisCouplingSkipLogged = true;
		}
		return;
	}

	if (PelvisBody->IsInstanceSimulatingPhysics())
	{
		if (!bPhase1PelvisCouplingSkipLogged)
		{
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_PELVIS_COUPLING_SKIPPED reason=pelvisAlreadySimulating state=%s"), GetRuntimeStateName(RuntimeState));
			bPhase1PelvisCouplingSkipLogged = true;
		}
		return;
	}

	if (UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get())
	{
		const FName PelvisModifierName = PhysAnimBridge::MakeBodyModifierName(RootBoneName);
		if (const FPhysicsBodyModifierRecord* Record = FPhysAnimPhysicsControlAccessor::GetModifierRecord(PhysicsControl, PelvisModifierName))
		{
			if (Record->BodyModifier.ModifierData.MovementType != EPhysicsMovementType::Kinematic ||
				!Record->BodyModifier.ModifierData.bUpdateKinematicFromSimulation)
			{
				if (!bPhase1PelvisCouplingSkipLogged)
				{
					UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_PELVIS_COUPLING_SKIPPED reason=modifierNotPhase1Kinematic state=%s modifier=%s updateFromSim=%d"),
						GetRuntimeStateName(RuntimeState),
						GetPhysicsMovementTypeName(Record->BodyModifier.ModifierData.MovementType),
						Record->BodyModifier.ModifierData.bUpdateKinematicFromSimulation ? 1 : 0);
					bPhase1PelvisCouplingSkipLogged = true;
				}
				return;
			}
		}
	}

	const int32 PelvisBoneIndex = Mesh->GetBoneIndex(RootBoneName);
	const FTransform AnimatedPelvisTransform =
		PelvisBoneIndex != INDEX_NONE
			? Mesh->GetBoneTransform(PelvisBoneIndex)
			: PelvisBody->GetUnrealWorldTransform();
	const UPhysicsAsset* const PhysicsAsset = Mesh->GetPhysicsAsset();
	FQuat DesiredPelvisRotation = AnimatedPelvisTransform.GetRotation();
	FString PelvisRotationSource = TEXT("animated_pelvis_rotation");
	struct FPhase1ConstraintRotationSample
	{
		FName ChildBoneName = NAME_None;
		FQuat CandidateRotation = FQuat::Identity;
		FQuat ChildConstraintWorldRotation = FQuat::Identity;
		FQuat ParentConstraintLocalRotation = FQuat::Identity;
		FVector ChildAnchorWorld = FVector::ZeroVector;
		bool bValid = false;
		FString Source;
	};
	TArray<FPhase1ConstraintRotationSample> RotationSamples;
	RotationSamples.Reserve(4);
	{
		FPhase1ConstraintRotationSample& LiveSample = RotationSamples.AddDefaulted_GetRef();
		LiveSample.CandidateRotation = PelvisBody->GetUnrealWorldTransform().GetRotation();
		LiveSample.Source = TEXT("live_pelvis_rotation");
		LiveSample.bValid = true;
	}
	{
		FPhase1ConstraintRotationSample& AnimatedSample = RotationSamples.AddDefaulted_GetRef();
		AnimatedSample.CandidateRotation = AnimatedPelvisTransform.GetRotation();
		AnimatedSample.Source = TEXT("animated_pelvis_rotation");
		AnimatedSample.bValid = true;
	}

	FVector DesiredPelvisLocation = FVector::ZeroVector;
	int32 DesiredPelvisLocationSamples = 0;
	const auto AccumulateConstraintAnchoredCandidate = [&](const FName ChildBoneName)
	{
		if (!PhysicsAsset)
		{
			return;
		}

		const int32 ConstraintIndex = PhysicsAsset->FindConstraintIndex(ChildBoneName, RootBoneName);
		if (ConstraintIndex == INDEX_NONE || !PhysicsAsset->ConstraintSetup.IsValidIndex(ConstraintIndex))
		{
			return;
		}

		const UPhysicsConstraintTemplate* const ConstraintTemplate = PhysicsAsset->ConstraintSetup[ConstraintIndex];
		const FConstraintInstance* const ConstraintInstance = ConstraintTemplate ? &ConstraintTemplate->DefaultInstance : nullptr;
		if (!ConstraintInstance)
		{
			return;
		}

		const int32 ChildBoneIndex = Mesh->GetBoneIndex(ChildBoneName);
		if (ChildBoneIndex == INDEX_NONE)
		{
			return;
		}

		const FTransform ChildWorldTransform =
			Mesh->GetBodyInstance(ChildBoneName)
				? Mesh->GetBodyInstance(ChildBoneName)->GetUnrealWorldTransform()
				: Mesh->GetBoneTransform(ChildBoneIndex);
		FPhase1ConstraintRotationSample& ConstraintSample = RotationSamples.AddDefaulted_GetRef();
		ConstraintSample.ChildBoneName = ChildBoneName;
		ConstraintSample.CandidateRotation =
			(ChildWorldTransform.GetRotation() *
			 ConstraintInstance->GetRefFrame(EConstraintFrame::Frame1).GetRotation() *
			 ConstraintInstance->GetRefFrame(EConstraintFrame::Frame2).GetRotation().Inverse()).GetNormalized();
		ConstraintSample.ChildConstraintWorldRotation =
			(ChildWorldTransform.GetRotation() * ConstraintInstance->GetRefFrame(EConstraintFrame::Frame1).GetRotation()).GetNormalized();
		ConstraintSample.ParentConstraintLocalRotation = ConstraintInstance->GetRefFrame(EConstraintFrame::Frame2).GetRotation();
		ConstraintSample.ChildAnchorWorld = ChildWorldTransform.TransformPosition(ConstraintInstance->Pos1);
		ConstraintSample.Source = FString::Printf(TEXT("constraint_%s"), *ChildBoneName.ToString());
		ConstraintSample.bValid = true;
		++DesiredPelvisLocationSamples;
	};

	AccumulateConstraintAnchoredCandidate(TEXT("thigh_l"));
	AccumulateConstraintAnchoredCandidate(TEXT("thigh_r"));
	AccumulateConstraintAnchoredCandidate(TEXT("spine_01"));
	TArray<const FPhase1ConstraintRotationSample*> ValidConstraintSamples;
	ValidConstraintSamples.Reserve(RotationSamples.Num());
	for (const FPhase1ConstraintRotationSample& Sample : RotationSamples)
	{
		if (Sample.bValid && Sample.ChildBoneName != NAME_None)
		{
			ValidConstraintSamples.Add(&Sample);
		}
	}
	for (int32 SampleIndex = 0; SampleIndex < ValidConstraintSamples.Num(); ++SampleIndex)
	{
		const FPhase1ConstraintRotationSample& SampleA = *ValidConstraintSamples[SampleIndex];
		for (int32 OtherIndex = SampleIndex + 1; OtherIndex < ValidConstraintSamples.Num(); ++OtherIndex)
		{
			const FPhase1ConstraintRotationSample& SampleB = *ValidConstraintSamples[OtherIndex];
			static const float BlendWeights[] = { 0.10f, 0.25f, 0.33f, 0.50f, 0.67f, 0.75f, 0.90f };
			for (const float BlendWeight : BlendWeights)
			{
				FQuat BlendedRotation = FQuat::Slerp(SampleA.CandidateRotation, SampleB.CandidateRotation, BlendWeight).GetNormalized();
				if ((DesiredPelvisRotation | BlendedRotation) < 0.0f)
				{
					BlendedRotation *= -1.0f;
				}

				FPhase1ConstraintRotationSample& BlendSample = RotationSamples.AddDefaulted_GetRef();
				BlendSample.CandidateRotation = BlendedRotation;
				BlendSample.Source = FString::Printf(TEXT("blend_%s_%s_%.2f"), *SampleA.ChildBoneName.ToString(), *SampleB.ChildBoneName.ToString(), BlendWeight);
				BlendSample.bValid = true;
			}
		}
	}
	if (ValidConstraintSamples.Num() >= 3)
	{
		FVector4 WeightedRotationSum = FVector4::Zero();
		const FQuat ReferenceRotation = ValidConstraintSamples[0]->CandidateRotation;
		for (const FPhase1ConstraintRotationSample* Sample : ValidConstraintSamples)
		{
			FQuat AlignedRotation = Sample->CandidateRotation;
			if ((ReferenceRotation | AlignedRotation) < 0.0f)
			{
				AlignedRotation *= -1.0f;
			}

			WeightedRotationSum.X += AlignedRotation.X;
			WeightedRotationSum.Y += AlignedRotation.Y;
			WeightedRotationSum.Z += AlignedRotation.Z;
			WeightedRotationSum.W += AlignedRotation.W;
		}

		FQuat AveragedRotation(WeightedRotationSum.X, WeightedRotationSum.Y, WeightedRotationSum.Z, WeightedRotationSum.W);
		if (AveragedRotation.SizeSquared() > KINDA_SMALL_NUMBER)
		{
			AveragedRotation.Normalize();
		}
		else
		{
			AveragedRotation = ReferenceRotation;
		}
		if ((DesiredPelvisRotation | AveragedRotation) < 0.0f)
		{
			AveragedRotation *= -1.0f;
		}

		FPhase1ConstraintRotationSample& AverageSample = RotationSamples.AddDefaulted_GetRef();
		AverageSample.CandidateRotation = AveragedRotation;
		AverageSample.Source = TEXT("blend_all_direct");
		AverageSample.bValid = true;
	}

	if (DesiredPelvisLocationSamples == 0)
	{
		if (!bPhase1PelvisCouplingSkipLogged)
		{
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_PELVIS_COUPLING_SKIPPED reason=noChildCandidates state=%s"), GetRuntimeStateName(RuntimeState));
			bPhase1PelvisCouplingSkipLogged = true;
		}
		return;
	}

	const auto EvaluateCandidateRotation = [&](const FQuat& CandidateRotation, float& OutMaxAngularErrorDeg, float& OutMeanAngularErrorDeg)
	{
		OutMaxAngularErrorDeg = 0.0f;
		OutMeanAngularErrorDeg = 0.0f;
		int32 ValidConstraintCount = 0;
		for (const FPhase1ConstraintRotationSample& Sample : RotationSamples)
		{
			if (!Sample.bValid || Sample.ParentConstraintLocalRotation.Equals(FQuat::Identity) && Sample.ChildConstraintWorldRotation.Equals(FQuat::Identity))
			{
				continue;
			}

			const FQuat ParentConstraintWorldRotation = (CandidateRotation * Sample.ParentConstraintLocalRotation).GetNormalized();
			const float AngularErrorDeg =
				FMath::RadiansToDegrees(ParentConstraintWorldRotation.AngularDistance(Sample.ChildConstraintWorldRotation));
			OutMaxAngularErrorDeg = FMath::Max(OutMaxAngularErrorDeg, AngularErrorDeg);
			OutMeanAngularErrorDeg += AngularErrorDeg;
			++ValidConstraintCount;
		}

		if (ValidConstraintCount > 0)
		{
			OutMeanAngularErrorDeg /= static_cast<float>(ValidConstraintCount);
		}
	};
	const auto EvaluateCandidateTiltDeg = [](const FQuat& CandidateRotation)
	{
		const FVector AxisX = CandidateRotation.GetAxisX();
		const FVector AxisY = CandidateRotation.GetAxisY();
		const FVector AxisZ = CandidateRotation.GetAxisZ();

		const float DotX = FMath::Abs(FVector::DotProduct(AxisX, FVector::UpVector));
		const float DotY = FMath::Abs(FVector::DotProduct(AxisY, FVector::UpVector));
		const float DotZ = FMath::Abs(FVector::DotProduct(AxisZ, FVector::UpVector));

		FVector CandidateUp = AxisZ;
		if (DotX > DotY && DotX > DotZ)
		{
			CandidateUp = AxisX;
		}
		else if (DotY > DotX && DotY > DotZ)
		{
			CandidateUp = AxisY;
		}

		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(CandidateUp, FVector::UpVector), -1.0f, 1.0f)));
	};
	float BestMaxAngularErrorDeg = TNumericLimits<float>::Max();
	float BestMeanAngularErrorDeg = TNumericLimits<float>::Max();
	float BestTiltDeg = TNumericLimits<float>::Max();
	bool bFoundTiltAdmissibleCandidate = false;
	for (const FPhase1ConstraintRotationSample& Sample : RotationSamples)
	{
		if (!Sample.bValid)
		{
			continue;
		}

		float CandidateMaxAngularErrorDeg = 0.0f;
		float CandidateMeanAngularErrorDeg = 0.0f;
		const float CandidateTiltDeg = EvaluateCandidateTiltDeg(Sample.CandidateRotation);
		EvaluateCandidateRotation(Sample.CandidateRotation, CandidateMaxAngularErrorDeg, CandidateMeanAngularErrorDeg);
		const bool bCandidateTiltAdmissible = CandidateTiltDeg <= BalanceQuietTiltThresholdDeg + KINDA_SMALL_NUMBER;
		if (bCandidateTiltAdmissible)
		{
			if (!bFoundTiltAdmissibleCandidate ||
				CandidateMaxAngularErrorDeg + KINDA_SMALL_NUMBER < BestMaxAngularErrorDeg ||
				(FMath::IsNearlyEqual(CandidateMaxAngularErrorDeg, BestMaxAngularErrorDeg, KINDA_SMALL_NUMBER) &&
				 CandidateMeanAngularErrorDeg + KINDA_SMALL_NUMBER < BestMeanAngularErrorDeg) ||
				(FMath::IsNearlyEqual(CandidateMaxAngularErrorDeg, BestMaxAngularErrorDeg, KINDA_SMALL_NUMBER) &&
				 FMath::IsNearlyEqual(CandidateMeanAngularErrorDeg, BestMeanAngularErrorDeg, KINDA_SMALL_NUMBER) &&
				 CandidateTiltDeg + KINDA_SMALL_NUMBER < BestTiltDeg))
			{
				bFoundTiltAdmissibleCandidate = true;
				BestMaxAngularErrorDeg = CandidateMaxAngularErrorDeg;
				BestMeanAngularErrorDeg = CandidateMeanAngularErrorDeg;
				BestTiltDeg = CandidateTiltDeg;
				DesiredPelvisRotation = Sample.CandidateRotation;
				PelvisRotationSource = Sample.Source;
			}
		}
		else if (!bFoundTiltAdmissibleCandidate &&
			(CandidateTiltDeg + KINDA_SMALL_NUMBER < BestTiltDeg ||
			 (FMath::IsNearlyEqual(CandidateTiltDeg, BestTiltDeg, KINDA_SMALL_NUMBER) &&
			  (CandidateMaxAngularErrorDeg + KINDA_SMALL_NUMBER < BestMaxAngularErrorDeg ||
			   (FMath::IsNearlyEqual(CandidateMaxAngularErrorDeg, BestMaxAngularErrorDeg, KINDA_SMALL_NUMBER) &&
			    CandidateMeanAngularErrorDeg + KINDA_SMALL_NUMBER < BestMeanAngularErrorDeg)))))
		{
			BestTiltDeg = CandidateTiltDeg;
			BestMaxAngularErrorDeg = CandidateMaxAngularErrorDeg;
			BestMeanAngularErrorDeg = CandidateMeanAngularErrorDeg;
			DesiredPelvisRotation = Sample.CandidateRotation;
			PelvisRotationSource = FString::Printf(TEXT("%s_tilt_fallback"), *Sample.Source);
		}
	}
	DesiredPelvisLocation = FVector::ZeroVector;
	DesiredPelvisLocationSamples = 0;
	for (const FPhase1ConstraintRotationSample& Sample : RotationSamples)
	{
		if (!Sample.bValid || Sample.ParentConstraintLocalRotation.Equals(FQuat::Identity) && Sample.ChildAnchorWorld.IsNearlyZero())
		{
			continue;
		}

		if (PhysicsAsset)
		{
			const int32 ConstraintIndex = PhysicsAsset->FindConstraintIndex(Sample.ChildBoneName, RootBoneName);
			if (ConstraintIndex != INDEX_NONE && PhysicsAsset->ConstraintSetup.IsValidIndex(ConstraintIndex))
			{
				const UPhysicsConstraintTemplate* const ConstraintTemplate = PhysicsAsset->ConstraintSetup[ConstraintIndex];
				const FConstraintInstance* const ConstraintInstance = ConstraintTemplate ? &ConstraintTemplate->DefaultInstance : nullptr;
				if (ConstraintInstance)
				{
					const FVector PelvisAnchorOffsetWorld = DesiredPelvisRotation.RotateVector(ConstraintInstance->Pos2);
					DesiredPelvisLocation += Sample.ChildAnchorWorld - PelvisAnchorOffsetWorld;
					++DesiredPelvisLocationSamples;
				}
			}
		}
	}
	if (DesiredPelvisLocationSamples == 0)
	{
		if (!bPhase1PelvisCouplingSkipLogged)
		{
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_PELVIS_COUPLING_SKIPPED reason=noResolvedAnchorLocations state=%s"), GetRuntimeStateName(RuntimeState));
			bPhase1PelvisCouplingSkipLogged = true;
		}
		return;
	}
	DesiredPelvisLocation /= static_cast<float>(DesiredPelvisLocationSamples);
	FTransform DesiredPelvisTransform = PelvisBody->GetUnrealWorldTransform();
	DesiredPelvisTransform.SetRotation(DesiredPelvisRotation);
	DesiredPelvisTransform.SetLocation(DesiredPelvisLocation);
	PelvisBody->SetBodyTransform(DesiredPelvisTransform, ETeleportType::TeleportPhysics, true);
	bPhase1PelvisCouplingSkipLogged = false;

	const FVector SolvedPelvisLocation = BalanceTransitionSets::ResolveBodyOrBoneLocationCm(Mesh, RootBoneName);
	BalanceTransitionSets::FDirectPelvisLinkForensicRecord PelvisThighLRecord;
	BalanceTransitionSets::FDirectPelvisLinkForensicRecord PelvisThighRRecord;
	BalanceTransitionSets::FDirectPelvisLinkForensicRecord PelvisSpine01Record;
	BalanceTransitionSets::BuildDirectPelvisLinkForensicRecord(Mesh, RootBoneName, TEXT("thigh_l"), PelvisThighLRecord);
	BalanceTransitionSets::BuildDirectPelvisLinkForensicRecord(Mesh, RootBoneName, TEXT("thigh_r"), PelvisThighRRecord);
	BalanceTransitionSets::BuildDirectPelvisLinkForensicRecord(Mesh, RootBoneName, TEXT("spine_01"), PelvisSpine01Record);
	const float PelvisThighLErrorCm = PelvisThighLRecord.bConstraintFound ? PelvisThighLRecord.AnchorDistanceCm : PelvisThighLRecord.BodyOriginDistanceCm;
	const float PelvisThighRErrorCm = PelvisThighRRecord.bConstraintFound ? PelvisThighRRecord.AnchorDistanceCm : PelvisThighRRecord.BodyOriginDistanceCm;
	const float PelvisSpine01ErrorCm = PelvisSpine01Record.bConstraintFound ? PelvisSpine01Record.AnchorDistanceCm : PelvisSpine01Record.BodyOriginDistanceCm;
	FString SolvedTiltSource;
	const float SolvedTiltDeg = ResolvePhase1Uprightness(Mesh, GetOwner(), RootBoneName, SolvedTiltSource);
	UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_PELVIS_COUPLING solvedLoc=(%.2f,%.2f,%.2f) rotationSource=%s solvedTiltDeg=%.2f tiltSource=%s tiltAdmissible=%d rotationScoreMax=%.2f rotationScoreMean=%.2f pelvisThighL=%.2f pelvisThighR=%.2f pelvisSpine01=%.2f pelvisThighLBodyOrigin=%.2f pelvisThighRBodyOrigin=%.2f pelvisSpine01BodyOrigin=%.2f state=%s"),
		SolvedPelvisLocation.X,
		SolvedPelvisLocation.Y,
		SolvedPelvisLocation.Z,
		*PelvisRotationSource,
		SolvedTiltDeg,
		*SolvedTiltSource,
		bFoundTiltAdmissibleCandidate ? 1 : 0,
		BestMaxAngularErrorDeg,
		BestMeanAngularErrorDeg,
		PelvisThighLErrorCm,
		PelvisThighRErrorCm,
		PelvisSpine01ErrorCm,
		PelvisThighLRecord.BodyOriginDistanceCm,
		PelvisThighRRecord.BodyOriginDistanceCm,
		PelvisSpine01Record.BodyOriginDistanceCm,
		GetRuntimeStateName(RuntimeState));
}

EPhysAnimBridgeTraceOutputMode UPhysAnimComponent::ResolveBridgeTraceOutputMode() const
{
	const int32 OverrideMode = PhysAnimComponentInternal::CVarPhysAnimTraceOutput.GetValueOnGameThread();
	if (OverrideMode >= 0)
	{
		switch (OverrideMode)
		{
		case 1:
			return EPhysAnimBridgeTraceOutputMode::MetadataAndEvents;
		case 2:
			return EPhysAnimBridgeTraceOutputMode::Full;
		default:
			return EPhysAnimBridgeTraceOutputMode::Off;
		}
	}

	if (!bEnableBridgeTraceOutput)
	{
		return EPhysAnimBridgeTraceOutputMode::Off;
	}

	return BridgeTraceOutputMode;
}


void UPhysAnimComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const EPhysAnimRuntimeState RuntimeStateAtTickStart = RuntimeState;
	const uint32 RootOnTickAtTickStart = BalanceEntryRootOnFrameCount;

	bPelvisResetAppliedThisTick = false;
	LastBridgePoseSearchDeltaTimeSeconds = DeltaTime;
	UpdateBridgeStatusIndicator(0.25f);

	for (auto It = PendingDistalOwnershipChecks.CreateIterator(); It; ++It)
	{
		FPhysAnimPendingDistalOwnershipCheck& Check = It.Value();
		if (Check.bActive)
		{
			Check.bActive = false;
			
			const FName BoneName = It.Key();
			const FPhysAnimStabilizationSettings Settings = ResolveEffectiveStabilizationSettings();
			EPhysicsMovementType CurrentModifierMovementType = EPhysicsMovementType::Simulated;
			if (UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get())
			{
				const FName CheckModifierName = PhysAnimBridge::MakeBodyModifierName(BoneName);
				if (const FPhysicsBodyModifierRecord* Record = FPhysAnimPhysicsControlAccessor::GetModifierRecord(PhysicsControl, CheckModifierName))
				{
					CurrentModifierMovementType = Record->BodyModifier.ModifierData.MovementType;
				}
			}
			
			bool bCurrentRawSimulating = false;
			FVector LinearVelocity = FVector::ZeroVector;
			FVector AngularVelocity = FVector::ZeroVector;
			if (USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get())
			{
				if (FBodyInstance* const BodyInstance = SkeletalMesh->GetBodyInstance(BoneName))
				{
					bCurrentRawSimulating = BodyInstance->IsInstanceSimulatingPhysics();
					LinearVelocity = BodyInstance->GetUnrealWorldVelocity();
					AngularVelocity = FMath::RadiansToDegrees(BodyInstance->GetUnrealWorldAngularVelocityInRadians());
				}
			}

			const float TargetDelta = LastControlTargetDiagnostics.MaxTargetDeltaDegrees;
			const bool bHasPendingReset = PendingBodyModifierCachedResetNames.Contains(BoneName);


			FString Classification = TEXT("fully_aligned");
			if (Check.IntendedOwnership != EPhysicsMovementType::Kinematic)
			{
				Classification = TEXT("not_a_kinematic_experiment_case");
			}
			else if (CurrentModifierMovementType == EPhysicsMovementType::Kinematic && !bCurrentRawSimulating)
			{
				BalanceReadyTransition.DistalBoneConsecutiveMismatchTicks.Remove(BoneName);
				if (Check.bRawBodySimulatingAtWrite)
				{
					Classification = TEXT("transient_match_after_delay");
					BalanceReadyTransition.DistalMismatchesTransientCount++;
				}
				else
				{
					Classification = TEXT("fully_aligned");
				}
			}
			else
			{
				const int32 MismatchTicks = ++BalanceReadyTransition.DistalBoneConsecutiveMismatchTicks.FindOrAdd(BoneName);
				constexpr int32 PersistenceThreshold = 2;

				if (MismatchTicks < PersistenceThreshold)
				{
					Classification = TEXT("modifier_not_yet_kinematic_pending");
					BalanceReadyTransition.DistalMismatchesPendingCount++;
				}
				else
				{
					if (CurrentModifierMovementType == EPhysicsMovementType::Simulated)
					{
						Classification = TEXT("persistent_modifier_not_yet_kinematic");
					}
					else
					{
						Classification = TEXT("persistent_raw_simulation");
					}
					BalanceReadyTransition.DistalMismatchesPersistentCount++;
					BalanceReadyTransition.DistalBonePersistentTicks.FindOrAdd(BoneName)++;
				}
			}

			const bool bClassificationChanged = !LastDistalClassification.Contains(BoneName) || LastDistalClassification[BoneName] != Classification;

			if (bClassificationChanged)
			{
				LastDistalClassification.Add(BoneName, Classification);
			}
		}
	}

	if (bPendingStartupRestPoseCapture)
	{
		bPendingStartupRestPoseCapture = false;
		FString StartupError;
		if (!FinalizeStartupTPoseCaptureAndStartBridge(StartupError))
		{
			UE_LOG(LogPhysAnimBridge, Error, TEXT("[PhysAnim] Startup blocked after live T-pose capture: %s"), *StartupError);
			TransitionRuntimeState(EPhysAnimRuntimeState::FailStopped);
			UpdateBridgeStatusIndicator(5.0f);
			SetComponentTickEnabled(false);
		}
		return;
	}
	if (RuntimeState != EPhysAnimRuntimeState::WaitingForPoseSearch &&
		RuntimeState != EPhysAnimRuntimeState::ReadyForActivation &&
		RuntimeState != EPhysAnimRuntimeState::BridgeActive &&
		!IsBalanceEntryState(RuntimeState) &&
		RuntimeState != EPhysAnimRuntimeState::BalanceActive_Recovery)
	{
		return;
	}

	UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	UAnimInstance* const LocalAnimInstance = this->AnimInstance.Get();
	const bool bCanTraceFrames =
		BridgeTraceWriter.IsValid() &&
		BridgeTraceWriter->CanWriteFrames() &&
		(RuntimeState == EPhysAnimRuntimeState::BridgeActive || RuntimeState == EPhysAnimRuntimeState::BalanceActive_Recovery);
	const int32 TraceSampleEveryNthFrame = FMath::Max(BridgeTraceSampleEveryNthFrame, 1);
	const double BridgeTickStartSeconds = FPlatformTime::Seconds();
	bool bTraceFrameFinalized = false;
	bool bWriteTraceFrameThisTick = false;
	FPhysAnimBridgeTraceFrame TraceFrame;

	auto MeasureElapsedMs = [](const double StartSeconds) -> float
	{
		return static_cast<float>((FPlatformTime::Seconds() - StartSeconds) * 1000.0);
	};

	auto FinalizeTraceFrame = [&]()
	{
		if (bTraceFrameFinalized)
		{
			return;
		}

		bTraceFrameFinalized = true;
		if (bWriteTraceFrameThisTick && BridgeTraceWriter.IsValid() && BridgeTraceWriter->CanWriteFrames())
		{
			TraceFrame.RuntimeState = GetRuntimeStateName(RuntimeState);
			TraceFrame.NneRuntimeName = ActiveRuntimeName;
			TraceFrame.bPolicyInfluenceActive = LastControlTargetDiagnostics.bPolicyInfluenceActive;
			TraceFrame.bFirstPolicyEnabledFrame = LastControlTargetDiagnostics.bFirstPolicyEnabledFrame;
			TraceFrame.NumNormalPolicyTargetsWritten = LastControlTargetDiagnostics.NumNormalPolicyTargetsWritten;
			TraceFrame.NumHeldTargetsWritten = LastControlTargetDiagnostics.NumHeldTargetsWritten;
			TraceFrame.NumTotalTargetsWritten = LastControlTargetDiagnostics.NumTotalTargetsWritten;
			TraceFrame.ActionDiagnostics = LastActionDiagnostics;
			TraceFrame.ControlTargetDiagnostics = LastControlTargetDiagnostics;
			TraceFrame.InstabilityDiagnostics = LastRuntimeInstabilityDiagnostics;
			TraceFrame.BridgeTickTotalMs = MeasureElapsedMs(BridgeTickStartSeconds);
			BridgeTraceWriter->AppendFrame(TraceFrame);
		}

		FlushBridgeTrace(false);
	};

	auto FailStopWithTrace = [&](const FString& Reason)
	{
		FinalizeTraceFrame();
		FailStop(Reason);
	};

	auto TracePreservedTick4 = [&](const TCHAR* Source)
	{
		if (BalanceReadyTransition.GetPhase2GuardTickCount() == 4)
		{
			if (USkeletalMeshComponent* const Mesh = GetMeshComponent())
			{
				static const FName PreservedBones[] = { PhysAnimBridge::GetRootBoneName(), TEXT("thigh_l"), TEXT("thigh_r"), TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03") };
				for (const FName& BoneName : PreservedBones)
				{
					if (FBodyInstance* const BI = Mesh->GetBodyInstance(BoneName))
					{
						EPhysicsMovementType ModifierType = EPhysicsMovementType::Static;
						if (UPhysicsControlComponent* const PC = PhysicsControlComponent.Get())
						{
							const FName ModifierName = PhysAnimBridge::MakeBodyModifierName(BoneName);
							if (const FPhysicsBodyModifierRecord* Record = FPhysAnimPhysicsControlAccessor::GetModifierRecord(PC, ModifierName))
							{
								ModifierType = Record->BodyModifier.ModifierData.MovementType;
							}
						}

						UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnim] PHASE2_PRESERVED_SET_LATE_LOOP_STATE source=%s bone=%s rawSim=%d modifier=%d linSpeed=%.1f angSpeed=%.1f"),
							Source, *BoneName.ToString(), BI->IsInstanceSimulatingPhysics() ? 1 : 0, (int32)ModifierType,
							BI->GetUnrealWorldVelocity().Size(), FMath::RadiansToDegrees(BI->GetUnrealWorldAngularVelocityInRadians()).Size());
					}
				}
			}
		}
	};

	auto TraceUpperBodyTick4 = [&](const TCHAR* Source)
	{
		if (BalanceReadyTransition.GetPhase2GuardTickCount() == 4)
		{
			if (USkeletalMeshComponent* const Mesh = GetMeshComponent())
			{
				static const FName UpperBodyBones[] = { TEXT("head"), TEXT("neck_01"), TEXT("clavicle_l"), TEXT("clavicle_r") };
				for (const FName& BoneName : UpperBodyBones)
				{
					if (FBodyInstance* const BI = Mesh->GetBodyInstance(BoneName))
					{
						EPhysicsMovementType ModifierType = EPhysicsMovementType::Static;
						if (UPhysicsControlComponent* const PC = PhysicsControlComponent.Get())
						{
							const FName ModifierName = PhysAnimBridge::MakeBodyModifierName(BoneName);
							if (const FPhysicsBodyModifierRecord* Record = FPhysAnimPhysicsControlAccessor::GetModifierRecord(PC, ModifierName))
							{
								ModifierType = Record->BodyModifier.ModifierData.MovementType;
							}
						}

						UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnim] PHASE2_UPPER_BODY_LATE_LOOP_STATE source=%s bone=%s rawSim=%d modifier=%d linSpeed=%.1f angSpeed=%.1f"),
							Source, *BoneName.ToString(), BI->IsInstanceSimulatingPhysics() ? 1 : 0, (int32)ModifierType,
							BI->GetUnrealWorldVelocity().Size(), FMath::RadiansToDegrees(BI->GetUnrealWorldAngularVelocityInRadians()).Size());
					}
				}
			}
		}
	};

	auto TraceSpineTick2 = [&](const TCHAR* Source)
	{
		static uint64 LastTraceFrame = 0;
		static bool bLatchedThisTick = false;
		if (LastTraceFrame != GFrameCounter)
		{
			bLatchedThisTick = (RuntimeStateAtTickStart == EPhysAnimRuntimeState::BalanceEntry_RootOn && RootOnTickAtTickStart == 1);
			LastTraceFrame = GFrameCounter;
		}

		if (bLatchedThisTick)
		{
			if (USkeletalMeshComponent* const Mesh = GetMeshComponent())
			{
				static const FName SpineBones[] =
				{
					TEXT("spine_01"),
					TEXT("spine_02"),
					TEXT("spine_03")
				};
				for (const FName& BoneName : SpineBones)
				{
					if (FBodyInstance* const BI = Mesh->GetBodyInstance(BoneName))
					{
						EPhysicsMovementType ModifierType = EPhysicsMovementType::Simulated;
						if (UPhysicsControlComponent* const PC = PhysicsControlComponent.Get())
						{
							const FName ModifierName = PhysAnimBridge::MakeBodyModifierName(BoneName);
							if (const FPhysicsBodyModifierRecord* Record = FPhysAnimPhysicsControlAccessor::GetModifierRecord(PC, ModifierName))
							{
								ModifierType = Record->BodyModifier.ModifierData.MovementType;
							}
						}

						float BoneMaxTargetDelta = 0.0f;
						if (LastControlTargetDiagnostics.MaxTargetDeltaBoneName == BoneName)
						{
							BoneMaxTargetDelta = LastControlTargetDiagnostics.MaxTargetDeltaDegrees;
						}
						UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnim] PHASE2_SPINE_TICK2_STATE source=%s bone=%s rawSim=%d modifier=%s linSpeed=%.1f angSpeed=%.1f targetDelta=%.2f pendingReset=%d policySuppressed=%d shellCorrectionActive=%d"),
							Source, *BoneName.ToString(), BI->IsInstanceSimulatingPhysics() ? 1 : 0, GetPhysicsMovementTypeName(ModifierType),
							BI->GetUnrealWorldVelocity().Size(), FMath::RadiansToDegrees(BI->GetUnrealWorldAngularVelocityInRadians()).Size(),
							BoneMaxTargetDelta, PendingBodyModifierCachedResetNames.Contains(BoneName) ? 1 : 0,
							BalanceReadyTransition.ShouldSuppressPolicy() ? 1 : 0, IsTransitionOwnedShellLocked() ? 1 : 0);
					}
				}
			}
		}
	};

	if (!PhysicsControl || !SkeletalMesh || !LocalAnimInstance)
	{
		FailStopWithTrace(TEXT("Runtime context became invalid after startup."));
		return;
	}

	UpdateStabilizationStressTestState(StabilizationSettings);
	const FPhysAnimStabilizationSettings EffectiveSettings = ResolveEffectiveStabilizationSettings();
	ApplyMovementSmokeInput(EffectiveSettings);
	if ((RuntimeState == EPhysAnimRuntimeState::BridgeActive || RuntimeState == EPhysAnimRuntimeState::BalanceActive_Recovery) && bStartupMovementLockActive)
	{
		const bool bPhase1Prepare = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Prepare;
		const bool bPhase1LateValidate = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_LateValidate;
		const bool bPhase1RootOn = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn;
		const bool bPhase1Settle = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle;
		const bool bBalanceOwnsShellLock =
			IsTransitionOwnedShellLocked() ||
			bPhase1Prepare ||
			bPhase1LateValidate ||
			bPhase1RootOn ||
			bPhase1Settle ||
			bPendingBalanceModeStartRequest;
		if (bBalanceOwnsShellLock)
		{
			ResetPolicySettleWindowState();
		}
		// In Balance Mode, we always want the CharacterMovement to be locked (MOVE_None)
		// to ensure the capsule doesn't move and we can measure drift/contamination accurately.
		if (RuntimeState != EPhysAnimRuntimeState::BalanceActive_Recovery && 
		!bPhase1Prepare &&
		!bPhase1LateValidate &&
		!bPhase1RootOn &&
		!bPhase1Settle &&
		!bBalanceOwnsShellLock)
		{
			if (!EffectiveSettings.bLockCharacterMovementUntilStartupReady)
			{
				ReleaseStartupMovementLock(true);
				UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnim] Released startup movement lock because startup movement locking is disabled."));
			}
			else
			{
				const float PolicyInfluenceAlpha = CalculateCurrentPolicyInfluenceAlpha(EffectiveSettings);
				if (!EffectiveSettings.bDelayMovementUnlockUntilPolicySettled)
				{
					if (PolicyInfluenceAlpha > KINDA_SMALL_NUMBER)
					{
						if (EffectiveSettings.bRestoreCharacterMovementAfterStartupReady)
						{
							ReleaseStartupMovementLock(true);
							UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnim] Released startup movement lock after bridge became ready for policy-driven motion."));
						}
						else
						{
							ReleaseStartupMovementLock(false);
							UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnim] Policy became ready, but retaining CharacterMovement lock for diagnostic isolation."));
						}
					}
				}
				else if (PolicyInfluenceAlpha >= EffectiveSettings.PolicySettleMinInfluenceAlpha)
				{
					float PolicySettleShellOffsetCm = 0.0f;
					float PolicySettleRootLinearSpeedCmPerSecond = 0.0f;
					float PolicySettleRootAngularSpeedDegPerSecond = 0.0f;
					if (UpdatePolicySettleWindow(EffectiveSettings, PolicySettleShellOffsetCm, PolicySettleRootLinearSpeedCmPerSecond, PolicySettleRootAngularSpeedDegPerSecond))
					{
						if (EffectiveSettings.bRestoreCharacterMovementAfterStartupReady)
						{
							ReleaseStartupMovementLock(true);
							UE_LOG(
								LogPhysAnimBridge,
								Log,
								TEXT("[PhysAnim] Released startup movement lock after policy settled: influenceAlpha=%.2f shellOffsetCm=%.1f rootLinearCmPerSec=%.1f rootAngularDegPerSec=%.1f settledFor=%.2f."),
								PolicyInfluenceAlpha,
								PolicySettleShellOffsetCm,
								PolicySettleRootLinearSpeedCmPerSecond,
								PolicySettleRootAngularSpeedDegPerSecond,
								PolicySettleWindowAccumulatedSeconds);
						}
						else
						{
							ReleaseStartupMovementLock(false);
							UE_LOG(
								LogPhysAnimBridge,
								Log,
								TEXT("[PhysAnim] Policy settled, but retaining CharacterMovement lock for diagnostic isolation: influenceAlpha=%.2f shellOffsetCm=%.1f rootLinearCmPerSec=%.1f rootAngularDegPerSec=%.1f settledFor=%.2f."),
								PolicyInfluenceAlpha,
								PolicySettleShellOffsetCm,
								PolicySettleRootLinearSpeedCmPerSecond,
								PolicySettleRootAngularSpeedDegPerSecond,
								PolicySettleWindowAccumulatedSeconds);
						}
					}
					else
					{
						const double CurrentTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
						if (LastPolicySettleGateLogTimeSeconds < 0.0 || (CurrentTimeSeconds - LastPolicySettleGateLogTimeSeconds) >= 0.5)
						{
							UE_LOG(
								LogPhysAnimBridge,
								Log,
								TEXT("[PhysAnim] Delaying movement unlock until policy settles: influenceAlpha=%.2f shellOffsetCm=%.1f rootLinearCmPerSec=%.1f rootAngularDegPerSec=%.1f quietAccum=%.2f/%.2f."),
								PolicyInfluenceAlpha,
								PolicySettleShellOffsetCm,
								PolicySettleRootLinearSpeedCmPerSecond,
								PolicySettleRootAngularSpeedDegPerSecond,
								PolicySettleWindowAccumulatedSeconds,
								EffectiveSettings.PolicySettleRequiredSeconds);
							LastPolicySettleGateLogTimeSeconds = CurrentTimeSeconds;
						}
					}
				}
				else
				{
					ResetPolicySettleWindowState();
				}
			}
		}
	}

	if (RuntimeState == EPhysAnimRuntimeState::BridgeActive)
	{
		CaptureBridgeIntent(EffectiveSettings);
		if (!bStartupMovementLockActive)
		{
			ApplyBridgeOwnedMovementDrive(DeltaTime, EffectiveSettings);
		}
	}

	TRACE_COUNTER_SET(COUNTER_PhysAnim_RuntimeState, static_cast<int64>(RuntimeState));
	FString TickError;
	FPoseSearchBlueprintResult SearchResult;

	if (RuntimeState == EPhysAnimRuntimeState::WaitingForPoseSearch)
	{
		const double PoseSearchStartSeconds = FPlatformTime::Seconds();
		TRACE_CPUPROFILER_EVENT_SCOPE(PhysAnim_PoseSearchQuery);
		const bool bPoseSearchValid = QueryPoseSearch(SearchResult, TickError);
		TRACE_COUNTER_SET(COUNTER_PhysAnim_PoseSearchQueryMs, static_cast<float>(MeasureElapsedMs(PoseSearchStartSeconds)));
		if (bWriteTraceFrameThisTick)
		{
			TraceFrame.PoseSearchQueryMs = MeasureElapsedMs(PoseSearchStartSeconds);
			TraceFrame.bPoseSearchValid = bPoseSearchValid;
		}

		if (!bPoseSearchValid)
		{
			const double WorldTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
			const double WaitSeconds = WorldTimeSeconds - InitialPoseSearchWaitStartTimeSeconds;
			if (IsInitialPoseSearchWaitTimedOut(WaitSeconds, PhysAnimComponentInternal::InitialPoseSearchWaitTimeoutSeconds))
			{
				FailStopWithTrace(FString::Printf(TEXT("Initial PoseSearch result was never produced within %.2fs. %s"), PhysAnimComponentInternal::InitialPoseSearchWaitTimeoutSeconds, *TickError));
				return;
			}

			FinalizeTraceFrame();
			return;
		}

		LastValidPoseSearchResult = SearchResult;
		ConsecutiveInvalidPoseSearchFrames = 0;

		if (EffectiveSettings.bLockCharacterMovementUntilStartupReady)
		{
			float StartupQuietLinearSpeedCmPerSecond = 0.0f;
			float StartupQuietAngularSpeedDegPerSecond = 0.0f;
			if (!UpdateStartupQuietWindow(DeltaTime, EffectiveSettings, StartupQuietLinearSpeedCmPerSecond, StartupQuietAngularSpeedDegPerSecond))
			{
				const double CurrentTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
				if (LastStartupQuietGateLogTimeSeconds < 0.0 || (CurrentTimeSeconds - LastStartupQuietGateLogTimeSeconds) >= 0.5)
				{
					UE_LOG(
						LogPhysAnimBridge,
						Log,
						TEXT("[PhysAnim] Startup quiet-window gate holding bridge activation: linearCmPerSec=%.1f angularDegPerSec=%.1f accumulated=%.2fs required=%.2fs"),
						StartupQuietLinearSpeedCmPerSecond,
						StartupQuietAngularSpeedDegPerSecond,
						StartupQuietWindowAccumulatedSeconds,
						EffectiveSettings.StartupQuietRequiredSeconds);
					LastStartupQuietGateLogTimeSeconds = CurrentTimeSeconds;
				}

				FinalizeTraceFrame();
				return;
			}

			ReleaseStartupMovementLock(true);
		}

		if (ResolveInitialPoseSearchSuccessState(EffectiveSettings.bForceZeroActions) == EPhysAnimRuntimeState::ReadyForActivation)
		{
			EnterReadyForActivation(EffectiveSettings, TEXT("StartupReadyForActivation"), true);
			EmitBridgeTraceEvent(TEXT("startup_success"), TEXT("Startup reached ReadyForActivation after the first valid PoseSearch result."));
			FinalizeTraceFrame();
			return;
		}

		if (!ActivateBridgeFromReadyState(EffectiveSettings, TEXT("StartupActivation"), TickError))
		{
			FailStopWithTrace(TickError);
			return;
		}
		if (EffectiveSettings.bLockCharacterMovementUntilStartupReady)
		{
			ApplyStartupMovementLock();
		}
		UE_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnim] Startup success. Runtime=%s Model=%s"),
			*ActiveRuntimeName,
			*GetPathNameSafe(LoadedModelData));
		UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnim] Stabilization %s"), *PhysAnimComponentInternal::BuildStabilizationSummary(EffectiveSettings));
		EmitBridgeTraceEvent(TEXT("startup_success"), TEXT("Startup activated the bridge after the first valid PoseSearch result."));
		FinalizeTraceFrame();
		return;
	}

	if (ShouldDeactivateBridgeToSafeMode(RuntimeState, EffectiveSettings.bForceZeroActions))
	{
		DeactivateRuntimePhysicsControl(TEXT("ReadyForActivation"));
		ResetBridgePhysicsState();
		EnterReadyForActivation(EffectiveSettings, TEXT("AfterDeferredDeactivation"), false);
		FinalizeTraceFrame();
		return;
	}

	if (ShouldActivateBridgeFromSafeMode(RuntimeState, EffectiveSettings.bForceZeroActions))
	{
		if (!ActivateBridgeFromReadyState(EffectiveSettings, TEXT("DeferredActivation"), TickError))
		{
			FailStopWithTrace(TickError);
			return;
		}

		UE_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnim] Deferred activation complete. Runtime=%s Model=%s"),
			*ActiveRuntimeName,
			*GetPathNameSafe(LoadedModelData));
		UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnim] Stabilization %s"), *PhysAnimComponentInternal::BuildStabilizationSummary(EffectiveSettings));
		FinalizeTraceFrame();
		return;
	}

	if (RuntimeState == EPhysAnimRuntimeState::ReadyForActivation)
	{
		FinalizeTraceFrame();
		return;
	}

	const float PolicyControlIntervalSeconds = ResolvePolicyControlIntervalSeconds(EffectiveSettings.PolicyControlRateHz);
	int32 ElapsedPolicySteps = 0;
	const bool bRunPolicyUpdateThisTick = AdvancePolicyControlAccumulator(
		DeltaTime,
		PolicyControlIntervalSeconds,
		PolicyUpdateAccumulatorSeconds,
		ElapsedPolicySteps);
	LastPolicyElapsedSteps = ElapsedPolicySteps;

	// State advancement must happen before tuning and control writes to avoid one-tick phase lags.
	const float PreviousSimulationHandoffAlpha = SimulationHandoffAlpha;
	SimulationHandoffAlpha = CalculateSimulationHandoffAlpha(EffectiveSettings);
	const bool bSimulationHandoffCompletedThisTick =
		PreviousSimulationHandoffAlpha < 1.0f && SimulationHandoffAlpha >= 1.0f;
	if (bSimulationHandoffCompletedThisTick)
	{
		SimulationHandoffCompletedTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : BridgeStartTimeSeconds;
		UnlockBringUpGroup(0, TEXT("SimulationHandoff"));
	}

	TrackStabilizationStressTestObservations();
	AdvanceBringUpState(DeltaTime, EffectiveSettings);
	MaintainTransitionOwnedShellLock();
	
	if (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn && BalanceReadyTransition.GetPhase2GuardTickCount() == 0)
	{
		static const FName AuditBones[] = { PhysAnimBridge::GetRootBoneName(), TEXT("thigh_l"), TEXT("thigh_r"), TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03") };
		for (const FName& BoneName : AuditBones)
		{
			if (SkeletalMesh && PhysicsControl)
			{
				if (FBodyInstance* BI = SkeletalMesh->GetBodyInstance(BoneName))
				{
					const FName ModifierName = PhysAnimBridge::MakeBodyModifierName(BoneName);
					const FName ControlName = PhysAnimBridge::MakeControlName(BoneName);
					EPhysicsMovementType ModifierType = EPhysicsMovementType::Static;
					if (const FPhysicsBodyModifierRecord* Record = FPhysAnimPhysicsControlAccessor::GetModifierRecord(PhysicsControl, ModifierName))
					{
						ModifierType = Record->BodyModifier.ModifierData.MovementType;
					}
					
					float BoneMaxTargetDelta = 0.0f;
					if (LastControlTargetDiagnostics.MaxTargetDeltaBoneName == BoneName)
					{
						BoneMaxTargetDelta = LastControlTargetDiagnostics.MaxTargetDeltaDegrees;
					}

					const bool bIsFirstFailureTrigger = BalanceReadyTransition.GetFailureReason().IsEmpty();
					const bool bIsSpikeFail = BalanceReadyTransition.GetFailureReason().Contains(TEXT("spike"));

					// Emit only if it's a spike (but trim repeated spikes)
					static TMap<UPhysAnimComponent*, bool> LoggedSpikeThisPhase;
					if (bIsSpikeFail && !LoggedSpikeThisPhase.FindOrAdd(this))
					{
						UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnim] PHASE2_PRE_GUARD_BODY_SPIKE_AUDIT bone=%s rawSim=%d mod=%d linVel=%.1f angVel=%.1f alpha=%.2f targetDelta=%.2f actor=%s component=%s"),
							*BoneName.ToString(),
							BI->IsInstanceSimulatingPhysics() ? 1 : 0,
							static_cast<int32>(ModifierType),
							BI->GetUnrealWorldVelocity().Size(),
							FMath::RadiansToDegrees(BI->GetUnrealWorldAngularVelocityInRadians()).Size(),
							CalculateBringUpGroupControlAuthorityAlpha(ResolveBringUpGroupIndex(BoneName), EffectiveSettings),
							BoneMaxTargetDelta,
							*GetOwner()->GetName(),
							*GetName());
						
						if (bIsSpikeFail)
						{
							LoggedSpikeThisPhase.FindOrAdd(this) = true;
						}
					}
				}
			}
		}
	}

	BalanceReadyTransition.Tick(DeltaTime, this, EffectiveSettings);

	if (RuntimeState != EPhysAnimRuntimeState::BalanceEntry_RootOn)
	{
		// Reset static maps if needed
	}

	// Authoritative state sync (Section 4)
	{
		EBalanceReadyTransitionPhase TransitionPhase = BalanceReadyTransition.GetPhase();
		if (TransitionPhase == EBalanceReadyTransitionPhase::BRT_Succeeded)
		{
			if (RuntimeState != EPhysAnimRuntimeState::BalanceActive_Recovery)
			{
				CompleteBalanceModeEntry();
			}
			TransitionPhase = BalanceReadyTransition.GetPhase();
		}

		const EPhysAnimRuntimeState MappedRuntimeState =
			TransitionPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare ? EPhysAnimRuntimeState::BalanceEntry_Prepare :
			TransitionPhase == EBalanceReadyTransitionPhase::BRT_Phase1_LateValidate ? EPhysAnimRuntimeState::BalanceEntry_LateValidate :
			TransitionPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn ? EPhysAnimRuntimeState::BalanceEntry_RootOn :
			TransitionPhase == EBalanceReadyTransitionPhase::BRT_Phase3_Settle ? EPhysAnimRuntimeState::BalanceEntry_Settle :
			TransitionPhase == EBalanceReadyTransitionPhase::BRT_SafeDenied ? EPhysAnimRuntimeState::BalanceSafeDeny :
			EPhysAnimRuntimeState::BridgeActive;

		if (TransitionPhase == EBalanceReadyTransitionPhase::BRT_Inactive)
		{
			if (RuntimeState == EPhysAnimRuntimeState::BalanceActive_Recovery)
			{
				// Completion path already published recovery.
			}
			else if (RuntimeState == EPhysAnimRuntimeState::BalanceSafeDeny)
			{
				// Safe deny remains published until the mode is explicitly reset.
			}
			else if (RuntimeState != EPhysAnimRuntimeState::BalanceSafeDeny)
			{
				TransitionRuntimeState(EPhysAnimRuntimeState::BridgeActive);
			}
		}
		else
		{
			TransitionRuntimeState(MappedRuntimeState);
		}
	}

	if (RuntimeState == EPhysAnimRuntimeState::BridgeActive &&
		!bPendingBalanceModeStartRequest &&
		!BalanceReadyTransition.HasActuallyStarted())
	{
		const int32 AutoBalanceTriggerGroupIndex = FMath::Min(1, GetBringUpGroupCount() - 1);
		if (GetHighestUnlockedBringUpGroupIndex() >= AutoBalanceTriggerGroupIndex &&
			GetLastControlTargetDiagnostics().bPolicyInfluenceActive)
		{
			FString AutoBalanceGateReason;
			if (EvaluateBalanceModeQueueGates(EffectiveSettings, AutoBalanceGateReason))
			{
				QueueBalanceModeStartRequest(TEXT("auto_trigger"));
			}
		}
	}

	TryStartPendingBalanceModeRequest(EffectiveSettings);



	if (RuntimeState == EPhysAnimRuntimeState::BalanceActive_Recovery)
	{
		UpdateBalancePerturbation(DeltaTime);
	}

	PhysicsControl->UpdateTargetCaches(DeltaTime);

	if (bRunPolicyUpdateThisTick)
	{
		++PolicyControlTicksExecuted;
		PolicyControlTicksSkipped += FMath::Max(ElapsedPolicySteps - 1, 0);
		LastPolicyControlUpdateTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : BridgeStartTimeSeconds;
		if (bCanTraceFrames)
		{
			const int64 TraceFrameIndex = BridgeTraceTickCounter++;
			bWriteTraceFrameThisTick = ((TraceFrameIndex % TraceSampleEveryNthFrame) == 0);
			if (bWriteTraceFrameThisTick)
			{
				TraceFrame.SessionId = CurrentBridgeTraceSessionId;
				TraceFrame.TraceVersion = 1;
				TraceFrame.FrameIndex = TraceFrameIndex;
				TraceFrame.WorldTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
				TraceFrame.DeltaTimeSeconds = DeltaTime;
				TraceFrame.bSampledPolicyStep = true;
			}
		}

		const double PoseSearchStartSeconds = FPlatformTime::Seconds();
		TRACE_CPUPROFILER_EVENT_SCOPE(PhysAnim_PoseSearchQuery);
		const bool bPoseSearchValid = QueryPoseSearch(SearchResult, TickError);
		TRACE_COUNTER_SET(COUNTER_PhysAnim_PoseSearchQueryMs, static_cast<float>(MeasureElapsedMs(PoseSearchStartSeconds)));
		if (bWriteTraceFrameThisTick)
		{
			TraceFrame.PoseSearchQueryMs = MeasureElapsedMs(PoseSearchStartSeconds);
			TraceFrame.bPoseSearchValid = bPoseSearchValid;
		}

		if (bPoseSearchValid)
		{
			LastValidPoseSearchResult = SearchResult;
			ConsecutiveInvalidPoseSearchFrames = 0;
		}
		else
		{
			++ConsecutiveInvalidPoseSearchFrames;
			if (ConsecutiveInvalidPoseSearchFrames > 1 || LastValidPoseSearchResult.SelectedAnim == nullptr)
			{
				FailStopWithTrace(FString::Printf(TEXT("PoseSearch query was invalid for two consecutive policy ticks. %s"), *TickError));
				return;
			}

			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnim] Reusing last valid PoseSearch result for one grace policy tick. Reason: %s"), *TickError);
			SearchResult = LastValidPoseSearchResult;
		}


		TArray<FPhysAnimBodySample> CurrentBodySamples;
		const double BodySampleStartSeconds = FPlatformTime::Seconds();
		if (!GatherCurrentBodySamples(CurrentBodySamples, TickError))
		{
			if (bWriteTraceFrameThisTick)
			{
				TraceFrame.BodySampleMs = MeasureElapsedMs(BodySampleStartSeconds);
			}
			FailStopWithTrace(TickError);
			return;
		}
		if (bWriteTraceFrameThisTick)
		{
			TraceFrame.BodySampleMs = MeasureElapsedMs(BodySampleStartSeconds);
		}

		TArray<FPhysAnimFuturePoseSample> FuturePoseSamples;
		const double FuturePoseSampleStartSeconds = FPlatformTime::Seconds();
		TRACE_CPUPROFILER_EVENT_SCOPE(PhysAnim_FuturePoseSampling);
		if (!SampleFuturePoses(SearchResult, FuturePoseSamples, TickError))
		{
			if (bWriteTraceFrameThisTick)
			{
				TraceFrame.FuturePoseSampleMs = MeasureElapsedMs(FuturePoseSampleStartSeconds);
			}
			TRACE_COUNTER_SET(COUNTER_PhysAnim_FuturePoseSampleMs, static_cast<float>(MeasureElapsedMs(FuturePoseSampleStartSeconds)));
			FailStopWithTrace(TickError);
			return;
		}
		TRACE_COUNTER_SET(COUNTER_PhysAnim_FuturePoseSampleMs, static_cast<float>(MeasureElapsedMs(FuturePoseSampleStartSeconds)));
		if (bWriteTraceFrameThisTick)
		{
			TraceFrame.FuturePoseSampleMs = MeasureElapsedMs(FuturePoseSampleStartSeconds);
		}

		FVector2D MimicTargetReferenceDataOffsetXY = FVector2D::ZeroVector;
		const double ObservationPackStartSeconds = FPlatformTime::Seconds();
		TRACE_CPUPROFILER_EVENT_SCOPE(PhysAnim_ObservationPack);
		if (!ResolveMimicTargetReferenceDataOffset(SearchResult, MimicTargetReferenceDataOffsetXY, TickError))
		{
			if (bWriteTraceFrameThisTick)
			{
				TraceFrame.ObservationPackMs = MeasureElapsedMs(ObservationPackStartSeconds);
			}
			TRACE_COUNTER_SET(COUNTER_PhysAnim_ObservationPackMs, static_cast<float>(MeasureElapsedMs(ObservationPackStartSeconds)));
			FailStopWithTrace(TickError);
			return;
		}

		const float MimicTargetReferenceGroundHeight = ResolveSelfObservationGroundHeight(CurrentBodySamples);
		if (!PhysAnimBridge::BuildSelfObservation(
			CurrentBodySamples,
			MimicTargetReferenceGroundHeight,
			SelfObservationBuffer,
			TickError))
		{
			if (bWriteTraceFrameThisTick)
			{
				TraceFrame.ObservationPackMs = MeasureElapsedMs(ObservationPackStartSeconds);
			}
			TRACE_COUNTER_SET(COUNTER_PhysAnim_ObservationPackMs, static_cast<float>(MeasureElapsedMs(ObservationPackStartSeconds)));
			FailStopWithTrace(TickError);
			return;
		}

		TArray<FPhysAnimBodySample> MimicCurrentReferenceBodySamples;
		MakeMimicTargetCurrentReferenceBodySamples(
			CurrentBodySamples,
			MimicTargetReferenceDataOffsetXY,
			MimicTargetReferenceGroundHeight,
			MimicCurrentReferenceBodySamples);

		if (!PhysAnimBridge::BuildMimicTargetPoses(MimicCurrentReferenceBodySamples, FuturePoseSamples, MimicTargetPosesBuffer, TickError))
		{
			if (bWriteTraceFrameThisTick)
			{
				TraceFrame.ObservationPackMs = MeasureElapsedMs(ObservationPackStartSeconds);
			}
			TRACE_COUNTER_SET(COUNTER_PhysAnim_ObservationPackMs, static_cast<float>(MeasureElapsedMs(ObservationPackStartSeconds)));
			FailStopWithTrace(TickError);
			return;
		}

		if (!BuildTerrainObservation(CurrentBodySamples, TerrainBuffer, TickError))
		{
			if (bWriteTraceFrameThisTick)
			{
				TraceFrame.ObservationPackMs = MeasureElapsedMs(ObservationPackStartSeconds);
			}
			TRACE_COUNTER_SET(COUNTER_PhysAnim_ObservationPackMs, static_cast<float>(MeasureElapsedMs(ObservationPackStartSeconds)));
			FailStopWithTrace(TickError);
			return;
		}
		TRACE_COUNTER_SET(COUNTER_PhysAnim_ObservationPackMs, static_cast<float>(MeasureElapsedMs(ObservationPackStartSeconds)));
		if (bWriteTraceFrameThisTick)
		{
			const PhysAnimComponentInternal::FFloatBufferSummary SelfObservationSummary =
				PhysAnimComponentInternal::SummarizeFloatBuffer(SelfObservationBuffer);
			const PhysAnimComponentInternal::FFloatBufferSummary MimicTargetPosesSummary =
				PhysAnimComponentInternal::SummarizeFloatBuffer(MimicTargetPosesBuffer);
			const PhysAnimComponentInternal::FFloatBufferSummary TerrainSummary =
				PhysAnimComponentInternal::SummarizeFloatBuffer(TerrainBuffer);
			float MinMimicFutureTimeSeconds = 0.0f;
			float MaxMimicFutureTimeSeconds = 0.0f;
			PhysAnimComponentInternal::ResolveMimicTargetPoseTimeRange(
				MimicTargetPosesBuffer,
				MinMimicFutureTimeSeconds,
				MaxMimicFutureTimeSeconds);

			TraceFrame.ObservationPackMs = MeasureElapsedMs(ObservationPackStartSeconds);
			TraceFrame.SelfObservationRootHeight = SelfObservationBuffer.IsValidIndex(0) ? SelfObservationBuffer[0] : 0.0f;
			TraceFrame.SelfObservationMeanAbs = SelfObservationSummary.MeanAbs;
			TraceFrame.MimicTargetPosesMeanAbs = MimicTargetPosesSummary.MeanAbs;
			TraceFrame.MimicTargetPosesMinFutureTimeSeconds = MinMimicFutureTimeSeconds;
			TraceFrame.MimicTargetPosesMaxFutureTimeSeconds = MaxMimicFutureTimeSeconds;
			TraceFrame.TerrainMean = TerrainSummary.Mean;
			TraceFrame.TerrainMin = TerrainSummary.bHasValues ? TerrainSummary.Min : 0.0f;
			TraceFrame.TerrainMax = TerrainSummary.bHasValues ? TerrainSummary.Max : 0.0f;
			TraceFrame.TerrainCenter = PhysAnimComponentInternal::ResolveTerrainCenterSampleValue(TerrainBuffer);
			TraceFrame.MovementSmokePhaseName = LastMovementSmokePhaseName.ToString();
			TraceFrame.bDistalLocomotionCompositionModeActive = bDistalLocomotionCompositionModeActive;
		}

		const double InferenceStartSeconds = FPlatformTime::Seconds();
		if (!RunInference(TickError))
		{
			if (bWriteTraceFrameThisTick)
			{
				TraceFrame.InferenceMs = MeasureElapsedMs(InferenceStartSeconds);
			}
			FailStopWithTrace(TickError);
			return;
		}
		if (bWriteTraceFrameThisTick)
		{
			TraceFrame.InferenceMs = MeasureElapsedMs(InferenceStartSeconds);
			TraceFrame.bRunSyncSucceeded = true;
		}
	}


	ApplyRuntimeControlTuning(EffectiveSettings);

	bool bPelvisSimAfterTuning = false;
	int32 TotalSimAfterTuning = 0;
	if (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn)
	{
		EPhysicsMovementType PelvisModifierType = EPhysicsMovementType::Kinematic;
		if (UPhysicsControlComponent* const PC = PhysicsControlComponent.Get())
		{
			const FName PelvisModifierName = PhysAnimBridge::MakeBodyModifierName(PhysAnimBridge::GetRootBoneName());
			if (const FPhysicsBodyModifierRecord* Record = FPhysAnimPhysicsControlAccessor::GetModifierRecord(PC, PelvisModifierName))
			{
				PelvisModifierType = Record->BodyModifier.ModifierData.MovementType;
			}
		}

		if (USkeletalMeshComponent* const Mesh = MeshComponent.Get())
		{
			const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
			if (FBodyInstance* const RootBI = Mesh->GetBodyInstance(RootBoneName))
			{
				bPelvisSimAfterTuning = RootBI->IsInstanceSimulatingPhysics();
			}

			for (const FName& BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
			{
				if (FBodyInstance* const BI = Mesh->GetBodyInstance(BoneName))
				{
					if (BI->IsInstanceSimulatingPhysics())
					{
						TotalSimAfterTuning++;
					}
				}
			}
		}

		static TMap<UPhysAnimComponent*, bool> LastPelvisRawSims;
		static TMap<UPhysAnimComponent*, int32> LastTotalSimCounts;
		static TMap<UPhysAnimComponent*, EPhysicsMovementType> LastPelvisModifiers;

		const bool bPelvisRawSimChanged = bLastPelvisRawSim != bPelvisSimAfterTuning;
		const bool bTotalSimCountChanged = LastTotalSimCount != TotalSimAfterTuning;
		const bool bPelvisModifierChanged = !LastPelvisModifiers.Contains(this) || LastPelvisModifiers[this] != PelvisModifierType;

		const bool bIsStableRootOnTick1 = 
			(BalanceEntryRootOnFrameCount == 1) && 
			(!bPelvisSimAfterTuning) && 
			(TotalSimAfterTuning == 5) && 
			(!bPelvisRawSimChanged && !bTotalSimCountChanged && !bPelvisModifierChanged);

		static EPhysAnimRuntimeState LastAuditState = EPhysAnimRuntimeState::Uninitialized;
		const bool bStateChanged = (RuntimeState != LastAuditState) || bPelvisRawSimChanged || bTotalSimCountChanged || bPelvisModifierChanged;

		const bool bIsRootOnTickRange = (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn && BalanceEntryRootOnFrameCount >= 2 && BalanceEntryRootOnFrameCount <= 5);
		bool bShouldLog = bIsRootOnTickRange || bStateChanged || bPelvisRawSimChanged || bTotalSimCountChanged || bPelvisModifierChanged;
		if (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn && BalanceEntryRootOnFrameCount == 1)
		{
			bShouldLog = bPelvisRawSimChanged || bTotalSimCountChanged;
		}
		if (bShouldLog)
		{
			bLastPelvisRawSim = bPelvisSimAfterTuning;
			LastTotalSimCount = TotalSimAfterTuning;
			LastPelvisModifiers.FindOrAdd(this) = PelvisModifierType;
			LastAuditState = RuntimeState;
		}
	}

	if (!PendingBodyModifierCachedResetNames.IsEmpty())
	{
		ResetPendingBodyModifiersToCachedTargets();


	}

	// Forensic distal stale-record check (Task: Fix stale modifier record)
	if (BalanceReadyTransition.IsDistalKinematicAccepted())
	{
		for (const FName BoneName : {TEXT("calf_l"), TEXT("calf_r"), TEXT("foot_l"), TEXT("foot_r"), TEXT("ball_l"), TEXT("ball_r")})
		{
			if (SkeletalMesh && PhysicsControl)
			{
				const FName ModifierName = PhysAnimBridge::MakeBodyModifierName(BoneName);
				if (const FPhysicsBodyModifierRecord* Record = FPhysAnimPhysicsControlAccessor::GetModifierRecord(PhysicsControl, ModifierName))
				{
					const EPhysicsMovementType ModifierType = Record->BodyModifier.ModifierData.MovementType;
					FBodyInstance* const BodyInstance = SkeletalMesh->GetBodyInstance(BoneName);
					const bool bRawSimulating = BodyInstance ? BodyInstance->IsInstanceSimulatingPhysics() : false;
					
					if (ModifierType == EPhysicsMovementType::Simulated && !bRawSimulating)
					{
						static TMap<FName, bool> LoggedStaleOnce;
						if (!LoggedStaleOnce.Contains(BoneName))
						{
							UE_LOG(LogPhysAnimBridge, Warning, TEXT("DISTAL_MODIFIER_RECORD_STALE bone=%s intended=Kinematic rawBody=Kinematic modifier=Simulated runtimeState=%s phase=%d"),
								*BoneName.ToString(),
								GetRuntimeStateName(RuntimeState),
								(int32)BalanceReadyTransition.GetPhase());
							LoggedStaleOnce.Add(BoneName, true);
						}
					}
				}
			}
		}
	}

	if (bRunPolicyUpdateThisTick)
	{
		const double ActionConditionStartSeconds = FPlatformTime::Seconds();
		if (!ConditionModelActions(EffectiveSettings, TickError))
		{
			if (bWriteTraceFrameThisTick)
			{
				TraceFrame.ActionConditionMs = MeasureElapsedMs(ActionConditionStartSeconds);
			}
			FailStopWithTrace(TickError);
			return;
		}
		if (bWriteTraceFrameThisTick)
		{
			TraceFrame.ActionConditionMs = MeasureElapsedMs(ActionConditionStartSeconds);
		}
	}

	const double ControlTargetStartSeconds = FPlatformTime::Seconds();
	TRACE_CPUPROFILER_EVENT_SCOPE(PhysAnim_ControlWrites);
	ApplyControlTargets(
		bRunPolicyUpdateThisTick
			? (PolicyControlIntervalSeconds * FMath::Max(ElapsedPolicySteps, 1))
			: 0.0f,
		EffectiveSettings,
		bRunPolicyUpdateThisTick,
		TickError);


	if (bWriteTraceFrameThisTick)
	{
		TraceFrame.ControlTargetMs = MeasureElapsedMs(ControlTargetStartSeconds);
	}
	TRACE_COUNTER_SET(COUNTER_PhysAnim_ControlWritesMs, static_cast<float>(MeasureElapsedMs(ControlTargetStartSeconds)));
	if (!TickError.IsEmpty())
	{
		FailStopWithTrace(TickError);
		return;
	}




	const double UpdateControlsStartSeconds = FPlatformTime::Seconds();

	bool bSkipUpdateControls = false;
	bool bPreTrackedValueChanged = false;
	bool bCurrentPelvisSim = false;
	int32 CurrentTotalSim = 0;
	EPhysicsMovementType CurrentPelvisModifierType = EPhysicsMovementType::Static;

	if (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn)
	{
		if (const UPhysicsControlComponent* const PC = PhysicsControlComponent.Get())
		{
			const FName PelvisModifierName = PhysAnimBridge::MakeBodyModifierName(PhysAnimBridge::GetRootBoneName());
			if (const FPhysicsBodyModifierRecord* Record = FPhysAnimPhysicsControlAccessor::GetModifierRecord(PC, PelvisModifierName))
			{
				CurrentPelvisModifierType = Record->BodyModifier.ModifierData.MovementType;
			}
		}

		if (const USkeletalMeshComponent* const Mesh = GetMeshComponent())
		{
			const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
			if (const FBodyInstance* const RootBI = Mesh->GetBodyInstance(RootBoneName))
			{
				bCurrentPelvisSim = RootBI->IsInstanceSimulatingPhysics();
			}

			for (const FName& BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
			{
				if (const FBodyInstance* const BI = Mesh->GetBodyInstance(BoneName))
				{
					if (BI->IsInstanceSimulatingPhysics())
					{
						CurrentTotalSim++;
					}
				}
			}
		}

		static TMap<UPhysAnimComponent*, EPhysicsMovementType> LastPrePelvisModifiers;

		const bool bPrePelvisRawSimChanged = bLastPelvisRawSim != bCurrentPelvisSim;
		const bool bPreTotalSimCountChanged = LastTotalSimCount != CurrentTotalSim;
		const bool bPrePelvisModifierChanged = !LastPrePelvisModifiers.Contains(this) || LastPrePelvisModifiers[this] != CurrentPelvisModifierType;
		bPreTrackedValueChanged = bPrePelvisRawSimChanged || bPreTotalSimCountChanged || bPrePelvisModifierChanged;

		const bool bShouldEmitPreAudit = (BalanceEntryRootOnFrameCount == 1) || (BalanceEntryRootOnFrameCount == 2) || bPreTrackedValueChanged;
		const bool bSuppressLogOnSkip = (bSkipUpdateControls && !bPreTrackedValueChanged);

		if (bShouldEmitPreAudit && !bSuppressLogOnSkip)
		{
			bLastPelvisRawSim = bCurrentPelvisSim;
			LastTotalSimCount = CurrentTotalSim;
			LastPrePelvisModifiers.FindOrAdd(this) = CurrentPelvisModifierType;
		}
	}
	else if (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle)
	{
		// Settle auditing and tracking
		if (const UPhysicsControlComponent* const PC = PhysicsControlComponent.Get())
		{
			const FName PelvisModifierName = PhysAnimBridge::MakeBodyModifierName(PhysAnimBridge::GetRootBoneName());
			if (const FPhysicsBodyModifierRecord* Record = FPhysAnimPhysicsControlAccessor::GetModifierRecord(PC, PelvisModifierName))
			{
				CurrentPelvisModifierType = Record->BodyModifier.ModifierData.MovementType;
			}
		}

		if (const USkeletalMeshComponent* const Mesh = GetMeshComponent())
		{
			const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
			if (const FBodyInstance* const RootBI = Mesh->GetBodyInstance(RootBoneName))
			{
				bCurrentPelvisSim = RootBI->IsInstanceSimulatingPhysics();
			}

			for (const FName& BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
			{
				if (const FBodyInstance* const BI = Mesh->GetBodyInstance(BoneName))
				{
					if (BI->IsInstanceSimulatingPhysics())
					{
						CurrentTotalSim++;
					}
				}
			}
		}

		static TMap<UPhysAnimComponent*, EPhysicsMovementType> LastPrePelvisModifiers;
		const bool bPrePelvisRawSimChanged = bLastPelvisRawSim != bCurrentPelvisSim;
		const bool bPreTotalSimCountChanged = LastTotalSimCount != CurrentTotalSim;
		const bool bPrePelvisModifierChanged = !LastPrePelvisModifiers.Contains(this) || LastPrePelvisModifiers[this] != CurrentPelvisModifierType;
		bPreTrackedValueChanged = bPrePelvisRawSimChanged || bPreTotalSimCountChanged || bPrePelvisModifierChanged;
	}

	const bool bIsRealRootOnTick4 = 
		(RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn) && 
		(BalanceEntryRootOnFrameCount == 4);

	if (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn)
	{
		if (bIsRealRootOnTick4 && !bPhase2Tick4AuditArmed)
		{
			bPhase2Tick4AuditArmed = true;
		}

		if (bIsRealRootOnTick4)
		{
			static uint64 LastLoggedTick4SummaryFrame = 0;
			if (LastLoggedTick4SummaryFrame != GFrameCounter)
			{
				for (const FName& BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
				{
					if (USkeletalMeshComponent* const Mesh = MeshComponent.Get())
					{
						if (FBodyInstance* const BI = Mesh->GetBodyInstance(BoneName))
						{
							EPhysicsMovementType ModifierType = EPhysicsMovementType::Static;
							if (UPhysicsControlComponent* const PC = PhysicsControlComponent.Get())
							{
								const FName ModifierName = PhysAnimBridge::MakeBodyModifierName(BoneName);
								if (const FPhysicsBodyModifierRecord* Record = FPhysAnimPhysicsControlAccessor::GetModifierRecord(PC, ModifierName))
								{
									ModifierType = Record->BodyModifier.ModifierData.MovementType;
								}
							}

							const bool bIsPreservedProximal = 
								(BoneName == TEXT("thigh_l") || BoneName == TEXT("thigh_r") ||
								 BoneName == TEXT("spine_01") || BoneName == TEXT("spine_02") || BoneName == TEXT("spine_03"));
							const bool bIsPelvis = (BoneName == PhysAnimBridge::GetRootBoneName());
							const int32 RawSimBit = BI->IsInstanceSimulatingPhysics() ? 1 : 0;

							const bool bIsRequiredSummaryBone = bIsPelvis || bIsPreservedProximal;

							if (bIsRequiredSummaryBone)
							{
								UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnim] PHASE2_TICK4_REQUIRED_BONE_SUMMARY bone=%s rawSim=%d modifierName=%s linSpeed=%.1f angSpeed=%.1f counted=%d"),
									*BoneName.ToString(),
									RawSimBit,
									GetPhysicsMovementTypeName(ModifierType),
									BI->GetUnrealWorldVelocity().Size(),
									FMath::RadiansToDegrees(BI->GetUnrealWorldAngularVelocityInRadians()).Size(),
									RawSimBit);
							}
						}
					}
				}
				LastLoggedTick4SummaryFrame = GFrameCounter;
			}
		}
	}

	if (bIsRealRootOnTick4)
	{
		const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
		const FName PelvisModifierName = PhysAnimBridge::MakeBodyModifierName(RootBoneName);
		EPhysicsMovementType PelvisModType = EPhysicsMovementType::Static;
		if (UPhysicsControlComponent* const PC = PhysicsControlComponent.Get())
		{
			if (const FPhysicsBodyModifierRecord* Record = FPhysAnimPhysicsControlAccessor::GetModifierRecord(PC, PelvisModifierName))
			{
				PelvisModType = Record->BodyModifier.ModifierData.MovementType;
			}
		}

		USkeletalMeshComponent* const Mesh = MeshComponent.Get();
		FBodyInstance* const RootBI = Mesh ? Mesh->GetBodyInstance(RootBoneName) : nullptr;
		const int32 PelvisRawSim = (RootBI && RootBI->IsInstanceSimulatingPhysics()) ? 1 : 0;
		int32 CurTotalSim = 0;
		if (Mesh)
		{
			for (const FName& BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
			{
				if (FBodyInstance* const BI = Mesh->GetBodyInstance(BoneName))
				{
					if (BI->IsInstanceSimulatingPhysics())
					{
						CurTotalSim++;
					}
				}
			}
		}

		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnim] PHASE2_TICK4_PRE_UPDATECONTROLS_STATE frame=%llu tick=%d pelvisRawSim=%d totalSimCount=%d pelvisModifierName=%s owner=%s actor=%s component=%s"),
			GFrameCounter,
			static_cast<int32>(BalanceEntryRootOnFrameCount),
			PelvisRawSim,
			CurTotalSim,
			GetPhysicsMovementTypeName(PelvisModType),
			*GetOwner()->GetName(),
			*GetOwner()->GetName(),
			*GetName());
	}

	TraceSpineTick2(TEXT("pre_updatecontrols"));
	TracePreservedTick4(TEXT("pre_updatecontrols_tick4"));
	PhysicsControl->UpdateControls(DeltaTime);
	TraceSpineTick2(TEXT("post_updatecontrols"));
	TracePreservedTick4(TEXT("post_updatecontrols_tick4"));
	TraceUpperBodyTick4(TEXT("post_updatecontrols_tick4"));
	ApplyPhase1PelvisRootCouplingSolve();

	if (bIsRealRootOnTick4)
	{
		bCurrentPelvisSim = false;
		CurrentTotalSim = 0;
		CurrentPelvisModifierType = EPhysicsMovementType::Static;

		if (UPhysicsControlComponent* const PC = PhysicsControlComponent.Get())
		{
			const FName PelvisModifierName = PhysAnimBridge::MakeBodyModifierName(PhysAnimBridge::GetRootBoneName());
			if (const FPhysicsBodyModifierRecord* Record = FPhysAnimPhysicsControlAccessor::GetModifierRecord(PC, PelvisModifierName))
			{
				CurrentPelvisModifierType = Record->BodyModifier.ModifierData.MovementType;
			}
		}

		if (USkeletalMeshComponent* const Mesh = MeshComponent.Get())
		{
			const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
			if (FBodyInstance* const RootBI = Mesh->GetBodyInstance(RootBoneName))
			{
				bCurrentPelvisSim = RootBI->IsInstanceSimulatingPhysics();
			}

			for (const FName& BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
			{
				if (FBodyInstance* const BI = Mesh->GetBodyInstance(BoneName))
				{
					if (BI->IsInstanceSimulatingPhysics())
					{
						CurrentTotalSim++;
					}
				}
			}
		}

		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnim] PHASE2_TICK4_POST_UPDATECONTROLS_STATE frame=%llu tick=%d pelvisRawSim=%d totalSimCount=%d pelvisModifierName=%s owner=%s actor=%s component=%s"),
			GFrameCounter,
			static_cast<int32>(BalanceEntryRootOnFrameCount),
			bCurrentPelvisSim ? 1 : 0,
			CurrentTotalSim,
			GetPhysicsMovementTypeName(CurrentPelvisModifierType),
			*GetOwner()->GetName(),
			*GetOwner()->GetName(),
			*GetName());

		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnim] PHASE2_TICK4_POST_GUARD_STATE frame=%llu tick=%d rootRawSim=%d pelvisRawSim=%d mod=%s simCount=%d pendingResetsEmpty=%d owner=%s actor=%s component=%s"),
			GFrameCounter,
			(int32)BalanceEntryRootOnFrameCount,
			bCurrentPelvisSim ? 1 : 0,
			bCurrentPelvisSim ? 1 : 0,
			GetPhysicsMovementTypeName(CurrentPelvisModifierType),
			CurrentTotalSim,
			PendingBodyModifierCachedResetNames.IsEmpty() ? 1 : 0,
			*GetOwner()->GetName(),
			*GetOwner()->GetName(),
			*GetName());
	}

	bool bPostTrackedValueChanged = false;
	bool bPostCurrentPelvisSim = false;
	int32 PostCurrentTotalSim = 0;
	EPhysicsMovementType PostCurrentPelvisModifierType = EPhysicsMovementType::Kinematic;
	struct FRootStateTransitionSnapshot
	{
		bool bPelvisRawSim = false;
		EPhysicsMovementType PelvisModifier = EPhysicsMovementType::Static;
		int32 TotalSimCount = 0;
	};
	static TMap<UPhysAnimComponent*, FRootStateTransitionSnapshot> LastRootStateTransitionSnapshots;
	auto ReadRootStateTransitionSnapshot = [this, PhysicsControl, SkeletalMesh]()
	{
		FRootStateTransitionSnapshot Snapshot;

		if (UPhysicsControlComponent* const PC = PhysicsControl ? PhysicsControl : PhysicsControlComponent.Get())
		{
			const FName PelvisModifierName = PhysAnimBridge::MakeBodyModifierName(PhysAnimBridge::GetRootBoneName());
			if (const FPhysicsBodyModifierRecord* Record = FPhysAnimPhysicsControlAccessor::GetModifierRecord(PC, PelvisModifierName))
			{
				Snapshot.PelvisModifier = Record->BodyModifier.ModifierData.MovementType;
			}
		}

		if (USkeletalMeshComponent* const Mesh = SkeletalMesh ? SkeletalMesh : MeshComponent.Get())
		{
			const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
			if (FBodyInstance* const RootBI = Mesh->GetBodyInstance(RootBoneName))
			{
				Snapshot.bPelvisRawSim = RootBI->IsInstanceSimulatingPhysics();
			}

			for (const FName& BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
			{
				if (FBodyInstance* const BI = Mesh->GetBodyInstance(BoneName))
				{
					if (BI->IsInstanceSimulatingPhysics())
					{
						++Snapshot.TotalSimCount;
					}
				}
			}
		}

		return Snapshot;
	};
	auto EmitRootStateTransitionTrace = [&](const TCHAR* Source, const FRootStateTransitionSnapshot& Snapshot)
	{
		FRootStateTransitionSnapshot& LastSnapshot = LastRootStateTransitionSnapshots.FindOrAdd(this);
		const bool bChanged =
			Snapshot.bPelvisRawSim != LastSnapshot.bPelvisRawSim ||
			Snapshot.PelvisModifier != LastSnapshot.PelvisModifier ||
			Snapshot.TotalSimCount != LastSnapshot.TotalSimCount;
		if (!bChanged)
		{
			return;
		}

		LastSnapshot = Snapshot;
		if (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn && BalanceReadyTransition.GetDiagnostics().FirstContradictionSource.IsEmpty())
		{
			BalanceReadyTransition.GetDiagnostics().FirstContradictionSource = Source;
		}
	};

	const bool bIsRootOnTransitionTick = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn;

	if (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn)
	{
		
		if (UPhysicsControlComponent* const PC = PhysicsControlComponent.Get())
		{
			const FName PelvisModifierName = PhysAnimBridge::MakeBodyModifierName(PhysAnimBridge::GetRootBoneName());
			if (const FPhysicsBodyModifierRecord* Record = FPhysAnimPhysicsControlAccessor::GetModifierRecord(PC, PelvisModifierName))
			{
				PostCurrentPelvisModifierType = Record->BodyModifier.ModifierData.MovementType;
			}
		}

		if (USkeletalMeshComponent* const Mesh = MeshComponent.Get())
		{
			const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
			if (FBodyInstance* const RootBI = Mesh->GetBodyInstance(RootBoneName))
			{
				bPostCurrentPelvisSim = RootBI->IsInstanceSimulatingPhysics();
			}

			for (const FName& BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
			{
				if (FBodyInstance* const BI = Mesh->GetBodyInstance(BoneName))
				{
					if (BI->IsInstanceSimulatingPhysics())
					{
						PostCurrentTotalSim++;
					}
				}
			}
		}

		static TMap<UPhysAnimComponent*, bool> LastPostPelvisRawSims;
		static TMap<UPhysAnimComponent*, int32> LastPostTotalSimCounts;
		static TMap<UPhysAnimComponent*, EPhysicsMovementType> LastPostPelvisModifiers;

		const bool bPostPelvisRawSimChanged = !LastPostPelvisRawSims.Contains(this) || LastPostPelvisRawSims[this] != bPostCurrentPelvisSim;
		const bool bPostTotalSimCountChanged = !LastPostTotalSimCounts.Contains(this) || LastPostTotalSimCounts[this] != PostCurrentTotalSim;
		const bool bPostPelvisModifierChanged = !LastPostPelvisModifiers.Contains(this) || LastPostPelvisModifiers[this] != PostCurrentPelvisModifierType;
		bPostTrackedValueChanged = bPostPelvisRawSimChanged || bPostTotalSimCountChanged || bPostPelvisModifierChanged;
		
		const bool bShouldEmitPostAudit = (BalanceEntryRootOnFrameCount == 1) || (BalanceEntryRootOnFrameCount == 2) || (BalanceEntryRootOnFrameCount == 4) || bPostTrackedValueChanged;
		const bool bSuppressLogOnSkip = (bSkipUpdateControls && !bPostTrackedValueChanged) ||
			((BalanceEntryRootOnFrameCount == 1 || BalanceEntryRootOnFrameCount == 2 || BalanceEntryRootOnFrameCount == 4) && bSkipUpdateControls && RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn) ||
			(RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle);

		if (bShouldEmitPostAudit && !bSuppressLogOnSkip)
		{
			LastPostPelvisRawSims.FindOrAdd(this) = bPostCurrentPelvisSim;
			LastPostTotalSimCounts.FindOrAdd(this) = PostCurrentTotalSim;
			LastPostPelvisModifiers.FindOrAdd(this) = PostCurrentPelvisModifierType;
		}
	}

	if (bIsRootOnTransitionTick && BalanceEntryRootOnFrameCount == 1)
	{
		BalanceReadyTransition.GetDiagnostics().FirstContradictionSource.Reset();
		LastRootStateTransitionSnapshots.Remove(this);
	}

	TRACE_COUNTER_SET(COUNTER_PhysAnim_UpdateControlsMs, static_cast<float>(MeasureElapsedMs(UpdateControlsStartSeconds)));
	if (bIsRootOnTransitionTick)
	{
		FString GranularSource = FString::Printf(TEXT("pre_updatecontrols_tick%d"), (int32)BalanceEntryRootOnFrameCount);
		EmitRootStateTransitionTrace(*GranularSource, ReadRootStateTransitionSnapshot());
	}
	if (bWriteTraceFrameThisTick)
	{
		TraceFrame.UpdateControlsMs = MeasureElapsedMs(UpdateControlsStartSeconds);
		TraceFrame.bUpdateControlsSucceeded = true;
	}
	const double InstabilityCheckStartSeconds = FPlatformTime::Seconds();
	if (!CheckRuntimeInstability(DeltaTime, EffectiveSettings, TickError))
	{
		if (bWriteTraceFrameThisTick)
		{
			TraceFrame.InstabilityCheckMs = MeasureElapsedMs(InstabilityCheckStartSeconds);
		}
		FailStopWithTrace(TickError);
		return;
	}
	if (bWriteTraceFrameThisTick)
	{
		TraceFrame.InstabilityCheckMs = MeasureElapsedMs(InstabilityCheckStartSeconds);
	}
	TRACE_COUNTER_SET(COUNTER_PhysAnim_MaxBodyAngularSpeedDegPerSec, LastRuntimeInstabilityDiagnostics.MaxBodyAngularSpeedDegPerSecond);
	TRACE_COUNTER_SET(COUNTER_PhysAnim_MaxLowerLimbLimitOccupancy, LastControlTargetDiagnostics.MaxLowerLimbLimitOccupancy);
	TRACE_COUNTER_SET(COUNTER_PhysAnim_NumNormalPolicyTargetsWritten, LastControlTargetDiagnostics.NumNormalPolicyTargetsWritten);
	TRACE_COUNTER_SET(COUNTER_PhysAnim_NumHeldTargetsWritten, LastControlTargetDiagnostics.NumHeldTargetsWritten);
	TRACE_COUNTER_SET(COUNTER_PhysAnim_NumTotalTargetsWritten, LastControlTargetDiagnostics.NumTotalTargetsWritten);
	if (bIsRootOnTransitionTick)
	{
		FString GranularSource = FString::Printf(TEXT("post_updatecontrols_tick%d"), (int32)BalanceEntryRootOnFrameCount);
		EmitRootStateTransitionTrace(*GranularSource, ReadRootStateTransitionSnapshot());
	}

	if (bSimulationHandoffCompletedThisTick)
	{
		LogBridgeStateSnapshot(TEXT("AfterSimulationHandoff"));
		LogBodyModifierTelemetrySnapshot(TEXT("AfterSimulationHandoff"));
		LogActivationSummary(EffectiveSettings, TEXT("SimulationHandoffComplete"), true, true, SimulationHandoffAlpha);
		EmitBridgeTraceEvent(TEXT("simulation_handoff_complete"), TEXT("Simulation handoff completed and bridge-owned physics is fully active."));
	}
	MaybeLogRuntimeDiagnostics(EffectiveSettings);

	// Populate and push Phase 1 convergence snapshot for authoritative gating in next frame's transition controller tick
	if (BalanceReadyTransition.IsActive())
	{
		SafePhase1ConvergenceSnapshot.FrameIndex = GFrameCounter;
		SafePhase1ConvergenceSnapshot.WorldTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		SafePhase1ConvergenceSnapshot.MaxBodyLinearSpeed = LastRuntimeInstabilityDiagnostics.MaxBodyLinearSpeedCmPerSecond;
		SafePhase1ConvergenceSnapshot.MaxBodyAngularSpeed = LastRuntimeInstabilityDiagnostics.MaxBodyAngularSpeedDegPerSecond;
		SafePhase1ConvergenceSnapshot.MaxBodyLinearSpeedBone = LastRuntimeInstabilityDiagnostics.MaxLinearSpeedBoneName;
		SafePhase1ConvergenceSnapshot.MaxBodyAngularSpeedBone = LastRuntimeInstabilityDiagnostics.MaxAngularSpeedBoneName;
		SafePhase1ConvergenceSnapshot.RootLinearSpeed = LastRuntimeInstabilityDiagnostics.RootLinearSpeedCmPerSecond;
		SafePhase1ConvergenceSnapshot.RootAngularSpeed = LastRuntimeInstabilityDiagnostics.RootAngularSpeedDegPerSecond;
		
		FString TiltSource;
		const float ResolvedTilt = ResolvePhase1Uprightness(GetMeshComponent(), GetOwner(), PhysAnimBridge::GetRootBoneName(), TiltSource);
		SafePhase1ConvergenceSnapshot.RootTilt = ResolvedTilt;

		if (!bPhase1TiltDiagnosticEmitted)
		{
			bPhase1TiltDiagnosticEmitted = true;
			const FQuat MeshRootQuat = GetMeshComponent() ? GetMeshComponent()->GetBoneQuaternion(PhysAnimBridge::GetRootBoneName()) : FQuat::Identity;
			const FVector MeshRootX = MeshRootQuat.GetAxisX();
			const FVector MeshRootY = MeshRootQuat.GetAxisY();
			const FVector MeshRootZ = MeshRootQuat.GetAxisZ();
			const FVector ActorUp = GetOwner() ? GetOwner()->GetActorUpVector() : FVector::UpVector;
			const FVector MeshCompUp = GetMeshComponent() ? GetMeshComponent()->GetUpVector() : FVector::UpVector;
			const FRotator LocalMeshRot = GetMeshComponent() ? GetMeshComponent()->GetRelativeRotation() : FRotator::ZeroRotator;

			UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnim] Phase 1 Tilt Diagnostic: rootTiltDeg=%.2f tiltSource=%s pelvisBodyValid=%s pelvisSimulating=%s MeshX=(%.2f,%.2f,%.2f) MeshY=(%.2f,%.2f,%.2f) MeshZ=(%.2f,%.2f,%.2f) actorUp=(%.2f,%.2f,%.2f) meshCompUp=(%.2f,%.2f,%.2f) localMeshRot=(%.2f,%.2f,%.2f)"),
				ResolvedTilt, *TiltSource,
				(GetMeshComponent() && GetMeshComponent()->GetBodyInstance(PhysAnimBridge::GetRootBoneName())) ? TEXT("true") : TEXT("false"),
				IsPelvisSimulatingNow() ? TEXT("true") : TEXT("false"),
				MeshRootX.X, MeshRootX.Y, MeshRootX.Z,
				MeshRootY.X, MeshRootY.Y, MeshRootY.Z,
				MeshRootZ.X, MeshRootZ.Y, MeshRootZ.Z,
				ActorUp.X, ActorUp.Y, ActorUp.Z,
				MeshCompUp.X, MeshCompUp.Y, MeshCompUp.Z,
				LocalMeshRot.Pitch, LocalMeshRot.Yaw, LocalMeshRot.Roll);
		}
		
		SafePhase1ConvergenceSnapshot.ShellPlanarOffset = GetCurrentShellPlanarOffsetDeltaCm();
		SafePhase1ConvergenceSnapshot.ShellPlanarVelocity = GetCurrentShellPlanarVelocityDeltaCmPerSecond();
		SafePhase1ConvergenceSnapshot.bIsInstabilityPrecursorActive = IsInstabilityPrecursorActive();
		SafePhase1ConvergenceSnapshot.bIsPelvisSimulating = IsPelvisSimulatingNow();
		SafePhase1ConvergenceSnapshot.bHasPendingResets = PendingBodyModifierCachedResetNames.Num() > 0;
		SafePhase1ConvergenceSnapshot.MaxTargetDeltaDegrees = LastControlTargetDiagnostics.MaxTargetDeltaDegrees;
		SafePhase1ConvergenceSnapshot.MeanTargetDeltaDegrees = LastControlTargetDiagnostics.MeanTargetDeltaDegrees;

		BalanceReadyTransition.PushConvergenceSnapshot(SafePhase1ConvergenceSnapshot);
	}
	else
	{
		bPhase1TiltDiagnosticEmitted = false;
	}

	FinalizeTraceFrame();
	TraceSpineTick2(TEXT("end_of_tick"));
	TracePreservedTick4(TEXT("end_of_tick_tick4"));
	TraceUpperBodyTick4(TEXT("end_of_tick_tick4"));

	if (bIsRealRootOnTick4)
	{
		const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
		USkeletalMeshComponent* const Mesh = MeshComponent.Get();
		FBodyInstance* const RootBI = Mesh ? Mesh->GetBodyInstance(RootBoneName) : nullptr;
		const int32 PelvisRawSim_Readback = (RootBI && RootBI->IsInstanceSimulatingPhysics()) ? 1 : 0;
		int32 TotalSimCount_Readback = 0;
		if (Mesh)
		{
			for (const FName& BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
			{
				if (FBodyInstance* const BI = Mesh->GetBodyInstance(BoneName))
				{
					if (BI->IsInstanceSimulatingPhysics())
					{
						TotalSimCount_Readback++;
					}
				}
			}
		}
		EPhysicsMovementType PelvisModType_Readback = EPhysicsMovementType::Static;
		if (UPhysicsControlComponent* const PC = PhysicsControlComponent.Get())
		{
			const FName PelvisModifierName = PhysAnimBridge::MakeBodyModifierName(RootBoneName);
			if (const FPhysicsBodyModifierRecord* Record = FPhysAnimPhysicsControlAccessor::GetModifierRecord(PC, PelvisModifierName))
			{
				PelvisModType_Readback = Record->BodyModifier.ModifierData.MovementType;
			}
		}

		if (!bSkipUpdateControls || bPostTrackedValueChanged)
		{
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnim] PHASE2_TICK4_END_OF_TICK_STATE pelvisRawSim=%d totalSimCount=%d pelvisModifier=%s pendingResetsEmpty=%d runtimeState=%s"),
				PelvisRawSim_Readback,
				TotalSimCount_Readback,
				GetPhysicsMovementTypeName(PelvisModType_Readback),
				PendingBodyModifierCachedResetNames.IsEmpty() ? 1 : 0,
				GetRuntimeStateName(RuntimeState));
		}
	}

	if (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle)
	{
		const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
		USkeletalMeshComponent* const Mesh = MeshComponent.Get();
		FBodyInstance* const RootBI = Mesh ? Mesh->GetBodyInstance(RootBoneName) : nullptr;
		const bool bRootRawSim = RootBI && RootBI->IsInstanceSimulatingPhysics();
		const bool bPelvisRawSim = bRootRawSim;

		BalanceReadyTransition.UpdateSettleContinuityState(bRootRawSim, bPelvisRawSim);
	}
	else if (RuntimeState != EPhysAnimRuntimeState::BalanceEntry_RootOn)
	{
		BalanceReadyTransition.UpdateSettleContinuityState(false, false);
	}

	if (bIsRootOnTransitionTick)
	{
		FString GranularSource = FString::Printf(TEXT("end_of_tick_tick%d"), (int32)BalanceEntryRootOnFrameCount);
		EmitRootStateTransitionTrace(*GranularSource, ReadRootStateTransitionSnapshot());
	}
}


void UPhysAnimComponent::ResetStabilizationRuntimeState()
{
	ConditionedActionBuffer.Reset();
	PreviousConditionedActionBuffer.Reset();
	PreviousControlTargetRotations.Reset();
	LastActionDiagnostics = {};
	LastControlTargetDiagnostics = {};
	RuntimeInstabilityState = {};
	LastRuntimeInstabilityDiagnostics = {};
	SimulationHandoffAlpha = 0.0f;
	bLastAppliedSimulationHandoffSettled = false;
	LastAppliedControlAuthorityAlpha = -1.0f;
	BridgeStartTimeSeconds = 0.0;
	SimulationHandoffCompletedTimeSeconds = -1.0;
	PolicyInfluenceRampStartTimeSeconds = -1.0;
	SetStartupBringUpFrozenByBalanceEntry(false, TEXT("reset_stabilization_runtime"));
	HighestUnlockedBringUpGroupIndex = INDEX_NONE;
	BringUpGroupStableAccumulatedSeconds = 0.0f;
	BringUpGroupActivationTimeSeconds.Init(-1.0, GetBringUpGroupCount());
	BringUpGroupControlRampStartTimeSeconds.Init(-1.0, GetBringUpGroupCount());
	PendingBodyModifierCachedResetNames.Reset();
	LastRuntimeDiagnosticsLogTimeSeconds = -1.0;
	PolicyUpdateAccumulatorSeconds = -1.0f;
	LastPolicyElapsedSteps = 0;
	PolicyControlTicksExecuted = 0;
	PolicyControlTicksSkipped = 0;
	LastPolicyControlUpdateTimeSeconds = -1.0;
	bDistalLocomotionCompositionModeActive = false;
	DistalLocomotionCompositionTimeAboveEnterSeconds = 0.0f;
	DistalLocomotionCompositionTimeBelowExitSeconds = 0.0f;
	DistalLocomotionCompositionTimeSinceActiveIntentSeconds = -1.0f;
	LastMovementSmokeLocalIntent = FVector::ZeroVector;
	LastMovementSmokeWorldIntent = FVector::ZeroVector;
	LastMovementSmokeOwnerVelocityCmPerSecond = FVector::ZeroVector;
	ResetBridgeLocomotionAuthorityState();
	LastBridgePoseSearchDeltaTimeSeconds = 1.0f / 30.0f;
	LastBridgePoseSearchTrajectoryLogTimeSeconds = -1.0;
	BridgePoseSearchLatchedWalkResult = FPoseSearchBlueprintResult();
	BridgePoseSearchLatchedQueryDirection = FVector::ZeroVector;
	BridgePoseSearchLatchedQuerySpeedCmPerSecond = 0.0f;
	BridgePoseSearchWalkLatchExpireTimeSeconds = -1.0;
	bHasBridgePoseSearchLatchedWalkResult = false;
	bBridgePoseSearchTrajectoryInitialized = false;
	MovementSmokeStartLocation = FVector::ZeroVector;
	ShellCouplingReferenceRootLocalOffsetCm = FVector::ZeroVector;
	LastMovementSmokePhaseName = NAME_None;
	bMovementSmokeScriptStarted = false;
	bMovementSmokeCompletionLogged = false;
	bHasShellCouplingReferenceRootLocalOffset = false;
	PresentationPerturbationOverrideEndTimeSeconds = -1.0;
	bLastAppliedPresentationRootSimulationEnabled = false;
	StabilizationStressTestStartTimeSeconds = -1.0;
	bStabilizationStressTestCompletionLogged = false;
	StabilizationStressTestFirstAngularSpikeTimeSeconds = -1.0;
	StabilizationStressTestFirstLinearSpikeTimeSeconds = -1.0;
	StabilizationStressTestFirstInstabilitySignTimeSeconds = -1.0;
	StabilizationStressTestFirstAngularSpikeMultiplier = 1.0f;
	StabilizationStressTestFirstLinearSpikeMultiplier = 1.0f;
	StabilizationStressTestFirstInstabilityMultiplier = 1.0f;
	StabilizationStressTestFirstAngularSpikeBoneName = NAME_None;
	StabilizationStressTestFirstLinearSpikeBoneName = NAME_None;
	StabilizationStressTestBaselineActorLocation = FVector::ZeroVector;
	StabilizationStressTestBaselineSpineLocalOffset = FVector::ZeroVector;
	StabilizationStressTestBaselineHeadLocalOffset = FVector::ZeroVector;
	StabilizationStressTestBaselineLeftFootLocalOffset = FVector::ZeroVector;
	StabilizationStressTestBaselineRightFootLocalOffset = FVector::ZeroVector;
	OriginalBodyMassScales.Reset();
	bHasSavedBodyMassScales = false;
	PolicyBlendStartControlTargetRotations.Reset();
	bPolicyTargetsAppliedLastFrame = false;
	bPolicyInfluenceRampReanchoredOnFirstPolicyEnabledFrame = false;
	LastAppliedStabilizationSettings = {};
}


void UPhysAnimComponent::FailStop(const FString& Reason)
{
	TRACE_BOOKMARK(TEXT("PhysAnim FailStop: %s"), *Reason);
	TRACE_COUNTER_ADD(COUNTER_PhysAnim_FailStopCount, 1);

	if (PhysAnimComponentInternal::CVarPaStabilizationStressTest.GetValueOnGameThread() > 0 &&
		StabilizationStressTestStartTimeSeconds >= 0.0)
	{
		const double CurrentTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : StabilizationStressTestStartTimeSeconds;
		const double ElapsedSinceStartSeconds = CurrentTimeSeconds - StabilizationStressTestStartTimeSeconds;
		const double CascadeSeconds =
			(StabilizationStressTestFirstInstabilitySignTimeSeconds >= 0.0)
				? (CurrentTimeSeconds - StabilizationStressTestFirstInstabilitySignTimeSeconds)
				: -1.0;
		UE_LOG(
			LogPhysAnimBridge,
			Error,
			TEXT("[PhysAnim] Stabilization stress-test collapse: multiplier=%.2f elapsed=%.2fs firstAngularSpike=%s:%.2f firstLinearSpike=%s:%.2f firstInstability=%.2f onsetToCollapse=%.2fs"),
			ResolveStabilizationStressTestMultiplier(),
			ElapsedSinceStartSeconds,
			*StabilizationStressTestFirstAngularSpikeBoneName.ToString(),
			StabilizationStressTestFirstAngularSpikeMultiplier,
			*StabilizationStressTestFirstLinearSpikeBoneName.ToString(),
			StabilizationStressTestFirstLinearSpikeMultiplier,
			StabilizationStressTestFirstInstabilityMultiplier,
			CascadeSeconds);
	}

	LogBridgeStateSnapshot(TEXT("FailStop"));
	UE_LOG(LogPhysAnimBridge, Error, TEXT("[PhysAnim] Fail-stop: %s"), *Reason);
	EmitBridgeTraceEvent(TEXT("fail_stop"), TEXT("Bridge entered fail-stop."), Reason);
	DeactivateRuntimePhysicsControl(TEXT("FailStop"));
	ResetBridgePhysicsState();
	TransitionRuntimeState(EPhysAnimRuntimeState::FailStopped);
	StopBridgeTraceSession(TEXT("FailStop"), TEXT("Bridge trace session stopped after fail-stop."));
	SetComponentTickEnabled(false);
	ResetStabilizationRuntimeState();
}


void UPhysAnimComponent::SetStartupBringUpFrozenByBalanceEntry(bool bFrozen, const FString& InReason)
{
	if (bStartupBringUpFrozenByBalanceEntry == bFrozen)
	{
		return;
	}

	bStartupBringUpFrozenByBalanceEntry = bFrozen;

	if (bFrozen)
	{
		UE_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("PHASE1_FREEZE_ACQUIRE reason=%s"),
			*InReason);
	}
	else
	{
		UE_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("PHASE1_FREEZE_RELEASE reason=%s phase=%s"),
			*InReason,
			GetRuntimeStateName(RuntimeState));
	}
}


void UPhysAnimComponent::TransitionRuntimeState(EPhysAnimRuntimeState NewState)
{
	if (RuntimeState == NewState)
	{
		return;
	}

	const TCHAR* const PreviousStateName = GetRuntimeStateName(RuntimeState);
	const TCHAR* const NewStateName = GetRuntimeStateName(NewState);

	// Smoke-facing contract filtering: Phase 1/2 is Pending, Phase 3 is LateValidate.
	UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnim] State Transition: %s -> %s"), PreviousStateName, NewStateName);

	RuntimeState = NewState;
	EmitBridgeTraceEvent(
		TEXT("runtime_state_transition"),
		FString::Printf(TEXT("Runtime state: %s -> %s"), PreviousStateName, NewStateName),
		FString(),
		PreviousStateName,
		NewStateName);

	if (IsBalanceEntryState(NewState))
	{
		if (NewState == EPhysAnimRuntimeState::BalanceEntry_RootOn)
		{
			BalanceEntryRootOnFrameCount = 0;
			bPhase2Tick4AuditArmed = false;
		}
		if (NewState == EPhysAnimRuntimeState::BalanceEntry_Settle)
		{
			BalanceEntrySettleFrameCount = 0;
		}

		// Phase 2 states (RootOn/Settle) will receive their tuning via the normal per-tick path in TickComponent.
		// Phase 1 states (Prepare/LateValidate) still require the eager publishing here.
		if (NewState != EPhysAnimRuntimeState::BalanceEntry_RootOn && NewState != EPhysAnimRuntimeState::BalanceEntry_Settle)
		{
			ApplyRuntimeControlTuning(ResolveEffectiveStabilizationSettings());
		}
	}

	if (NewState == EPhysAnimRuntimeState::BalanceEntry_Prepare)
	{
		bPhase2Tick4AuditArmed = false;
		ReconcilePhase1DistalModifierRecords(ResolveEffectiveStabilizationSettings());
	}

	UpdateBridgeStatusIndicator(60.0f);
}

