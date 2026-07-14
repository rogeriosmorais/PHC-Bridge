#include "PhysAnimBridge.h"
#include "PhysAnimComponent.h"
#include "Misc/AutomationTest.h"

#include <limits>

namespace
{
	using namespace PhysAnimBridge;

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimFutureScheduleTest,
		"PhysAnim.Bridge.FutureSchedule",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimFutureScheduleTest::RunTest(const FString& Parameters)
	{
		const TArray<float> Schedule = BuildFutureSampleTimeSchedule();
		TestEqual(TEXT("Future step count"), Schedule.Num(), NumFutureSteps);
		TestEqual(TEXT("First future step"), Schedule[0], FutureStepSeconds);
		TestEqual(TEXT("Last future step"), Schedule.Last(), NumFutureSteps * FutureStepSeconds);
		TestEqual(
			TEXT("Unclamped future target time keeps the requested offset"),
			ResolveFutureTargetTimeSeconds(0.5f, 0.2f, 2.0f),
			0.2f);
		TestEqual(
			TEXT("Clamped future target time shrinks at animation end"),
			ResolveFutureTargetTimeSeconds(1.9f, 0.4f, 2.0f),
			0.1f);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimTensorIndexMapTest,
		"PhysAnim.Bridge.TensorIndexMap",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimTensorIndexMapTest::RunTest(const FString& Parameters)
	{
		TArray<UE::NNE::FTensorDesc> TensorDescs;
		TensorDescs.Add(UE::NNE::FTensorDesc::Make(TEXT("terrain"), UE::NNE::FSymbolicTensorShape::Make({1, TerrainSize}), ENNETensorDataType::Float));
		TensorDescs.Add(UE::NNE::FTensorDesc::Make(TEXT("self_obs"), UE::NNE::FSymbolicTensorShape::Make({1, SelfObsSize}), ENNETensorDataType::Float));
		TensorDescs.Add(UE::NNE::FTensorDesc::Make(TEXT("mimic_target_poses"), UE::NNE::FSymbolicTensorShape::Make({1, MimicTargetPosesSize}), ENNETensorDataType::Float));

		FPhysAnimTensorIndexMap IndexMap;
		FString Error;
		TestTrue(TEXT("Tensor map should succeed"), BuildInputTensorIndexMap(TensorDescs, IndexMap, Error));
		TestEqual(TEXT("self_obs index"), IndexMap.SelfObs, 1);
		TestEqual(TEXT("mimic_target_poses index"), IndexMap.MimicTargetPoses, 2);
		TestEqual(TEXT("terrain index"), IndexMap.Terrain, 0);

		TensorDescs.Add(UE::NNE::FTensorDesc::Make(TEXT("terrain"), UE::NNE::FSymbolicTensorShape::Make({1, TerrainSize}), ENNETensorDataType::Float));
		TestFalse(TEXT("Duplicate tensor map should fail"), BuildInputTensorIndexMap(TensorDescs, IndexMap, Error));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimInputDescriptorContractTest,
		"PhysAnim.Bridge.InputDescriptorContract",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimInputDescriptorContractTest::RunTest(const FString& Parameters)
	{
		auto MakeValidInputs = []()
		{
			TArray<UE::NNE::FTensorDesc> TensorDescs;
			TensorDescs.Add(UE::NNE::FTensorDesc::Make(TEXT("self_obs"), UE::NNE::FSymbolicTensorShape::Make({1, SelfObsSize}), ENNETensorDataType::Float));
			TensorDescs.Add(UE::NNE::FTensorDesc::Make(TEXT("mimic_target_poses"), UE::NNE::FSymbolicTensorShape::Make({1, MimicTargetPosesSize}), ENNETensorDataType::Float));
			TensorDescs.Add(UE::NNE::FTensorDesc::Make(TEXT("terrain"), UE::NNE::FSymbolicTensorShape::Make({1, TerrainSize}), ENNETensorDataType::Float));
			return TensorDescs;
		};

		FPhysAnimTensorIndexMap IndexMap;
		FString Error;

		TArray<UE::NNE::FTensorDesc> TensorDescs = MakeValidInputs();
		TestTrue(TEXT("Valid input descriptor contract succeeds"), ValidateInputTensorDescs(TensorDescs, IndexMap, Error));
		TestEqual(TEXT("self_obs index is recorded"), IndexMap.SelfObs, 0);
		TestEqual(TEXT("mimic_target_poses index is recorded"), IndexMap.MimicTargetPoses, 1);
		TestEqual(TEXT("terrain index is recorded"), IndexMap.Terrain, 2);

		TensorDescs.Reset();
		TensorDescs.Add(UE::NNE::FTensorDesc::Make(TEXT("terrain"), UE::NNE::FSymbolicTensorShape::Make({-1, TerrainSize}), ENNETensorDataType::Float));
		TensorDescs.Add(UE::NNE::FTensorDesc::Make(TEXT("self_obs"), UE::NNE::FSymbolicTensorShape::Make({-1, SelfObsSize}), ENNETensorDataType::Float));
		TensorDescs.Add(UE::NNE::FTensorDesc::Make(TEXT("mimic_target_poses"), UE::NNE::FSymbolicTensorShape::Make({-1, MimicTargetPosesSize}), ENNETensorDataType::Float));
		TestTrue(TEXT("Reordered dynamic-batch input descriptors succeed"), ValidateInputTensorDescs(TensorDescs, IndexMap, Error));
		TestEqual(TEXT("reordered terrain index is recorded"), IndexMap.Terrain, 0);
		TestEqual(TEXT("reordered self_obs index is recorded"), IndexMap.SelfObs, 1);
		TestEqual(TEXT("reordered mimic target index is recorded"), IndexMap.MimicTargetPoses, 2);

		TensorDescs = MakeValidInputs();
		TensorDescs[0] = UE::NNE::FTensorDesc::Make(TEXT("terrain"), UE::NNE::FSymbolicTensorShape::Make({1, TerrainSize}), ENNETensorDataType::Float);
		TestFalse(TEXT("Duplicate input names are rejected"), ValidateInputTensorDescs(TensorDescs, IndexMap, Error));

		TensorDescs = MakeValidInputs();
		TensorDescs.RemoveAt(2);
		TestFalse(TEXT("Missing input descriptors are rejected"), ValidateInputTensorDescs(TensorDescs, IndexMap, Error));

		TensorDescs = MakeValidInputs();
		TensorDescs[2] = UE::NNE::FTensorDesc::Make(TEXT("unknown"), UE::NNE::FSymbolicTensorShape::Make({1, TerrainSize}), ENNETensorDataType::Float);
		TestFalse(TEXT("Unknown input descriptors are rejected"), ValidateInputTensorDescs(TensorDescs, IndexMap, Error));

		TensorDescs = MakeValidInputs();
		TensorDescs[0] = UE::NNE::FTensorDesc::Make(TEXT("self_obs"), UE::NNE::FSymbolicTensorShape::Make({1, SelfObsSize}), ENNETensorDataType::Half);
		TestFalse(TEXT("Non-float input descriptors are rejected"), ValidateInputTensorDescs(TensorDescs, IndexMap, Error));

		TensorDescs = MakeValidInputs();
		TensorDescs[0] = UE::NNE::FTensorDesc::Make(TEXT("self_obs"), UE::NNE::FSymbolicTensorShape::Make({SelfObsSize}), ENNETensorDataType::Float);
		TestFalse(TEXT("Rank-1 input descriptors are rejected"), ValidateInputTensorDescs(TensorDescs, IndexMap, Error));

		TensorDescs = MakeValidInputs();
		TensorDescs[0] = UE::NNE::FTensorDesc::Make(TEXT("self_obs"), UE::NNE::FSymbolicTensorShape::Make({2, SelfObsSize}), ENNETensorDataType::Float);
		TestFalse(TEXT("Unexpected fixed batch size is rejected"), ValidateInputTensorDescs(TensorDescs, IndexMap, Error));

		TensorDescs = MakeValidInputs();
		TensorDescs[0] = UE::NNE::FTensorDesc::Make(TEXT("self_obs"), UE::NNE::FSymbolicTensorShape::Make({1, SelfObsSize - 1}), ENNETensorDataType::Float);
		TestFalse(TEXT("Wrong input feature width is rejected"), ValidateInputTensorDescs(TensorDescs, IndexMap, Error));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimActionOutputDescriptorContractTest,
		"PhysAnim.Bridge.ActionOutputDescriptorContract",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimActionOutputDescriptorContractTest::RunTest(const FString& Parameters)
	{
		TArray<UE::NNE::FTensorDesc> OutputDescs;
		FString Error;

		OutputDescs.Add(UE::NNE::FTensorDesc::Make(TEXT("actions"), UE::NNE::FSymbolicTensorShape::Make({1, NumActionFloats}), ENNETensorDataType::Float));
		TestTrue(TEXT("Fixed batch action output descriptor is accepted"), ValidateActionOutputTensorDescs(OutputDescs, Error));

		OutputDescs.Reset();
		OutputDescs.Add(UE::NNE::FTensorDesc::Make(TEXT("actions"), UE::NNE::FSymbolicTensorShape::Make({-1, NumActionFloats}), ENNETensorDataType::Float));
		TestTrue(TEXT("Dynamic batch action output descriptor is accepted"), ValidateActionOutputTensorDescs(OutputDescs, Error));

		OutputDescs.Add(UE::NNE::FTensorDesc::Make(TEXT("extra"), UE::NNE::FSymbolicTensorShape::Make({1, NumActionFloats}), ENNETensorDataType::Float));
		TestFalse(TEXT("Multiple output tensors are rejected"), ValidateActionOutputTensorDescs(OutputDescs, Error));

		OutputDescs.Reset();
		OutputDescs.Add(UE::NNE::FTensorDesc::Make(TEXT("actions"), UE::NNE::FSymbolicTensorShape::Make({1, NumActionFloats}), ENNETensorDataType::Half));
		TestFalse(TEXT("Non-float action output is rejected"), ValidateActionOutputTensorDescs(OutputDescs, Error));

		OutputDescs.Reset();
		OutputDescs.Add(UE::NNE::FTensorDesc::Make(TEXT("actions"), UE::NNE::FSymbolicTensorShape::Make({NumActionFloats}), ENNETensorDataType::Float));
		TestFalse(TEXT("Rank-1 action output is rejected"), ValidateActionOutputTensorDescs(OutputDescs, Error));

		OutputDescs.Reset();
		OutputDescs.Add(UE::NNE::FTensorDesc::Make(TEXT("actions"), UE::NNE::FSymbolicTensorShape::Make({2, NumActionFloats}), ENNETensorDataType::Float));
		TestFalse(TEXT("Unexpected fixed batch size is rejected"), ValidateActionOutputTensorDescs(OutputDescs, Error));

		OutputDescs.Reset();
		OutputDescs.Add(UE::NNE::FTensorDesc::Make(TEXT("actions"), UE::NNE::FSymbolicTensorShape::Make({1, NumActionFloats - 1}), ENNETensorDataType::Float));
		TestFalse(TEXT("Wrong action width is rejected"), ValidateActionOutputTensorDescs(OutputDescs, Error));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimFiniteFloatBufferContractTest,
		"PhysAnim.Bridge.FiniteFloatBufferContract",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimFiniteFloatBufferContractTest::RunTest(const FString& Parameters)
	{
		TArray<float> Values;
		Values.Add(0.0f);
		Values.Add(1.0f);
		Values.Add(-1.0f);

		FString Error;
		TestTrue(TEXT("Finite buffer is accepted"), ValidateFiniteFloatBuffer(TEXT("terrain"), Values, Error));

		Values[1] = std::numeric_limits<float>::quiet_NaN();
		TestFalse(TEXT("NaN buffer is rejected"), ValidateFiniteFloatBuffer(TEXT("terrain"), Values, Error));
		TestEqual(TEXT("Error names the rejected buffer"), Error, TEXT("terrain contained NaN or Inf."));

		Values[1] = std::numeric_limits<float>::infinity();
		TestFalse(TEXT("Inf buffer is rejected"), ValidateFiniteFloatBuffer(TEXT("model_actions"), Values, Error));
		TestEqual(TEXT("Error names the rejected output buffer"), Error, TEXT("model_actions contained NaN or Inf."));

		Values[1] = 0.0f;
		TestTrue(TEXT("Recovered finite buffer is accepted"), ValidateFiniteFloatBuffer(TEXT("terrain"), Values, Error));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimSmplOrderContractTest,
		"PhysAnim.Bridge.SmplOrderContract",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimSmplOrderContractTest::RunTest(const FString& Parameters)
	{
		const TArray<FName>& Bones = GetSmplObservationBoneNames();
		TestEqual(TEXT("SMPL body count"), Bones.Num(), NumSmplBodies);
		if (Bones.Num() >= NumSmplBodies)
		{
			TestEqual(TEXT("Index 0: Pelvis"), Bones[0], TEXT("pelvis"));
			TestEqual(TEXT("Index 1: L_Hip"), Bones[1], TEXT("thigh_l"));
			TestEqual(TEXT("Index 3: L_Ankle"), Bones[3], TEXT("foot_l"));
			TestEqual(TEXT("Index 9: Torso"), Bones[9], TEXT("spine_01"));
			TestEqual(TEXT("Index 23: R_Hand"), Bones[23], TEXT("hand_r"));
		}
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimFrameConversionTest,
		"PhysAnim.Bridge.FrameConversion",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimFrameConversionTest::RunTest(const FString& Parameters)
	{
		const FVector SmplUp(0.0, 1.0, 0.0);
		const FVector UeUp = SmplVectorToUe(SmplUp);
		TestTrue(TEXT("SMPL local authoring Y-up becomes UE Z-up"), UeUp.Equals(FVector(0.0, 0.0, 1.0), KINDA_SMALL_NUMBER));
		TestTrue(
			TEXT("Identity SMPL local rotation stays identity after UE local basis conversion"),
			SmplQuaternionToUe(FQuat::Identity).Equals(FQuat::Identity, KINDA_SMALL_NUMBER));
		TestTrue(
			TEXT("Identity UE local rotation stays identity after SMPL local basis conversion"),
			UeQuaternionToSmpl(FQuat::Identity).Equals(FQuat::Identity, KINDA_SMALL_NUMBER));

		const FQuat SmplRotation = ExpMapToQuaternion(FVector(0.2, -0.1, 0.3));
		const FQuat RoundTrip = UeQuaternionToSmpl(SmplQuaternionToUe(SmplRotation));
		TestTrue(TEXT("Quaternion roundtrip should stay close"), RoundTrip.Equals(SmplRotation, 1.0e-3f));

		const FQuat ProtoJointXRotation = ExpMapToQuaternion(FVector(0.5 * PI, 0.0, 0.0));
		const FQuat ExpectedUeJointXRotation(FVector::ForwardVector, -0.5 * PI);
		TestTrue(
			TEXT("ProtoMotions joint-local X rotation includes the Isaac-to-UE handedness change"),
			ProtoJointQuaternionToUe(ProtoJointXRotation).Equals(ExpectedUeJointXRotation, 1.0e-3f));

		const FQuat ProtoJointYRotation = ExpMapToQuaternion(FVector(0.0, 0.5 * PI, 0.0));
		const FQuat ExpectedUeJointYRotation(FVector::RightVector, 0.5 * PI);
		TestTrue(
			TEXT("ProtoMotions joint-local Y rotation preserves the lateral axis sign"),
			ProtoJointQuaternionToUe(ProtoJointYRotation).Equals(ExpectedUeJointYRotation, 1.0e-3f));

		const FQuat ProtoJointZRotation = ExpMapToQuaternion(FVector(0.0, 0.0, 0.5 * PI));
		const FQuat ExpectedUeJointZRotation(FVector::UpVector, -0.5 * PI);
		TestTrue(
			TEXT("ProtoMotions joint-local Z rotation includes the Isaac-to-UE handedness change"),
			ProtoJointQuaternionToUe(ProtoJointZRotation).Equals(ExpectedUeJointZRotation, 1.0e-3f));

		TestTrue(
			TEXT("UE angular velocity about X converts as an axial vector"),
			UeWorldRotationVectorToProtoRuntime(FVector::ForwardVector).Equals(
				-FVector::ForwardVector,
				KINDA_SMALL_NUMBER));
		TestTrue(
			TEXT("UE angular velocity about Y preserves the lateral axial component"),
			UeWorldRotationVectorToProtoRuntime(FVector::RightVector).Equals(
				FVector::RightVector,
				KINDA_SMALL_NUMBER));
		TestTrue(
			TEXT("UE angular velocity about Z converts as an axial vector"),
			UeWorldRotationVectorToProtoRuntime(FVector::UpVector).Equals(
				-FVector::UpVector,
				KINDA_SMALL_NUMBER));

		const double SmallWorldRotation = 1.0e-3;
		const FVector UeAngularAxes[] =
		{
			FVector::ForwardVector,
			FVector::RightVector,
			FVector::UpVector,
		};
		for (int32 AxisIndex = 0; AxisIndex < UE_ARRAY_COUNT(UeAngularAxes); ++AxisIndex)
		{
			const FVector& UeAxis = UeAngularAxes[AxisIndex];
			const FQuat ConvertedIntegratedRotation = UeWorldQuaternionToProtoRuntime(
				FQuat(UeAxis, SmallWorldRotation));
			const FQuat IntegratedConvertedAngularVelocity = ExpMapToQuaternion(
				UeWorldRotationVectorToProtoRuntime(UeAxis) * SmallWorldRotation);
			TestTrue(
				*FString::Printf(
					TEXT("World angular-velocity conversion matches converted quaternion integration for UE axis %d"),
					AxisIndex),
				ConvertedIntegratedRotation.Equals(IntegratedConvertedAngularVelocity, 1.0e-5));
		}
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimActionConditioningContractTest,
		"PhysAnim.Bridge.ActionConditioningContract",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimActionConditioningContractTest::RunTest(const FString& Parameters)
	{
		const FPhysAnimStabilizationSettings RuntimeDefaults;
		TestEqual(TEXT("ProtoMotions action scale defaults to full authority"), RuntimeDefaults.ActionScale, 1.0f);
		TestEqual(TEXT("ProtoMotions action clamp defaults to the trained range"), RuntimeDefaults.ActionClampAbs, 1.0f);
		TestEqual(TEXT("ProtoMotions actions are not temporally smoothed by default"), RuntimeDefaults.ActionSmoothingAlpha, 1.0f);
		TestFalse(
			TEXT("Lower-limb policy targets are not range-reduced by default"),
			RuntimeDefaults.bApplyTrainingAlignedLowerLimbTargetRangePolicy);
		TestFalse(
			TEXT("Distal locomotion policy targets are not range-reduced by default"),
			RuntimeDefaults.bApplyTrainingAlignedDistalLocomotionTargetPolicy);
		TestFalse(
			TEXT("Distal target-write composition is not altered by default"),
			RuntimeDefaults.bApplyTrainingAlignedDistalLocomotionCompositionPolicy);
		TestEqual(
			TEXT("Policy target slew limiting is disabled by default"),
			RuntimeDefaults.MaxAngularStepDegreesPerSecond,
			0.0f);

		TArray<float> RawActions;
		RawActions.Init(2.0f, NumActionFloats);

		TArray<float> PreviousActions;
		PreviousActions.Init(0.0f, NumActionFloats);

		FPhysAnimActionConditioningSettings Settings;
		Settings.ActionScale = 1.0f;
		Settings.ActionClampAbs = 0.25f;
		Settings.ActionSmoothingAlpha = 0.5f;

		TArray<float> ConditionedActions;
		FPhysAnimActionDiagnostics Diagnostics;
		FString Error;
		TestTrue(
			TEXT("Conditioning accepts the PHC action width"),
			ConditionModelActions(RawActions, &PreviousActions, Settings, ConditionedActions, Diagnostics, Error));
		TestEqual(TEXT("Conditioned action width"), ConditionedActions.Num(), NumActionFloats);
		TestEqual(TEXT("All oversized action floats were clamped"), Diagnostics.NumClampedActionFloats, NumActionFloats);
		TestTrue(TEXT("Raw mean abs records pre-clamp policy output"), FMath::IsNearlyEqual(Diagnostics.RawMeanAbs, 2.0f));
		TestTrue(TEXT("Conditioned mean abs records post-smoothing target"), FMath::IsNearlyEqual(Diagnostics.ConditionedMeanAbs, 0.125f));

		for (const float Action : ConditionedActions)
		{
			TestTrue(TEXT("Conditioned action respects clamp and smoothing"), FMath::IsNearlyEqual(Action, 0.125f));
		}

		Settings.bForceZeroActions = true;
		ConditionedActions.Reset();
		Diagnostics = {};
		TestTrue(
			TEXT("Force-zero action policy still accepts valid action width"),
			ConditionModelActions(RawActions, nullptr, Settings, ConditionedActions, Diagnostics, Error));
		TestTrue(TEXT("Force-zero removes policy influence"), FMath::IsNearlyZero(Diagnostics.ConditionedMeanAbs));

		RawActions.RemoveAt(RawActions.Num() - 1);
		TestFalse(
			TEXT("Conditioning rejects malformed action width"),
			ConditionModelActions(RawActions, nullptr, Settings, ConditionedActions, Diagnostics, Error));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimActionToBoneMappingContractTest,
		"PhysAnim.Bridge.ActionToBoneMappingContract",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimActionToBoneMappingContractTest::RunTest(const FString& Parameters)
	{
		TArray<float> Actions;
		Actions.Init(0.0f, NumActionFloats);
		
		// Joint 0 in ProtoMotions runtime order is L_Hip -> thigh_l.
		Actions[0] = 0.5f; // +90 degrees about the Isaac joint-local X axis.
		
		TMap<FName, FQuat> ControlRotations;
		FString Error;
		TestTrue(TEXT("Action conversion succeed"), ConvertModelActionsToControlRotations(Actions, ControlRotations, Error));
		TestEqual(TEXT("Conversion writes exactly the controlled bone count"), ControlRotations.Num(), NumControlledBones);
		for (const FName BoneName : GetControlledBoneNames())
		{
			TestTrue(
				FString::Printf(TEXT("Controlled bone target exists: %s"), *BoneName.ToString()),
				ControlRotations.Contains(BoneName));
			if (const FQuat* const TargetRotation = ControlRotations.Find(BoneName))
			{
				TestTrue(
					FString::Printf(TEXT("Controlled bone target is normalized: %s"), *BoneName.ToString()),
					TargetRotation->IsNormalized());
			}
		}
		
		TestTrue(TEXT("thigh_l target exists"), ControlRotations.Contains(TEXT("thigh_l")));
		if (ControlRotations.Contains(TEXT("thigh_l")))
		{
			const FQuat ThighRot = ControlRotations[TEXT("thigh_l")];
			TestFalse(TEXT("thigh_l rotation is non-identity"), ThighRot.IsIdentity());
			const FQuat ExpectedThighRotation(FVector::ForwardVector, -0.5 * PI);
			TestTrue(
				TEXT("thigh_l action uses the ProtoMotions Isaac joint basis in UE"),
				ThighRot.Equals(ExpectedThighRotation, 1.0e-3f));
		}

		// Distal Collapse Verification: Joint 16 (L_Wrist) and 17 (L_Hand)
		Actions.Init(0.0f, NumActionFloats);
		Actions[16 * 3 + 1] = 0.1f; // wrist Y
		Actions[17 * 3 + 1] = 0.1f; // hand Y
		
		ControlRotations.Reset();
		TestTrue(TEXT("Collapse conversion succeed"), ConvertModelActionsToControlRotations(Actions, ControlRotations, Error));
		TestTrue(TEXT("hand_l target exists"), ControlRotations.Contains(TEXT("hand_l")));
		TestFalse(TEXT("No standalone wrist target"), ControlRotations.Contains(TEXT("wrist_l")));

		TArray<float> RawActions;
		RawActions.Init(4.0f, NumActionFloats);
		FPhysAnimActionConditioningSettings Settings;
		Settings.ActionScale = 1.0f;
		Settings.ActionClampAbs = 0.2f;
		Settings.ActionSmoothingAlpha = 1.0f;

		TArray<float> ConditionedActions;
		FPhysAnimActionDiagnostics Diagnostics;
		TestTrue(
			TEXT("Conditioning prepares bounded policy actions"),
			ConditionModelActions(RawActions, nullptr, Settings, ConditionedActions, Diagnostics, Error));
		TestEqual(TEXT("All raw actions are clamped before conversion"), Diagnostics.NumClampedActionFloats, NumActionFloats);

		ControlRotations.Reset();
		TestTrue(
			TEXT("Bounded conditioned actions convert to control rotations"),
			ConvertModelActionsToControlRotations(ConditionedActions, ControlRotations, Error));
		for (const TPair<FName, FQuat>& Pair : ControlRotations)
		{
			TestTrue(
				FString::Printf(TEXT("Bounded conditioned target is normalized: %s"), *Pair.Key.ToString()),
				Pair.Value.IsNormalized());
			TestTrue(
				FString::Printf(TEXT("Bounded conditioned target stays below full-turn distance: %s"), *Pair.Key.ToString()),
				FMath::RadiansToDegrees(Pair.Value.AngularDistance(FQuat::Identity)) <= 180.0 + KINDA_SMALL_NUMBER);
		}
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimControlTargetStepLimitContractTest,
		"PhysAnim.Bridge.ControlTargetStepLimitContract",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimControlTargetStepLimitContractTest::RunTest(const FString& Parameters)
	{
		const FQuat PreviousRotation = FQuat::Identity;
		const FQuat LargeTargetRotation(FVector::UpVector, FMath::DegreesToRadians(90.0));
		const FQuat LimitedRotation = LimitControlRotationStep(PreviousRotation, LargeTargetRotation, 10.0f);
		const double LimitedStepDegrees = FMath::RadiansToDegrees(PreviousRotation.AngularDistance(LimitedRotation));

		TestTrue(TEXT("Limited target remains normalized"), LimitedRotation.IsNormalized());
		TestTrue(TEXT("LimitControlRotationStep respects max angular step"), LimitedStepDegrees <= 10.0 + KINDA_SMALL_NUMBER);
		TestFalse(TEXT("Limited target still advances toward policy target"), LimitedRotation.Equals(PreviousRotation, KINDA_SMALL_NUMBER));

		const FQuat SmallTargetRotation(FVector::UpVector, FMath::DegreesToRadians(5.0));
		TestTrue(
			TEXT("Targets already inside the limit are preserved"),
			LimitControlRotationStep(PreviousRotation, SmallTargetRotation, 10.0f).Equals(SmallTargetRotation, KINDA_SMALL_NUMBER));
		TestTrue(
			TEXT("Non-positive limit disables step limiting"),
			LimitControlRotationStep(PreviousRotation, LargeTargetRotation, 0.0f).Equals(LargeTargetRotation, KINDA_SMALL_NUMBER));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimSelfObservationContractTest,
		"PhysAnim.Bridge.SelfObservationContract",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimSelfObservationContractTest::RunTest(const FString& Parameters)
	{
		TArray<FPhysAnimBodySample> BodySamples;
		BodySamples.SetNum(NumSmplBodies);
		for (int32 i = 0; i < NumSmplBodies; ++i)
		{
			BodySamples[i].Position = FVector::ZeroVector;
			BodySamples[i].Rotation = FQuat::Identity;
			BodySamples[i].LinearVelocity = FVector::ZeroVector;
			BodySamples[i].AngularVelocity = FVector::ZeroVector;
		}

		// The pure builder receives runtime-converted policy-space positions in meters.
		BodySamples[0].Position = FVector(0.0f, 0.0f, 1.0f);
		
		TArray<float> SelfObs;
		FString Error;
		TestTrue(TEXT("SelfObs build succeed"), BuildSelfObservation(BodySamples, 0.0f, SelfObs, Error));
		TestEqual(TEXT("SelfObs size"), SelfObs.Num(), SelfObsSize);
		
		// Index 0 in self_obs is root height relative to ground (meters)
		TestEqual(TEXT("Root height (meters)"), SelfObs[0], 1.0f);

		// Thigh_l (Index 1) position relative to root
		BodySamples[1].Position = FVector(0.5f, 0.0f, 1.0f);
		SelfObs.Reset();
		TestTrue(TEXT("Relative pos build succeed"), BuildSelfObservation(BodySamples, 0.0f, SelfObs, Error));
		
		// Relative positions start at index 1. Index 1: X, 2: Y, 3: Z in policy basis.
		TestEqual(TEXT("Thigh_l relative X (meters)"), SelfObs[1], 0.5f);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimMimicTargetPosesContractTest,
		"PhysAnim.Bridge.MimicTargetPosesContract",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimMimicTargetPosesContractTest::RunTest(const FString& Parameters)
	{
		TArray<FPhysAnimBodySample> CurrentSamples;
		CurrentSamples.SetNum(NumSmplBodies);
		for (int32 i = 0; i < NumSmplBodies; ++i) { CurrentSamples[i] = {FVector::ZeroVector, FQuat::Identity}; }

		TArray<FPhysAnimFuturePoseSample> FutureSamples;
		for (int32 i = 0; i < NumFutureSteps; ++i)
		{
			FPhysAnimFuturePoseSample Sample;
			Sample.FutureTimeSeconds = (i + 1) * FutureStepSeconds;
			Sample.BodyTransforms.SetNum(NumSmplBodies);
			for (int32 b = 0; b < NumSmplBodies; ++b) { Sample.BodyTransforms[b] = FTransform::Identity; }
			FutureSamples.Add(Sample);
		}

		// Future samples already use the policy basis and meters at this boundary.
		FutureSamples[0].BodyTransforms[0].SetLocation(FVector(0.0f, 0.0f, 0.1f));

		TArray<float> MimicObs;
		FString Error;
		TestTrue(TEXT("Mimic build succeed"), BuildMimicTargetPoses(CurrentSamples, FutureSamples, MimicObs, Error));
		TestEqual(TEXT("Mimic size"), MimicObs.Num(), MimicTargetPosesSize);

		// First block is relative positions to the previous frame in policy space.
		// Index 0: X, 1: Y, 2: Z
		TestEqual(TEXT("Future Pelvis relative Z (meters)"), MimicObs[2], 0.1f);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimTerrainObservationContractTest,
		"PhysAnim.Bridge.TerrainObservationContract",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimTerrainObservationContractTest::RunTest(const FString& Parameters)
	{
		TArray<float> Heights;
		Heights.Init(0.0f, TerrainSize);
		Heights[0] = -0.5f;

		TArray<float> Terrain;
		FString Error;
		// Root and sampled ground heights are already expressed in meters.
		TestTrue(TEXT("Terrain build succeed"), BuildTerrainObservation(1.0f, Heights, Terrain, Error));
		TestEqual(TEXT("Sample 0 height (meters)"), Terrain[0], 1.5f);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPolicyInferenceSnapshotContractTest,
		"PhysAnim.Bridge.PolicyInferenceSnapshotContract",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimPolicyInferenceSnapshotContractTest::RunTest(const FString& Parameters)
	{
		TArray<float> SelfObservation;
		SelfObservation.Init(1.0f, SelfObsSize);
		TArray<float> MimicTargetPoses;
		MimicTargetPoses.Init(2.0f, MimicTargetPosesSize);
		TArray<float> Terrain;
		Terrain.Init(3.0f, TerrainSize);
		TArray<float> Actions;
		Actions.Init(4.0f, NumActionFloats);

		FPhysAnimPolicyInferenceSnapshot Snapshot;
		TestFalse(TEXT("New policy snapshot is uncaptured"), Snapshot.bCaptured);
		TestFalse(
			TEXT("A gated snapshot ignores inference outside its requested runtime state"),
			Snapshot.CaptureFirstIf(false, SelfObservation, MimicTargetPoses, Terrain, Actions));
		TestFalse(TEXT("A rejected gated capture leaves the snapshot uncaptured"), Snapshot.bCaptured);
		TestTrue(TEXT("First successful inference is captured"), Snapshot.CaptureFirst(SelfObservation, MimicTargetPoses, Terrain, Actions));
		TestTrue(TEXT("Captured policy snapshot is marked captured"), Snapshot.bCaptured);
		TestEqual(TEXT("Captured self observation width"), Snapshot.SelfObservation.Num(), SelfObsSize);
		TestEqual(TEXT("Captured mimic target width"), Snapshot.MimicTargetPoses.Num(), MimicTargetPosesSize);
		TestEqual(TEXT("Captured terrain width"), Snapshot.Terrain.Num(), TerrainSize);
		TestEqual(TEXT("Captured action width"), Snapshot.Actions.Num(), NumActionFloats);
		TestEqual(TEXT("Captured self observation value"), Snapshot.SelfObservation[0], 1.0f);
		TestEqual(TEXT("Captured mimic target value"), Snapshot.MimicTargetPoses[0], 2.0f);
		TestEqual(TEXT("Captured terrain value"), Snapshot.Terrain[0], 3.0f);
		TestEqual(TEXT("Captured action value"), Snapshot.Actions[0], 4.0f);

		SelfObservation[0] = 10.0f;
		MimicTargetPoses[0] = 20.0f;
		Terrain[0] = 30.0f;
		Actions[0] = 40.0f;
		TestFalse(TEXT("Later inference does not overwrite the first snapshot"), Snapshot.CaptureFirst(SelfObservation, MimicTargetPoses, Terrain, Actions));
		TestEqual(TEXT("First self observation remains latched"), Snapshot.SelfObservation[0], 1.0f);
		TestEqual(TEXT("First mimic target remains latched"), Snapshot.MimicTargetPoses[0], 2.0f);
		TestEqual(TEXT("First terrain remains latched"), Snapshot.Terrain[0], 3.0f);
		TestEqual(TEXT("First action remains latched"), Snapshot.Actions[0], 4.0f);

		Snapshot.Reset();
		TestFalse(TEXT("Reset clears the captured flag"), Snapshot.bCaptured);
		TestEqual(TEXT("Reset clears self observation"), Snapshot.SelfObservation.Num(), 0);
		TestEqual(TEXT("Reset clears mimic target poses"), Snapshot.MimicTargetPoses.Num(), 0);
		TestEqual(TEXT("Reset clears terrain"), Snapshot.Terrain.Num(), 0);
		TestEqual(TEXT("Reset clears actions"), Snapshot.Actions.Num(), 0);
		TestTrue(
			TEXT("A gated snapshot captures the first inference inside its requested runtime state"),
			Snapshot.CaptureFirstIf(true, SelfObservation, MimicTargetPoses, Terrain, Actions));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimTerrainTraceFrameContractTest,
		"PhysAnim.Bridge.TerrainTraceFrameContract",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimTerrainTraceFrameContractTest::RunTest(const FString& Parameters)
	{
		const FVector RootWorldLocationCm(1234.0f, -567.0f, 250.0f);
		const FQuat RootWorldRotation(FVector::UpVector, FMath::DegreesToRadians(90.0f));
		const FVector SampleWorldLocationCm = BuildTerrainSampleWorldLocation(
			RootWorldLocationCm,
			RootWorldRotation,
			FVector2D(1.0f, 0.0f));

		TestTrue(
			TEXT("A one-meter ProtoMotions terrain offset becomes 100 UE centimeters"),
			SampleWorldLocationCm.Equals(FVector(1234.0f, -467.0f, 250.0f), KINDA_SMALL_NUMBER));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimBalanceStatelessTests,
		"PhysAnim.Bridge.BalanceStateless",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimBalanceStatelessTests::RunTest(const FString& Parameters)
	{
		auto GetDefaultSettings = []()
		{
			FPhysAnimStabilizationSettings S;
			S.MaxRootLinearSpeedCmPerSecond = 100.0f;
			S.MaxRootAngularSpeedDegPerSecond = 45.0f;
			S.BalanceEntryMaxGroundDistanceCm = 15.0f;
			S.BalancePhase2EntryMaxRootTiltDeg = 20.0f;
			S.BalancePhase2EntryMaxShellOffsetDelta = 5.0f;
			S.BalancePhase2EntryMaxShellVelocityDelta = 10.0f;
			S.BalancePhase2EntryMaxTargetDeltaDeg = 15.0f;
			return S;
		};

		auto GetStableDomain = []()
		{
			FPhysAnimStabilizationDomain D;
			D.bRootSimulating = true;
			D.RootLinearSpeed = 10.0f;
			D.RootAngularSpeed = 5.0f;
			D.RootGroundDistance = 5.0f;
			D.RootTiltDeg = 2.0f;
			D.ShellPlanarOffsetCm = 1.0f;
			D.ShellPlanarVelocityCmPerSec = 2.0f;
			D.MaxTargetDeltaDegrees = 3.0f;
			D.MeanTargetDeltaDegrees = 1.0f;
			return D;
		};

		FString Reason;
		TestTrue(TEXT("Baseline stable domain"), FPhysAnimBalanceReadyTransition::IsSnapshotReady(GetStableDomain(), GetDefaultSettings(), Reason));
		TestEqual(
			TEXT("RootOn resolver reports pelvis-spine margin insufficiency"),
			FPhysAnimBalanceReadyTransition::ResolveRootOnReadinessGateReason(
				EBalanceReadyRootOnReadinessClassification::RootCoupledReady,
				true,
				true,
				true,
				false,
				true,
				true,
				true,
				true,
				true,
				false,
				1.0f),
			TEXT("phase1_root_on_readiness_pelvis_spine_margin_insufficient"));
		TestEqual(
			TEXT("RootOn resolver reports pelvis-thigh margin insufficiency"),
			FPhysAnimBalanceReadyTransition::ResolveRootOnReadinessGateReason(
				EBalanceReadyRootOnReadinessClassification::RootCoupledReady,
				true,
				true,
				false,
				true,
				true,
				true,
				true,
				true,
				true,
				false,
				1.0f),
			TEXT("phase1_root_on_readiness_pelvis_thigh_margin_insufficient"));
		TestEqual(
			TEXT("RootOn resolver reports tilt-limited viability before generic angular incoherence"),
			FPhysAnimBalanceReadyTransition::ResolveRootOnReadinessGateReason(
				EBalanceReadyRootOnReadinessClassification::RootCoupledReady,
				true,
				false,
				true,
				true,
				true,
				true,
				true,
				true,
				true,
				true,
				1.0f),
			TEXT("phase1_root_on_readiness_tilt_limited_viability"));
		FPhysAnimStabilizationDomain Phase2TopologyLoss = GetStableDomain();
		Phase2TopologyLoss.CurrentPhase = EBalanceReadyTransitionPhase::BRT_Phase2_RootOn;
		Phase2TopologyLoss.CertifiedSimCount = 6;
		Phase2TopologyLoss.SimCount = 5;
		Phase2TopologyLoss.CertifiedDistalSimCount = 0;
		Phase2TopologyLoss.DistalSimCount = 0;
		TestFalse(
			TEXT("Phase 2 topology loss fails readiness"),
			FPhysAnimBalanceReadyTransition::IsSnapshotReady(Phase2TopologyLoss, GetDefaultSettings(), Reason));
		TestEqual(
			TEXT("Phase 2 topology loss uses the RootOn-specific reason"),
			Reason,
			BalanceReadinessReasons::Phase2TopologyNotPreserved);
		FPhysAnimStabilizationDomain Phase2RootDropped = GetStableDomain();
		Phase2RootDropped.CurrentPhase = EBalanceReadyTransitionPhase::BRT_Phase2_RootOn;
		Phase2RootDropped.bRootSimulating = false;
		TestFalse(
			TEXT("Phase 2 root simulation drop fails readiness"),
			FPhysAnimBalanceReadyTransition::IsSnapshotReady(Phase2RootDropped, GetDefaultSettings(), Reason));
		TestEqual(
			TEXT("Phase 2 root simulation drop uses the RootOn-specific reason"),
			Reason,
			BalanceReadinessReasons::Phase2RootSimulationDropped);
		FPhysAnimStabilizationDomain Phase2Instability = GetStableDomain();
		Phase2Instability.CurrentPhase = EBalanceReadyTransitionPhase::BRT_Phase2_RootOn;
		Phase2Instability.RootLinearSpeed = 300.0f;
		TestFalse(
			TEXT("Phase 2 instability fails readiness"),
			FPhysAnimBalanceReadyTransition::IsSnapshotReady(Phase2Instability, GetDefaultSettings(), Reason));
		TestEqual(
			TEXT("Phase 2 instability uses the RootOn-specific spike reason"),
			Reason,
			BalanceReadinessReasons::Phase2RootOnSpike);
		FPhysAnimStabilizationDomain Phase3TopologyRegression = GetStableDomain();
		Phase3TopologyRegression.CurrentPhase = EBalanceReadyTransitionPhase::BRT_Phase3_Settle;
		Phase3TopologyRegression.CertifiedSimCount = 6;
		Phase3TopologyRegression.SimCount = 5;
		Phase3TopologyRegression.CertifiedDistalSimCount = 0;
		Phase3TopologyRegression.DistalSimCount = 0;
		TestFalse(
			TEXT("Phase 3 topology regression fails readiness"),
			FPhysAnimBalanceReadyTransition::IsSnapshotReady(Phase3TopologyRegression, GetDefaultSettings(), Reason));
		TestEqual(
			TEXT("Phase 3 topology regression uses the Settle-specific reason"),
			Reason,
			BalanceReadinessReasons::Phase3TopologyRegressed);
		FPhysAnimStabilizationDomain Phase3RootDropped = GetStableDomain();
		Phase3RootDropped.CurrentPhase = EBalanceReadyTransitionPhase::BRT_Phase3_Settle;
		Phase3RootDropped.bRootSimulating = false;
		TestFalse(
			TEXT("Phase 3 root simulation drop fails readiness"),
			FPhysAnimBalanceReadyTransition::IsSnapshotReady(Phase3RootDropped, GetDefaultSettings(), Reason));
		TestEqual(
			TEXT("Phase 3 root simulation drop uses the Settle-specific reason"),
			Reason,
			BalanceReadinessReasons::Phase3RootSimulationDropped);
		FPhysAnimStabilizationDomain Phase3Instability = GetStableDomain();
		Phase3Instability.CurrentPhase = EBalanceReadyTransitionPhase::BRT_Phase3_Settle;
		Phase3Instability.RootLinearSpeed = 300.0f;
		TestFalse(
			TEXT("Phase 3 instability fails readiness"),
			FPhysAnimBalanceReadyTransition::IsSnapshotReady(Phase3Instability, GetDefaultSettings(), Reason));
		TestEqual(
			TEXT("Phase 3 instability uses the Settle-specific instability reason"),
			Reason,
			BalanceReadinessReasons::Phase3InstabilitySpike);
		FPhysAnimStabilizationDomain Phase3ThresholdRegression = GetStableDomain();
		Phase3ThresholdRegression.CurrentPhase = EBalanceReadyTransitionPhase::BRT_Phase3_Settle;
		Phase3ThresholdRegression.RootLinearSpeed = 300.0f;
		Phase3ThresholdRegression.RootAngularSpeed = 200.0f;
		TestFalse(
			TEXT("Phase 3 does not widen instability thresholds beyond the bounded 2.5x and 3.0x Settle multipliers"),
			FPhysAnimBalanceReadyTransition::IsSnapshotReady(Phase3ThresholdRegression, GetDefaultSettings(), Reason));
		TestEqual(
			TEXT("Phase 3 widened-threshold regression still reports the Settle instability reason"),
			Reason,
			BalanceReadinessReasons::Phase3InstabilitySpike);
		FPhase1AcceptedConvergenceSnapshot Phase3Snapshot;
		Phase3Snapshot.bIsPelvisSimulating = true;
		Phase3Snapshot.RootLinearSpeed = 200.0f;
		Phase3Snapshot.RootAngularSpeed = 100.0f;
		Phase3Snapshot.RootGroundDistance = 5.0f;
		TestTrue(
			TEXT("Phase-aware root stability uses the Settle thresholds instead of silently falling back to Phase 1"),
			FPhysAnimBalanceReadyTransition::IsRootStable(
				Phase3Snapshot,
				EBalanceReadyTransitionPhase::BRT_Phase3_Settle,
				GetDefaultSettings(),
				Reason));
		TestEqual(
			TEXT("Phase-aware Settle root stability reports ready when only the relaxed Phase 3 thresholds permit it"),
			Reason,
			BalanceReadinessReasons::Ready);
		Phase3Snapshot.bIsPelvisSimulating = false;
		TestFalse(
			TEXT("Phase-aware root stability keeps the Settle-specific root simulation drop reason"),
			FPhysAnimBalanceReadyTransition::IsRootStable(
				Phase3Snapshot,
				EBalanceReadyTransitionPhase::BRT_Phase3_Settle,
				GetDefaultSettings(),
				Reason));
		TestEqual(
			TEXT("Phase-aware root stability does not overclaim a simulated root from a stale snapshot wrapper"),
			Reason,
			BalanceReadinessReasons::Phase3RootSimulationDropped);
		TestFalse(
			TEXT("Idle shell state with large deltas is not material shell correction by itself"),
			FPhysAnimBalanceReadyTransition::IsMaterialShellCorrectionActive(
				false,
				0.0f,
				410.11f,
				GetDefaultSettings().BalancePhase2AbortShellOffsetDelta,
				GetDefaultSettings().BalancePhase2AbortShellVelocityDelta));
		TestTrue(
			TEXT("Active shell correction owner with threshold breach is material shell correction"),
			FPhysAnimBalanceReadyTransition::IsMaterialShellCorrectionActive(
				true,
				0.0f,
				GetDefaultSettings().BalancePhase2AbortShellVelocityDelta + 1.0f,
				GetDefaultSettings().BalancePhase2AbortShellOffsetDelta,
				GetDefaultSettings().BalancePhase2AbortShellVelocityDelta));
		TestTrue(
			TEXT("Phase 2 treats an explicit transition-owned shell lock as an active shell correction owner while locomotion is idle"),
			FPhysAnimBalanceReadyTransition::IsPhase2ShellCorrectionOwnerActive(
				true,
				true));
		TestTrue(
			TEXT("Phase 2 shell correction remains material at idle when the explicit transition-owned shell lock still owns the shell"),
			FPhysAnimBalanceReadyTransition::IsMaterialShellCorrectionActive(
				FPhysAnimBalanceReadyTransition::IsPhase2ShellCorrectionOwnerActive(
					true,
					true),
				0.0f,
				GetDefaultSettings().BalancePhase2AbortShellVelocityDelta + 1.0f,
				GetDefaultSettings().BalancePhase2AbortShellOffsetDelta,
				GetDefaultSettings().BalancePhase2AbortShellVelocityDelta));
		TestFalse(
			TEXT("Phase 2 idle locomotion without the explicit transition-owned shell lock does not overclaim shell correction ownership"),
			FPhysAnimBalanceReadyTransition::IsPhase2ShellCorrectionOwnerActive(
				false,
				true));
		TestTrue(
			TEXT("Phase 3 keeps shell correction classification active while transition-owned shell lock is held"),
			FPhysAnimBalanceReadyTransition::IsPhase3ShellCorrectionOwnerActive(
				true,
				true));
		TestTrue(
			TEXT("Non-idle locomotion also keeps shell correction classification active without the transition-owned shell lock"),
			FPhysAnimBalanceReadyTransition::IsPhase3ShellCorrectionOwnerActive(
				false,
				false));
		TestFalse(
			TEXT("Idle locomotion without the transition-owned shell lock does not treat shell correction as active by itself"),
			FPhysAnimBalanceReadyTransition::IsPhase3ShellCorrectionOwnerActive(
				false,
				true));
		FPhysAnimBalanceReadyTransition PolicySuppressionTransition;
		FPhysAnimBalanceReadyTransitionSnapshot RootOnSuppressionSnapshot;
		RootOnSuppressionSnapshot.InternalPhase = EBalanceReadyTransitionPhase::BRT_Phase2_RootOn;
		PolicySuppressionTransition.ImportSnapshot(RootOnSuppressionSnapshot);
		TestTrue(
			TEXT("RootOn keeps normal policy fully suppressed to preserve truthful handoff evaluation"),
			PolicySuppressionTransition.ShouldSuppressPolicy());
		TestTrue(
			TEXT("RootOn keeps per-bone policy writes suppressed to preserve truthful handoff evaluation"),
			PolicySuppressionTransition.ShouldSuppressPolicyWrites(TEXT("pelvis")));

		TestEqual(
			TEXT("Phase 2 root simulation drop is owned by RootOn execution"),
			FPhysAnimBalanceReadyTransition::ClassifyConditionOwner(TEXT("phase2_root_simulation_dropped")),
			EBalanceReadyConditionOwner::Phase2RootOnExecution);
		TestEqual(
			TEXT("Phase 2 topology loss is owned by topology enforcement"),
			FPhysAnimBalanceReadyTransition::ClassifyConditionOwner(TEXT("phase2_topology_not_preserved")),
			EBalanceReadyConditionOwner::Phase2TopologyEnforcement);
		TestEqual(
			TEXT("Phase 3 post-root instability is owned by RootOn execution"),
			FPhysAnimBalanceReadyTransition::ClassifyConditionOwner(TEXT("phase3_post_root_on_instability")),
			EBalanceReadyConditionOwner::Phase2RootOnExecution);
		TestEqual(
			TEXT("Phase 3 shell correction is owned by shell maintenance"),
			FPhysAnimBalanceReadyTransition::ClassifyConditionOwner(TEXT("phase3_material_shell_correction")),
			EBalanceReadyConditionOwner::ShellAuthorityMaintenance);
		TestEqual(
			TEXT("Phase 3 timeout is owned by transition recovery"),
			FPhysAnimBalanceReadyTransition::ClassifyConditionOwner(TEXT("phase3_no_convergence_path")),
			EBalanceReadyConditionOwner::TransitionRecovery);
		TestEqual(
			TEXT("Current pelvis-spine readiness blocker is owned by Phase 1 topology shaping"),
			FPhysAnimBalanceReadyTransition::ClassifyConditionOwner(TEXT("phase1_root_on_readiness_pelvis_spine_margin_insufficient")),
			EBalanceReadyConditionOwner::Phase1TopologyShaping);
		TestTrue(
			TEXT("Direct current blocker is recognized as Phase 1 owned"),
			FPhysAnimBalanceReadyTransition::IsPhase1OwnedCondition(TEXT("phase1_root_on_readiness_pelvis_spine_margin_insufficient")));
		TestEqual(
			TEXT("Composite no-convergence reason preserves the underlying current blocker owner"),
			FPhysAnimBalanceReadyTransition::ClassifyConditionOwner(TEXT("phase1_no_convergence_path_phase1_root_on_readiness_pelvis_spine_margin_insufficient")),
			EBalanceReadyConditionOwner::Phase1TopologyShaping);
		TestTrue(
			TEXT("Composite no-convergence reason is still recognized as Phase 1 owned"),
			FPhysAnimBalanceReadyTransition::IsPhase1OwnedCondition(TEXT("phase1_no_convergence_path_phase1_root_on_readiness_pelvis_spine_margin_insufficient")));

		TestTrue(
			TEXT("Topology not preserved remains retryable as a failure class"),
			FPhysAnimBalanceReadyTransition::IsFailureClassRetryable(TEXT("phase2_topology_not_preserved")));
		TestFalse(
			TEXT("RootOn spike is not retryable"),
			FPhysAnimBalanceReadyTransition::IsFailureClassRetryable(TEXT("phase2_root_on_spike")));
		TestFalse(
			TEXT("Phase 3 instability is not retryable"),
			FPhysAnimBalanceReadyTransition::IsFailureClassRetryable(TEXT("phase3_post_root_on_instability")));
		TestFalse(
			TEXT("Phase 3 timeout is not retryable"),
			FPhysAnimBalanceReadyTransition::IsFailureClassRetryable(TEXT("phase3_no_convergence_path")));
		TestFalse(
			TEXT("Phase 3 shell maintenance failure is not retryable"),
			FPhysAnimBalanceReadyTransition::IsFailureClassRetryable(TEXT("phase3_material_shell_correction")));
		TestFalse(
			TEXT("Current pelvis-spine readiness blocker is not retryable as a failure class"),
			FPhysAnimBalanceReadyTransition::IsFailureClassRetryable(TEXT("phase1_root_on_readiness_pelvis_spine_margin_insufficient")));
		TestFalse(
			TEXT("Automatic retry is denied for the direct current blocker"),
			FPhysAnimBalanceReadyTransition::IsAutomaticRetryAllowed(
				TEXT("phase1_root_on_readiness_pelvis_spine_margin_insufficient"),
				true,
				true,
				true,
				true,
				true));

		TestTrue(
			TEXT("Automatic retry requires the full retry gate set"),
			FPhysAnimBalanceReadyTransition::IsAutomaticRetryAllowed(
				TEXT("phase2_topology_not_preserved"),
				true,
				true,
				true,
				true,
				true));
		TestFalse(
			TEXT("Automatic retry is denied for terminal RootOn spike even if gates are satisfied"),
			FPhysAnimBalanceReadyTransition::IsAutomaticRetryAllowed(
				TEXT("phase2_root_on_spike"),
				true,
				true,
				true,
				true,
				true));
		TestFalse(
			TEXT("Automatic retry is denied for Phase 3 shell maintenance failure even if gates are satisfied"),
			FPhysAnimBalanceReadyTransition::IsAutomaticRetryAllowed(
				TEXT("phase3_material_shell_correction"),
				true,
				true,
				true,
				true,
				true));
		TestFalse(
			TEXT("Automatic retry is denied when recovery gates are incomplete"),
			FPhysAnimBalanceReadyTransition::IsAutomaticRetryAllowed(
				TEXT("phase2_topology_not_preserved"),
				true,
				false,
				true,
				true,
				true));

#if !UE_BUILD_SHIPPING
		FPhysAnimBalanceReadyTransitionSnapshot ImportedSnapshot;
		ImportedSnapshot.InternalPhase = EBalanceReadyTransitionPhase::BRT_Phase1_LateValidate;
		ImportedSnapshot.PreviousPhase = EBalanceReadyTransitionPhase::BRT_Phase1_Prepare;
		ImportedSnapshot.RequestReason = TEXT("phase1_auto_calib_test");
		ImportedSnapshot.StableHoldAccumulatedSeconds = 0.25f;
		ImportedSnapshot.PhaseTimeSeconds = 0.50f;
		ImportedSnapshot.TotalTransitionTimeSeconds = 1.25f;
		ImportedSnapshot.TargetDiscontinuityAccumulatedSeconds = 0.10f;
		ImportedSnapshot.LastQuietBlockReason = TEXT("quiet_gate");
		ImportedSnapshot.bPhase1RootOnReadinessNoCouplingProofActive = true;
		ImportedSnapshot.RootOnReadinessNoCouplingProofAccumulatedSeconds = 0.15f;
		ImportedSnapshot.RootOnReadinessNoCouplingPeakBodyLinearSpeed = 44.0f;
		ImportedSnapshot.RootOnReadinessNoCouplingPeakBodyAngularSpeed = 55.0f;
		ImportedSnapshot.RootOnReadinessNoCouplingWorstBone = TEXT("spine_01");
		ImportedSnapshot.CertifiedHandoff.TopologyClass = TEXT("phase1_test_topology");
		ImportedSnapshot.CertifiedHandoff.PelvisSpine01AngularErrorDeg = 17.91f;
		ImportedSnapshot.CertifiedLateValidationResult.RootOnReadinessGateReason = TEXT("phase1_root_on_readiness_pelvis_thigh_margin_insufficient");
		ImportedSnapshot.Diagnostics.FailureReason = TEXT("phase1_root_on_readiness_pelvis_thigh_margin_insufficient");
		ImportedSnapshot.Diagnostics.Phase1RootOnReadinessGateReason = TEXT("phase1_root_on_readiness_pelvis_thigh_margin_insufficient");
		ImportedSnapshot.SafePhase2DenialReason = TEXT("phase1_safe_deny");
		ImportedSnapshot.CachedConvergenceSnapshot.FrameIndex = 42;
		ImportedSnapshot.DistalBoneMismatchTicks.Add(TEXT("foot_l"), 3);
		ImportedSnapshot.LoggedProximalStates.Add(TEXT("spine_01"));
		ImportedSnapshot.Audit.bUsedRelaxedCertification = true;

		FPhysAnimBalanceReadyTransition TransitionSnapshotRoundTrip;
		TransitionSnapshotRoundTrip.ImportSnapshot(ImportedSnapshot);
		const FPhysAnimBalanceReadyTransitionSnapshot ExportedSnapshot = TransitionSnapshotRoundTrip.ExportSnapshot();

		TestEqual(TEXT("Transition snapshot restores internal phase"), ExportedSnapshot.InternalPhase, ImportedSnapshot.InternalPhase);
		TestEqual(TEXT("Transition snapshot restores previous phase"), ExportedSnapshot.PreviousPhase, ImportedSnapshot.PreviousPhase);
		TestEqual(TEXT("Transition snapshot restores request reason"), ExportedSnapshot.RequestReason, ImportedSnapshot.RequestReason);
		TestEqual(TEXT("Transition snapshot restores no-coupling proof timer"), ExportedSnapshot.RootOnReadinessNoCouplingProofAccumulatedSeconds, ImportedSnapshot.RootOnReadinessNoCouplingProofAccumulatedSeconds);
		TestEqual(TEXT("Transition snapshot restores certified topology"), ExportedSnapshot.CertifiedHandoff.TopologyClass, ImportedSnapshot.CertifiedHandoff.TopologyClass);
		TestEqual(TEXT("Transition snapshot restores late-validation reason"), ExportedSnapshot.CertifiedLateValidationResult.RootOnReadinessGateReason, ImportedSnapshot.CertifiedLateValidationResult.RootOnReadinessGateReason);
		TestEqual(TEXT("Transition snapshot restores diagnostics reason"), ExportedSnapshot.Diagnostics.FailureReason, ImportedSnapshot.Diagnostics.FailureReason);
		TestEqual(TEXT("Transition snapshot restores cached convergence frame"), ExportedSnapshot.CachedConvergenceSnapshot.FrameIndex, ImportedSnapshot.CachedConvergenceSnapshot.FrameIndex);
		TestEqual(TEXT("Transition snapshot restores distal mismatch counts"), ExportedSnapshot.DistalBoneMismatchTicks.FindRef(TEXT("foot_l")), 3);
		TestTrue(TEXT("Transition snapshot restores audit flags"), ExportedSnapshot.Audit.bUsedRelaxedCertification);
#endif
		return true;
	}
}
