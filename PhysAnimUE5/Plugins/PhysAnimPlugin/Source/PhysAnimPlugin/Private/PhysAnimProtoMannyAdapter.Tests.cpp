#if WITH_DEV_AUTOMATION_TESTS

#include "PhysAnimProtoMannyAdapter.h"

#include "PhysAnimBridge.h"
#include "PhysAnimComponent.h"

#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Misc/AutomationTest.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"

namespace
{
	float SignedTwistDegrees(const FQuat& Rotation)
	{
		FQuat Swing;
		FQuat Twist;
		Rotation.GetNormalized().ToSwingTwist(FVector::ForwardVector, Swing, Twist);
		Twist.Normalize();
		float AngleRadians = Twist.GetAngle();
		if (AngleRadians > UE_PI)
		{
			AngleRadians -= 2.0f * UE_PI;
		}
		if (Twist.X < 0.0f)
		{
			AngleRadians = -AngleRadians;
		}
		return FMath::RadiansToDegrees(AngleRadians);
	}

	FVector2D SwingDegrees(const FQuat& Rotation)
	{
		FQuat Swing;
		FQuat Twist;
		Rotation.GetNormalized().ToSwingTwist(FVector::ForwardVector, Swing, Twist);
		Swing.Normalize();
		return FVector2D(
			FMath::RadiansToDegrees(4.0f * FMath::Atan2(Swing.Y, 1.0f + Swing.W)),
			FMath::RadiansToDegrees(4.0f * FMath::Atan2(Swing.Z, 1.0f + Swing.W)));
	}

	FQuat BuildSwing(float Swing2Degrees, float Swing1Degrees)
	{
		const float TanY = FMath::Tan(FMath::DegreesToRadians(Swing2Degrees) * 0.25f);
		const float TanZ = FMath::Tan(FMath::DegreesToRadians(Swing1Degrees) * 0.25f);
		const float RadiusSquared = TanY * TanY + TanZ * TanZ;
		const float Denominator = 1.0f + RadiusSquared;
		return FQuat(
			0.0f,
			2.0f * TanY / Denominator,
			2.0f * TanZ / Denominator,
			(1.0f - RadiusSquared) / Denominator).GetNormalized();
	}

	bool QuaternionsNear(const FQuat& A, const FQuat& B, float ToleranceDegrees = 0.01f)
	{
		return FMath::RadiansToDegrees(A.AngularDistance(B)) <= ToleranceDegrees;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimProtoMannyRotationFrameTest,
	"PhysAnim.Adapter.ProtoMannyRotationFrame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimProtoMannyRotationFrameTest::RunTest(const FString& Parameters)
{
	static const int32 ParentIndices[PhysAnimBridge::NumSmplBodies] =
	{
		INDEX_NONE, 0, 1, 2, 3, 0, 5, 6, 7, 0, 9, 10,
		11, 12, 11, 14, 15, 16, 17, 11, 19, 20, 21, 22
	};

	TArray<FQuat> BindBoneRotations;
	TArray<FQuat> BindBodyRotations;
	TArray<FQuat> CurrentBodyRotations;
	TArray<FQuat> ExpectedLocalRotations;
	BindBoneRotations.Reserve(PhysAnimBridge::NumSmplBodies);
	BindBodyRotations.Reserve(PhysAnimBridge::NumSmplBodies);
	CurrentBodyRotations.Reserve(PhysAnimBridge::NumSmplBodies);
	ExpectedLocalRotations.Init(FQuat::Identity, PhysAnimBridge::NumSmplBodies);

	for (int32 BodyIndex = 0; BodyIndex < PhysAnimBridge::NumSmplBodies; ++BodyIndex)
	{
		const FVector Axis = FVector(1.0 + BodyIndex, 2.0, 3.0).GetSafeNormal();
		BindBoneRotations.Add(FQuat(Axis, FMath::DegreesToRadians(BodyIndex * 1.25f)).GetNormalized());
		BindBodyRotations.Add(FQuat(Axis, FMath::DegreesToRadians(15.0f + BodyIndex * 2.0f)).GetNormalized());
	}

	const FQuat MeshWorldRotation(FVector::UpVector, FMath::DegreesToRadians(37.0f));
	const FQuat RootMotionDelta(FVector(1.0f, 2.0f, 0.5f).GetSafeNormal(), FMath::DegreesToRadians(23.0f));
	const FQuat CurrentRootBoneComponentRotation =
		(RootMotionDelta * BindBoneRotations[0]).GetNormalized();
	TestTrue(
		TEXT("The live policy root remains in PoseSearch's motion/bone frame"),
		QuaternionsNear(
			PhysAnimProtoMannyAdapter::BuildCanonicalSmplRootRotationFromBonePose(
				MeshWorldRotation,
				BindBoneRotations[0],
				CurrentRootBoneComponentRotation),
			(MeshWorldRotation * RootMotionDelta).GetNormalized()));

	const FQuat CanonicalKneeFlexion(FVector::RightVector, FMath::DegreesToRadians(82.0f));
	ExpectedLocalRotations[2] = CanonicalKneeFlexion;
	CurrentBodyRotations = BindBodyRotations;
	for (int32 BodyIndex = 1; BodyIndex < PhysAnimBridge::NumSmplBodies; ++BodyIndex)
	{
		const int32 ParentIndex = ParentIndices[BodyIndex];
		const FQuat ParentBind = BindBodyRotations[ParentIndex];
		const FQuat BindRelative =
			(ParentBind.Inverse() * BindBodyRotations[BodyIndex]).GetNormalized();
		const FQuat CurrentRelative =
			(ParentBind.Inverse() * ExpectedLocalRotations[BodyIndex] * ParentBind * BindRelative).GetNormalized();
		CurrentBodyRotations[BodyIndex] =
			(CurrentBodyRotations[ParentIndex] * CurrentRelative).GetNormalized();
	}

	const FQuat RecoveredKnee = PhysAnimProtoMannyAdapter::RecoverCanonicalJointRotation(
		BindBodyRotations[1],
		(BindBodyRotations[1].Inverse() * BindBodyRotations[2]).GetNormalized(),
		(CurrentBodyRotations[1].Inverse() * CurrentBodyRotations[2]).GetNormalized());
	TestTrue(
		TEXT("The observation adapter exactly inverts the action-side bind-frame composition"),
		QuaternionsNear(RecoveredKnee, CanonicalKneeFlexion));

	const FQuat RootCanonicalRotation(FVector::UpVector, FMath::DegreesToRadians(31.0f));
	TArray<FQuat> BodyPoseCanonicalRotations;
	FString Error;
	TestTrue(
		TEXT("Live Manny body frames rebuild a canonical SMPL rotation tree"),
		PhysAnimProtoMannyAdapter::BuildCanonicalSmplRotationsFromBodyPose(
			RootCanonicalRotation,
			BindBodyRotations,
			CurrentBodyRotations,
			BodyPoseCanonicalRotations,
			Error));
	TestTrue(TEXT("Body-frame reconstruction reports no error"), Error.IsEmpty());
	TestTrue(
		TEXT("The canonical knee global rotation retains its canonical flexion axis"),
		QuaternionsNear(
			BodyPoseCanonicalRotations[2],
			(RootCanonicalRotation * CanonicalKneeFlexion).GetNormalized()));

	TArray<FQuat> SampleBoneRotations;
	SampleBoneRotations.Reserve(PhysAnimBridge::NumSmplBodies);
	for (int32 BodyIndex = 0; BodyIndex < PhysAnimBridge::NumSmplBodies; ++BodyIndex)
	{
		const FQuat BoneToBody =
			(BindBoneRotations[BodyIndex].Inverse() * BindBodyRotations[BodyIndex]).GetNormalized();
		SampleBoneRotations.Add(
			(CurrentBodyRotations[BodyIndex] * BoneToBody.Inverse()).GetNormalized());
	}

	TArray<FQuat> BonePoseCanonicalRotations;
	TestTrue(
		TEXT("PoseSearch bone frames pass through captured Manny body frames"),
		PhysAnimProtoMannyAdapter::BuildCanonicalSmplRotationsFromBonePose(
			RootCanonicalRotation,
			BindBoneRotations,
			SampleBoneRotations,
			BindBodyRotations,
			BonePoseCanonicalRotations,
			Error));
	TestTrue(TEXT("Bone-frame reconstruction reports no error"), Error.IsEmpty());
	TestTrue(
		TEXT("PoseSearch and live-physics paths produce the same canonical SMPL rotations"),
		BonePoseCanonicalRotations.Num() == BodyPoseCanonicalRotations.Num());
	for (int32 BodyIndex = 0;
		BodyIndex < FMath::Min(BonePoseCanonicalRotations.Num(), BodyPoseCanonicalRotations.Num());
		++BodyIndex)
	{
		TestTrue(
			*FString::Printf(TEXT("Canonical body %d agrees across both adapter paths"), BodyIndex),
			QuaternionsNear(BonePoseCanonicalRotations[BodyIndex], BodyPoseCanonicalRotations[BodyIndex]));
	}

	TArray<FQuat> InvalidRotations;
	Error.Reset();
	TestFalse(
		TEXT("Incomplete bind calibration cannot silently enter the policy observation"),
		PhysAnimProtoMannyAdapter::BuildCanonicalSmplRotationsFromBodyPose(
			RootCanonicalRotation,
			InvalidRotations,
			CurrentBodyRotations,
			BodyPoseCanonicalRotations,
			Error));
	TestFalse(TEXT("Incomplete rotation calibration rejection is explicit"), Error.IsEmpty());

	const FQuat RootBodyComponentRotation(FVector::ForwardVector, FMath::DegreesToRadians(11.0f));
	const FQuat RootBodyWorldRotation =
		(MeshWorldRotation * RootBodyComponentRotation).GetNormalized();
	TMap<FName, FPhysAnimControlTargetSeed> TPoseSeeds;
	const TArray<FName>& ControlledBones = PhysAnimBridge::GetControlledBoneNames();
	for (int32 BoneIndex = 0; BoneIndex < ControlledBones.Num(); ++BoneIndex)
	{
		const FQuat ChildComponentRotation(
			FVector(1.0, 2.0 + BoneIndex, 3.0).GetSafeNormal(),
			FMath::DegreesToRadians(2.0f * BoneIndex));
		const FQuat ChildWorldRotation =
			(MeshWorldRotation * ChildComponentRotation).GetNormalized();
		TPoseSeeds.Add(
			PhysAnimBridge::MakeControlName(ControlledBones[BoneIndex]),
			UPhysAnimComponent::BuildCurrentPoseControlTargetSeed(
				RootBodyWorldRotation,
				ChildWorldRotation));
	}

	TArray<FQuat> BodyComponentRotations;
	Error.Reset();
	TestTrue(
		TEXT("Synchronized T-pose Physics Control seeds produce the 24 policy body frames"),
		UPhysAnimComponent::BuildSmplBindBodyComponentRotations(
			MeshWorldRotation,
			TPoseSeeds,
			BodyComponentRotations,
			Error));
	TestTrue(TEXT("Synchronized body-frame calibration reports no error"), Error.IsEmpty());
	TestEqual(
		TEXT("The synchronized body-frame calibration preserves the SMPL body count"),
		BodyComponentRotations.Num(),
		PhysAnimBridge::NumSmplBodies);
	TestTrue(
		TEXT("The pelvis bind body is recovered from a child's parent seed in component space"),
		BodyComponentRotations.IsValidIndex(0) &&
			QuaternionsNear(BodyComponentRotations[0], RootBodyComponentRotation));
	TestTrue(
		TEXT("Collapsed left wrist and hand entries share the same synchronized body frame"),
		BodyComponentRotations.IsValidIndex(18) &&
			QuaternionsNear(BodyComponentRotations[17], BodyComponentRotations[18]));

	TPoseSeeds.Remove(PhysAnimBridge::MakeControlName(TEXT("calf_l")));
	Error.Reset();
	TestFalse(
		TEXT("An incomplete synchronized body-frame calibration cannot enter policy observations"),
		UPhysAnimComponent::BuildSmplBindBodyComponentRotations(
			MeshWorldRotation,
			TPoseSeeds,
			BodyComponentRotations,
			Error));
	TestFalse(TEXT("Missing synchronized body-frame calibration is explicit"), Error.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimProtoMannyConstraintProjectionTest,
	"PhysAnim.Adapter.ProtoMannyConstraintProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimProtoMannyConstraintProjectionTest::RunTest(const FString& Parameters)
{
	UPhysicsAsset* const PhysicsAsset = NewObject<UPhysicsAsset>();
	UPhysicsConstraintTemplate* const ConstraintTemplate = NewObject<UPhysicsConstraintTemplate>(PhysicsAsset);
	ConstraintTemplate->DefaultInstance.ConstraintBone1 = TEXT("calf_l");
	ConstraintTemplate->DefaultInstance.ConstraintBone2 = TEXT("thigh_l");
	ConstraintTemplate->DefaultInstance.SetRefFrame(
		EConstraintFrame::Frame1,
		FTransform(FQuat(FVector::RightVector, FMath::DegreesToRadians(-15.0f))));
	ConstraintTemplate->DefaultInstance.SetRefFrame(
		EConstraintFrame::Frame2,
		FTransform(FQuat(FVector::UpVector, FMath::DegreesToRadians(25.0f))));
	ConstraintTemplate->DefaultInstance.SetAngularTwistLimit(ACM_Limited, 22.0f);
	ConstraintTemplate->DefaultInstance.SetAngularSwing1Limit(ACM_Locked, 0.0f);
	ConstraintTemplate->DefaultInstance.SetAngularSwing2Limit(ACM_Limited, 17.0f);
	PhysicsAsset->ConstraintSetup.Add(ConstraintTemplate);

	FPhysAnimMannyConstraintProfile ExtractedProfile;
	TestTrue(
		TEXT("A direct Manny constraint produces an adapter profile"),
		PhysAnimProtoMannyAdapter::BuildConstraintProfile(
			PhysicsAsset,
			TEXT("calf_l"),
			TEXT("thigh_l"),
			ExtractedProfile));
	TestEqual(TEXT("The authored twist limit is preserved"), ExtractedProfile.TwistLimitDegrees, 22.0f);
	TestEqual(TEXT("The authored Swing1 motion is preserved"), ExtractedProfile.Swing1Motion, ACM_Locked);
	TestEqual(TEXT("The authored Swing2 limit is preserved"), ExtractedProfile.Swing2LimitDegrees, 17.0f);
	TestFalse(
		TEXT("A missing direct constraint does not invent an adapter profile"),
		PhysAnimProtoMannyAdapter::BuildConstraintProfile(
			PhysicsAsset,
			TEXT("neck_01"),
			TEXT("spine_03"),
			ExtractedProfile));

	FPhysAnimMannyConstraintProfile Profile;
	Profile.TwistMotion = ACM_Limited;
	Profile.Swing1Motion = ACM_Limited;
	Profile.Swing2Motion = ACM_Limited;
	Profile.TwistLimitDegrees = 20.0f;
	Profile.Swing1LimitDegrees = 30.0f;
	Profile.Swing2LimitDegrees = 40.0f;

	const FQuat HalfRangeProtoTwist(
		FVector::ForwardVector,
		FMath::DegreesToRadians(90.0f));
	const FQuat HalfRangeMannyTwist =
		PhysAnimProtoMannyAdapter::MapProtoPolicyTargetToMannyConstraintRange(
			HalfRangeProtoTwist,
			FQuat::Identity,
			Profile);
	TestEqual(
		TEXT("Half of Proto's twist range maps to half of Manny's twist range"),
		SignedTwistDegrees(HalfRangeMannyTwist),
		10.0f,
		0.02f);

	const FQuat OffsetBindTwist(
		FVector::ForwardVector,
		FMath::DegreesToRadians(-15.0f));
	const FQuat OffsetBindHalfRangeTarget =
		(HalfRangeProtoTwist * OffsetBindTwist).GetNormalized();
	const FQuat OffsetBindHalfRangeResult =
		PhysAnimProtoMannyAdapter::MapProtoPolicyTargetToMannyConstraintRange(
			OffsetBindHalfRangeTarget,
			OffsetBindTwist,
			Profile);
	TestEqual(
		TEXT("Positive Proto occupancy uses the positive range available from Manny's asymmetric bind"),
		SignedTwistDegrees(OffsetBindHalfRangeResult),
		2.5f,
		0.02f);

	const FQuat HalfRangeProtoSwing2 = BuildSwing(90.0f, 0.0f);
	const FQuat HalfRangeMannySwing2 =
		PhysAnimProtoMannyAdapter::MapProtoPolicyTargetToMannyConstraintRange(
			HalfRangeProtoSwing2,
			FQuat::Identity,
			Profile);
	TestEqual(
		TEXT("Half of Proto's Swing2 range maps to half of Manny's Swing2 range"),
		SwingDegrees(HalfRangeMannySwing2).X,
		20.0,
		0.02);

	const FQuat IdentityResult = PhysAnimProtoMannyAdapter::AdaptParentRelativeTarget(
		FQuat::Identity,
		FQuat::Identity,
		Profile);
	TestTrue(TEXT("Identity remains identity"), QuaternionsNear(IdentityResult, FQuat::Identity));

	const FQuat InRangeProtoTarget = (
		BuildSwing(20.0f, 15.0f) *
		FQuat(FVector::ForwardVector, FMath::DegreesToRadians(10.0f))).GetNormalized();
	const FQuat InRangeResult = PhysAnimProtoMannyAdapter::AdaptParentRelativeTarget(
		InRangeProtoTarget,
		FQuat::Identity,
		Profile);
	TestEqual(TEXT("An in-range Proto twist angle is preserved"), SignedTwistDegrees(InRangeResult), 10.0f, 0.02f);
	TestEqual(TEXT("An in-range Proto Swing2 angle is preserved"), SwingDegrees(InRangeResult).X, 20.0, 0.02);
	TestEqual(TEXT("An in-range Proto Swing1 angle is preserved"), SwingDegrees(InRangeResult).Y, 15.0, 0.02);

	const FQuat OutsideTarget = (
		BuildSwing(60.0f, 0.0f) *
		FQuat(FVector::ForwardVector, FMath::DegreesToRadians(50.0f))).GetNormalized();
	const FQuat OutsideResult = PhysAnimProtoMannyAdapter::AdaptParentRelativeTarget(
		OutsideTarget,
		FQuat::Identity,
		Profile);
	TestEqual(TEXT("An out-of-range Proto twist is clamped to Manny's twist limit"), SignedTwistDegrees(OutsideResult), 20.0f, 0.02f);
	TestEqual(TEXT("An out-of-range Proto Swing2 is clamped to Manny's Swing2 limit"), SwingDegrees(OutsideResult).X, 40.0, 0.02);

	const FQuat CombinedSwingTarget = BuildSwing(127.2792f, 127.2792f);
	const FQuat CombinedSwingResult = PhysAnimProtoMannyAdapter::AdaptParentRelativeTarget(
		CombinedSwingTarget,
		FQuat::Identity,
		Profile);
	const FVector2D CombinedSwing = SwingDegrees(CombinedSwingResult);
	const float SwingOccupancy = FMath::Sqrt(
		FMath::Square(CombinedSwing.X / Profile.Swing2LimitDegrees) +
		FMath::Square(CombinedSwing.Y / Profile.Swing1LimitDegrees));
	TestEqual(TEXT("Combined swing is projected onto the authored elliptical cone"), SwingOccupancy, 1.0f, 0.002f);

	Profile.ParentConstraintFrameRotation = FQuat(FVector::UpVector, FMath::DegreesToRadians(35.0f));
	Profile.ChildConstraintFrameRotation = FQuat(FVector::RightVector, FMath::DegreesToRadians(-20.0f));
	const FQuat FramedBindNeutral = (
		Profile.ParentConstraintFrameRotation *
		Profile.ChildConstraintFrameRotation.Inverse()).GetNormalized();
	const FQuat ConstraintSpaceTarget = (
		BuildSwing(20.0f, 0.0f) *
		FQuat(FVector::ForwardVector, FMath::DegreesToRadians(10.0f))).GetNormalized();
	const FQuat ParentRelativeTarget = (
		Profile.ParentConstraintFrameRotation *
		ConstraintSpaceTarget *
		Profile.ChildConstraintFrameRotation.Inverse()).GetNormalized();
	const FQuat FramedResult = PhysAnimProtoMannyAdapter::AdaptParentRelativeTarget(
		ParentRelativeTarget,
		FramedBindNeutral,
		Profile);
	const FQuat ResultInConstraintSpace = (
		Profile.ParentConstraintFrameRotation.Inverse() *
		FramedResult *
		Profile.ChildConstraintFrameRotation).GetNormalized();
	TestEqual(TEXT("Constraint-frame twist angle is preserved"), SignedTwistDegrees(ResultInConstraintSpace), 10.0f, 0.02f);
	TestEqual(TEXT("Constraint-frame Swing2 angle is preserved"), SwingDegrees(ResultInConstraintSpace).X, 20.0, 0.02);

	Profile.ParentConstraintFrameRotation = FQuat::Identity;
	Profile.ChildConstraintFrameRotation = FQuat::Identity;
	Profile.Swing1Motion = ACM_Locked;
	const FQuat LockedSwingResult = PhysAnimProtoMannyAdapter::AdaptParentRelativeTarget(
		BuildSwing(0.0f, 25.0f),
		FQuat::Identity,
		Profile);
	TestEqual(TEXT("A locked Swing1 axis is removed"), SwingDegrees(LockedSwingResult).Y, 0.0, 0.02);

	const FQuat DisabledResult = PhysAnimProtoMannyAdapter::AdaptParentRelativeTarget(
		OutsideTarget,
		FQuat::Identity,
		Profile,
		false);
	TestTrue(TEXT("The disabled adapter preserves the raw target"), QuaternionsNear(DisabledResult, OutsideTarget));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimProtoMannyKneeAxisContractTest,
	"PhysAnim.Adapter.ProtoMannyKneeAxis",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimProtoMannyKneeAxisContractTest::RunTest(const FString& Parameters)
{
	const UPhysicsAsset* const PhysicsAsset = LoadObject<UPhysicsAsset>(
		nullptr,
		TEXT("/Game/Characters/Mannequins/Rigs/PA_Mannequin.PA_Mannequin"));
	const USkeletalMesh* const SkeletalMesh = LoadObject<USkeletalMesh>(
		nullptr,
		TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	UAnimSequence* const TPoseReference = LoadObject<UAnimSequence>(
		nullptr,
		TEXT("/Game/Characters/Mannequins/Animations/AS_Manny_TPose_1F.AS_Manny_TPose_1F"));
	TestNotNull(TEXT("Manny physics asset should load"), PhysicsAsset);
	TestNotNull(TEXT("Manny skeletal mesh should load"), SkeletalMesh);
	TestNotNull(TEXT("The adapter fixture uses the same one-frame Manny T-pose as startup"), TPoseReference);
	if (!PhysicsAsset || !SkeletalMesh || !TPoseReference)
	{
		return false;
	}

	const FReferenceSkeleton& ReferenceSkeleton = SkeletalMesh->GetRefSkeleton();
	TArray<FTransform> ComponentSpaceRefPose;
	FAnimationRuntime::FillUpComponentSpaceTransforms(
		ReferenceSkeleton,
		ReferenceSkeleton.GetRefBonePose(),
		ComponentSpaceRefPose);
	const int32 ParentBoneIndex = ReferenceSkeleton.FindBoneIndex(TEXT("thigh_l"));
	const int32 ChildBoneIndex = ReferenceSkeleton.FindBoneIndex(TEXT("calf_l"));
	TestTrue(TEXT("Manny left thigh exists"), ComponentSpaceRefPose.IsValidIndex(ParentBoneIndex));
	TestTrue(TEXT("Manny left calf exists"), ComponentSpaceRefPose.IsValidIndex(ChildBoneIndex));
	if (!ComponentSpaceRefPose.IsValidIndex(ParentBoneIndex) ||
		!ComponentSpaceRefPose.IsValidIndex(ChildBoneIndex))
	{
		return false;
	}

	FPhysAnimMannyConstraintProfile Profile;
	TestTrue(
		TEXT("Manny left knee has a direct constraint profile"),
		PhysAnimProtoMannyAdapter::BuildConstraintProfile(
			PhysicsAsset,
			TEXT("calf_l"),
			TEXT("thigh_l"),
			Profile));

	UPhysAnimComponent* ProductComponent = nullptr;
	if (GEditor)
	{
		if (UWorld* const EditorWorld = GEditor->GetEditorWorldContext().World())
		{
			for (TActorIterator<ACharacter> It(EditorWorld); It; ++It)
			{
				if (UPhysAnimComponent* const Candidate = It->FindComponentByClass<UPhysAnimComponent>())
				{
					ProductComponent = Candidate;
					break;
				}
			}
		}
	}
	TestNotNull(TEXT("The standing fixture exposes its PhysAnim component"), ProductComponent);
	ACharacter* const ProductCharacter = ProductComponent
		? Cast<ACharacter>(ProductComponent->GetOwner())
		: nullptr;
	USkeletalMeshComponent* const ProductMesh = ProductComponent && ProductComponent->GetMeshComponent()
		? ProductComponent->GetMeshComponent()
		: (ProductCharacter ? ProductCharacter->GetMesh() : nullptr);
	TestNotNull(TEXT("The standing fixture exposes Manny's mesh component"), ProductMesh);
	if (!ProductMesh)
	{
		return false;
	}
	const FBodyInstance* const ParentBody = ProductMesh->GetBodyInstance(TEXT("thigh_l"));
	const FBodyInstance* const ChildBody = ProductMesh->GetBodyInstance(TEXT("calf_l"));
	TestNotNull(TEXT("The standing fixture creates the left-thigh physics body"), ParentBody);
	TestNotNull(TEXT("The standing fixture creates the left-calf physics body"), ChildBody);
	if (!ParentBody || !ChildBody ||
		!ParentBody->IsValidBodyInstance() || !ChildBody->IsValidBodyInstance())
	{
		AddError(TEXT("The standing fixture's knee bodies must have valid physics actors"));
		return false;
	}
	USkeletalMeshComponent* const TPoseMesh = NewObject<USkeletalMeshComponent>();
	TPoseMesh->SetSkeletalMeshAsset(const_cast<USkeletalMesh*>(SkeletalMesh));
	TPoseMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	TPoseMesh->SetAnimation(TPoseReference);
	TPoseMesh->PlayAnimation(TPoseReference, false);
	TPoseMesh->TickAnimation(0.0f, false);
	TPoseMesh->RefreshBoneTransforms();
	const int32 TPoseParentBoneIndex = TPoseMesh->GetBoneIndex(TEXT("thigh_l"));
	TestTrue(TEXT("The T-pose fixture contains Manny's left thigh"), TPoseParentBoneIndex != INDEX_NONE);
	if (TPoseParentBoneIndex == INDEX_NONE)
	{
		return false;
	}
	FTransform TPoseParentComponentTransform = TPoseMesh->GetBoneTransform(
		TEXT("thigh_l"),
		RTS_Component);
	TPoseParentComponentTransform.NormalizeRotation();
	const FQuat TPoseParentComponentRotation =
		TPoseParentComponentTransform.GetRotation().GetNormalized();
	const FQuat ComponentWorldRotation = ProductMesh->GetComponentQuat().GetNormalized();
	const FQuat ParentWorldRotation = ParentBody->GetUnrealWorldTransform().GetRotation().GetNormalized();
	const FQuat ChildWorldRotation = ChildBody->GetUnrealWorldTransform().GetRotation().GetNormalized();
	const FQuat BindParentRelativeRotation =
		(ParentWorldRotation.Inverse() * ChildWorldRotation).GetNormalized();
	const FQuat BindInConstraintSpace = (
		Profile.ParentConstraintFrameRotation.Inverse() *
		BindParentRelativeRotation *
		Profile.ChildConstraintFrameRotation).GetNormalized();
	const FVector KneeHingeAxis = (
		ParentWorldRotation * Profile.ParentConstraintFrameRotation).RotateVector(
			FVector::ForwardVector);
	const FVector TPoseKneeHingeAxis = (
		TPoseParentComponentRotation *
		Profile.ParentConstraintFrameRotation).RotateVector(FVector::ForwardVector);
	const FQuat ProtoKneeFlexion = PhysAnimBridge::ProtoJointQuaternionToUe(
		PhysAnimBridge::ExpMapToQuaternion(FVector(0.0, 0.5 * UE_PI, 0.0)));
	const FQuat PolicyRotationInParentBindFrame =
		(ParentWorldRotation.Inverse() * ProtoKneeFlexion * ParentWorldRotation).GetNormalized();
	const FQuat RawParentRelativeTarget =
		(PolicyRotationInParentBindFrame * BindParentRelativeRotation).GetNormalized();
	const FQuat RawTargetInConstraintSpace = (
		Profile.ParentConstraintFrameRotation.Inverse() *
		RawParentRelativeTarget *
		Profile.ChildConstraintFrameRotation).GetNormalized();
	const FQuat RangeMappedParentRelativeTarget =
		PhysAnimProtoMannyAdapter::MapProtoPolicyTargetToMannyConstraintRange(
			RawParentRelativeTarget,
			BindParentRelativeRotation,
			Profile);
	const FQuat AdaptedParentRelativeTarget = PhysAnimProtoMannyAdapter::AdaptParentRelativeTarget(
		RangeMappedParentRelativeTarget,
		BindParentRelativeRotation,
		Profile);

	const double BindConstraintOffsetDegrees = FMath::RadiansToDegrees(
		BindInConstraintSpace.AngularDistance(FQuat::Identity));
	const double RetainedFlexionDegrees = FMath::RadiansToDegrees(
		AdaptedParentRelativeTarget.AngularDistance(BindParentRelativeRotation));
	const FQuat AdaptedTargetInConstraintSpace = (
		Profile.ParentConstraintFrameRotation.Inverse() *
		AdaptedParentRelativeTarget *
		Profile.ChildConstraintFrameRotation).GetNormalized();
	const float BindTwistDegrees = SignedTwistDegrees(BindInConstraintSpace);
	const float ExpectedMappedTwistDegrees =
		BindTwistDegrees + 0.5f * (Profile.TwistLimitDegrees - BindTwistDegrees);
	const FVector2D RawSwingDegrees = SwingDegrees(RawTargetInConstraintSpace);
	const FVector2D AdaptedSwingDegrees = SwingDegrees(AdaptedTargetInConstraintSpace);
	AddInfo(FString::Printf(
		TEXT("MANNY_KNEE_ADAPTER component_rotation=%s hinge=(%.3f,%.3f,%.3f) tpose_hinge=(%.3f,%.3f,%.3f) bind_error_deg=%.3f raw_twist_deg=%.3f raw_swing2_deg=%.3f raw_swing1_deg=%.3f adapted_twist_deg=%.3f adapted_swing2_deg=%.3f adapted_swing1_deg=%.3f retained_flexion_deg=%.3f twist=%d/%.3f swing1=%d/%.3f swing2=%d/%.3f"),
		*ComponentWorldRotation.ToString(),
		KneeHingeAxis.X,
		KneeHingeAxis.Y,
		KneeHingeAxis.Z,
		TPoseKneeHingeAxis.X,
		TPoseKneeHingeAxis.Y,
		TPoseKneeHingeAxis.Z,
		BindConstraintOffsetDegrees,
		SignedTwistDegrees(RawTargetInConstraintSpace),
		RawSwingDegrees.X,
		RawSwingDegrees.Y,
		SignedTwistDegrees(AdaptedTargetInConstraintSpace),
		AdaptedSwingDegrees.X,
		AdaptedSwingDegrees.Y,
		RetainedFlexionDegrees,
		static_cast<int32>(Profile.TwistMotion),
		Profile.TwistLimitDegrees,
		static_cast<int32>(Profile.Swing1Motion),
		Profile.Swing1LimitDegrees,
		static_cast<int32>(Profile.Swing2Motion),
		Profile.Swing2LimitDegrees));
	TestTrue(
		TEXT("The standing fixture aligns Manny's left-knee hinge with canonical UE Y"),
		FMath::Abs(FVector::DotProduct(KneeHingeAxis.GetSafeNormal(), FVector(0.0, 1.0, 0.0))) >= 0.98);
	TestTrue(
		TEXT("Manny's animated T-pose bone frame is observably incompatible with its body-space knee constraint"),
		FMath::Abs(FVector::DotProduct(TPoseKneeHingeAxis.GetSafeNormal(), FVector::RightVector)) <= 0.10);
	TestTrue(
		TEXT("Manny's asymmetric knee range is represented by a 40-50 degree neutral constraint offset"),
		BindConstraintOffsetDegrees >= 40.0 && BindConstraintOffsetDegrees <= 50.0);
	TestTrue(
		TEXT("A half-range Proto knee command occupies approximately half of Manny's available positive knee travel"),
		FMath::IsNearlyEqual(
			SignedTwistDegrees(AdaptedTargetInConstraintSpace),
			ExpectedMappedTwistDegrees,
			1.0f));
	TestTrue(
		TEXT("The adapted knee twist remains within Manny's authored limit"),
		FMath::Abs(SignedTwistDegrees(AdaptedTargetInConstraintSpace)) <= Profile.TwistLimitDegrees + 0.1);
	TestTrue(
		TEXT("The adapted knee swing1 remains within Manny's authored limit"),
		FMath::Abs(AdaptedSwingDegrees.Y) <= Profile.Swing1LimitDegrees + 0.1);
	TestTrue(
		TEXT("The adapted knee swing2 remains within Manny's authored limit"),
		FMath::Abs(AdaptedSwingDegrees.X) <= Profile.Swing2LimitDegrees + 0.1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimProtoMannyObservationGeometryTest,
	"PhysAnim.Adapter.ProtoMannyObservationGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimProtoMannyObservationGeometryTest::RunTest(const FString& Parameters)
{
	const FQuat RootRotation(FVector::UpVector, FMath::DegreesToRadians(90.0f));
	const FVector RootPosition(10.0, 20.0, 0.94);
	const FVector RootLinearVelocity(1.0, 2.0, 3.0);
	const FVector RootAngularVelocity(0.0, 0.0, 2.0);
	TArray<FPhysAnimBodySample> MannySamples;
	MannySamples.Reserve(PhysAnimBridge::NumSmplBodies);
	for (int32 BodyIndex = 0; BodyIndex < PhysAnimBridge::NumSmplBodies; ++BodyIndex)
	{
		MannySamples.Add(FPhysAnimBodySample{
			RootPosition + FVector(BodyIndex, -BodyIndex, BodyIndex * 0.5),
			RootRotation,
			FVector(BodyIndex * 10.0, 0.0, 0.0),
			RootAngularVelocity});
	}
	MannySamples[0].Position = RootPosition;
	MannySamples[0].LinearVelocity = RootLinearVelocity;

	TArray<FPhysAnimBodySample> ProtoSamples;
	FString Error;
	TestTrue(
		TEXT("The Manny body sample set adapts to canonical SMPL geometry"),
		PhysAnimProtoMannyAdapter::AdaptBodySamplesToCanonicalSmpl(MannySamples, ProtoSamples, Error));
	TestTrue(TEXT("Canonical body adaptation reports no error"), Error.IsEmpty());
	TestEqual(TEXT("Canonical body adaptation preserves the 24-body contract"), ProtoSamples.Num(), PhysAnimBridge::NumSmplBodies);
	TestTrue(TEXT("The live pelvis position is preserved"), ProtoSamples[0].Position.Equals(RootPosition, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("The live pelvis rotation is preserved"), QuaternionsNear(ProtoSamples[0].Rotation, RootRotation));

	const FVector ExpectedLeftHipOffset = RootRotation.RotateVector(FVector(-0.0068, 0.0695, -0.0914));
	TestTrue(
		TEXT("The left hip uses the ProtoMotions v2.3 SMPL parent offset"),
		ProtoSamples[1].Position.Equals(RootPosition + ExpectedLeftHipOffset, 0.0001));
	const FVector ExpectedLeftKneeOffset = RootRotation.RotateVector(FVector(-0.0045, 0.0343, -0.3752));
	TestTrue(
		TEXT("Canonical offsets accumulate through the SMPL hierarchy"),
		ProtoSamples[2].Position.Equals(
			RootPosition + ExpectedLeftHipOffset + ExpectedLeftKneeOffset,
			0.0001));
	TestTrue(
		TEXT("Virtual body velocity follows parent rigid-body kinematics"),
		ProtoSamples[1].LinearVelocity.Equals(
			RootLinearVelocity + FVector::CrossProduct(RootAngularVelocity, ExpectedLeftHipOffset),
			0.0001));
	TestTrue(
		TEXT("The collapsed Manny hand sample still produces distinct SMPL wrist and hand positions"),
		!ProtoSamples[17].Position.Equals(ProtoSamples[18].Position, 0.001));

	FPhysAnimFuturePoseSample MannyFuture;
	MannyFuture.FutureTimeSeconds = 1.0f / 30.0f;
	for (const FPhysAnimBodySample& BodySample : MannySamples)
	{
		MannyFuture.BodyTransforms.Add(FTransform(BodySample.Rotation, BodySample.Position));
	}
	TArray<FPhysAnimFuturePoseSample> ProtoFuture;
	TestTrue(
		TEXT("Future PoseSearch samples use the same canonical SMPL geometry"),
		PhysAnimProtoMannyAdapter::AdaptFuturePoseSamplesToCanonicalSmpl(
			TArray<FPhysAnimFuturePoseSample>{MannyFuture},
			ProtoFuture,
			Error));
	TestEqual(TEXT("The future time channel is preserved"), ProtoFuture[0].FutureTimeSeconds, MannyFuture.FutureTimeSeconds);
	TestTrue(
		TEXT("Future left hip geometry matches current canonical geometry"),
		ProtoFuture[0].BodyTransforms[1].GetLocation().Equals(ProtoSamples[1].Position, 0.0001));

	TArray<FPhysAnimBodySample> InvalidSamples;
	Error.Reset();
	TestFalse(
		TEXT("Incomplete Manny samples cannot silently enter the fixed-width policy input"),
		PhysAnimProtoMannyAdapter::AdaptBodySamplesToCanonicalSmpl(InvalidSamples, ProtoSamples, Error));
	TestFalse(TEXT("Incomplete sample rejection is explicit"), Error.IsEmpty());

	return true;
}

#endif
