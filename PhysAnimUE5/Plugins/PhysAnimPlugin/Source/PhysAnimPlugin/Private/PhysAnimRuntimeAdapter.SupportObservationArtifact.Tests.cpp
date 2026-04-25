#include "PhysAnimRuntimeAdapter.h"
#include "Misc/AutomationTest.h"

namespace
{
	FPhysAnimSupportBodyMapping SupportObservationArtifact_MakeSupportBodyMapping(const FName BodyName, const EPhysAnimSupportSide SupportSide)
	{
		FPhysAnimSupportBodyMapping Mapping;
		Mapping.BodyName = BodyName;
		Mapping.SupportSide = SupportSide;
		return Mapping;
	}

	FPhysAnimSupportHitRecord SupportObservationArtifact_MakeSupportHit(
		const FName BodyName,
		const FVector& WorldPositionCm,
		const bool bBlockingHit = true,
		const bool bFromWorldStatic = true)
	{
		FPhysAnimSupportHitRecord Hit;
		Hit.BodyName = BodyName;
		Hit.WorldPositionCm = WorldPositionCm;
		Hit.bBlockingHit = bBlockingHit;
		Hit.bFromWorldStatic = bFromWorldStatic;
		return Hit;
	}

	FPhysAnimFailureCandidate SupportObservationArtifact_MakeFailureCandidate(EPhysAnimTerminalReason Reason, int64 TerminalSubstepTimestamp)
	{
		FPhysAnimFailureCandidate Candidate;
		Candidate.TerminalReason = Reason;
		Candidate.TerminalSubstepTimestamp = TerminalSubstepTimestamp;
		return Candidate;
	}

	void SupportObservationArtifact_AddLeftFootSquareHits(FPhysAnimSupportHitSnapshotCaptureInput& Input)
	{
		Input.SupportBodies.Add(SupportObservationArtifact_MakeSupportBodyMapping(TEXT("foot_l"), EPhysAnimSupportSide::Left));
		Input.Hits.Add(SupportObservationArtifact_MakeSupportHit(TEXT("foot_l"), FVector(0.0, 0.0, 0.0)));
		Input.Hits.Add(SupportObservationArtifact_MakeSupportHit(TEXT("foot_l"), FVector(10.0, 0.0, 0.0)));
		Input.Hits.Add(SupportObservationArtifact_MakeSupportHit(TEXT("foot_l"), FVector(10.0, 10.0, 0.0)));
		Input.Hits.Add(SupportObservationArtifact_MakeSupportHit(TEXT("foot_l"), FVector(0.0, 10.0, 0.0)));
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimRuntimeAdapterSupportObservationArtifactTest,
		"PhysAnim.RuntimeAdapter.SupportObservationArtifact",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimRuntimeAdapterSupportObservationArtifactTest::RunTest(const FString& Parameters)
	{
		{
			FPhysAnimSupportObservationInput ObservationInput;
			ObservationInput.HitSnapshot.ComProxyPosCm = FVector2D(5.0, 5.0);
			ObservationInput.HitSnapshot.DeltaMs = 10.0;
			SupportObservationArtifact_AddLeftFootSquareHits(ObservationInput.HitSnapshot);

			FPhysAnimSupportObservationArtifactInput ArtifactInput;
			ArtifactInput.Observation = PhysAnimRuntimeAdapter::BuildSupportObservationFromHits(ObservationInput);
			ArtifactInput.Values.TerminalSubstepTimestamp = 100;

			const FPhysAnimRunArtifactSnapshot Artifact = PhysAnimRuntimeAdapter::BuildSupportObservationArtifactSnapshot(ArtifactInput);

			TestEqual(
				TEXT("SUPPORT-OBS-ARTIFACT-01 valid observation has no terminal reason"),
				static_cast<uint8>(Artifact.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::None));
			TestEqual(TEXT("SUPPORT-OBS-ARTIFACT-01 active side count copied"), Artifact.ActiveSupportSideCount, 1);
			TestEqual(
				TEXT("SUPPORT-OBS-ARTIFACT-01 support mode copied"),
				static_cast<uint8>(Artifact.SupportMode),
				static_cast<uint8>(EPhysAnimSupportMode::SingleFootSurvival));
			TestTrue(TEXT("SUPPORT-OBS-ARTIFACT-01 proxy inside copied"), Artifact.ProxyInsideHull.IsSet() && Artifact.ProxyInsideHull.GetValue());
		}

		{
			FPhysAnimSupportObservationInput ObservationInput;
			ObservationInput.HitSnapshot.PreviousSupportGapTimerMs = 90.0;
			ObservationInput.HitSnapshot.DeltaMs = 20.0;
			ObservationInput.HitSnapshot.SupportGapMaxMs = 100.0;

			FPhysAnimSupportObservationArtifactInput ArtifactInput;
			ArtifactInput.Observation = PhysAnimRuntimeAdapter::BuildSupportObservationFromHits(ObservationInput);
			ArtifactInput.Values.TerminalSubstepTimestamp = 200;

			const FPhysAnimRunArtifactSnapshot Artifact = PhysAnimRuntimeAdapter::BuildSupportObservationArtifactSnapshot(ArtifactInput);

			TestEqual(
				TEXT("SUPPORT-OBS-ARTIFACT-02 support failure observation produces support failure artifact"),
				static_cast<uint8>(Artifact.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationSupportFailure));
			TestEqual(TEXT("SUPPORT-OBS-ARTIFACT-02 terminal timestamp copied"), Artifact.TerminalSubstepTimestamp, static_cast<int64>(200));
			TestEqual(TEXT("SUPPORT-OBS-ARTIFACT-02 support gap copied"), Artifact.SupportGapTimerMs, 110.0);
		}

		{
			FPhysAnimSupportObservationInput ObservationInput;
			ObservationInput.HitSnapshot.ComProxyPosCm = FVector2D(50.0, 50.0);
			ObservationInput.HitSnapshot.PreviousProxyOutsideHullDurationMs = 90.0;
			ObservationInput.HitSnapshot.DeltaMs = 20.0;
			ObservationInput.HitSnapshot.ProxyDriftLimitMs = 100.0;
			SupportObservationArtifact_AddLeftFootSquareHits(ObservationInput.HitSnapshot);

			FPhysAnimSupportObservationArtifactInput ArtifactInput;
			ArtifactInput.Observation = PhysAnimRuntimeAdapter::BuildSupportObservationFromHits(ObservationInput);
			ArtifactInput.Values.TerminalSubstepTimestamp = 300;

			const FPhysAnimRunArtifactSnapshot Artifact = PhysAnimRuntimeAdapter::BuildSupportObservationArtifactSnapshot(ArtifactInput);

			TestEqual(
				TEXT("SUPPORT-OBS-ARTIFACT-03 proxy failure observation produces proxy artifact"),
				static_cast<uint8>(Artifact.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion));
			TestFalse(TEXT("SUPPORT-OBS-ARTIFACT-03 proxy inside copied false"), Artifact.ProxyInsideHull.IsSet() && Artifact.ProxyInsideHull.GetValue());
			TestTrue(TEXT("SUPPORT-OBS-ARTIFACT-03 proxy outside duration copied"), Artifact.ProxyOutsideHullDurationMs.IsSet());
			TestEqual(TEXT("SUPPORT-OBS-ARTIFACT-03 proxy outside duration value"), Artifact.ProxyOutsideHullDurationMs.GetValue(), 110.0);
		}

		{
			FPhysAnimSupportObservationInput ObservationInput;
			ObservationInput.HitSnapshot.PreviousSupportGapTimerMs = 90.0;
			ObservationInput.HitSnapshot.DeltaMs = 20.0;
			ObservationInput.HitSnapshot.SupportGapMaxMs = 100.0;

			FPhysAnimSupportObservationArtifactInput ArtifactInput;
			ArtifactInput.Observation = PhysAnimRuntimeAdapter::BuildSupportObservationFromHits(ObservationInput);
			ArtifactInput.Values.TerminalSubstepTimestamp = 400;
			ArtifactInput.AdditionalFailureCandidates.Add(
				SupportObservationArtifact_MakeFailureCandidate(EPhysAnimTerminalReason::ActivationAuthorityConflict, 399));

			const FPhysAnimRunArtifactSnapshot Artifact = PhysAnimRuntimeAdapter::BuildSupportObservationArtifactSnapshot(ArtifactInput);

			TestEqual(
				TEXT("SUPPORT-OBS-ARTIFACT-04 earlier authority candidate wins temporal precedence"),
				static_cast<uint8>(Artifact.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationAuthorityConflict));
			TestEqual(TEXT("SUPPORT-OBS-ARTIFACT-04 earlier timestamp wins"), Artifact.TerminalSubstepTimestamp, static_cast<int64>(399));
		}

		{
			FPhysAnimSupportObservationInput ObservationInput;
			ObservationInput.HitSnapshot.PreviousSupportGapTimerMs = 90.0;
			ObservationInput.HitSnapshot.DeltaMs = 20.0;
			ObservationInput.HitSnapshot.SupportGapMaxMs = 100.0;

			FPhysAnimSupportObservationArtifactInput ArtifactInput;
			ArtifactInput.Observation = PhysAnimRuntimeAdapter::BuildSupportObservationFromHits(ObservationInput);
			ArtifactInput.Values.TerminalSubstepTimestamp = 500;
			ArtifactInput.AdditionalFailureCandidates.Add(
				SupportObservationArtifact_MakeFailureCandidate(EPhysAnimTerminalReason::ActivationAuthorityConflict, 500));

			const FPhysAnimRunArtifactSnapshot Artifact = PhysAnimRuntimeAdapter::BuildSupportObservationArtifactSnapshot(ArtifactInput);

			TestEqual(
				TEXT("SUPPORT-OBS-ARTIFACT-05 support failure outranks simultaneous authority conflict"),
				static_cast<uint8>(Artifact.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationSupportFailure));
			TestEqual(TEXT("SUPPORT-OBS-ARTIFACT-05 one co-terminal reason recorded"), Artifact.CoTerminalReasons.Num(), 1);
			TestTrue(
				TEXT("SUPPORT-OBS-ARTIFACT-05 authority conflict is co-terminal"),
				Artifact.CoTerminalReasons.Contains(EPhysAnimTerminalReason::ActivationAuthorityConflict));
		}

		{
			FPhysAnimSupportObservationInput ObservationInput;
			ObservationInput.HitSnapshot.ComProxyPosCm = FVector2D(5.0, 5.0);
			ObservationInput.HitSnapshot.SupportChurnCount = 4;
			ObservationInput.HitSnapshot.SupportChurnHz = 2.0;
			SupportObservationArtifact_AddLeftFootSquareHits(ObservationInput.HitSnapshot);

			FPhysAnimSupportObservationArtifactInput ArtifactInput;
			ArtifactInput.Observation = PhysAnimRuntimeAdapter::BuildSupportObservationFromHits(ObservationInput);
			ArtifactInput.Values.AttemptUuid = TEXT("support-observation-artifact-test");
			ArtifactInput.Values.TerminalSubstepTimestamp = 600;

			const FPhysAnimRunArtifactSnapshot Artifact = PhysAnimRuntimeAdapter::BuildSupportObservationArtifactSnapshot(ArtifactInput);

			TestEqual(TEXT("SUPPORT-OBS-ARTIFACT-06 attempt uuid preserved"), Artifact.AttemptUuid, FString(TEXT("support-observation-artifact-test")));
			TestEqual(TEXT("SUPPORT-OBS-ARTIFACT-06 support churn count copied"), Artifact.SupportChurnCount, 4);
			TestEqual(TEXT("SUPPORT-OBS-ARTIFACT-06 support churn hz copied"), Artifact.SupportChurnHz, 2.0);
			TestEqual(TEXT("SUPPORT-OBS-ARTIFACT-06 support hull area copied"), Artifact.SupportHullAreaCm2, ArtifactInput.Observation.Validation.SupportHullAreaCm2);
		}

		return true;
	}
}
