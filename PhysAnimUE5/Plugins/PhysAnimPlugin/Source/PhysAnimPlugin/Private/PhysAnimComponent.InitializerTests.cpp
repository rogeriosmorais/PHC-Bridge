#if WITH_DEV_AUTOMATION_TESTS

#include "PhysAnimComponent.h"
#include "PhysAnimStage1InitializerComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
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
		UPhysicsAsset* const PhysicsAsset = LoadObject<UPhysicsAsset>(nullptr, TEXT("/Game/Characters/Mannequins/Rigs/PA_Mannequin.PA_Mannequin"));
		TestNotNull(TEXT("Manny physics asset should load"), PhysicsAsset);
		return PhysicsAsset != nullptr;
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
}

#endif
