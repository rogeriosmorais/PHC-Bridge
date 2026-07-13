#if WITH_DEV_AUTOMATION_TESTS

#include "PhysAnimComponent.h"
#include "PhysAnimStage1InitializerComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/AutomationTest.h"

namespace
{
	using namespace PhysAnimBridge;

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimStage1InitializerDefaultsTest,
		"PhysAnim.Component.Stage1InitializerDefaults",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimStage1InitializerDefaultsTest::RunTest(const FString& Parameters)
	{
		UPhysAnimStage1InitializerComponent* const Initializer = NewObject<UPhysAnimStage1InitializerComponent>();
		TestNotNull(TEXT("Stage 1 initializer should exist"), Initializer);
		if (!Initializer) return false;

		TestEqual(TEXT("Bring-up group count"), UPhysAnimComponent::GetBringUpGroupCount(), 5);
		TestEqual(TEXT("Manny initializer creates every controlled joint"), Initializer->InitialControls.Num(), 21);
		for (TPair<FName, FInitialPhysicsControl>& Pair : Initializer->InitialControls)
		{
			TestEqual(*FString::Printf(TEXT("%s uses an 8 Hz angular spring"), *Pair.Key.ToString()), Pair.Value.ControlData.AngularStrength, 8.0f);
			TestEqual(*FString::Printf(TEXT("%s starts critically damped"), *Pair.Key.ToString()), Pair.Value.ControlData.AngularDampingRatio, 1.0f);
			TestEqual(*FString::Printf(TEXT("%s uses bounded extra damping"), *Pair.Key.ToString()), Pair.Value.ControlData.AngularExtraDamping, 1.0f);
		}
		FInitialPhysicsControl* const MutatedControl = Initializer->InitialControls.Find(PhysAnimBridge::MakeControlName(TEXT("thigh_l")));
		TestNotNull(TEXT("Thigh control exists for runtime contract check"), MutatedControl);
		if (MutatedControl)
		{
			MutatedControl->ControlData.AngularStrength = 800.0f;
			Initializer->PrepareRuntimeDefaults();
			TestEqual(TEXT("Runtime defaults replace stale serialized strength"), MutatedControl->ControlData.AngularStrength, 8.0f);
		}
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimMannyConstraintInventoryTest,
		"PhysAnim.Component.MannyConstraintInventory",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimMannyConstraintInventoryTest::RunTest(const FString& Parameters)
	{
		const UPhysicsAsset* const PhysicsAsset = LoadObject<UPhysicsAsset>(nullptr, TEXT("/Game/Characters/Mannequins/Rigs/PA_Mannequin.PA_Mannequin"));
		TestNotNull(TEXT("Manny physics asset should load"), PhysicsAsset);
		if (!PhysicsAsset)
		{
			return false;
		}

		static const TPair<FName, FName> RequiredJointPairs[] =
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
		for (const TPair<FName, FName>& Pair : RequiredJointPairs)
		{
			const int32 ParentBodyIndex = PhysicsAsset->FindBodyIndex(Pair.Key);
			const int32 ChildBodyIndex = PhysicsAsset->FindBodyIndex(Pair.Value);
			TestTrue(
				*FString::Printf(TEXT("Required parent body exists: %s"), *Pair.Key.ToString()),
				PhysicsAsset->SkeletalBodySetups.IsValidIndex(ParentBodyIndex));
			TestTrue(
				*FString::Printf(TEXT("Required child body exists: %s"), *Pair.Value.ToString()),
				PhysicsAsset->SkeletalBodySetups.IsValidIndex(ChildBodyIndex));
			if (PhysicsAsset->SkeletalBodySetups.IsValidIndex(ParentBodyIndex) &&
				PhysicsAsset->SkeletalBodySetups.IsValidIndex(ChildBodyIndex))
			{
				TestFalse(
					*FString::Printf(TEXT("Adjacent plant bodies do not collide: %s/%s"), *Pair.Key.ToString(), *Pair.Value.ToString()),
					PhysicsAsset->IsCollisionEnabled(ParentBodyIndex, ChildBodyIndex));
			}

			const UPhysicsConstraintTemplate* ConstraintTemplate = nullptr;
			for (const UPhysicsConstraintTemplate* Candidate : PhysicsAsset->ConstraintSetup)
			{
				if (Candidate && Candidate->DefaultInstance.ConstraintBone1 == Pair.Value)
				{
					ConstraintTemplate = Candidate;
					break;
				}
			}
			TestNotNull(
				*FString::Printf(TEXT("Required plant constraint exists: %s/%s"), *Pair.Key.ToString(), *Pair.Value.ToString()),
				ConstraintTemplate);
			if (ConstraintTemplate)
			{
				const FConstraintInstance& Constraint = ConstraintTemplate->DefaultInstance;
				TestFalse(
					*FString::Printf(TEXT("Physics Asset linear position drive is disabled: %s/%s"), *Pair.Key.ToString(), *Pair.Value.ToString()),
					Constraint.IsLinearPositionDriveEnabled());
				TestFalse(
					*FString::Printf(TEXT("Physics Asset linear velocity drive is disabled: %s/%s"), *Pair.Key.ToString(), *Pair.Value.ToString()),
					Constraint.IsLinearVelocityDriveEnabled());
				TestFalse(
					*FString::Printf(TEXT("Physics Asset angular orientation drive is disabled: %s/%s"), *Pair.Key.ToString(), *Pair.Value.ToString()),
					Constraint.IsAngularOrientationDriveEnabled());
				TestTrue(
					*FString::Printf(TEXT("Physics Asset angular velocity target is passive: %s/%s"), *Pair.Key.ToString(), *Pair.Value.ToString()),
					Constraint.ProfileInstance.AngularDrive.AngularVelocityTarget.IsNearlyZero());
			}
		}

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimMannyMassInventoryTest,
		"PhysAnim.Component.MannyMassInventory",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimMannyMassInventoryTest::RunTest(const FString& Parameters)
	{
		USkeletalMesh* const MannyMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
		TestNotNull(TEXT("Manny skeletal mesh should load"), MannyMesh);
		return MannyMesh != nullptr;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimMannyPassivePlantDampingContractTest,
		"PhysAnim.Component.MannyPassivePlantDampingContract",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimMannyPassivePlantDampingContractTest::RunTest(const FString& Parameters)
	{
		const UPhysicsAsset* const PhysicsAsset = LoadObject<UPhysicsAsset>(nullptr, TEXT("/Game/Characters/Mannequins/Rigs/PA_Mannequin.PA_Mannequin"));
		TestNotNull(TEXT("Manny physics asset should load"), PhysicsAsset);
		if (!PhysicsAsset)
		{
			return false;
		}

		for (const FName BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
		{
			const int32 BodyIndex = PhysicsAsset->FindBodyIndex(BoneName);
			TestTrue(*FString::Printf(TEXT("Required body exists: %s"), *BoneName.ToString()), PhysicsAsset->SkeletalBodySetups.IsValidIndex(BodyIndex));
			if (!PhysicsAsset->SkeletalBodySetups.IsValidIndex(BodyIndex) || !PhysicsAsset->SkeletalBodySetups[BodyIndex])
			{
				continue;
			}

			const FBodyInstance& Body = PhysicsAsset->SkeletalBodySetups[BodyIndex]->DefaultInstance;
			AddInfo(FString::Printf(
				TEXT("PASSIVE_PLANT_DAMPING body=%s linear=%.3f angular=%.3f"),
				*BoneName.ToString(),
				Body.LinearDamping,
				Body.AngularDamping));
			TestTrue(
				*FString::Printf(TEXT("%s linear damping is authored at 30.0"), *BoneName.ToString()),
				FMath::IsNearlyEqual(Body.LinearDamping, 30.0f));
			TestTrue(
				*FString::Printf(TEXT("%s angular damping is authored at 100.0"), *BoneName.ToString()),
				FMath::IsNearlyEqual(Body.AngularDamping, 100.0f));
		}
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimMannyPassivePlantBallInertiaContractTest,
		"PhysAnim.Component.MannyPassivePlantBallInertiaContract",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimMannyPassivePlantBallInertiaContractTest::RunTest(const FString& Parameters)
	{
		const UPhysicsAsset* const PhysicsAsset = LoadObject<UPhysicsAsset>(nullptr, TEXT("/Game/Characters/Mannequins/Rigs/PA_Mannequin.PA_Mannequin"));
		TestNotNull(TEXT("Manny physics asset should load"), PhysicsAsset);
		if (!PhysicsAsset) return false;

		static const FName BallBones[] = { TEXT("ball_l"), TEXT("ball_r") };
		for (const FName BallBone : BallBones)
		{
			const int32 BodyIndex = PhysicsAsset->FindBodyIndex(BallBone);
			TestTrue(*FString::Printf(TEXT("Ball body exists: %s"), *BallBone.ToString()), PhysicsAsset->SkeletalBodySetups.IsValidIndex(BodyIndex));
			if (!PhysicsAsset->SkeletalBodySetups.IsValidIndex(BodyIndex) || !PhysicsAsset->SkeletalBodySetups[BodyIndex]) continue;
			const FBodyInstance& Body = PhysicsAsset->SkeletalBodySetups[BodyIndex]->DefaultInstance;
			TestTrue(
				*FString::Printf(TEXT("%s uses the small-body inertia stabilizer"), *BallBone.ToString()),
				Body.InertiaTensorScale.Equals(FVector(3.0), KINDA_SMALL_NUMBER));
			TestTrue(
				*FString::Printf(TEXT("%s retains automatic inertia conditioning"), *BallBone.ToString()),
				Body.IsInertiaConditioningEnabled());
		}
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimMannyPassivePlantMaterialContractTest,
		"PhysAnim.Component.MannyPassivePlantMaterialContract",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimMannyPassivePlantMaterialContractTest::RunTest(const FString& Parameters)
	{
		const UPhysicsAsset* const PhysicsAsset = LoadObject<UPhysicsAsset>(nullptr, TEXT("/Game/Characters/Mannequins/Rigs/PA_Mannequin.PA_Mannequin"));
		TestNotNull(TEXT("Manny physics asset should load"), PhysicsAsset);
		if (!PhysicsAsset) return false;
		for (const FName BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
		{
			const int32 BodyIndex = PhysicsAsset->FindBodyIndex(BoneName);
			if (!PhysicsAsset->SkeletalBodySetups.IsValidIndex(BodyIndex) || !PhysicsAsset->SkeletalBodySetups[BodyIndex]) continue;
			const FBodyInstance& Body = PhysicsAsset->SkeletalBodySetups[BodyIndex]->DefaultInstance;
			const UPhysicalMaterial* const Material = Body.GetSimplePhysicalMaterial();
			TestNotNull(*FString::Printf(TEXT("%s resolves a physical material"), *BoneName.ToString()), Material);
			if (!Material) continue;
			TestEqual(
				*FString::Printf(TEXT("%s uses the project passive-plant material"), *BoneName.ToString()),
				Material->GetPathName(),
				FString(TEXT("/Game/Characters/Mannequins/Rigs/PM_PhysAnimPassivePlant.PM_PhysAnimPassivePlant")));
			TestTrue(
				*FString::Printf(TEXT("%s has zero restitution"), *BoneName.ToString()),
				FMath::IsNearlyZero(Material->Restitution));
			TestTrue(
				*FString::Printf(TEXT("%s overrides restitution combination"), *BoneName.ToString()),
				Material->bOverrideRestitutionCombineMode);
			TestEqual(
				*FString::Printf(TEXT("%s uses minimum restitution combination"), *BoneName.ToString()),
				Material->RestitutionCombineMode,
				EFrictionCombineMode::Min);
		}
		return true;
	}

}

#endif
