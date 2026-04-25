#include "PhysAnimValidators.h"
#include "Misc/AutomationTest.h"

namespace
{
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimSupportContractValidationTest,
		"PhysAnim.Validators.Support",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimSupportContractValidationTest::RunTest(const FString& Parameters)
	{
		{
			FPhysAnimSupportContractSnapshot Snapshot;
			Snapshot.bSupportStateL = true;
			Snapshot.bSupportStateR = false;
			Snapshot.SupportMode = EPhysAnimSupportMode::SingleFootSurvival;
			Snapshot.ActiveSupportSideCount = 1;
			Snapshot.SupportHullAreaCm2 = 75.0;
			Snapshot.SupportAreaMinCm2 = 50.0;
			Snapshot.ProxyInsideHull = true;
			Snapshot.ProxyOutsideHullDurationMs = 0.0;

			const FPhysAnimSupportContractValidationResult Result = PhysAnimValidators::ValidateSupport(Snapshot);

			TestTrue(TEXT("SUPPORT-01 valid one-foot support passes"), Result.bSupportContractPassed);
			TestEqual(
				TEXT("SUPPORT-01 terminal reason is None"),
				static_cast<uint8>(Result.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::None));
			TestEqual(TEXT("SUPPORT-01 active side count copied"), Result.ActiveSupportSideCount, 1);
			TestEqual(TEXT("SUPPORT-01 support hull area copied"), Result.SupportHullAreaCm2, 75.0);
		}

		{
			FPhysAnimSupportContractSnapshot Snapshot;
			Snapshot.SupportMode = EPhysAnimSupportMode::SingleFootSurvival;
			Snapshot.ActiveSupportSideCount = 1;
			Snapshot.SupportHullAreaCm2 = 25.0;
			Snapshot.SupportAreaMinCm2 = 50.0;

			const FPhysAnimSupportContractValidationResult Result = PhysAnimValidators::ValidateSupport(Snapshot);

			TestFalse(TEXT("SUPPORT-02 active support area below minimum fails"), Result.bSupportContractPassed);
			TestEqual(
				TEXT("SUPPORT-02 terminal reason is ActivationSupportFailure"),
				static_cast<uint8>(Result.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationSupportFailure));
		}

		{
			FPhysAnimSupportContractSnapshot Snapshot;
			Snapshot.SupportMode = EPhysAnimSupportMode::Airborne;
			Snapshot.SupportGapTimerMs = 125.0;
			Snapshot.SupportGapMaxMs = 100.0;
			Snapshot.ActiveSupportSideCount = 0;
			Snapshot.SupportHullAreaCm2 = 0.0;
			Snapshot.SupportAreaMinCm2 = 50.0;

			const FPhysAnimSupportContractValidationResult Result = PhysAnimValidators::ValidateSupport(Snapshot);

			TestFalse(TEXT("SUPPORT-03 airborne gap over limit fails"), Result.bSupportContractPassed);
			TestEqual(
				TEXT("SUPPORT-03 terminal reason is ActivationSupportFailure"),
				static_cast<uint8>(Result.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationSupportFailure));
		}

		{
			FPhysAnimSupportContractSnapshot Snapshot;
			Snapshot.SupportMode = EPhysAnimSupportMode::SingleFootSurvival;
			Snapshot.ActiveSupportSideCount = 1;
			Snapshot.SupportHullAreaCm2 = 75.0;
			Snapshot.SupportAreaMinCm2 = 50.0;
			Snapshot.ProxyInsideHull = false;
			Snapshot.ProxyOutsideHullDurationMs = 120.0;
			Snapshot.ProxyTerminalReason = EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion;

			const FPhysAnimSupportContractValidationResult Result = PhysAnimValidators::ValidateSupport(Snapshot);

			TestFalse(TEXT("SUPPORT-04 proxy breach fails"), Result.bSupportContractPassed);
			TestEqual(
				TEXT("SUPPORT-04 terminal reason is ActivationProxyOutsideSupportRegion"),
				static_cast<uint8>(Result.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion));
			TestTrue(TEXT("SUPPORT-04 proxy inside hull copied"), Result.ProxyInsideHull.IsSet() && !Result.ProxyInsideHull.GetValue());
			TestTrue(TEXT("SUPPORT-04 proxy outside duration copied"), Result.ProxyOutsideHullDurationMs.IsSet());
		}

		{
			FPhysAnimSupportContractSnapshot Snapshot;
			Snapshot.SupportMode = EPhysAnimSupportMode::TransientRecovery;
			Snapshot.SupportGapTimerMs = 50.0;
			Snapshot.SupportGapMaxMs = 100.0;
			Snapshot.ActiveSupportSideCount = 0;
			Snapshot.SupportHullAreaCm2 = 0.0;
			Snapshot.SupportAreaMinCm2 = 50.0;

			const FPhysAnimSupportContractValidationResult Result = PhysAnimValidators::ValidateSupport(Snapshot);

			TestTrue(TEXT("SUPPORT-05 zero active support under gap limit does not fail on area"), Result.bSupportContractPassed);
			TestEqual(
				TEXT("SUPPORT-05 terminal reason is None"),
				static_cast<uint8>(Result.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::None));
		}

		{
			FPhysAnimSupportContractSnapshot Snapshot;
			Snapshot.SupportMode = EPhysAnimSupportMode::SingleFootSurvival;
			Snapshot.ActiveSupportSideCount = 1;
			Snapshot.SupportHullAreaCm2 = 25.0;
			Snapshot.SupportAreaMinCm2 = 50.0;
			Snapshot.ProxyTerminalReason = EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion;

			const FPhysAnimSupportContractValidationResult Result = PhysAnimValidators::ValidateSupport(Snapshot);

			TestFalse(TEXT("SUPPORT-06 simultaneous support and proxy breach fails"), Result.bSupportContractPassed);
			TestEqual(
				TEXT("SUPPORT-06 support failure outranks proxy inside one support snapshot"),
				static_cast<uint8>(Result.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationSupportFailure));
		}

		{
			FPhysAnimRunArtifactSnapshotInput Input;
			Input.Values.TerminalSubstepTimestamp = 300;

			FPhysAnimSupportContractSnapshot SupportSnapshot;
			SupportSnapshot.SupportMode = EPhysAnimSupportMode::Airborne;
			SupportSnapshot.SupportGapTimerMs = 125.0;
			SupportSnapshot.SupportGapMaxMs = 100.0;

			Input.Support = PhysAnimValidators::ValidateSupport(SupportSnapshot);

			const FPhysAnimRunArtifactSnapshot Artifact = PhysAnimValidators::BuildRunArtifactSnapshot(Input);

			TestEqual(
				TEXT("SUPPORT-07 support terminal reason flows into artifact fallback arbitration"),
				static_cast<uint8>(Artifact.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationSupportFailure));
			TestEqual(TEXT("SUPPORT-07 artifact timestamp preserved"), Artifact.TerminalSubstepTimestamp, static_cast<int64>(300));
			TestEqual(
				TEXT("SUPPORT-07 artifact support mode copied"),
				static_cast<uint8>(Artifact.SupportMode),
				static_cast<uint8>(EPhysAnimSupportMode::Airborne));
			TestEqual(TEXT("SUPPORT-07 artifact support gap copied"), Artifact.SupportGapTimerMs, 125.0);
		}

		return true;
	}
}
