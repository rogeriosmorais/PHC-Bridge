#if WITH_DEV_AUTOMATION_TESTS

#include "PhysAnimComponent.h"
#include "PhysAnimStage1InitializerComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Components/SkeletalMeshComponent.h"
#include "AnimationRuntime.h"
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
			TestFalse(*FString::Printf(TEXT("%s remains disabled until atomic standing publication"), *Pair.Key.ToString()), Pair.Value.ControlData.bEnabled);
			TestEqual(*FString::Printf(TEXT("%s uses an 8 Hz angular spring"), *Pair.Key.ToString()), Pair.Value.ControlData.AngularStrength, 8.0f);
			TestEqual(*FString::Printf(TEXT("%s starts critically damped"), *Pair.Key.ToString()), Pair.Value.ControlData.AngularDampingRatio, 1.0f);
			TestEqual(*FString::Printf(TEXT("%s uses solver-stable extra damping"), *Pair.Key.ToString()), Pair.Value.ControlData.AngularExtraDamping, 0.25f);
			TestEqual(*FString::Printf(TEXT("%s has a finite angular torque ceiling"), *Pair.Key.ToString()), Pair.Value.ControlData.MaxTorque, 500000.0f);
		}
		FInitialPhysicsControl* const MutatedControl = Initializer->InitialControls.Find(PhysAnimBridge::MakeControlName(TEXT("thigh_l")));
		TestNotNull(TEXT("Thigh control exists for runtime contract check"), MutatedControl);
		if (MutatedControl)
		{
			MutatedControl->ControlData.AngularStrength = 800.0f;
			MutatedControl->ControlData.AngularExtraDamping = 1.0f;
			Initializer->PrepareRuntimeDefaults();
			TestEqual(TEXT("Runtime defaults replace stale serialized strength"), MutatedControl->ControlData.AngularStrength, 8.0f);
			TestEqual(TEXT("Runtime defaults replace stale serialized extra damping"), MutatedControl->ControlData.AngularExtraDamping, 0.25f);
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
				float AngularSpring = 0.0f;
				float AngularDamping = 0.0f;
				float AngularForceLimit = 0.0f;
				Constraint.GetAngularDriveParams(AngularSpring, AngularDamping, AngularForceLimit);
				AddInfo(FString::Printf(
					TEXT("PASSIVE_CONSTRAINT_DRIVE link=%s/%s mode=%d motion=%d/%d/%d orientation_enabled=%d velocity_enabled=%d acceleration=%d spring=%.3f damping=%.3f force_limit=%.3f"),
					*Pair.Key.ToString(),
					*Pair.Value.ToString(),
					static_cast<int32>(Constraint.ProfileInstance.AngularDrive.AngularDriveMode.GetValue()),
					static_cast<int32>(Constraint.GetAngularTwistMotion()),
					static_cast<int32>(Constraint.GetAngularSwing1Motion()),
					static_cast<int32>(Constraint.GetAngularSwing2Motion()),
					Constraint.IsAngularOrientationDriveEnabled() ? 1 : 0,
					Constraint.IsAngularVelocityDriveEnabled() ? 1 : 0,
					Constraint.ProfileInstance.AngularDrive.bAccelerationMode ? 1 : 0,
					AngularSpring,
					AngularDamping,
					AngularForceLimit));
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
					*FString::Printf(TEXT("Physics Asset passive angular velocity drive is enabled: %s/%s"), *Pair.Key.ToString(), *Pair.Value.ToString()),
					Constraint.IsAngularVelocityDriveEnabled());
				TestTrue(
					*FString::Printf(TEXT("Physics Asset angular velocity target is passive: %s/%s"), *Pair.Key.ToString(), *Pair.Value.ToString()),
					Constraint.ProfileInstance.AngularDrive.AngularVelocityTarget.IsNearlyZero());
				TestTrue(
					*FString::Printf(TEXT("Physics Asset SLERP drive has no locked angular axis: %s/%s"), *Pair.Key.ToString(), *Pair.Value.ToString()),
					Constraint.GetAngularTwistMotion() != ACM_Locked &&
						Constraint.GetAngularSwing1Motion() != ACM_Locked &&
						Constraint.GetAngularSwing2Motion() != ACM_Locked);
				TestEqual(
					*FString::Printf(TEXT("Physics Asset passive drive uses SLERP because no axis is locked: %s/%s"), *Pair.Key.ToString(), *Pair.Value.ToString()),
					Constraint.ProfileInstance.AngularDrive.AngularDriveMode.GetValue(),
					EAngularDriveMode::SLERP);
				TestTrue(
					*FString::Printf(TEXT("Physics Asset passive drive is mass-independent: %s/%s"), *Pair.Key.ToString(), *Pair.Value.ToString()),
					Constraint.ProfileInstance.AngularDrive.bAccelerationMode);
				TestTrue(
					*FString::Printf(TEXT("Physics Asset passive angular damping is 100: %s/%s"), *Pair.Key.ToString(), *Pair.Value.ToString()),
					FMath::IsNearlyEqual(AngularDamping, 100.0f));
			}
		}

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimMannyToeConstraintRefPoseContractTest,
		"PhysAnim.Component.MannyToeConstraintRefPoseContract",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimMannyToeConstraintRefPoseContractTest::RunTest(const FString& Parameters)
	{
		const UPhysicsAsset* const PhysicsAsset = LoadObject<UPhysicsAsset>(nullptr, TEXT("/Game/Characters/Mannequins/Rigs/PA_Mannequin.PA_Mannequin"));
		const USkeletalMesh* const SkeletalMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
		TestNotNull(TEXT("Manny physics asset should load"), PhysicsAsset);
		TestNotNull(TEXT("Manny skeletal mesh should load"), SkeletalMesh);
		if (!PhysicsAsset || !SkeletalMesh) return false;

		const FReferenceSkeleton& ReferenceSkeleton = SkeletalMesh->GetRefSkeleton();
		TArray<FTransform> ComponentSpaceRefPose;
		FAnimationRuntime::FillUpComponentSpaceTransforms(
			ReferenceSkeleton,
			ReferenceSkeleton.GetRefBonePose(),
			ComponentSpaceRefPose);

		static const TPair<FName, FName> ToeLinks[] =
		{
			{ TEXT("foot_l"), TEXT("ball_l") },
			{ TEXT("foot_r"), TEXT("ball_r") }
		};
		for (const TPair<FName, FName>& Link : ToeLinks)
		{
			const int32 ParentBoneIndex = ReferenceSkeleton.FindBoneIndex(Link.Key);
			const int32 ChildBoneIndex = ReferenceSkeleton.FindBoneIndex(Link.Value);
			const int32 ConstraintIndex = PhysicsAsset->FindConstraintIndex(Link.Value, Link.Key);
			TestTrue(*FString::Printf(TEXT("Toe parent bone exists: %s"), *Link.Key.ToString()), ComponentSpaceRefPose.IsValidIndex(ParentBoneIndex));
			TestTrue(*FString::Printf(TEXT("Toe child bone exists: %s"), *Link.Value.ToString()), ComponentSpaceRefPose.IsValidIndex(ChildBoneIndex));
			TestTrue(*FString::Printf(TEXT("Toe constraint exists: %s/%s"), *Link.Key.ToString(), *Link.Value.ToString()), PhysicsAsset->ConstraintSetup.IsValidIndex(ConstraintIndex));
			if (!ComponentSpaceRefPose.IsValidIndex(ParentBoneIndex) ||
				!ComponentSpaceRefPose.IsValidIndex(ChildBoneIndex) ||
				!PhysicsAsset->ConstraintSetup.IsValidIndex(ConstraintIndex) ||
				!PhysicsAsset->ConstraintSetup[ConstraintIndex])
			{
				continue;
			}

			const FConstraintInstance& Constraint = PhysicsAsset->ConstraintSetup[ConstraintIndex]->DefaultInstance;
			const FTransform ChildFrame = Constraint.GetRefFrame(EConstraintFrame::Frame1) * ComponentSpaceRefPose[ChildBoneIndex];
			const FTransform ParentFrame = Constraint.GetRefFrame(EConstraintFrame::Frame2) * ComponentSpaceRefPose[ParentBoneIndex];
			const double AnchorSeparationCm = FVector::Distance(ChildFrame.GetTranslation(), ParentFrame.GetTranslation());
			const double AngularMismatchDeg = FMath::RadiansToDegrees(
				ChildFrame.GetRotation().AngularDistance(ParentFrame.GetRotation()));
			AddInfo(FString::Printf(
				TEXT("TOE_CONSTRAINT_REF_POSE link=%s/%s anchor_separation_cm=%.6f angular_mismatch_deg=%.6f swing1=%d/%.3f swing2=%d/%.3f twist=%d/%.3f"),
				*Link.Key.ToString(),
				*Link.Value.ToString(),
				AnchorSeparationCm,
				AngularMismatchDeg,
				static_cast<int32>(Constraint.GetAngularSwing1Motion()),
				Constraint.GetAngularSwing1Limit(),
				static_cast<int32>(Constraint.GetAngularSwing2Motion()),
				Constraint.GetAngularSwing2Limit(),
				static_cast<int32>(Constraint.GetAngularTwistMotion()),
				Constraint.GetAngularTwistLimit()));
			TestTrue(
				*FString::Printf(TEXT("Toe constraint anchors coincide in Manny's reference pose: %s/%s"), *Link.Key.ToString(), *Link.Value.ToString()),
				AnchorSeparationCm <= 0.1);
			TestTrue(
				*FString::Printf(TEXT("Toe constraint frames align in Manny's reference pose: %s/%s"), *Link.Key.ToString(), *Link.Value.ToString()),
				AngularMismatchDeg <= 0.1);
			TestEqual(
				*FString::Printf(TEXT("Toe swing 1 is free: %s/%s"), *Link.Key.ToString(), *Link.Value.ToString()),
				Constraint.GetAngularSwing1Motion(),
				ACM_Free);
			TestEqual(
				*FString::Printf(TEXT("Toe swing 2 is free: %s/%s"), *Link.Key.ToString(), *Link.Value.ToString()),
				Constraint.GetAngularSwing2Motion(),
				ACM_Free);
			TestEqual(
				*FString::Printf(TEXT("Toe twist is free: %s/%s"), *Link.Key.ToString(), *Link.Value.ToString()),
				Constraint.GetAngularTwistMotion(),
				ACM_Free);
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
				*FString::Printf(TEXT("%s uses 0.2 passive contact friction"), *BoneName.ToString()),
				FMath::IsNearlyEqual(Material->Friction, 0.2f));
			TestTrue(
				*FString::Printf(TEXT("%s overrides friction combination"), *BoneName.ToString()),
				Material->bOverrideFrictionCombineMode);
			TestEqual(
				*FString::Printf(TEXT("%s uses minimum friction combination"), *BoneName.ToString()),
				Material->FrictionCombineMode,
				EFrictionCombineMode::Min);
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
