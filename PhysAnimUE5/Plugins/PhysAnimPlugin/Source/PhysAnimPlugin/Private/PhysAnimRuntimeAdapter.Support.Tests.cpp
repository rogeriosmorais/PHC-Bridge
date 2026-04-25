#include "PhysAnimRuntimeAdapter.h"
#include "Misc/AutomationTest.h"

namespace
{
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimRuntimeAdapterSupportTest,
		"PhysAnim.RuntimeAdapter.Support",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimRuntimeAdapterSupportTest::RunTest(const FString& Parameters)
	{
		{
			FPhysAnimSupportSnapshotCaptureInput Input;
			Input.bSupportStateL = true;
			Input.bSupportStateR = false;

			const FPhysAnimSupportContractSnapshot Snapshot = PhysAnimRuntimeAdapter::CaptureSupportSnapshot(Input);

			TestTrue(TEXT("SUPPORT-ADAPTER-01 copies left support state"), Snapshot.bSupportStateL);
			TestFalse(TEXT("SUPPORT-ADAPTER-01 copies right support state"), Snapshot.bSupportStateR);
		}

		{
			FPhysAnimSupportSnapshotCaptureInput Input;
			Input.SupportHullAreaCm2 = 120.0;
			Input.ActiveSupportSideCount = 2;

			const FPhysAnimSupportContractSnapshot Snapshot = PhysAnimRuntimeAdapter::CaptureSupportSnapshot(Input);

			TestEqual(TEXT("SUPPORT-ADAPTER-02 copies support hull area"), Snapshot.SupportHullAreaCm2, 120.0);
			TestEqual(TEXT("SUPPORT-ADAPTER-02 copies active side count"), Snapshot.ActiveSupportSideCount, 2);
		}

		{
			FPhysAnimSupportSnapshotCaptureInput Input;
			Input.ProxyInsideHull = true;
			Input.ProxyOutsideHullDurationMs = 0.0;

			const FPhysAnimSupportContractSnapshot Snapshot = PhysAnimRuntimeAdapter::CaptureSupportSnapshot(Input);

			TestTrue(TEXT("SUPPORT-ADAPTER-03 preserves proxy inside hull"), Snapshot.ProxyInsideHull.IsSet() && Snapshot.ProxyInsideHull.GetValue());
			TestTrue(TEXT("SUPPORT-ADAPTER-03 preserves proxy outside duration"), Snapshot.ProxyOutsideHullDurationMs.IsSet() && Snapshot.ProxyOutsideHullDurationMs.GetValue() == 0.0);
		}

		{
			FPhysAnimSupportSnapshotCaptureInput Input;
			Input.ProxyTerminalReason = EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion;

			const FPhysAnimSupportContractSnapshot Snapshot = PhysAnimRuntimeAdapter::CaptureSupportSnapshot(Input);

			TestEqual(
				TEXT("SUPPORT-ADAPTER-04 passes proxy terminal reason through"),
				static_cast<uint8>(Snapshot.ProxyTerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion));
		}

		{
			FPhysAnimSupportSnapshotCaptureInput Input;
			Input.SupportMode = EPhysAnimSupportMode::Airborne;
			Input.SupportGapTimerMs = 150.0;
			Input.SupportGapMaxMs = 100.0;

			const FPhysAnimSupportContractSnapshot Snapshot = PhysAnimRuntimeAdapter::CaptureSupportSnapshot(Input);
			const FPhysAnimSupportContractValidationResult Validation = PhysAnimValidators::ValidateSupport(Snapshot);

			TestFalse(TEXT("SUPPORT-ADAPTER-05 captured snapshot validates through ValidateSupport"), Validation.bSupportContractPassed);
			TestEqual(
				TEXT("SUPPORT-ADAPTER-05 terminal reason is ActivationSupportFailure"),
				static_cast<uint8>(Validation.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationSupportFailure));
		}

		return true;
	}
}
