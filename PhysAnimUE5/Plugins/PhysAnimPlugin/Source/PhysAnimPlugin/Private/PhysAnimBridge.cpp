#include "PhysAnimBridge.h"

#include "Algo/AllOf.h"
#include "Algo/Find.h"
#include "Containers/StaticArray.h"
#include "Misc/Crc.h"

namespace PhysAnimBridge
{
	namespace
	{
		const TArray<FName>& MakeControlledBones()
		{
			static const TArray<FName> Bones =
			{
				TEXT("thigh_l"),
				TEXT("calf_l"),
				TEXT("foot_l"),
				TEXT("ball_l"),
				TEXT("thigh_r"),
				TEXT("calf_r"),
				TEXT("foot_r"),
				TEXT("ball_r"),
				TEXT("spine_01"),
				TEXT("spine_02"),
				TEXT("spine_03"),
				TEXT("neck_01"),
				TEXT("head"),
				TEXT("clavicle_l"),
				TEXT("upperarm_l"),
				TEXT("lowerarm_l"),
				TEXT("hand_l"),
				TEXT("clavicle_r"),
				TEXT("upperarm_r"),
				TEXT("lowerarm_r"),
				TEXT("hand_r")
			};

			return Bones;
		}

		const TArray<FName>& MakeRequiredModifiers()
		{
			static const TArray<FName> Bones =
			{
				TEXT("pelvis"),
				TEXT("thigh_l"),
				TEXT("calf_l"),
				TEXT("foot_l"),
				TEXT("ball_l"),
				TEXT("thigh_r"),
				TEXT("calf_r"),
				TEXT("foot_r"),
				TEXT("ball_r"),
				TEXT("spine_01"),
				TEXT("spine_02"),
				TEXT("spine_03"),
				TEXT("neck_01"),
				TEXT("head"),
				TEXT("clavicle_l"),
				TEXT("upperarm_l"),
				TEXT("lowerarm_l"),
				TEXT("hand_l"),
				TEXT("clavicle_r"),
				TEXT("upperarm_r"),
				TEXT("lowerarm_r"),
				TEXT("hand_r")
			};

			return Bones;
		}

		const TArray<FName>& MakeSmplObservationBones()
		{
			// Must match ProtoMotions smpl.yaml body_names (DFS traversal order).
			static const TArray<FName> Bones =
			{
				TEXT("pelvis"),      // 0:  Pelvis
				TEXT("thigh_l"),     // 1:  L_Hip
				TEXT("calf_l"),      // 2:  L_Knee
				TEXT("foot_l"),      // 3:  L_Ankle
				TEXT("ball_l"),      // 4:  L_Toe
				TEXT("thigh_r"),     // 5:  R_Hip
				TEXT("calf_r"),      // 6:  R_Knee
				TEXT("foot_r"),      // 7:  R_Ankle
				TEXT("ball_r"),      // 8:  R_Toe
				TEXT("spine_01"),    // 9:  Torso
				TEXT("spine_02"),    // 10: Spine
				TEXT("spine_03"),    // 11: Chest
				TEXT("neck_01"),     // 12: Neck
				TEXT("head"),        // 13: Head
				TEXT("clavicle_l"),  // 14: L_Thorax
				TEXT("upperarm_l"),  // 15: L_Shoulder
				TEXT("lowerarm_l"),  // 16: L_Elbow
				TEXT("hand_l"),      // 17: L_Wrist
				TEXT("hand_l"),      // 18: L_Hand (collapsed with L_Wrist)
				TEXT("clavicle_r"),  // 19: R_Thorax
				TEXT("upperarm_r"),  // 20: R_Shoulder
				TEXT("lowerarm_r"),  // 21: R_Elbow
				TEXT("hand_r"),      // 22: R_Wrist
				TEXT("hand_r")       // 23: R_Hand (collapsed with R_Wrist)
			};

			return Bones;
		}

		FName MakeRootBone()
		{
			return TEXT("pelvis");
		}

		FQuat MakeAxisAngleQuaternion(const FVector& Axis, double AngleRadians)
		{
			if (FMath::IsNearlyZero(AngleRadians))
			{
				return FQuat::Identity;
			}

			return FQuat(Axis.GetSafeNormal(), AngleRadians).GetNormalized();
		}

		FQuat MakeQuaternionFromBasis(const FVector& XAxis, const FVector& ZAxis)
		{
			return FRotationMatrix::MakeFromXZ(XAxis.GetSafeNormal(), ZAxis.GetSafeNormal()).ToQuat().GetNormalized();
		}

		void AppendQuaternionTanNorm(const FQuat& Rotation, TArray<float>& OutValues)
		{
			float TanNorm[6];
			QuaternionToTanNorm(Rotation, TanNorm);
			OutValues.Append(TanNorm, UE_ARRAY_COUNT(TanNorm));
		}

		FTransform ValidateAndGetFutureTransform(
			const FPhysAnimFuturePoseSample& FutureSample,
			int32 BodyIndex,
			FString& OutError)
		{
			if (!FutureSample.BodyTransforms.IsValidIndex(BodyIndex))
			{
				OutError = FString::Printf(TEXT("Future pose sample is missing body transform at index %d."), BodyIndex);
				return FTransform::Identity;
			}

			return FutureSample.BodyTransforms[BodyIndex];
		}

		const TArray<FVector2D>& MakeTerrainSampleOffsets()
		{
			static const TArray<FVector2D> Offsets = []
			{
				TArray<FVector2D> Result;
				Result.Reserve(TerrainSize);

				for (int32 XIndex = 0; XIndex < TerrainSamplesPerAxis; ++XIndex)
				{
					const float AlphaX = static_cast<float>(XIndex) / static_cast<float>(TerrainSamplesPerAxis - 1);
					const float LocalX = FMath::Lerp(-TerrainSampleWidth, TerrainSampleWidth, AlphaX);

					for (int32 YIndex = 0; YIndex < TerrainSamplesPerAxis; ++YIndex)
					{
						const float AlphaY = static_cast<float>(YIndex) / static_cast<float>(TerrainSamplesPerAxis - 1);
						const float LocalY = FMath::Lerp(-TerrainSampleWidth, TerrainSampleWidth, AlphaY);
						Result.Emplace(LocalX, LocalY);
					}
				}

				return Result;
			}();

			return Offsets;
		}

		bool ValidateFloatBatchFeatureTensorDesc(
			const UE::NNE::FTensorDesc& TensorDesc,
			int32 ExpectedFeatureWidth,
			FString& OutError)
		{
			if (TensorDesc.GetDataType() != ENNETensorDataType::Float)
			{
				OutError = FString::Printf(
					TEXT("Expected input tensor '%s' to be Float but found data type %d."),
					*TensorDesc.GetName(),
					static_cast<int32>(TensorDesc.GetDataType()));
				return false;
			}

			const TConstArrayView<int32> ShapeData = TensorDesc.GetShape().GetData();
			if (ShapeData.Num() != 2)
			{
				OutError = FString::Printf(
					TEXT("Expected input tensor '%s' to have rank 2 but found rank %d."),
					*TensorDesc.GetName(),
					ShapeData.Num());
				return false;
			}

			const int32 BatchDim = ShapeData[0];
			if (BatchDim != 1 && BatchDim != -1)
			{
				OutError = FString::Printf(
					TEXT("Expected input tensor '%s' batch dimension to be 1 or -1 but found %d."),
					*TensorDesc.GetName(),
					BatchDim);
				return false;
			}

			const int32 FeatureWidth = ShapeData[1];
			if (FeatureWidth != ExpectedFeatureWidth)
			{
				OutError = FString::Printf(
					TEXT("Expected input tensor '%s' feature width to be %d but found %d."),
					*TensorDesc.GetName(),
					ExpectedFeatureWidth,
					FeatureWidth);
				return false;
			}

			return true;
		}
	}

	const TArray<FName>& GetControlledBoneNames()
	{
		return MakeControlledBones();
	}

	const TArray<FPhysAnimProtoActionJointDescriptor>& GetProtoActionJointDescriptors()
	{
		static const TArray<FPhysAnimProtoActionJointDescriptor> Descriptors = {
			{ 0, TEXT("L_Hip"), TEXT("thigh_l"), false },
			{ 1, TEXT("L_Knee"), TEXT("calf_l"), false },
			{ 2, TEXT("L_Ankle"), TEXT("foot_l"), false },
			{ 3, TEXT("L_Toe"), TEXT("ball_l"), false },
			{ 4, TEXT("R_Hip"), TEXT("thigh_r"), false },
			{ 5, TEXT("R_Knee"), TEXT("calf_r"), false },
			{ 6, TEXT("R_Ankle"), TEXT("foot_r"), false },
			{ 7, TEXT("R_Toe"), TEXT("ball_r"), false },
			{ 8, TEXT("Torso"), TEXT("spine_01"), false },
			{ 9, TEXT("Spine"), TEXT("spine_02"), false },
			{ 10, TEXT("Chest"), TEXT("spine_03"), false },
			{ 11, TEXT("Neck"), TEXT("neck_01"), false },
			{ 12, TEXT("Head"), TEXT("head"), false },
			{ 13, TEXT("L_Thorax"), TEXT("clavicle_l"), false },
			{ 14, TEXT("L_Shoulder"), TEXT("upperarm_l"), false },
			{ 15, TEXT("L_Elbow"), TEXT("lowerarm_l"), false },
			{ 16, TEXT("L_Wrist"), TEXT("hand_l"), true },
			{ 17, TEXT("L_Hand"), TEXT("hand_l"), true },
			{ 18, TEXT("R_Thorax"), TEXT("clavicle_r"), false },
			{ 19, TEXT("R_Shoulder"), TEXT("upperarm_r"), false },
			{ 20, TEXT("R_Elbow"), TEXT("lowerarm_r"), false },
			{ 21, TEXT("R_Wrist"), TEXT("hand_r"), true },
			{ 22, TEXT("R_Hand"), TEXT("hand_r"), true }
		};
		return Descriptors;
	}

	const TArray<FName>& GetRequiredBodyModifierBoneNames()
	{
		return MakeRequiredModifiers();
	}

	const TArray<FName>& GetSmplObservationBoneNames()
	{
		return MakeSmplObservationBones();
	}

	FName GetRootBoneName()
	{
		return MakeRootBone();
	}

	FName MakeControlName(const FName BoneName)
	{
		return *FString::Printf(TEXT("PACtrl_%s"), *BoneName.ToString());
	}

	FName MakeBodyModifierName(const FName BoneName)
	{
		return *FString::Printf(TEXT("PAMod_%s"), *BoneName.ToString());
	}

	FName GetBoneNameFromControlName(const FName ControlName)
	{
		FString Name = ControlName.ToString();
		if (Name.StartsWith(TEXT("PACtrl_")))
		{
			return *Name.RightChop(7);
		}
		return ControlName;
	}
	
	FName GetBoneNameFromBodyModifierName(const FName ModifierName)
	{
		FString Name = ModifierName.ToString();
		if (Name.StartsWith(TEXT("PAMod_")))
		{
			return *Name.RightChop(6);
		}
		return ModifierName;
	}

	bool BuildInputTensorIndexMap(
		const TArray<UE::NNE::FTensorDesc>& InputTensorDescs,
		FPhysAnimTensorIndexMap& OutIndexMap,
		FString& OutError)
	{
		OutIndexMap = {};

		if (InputTensorDescs.Num() != 3)
		{
			OutError = FString::Printf(TEXT("Expected exactly 3 input tensors but found %d."), InputTensorDescs.Num());
			return false;
		}

		for (int32 TensorIndex = 0; TensorIndex < InputTensorDescs.Num(); ++TensorIndex)
		{
			const FString& Name = InputTensorDescs[TensorIndex].GetName();
			if (Name == TEXT("self_obs"))
			{
				if (OutIndexMap.SelfObs != INDEX_NONE)
				{
					OutError = TEXT("Input tensor 'self_obs' was duplicated.");
					return false;
				}
				OutIndexMap.SelfObs = TensorIndex;
			}
			else if (Name == TEXT("mimic_target_poses"))
			{
				if (OutIndexMap.MimicTargetPoses != INDEX_NONE)
				{
					OutError = TEXT("Input tensor 'mimic_target_poses' was duplicated.");
					return false;
				}
				OutIndexMap.MimicTargetPoses = TensorIndex;
			}
			else if (Name == TEXT("terrain"))
			{
				if (OutIndexMap.Terrain != INDEX_NONE)
				{
					OutError = TEXT("Input tensor 'terrain' was duplicated.");
					return false;
				}
				OutIndexMap.Terrain = TensorIndex;
			}
			else
			{
				OutError = FString::Printf(TEXT("Unexpected input tensor '%s'."), *Name);
				return false;
			}
		}

		if (!OutIndexMap.IsValid())
		{
			OutError = TEXT("One or more required input tensors were missing.");
			return false;
		}

		return true;
	}

	bool ValidateInputTensorDescs(
		const TArray<UE::NNE::FTensorDesc>& InputTensorDescs,
		FPhysAnimTensorIndexMap& OutIndexMap,
		FString& OutError)
	{
		if (!BuildInputTensorIndexMap(InputTensorDescs, OutIndexMap, OutError))
		{
			return false;
		}

		if (!ValidateFloatBatchFeatureTensorDesc(InputTensorDescs[OutIndexMap.SelfObs], SelfObsSize, OutError))
		{
			return false;
		}

		if (!ValidateFloatBatchFeatureTensorDesc(InputTensorDescs[OutIndexMap.MimicTargetPoses], MimicTargetPosesSize, OutError))
		{
			return false;
		}

		if (!ValidateFloatBatchFeatureTensorDesc(InputTensorDescs[OutIndexMap.Terrain], TerrainSize, OutError))
		{
			return false;
		}

		return true;
	}

	bool ValidateActionOutputTensorDescs(
		TConstArrayView<UE::NNE::FTensorDesc> OutputTensorDescs,
		FString& OutError)
	{
		if (OutputTensorDescs.Num() != 1)
		{
			OutError = FString::Printf(TEXT("Expected exactly one output tensor but found %d."), OutputTensorDescs.Num());
			return false;
		}

		const UE::NNE::FTensorDesc& ActionOutputDesc = OutputTensorDescs[0];
		if (ActionOutputDesc.GetDataType() != ENNETensorDataType::Float)
		{
			OutError = FString::Printf(
				TEXT("Expected action output tensor '%s' to be Float but found data type %d."),
				*ActionOutputDesc.GetName(),
				static_cast<int32>(ActionOutputDesc.GetDataType()));
			return false;
		}

		const TConstArrayView<int32> ShapeData = ActionOutputDesc.GetShape().GetData();
		if (ShapeData.Num() != 2)
		{
			OutError = FString::Printf(
				TEXT("Expected action output tensor '%s' to have rank 2 but found rank %d."),
				*ActionOutputDesc.GetName(),
				ShapeData.Num());
			return false;
		}

		const int32 BatchDim = ShapeData[0];
		if (BatchDim != 1 && BatchDim != -1)
		{
			OutError = FString::Printf(
				TEXT("Expected action output tensor '%s' batch dimension to be 1 or -1 but found %d."),
				*ActionOutputDesc.GetName(),
				BatchDim);
			return false;
		}

		const int32 ActionDim = ShapeData[1];
		if (ActionDim != NumActionFloats)
		{
			OutError = FString::Printf(
				TEXT("Expected action output tensor '%s' action dimension to be %d but found %d."),
				*ActionOutputDesc.GetName(),
				NumActionFloats,
				ActionDim);
			return false;
		}

		return true;
	}

	bool ValidateFiniteFloatBuffer(
		const TCHAR* BufferName,
		TConstArrayView<float> Values,
		FString& OutError)
	{
		for (int32 ValueIndex = 0; ValueIndex < Values.Num(); ++ValueIndex)
		{
			if (!FMath::IsFinite(Values[ValueIndex]))
			{
				OutError = FString::Printf(TEXT("%s contained NaN or Inf."), BufferName ? BufferName : TEXT("float_buffer"));
				return false;
			}
		}

		return true;
	}

	TArray<float> BuildFutureSampleTimeSchedule()
	{
		TArray<float> SampleTimes;
		SampleTimes.Reserve(NumFutureSteps);

		for (int32 StepIndex = 1; StepIndex <= NumFutureSteps; ++StepIndex)
		{
			SampleTimes.Add(static_cast<float>(StepIndex) * FutureStepSeconds);
		}

		return SampleTimes;
	}

	float ResolveFutureTargetTimeSeconds(float CurrentTimeSeconds, float RequestedFutureOffsetSeconds, float AnimationLengthSeconds)
	{
		if (!FMath::IsFinite(CurrentTimeSeconds) ||
			!FMath::IsFinite(RequestedFutureOffsetSeconds) ||
			!FMath::IsFinite(AnimationLengthSeconds) ||
			AnimationLengthSeconds < 0.0f)
		{
			return 0.0f;
		}

		// The sampled animation pose may clamp at the end of a clip, but the policy
		// time channel must retain its fixed future horizon. Query-trajectory placement
		// and the model input both consume this relative schedule.
		return FMath::Max(RequestedFutureOffsetSeconds, 0.0f);
	}

	FVector SmplVectorToUe(const FVector& SmplVector)
	{
		return FVector(SmplVector.Z, SmplVector.X, SmplVector.Y);
	}

	FVector UeVectorToSmpl(const FVector& UeVector)
	{
		return FVector(UeVector.Y, UeVector.Z, UeVector.X);
	}

	FVector UeVectorToIsaacGym(const FVector& UeVector)
	{
		// UE is Z-up, X-forward, Y-right (Left-Handed)
		// Isaac Gym is Z-up, X-forward, Y-left (Right-Handed)
		return FVector(UeVector.X, -UeVector.Y, UeVector.Z);
	}

	FVector IsaacGymVectorToUe(const FVector& IsaacVector)
	{
		return FVector(IsaacVector.X, -IsaacVector.Y, IsaacVector.Z);
	}

	FQuat SmplQuaternionToUe(const FQuat& SmplQuaternion)
	{
		const FVector UeXAxisInSmpl = UeVectorToSmpl(FVector::ForwardVector);
		const FVector UeZAxisInSmpl = UeVectorToSmpl(FVector::UpVector);
		const FVector UeRotatedXAxis = SmplVectorToUe(SmplQuaternion.RotateVector(UeXAxisInSmpl));
		const FVector UeRotatedZAxis = SmplVectorToUe(SmplQuaternion.RotateVector(UeZAxisInSmpl));
		return MakeQuaternionFromBasis(UeRotatedXAxis, UeRotatedZAxis);
	}

	FQuat UeQuaternionToSmpl(const FQuat& UeQuaternion)
	{
		const FVector SmplXAxisInUe = SmplVectorToUe(FVector::ForwardVector);
		const FVector SmplZAxisInUe = SmplVectorToUe(FVector::UpVector);
		const FVector SmplRotatedXAxis = UeVectorToSmpl(UeQuaternion.RotateVector(SmplXAxisInUe));
		const FVector SmplRotatedZAxis = UeVectorToSmpl(UeQuaternion.RotateVector(SmplZAxisInUe));
		return MakeQuaternionFromBasis(SmplRotatedXAxis, SmplRotatedZAxis);
	}

	FQuat ProtoJointQuaternionToUe(const FQuat& ProtoJointQuaternion)
	{
		const FVector UeXAxisInProto = UeVectorToIsaacGym(FVector::ForwardVector);
		const FVector UeZAxisInProto = UeVectorToIsaacGym(FVector::UpVector);
		const FVector UeRotatedXAxis = IsaacGymVectorToUe(ProtoJointQuaternion.RotateVector(UeXAxisInProto));
		const FVector UeRotatedZAxis = IsaacGymVectorToUe(ProtoJointQuaternion.RotateVector(UeZAxisInProto));
		return MakeQuaternionFromBasis(UeRotatedXAxis, UeRotatedZAxis);
	}

	FQuat UeQuaternionToIsaacGym(const FQuat& UeQuaternion)
	{
		const FVector IsaacXAxisInUe = IsaacGymVectorToUe(FVector::ForwardVector);
		const FVector IsaacZAxisInUe = IsaacGymVectorToUe(FVector::UpVector);
		const FVector IsaacRotatedXAxis = UeVectorToIsaacGym(UeQuaternion.RotateVector(IsaacXAxisInUe));
		const FVector IsaacRotatedZAxis = UeVectorToIsaacGym(UeQuaternion.RotateVector(IsaacZAxisInUe));
		return MakeQuaternionFromBasis(IsaacRotatedXAxis, IsaacRotatedZAxis);
	}

	FVector UeWorldPositionToProtoRuntime(const FVector& UeVector)
	{
		return UeVectorToIsaacGym(UeVector) * CmToMeters;
	}
	
	FVector UeWorldVelocityToProtoRuntime(const FVector& UeVector)
	{
		return UeVectorToIsaacGym(UeVector) * CmToMeters;
	}
	
	FVector UeWorldRotationVectorToProtoRuntime(const FVector& UeVector)
	{
		// Angular velocity is an axial vector. Crossing from UE's left-handed
		// basis to ProtoMotions' right-handed basis therefore needs the
		// determinant sign in addition to the polar-vector Y reflection.
		return FVector(-UeVector.X, UeVector.Y, -UeVector.Z);
	}

	FQuat UeWorldQuaternionToProtoRuntime(const FQuat& UeQuaternion)
	{
		return UeQuaternionToIsaacGym(UeQuaternion);
	}

	FQuat ExpMapToQuaternion(const FVector& ExpMap)
	{
		const double AngleRadians = ExpMap.Length();
		if (AngleRadians <= UE_DOUBLE_SMALL_NUMBER)
		{
			return FQuat::Identity;
		}

		return MakeAxisAngleQuaternion(ExpMap / AngleRadians, AngleRadians);
	}

	FQuat CalculateHeadingInverseSmpl(const FQuat& SmplRootRotation)
	{
		const FVector Forward = SmplRootRotation.RotateVector(FVector::ForwardVector);
		const double HeadingRadians = FMath::Atan2(Forward.Y, Forward.X);
		return MakeAxisAngleQuaternion(FVector::UpVector, -HeadingRadians);
	}

	void QuaternionToTanNorm(const FQuat& Rotation, float OutTanNorm[6])
	{
		const FVector Tangent = Rotation.RotateVector(FVector::ForwardVector);
		const FVector Normal = Rotation.RotateVector(FVector::UpVector);

		OutTanNorm[0] = static_cast<float>(Tangent.X);
		OutTanNorm[1] = static_cast<float>(Tangent.Y);
		OutTanNorm[2] = static_cast<float>(Tangent.Z);
		OutTanNorm[3] = static_cast<float>(Normal.X);
		OutTanNorm[4] = static_cast<float>(Normal.Y);
		OutTanNorm[5] = static_cast<float>(Normal.Z);
	}

	FQuat CollapseDistalHandRotation(const FQuat& WristRotation, const FQuat& HandRotation)
	{
		return (WristRotation * HandRotation).GetNormalized();
	}

	bool BuildSelfObservation(
		const TArray<FPhysAnimBodySample>& BodySamples,
		float GroundHeight,
		TArray<float>& OutSelfObservation,
		FString& OutError)
	{
		if (BodySamples.Num() != NumSmplBodies)
		{
			OutError = FString::Printf(TEXT("Expected %d body samples but found %d."), NumSmplBodies, BodySamples.Num());
			return false;
		}

		OutSelfObservation.Reset();
		OutSelfObservation.Reserve(SelfObsSize);

		const FPhysAnimBodySample& RootSample = BodySamples[0];
		const FQuat HeadingInverse = CalculateHeadingInverseSmpl(RootSample.Rotation);

		OutSelfObservation.Add(static_cast<float>(RootSample.Position.Z - GroundHeight));

		for (int32 BodyIndex = 1; BodyIndex < BodySamples.Num(); ++BodyIndex)
		{
			const FVector LocalPosition = HeadingInverse.RotateVector(BodySamples[BodyIndex].Position - RootSample.Position);
			OutSelfObservation.Add(static_cast<float>(LocalPosition.X));
			OutSelfObservation.Add(static_cast<float>(LocalPosition.Y));
			OutSelfObservation.Add(static_cast<float>(LocalPosition.Z));
		}

		for (const FPhysAnimBodySample& BodySample : BodySamples)
		{
			AppendQuaternionTanNorm((HeadingInverse * BodySample.Rotation).GetNormalized(), OutSelfObservation);
		}

		for (const FPhysAnimBodySample& BodySample : BodySamples)
		{
			const FVector LocalVelocity = HeadingInverse.RotateVector(BodySample.LinearVelocity);
			OutSelfObservation.Add(static_cast<float>(LocalVelocity.X));
			OutSelfObservation.Add(static_cast<float>(LocalVelocity.Y));
			OutSelfObservation.Add(static_cast<float>(LocalVelocity.Z));
		}

		for (const FPhysAnimBodySample& BodySample : BodySamples)
		{
			const FVector LocalAngularVelocity = HeadingInverse.RotateVector(BodySample.AngularVelocity);
			OutSelfObservation.Add(static_cast<float>(LocalAngularVelocity.X));
			OutSelfObservation.Add(static_cast<float>(LocalAngularVelocity.Y));
			OutSelfObservation.Add(static_cast<float>(LocalAngularVelocity.Z));
		}

		if (OutSelfObservation.Num() != SelfObsSize)
		{
			OutError = FString::Printf(TEXT("Built self observation with %d floats instead of %d."), OutSelfObservation.Num(), SelfObsSize);
			return false;
		}

		return true;
	}

	bool BuildMimicTargetPoses(
		const TArray<FPhysAnimBodySample>& CurrentBodySamples,
		const TArray<FPhysAnimFuturePoseSample>& FuturePoseSamples,
		TArray<float>& OutMimicTargetPoses,
		FString& OutError)
	{
		if (CurrentBodySamples.Num() != NumSmplBodies)
		{
			OutError = FString::Printf(TEXT("Expected %d current body samples but found %d."), NumSmplBodies, CurrentBodySamples.Num());
			return false;
		}

		if (FuturePoseSamples.Num() != NumFutureSteps)
		{
			OutError = FString::Printf(TEXT("Expected %d future pose samples but found %d."), NumFutureSteps, FuturePoseSamples.Num());
			return false;
		}

		OutMimicTargetPoses.Reset();
		OutMimicTargetPoses.Reserve(MimicTargetPosesSize);

		for (int32 FutureIndex = 0; FutureIndex < FuturePoseSamples.Num(); ++FutureIndex)
		{
			const FPhysAnimFuturePoseSample& TargetSample = FuturePoseSamples[FutureIndex];
			const bool bUseCurrentReference = FutureIndex == 0;
			const FPhysAnimFuturePoseSample* const PreviousSample = bUseCurrentReference ? nullptr : &FuturePoseSamples[FutureIndex - 1];

			const FVector ReferenceRootPosition = bUseCurrentReference
				? CurrentBodySamples[0].Position
				: ValidateAndGetFutureTransform(*PreviousSample, 0, OutError).GetLocation();
			if (!OutError.IsEmpty())
			{
				return false;
			}

			const FQuat ReferenceRootRotation = bUseCurrentReference
				? CurrentBodySamples[0].Rotation
				: ValidateAndGetFutureTransform(*PreviousSample, 0, OutError).GetRotation();
			if (!OutError.IsEmpty())
			{
				return false;
			}

			const FQuat HeadingInverse = CalculateHeadingInverseSmpl(ReferenceRootRotation);

			for (int32 BodyIndex = 0; BodyIndex < NumSmplBodies; ++BodyIndex)
			{
				const FTransform TargetBody = ValidateAndGetFutureTransform(TargetSample, BodyIndex, OutError);
				if (!OutError.IsEmpty())
				{
					return false;
				}

				const FVector ReferencePosition = bUseCurrentReference
					? CurrentBodySamples[BodyIndex].Position
					: ValidateAndGetFutureTransform(*PreviousSample, BodyIndex, OutError).GetLocation();
				if (!OutError.IsEmpty())
				{
					return false;
				}

				const FVector RelativeBodyPosition = HeadingInverse.RotateVector(TargetBody.GetLocation() - ReferencePosition);
				OutMimicTargetPoses.Add(static_cast<float>(RelativeBodyPosition.X));
				OutMimicTargetPoses.Add(static_cast<float>(RelativeBodyPosition.Y));
				OutMimicTargetPoses.Add(static_cast<float>(RelativeBodyPosition.Z));
			}

			for (int32 BodyIndex = 0; BodyIndex < NumSmplBodies; ++BodyIndex)
			{
				const FTransform TargetBody = ValidateAndGetFutureTransform(TargetSample, BodyIndex, OutError);
				if (!OutError.IsEmpty())
				{
					return false;
				}

				const FVector RootRelativeBodyPosition = HeadingInverse.RotateVector(TargetBody.GetLocation() - ReferenceRootPosition);
				OutMimicTargetPoses.Add(static_cast<float>(RootRelativeBodyPosition.X));
				OutMimicTargetPoses.Add(static_cast<float>(RootRelativeBodyPosition.Y));
				OutMimicTargetPoses.Add(static_cast<float>(RootRelativeBodyPosition.Z));
			}

			for (int32 BodyIndex = 0; BodyIndex < NumSmplBodies; ++BodyIndex)
			{
				const FTransform TargetBody = ValidateAndGetFutureTransform(TargetSample, BodyIndex, OutError);
				if (!OutError.IsEmpty())
				{
					return false;
				}

				const FQuat ReferenceRotation = bUseCurrentReference
					? CurrentBodySamples[BodyIndex].Rotation
					: ValidateAndGetFutureTransform(*PreviousSample, BodyIndex, OutError).GetRotation();
				if (!OutError.IsEmpty())
				{
					return false;
				}

				AppendQuaternionTanNorm((ReferenceRotation.Inverse() * TargetBody.GetRotation()).GetNormalized(), OutMimicTargetPoses);
			}

			for (int32 BodyIndex = 0; BodyIndex < NumSmplBodies; ++BodyIndex)
			{
				const FTransform TargetBody = ValidateAndGetFutureTransform(TargetSample, BodyIndex, OutError);
				if (!OutError.IsEmpty())
				{
					return false;
				}

				AppendQuaternionTanNorm((HeadingInverse * TargetBody.GetRotation()).GetNormalized(), OutMimicTargetPoses);
			}

			OutMimicTargetPoses.Add(TargetSample.FutureTimeSeconds);
		}

		if (OutMimicTargetPoses.Num() != MimicTargetPosesSize)
		{
			OutError = FString::Printf(
				TEXT("Built mimic target poses with %d floats instead of %d."),
				OutMimicTargetPoses.Num(),
				MimicTargetPosesSize);
			return false;
		}

		return true;
	}

	const TArray<FVector2D>& GetTerrainSampleOffsets()
	{
		return MakeTerrainSampleOffsets();
	}

	FVector BuildTerrainSampleWorldLocation(
		const FVector& RootWorldLocationCm,
		const FQuat& RootWorldRotation,
		const FVector2D& LocalOffsetMeters)
	{
		const FQuat RootYawRotation = FRotator(0.0f, RootWorldRotation.Rotator().Yaw, 0.0f).Quaternion();
		const FVector LocalOffsetCm(LocalOffsetMeters.X * MetersToCm, LocalOffsetMeters.Y * MetersToCm, 0.0f);
		return RootWorldLocationCm + RootYawRotation.RotateVector(LocalOffsetCm);
	}

	bool BuildTerrainObservation(
		float RootHeight,
		const TArray<float>& SampleGroundHeights,
		TArray<float>& OutTerrain,
		FString& OutError)
	{
		if (SampleGroundHeights.Num() != TerrainSize)
		{
			OutError = FString::Printf(
				TEXT("Expected %d terrain ground-height samples but found %d."),
				TerrainSize,
				SampleGroundHeights.Num());
			return false;
		}

		OutTerrain.SetNumUninitialized(TerrainSize);
		for (int32 SampleIndex = 0; SampleIndex < TerrainSize; ++SampleIndex)
		{
			OutTerrain[SampleIndex] = RootHeight - SampleGroundHeights[SampleIndex];
		}

		return true;
	}


#if WITH_DEV_AUTOMATION_TESTS
	bool FPhysAnimLocomotionFrameReplaySnapshot::CaptureFirstIf(
		bool bCondition,
		const FString& InRuntimeState,
		double InWorldTimeSeconds,
		int32 InPolicyControlTick,
		const FString& InPoseSearchAnimation,
		float InPoseSearchSelectedTime,
		bool bInPoseSearchMirrored,
		const FTransform& InOwnerActorWorldTransform,
		const FTransform& InMeshWorldTransform,
		const FTransform& InSelectedAnimationWorldRootProtoMeters,
		const FTransform& InSelectedAnimationDataRootProtoMeters,
		float InIntentMagnitude,
		const FVector& InDesiredVelocityCmPerSecond,
		const FVector& InAcceptedVelocityCmPerSecond,
		const FVector& InResolvedQueryVelocityCmPerSecond,
		bool bInUseStabilizedWalkQuerySpeed,
		float InWalkIntentThreshold,
		float InStabilizedWalkSpeedCmPerSecond,
		float InIdlePredictedSpeedCutoffCmPerSecond,
		TConstArrayView<float> InRawQueryTrajectorySampleTimesSeconds,
		TConstArrayView<FTransform> InRawQueryTrajectoryWorldTransformsCm,
		TConstArrayView<float> InQueryTrajectorySampleTimesSeconds,
		TConstArrayView<FTransform> InQueryTrajectoryWorldTransformsCm,
		TConstArrayView<FPhysAnimBodySample> InLiveBodySamplesProtoWorldMeters,
		TConstArrayView<FPhysAnimBodySample> InPhysicalBodySamplesProtoWorldMeters,
		TConstArrayView<FPhysAnimBodySample> InCanonicalBodySamplesProtoMeters,
		TConstArrayView<FPhysAnimFuturePoseSample> InRawCanonicalFuturePoseSamples,
		TConstArrayView<FPhysAnimFuturePoseSample> InPlacedCanonicalFuturePoseSamples,
		TConstArrayView<float> InSelfObservation,
		TConstArrayView<float> InMimicTarget,
		TConstArrayView<float> InTerrain,
		TConstArrayView<float> InConditionedActions)
	{
		if (!bCondition || bCaptured)
		{
			return false;
		}

		Reset();
		CaptureScope = TEXT("first_locomotion_policy_step_post_conditioning");
		RuntimeState = InRuntimeState;
		WorldTimeSeconds = InWorldTimeSeconds;
		PolicyControlTick = InPolicyControlTick;
		PoseSearchAnimation = InPoseSearchAnimation;
		PoseSearchSelectedTime = InPoseSearchSelectedTime;
		bPoseSearchMirrored = bInPoseSearchMirrored;
		OwnerActorWorldTransform = InOwnerActorWorldTransform;
		MeshWorldTransform = InMeshWorldTransform;
		SelectedAnimationWorldRootProtoMeters = InSelectedAnimationWorldRootProtoMeters;
		SelectedAnimationDataRootProtoMeters = InSelectedAnimationDataRootProtoMeters;
		IntentMagnitude = InIntentMagnitude;
		DesiredVelocityCmPerSecond = InDesiredVelocityCmPerSecond;
		AcceptedVelocityCmPerSecond = InAcceptedVelocityCmPerSecond;
		ResolvedQueryVelocityCmPerSecond = InResolvedQueryVelocityCmPerSecond;
		bUseStabilizedWalkQuerySpeed = bInUseStabilizedWalkQuerySpeed;
		WalkIntentThreshold = InWalkIntentThreshold;
		StabilizedWalkSpeedCmPerSecond = InStabilizedWalkSpeedCmPerSecond;
		IdlePredictedSpeedCutoffCmPerSecond = InIdlePredictedSpeedCutoffCmPerSecond;
		RawQueryTrajectorySampleTimesSeconds.Append(
			InRawQueryTrajectorySampleTimesSeconds.GetData(),
			InRawQueryTrajectorySampleTimesSeconds.Num());
		RawQueryTrajectoryWorldTransformsCm.Append(
			InRawQueryTrajectoryWorldTransformsCm.GetData(),
			InRawQueryTrajectoryWorldTransformsCm.Num());
		QueryTrajectorySampleTimesSeconds.Append(
			InQueryTrajectorySampleTimesSeconds.GetData(),
			InQueryTrajectorySampleTimesSeconds.Num());
		QueryTrajectoryWorldTransformsCm.Append(
			InQueryTrajectoryWorldTransformsCm.GetData(),
			InQueryTrajectoryWorldTransformsCm.Num());
		LiveBodySamplesProtoWorldMeters.Append(
			InLiveBodySamplesProtoWorldMeters.GetData(),
			InLiveBodySamplesProtoWorldMeters.Num());
		PhysicalBodySamplesProtoWorldMeters.Append(
			InPhysicalBodySamplesProtoWorldMeters.GetData(),
			InPhysicalBodySamplesProtoWorldMeters.Num());
		CanonicalBodySamplesProtoMeters.Append(
			InCanonicalBodySamplesProtoMeters.GetData(),
			InCanonicalBodySamplesProtoMeters.Num());
		RawCanonicalFuturePoseSamples.Append(
			InRawCanonicalFuturePoseSamples.GetData(),
			InRawCanonicalFuturePoseSamples.Num());
		PlacedCanonicalFuturePoseSamples.Append(
			InPlacedCanonicalFuturePoseSamples.GetData(),
			InPlacedCanonicalFuturePoseSamples.Num());
		SelfObservation.Append(InSelfObservation.GetData(), InSelfObservation.Num());
		MimicTarget.Append(InMimicTarget.GetData(), InMimicTarget.Num());
		Terrain.Append(InTerrain.GetData(), InTerrain.Num());
		ConditionedActions.Append(InConditionedActions.GetData(), InConditionedActions.Num());
		ConditionedActionSignatureAlgorithm = LocomotionFrameReplayActionSignatureAlgorithm;
		ConditionedActionCrc32 = InConditionedActions.IsEmpty()
			? 0u
			: FCrc::MemCrc32(
				InConditionedActions.GetData(),
				InConditionedActions.Num() * static_cast<int32>(sizeof(float)));
		bCaptured = true;
		return true;
	}

	void FPhysAnimLocomotionFrameReplaySnapshot::Reset()
	{
		bCaptured = false;
		CaptureScope.Reset();
		RuntimeState.Reset();
		WorldTimeSeconds = 0.0;
		PolicyControlTick = 0;
		PoseSearchAnimation.Reset();
		PoseSearchSelectedTime = 0.0f;
		bPoseSearchMirrored = false;
		OwnerActorWorldTransform = FTransform::Identity;
		MeshWorldTransform = FTransform::Identity;
		SelectedAnimationWorldRootProtoMeters = FTransform::Identity;
		SelectedAnimationDataRootProtoMeters = FTransform::Identity;
		IntentMagnitude = 0.0f;
		DesiredVelocityCmPerSecond = FVector::ZeroVector;
		AcceptedVelocityCmPerSecond = FVector::ZeroVector;
		ResolvedQueryVelocityCmPerSecond = FVector::ZeroVector;
		bUseStabilizedWalkQuerySpeed = false;
		WalkIntentThreshold = 0.0f;
		StabilizedWalkSpeedCmPerSecond = 0.0f;
		IdlePredictedSpeedCutoffCmPerSecond = 0.0f;
		RawQueryTrajectorySampleTimesSeconds.Reset();
		RawQueryTrajectoryWorldTransformsCm.Reset();
		QueryTrajectorySampleTimesSeconds.Reset();
		QueryTrajectoryWorldTransformsCm.Reset();
		LiveBodySamplesProtoWorldMeters.Reset();
		PhysicalBodySamplesProtoWorldMeters.Reset();
		CanonicalBodySamplesProtoMeters.Reset();
		RawCanonicalFuturePoseSamples.Reset();
		PlacedCanonicalFuturePoseSamples.Reset();
		SelfObservation.Reset();
		MimicTarget.Reset();
		Terrain.Reset();
		ConditionedActions.Reset();
		ConditionedActionSignatureAlgorithm.Reset();
		ConditionedActionCrc32 = 0u;
	}

	bool ShouldCaptureLocomotionFrameReplay(
		const bool bTraceEnabled,
		const bool bPolicyInferenceEnabled,
		const bool bVariantUsesPolicyInference,
		const bool bLocomotionActive,
		const bool bAlreadyCaptured)
	{
		return bTraceEnabled &&
			bPolicyInferenceEnabled &&
			bVariantUsesPolicyInference &&
			bLocomotionActive &&
			!bAlreadyCaptured;
	}

	bool ValidateLocomotionFrameReplaySnapshot(
		const FPhysAnimLocomotionFrameReplaySnapshot& Snapshot,
		FString& OutError)
	{
		auto IsFiniteVector = [](const FVector& Value)
		{
			return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
		};
		auto IsFiniteNormalizedQuat = [](const FQuat& Value)
		{
			return FMath::IsFinite(Value.X) &&
				FMath::IsFinite(Value.Y) &&
				FMath::IsFinite(Value.Z) &&
				FMath::IsFinite(Value.W) &&
				Value.IsNormalized();
		};
		auto IsFiniteTransform = [&](const FTransform& Value)
		{
			return IsFiniteVector(Value.GetLocation()) &&
				IsFiniteNormalizedQuat(Value.GetRotation()) &&
				IsFiniteVector(Value.GetScale3D());
		};
		auto IsFiniteBodySample = [&](const FPhysAnimBodySample& Sample)
		{
			return IsFiniteVector(Sample.Position) &&
				IsFiniteNormalizedQuat(Sample.Rotation) &&
				IsFiniteVector(Sample.LinearVelocity) &&
				IsFiniteVector(Sample.AngularVelocity);
		};
		auto ValidateBodySamples = [&](TConstArrayView<FPhysAnimBodySample> Samples, const TCHAR* Label)
		{
			if (Samples.Num() != NumSmplBodies)
			{
				OutError = FString::Printf(TEXT("%s has %d bodies instead of %d."), Label, Samples.Num(), NumSmplBodies);
				return false;
			}
			for (int32 BodyIndex = 0; BodyIndex < Samples.Num(); ++BodyIndex)
			{
				if (!IsFiniteBodySample(Samples[BodyIndex]))
				{
					OutError = FString::Printf(TEXT("%s body %d is invalid."), Label, BodyIndex);
					return false;
				}
			}
			return true;
		};
		auto ValidateFutureSamples = [&](TConstArrayView<FPhysAnimFuturePoseSample> Samples, const TCHAR* Label)
		{
			if (Samples.Num() != NumFutureSteps)
			{
				OutError = FString::Printf(TEXT("%s has %d future samples instead of %d."), Label, Samples.Num(), NumFutureSteps);
				return false;
			}
			float PreviousTimeSeconds = 0.0f;
			for (int32 FutureIndex = 0; FutureIndex < Samples.Num(); ++FutureIndex)
			{
				const FPhysAnimFuturePoseSample& Sample = Samples[FutureIndex];
				if (!FMath::IsFinite(Sample.FutureTimeSeconds) ||
					Sample.FutureTimeSeconds <= PreviousTimeSeconds ||
					Sample.BodyTransforms.Num() != NumSmplBodies)
				{
					OutError = FString::Printf(TEXT("%s future sample %d has an invalid time or body count."), Label, FutureIndex);
					return false;
				}
				for (int32 BodyIndex = 0; BodyIndex < Sample.BodyTransforms.Num(); ++BodyIndex)
				{
					if (!IsFiniteTransform(Sample.BodyTransforms[BodyIndex]))
					{
						OutError = FString::Printf(TEXT("%s future sample %d body %d is invalid."), Label, FutureIndex, BodyIndex);
						return false;
					}
				}
				PreviousTimeSeconds = Sample.FutureTimeSeconds;
			}
			return true;
		};
		auto ValidateFloatVector = [&](TConstArrayView<float> Values, int32 ExpectedCount, const TCHAR* Label)
		{
			if (Values.Num() != ExpectedCount)
			{
				OutError = FString::Printf(TEXT("%s has %d values instead of %d."), Label, Values.Num(), ExpectedCount);
				return false;
			}
			for (const float Value : Values)
			{
				if (!FMath::IsFinite(Value))
				{
					OutError = FString::Printf(TEXT("%s contains a non-finite value."), Label);
					return false;
				}
			}
			return true;
		};

		OutError.Reset();
		if (!Snapshot.bCaptured || Snapshot.CaptureScope.IsEmpty())
		{
			OutError = TEXT("Locomotion frame replay was not captured.");
			return false;
		}
		if (Snapshot.RuntimeState != TEXT("LocomotionActiveShell") ||
			Snapshot.PoseSearchAnimation.IsEmpty() ||
			!FMath::IsFinite(Snapshot.WorldTimeSeconds) ||
			!FMath::IsFinite(Snapshot.PoseSearchSelectedTime) ||
			Snapshot.PolicyControlTick < 1)
		{
			OutError = TEXT("Locomotion frame replay identity or scalar fields are invalid.");
			return false;
		}
		if (!IsFiniteTransform(Snapshot.OwnerActorWorldTransform) ||
			!IsFiniteTransform(Snapshot.MeshWorldTransform) ||
			!IsFiniteTransform(Snapshot.SelectedAnimationWorldRootProtoMeters) ||
			!IsFiniteTransform(Snapshot.SelectedAnimationDataRootProtoMeters))
		{
			OutError = TEXT("Locomotion frame replay contains an invalid frame transform.");
			return false;
		}
		if (!FMath::IsFinite(Snapshot.IntentMagnitude) ||
			Snapshot.IntentMagnitude < 0.0f ||
			Snapshot.IntentMagnitude > 1.0f ||
			!IsFiniteVector(Snapshot.DesiredVelocityCmPerSecond) ||
			!IsFiniteVector(Snapshot.AcceptedVelocityCmPerSecond) ||
			!IsFiniteVector(Snapshot.ResolvedQueryVelocityCmPerSecond) ||
			!FMath::IsFinite(Snapshot.WalkIntentThreshold) ||
			!FMath::IsFinite(Snapshot.StabilizedWalkSpeedCmPerSecond) ||
			!FMath::IsFinite(Snapshot.IdlePredictedSpeedCutoffCmPerSecond))
		{
			OutError = TEXT("Locomotion frame replay query-state fields are invalid.");
			return false;
		}
		if (Snapshot.RawQueryTrajectorySampleTimesSeconds.IsEmpty() ||
			Snapshot.RawQueryTrajectorySampleTimesSeconds.Num() != Snapshot.RawQueryTrajectoryWorldTransformsCm.Num())
		{
			OutError = TEXT("Raw query trajectory replay is empty or mismatched.");
			return false;
		}
		float PreviousRawQueryTimeSeconds = -TNumericLimits<float>::Max();
		for (int32 SampleIndex = 0; SampleIndex < Snapshot.RawQueryTrajectoryWorldTransformsCm.Num(); ++SampleIndex)
		{
			const float QueryTimeSeconds = Snapshot.RawQueryTrajectorySampleTimesSeconds[SampleIndex];
			if (!FMath::IsFinite(QueryTimeSeconds) ||
				QueryTimeSeconds + UE_SMALL_NUMBER < PreviousRawQueryTimeSeconds ||
				!IsFiniteTransform(Snapshot.RawQueryTrajectoryWorldTransformsCm[SampleIndex]))
			{
				OutError = FString::Printf(TEXT("Raw query trajectory sample %d has an invalid time or transform."), SampleIndex);
				return false;
			}
			PreviousRawQueryTimeSeconds = QueryTimeSeconds;
		}
		if (Snapshot.QueryTrajectorySampleTimesSeconds.Num() != NumFutureSteps + 1 ||
			Snapshot.QueryTrajectoryWorldTransformsCm.Num() != NumFutureSteps + 1)
		{
			OutError = FString::Printf(
				TEXT("Query trajectory replay has %d times and %d transforms instead of %d each."),
				Snapshot.QueryTrajectorySampleTimesSeconds.Num(),
				Snapshot.QueryTrajectoryWorldTransformsCm.Num(),
				NumFutureSteps + 1);
			return false;
		}
		if (Snapshot.QueryTrajectorySampleTimesSeconds[0] != 0.0f)
		{
			OutError = TEXT("Query trajectory replay current sample time is not exact zero.");
			return false;
		}
		float PreviousQueryTimeSeconds = -UE_SMALL_NUMBER;
		for (int32 SampleIndex = 0; SampleIndex < Snapshot.QueryTrajectoryWorldTransformsCm.Num(); ++SampleIndex)
		{
			const float QueryTimeSeconds = Snapshot.QueryTrajectorySampleTimesSeconds[SampleIndex];
			if (!FMath::IsFinite(QueryTimeSeconds) ||
				QueryTimeSeconds + UE_SMALL_NUMBER < PreviousQueryTimeSeconds ||
				!IsFiniteTransform(Snapshot.QueryTrajectoryWorldTransformsCm[SampleIndex]))
			{
				OutError = FString::Printf(TEXT("Query trajectory sample %d has an invalid time or transform."), SampleIndex);
				return false;
			}
			PreviousQueryTimeSeconds = QueryTimeSeconds;
		}
		if (!ValidateBodySamples(Snapshot.LiveBodySamplesProtoWorldMeters, TEXT("Live body replay")) ||
			!ValidateBodySamples(Snapshot.PhysicalBodySamplesProtoWorldMeters, TEXT("Physical body replay")) ||
			!ValidateBodySamples(Snapshot.CanonicalBodySamplesProtoMeters, TEXT("Canonical body replay")) ||
			!ValidateFutureSamples(Snapshot.RawCanonicalFuturePoseSamples, TEXT("Raw canonical future replay")) ||
			!ValidateFutureSamples(Snapshot.PlacedCanonicalFuturePoseSamples, TEXT("Placed canonical future replay")) ||
			!ValidateFloatVector(Snapshot.SelfObservation, SelfObsSize, TEXT("Self observation replay")) ||
			!ValidateFloatVector(Snapshot.MimicTarget, MimicTargetPosesSize, TEXT("Mimic target replay")) ||
			!ValidateFloatVector(Snapshot.Terrain, TerrainSize, TEXT("Terrain replay")) ||
			!ValidateFloatVector(Snapshot.ConditionedActions, NumActionFloats, TEXT("Conditioned action replay")))
		{
			return false;
		}
		for (int32 FutureIndex = 0; FutureIndex < NumFutureSteps; ++FutureIndex)
		{
			const float RawFutureTimeSeconds =
				Snapshot.RawCanonicalFuturePoseSamples[FutureIndex].FutureTimeSeconds;
			if (!FMath::IsNearlyEqual(
				RawFutureTimeSeconds,
				Snapshot.PlacedCanonicalFuturePoseSamples[FutureIndex].FutureTimeSeconds,
				UE_SMALL_NUMBER) ||
				!FMath::IsNearlyEqual(
					RawFutureTimeSeconds,
					Snapshot.QueryTrajectorySampleTimesSeconds[FutureIndex + 1],
					UE_SMALL_NUMBER))
			{
				OutError = FString::Printf(
					TEXT("Raw, placed, and query future replay sample %d use different times."),
					FutureIndex);
				return false;
			}
		}
		if (Snapshot.ConditionedActionSignatureAlgorithm != LocomotionFrameReplayActionSignatureAlgorithm)
		{
			OutError = TEXT("Conditioned action signature algorithm is invalid.");
			return false;
		}
		const uint32 ExpectedCrc32 = Snapshot.ConditionedActions.IsEmpty()
			? 0u
			: FCrc::MemCrc32(
				Snapshot.ConditionedActions.GetData(),
				Snapshot.ConditionedActions.Num() * static_cast<int32>(sizeof(float)));
		if (Snapshot.ConditionedActionCrc32 != ExpectedCrc32)
		{
			OutError = TEXT("Conditioned action signature does not match the captured action vector.");
			return false;
		}
		return true;
	}

	int32 SelectNearOptimalLocomotionTransitionCandidate(
		TConstArrayView<FPhysAnimLocomotionTransitionCandidateScore> Candidates,
		float MaxRelativePoseCostIncrease,
		float MinimumDiscontinuityImprovement)
	{
		if (Candidates.IsEmpty() ||
			!FMath::IsFinite(MaxRelativePoseCostIncrease) ||
			MaxRelativePoseCostIncrease < 0.0f ||
			!FMath::IsFinite(MinimumDiscontinuityImprovement) ||
			MinimumDiscontinuityImprovement < 0.0f)
		{
			return INDEX_NONE;
		}

		int32 BestPoseCostIndex = INDEX_NONE;
		for (int32 CandidateIndex = 0; CandidateIndex < Candidates.Num(); ++CandidateIndex)
		{
			const FPhysAnimLocomotionTransitionCandidateScore& Candidate = Candidates[CandidateIndex];
			if (!FMath::IsFinite(Candidate.PoseCost) ||
				Candidate.PoseCost < 0.0f ||
				!FMath::IsFinite(Candidate.TransitionDiscontinuity) ||
				Candidate.TransitionDiscontinuity < 0.0f)
			{
				continue;
			}
			if (BestPoseCostIndex == INDEX_NONE ||
				Candidate.PoseCost < Candidates[BestPoseCostIndex].PoseCost)
			{
				BestPoseCostIndex = CandidateIndex;
			}
		}
		if (BestPoseCostIndex == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		const float BestPoseCost = Candidates[BestPoseCostIndex].PoseCost;
		const float PoseCostCeiling = BestPoseCost * (1.0f + MaxRelativePoseCostIncrease);
		int32 BestCompatibleIndex = BestPoseCostIndex;
		for (int32 CandidateIndex = 0; CandidateIndex < Candidates.Num(); ++CandidateIndex)
		{
			const FPhysAnimLocomotionTransitionCandidateScore& Candidate = Candidates[CandidateIndex];
			if (!FMath::IsFinite(Candidate.PoseCost) ||
				Candidate.PoseCost < 0.0f ||
				Candidate.PoseCost > PoseCostCeiling ||
				!FMath::IsFinite(Candidate.TransitionDiscontinuity) ||
				Candidate.TransitionDiscontinuity < 0.0f)
			{
				continue;
			}
			const FPhysAnimLocomotionTransitionCandidateScore& BestCompatible =
				Candidates[BestCompatibleIndex];
			if (Candidate.TransitionDiscontinuity <
					BestCompatible.TransitionDiscontinuity ||
				(FMath::IsNearlyEqual(
					Candidate.TransitionDiscontinuity,
					BestCompatible.TransitionDiscontinuity) &&
				 Candidate.PoseCost < BestCompatible.PoseCost))
			{
				BestCompatibleIndex = CandidateIndex;
			}
		}

		const float DiscontinuityImprovement =
			Candidates[BestPoseCostIndex].TransitionDiscontinuity -
			Candidates[BestCompatibleIndex].TransitionDiscontinuity;
		return DiscontinuityImprovement >= MinimumDiscontinuityImprovement
			? BestCompatibleIndex
			: BestPoseCostIndex;
	}

	bool BlendMimicTargetPosesForTransition(
		TConstArrayView<float> SourceMimicTarget,
		TConstArrayView<float> TargetMimicTarget,
		float Alpha,
		TArray<float>& OutBlendedMimicTarget,
		FString& OutError)
	{
		OutBlendedMimicTarget.Reset();
		OutError.Reset();
		if (SourceMimicTarget.Num() != MimicTargetPosesSize ||
			TargetMimicTarget.Num() != MimicTargetPosesSize)
		{
			OutError = FString::Printf(
				TEXT("Mimic transition blend expected %d source/target floats but found %d/%d."),
				MimicTargetPosesSize,
				SourceMimicTarget.Num(),
				TargetMimicTarget.Num());
			return false;
		}
		if (!FMath::IsFinite(Alpha))
		{
			OutError = TEXT("Mimic transition blend alpha is non-finite.");
			return false;
		}
		for (int32 ValueIndex = 0; ValueIndex < MimicTargetPosesSize; ++ValueIndex)
		{
			if (!FMath::IsFinite(SourceMimicTarget[ValueIndex]) ||
				!FMath::IsFinite(TargetMimicTarget[ValueIndex]))
			{
				OutError = FString::Printf(
					TEXT("Mimic transition blend value %d is non-finite."),
					ValueIndex);
				return false;
			}
		}

		constexpr int32 FloatsPerFutureStep = MimicTargetPosesSize / NumFutureSteps;
		constexpr int32 PositionFloatsPerFutureStep = NumSmplBodies * 3 * 2;
		constexpr int32 RotationCountPerFutureStep = NumSmplBodies * 2;
		constexpr int32 RotationFloats = 6;
		static_assert(
			FloatsPerFutureStep ==
				PositionFloatsPerFutureStep + RotationCountPerFutureStep * RotationFloats + 1,
			"Mimic transition layout must match BuildMimicTargetPoses.");

		const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
		OutBlendedMimicTarget.Append(
			TargetMimicTarget.GetData(),
			TargetMimicTarget.Num());
		for (int32 FutureIndex = 0; FutureIndex < NumFutureSteps; ++FutureIndex)
		{
			const int32 StepOffset = FutureIndex * FloatsPerFutureStep;
			for (int32 PositionIndex = 0;
				PositionIndex < PositionFloatsPerFutureStep;
				++PositionIndex)
			{
				const int32 ValueIndex = StepOffset + PositionIndex;
				OutBlendedMimicTarget[ValueIndex] = FMath::Lerp(
					SourceMimicTarget[ValueIndex],
					TargetMimicTarget[ValueIndex],
					ClampedAlpha);
			}

			for (int32 RotationIndex = 0;
				RotationIndex < RotationCountPerFutureStep;
				++RotationIndex)
			{
				const int32 RotationOffset =
					StepOffset + PositionFloatsPerFutureStep + RotationIndex * RotationFloats;
				const FVector SourceTangent(
					SourceMimicTarget[RotationOffset + 0],
					SourceMimicTarget[RotationOffset + 1],
					SourceMimicTarget[RotationOffset + 2]);
				const FVector SourceNormal(
					SourceMimicTarget[RotationOffset + 3],
					SourceMimicTarget[RotationOffset + 4],
					SourceMimicTarget[RotationOffset + 5]);
				const FVector TargetTangent(
					TargetMimicTarget[RotationOffset + 0],
					TargetMimicTarget[RotationOffset + 1],
					TargetMimicTarget[RotationOffset + 2]);
				const FVector TargetNormal(
					TargetMimicTarget[RotationOffset + 3],
					TargetMimicTarget[RotationOffset + 4],
					TargetMimicTarget[RotationOffset + 5]);

				auto BuildRotation =
					[&](const FVector& Tangent, const FVector& Normal, FQuat& OutRotation) -> bool
					{
						const FVector XAxis = Tangent.GetSafeNormal();
						const FVector ZAxis =
							(Normal - XAxis * FVector::DotProduct(Normal, XAxis)).GetSafeNormal();
						if (XAxis.IsNearlyZero() || ZAxis.IsNearlyZero())
						{
							return false;
						}
						OutRotation = MakeQuaternionFromBasis(XAxis, ZAxis);
						return true;
					};

				FQuat SourceRotation = FQuat::Identity;
				FQuat TargetRotation = FQuat::Identity;
				if (!BuildRotation(SourceTangent, SourceNormal, SourceRotation) ||
					!BuildRotation(TargetTangent, TargetNormal, TargetRotation))
				{
					OutBlendedMimicTarget.Reset();
					OutError = FString::Printf(
						TEXT("Mimic transition rotation %d at future step %d is degenerate."),
						RotationIndex,
						FutureIndex);
					return false;
				}
				const FQuat BlendedRotation =
					FQuat::Slerp(SourceRotation, TargetRotation, ClampedAlpha).GetNormalized();
				float BlendedTanNorm[RotationFloats];
				QuaternionToTanNorm(BlendedRotation, BlendedTanNorm);
				for (int32 ComponentIndex = 0; ComponentIndex < RotationFloats; ++ComponentIndex)
				{
					OutBlendedMimicTarget[RotationOffset + ComponentIndex] =
						BlendedTanNorm[ComponentIndex];
				}
			}
			// The target schedule is authoritative; future-time channels are never blended.
			OutBlendedMimicTarget[StepOffset + FloatsPerFutureStep - 1] =
				TargetMimicTarget[StepOffset + FloatsPerFutureStep - 1];
		}
		return true;
	}

	bool CalculatePoseSearchChannelCosts(
		TConstArrayView<float> PoseValues,
		TConstArrayView<float> QueryValues,
		TConstArrayView<float> WeightsSqrt,
		TConstArrayView<FPhysAnimPoseSearchChannelSlice> ChannelSlices,
		TArray<FPhysAnimPoseSearchChannelCost>& OutChannelCosts,
		float& OutTotalCost,
		FString& OutError)
	{
		OutChannelCosts.Reset();
		OutTotalCost = 0.0f;
		OutError.Reset();

		if (PoseValues.IsEmpty() ||
			PoseValues.Num() != QueryValues.Num() ||
			PoseValues.Num() != WeightsSqrt.Num())
		{
			OutError = TEXT("Pose Search channel cost vectors are empty or mismatched.");
			return false;
		}
		if (ChannelSlices.IsEmpty())
		{
			OutError = TEXT("Pose Search channel cost slices are empty.");
			return false;
		}

		TArray<uint8> ClaimedDimensions;
		ClaimedDimensions.Init(0u, PoseValues.Num());
		OutChannelCosts.Reserve(ChannelSlices.Num());
		for (const FPhysAnimPoseSearchChannelSlice& Slice : ChannelSlices)
		{
			if (Slice.Label.IsEmpty() ||
				Slice.DataOffset < 0 ||
				Slice.Cardinality <= 0 ||
				Slice.DataOffset + Slice.Cardinality > PoseValues.Num())
			{
				OutError = FString::Printf(
					TEXT("Pose Search channel slice '%s' has an invalid range [%d, %d)."),
					*Slice.Label,
					Slice.DataOffset,
					Slice.DataOffset + Slice.Cardinality);
				return false;
			}

			FPhysAnimPoseSearchChannelCost& ChannelCost = OutChannelCosts.AddDefaulted_GetRef();
			ChannelCost.Label = Slice.Label;
			ChannelCost.DataOffset = Slice.DataOffset;
			ChannelCost.Cardinality = Slice.Cardinality;
			for (int32 DimensionIndex = Slice.DataOffset;
				DimensionIndex < Slice.DataOffset + Slice.Cardinality;
				++DimensionIndex)
			{
				if (ClaimedDimensions[DimensionIndex] != 0u)
				{
					OutError = FString::Printf(
						TEXT("Pose Search channel slices overlap at dimension %d."),
						DimensionIndex);
					return false;
				}
				const float PoseValue = PoseValues[DimensionIndex];
				const float QueryValue = QueryValues[DimensionIndex];
				const float WeightSqrt = WeightsSqrt[DimensionIndex];
				if (!FMath::IsFinite(PoseValue) ||
					!FMath::IsFinite(QueryValue) ||
					!FMath::IsFinite(WeightSqrt))
				{
					OutError = FString::Printf(
						TEXT("Pose Search channel cost dimension %d is non-finite."),
						DimensionIndex);
					return false;
				}
				const float WeightedDifference = (PoseValue - QueryValue) * WeightSqrt;
				const float DimensionCost = WeightedDifference * WeightedDifference;
				ChannelCost.Cost += DimensionCost;
				OutTotalCost += DimensionCost;
				ClaimedDimensions[DimensionIndex] = 1u;
			}
		}

		for (int32 DimensionIndex = 0; DimensionIndex < ClaimedDimensions.Num(); ++DimensionIndex)
		{
			if (ClaimedDimensions[DimensionIndex] == 0u)
			{
				OutError = FString::Printf(
					TEXT("Pose Search channel slices do not cover dimension %d."),
					DimensionIndex);
				return false;
			}
		}
		return true;
	}

	bool ValidatePolicyInputProvenanceSnapshot(
		const FPhysAnimPolicyInputProvenanceSnapshot& Snapshot,
		FString& OutError)
	{
		auto IsFiniteVector = [](const FVector& Value)
		{
			return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
		};
		auto IsFiniteNormalizedQuat = [](const FQuat& Value)
		{
			return FMath::IsFinite(Value.X) &&
				FMath::IsFinite(Value.Y) &&
				FMath::IsFinite(Value.Z) &&
				FMath::IsFinite(Value.W) &&
				Value.IsNormalized();
		};
		auto IsFiniteTransform = [&](const FTransform& Value)
		{
			return IsFiniteVector(Value.GetLocation()) &&
				IsFiniteNormalizedQuat(Value.GetRotation()) &&
				IsFiniteVector(Value.GetScale3D());
		};
		auto IsFiniteBodySample = [&](const FPhysAnimBodySample& Sample)
		{
			return IsFiniteVector(Sample.Position) &&
				IsFiniteNormalizedQuat(Sample.Rotation) &&
				IsFiniteVector(Sample.LinearVelocity) &&
				IsFiniteVector(Sample.AngularVelocity);
		};
		auto ValidateBodySamples = [&](TConstArrayView<FPhysAnimBodySample> Samples, const TCHAR* Label)
		{
			if (Samples.Num() != NumSmplBodies)
			{
				OutError = FString::Printf(
					TEXT("%s has %d bodies instead of %d."),
					Label,
					Samples.Num(),
					NumSmplBodies);
				return false;
			}
			for (int32 BodyIndex = 0; BodyIndex < Samples.Num(); ++BodyIndex)
			{
				if (!IsFiniteBodySample(Samples[BodyIndex]))
				{
					OutError = FString::Printf(TEXT("%s body %d is non-finite or has a non-normalized rotation."), Label, BodyIndex);
					return false;
				}
			}
			return true;
		};
		auto ValidateFutureSamples = [&](TConstArrayView<FPhysAnimFuturePoseSample> Samples, const TCHAR* Label)
		{
			if (Samples.Num() != NumFutureSteps)
			{
				OutError = FString::Printf(
					TEXT("%s has %d future samples instead of %d."),
					Label,
					Samples.Num(),
					NumFutureSteps);
				return false;
			}
			for (int32 FutureIndex = 0; FutureIndex < Samples.Num(); ++FutureIndex)
			{
				const FPhysAnimFuturePoseSample& Sample = Samples[FutureIndex];
				if (!FMath::IsFinite(Sample.FutureTimeSeconds) || Sample.BodyTransforms.Num() != NumSmplBodies)
				{
					OutError = FString::Printf(TEXT("%s future sample %d has an invalid time or body count."), Label, FutureIndex);
					return false;
				}
				for (int32 BodyIndex = 0; BodyIndex < Sample.BodyTransforms.Num(); ++BodyIndex)
				{
					if (!IsFiniteTransform(Sample.BodyTransforms[BodyIndex]))
					{
						OutError = FString::Printf(TEXT("%s future sample %d body %d is invalid."), Label, FutureIndex, BodyIndex);
						return false;
					}
				}
			}
			return true;
		};

		OutError.Reset();
		if (!Snapshot.bCaptured || Snapshot.CaptureScope.IsEmpty())
		{
			OutError = TEXT("Policy-input provenance was not captured.");
			return false;
		}
		if (Snapshot.RuntimeState.IsEmpty() ||
			Snapshot.PoseSearchAnimation.IsEmpty() ||
			!FMath::IsFinite(Snapshot.WorldTimeSeconds) ||
			!FMath::IsFinite(Snapshot.PoseSearchSelectedTime) ||
			!FMath::IsFinite(Snapshot.SelfObservationGroundHeight) ||
			Snapshot.PolicyControlTick < 1)
		{
			OutError = TEXT("Policy-input provenance identity or scalar fields are invalid.");
			return false;
		}
		if (!IsFiniteTransform(Snapshot.OwnerActorWorldTransform) ||
			!IsFiniteTransform(Snapshot.MeshWorldTransform) ||
			!IsFiniteTransform(Snapshot.RootBoneWorldTransform) ||
			!IsFiniteTransform(Snapshot.MimicTargetReferenceWorldRoot) ||
			!IsFiniteTransform(Snapshot.MimicTargetReferenceDataRoot))
		{
			OutError = TEXT("Policy-input provenance contains an invalid transform.");
			return false;
		}
		if (!ValidateBodySamples(Snapshot.MannyBodySamples, TEXT("Manny body source")) ||
			!ValidateBodySamples(Snapshot.CanonicalBodySamples, TEXT("Canonical body source")) ||
			!ValidateBodySamples(Snapshot.MimicReferenceBodySamples, TEXT("Mimic reference body source")) ||
			!ValidateFutureSamples(Snapshot.MannyFuturePoseSamples, TEXT("Manny future source")) ||
			!ValidateFutureSamples(Snapshot.CanonicalFuturePoseSamples, TEXT("Canonical future source")))
		{
			return false;
		}
		if (Snapshot.TerrainGroundHeights.Num() != TerrainSize)
		{
			OutError = FString::Printf(
				TEXT("Terrain provenance has %d samples instead of %d."),
				Snapshot.TerrainGroundHeights.Num(),
				TerrainSize);
			return false;
		}
		for (const float GroundHeight : Snapshot.TerrainGroundHeights)
		{
			if (!FMath::IsFinite(GroundHeight))
			{
				OutError = TEXT("Terrain provenance contains a non-finite ground height.");
				return false;
			}
		}
		if (Snapshot.PreviousActions.Num() != NumActionFloats)
		{
			OutError = FString::Printf(
				TEXT("Previous-action provenance has %d floats instead of %d."),
				Snapshot.PreviousActions.Num(),
				NumActionFloats);
			return false;
		}
		for (const float Action : Snapshot.PreviousActions)
		{
			if (!FMath::IsFinite(Action))
			{
				OutError = TEXT("Previous-action provenance contains a non-finite value.");
				return false;
			}
		}
		return true;
	}

	namespace
	{
		bool ValidateFirstPolicyBodySamples(
			TConstArrayView<FPhysAnimBodySample> BodySamples,
			const TCHAR* Label,
			FString& OutError)
		{
			if (BodySamples.Num() != NumSmplBodies)
			{
				OutError = FString::Printf(
					TEXT("%s has %d bodies instead of %d."),
					Label,
					BodySamples.Num(),
					NumSmplBodies);
				return false;
			}

			for (int32 BodyIndex = 0; BodyIndex < BodySamples.Num(); ++BodyIndex)
			{
				const FPhysAnimBodySample& Sample = BodySamples[BodyIndex];
				const bool bFinite =
					FMath::IsFinite(Sample.Position.X) &&
					FMath::IsFinite(Sample.Position.Y) &&
					FMath::IsFinite(Sample.Position.Z) &&
					FMath::IsFinite(Sample.Rotation.X) &&
					FMath::IsFinite(Sample.Rotation.Y) &&
					FMath::IsFinite(Sample.Rotation.Z) &&
					FMath::IsFinite(Sample.Rotation.W) &&
					FMath::IsFinite(Sample.LinearVelocity.X) &&
					FMath::IsFinite(Sample.LinearVelocity.Y) &&
					FMath::IsFinite(Sample.LinearVelocity.Z) &&
					FMath::IsFinite(Sample.AngularVelocity.X) &&
					FMath::IsFinite(Sample.AngularVelocity.Y) &&
					FMath::IsFinite(Sample.AngularVelocity.Z);
				if (!bFinite || !Sample.Rotation.IsNormalized())
				{
					OutError = FString::Printf(
						TEXT("%s body %d is non-finite or has a non-normalized rotation."),
						Label,
						BodyIndex);
					return false;
				}
			}

			return true;
		}

		bool FirstPolicyBodySamplesEqual(
			TConstArrayView<FPhysAnimBodySample> A,
			TConstArrayView<FPhysAnimBodySample> B)
		{
			if (A.Num() != B.Num())
			{
				return false;
			}

			for (int32 BodyIndex = 0; BodyIndex < A.Num(); ++BodyIndex)
			{
				const FPhysAnimBodySample& Left = A[BodyIndex];
				const FPhysAnimBodySample& Right = B[BodyIndex];
				if (Left.Position != Right.Position ||
					Left.Rotation != Right.Rotation ||
					Left.LinearVelocity != Right.LinearVelocity ||
					Left.AngularVelocity != Right.AngularVelocity)
				{
					return false;
				}
			}

			return true;
		}

		bool BuildFirstPolicyBodySourceRecord(
			const FString& Stage,
			const double WorldTimeSeconds,
			const FString& RuntimeState,
			const int32 PolicyControlTick,
			TConstArrayView<FPhysAnimBodySample> BodySamples,
			FPhysAnimFirstPolicyBodySourceRecord& OutRecord,
			FString& OutError)
		{
			if (Stage.IsEmpty() ||
				RuntimeState.IsEmpty() ||
				!FMath::IsFinite(WorldTimeSeconds) ||
				WorldTimeSeconds < 0.0 ||
				PolicyControlTick < 0)
			{
				OutError = TEXT("First-policy body-source metadata is invalid.");
				return false;
			}
			if (!ValidateFirstPolicyBodySamples(BodySamples, TEXT("First-policy body source"), OutError))
			{
				return false;
			}

			FString Fingerprint;
			if (!BuildFirstPolicyBodySourceFingerprint(BodySamples, Fingerprint, OutError))
			{
				return false;
			}

			FPhysAnimFirstPolicyBodySourceRecord Record;
			Record.bRecorded = true;
			Record.Stage = Stage;
			Record.WorldTimeSeconds = WorldTimeSeconds;
			Record.RuntimeState = RuntimeState;
			Record.PolicyControlTick = PolicyControlTick;
			Record.BodySampleCount = BodySamples.Num();
			Record.FingerprintAlgorithm = FirstPolicyBodySourceFingerprintAlgorithm;
			Record.Fingerprint = MoveTemp(Fingerprint);
			Record.BodySamples.Append(BodySamples.GetData(), BodySamples.Num());
			OutRecord = MoveTemp(Record);
			return true;
		}
	}

	void FPhysAnimFirstPolicyBodySourceRecord::Reset()
	{
		bRecorded = false;
		Stage.Reset();
		WorldTimeSeconds = -1.0;
		RuntimeState.Reset();
		PolicyControlTick = INDEX_NONE;
		BodySampleCount = 0;
		FingerprintAlgorithm.Reset();
		Fingerprint.Reset();
		BodySamples.Reset();
	}

	bool BuildFirstPolicyBodySourceFingerprint(
		TConstArrayView<FPhysAnimBodySample> BodySamples,
		FString& OutFingerprint,
		FString& OutError)
	{
		OutFingerprint.Reset();
		OutError.Reset();
		if (!ValidateFirstPolicyBodySamples(BodySamples, TEXT("Fingerprint body source"), OutError))
		{
			return false;
		}

		static_assert(sizeof(double) == 8, "The body-source fingerprint requires IEEE-754 binary64 values.");
		uint64 Hash = 14695981039346656037ull;
		constexpr uint64 Prime = 1099511628211ull;
		auto AppendDoubleLittleEndian = [&Hash](const double Value)
		{
			uint64 Bits = 0;
			FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
			for (int32 ByteIndex = 0; ByteIndex < 8; ++ByteIndex)
			{
				Hash ^= (Bits >> (ByteIndex * 8)) & 0xffull;
				Hash *= Prime;
			}
		};

		for (const FPhysAnimBodySample& Sample : BodySamples)
		{
			AppendDoubleLittleEndian(static_cast<double>(Sample.Position.X));
			AppendDoubleLittleEndian(static_cast<double>(Sample.Position.Y));
			AppendDoubleLittleEndian(static_cast<double>(Sample.Position.Z));
			AppendDoubleLittleEndian(static_cast<double>(Sample.Rotation.X));
			AppendDoubleLittleEndian(static_cast<double>(Sample.Rotation.Y));
			AppendDoubleLittleEndian(static_cast<double>(Sample.Rotation.Z));
			AppendDoubleLittleEndian(static_cast<double>(Sample.Rotation.W));
			AppendDoubleLittleEndian(static_cast<double>(Sample.LinearVelocity.X));
			AppendDoubleLittleEndian(static_cast<double>(Sample.LinearVelocity.Y));
			AppendDoubleLittleEndian(static_cast<double>(Sample.LinearVelocity.Z));
			AppendDoubleLittleEndian(static_cast<double>(Sample.AngularVelocity.X));
			AppendDoubleLittleEndian(static_cast<double>(Sample.AngularVelocity.Y));
			AppendDoubleLittleEndian(static_cast<double>(Sample.AngularVelocity.Z));
		}

		OutFingerprint = FString::Printf(TEXT("%016llX"), static_cast<unsigned long long>(Hash));
		return true;
	}

	bool FPhysAnimFirstPolicyBodySourceTrace::CapturePriorIf(
		const bool bCondition,
		const FString& InStage,
		const double InWorldTimeSeconds,
		const FString& InRuntimeState,
		const int32 InPolicyControlTick,
		TConstArrayView<FPhysAnimBodySample> InBodySamples)
	{
		if (!bCondition || Prior.bRecorded || !ValidationError.IsEmpty())
		{
			return false;
		}
		if (InStage != TEXT("pre_state_machine") ||
			InRuntimeState != TEXT("WaitingForPoseSearch") ||
			InPolicyControlTick != 0)
		{
			ValidationError = TEXT("The prior body source must come from pre_state_machine in WaitingForPoseSearch before any policy tick.");
			return false;
		}

		FString Error;
		if (!BuildFirstPolicyBodySourceRecord(
				InStage,
				InWorldTimeSeconds,
				InRuntimeState,
				InPolicyControlTick,
				InBodySamples,
				Prior,
				Error))
		{
			ValidationError = MoveTemp(Error);
			return false;
		}
		return true;
	}

	bool FPhysAnimFirstPolicyBodySourceTrace::RecordFirstPolicySourceIf(
		const bool bCondition,
		const FString& InStage,
		const double InWorldTimeSeconds,
		const FString& InRuntimeState,
		const int32 InPolicyControlTick,
		TConstArrayView<FPhysAnimBodySample> InLiveBodySamples,
		FString& OutError)
	{
		OutError.Reset();
		if (!bCondition || bFirstInferenceRecorded)
		{
			return true;
		}
		if (!ValidationError.IsEmpty())
		{
			OutError = ValidationError;
			return false;
		}
		if (!Prior.bRecorded)
		{
			ValidationError = TEXT("The first-policy source cannot be selected before the prior source is captured.");
			OutError = ValidationError;
			return false;
		}
		if (InStage != TEXT("first_policy_pre_adapter") ||
			InPolicyControlTick != 1 ||
			!FMath::IsFinite(InWorldTimeSeconds) ||
			InWorldTimeSeconds <= Prior.WorldTimeSeconds)
		{
			ValidationError = TEXT("The live first-policy source metadata does not identify the first post-prior policy tick.");
			OutError = ValidationError;
			return false;
		}

		FPhysAnimFirstPolicyBodySourceRecord LiveRecord;
		if (!BuildFirstPolicyBodySourceRecord(
				InStage,
				InWorldTimeSeconds,
				InRuntimeState,
				InPolicyControlTick,
				InLiveBodySamples,
				LiveRecord,
				OutError))
		{
			ValidationError = OutError;
			return false;
		}

		FPhysAnimFirstPolicyBodySourceRecord EffectiveRecord;
		if (!BuildFirstPolicyBodySourceRecord(
				InStage,
				InWorldTimeSeconds,
				InRuntimeState,
				InPolicyControlTick,
				InLiveBodySamples,
				EffectiveRecord,
				OutError))
		{
			ValidationError = OutError;
			return false;
		}

		Live = MoveTemp(LiveRecord);
		Effective = MoveTemp(EffectiveRecord);
		bFirstInferenceRecorded = true;
		return true;
	}

	void FPhysAnimFirstPolicyBodySourceTrace::Reset()
	{
		bFirstInferenceRecorded = false;
		ValidationError.Reset();
		Prior.Reset();
		Live.Reset();
		Effective.Reset();
	}

	bool ValidateFirstPolicyBodySourceTrace(
		const FPhysAnimFirstPolicyBodySourceTrace& Trace,
		FString& OutError)
	{
		OutError.Reset();
		if (!Trace.ValidationError.IsEmpty())
		{
			OutError = Trace.ValidationError;
			return false;
		}
		if (!Trace.bFirstInferenceRecorded ||
			!Trace.Prior.bRecorded ||
			!Trace.Live.bRecorded ||
			!Trace.Effective.bRecorded)
		{
			OutError = TEXT("First-policy body-source evidence is incomplete.");
			return false;
		}
		if (Trace.Prior.Stage != TEXT("pre_state_machine") ||
			Trace.Prior.RuntimeState != TEXT("WaitingForPoseSearch") ||
			Trace.Prior.PolicyControlTick != 0 ||
			Trace.Live.Stage != TEXT("first_policy_pre_adapter") ||
			Trace.Live.PolicyControlTick != 1 ||
			Trace.Effective.Stage != Trace.Live.Stage ||
			Trace.Effective.RuntimeState != Trace.Live.RuntimeState ||
			Trace.Effective.PolicyControlTick != Trace.Live.PolicyControlTick ||
			Trace.Effective.WorldTimeSeconds != Trace.Live.WorldTimeSeconds ||
			Trace.Prior.WorldTimeSeconds >= Trace.Live.WorldTimeSeconds)
		{
			OutError = TEXT("First-policy body-source stage, state, time, or tick ownership is invalid.");
			return false;
		}

		auto ValidateRecord = [&](const FPhysAnimFirstPolicyBodySourceRecord& Record, const TCHAR* Label)
		{
			if (Record.BodySampleCount != Record.BodySamples.Num() ||
				Record.FingerprintAlgorithm != FirstPolicyBodySourceFingerprintAlgorithm)
			{
				OutError = FString::Printf(TEXT("%s count or fingerprint algorithm is invalid."), Label);
				return false;
			}
			FString RecomputedFingerprint;
			FString FingerprintError;
			if (!BuildFirstPolicyBodySourceFingerprint(Record.BodySamples, RecomputedFingerprint, FingerprintError) ||
				RecomputedFingerprint != Record.Fingerprint)
			{
				OutError = FingerprintError.IsEmpty()
					? FString::Printf(TEXT("%s fingerprint does not match its body samples."), Label)
					: FingerprintError;
				return false;
			}
			return true;
		};
		if (!ValidateRecord(Trace.Prior, TEXT("Prior source")) ||
			!ValidateRecord(Trace.Live, TEXT("Live source")) ||
			!ValidateRecord(Trace.Effective, TEXT("Effective source")))
		{
			return false;
		}

		if (!FirstPolicyBodySamplesEqual(Trace.Effective.BodySamples, Trace.Live.BodySamples))
		{
			OutError = TEXT("Instrumentation-only recording did not preserve the exact live source as effective.");
			return false;
		}
		return true;
	}

	namespace
	{
		bool ValidateGroundReferenceValues(
			const FPhysAnimSelfObservationGroundReferenceValues& Values,
			FString& OutError)
		{
			const bool bFinite =
				FMath::IsFinite(Values.BodyRootProtoZM) &&
				FMath::IsFinite(Values.RootBoneWorldZCm) &&
				FMath::IsFinite(Values.StaticTraceImpactZCm) &&
				FMath::IsFinite(Values.FloorImpactZCm) &&
				FMath::IsFinite(Values.CapsuleCenterZCm) &&
				FMath::IsFinite(Values.CapsuleHalfHeightCm) &&
				FMath::IsFinite(Values.FloorDistanceCm) &&
				FMath::IsFinite(Values.FallbackGroundWorldZCm) &&
				FMath::IsFinite(Values.GroundWorldZCm) &&
				FMath::IsFinite(Values.SyntheticGroundHeightM) &&
				FMath::IsFinite(Values.FinalRootHeightM);
			if (!bFinite)
			{
				OutError = TEXT("Ground-reference evidence contains a non-finite value.");
				return false;
			}
			if (Values.bStaticTraceSucceeded && !Values.bStaticTraceAttempted)
			{
				OutError = TEXT("A successful static ground trace must have been attempted.");
				return false;
			}
			if (Values.CapsuleHalfHeightCm < 0.0)
			{
				OutError = TEXT("Ground-reference capsule half-height cannot be negative.");
				return false;
			}

			const float ExpectedGroundWorldZCm = Values.bStaticTraceSucceeded
				? static_cast<float>(Values.StaticTraceImpactZCm)
				: (Values.bHasWalkableFloor
					? (Values.bHasBlockingFloorHit
						? static_cast<float>(Values.FloorImpactZCm)
						: static_cast<float>(Values.CapsuleCenterZCm) -
							static_cast<float>(Values.CapsuleHalfHeightCm) -
							FMath::Max(static_cast<float>(Values.FloorDistanceCm), 0.0f))
					: static_cast<float>(Values.FallbackGroundWorldZCm));
			if (static_cast<float>(Values.GroundWorldZCm) != ExpectedGroundWorldZCm)
			{
				OutError = TEXT("Resolved ground-world Z does not match the recorded branch inputs.");
				return false;
			}

			const float ExpectedDesiredRootHeightM =
				(static_cast<float>(Values.RootBoneWorldZCm) - ExpectedGroundWorldZCm) * CmToMeters;
			const float ExpectedSyntheticGroundHeightM =
				static_cast<float>(Values.BodyRootProtoZM) - ExpectedDesiredRootHeightM;
			if (static_cast<float>(Values.SyntheticGroundHeightM) != ExpectedSyntheticGroundHeightM)
			{
				OutError = FString::Printf(
					TEXT("Synthetic ground height does not match the recorded float-path inputs: recorded=%.17g expected=%.17g body_root=%.17g desired_root_height=%.17g."),
					Values.SyntheticGroundHeightM,
					static_cast<double>(ExpectedSyntheticGroundHeightM),
					Values.BodyRootProtoZM,
					static_cast<double>(ExpectedDesiredRootHeightM));
				return false;
			}

			const float ExpectedFinalRootHeightM = static_cast<float>(
				Values.BodyRootProtoZM - static_cast<double>(ExpectedSyntheticGroundHeightM));
			if (static_cast<float>(Values.FinalRootHeightM) != ExpectedFinalRootHeightM)
			{
				OutError = TEXT("Final self-observation root height does not match the recorded float-path arithmetic.");
				return false;
			}
			return true;
		}

		bool BuildGroundReferenceRecord(
			const FString& Stage,
			const double WorldTimeSeconds,
			const FString& RuntimeState,
			const int32 PolicyControlTick,
			const FPhysAnimSelfObservationGroundReferenceValues& Values,
			FPhysAnimFirstPolicyGroundReferenceRecord& OutRecord,
			FString& OutError)
		{
			if (Stage.IsEmpty() ||
				RuntimeState.IsEmpty() ||
				!FMath::IsFinite(WorldTimeSeconds) ||
				WorldTimeSeconds < 0.0 ||
				PolicyControlTick < 0)
			{
				OutError = TEXT("Ground-reference record metadata is invalid.");
				return false;
			}
			if (!ValidateGroundReferenceValues(Values, OutError))
			{
				return false;
			}

			FPhysAnimFirstPolicyGroundReferenceRecord Record;
			Record.bRecorded = true;
			Record.Stage = Stage;
			Record.WorldTimeSeconds = WorldTimeSeconds;
			Record.RuntimeState = RuntimeState;
			Record.PolicyControlTick = PolicyControlTick;
			Record.Values = Values;
			OutRecord = MoveTemp(Record);
			return true;
		}
	}

	void FPhysAnimFirstPolicyGroundReferenceRecord::Reset()
	{
		bRecorded = false;
		Stage.Reset();
		WorldTimeSeconds = -1.0;
		RuntimeState.Reset();
		PolicyControlTick = INDEX_NONE;
		Values = FPhysAnimSelfObservationGroundReferenceValues();
	}

	bool FPhysAnimFirstPolicyGroundReferenceTrace::CapturePriorIf(
		const bool bCondition,
		const FString& InStage,
		const double InWorldTimeSeconds,
		const FString& InRuntimeState,
		const int32 InPolicyControlTick,
		const FPhysAnimSelfObservationGroundReferenceValues& InValues)
	{
		if (!bCondition || Prior.bRecorded || !ValidationError.IsEmpty())
		{
			return false;
		}
		if (InStage != TEXT("pre_state_machine") ||
			InRuntimeState != TEXT("WaitingForPoseSearch") ||
			InPolicyControlTick != 0)
		{
			ValidationError = TEXT("The prior ground reference must come from pre_state_machine in WaitingForPoseSearch before any policy tick.");
			return false;
		}

		FString Error;
		if (!BuildGroundReferenceRecord(
				InStage,
				InWorldTimeSeconds,
				InRuntimeState,
				InPolicyControlTick,
				InValues,
				Prior,
				Error))
		{
			ValidationError = MoveTemp(Error);
			return false;
		}
		return true;
	}

	bool FPhysAnimFirstPolicyGroundReferenceTrace::RecordFirstPolicyIf(
		const bool bCondition,
		const FString& InStage,
		const double InWorldTimeSeconds,
		const FString& InRuntimeState,
		const int32 InPolicyControlTick,
		const FPhysAnimSelfObservationGroundReferenceValues& InValues,
		FString& OutError)
	{
		OutError.Reset();
		if (!bCondition || bFirstPolicyRecorded)
		{
			return true;
		}
		if (!ValidationError.IsEmpty())
		{
			OutError = ValidationError;
			return false;
		}
		if (!Prior.bRecorded)
		{
			ValidationError = TEXT("The first-policy ground reference cannot be recorded before the prior reference.");
			OutError = ValidationError;
			return false;
		}
		if (InStage != TEXT("first_policy_self_observation") ||
			InPolicyControlTick != 1 ||
			!FMath::IsFinite(InWorldTimeSeconds) ||
			InWorldTimeSeconds <= Prior.WorldTimeSeconds)
		{
			ValidationError = TEXT("The live ground-reference metadata does not identify the first post-prior policy observation.");
			OutError = ValidationError;
			return false;
		}

		if (!BuildGroundReferenceRecord(
				InStage,
				InWorldTimeSeconds,
				InRuntimeState,
				InPolicyControlTick,
				InValues,
				Live,
				OutError))
		{
			ValidationError = OutError;
			return false;
		}
		bFirstPolicyRecorded = true;
		return true;
	}

	void FPhysAnimFirstPolicyGroundReferenceTrace::Reset()
	{
		bFirstPolicyRecorded = false;
		ValidationError.Reset();
		Prior.Reset();
		Live.Reset();
	}

	bool ValidateFirstPolicyGroundReferenceTrace(
		const FPhysAnimFirstPolicyGroundReferenceTrace& Trace,
		FString& OutError)
	{
		OutError.Reset();
		if (!Trace.ValidationError.IsEmpty())
		{
			OutError = Trace.ValidationError;
			return false;
		}
		if (!Trace.bFirstPolicyRecorded || !Trace.Prior.bRecorded || !Trace.Live.bRecorded)
		{
			OutError = TEXT("First-policy ground-reference evidence is incomplete.");
			return false;
		}
		if (Trace.Prior.Stage != TEXT("pre_state_machine") ||
			Trace.Prior.RuntimeState != TEXT("WaitingForPoseSearch") ||
			Trace.Prior.PolicyControlTick != 0 ||
			Trace.Live.Stage != TEXT("first_policy_self_observation") ||
			Trace.Live.RuntimeState == TEXT("WaitingForPoseSearch") ||
			Trace.Live.PolicyControlTick != 1 ||
			Trace.Prior.WorldTimeSeconds >= Trace.Live.WorldTimeSeconds)
		{
			OutError = TEXT("First-policy ground-reference stage, state, time, or tick ownership is invalid.");
			return false;
		}
		if (!ValidateGroundReferenceValues(Trace.Prior.Values, OutError) ||
			!ValidateGroundReferenceValues(Trace.Live.Values, OutError))
		{
			return false;
		}
		return true;
	}

	bool ValidateStartupChronologyTrace(
		const FPhysAnimStartupChronologyTrace& Trace,
		FString& OutError)
	{
		auto IsFiniteVector = [](const FVector& Value)
		{
			return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
		};
		auto IsFiniteNormalizedQuat = [](const FQuat& Value)
		{
			return FMath::IsFinite(Value.X) &&
				FMath::IsFinite(Value.Y) &&
				FMath::IsFinite(Value.Z) &&
				FMath::IsFinite(Value.W) &&
				Value.IsNormalized();
		};
		auto IsFiniteTransform = [&](const FTransform& Value)
		{
			return IsFiniteVector(Value.GetLocation()) &&
				IsFiniteNormalizedQuat(Value.GetRotation()) &&
				IsFiniteVector(Value.GetScale3D());
		};
		auto IsFiniteBodySample = [&](const FPhysAnimBodySample& Sample)
		{
			return IsFiniteVector(Sample.Position) &&
				IsFiniteNormalizedQuat(Sample.Rotation) &&
				IsFiniteVector(Sample.LinearVelocity) &&
				IsFiniteVector(Sample.AngularVelocity);
		};

		OutError.Reset();
		if (!Trace.CaptureError.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Startup chronology capture failed: %s"), *Trace.CaptureError);
			return false;
		}
		if (!Trace.bComplete ||
			Trace.Samples.Num() < 3 ||
			Trace.Samples.Num() > MaxStartupChronologySamples ||
			Trace.Samples.Num() % 3 != 0)
		{
			OutError = TEXT("Startup chronology is incomplete or has an invalid sample count.");
			return false;
		}

		double PreviousWorldTimeSeconds = -TNumericLimits<double>::Max();
		int32 PreviousPolicyControlTicks = 0;
		for (int32 SampleIndex = 0; SampleIndex < Trace.Samples.Num(); ++SampleIndex)
		{
			const FPhysAnimStartupChronologySample& Sample = Trace.Samples[SampleIndex];
			const TCHAR* ExpectedStage = SampleIndex % 3 == 0
				? TEXT("pre_state_machine")
				: (SampleIndex % 3 == 1 ? TEXT("post_state_machine") : TEXT("post_policy"));
			if (Sample.Sequence != SampleIndex || Sample.Stage != ExpectedStage)
			{
				OutError = FString::Printf(TEXT("Startup chronology sample %d has invalid ordering."), SampleIndex);
				return false;
			}
			if (Sample.RuntimeState.IsEmpty() ||
				!FMath::IsFinite(Sample.WorldTimeSeconds) ||
				!FMath::IsFinite(Sample.PolicyUpdateAccumulatorSeconds) ||
				Sample.LastPolicyElapsedSteps < 0 ||
				Sample.PolicyControlTicksExecuted < 0 ||
				Sample.WorldTimeSeconds < PreviousWorldTimeSeconds ||
				Sample.PolicyControlTicksExecuted < PreviousPolicyControlTicks)
			{
				OutError = FString::Printf(TEXT("Startup chronology sample %d has invalid scalar metadata."), SampleIndex);
				return false;
			}
			if (SampleIndex % 3 != 0 &&
				!FMath::IsNearlyEqual(Sample.WorldTimeSeconds, Trace.Samples[SampleIndex - 1].WorldTimeSeconds, 1.0e-9))
			{
				OutError = FString::Printf(TEXT("Startup chronology sample %d crossed a world tick inside a stage triplet."), SampleIndex);
				return false;
			}
			if (!IsFiniteTransform(Sample.OwnerActorWorldTransform) ||
				!IsFiniteTransform(Sample.MeshWorldTransform) ||
				!IsFiniteTransform(Sample.RootBoneWorldTransform))
			{
				OutError = FString::Printf(TEXT("Startup chronology sample %d contains an invalid transform."), SampleIndex);
				return false;
			}
			if (Sample.BodySamples.Num() != NumSmplBodies)
			{
				OutError = FString::Printf(
					TEXT("Startup chronology sample %d has %d bodies instead of %d."),
					SampleIndex,
					Sample.BodySamples.Num(),
					NumSmplBodies);
				return false;
			}
			for (int32 BodyIndex = 0; BodyIndex < Sample.BodySamples.Num(); ++BodyIndex)
			{
				if (!IsFiniteBodySample(Sample.BodySamples[BodyIndex]))
				{
					OutError = FString::Printf(
						TEXT("Startup chronology sample %d body %d is invalid."),
						SampleIndex,
						BodyIndex);
					return false;
				}
			}
			PreviousWorldTimeSeconds = Sample.WorldTimeSeconds;
			PreviousPolicyControlTicks = Sample.PolicyControlTicksExecuted;
		}

		const FPhysAnimStartupChronologySample& LastSample = Trace.Samples.Last();
		if (LastSample.Stage != TEXT("post_policy") || LastSample.PolicyControlTicksExecuted < 1)
		{
			OutError = TEXT("Startup chronology did not close after a completed policy-control tick.");
			return false;
		}
		return true;
	}
#endif

	void BuildZeroTerrain(TArray<float>& OutTerrain)
	{
		OutTerrain.Init(0.0f, TerrainSize);
	}

	bool ConditionModelActions(
		const TArray<float>& RawActions,
		const TArray<float>* PreviousConditionedActions,
		const FPhysAnimActionConditioningSettings& Settings,
		TArray<float>& OutConditionedActions,
		FPhysAnimActionDiagnostics& OutDiagnostics,
		FString& OutError)
	{
		if (RawActions.Num() != NumActionFloats)
		{
			OutError = FString::Printf(TEXT("Expected %d action floats but found %d."), NumActionFloats, RawActions.Num());
			return false;
		}

		const bool bUsePrevious = PreviousConditionedActions && PreviousConditionedActions->Num() == RawActions.Num();
		const float ClampAbs = FMath::Max(Settings.ActionClampAbs, 0.0f);
		const float SmoothingAlpha = FMath::Clamp(Settings.ActionSmoothingAlpha, 0.0f, 1.0f);
		const float Scale = FMath::Max(Settings.ActionScale, 0.0f);

		OutConditionedActions.SetNumUninitialized(RawActions.Num());
		OutDiagnostics = {};
		OutDiagnostics.RawMin = RawActions[0];
		OutDiagnostics.RawMax = RawActions[0];

		for (int32 Index = 0; Index < RawActions.Num(); ++Index)
		{
			const float RawValue = RawActions[Index];
			OutDiagnostics.RawMin = FMath::Min(OutDiagnostics.RawMin, RawValue);
			OutDiagnostics.RawMax = FMath::Max(OutDiagnostics.RawMax, RawValue);
			OutDiagnostics.RawMeanAbs += FMath::Abs(RawValue);

			float ConditionedValue = Settings.bForceZeroActions ? 0.0f : (RawValue * Scale);
			const float ClampedValue = FMath::Clamp(ConditionedValue, -ClampAbs, ClampAbs);
			if (!FMath::IsNearlyEqual(ConditionedValue, ClampedValue))
			{
				++OutDiagnostics.NumClampedActionFloats;
			}
			ConditionedValue = ClampedValue;

			if (bUsePrevious)
			{
				ConditionedValue = FMath::Lerp((*PreviousConditionedActions)[Index], ConditionedValue, SmoothingAlpha);
			}

			OutConditionedActions[Index] = ConditionedValue;
			OutDiagnostics.ConditionedMeanAbs += FMath::Abs(ConditionedValue);
		}

		OutDiagnostics.RawMeanAbs /= static_cast<float>(RawActions.Num());
		OutDiagnostics.ConditionedMeanAbs /= static_cast<float>(OutConditionedActions.Num());
		return true;
	}

	bool ConvertModelActionsToControlRotations(
		const TArray<float>& ModelActions,
		TMap<FName, FQuat>& OutControlRotations,
		FString& OutError)
	{
		if (ModelActions.Num() != NumActionFloats)
		{
			OutError = FString::Printf(TEXT("Expected %d action floats but found %d."), NumActionFloats, ModelActions.Num());
			return false;
		}

		TStaticArray<FQuat, NumActionJoints> ProtoJointRotations;
		for (int32 JointIndex = 0; JointIndex < NumActionJoints; ++JointIndex)
		{
			const int32 BaseIndex = JointIndex * 3;
			const FVector ExpMap(
				static_cast<double>(PI) * ModelActions[BaseIndex + 0],
				static_cast<double>(PI) * ModelActions[BaseIndex + 1],
				static_cast<double>(PI) * ModelActions[BaseIndex + 2]);
			ProtoJointRotations[JointIndex] = ExpMapToQuaternion(ExpMap);
		}

		OutControlRotations.Reset();
		OutControlRotations.Reserve(NumControlledBones);
		OutControlRotations.Add(TEXT("thigh_l"), ProtoJointQuaternionToUe(ProtoJointRotations[0]));
		OutControlRotations.Add(TEXT("calf_l"), ProtoJointQuaternionToUe(ProtoJointRotations[1]));
		OutControlRotations.Add(TEXT("foot_l"), ProtoJointQuaternionToUe(ProtoJointRotations[2]));
		OutControlRotations.Add(TEXT("ball_l"), ProtoJointQuaternionToUe(ProtoJointRotations[3]));
		OutControlRotations.Add(TEXT("thigh_r"), ProtoJointQuaternionToUe(ProtoJointRotations[4]));
		OutControlRotations.Add(TEXT("calf_r"), ProtoJointQuaternionToUe(ProtoJointRotations[5]));
		OutControlRotations.Add(TEXT("foot_r"), ProtoJointQuaternionToUe(ProtoJointRotations[6]));
		OutControlRotations.Add(TEXT("ball_r"), ProtoJointQuaternionToUe(ProtoJointRotations[7]));
		OutControlRotations.Add(TEXT("spine_01"), ProtoJointQuaternionToUe(ProtoJointRotations[8]));
		OutControlRotations.Add(TEXT("spine_02"), ProtoJointQuaternionToUe(ProtoJointRotations[9]));
		OutControlRotations.Add(TEXT("spine_03"), ProtoJointQuaternionToUe(ProtoJointRotations[10]));
		OutControlRotations.Add(TEXT("neck_01"), ProtoJointQuaternionToUe(ProtoJointRotations[11]));
		OutControlRotations.Add(TEXT("head"), ProtoJointQuaternionToUe(ProtoJointRotations[12]));
		OutControlRotations.Add(TEXT("clavicle_l"), ProtoJointQuaternionToUe(ProtoJointRotations[13]));
		OutControlRotations.Add(TEXT("upperarm_l"), ProtoJointQuaternionToUe(ProtoJointRotations[14]));
		OutControlRotations.Add(TEXT("lowerarm_l"), ProtoJointQuaternionToUe(ProtoJointRotations[15]));
		OutControlRotations.Add(TEXT("hand_l"), ProtoJointQuaternionToUe(CollapseDistalHandRotation(ProtoJointRotations[16], ProtoJointRotations[17])));
		OutControlRotations.Add(TEXT("clavicle_r"), ProtoJointQuaternionToUe(ProtoJointRotations[18]));
		OutControlRotations.Add(TEXT("upperarm_r"), ProtoJointQuaternionToUe(ProtoJointRotations[19]));
		OutControlRotations.Add(TEXT("lowerarm_r"), ProtoJointQuaternionToUe(ProtoJointRotations[20]));
		OutControlRotations.Add(TEXT("hand_r"), ProtoJointQuaternionToUe(CollapseDistalHandRotation(ProtoJointRotations[21], ProtoJointRotations[22])));

		return true;
	}

	bool BuildActionJointSemanticTrace(
		const TArray<float>& RawActions,
		const TArray<float>& ConditionedActions,
		TArray<FPhysAnimActionJointSemanticTrace>& OutTrace,
		FString& OutError)
	{
		OutTrace.Reset();
		if (RawActions.Num() != NumActionFloats || ConditionedActions.Num() != NumActionFloats)
		{
			OutError = FString::Printf(
				TEXT("Action semantic trace requires %d raw and conditioned values but found %d and %d."),
				NumActionFloats,
				RawActions.Num(),
				ConditionedActions.Num());
			return false;
		}
		if (!ValidateFiniteFloatBuffer(TEXT("Raw action semantic trace"), RawActions, OutError) ||
			!ValidateFiniteFloatBuffer(TEXT("Conditioned action semantic trace"), ConditionedActions, OutError))
		{
			return false;
		}

		const TArray<FPhysAnimProtoActionJointDescriptor>& Descriptors = GetProtoActionJointDescriptors();
		if (Descriptors.Num() != NumActionJoints)
		{
			OutError = FString::Printf(
				TEXT("Action semantic trace expected %d joint descriptors but found %d."),
				NumActionJoints,
				Descriptors.Num());
			return false;
		}

		OutTrace.Reserve(NumActionJoints);
		for (const FPhysAnimProtoActionJointDescriptor& Descriptor : Descriptors)
		{
			const int32 BaseIndex = Descriptor.ProtoJointIndex * 3;
			FPhysAnimActionJointSemanticTrace& Entry = OutTrace.AddDefaulted_GetRef();
			Entry.ProtoJointIndex = Descriptor.ProtoJointIndex;
			Entry.ProtoJointName = Descriptor.ProtoJointName;
			Entry.MannyBoneName = Descriptor.MannyBoneName;
			Entry.bSharesMappedControl = Descriptor.bSharesMappedControl;
			Entry.RawAction = FVector(
				RawActions[BaseIndex + 0],
				RawActions[BaseIndex + 1],
				RawActions[BaseIndex + 2]);
			Entry.ConditionedAction = FVector(
				ConditionedActions[BaseIndex + 0],
				ConditionedActions[BaseIndex + 1],
				ConditionedActions[BaseIndex + 2]);
			Entry.RawDecodedRotationUe = ProtoJointQuaternionToUe(
				ExpMapToQuaternion(PI * Entry.RawAction));
			Entry.ConditionedDecodedRotationUe = ProtoJointQuaternionToUe(
				ExpMapToQuaternion(PI * Entry.ConditionedAction));
		}

		OutError.Reset();
		return true;
	}

	bool ValidateActionSemanticTrace(
		const FPhysAnimActionSemanticTrace& Trace,
		FString& OutError)
	{
		auto IsFiniteVector = [](const FVector& Value)
		{
			return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
		};
		auto IsFiniteNormalizedQuat = [](const FQuat& Value)
		{
			return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) &&
				FMath::IsFinite(Value.Z) && FMath::IsFinite(Value.W) && Value.IsNormalized();
		};
		if (!Trace.bCaptured || Trace.CaptureScope.IsEmpty() || !Trace.CaptureError.IsEmpty())
		{
			OutError = TEXT("Action semantic trace is not a successful named capture.");
			return false;
		}
		if (!FMath::IsFinite(Trace.PolicyStepDeltaTime) || Trace.PolicyStepDeltaTime < 0.0f ||
			!FMath::IsFinite(Trace.PolicyInfluenceAlpha) || Trace.PolicyInfluenceAlpha < 0.0f || Trace.PolicyInfluenceAlpha > 1.0f ||
			!FMath::IsFinite(Trace.MaxAngularStepDegrees) || Trace.MaxAngularStepDegrees < 0.0f)
		{
			OutError = TEXT("Action semantic trace has invalid global policy-step values.");
			return false;
		}

		const TArray<FPhysAnimProtoActionJointDescriptor>& Descriptors = GetProtoActionJointDescriptors();
		if (Trace.ActionJoints.Num() != NumActionJoints || Descriptors.Num() != NumActionJoints)
		{
			OutError = FString::Printf(
				TEXT("Action semantic trace expected %d ordered joints but found %d."),
				NumActionJoints,
				Trace.ActionJoints.Num());
			return false;
		}
		for (int32 JointIndex = 0; JointIndex < NumActionJoints; ++JointIndex)
		{
			const FPhysAnimActionJointSemanticTrace& Entry = Trace.ActionJoints[JointIndex];
			const FPhysAnimProtoActionJointDescriptor& Descriptor = Descriptors[JointIndex];
			if (Entry.ProtoJointIndex != JointIndex ||
				Entry.ProtoJointName != Descriptor.ProtoJointName ||
				Entry.MannyBoneName != Descriptor.MannyBoneName ||
				Entry.bSharesMappedControl != Descriptor.bSharesMappedControl ||
				!IsFiniteVector(Entry.RawAction) ||
				!IsFiniteVector(Entry.ConditionedAction) ||
				!IsFiniteNormalizedQuat(Entry.RawDecodedRotationUe) ||
				!IsFiniteNormalizedQuat(Entry.ConditionedDecodedRotationUe))
			{
				OutError = FString::Printf(TEXT("Action semantic trace joint %d is malformed."), JointIndex);
				return false;
			}
		}

		const TArray<FName>& ControlledBones = GetControlledBoneNames();
		if (Trace.ControlTargets.Num() != NumControlledBones || ControlledBones.Num() != NumControlledBones)
		{
			OutError = FString::Printf(
				TEXT("Action semantic trace expected %d control targets but found %d."),
				NumControlledBones,
				Trace.ControlTargets.Num());
			return false;
		}
		for (int32 ControlIndex = 0; ControlIndex < NumControlledBones; ++ControlIndex)
		{
			const FPhysAnimControlTargetSemanticTrace& Entry = Trace.ControlTargets[ControlIndex];
			const FName ExpectedBoneName = ControlledBones[ControlIndex];
			TArray<int32> ExpectedSourceIndices;
			for (const FPhysAnimProtoActionJointDescriptor& Descriptor : Descriptors)
			{
				if (Descriptor.MannyBoneName == ExpectedBoneName)
				{
					ExpectedSourceIndices.Add(Descriptor.ProtoJointIndex);
				}
			}

			const bool bFiniteScalars =
				FMath::IsFinite(Entry.TwistLimitDegrees) &&
				FMath::IsFinite(Entry.Swing1LimitDegrees) &&
				FMath::IsFinite(Entry.Swing2LimitDegrees) &&
				FMath::IsFinite(Entry.LowerLimbRangeScale) && Entry.LowerLimbRangeScale >= 0.0f &&
				FMath::IsFinite(Entry.DistalRangeScale) && Entry.DistalRangeScale >= 0.0f &&
				FMath::IsFinite(Entry.RawPolicyOffsetDegrees) && Entry.RawPolicyOffsetDegrees >= 0.0f &&
				FMath::IsFinite(Entry.RangeScaleDeltaDegrees) && Entry.RangeScaleDeltaDegrees >= 0.0f &&
				FMath::IsFinite(Entry.DistalScaleDeltaDegrees) && Entry.DistalScaleDeltaDegrees >= 0.0f &&
				FMath::IsFinite(Entry.ConstraintRangeMappingDeltaDegrees) && Entry.ConstraintRangeMappingDeltaDegrees >= 0.0f &&
				FMath::IsFinite(Entry.ConstraintProjectionDeltaDegrees) && Entry.ConstraintProjectionDeltaDegrees >= 0.0f &&
				FMath::IsFinite(Entry.AdaptedToPublishedDeltaDegrees) && Entry.AdaptedToPublishedDeltaDegrees >= 0.0f &&
				FMath::IsFinite(Entry.ReadbackErrorDegrees) && Entry.ReadbackErrorDegrees >= 0.0f;
			const bool bFiniteStages =
				IsFiniteNormalizedQuat(Entry.CombinedDecodedRotationUe) &&
				IsFiniteNormalizedQuat(Entry.MannyNeutralRotation) &&
				IsFiniteNormalizedQuat(Entry.BindComposedRotation) &&
				IsFiniteNormalizedQuat(Entry.RangeScaledRotation) &&
				IsFiniteNormalizedQuat(Entry.DistalScaledRotation) &&
				IsFiniteNormalizedQuat(Entry.ConstraintRangeMappedRotation) &&
				IsFiniteNormalizedQuat(Entry.ConstraintAdaptedRotation) &&
				IsFiniteNormalizedQuat(Entry.BlendedRotation) &&
				IsFiniteNormalizedQuat(Entry.PublishedRotation) &&
				IsFiniteNormalizedQuat(Entry.ReadbackRotation);
			if (Entry.MannyBoneName != ExpectedBoneName ||
				Entry.ControlName != MakeControlName(ExpectedBoneName) ||
				Entry.SourceProtoJointIndices != ExpectedSourceIndices ||
				!Entry.bTargetWritten || !Entry.bReadbackSucceeded ||
				!bFiniteScalars || !bFiniteStages)
			{
				OutError = FString::Printf(
					TEXT("Action semantic trace control target %d (%s) is malformed."),
					ControlIndex,
					*ExpectedBoneName.ToString());
				return false;
			}
		}

		OutError.Reset();
		return true;
	}

#if WITH_DEV_AUTOMATION_TESTS
	bool ValidateMannyLocalFrameRoundtripTrace(
		const FPhysAnimMannyLocalFrameRoundtripTrace& Trace,
		FString& OutError)
	{
		auto IsFiniteNormalizedQuat = [](const FQuat& Value)
		{
			return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) &&
				FMath::IsFinite(Value.Z) && FMath::IsFinite(Value.W) && Value.IsNormalized();
		};
		auto IsFiniteNonNegative = [](double Value)
		{
			return FMath::IsFinite(Value) && Value >= 0.0;
		};

		if (!Trace.bCaptured ||
			Trace.CaptureScope != TEXT("first_active_standing_pre_range_target") ||
			!Trace.CaptureError.IsEmpty())
		{
			OutError = TEXT("Manny local-frame round-trip trace is not a successful first-active-standing capture.");
			return false;
		}
		if (!FMath::IsFinite(Trace.AxisProbeDegrees) ||
			!FMath::IsNearlyEqual(Trace.AxisProbeDegrees, MannyLocalFrameRoundtripAxisProbeDegrees))
		{
			OutError = TEXT("Manny local-frame round-trip trace has an invalid axis probe.");
			return false;
		}
		const bool bValidConfiguredMode =
			Trace.ConfiguredActionAxisMode == MannyLocalFrameRoundtripWorldAxisMode ||
			Trace.ConfiguredActionAxisMode == MannyLocalFrameRoundtripComponentAxisMode;
		if (!bValidConfiguredMode ||
			Trace.EffectiveActionAxisMode != Trace.ConfiguredActionAxisMode)
		{
			OutError = TEXT("Manny local-frame round-trip trace has inconsistent configured/effective action-axis modes.");
			return false;
		}

		const TArray<FName>& ControlledBones = GetControlledBoneNames();
		const TArray<FName>& ObservationBodyNames = GetSmplObservationBoneNames();
		const TArray<FPhysAnimProtoActionJointDescriptor>& Descriptors = GetProtoActionJointDescriptors();
		if (Trace.Controls.Num() != NumControlledBones ||
			ControlledBones.Num() != NumControlledBones ||
			ObservationBodyNames.Num() != NumSmplBodies ||
			Descriptors.Num() != NumActionJoints)
		{
			OutError = FString::Printf(
				TEXT("Manny local-frame round-trip trace expected %d ordered controls but found %d."),
				NumControlledBones,
				Trace.Controls.Num());
			return false;
		}

		static const TArray<FName> ExpectedCaseLabels = {
			TEXT("identity"),
			TEXT("actual_decoded"),
			TEXT("positive_x_10_deg"),
			TEXT("negative_x_10_deg"),
			TEXT("positive_y_10_deg"),
			TEXT("negative_y_10_deg"),
			TEXT("positive_z_10_deg"),
			TEXT("negative_z_10_deg")
		};

		int32 DecisiveControlCount = 0;
		for (int32 ControlIndex = 0; ControlIndex < NumControlledBones; ++ControlIndex)
		{
			const FPhysAnimMannyLocalFrameRoundtripControl& Entry = Trace.Controls[ControlIndex];
			const FName ExpectedBoneName = ControlledBones[ControlIndex];
			TArray<int32> ExpectedSourceIndices;
			TArray<FName> ExpectedSourceNames;
			TArray<int32> ExpectedObservationIndices;
			TArray<FName> ExpectedObservationNames;
			for (const FPhysAnimProtoActionJointDescriptor& Descriptor : Descriptors)
			{
				if (Descriptor.MannyBoneName != ExpectedBoneName)
				{
					continue;
				}
				const int32 ObservationBodyIndex = Descriptor.ProtoJointIndex + 1;
				if (!ObservationBodyNames.IsValidIndex(ObservationBodyIndex))
				{
					OutError = FString::Printf(
						TEXT("Manny local-frame round-trip control %d has an invalid observation body mapping."),
						ControlIndex);
					return false;
				}
				ExpectedSourceIndices.Add(Descriptor.ProtoJointIndex);
				ExpectedSourceNames.Add(Descriptor.ProtoJointName);
				ExpectedObservationIndices.Add(ObservationBodyIndex);
				ExpectedObservationNames.Add(ObservationBodyNames[ObservationBodyIndex]);
			}

			const bool bExpectedDecisive = ExpectedSourceIndices.Num() == 1;
			DecisiveControlCount += bExpectedDecisive ? 1 : 0;
			const bool bValidParent =
				ObservationBodyNames.IsValidIndex(Entry.ObservationParentBodyIndex) &&
				Entry.ObservationParentBodyName == ObservationBodyNames[Entry.ObservationParentBodyIndex] &&
				Entry.InitialControlParentBoneName == Entry.ObservationParentBodyName;
			const bool bValidFrames =
				IsFiniteNormalizedQuat(Entry.CachedActionAxisReferenceRotation) &&
				IsFiniteNormalizedQuat(Entry.ActionBindComponentWorldRotation) &&
				IsFiniteNormalizedQuat(Entry.ComponentCorrectedActionAxisRotation) &&
				IsFiniteNormalizedQuat(Entry.EffectiveActionAxisRotation) &&
				IsFiniteNormalizedQuat(Entry.ActionBindParentRelativeRotation) &&
				IsFiniteNormalizedQuat(Entry.PolicyNeutralParentRelativeRotation) &&
				IsFiniteNormalizedQuat(Entry.ObservationParentBindComponentRotation) &&
				IsFiniteNormalizedQuat(Entry.ObservationBodyBindComponentRotation) &&
				IsFiniteNormalizedQuat(Entry.ObservationBindParentRelativeRotation) &&
				IsFiniteNormalizedQuat(Entry.ActualDecodedRotationUe) &&
				IsFiniteNormalizedQuat(Entry.ActualMannyPreRangeTargetParentRelative);
			const bool bValidRelationshipMetrics =
				IsFiniteNonNegative(Entry.ActionAxisVsObservationParentBindAngularDeltaDegrees) &&
				IsFiniteNonNegative(Entry.EffectiveActionAxisVsObservationParentBindComponentAngularDeltaDegrees) &&
				IsFiniteNonNegative(Entry.ActionBindVsObservationBindParentRelativeAngularDeltaDegrees) &&
				IsFiniteNonNegative(Entry.PolicyNeutralVsActionBindParentRelativeAngularDeltaDegrees) &&
				IsFiniteNonNegative(Entry.PolicyNeutralVsObservationBindParentRelativeAngularDeltaDegrees);

			if (Entry.ControlIndex != ControlIndex ||
				Entry.MannyBoneName != ExpectedBoneName ||
				Entry.ControlName != MakeControlName(ExpectedBoneName) ||
				Entry.InitialControlChildBoneName != ExpectedBoneName ||
				Entry.SourceProtoJointIndices != ExpectedSourceIndices ||
				Entry.SourceProtoJointNames != ExpectedSourceNames ||
				Entry.ObservationBodyIndices != ExpectedObservationIndices ||
				Entry.ObservationBodyNames != ExpectedObservationNames ||
				ExpectedObservationIndices.IsEmpty() ||
				Entry.RoundtripObservationBodyIndex != ExpectedObservationIndices[0] ||
				Entry.RoundtripObservationBodyName != ExpectedObservationNames[0] ||
				Entry.EffectiveActionAxisMode != Trace.EffectiveActionAxisMode ||
				Entry.bDecisiveOneToOne != bExpectedDecisive ||
				!Entry.bOwnershipComplete || !bValidParent || !bValidFrames || !bValidRelationshipMetrics ||
				Entry.RoundtripCases.Num() != ExpectedCaseLabels.Num())
			{
				OutError = FString::Printf(
					TEXT("Manny local-frame round-trip control %d (%s) is malformed."),
					ControlIndex,
					*ExpectedBoneName.ToString());
				return false;
			}

			const FQuat ExpectedComponentCorrectedAxis =
				(Entry.ActionBindComponentWorldRotation.Inverse() *
				 Entry.CachedActionAxisReferenceRotation).GetNormalized();
			const FQuat ExpectedEffectiveAxis =
				Trace.EffectiveActionAxisMode == MannyLocalFrameRoundtripWorldAxisMode
					? Entry.CachedActionAxisReferenceRotation
					: ExpectedComponentCorrectedAxis;
			if (ExpectedComponentCorrectedAxis.AngularDistance(
					Entry.ComponentCorrectedActionAxisRotation) > 1.0e-6 ||
				ExpectedEffectiveAxis.AngularDistance(Entry.EffectiveActionAxisRotation) > 1.0e-6)
			{
				OutError = FString::Printf(
					TEXT("Manny local-frame round-trip control %d effective action axis does not match its mode."),
					ControlIndex);
				return false;
			}

			for (int32 CaseIndex = 0; CaseIndex < ExpectedCaseLabels.Num(); ++CaseIndex)
			{
				const FPhysAnimMannyLocalFrameRoundtripCase& Case = Entry.RoundtripCases[CaseIndex];
				if (Case.Label != ExpectedCaseLabels[CaseIndex] ||
					!IsFiniteNormalizedQuat(Case.InputCanonicalRotationUe) ||
					!IsFiniteNormalizedQuat(Case.MannyPreRangeTargetParentRelative) ||
					!IsFiniteNormalizedQuat(Case.RecoveredCanonicalRotationUe) ||
					!IsFiniteNonNegative(Case.AngularErrorDegrees))
				{
					OutError = FString::Printf(
						TEXT("Manny local-frame round-trip control %d case %d is malformed."),
						ControlIndex,
						CaseIndex);
					return false;
				}
			}

			const FPhysAnimMannyLocalFrameRoundtripCase& ActualCase = Entry.RoundtripCases[1];
			if (!ActualCase.InputCanonicalRotationUe.Equals(Entry.ActualDecodedRotationUe) ||
				!ActualCase.MannyPreRangeTargetParentRelative.Equals(
					Entry.ActualMannyPreRangeTargetParentRelative))
			{
				OutError = FString::Printf(
					TEXT("Manny local-frame round-trip control %d actual case does not match captured runtime values."),
					ControlIndex);
				return false;
			}
		}

		if (DecisiveControlCount != 19)
		{
			OutError = FString::Printf(
				TEXT("Manny local-frame round-trip trace expected 19 decisive controls but found %d."),
				DecisiveControlCount);
			return false;
		}

		OutError.Reset();
		return true;
	}
#endif

	FQuat LimitControlRotationStep(
		const FQuat& PreviousRotation,
		const FQuat& TargetRotation,
		float MaxAngularStepDegrees)
	{
		if (MaxAngularStepDegrees <= 0.0f)
		{
			return TargetRotation;
		}

		const double MaxAngularStepRadians = FMath::DegreesToRadians(static_cast<double>(MaxAngularStepDegrees));
		const double AngularDistance = PreviousRotation.AngularDistance(TargetRotation);
		if (AngularDistance <= MaxAngularStepRadians || AngularDistance <= UE_DOUBLE_SMALL_NUMBER)
		{
			return TargetRotation;
		}

		const double SlerpAlpha = MaxAngularStepRadians / AngularDistance;
		return FQuat::Slerp(PreviousRotation, TargetRotation, SlerpAlpha).GetNormalized();
	}

	bool UpdateRuntimeInstabilityState(
		const FVector& RootLocationCm,
		const FVector& RootLinearVelocityCmPerSecond,
		const FVector& RootAngularVelocityDegPerSecond,
		float DeltaTimeSeconds,
		const FPhysAnimRuntimeInstabilitySettings& Settings,
		FPhysAnimRuntimeInstabilityState& InOutState,
		FPhysAnimRuntimeInstabilityDiagnostics& OutDiagnostics,
		FString& OutError)
	{
		OutDiagnostics = {};
		OutError.Reset();

		if (!Settings.bEnableAutomaticFailStop)
		{
			if (!InOutState.bHasReferenceRootLocation)
			{
				InOutState.bHasReferenceRootLocation = true;
				InOutState.ReferenceRootLocation = RootLocationCm;
			}

			return true;
		}

		if (!InOutState.bHasReferenceRootLocation)
		{
			InOutState.bHasReferenceRootLocation = true;
			InOutState.ReferenceRootLocation = RootLocationCm;
		}

		OutDiagnostics.RootLocationCm = RootLocationCm;
		OutDiagnostics.RootLinearVelocityCmPerSecondVector = RootLinearVelocityCmPerSecond;
		OutDiagnostics.RootHeightDeltaCm = FMath::Abs(RootLocationCm.Z - InOutState.ReferenceRootLocation.Z);
		OutDiagnostics.RootLinearSpeedCmPerSecond = RootLinearVelocityCmPerSecond.Size();
		OutDiagnostics.RootAngularSpeedDegPerSecond = RootAngularVelocityDegPerSecond.Size();

		OutDiagnostics.bHeightExceeded =
			OutDiagnostics.RootHeightDeltaCm > FMath::Max(Settings.MaxRootHeightDeltaCm, 0.0f);
		OutDiagnostics.bLinearSpeedExceeded =
			OutDiagnostics.RootLinearSpeedCmPerSecond > FMath::Max(Settings.MaxRootLinearSpeedCmPerSecond, 0.0f);
		OutDiagnostics.bAngularSpeedExceeded =
			OutDiagnostics.RootAngularSpeedDegPerSecond > FMath::Max(Settings.MaxRootAngularSpeedDegPerSecond, 0.0f);

		const bool bAnyLimitExceeded =
			OutDiagnostics.bHeightExceeded ||
			OutDiagnostics.bLinearSpeedExceeded ||
			OutDiagnostics.bAngularSpeedExceeded;
		if (bAnyLimitExceeded)
		{
			InOutState.UnstableAccumulatedSeconds += FMath::Max(DeltaTimeSeconds, 0.0f);
		}
		else
		{
			InOutState.UnstableAccumulatedSeconds = 0.0f;
		}

		OutDiagnostics.UnstableAccumulatedSeconds = InOutState.UnstableAccumulatedSeconds;
		if (InOutState.UnstableAccumulatedSeconds < FMath::Max(Settings.UnstableGracePeriodSeconds, 0.0f))
		{
			return true;
		}

		TArray<FString> ExceededReasons;
		if (OutDiagnostics.bHeightExceeded)
		{
			ExceededReasons.Add(FString::Printf(
				TEXT("rootHeightDeltaCm=%.1f>%.1f"),
				OutDiagnostics.RootHeightDeltaCm,
				Settings.MaxRootHeightDeltaCm));
		}
		if (OutDiagnostics.bLinearSpeedExceeded)
		{
			ExceededReasons.Add(FString::Printf(
				TEXT("rootLinearSpeedCmPerSec=%.1f>%.1f"),
				OutDiagnostics.RootLinearSpeedCmPerSecond,
				Settings.MaxRootLinearSpeedCmPerSecond));
		}
		if (OutDiagnostics.bAngularSpeedExceeded)
		{
			ExceededReasons.Add(FString::Printf(
				TEXT("rootAngularSpeedDegPerSec=%.1f>%.1f"),
				OutDiagnostics.RootAngularSpeedDegPerSecond,
				Settings.MaxRootAngularSpeedDegPerSecond));
		}

		OutError = FString::Printf(
			TEXT("Runtime instability detected after %.2fs (%s)."),
			InOutState.UnstableAccumulatedSeconds,
			*FString::Join(ExceededReasons, TEXT(", ")));
		return false;
	}

	void EvaluatePerBodyInstabilitySamples(
		const TArray<FPhysAnimBodyInstabilitySample>& Samples,
		const FVector& ReferenceRootLocationCm,
		FPhysAnimRuntimeInstabilityDiagnostics& OutDiagnostics)
	{
		OutDiagnostics.NumBodiesConsidered = Samples.Num();
		OutDiagnostics.NumSimulatingBodies = 0;
		OutDiagnostics.MaxLinearSpeedBoneName = NAME_None;
		OutDiagnostics.MaxBodyLinearSpeedCmPerSecond = 0.0f;
		OutDiagnostics.bMaxLinearSpeedBoneSimulatingPhysics = false;
		OutDiagnostics.MaxAngularSpeedBoneName = NAME_None;
		OutDiagnostics.MaxBodyAngularSpeedDegPerSecond = 0.0f;
		OutDiagnostics.bMaxAngularSpeedBoneSimulatingPhysics = false;
		OutDiagnostics.MaxHeightDeltaBoneName = NAME_None;
		OutDiagnostics.MaxBodyHeightDeltaCm = 0.0f;
		OutDiagnostics.bMaxHeightDeltaBoneSimulatingPhysics = false;

		for (const FPhysAnimBodyInstabilitySample& Sample : Samples)
		{
			if (Sample.bIsSimulatingPhysics)
			{
				++OutDiagnostics.NumSimulatingBodies;

				const float LinearSpeedCmPerSecond = Sample.LinearVelocity.Size();
				if (OutDiagnostics.MaxLinearSpeedBoneName == NAME_None ||
					LinearSpeedCmPerSecond > OutDiagnostics.MaxBodyLinearSpeedCmPerSecond)
				{
					OutDiagnostics.MaxLinearSpeedBoneName = Sample.BoneName;
					OutDiagnostics.MaxBodyLinearSpeedCmPerSecond = LinearSpeedCmPerSecond;
					OutDiagnostics.bMaxLinearSpeedBoneSimulatingPhysics = Sample.bIsSimulatingPhysics;
				}

				const float AngularSpeedDegPerSecond = Sample.AngularVelocity.Size();
				if (OutDiagnostics.MaxAngularSpeedBoneName == NAME_None ||
					AngularSpeedDegPerSecond > OutDiagnostics.MaxBodyAngularSpeedDegPerSecond)
				{
					OutDiagnostics.MaxAngularSpeedBoneName = Sample.BoneName;
					OutDiagnostics.MaxBodyAngularSpeedDegPerSecond = AngularSpeedDegPerSecond;
					OutDiagnostics.bMaxAngularSpeedBoneSimulatingPhysics = Sample.bIsSimulatingPhysics;
				}

				const float HeightDeltaCm = FMath::Abs(Sample.Location.Z - ReferenceRootLocationCm.Z);
				if (OutDiagnostics.MaxHeightDeltaBoneName == NAME_None ||
					HeightDeltaCm > OutDiagnostics.MaxBodyHeightDeltaCm)
				{
					OutDiagnostics.MaxHeightDeltaBoneName = Sample.BoneName;
					OutDiagnostics.MaxBodyHeightDeltaCm = HeightDeltaCm;
					OutDiagnostics.bMaxHeightDeltaBoneSimulatingPhysics = Sample.bIsSimulatingPhysics;
				}
			}
		}
	}
}
