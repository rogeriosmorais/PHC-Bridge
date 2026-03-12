#include "PhysAnimComparisonSubsystem.h"

#include "PhysAnimComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraActor.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RotationMatrix.h"

namespace
{
	const TCHAR* const PhysAnimG2LogPrefix = TEXT("[PhysAnimG2]");
	const TCHAR* const KinematicRoleName = TEXT("Kinematic");
	const TCHAR* const PhysicsDrivenRoleName = TEXT("Physics-Driven");
	const TCHAR* const PresentationCameraActorName = TEXT("PhysAnimG2Camera");
	constexpr float PresentationPerturbationImpulseMagnitudeCmPerSec = 5000.0f;
	constexpr float PresentationPerturbationLeadInSeconds = 1.0f;
	constexpr int32 PresentationPerturbationBurstPulseCount = 5;
	constexpr float PresentationPerturbationBurstPulseIntervalSeconds = 0.06f;
	constexpr float PresentationPerturbationObserveSeconds = 3.0f;
	constexpr float PresentationPerturbationDurationSeconds =
		PresentationPerturbationLeadInSeconds + PresentationPerturbationObserveSeconds;
	constexpr float PresentationPerturbationTelemetryDurationSeconds = 1.5f;
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
		return FVector::ZeroVector;
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
		return 0.0f;
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
		return TEXT("PerturbationPush");
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
		? FVector(220.0f, 0.0f, 155.0f)
		: FVector(325.0f, 0.0f, 170.0f);
}

FVector UPhysAnimComparisonSubsystem::ResolvePresentationPerturbationImpulseCmPerSec()
{
	return FVector(0.0f, PresentationPerturbationImpulseMagnitudeCmPerSec, 0.0f);
}

FName UPhysAnimComparisonSubsystem::ResolvePresentationPerturbationBoneName()
{
	return TEXT("spine_01");
}

bool UPhysAnimComparisonSubsystem::ConfigureSourcePhysicsCharacter(ACharacter& Character, FString& OutError)
{
	if (UCapsuleComponent* const Capsule = Character.GetCapsuleComponent())
	{
		SourceOriginalCapsulePawnResponse = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}

	if (USkeletalMeshComponent* const Mesh = Character.GetMesh())
	{
		SourceOriginalMeshPawnResponse = Mesh->GetCollisionResponseToChannel(ECC_Pawn);
		Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}

	if (UPhysAnimComponent* const PhysAnim = Character.FindComponentByClass<UPhysAnimComponent>())
	{
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
	}

	if (USkeletalMeshComponent* const Mesh = Character.GetMesh())
	{
		Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
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
	PresentationPerturbationBurstPulsesApplied = 0;
	LastPerturbationPulseTimeSeconds = -1.0;
	bPresentationModeActive = true;
	UpdatePresentationCamera();
	PlayerController->SetViewTarget(CameraActor);
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
		LastPresentationPhaseName = TEXT("PerturbationPush");
		UE_LOG(LogTemp, Log, TEXT("%s Scripted G2 presentation sequence started from steady BridgeActive state."), PhysAnimG2LogPrefix);
	}

	const float ElapsedSeconds = static_cast<float>(World->GetTimeSeconds() - PresentationStartTimeSeconds);
	if (ShouldApplyPresentationPerturbation(ElapsedSeconds) && PresentationPerturbationBurstPulsesApplied < PresentationPerturbationBurstPulseCount)
	{
		const bool bCanApplyNextPulse =
			PresentationPerturbationBurstPulsesApplied == 0 ||
			(World->GetTimeSeconds() - LastPerturbationPulseTimeSeconds) >= PresentationPerturbationBurstPulseIntervalSeconds;
		if (bCanApplyNextPulse)
		{
			const FRotator ImpulseRotation(0.0f, ComparisonAnchorRotation.Yaw, 0.0f);
			const FVector WorldImpulse = ImpulseRotation.RotateVector(ResolvePresentationPerturbationImpulseCmPerSec());
			const FName PerturbationBoneName = ResolvePresentationPerturbationBoneName();
			FVector PerturbationOrigin = SourceCharacter.GetActorLocation() + FVector(0.0f, 0.0f, 120.0f);

			if (PresentationPerturbationBurstPulsesApplied == 0)
			{
				CapturePerturbationBaseline(SourceCharacter, KinematicCharacter);
				PresentationPerturbationAppliedTimeSeconds = World->GetTimeSeconds();
				LastPerturbationTelemetryLogTimeSeconds =
					PresentationPerturbationAppliedTimeSeconds - PresentationPerturbationTelemetryIntervalSeconds;
			}

			if (USkeletalMeshComponent* const PhysicsMesh = SourceCharacter.GetMesh())
			{
				PerturbationOrigin = PhysicsMesh->GetBoneLocation(PerturbationBoneName);
				PhysicsMesh->AddImpulseToAllBodiesBelow(WorldImpulse, PerturbationBoneName, true, true);
			}

			if (USkeletalMeshComponent* const KinematicMesh = KinematicCharacter.GetMesh())
			{
				KinematicMesh->AddImpulseToAllBodiesBelow(WorldImpulse, PerturbationBoneName, true, true);
			}

			PresentationPerturbationBurstPulsesApplied += 1;
			LastPerturbationPulseTimeSeconds = World->GetTimeSeconds();
			bPresentationPerturbationApplied = true;

			DrawDebugDirectionalArrow(
				World,
				PerturbationOrigin,
				PerturbationOrigin + (WorldImpulse.GetSafeNormal() * 180.0f),
				70.0f,
				FColor::Orange,
				false,
				3.0f,
				0,
				8.0f);
			DrawDebugSphere(
				World,
				PerturbationOrigin,
				12.0f,
				12,
				FColor::Orange,
				false,
				3.0f,
				0,
				2.0f);

			if (PresentationPerturbationBurstPulsesApplied == 1)
			{
				UE_LOG(
					LogTemp,
					Log,
					TEXT("%s Applied scripted G2 perturbation burst: bone=%s impulse=(%.1f, %.1f, %.1f) pulses=%d interval=%.2fs"),
					PhysAnimG2LogPrefix,
					*PerturbationBoneName.ToString(),
					WorldImpulse.X,
					WorldImpulse.Y,
					WorldImpulse.Z,
					PresentationPerturbationBurstPulseCount,
					PresentationPerturbationBurstPulseIntervalSeconds);
			}
		}
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
	const bool bPerturbationPhase = LastPresentationPhaseName == TEXT("PerturbationPush");
	const FVector Offset = ComparisonAnchorRotation.RotateVector(ResolvePresentationCameraOffsetCm(bPerturbationPhase));
	const FVector CameraLocation = Midpoint + Offset;
	const FVector LookAtLocation = Midpoint + FVector(0.0f, 0.0f, 90.0f);
	const FRotator CameraRotation = (LookAtLocation - CameraLocation).Rotation();
	CameraActor->SetActorLocationAndRotation(CameraLocation, CameraRotation, false, nullptr, ETeleportType::TeleportPhysics);
}

void UPhysAnimComparisonSubsystem::CapturePerturbationBaseline(const ACharacter& SourceCharacter, const ACharacter& KinematicCharacter)
{
	SourcePerturbationBaselineLocation = SourceCharacter.GetActorLocation();
	KinematicPerturbationBaselineLocation = KinematicCharacter.GetActorLocation();

	if (const USkeletalMeshComponent* const SourceMesh = SourceCharacter.GetMesh())
	{
		SourcePerturbationBaselineSpineLocation = SourceMesh->GetBoneLocation(TEXT("spine_01"));
		SourcePerturbationBaselineHeadLocation = SourceMesh->GetBoneLocation(TEXT("head"));
		SourcePerturbationBaselineLeftFootLocation = SourceMesh->GetBoneLocation(TEXT("foot_l"));
		SourcePerturbationBaselineRightFootLocation = SourceMesh->GetBoneLocation(TEXT("foot_r"));
	}

	if (const USkeletalMeshComponent* const KinematicMesh = KinematicCharacter.GetMesh())
	{
		KinematicPerturbationBaselineSpineLocation = KinematicMesh->GetBoneLocation(TEXT("spine_01"));
		KinematicPerturbationBaselineHeadLocation = KinematicMesh->GetBoneLocation(TEXT("head"));
		KinematicPerturbationBaselineLeftFootLocation = KinematicMesh->GetBoneLocation(TEXT("foot_l"));
		KinematicPerturbationBaselineRightFootLocation = KinematicMesh->GetBoneLocation(TEXT("foot_r"));
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
	const float SpineGapCm = SourceSpineDeltaCm - KinematicSpineDeltaCm;
	const float HeadGapCm = SourceHeadDeltaCm - KinematicHeadDeltaCm;
	const float FootGapCm = SourceMaxFootDeltaCm - KinematicMaxFootDeltaCm;

	UE_LOG(
		LogTemp,
		Log,
		TEXT("%s Perturbation telemetry t=%.2fs physics[actorDelta=%.1f spineDelta=%.1f headDelta=%.1f maxFootDelta=%.1f] kinematic[actorDelta=%.1f spineDelta=%.1f headDelta=%.1f maxFootDelta=%.1f] gap[spine=%.1f head=%.1f foot=%.1f]"),
		PhysAnimG2LogPrefix,
		ElapsedSincePerturbation,
		SourceActorDeltaCm,
		SourceSpineDeltaCm,
		SourceHeadDeltaCm,
		SourceMaxFootDeltaCm,
		KinematicActorDeltaCm,
		KinematicSpineDeltaCm,
		KinematicHeadDeltaCm,
		KinematicMaxFootDeltaCm,
		SpineGapCm,
		HeadGapCm,
		FootGapCm);
}

void UPhysAnimComparisonSubsystem::RestoreSourcePhysicsCharacter()
{
	if (ACharacter* const SourceCharacter = SourcePhysicsCharacter.Get())
	{
		if (UCapsuleComponent* const Capsule = SourceCharacter->GetCapsuleComponent())
		{
			Capsule->SetCollisionResponseToChannel(ECC_Pawn, SourceOriginalCapsulePawnResponse);
		}

		if (USkeletalMeshComponent* const Mesh = SourceCharacter->GetMesh())
		{
			Mesh->SetCollisionResponseToChannel(ECC_Pawn, SourceOriginalMeshPawnResponse);
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

	ComparisonCameraActor.Reset();
	PresentationPlayerController.Reset();
	OriginalViewTarget.Reset();
	bHasSavedControllerInputState = false;
	bOriginalIgnoreMoveInput = false;
	bOriginalIgnoreLookInput = false;
	PresentationStartTimeSeconds = -1.0;
	bPresentationPerturbationApplied = false;
	PresentationPerturbationAppliedTimeSeconds = -1.0;
	LastPerturbationPulseTimeSeconds = -1.0;
	LastPerturbationTelemetryLogTimeSeconds = -1.0;
	PresentationPerturbationBurstPulsesApplied = 0;
	bPresentationModeActive = false;
	LastPresentationPhaseName = NAME_None;
}
