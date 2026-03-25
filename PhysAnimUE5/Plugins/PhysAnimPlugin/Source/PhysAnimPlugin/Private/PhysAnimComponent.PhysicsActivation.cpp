#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"

bool UPhysAnimComponent::ActivateBridgeFromReadyState(
	const FPhysAnimStabilizationSettings& EffectiveSettings,
	const TCHAR* ActivationContext,
	FString& OutError)
{
	UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	if (!PhysicsControl)
	{
		OutError = TEXT("Bridge activation requires a valid Physics Control component.");
		return false;
	}

	if (!ActivateRuntimePhysicsControl(OutError))
	{
		return false;
	}

	TransitionRuntimeState(EPhysAnimRuntimeState::BridgeActive);
	LogBridgeStateSnapshot(TEXT("BeforeActivateBridgePhysicsState"));
	ActivateBridgePhysicsState(EffectiveSettings);
	LogBridgeStateSnapshot(TEXT("AfterActivateBridgePhysicsState"));
	ResetStabilizationRuntimeState();
	BridgeStartTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (const AActor* const OwnerActor = GetOwner())
	{
		MovementSmokeStartLocation = OwnerActor->GetActorLocation();
	}
	SimulationHandoffAlpha = CalculateSimulationHandoffAlpha(EffectiveSettings);
	PrewarmPhysicsControlActivationPose();
	PhysicsControl->UpdateTargetCaches(0.0f);
	if (!SeedControlTargetsFromCurrentPose(0.0f, OutError))
	{
		return false;
	}
	ApplyRuntimeControlTuning(EffectiveSettings);
	PhysicsControl->UpdateControls(0.0f);
	LogBridgeStateSnapshot(TEXT("AfterActivationPrepass"));
	LogActivationSummary(EffectiveSettings, ActivationContext, true, true, SimulationHandoffAlpha);

	UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnim] Bridge physics activation[%s] complete."), ActivationContext);
	return true;
}


bool UPhysAnimComponent::PrewarmPhysicsControlActivationPose()
{
	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!ShouldPrewarmPhysicsControlActivationPose(
		SkeletalMesh != nullptr,
		SkeletalMesh && SkeletalMesh->LeaderPoseComponent.IsValid()))
	{
		return false;
	}

	SkeletalMesh->TickAnimation(0.0f, false);
	SkeletalMesh->RefreshBoneTransforms();
	UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnim] Prewarmed skeletal pose for PhysicsControl activation cache."));
	return true;
}


void UPhysAnimComponent::EnterReadyForActivation(
	const FPhysAnimStabilizationSettings& EffectiveSettings,
	const TCHAR* Context,
	bool bLogDeferredStartupSuccess)
{
	ResetStabilizationRuntimeState();
	TransitionRuntimeState(EPhysAnimRuntimeState::ReadyForActivation);
	LogBridgeStateSnapshot(Context);

	if (bLogDeferredStartupSuccess)
	{
		UE_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnim] Startup success. Runtime=%s Model=%s DeferredActivation=true"),
			*ActiveRuntimeName,
			*GetPathNameSafe(LoadedModelData));
		UE_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnim] Bridge physics activation deferred by zero-action safe mode."));
		UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnim] Stabilization %s"), *PhysAnimComponentInternal::BuildStabilizationSummary(EffectiveSettings));
	}
}


bool UPhysAnimComponent::GatherCurrentPoseControlTargetOrientations(TMap<FName, FQuat>& OutTargetOrientations, FString& OutError) const
{
	OutTargetOrientations.Reset();
	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	AActor* const OwnerActor = GetOwner();
	if (!SkeletalMesh || !PhysicsControl || !OwnerActor)
	{
		OutError = TEXT("Current-pose target gathering requires the owner, skeletal mesh component, and Physics Control component.");
		return false;
	}

	auto FindInitialControl = [OwnerActor](const FName ControlName) -> const FInitialPhysicsControl*
	{
		if (const UPhysAnimStage1InitializerComponent* const Stage1Initializer = OwnerActor->FindComponentByClass<UPhysAnimStage1InitializerComponent>())
		{
			return Stage1Initializer->InitialControls.Find(ControlName);
		}

		if (const UPhysicsControlInitializerComponent* const Initializer = OwnerActor->FindComponentByClass<UPhysicsControlInitializerComponent>())
		{
			return Initializer->InitialControls.Find(ControlName);
		}

		return nullptr;
	};

	for (const FName BoneName : PhysAnimBridge::GetControlledBoneNames())
	{
		const FName ControlName = PhysAnimBridge::MakeControlName(BoneName);
		const FInitialPhysicsControl* const InitialControl = FindInitialControl(ControlName);
		if (!InitialControl)
		{
			OutError = FString::Printf(TEXT("Missing authored control definition for '%s' while gathering current-pose targets."), *ControlName.ToString());
			return false;
		}

		if (SkeletalMesh->GetBoneIndex(InitialControl->ParentBoneName) == INDEX_NONE ||
			SkeletalMesh->GetBoneIndex(InitialControl->ChildBoneName) == INDEX_NONE)
		{
			OutError = FString::Printf(
				TEXT("Could not resolve current-pose bones for control '%s' (parent='%s', child='%s')."),
				*ControlName.ToString(),
				*InitialControl->ParentBoneName.ToString(),
				*InitialControl->ChildBoneName.ToString());
			return false;
		}

		const FQuat ParentWorldRotation = PhysicsControl->GetCachedBoneOrientation(SkeletalMesh, InitialControl->ParentBoneName).Quaternion();
		const FQuat ChildWorldRotation = PhysicsControl->GetCachedBoneOrientation(SkeletalMesh, InitialControl->ChildBoneName).Quaternion();
		OutTargetOrientations.Add(
			ControlName,
			BuildCurrentPoseControlTargetOrientation(ParentWorldRotation, ChildWorldRotation));
	}

	return true;
}


bool UPhysAnimComponent::SeedControlTargetsFromCurrentPose(float DeltaTime, FString& OutError)
{
	UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	if (!PhysicsControl)
	{
		OutError = TEXT("Current-pose target seeding requires the Physics Control component.");
		return false;
	}

	TMap<FName, FQuat> CurrentPoseTargetOrientations;
	if (!GatherCurrentPoseControlTargetOrientations(CurrentPoseTargetOrientations, OutError))
	{
		return false;
	}

	PreviousControlTargetRotations.Reset();
	PolicyBlendStartControlTargetRotations.Reset();

	for (const TPair<FName, FQuat>& Pair : CurrentPoseTargetOrientations)
	{
		if (!PhysicsControl->GetControlExists(Pair.Key))
		{
			OutError = FString::Printf(TEXT("Missing required control '%s' while seeding current-pose targets."), *Pair.Key.ToString());
			return false;
		}

		PreviousControlTargetRotations.Add(Pair.Key, Pair.Value);
		PolicyBlendStartControlTargetRotations.Add(Pair.Key, Pair.Value);
		PhysicsControl->SetControlTargetOrientation(
			Pair.Key,
			Pair.Value.Rotator(),
			DeltaTime,
			true,
			false,
			true,
			false);
	}

	return true;
}

