#include "PhysAnimRuntimeAdapter.h"
#include "Misc/AutomationTest.h"

namespace
{
	FPhysAnimSupportBodyMapping SupportHits_MakeSupportBodyMapping(const FName BodyName, const EPhysAnimSupportSide SupportSide)
	{
		FPhysAnimSupportBodyMapping Mapping;
		Mapping.BodyName = BodyName;
		Mapping.SupportSide = SupportSide;
		return Mapping;
	}

	FPhysAnimSupportHitRecord SupportHits_MakeSupportHit(
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

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimRuntimeAdapterSupportHitsTest,
		"PhysAnim.RuntimeAdapter.SupportHits",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimRuntimeAdapterSupportHitsTest::RunTest(const FString& Parameters)
	{
		{
			FPhysAnimSupportHitSnapshotCaptureInput Input_A;
			Input_A.SupportBodies.Add(SupportHits_MakeSupportBodyMapping(TEXT("foot_l"), EPhysAnimSupportSide::Left));
			Input_A.Hits.Add(SupportHits_MakeSupportHit(TEXT("foot_l"), FVector(10.0, 20.0, 30.0)));

			const FPhysAnimSupportContractSnapshot Snapshot = PhysAnimRuntimeAdapter::CaptureSupportSnapshotFromHits(Input_A);

			TestTrue(TEXT("SUPPORT-HITS-01 left side active"), Snapshot.bSupportStateL);
			TestFalse(TEXT("SUPPORT-HITS-01 right side inactive"), Snapshot.bSupportStateR);
			TestEqual(TEXT("SUPPORT-HITS-01 active side count is 1"), Snapshot.ActiveSupportSideCount, 1);
		}

		{
			FPhysAnimSupportHitSnapshotCaptureInput Input_B;
			Input_B.WorldOriginCm = FVector(100.0, 200.0, 0.0);
			Input_B.SupportBodies.Add(SupportHits_MakeSupportBodyMapping(TEXT("foot_l"), EPhysAnimSupportSide::Left));
			Input_B.Hits.Add(SupportHits_MakeSupportHit(TEXT("foot_l"), FVector(100.0, 200.0, 0.0)));

			const FPhysAnimSupportContractSnapshot Snapshot = PhysAnimRuntimeAdapter::CaptureSupportSnapshotFromHits(Input_B);

			TestEqual(TEXT("SUPPORT-HITS-02 active side count after rebasing"), Snapshot.ActiveSupportSideCount, 1);
			TestTrue(TEXT("SUPPORT-HITS-02 hull contains rebased origin point"),
				Snapshot.SupportHullPointsCm.ContainsByPredicate(
					[](const FVector2D& Point)
					{
						return Point.Equals(FVector2D(0.0, 0.0), UE_SMALL_NUMBER);
					}));
		}

		{
			FPhysAnimSupportHitSnapshotCaptureInput Input_C;
			Input_C.SupportBodies.Add(SupportHits_MakeSupportBodyMapping(TEXT("foot_l"), EPhysAnimSupportSide::Left));
			Input_C.Hits.Add(SupportHits_MakeSupportHit(TEXT("foot_l"), FVector(0.0, 0.0, 0.0), false, true));
			Input_C.Hits.Add(SupportHits_MakeSupportHit(TEXT("foot_l"), FVector(10.0, 0.0, 0.0), true, false));
			Input_C.Hits.Add(SupportHits_MakeSupportHit(TEXT("hand_l"), FVector(20.0, 0.0, 0.0), true, true));

			const FPhysAnimSupportContractSnapshot Snapshot = PhysAnimRuntimeAdapter::CaptureSupportSnapshotFromHits(Input_C);

			TestFalse(TEXT("SUPPORT-HITS-03 invalid/unmapped hits produce no support"), Snapshot.bSupportStateL || Snapshot.bSupportStateR);
			TestEqual(TEXT("SUPPORT-HITS-03 active side count is 0"), Snapshot.ActiveSupportSideCount, 0);
		}

		return true;
	}
}
