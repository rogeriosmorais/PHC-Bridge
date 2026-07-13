#if WITH_DEV_AUTOMATION_TESTS

#include "PhysAnimComponent.h"
#include "PhysAnimStage1InitializerComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
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
				*FString::Printf(TEXT("%s linear damping is authored at 1.0"), *BoneName.ToString()),
				FMath::IsNearlyEqual(Body.LinearDamping, 1.0f));
			TestTrue(
				*FString::Printf(TEXT("%s angular damping is authored at 5.0"), *BoneName.ToString()),
				FMath::IsNearlyEqual(Body.AngularDamping, 5.0f));
		}
		return true;
	}

}

#endif
