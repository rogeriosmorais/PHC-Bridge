#include "PhysAnimRuntimeAdapter.h"

#include "Components/BoxComponent.h"
#include "Misc/AutomationTest.h"

namespace
{
	UBoxComponent* MakeHitComponent(EComponentMobility::Type Mobility)
	{
		UBoxComponent* Component = NewObject<UBoxComponent>();
		Component->SetMobility(Mobility);
		return Component;
	}

	FPhysAnimSupportBodyMapping MakeSupportBodyMapping(const FName BodyName, const EPhysAnimSupportSide SupportSide)
	{
		FPhysAnimSupportBodyMapping Mapping;
		Mapping.BodyName = BodyName;
		Mapping.SupportSide = SupportSide;
		return Mapping;
	}

	FHitResult MakeHitResult(
		const FName BoneName,
		const FVector& ImpactPoint,
		UPrimitiveComponent* Component,
		const bool bBlockingHit = true)
	{
		FHitResult Hit;
		Hit.BoneName = BoneName;
		Hit.ImpactPoint = ImpactPoint;
		Hit.Location = ImpactPoint;
		Hit.Component = Component;
		Hit.bBlockingHit = bBlockingHit;
		return Hit;
	}

	void AddLeftFootSquareHitResults(FPhysAnimSupportHitResultObservationInput& Input, UPrimitiveComponent* Component)
	{
		Input.SupportBodies.Add(MakeSupportBodyMapping(TEXT("foot_l"), EPhysAnimSupportSide::Left));
		Input.HitResults.Add(MakeHitResult(TEXT("foot_l"), FVector(0.0, 0.0, 0.0), Component));
		Input.HitResults.Add(MakeHitResult(TEXT("foot_l"), FVector(10.0, 0.0, 0.0), Component));
		Input.HitResults.Add(MakeHitResult(TEXT("foot_l"), FVector(10.0, 10.0, 0.0), Component));
		Input.HitResults.Add(MakeHitResult(TEXT("foot_l"), FVector(0.0, 10.0, 0.0), Component));
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimRuntimeAdapterLiveHitResultObservationTest,
		"PhysAnim.RuntimeAdapter.LiveHitResultObservation",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimRuntimeAdapterLiveHitResultObservationTest::RunTest(const FString& Parameters)
	{
		{
			UBoxComponent* StaticComponent = MakeHitComponent(EComponentMobility::Static);

			FPhysAnimSupportHitResultConversionInput Input;
			Input.SupportBodies.Add(MakeSupportBodyMapping(TEXT("foot_l"), EPhysAnimSupportSide::Left));
			Input.HitResults.Add(MakeHitResult(TEXT("foot_l"), FVector(10.0, 20.0, 30.0), StaticComponent));

			const TArray<FPhysAnimSupportHitRecord> Records = PhysAnimRuntimeAdapter::ConvertSupportHitResultsToHitRecords(Input);

			TestEqual(TEXT("LIVE-HITRESULT-01 static blocking hit produces one record"), Records.Num(), 1);
			TestEqual(TEXT("LIVE-HITRESULT-01 body name copied from bone name"), Records[0].BodyName, FName(TEXT("foot_l")));
			TestTrue(TEXT("LIVE-HITRESULT-01 blocking flag copied"), Records[0].bBlockingHit);
			TestTrue(TEXT("LIVE-HITRESULT-01 static component marked world static"), Records[0].bFromWorldStatic);
			TestTrue(TEXT("LIVE-HITRESULT-01 impact point copied"), Records[0].WorldPositionCm.Equals(FVector(10.0, 20.0, 30.0), UE_SMALL_NUMBER));
		}

		{
			UBoxComponent* StaticComponent = MakeHitComponent(EComponentMobility::Static);

			FPhysAnimSupportHitResultObservationInput Input;
			Input.SupportBodies.Add(MakeSupportBodyMapping(TEXT("foot_l"), EPhysAnimSupportSide::Left));
			Input.HitResults.Add(MakeHitResult(TEXT("foot_l"), FVector(0.0, 0.0, 0.0), StaticComponent, false));
			Input.PreviousSupportGapTimerMs = 10.0;
			Input.DeltaMs = 15.0;
			Input.SupportGapMaxMs = 100.0;

			const FPhysAnimSupportObservationResult Result = PhysAnimRuntimeAdapter::BuildSupportObservationFromHitResults(Input);

			TestTrue(TEXT("LIVE-HITRESULT-02 non-blocking hit ignored through observation"), Result.bObservationValid);
			TestEqual(TEXT("LIVE-HITRESULT-02 no active side"), Result.Validation.ActiveSupportSideCount, 0);
			TestEqual(TEXT("LIVE-HITRESULT-02 support gap increments"), Result.Validation.SupportGapTimerMs, 25.0);
		}

		{
			UBoxComponent* MovableComponent = MakeHitComponent(EComponentMobility::Movable);

			FPhysAnimSupportHitResultObservationInput Input;
			Input.SupportBodies.Add(MakeSupportBodyMapping(TEXT("foot_l"), EPhysAnimSupportSide::Left));
			Input.HitResults.Add(MakeHitResult(TEXT("foot_l"), FVector(0.0, 0.0, 0.0), MovableComponent));
			Input.PreviousSupportGapTimerMs = 10.0;
			Input.DeltaMs = 15.0;
			Input.SupportGapMaxMs = 100.0;

			const FPhysAnimSupportObservationResult Result = PhysAnimRuntimeAdapter::BuildSupportObservationFromHitResults(Input);

			TestTrue(TEXT("LIVE-HITRESULT-03 movable component hit ignored when world-static required"), Result.bObservationValid);
			TestEqual(TEXT("LIVE-HITRESULT-03 no active side"), Result.Validation.ActiveSupportSideCount, 0);
			TestEqual(TEXT("LIVE-HITRESULT-03 support gap increments"), Result.Validation.SupportGapTimerMs, 25.0);
		}

		{
			UBoxComponent* StaticComponent = MakeHitComponent(EComponentMobility::Static);

			FPhysAnimSupportHitResultObservationInput Input;
			Input.SupportBodies.Add(MakeSupportBodyMapping(TEXT("foot_l"), EPhysAnimSupportSide::Left));
			Input.HitResults.Add(MakeHitResult(TEXT("hand_l"), FVector(0.0, 0.0, 0.0), StaticComponent));
			Input.PreviousSupportGapTimerMs = 10.0;
			Input.DeltaMs = 15.0;
			Input.SupportGapMaxMs = 100.0;

			const FPhysAnimSupportObservationResult Result = PhysAnimRuntimeAdapter::BuildSupportObservationFromHitResults(Input);

			TestTrue(TEXT("LIVE-HITRESULT-04 unmapped bone ignored"), Result.bObservationValid);
			TestEqual(TEXT("LIVE-HITRESULT-04 no active side"), Result.Validation.ActiveSupportSideCount, 0);
			TestEqual(TEXT("LIVE-HITRESULT-04 support gap increments"), Result.Validation.SupportGapTimerMs, 25.0);
		}

		{
			UBoxComponent* StaticComponent = MakeHitComponent(EComponentMobility::Static);

			FPhysAnimSupportHitResultObservationInput Input;
			Input.WorldOriginCm = FVector(100.0, 200.0, 0.0);
			Input.ComProxyPosCm = FVector2D(5.0, 5.0);
			Input.DeltaMs = 10.0;
			Input.SupportBodies.Add(MakeSupportBodyMapping(TEXT("foot_l"), EPhysAnimSupportSide::Left));
			Input.HitResults.Add(MakeHitResult(TEXT("foot_l"), FVector(100.0, 200.0, 0.0), StaticComponent));
			Input.HitResults.Add(MakeHitResult(TEXT("foot_l"), FVector(110.0, 200.0, 0.0), StaticComponent));
			Input.HitResults.Add(MakeHitResult(TEXT("foot_l"), FVector(110.0, 210.0, 0.0), StaticComponent));
			Input.HitResults.Add(MakeHitResult(TEXT("foot_l"), FVector(100.0, 210.0, 0.0), StaticComponent));

			const FPhysAnimSupportObservationResult Result = PhysAnimRuntimeAdapter::BuildSupportObservationFromHitResults(Input);

			TestTrue(TEXT("LIVE-HITRESULT-05 rebased live hit observation valid"), Result.bObservationValid);
			TestEqual(TEXT("LIVE-HITRESULT-05 active side count is 1"), Result.Validation.ActiveSupportSideCount, 1);
			TestTrue(TEXT("LIVE-HITRESULT-05 proxy inside rebased hull"), Result.Validation.ProxyInsideHull.IsSet() && Result.Validation.ProxyInsideHull.GetValue());
		}

		{
			UBoxComponent* StaticComponent = MakeHitComponent(EComponentMobility::Static);

			FPhysAnimSupportHitResultObservationInput Input;
			Input.ComProxyPosCm = FVector2D(5.0, 5.0);
			Input.DeltaMs = 10.0;
			AddLeftFootSquareHitResults(Input, StaticComponent);

			const FPhysAnimSupportObservationResult Result = PhysAnimRuntimeAdapter::BuildSupportObservationFromHitResults(Input);

			TestTrue(TEXT("LIVE-HITRESULT-06 valid live hit results produce valid support observation"), Result.bObservationValid);
			TestTrue(TEXT("LIVE-HITRESULT-06 validation passes"), Result.Validation.bSupportContractPassed);
			TestEqual(TEXT("LIVE-HITRESULT-06 active side count is 1"), Result.Validation.ActiveSupportSideCount, 1);
			TestEqual(
				TEXT("LIVE-HITRESULT-06 support mode is SingleFootSurvival"),
				static_cast<uint8>(Result.Validation.SupportMode),
				static_cast<uint8>(EPhysAnimSupportMode::SingleFootSurvival));
		}

		{
			UBoxComponent* StaticComponent = MakeHitComponent(EComponentMobility::Static);

			FPhysAnimSupportHitResultObservationInput Input;
			Input.ComProxyPosCm = FVector2D(50.0, 50.0);
			Input.PreviousProxyOutsideHullDurationMs = 90.0;
			Input.DeltaMs = 20.0;
			Input.ProxyDriftLimitMs = 100.0;
			AddLeftFootSquareHitResults(Input, StaticComponent);

			const FPhysAnimSupportObservationResult Result = PhysAnimRuntimeAdapter::BuildSupportObservationFromHitResults(Input);

			TestFalse(TEXT("LIVE-HITRESULT-07 proxy breach invalidates live hit observation"), Result.bObservationValid);
			TestEqual(
				TEXT("LIVE-HITRESULT-07 terminal reason is proxy outside support region"),
				static_cast<uint8>(Result.Validation.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion));
			TestTrue(TEXT("LIVE-HITRESULT-07 proxy outside duration copied"),
				Result.Validation.ProxyOutsideHullDurationMs.IsSet() && Result.Validation.ProxyOutsideHullDurationMs.GetValue() == 110.0);
		}

		return true;
	}
}
