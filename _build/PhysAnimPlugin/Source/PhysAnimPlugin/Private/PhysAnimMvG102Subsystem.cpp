#include "PhysAnimMvG102Subsystem.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/CollisionProfile.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsControlComponent.h"
#include "PhysicsControlData.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Stats/Stats.h"
#include "Templates/Tuple.h"

namespace PhysAnimMvG102
{
	static const FName BodyModifierSetName(TEXT("MVG102_LeftArm"));
	static const FName ControlName(TEXT("MVG102_LeftHandWorld"));
	static const FName ControlSetName(TEXT("MVG102_LeftHandWorld"));
	static const FName ArmRootBoneName(TEXT("lowerarm_l"));
	static const FName HandBoneName(TEXT("hand_l"));
	static constexpr float TestDurationSeconds = 30.0f;
	static constexpr float OscillationHz = 0.55f;
	static constexpr float VerticalAmplitudeCm = 6.0f;
	static constexpr float ForwardAmplitudeCm = 2.0f;

	struct FMeshCollisionState
	{
		FName ProfileName = NAME_None;
		ECollisionEnabled::Type CollisionEnabled = ECollisionEnabled::NoCollision;
	};

	static TMap<TWeakObjectPtr<USkeletalMeshComponent>, FMeshCollisionState> OriginalCollisionStates;

	static void LogMeshDiagnostics(const TCHAR* Context, USkeletalMeshComponent* MeshComponent)
	{
		if (!MeshComponent)
		{
			return;
		}

		const UPhysicsAsset* const PhysicsAsset = MeshComponent->GetPhysicsAsset();
		const FBodyInstance* const ArmRootBody = MeshComponent->GetBodyInstance(ArmRootBoneName);
		const FBodyInstance* const HandBody = MeshComponent->GetBodyInstance(HandBoneName);

		UE_LOG(
			LogTemp,
			Display,
			TEXT("[MV-G1-02] %s | mesh=%s profile=%s collision=%s physics_state=%s physics_asset=%s arm_root_body=%s arm_root_sim=%s hand_body=%s hand_sim=%s"),
			Context,
			*MeshComponent->GetName(),
			*MeshComponent->GetCollisionProfileName().ToString(),
			*UEnum::GetValueAsString(MeshComponent->GetCollisionEnabled()),
			MeshComponent->IsPhysicsStateCreated() ? TEXT("true") : TEXT("false"),
			PhysicsAsset ? *PhysicsAsset->GetPathName() : TEXT("None"),
			ArmRootBody ? TEXT("true") : TEXT("false"),
			ArmRootBody && ArmRootBody->bSimulatePhysics && ArmRootBody->IsValidBodyInstance() ? TEXT("true") : TEXT("false"),
			HandBody ? TEXT("true") : TEXT("false"),
			HandBody && HandBody->bSimulatePhysics && HandBody->IsValidBodyInstance() ? TEXT("true") : TEXT("false"));
	}

	static void StartCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}

		if (UPhysAnimMvG102Subsystem* Subsystem = World->GetSubsystem<UPhysAnimMvG102Subsystem>())
		{
			Subsystem->StartControlPathTest();
		}
	}

	static void StopCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}

		if (UPhysAnimMvG102Subsystem* Subsystem = World->GetSubsystem<UPhysAnimMvG102Subsystem>())
		{
			Subsystem->StopControlPathTest(true);
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs StartMvG102Cmd(
		TEXT("PhysAnim.MVG102.Start"),
		TEXT("Starts the MV-G1-02 Physics Control left-arm response test in PIE."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StartCommand));

	static FAutoConsoleCommandWithWorldAndArgs StopMvG102Cmd(
		TEXT("PhysAnim.MVG102.Stop"),
		TEXT("Stops the MV-G1-02 Physics Control left-arm response test in PIE."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StopCommand));
}

void UPhysAnimMvG102Subsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UPhysAnimMvG102Subsystem::Deinitialize()
{
	StopControlPathTest(true);
	Super::Deinitialize();
}

void UPhysAnimMvG102Subsystem::Tick(float DeltaTime)
{
	if (!bTestActive)
	{
		return;
	}

	UpdateControlTarget(DeltaTime);
}

TStatId UPhysAnimMvG102Subsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UPhysAnimMvG102Subsystem, STATGROUP_Tickables);
}

bool UPhysAnimMvG102Subsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UPhysAnimMvG102Subsystem::StartControlPathTest()
{
	ACharacter* const Character = ResolvePlayerCharacter();
	if (!Character)
	{
		LogStatus(TEXT("Start failed: no possessed player character was found. Run this in PIE inside /Game/ThirdPerson/Lvl_ThirdPerson."));
		return false;
	}

	USkeletalMeshComponent* const MeshComponent = ResolveCharacterMesh(Character);
	if (!MeshComponent)
	{
		LogStatus(TEXT("Start failed: the possessed player character does not expose a skeletal mesh."));
		return false;
	}

	if (MeshComponent->GetBoneIndex(PhysAnimMvG102::ArmRootBoneName) == INDEX_NONE ||
		MeshComponent->GetBoneIndex(PhysAnimMvG102::HandBoneName) == INDEX_NONE)
	{
		LogStatus(FString::Printf(
			TEXT("Start failed: expected mannequin bones '%s' and '%s' were not found on mesh '%s'."),
			*PhysAnimMvG102::ArmRootBoneName.ToString(),
			*PhysAnimMvG102::HandBoneName.ToString(),
			*MeshComponent->GetName()));
		return false;
	}

	if (!EnsureHarnessCreated(Character, MeshComponent))
	{
		return false;
	}

	ControlledCharacter = Character;
	ControlledMesh = MeshComponent;
	InitialHandLocation = Character->GetTransform().InverseTransformPosition(
		MeshComponent->GetSocketLocation(PhysAnimMvG102::HandBoneName));
	InitialHandOrientation = Character->GetTransform().InverseTransformRotation(
		MeshComponent->GetSocketQuaternion(PhysAnimMvG102::HandBoneName)).Rotator();
	ElapsedTimeSeconds = 0.0f;
	bTestActive = true;

	ActivateVisualPhysicsState(MeshComponent);
	PhysAnimMvG102::LogMeshDiagnostics(TEXT("after visual activation"), MeshComponent);

	ControlComponent->SetBodyModifiersInSetMovementType(
		PhysAnimMvG102::BodyModifierSetName,
		EPhysicsMovementType::Simulated);
	ControlComponent->SetBodyModifiersInSetPhysicsBlendWeight(
		PhysAnimMvG102::BodyModifierSetName,
		1.0f);
	ResetControlTarget();
	ControlComponent->UpdateControls(0.0f);
	PhysAnimMvG102::LogMeshDiagnostics(TEXT("after body modifiers"), MeshComponent);

	const FString MeshPath = MeshComponent->GetSkinnedAsset()
		? MeshComponent->GetSkinnedAsset()->GetPathName()
		: MeshComponent->GetName();

	LogStatus(FString::Printf(
		TEXT("MV-G1-02 started on mesh '%s'. Expected first-moving region: left arm / left hand. Command: PhysAnim.MVG102.Start"),
		*MeshPath));

	return true;
}

void UPhysAnimMvG102Subsystem::StopControlPathTest(bool bResetTarget)
{
	if (!ControlComponent)
	{
		bTestActive = false;
		return;
	}

	bTestActive = false;
	ElapsedTimeSeconds = 0.0f;

	if (bResetTarget)
	{
		ResetControlTarget();
	}

	if (USkeletalMeshComponent* const MeshComponent = ControlledMesh.Get())
	{
		ResetVisualPhysicsState(MeshComponent);
	}

	ControlComponent->SetBodyModifiersInSetMovementType(
		PhysAnimMvG102::BodyModifierSetName,
		EPhysicsMovementType::Default);

	LogStatus(TEXT("MV-G1-02 stopped."));
}

ACharacter* UPhysAnimMvG102Subsystem::ResolvePlayerCharacter() const
{
	UWorld* const World = GetWorld();
	return World ? UGameplayStatics::GetPlayerCharacter(World, 0) : nullptr;
}

USkeletalMeshComponent* UPhysAnimMvG102Subsystem::ResolveCharacterMesh(ACharacter* Character) const
{
	if (!Character)
	{
		return nullptr;
	}

	if (USkeletalMeshComponent* const CharacterMesh = Character->GetMesh())
	{
		return CharacterMesh;
	}

	return Character->FindComponentByClass<USkeletalMeshComponent>();
}

bool UPhysAnimMvG102Subsystem::EnsureHarnessCreated(ACharacter* Character, USkeletalMeshComponent* MeshComponent)
{
	if (!Character || !MeshComponent)
	{
		return false;
	}

	UPhysicsControlComponent* ExistingComponent = Character->FindComponentByClass<UPhysicsControlComponent>();
	if (!ExistingComponent)
	{
		ExistingComponent = NewObject<UPhysicsControlComponent>(Character, TEXT("MVG102PhysicsControlComponent"));
		Character->AddOwnedComponent(ExistingComponent);
		ExistingComponent->RegisterComponent();
		ExistingComponent->AttachToComponent(Character->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	}

	ControlComponent = ExistingComponent;
	ControlComponent->SetComponentTickEnabled(true);
	ControlComponent->RegisterAllComponentTickFunctions(true);

	ControlComponent->DestroyControl(PhysAnimMvG102::ControlName, true, false);
	ControlComponent->DestroyBodyModifier(PhysAnimMvG102::BodyModifierSetName, false, true);

	FPhysicsControlModifierData BodyModifierData;
	BodyModifierData.MovementType = EPhysicsMovementType::Simulated;
	BodyModifierData.CollisionType = ECollisionEnabled::QueryAndPhysics;
	BodyModifierData.GravityMultiplier = 1.0f;
	BodyModifierData.PhysicsBlendWeight = 1.0f;
	BodyModifierData.KinematicTargetSpace = EPhysicsControlKinematicTargetSpace::OffsetInBoneSpace;
	BodyModifierData.bUpdateKinematicFromSimulation = true;

	const TArray<FName> BodyModifierNames = ControlComponent->CreateBodyModifiersFromSkeletalMeshBelow(
		MeshComponent,
		PhysAnimMvG102::ArmRootBoneName,
		true,
		PhysAnimMvG102::BodyModifierSetName,
		BodyModifierData);

	if (BodyModifierNames.IsEmpty())
	{
		LogStatus(TEXT("Harness creation failed: no left-arm body modifiers were created."));
		return false;
	}

	FPhysicsControlData ControlData;
	ControlData.bEnabled = true;
	ControlData.LinearStrength = 2200.0f;
	ControlData.LinearDampingRatio = 1.35f;
	ControlData.LinearExtraDamping = 110.0f;
	ControlData.MaxForce = 1400.0f;
	ControlData.AngularStrength = 500.0f;
	ControlData.AngularDampingRatio = 1.2f;
	ControlData.AngularExtraDamping = 40.0f;
	ControlData.MaxTorque = 800.0f;
	ControlData.LinearTargetVelocityMultiplier = 0.0f;
	ControlData.AngularTargetVelocityMultiplier = 0.0f;
	ControlData.bUseSkeletalAnimation = false;
	ControlData.bDisableCollision = true;
	ControlData.bOnlyControlChildObject = true;

	if (!ControlComponent->CreateNamedControl(
		PhysAnimMvG102::ControlName,
		nullptr,
		NAME_None,
		MeshComponent,
		PhysAnimMvG102::HandBoneName,
		ControlData,
		FPhysicsControlTarget(),
		PhysAnimMvG102::ControlSetName))
	{
		LogStatus(TEXT("Harness creation failed: the left-hand world-space control could not be created."));
		return false;
	}

	return true;
}

void UPhysAnimMvG102Subsystem::ActivateVisualPhysicsState(USkeletalMeshComponent* MeshComponent)
{
	if (!MeshComponent)
	{
		return;
	}

	PhysAnimMvG102::OriginalCollisionStates.FindOrAdd(MeshComponent) =
		{ MeshComponent->GetCollisionProfileName(), MeshComponent->GetCollisionEnabled() };

	MeshComponent->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	MeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComponent->RecreatePhysicsState();
	MeshComponent->SetEnablePhysicsBlending(true);
	MeshComponent->SetAllBodiesBelowSimulatePhysics(PhysAnimMvG102::ArmRootBoneName, true, true);
	MeshComponent->SetAllBodiesBelowPhysicsBlendWeight(PhysAnimMvG102::ArmRootBoneName, 1.0f, false, true);
	MeshComponent->WakeAllRigidBodies();
}

void UPhysAnimMvG102Subsystem::ResetVisualPhysicsState(USkeletalMeshComponent* MeshComponent)
{
	if (!MeshComponent)
	{
		return;
	}

	if (MeshComponent->IsPhysicsStateCreated())
	{
		MeshComponent->SetAllBodiesBelowPhysicsBlendWeight(PhysAnimMvG102::ArmRootBoneName, 0.0f, false, true);
		MeshComponent->SetAllBodiesBelowSimulatePhysics(PhysAnimMvG102::ArmRootBoneName, false, true);
		MeshComponent->PutAllRigidBodiesToSleep();
	}

	MeshComponent->SetEnablePhysicsBlending(false);
	if (const PhysAnimMvG102::FMeshCollisionState* const OriginalState =
		PhysAnimMvG102::OriginalCollisionStates.Find(MeshComponent))
	{
		MeshComponent->SetCollisionProfileName(OriginalState->ProfileName);
		MeshComponent->SetCollisionEnabled(OriginalState->CollisionEnabled);
		PhysAnimMvG102::OriginalCollisionStates.Remove(MeshComponent);
	}
}

void UPhysAnimMvG102Subsystem::UpdateControlTarget(float DeltaTime)
{
	ACharacter* const Character = ControlledCharacter.Get();
	USkeletalMeshComponent* const MeshComponent = ControlledMesh.Get();
	if (!Character || !MeshComponent || !ControlComponent)
	{
		StopControlPathTest(false);
		LogStatus(TEXT("MV-G1-02 stopped early because the controlled character or mesh disappeared."));
		return;
	}

	ElapsedTimeSeconds += DeltaTime;

	const float Phase = FMath::Sin(ElapsedTimeSeconds * PhysAnimMvG102::OscillationHz * 2.0f * PI);
	const FTransform CharacterTransform = Character->GetActorTransform();
	const FVector BaseHandLocation = CharacterTransform.TransformPosition(InitialHandLocation);
	const FRotator BaseHandOrientation =
		CharacterTransform.TransformRotation(InitialHandOrientation.Quaternion()).Rotator();
	const FVector TargetLocation =
		BaseHandLocation +
		(Character->GetActorUpVector() * (PhysAnimMvG102::VerticalAmplitudeCm * Phase)) +
		(Character->GetActorForwardVector() * (PhysAnimMvG102::ForwardAmplitudeCm * Phase));

	ControlComponent->SetControlTargetPositionAndOrientation(
		PhysAnimMvG102::ControlName,
		TargetLocation,
		BaseHandOrientation,
		DeltaTime,
		true,
		true,
		true,
		false);

	if (ElapsedTimeSeconds >= PhysAnimMvG102::TestDurationSeconds)
	{
		StopControlPathTest(true);
		LogStatus(TEXT("MV-G1-02 completed its 30 second window."));
	}
}

void UPhysAnimMvG102Subsystem::ResetControlTarget()
{
	if (!ControlComponent)
	{
		return;
	}

	ControlComponent->SetControlTargetPositionAndOrientation(
		PhysAnimMvG102::ControlName,
		ControlledCharacter.IsValid()
			? ControlledCharacter->GetActorTransform().TransformPosition(InitialHandLocation)
			: InitialHandLocation,
		ControlledCharacter.IsValid()
			? ControlledCharacter->GetActorTransform().TransformRotation(InitialHandOrientation.Quaternion()).Rotator()
			: InitialHandOrientation,
		0.0f,
		true,
		true,
		true,
		false);
}

void UPhysAnimMvG102Subsystem::LogStatus(const FString& Message) const
{
	UE_LOG(LogTemp, Display, TEXT("[MV-G1-02] %s"), *Message);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			4.0f,
			FColor::Cyan,
			FString::Printf(TEXT("[MV-G1-02] %s"), *Message));
	}
}
