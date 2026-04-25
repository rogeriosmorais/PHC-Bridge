#include "PhysAnimRuntimeAdapter.h"
#include "Misc/AutomationTest.h"

namespace
{
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimRuntimeAdapterSupportContactsTest,
		"PhysAnim.RuntimeAdapter.SupportContacts",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimRuntimeAdapterSupportContactsTest::RunTest(const FString& Parameters)
	{
		{
			FPhysAnimSupportContactsSnapshotCaptureInput Input;
			Input.ComProxyPosCm = FVector2D(5.0, 5.0);
			Input.DeltaMs = 10.0;

			FPhysAnimSupportContactSample C1, C2, C3, C4;
			C1.PositionCm = FVector2D(0, 0); C1.BodyName = "foot_l"; C1.SupportSide = EPhysAnimSupportSide::Left;
			C2.PositionCm = FVector2D(10, 0); C2.BodyName = "foot_l"; C2.SupportSide = EPhysAnimSupportSide::Left;
			C3.PositionCm = FVector2D(10, 10); C3.BodyName = "foot_l"; C3.SupportSide = EPhysAnimSupportSide::Left;
			C4.PositionCm = FVector2D(0, 10); C4.BodyName = "foot_l"; C4.SupportSide = EPhysAnimSupportSide::Left;
			Input.Contacts = {C1, C2, C3, C4};

			const FPhysAnimSupportContractSnapshot Snapshot = PhysAnimRuntimeAdapter::CaptureSupportSnapshotFromContacts(Input);

			TestTrue(TEXT("SUPPORT-CONTACTS-01 left side active"), Snapshot.bSupportStateL);
			TestFalse(TEXT("SUPPORT-CONTACTS-01 right side inactive"), Snapshot.bSupportStateR);
			TestEqual(TEXT("SUPPORT-CONTACTS-01 active side count is 1"), Snapshot.ActiveSupportSideCount, 1);
			TestTrue(TEXT("SUPPORT-CONTACTS-01 area is positive"), Snapshot.SupportHullAreaCm2 > 0.0);
			TestEqual(
				TEXT("SUPPORT-CONTACTS-01 mode is SingleFootSurvival"),
				static_cast<uint8>(Snapshot.SupportMode),
				static_cast<uint8>(EPhysAnimSupportMode::SingleFootSurvival));
			TestTrue(TEXT("SUPPORT-CONTACTS-01 proxy inside hull"), Snapshot.ProxyInsideHull.IsSet() && Snapshot.ProxyInsideHull.GetValue());
		}

		{
			FPhysAnimSupportContactsSnapshotCaptureInput Input;
			FPhysAnimSupportContactSample C1, C2, C3, C4, C5, C6, C7, C8;
			C1.PositionCm = FVector2D(0, 0); C1.BodyName = "foot_l"; C1.SupportSide = EPhysAnimSupportSide::Left;
			C2.PositionCm = FVector2D(1, 0); C2.BodyName = "foot_l"; C2.SupportSide = EPhysAnimSupportSide::Left;
			C3.PositionCm = FVector2D(1, 1); C3.BodyName = "foot_l"; C3.SupportSide = EPhysAnimSupportSide::Left;
			C4.PositionCm = FVector2D(0, 1); C4.BodyName = "foot_l"; C4.SupportSide = EPhysAnimSupportSide::Left;
			
			C5.PositionCm = FVector2D(10, 10); C5.BodyName = "foot_r"; C5.SupportSide = EPhysAnimSupportSide::Right;
			C6.PositionCm = FVector2D(11, 10); C6.BodyName = "foot_r"; C6.SupportSide = EPhysAnimSupportSide::Right;
			C7.PositionCm = FVector2D(11, 11); C7.BodyName = "foot_r"; C7.SupportSide = EPhysAnimSupportSide::Right;
			C8.PositionCm = FVector2D(10, 11); C8.BodyName = "foot_r"; C8.SupportSide = EPhysAnimSupportSide::Right;
			
			Input.Contacts = {C1, C2, C3, C4, C5, C6, C7, C8};

			const FPhysAnimSupportContractSnapshot Snapshot = PhysAnimRuntimeAdapter::CaptureSupportSnapshotFromContacts(Input);

			TestTrue(TEXT("SUPPORT-CONTACTS-02 left active"), Snapshot.bSupportStateL);
			TestTrue(TEXT("SUPPORT-CONTACTS-02 right active"), Snapshot.bSupportStateR);
			TestEqual(TEXT("SUPPORT-CONTACTS-02 active side count is 2"), Snapshot.ActiveSupportSideCount, 2);
			TestEqual(
				TEXT("SUPPORT-CONTACTS-02 mode is TwoFootStable"),
				static_cast<uint8>(Snapshot.SupportMode),
				static_cast<uint8>(EPhysAnimSupportMode::TwoFootStable));
		}

		{
			FPhysAnimSupportContactsSnapshotCaptureInput Input;
			Input.DeltaMs = 20.0;
			Input.PreviousSupportGapTimerMs = 30.0;

			const FPhysAnimSupportContractSnapshot Snapshot = PhysAnimRuntimeAdapter::CaptureSupportSnapshotFromContacts(Input);

			TestFalse(TEXT("SUPPORT-CONTACTS-03 no sides active"), Snapshot.bSupportStateL || Snapshot.bSupportStateR);
			TestEqual(TEXT("SUPPORT-CONTACTS-03 gap timer incremented"), Snapshot.SupportGapTimerMs, 50.0);
			TestEqual(
				TEXT("SUPPORT-CONTACTS-03 mode is TransientRecovery"),
				static_cast<uint8>(Snapshot.SupportMode),
				static_cast<uint8>(EPhysAnimSupportMode::TransientRecovery));
		}

		{
			FPhysAnimSupportContactsSnapshotCaptureInput Input;
			Input.DeltaMs = 20.0;
			Input.PreviousSupportGapTimerMs = 90.0;
			Input.SupportGapMaxMs = 100.0;

			const FPhysAnimSupportContractSnapshot Snapshot = PhysAnimRuntimeAdapter::CaptureSupportSnapshotFromContacts(Input);

			TestEqual(TEXT("SUPPORT-CONTACTS-04 gap timer over limit"), Snapshot.SupportGapTimerMs, 110.0);
			TestEqual(
				TEXT("SUPPORT-CONTACTS-04 mode is Airborne"),
				static_cast<uint8>(Snapshot.SupportMode),
				static_cast<uint8>(EPhysAnimSupportMode::Airborne));
			
			const FPhysAnimSupportContractValidationResult Validation = PhysAnimValidators::ValidateSupport(Snapshot);
			TestFalse(TEXT("SUPPORT-CONTACTS-04 validation fails"), Validation.bSupportContractPassed);
		}

		{
			FPhysAnimSupportContactsSnapshotCaptureInput Input;
			Input.ComProxyPosCm = FVector2D(5, 5);
			Input.PreviousProxyOutsideHullDurationMs = 50.0;

			FPhysAnimSupportContactSample C1, C2, C3, C4;
			C1.PositionCm = FVector2D(0, 0); C1.BodyName = "foot_l"; C1.SupportSide = EPhysAnimSupportSide::Left;
			C2.PositionCm = FVector2D(10, 0); C2.BodyName = "foot_l"; C2.SupportSide = EPhysAnimSupportSide::Left;
			C3.PositionCm = FVector2D(10, 10); C3.BodyName = "foot_l"; C3.SupportSide = EPhysAnimSupportSide::Left;
			C4.PositionCm = FVector2D(0, 10); C4.BodyName = "foot_l"; C4.SupportSide = EPhysAnimSupportSide::Left;
			Input.Contacts = {C1, C2, C3, C4};

			const FPhysAnimSupportContractSnapshot Snapshot = PhysAnimRuntimeAdapter::CaptureSupportSnapshotFromContacts(Input);

			TestTrue(TEXT("SUPPORT-CONTACTS-05 proxy inside"), Snapshot.ProxyInsideHull.IsSet() && Snapshot.ProxyInsideHull.GetValue());
			TestEqual(TEXT("SUPPORT-CONTACTS-05 outside timer reset"), Snapshot.ProxyOutsideHullDurationMs.GetValue(), 0.0);
		}

		{
			FPhysAnimSupportContactsSnapshotCaptureInput Input;
			Input.ComProxyPosCm = FVector2D(50, 50);
			Input.PreviousProxyOutsideHullDurationMs = 90.0;
			Input.DeltaMs = 20.0;
			Input.ProxyDriftLimitMs = 100.0;

			FPhysAnimSupportContactSample C1, C2, C3, C4;
			C1.PositionCm = FVector2D(0, 0); C1.BodyName = "foot_l"; C1.SupportSide = EPhysAnimSupportSide::Left;
			C2.PositionCm = FVector2D(10, 0); C2.BodyName = "foot_l"; C2.SupportSide = EPhysAnimSupportSide::Left;
			C3.PositionCm = FVector2D(10, 10); C3.BodyName = "foot_l"; C3.SupportSide = EPhysAnimSupportSide::Left;
			C4.PositionCm = FVector2D(0, 10); C4.BodyName = "foot_l"; C4.SupportSide = EPhysAnimSupportSide::Left;
			Input.Contacts = {C1, C2, C3, C4};

			const FPhysAnimSupportContractSnapshot Snapshot = PhysAnimRuntimeAdapter::CaptureSupportSnapshotFromContacts(Input);

			TestFalse(TEXT("SUPPORT-CONTACTS-06 proxy outside"), Snapshot.ProxyInsideHull.IsSet() && Snapshot.ProxyInsideHull.GetValue());
			TestEqual(TEXT("SUPPORT-CONTACTS-06 outside timer accumulated"), Snapshot.ProxyOutsideHullDurationMs.GetValue(), 110.0);
			TestEqual(
				TEXT("SUPPORT-CONTACTS-06 proxy terminal reason set"),
				static_cast<uint8>(Snapshot.ProxyTerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion));
		}

		{
			FPhysAnimSupportContactsSnapshotCaptureInput Input;
			FPhysAnimSupportContactSample C1;
			C1.PositionCm = FVector2D(0, 0); C1.BodyName = "foot_l"; C1.SupportSide = EPhysAnimSupportSide::Left;
			C1.bIsValidSupportContact = false;
			Input.Contacts = {C1};

			const FPhysAnimSupportContractSnapshot Snapshot = PhysAnimRuntimeAdapter::CaptureSupportSnapshotFromContacts(Input);

			TestFalse(TEXT("SUPPORT-CONTACTS-07 invalid contact ignored"), Snapshot.bSupportStateL);
			TestEqual(TEXT("SUPPORT-CONTACTS-07 active side count is 0"), Snapshot.ActiveSupportSideCount, 0);
		}

		return true;
	}
}
