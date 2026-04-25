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

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimRuntimeAdapterSupportHitsTest,
		"PhysAnim.RuntimeAdapter.SupportHits",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimRuntimeAdapterSupportHitsTest::RunTest(const FString& Parameters)
	{
		{
			FPhysAnimSupportHitConversionInput Input;
			Input.SupportBodies.Add(MakeSupportBodyMapping(TEXT("foot_l"), EPhysAnimSupportSide::Left));
			Input.Hits.Add(MakeSupportHit(TEXT("foot_l"), FVector(10.0, 20.0, 30.0)));

			const TArray<FPhysAnimSupportContactSample> Samples = PhysAnimRuntimeAdapter::ConvertSupportHitsToContactSamples(Input);

			TestEqual(TEXT("SUPPORT-HITS-01 valid hit produces one sample"), Samples.Num(), 1);
			TestEqual(TEXT("SUPPORT-HITS-01 body name preserved"), Samples[0].BodyName, FName(TEXT("foot_l")));
			TestEqual(
				TEXT("SUPPORT-HITS-01 support side mapped"),
				static_cast<uint8>(Samples[0].SupportSide),
				static_cast<uint8>(EPhysAnimSupportSide::Left));
			TestTrue(TEXT("SUPPORT-HITS-01 sample is valid"), Samples[0].bIsValidSupportContact);
			TestTrue(TEXT("SUPPORT-HITS-01 planar position uses X/Y"), Samples[0].PositionCm.Equals(FVector2D(10.0, 20.0), UE_SMALL_NUMBER));
		}

		{
			FPhysAnimSupportHitConversionInput Input;
			Input.SupportBodies.Add(MakeSupportBodyMapping(TEXT("foot_l"), EPhysAnimSupportSide::Left));
			Input.Hits.Add(MakeSupportHit(TEXT("foot_l"), FVector(10.0, 20.0, 30.0), false, true));

			const TArray<FPhysAnimSupportContactSample> Samples = PhysAnimRuntimeAdapter::ConvertSupportHitsToContactSamples(Input);

			TestEqual(TEXT("SUPPORT-HITS-02 non-blocking hit is ignored"), Samples.Num(), 0);
		}

		{
			FPhysAnimSupportHitConversionInput Input;
			Input.SupportBodies.Add(MakeSupportBodyMapping(TEXT("foot_l"), EPhysAnimSupportSide::Left));
			Input.Hits.Add(MakeSupportHit(TEXT("foot_l"), FVector(10.0, 20.0, 30.0), true, false));

			const TArray<FPhysAnimSupportContactSample> Samples = PhysAnimRuntimeAdapter::ConvertSupportHitsToContactSamples(Input);

			TestEqual(TEXT("SUPPORT-HITS-03 non-world-static hit is ignored"), Samples.Num(), 0);
		}

		{
			FPhysAnimSupportHitConversionInput Input;
			Input.SupportBodies.Add(MakeSupportBodyMapping(TEXT("foot_l"), EPhysAnimSupportSide::Left));
			Input.Hits.Add(MakeSupportHit(TEXT("hand_l"), FVector(10.0, 20.0, 30.0)));

			const TArray<FPhysAnimSupportContactSample> Samples = PhysAnimRuntimeAdapter::ConvertSupportHitsToContactSamples(Input);

			TestEqual(TEXT("SUPPORT-HITS-04 unmapped body is ignored"), Samples.Num(), 0);
		}

		{
			FPhysAnimSupportHitConversionInput Input;
			Input.SupportBodies.Add(MakeSupportBodyMapping(TEXT("foot_l"), EPhysAnimSupportSide::Left));
			Input.SupportBodies.Add(MakeSupportBodyMapping(TEXT("foot_r"), EPhysAnimSupportSide::Right));
			Input.Hits.Add(MakeSupportHit(TEXT("foot_l"), FVector(0.0, 0.0, 0.0)));
			Input.Hits.Add(MakeSupportHit(TEXT("foot_r"), FVector(10.0, 0.0, 0.0)));

			const TArray<FPhysAnimSupportContactSample> Samples = PhysAnimRuntimeAdapter::ConvertSupportHitsToContactSamples(Input);

			TestEqual(TEXT("SUPPORT-HITS-05 two mapped hits produce two samples"), Samples.Num(), 2);
			TestEqual(
				TEXT("SUPPORT-HITS-05 left body mapping preserved"),
				static_cast<uint8>(Samples[0].SupportSide),
				static_cast<uint8>(EPhysAnimSupportSide::Left));
			TestEqual(
				TEXT("SUPPORT-HITS-05 right body mapping preserved"),
				static_cast<uint8>(Samples[1].SupportSide),
				static_cast<uint8>(EPhysAnimSupportSide::Right));
		}

		{
			FPhysAnimSupportHitConversionInput Input;
			Input.WorldOriginCm = FVector(100.0, 200.0, 300.0);
			Input.SupportBodies.Add(MakeSupportBodyMapping(TEXT("foot_l"), EPhysAnimSupportSide::Left));
			Input.Hits.Add(MakeSupportHit(TEXT("foot_l"), FVector(110.0, 225.0, 400.0)));

			const TArray<FPhysAnimSupportContactSample> Samples = PhysAnimRuntimeAdapter::ConvertSupportHitsToContactSamples(Input);

			TestEqual(TEXT("SUPPORT-HITS-06 rebased sample count"), Samples.Num(), 1);
			TestTrue(TEXT("SUPPORT-HITS-06 world origin rebasing uses X/Y"), Samples[0].PositionCm.Equals(FVector2D(10.0, 25.0), UE_SMALL_NUMBER));
		}

		{
			FPhysAnimSupportHitConversionInput HitInput;
			HitInput.SupportBodies.Add(MakeSupportBodyMapping(TEXT("foot_l"), EPhysAnimSupportSide::Left));
			HitInput.Hits.Add(MakeSupportHit(TEXT("foot_l"), FVector(0.0, 0.0, 0.0)));
			HitInput.Hits.Add(MakeSupportHit(TEXT("foot_l"), FVector(10.0, 0.0, 0.0)));
			HitInput.Hits.Add(MakeSupportHit(TEXT("foot_l"), FVector(10.0, 10.0, 0.0)));
			HitInput.Hits.Add(MakeSupportHit(TEXT("foot_l"), FVector(0.0, 10.0, 0.0)));

			const TArray<FPhysAnimSupportContactSample> Samples = PhysAnimRuntimeAdapter::ConvertSupportHitsToContactSamples(HitInput);

			FPhysAnimSupportContactsSnapshotCaptureInput SnapshotInput;
			SnapshotInput.Contacts = Samples;
			SnapshotInput.ComProxyPosCm = FVector2D(5.0, 5.0);
			SnapshotInput.DeltaMs = 10.0;

			const FPhysAnimSupportContractSnapshot Snapshot = PhysAnimRuntimeAdapter::CaptureSupportSnapshotFromContacts(SnapshotInput);

			TestEqual(TEXT("SUPPORT-HITS-07 converted samples feed support snapshot"), Snapshot.ActiveSupportSideCount, 1);
			TestTrue(TEXT("SUPPORT-HITS-07 support hull area is positive"), Snapshot.SupportHullAreaCm2 > 0.0);
			TestEqual(
				TEXT("SUPPORT-HITS-07 mode is SingleFootSurvival"),
				static_cast<uint8>(Snapshot.SupportMode),
				static_cast<uint8>(EPhysAnimSupportMode::SingleFootSurvival));
			TestTrue(TEXT("SUPPORT-HITS-07 proxy is inside support hull"), Snapshot.ProxyInsideHull.IsSet() && Snapshot.ProxyInsideHull.GetValue());
		}

		return true;
	}
}
