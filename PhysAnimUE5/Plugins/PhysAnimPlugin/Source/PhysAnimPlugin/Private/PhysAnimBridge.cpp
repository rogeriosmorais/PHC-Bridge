#include "PhysAnimBridge.h"

#include "Algo/AllOf.h"
#include "Algo/Find.h"
#include "Containers/StaticArray.h"

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
	}

	const TArray<FName>& GetControlledBoneNames()
	{
		return MakeControlledBones();
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

	FVector SmplVectorToUe(const FVector& SmplVector)
	{
		return FVector(SmplVector.Z, SmplVector.X, SmplVector.Y);
	}

	FVector UeVectorToSmpl(const FVector& UeVector)
	{
		return FVector(UeVector.Y, UeVector.Z, UeVector.X);
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

		TStaticArray<FQuat, NumActionJoints> SmplJointRotations;
		for (int32 JointIndex = 0; JointIndex < NumActionJoints; ++JointIndex)
		{
			const int32 BaseIndex = JointIndex * 3;
			const FVector ExpMap(
				static_cast<double>(PI) * ModelActions[BaseIndex + 0],
				static_cast<double>(PI) * ModelActions[BaseIndex + 1],
				static_cast<double>(PI) * ModelActions[BaseIndex + 2]);
			SmplJointRotations[JointIndex] = ExpMapToQuaternion(ExpMap);
		}

		OutControlRotations.Reset();
		OutControlRotations.Reserve(NumControlledBones);
		OutControlRotations.Add(TEXT("thigh_l"), SmplQuaternionToUe(SmplJointRotations[0]));
		OutControlRotations.Add(TEXT("calf_l"), SmplQuaternionToUe(SmplJointRotations[1]));
		OutControlRotations.Add(TEXT("foot_l"), SmplQuaternionToUe(SmplJointRotations[2]));
		OutControlRotations.Add(TEXT("ball_l"), SmplQuaternionToUe(SmplJointRotations[3]));
		OutControlRotations.Add(TEXT("thigh_r"), SmplQuaternionToUe(SmplJointRotations[4]));
		OutControlRotations.Add(TEXT("calf_r"), SmplQuaternionToUe(SmplJointRotations[5]));
		OutControlRotations.Add(TEXT("foot_r"), SmplQuaternionToUe(SmplJointRotations[6]));
		OutControlRotations.Add(TEXT("ball_r"), SmplQuaternionToUe(SmplJointRotations[7]));
		OutControlRotations.Add(TEXT("spine_01"), SmplQuaternionToUe(SmplJointRotations[8]));
		OutControlRotations.Add(TEXT("spine_02"), SmplQuaternionToUe(SmplJointRotations[9]));
		OutControlRotations.Add(TEXT("spine_03"), SmplQuaternionToUe(SmplJointRotations[10]));
		OutControlRotations.Add(TEXT("neck_01"), SmplQuaternionToUe(SmplJointRotations[11]));
		OutControlRotations.Add(TEXT("head"), SmplQuaternionToUe(SmplJointRotations[12]));
		OutControlRotations.Add(TEXT("clavicle_l"), SmplQuaternionToUe(SmplJointRotations[13]));
		OutControlRotations.Add(TEXT("upperarm_l"), SmplQuaternionToUe(SmplJointRotations[14]));
		OutControlRotations.Add(TEXT("lowerarm_l"), SmplQuaternionToUe(SmplJointRotations[15]));
		OutControlRotations.Add(TEXT("hand_l"), SmplQuaternionToUe(CollapseDistalHandRotation(SmplJointRotations[16], SmplJointRotations[17])));
		OutControlRotations.Add(TEXT("clavicle_r"), SmplQuaternionToUe(SmplJointRotations[18]));
		OutControlRotations.Add(TEXT("upperarm_r"), SmplQuaternionToUe(SmplJointRotations[19]));
		OutControlRotations.Add(TEXT("lowerarm_r"), SmplQuaternionToUe(SmplJointRotations[20]));
		OutControlRotations.Add(TEXT("hand_r"), SmplQuaternionToUe(CollapseDistalHandRotation(SmplJointRotations[21], SmplJointRotations[22])));

		return true;
	}

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
			}

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
