#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"

float UPhysAnimComponent::GetCurrentShellPlanarOffsetDeltaCm() const
{
	const AActor* const OwnerActor = GetOwner();
	const USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!OwnerActor || !SkeletalMesh)
	{
		return 0.0f;
	}

	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	const FVector OwnerLocation = OwnerActor->GetActorLocation();
	const FVector RootLocation = SkeletalMesh->GetBoneLocation(RootBoneName, EBoneSpaces::WorldSpace);
	FVector ReferenceRootLocalOffset = ShellCouplingReferenceRootLocalOffsetCm;
	if (!bHasShellCouplingReferenceRootLocalOffset)
	{
		ReferenceRootLocalOffset = RootLocation - OwnerLocation;
	}

	return ResolveShellCouplingPlanarOffsetDeltaCm(
		OwnerLocation,
		RootLocation,
		ReferenceRootLocalOffset);
}


float UPhysAnimComponent::GetCurrentShellPlanarVelocityDeltaCmPerSecond() const
{
	const AActor* const OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return 0.0f;
	}

	const FVector EffectiveOwnerVelocityCmPerSecond = ResolveEffectiveShellCouplingPlanarVelocityCmPerSecond(
		OwnerActor->GetVelocity(),
		BridgeShellState.AppliedPlanarCorrectionVelocityCmPerSecond,
		HasExplicitTransitionOwnedShellLock());
	return ResolveShellCouplingPlanarVelocityDeltaCmPerSecond(
		EffectiveOwnerVelocityCmPerSecond,
		LastRuntimeInstabilityDiagnostics.RawRootLinearVelocityCmPerSecondVector);
}


bool UPhysAnimComponent::IsTransitionOwnedShellLocked() const
{
	return (BalanceTransitionShellAuthorityMode == EBalanceTransitionShellAuthorityMode::TransitionOwnedShellLocked) ||
		IsBalanceEntryState(RuntimeState) || 
		IsBalanceActiveState(RuntimeState);
}


void UPhysAnimComponent::ReanchorShellCouplingReferenceToCurrentRoot(const TCHAR* Source)
{
	const AActor* const OwnerActor = GetOwner();
	const USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!OwnerActor || !SkeletalMesh)
	{
		return;
	}

	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	ShellCouplingReferenceRootLocalOffsetCm =
		SkeletalMesh->GetBoneLocation(RootBoneName, EBoneSpaces::WorldSpace) - OwnerActor->GetActorLocation();
	bHasShellCouplingReferenceRootLocalOffset = true;
	if (BalanceTransitionShellAuthorityMode == EBalanceTransitionShellAuthorityMode::TransitionOwnedShellLocked)
	{
		if (bTransitionOwnedShellReferenceReanchored)
		{
			bTransitionOwnedShellReferenceReseededAfterLock = true;
		}
		bTransitionOwnedShellReferenceReanchored = true;
	}

	UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_SHELL_REANCHOR_EVENT source=%s locked=%d reanchored=%d reseeded=%d"),
		Source ? Source : TEXT("unknown"),
		IsTransitionOwnedShellLocked() ? 1 : 0,
		bTransitionOwnedShellReferenceReanchored ? 1 : 0,
		bTransitionOwnedShellReferenceReseededAfterLock ? 1 : 0);
}


void UPhysAnimComponent::ActivateTransitionOwnedShellLock()
{
	if (BalanceTransitionShellAuthorityMode == EBalanceTransitionShellAuthorityMode::TransitionOwnedShellLocked)
	{
		return;
	}

	BalanceTransitionShellAuthorityMode = EBalanceTransitionShellAuthorityMode::TransitionOwnedShellLocked;
	bTransitionOwnedShellReferenceReanchored = false;
	bTransitionOwnedShellReferenceReseededAfterLock = false;
	ApplyTransitionOwnedShellLock();
	ResetBridgeLocomotionAuthorityState();
	ReanchorShellCouplingReferenceToCurrentRoot(TEXT("activate_lock"));
}


void UPhysAnimComponent::ReleaseTransitionOwnedShellLock()
{
	if (BalanceTransitionShellAuthorityMode != EBalanceTransitionShellAuthorityMode::TransitionOwnedShellLocked)
	{
		return;
	}

	ReleaseTransitionOwnedShellLockInternal(!IsBalanceActiveState(RuntimeState));
	BalanceTransitionShellAuthorityMode = EBalanceTransitionShellAuthorityMode::GameplayShellObservedOnly;
	bTransitionOwnedShellReferenceReanchored = false;
	bTransitionOwnedShellReferenceReseededAfterLock = false;

	UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_SHELL_REANCHOR_EVENT source=release_lock locked=0 reanchored=0 reseeded=0"));
}




void UPhysAnimComponent::ApplyTransitionOwnedShellLock()
{
	// NOTE: Phase 1 transition does NOT drop the shell. It keeps the character movement locked 
	// (input neutralized) and capsule collision enabled.
	// Section 10: "This ensures the character can still be influenced by GamePlayShell logic (e.g. falling, moving platforms)."
	
	ApplyStartupMovementLock();
	
	// BUT, if we have a capsule, RE-ENABLE its collision for Phase 1.
	// It will be explicitly dropped later via CommitTransitionOwnedShellDrop.
	if (ACharacter* const CharacterOwner = Cast<ACharacter>(GetOwner()))
	{
		if (UCapsuleComponent* const CapsuleComponent = CharacterOwner->GetCapsuleComponent())
		{
			if (bHasSavedCapsuleCollisionState)
			{
				CapsuleComponent->SetCollisionEnabled(OriginalCapsuleCollisionEnabled);
			}
		}
		
		// Also ensure CharacterMovement can still perform its physics simulation (falling, etc.)
		if (UCharacterMovementComponent* const CharacterMovement = CharacterOwner->GetCharacterMovement())
		{
			CharacterMovement->SetComponentTickEnabled(true);
			// We keep it in MOVE_None or similar to prevent input? 
			// ApplyStartupMovementLock sets it to MOVE_None.
		}
	}
	
	bStartupMovementLockActive = true;
}


void UPhysAnimComponent::CommitTransitionOwnedShellDrop()
{
	if (!IsTransitionOwnedShellLocked())
	{
		return;
	}

	if (ACharacter* const CharacterOwner = Cast<ACharacter>(GetOwner()))
	{
		if (UCapsuleComponent* const CapsuleComponent = CharacterOwner->GetCapsuleComponent())
		{
			if (!bHasSavedCapsuleCollisionState)
			{
				OriginalCapsuleCollisionEnabled = CapsuleComponent->GetCollisionEnabled();
				bHasSavedCapsuleCollisionState = true;
			}

			CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		
		if (UCharacterMovementComponent* const CharacterMovement = CharacterOwner->GetCharacterMovement())
		{
			CharacterMovement->Velocity = FVector::ZeroVector;
			CharacterMovement->SetComponentTickEnabled(false);
			CharacterMovement->DisableMovement();
		}
	}
	
	// Phase 2 Entry Audit (One-Shot)
	static int32 LastLoggedShellDropFrame = -1;
	const int32 CurrentFrame = static_cast<int32>(GFrameNumber);
	if (LastLoggedShellDropFrame != CurrentFrame)
	{
		const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
		const FName ModifierName = PhysAnimBridge::MakeBodyModifierName(RootBoneName);
		EPhysicsMovementType RecordMovement = EPhysicsMovementType::Static;
		if (UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get())
		{
			if (const FPhysicsBodyModifierRecord* Record = FPhysAnimPhysicsControlAccessor::GetModifierRecord(PhysicsControl, ModifierName))
			{
				RecordMovement = Record->BodyModifier.ModifierData.MovementType;
			}
		}

		bool bRawSimulating = false;
		if (USkeletalMeshComponent* const Mesh = GetMeshComponent())
		{
			if (const FBodyInstance* const PelvisBody = Mesh->GetBodyInstance(RootBoneName))
			{
				bRawSimulating = PelvisBody->IsInstanceSimulatingPhysics();
			}
		}

		UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnimBalance] PHASE2_SHELL_DROP_AUDIT frame=%d bone=%s recordMovement=%s actualSim=%d"), 
			CurrentFrame, *RootBoneName.ToString(), GetPhysicsMovementTypeName(RecordMovement), bRawSimulating ? 1 : 0);
		LastLoggedShellDropFrame = CurrentFrame;
	}

	UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnimBalance] Shell explicitly dropped for transition maturation."));
}


void UPhysAnimComponent::MaintainTransitionOwnedShellLock(float DeltaTime)
{
	BridgeShellState.AppliedPlanarCorrectionVelocityCmPerSecond = FVector::ZeroVector;

	if (!IsTransitionOwnedShellLocked())
	{
		return;
	}

	AActor* const OwnerActor = GetOwner();
	const USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!OwnerActor || !SkeletalMesh || !bHasShellCouplingReferenceRootLocalOffset)
	{
		return;
	}

	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	const FVector RootLocation = SkeletalMesh->GetBoneLocation(RootBoneName, EBoneSpaces::WorldSpace);
	const FVector OwnerLocation = OwnerActor->GetActorLocation();
	const FVector DesiredLocation(
		RootLocation.X - ShellCouplingReferenceRootLocalOffsetCm.X,
		RootLocation.Y - ShellCouplingReferenceRootLocalOffsetCm.Y,
		OwnerLocation.Z);
	const FVector PlanarDelta = DesiredLocation - OwnerLocation;
	if (DeltaTime > UE_SMALL_NUMBER)
	{
		BridgeShellState.AppliedPlanarCorrectionVelocityCmPerSecond = FVector(
			PlanarDelta.X / DeltaTime,
			PlanarDelta.Y / DeltaTime,
			0.0f);
	}

	if (PlanarDelta.SizeSquared2D() <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	OwnerActor->SetActorLocation(DesiredLocation, false, nullptr, ETeleportType::TeleportPhysics);
}


void UPhysAnimComponent::ReleaseTransitionOwnedShellLockInternal(bool bRestoreCharacterMovement)
{
	ReleaseStartupMovementLock(bRestoreCharacterMovement);
}

