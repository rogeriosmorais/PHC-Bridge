#include "PhysAnimStage1InitializerComponent.h"

#include "PhysAnimBridge.h"

#include "Components/MeshComponent.h"
#include "PhysicsControlComponent.h"

namespace PhysAnimStage1InitializerComponentInternal
{
	const FName CharacterMeshComponentName(TEXT("CharacterMesh0"));

	struct FControlBonePair
	{
		FName ParentBone;
		FName ChildBone;
	};

	const TArray<FControlBonePair>& GetStage1ControlBonePairs()
	{
		static const TArray<FControlBonePair> Pairs =
		{
			{ TEXT("pelvis"), TEXT("thigh_l") },
			{ TEXT("thigh_l"), TEXT("calf_l") },
			{ TEXT("calf_l"), TEXT("foot_l") },
			{ TEXT("foot_l"), TEXT("ball_l") },
			{ TEXT("pelvis"), TEXT("thigh_r") },
			{ TEXT("thigh_r"), TEXT("calf_r") },
			{ TEXT("calf_r"), TEXT("foot_r") },
			{ TEXT("foot_r"), TEXT("ball_r") },
			{ TEXT("pelvis"), TEXT("spine_01") },
			{ TEXT("spine_01"), TEXT("spine_02") },
			{ TEXT("spine_02"), TEXT("spine_03") },
			{ TEXT("spine_03"), TEXT("neck_01") },
			{ TEXT("neck_01"), TEXT("head") },
			{ TEXT("spine_03"), TEXT("clavicle_l") },
			{ TEXT("clavicle_l"), TEXT("upperarm_l") },
			{ TEXT("upperarm_l"), TEXT("lowerarm_l") },
			{ TEXT("lowerarm_l"), TEXT("hand_l") },
			{ TEXT("spine_03"), TEXT("clavicle_r") },
			{ TEXT("clavicle_r"), TEXT("upperarm_r") },
			{ TEXT("upperarm_r"), TEXT("lowerarm_r") },
			{ TEXT("lowerarm_r"), TEXT("hand_r") }
		};

		return Pairs;
	}

	FPhysicsControlData MakeDefaultStage1ControlData()
	{
		FPhysicsControlData Data;
		Data.bEnabled = false;
		Data.LinearStrength = 0.0f;
		Data.LinearDampingRatio = 1.0f;
		Data.LinearExtraDamping = 0.0f;
		Data.MaxForce = 0.0f;
		// Physics Control strength is an oscillation frequency, not a raw spring
		// coefficient. Keep the authored plant within the solver's temporal range.
		Data.AngularStrength = 8.0f;
		Data.AngularDampingRatio = 1.0f;
		Data.AngularExtraDamping = 50.0f;
		Data.MaxTorque = 0.0f;
		Data.LinearTargetVelocityMultiplier = 0.0f;
		Data.AngularTargetVelocityMultiplier = 0.0f;
		Data.CustomControlPoint = FVector::ZeroVector;
		Data.bUseCustomControlPoint = false;
		Data.bUseSkeletalAnimation = false;
		Data.bDisableCollision = true;
		Data.bOnlyControlChildObject = true;
		return Data;
	}

	FPhysicsControlModifierData MakeDefaultStage1BodyModifierData()
	{
		FPhysicsControlModifierData Data;
		Data.MovementType = EPhysicsMovementType::Kinematic;
		Data.CollisionType = ECollisionEnabled::NoCollision;
		Data.GravityMultiplier = 1.0f;
		Data.PhysicsBlendWeight = 0.0f;
		Data.KinematicTargetSpace = EPhysicsControlKinematicTargetSpace::OffsetInBoneSpace;
		Data.bUpdateKinematicFromSimulation = false;
		return Data;
	}

	UMeshComponent* ResolveMeshComponent(AActor* Actor, const FName MeshComponentName)
	{
		if (!Actor)
		{
			return nullptr;
		}

		if (MeshComponentName != NAME_None)
		{
			for (UActorComponent* Component : Actor->GetComponents())
			{
				if (Component->GetFName() == MeshComponentName)
				{
					return Cast<UMeshComponent>(Component);
				}
			}
			return nullptr;
		}

		return Cast<UMeshComponent>(Actor->GetRootComponent());
	}
}

UPhysAnimStage1InitializerComponent::UPhysAnimStage1InitializerComponent()
{
	bCreateControlsAtBeginPlay = false;
	InitialControls.Reset();
	InitialBodyModifiers.Reset();

	const FPhysicsControlData DefaultControlData =
		PhysAnimStage1InitializerComponentInternal::MakeDefaultStage1ControlData();
	for (const PhysAnimStage1InitializerComponentInternal::FControlBonePair& Pair :
		PhysAnimStage1InitializerComponentInternal::GetStage1ControlBonePairs())
	{
		FInitialPhysicsControl InitialControl;
		InitialControl.ParentMeshComponentName = PhysAnimStage1InitializerComponentInternal::CharacterMeshComponentName;
		InitialControl.ParentBoneName = Pair.ParentBone;
		InitialControl.ChildMeshComponentName = PhysAnimStage1InitializerComponentInternal::CharacterMeshComponentName;
		InitialControl.ChildBoneName = Pair.ChildBone;
		InitialControl.ControlData = DefaultControlData;

		InitialControls.Add(PhysAnimBridge::MakeControlName(Pair.ChildBone), InitialControl);
	}

	const FPhysicsControlModifierData DefaultBodyModifierData =
		PhysAnimStage1InitializerComponentInternal::MakeDefaultStage1BodyModifierData();
	for (const FName BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
	{
		FInitialBodyModifier InitialBodyModifier;
		InitialBodyModifier.MeshComponentName = PhysAnimStage1InitializerComponentInternal::CharacterMeshComponentName;
		InitialBodyModifier.BoneName = BoneName;
		InitialBodyModifier.BodyModifierData = DefaultBodyModifierData;

		InitialBodyModifiers.Add(PhysAnimBridge::MakeBodyModifierName(BoneName), InitialBodyModifier);
	}
}

void UPhysAnimStage1InitializerComponent::BeginPlay()
{
	Super::BeginPlay();
	PrepareRuntimeDefaults();
}

void UPhysAnimStage1InitializerComponent::CreateControls(UPhysicsControlComponent* PhysicsControlComponent)
{
	if (!PhysicsControlComponent)
	{
		return;
	}

	PrepareRuntimeDefaults();
	PhysicsControlComponent->RegisterAllComponentTickFunctions(true);
	CreateOrUpdateInitialControls(PhysicsControlComponent);
	CreateOrUpdateInitialBodyModifiers(PhysicsControlComponent);
}

void UPhysAnimStage1InitializerComponent::PrepareRuntimeDefaults()
{
	AActor* const OwnerActor = GetOwner();
	AActor* const ControlParentActor = DefaultControlParentActor.IsValid() ? DefaultControlParentActor.Get() : OwnerActor;
	AActor* const ControlChildActor = DefaultControlChildActor.IsValid() ? DefaultControlChildActor.Get() : OwnerActor;
	AActor* const BodyModifierActor = DefaultBodyModifierActor.IsValid() ? DefaultBodyModifierActor.Get() : OwnerActor;
	const FPhysicsControlData DefaultControlData =
		PhysAnimStage1InitializerComponentInternal::MakeDefaultStage1ControlData();

	for (TPair<FName, FInitialPhysicsControl>& Pair : InitialControls)
	{
		FInitialPhysicsControl& InitialControl = Pair.Value;
		// Blueprint instances can retain obsolete serialized spring values. The
		// Stage 1 runtime initializer owns one deterministic Manny actuator contract.
		InitialControl.ControlData = DefaultControlData;
		if (!InitialControl.ParentActor.IsValid() && ControlParentActor)
		{
			InitialControl.ParentActor = ControlParentActor;
		}
		if (InitialControl.ParentMeshComponentName.IsNone())
		{
			InitialControl.ParentMeshComponentName = PhysAnimStage1InitializerComponentInternal::CharacterMeshComponentName;
		}
		if (!InitialControl.ChildActor.IsValid() && ControlChildActor)
		{
			InitialControl.ChildActor = ControlChildActor;
		}
		if (InitialControl.ChildMeshComponentName.IsNone())
		{
			InitialControl.ChildMeshComponentName = PhysAnimStage1InitializerComponentInternal::CharacterMeshComponentName;
		}
	}

	for (TPair<FName, FInitialBodyModifier>& Pair : InitialBodyModifiers)
	{
		FInitialBodyModifier& InitialBodyModifier = Pair.Value;
		if (!InitialBodyModifier.Actor.IsValid() && BodyModifierActor)
		{
			InitialBodyModifier.Actor = BodyModifierActor;
		}
		if (InitialBodyModifier.MeshComponentName.IsNone())
		{
			InitialBodyModifier.MeshComponentName = PhysAnimStage1InitializerComponentInternal::CharacterMeshComponentName;
		}
	}
}

void UPhysAnimStage1InitializerComponent::CreateOrUpdateInitialControls(UPhysicsControlComponent* PhysicsControlComponent)
{
	for (const TPair<FName, FInitialPhysicsControl>& InitialPhysicsControlPair : InitialControls)
	{
		const FName Name = InitialPhysicsControlPair.Key;
		const FInitialPhysicsControl& InitialPhysicsControl = InitialPhysicsControlPair.Value;

		UMeshComponent* const ChildMeshComponent = PhysAnimStage1InitializerComponentInternal::ResolveMeshComponent(
			InitialPhysicsControl.ChildActor.Get(),
			InitialPhysicsControl.ChildMeshComponentName);
		if (!ChildMeshComponent)
		{
			continue;
		}

		UMeshComponent* const ParentMeshComponent = PhysAnimStage1InitializerComponentInternal::ResolveMeshComponent(
			InitialPhysicsControl.ParentActor.Get(),
			InitialPhysicsControl.ParentMeshComponentName);

		const TArray<FName> Sets = PhysicsControlComponent->GetSetsContainingControl(Name);
		if (PhysicsControlComponent->GetControlExists(Name))
		{
			PhysicsControlComponent->DestroyControl(Name);
		}

		if (PhysicsControlComponent->CreateNamedControl(
			Name,
			ParentMeshComponent,
			InitialPhysicsControl.ParentBoneName,
			ChildMeshComponent,
			InitialPhysicsControl.ChildBoneName,
			InitialPhysicsControl.ControlData,
			InitialPhysicsControl.ControlTarget,
			TEXT("All")))
		{
			FPhysicsControlNames Names;
			for (const FName Set : Sets)
			{
				PhysicsControlComponent->AddControlToSet(Names, Name, Set);
			}
		}
	}
}

void UPhysAnimStage1InitializerComponent::CreateOrUpdateInitialBodyModifiers(UPhysicsControlComponent* PhysicsControlComponent)
{
	for (const TPair<FName, FInitialBodyModifier>& InitialBodyModifierPair : InitialBodyModifiers)
	{
		const FName Name = InitialBodyModifierPair.Key;
		const FInitialBodyModifier& InitialBodyModifier = InitialBodyModifierPair.Value;

		UMeshComponent* const MeshComponent = PhysAnimStage1InitializerComponentInternal::ResolveMeshComponent(
			InitialBodyModifier.Actor.Get(),
			InitialBodyModifier.MeshComponentName);
		if (!MeshComponent)
		{
			continue;
		}

		const TArray<FName> Sets = PhysicsControlComponent->GetSetsContainingBodyModifier(Name);
		if (PhysicsControlComponent->GetBodyModifierExists(Name))
		{
			PhysicsControlComponent->DestroyBodyModifier(Name);
		}

		if (PhysicsControlComponent->CreateNamedBodyModifier(
			Name,
			MeshComponent,
			InitialBodyModifier.BoneName,
			TEXT("All"),
			InitialBodyModifier.BodyModifierData))
		{
			FPhysicsControlNames Names;
			for (const FName Set : Sets)
			{
				PhysicsControlComponent->AddBodyModifierToSet(Names, Name, Set);
			}
		}
	}
}
