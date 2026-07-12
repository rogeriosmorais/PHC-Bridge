#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"
#include "PhysAnimBalanceReadyTransitionPrivate.h"
#include "PhysAnimPhase1AutoCalibSubsystem.h"
#include "PhysAnimPhase1PelvisCouplingSearch.h"
#include "PhysAnimLogger.h"

#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/BodyInstance.h"

void UPhysAnimComponent::ApplyPhase1PelvisRootCouplingSolve()
{
	const bool bPhase1Prepare = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Prepare;
	const bool bPhase1LateValidate = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_LateValidate;
	if (!bPhase1Prepare && !bPhase1LateValidate)
	{
		bPhase1PelvisCouplingSkipLogged = false;
		return;
	}

	if (!BalanceReadyTransition.ShouldApplyPhase1PelvisRootCouplingSolve())
	{
		if (!bPhase1PelvisCouplingSkipLogged)
		{
			PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimBalance] PHASE1_PELVIS_COUPLING_SKIPPED reason=noCouplingProof state=%s"), GetRuntimeStateName(RuntimeState));
			bPhase1PelvisCouplingSkipLogged = true;
		}
		return;
	}

	USkeletalMeshComponent* const Mesh = GetMeshComponent();
	if (!Mesh)
	{
		if (!bPhase1PelvisCouplingSkipLogged)
		{
			PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimBalance] PHASE1_PELVIS_COUPLING_SKIPPED reason=noMesh state=%s"), GetRuntimeStateName(RuntimeState));
			bPhase1PelvisCouplingSkipLogged = true;
		}
		return;
	}

	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	FBodyInstance* const PelvisBody = Mesh->GetBodyInstance(RootBoneName);
	if (!PelvisBody || !PelvisBody->IsValidBodyInstance())
	{
		if (!bPhase1PelvisCouplingSkipLogged)
		{
			PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimBalance] PHASE1_PELVIS_COUPLING_SKIPPED reason=noPelvisBody state=%s"), GetRuntimeStateName(RuntimeState));
			bPhase1PelvisCouplingSkipLogged = true;
		}
		return;
	}

	if (PelvisBody->IsInstanceSimulatingPhysics())
	{
		if (!bPhase1PelvisCouplingSkipLogged)
		{
			PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimBalance] PHASE1_PELVIS_COUPLING_SKIPPED reason=pelvisAlreadySimulating state=%s"), GetRuntimeStateName(RuntimeState));
			bPhase1PelvisCouplingSkipLogged = true;
		}
		return;
	}

	if (UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get())
	{
		const FName PelvisModifierName = PhysAnimBridge::MakeBodyModifierName(RootBoneName);
		if (const FPhysicsBodyModifierRecord* Record = FPhysAnimPhysicsControlAccessor::GetModifierRecord(PhysicsControl, PelvisModifierName))
		{
			if (Record->BodyModifier.ModifierData.MovementType != EPhysicsMovementType::Kinematic ||
				!Record->BodyModifier.ModifierData.bUpdateKinematicFromSimulation)
			{
				if (!bPhase1PelvisCouplingSkipLogged)
				{
					PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimBalance] PHASE1_PELVIS_COUPLING_SKIPPED reason=modifierNotPhase1Kinematic state=%s modifier=%s updateFromSim=%d"),
						GetRuntimeStateName(RuntimeState),
						GetPhysicsMovementTypeName(Record->BodyModifier.ModifierData.MovementType),
						Record->BodyModifier.ModifierData.bUpdateKinematicFromSimulation ? 1 : 0);
					bPhase1PelvisCouplingSkipLogged = true;
				}
				return;
			}
		}
	}

	const int32 PelvisBoneIndex = Mesh->GetBoneIndex(RootBoneName);
	const FTransform AnimatedPelvisTransform =
		PelvisBoneIndex != INDEX_NONE
			? Mesh->GetBoneTransform(PelvisBoneIndex)
			: PelvisBody->GetUnrealWorldTransform();
	const UPhysicsAsset* const PhysicsAsset = Mesh->GetPhysicsAsset();
	const FPhysAnimStabilizationSettings EffectiveSettings = ResolveEffectiveStabilizationSettings();
	const FPhase1PelvisCouplingSearchConfig SearchConfig = BuildPhase1PelvisCouplingSearchConfig(ActivePhase1AutoCalibParams);
	const FPhase1AutoCalibParams* const AutoCalibParams =
		ActivePhase1AutoCalibParams.IsSet() ? &ActivePhase1AutoCalibParams.GetValue() : nullptr;
	const float AutoCalibSpineInterpolationAlpha = SearchConfig.SpineInterpolationAlpha;
	const float AutoCalibWorstThighInterpolationAlpha = SearchConfig.WorstThighInterpolationAlpha;
	const float AutoCalibFocusedDeltaScale = SearchConfig.FocusedDeltaScale;
	const float AutoCalibClampStrengthScale = SearchConfig.ClampStrengthScale;
	const bool bAutoCalibPreferUprightnessEarly =
		SearchConfig.UprightnessWeightScale > 1.0f + KINDA_SMALL_NUMBER;
	bool bRanSpineBiasedDirectBlend = false;
	bool bRanPairBlendSeeds = false;
	bool bRanConstraintInterpolation = false;
	bool bRanWorstThighInterpolation = false;
	bool bRanCoupledTradeControl = false;
	bool bRanPairBlendFrontierFollowThrough = false;
	FQuat DesiredPelvisRotation = AnimatedPelvisTransform.GetRotation();
	FString PelvisRotationSource = TEXT("animated_pelvis_rotation");
	struct FPhase1ConstraintRotationSample
	{
		FName ChildBoneName = NAME_None;
		FQuat CandidateRotation = FQuat::Identity;
		FQuat ChildConstraintWorldRotation = FQuat::Identity;
		FQuat ParentConstraintLocalRotation = FQuat::Identity;
		FVector ChildAnchorWorld = FVector::ZeroVector;
		bool bValid = false;
		FString Source;
	};
	TArray<FPhase1ConstraintRotationSample> RotationSamples;
	RotationSamples.Reserve(4);
	{
		FPhase1ConstraintRotationSample& LiveSample = RotationSamples.AddDefaulted_GetRef();
		LiveSample.CandidateRotation = PelvisBody->GetUnrealWorldTransform().GetRotation();
		LiveSample.Source = TEXT("live_pelvis_rotation");
		LiveSample.bValid = true;
	}
	{
		FPhase1ConstraintRotationSample& AnimatedSample = RotationSamples.AddDefaulted_GetRef();
		AnimatedSample.CandidateRotation = AnimatedPelvisTransform.GetRotation();
		AnimatedSample.Source = TEXT("animated_pelvis_rotation");
		AnimatedSample.bValid = true;
	}

	FVector DesiredPelvisLocation = FVector::ZeroVector;
	int32 DesiredPelvisLocationSamples = 0;
	const auto AccumulateConstraintAnchoredCandidate = [&](const FName ChildBoneName)
	{
		if (!PhysicsAsset)
		{
			return;
		}

		const int32 ConstraintIndex = PhysicsAsset->FindConstraintIndex(ChildBoneName, RootBoneName);
		if (ConstraintIndex == INDEX_NONE || !PhysicsAsset->ConstraintSetup.IsValidIndex(ConstraintIndex))
		{
			return;
		}

		const UPhysicsConstraintTemplate* const ConstraintTemplate = PhysicsAsset->ConstraintSetup[ConstraintIndex];
		const FConstraintInstance* const ConstraintInstance = ConstraintTemplate ? &ConstraintTemplate->DefaultInstance : nullptr;
		if (!ConstraintInstance)
		{
			return;
		}

		const int32 ChildBoneIndex = Mesh->GetBoneIndex(ChildBoneName);
		if (ChildBoneIndex == INDEX_NONE)
		{
			return;
		}

		const FTransform ChildWorldTransform =
			Mesh->GetBodyInstance(ChildBoneName)
				? Mesh->GetBodyInstance(ChildBoneName)->GetUnrealWorldTransform()
				: Mesh->GetBoneTransform(ChildBoneIndex);
		FPhase1ConstraintRotationSample& ConstraintSample = RotationSamples.AddDefaulted_GetRef();
		ConstraintSample.ChildBoneName = ChildBoneName;
		ConstraintSample.CandidateRotation =
			(ChildWorldTransform.GetRotation() *
			 ConstraintInstance->GetRefFrame(EConstraintFrame::Frame1).GetRotation() *
			 ConstraintInstance->GetRefFrame(EConstraintFrame::Frame2).GetRotation().Inverse()).GetNormalized();
		ConstraintSample.ChildConstraintWorldRotation =
			(ChildWorldTransform.GetRotation() * ConstraintInstance->GetRefFrame(EConstraintFrame::Frame1).GetRotation()).GetNormalized();
		ConstraintSample.ParentConstraintLocalRotation = ConstraintInstance->GetRefFrame(EConstraintFrame::Frame2).GetRotation();
		ConstraintSample.ChildAnchorWorld = ChildWorldTransform.TransformPosition(ConstraintInstance->Pos1);
		ConstraintSample.Source = FString::Printf(TEXT("constraint_%s"), *ChildBoneName.ToString());
		ConstraintSample.bValid = true;
		++DesiredPelvisLocationSamples;
	};

	AccumulateConstraintAnchoredCandidate(TEXT("thigh_l"));
	AccumulateConstraintAnchoredCandidate(TEXT("thigh_r"));
	AccumulateConstraintAnchoredCandidate(TEXT("spine_01"));
	{
		TArray<FPhase1ConstraintRotationSample> DirectConstraintSamples;
		DirectConstraintSamples.Reserve(RotationSamples.Num());
		for (const FPhase1ConstraintRotationSample& Sample : RotationSamples)
		{
			if (Sample.bValid && Sample.ChildBoneName != NAME_None)
			{
				DirectConstraintSamples.Add(Sample);
			}
		}

		if (DirectConstraintSamples.Num() >= 3)
		{
			constexpr int32 WeightDenominator = 10;
			const auto AddWeightedDirectBlendSamples = [&](const FQuat& ReferenceRotation, const TCHAR* SourceTag)
			{
				for (int32 WeightA = 1; WeightA < WeightDenominator - 1; ++WeightA)
				{
					for (int32 WeightB = 1; WeightB < (WeightDenominator - WeightA); ++WeightB)
					{
						const int32 WeightC = WeightDenominator - WeightA - WeightB;
						if (WeightC <= 0)
						{
							continue;
						}

						FVector4 WeightedRotationSum = FVector4::Zero();
						const int32 Weights[] = { WeightA, WeightB, WeightC };
						for (int32 SampleIndex = 0; SampleIndex < 3; ++SampleIndex)
						{
							FQuat AlignedRotation = DirectConstraintSamples[SampleIndex].CandidateRotation;
							if ((ReferenceRotation | AlignedRotation) < 0.0f)
							{
								AlignedRotation *= -1.0f;
							}

							const float SampleWeight = static_cast<float>(Weights[SampleIndex]) / static_cast<float>(WeightDenominator);
							WeightedRotationSum.X += AlignedRotation.X * SampleWeight;
							WeightedRotationSum.Y += AlignedRotation.Y * SampleWeight;
							WeightedRotationSum.Z += AlignedRotation.Z * SampleWeight;
							WeightedRotationSum.W += AlignedRotation.W * SampleWeight;
						}

						FQuat WeightedRotation(WeightedRotationSum.X, WeightedRotationSum.Y, WeightedRotationSum.Z, WeightedRotationSum.W);
						if (WeightedRotation.SizeSquared() <= KINDA_SMALL_NUMBER)
						{
							continue;
						}

						WeightedRotation.Normalize();
						if ((DesiredPelvisRotation | WeightedRotation) < 0.0f)
						{
							WeightedRotation *= -1.0f;
						}

						FPhase1ConstraintRotationSample& WeightedBlendSample = RotationSamples.AddDefaulted_GetRef();
						WeightedBlendSample.CandidateRotation = WeightedRotation;
						WeightedBlendSample.Source = FString::Printf(
							TEXT("%s_%s_%.2f_%s_%.2f_%s_%.2f"),
							SourceTag,
							*DirectConstraintSamples[0].ChildBoneName.ToString(),
							static_cast<float>(WeightA) / static_cast<float>(WeightDenominator),
							*DirectConstraintSamples[1].ChildBoneName.ToString(),
							static_cast<float>(WeightB) / static_cast<float>(WeightDenominator),
							*DirectConstraintSamples[2].ChildBoneName.ToString(),
							static_cast<float>(WeightC) / static_cast<float>(WeightDenominator));
						WeightedBlendSample.bValid = true;
					}
				}
			};

			const FQuat PrimaryReferenceRotation = DirectConstraintSamples[0].CandidateRotation;
			AddWeightedDirectBlendSamples(PrimaryReferenceRotation, TEXT("blend_weighted_direct"));

			auto AddSpineBiasedDirectBlendSamples = [&](const int32 FineWeightDenominator, const FQuat& ReferenceRotation, const TCHAR* SourceTag)
			{
				for (int32 SpineWeight = FineWeightDenominator - 1; SpineWeight >= FineWeightDenominator / 2; --SpineWeight)
				{
					const int32 RemainingWeight = FineWeightDenominator - SpineWeight;
					if (RemainingWeight < 2)
					{
						continue;
					}

					for (int32 LeftWeight = 1; LeftWeight < RemainingWeight; ++LeftWeight)
					{
						const int32 RightWeight = RemainingWeight - LeftWeight;
						if (RightWeight <= 0)
						{
							continue;
						}

						FVector4 WeightedRotationSum = FVector4::Zero();
						const int32 Weights[] = { LeftWeight, RightWeight, SpineWeight };
						for (int32 SampleIndex = 0; SampleIndex < 3; ++SampleIndex)
						{
							FQuat AlignedRotation = DirectConstraintSamples[SampleIndex].CandidateRotation;
							if ((ReferenceRotation | AlignedRotation) < 0.0f)
							{
								AlignedRotation *= -1.0f;
							}

							const float SampleWeight = static_cast<float>(Weights[SampleIndex]) / static_cast<float>(FineWeightDenominator);
							WeightedRotationSum.X += AlignedRotation.X * SampleWeight;
							WeightedRotationSum.Y += AlignedRotation.Y * SampleWeight;
							WeightedRotationSum.Z += AlignedRotation.Z * SampleWeight;
							WeightedRotationSum.W += AlignedRotation.W * SampleWeight;
						}

						FQuat WeightedRotation(WeightedRotationSum.X, WeightedRotationSum.Y, WeightedRotationSum.Z, WeightedRotationSum.W);
						if (WeightedRotation.SizeSquared() <= KINDA_SMALL_NUMBER)
						{
							continue;
						}

						WeightedRotation.Normalize();
						if ((DesiredPelvisRotation | WeightedRotation) < 0.0f)
						{
							WeightedRotation *= -1.0f;
						}

						FPhase1ConstraintRotationSample& WeightedBlendSample = RotationSamples.AddDefaulted_GetRef();
						WeightedBlendSample.CandidateRotation = WeightedRotation;
						WeightedBlendSample.Source = FString::Printf(
							TEXT("%s_%s_%.2f_%s_%.2f_%s_%.2f"),
							SourceTag,
							*DirectConstraintSamples[0].ChildBoneName.ToString(),
							static_cast<float>(LeftWeight) / static_cast<float>(FineWeightDenominator),
							*DirectConstraintSamples[1].ChildBoneName.ToString(),
							static_cast<float>(RightWeight) / static_cast<float>(FineWeightDenominator),
							*DirectConstraintSamples[2].ChildBoneName.ToString(),
							static_cast<float>(SpineWeight) / static_cast<float>(FineWeightDenominator));
						WeightedBlendSample.bValid = true;
					}
				}
			};

			const FPhase1ConstraintRotationSample* LeftConstraintSample = nullptr;
			const FPhase1ConstraintRotationSample* RightConstraintSample = nullptr;
			const FPhase1ConstraintRotationSample* SpineConstraintSample = nullptr;
			for (const FPhase1ConstraintRotationSample& Sample : DirectConstraintSamples)
			{
				if (Sample.ChildBoneName == TEXT("thigh_l"))
				{
					LeftConstraintSample = &Sample;
				}
				else if (Sample.ChildBoneName == TEXT("thigh_r"))
				{
					RightConstraintSample = &Sample;
				}
				else if (Sample.ChildBoneName == TEXT("spine_01"))
				{
					SpineConstraintSample = &Sample;
				}
			}
			if (LeftConstraintSample && RightConstraintSample && SpineConstraintSample)
			{
				const auto AddAutoCalibBiasSeed = [&](const FQuat& SeedRotation, const TCHAR* SourceTag)
				{
					if (FMath::IsNearlyZero(SearchConfig.PelvisPitchBiasDeg, KINDA_SMALL_NUMBER) &&
						FMath::IsNearlyZero(SearchConfig.PelvisRollBiasDeg, KINDA_SMALL_NUMBER))
					{
						return;
					}

					const FQuat PitchDelta(FVector::RightVector, FMath::DegreesToRadians(SearchConfig.PelvisPitchBiasDeg));
					const FQuat RollDelta(FVector::ForwardVector, FMath::DegreesToRadians(SearchConfig.PelvisRollBiasDeg));
					FPhase1ConstraintRotationSample& BiasSample = RotationSamples.AddDefaulted_GetRef();
					BiasSample.CandidateRotation = (RollDelta * PitchDelta * SeedRotation).GetNormalized();
					BiasSample.Source = FString::Printf(TEXT("%s_pitch%.2f_roll%.2f"), SourceTag, SearchConfig.PelvisPitchBiasDeg, SearchConfig.PelvisRollBiasDeg);
					BiasSample.bValid = true;
				};
				const auto AddAutoCalibPresetSeedFamily = [&](const EPhase1AutoCalibStrategyPreset Preset, const TCHAR* FamilyTag)
				{
					const FPhase1ConstraintRotationSample* const WorstThighSample =
						(FMath::RadiansToDegrees(AnimatedPelvisTransform.GetRotation().AngularDistance(LeftConstraintSample->CandidateRotation)) >=
						 FMath::RadiansToDegrees(AnimatedPelvisTransform.GetRotation().AngularDistance(RightConstraintSample->CandidateRotation)))
							? LeftConstraintSample
							: RightConstraintSample;

					switch (Preset)
					{
					case EPhase1AutoCalibStrategyPreset::SpineBiased:
					{
						FPhase1ConstraintRotationSample& Sample = RotationSamples.AddDefaulted_GetRef();
						Sample.CandidateRotation = FQuat::Slerp(AnimatedPelvisTransform.GetRotation(), SpineConstraintSample->CandidateRotation, AutoCalibSpineInterpolationAlpha).GetNormalized();
						Sample.Source = FString::Printf(TEXT("autocalib_%s_spine_a%.2f"), FamilyTag, AutoCalibSpineInterpolationAlpha);
						Sample.bValid = true;
						AddAutoCalibBiasSeed(Sample.CandidateRotation, *Sample.Source);
						break;
					}
					case EPhase1AutoCalibStrategyPreset::WorstThighBiased:
					{
						FPhase1ConstraintRotationSample& Sample = RotationSamples.AddDefaulted_GetRef();
						Sample.CandidateRotation = FQuat::Slerp(AnimatedPelvisTransform.GetRotation(), WorstThighSample->CandidateRotation, AutoCalibWorstThighInterpolationAlpha).GetNormalized();
						Sample.Source = FString::Printf(TEXT("autocalib_%s_%s_a%.2f"), FamilyTag, *WorstThighSample->ChildBoneName.ToString(), AutoCalibWorstThighInterpolationAlpha);
						Sample.bValid = true;
						AddAutoCalibBiasSeed(Sample.CandidateRotation, *Sample.Source);
						break;
					}
					case EPhase1AutoCalibStrategyPreset::BalancedCoupled:
					{
						FQuat BalancedRotation = FQuat::Slerp(LeftConstraintSample->CandidateRotation, RightConstraintSample->CandidateRotation, 0.5f).GetNormalized();
						BalancedRotation = FQuat::Slerp(BalancedRotation, SpineConstraintSample->CandidateRotation, 0.5f).GetNormalized();
						FPhase1ConstraintRotationSample& Sample = RotationSamples.AddDefaulted_GetRef();
						Sample.CandidateRotation = BalancedRotation;
						Sample.Source = FString::Printf(TEXT("autocalib_%s_balanced"), FamilyTag);
						Sample.bValid = true;
						AddAutoCalibBiasSeed(Sample.CandidateRotation, *Sample.Source);
						break;
					}
					case EPhase1AutoCalibStrategyPreset::SpineThenWorstThigh:
					{
						const FQuat SpineRotation = FQuat::Slerp(AnimatedPelvisTransform.GetRotation(), SpineConstraintSample->CandidateRotation, AutoCalibSpineInterpolationAlpha).GetNormalized();
						FPhase1ConstraintRotationSample& Sample = RotationSamples.AddDefaulted_GetRef();
						Sample.CandidateRotation = FQuat::Slerp(SpineRotation, WorstThighSample->CandidateRotation, AutoCalibWorstThighInterpolationAlpha).GetNormalized();
						Sample.Source = FString::Printf(TEXT("autocalib_%s_spine_then_%s"), FamilyTag, *WorstThighSample->ChildBoneName.ToString());
						Sample.bValid = true;
						AddAutoCalibBiasSeed(Sample.CandidateRotation, *Sample.Source);
						break;
					}
					case EPhase1AutoCalibStrategyPreset::RescueOnly:
						AddAutoCalibBiasSeed(PelvisBody->GetUnrealWorldTransform().GetRotation(), TEXT("autocalib_rescue_live"));
						AddAutoCalibBiasSeed(AnimatedPelvisTransform.GetRotation(), TEXT("autocalib_rescue_animated"));
						break;
					case EPhase1AutoCalibStrategyPreset::CoupledTradeControlFamily:
					{
						const FQuat SpineRotation = FQuat::Slerp(AnimatedPelvisTransform.GetRotation(), SpineConstraintSample->CandidateRotation, AutoCalibSpineInterpolationAlpha).GetNormalized();
						FPhase1ConstraintRotationSample& Sample = RotationSamples.AddDefaulted_GetRef();
						Sample.CandidateRotation = FQuat::Slerp(SpineRotation, WorstThighSample->CandidateRotation, AutoCalibWorstThighInterpolationAlpha).GetNormalized();
						Sample.Source = FString::Printf(TEXT("autocalib_%s_coupled_trade_seed_%s"), FamilyTag, *WorstThighSample->ChildBoneName.ToString());
						Sample.bValid = true;
						AddAutoCalibBiasSeed(Sample.CandidateRotation, *Sample.Source);
						break;
					}
					case EPhase1AutoCalibStrategyPreset::PairBlendFrontierFollowThrough:
						AddAutoCalibBiasSeed(PelvisBody->GetUnrealWorldTransform().GetRotation(), TEXT("autocalib_pair_frontier_live"));
						AddAutoCalibBiasSeed(AnimatedPelvisTransform.GetRotation(), TEXT("autocalib_pair_frontier_animated"));
						break;
					case EPhase1AutoCalibStrategyPreset::CurrentDefault:
					default:
						AddAutoCalibBiasSeed(AnimatedPelvisTransform.GetRotation(), TEXT("autocalib_default"));
						break;
					}
				};
				if (AutoCalibParams)
				{
					AddAutoCalibPresetSeedFamily(SearchConfig.SourcePreset, TEXT("source"));
					AddAutoCalibPresetSeedFamily(SearchConfig.SeedFamilyPreset, TEXT("seed"));
				}

				const float ReferenceLeftAngularErrorDeg = 0.0f;
				const float ReferenceRightAngularErrorDeg =
					FMath::RadiansToDegrees(PrimaryReferenceRotation.AngularDistance(RightConstraintSample->CandidateRotation));
				const float ReferenceSpineAngularErrorDeg =
					FMath::RadiansToDegrees(PrimaryReferenceRotation.AngularDistance(SpineConstraintSample->CandidateRotation));
				if (SearchConfig.bEnableSpineBiasedDirectBlendSeeds &&
					ShouldRunSpineBiasedDirectConstraintBlendSweep(
						ReferenceLeftAngularErrorDeg,
						ReferenceRightAngularErrorDeg,
						ReferenceSpineAngularErrorDeg))
				{
					bRanSpineBiasedDirectBlend = true;
					AddSpineBiasedDirectBlendSamples(20, PrimaryReferenceRotation, TEXT("blend_spine_bias"));
					if (ShouldRunAlternateReferenceDirectConstraintBlendSweep(
							ReferenceLeftAngularErrorDeg,
							ReferenceRightAngularErrorDeg,
							ReferenceSpineAngularErrorDeg))
					{
						AddWeightedDirectBlendSamples(RightConstraintSample->CandidateRotation, TEXT("blend_weighted_direct_alt_ref_thigh_r"));
						AddWeightedDirectBlendSamples(SpineConstraintSample->CandidateRotation, TEXT("blend_weighted_direct_alt_ref_spine"));
						AddSpineBiasedDirectBlendSamples(20, RightConstraintSample->CandidateRotation, TEXT("blend_spine_bias_alt_ref_thigh_r"));
						AddSpineBiasedDirectBlendSamples(20, SpineConstraintSample->CandidateRotation, TEXT("blend_spine_bias_alt_ref_spine"));
					}
				}
			}
		}
	}
	TArray<FPhase1ConstraintRotationSample> ValidSeedSamples;
	ValidSeedSamples.Reserve(RotationSamples.Num());
	for (const FPhase1ConstraintRotationSample& Sample : RotationSamples)
	{
		if (Sample.bValid)
		{
			ValidSeedSamples.Add(Sample);
		}
	}
	TArray<FPhase1ConstraintRotationSample> ValidConstraintSamples;
	ValidConstraintSamples.Reserve(RotationSamples.Num());
	for (const FPhase1ConstraintRotationSample& Sample : RotationSamples)
	{
		if (Sample.bValid && Sample.ChildBoneName != NAME_None)
		{
			ValidConstraintSamples.Add(Sample);
		}
	}
	if (SearchConfig.bEnablePairBlendSeeds)
	{
		bRanPairBlendSeeds = true;
		for (int32 SampleIndex = 0; SampleIndex < ValidSeedSamples.Num(); ++SampleIndex)
		{
			const FPhase1ConstraintRotationSample& SampleA = ValidSeedSamples[SampleIndex];
			for (int32 OtherIndex = SampleIndex + 1; OtherIndex < ValidSeedSamples.Num(); ++OtherIndex)
			{
				const FPhase1ConstraintRotationSample& SampleB = ValidSeedSamples[OtherIndex];
				static const float BlendWeights[] = { 0.10f, 0.25f, 0.33f, 0.50f, 0.67f, 0.75f, 0.90f };
				for (const float BlendWeight : BlendWeights)
				{
					FQuat BlendedRotation = FQuat::Slerp(SampleA.CandidateRotation, SampleB.CandidateRotation, BlendWeight).GetNormalized();
					if ((DesiredPelvisRotation | BlendedRotation) < 0.0f)
					{
						BlendedRotation *= -1.0f;
					}

					FPhase1ConstraintRotationSample& BlendSample = RotationSamples.AddDefaulted_GetRef();
					BlendSample.CandidateRotation = BlendedRotation;
					BlendSample.Source = FString::Printf(TEXT("blend_%s_%s_%.2f"), *SampleA.ChildBoneName.ToString(), *SampleB.ChildBoneName.ToString(), BlendWeight);
					BlendSample.bValid = true;
				}

				const bool bSpinePair =
					SampleA.ChildBoneName == TEXT("spine_01") ||
					SampleB.ChildBoneName == TEXT("spine_01");
				if (bSpinePair)
				{
					static const float FineBlendWeights[] = { 0.02f, 0.05f, 0.08f, 0.92f, 0.95f, 0.98f };
					for (const float BlendWeight : FineBlendWeights)
					{
						FQuat BlendedRotation = FQuat::Slerp(SampleA.CandidateRotation, SampleB.CandidateRotation, BlendWeight).GetNormalized();
						if ((DesiredPelvisRotation | BlendedRotation) < 0.0f)
						{
							BlendedRotation *= -1.0f;
						}

						FPhase1ConstraintRotationSample& BlendSample = RotationSamples.AddDefaulted_GetRef();
						BlendSample.CandidateRotation = BlendedRotation;
						BlendSample.Source = FString::Printf(TEXT("blend_spine_pair_%s_%s_%.2f"), *SampleA.ChildBoneName.ToString(), *SampleB.ChildBoneName.ToString(), BlendWeight);
						BlendSample.bValid = true;
					}
				}
			}
		}
	}
	if (ValidConstraintSamples.Num() >= 3)
	{
		FVector4 WeightedRotationSum = FVector4::Zero();
		const FQuat ReferenceRotation = ValidConstraintSamples[0].CandidateRotation;
		for (const FPhase1ConstraintRotationSample& Sample : ValidConstraintSamples)
		{
			FQuat AlignedRotation = Sample.CandidateRotation;
			if ((ReferenceRotation | AlignedRotation) < 0.0f)
			{
				AlignedRotation *= -1.0f;
			}

			WeightedRotationSum.X += AlignedRotation.X;
			WeightedRotationSum.Y += AlignedRotation.Y;
			WeightedRotationSum.Z += AlignedRotation.Z;
			WeightedRotationSum.W += AlignedRotation.W;
		}

		FQuat AveragedRotation(WeightedRotationSum.X, WeightedRotationSum.Y, WeightedRotationSum.Z, WeightedRotationSum.W);
		if (AveragedRotation.SizeSquared() > KINDA_SMALL_NUMBER)
		{
			AveragedRotation.Normalize();
		}
		else
		{
			AveragedRotation = ReferenceRotation;
		}
		if ((DesiredPelvisRotation | AveragedRotation) < 0.0f)
		{
			AveragedRotation *= -1.0f;
		}

		FPhase1ConstraintRotationSample& AverageSample = RotationSamples.AddDefaulted_GetRef();
		AverageSample.CandidateRotation = AveragedRotation;
		AverageSample.Source = TEXT("blend_all_direct");
		AverageSample.bValid = true;
	}

	if (DesiredPelvisLocationSamples == 0)
	{
		if (!bPhase1PelvisCouplingSkipLogged)
		{
			PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimBalance] PHASE1_PELVIS_COUPLING_SKIPPED reason=noChildCandidates state=%s"), GetRuntimeStateName(RuntimeState));
			bPhase1PelvisCouplingSkipLogged = true;
		}
		return;
	}

	const auto EvaluateCandidateRotation = [&](
		const FQuat& CandidateRotation,
		float& OutMaxAngularErrorDeg,
		float& OutMeanAngularErrorDeg,
		float& OutMaxThighAngularErrorDeg,
		float& OutSpineAngularErrorDeg,
		float& OutLeftThighAngularErrorDeg,
		float& OutRightThighAngularErrorDeg)
	{
		OutMaxAngularErrorDeg = 0.0f;
		OutMeanAngularErrorDeg = 0.0f;
		OutMaxThighAngularErrorDeg = 0.0f;
		OutSpineAngularErrorDeg = TNumericLimits<float>::Max();
		OutLeftThighAngularErrorDeg = TNumericLimits<float>::Max();
		OutRightThighAngularErrorDeg = TNumericLimits<float>::Max();
		int32 ValidConstraintCount = 0;
		for (const FPhase1ConstraintRotationSample& Sample : RotationSamples)
		{
			if (!Sample.bValid || Sample.ParentConstraintLocalRotation.Equals(FQuat::Identity) && Sample.ChildConstraintWorldRotation.Equals(FQuat::Identity))
			{
				continue;
			}

			const FQuat ParentConstraintWorldRotation = (CandidateRotation * Sample.ParentConstraintLocalRotation).GetNormalized();
			const float AngularErrorDeg =
				FMath::RadiansToDegrees(ParentConstraintWorldRotation.AngularDistance(Sample.ChildConstraintWorldRotation));
			OutMaxAngularErrorDeg = FMath::Max(OutMaxAngularErrorDeg, AngularErrorDeg);
			OutMeanAngularErrorDeg += AngularErrorDeg;
			if (Sample.ChildBoneName == TEXT("thigh_l") || Sample.ChildBoneName == TEXT("thigh_r"))
			{
				OutMaxThighAngularErrorDeg = FMath::Max(OutMaxThighAngularErrorDeg, AngularErrorDeg);
				if (Sample.ChildBoneName == TEXT("thigh_l"))
				{
					OutLeftThighAngularErrorDeg = AngularErrorDeg;
				}
				else
				{
					OutRightThighAngularErrorDeg = AngularErrorDeg;
				}
			}
			else if (Sample.ChildBoneName == TEXT("spine_01"))
			{
				OutSpineAngularErrorDeg = AngularErrorDeg;
			}
			++ValidConstraintCount;
		}

		if (ValidConstraintCount > 0)
		{
			OutMeanAngularErrorDeg /= static_cast<float>(ValidConstraintCount);
		}
		if (OutSpineAngularErrorDeg == TNumericLimits<float>::Max())
		{
			OutSpineAngularErrorDeg = 0.0f;
		}
		if (OutLeftThighAngularErrorDeg == TNumericLimits<float>::Max())
		{
			OutLeftThighAngularErrorDeg = 0.0f;
		}
		if (OutRightThighAngularErrorDeg == TNumericLimits<float>::Max())
		{
			OutRightThighAngularErrorDeg = 0.0f;
		}
	};
	const auto EvaluateCandidateTiltDeg = [](const FQuat& CandidateRotation)
	{
		const FVector AxisX = CandidateRotation.GetAxisX();
		const FVector AxisY = CandidateRotation.GetAxisY();
		const FVector AxisZ = CandidateRotation.GetAxisZ();

		const float DotX = FMath::Abs(FVector::DotProduct(AxisX, FVector::UpVector));
		const float DotY = FMath::Abs(FVector::DotProduct(AxisY, FVector::UpVector));
		const float DotZ = FMath::Abs(FVector::DotProduct(AxisZ, FVector::UpVector));

		FVector CandidateUp = AxisZ;
		if (DotX > DotY && DotX > DotZ)
		{
			CandidateUp = AxisX;
		}
		else if (DotY > DotX && DotY > DotZ)
		{
			CandidateUp = AxisY;
		}

		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(CandidateUp, FVector::UpVector), -1.0f, 1.0f)));
	};
	struct FPhase1PelvisRotationEvaluation
	{
		FQuat Rotation = FQuat::Identity;
		FString Source;
		float MaxAngularErrorDeg = TNumericLimits<float>::Max();
		float MeanAngularErrorDeg = TNumericLimits<float>::Max();
		float MaxThighAngularErrorDeg = TNumericLimits<float>::Max();
		float SpineAngularErrorDeg = TNumericLimits<float>::Max();
		float LeftThighAngularErrorDeg = TNumericLimits<float>::Max();
		float RightThighAngularErrorDeg = TNumericLimits<float>::Max();
		float AngularThresholdOverflowDeg = TNumericLimits<float>::Max();
		float RootOnReadinessMinMarginDeg = -TNumericLimits<float>::Max();
		float RootOnReadinessTotalDeficitDeg = TNumericLimits<float>::Max();
		float TiltDeg = TNumericLimits<float>::Max();
		bool bRootOnAngularReady = false;
		bool bRootOnReadinessMarginSatisfied = false;
		bool bTiltAdmissible = false;
	};
	const auto BuildRotationEvaluation = [&](const FQuat& CandidateRotation, const FString& Source)
	{
		FPhase1PelvisRotationEvaluation Evaluation;
		Evaluation.Rotation = CandidateRotation.GetNormalized();
		Evaluation.Source = Source;
		EvaluateCandidateRotation(
			Evaluation.Rotation,
			Evaluation.MaxAngularErrorDeg,
			Evaluation.MeanAngularErrorDeg,
			Evaluation.MaxThighAngularErrorDeg,
			Evaluation.SpineAngularErrorDeg,
			Evaluation.LeftThighAngularErrorDeg,
			Evaluation.RightThighAngularErrorDeg);
		const float LeftThighOverflowDeg = FMath::Max(
			0.0f,
			Evaluation.LeftThighAngularErrorDeg - BalanceTransitionSets::Phase2MaxPelvisThighDirectLinkAngularErrorDeg);
		const float RightThighOverflowDeg = FMath::Max(
			0.0f,
			Evaluation.RightThighAngularErrorDeg - BalanceTransitionSets::Phase2MaxPelvisThighDirectLinkAngularErrorDeg);
		const float SpineOverflowDeg = FMath::Max(
			0.0f,
			Evaluation.SpineAngularErrorDeg - BalanceTransitionSets::Phase2MaxPelvisSpineDirectLinkAngularErrorDeg);
		Evaluation.AngularThresholdOverflowDeg = FMath::Max3(LeftThighOverflowDeg, RightThighOverflowDeg, SpineOverflowDeg);
		Evaluation.bRootOnAngularReady = Evaluation.AngularThresholdOverflowDeg <= KINDA_SMALL_NUMBER;
		const float LeftThighReadinessMarginDeg =
			BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg - Evaluation.LeftThighAngularErrorDeg;
		const float RightThighReadinessMarginDeg =
			BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg - Evaluation.RightThighAngularErrorDeg;
		const float SpineReadinessMarginDeg =
			BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisSpineDirectLinkAngularErrorDeg - Evaluation.SpineAngularErrorDeg;
		Evaluation.RootOnReadinessMinMarginDeg =
			FMath::Min3(LeftThighReadinessMarginDeg, RightThighReadinessMarginDeg, SpineReadinessMarginDeg);
		Evaluation.RootOnReadinessTotalDeficitDeg =
			FMath::Max(0.0f, -LeftThighReadinessMarginDeg) +
			FMath::Max(0.0f, -RightThighReadinessMarginDeg) +
			FMath::Max(0.0f, -SpineReadinessMarginDeg);
		Evaluation.bRootOnReadinessMarginSatisfied = Evaluation.RootOnReadinessMinMarginDeg >= -KINDA_SMALL_NUMBER;
		Evaluation.TiltDeg = EvaluateCandidateTiltDeg(Evaluation.Rotation);
		Evaluation.bTiltAdmissible = Evaluation.TiltDeg <= EffectiveSettings.BalancePhase2EntryMaxRootTiltDeg + KINDA_SMALL_NUMBER;
		return Evaluation;
	};
	const auto IsBetterRotationEvaluation = [bAutoCalibPreferUprightnessEarly](const FPhase1PelvisRotationEvaluation& Candidate, const FPhase1PelvisRotationEvaluation& CurrentBest)
	{
		const auto PreferByTilt = [](const FPhase1PelvisRotationEvaluation& A, const FPhase1PelvisRotationEvaluation& B)
		{
			return A.TiltDeg + KINDA_SMALL_NUMBER < B.TiltDeg;
		};

		if (Candidate.AngularThresholdOverflowDeg + KINDA_SMALL_NUMBER < CurrentBest.AngularThresholdOverflowDeg)
		{
			return true;
		}
		if (FMath::IsNearlyEqual(Candidate.AngularThresholdOverflowDeg, CurrentBest.AngularThresholdOverflowDeg, KINDA_SMALL_NUMBER))
		{
			if (Candidate.bRootOnReadinessMarginSatisfied != CurrentBest.bRootOnReadinessMarginSatisfied)
			{
				return Candidate.bRootOnReadinessMarginSatisfied;
			}
			if (Candidate.RootOnReadinessTotalDeficitDeg + KINDA_SMALL_NUMBER < CurrentBest.RootOnReadinessTotalDeficitDeg)
			{
				return true;
			}
			if (FMath::IsNearlyEqual(Candidate.RootOnReadinessTotalDeficitDeg, CurrentBest.RootOnReadinessTotalDeficitDeg, KINDA_SMALL_NUMBER) &&
				Candidate.RootOnReadinessMinMarginDeg > CurrentBest.RootOnReadinessMinMarginDeg + KINDA_SMALL_NUMBER)
			{
				return true;
			}
			if (FMath::IsNearlyEqual(Candidate.RootOnReadinessTotalDeficitDeg, CurrentBest.RootOnReadinessTotalDeficitDeg, KINDA_SMALL_NUMBER) &&
				FMath::IsNearlyEqual(Candidate.RootOnReadinessMinMarginDeg, CurrentBest.RootOnReadinessMinMarginDeg, KINDA_SMALL_NUMBER) &&
				Candidate.bRootOnReadinessMarginSatisfied != CurrentBest.bRootOnReadinessMarginSatisfied)
			{
				return Candidate.bRootOnReadinessMarginSatisfied;
			}

			if (Candidate.bRootOnAngularReady != CurrentBest.bRootOnAngularReady)
			{
				return Candidate.bRootOnAngularReady;
			}

			if (Candidate.bRootOnAngularReady)
			{
				if (Candidate.bTiltAdmissible != CurrentBest.bTiltAdmissible)
				{
					return Candidate.bTiltAdmissible;
				}
				if (bAutoCalibPreferUprightnessEarly && PreferByTilt(Candidate, CurrentBest))
				{
					return true;
				}
			}
			else if (Candidate.bTiltAdmissible != CurrentBest.bTiltAdmissible)
			{
				return Candidate.bTiltAdmissible;
			}

			if (Candidate.MaxThighAngularErrorDeg + KINDA_SMALL_NUMBER < CurrentBest.MaxThighAngularErrorDeg)
			{
				return true;
			}
			if (!bAutoCalibPreferUprightnessEarly &&
				FMath::IsNearlyEqual(Candidate.MaxThighAngularErrorDeg, CurrentBest.MaxThighAngularErrorDeg, KINDA_SMALL_NUMBER) &&
				PreferByTilt(Candidate, CurrentBest))
			{
				return true;
			}
			if (FMath::IsNearlyEqual(Candidate.MaxThighAngularErrorDeg, CurrentBest.MaxThighAngularErrorDeg, KINDA_SMALL_NUMBER) &&
				Candidate.SpineAngularErrorDeg + KINDA_SMALL_NUMBER < CurrentBest.SpineAngularErrorDeg)
			{
				return true;
			}
			if (FMath::IsNearlyEqual(Candidate.MaxThighAngularErrorDeg, CurrentBest.MaxThighAngularErrorDeg, KINDA_SMALL_NUMBER) &&
				FMath::IsNearlyEqual(Candidate.SpineAngularErrorDeg, CurrentBest.SpineAngularErrorDeg, KINDA_SMALL_NUMBER) &&
				Candidate.MaxAngularErrorDeg + KINDA_SMALL_NUMBER < CurrentBest.MaxAngularErrorDeg)
			{
				return true;
			}
			if (FMath::IsNearlyEqual(Candidate.MaxThighAngularErrorDeg, CurrentBest.MaxThighAngularErrorDeg, KINDA_SMALL_NUMBER) &&
				FMath::IsNearlyEqual(Candidate.SpineAngularErrorDeg, CurrentBest.SpineAngularErrorDeg, KINDA_SMALL_NUMBER) &&
				FMath::IsNearlyEqual(Candidate.MaxAngularErrorDeg, CurrentBest.MaxAngularErrorDeg, KINDA_SMALL_NUMBER) &&
				Candidate.MeanAngularErrorDeg + KINDA_SMALL_NUMBER < CurrentBest.MeanAngularErrorDeg)
			{
				return true;
			}
		}
		return false;
	};
	const auto RefineByConstraintCorrection = [&](
		const FPhase1PelvisRotationEvaluation& SeedEvaluation,
		const TCHAR* ContextTag,
		const bool bRequireTiltAdmissible)
	{
		FPhase1PelvisRotationEvaluation BestEvaluation = SeedEvaluation;
		if (ValidConstraintSamples.IsEmpty())
		{
			return BestEvaluation;
		}

		static const float CorrectionBlendAlphas[] = { 1.00f, 0.50f, 0.25f };
		for (const float CorrectionBlendAlpha : CorrectionBlendAlphas)
		{
			FQuat WorkingRotation = SeedEvaluation.Rotation;
			for (int32 PassIndex = 0; PassIndex < 8; ++PassIndex)
			{
				FVector4 WeightedDeltaSum = FVector4::Zero();
				float TotalWeight = 0.0f;
				for (const FPhase1ConstraintRotationSample& ConstraintSample : ValidConstraintSamples)
				{
					const FQuat ParentConstraintWorldRotation =
						(WorkingRotation * ConstraintSample.ParentConstraintLocalRotation).GetNormalized();
					FQuat DeltaRotation =
						(ConstraintSample.ChildConstraintWorldRotation * ParentConstraintWorldRotation.Inverse()).GetNormalized();
					if ((FQuat::Identity | DeltaRotation) < 0.0f)
					{
						DeltaRotation *= -1.0f;
					}

					const float SampleWeight =
						(ConstraintSample.ChildBoneName == TEXT("thigh_l") || ConstraintSample.ChildBoneName == TEXT("thigh_r"))
							? 2.0f
							: 1.0f;
					WeightedDeltaSum.X += DeltaRotation.X * SampleWeight;
					WeightedDeltaSum.Y += DeltaRotation.Y * SampleWeight;
					WeightedDeltaSum.Z += DeltaRotation.Z * SampleWeight;
					WeightedDeltaSum.W += DeltaRotation.W * SampleWeight;
					TotalWeight += SampleWeight;
				}

				if (TotalWeight <= KINDA_SMALL_NUMBER)
				{
					break;
				}

				FQuat AveragedDelta(
					WeightedDeltaSum.X / TotalWeight,
					WeightedDeltaSum.Y / TotalWeight,
					WeightedDeltaSum.Z / TotalWeight,
					WeightedDeltaSum.W / TotalWeight);
				if (AveragedDelta.SizeSquared() <= KINDA_SMALL_NUMBER)
				{
					break;
				}

				AveragedDelta.Normalize();
				const FQuat AppliedDelta = FQuat::Slerp(FQuat::Identity, AveragedDelta, CorrectionBlendAlpha).GetNormalized();
				const FQuat CandidateRotation = (AppliedDelta * WorkingRotation).GetNormalized();
				const FPhase1PelvisRotationEvaluation CandidateEvaluation = BuildRotationEvaluation(
					CandidateRotation,
					FString::Printf(TEXT("%s_%s_p%d_a%.2f"),
						*BestEvaluation.Source,
						ContextTag,
						PassIndex,
						CorrectionBlendAlpha));
				WorkingRotation = CandidateRotation;
				if (bRequireTiltAdmissible && !CandidateEvaluation.bTiltAdmissible)
				{
					continue;
				}
				if (IsBetterRotationEvaluation(CandidateEvaluation, BestEvaluation))
				{
					BestEvaluation = CandidateEvaluation;
				}
			}
		}

		return BestEvaluation;
	};
	const auto RefineByWorstConstraintCorrection = [&](
		const FPhase1PelvisRotationEvaluation& SeedEvaluation,
		const TCHAR* ContextTag,
		const bool bRequireTiltAdmissible)
	{
		FPhase1PelvisRotationEvaluation BestEvaluation = SeedEvaluation;
		if (ValidConstraintSamples.IsEmpty())
		{
			return BestEvaluation;
		}

		static const float CorrectionBlendAlphas[] = { 1.00f, 0.50f, 0.25f, 0.10f, 0.05f, 0.02f, 0.01f, 0.005f };
		for (const float CorrectionBlendAlpha : CorrectionBlendAlphas)
		{
			FQuat WorkingRotation = SeedEvaluation.Rotation;
			FPhase1PelvisRotationEvaluation WorkingEvaluation = SeedEvaluation;
			for (int32 PassIndex = 0; PassIndex < 10; ++PassIndex)
			{
				FName FocusChildBone = TEXT("thigh_r");
				float FocusMarginDeg =
					BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg - WorkingEvaluation.RightThighAngularErrorDeg;
				const float LeftThighMarginDeg =
					BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg - WorkingEvaluation.LeftThighAngularErrorDeg;
				const float SpineMarginDeg =
					BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisSpineDirectLinkAngularErrorDeg - WorkingEvaluation.SpineAngularErrorDeg;
				if (LeftThighMarginDeg + KINDA_SMALL_NUMBER < FocusMarginDeg)
				{
					FocusChildBone = TEXT("thigh_l");
					FocusMarginDeg = LeftThighMarginDeg;
				}
				if (SpineMarginDeg + KINDA_SMALL_NUMBER < FocusMarginDeg)
				{
					FocusChildBone = TEXT("spine_01");
				}

				FVector4 WeightedDeltaSum = FVector4::Zero();
				float TotalWeight = 0.0f;
				for (const FPhase1ConstraintRotationSample& ConstraintSample : ValidConstraintSamples)
				{
					const FQuat ParentConstraintWorldRotation =
						(WorkingRotation * ConstraintSample.ParentConstraintLocalRotation).GetNormalized();
					FQuat DeltaRotation =
						(ConstraintSample.ChildConstraintWorldRotation * ParentConstraintWorldRotation.Inverse()).GetNormalized();
					if ((FQuat::Identity | DeltaRotation) < 0.0f)
					{
						DeltaRotation *= -1.0f;
					}

					float SampleWeight = 0.5f;
					if (ConstraintSample.ChildBoneName == FocusChildBone)
					{
						SampleWeight = 6.0f;
					}
					else if (ConstraintSample.ChildBoneName == TEXT("thigh_l") || ConstraintSample.ChildBoneName == TEXT("thigh_r"))
					{
						SampleWeight = 1.5f;
					}

					WeightedDeltaSum.X += DeltaRotation.X * SampleWeight;
					WeightedDeltaSum.Y += DeltaRotation.Y * SampleWeight;
					WeightedDeltaSum.Z += DeltaRotation.Z * SampleWeight;
					WeightedDeltaSum.W += DeltaRotation.W * SampleWeight;
					TotalWeight += SampleWeight;
				}

				if (TotalWeight <= KINDA_SMALL_NUMBER)
				{
					break;
				}

				FQuat AveragedDelta(
					WeightedDeltaSum.X / TotalWeight,
					WeightedDeltaSum.Y / TotalWeight,
					WeightedDeltaSum.Z / TotalWeight,
					WeightedDeltaSum.W / TotalWeight);
				if (AveragedDelta.SizeSquared() <= KINDA_SMALL_NUMBER)
				{
					break;
				}

				AveragedDelta.Normalize();
				const FQuat AppliedDelta = FQuat::Slerp(FQuat::Identity, AveragedDelta, CorrectionBlendAlpha).GetNormalized();
				const FQuat CandidateRotation = (AppliedDelta * WorkingRotation).GetNormalized();
				const FPhase1PelvisRotationEvaluation CandidateEvaluation = BuildRotationEvaluation(
					CandidateRotation,
					FString::Printf(TEXT("%s_%s_%s_p%d_a%.2f"),
						*BestEvaluation.Source,
						ContextTag,
						*FocusChildBone.ToString(),
						PassIndex,
						CorrectionBlendAlpha));
				WorkingRotation = CandidateRotation;
				WorkingEvaluation = CandidateEvaluation;
				if (bRequireTiltAdmissible && !CandidateEvaluation.bTiltAdmissible)
				{
					continue;
				}
				if (IsBetterRotationEvaluation(CandidateEvaluation, BestEvaluation))
				{
					BestEvaluation = CandidateEvaluation;
				}
			}
		}

		return BestEvaluation;
	};
	const auto RefineByTiltAdmissibleSweep = [&](
		const FPhase1PelvisRotationEvaluation& SeedEvaluation,
		const TCHAR* ContextTag)
	{
		FPhase1PelvisRotationEvaluation BestEvaluation = SeedEvaluation;
		if (!SeedEvaluation.bTiltAdmissible)
		{
			return BestEvaluation;
		}

		static const float PitchSweepDegrees[] = { -9.0f, -6.0f, -3.0f, 0.0f, 3.0f, 6.0f, 9.0f };
		static const float RollSweepDegrees[] = { -9.0f, -6.0f, -3.0f, 0.0f, 3.0f, 6.0f, 9.0f };
		static const float YawSweepDegrees[] = { -12.0f, -8.0f, -4.0f, 0.0f, 4.0f, 8.0f, 12.0f };
		for (const float PitchDeg : PitchSweepDegrees)
		{
			for (const float RollDeg : RollSweepDegrees)
			{
				for (const float YawDeg : YawSweepDegrees)
				{
					if (FMath::IsNearlyZero(PitchDeg) && FMath::IsNearlyZero(RollDeg) && FMath::IsNearlyZero(YawDeg))
					{
						continue;
					}

					const FQuat PitchDelta(FVector::RightVector, FMath::DegreesToRadians(PitchDeg));
					const FQuat RollDelta(FVector::ForwardVector, FMath::DegreesToRadians(RollDeg));
					const FQuat YawDelta(FVector::UpVector, FMath::DegreesToRadians(YawDeg));
					const FQuat CandidateRotation =
						(YawDelta * RollDelta * PitchDelta * SeedEvaluation.Rotation).GetNormalized();
					const FPhase1PelvisRotationEvaluation CandidateEvaluation = BuildRotationEvaluation(
						CandidateRotation,
						FString::Printf(TEXT("%s_%s_y%.1f_p%.1f_r%.1f"),
							*SeedEvaluation.Source,
							ContextTag,
							YawDeg,
							PitchDeg,
							RollDeg));
					if (!CandidateEvaluation.bTiltAdmissible)
					{
						continue;
					}
					if (IsBetterRotationEvaluation(CandidateEvaluation, BestEvaluation))
					{
						BestEvaluation = CandidateEvaluation;
					}
				}
			}
		}

		return BestEvaluation;
	};
	const auto RefineByLocalMarginSweep = [&](
		const FPhase1PelvisRotationEvaluation& SeedEvaluation,
		const TCHAR* ContextTag,
		const bool bRequireTiltAdmissible)
	{
		FPhase1PelvisRotationEvaluation BestEvaluation = SeedEvaluation;
		if (bRequireTiltAdmissible && !SeedEvaluation.bTiltAdmissible)
		{
			return BestEvaluation;
		}

		static const float PitchSweepDegrees[] = { -2.0f, -1.0f, -0.5f, -0.25f, 0.0f, 0.25f, 0.5f, 1.0f, 2.0f };
		static const float RollSweepDegrees[] = { -2.0f, -1.0f, -0.5f, -0.25f, 0.0f, 0.25f, 0.5f, 1.0f, 2.0f };
		static const float YawSweepDegrees[] = { -2.0f, -1.0f, -0.5f, -0.25f, 0.0f, 0.25f, 0.5f, 1.0f, 2.0f };
		for (const float PitchDeg : PitchSweepDegrees)
		{
			for (const float RollDeg : RollSweepDegrees)
			{
				for (const float YawDeg : YawSweepDegrees)
				{
					if (FMath::IsNearlyZero(PitchDeg) && FMath::IsNearlyZero(RollDeg) && FMath::IsNearlyZero(YawDeg))
					{
						continue;
					}

					const FQuat PitchDelta(FVector::RightVector, FMath::DegreesToRadians(PitchDeg));
					const FQuat RollDelta(FVector::ForwardVector, FMath::DegreesToRadians(RollDeg));
					const FQuat YawDelta(FVector::UpVector, FMath::DegreesToRadians(YawDeg));
					const FQuat CandidateRotation =
						(YawDelta * RollDelta * PitchDelta * SeedEvaluation.Rotation).GetNormalized();
					const FPhase1PelvisRotationEvaluation CandidateEvaluation = BuildRotationEvaluation(
						CandidateRotation,
						FString::Printf(TEXT("%s_%s_y%.1f_p%.1f_r%.1f"),
							*SeedEvaluation.Source,
							ContextTag,
							YawDeg,
							PitchDeg,
							RollDeg));
					if (bRequireTiltAdmissible && !CandidateEvaluation.bTiltAdmissible)
					{
						continue;
					}
					if (ShouldAcceptSpineOnlyRootOnReadinessRescueCandidate(
							BestEvaluation.LeftThighAngularErrorDeg,
							BestEvaluation.RightThighAngularErrorDeg,
							BestEvaluation.SpineAngularErrorDeg,
							CandidateEvaluation.LeftThighAngularErrorDeg,
							CandidateEvaluation.RightThighAngularErrorDeg,
							CandidateEvaluation.SpineAngularErrorDeg) ||
						IsBetterRotationEvaluation(CandidateEvaluation, BestEvaluation))
					{
						BestEvaluation = CandidateEvaluation;
					}
				}
			}
		}

		return BestEvaluation;
	};
	const auto RefineByUltraFineMarginSweep = [&](
		const FPhase1PelvisRotationEvaluation& SeedEvaluation,
		const TCHAR* ContextTag,
		const bool bRequireTiltAdmissible)
	{
		FPhase1PelvisRotationEvaluation BestEvaluation = SeedEvaluation;
		if (bRequireTiltAdmissible && !SeedEvaluation.bTiltAdmissible)
		{
			return BestEvaluation;
		}

		static const float PitchSweepDegrees[] = { -1.00f, -0.50f, -0.20f, -0.10f, -0.05f, 0.0f, 0.05f, 0.10f, 0.20f, 0.50f, 1.00f };
		static const float RollSweepDegrees[] = { -1.00f, -0.50f, -0.20f, -0.10f, -0.05f, 0.0f, 0.05f, 0.10f, 0.20f, 0.50f, 1.00f };
		static const float YawSweepDegrees[] = { -1.00f, -0.50f, -0.20f, -0.10f, -0.05f, 0.0f, 0.05f, 0.10f, 0.20f, 0.50f, 1.00f };
		for (const float PitchDeg : PitchSweepDegrees)
		{
			for (const float RollDeg : RollSweepDegrees)
			{
				for (const float YawDeg : YawSweepDegrees)
				{
					if (FMath::IsNearlyZero(PitchDeg) && FMath::IsNearlyZero(RollDeg) && FMath::IsNearlyZero(YawDeg))
					{
						continue;
					}

					const FQuat PitchDelta(FVector::RightVector, FMath::DegreesToRadians(PitchDeg));
					const FQuat RollDelta(FVector::ForwardVector, FMath::DegreesToRadians(RollDeg));
					const FQuat YawDelta(FVector::UpVector, FMath::DegreesToRadians(YawDeg));
					const FQuat CandidateRotation =
						(YawDelta * RollDelta * PitchDelta * SeedEvaluation.Rotation).GetNormalized();
					const FPhase1PelvisRotationEvaluation CandidateEvaluation = BuildRotationEvaluation(
						CandidateRotation,
						FString::Printf(TEXT("%s_%s_y%.2f_p%.2f_r%.2f"),
							*SeedEvaluation.Source,
							ContextTag,
							YawDeg,
							PitchDeg,
							RollDeg));
					if (bRequireTiltAdmissible && !CandidateEvaluation.bTiltAdmissible)
					{
						continue;
					}
					if (ShouldPreferSpineOnlyRootOnReadinessRescueCandidate(
							BestEvaluation.LeftThighAngularErrorDeg,
							BestEvaluation.RightThighAngularErrorDeg,
							BestEvaluation.SpineAngularErrorDeg,
							CandidateEvaluation.LeftThighAngularErrorDeg,
							CandidateEvaluation.RightThighAngularErrorDeg,
							CandidateEvaluation.SpineAngularErrorDeg) ||
						IsBetterRotationEvaluation(CandidateEvaluation, BestEvaluation))
					{
						BestEvaluation = CandidateEvaluation;
					}
				}
			}
		}

		return BestEvaluation;
	};
	const auto RefineBySpineOnlyReadinessRescueSweep = [&](
		const FPhase1PelvisRotationEvaluation& SeedEvaluation,
		const TCHAR* ContextTag,
		const bool bRequireTiltAdmissible)
	{
		FPhase1PelvisRotationEvaluation BestEvaluation = SeedEvaluation;
		if (bRequireTiltAdmissible && !SeedEvaluation.bTiltAdmissible)
		{
			return BestEvaluation;
		}

		static const float PitchSweepDegrees[] = { -3.0f, -2.0f, -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 2.0f, 3.0f };
		static const float RollSweepDegrees[] = { -3.0f, -2.0f, -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 2.0f, 3.0f };
		static const float YawSweepDegrees[] = { -2.0f, -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 2.0f };
		for (const float PitchDeg : PitchSweepDegrees)
		{
			for (const float RollDeg : RollSweepDegrees)
			{
				for (const float YawDeg : YawSweepDegrees)
				{
					if (FMath::IsNearlyZero(PitchDeg) && FMath::IsNearlyZero(RollDeg) && FMath::IsNearlyZero(YawDeg))
					{
						continue;
					}

					const FQuat PitchDelta(FVector::RightVector, FMath::DegreesToRadians(PitchDeg));
					const FQuat RollDelta(FVector::ForwardVector, FMath::DegreesToRadians(RollDeg));
					const FQuat YawDelta(FVector::UpVector, FMath::DegreesToRadians(YawDeg));
					const FQuat CandidateRotation =
						(YawDelta * RollDelta * PitchDelta * SeedEvaluation.Rotation).GetNormalized();
					const FPhase1PelvisRotationEvaluation CandidateEvaluation = BuildRotationEvaluation(
						CandidateRotation,
						FString::Printf(TEXT("%s_%s_y%.1f_p%.1f_r%.1f"),
							*SeedEvaluation.Source,
							ContextTag,
							YawDeg,
							PitchDeg,
							RollDeg));
					if (bRequireTiltAdmissible && !CandidateEvaluation.bTiltAdmissible)
					{
						continue;
					}
					if (IsBetterRotationEvaluation(CandidateEvaluation, BestEvaluation))
					{
						BestEvaluation = CandidateEvaluation;
					}
				}
			}
		}

		return BestEvaluation;
	};
	const auto RefineBySpineConstraintInterpolationSweep = [&](
		const FPhase1PelvisRotationEvaluation& SeedEvaluation,
		const TCHAR* ContextTag,
		const bool bRequireTiltAdmissible)
	{
		FPhase1PelvisRotationEvaluation BestEvaluation = SeedEvaluation;
		if (bRequireTiltAdmissible && !SeedEvaluation.bTiltAdmissible)
		{
			return BestEvaluation;
		}

		const FPhase1ConstraintRotationSample* SpineConstraintSample = nullptr;
		for (const FPhase1ConstraintRotationSample& ConstraintSample : ValidConstraintSamples)
		{
			if (ConstraintSample.ChildBoneName == TEXT("spine_01"))
			{
				SpineConstraintSample = &ConstraintSample;
				break;
			}
		}
		if (!SpineConstraintSample)
		{
			return BestEvaluation;
		}

		static const float InterpolationAlphas[] = { 0.02f, 0.05f, 0.08f, 0.10f, 0.15f, 0.20f, 0.25f, 0.33f, 0.50f, 0.67f, 0.80f };
		for (const float Alpha : InterpolationAlphas)
		{
			const FQuat CandidateRotation = FQuat::Slerp(
				SeedEvaluation.Rotation,
				SpineConstraintSample->CandidateRotation,
				Alpha).GetNormalized();
			const FPhase1PelvisRotationEvaluation CandidateEvaluation = BuildRotationEvaluation(
				CandidateRotation,
				FString::Printf(TEXT("%s_%s_a%.2f"),
					*SeedEvaluation.Source,
					ContextTag,
					Alpha));
			if (bRequireTiltAdmissible && !CandidateEvaluation.bTiltAdmissible)
			{
				continue;
			}
			if (ShouldPreferSpineOnlyRootOnReadinessRescueCandidate(
					BestEvaluation.LeftThighAngularErrorDeg,
					BestEvaluation.RightThighAngularErrorDeg,
					BestEvaluation.SpineAngularErrorDeg,
					CandidateEvaluation.LeftThighAngularErrorDeg,
					CandidateEvaluation.RightThighAngularErrorDeg,
					CandidateEvaluation.SpineAngularErrorDeg) ||
				IsBetterRotationEvaluation(CandidateEvaluation, BestEvaluation))
			{
				BestEvaluation = CandidateEvaluation;
			}
		}
		if (AutoCalibParams)
		{
			const float Alpha = AutoCalibSpineInterpolationAlpha;
			const FQuat CandidateRotation = FQuat::Slerp(
				SeedEvaluation.Rotation,
				SpineConstraintSample->CandidateRotation,
				Alpha).GetNormalized();
			const FPhase1PelvisRotationEvaluation CandidateEvaluation = BuildRotationEvaluation(
				CandidateRotation,
				FString::Printf(TEXT("%s_%s_autocalib_a%.2f"),
					*SeedEvaluation.Source,
					ContextTag,
					Alpha));
			if ((!bRequireTiltAdmissible || CandidateEvaluation.bTiltAdmissible) &&
				(ShouldPreferSpineOnlyRootOnReadinessRescueCandidate(
						BestEvaluation.LeftThighAngularErrorDeg,
						BestEvaluation.RightThighAngularErrorDeg,
						BestEvaluation.SpineAngularErrorDeg,
						CandidateEvaluation.LeftThighAngularErrorDeg,
						CandidateEvaluation.RightThighAngularErrorDeg,
						CandidateEvaluation.SpineAngularErrorDeg) ||
					IsBetterRotationEvaluation(CandidateEvaluation, BestEvaluation)))
			{
				BestEvaluation = CandidateEvaluation;
			}
		}

		return BestEvaluation;
	};
	const auto RefineByWorstThighConstraintInterpolationSweep = [&](
		const FPhase1PelvisRotationEvaluation& SeedEvaluation,
		const TCHAR* ContextTag,
		const bool bRequireTiltAdmissible)
	{
		FPhase1PelvisRotationEvaluation BestEvaluation = SeedEvaluation;
		if (bRequireTiltAdmissible && !SeedEvaluation.bTiltAdmissible)
		{
			return BestEvaluation;
		}

		FName FocusChildBone = TEXT("thigh_r");
		float FocusMarginDeg =
			BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg - SeedEvaluation.RightThighAngularErrorDeg;
		const float LeftThighMarginDeg =
			BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg - SeedEvaluation.LeftThighAngularErrorDeg;
		if (LeftThighMarginDeg + KINDA_SMALL_NUMBER < FocusMarginDeg)
		{
			FocusChildBone = TEXT("thigh_l");
		}
		if (FocusChildBone != TEXT("thigh_l") && FocusChildBone != TEXT("thigh_r"))
		{
			return BestEvaluation;
		}

		const FPhase1ConstraintRotationSample* FocusConstraintSample = nullptr;
		for (const FPhase1ConstraintRotationSample& ConstraintSample : ValidConstraintSamples)
		{
			if (ConstraintSample.ChildBoneName == FocusChildBone)
			{
				FocusConstraintSample = &ConstraintSample;
				break;
			}
		}
		if (!FocusConstraintSample)
		{
			return BestEvaluation;
		}

		static const float InterpolationAlphas[] = { 0.01f, 0.02f, 0.03f, 0.05f, 0.08f, 0.10f, 0.15f, 0.20f };
		for (const float Alpha : InterpolationAlphas)
		{
			const FQuat CandidateRotation = FQuat::Slerp(
				SeedEvaluation.Rotation,
				FocusConstraintSample->CandidateRotation,
				Alpha).GetNormalized();
			const FPhase1PelvisRotationEvaluation CandidateEvaluation = BuildRotationEvaluation(
				CandidateRotation,
				FString::Printf(TEXT("%s_%s_%s_a%.2f"),
					*SeedEvaluation.Source,
					ContextTag,
					*FocusChildBone.ToString(),
					Alpha));
			if (bRequireTiltAdmissible && !CandidateEvaluation.bTiltAdmissible)
			{
				continue;
			}
			if (ShouldAcceptWorstThighConstraintInterpolationCandidate(
					BestEvaluation.LeftThighAngularErrorDeg,
					BestEvaluation.RightThighAngularErrorDeg,
					BestEvaluation.SpineAngularErrorDeg,
					CandidateEvaluation.LeftThighAngularErrorDeg,
					CandidateEvaluation.RightThighAngularErrorDeg,
					CandidateEvaluation.SpineAngularErrorDeg) &&
				IsBetterRotationEvaluation(CandidateEvaluation, BestEvaluation))
			{
				BestEvaluation = CandidateEvaluation;
			}
		}
		if (AutoCalibParams)
		{
			const float Alpha = AutoCalibWorstThighInterpolationAlpha;
			const FQuat CandidateRotation = FQuat::Slerp(
				SeedEvaluation.Rotation,
				FocusConstraintSample->CandidateRotation,
				Alpha).GetNormalized();
			const FPhase1PelvisRotationEvaluation CandidateEvaluation = BuildRotationEvaluation(
				CandidateRotation,
				FString::Printf(TEXT("%s_%s_%s_autocalib_a%.2f"),
					*SeedEvaluation.Source,
					ContextTag,
					*FocusChildBone.ToString(),
					Alpha));
			if ((!bRequireTiltAdmissible || CandidateEvaluation.bTiltAdmissible) &&
				ShouldAcceptWorstThighConstraintInterpolationCandidate(
					BestEvaluation.LeftThighAngularErrorDeg,
					BestEvaluation.RightThighAngularErrorDeg,
					BestEvaluation.SpineAngularErrorDeg,
					CandidateEvaluation.LeftThighAngularErrorDeg,
					CandidateEvaluation.RightThighAngularErrorDeg,
					CandidateEvaluation.SpineAngularErrorDeg) &&
				IsBetterRotationEvaluation(CandidateEvaluation, BestEvaluation))
			{
				BestEvaluation = CandidateEvaluation;
			}
		}

		return BestEvaluation;
	};
	const auto RefineBySpineSafeWorstThighMarginSweep = [&](
		const FPhase1PelvisRotationEvaluation& SeedEvaluation,
		const TCHAR* ContextTag,
		const bool bRequireTiltAdmissible)
	{
		FPhase1PelvisRotationEvaluation BestEvaluation = SeedEvaluation;
		if (bRequireTiltAdmissible && !SeedEvaluation.bTiltAdmissible)
		{
			return BestEvaluation;
		}

		static const float PitchSweepDegrees[] = { -0.50f, -0.25f, -0.10f, -0.05f, 0.0f, 0.05f, 0.10f, 0.25f, 0.50f };
		static const float RollSweepDegrees[] = { -0.50f, -0.25f, -0.10f, -0.05f, 0.0f, 0.05f, 0.10f, 0.25f, 0.50f };
		static const float YawSweepDegrees[] = { -0.50f, -0.25f, -0.10f, -0.05f, 0.0f, 0.05f, 0.10f, 0.25f, 0.50f };
		for (const float PitchDeg : PitchSweepDegrees)
		{
			for (const float RollDeg : RollSweepDegrees)
			{
				for (const float YawDeg : YawSweepDegrees)
				{
					if (FMath::IsNearlyZero(PitchDeg) && FMath::IsNearlyZero(RollDeg) && FMath::IsNearlyZero(YawDeg))
					{
						continue;
					}

					const FQuat PitchDelta(FVector::RightVector, FMath::DegreesToRadians(PitchDeg));
					const FQuat RollDelta(FVector::ForwardVector, FMath::DegreesToRadians(RollDeg));
					const FQuat YawDelta(FVector::UpVector, FMath::DegreesToRadians(YawDeg));
					const FQuat CandidateRotation =
						(YawDelta * RollDelta * PitchDelta * SeedEvaluation.Rotation).GetNormalized();
					const FPhase1PelvisRotationEvaluation CandidateEvaluation = BuildRotationEvaluation(
						CandidateRotation,
						FString::Printf(TEXT("%s_%s_y%.2f_p%.2f_r%.2f"),
							*SeedEvaluation.Source,
							ContextTag,
							YawDeg,
							PitchDeg,
							RollDeg));
					if (bRequireTiltAdmissible && !CandidateEvaluation.bTiltAdmissible)
					{
						continue;
					}
					if (ShouldAcceptSpineSafeWorstThighMarginSweepCandidate(
							BestEvaluation.LeftThighAngularErrorDeg,
							BestEvaluation.RightThighAngularErrorDeg,
							BestEvaluation.SpineAngularErrorDeg,
							CandidateEvaluation.LeftThighAngularErrorDeg,
							CandidateEvaluation.RightThighAngularErrorDeg,
							CandidateEvaluation.SpineAngularErrorDeg) &&
						IsBetterRotationEvaluation(CandidateEvaluation, BestEvaluation))
					{
						BestEvaluation = CandidateEvaluation;
					}
				}
			}
		}

		return BestEvaluation;
	};
	const auto RefineBySpineSafeWorstThighFocusedDelta = [&](
		const FPhase1PelvisRotationEvaluation& SeedEvaluation,
		const TCHAR* ContextTag,
		const bool bRequireTiltAdmissible)
	{
		FPhase1PelvisRotationEvaluation BestEvaluation = SeedEvaluation;
		if (bRequireTiltAdmissible && !SeedEvaluation.bTiltAdmissible)
		{
			return BestEvaluation;
		}

		FName FocusChildBone = TEXT("thigh_r");
		float FocusMarginDeg =
			BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg - SeedEvaluation.RightThighAngularErrorDeg;
		const float LeftThighMarginDeg =
			BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg - SeedEvaluation.LeftThighAngularErrorDeg;
		if (LeftThighMarginDeg + KINDA_SMALL_NUMBER < FocusMarginDeg)
		{
			FocusChildBone = TEXT("thigh_l");
		}

		static const float CorrectionBlendAlphas[] = { 0.10f, 0.05f, 0.02f, 0.01f, 0.005f };
		FQuat WorkingRotation = SeedEvaluation.Rotation;
		bool bFoundRelevantFocusSample = false;
		for (int32 PassIndex = 0; PassIndex < 6; ++PassIndex)
		{
			bool bImprovedThisPass = false;
			for (const FPhase1ConstraintRotationSample& ConstraintSample : ValidConstraintSamples)
			{
				if (!ConstraintSample.bValid ||
					!IsConstraintSampleRelevantToFocusedBone(
						ConstraintSample.ChildBoneName,
						ConstraintSample.Source,
						FocusChildBone))
				{
					continue;
				}

				bFoundRelevantFocusSample = true;
				const FQuat DeltaRotation = (ConstraintSample.CandidateRotation * WorkingRotation.Inverse()).GetNormalized();
				for (const float CorrectionBlendAlpha : CorrectionBlendAlphas)
				{
					const float ScaledCorrectionBlendAlpha =
						FMath::Clamp(CorrectionBlendAlpha * AutoCalibFocusedDeltaScale, 0.001f, 1.0f);
					const FQuat CandidateRotation = FQuat::Slerp(
						WorkingRotation,
						(DeltaRotation * WorkingRotation).GetNormalized(),
						ScaledCorrectionBlendAlpha).GetNormalized();
					const FPhase1PelvisRotationEvaluation CandidateEvaluation = BuildRotationEvaluation(
						CandidateRotation,
						FString::Printf(TEXT("%s_%s_%s_p%d_a%.3f"),
							*BestEvaluation.Source,
							ContextTag,
							*FocusChildBone.ToString(),
							PassIndex,
							ScaledCorrectionBlendAlpha));
					if (bRequireTiltAdmissible && !CandidateEvaluation.bTiltAdmissible)
					{
						continue;
					}
					if (ShouldAcceptSpineSafeWorstThighMarginSweepCandidate(
							BestEvaluation.LeftThighAngularErrorDeg,
							BestEvaluation.RightThighAngularErrorDeg,
							BestEvaluation.SpineAngularErrorDeg,
							CandidateEvaluation.LeftThighAngularErrorDeg,
							CandidateEvaluation.RightThighAngularErrorDeg,
							CandidateEvaluation.SpineAngularErrorDeg) &&
						IsBetterRotationEvaluation(CandidateEvaluation, BestEvaluation))
					{
						BestEvaluation = CandidateEvaluation;
						WorkingRotation = CandidateRotation;
						bImprovedThisPass = true;
					}
				}
			}
			if (!bFoundRelevantFocusSample)
			{
				return BestEvaluation;
			}
			if (!bImprovedThisPass)
			{
				break;
			}
		}

		return BestEvaluation;
	};
	const auto RefineByFocusedConstraintDelta = [&](
		const FPhase1PelvisRotationEvaluation& SeedEvaluation,
		const FName FocusChildBone,
		const TCHAR* ContextTag,
		const bool bRequireTiltAdmissible,
		const bool bPreferSpineOnlyReadinessRescue = false)
	{
		FPhase1PelvisRotationEvaluation BestEvaluation = SeedEvaluation;
		if (bRequireTiltAdmissible && !SeedEvaluation.bTiltAdmissible)
		{
			return BestEvaluation;
		}

		const FPhase1ConstraintRotationSample* FocusSample = nullptr;
		for (const FPhase1ConstraintRotationSample& ConstraintSample : ValidConstraintSamples)
		{
			if (ConstraintSample.ChildBoneName == FocusChildBone)
			{
				FocusSample = &ConstraintSample;
				break;
			}
		}
		if (!FocusSample)
		{
			return BestEvaluation;
		}

		static const float CorrectionBlendAlphas[] = { 1.00f, 0.75f, 0.50f, 0.25f, 0.10f, 0.05f, 0.02f, 0.01f, 0.005f };
		FPhase1PelvisRotationEvaluation WorkingEvaluation = SeedEvaluation;
		for (int32 PassIndex = 0; PassIndex < 10; ++PassIndex)
		{
			const FQuat ParentConstraintWorldRotation =
				(WorkingEvaluation.Rotation * FocusSample->ParentConstraintLocalRotation).GetNormalized();
			FQuat DeltaRotation =
				(FocusSample->ChildConstraintWorldRotation * ParentConstraintWorldRotation.Inverse()).GetNormalized();
			if ((FQuat::Identity | DeltaRotation) < 0.0f)
			{
				DeltaRotation *= -1.0f;
			}

			bool bImprovedThisPass = false;
			for (const float CorrectionBlendAlpha : CorrectionBlendAlphas)
			{
				const float ScaledCorrectionBlendAlpha =
					FMath::Clamp(CorrectionBlendAlpha * AutoCalibFocusedDeltaScale, 0.001f, 1.0f);
				const FQuat AppliedDelta = FQuat::Slerp(FQuat::Identity, DeltaRotation, ScaledCorrectionBlendAlpha).GetNormalized();
				const FQuat CandidateRotation = (AppliedDelta * WorkingEvaluation.Rotation).GetNormalized();
				const FPhase1PelvisRotationEvaluation CandidateEvaluation = BuildRotationEvaluation(
					CandidateRotation,
					FString::Printf(TEXT("%s_%s_%s_p%d_a%.2f"),
						*BestEvaluation.Source,
						ContextTag,
						*FocusChildBone.ToString(),
						PassIndex,
						ScaledCorrectionBlendAlpha));
				if (bRequireTiltAdmissible && !CandidateEvaluation.bTiltAdmissible)
				{
					continue;
				}
				if ((bPreferSpineOnlyReadinessRescue &&
						ShouldPreferSpineOnlyRootOnReadinessRescueCandidate(
							BestEvaluation.LeftThighAngularErrorDeg,
							BestEvaluation.RightThighAngularErrorDeg,
							BestEvaluation.SpineAngularErrorDeg,
							CandidateEvaluation.LeftThighAngularErrorDeg,
							CandidateEvaluation.RightThighAngularErrorDeg,
							CandidateEvaluation.SpineAngularErrorDeg)) ||
					IsBetterRotationEvaluation(CandidateEvaluation, BestEvaluation))
				{
					BestEvaluation = CandidateEvaluation;
					WorkingEvaluation = CandidateEvaluation;
					bImprovedThisPass = true;
				}
			}

			if (!bImprovedThisPass)
			{
				break;
			}
		}

		return BestEvaluation;
	};
	const auto ResolveWorstReadinessConstraintBone = [](const FPhase1PelvisRotationEvaluation& Evaluation)
	{
		FName FocusChildBone = TEXT("thigh_r");
		float FocusMarginDeg =
			BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg - Evaluation.RightThighAngularErrorDeg;
		const float LeftThighMarginDeg =
			BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg - Evaluation.LeftThighAngularErrorDeg;
		const float SpineMarginDeg =
			BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisSpineDirectLinkAngularErrorDeg - Evaluation.SpineAngularErrorDeg;
		if (LeftThighMarginDeg + KINDA_SMALL_NUMBER < FocusMarginDeg)
		{
			FocusChildBone = TEXT("thigh_l");
			FocusMarginDeg = LeftThighMarginDeg;
		}
		if (SpineMarginDeg + KINDA_SMALL_NUMBER < FocusMarginDeg)
		{
			FocusChildBone = TEXT("spine_01");
		}
		return FocusChildBone;
	};
	const FPhase1PelvisRotationEvaluation LiveRotationEvaluation =
		BuildRotationEvaluation(RotationSamples[0].CandidateRotation, RotationSamples[0].Source);
	const bool bProtectLiveTilt = LiveRotationEvaluation.bTiltAdmissible;
	bool bTriggeredTiltSpineRescuePath = false;
	bool bTriggeredForensicSpineRescuePath = false;
	FPhase1PelvisRotationEvaluation TiltSpineRescueEvaluation;
	FPhase1PelvisRotationEvaluation ForensicSpineRescueEvaluation;
	FPhase1PelvisRotationEvaluation BestRotationEvaluation;
	FPhase1PelvisRotationEvaluation BestTiltAdmissibleRotationEvaluation;
	bool bHasTiltAdmissibleRotationEvaluation = false;
	for (const FPhase1ConstraintRotationSample& Sample : RotationSamples)
	{
		if (!Sample.bValid)
		{
			continue;
		}

		const FPhase1PelvisRotationEvaluation CandidateEvaluation = BuildRotationEvaluation(Sample.CandidateRotation, Sample.Source);
		if (IsBetterRotationEvaluation(CandidateEvaluation, BestRotationEvaluation))
		{
			BestRotationEvaluation = CandidateEvaluation;
		}
		if (CandidateEvaluation.bTiltAdmissible &&
			(!bHasTiltAdmissibleRotationEvaluation || IsBetterRotationEvaluation(CandidateEvaluation, BestTiltAdmissibleRotationEvaluation)))
		{
			BestTiltAdmissibleRotationEvaluation = CandidateEvaluation;
			bHasTiltAdmissibleRotationEvaluation = true;
		}
	}

	if (BestRotationEvaluation.MaxAngularErrorDeg < TNumericLimits<float>::Max())
	{
		constexpr int32 RefinementPassCount = 7;
		float RefinementStepDeg = 24.0f;
		for (int32 PassIndex = 0; PassIndex < RefinementPassCount; ++PassIndex)
		{
			bool bImprovedThisPass = false;
			const FVector SearchAxes[] =
			{
				FVector::ForwardVector,
				FVector::RightVector,
				FVector::UpVector,
				BestRotationEvaluation.Rotation.GetAxisX().GetSafeNormal(),
				BestRotationEvaluation.Rotation.GetAxisY().GetSafeNormal(),
				BestRotationEvaluation.Rotation.GetAxisZ().GetSafeNormal()
			};

			for (const FVector& SearchAxis : SearchAxes)
			{
				if (SearchAxis.IsNearlyZero())
				{
					continue;
				}

				for (const float Direction : { -1.0f, 1.0f })
				{
					const FQuat DeltaRotation(SearchAxis, FMath::DegreesToRadians(RefinementStepDeg * Direction));
					const FQuat CandidateRotation = (DeltaRotation * BestRotationEvaluation.Rotation).GetNormalized();
					const FPhase1PelvisRotationEvaluation CandidateEvaluation = BuildRotationEvaluation(
						CandidateRotation,
						FString::Printf(TEXT("%s_refine_p%d_%s_%.2f"),
							*BestRotationEvaluation.Source,
							PassIndex,
							Direction < 0.0f ? TEXT("neg") : TEXT("pos"),
							RefinementStepDeg));
					if (IsBetterRotationEvaluation(CandidateEvaluation, BestRotationEvaluation))
					{
						BestRotationEvaluation = CandidateEvaluation;
						bImprovedThisPass = true;
					}
					if (CandidateEvaluation.bTiltAdmissible &&
						(!bHasTiltAdmissibleRotationEvaluation || IsBetterRotationEvaluation(CandidateEvaluation, BestTiltAdmissibleRotationEvaluation)))
					{
						BestTiltAdmissibleRotationEvaluation = CandidateEvaluation;
						bHasTiltAdmissibleRotationEvaluation = true;
					}
				}
			}

			if (!bImprovedThisPass)
			{
				RefinementStepDeg *= 0.5f;
			}
		}
	}
	if (bProtectLiveTilt && bHasTiltAdmissibleRotationEvaluation)
	{
		constexpr int32 TiltProtectedRefinementPassCount = 7;
		float TiltProtectedRefinementStepDeg = 12.0f;
		for (int32 PassIndex = 0; PassIndex < TiltProtectedRefinementPassCount; ++PassIndex)
		{
			bool bImprovedThisPass = false;
			const FVector SearchAxes[] =
			{
				FVector::ForwardVector,
				FVector::RightVector,
				FVector::UpVector,
				BestTiltAdmissibleRotationEvaluation.Rotation.GetAxisX().GetSafeNormal(),
				BestTiltAdmissibleRotationEvaluation.Rotation.GetAxisY().GetSafeNormal(),
				BestTiltAdmissibleRotationEvaluation.Rotation.GetAxisZ().GetSafeNormal()
			};

			for (const FVector& SearchAxis : SearchAxes)
			{
				if (SearchAxis.IsNearlyZero())
				{
					continue;
				}

				for (const float Direction : { -1.0f, 1.0f })
				{
					const FQuat DeltaRotation(SearchAxis, FMath::DegreesToRadians(TiltProtectedRefinementStepDeg * Direction));
					const FQuat CandidateRotation = (DeltaRotation * BestTiltAdmissibleRotationEvaluation.Rotation).GetNormalized();
					const FPhase1PelvisRotationEvaluation CandidateEvaluation = BuildRotationEvaluation(
						CandidateRotation,
						FString::Printf(TEXT("%s_tilt_refine_p%d_%s_%.2f"),
							*BestTiltAdmissibleRotationEvaluation.Source,
							PassIndex,
							Direction < 0.0f ? TEXT("neg") : TEXT("pos"),
							TiltProtectedRefinementStepDeg));
					if (CandidateEvaluation.bTiltAdmissible &&
						IsBetterRotationEvaluation(CandidateEvaluation, BestTiltAdmissibleRotationEvaluation))
					{
						BestTiltAdmissibleRotationEvaluation = CandidateEvaluation;
						bImprovedThisPass = true;
					}
				}
			}

			if (!bImprovedThisPass)
			{
				TiltProtectedRefinementStepDeg *= 0.5f;
			}
		}
	}
	if (bProtectLiveTilt && !BestRotationEvaluation.bTiltAdmissible && bHasTiltAdmissibleRotationEvaluation)
	{
		BestRotationEvaluation = BestTiltAdmissibleRotationEvaluation;
	}
	const FPhase1PelvisRotationEvaluation PreTiltProtectionBestRotationEvaluation = BestRotationEvaluation;
	if (BestRotationEvaluation.MaxAngularErrorDeg < TNumericLimits<float>::Max())
	{
		BestRotationEvaluation = RefineByConstraintCorrection(
			BestRotationEvaluation,
			TEXT("constraint_correct"),
			false);
		BestRotationEvaluation = RefineByWorstConstraintCorrection(
			BestRotationEvaluation,
			TEXT("worst_constraint_correct"),
			false);
	}
	if (bHasTiltAdmissibleRotationEvaluation)
	{
		BestTiltAdmissibleRotationEvaluation = RefineByConstraintCorrection(
			BestTiltAdmissibleRotationEvaluation,
			TEXT("tilt_constraint_correct"),
			true);
		BestTiltAdmissibleRotationEvaluation = RefineByWorstConstraintCorrection(
			BestTiltAdmissibleRotationEvaluation,
			TEXT("tilt_worst_constraint_correct"),
			true);
		BestTiltAdmissibleRotationEvaluation = RefineByTiltAdmissibleSweep(
			BestTiltAdmissibleRotationEvaluation,
			TEXT("tilt_sweep"));
		BestTiltAdmissibleRotationEvaluation = RefineByLocalMarginSweep(
			BestTiltAdmissibleRotationEvaluation,
			TEXT("tilt_margin_sweep"),
			true);
		BestTiltAdmissibleRotationEvaluation = RefineByFocusedConstraintDelta(
			BestTiltAdmissibleRotationEvaluation,
			ResolveWorstReadinessConstraintBone(BestTiltAdmissibleRotationEvaluation),
			TEXT("tilt_focus_delta"),
			true);
		BestTiltAdmissibleRotationEvaluation = RefineByFocusedConstraintDelta(
			BestTiltAdmissibleRotationEvaluation,
			ResolveWorstReadinessConstraintBone(BestTiltAdmissibleRotationEvaluation),
			TEXT("tilt_second_focus_delta"),
			true);
		if (BestTiltAdmissibleRotationEvaluation.RightThighAngularErrorDeg >
			BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg + KINDA_SMALL_NUMBER)
		{
			BestTiltAdmissibleRotationEvaluation = RefineByFocusedConstraintDelta(
				BestTiltAdmissibleRotationEvaluation,
				TEXT("thigh_r"),
				TEXT("tilt_right_thigh_delta"),
				true);
			BestTiltAdmissibleRotationEvaluation = RefineByLocalMarginSweep(
				BestTiltAdmissibleRotationEvaluation,
				TEXT("tilt_right_thigh_margin_sweep"),
				true);
		}
		if (!BestTiltAdmissibleRotationEvaluation.bRootOnReadinessMarginSatisfied)
		{
			for (int32 CleanupIteration = 0;
				CleanupIteration < 2 && !BestTiltAdmissibleRotationEvaluation.bRootOnReadinessMarginSatisfied;
				++CleanupIteration)
			{
				BestTiltAdmissibleRotationEvaluation = RefineByFocusedConstraintDelta(
					BestTiltAdmissibleRotationEvaluation,
					ResolveWorstReadinessConstraintBone(BestTiltAdmissibleRotationEvaluation),
					TEXT("tilt_final_focus_delta"),
					true);
				BestTiltAdmissibleRotationEvaluation = RefineByLocalMarginSweep(
					BestTiltAdmissibleRotationEvaluation,
					TEXT("tilt_final_margin_sweep"),
					true);
			}
			if (!BestTiltAdmissibleRotationEvaluation.bRootOnReadinessMarginSatisfied &&
				ShouldRunRootOnReadinessUltraFineMarginSweep(
					BestTiltAdmissibleRotationEvaluation.RootOnReadinessTotalDeficitDeg))
			{
				BestTiltAdmissibleRotationEvaluation = RefineByUltraFineMarginSweep(
					BestTiltAdmissibleRotationEvaluation,
					TEXT("tilt_ultrafine_margin_sweep"),
					true);
			}
			if (ShouldRunSpineOnlyRootOnReadinessRescueSweep(
					BestTiltAdmissibleRotationEvaluation.LeftThighAngularErrorDeg,
					BestTiltAdmissibleRotationEvaluation.RightThighAngularErrorDeg,
					BestTiltAdmissibleRotationEvaluation.SpineAngularErrorDeg))
			{
				bTriggeredTiltSpineRescuePath = true;
				BestTiltAdmissibleRotationEvaluation = RefineByFocusedConstraintDelta(
					BestTiltAdmissibleRotationEvaluation,
					TEXT("spine_01"),
					TEXT("tilt_spine_rescue_focus_delta"),
					true,
					true);
				BestTiltAdmissibleRotationEvaluation = RefineBySpineOnlyReadinessRescueSweep(
					BestTiltAdmissibleRotationEvaluation,
					TEXT("tilt_spine_rescue_sweep"),
					true);
				TiltSpineRescueEvaluation = BestTiltAdmissibleRotationEvaluation;
			}
		}
		const bool bAcceptTiltRescueCandidate =
			bTriggeredTiltSpineRescuePath
				? ShouldAcceptSpineOnlyRootOnReadinessRescueCandidate(
					BestRotationEvaluation.LeftThighAngularErrorDeg,
					BestRotationEvaluation.RightThighAngularErrorDeg,
					BestRotationEvaluation.SpineAngularErrorDeg,
					BestTiltAdmissibleRotationEvaluation.LeftThighAngularErrorDeg,
					BestTiltAdmissibleRotationEvaluation.RightThighAngularErrorDeg,
					BestTiltAdmissibleRotationEvaluation.SpineAngularErrorDeg)
				: (ShouldPreferSpineOnlyRootOnReadinessRescueCandidate(
						BestRotationEvaluation.LeftThighAngularErrorDeg,
						BestRotationEvaluation.RightThighAngularErrorDeg,
						BestRotationEvaluation.SpineAngularErrorDeg,
						BestTiltAdmissibleRotationEvaluation.LeftThighAngularErrorDeg,
						BestTiltAdmissibleRotationEvaluation.RightThighAngularErrorDeg,
						BestTiltAdmissibleRotationEvaluation.SpineAngularErrorDeg) ||
					IsBetterRotationEvaluation(BestTiltAdmissibleRotationEvaluation, BestRotationEvaluation));
		if (bProtectLiveTilt && BestRotationEvaluation.bTiltAdmissible && bAcceptTiltRescueCandidate)
		{
			BestRotationEvaluation = BestTiltAdmissibleRotationEvaluation;
		}
	}
	FPhase1PelvisRotationEvaluation BestUnconstrainedRotationEvaluation = PreTiltProtectionBestRotationEvaluation;
	if (SearchConfig.bEnableForensicSearch &&
		BestUnconstrainedRotationEvaluation.MaxAngularErrorDeg < TNumericLimits<float>::Max())
	{
		BestUnconstrainedRotationEvaluation = RefineByConstraintCorrection(
			BestUnconstrainedRotationEvaluation,
			TEXT("forensic_constraint_correct"),
			false);
		BestUnconstrainedRotationEvaluation = RefineByWorstConstraintCorrection(
			BestUnconstrainedRotationEvaluation,
			TEXT("forensic_worst_constraint_correct"),
			false);
		BestUnconstrainedRotationEvaluation = RefineByLocalMarginSweep(
			BestUnconstrainedRotationEvaluation,
			TEXT("forensic_margin_sweep"),
			false);
		BestUnconstrainedRotationEvaluation = RefineByFocusedConstraintDelta(
			BestUnconstrainedRotationEvaluation,
			ResolveWorstReadinessConstraintBone(BestUnconstrainedRotationEvaluation),
			TEXT("forensic_focus_delta"),
			false);
		BestUnconstrainedRotationEvaluation = RefineByFocusedConstraintDelta(
			BestUnconstrainedRotationEvaluation,
			ResolveWorstReadinessConstraintBone(BestUnconstrainedRotationEvaluation),
			TEXT("forensic_second_focus_delta"),
			false);
		if (BestUnconstrainedRotationEvaluation.RightThighAngularErrorDeg >
			BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg + KINDA_SMALL_NUMBER)
		{
			BestUnconstrainedRotationEvaluation = RefineByFocusedConstraintDelta(
				BestUnconstrainedRotationEvaluation,
				TEXT("thigh_r"),
				TEXT("forensic_right_thigh_delta"),
				false);
			BestUnconstrainedRotationEvaluation = RefineByLocalMarginSweep(
				BestUnconstrainedRotationEvaluation,
				TEXT("forensic_right_thigh_margin_sweep"),
				false);
		}
		if (!BestUnconstrainedRotationEvaluation.bRootOnReadinessMarginSatisfied)
		{
			for (int32 CleanupIteration = 0;
				CleanupIteration < 2 && !BestUnconstrainedRotationEvaluation.bRootOnReadinessMarginSatisfied;
				++CleanupIteration)
			{
				BestUnconstrainedRotationEvaluation = RefineByFocusedConstraintDelta(
					BestUnconstrainedRotationEvaluation,
					ResolveWorstReadinessConstraintBone(BestUnconstrainedRotationEvaluation),
					TEXT("forensic_final_focus_delta"),
					false);
				BestUnconstrainedRotationEvaluation = RefineByLocalMarginSweep(
					BestUnconstrainedRotationEvaluation,
					TEXT("forensic_final_margin_sweep"),
					false);
			}
			if (!BestUnconstrainedRotationEvaluation.bRootOnReadinessMarginSatisfied &&
				ShouldRunRootOnReadinessUltraFineMarginSweep(
					BestUnconstrainedRotationEvaluation.RootOnReadinessTotalDeficitDeg))
			{
				BestUnconstrainedRotationEvaluation = RefineByUltraFineMarginSweep(
					BestUnconstrainedRotationEvaluation,
					TEXT("forensic_ultrafine_margin_sweep"),
					false);
			}
			if (ShouldRunSpineOnlyRootOnReadinessRescueSweep(
					BestUnconstrainedRotationEvaluation.LeftThighAngularErrorDeg,
					BestUnconstrainedRotationEvaluation.RightThighAngularErrorDeg,
					BestUnconstrainedRotationEvaluation.SpineAngularErrorDeg))
			{
				bTriggeredForensicSpineRescuePath = true;
				BestUnconstrainedRotationEvaluation = RefineByFocusedConstraintDelta(
					BestUnconstrainedRotationEvaluation,
					TEXT("spine_01"),
					TEXT("forensic_spine_rescue_focus_delta"),
					false,
					true);
				BestUnconstrainedRotationEvaluation = RefineBySpineOnlyReadinessRescueSweep(
					BestUnconstrainedRotationEvaluation,
					TEXT("forensic_spine_rescue_sweep"),
					false);
				ForensicSpineRescueEvaluation = BestUnconstrainedRotationEvaluation;
			}
		}
	}
	const bool bAcceptForensicRescueCandidate =
		bTriggeredForensicSpineRescuePath
			? ShouldAcceptSpineOnlyRootOnReadinessRescueCandidate(
				BestRotationEvaluation.LeftThighAngularErrorDeg,
				BestRotationEvaluation.RightThighAngularErrorDeg,
				BestRotationEvaluation.SpineAngularErrorDeg,
				BestUnconstrainedRotationEvaluation.LeftThighAngularErrorDeg,
				BestUnconstrainedRotationEvaluation.RightThighAngularErrorDeg,
				BestUnconstrainedRotationEvaluation.SpineAngularErrorDeg)
			: (ShouldPreferSpineOnlyRootOnReadinessRescueCandidate(
					BestRotationEvaluation.LeftThighAngularErrorDeg,
					BestRotationEvaluation.RightThighAngularErrorDeg,
					BestRotationEvaluation.SpineAngularErrorDeg,
					BestUnconstrainedRotationEvaluation.LeftThighAngularErrorDeg,
					BestUnconstrainedRotationEvaluation.RightThighAngularErrorDeg,
					BestUnconstrainedRotationEvaluation.SpineAngularErrorDeg) ||
				IsBetterRotationEvaluation(BestUnconstrainedRotationEvaluation, BestRotationEvaluation));
	if (SearchConfig.bEnableForensicSearch &&
		BestUnconstrainedRotationEvaluation.bTiltAdmissible &&
		bAcceptForensicRescueCandidate)
	{
		BestRotationEvaluation = BestUnconstrainedRotationEvaluation;
	}
	if (SearchConfig.bEnableConstraintInterpolationSweep &&
		ShouldRunSpineConstraintInterpolationSweep(
			BestRotationEvaluation.LeftThighAngularErrorDeg,
			BestRotationEvaluation.RightThighAngularErrorDeg,
			BestRotationEvaluation.SpineAngularErrorDeg))
	{
		bRanConstraintInterpolation = true;
		BestRotationEvaluation = RefineBySpineConstraintInterpolationSweep(
			BestRotationEvaluation,
			TEXT("spine_interp"),
			bProtectLiveTilt);
	}
	if (SearchConfig.bEnableWorstThighInterpolationSweep &&
		ShouldRunWorstThighConstraintInterpolationSweep(
			BestRotationEvaluation.LeftThighAngularErrorDeg,
			BestRotationEvaluation.RightThighAngularErrorDeg,
			BestRotationEvaluation.SpineAngularErrorDeg))
	{
		bRanWorstThighInterpolation = true;
		BestRotationEvaluation = RefineByWorstThighConstraintInterpolationSweep(
			BestRotationEvaluation,
			TEXT("worst_thigh_interp"),
			bProtectLiveTilt);
		BestRotationEvaluation = RefineBySpineSafeWorstThighMarginSweep(
			BestRotationEvaluation,
			TEXT("worst_thigh_margin_sweep"),
			bProtectLiveTilt);
		if (ShouldRunSpineSafeWorstThighFocusedDelta(
				BestRotationEvaluation.LeftThighAngularErrorDeg,
				BestRotationEvaluation.RightThighAngularErrorDeg,
				BestRotationEvaluation.SpineAngularErrorDeg))
		{
			BestRotationEvaluation = RefineBySpineSafeWorstThighFocusedDelta(
				BestRotationEvaluation,
				TEXT("worst_thigh_focus_delta"),
				bProtectLiveTilt);
		}
	}
	if (SearchConfig.bEnableCoupledTradeControlPass)
	{
		bRanCoupledTradeControl = true;
		FPhase1PelvisRotationEvaluation CoupledTradeEvaluation = BestRotationEvaluation;
		const FPhase1PelvisRotationEvaluation SpineFrontierEvaluation =
			RefineBySpineConstraintInterpolationSweep(
				BestRotationEvaluation,
				TEXT("coupled_trade_spine_frontier"),
				bProtectLiveTilt);
		FPhase1PelvisRotationEvaluation ThighFrontierEvaluation = BestRotationEvaluation;
		if (ShouldRunWorstThighConstraintInterpolationSweep(
				BestRotationEvaluation.LeftThighAngularErrorDeg,
				BestRotationEvaluation.RightThighAngularErrorDeg,
				BestRotationEvaluation.SpineAngularErrorDeg))
		{
			ThighFrontierEvaluation = RefineByWorstThighConstraintInterpolationSweep(
				BestRotationEvaluation,
				TEXT("coupled_trade_thigh_frontier"),
				bProtectLiveTilt);
		}

		static const float CoupledTradeBlendWeights[] = { 0.20f, 0.35f, 0.50f, 0.65f, 0.80f };
		for (const float BlendWeight : CoupledTradeBlendWeights)
		{
			const FQuat CandidateRotation = FQuat::Slerp(
				SpineFrontierEvaluation.Rotation,
				ThighFrontierEvaluation.Rotation,
				BlendWeight).GetNormalized();
			const FPhase1PelvisRotationEvaluation CandidateEvaluation = BuildRotationEvaluation(
				CandidateRotation,
				FString::Printf(TEXT("coupled_trade_blend_a%.2f"), BlendWeight));
			if (bProtectLiveTilt && !CandidateEvaluation.bTiltAdmissible)
			{
				continue;
			}
			if (ShouldAcceptPhase1CoupledTradeControlCandidate(
					CoupledTradeEvaluation.LeftThighAngularErrorDeg,
					CoupledTradeEvaluation.RightThighAngularErrorDeg,
					CoupledTradeEvaluation.SpineAngularErrorDeg,
					CandidateEvaluation.LeftThighAngularErrorDeg,
					CandidateEvaluation.RightThighAngularErrorDeg,
					CandidateEvaluation.SpineAngularErrorDeg,
					SearchConfig.CoupledTradeSpineGainWeight,
					SearchConfig.CoupledTradeThighGainWeight,
					SearchConfig.CoupledTradeMaxPairedRegressionDeg) ||
				IsBetterRotationEvaluation(CandidateEvaluation, CoupledTradeEvaluation))
			{
				CoupledTradeEvaluation = CandidateEvaluation;
			}
		}

		if (ShouldAcceptPhase1CoupledTradeControlCandidate(
				BestRotationEvaluation.LeftThighAngularErrorDeg,
				BestRotationEvaluation.RightThighAngularErrorDeg,
				BestRotationEvaluation.SpineAngularErrorDeg,
				CoupledTradeEvaluation.LeftThighAngularErrorDeg,
				CoupledTradeEvaluation.RightThighAngularErrorDeg,
				CoupledTradeEvaluation.SpineAngularErrorDeg,
				SearchConfig.CoupledTradeSpineGainWeight,
				SearchConfig.CoupledTradeThighGainWeight,
				SearchConfig.CoupledTradeMaxPairedRegressionDeg))
		{
			BestRotationEvaluation = CoupledTradeEvaluation;
		}
	}
	if (SearchConfig.bEnablePairBlendFrontierFollowThroughPass)
	{
		bRanPairBlendFrontierFollowThrough = true;
		const FPhase1ConstraintRotationSample* PairFrontierLeftConstraintSample = nullptr;
		const FPhase1ConstraintRotationSample* PairFrontierRightConstraintSample = nullptr;
		const FPhase1ConstraintRotationSample* PairFrontierSpineConstraintSample = nullptr;
		for (const FPhase1ConstraintRotationSample& Sample : RotationSamples)
		{
			if (!Sample.bValid)
			{
				continue;
			}

			if (Sample.ChildBoneName == TEXT("thigh_l"))
			{
				PairFrontierLeftConstraintSample = &Sample;
			}
			else if (Sample.ChildBoneName == TEXT("thigh_r"))
			{
				PairFrontierRightConstraintSample = &Sample;
			}
			else if (Sample.ChildBoneName == TEXT("spine_01"))
			{
				PairFrontierSpineConstraintSample = &Sample;
			}
		}

		if (PairFrontierLeftConstraintSample && PairFrontierRightConstraintSample && PairFrontierSpineConstraintSample)
		{
			const auto TryExtractPairBlendWeight = [](const FString& Source, const TCHAR* Token, float& OutWeight) -> bool
			{
				const FString Needle = FString(Token);
				const int32 TokenStart = Source.Find(Needle, ESearchCase::IgnoreCase, ESearchDir::FromStart);
				if (TokenStart == INDEX_NONE)
				{
					return false;
				}

				int32 ValueStart = TokenStart + Needle.Len();
				int32 ValueEnd = ValueStart;
				while (ValueEnd < Source.Len())
				{
					const TCHAR Character = Source[ValueEnd];
					if ((Character >= TEXT('0') && Character <= TEXT('9')) || Character == TEXT('.') || Character == TEXT('-'))
					{
						++ValueEnd;
						continue;
					}
					break;
				}

				const FString ValueString = Source.Mid(ValueStart, ValueEnd - ValueStart);
				if (ValueString.IsEmpty())
				{
					return false;
				}

				OutWeight = FCString::Atof(*ValueString);
				return true;
			};
			const auto BuildFrontierWeightedRotation = [&](const float LeftWeight, const float RightWeight, const float SpineWeight) -> FQuat
			{
				const FQuat ConstraintRotations[] =
				{
					PairFrontierLeftConstraintSample->CandidateRotation,
					PairFrontierRightConstraintSample->CandidateRotation,
					PairFrontierSpineConstraintSample->CandidateRotation
				};
				const float Weights[] = { LeftWeight, RightWeight, SpineWeight };
				FVector4 WeightedRotationSum = FVector4::Zero();
				for (int32 SampleIndex = 0; SampleIndex < UE_ARRAY_COUNT(Weights); ++SampleIndex)
				{
					FQuat AlignedRotation = ConstraintRotations[SampleIndex];
					if ((BestRotationEvaluation.Rotation | AlignedRotation) < 0.0f)
					{
						AlignedRotation *= -1.0f;
					}
					WeightedRotationSum.X += AlignedRotation.X * Weights[SampleIndex];
					WeightedRotationSum.Y += AlignedRotation.Y * Weights[SampleIndex];
					WeightedRotationSum.Z += AlignedRotation.Z * Weights[SampleIndex];
					WeightedRotationSum.W += AlignedRotation.W * Weights[SampleIndex];
				}

				FQuat WeightedRotation(WeightedRotationSum.X, WeightedRotationSum.Y, WeightedRotationSum.Z, WeightedRotationSum.W);
				if (WeightedRotation.SizeSquared() <= KINDA_SMALL_NUMBER)
				{
					return BestRotationEvaluation.Rotation;
				}
				WeightedRotation.Normalize();
				if ((BestRotationEvaluation.Rotation | WeightedRotation) < 0.0f)
				{
					WeightedRotation *= -1.0f;
				}
				return WeightedRotation.GetNormalized();
			};
			const auto ShouldAcceptPairFrontierCandidate = [&](const FPhase1PelvisRotationEvaluation& CurrentEvaluation, const FPhase1PelvisRotationEvaluation& CandidateEvaluation) -> bool
			{
				const float CurrentWorstThigh = FMath::Max(CurrentEvaluation.LeftThighAngularErrorDeg, CurrentEvaluation.RightThighAngularErrorDeg);
				const bool bPrioritizeSpineBlocker = CurrentEvaluation.SpineAngularErrorDeg >= CurrentWorstThigh - KINDA_SMALL_NUMBER;
				return ShouldAcceptPhase1PairBlendFrontierCandidate(
					CurrentEvaluation.LeftThighAngularErrorDeg,
					CurrentEvaluation.RightThighAngularErrorDeg,
					CurrentEvaluation.SpineAngularErrorDeg,
					CandidateEvaluation.LeftThighAngularErrorDeg,
					CandidateEvaluation.RightThighAngularErrorDeg,
					CandidateEvaluation.SpineAngularErrorDeg,
					bPrioritizeSpineBlocker,
					SearchConfig.PairBlendFrontierBlockerPriorityGainWeight,
					SearchConfig.PairBlendFrontierSecondaryGainWeight,
					SearchConfig.PairBlendFrontierMaxPairedRegressionDeg);
			};

			FPhase1PelvisRotationEvaluation PairFrontierEvaluation = BestRotationEvaluation;
			float BaseLeftWeight = 0.10f;
			float BaseRightWeight = 0.30f;
			float BaseSpineWeight = 0.60f;
			const bool bHasExplicitWeights =
				TryExtractPairBlendWeight(BestRotationEvaluation.Source, TEXT("thigh_l_"), BaseLeftWeight) &&
				TryExtractPairBlendWeight(BestRotationEvaluation.Source, TEXT("thigh_r_"), BaseRightWeight) &&
				TryExtractPairBlendWeight(BestRotationEvaluation.Source, TEXT("spine_01_"), BaseSpineWeight);
			if (!bHasExplicitWeights)
			{
				BaseLeftWeight = 0.10f;
				BaseRightWeight = 0.30f;
				BaseSpineWeight = 0.60f;
			}

			const float WeightRadius = FMath::Max(0.0f, SearchConfig.PairBlendFrontierWeightPerturbationRadius);
			static const float FrontierWeightOffsets[] = { -1.0f, -0.5f, 0.0f, 0.5f, 1.0f };
			for (const float LeftOffsetScale : FrontierWeightOffsets)
			{
				for (const float RightOffsetScale : FrontierWeightOffsets)
				{
					const float CandidateLeftWeight = BaseLeftWeight + (LeftOffsetScale * WeightRadius);
					const float CandidateRightWeight = BaseRightWeight + (RightOffsetScale * WeightRadius);
					const float CandidateSpineWeight = 1.0f - CandidateLeftWeight - CandidateRightWeight;
					if (CandidateLeftWeight <= 0.0f || CandidateRightWeight <= 0.0f || CandidateSpineWeight <= 0.0f)
					{
						continue;
					}

					const FPhase1PelvisRotationEvaluation CandidateEvaluation = BuildRotationEvaluation(
						BuildFrontierWeightedRotation(CandidateLeftWeight, CandidateRightWeight, CandidateSpineWeight),
						FString::Printf(TEXT("pair_frontier_weight_thigh_l_%.2f_thigh_r_%.2f_spine_01_%.2f"), CandidateLeftWeight, CandidateRightWeight, CandidateSpineWeight));
					if (bProtectLiveTilt && !CandidateEvaluation.bTiltAdmissible)
					{
						continue;
					}
					if (ShouldAcceptPairFrontierCandidate(PairFrontierEvaluation, CandidateEvaluation) ||
						IsBetterRotationEvaluation(CandidateEvaluation, PairFrontierEvaluation))
					{
						PairFrontierEvaluation = CandidateEvaluation;
					}
				}
			}

			const float PitchRadiusDeg = FMath::Max(0.0f, SearchConfig.PairBlendFrontierPitchDeltaRadiusDeg);
			const float RollRadiusDeg = FMath::Max(0.0f, SearchConfig.PairBlendFrontierRollDeltaRadiusDeg);
			static const float FrontierPitchRollOffsets[] = { -1.0f, -0.5f, 0.0f, 0.5f, 1.0f };
			for (const float PitchOffsetScale : FrontierPitchRollOffsets)
			{
				for (const float RollOffsetScale : FrontierPitchRollOffsets)
				{
					const float PitchDeg = PitchOffsetScale * PitchRadiusDeg;
					const float RollDeg = RollOffsetScale * RollRadiusDeg;
					if (FMath::IsNearlyZero(PitchDeg) && FMath::IsNearlyZero(RollDeg))
					{
						continue;
					}

					const FQuat PitchDelta(FVector::RightVector, FMath::DegreesToRadians(PitchDeg));
					const FQuat RollDelta(FVector::ForwardVector, FMath::DegreesToRadians(RollDeg));
					const FPhase1PelvisRotationEvaluation CandidateEvaluation = BuildRotationEvaluation(
						(RollDelta * PitchDelta * PairFrontierEvaluation.Rotation).GetNormalized(),
						FString::Printf(TEXT("pair_frontier_pitch%.2f_roll%.2f"), PitchDeg, RollDeg));
					if (bProtectLiveTilt && !CandidateEvaluation.bTiltAdmissible)
					{
						continue;
					}
					if (ShouldAcceptPairFrontierCandidate(PairFrontierEvaluation, CandidateEvaluation) ||
						IsBetterRotationEvaluation(CandidateEvaluation, PairFrontierEvaluation))
					{
						PairFrontierEvaluation = CandidateEvaluation;
					}
				}
			}

			if (SearchConfig.bEnablePairBlendFrontierInterpolationPass)
			{
				const FPhase1PelvisRotationEvaluation SpineFrontierEvaluation = RefineBySpineConstraintInterpolationSweep(
					PairFrontierEvaluation,
					TEXT("pair_frontier_spine_neighbor"),
					bProtectLiveTilt);
				FPhase1PelvisRotationEvaluation ThighFrontierEvaluation = PairFrontierEvaluation;
				if (ShouldRunWorstThighConstraintInterpolationSweep(
						PairFrontierEvaluation.LeftThighAngularErrorDeg,
						PairFrontierEvaluation.RightThighAngularErrorDeg,
						PairFrontierEvaluation.SpineAngularErrorDeg))
				{
					ThighFrontierEvaluation = RefineByWorstThighConstraintInterpolationSweep(
						PairFrontierEvaluation,
						TEXT("pair_frontier_thigh_neighbor"),
						bProtectLiveTilt);
				}

				static const float PairFrontierBlendWeights[] = { 0.20f, 0.40f, 0.60f, 0.80f };
				for (const float BlendWeight : PairFrontierBlendWeights)
				{
					const FPhase1PelvisRotationEvaluation CandidateEvaluation = BuildRotationEvaluation(
						FQuat::Slerp(SpineFrontierEvaluation.Rotation, ThighFrontierEvaluation.Rotation, BlendWeight).GetNormalized(),
						FString::Printf(TEXT("pair_frontier_interp_a%.2f"), BlendWeight));
					if (bProtectLiveTilt && !CandidateEvaluation.bTiltAdmissible)
					{
						continue;
					}
					if (ShouldAcceptPairFrontierCandidate(PairFrontierEvaluation, CandidateEvaluation) ||
						IsBetterRotationEvaluation(CandidateEvaluation, PairFrontierEvaluation))
					{
						PairFrontierEvaluation = CandidateEvaluation;
					}
				}
			}

			if (ShouldAcceptPairFrontierCandidate(BestRotationEvaluation, PairFrontierEvaluation))
			{
				BestRotationEvaluation = PairFrontierEvaluation;
			}
		}
	}

	FPhase1PelvisRotationEvaluation AppliedRotationEvaluation = BestRotationEvaluation;
	const UWorld* const World = GetWorld();
	const float Phase1PelvisRotationStepDeg =
		FMath::Max(0.0f, EffectiveSettings.MaxAngularStepDegreesPerSecond) *
		AutoCalibClampStrengthScale *
		static_cast<float>(World ? World->GetDeltaSeconds() : 0.0);
	if (Phase1PelvisRotationStepDeg > UE_SMALL_NUMBER)
	{
		const FQuat CurrentPelvisRotation = PelvisBody->GetUnrealWorldTransform().GetRotation();
		const FQuat StepLimitedRotation = LimitTargetRotationStep(
			CurrentPelvisRotation,
			BestRotationEvaluation.Rotation,
			Phase1PelvisRotationStepDeg);
		if (!StepLimitedRotation.Equals(BestRotationEvaluation.Rotation, KINDA_SMALL_NUMBER))
		{
			const FPhase1PelvisRotationEvaluation StepLimitedEvaluation = BuildRotationEvaluation(
				StepLimitedRotation,
				FString::Printf(TEXT("%s_step_limited_%.2f"),
					*BestRotationEvaluation.Source,
					Phase1PelvisRotationStepDeg));
			if (ShouldAcceptStepLimitedPhase1PelvisRotation(
					BestRotationEvaluation.bTiltAdmissible,
					BestRotationEvaluation.bRootOnAngularReady,
					BestRotationEvaluation.bRootOnReadinessMarginSatisfied,
					StepLimitedEvaluation.bTiltAdmissible,
					StepLimitedEvaluation.bRootOnAngularReady,
					StepLimitedEvaluation.bRootOnReadinessMarginSatisfied))
			{
				AppliedRotationEvaluation = StepLimitedEvaluation;
			}
		}
	}
	if (bProtectLiveTilt)
	{
		constexpr float Phase1PelvisCouplingTiltMarginDeg = 0.5f;
		const float TargetMaxTiltDeg = FMath::Max(0.0f, EffectiveSettings.BalancePhase2EntryMaxRootTiltDeg - Phase1PelvisCouplingTiltMarginDeg);
		if (LiveRotationEvaluation.TiltDeg <= TargetMaxTiltDeg + KINDA_SMALL_NUMBER &&
			AppliedRotationEvaluation.TiltDeg > TargetMaxTiltDeg + KINDA_SMALL_NUMBER)
		{
			FPhase1PelvisRotationEvaluation MarginRotationEvaluation = LiveRotationEvaluation;
			float LowAlpha = 0.0f;
			float HighAlpha = 1.0f;
			for (int32 IterationIndex = 0; IterationIndex < 12; ++IterationIndex)
			{
				const float MidAlpha = 0.5f * (LowAlpha + HighAlpha);
				const FQuat CandidateRotation = FQuat::Slerp(
					LiveRotationEvaluation.Rotation,
					AppliedRotationEvaluation.Rotation,
					MidAlpha).GetNormalized();
				const FPhase1PelvisRotationEvaluation CandidateEvaluation = BuildRotationEvaluation(
					CandidateRotation,
					FString::Printf(TEXT("%s_tilt_margin_%.2f_a%.3f"),
						*AppliedRotationEvaluation.Source,
						TargetMaxTiltDeg,
						MidAlpha));
				if (CandidateEvaluation.TiltDeg <= TargetMaxTiltDeg + KINDA_SMALL_NUMBER)
				{
					MarginRotationEvaluation = CandidateEvaluation;
					LowAlpha = MidAlpha;
				}
				else
				{
					HighAlpha = MidAlpha;
				}
			}

			AppliedRotationEvaluation = MarginRotationEvaluation;
		}
	}
	if (ShouldRunSpineOnlyRootOnReadinessRescueSweep(
			AppliedRotationEvaluation.LeftThighAngularErrorDeg,
			AppliedRotationEvaluation.RightThighAngularErrorDeg,
			AppliedRotationEvaluation.SpineAngularErrorDeg))
	{
		FPhase1PelvisRotationEvaluation AppliedSpineRescueEvaluation = RefineByFocusedConstraintDelta(
			AppliedRotationEvaluation,
			TEXT("spine_01"),
			TEXT("applied_spine_rescue_focus_delta"),
			bProtectLiveTilt,
			true);
		AppliedSpineRescueEvaluation = RefineBySpineOnlyReadinessRescueSweep(
			AppliedSpineRescueEvaluation,
			TEXT("applied_spine_rescue_sweep"),
			bProtectLiveTilt);
		if (ShouldAcceptSpineOnlyRootOnReadinessRescueCandidate(
				AppliedRotationEvaluation.LeftThighAngularErrorDeg,
				AppliedRotationEvaluation.RightThighAngularErrorDeg,
				AppliedRotationEvaluation.SpineAngularErrorDeg,
				AppliedSpineRescueEvaluation.LeftThighAngularErrorDeg,
				AppliedSpineRescueEvaluation.RightThighAngularErrorDeg,
				AppliedSpineRescueEvaluation.SpineAngularErrorDeg))
		{
			AppliedRotationEvaluation = AppliedSpineRescueEvaluation;
		}
	}

	DesiredPelvisRotation = AppliedRotationEvaluation.Rotation;
	PelvisRotationSource = AppliedRotationEvaluation.Source;
	DesiredPelvisLocation = FVector::ZeroVector;
	DesiredPelvisLocationSamples = 0;
	for (const FPhase1ConstraintRotationSample& Sample : RotationSamples)
	{
		if (!Sample.bValid || Sample.ParentConstraintLocalRotation.Equals(FQuat::Identity) && Sample.ChildAnchorWorld.IsNearlyZero())
		{
			continue;
		}

		if (PhysicsAsset)
		{
			const int32 ConstraintIndex = PhysicsAsset->FindConstraintIndex(Sample.ChildBoneName, RootBoneName);
			if (ConstraintIndex != INDEX_NONE && PhysicsAsset->ConstraintSetup.IsValidIndex(ConstraintIndex))
			{
				const UPhysicsConstraintTemplate* const ConstraintTemplate = PhysicsAsset->ConstraintSetup[ConstraintIndex];
				const FConstraintInstance* const ConstraintInstance = ConstraintTemplate ? &ConstraintTemplate->DefaultInstance : nullptr;
				if (ConstraintInstance)
				{
					const FVector PelvisAnchorOffsetWorld = DesiredPelvisRotation.RotateVector(ConstraintInstance->Pos2);
					DesiredPelvisLocation += Sample.ChildAnchorWorld - PelvisAnchorOffsetWorld;
					++DesiredPelvisLocationSamples;
				}
			}
		}
	}
	if (DesiredPelvisLocationSamples == 0)
	{
		if (!bPhase1PelvisCouplingSkipLogged)
		{
			PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimBalance] PHASE1_PELVIS_COUPLING_SKIPPED reason=noResolvedAnchorLocations state=%s"), GetRuntimeStateName(RuntimeState));
			bPhase1PelvisCouplingSkipLogged = true;
		}
		return;
	}
	DesiredPelvisLocation /= static_cast<float>(DesiredPelvisLocationSamples);
	FTransform DesiredPelvisTransform = PelvisBody->GetUnrealWorldTransform();
	DesiredPelvisTransform.SetRotation(DesiredPelvisRotation);
	DesiredPelvisTransform.SetLocation(DesiredPelvisLocation);
	PelvisBody->SetBodyTransform(DesiredPelvisTransform, ETeleportType::TeleportPhysics, true);
	bPhase1PelvisCouplingSkipLogged = false;

	const FVector SolvedPelvisLocation = BalanceTransitionSets::ResolveBodyOrBoneLocationCm(Mesh, RootBoneName);
	BalanceTransitionSets::FDirectPelvisLinkForensicRecord PelvisThighLRecord;
	BalanceTransitionSets::FDirectPelvisLinkForensicRecord PelvisThighRRecord;
	BalanceTransitionSets::FDirectPelvisLinkForensicRecord PelvisSpine01Record;
	BalanceTransitionSets::BuildDirectPelvisLinkForensicRecord(Mesh, RootBoneName, TEXT("thigh_l"), PelvisThighLRecord);
	BalanceTransitionSets::BuildDirectPelvisLinkForensicRecord(Mesh, RootBoneName, TEXT("thigh_r"), PelvisThighRRecord);
	BalanceTransitionSets::BuildDirectPelvisLinkForensicRecord(Mesh, RootBoneName, TEXT("spine_01"), PelvisSpine01Record);
	const float PelvisThighLErrorCm = PelvisThighLRecord.bConstraintFound ? PelvisThighLRecord.AnchorDistanceCm : PelvisThighLRecord.BodyOriginDistanceCm;
	const float PelvisThighRErrorCm = PelvisThighRRecord.bConstraintFound ? PelvisThighRRecord.AnchorDistanceCm : PelvisThighRRecord.BodyOriginDistanceCm;
	const float PelvisSpine01ErrorCm = PelvisSpine01Record.bConstraintFound ? PelvisSpine01Record.AnchorDistanceCm : PelvisSpine01Record.BodyOriginDistanceCm;
	FString SolvedTiltSource;
	const float SolvedTiltDeg = ResolvePhase1Uprightness(
		Mesh, GetOwner(), RootBoneName,
		bHasNeutralPelvisActorRelativeRotation, NeutralPelvisActorRelativeRotation, SolvedTiltSource);
	const int32 TiltProtectionForced = (bProtectLiveTilt &&
		BestUnconstrainedRotationEvaluation.MaxAngularErrorDeg < TNumericLimits<float>::Max() &&
		!BestUnconstrainedRotationEvaluation.bTiltAdmissible &&
		bHasTiltAdmissibleRotationEvaluation) ? 1 : 0;
	LastPhase1PelvisCouplingRotationForensics.bLiveTiltProtected = bProtectLiveTilt;
	LastPhase1PelvisCouplingRotationForensics.bTiltProtectionForced = TiltProtectionForced != 0;
	LastPhase1PelvisCouplingRotationForensics.UnconstrainedTiltDeg = BestUnconstrainedRotationEvaluation.TiltDeg;
	LastPhase1PelvisCouplingRotationForensics.UnconstrainedAngularThresholdOverflowDeg = BestUnconstrainedRotationEvaluation.AngularThresholdOverflowDeg;
	LastPhase1PelvisCouplingRotationForensics.UnconstrainedLeftThighAngularErrorDeg = BestUnconstrainedRotationEvaluation.LeftThighAngularErrorDeg;
	LastPhase1PelvisCouplingRotationForensics.UnconstrainedRightThighAngularErrorDeg = BestUnconstrainedRotationEvaluation.RightThighAngularErrorDeg;
	LastPhase1PelvisCouplingRotationForensics.UnconstrainedSpineAngularErrorDeg = BestUnconstrainedRotationEvaluation.SpineAngularErrorDeg;
	LastPhase1PelvisCouplingRotationForensics.AppliedTiltDeg = AppliedRotationEvaluation.TiltDeg;
	LastPhase1PelvisCouplingRotationForensics.AppliedAngularThresholdOverflowDeg = AppliedRotationEvaluation.AngularThresholdOverflowDeg;
	LastPhase1PelvisCouplingRotationForensics.bTriggeredTiltSpineRescuePath = bTriggeredTiltSpineRescuePath;
	LastPhase1PelvisCouplingRotationForensics.bTriggeredForensicSpineRescuePath = bTriggeredForensicSpineRescuePath;
	LastPhase1PelvisCouplingRotationForensics.bRanSpineBiasedDirectBlend = bRanSpineBiasedDirectBlend;
	LastPhase1PelvisCouplingRotationForensics.bRanPairBlendSeeds = bRanPairBlendSeeds;
	LastPhase1PelvisCouplingRotationForensics.bRanConstraintInterpolation = bRanConstraintInterpolation;
	LastPhase1PelvisCouplingRotationForensics.bRanWorstThighInterpolation = bRanWorstThighInterpolation;
	LastPhase1PelvisCouplingRotationForensics.bRanFocusedDelta = bHasTiltAdmissibleRotationEvaluation || bTriggeredForensicSpineRescuePath || bRanWorstThighInterpolation;
	LastPhase1PelvisCouplingRotationForensics.bRanCoupledTradeControl = bRanCoupledTradeControl;
	LastPhase1PelvisCouplingRotationForensics.bRanPairBlendFrontierFollowThrough = bRanPairBlendFrontierFollowThrough;
	LastPhase1PelvisCouplingRotationForensics.TiltSpineRescueSpineAngularErrorDeg =
		bTriggeredTiltSpineRescuePath ? TiltSpineRescueEvaluation.SpineAngularErrorDeg : 0.0f;
	LastPhase1PelvisCouplingRotationForensics.ForensicSpineRescueSpineAngularErrorDeg =
		bTriggeredForensicSpineRescuePath ? ForensicSpineRescueEvaluation.SpineAngularErrorDeg : 0.0f;
	LastPhase1PelvisCouplingRotationForensics.TiltSpineRescueSource =
		bTriggeredTiltSpineRescuePath ? TiltSpineRescueEvaluation.Source : FString();
	LastPhase1PelvisCouplingRotationForensics.ForensicSpineRescueSource =
		bTriggeredForensicSpineRescuePath ? ForensicSpineRescueEvaluation.Source : FString();
	LastPhase1PelvisCouplingRotationForensics.WinningSearchSource = AppliedRotationEvaluation.Source;
	LastPhase1PelvisCouplingRotationForensics.WinningSearchFamily =
		Phase1PelvisCouplingSearchFamilyToString(ClassifyPhase1PelvisCouplingSearchFamily(AppliedRotationEvaluation.Source));
	BuildPhase1PelvisCouplingExecutedFamilies(
		SearchConfig,
		LastPhase1PelvisCouplingRotationForensics,
		AppliedRotationEvaluation.Source,
		LastPhase1PelvisCouplingRotationForensics.ExecutedSearchFamilies);
	LastPhase1PelvisCouplingRotationForensics.bCoupledTradeControlWon =
		ClassifyPhase1PelvisCouplingSearchFamily(AppliedRotationEvaluation.Source) == EPhase1PelvisCouplingSearchFamily::CoupledTradeControl;
	PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimBalance] PHASE1_PELVIS_COUPLING solvedLoc=(%.2f,%.2f,%.2f) rotationSource=%s solvedTiltDeg=%.2f tiltSource=%s tiltAdmissible=%d rotationScoreMax=%.2f rotationScoreMean=%.2f pelvisThighL=%.2f pelvisThighR=%.2f pelvisSpine01=%.2f pelvisThighLAngular=%.2f pelvisThighRAngular=%.2f pelvisSpine01Angular=%.2f pelvisThighLBodyOrigin=%.2f pelvisThighRBodyOrigin=%.2f pelvisSpine01BodyOrigin=%.2f state=%s"),
		SolvedPelvisLocation.X,
		SolvedPelvisLocation.Y,
		SolvedPelvisLocation.Z,
		*PelvisRotationSource,
		SolvedTiltDeg,
		*SolvedTiltSource,
		AppliedRotationEvaluation.bTiltAdmissible ? 1 : 0,
		AppliedRotationEvaluation.MaxAngularErrorDeg,
		AppliedRotationEvaluation.MeanAngularErrorDeg,
		PelvisThighLErrorCm,
		PelvisThighRErrorCm,
		PelvisSpine01ErrorCm,
		PelvisThighLRecord.ConstraintAngularErrorDeg,
		PelvisThighRRecord.ConstraintAngularErrorDeg,
		PelvisSpine01Record.ConstraintAngularErrorDeg,
		PelvisThighLRecord.BodyOriginDistanceCm,
		PelvisThighRRecord.BodyOriginDistanceCm,
		PelvisSpine01Record.BodyOriginDistanceCm,
		GetRuntimeStateName(RuntimeState));
	PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimBalance] PHASE1_PELVIS_COUPLING_ROTATION_FORENSICS liveTiltProtected=%d tiltProtectionForced=%d liveSource=%s liveTiltDeg=%.2f liveOverflowDeg=%.2f liveL=%.2f liveR=%.2f liveSpine=%.2f unconstrainedSource=%s unconstrainedTiltDeg=%.2f unconstrainedOverflowDeg=%.2f unconstrainedL=%.2f unconstrainedR=%.2f unconstrainedSpine=%.2f tiltSource=%s tiltTiltDeg=%.2f tiltOverflowDeg=%.2f tiltL=%.2f tiltR=%.2f tiltSpine=%.2f tiltSpineRescueTriggered=%d tiltSpineRescueSpine=%.2f tiltSpineRescueSource=%s forensicSpineRescueTriggered=%d forensicSpineRescueSpine=%.2f forensicSpineRescueSource=%s appliedSource=%s appliedTiltDeg=%.2f appliedOverflowDeg=%.2f appliedL=%.2f appliedR=%.2f appliedSpine=%.2f state=%s"),
		bProtectLiveTilt ? 1 : 0,
		TiltProtectionForced,
		*LiveRotationEvaluation.Source,
		LiveRotationEvaluation.TiltDeg,
		LiveRotationEvaluation.AngularThresholdOverflowDeg,
		LiveRotationEvaluation.LeftThighAngularErrorDeg,
		LiveRotationEvaluation.RightThighAngularErrorDeg,
		LiveRotationEvaluation.SpineAngularErrorDeg,
		*BestUnconstrainedRotationEvaluation.Source,
		BestUnconstrainedRotationEvaluation.TiltDeg,
		BestUnconstrainedRotationEvaluation.AngularThresholdOverflowDeg,
		BestUnconstrainedRotationEvaluation.LeftThighAngularErrorDeg,
		BestUnconstrainedRotationEvaluation.RightThighAngularErrorDeg,
		BestUnconstrainedRotationEvaluation.SpineAngularErrorDeg,
		bHasTiltAdmissibleRotationEvaluation ? *BestTiltAdmissibleRotationEvaluation.Source : TEXT("none"),
		bHasTiltAdmissibleRotationEvaluation ? BestTiltAdmissibleRotationEvaluation.TiltDeg : 0.0f,
		bHasTiltAdmissibleRotationEvaluation ? BestTiltAdmissibleRotationEvaluation.AngularThresholdOverflowDeg : 0.0f,
		bHasTiltAdmissibleRotationEvaluation ? BestTiltAdmissibleRotationEvaluation.LeftThighAngularErrorDeg : 0.0f,
		bHasTiltAdmissibleRotationEvaluation ? BestTiltAdmissibleRotationEvaluation.RightThighAngularErrorDeg : 0.0f,
		bHasTiltAdmissibleRotationEvaluation ? BestTiltAdmissibleRotationEvaluation.SpineAngularErrorDeg : 0.0f,
		bTriggeredTiltSpineRescuePath ? 1 : 0,
		bTriggeredTiltSpineRescuePath ? TiltSpineRescueEvaluation.SpineAngularErrorDeg : 0.0f,
		bTriggeredTiltSpineRescuePath ? *TiltSpineRescueEvaluation.Source : TEXT("none"),
		bTriggeredForensicSpineRescuePath ? 1 : 0,
		bTriggeredForensicSpineRescuePath ? ForensicSpineRescueEvaluation.SpineAngularErrorDeg : 0.0f,
		bTriggeredForensicSpineRescuePath ? *ForensicSpineRescueEvaluation.Source : TEXT("none"),
		*AppliedRotationEvaluation.Source,
		AppliedRotationEvaluation.TiltDeg,
		AppliedRotationEvaluation.AngularThresholdOverflowDeg,
		AppliedRotationEvaluation.LeftThighAngularErrorDeg,
		AppliedRotationEvaluation.RightThighAngularErrorDeg,
		AppliedRotationEvaluation.SpineAngularErrorDeg,
		GetRuntimeStateName(RuntimeState));
}
