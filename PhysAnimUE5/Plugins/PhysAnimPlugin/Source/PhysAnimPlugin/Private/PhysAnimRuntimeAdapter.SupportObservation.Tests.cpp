#include "PhysAnimRuntimeAdapter.h"
#include "Misc/AutomationTest.h"

namespace
{
	FPhysAnimSupportBodyMapping SupportObservation_MakeSupportBodyMapping(const FName BodyName, const EPhysAnimSupportSide SupportSide)
	{
		FPhysAnimSupportBodyMapping Mapping;
		Mapping.BodyName = BodyName;
		Mapping.SupportSide = SupportSide;
		return Mapping;
	}

	FPhysAnimSupportHitRecord SupportObservation_MakeSupportHit(
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

	void SupportObservation_AddLeftFootSquareHits(FPhysAnimSupportHitSnapshotCaptureInput& Input)
	{
		Input.SupportBodies.Add(SupportObservation_MakeSupportBodyMapping(TEXT("foot_l"), EPhysAnimSupportSide::Left));
		Input.Hits.Add(SupportObservation_MakeSupportHit(TEXT("foot_l"), FVector(0.0, 0.0, 0.0)));
		Input.Hits.Add(SupportObservation_MakeSupportHit(TEXT("foot_l"), FVector(10.0, 0.0, 0.0)));
		Input.Hits.Add(SupportObservation_MakeSupportHit(TEXT("foot_l"), FVector(10.0, 10.0, 0.0)));
		Input.Hits.Add(SupportObservation_MakeSupportHit(TEXT("foot_l"), FVector(0.0, 10.0, 0.0)));
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimRuntimeAdapterSupportObservationTest,
		"PhysAnim.RuntimeAdapter.SupportObservation",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimRuntimeAdapterSupportObservationTest::RunTest(const FString& Parameters)
	{
		{
			FPhysAnimSupportObservationInput Input;
			Input.HitSnapshot.ComProxyPosCm = FVector2D(5.0, 5.0);
			Input.HitSnapshot.DeltaMs = 10.0;
			SupportObservation_AddLeftFootSquareHits(Input.HitSnapshot);

			const FPhysAnimSupportObservationResult Result = PhysAnimRuntimeAdapter::BuildSupportObservationFromHits(Input);

			TestTrue(TEXT("SUPPORT-OBSERVATION-01 valid square hits produce valid observation"), Result.bObservationValid);
			TestTrue(TEXT("SUPPORT-OBSERVATION-01 validation passes"), Result.Validation.bSupportContractPassed);
			TestEqual(
				TEXT("SUPPORT-OBSERVATION-01 terminal reason is None"),
				static_cast<uint8>(Result.Validation.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::None));
			TestEqual(TEXT("SUPPORT-OBSERVATION-01 active side count copied to validation"), Result.Validation.ActiveSupportSideCount, 1);
			TestEqual(
				TEXT("SUPPORT-OBSERVATION-01 support mode copied to validation"),
				static_cast<uint8>(Result.Validation.SupportMode),
				static_cast<uint8>(EPhysAnimSupportMode::SingleFootSurvival));
		}

		{
			FPhysAnimSupportObservationInput Input;
			Input.HitSnapshot.PreviousSupportGapTimerMs = 90.0;
			Input.HitSnapshot.DeltaMs = 20.0;
			Input.HitSnapshot.SupportGapMaxMs = 100.0;

			const FPhysAnimSupportObservationResult Result = PhysAnimRuntimeAdapter::BuildSupportObservationFromHits(Input);

			TestFalse(TEXT("SUPPORT-OBSERVATION-02 airborne gap breach invalidates observation"), Result.bObservationValid);
			TestFalse(TEXT("SUPPORT-OBSERVATION-02 validation fails"), Result.Validation.bSupportContractPassed);
			TestEqual(
				TEXT("SUPPORT-OBSERVATION-02 terminal reason is support failure"),
				static_cast<uint8>(Result.Validation.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationSupportFailure));
			TestEqual(TEXT("SUPPORT-OBSERVATION-02 support gap copied to validation"), Result.Validation.SupportGapTimerMs, 110.0);
		}

		{
			FPhysAnimSupportObservationInput Input;
			Input.HitSnapshot.ComProxyPosCm = FVector2D(50.0, 50.0);
			Input.HitSnapshot.PreviousProxyOutsideHullDurationMs = 90.0;
			Input.HitSnapshot.DeltaMs = 20.0;
			Input.HitSnapshot.ProxyDriftLimitMs = 100.0;
			SupportObservation_AddLeftFootSquareHits(Input.HitSnapshot);

			const FPhysAnimSupportObservationResult Result = PhysAnimRuntimeAdapter::BuildSupportObservationFromHits(Input);

			TestFalse(TEXT("SUPPORT-OBSERVATION-03 proxy outside invalidates observation"), Result.bObservationValid);
			TestFalse(TEXT("SUPPORT-OBSERVATION-03 validation fails"), Result.Validation.bSupportContractPassed);
			TestEqual(
				TEXT("SUPPORT-OBSERVATION-03 terminal reason is proxy outside support region"),
				static_cast<uint8>(Result.Validation.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion));
			TestTrue(TEXT("SUPPORT-OBSERVATION-03 proxy inside copied as false"),
				Result.Validation.ProxyInsideHull.IsSet() && !Result.Validation.ProxyInsideHull.GetValue());
			TestTrue(TEXT("SUPPORT-OBSERVATION-03 proxy outside duration copied"),
				Result.Validation.ProxyOutsideHullDurationMs.IsSet() && Result.Validation.ProxyOutsideHullDurationMs.GetValue() == 110.0);
		}

		{
			FPhysAnimSupportObservationInput Input;
			Input.HitSnapshot.SupportBodies.Add(SupportObservation_MakeSupportBodyMapping(TEXT("foot_l"), EPhysAnimSupportSide::Left));
			Input.HitSnapshot.Hits.Add(SupportObservation_MakeSupportHit(TEXT("foot_l"), FVector(0.0, 0.0, 0.0), false, true));
			Input.HitSnapshot.Hits.Add(SupportObservation_MakeSupportHit(TEXT("foot_l"), FVector(10.0, 0.0, 0.0), true, false));
			Input.HitSnapshot.Hits.Add(SupportObservation_MakeSupportHit(TEXT("hand_l"), FVector(20.0, 0.0, 0.0), true, true));
			Input.HitSnapshot.PreviousSupportGapTimerMs = 30.0;
			Input.HitSnapshot.DeltaMs = 20.0;
			Input.HitSnapshot.SupportGapMaxMs = 100.0;

			const FPhysAnimSupportObservationResult Result = PhysAnimRuntimeAdapter::BuildSupportObservationFromHits(Input);

			TestTrue(TEXT("SUPPORT-OBSERVATION-04 invalid/unmapped hits stay under gap limit"), Result.bObservationValid);
			TestTrue(TEXT("SUPPORT-OBSERVATION-04 validation passes"), Result.Validation.bSupportContractPassed);
			TestEqual(TEXT("SUPPORT-OBSERVATION-04 no active support"), Result.Validation.ActiveSupportSideCount, 0);
			TestEqual(TEXT("SUPPORT-OBSERVATION-04 support gap copied"), Result.Validation.SupportGapTimerMs, 50.0);
			TestEqual(
				TEXT("SUPPORT-OBSERVATION-04 support mode is TransientRecovery"),
				static_cast<uint8>(Result.Validation.SupportMode),
				static_cast<uint8>(EPhysAnimSupportMode::TransientRecovery));
		}

		{
			FPhysAnimSupportObservationInput Input;
			Input.HitSnapshot.ComProxyPosCm = FVector2D(5.0, 5.0);
			Input.HitSnapshot.SupportChurnCount = 3;
			Input.HitSnapshot.SupportChurnHz = 1.5;
			SupportObservation_AddLeftFootSquareHits(Input.HitSnapshot);

			const FPhysAnimSupportObservationResult Result = PhysAnimRuntimeAdapter::BuildSupportObservationFromHits(Input);

			TestEqual(TEXT("SUPPORT-OBSERVATION-05 snapshot active side copied"), Result.Snapshot.ActiveSupportSideCount, Result.Validation.ActiveSupportSideCount);
			TestEqual(TEXT("SUPPORT-OBSERVATION-05 snapshot hull area copied"), Result.Snapshot.SupportHullAreaCm2, Result.Validation.SupportHullAreaCm2);
			TestEqual(TEXT("SUPPORT-OBSERVATION-05 support churn count copied"), Result.Validation.SupportChurnCount, 3);
			TestEqual(TEXT("SUPPORT-OBSERVATION-05 support churn hz copied"), Result.Validation.SupportChurnHz, 1.5);
			TestEqual(
				TEXT("SUPPORT-OBSERVATION-05 observation valid mirrors validation"),
				Result.bObservationValid,
				Result.Validation.bSupportContractPassed);
		}

		return true;
	}
}
