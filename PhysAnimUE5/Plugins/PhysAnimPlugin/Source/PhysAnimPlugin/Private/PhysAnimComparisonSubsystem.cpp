#include "PhysAnimComparisonSubsystem.h"

#include "PhysAnimComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraActor.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PhysicsControlComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RotationMatrix.h"
#include "Engine/OverlapResult.h"

namespace
{
	const TCHAR* const PhysAnimG2LogPrefix = TEXT("[PhysAnimG2]");
	const TCHAR* const KinematicRoleName = TEXT("Kinematic");
	const TCHAR* const PhysicsDrivenRoleName = TEXT("Physics-Driven");
	const TCHAR* const PresentationCameraActorName = TEXT("PhysAnimG2Camera");
	constexpr float PresentationPerturbationLeadInSeconds = 1.0f;
	constexpr float PresentationPerturbationObserveSeconds = 3.0f;
	constexpr float PresentationPerturbationDurationSeconds =
		PresentationPerturbationLeadInSeconds + PresentationPerturbationObserveSeconds;
	constexpr float PresentationPerturbationTelemetryDurationSeconds = 2.0f;
	constexpr float PresentationPerturbationTelemetryIntervalSeconds = 0.25f;
	constexpr float PresentationIdleDurationSeconds = 3.0f;
	constexpr float PresentationWalkDurationSeconds = 5.0f;
	constexpr float PresentationJogDurationSeconds = 4.0f;
	constexpr float PresentationBrakeDurationSeconds = 2.0f;
	constexpr float PresentationTurnDurationSeconds = 4.0f;
	constexpr float PresentationRecoveryDurationSeconds = 3.0f;
	constexpr float PresentationDurationSeconds =
		PresentationPerturbationDurationSeconds +
		PresentationIdleDurationSeconds +
		PresentationWalkDurationSeconds +
		PresentationJogDurationSeconds +
		PresentationBrakeDurationSeconds +
		PresentationTurnDurationSeconds +
		PresentationRecoveryDurationSeconds;

	UWorld* ResolveComparisonWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				return Context.World();
			}
		}

		return nullptr;
	}

	void HandleStartSideBySideCommand()
	{
		if (UWorld* const World = ResolveComparisonWorld())
		{
			if (UPhysAnimComparisonSubsystem* const ComparisonSubsystem = World->GetSubsystem<UPhysAnimComparisonSubsystem>())
			{
				FString Error;
				if (!ComparisonSubsystem->StartSideBySide(Error))
				{
					UE_LOG(LogTemp, Error, TEXT("%s Start failed: %s"), PhysAnimG2LogPrefix, *Error);
				}
				return;
			}
		}

		UE_LOG(LogTemp, Error, TEXT("%s Start failed: no active PIE/game world."), PhysAnimG2LogPrefix);
	}

	void HandleStartPresentationCommand()
	{
		if (UWorld* const World = ResolveComparisonWorld())
		{
			if (UPhysAnimComparisonSubsystem* const ComparisonSubsystem = World->GetSubsystem<UPhysAnimComparisonSubsystem>())
			{
				FString Error;
				if (!ComparisonSubsystem->StartPresentation(Error))
				{
					UE_LOG(LogTemp, Error, TEXT("%s Presentation start failed: %s"), PhysAnimG2LogPrefix, *Error);
				}
				return;
			}
		}

		UE_LOG(LogTemp, Error, TEXT("%s Presentation start failed: no active PIE/game world."), PhysAnimG2LogPrefix);
	}

	void HandleStopSideBySideCommand()
	{
		if (UWorld* const World = ResolveComparisonWorld())
		{
			if (UPhysAnimComparisonSubsystem* const ComparisonSubsystem = World->GetSubsystem<UPhysAnimComparisonSubsystem>())
			{
				ComparisonSubsystem->StopSideBySide();
				return;
			}
		}

		UE_LOG(LogTemp, Warning, TEXT("%s Stop ignored: no active PIE/game world."), PhysAnimG2LogPrefix);
	}

	static FAutoConsoleCommand GPhysAnimStartSideBySideCommand(
		TEXT("PhysAnim.G2.StartSideBySide"),
		TEXT("Starts the PIE side-by-side G2 comparison harness with the current player Manny as physics-driven and a spawned mirror Manny as kinematic baseline."),
		FConsoleCommandDelegate::CreateStatic(&HandleStartSideBySideCommand));

	static FAutoConsoleCommand GPhysAnimStartPresentationCommand(
		TEXT("PhysAnim.G2.StartPresentation"),
		TEXT("Starts the scripted PIE G2 presentation harness with a fixed camera and synchronized physics-driven versus kinematic playback."),
		FConsoleCommandDelegate::CreateStatic(&HandleStartPresentationCommand));

	static FAutoConsoleCommand GPhysAnimStopSideBySideCommand(
		TEXT("PhysAnim.G2.StopSideBySide"),
		TEXT("Stops the PIE side-by-side G2 comparison harness and restores the original player Manny."),
		FConsoleCommandDelegate::CreateStatic(&HandleStopSideBySideCommand));
}

bool UPhysAnimComparisonSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::PIE || WorldType == EWorldType::Game;
}

void UPhysAnimComparisonSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bComparisonActive)
	{
		return;
	}

	ACharacter* const SourceCharacter = SourcePhysicsCharacter.Get();
	ACharacter* const KinematicCharacter = SpawnedKinematicCharacter.Get();
	if (!SourceCharacter || !KinematicCharacter)
	{
		StopSideBySide();
		return;
	}

	if (bPresentationModeActive)
	{
		TickPresentationComparison(*SourceCharacter, *KinematicCharacter);
		UpdatePresentationCamera();
	}
	else
	{
		TickManualComparison(*SourceCharacter, *KinematicCharacter);
	}

	TickComparisonLabels();
}

TStatId UPhysAnimComparisonSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UPhysAnimComparisonSubsystem, STATGROUP_Tickables);
}

void UPhysAnimComparisonSubsystem::Deinitialize()
{
	StopSideBySide();
	Super::Deinitialize();
}

bool UPhysAnimComparisonSubsystem::StartSideBySide(FString& OutError)
{
	return StartComparison(false, OutError);
}

bool UPhysAnimComparisonSubsystem::StartPresentation(FString& OutError)
{
	return StartComparison(true, OutError);
}

bool UPhysAnimComparisonSubsystem::StartComparison(bool bEnablePresentationMode, FString& OutError)
{
	OutError.Reset();

	if (bComparisonActive)
	{
		StopSideBySide();
	}

	UWorld* const World = GetWorld();
	if (!World)
	{
		OutError = TEXT("Side-by-side comparison requires a valid world.");
		return false;
	}

	ACharacter* const SourceCharacter = UGameplayStatics::GetPlayerCharacter(World, 0);
	if (!SourceCharacter)
	{
		OutError = TEXT("Side-by-side comparison requires the current player pawn to be an ACharacter.");
		return false;
	}

	SourcePhysicsCharacter = SourceCharacter;
	SourceOriginalTransform = SourceCharacter->GetActorTransform();
	bHasSavedSourceOriginalTransform = true;
	ComparisonAnchorRotation = SourceCharacter->GetActorRotation();

	const FVector ComparisonCenter = SourceCharacter->GetActorLocation() - ResolveComparisonSpawnOffset(true, LateralSeparationCm);
	const FVector PhysicsLocation = ComparisonCenter + ResolveComparisonSpawnOffset(true, LateralSeparationCm);
	const FVector KinematicLocation = ComparisonCenter + ResolveComparisonSpawnOffset(false, LateralSeparationCm);
	const FRotator SpawnRotation = SourceCharacter->GetActorRotation();

	SourceCharacter->SetActorLocation(PhysicsLocation, false, nullptr, ETeleportType::TeleportPhysics);

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.Name = MakeUniqueObjectName(World, SourceCharacter->GetClass(), TEXT("BP_G2_KinematicBaseline"));
	ACharacter* const KinematicCharacter = World->SpawnActor<ACharacter>(
		SourceCharacter->GetClass(),
		FTransform(SpawnRotation, KinematicLocation, FVector::OneVector),
		SpawnParameters);
	if (!KinematicCharacter)
	{
		RestoreSourcePhysicsCharacter();
		OutError = TEXT("Failed to spawn the kinematic comparison Manny.");
		return false;
	}

	SpawnedKinematicCharacter = KinematicCharacter;

	if (!ConfigureSourcePhysicsCharacter(*SourceCharacter, OutError))
	{
		StopSideBySide();
		return false;
	}

	if (!ConfigureKinematicBaselineCharacter(*KinematicCharacter, OutError))
	{
		StopSideBySide();
		return false;
	}

	if (bEnablePresentationMode && !ActivatePresentationMode(OutError))
	{
		StopSideBySide();
		return false;
	}

	bComparisonActive = true;
	UE_LOG(
		LogTemp,
		Log,
		TEXT("%s Started %s comparison. physics=%s kinematic=%s separationCm=%.1f"),
		PhysAnimG2LogPrefix,
		bEnablePresentationMode ? TEXT("scripted G2 presentation") : TEXT("live side-by-side"),
		*GetNameSafe(SourceCharacter),
		*GetNameSafe(KinematicCharacter),
		LateralSeparationCm);
	return true;
}

void UPhysAnimComparisonSubsystem::StopSideBySide()
{
	if (ACharacter* const KinematicCharacter = SpawnedKinematicCharacter.Get())
	{
		KinematicCharacter->Destroy();
	}
	SpawnedKinematicCharacter.Reset();

	RestorePresentationMode();
	RestoreSourcePhysicsCharacter();
	bComparisonActive = false;
}

bool UPhysAnimComparisonSubsystem::IsSideBySideActive() const
{
	return bComparisonActive;
}

FString UPhysAnimComparisonSubsystem::ResolveComparisonRoleLabel(bool bPhysicsDriven)
{
	return bPhysicsDriven ? PhysicsDrivenRoleName : KinematicRoleName;
}

FVector UPhysAnimComparisonSubsystem::ResolveComparisonSpawnOffset(bool bPhysicsDriven, float LateralSeparationCm)
{
	const float HalfSeparation = FMath::Max(LateralSeparationCm, 0.0f) * 0.5f;
	return FVector(0.0f, bPhysicsDriven ? HalfSeparation : -HalfSeparation, 0.0f);
}

FVector UPhysAnimComparisonSubsystem::ResolveMirroredWorldInput(const FVector& PendingWorldInput, const FVector& LastWorldInput)
{
	return !PendingWorldInput.IsNearlyZero() ? PendingWorldInput : LastWorldInput;
}

FVector UPhysAnimComparisonSubsystem::ResolvePresentationLocalIntent(float ElapsedSeconds)
{
	if (ElapsedSeconds < PresentationPerturbationDurationSeconds)
	{
		return FVector::ZeroVector; // Changed from Walk to Idle
	}

	ElapsedSeconds -= PresentationPerturbationDurationSeconds;

	if (ElapsedSeconds < PresentationIdleDurationSeconds)
	{
		return FVector::ZeroVector;
	}

	if (ElapsedSeconds < (PresentationIdleDurationSeconds + PresentationWalkDurationSeconds))
	{
		return FVector(1.0f, 0.0f, 0.0f);
	}

	if (ElapsedSeconds < (PresentationIdleDurationSeconds + PresentationWalkDurationSeconds + PresentationJogDurationSeconds))
	{
		return FVector(1.0f, 0.0f, 0.0f);
	}

	if (ElapsedSeconds < (PresentationIdleDurationSeconds + PresentationWalkDurationSeconds + PresentationJogDurationSeconds + PresentationBrakeDurationSeconds))
	{
		return FVector::ZeroVector;
	}

	if (ElapsedSeconds < (PresentationIdleDurationSeconds + PresentationWalkDurationSeconds + PresentationJogDurationSeconds + PresentationBrakeDurationSeconds + PresentationTurnDurationSeconds))
	{
		return FVector(0.7f, 0.7f, 0.0f);
	}

	if (ElapsedSeconds < PresentationDurationSeconds)
	{
		return FVector::ZeroVector;
	}

	return FVector::ZeroVector;
}

float UPhysAnimComparisonSubsystem::ResolvePresentationInputScale(float ElapsedSeconds)
{
	if (ElapsedSeconds < PresentationPerturbationDurationSeconds)
	{
		return 0.0f; // Changed from 0.32 to 0.0
	}

	ElapsedSeconds -= PresentationPerturbationDurationSeconds;

	if (ElapsedSeconds < PresentationIdleDurationSeconds)
	{
		return 0.0f;
	}

	if (ElapsedSeconds < (PresentationIdleDurationSeconds + PresentationWalkDurationSeconds))
	{
		return 0.45f;
	}

	if (ElapsedSeconds < (PresentationIdleDurationSeconds + PresentationWalkDurationSeconds + PresentationJogDurationSeconds))
	{
		return 1.0f;
	}

	if (ElapsedSeconds < (PresentationIdleDurationSeconds + PresentationWalkDurationSeconds + PresentationJogDurationSeconds + PresentationBrakeDurationSeconds))
	{
		return 0.0f;
	}

	if (ElapsedSeconds < (PresentationIdleDurationSeconds + PresentationWalkDurationSeconds + PresentationJogDurationSeconds + PresentationBrakeDurationSeconds + PresentationTurnDurationSeconds))
	{
		return 1.0f;
	}

	if (ElapsedSeconds < PresentationDurationSeconds)
	{
		return 0.0f;
	}

	return 0.0f;
}

FName UPhysAnimComparisonSubsystem::ResolvePresentationPhaseName(float ElapsedSeconds)
{
	if (ElapsedSeconds < PresentationPerturbationDurationSeconds)
	{
		return TEXT("IdlePerturbation"); // Was WalkPerturbation
	}

	ElapsedSeconds -= PresentationPerturbationDurationSeconds;

	if (ElapsedSeconds < PresentationIdleDurationSeconds)
	{
		return TEXT("IdleReady");
	}

	if (ElapsedSeconds < (PresentationIdleDurationSeconds + PresentationWalkDurationSeconds))
	{
		return TEXT("WalkForward");
	}

	if (ElapsedSeconds < (PresentationIdleDurationSeconds + PresentationWalkDurationSeconds + PresentationJogDurationSeconds))
	{
		return TEXT("JogForward");
	}

	if (ElapsedSeconds < (PresentationIdleDurationSeconds + PresentationWalkDurationSeconds + PresentationJogDurationSeconds + PresentationBrakeDurationSeconds))
	{
		return TEXT("BrakeStop");
	}

	if (ElapsedSeconds < (PresentationIdleDurationSeconds + PresentationWalkDurationSeconds + PresentationJogDurationSeconds + PresentationBrakeDurationSeconds + PresentationTurnDurationSeconds))
	{
		return TEXT("TurnPivot");
	}

	if (ElapsedSeconds < PresentationDurationSeconds)
	{
		return TEXT("Recovery");
	}

	return TEXT("Complete");
}

bool UPhysAnimComparisonSubsystem::ShouldApplyPresentationPerturbation(float ElapsedSeconds)
{
	return ElapsedSeconds >= PresentationPerturbationLeadInSeconds;
}

float UPhysAnimComparisonSubsystem::GetPresentationDurationSeconds()
{
	return PresentationDurationSeconds;
}

FVector UPhysAnimComparisonSubsystem::ResolvePresentationCameraOffsetCm(bool bPerturbationPhase)
{
	return bPerturbationPhase
		? FVector(235.0f, 0.0f, 138.0f)
		: FVector(325.0f, 0.0f, 170.0f);
}

FVector UPhysAnimComparisonSubsystem::ResolvePresentationPusherHalfExtentCm()
{
	return FVector(60.0f, 24.0f, 48.0f); // Widened X for a bigger hit zone
}

FVector UPhysAnimComparisonSubsystem::ResolvePresentationPusherStartOffsetCm()
{
	return FVector(0.0f, -120.0f, 54.0f); // Start dead-center in X, further out in Y
}

float UPhysAnimComparisonSubsystem::ResolvePresentationPusherTravelDistanceCm()
{
	return 250.0f; // Increased from 118 for visibility
}

float UPhysAnimComparisonSubsystem::ResolvePresentationPusherTravelSeconds()
{
	return 1.1f;
}

FVector UPhysAnimComparisonSubsystem::ResolvePresentationShellPushForce()
{
	return FVector::ZeroVector;
}

float UPhysAnimComparisonSubsystem::ResolvePresentationStabilizationOverrideSeconds()
{
	return 0.45f;
}

float UPhysAnimComparisonSubsystem::ResolvePresentationStrengthRelaxationMultiplier()
{
	return 0.72f;
}

float UPhysAnimComparisonSubsystem::ResolvePresentationDampingRatioRelaxationMultiplier()
{
	return 0.78f;
}

float UPhysAnimComparisonSubsystem::ResolvePresentationExtraDampingRelaxationMultiplier()
{
	return 0.74f;
}

FName UPhysAnimComparisonSubsystem::ResolvePresentationRootBoneName()
{
	return TEXT("pelvis");
}

bool UPhysAnimComparisonSubsystem::ConfigureSourcePhysicsCharacter(ACharacter& Character, FString& OutError)
{
	if (UCapsuleComponent* const Capsule = Character.GetCapsuleComponent())
	{
		SourceOriginalCapsulePawnResponse = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
		SourceOriginalCapsuleWorldDynamicResponse = Capsule->GetCollisionResponseToChannel(ECC_WorldDynamic);
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		Capsule->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	}

	if (USkeletalMeshComponent* const Mesh = Character.GetMesh())
	{
		SourceOriginalMeshPawnResponse = Mesh->GetCollisionResponseToChannel(ECC_Pawn);
		SourceOriginalMeshWorldDynamicResponse = Mesh->GetCollisionResponseToChannel(ECC_WorldDynamic);
		Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		Mesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); // Required for simulation
		Mesh->SetSimulatePhysics(true);
	}

	if (UPhysAnimComponent* const PhysAnim = Character.FindComponentByClass<UPhysAnimComponent>())
	{
		if (UCharacterMovementComponent* const Movement = Character.GetCharacterMovement())
		{
			Movement->bRunPhysicsWithNoController = true;
			Movement->SetComponentTickEnabled(true);
			if (Movement->MovementMode == MOVE_None)
			{
				Movement->SetMovementMode(MOVE_Walking);
			}
		}

		if (PhysAnim->GetRuntimeState() == EPhysAnimRuntimeState::Uninitialized ||
			PhysAnim->GetRuntimeState() == EPhysAnimRuntimeState::FailStopped)
		{
			PhysAnim->StartBridge();
		}
		return true;
	}

	OutError = TEXT("Source comparison character is missing UPhysAnimComponent.");
	return false;
}

bool UPhysAnimComparisonSubsystem::ConfigureKinematicBaselineCharacter(ACharacter& Character, FString& OutError)
{
	if (UCapsuleComponent* const Capsule = Character.GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		Capsule->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
	}

	if (USkeletalMeshComponent* const Mesh = Character.GetMesh())
	{
		Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		Mesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
	}

	if (UCharacterMovementComponent* const Movement = Character.GetCharacterMovement())
	{
		Movement->bRunPhysicsWithNoController = true;
		Movement->SetComponentTickEnabled(true);
		if (Movement->MovementMode == MOVE_None)
		{
			Movement->SetMovementMode(MOVE_Walking);
		}
	}

	if (UPhysAnimComponent* const PhysAnim = Character.FindComponentByClass<UPhysAnimComponent>())
	{
		PhysAnim->StopBridge();
		return true;
	}

	OutError = TEXT("Kinematic comparison character is missing UPhysAnimComponent.");
	return false;
}

bool UPhysAnimComparisonSubsystem::ActivatePresentationMode(FString& OutError)
{
	ACharacter* const SourceCharacter = SourcePhysicsCharacter.Get();
	if (!SourceCharacter)
	{
		OutError = TEXT("Presentation mode requires a valid source character.");
		return false;
	}

	APlayerController* const PlayerController = Cast<APlayerController>(SourceCharacter->GetController());
	if (!PlayerController)
	{
		OutError = TEXT("Presentation mode requires the source comparison character to be player controlled.");
		return false;
	}

	PresentationPlayerController = PlayerController;
	OriginalViewTarget = PlayerController->GetViewTarget();
	bHasSavedControllerInputState = true;
	bOriginalIgnoreMoveInput = PlayerController->IsMoveInputIgnored();
	bOriginalIgnoreLookInput = PlayerController->IsLookInputIgnored();
	PlayerController->SetIgnoreMoveInput(true);
	PlayerController->SetIgnoreLookInput(true);

	UWorld* const World = GetWorld();
	if (!World)
	{
		OutError = TEXT("Presentation mode requires a valid world.");
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.Name = MakeUniqueObjectName(World, ACameraActor::StaticClass(), PresentationCameraActorName);
	ACameraActor* const CameraActor = World->SpawnActor<ACameraActor>(
		ACameraActor::StaticClass(),
		SourceCharacter->GetActorLocation(),
		FRotator::ZeroRotator,
		SpawnParameters);
	if (!CameraActor)
	{
		OutError = TEXT("Failed to spawn the G2 presentation camera.");
		return false;
	}

	ComparisonCameraActor = CameraActor;
	PresentationStartTimeSeconds = -1.0;
	LastPresentationPhaseName = TEXT("WaitingForBridgeReady");
	bPresentationPerturbationApplied = false;
	PresentationPerturbationAppliedTimeSeconds = -1.0;
	LastPerturbationTelemetryLogTimeSeconds = -1.0;
	bPresentationModeActive = true;
	if (!CreatePresentationPushers(OutError))
	{
		return false;
	}
	UpdatePresentationCamera();
	PlayerController->SetViewTarget(CameraActor);
	return true;
}

bool UPhysAnimComparisonSubsystem::CreatePresentationPushers(FString& OutError)
{
	UWorld* const World = GetWorld();
	ACharacter* const SourceCharacter = SourcePhysicsCharacter.Get();
	ACharacter* const KinematicCharacter = SpawnedKinematicCharacter.Get();
	if (!World || !SourceCharacter || !KinematicCharacter)
	{
		OutError = TEXT("Presentation pushers require valid comparison actors and world.");
		return false;
	}

	const FVector HalfExtent = ResolvePresentationPusherHalfExtentCm();
	const FVector InitialOffset = ComparisonAnchorRotation.RotateVector(ResolvePresentationPusherStartOffsetCm());
	const FRotator PusherRotation = ComparisonAnchorRotation;

	auto SpawnPusher = [&](const TCHAR* NameSuffix, const FVector& InitialLocation, TWeakObjectPtr<AActor>& OutActor, TWeakObjectPtr<UBoxComponent>& OutBox) -> bool
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParameters.Name = MakeUniqueObjectName(World, AActor::StaticClass(), FName(NameSuffix));
		AActor* const PusherActor = World->SpawnActor<AActor>(
			AActor::StaticClass(),
			FTransform(PusherRotation, InitialLocation, FVector::OneVector),
			SpawnParameters);
		if (!PusherActor)
		{
			return false;
		}

		UBoxComponent* const PusherBox = NewObject<UBoxComponent>(PusherActor, TEXT("PusherBox"));
		if (!PusherBox)
		{
			PusherActor->Destroy();
			return false;
		}

		PusherActor->SetRootComponent(PusherBox);
		PusherActor->AddInstanceComponent(PusherBox);
		PusherBox->SetMobility(EComponentMobility::Movable);
		PusherBox->SetBoxExtent(HalfExtent);
		PusherBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		PusherBox->SetCollisionObjectType(ECC_WorldDynamic);
		PusherBox->SetCollisionResponseToAllChannels(ECR_Block); // Changed from Ignore to Block
		PusherBox->SetGenerateOverlapEvents(false);
		PusherBox->SetHiddenInGame(false);
		PusherBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PusherBox->RegisterComponent();
		PusherActor->SetActorHiddenInGame(true);
		PusherActor->SetActorEnableCollision(true);
		PusherActor->SetActorLocationAndRotation(InitialLocation, PusherRotation, false, nullptr, ETeleportType::TeleportPhysics);

		OutActor = PusherActor;
		OutBox = PusherBox;
		return true;
	};

	if (!SpawnPusher(TEXT("PhysAnimG2SourcePusher"), SourceCharacter->GetActorLocation() + InitialOffset, SourcePerturbationPusherActor, SourcePerturbationPusherBox) ||
		!SpawnPusher(TEXT("PhysAnimG2KinematicPusher"), KinematicCharacter->GetActorLocation() + InitialOffset, KinematicPerturbationPusherActor, KinematicPerturbationPusherBox))
	{
		OutError = TEXT("Failed to spawn scripted G2 perturbation pushers.");
		return false;
	}

	return true;
}

void UPhysAnimComparisonSubsystem::TickManualComparison(ACharacter& SourceCharacter, ACharacter& KinematicCharacter)
{
	const FVector MirroredWorldInput = ResolveMirroredWorldInput(
		SourceCharacter.GetPendingMovementInputVector(),
		SourceCharacter.GetLastMovementInputVector());
	if (!MirroredWorldInput.IsNearlyZero())
	{
		KinematicCharacter.AddMovementInput(MirroredWorldInput.GetSafeNormal(), MirroredWorldInput.Size(), true);
	}
}

void UPhysAnimComparisonSubsystem::TickPresentationComparison(ACharacter& SourceCharacter, ACharacter& KinematicCharacter)
{
	UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}

	if (PresentationStartTimeSeconds < 0.0)
	{
		if (const UPhysAnimComponent* const PhysAnim = SourceCharacter.FindComponentByClass<UPhysAnimComponent>())
		{
			if (!PhysAnim->IsReadyForScriptedPresentation())
			{
				LastPresentationPhaseName = TEXT("WaitingForBridgeReady");
				return;
			}
		}

		PresentationStartTimeSeconds = World->GetTimeSeconds();
		LastPresentationPhaseName = TEXT("WalkPerturbation");
		UE_LOG(LogTemp, Log, TEXT("%s Scripted G2 presentation sequence started from steady BridgeActive state."), PhysAnimG2LogPrefix);
	}

	const float ElapsedSeconds = static_cast<float>(World->GetTimeSeconds() - PresentationStartTimeSeconds);
	const bool bPerturbationPhaseActive = ShouldApplyPresentationPerturbation(ElapsedSeconds) &&
		ElapsedSeconds < PresentationPerturbationDurationSeconds;
	if (ShouldApplyPresentationPerturbation(ElapsedSeconds))
	{
		if (!bPresentationPerturbationApplied)
		{
			CapturePerturbationBaseline(SourceCharacter, KinematicCharacter);
			PresentationPerturbationAppliedTimeSeconds = World->GetTimeSeconds();
			LastPerturbationTelemetryLogTimeSeconds =
				PresentationPerturbationAppliedTimeSeconds - PresentationPerturbationTelemetryIntervalSeconds;
			bPresentationPerturbationApplied = true;
			UE_LOG(
				LogTemp,
				Log,
				TEXT("%s Applied scripted G2 contact push: halfExtent=(%.1f, %.1f, %.1f) travel=%.1fcm duration=%.2fs shellForce=(%.1f, %.1f, %.1f) stabilizationOverride=%.2fs"),
				PhysAnimG2LogPrefix,
				ResolvePresentationPusherHalfExtentCm().X,
				ResolvePresentationPusherHalfExtentCm().Y,
				ResolvePresentationPusherHalfExtentCm().Z,
				ResolvePresentationPusherTravelDistanceCm(),
				ResolvePresentationPusherTravelSeconds(),
				ResolvePresentationShellPushForce().X,
				ResolvePresentationShellPushForce().Y,
				ResolvePresentationShellPushForce().Z,
				ResolvePresentationStabilizationOverrideSeconds());

			if (UPhysAnimComponent* const PhysAnim = SourceCharacter.FindComponentByClass<UPhysAnimComponent>())
			{
				PhysAnim->SetPresentationPerturbationOverrideSeconds(ResolvePresentationStabilizationOverrideSeconds());
			}
		}

		UpdatePerturbationPushers(SourceCharacter, KinematicCharacter, ElapsedSeconds);
	}

	MaybeLogPerturbationTelemetry(SourceCharacter, KinematicCharacter);

	const FVector LocalIntent = ResolvePresentationLocalIntent(ElapsedSeconds);
	const float InputScale = ResolvePresentationInputScale(ElapsedSeconds);
	LastPresentationPhaseName = ResolvePresentationPhaseName(ElapsedSeconds);
	if (InputScale <= KINDA_SMALL_NUMBER || LocalIntent.IsNearlyZero())
	{
		return;
	}

	FRotator IntentRotation = ComparisonAnchorRotation;
	IntentRotation.Pitch = 0.0f;
	IntentRotation.Roll = 0.0f;
	const FVector Forward = FRotationMatrix(IntentRotation).GetScaledAxis(EAxis::X).GetSafeNormal2D();
	const FVector Right = FRotationMatrix(IntentRotation).GetScaledAxis(EAxis::Y).GetSafeNormal2D();
	const FVector WorldIntent = ((Forward * LocalIntent.X) + (Right * LocalIntent.Y)).GetClampedToMaxSize(1.0f);

	SourceCharacter.AddMovementInput(WorldIntent, InputScale, true);
	KinematicCharacter.AddMovementInput(WorldIntent, InputScale, true);
}

void UPhysAnimComparisonSubsystem::TickComparisonLabels() const
{
	UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}

	if (const ACharacter* const KinematicCharacter = SpawnedKinematicCharacter.Get())
	{
		DrawDebugString(
			World,
			KinematicCharacter->GetActorLocation() + FVector(0.0f, 0.0f, 120.0f),
			ResolveComparisonRoleLabel(false),
			nullptr,
			FColor::Cyan,
			0.0f,
			true);
	}

	if (const ACharacter* const SourceCharacter = SourcePhysicsCharacter.Get())
	{
		DrawDebugString(
			World,
			SourceCharacter->GetActorLocation() + FVector(0.0f, 0.0f, 120.0f),
			ResolveComparisonRoleLabel(true),
			nullptr,
			FColor::Green,
			0.0f,
			true);
	}

	if (bPresentationModeActive)
	{
		const FVector Midpoint = SourcePhysicsCharacter.IsValid() && SpawnedKinematicCharacter.IsValid()
			? (SourcePhysicsCharacter->GetActorLocation() + SpawnedKinematicCharacter->GetActorLocation()) * 0.5f
			: FVector::ZeroVector;
		DrawDebugString(
			World,
			Midpoint + FVector(0.0f, 0.0f, 220.0f),
			FString::Printf(TEXT("G2 Sequence: %s"), *LastPresentationPhaseName.ToString()),
			nullptr,
			FColor::White,
			0.0f,
			true);
	}
}

void UPhysAnimComparisonSubsystem::UpdatePresentationCamera()
{
	ACharacter* const SourceCharacter = SourcePhysicsCharacter.Get();
	ACharacter* const KinematicCharacter = SpawnedKinematicCharacter.Get();
	ACameraActor* const CameraActor = ComparisonCameraActor.Get();
	if (!SourceCharacter || !KinematicCharacter || !CameraActor)
	{
		return;
	}

	const FVector Midpoint = (SourceCharacter->GetActorLocation() + KinematicCharacter->GetActorLocation()) * 0.5f;
	const bool bPerturbationPhase = LastPresentationPhaseName == TEXT("WalkPerturbation");
	const FRotator FramingRotation(0.0f, ComparisonAnchorRotation.Yaw, 0.0f);
	const FVector Forward = FRotationMatrix(FramingRotation).GetScaledAxis(EAxis::X).GetSafeNormal();
	const FVector Right = FRotationMatrix(FramingRotation).GetScaledAxis(EAxis::Y).GetSafeNormal();
	const FVector Separation = SourceCharacter->GetActorLocation() - KinematicCharacter->GetActorLocation();
	const float LateralSpreadCm = FMath::Abs(FVector::DotProduct(Separation, Right));
	const float ForwardSpreadCm = FMath::Abs(FVector::DotProduct(Separation, Forward));
	const float ExtraBackoffCm = bPerturbationPhase
		? FMath::Clamp((LateralSpreadCm * 1.15f) + (ForwardSpreadCm * 0.35f), 0.0f, 420.0f)
		: FMath::Clamp((LateralSpreadCm * 0.40f) + (ForwardSpreadCm * 0.20f), 0.0f, 180.0f);
	const FVector BaseOffset = ResolvePresentationCameraOffsetCm(bPerturbationPhase);
	const FVector DynamicOffset(
		BaseOffset.X + ExtraBackoffCm,
		BaseOffset.Y,
		BaseOffset.Z + (bPerturbationPhase ? (ExtraBackoffCm * 0.18f) : (ExtraBackoffCm * 0.10f)));
	const FVector CameraLocation = Midpoint + ComparisonAnchorRotation.RotateVector(DynamicOffset);
	const FVector LookAtLocation = Midpoint + FVector(0.0f, 0.0f, bPerturbationPhase ? 88.0f : 90.0f);
	const FRotator CameraRotation = (LookAtLocation - CameraLocation).Rotation();
	CameraActor->SetActorLocationAndRotation(CameraLocation, CameraRotation, false, nullptr, ETeleportType::TeleportPhysics);

	if (UCameraComponent* const CameraComponent = CameraActor->GetCameraComponent())
	{
		const float TargetFov = bPerturbationPhase
			? FMath::Clamp(58.0f + (LateralSpreadCm / 18.0f), 58.0f, 82.0f)
			: 60.0f;
		CameraComponent->SetFieldOfView(TargetFov);
	}
}

void UPhysAnimComparisonSubsystem::UpdatePerturbationPushers(
	ACharacter& SourceCharacter,
	ACharacter& KinematicCharacter,
	float ElapsedSeconds)
{
	const float PushElapsedSeconds = FMath::Clamp(ElapsedSeconds - PresentationPerturbationLeadInSeconds, 0.0f, ResolvePresentationPusherTravelSeconds());
	const float PushAlpha = ResolvePresentationPusherTravelSeconds() > KINDA_SMALL_NUMBER
		? FMath::Clamp(PushElapsedSeconds / ResolvePresentationPusherTravelSeconds(), 0.0f, 1.0f)
		: 1.0f;

	auto AuditCollision = [](UPrimitiveComponent* Comp, const TCHAR* Name)
	{
		if (!Comp) return;
		const ECollisionEnabled::Type Enabled = Comp->GetCollisionEnabled();
		const ECollisionResponse Response = Comp->GetCollisionResponseToChannel(ECC_WorldDynamic);
		const FName Profile = Comp->GetCollisionProfileName();
		UE_LOG(LogTemp, Log, TEXT("[PhysAnimG2] Audit %s: enabled=%d response_to_worlddynamic=%d profile=%s"), 
			Name, (int)Enabled, (int)Response, *Profile.ToString());
	};

	const FVector LocalStartOffset = ResolvePresentationPusherStartOffsetCm();
	const FVector LocalTravelOffset(0.0f, ResolvePresentationPusherTravelDistanceCm() * PushAlpha, 0.0f);
	const FVector WorldOffset = ComparisonAnchorRotation.RotateVector(LocalStartOffset + LocalTravelOffset);
	const FVector HalfExtent = ResolvePresentationPusherHalfExtentCm();

	auto MovePusher = [&](const FVector& AnchorLocation, bool bIsSourcePusher, TWeakObjectPtr<AActor>& PusherActorPtr, TWeakObjectPtr<UBoxComponent>& PusherBoxPtr)
	{
		AActor* const PusherActor = PusherActorPtr.Get();
		UBoxComponent* const PusherBox = PusherBoxPtr.Get();
		if (!PusherActor || !PusherBox)
		{
			return;
		}

		PusherActor->SetActorHiddenInGame(false);
		PusherBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		const FVector TargetLocation = AnchorLocation + WorldOffset;
		// Disable sweep (false) so the box penetrates the character and hits the simulated mesh bones
		PusherActor->SetActorLocationAndRotation(TargetLocation, ComparisonAnchorRotation, false, nullptr);

		// AUDIT: Check for overlaps manually and log them
		TArray<FOverlapResult> Overlaps;
		FComponentQueryParams Params;
		Params.AddIgnoredActor(PusherActor);
		// Ignore the capsule so we can 'see' the mesh bones behind it
		if (SourceCharacter.GetCapsuleComponent()) Params.AddIgnoredComponent(SourceCharacter.GetCapsuleComponent());
		if (KinematicCharacter.GetCapsuleComponent()) Params.AddIgnoredComponent(KinematicCharacter.GetCapsuleComponent());

		if (GetWorld()->ComponentOverlapMulti(Overlaps, PusherBox, TargetLocation, ComparisonAnchorRotation.Quaternion(), Params))
		{
			// REJECTED for Balance Testing: Use pa.StartBalanceMode for authoritative testing.
			// This legacy path uses capsule/move-component impulses which are invalid for pure balance verification.
			/*
			const FVector ShoveDirection = ComparisonAnchorRotation.RotateVector(FVector(0.0f, 1.0f, 0.0f));
			const float CapsuleShoveMagnitude = 12000.0f; 
			const float BoneShoveMagnitude = 4000.0f;

			bool bKickedCapsule = false;

			for (const FOverlapResult& Overlap : Overlaps)
			{
				UPrimitiveComponent* OverlapComponent = Overlap.Component.Get();
				if (!OverlapComponent) continue;

				// Apply a direct impulse if it's the mesh
				if (OverlapComponent->IsSimulatingPhysics() || OverlapComponent->GetName().Contains(TEXT("Mesh")))
				{
					FName HitBoneName = NAME_None;
					if (USkeletalMeshComponent* const Mesh = Cast<USkeletalMeshComponent>(OverlapComponent))
					{
						HitBoneName = Mesh->GetBoneName(Overlap.ItemIndex);
					}

					// 1. Kick the whole character via its movement component (this moves the waist/root)
					if (!bKickedCapsule && Overlap.GetActor())
					{
						if (ACharacter* Char = Cast<ACharacter>(Overlap.GetActor()))
						{
							if (UCharacterMovementComponent* MoveComp = Char->GetCharacterMovement())
							{
								// Using bVelocityChange = true for an unmistakable mass-independent kick
								MoveComp->AddImpulse(ShoveDirection * (CapsuleShoveMagnitude / 100.0f), true);
								bKickedCapsule = true;
							}
						}
					}

					// 2. Shake the specific bone for visual stumble, only if it's simulating
					if (OverlapComponent->IsSimulatingPhysics())
					{
						OverlapComponent->AddImpulse(ShoveDirection * BoneShoveMagnitude, HitBoneName);
					}
				}
			}
			*/
		}

		DrawDebugBox(
			GetWorld(),
			TargetLocation,
			HalfExtent,
			ComparisonAnchorRotation.Quaternion(),
			bIsSourcePusher ? FColor::Orange : FColor::Cyan,
			false,
			1.5f, // Increased from 0.05
			0,
			4.0f); // Increased from 2.0
	};

	MovePusher(SourcePerturbationBaselineLocation, true, SourcePerturbationPusherActor, SourcePerturbationPusherBox);

	if (UBoxComponent* const KinematicPusherBox = KinematicPerturbationPusherBox.Get())
	{
		KinematicPusherBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (AActor* const KinematicPusherActor = KinematicPerturbationPusherActor.Get())
	{
		KinematicPusherActor->SetActorHiddenInGame(true);
	}
}

void UPhysAnimComparisonSubsystem::SetPresentationSourceShellSuppressed(ACharacter& SourceCharacter, bool bSuppressed)
{
	if (bPresentationSourceShellSuppressed == bSuppressed)
	{
		return;
	}

	if (UCharacterMovementComponent* const Movement = SourceCharacter.GetCharacterMovement())
	{
		if (bSuppressed)
		{
			if (!bHasSavedPresentationSourceMovementState)
			{
				bSavedPresentationSourceMovementTickEnabled = Movement->IsComponentTickEnabled();
				SavedPresentationSourceMovementMode = Movement->MovementMode;
				SavedPresentationSourceCustomMovementMode = Movement->CustomMovementMode;
				bHasSavedPresentationSourceMovementState = true;
			}
			Movement->DisableMovement();
			Movement->SetComponentTickEnabled(false);
		}
		else if (bHasSavedPresentationSourceMovementState)
		{
			Movement->SetComponentTickEnabled(bSavedPresentationSourceMovementTickEnabled);
			Movement->SetMovementMode(
				static_cast<EMovementMode>(SavedPresentationSourceMovementMode),
				SavedPresentationSourceCustomMovementMode);
			bHasSavedPresentationSourceMovementState = false;
		}
	}

	if (UCapsuleComponent* const Capsule = SourceCharacter.GetCapsuleComponent())
	{
		if (bSuppressed)
		{
			SavedPresentationSourceCapsuleCollisionEnabled = Capsule->GetCollisionEnabled();
			Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		else
		{
			Capsule->SetCollisionEnabled(SavedPresentationSourceCapsuleCollisionEnabled);
		}
	}

	bPresentationSourceShellSuppressed = bSuppressed;
}

void UPhysAnimComparisonSubsystem::UpdatePresentationSourceRootFollow(ACharacter& SourceCharacter)
{
	USkeletalMeshComponent* const SourceMesh = SourceCharacter.GetMesh();
	if (!SourceMesh)
	{
		return;
	}

	const FVector CurrentRootLocation = SourceMesh->GetBoneLocation(ResolvePresentationRootBoneName());
	const FVector DesiredActorLocation = CurrentRootLocation - SourcePerturbationBaselineRootLocalOffset;
	SourceCharacter.SetActorLocation(DesiredActorLocation, false, nullptr, ETeleportType::TeleportPhysics);
}

void UPhysAnimComparisonSubsystem::CapturePerturbationBaseline(const ACharacter& SourceCharacter, const ACharacter& KinematicCharacter)
{
	SourcePerturbationBaselineLocation = SourceCharacter.GetActorLocation();
	KinematicPerturbationBaselineLocation = KinematicCharacter.GetActorLocation();

	if (const USkeletalMeshComponent* const SourceMesh = SourceCharacter.GetMesh())
	{
		SourcePerturbationBaselineRootLocation = SourceMesh->GetBoneLocation(ResolvePresentationRootBoneName());
		SourcePerturbationBaselineSpineLocation = SourceMesh->GetBoneLocation(TEXT("spine_01"));
		SourcePerturbationBaselineHeadLocation = SourceMesh->GetBoneLocation(TEXT("head"));
		SourcePerturbationBaselineLeftFootLocation = SourceMesh->GetBoneLocation(TEXT("foot_l"));
		SourcePerturbationBaselineRightFootLocation = SourceMesh->GetBoneLocation(TEXT("foot_r"));
		SourcePerturbationBaselineRootLocalOffset = SourcePerturbationBaselineRootLocation - SourcePerturbationBaselineLocation;
		SourcePerturbationBaselineSpineLocalOffset = SourcePerturbationBaselineSpineLocation - SourcePerturbationBaselineLocation;
		SourcePerturbationBaselineHeadLocalOffset = SourcePerturbationBaselineHeadLocation - SourcePerturbationBaselineLocation;
		SourcePerturbationBaselineLeftFootLocalOffset = SourcePerturbationBaselineLeftFootLocation - SourcePerturbationBaselineLocation;
		SourcePerturbationBaselineRightFootLocalOffset = SourcePerturbationBaselineRightFootLocation - SourcePerturbationBaselineLocation;
	}

	if (const USkeletalMeshComponent* const KinematicMesh = KinematicCharacter.GetMesh())
	{
		KinematicPerturbationBaselineSpineLocation = KinematicMesh->GetBoneLocation(TEXT("spine_01"));
		KinematicPerturbationBaselineHeadLocation = KinematicMesh->GetBoneLocation(TEXT("head"));
		KinematicPerturbationBaselineLeftFootLocation = KinematicMesh->GetBoneLocation(TEXT("foot_l"));
		KinematicPerturbationBaselineRightFootLocation = KinematicMesh->GetBoneLocation(TEXT("foot_r"));
		KinematicPerturbationBaselineSpineLocalOffset = KinematicPerturbationBaselineSpineLocation - KinematicPerturbationBaselineLocation;
		KinematicPerturbationBaselineHeadLocalOffset = KinematicPerturbationBaselineHeadLocation - KinematicPerturbationBaselineLocation;
		KinematicPerturbationBaselineLeftFootLocalOffset = KinematicPerturbationBaselineLeftFootLocation - KinematicPerturbationBaselineLocation;
		KinematicPerturbationBaselineRightFootLocalOffset = KinematicPerturbationBaselineRightFootLocation - KinematicPerturbationBaselineLocation;
	}
}

void UPhysAnimComparisonSubsystem::MaybeLogPerturbationTelemetry(const ACharacter& SourceCharacter, const ACharacter& KinematicCharacter)
{
	UWorld* const World = GetWorld();
	if (!World || PresentationPerturbationAppliedTimeSeconds < 0.0)
	{
		return;
	}

	const double ElapsedSincePerturbation = World->GetTimeSeconds() - PresentationPerturbationAppliedTimeSeconds;
	if (ElapsedSincePerturbation < 0.0 || ElapsedSincePerturbation > PresentationPerturbationTelemetryDurationSeconds)
	{
		return;
	}

	if ((World->GetTimeSeconds() - LastPerturbationTelemetryLogTimeSeconds) < PresentationPerturbationTelemetryIntervalSeconds)
	{
		return;
	}

	LastPerturbationTelemetryLogTimeSeconds = World->GetTimeSeconds();

	const USkeletalMeshComponent* const SourceMesh = SourceCharacter.GetMesh();
	const USkeletalMeshComponent* const KinematicMesh = KinematicCharacter.GetMesh();
	if (!SourceMesh || !KinematicMesh)
	{
		return;
	}

	const float SourceActorDeltaCm = FVector::Dist(SourceCharacter.GetActorLocation(), SourcePerturbationBaselineLocation);
	const float KinematicActorDeltaCm = FVector::Dist(KinematicCharacter.GetActorLocation(), KinematicPerturbationBaselineLocation);
	const float SourceSpineDeltaCm = FVector::Dist(SourceMesh->GetBoneLocation(TEXT("spine_01")), SourcePerturbationBaselineSpineLocation);
	const float KinematicSpineDeltaCm = FVector::Dist(KinematicMesh->GetBoneLocation(TEXT("spine_01")), KinematicPerturbationBaselineSpineLocation);
	const float SourceHeadDeltaCm = FVector::Dist(SourceMesh->GetBoneLocation(TEXT("head")), SourcePerturbationBaselineHeadLocation);
	const float KinematicHeadDeltaCm = FVector::Dist(KinematicMesh->GetBoneLocation(TEXT("head")), KinematicPerturbationBaselineHeadLocation);
	const float SourceMaxFootDeltaCm = FMath::Max(
		FVector::Dist(SourceMesh->GetBoneLocation(TEXT("foot_l")), SourcePerturbationBaselineLeftFootLocation),
		FVector::Dist(SourceMesh->GetBoneLocation(TEXT("foot_r")), SourcePerturbationBaselineRightFootLocation));
	const float KinematicMaxFootDeltaCm = FMath::Max(
		FVector::Dist(KinematicMesh->GetBoneLocation(TEXT("foot_l")), KinematicPerturbationBaselineLeftFootLocation),
		FVector::Dist(KinematicMesh->GetBoneLocation(TEXT("foot_r")), KinematicPerturbationBaselineRightFootLocation));
	const float SourceSpineLocalDeltaCm = FVector::Dist(
		SourceMesh->GetBoneLocation(TEXT("spine_01")) - SourceCharacter.GetActorLocation(),
		SourcePerturbationBaselineSpineLocalOffset);
	const float KinematicSpineLocalDeltaCm = FVector::Dist(
		KinematicMesh->GetBoneLocation(TEXT("spine_01")) - KinematicCharacter.GetActorLocation(),
		KinematicPerturbationBaselineSpineLocalOffset);
	const float SourceHeadLocalDeltaCm = FVector::Dist(
		SourceMesh->GetBoneLocation(TEXT("head")) - SourceCharacter.GetActorLocation(),
		SourcePerturbationBaselineHeadLocalOffset);
	const float KinematicHeadLocalDeltaCm = FVector::Dist(
		KinematicMesh->GetBoneLocation(TEXT("head")) - KinematicCharacter.GetActorLocation(),
		KinematicPerturbationBaselineHeadLocalOffset);
	const float SourceMaxFootLocalDeltaCm = FMath::Max(
		FVector::Dist(
			SourceMesh->GetBoneLocation(TEXT("foot_l")) - SourceCharacter.GetActorLocation(),
			SourcePerturbationBaselineLeftFootLocalOffset),
		FVector::Dist(
			SourceMesh->GetBoneLocation(TEXT("foot_r")) - SourceCharacter.GetActorLocation(),
			SourcePerturbationBaselineRightFootLocalOffset));
	const float KinematicMaxFootLocalDeltaCm = FMath::Max(
		FVector::Dist(
			KinematicMesh->GetBoneLocation(TEXT("foot_l")) - KinematicCharacter.GetActorLocation(),
			KinematicPerturbationBaselineLeftFootLocalOffset),
		FVector::Dist(
			KinematicMesh->GetBoneLocation(TEXT("foot_r")) - KinematicCharacter.GetActorLocation(),
			KinematicPerturbationBaselineRightFootLocalOffset));
	const float SpineGapCm = SourceSpineDeltaCm - KinematicSpineDeltaCm;
	const float HeadGapCm = SourceHeadDeltaCm - KinematicHeadDeltaCm;
	const float FootGapCm = SourceMaxFootDeltaCm - KinematicMaxFootDeltaCm;
	const float SpineLocalGapCm = SourceSpineLocalDeltaCm - KinematicSpineLocalDeltaCm;
	const float HeadLocalGapCm = SourceHeadLocalDeltaCm - KinematicHeadLocalDeltaCm;
	const float FootLocalGapCm = SourceMaxFootLocalDeltaCm - KinematicMaxFootLocalDeltaCm;

	UE_LOG(
		LogTemp,
		Log,
		TEXT("%s Perturbation telemetry t=%.2fs physics[actorDelta=%.1f spineDelta=%.1f headDelta=%.1f maxFootDelta=%.1f localSpine=%.1f localHead=%.1f localFoot=%.1f] kinematic[actorDelta=%.1f spineDelta=%.1f headDelta=%.1f maxFootDelta=%.1f localSpine=%.1f localHead=%.1f localFoot=%.1f] gap[spine=%.1f head=%.1f foot=%.1f localSpine=%.1f localHead=%.1f localFoot=%.1f]"),
		PhysAnimG2LogPrefix,
		ElapsedSincePerturbation,
		SourceActorDeltaCm,
		SourceSpineDeltaCm,
		SourceHeadDeltaCm,
		SourceMaxFootDeltaCm,
		SourceSpineLocalDeltaCm,
		SourceHeadLocalDeltaCm,
		SourceMaxFootLocalDeltaCm,
		KinematicActorDeltaCm,
		KinematicSpineDeltaCm,
		KinematicHeadDeltaCm,
		KinematicMaxFootDeltaCm,
		KinematicSpineLocalDeltaCm,
		KinematicHeadLocalDeltaCm,
		KinematicMaxFootLocalDeltaCm,
		SpineGapCm,
		HeadGapCm,
		FootGapCm,
		SpineLocalGapCm,
		HeadLocalGapCm,
		FootLocalGapCm);
}

void UPhysAnimComparisonSubsystem::RestoreSourcePhysicsCharacter()
{
	if (ACharacter* const SourceCharacter = SourcePhysicsCharacter.Get())
	{
		SetPresentationSourceShellSuppressed(*SourceCharacter, false);

		if (UPhysAnimComponent* const PhysAnim = SourceCharacter->FindComponentByClass<UPhysAnimComponent>())
		{
			PhysAnim->ClearPresentationPerturbationOverride();
		}

		if (UCapsuleComponent* const Capsule = SourceCharacter->GetCapsuleComponent())
		{
			Capsule->SetCollisionResponseToChannel(ECC_Pawn, SourceOriginalCapsulePawnResponse);
			Capsule->SetCollisionResponseToChannel(ECC_WorldDynamic, SourceOriginalCapsuleWorldDynamicResponse);
		}

		if (USkeletalMeshComponent* const Mesh = SourceCharacter->GetMesh())
		{
			Mesh->SetCollisionResponseToChannel(ECC_Pawn, SourceOriginalMeshPawnResponse);
			Mesh->SetCollisionResponseToChannel(ECC_WorldDynamic, SourceOriginalMeshWorldDynamicResponse);
		}

		if (bHasSavedSourceOriginalTransform)
		{
			SourceCharacter->SetActorTransform(SourceOriginalTransform, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}

	SourcePhysicsCharacter.Reset();
	bHasSavedSourceOriginalTransform = false;
}

void UPhysAnimComparisonSubsystem::RestorePresentationMode()
{
	if (APlayerController* const PlayerController = PresentationPlayerController.Get())
	{
		if (bHasSavedControllerInputState)
		{
			PlayerController->SetIgnoreMoveInput(bOriginalIgnoreMoveInput);
			PlayerController->SetIgnoreLookInput(bOriginalIgnoreLookInput);
		}

		if (AActor* const ViewTarget = OriginalViewTarget.Get())
		{
			PlayerController->SetViewTarget(ViewTarget);
		}
	}

	if (ACameraActor* const CameraActor = ComparisonCameraActor.Get())
	{
		CameraActor->Destroy();
	}

	if (AActor* const SourcePusherActor = SourcePerturbationPusherActor.Get())
	{
		SourcePusherActor->Destroy();
	}

	if (AActor* const KinematicPusherActor = KinematicPerturbationPusherActor.Get())
	{
		KinematicPusherActor->Destroy();
	}

	ComparisonCameraActor.Reset();
	SourcePerturbationPusherActor.Reset();
	KinematicPerturbationPusherActor.Reset();
	SourcePerturbationPusherBox.Reset();
	KinematicPerturbationPusherBox.Reset();
	PresentationPlayerController.Reset();
	OriginalViewTarget.Reset();
	bHasSavedControllerInputState = false;
	bOriginalIgnoreMoveInput = false;
	bOriginalIgnoreLookInput = false;
	PresentationStartTimeSeconds = -1.0;
	bPresentationPerturbationApplied = false;
	PresentationPerturbationAppliedTimeSeconds = -1.0;
	LastPerturbationTelemetryLogTimeSeconds = -1.0;
	bPresentationModeActive = false;
	LastPresentationPhaseName = NAME_None;
	bPresentationSourceShellSuppressed = false;
	bHasSavedPresentationSourceMovementState = false;
	bSavedPresentationSourceMovementTickEnabled = false;
	SavedPresentationSourceMovementMode = 0;
	SavedPresentationSourceCustomMovementMode = 0;
	SavedPresentationSourceCapsuleCollisionEnabled = ECollisionEnabled::QueryAndPhysics;
}
