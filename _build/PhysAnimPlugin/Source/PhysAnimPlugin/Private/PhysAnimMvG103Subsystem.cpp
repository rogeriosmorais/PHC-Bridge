#include "PhysAnimMvG103Subsystem.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsControlComponent.h"
#include "PhysicsControlData.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Stats/Stats.h"

namespace PhysAnimMvG103
{
	static const FName BodyModifierSetName(TEXT("MVG103_LeftElbowPose"));
	static const FName LowerarmModifierName(TEXT("MVG103_LowerarmModifier"));
	static const FName HandModifierName(TEXT("MVG103_HandModifier"));
	static const FName ElbowControlName(TEXT("MVG103_LeftElbowControl"));
	static const FName HandControlName(TEXT("MVG103_LeftHandNeutralControl"));
	static const FName ControlSetName(TEXT("MVG103_LeftArmControls"));
	static const FName UpperarmBoneName(TEXT("upperarm_l"));
	static const FName LowerarmBoneName(TEXT("lowerarm_l"));
	static const FName HandBoneName(TEXT("hand_l"));

	static constexpr float TotalDurationSeconds = 10.0f;
	static constexpr float RampInEndSeconds = 2.0f;
	static constexpr float HoldEndSeconds = 6.0f;
	static constexpr float RampOutEndSeconds = 8.0f;
	static const FRotator ElbowFlexionOffset = FRotator(-50.0f, 0.0f, 0.0f);
	static const FRotator NeutralOffset = FRotator::ZeroRotator;

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
		const FBodyInstance* const UpperarmBody = MeshComponent->GetBodyInstance(UpperarmBoneName);
		const FBodyInstance* const LowerarmBody = MeshComponent->GetBodyInstance(LowerarmBoneName);
		const FBodyInstance* const HandBody = MeshComponent->GetBodyInstance(HandBoneName);

		UE_LOG(
			LogTemp,
			Display,
			TEXT("[MV-G1-03] %s | mesh=%s profile=%s collision=%s physics_state=%s physics_asset=%s upperarm_body=%s lowerarm_body=%s hand_body=%s"),
			Context,
			*MeshComponent->GetName(),
			*MeshComponent->GetCollisionProfileName().ToString(),
			*UEnum::GetValueAsString(MeshComponent->GetCollisionEnabled()),
			MeshComponent->IsPhysicsStateCreated() ? TEXT("true") : TEXT("false"),
			PhysicsAsset ? *PhysicsAsset->GetPathName() : TEXT("None"),
			UpperarmBody ? TEXT("true") : TEXT("false"),
			LowerarmBody ? TEXT("true") : TEXT("false"),
			HandBody ? TEXT("true") : TEXT("false"));
	}

	static bool ValidateRequiredPhysicsBodies(USkeletalMeshComponent* MeshComponent, FString& OutMissingBodies)
	{
		if (!MeshComponent)
		{
			OutMissingBodies = TEXT("no mesh");
			return false;
		}

		TArray<FString> MissingBodies;

		if (!MeshComponent->GetBodyInstance(UpperarmBoneName))
		{
			MissingBodies.Add(UpperarmBoneName.ToString());
		}

		if (!MeshComponent->GetBodyInstance(LowerarmBoneName))
		{
			MissingBodies.Add(LowerarmBoneName.ToString());
		}

		if (!MeshComponent->GetBodyInstance(HandBoneName))
		{
			MissingBodies.Add(HandBoneName.ToString());
		}

		if (MissingBodies.IsEmpty())
		{
			OutMissingBodies.Reset();
			return true;
		}

		OutMissingBodies = FString::Join(MissingBodies, TEXT(", "));
		return false;
	}

	static float ComputeElbowBlendAlpha(const float TimeSeconds)
	{
		if (TimeSeconds <= 0.0f)
		{
			return 0.0f;
		}

		if (TimeSeconds < RampInEndSeconds)
		{
			return FMath::InterpEaseInOut(0.0f, 1.0f, TimeSeconds / RampInEndSeconds, 2.0f);
		}

		if (TimeSeconds < HoldEndSeconds)
		{
			return 1.0f;
		}

		if (TimeSeconds < RampOutEndSeconds)
		{
			return FMath::InterpEaseInOut(1.0f, 0.0f, (TimeSeconds - HoldEndSeconds) / (RampOutEndSeconds - HoldEndSeconds), 2.0f);
		}

		return 0.0f;
	}

	static void StartCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}

		if (UPhysAnimMvG103Subsystem* Subsystem = World->GetSubsystem<UPhysAnimMvG103Subsystem>())
		{
			Subsystem->StartSmokeTest();
		}
	}

	static void StopCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}

		if (UPhysAnimMvG103Subsystem* Subsystem = World->GetSubsystem<UPhysAnimMvG103Subsystem>())
		{
			Subsystem->StopSmokeTest(true);
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs StartMvG103Cmd(
		TEXT("PhysAnim.MVG103.Start"),
		TEXT("Starts the MV-G1-03 stationary mapped-joint smoke test in PIE."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StartCommand));

	static FAutoConsoleCommandWithWorldAndArgs StopMvG103Cmd(
		TEXT("PhysAnim.MVG103.Stop"),
		TEXT("Stops the MV-G1-03 stationary mapped-joint smoke test in PIE."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StopCommand));
}

void UPhysAnimMvG103Subsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UPhysAnimMvG103Subsystem::Deinitialize()
{
	StopSmokeTest(true);
	Super::Deinitialize();
}

void UPhysAnimMvG103Subsystem::Tick(float DeltaTime)
{
	if (!bTestActive)
	{
		return;
	}

	UpdatePoseTargets(DeltaTime);
}

TStatId UPhysAnimMvG103Subsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UPhysAnimMvG103Subsystem, STATGROUP_Tickables);
}

bool UPhysAnimMvG103Subsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UPhysAnimMvG103Subsystem::StartSmokeTest()
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

	if (MeshComponent->GetBoneIndex(PhysAnimMvG103::UpperarmBoneName) == INDEX_NONE ||
		MeshComponent->GetBoneIndex(PhysAnimMvG103::LowerarmBoneName) == INDEX_NONE ||
		MeshComponent->GetBoneIndex(PhysAnimMvG103::HandBoneName) == INDEX_NONE)
	{
		LogStatus(TEXT("Start failed: expected mannequin arm bones were not found on the current mesh."));
		return false;
	}

	FString MissingBodies;
	if (!PhysAnimMvG103::ValidateRequiredPhysicsBodies(MeshComponent, MissingBodies))
	{
		LogStatus(FString::Printf(
			TEXT("Start failed: required mannequin physics bodies were not found on the current mesh (%s)."),
			*MissingBodies));
		PhysAnimMvG103::LogMeshDiagnostics(TEXT("missing required physics bodies"), MeshComponent);
		return false;
	}

	if (!EnsureHarnessCreated(Character, MeshComponent))
	{
		return false;
	}

	ControlledCharacter = Character;
	ControlledMesh = MeshComponent;
	ElapsedTimeSeconds = 0.0f;
	bTestActive = true;

	ActivateVisualPhysicsState(MeshComponent);
	PhysAnimMvG103::LogMeshDiagnostics(TEXT("after visual activation"), MeshComponent);

	// Parent-space controls with skeletal-animation targets need a warm bone cache before their
	// first UpdateControls call, otherwise the control component cannot resolve the authored pose.
	ControlComponent->UpdateTargetCaches(0.0f);

	ControlComponent->SetBodyModifiersInSetMovementType(
		PhysAnimMvG103::BodyModifierSetName,
		EPhysicsMovementType::Simulated);
	ControlComponent->SetBodyModifiersInSetPhysicsBlendWeight(
		PhysAnimMvG103::BodyModifierSetName,
		1.0f);

	ResetPoseTargets();
	ControlComponent->UpdateControls(0.0f);
	PhysAnimMvG103::LogMeshDiagnostics(TEXT("after body modifiers"), MeshComponent);

	LogStatus(TEXT("MV-G1-03 started. Frozen pose case: isolated left elbow flexion. Expected result: left elbow bends while the right arm stays neutral. Command: PhysAnim.MVG103.Start"));
	return true;
}

void UPhysAnimMvG103Subsystem::StopSmokeTest(bool bResetTargets)
{
	if (!ControlComponent)
	{
		bTestActive = false;
		return;
	}

	bTestActive = false;
	ElapsedTimeSeconds = 0.0f;

	if (bResetTargets)
	{
		ResetPoseTargets();
	}

	if (USkeletalMeshComponent* const MeshComponent = ControlledMesh.Get())
	{
		ResetVisualPhysicsState(MeshComponent);
	}

	ControlComponent->SetBodyModifiersInSetMovementType(
		PhysAnimMvG103::BodyModifierSetName,
		EPhysicsMovementType::Default);

	LogStatus(TEXT("MV-G1-03 stopped."));
}

ACharacter* UPhysAnimMvG103Subsystem::ResolvePlayerCharacter() const
{
	UWorld* const World = GetWorld();
	return World ? UGameplayStatics::GetPlayerCharacter(World, 0) : nullptr;
}

USkeletalMeshComponent* UPhysAnimMvG103Subsystem::ResolveCharacterMesh(ACharacter* Character) const
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

bool UPhysAnimMvG103Subsystem::EnsureHarnessCreated(ACharacter* Character, USkeletalMeshComponent* MeshComponent)
{
	if (!Character || !MeshComponent)
	{
		return false;
	}

	UPhysicsControlComponent* ExistingComponent = Character->FindComponentByClass<UPhysicsControlComponent>();
	if (!ExistingComponent)
	{
		ExistingComponent = NewObject<UPhysicsControlComponent>(Character, TEXT("MVG103PhysicsControlComponent"));
		Character->AddOwnedComponent(ExistingComponent);
		ExistingComponent->RegisterComponent();
		ExistingComponent->AttachToComponent(Character->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	}

	ControlComponent = ExistingComponent;
	ControlComponent->SetComponentTickEnabled(true);
	ControlComponent->RegisterAllComponentTickFunctions(true);

	ControlComponent->DestroyControl(PhysAnimMvG103::ElbowControlName, true, false);
	ControlComponent->DestroyControl(PhysAnimMvG103::HandControlName, true, false);
	ControlComponent->DestroyBodyModifier(PhysAnimMvG103::LowerarmModifierName, true, false);
	ControlComponent->DestroyBodyModifier(PhysAnimMvG103::HandModifierName, true, false);

	FPhysicsControlModifierData BodyModifierData;
	BodyModifierData.MovementType = EPhysicsMovementType::Simulated;
	BodyModifierData.CollisionType = ECollisionEnabled::QueryAndPhysics;
	BodyModifierData.GravityMultiplier = 1.0f;
	BodyModifierData.PhysicsBlendWeight = 1.0f;
	BodyModifierData.KinematicTargetSpace = EPhysicsControlKinematicTargetSpace::OffsetInBoneSpace;
	BodyModifierData.bUpdateKinematicFromSimulation = true;

	if (!ControlComponent->CreateNamedBodyModifier(
		PhysAnimMvG103::LowerarmModifierName,
		MeshComponent,
		PhysAnimMvG103::LowerarmBoneName,
		PhysAnimMvG103::BodyModifierSetName,
		BodyModifierData))
	{
		LogStatus(TEXT("Harness creation failed: the left lowerarm body modifier could not be created."));
		return false;
	}

	if (!ControlComponent->CreateNamedBodyModifier(
		PhysAnimMvG103::HandModifierName,
		MeshComponent,
		PhysAnimMvG103::HandBoneName,
		PhysAnimMvG103::BodyModifierSetName,
		BodyModifierData))
	{
		LogStatus(TEXT("Harness creation failed: the left hand body modifier could not be created."));
		return false;
	}

	FPhysicsControlData PoseControlData;
	PoseControlData.bEnabled = true;
	PoseControlData.LinearStrength = 0.0f;
	PoseControlData.LinearDampingRatio = 1.0f;
	PoseControlData.LinearExtraDamping = 0.0f;
	PoseControlData.MaxForce = 0.0f;
	PoseControlData.AngularStrength = 800.0f;
	PoseControlData.AngularDampingRatio = 1.25f;
	PoseControlData.AngularExtraDamping = 30.0f;
	PoseControlData.MaxTorque = 0.0f;
	PoseControlData.LinearTargetVelocityMultiplier = 0.0f;
	PoseControlData.AngularTargetVelocityMultiplier = 0.0f;
	PoseControlData.bUseSkeletalAnimation = true;
	PoseControlData.bDisableCollision = true;
	PoseControlData.bOnlyControlChildObject = true;

	if (!ControlComponent->CreateNamedControl(
		PhysAnimMvG103::ElbowControlName,
		MeshComponent,
		PhysAnimMvG103::UpperarmBoneName,
		MeshComponent,
		PhysAnimMvG103::LowerarmBoneName,
		PoseControlData,
		FPhysicsControlTarget(),
		PhysAnimMvG103::ControlSetName))
	{
		LogStatus(TEXT("Harness creation failed: the left elbow parent-space control could not be created."));
		return false;
	}

	FPhysicsControlData HandControlData = PoseControlData;
	HandControlData.AngularStrength = 600.0f;
	HandControlData.AngularExtraDamping = 25.0f;

	if (!ControlComponent->CreateNamedControl(
		PhysAnimMvG103::HandControlName,
		MeshComponent,
		PhysAnimMvG103::LowerarmBoneName,
		MeshComponent,
		PhysAnimMvG103::HandBoneName,
		HandControlData,
		FPhysicsControlTarget(),
		PhysAnimMvG103::ControlSetName))
	{
		LogStatus(TEXT("Harness creation failed: the left hand neutral parent-space control could not be created."));
		return false;
	}

	return true;
}

void UPhysAnimMvG103Subsystem::ActivateVisualPhysicsState(USkeletalMeshComponent* MeshComponent)
{
	if (!MeshComponent)
	{
		return;
	}

	PhysAnimMvG103::OriginalCollisionStates.FindOrAdd(MeshComponent) =
		{ MeshComponent->GetCollisionProfileName(), MeshComponent->GetCollisionEnabled() };

	MeshComponent->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	MeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComponent->RecreatePhysicsState();
	MeshComponent->SetEnablePhysicsBlending(true);
	MeshComponent->SetAllBodiesBelowSimulatePhysics(PhysAnimMvG103::LowerarmBoneName, true, true);
	MeshComponent->SetAllBodiesBelowPhysicsBlendWeight(PhysAnimMvG103::LowerarmBoneName, 1.0f, false, true);
	MeshComponent->WakeAllRigidBodies();
}

void UPhysAnimMvG103Subsystem::ResetVisualPhysicsState(USkeletalMeshComponent* MeshComponent)
{
	if (!MeshComponent)
	{
		return;
	}

	if (MeshComponent->IsPhysicsStateCreated())
	{
		MeshComponent->SetAllBodiesBelowPhysicsBlendWeight(PhysAnimMvG103::LowerarmBoneName, 0.0f, false, true);
		MeshComponent->SetAllBodiesBelowSimulatePhysics(PhysAnimMvG103::LowerarmBoneName, false, true);
		MeshComponent->PutAllRigidBodiesToSleep();
	}

	MeshComponent->SetEnablePhysicsBlending(false);

	if (const PhysAnimMvG103::FMeshCollisionState* const OriginalState =
		PhysAnimMvG103::OriginalCollisionStates.Find(MeshComponent))
	{
		MeshComponent->SetCollisionProfileName(OriginalState->ProfileName);
		MeshComponent->SetCollisionEnabled(OriginalState->CollisionEnabled);
		PhysAnimMvG103::OriginalCollisionStates.Remove(MeshComponent);
	}
}

void UPhysAnimMvG103Subsystem::UpdatePoseTargets(float DeltaTime)
{
	if (!ControlComponent)
	{
		StopSmokeTest(false);
		LogStatus(TEXT("MV-G1-03 stopped early because the Physics Control component disappeared."));
		return;
	}

	ElapsedTimeSeconds += DeltaTime;

	const float FlexionAlpha = PhysAnimMvG103::ComputeElbowBlendAlpha(ElapsedTimeSeconds);
	const FRotator ElbowTarget = FMath::Lerp(
		PhysAnimMvG103::NeutralOffset,
		PhysAnimMvG103::ElbowFlexionOffset,
		FlexionAlpha);

	ControlComponent->SetControlTargetOrientation(
		PhysAnimMvG103::ElbowControlName,
		ElbowTarget,
		DeltaTime,
		true,
		false,
		true,
		false);

	ControlComponent->SetControlTargetOrientation(
		PhysAnimMvG103::HandControlName,
		PhysAnimMvG103::NeutralOffset,
		DeltaTime,
		true,
		false,
		true,
		false);

	if (ElapsedTimeSeconds >= PhysAnimMvG103::TotalDurationSeconds)
	{
		StopSmokeTest(true);
		LogStatus(TEXT("MV-G1-03 completed its 10 second window."));
	}
}

void UPhysAnimMvG103Subsystem::ResetPoseTargets()
{
	if (!ControlComponent)
	{
		return;
	}

	ControlComponent->SetControlTargetOrientation(
		PhysAnimMvG103::ElbowControlName,
		PhysAnimMvG103::NeutralOffset,
		0.0f,
		true,
		false,
		true,
		false);

	ControlComponent->SetControlTargetOrientation(
		PhysAnimMvG103::HandControlName,
		PhysAnimMvG103::NeutralOffset,
		0.0f,
		true,
		false,
		true,
		false);
}

void UPhysAnimMvG103Subsystem::LogStatus(const FString& Message) const
{
	UE_LOG(LogTemp, Display, TEXT("[MV-G1-03] %s"), *Message);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			4.0f,
			FColor::Green,
			FString::Printf(TEXT("[MV-G1-03] %s"), *Message));
	}
}
