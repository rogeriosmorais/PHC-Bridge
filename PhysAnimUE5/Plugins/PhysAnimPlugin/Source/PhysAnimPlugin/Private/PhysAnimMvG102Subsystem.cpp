#include "PhysAnimMvG102Subsystem.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsControlComponent.h"
#include "PhysicsControlData.h"
#include "Stats/Stats.h"

namespace PhysAnimMvG102
{
	static const FName BodyModifierSetName(TEXT("MVG102_LeftArm"));
	static const FName ControlName(TEXT("MVG102_LeftHandWorld"));
	static const FName ControlSetName(TEXT("MVG102_LeftHandWorld"));
	static const FName ArmRootBoneName(TEXT("clavicle_l"));
	static const FName HandBoneName(TEXT("hand_l"));
	static constexpr float TestDurationSeconds = 30.0f;
	static constexpr float OscillationHz = 0.55f;
	static constexpr float VerticalAmplitudeCm = 18.0f;
	static constexpr float ForwardAmplitudeCm = 10.0f;

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
	InitialHandLocation = MeshComponent->GetSocketLocation(PhysAnimMvG102::HandBoneName);
	InitialHandOrientation = MeshComponent->GetSocketRotation(PhysAnimMvG102::HandBoneName);
	ElapsedTimeSeconds = 0.0f;
	bTestActive = true;

	ControlComponent->SetBodyModifiersInSetMovementType(
		PhysAnimMvG102::BodyModifierSetName,
		EPhysicsMovementType::Simulated);

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
	ControlData.LinearStrength = 1400.0f;
	ControlData.LinearDampingRatio = 1.2f;
	ControlData.LinearExtraDamping = 40.0f;
	ControlData.MaxForce = 0.0f;
	ControlData.AngularStrength = 400.0f;
	ControlData.AngularDampingRatio = 1.1f;
	ControlData.AngularExtraDamping = 10.0f;
	ControlData.MaxTorque = 0.0f;
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
	const FVector TargetLocation =
		InitialHandLocation +
		(Character->GetActorUpVector() * (PhysAnimMvG102::VerticalAmplitudeCm * Phase)) +
		(Character->GetActorForwardVector() * (PhysAnimMvG102::ForwardAmplitudeCm * Phase));

	ControlComponent->SetControlTargetPositionAndOrientation(
		PhysAnimMvG102::ControlName,
		TargetLocation,
		InitialHandOrientation,
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
		InitialHandLocation,
		InitialHandOrientation,
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
