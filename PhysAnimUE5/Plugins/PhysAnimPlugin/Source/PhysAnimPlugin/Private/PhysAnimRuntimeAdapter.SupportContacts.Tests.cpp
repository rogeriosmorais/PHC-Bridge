#include "PhysAnimRuntimeAdapter.h"
#include "Misc/AutomationTest.h"

namespace
{
	FPhysAnimSupportContactSample SupportContacts_MakeContact(
		const FVector2D& PositionCm,
		const FName BodyName,
		const EPhysAnimSupportSide SupportSide,
		const bool bIsValidSupportContact = true)
	{
		FPhysAnimSupportContactSample Contact;
		Contact.PositionCm = PositionCm;
		Contact.BodyName = BodyName;
		Contact.SupportSide = SupportSide;
		Contact.bIsValidSupportContact = bIsValidSupportContact;
		return Contact;
	}

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

			Input.Contacts = {
				SupportContacts_MakeContact(FVector2D(0.0, 0.0), TEXT("foot_l"), EPhysAnimSupportSide::Left),
				SupportContacts_MakeContact(FVector2D(10.0, 0.0), TEXT("foot_l"), EPhysAnimSupportSide::Left),
				SupportContacts_MakeContact(FVector2D(10.0, 10.0), TEXT("foot_l"), EPhysAnimSupportSide::Left),
				SupportContacts_MakeContact(FVector2D(0.0, 10.0), TEXT("foot_l"), EPhysAnimSupportSide::Left)
			};

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
			Input.Contacts = {
				SupportContacts_MakeContact(FVector2D(0.0, 0.0), TEXT("foot_l"), EPhysAnimSupportSide::Left),
				SupportContacts_MakeContact(FVector2D(1.0, 0.0), TEXT("foot_l"), EPhysAnimSupportSide::Left),
				SupportContacts_MakeContact(FVector2D(1.0, 1.0), TEXT("foot_l"), EPhysAnimSupportSide::Left),
				SupportContacts_MakeContact(FVector2D(0.0, 1.0), TEXT("foot_l"), EPhysAnimSupportSide::Left),
				SupportContacts_MakeContact(FVector2D(10.0, 10.0), TEXT("foot_r"), EPhysAnimSupportSide::Right),
				SupportContacts_MakeContact(FVector2D(11.0, 10.0), TEXT("foot_r"), EPhysAnimSupportSide::Right),
				SupportContacts_MakeContact(FVector2D(11.0, 11.0), TEXT("foot_r"), EPhysAnimSupportSide::Right),
				SupportContacts_MakeContact(FVector2D(10.0, 11.0), TEXT("foot_r"), EPhysAnimSupportSide::Right)
			};

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
			Input.ComProxyPosCm = FVector2D(5.0, 5.0);
			Input.PreviousProxyOutsideHullDurationMs = 50.0;

			Input.Contacts = {
				SupportContacts_MakeContact(FVector2D(0.0, 0.0), TEXT("foot_l"), EPhysAnimSupportSide::Left),
				SupportContacts_MakeContact(FVector2D(10.0, 0.0), TEXT("foot_l"), EPhysAnimSupportSide::Left),
				SupportContacts_MakeContact(FVector2D(10.0, 10.0), TEXT("foot_l"), EPhysAnimSupportSide::Left),
				SupportContacts_MakeContact(FVector2D(0.0, 10.0), TEXT("foot_l"), EPhysAnimSupportSide::Left)
			};

			const FPhysAnimSupportContractSnapshot Snapshot = PhysAnimRuntimeAdapter::CaptureSupportSnapshotFromContacts(Input);

			TestTrue(TEXT("SUPPORT-CONTACTS-05 proxy inside"), Snapshot.ProxyInsideHull.IsSet() && Snapshot.ProxyInsideHull.GetValue());
			TestEqual(TEXT("SUPPORT-CONTACTS-05 outside timer reset"), Snapshot.ProxyOutsideHullDurationMs.GetValue(), 0.0);
		}

		{
			FPhysAnimSupportContactsSnapshotCaptureInput Input;
			Input.ComProxyPosCm = FVector2D(50.0, 50.0);
			Input.PreviousProxyOutsideHullDurationMs = 90.0;
			Input.DeltaMs = 20.0;
			Input.ProxyDriftLimitMs = 100.0;

			Input.Contacts = {
				SupportContacts_MakeContact(FVector2D(0.0, 0.0), TEXT("foot_l"), EPhysAnimSupportSide::Left),
				SupportContacts_MakeContact(FVector2D(10.0, 0.0), TEXT("foot_l"), EPhysAnimSupportSide::Left),
				SupportContacts_MakeContact(FVector2D(10.0, 10.0), TEXT("foot_l"), EPhysAnimSupportSide::Left),
				SupportContacts_MakeContact(FVector2D(0.0, 10.0), TEXT("foot_l"), EPhysAnimSupportSide::Left)
			};

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
			Input.Contacts = {
				SupportContacts_MakeContact(FVector2D(0.0, 0.0), TEXT("foot_l"), EPhysAnimSupportSide::Left, false)
			};

			const FPhysAnimSupportContractSnapshot Snapshot = PhysAnimRuntimeAdapter::CaptureSupportSnapshotFromContacts(Input);

			TestFalse(TEXT("SUPPORT-CONTACTS-07 invalid contact ignored"), Snapshot.bSupportStateL);
			TestEqual(TEXT("SUPPORT-CONTACTS-07 active side count is 0"), Snapshot.ActiveSupportSideCount, 0);
		}

		return true;
	}
}
