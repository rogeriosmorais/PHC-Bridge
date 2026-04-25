#include "PhysAnimRuntimeAdapter.h"
#include "Misc/AutomationTest.h"

namespace
{
	FPhysAnimSupportBodyMapping MakeSupportBodyMapping(const FName BodyName, const EPhysAnimSupportSide SupportSide)
	{
		FPhysAnimSupportBodyMapping Mapping;
		Mapping.BodyName = BodyName;
		Mapping.SupportSide = SupportSide;
		return Mapping;
	}

	FPhysAnimSupportHitRecord MakeSupportHit(
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

	void AddLeftFootSquareHits(FPhysAnimSupportHitSnapshotCaptureInput& Input, const FVector& Offset = FVector::ZeroVector)
	{
		Input.SupportBodies.Add(MakeSupportBodyMapping(TEXT("foot_l"), EPhysAnimSupportSide::Left));
		Input.Hits.Add(MakeSupportHit(TEXT("foot_l"), Offset + FVector(0.0, 0.0, 0.0)));
		Input.Hits.Add(MakeSupportHit(TEXT("foot_l"), Offset + FVector(10.0, 0.0, 0.0)));
		Input.Hits.Add(MakeSupportHit(TEXT("foot_l"), Offset + FVector(10.0, 10.0, 0.0)));
		Input.Hits.Add(MakeSupportHit(TEXT("foot_l"), Offset + FVector(0.0, 10.0, 0.0)));
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimRuntimeAdapterSupportHitSnapshotTest,
		"PhysAnim.RuntimeAdapter.SupportHitSnapshot",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimRuntimeAdapterSupportHitSnapshotTest::RunTest(const FString& Parameters)
	{
		{
			FPhysAnimSupportHitSnapshotCaptureInput Input;
			Input.ComProxyPosCm = FVector2D(5.0, 5.0);
			Input.DeltaMs = 10.0;
			AddLeftFootSquareHits(Input);

			const FPhysAnimSupportContractSnapshot Snapshot = PhysAnimRuntimeAdapter::CaptureSupportSnapshotFromHits(Input);

			TestTrue(TEXT("SUPPORT-HIT-SNAPSHOT-01 left side active"), Snapshot.bSupportStateL);
			TestFalse(TEXT("SUPPORT-HIT-SNAPSHOT-01 right side inactive"), Snapshot.bSupportStateR);
			TestEqual(TEXT("SUPPORT-HIT-SNAPSHOT-01 active side count is 1"), Snapshot.ActiveSupportSideCount, 1);
			TestTrue(TEXT("SUPPORT-HIT-SNAPSHOT-01 support hull area is positive"), Snapshot.SupportHullAreaCm2 > 0.0);
			TestEqual(
				TEXT("SUPPORT-HIT-SNAPSHOT-01 mode is SingleFootSurvival"),
				static_cast<uint8>(Snapshot.SupportMode),
				static_cast<uint8>(EPhysAnimSupportMode::SingleFootSurvival));
			TestTrue(TEXT("SUPPORT-HIT-SNAPSHOT-01 proxy inside hull"), Snapshot.ProxyInsideHull.IsSet() && Snapshot.ProxyInsideHull.GetValue());
		}

		{
			FPhysAnimSupportHitSnapshotCaptureInput Input;
			Input.WorldOriginCm = FVector(100.0, 200.0, 0.0);
			Input.ComProxyPosCm = FVector2D(5.0, 5.0);
			Input.DeltaMs = 10.0;
			AddLeftFootSquareHits(Input, FVector(100.0, 200.0, 0.0));

			const FPhysAnimSupportContractSnapshot Snapshot = PhysAnimRuntimeAdapter::CaptureSupportSnapshotFromHits(Input);

			TestEqual(TEXT("SUPPORT-HIT-SNAPSHOT-02 active side count after rebasing"), Snapshot.ActiveSupportSideCount, 1);
			TestTrue(TEXT("SUPPORT-HIT-SNAPSHOT-02 rebased proxy inside hull"), Snapshot.ProxyInsideHull.IsSet() && Snapshot.ProxyInsideHull.GetValue());
			TestTrue(TEXT("SUPPORT-HIT-SNAPSHOT-02 hull contains rebased origin point"),
				Snapshot.SupportHullPointsCm.ContainsByPredicate(
					[](const FVector2D& Point)
					{
						return Point.Equals(FVector2D(0.0, 0.0), UE_SMALL_NUMBER);
					}));
		}

		{
			FPhysAnimSupportHitSnapshotCaptureInput Input;
			Input.SupportBodies.Add(MakeSupportBodyMapping(TEXT("foot_l"), EPhysAnimSupportSide::Left));
			Input.Hits.Add(MakeSupportHit(TEXT("foot_l"), FVector(0.0, 0.0, 0.0), false, true));
			Input.Hits.Add(MakeSupportHit(TEXT("foot_l"), FVector(10.0, 0.0, 0.0), true, false));
			Input.Hits.Add(MakeSupportHit(TEXT("hand_l"), FVector(20.0, 0.0, 0.0), true, true));
			Input.PreviousSupportGapTimerMs = 30.0;
			Input.DeltaMs = 20.0;
			Input.SupportGapMaxMs = 100.0;

			const FPhysAnimSupportContractSnapshot Snapshot = PhysAnimRuntimeAdapter::CaptureSupportSnapshotFromHits(Input);

			TestFalse(TEXT("SUPPORT-HIT-SNAPSHOT-03 invalid/unmapped hits produce no left support"), Snapshot.bSupportStateL);
			TestFalse(TEXT("SUPPORT-HIT-SNAPSHOT-03 invalid/unmapped hits produce no right support"), Snapshot.bSupportStateR);
			TestEqual(TEXT("SUPPORT-HIT-SNAPSHOT-03 active side count is 0"), Snapshot.ActiveSupportSideCount, 0);
			TestEqual(TEXT("SUPPORT-HIT-SNAPSHOT-03 support gap increments"), Snapshot.SupportGapTimerMs, 50.0);
			TestEqual(
				TEXT("SUPPORT-HIT-SNAPSHOT-03 mode is TransientRecovery"),
				static_cast<uint8>(Snapshot.SupportMode),
				static_cast<uint8>(EPhysAnimSupportMode::TransientRecovery));
		}

		{
			FPhysAnimSupportHitSnapshotCaptureInput Input;
			Input.PreviousSupportGapTimerMs = 90.0;
			Input.DeltaMs = 20.0;
			Input.SupportGapMaxMs = 100.0;

			const FPhysAnimSupportContractSnapshot Snapshot = PhysAnimRuntimeAdapter::CaptureSupportSnapshotFromHits(Input);
			const FPhysAnimSupportContractValidationResult Validation = PhysAnimValidators::ValidateSupport(Snapshot);

			TestEqual(TEXT("SUPPORT-HIT-SNAPSHOT-04 support gap exceeds limit"), Snapshot.SupportGapTimerMs, 110.0);
			TestEqual(
				TEXT("SUPPORT-HIT-SNAPSHOT-04 mode is Airborne"),
				static_cast<uint8>(Snapshot.SupportMode),
				static_cast<uint8>(EPhysAnimSupportMode::Airborne));
			TestFalse(TEXT("SUPPORT-HIT-SNAPSHOT-04 support validation fails"), Validation.bSupportContractPassed);
			TestEqual(
				TEXT("SUPPORT-HIT-SNAPSHOT-04 terminal reason is support failure"),
				static_cast<uint8>(Validation.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationSupportFailure));
		}

		{
			FPhysAnimSupportHitSnapshotCaptureInput Input;
			Input.ComProxyPosCm = FVector2D(50.0, 50.0);
			Input.PreviousProxyOutsideHullDurationMs = 90.0;
			Input.DeltaMs = 20.0;
			Input.ProxyDriftLimitMs = 100.0;
			AddLeftFootSquareHits(Input);

			const FPhysAnimSupportContractSnapshot Snapshot = PhysAnimRuntimeAdapter::CaptureSupportSnapshotFromHits(Input);

			TestFalse(TEXT("SUPPORT-HIT-SNAPSHOT-05 proxy outside hull"), Snapshot.ProxyInsideHull.IsSet() && Snapshot.ProxyInsideHull.GetValue());
			TestTrue(TEXT("SUPPORT-HIT-SNAPSHOT-05 outside duration is set"), Snapshot.ProxyOutsideHullDurationMs.IsSet());
			TestEqual(TEXT("SUPPORT-HIT-SNAPSHOT-05 outside duration accumulated"), Snapshot.ProxyOutsideHullDurationMs.GetValue(), 110.0);
			TestEqual(
				TEXT("SUPPORT-HIT-SNAPSHOT-05 proxy terminal reason set"),
				static_cast<uint8>(Snapshot.ProxyTerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion));
		}

		{
			FPhysAnimSupportHitSnapshotCaptureInput Input;
			Input.SupportBodies.Add(MakeSupportBodyMapping(TEXT("foot_l"), EPhysAnimSupportSide::Left));
			Input.SupportBodies.Add(MakeSupportBodyMapping(TEXT("foot_r"), EPhysAnimSupportSide::Right));
			Input.Hits.Add(MakeSupportHit(TEXT("foot_l"), FVector(0.0, 0.0, 0.0)));
			Input.Hits.Add(MakeSupportHit(TEXT("foot_l"), FVector(1.0, 0.0, 0.0)));
			Input.Hits.Add(MakeSupportHit(TEXT("foot_l"), FVector(1.0, 1.0, 0.0)));
			Input.Hits.Add(MakeSupportHit(TEXT("foot_l"), FVector(0.0, 1.0, 0.0)));
			Input.Hits.Add(MakeSupportHit(TEXT("foot_r"), FVector(10.0, 0.0, 0.0)));
			Input.Hits.Add(MakeSupportHit(TEXT("foot_r"), FVector(11.0, 0.0, 0.0)));
			Input.Hits.Add(MakeSupportHit(TEXT("foot_r"), FVector(11.0, 1.0, 0.0)));
			Input.Hits.Add(MakeSupportHit(TEXT("foot_r"), FVector(10.0, 1.0, 0.0)));

			const FPhysAnimSupportContractSnapshot Snapshot = PhysAnimRuntimeAdapter::CaptureSupportSnapshotFromHits(Input);

			TestTrue(TEXT("SUPPORT-HIT-SNAPSHOT-06 left support active"), Snapshot.bSupportStateL);
			TestTrue(TEXT("SUPPORT-HIT-SNAPSHOT-06 right support active"), Snapshot.bSupportStateR);
			TestEqual(TEXT("SUPPORT-HIT-SNAPSHOT-06 active side count is 2"), Snapshot.ActiveSupportSideCount, 2);
			TestEqual(
				TEXT("SUPPORT-HIT-SNAPSHOT-06 mode is TwoFootStable"),
				static_cast<uint8>(Snapshot.SupportMode),
				static_cast<uint8>(EPhysAnimSupportMode::TwoFootStable));
		}

		return true;
	}
}
