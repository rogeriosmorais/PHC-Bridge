#include "PhysAnimBalanceReadyTransition.h"
#include "PhysAnimBalanceReadyTransitionPrivate.h"
#include "PhysAnimComponent.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPhase3HardeningTests,
		"PhysAnim.Bridge.Phase3Hardening",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimPhase3HardeningTests::RunTest(const FString& Parameters)
	{
		FBalanceReadyTransitionDiagnostics Diags;
		// 1. TickCount == 0 must not trigger release grace.
		{
			const bool bGraceActive = FPhysAnimBalanceReadyTransition::IsPhase3EarlySettleInstabilityGraceActive(
				Diags,
				0,     // KineticGateReleaseTickCount
				0,     // Phase3TickCount
				true,  // bTransitionOwnedShellLocked
				true,  // bLocomotionAuthorityIdle
				10.0f, // RootLinearSpeed
				100.0f, // LinearThreshold
				5.0f,  // RootAngularSpeed
				45.0f, // AngularThreshold
				0.0f,  // ShellPlanarOffsetCm
				5.0f,  // MaxAllowedShellOffsetCm
				5.0f,  // CurrentMaxNonRootAngularSpeed
				30.0f, // NonRootAngularThreshold
				1.0f,  // PrePhase3PeakRootAngularSpeed
				1.0f,  // PrePhase3PeakNonRootAngularSpeed
				1.0f,  // CurrentShellVelocity
				10.0f, // ShellVelocityThreshold
				1.0f,  // PrePhase3PeakShellVelocity
				TEXT("spine_01"),
				0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

			TestFalse(TEXT("KineticGateReleaseTickCount == 0 must not trigger instability grace"), bGraceActive);
		}

		// 2. Shell offset breach during kinetic grace must still be material.
		{
			// Even with high KineticGateReleaseTickCount, offset breach should fail.
			const bool bMaterial = FPhysAnimBalanceReadyTransition::IsMaterialPhase3ShellCorrectionActive(
				Diags,
				5,     // KineticGateReleaseTickCount (within grace)
				true,  // bTransitionOwnedShellLocked
				true,  // bLocomotionAuthorityIdle
				5,     // Phase3TickCount
				15.0f, // ShellPlanarOffsetCm (breached: > 5.0f)
				100.0f, // ShellPlanarVelocityCmPerSec
				5.0f,  // MaxAllowedShellOffsetCm
				10.0f  // MaxAllowedShellVelocityCmPerSec
			);

			TestTrue(TEXT("Shell offset breach during kinetic grace must be material"), bMaterial);
		}

		// 3. Root angular burst cannot borrow non-root pre-Phase 3 angular peak.
		{
			// If we have a huge non-root peak but a small root peak, 
			// the root angular threshold should not be expanded by the non-root peak.
			
			const float RootAngularSpeed = 500.0f; // High speed
			const float AngularThreshold = 45.0f;
			const float PrePhase3PeakRootAngularSpeed = 1.0f; // Small root peak
			const float PrePhase3PeakNonRootAngularSpeed = 1000.0f; // Huge non-root peak
			
			const bool bGraceActive = FPhysAnimBalanceReadyTransition::IsPhase3EarlySettleInstabilityGraceActive(
				Diags,
				5,      // KineticGateReleaseTickCount
				5,      // Phase3TickCount
				true,   // bTransitionOwnedShellLocked
				true,   // bLocomotionAuthorityIdle
				10.0f,  // RootLinearSpeed
				100.0f, // LinearThreshold
				RootAngularSpeed,
				AngularThreshold,
				0.0f,   // ShellPlanarOffsetCm
				5.0f,   // MaxAllowedShellOffsetCm
				5.0f,   // CurrentMaxNonRootAngularSpeed
				30.0f,  // NonRootAngularThreshold
				PrePhase3PeakRootAngularSpeed,
				PrePhase3PeakNonRootAngularSpeed,
				1.0f,   // CurrentShellVelocity
				10.0f,  // ShellVelocityThreshold
				1.0f,   // PrePhase3PeakShellVelocity
				TEXT("spine_01"),
				0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

			// RootAngularExpansionLimit = Max(1.0 * 100, 45) = 100.
			// CurrentEnergyMultiplier @ tick 5 = Lerp(1, 150, Exp(-5/15)) = Lerp(1, 150, 0.716) = 107.7.
			// DynamicAngularThreshold = 45 * 107.7 = 4846.5.
			// Wait, the dynamic threshold is huge because of the 150x multiplier.
			
			// Let's adjust the multiplier in the test or wait for the decay.
			// If KineticGateReleaseTickCount is 40:
			// DecayAlpha = Exp(-40/15) = 0.069.
			// CurrentEnergyMultiplier = Lerp(1, 150, 0.069) = 11.35.
			// DynamicAngularThreshold = 45 * 11.35 = 510.75.
			// RootAngularExpansionLimit = Max(1.0 * 100, 45) = 100.
			// FinalRootAngularThreshold = Max(100, 510.75) = 510.75.
			
			// If RootAngularSpeed is 600, it should fail.
			
			const bool bGraceActiveAtTick40 = FPhysAnimBalanceReadyTransition::IsPhase3EarlySettleInstabilityGraceActive(
				Diags,
				40,     // KineticGateReleaseTickCount
				40,     // Phase3TickCount
				true,   // bTransitionOwnedShellLocked
				true,   // bLocomotionAuthorityIdle
				10.0f,  // RootLinearSpeed
				100.0f, // LinearThreshold
				600.0f, // RootAngularSpeed
				AngularThreshold,
				0.0f,   // ShellPlanarOffsetCm
				5.0f,   // MaxAllowedShellOffsetCm
				5.0f,   // CurrentMaxNonRootAngularSpeed
				30.0f,  // NonRootAngularThreshold
				PrePhase3PeakRootAngularSpeed,
				PrePhase3PeakNonRootAngularSpeed,
				1.0f,   // CurrentShellVelocity
				10.0f,  // ShellVelocityThreshold
				1.0f,   // PrePhase3PeakShellVelocity
				TEXT("spine_01"),
				0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
				
			TestFalse(TEXT("Root angular burst cannot borrow non-root peak for expansion"), bGraceActiveAtTick40);
		}

		// 4. Global watchdog grace should apply only for SettleTicks 1..40, not 0, not 999.
		{
			const float AngularThreshold = 45.0f;
			const float RootAngularSpeed = 600.0f; // Above base but below initial grace

			// Tick 0 -> False
			TestFalse(TEXT("Watchdog grace inactive at tick 0"), 
				FPhysAnimBalanceReadyTransition::IsPhase3EarlySettleInstabilityGraceActive(
				Diags,
					0, 0, true, true, 10.0f, 100.0f, 5000.0f, 45.0f, 0.0f, 5.0f, 5.0f, 30.0f, 1.0f, 1.0f, 1.0f, 10.0f, 1.0f, TEXT("spine_01"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

			// Tick 20 -> True (within 60 ticks max)
			TestTrue(TEXT("Watchdog grace active at tick 20"), 
				FPhysAnimBalanceReadyTransition::IsPhase3EarlySettleInstabilityGraceActive(
				Diags,
					20, 20, true, true, 10.0f, 100.0f, 80.0f, 45.0f, 0.0f, 5.0f, 5.0f, 30.0f, 60.0f, 1.0f, 1.0f, 10.0f, 1.0f, TEXT("spine_01"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

			// Tick 999 -> False
			TestFalse(TEXT("Watchdog grace inactive at tick 999"), 
				FPhysAnimBalanceReadyTransition::IsPhase3EarlySettleInstabilityGraceActive(
				Diags,
					999, 999, true, true, 10.0f, 100.0f, RootAngularSpeed, AngularThreshold, 0.0f, 5.0f, 5.0f, 30.0f, 1.0f, 1.0f, 1.0f, 10.0f, 1.0f, TEXT("spine_01"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
		}

		return true;
	}
}

#endif
