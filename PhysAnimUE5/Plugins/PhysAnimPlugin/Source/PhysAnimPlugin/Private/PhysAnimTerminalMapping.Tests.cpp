#include "PhysAnimValidators.h"
#include "PhysAnimComponent.h"
#include "PhysAnimFailureArbitration.h"
#include "Misc/AutomationTest.h"

namespace
{
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimTerminalMappingAuditTest,
		"PhysAnim.Contract.TerminalMapping.Audit",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimTerminalMappingAuditTest::RunTest(const FString& Parameters)
	{
		// 1. ActivationKineticGateActive -> ActivationContinuousSimulationLost
		{
			FPhysAnimContinuitySnapshot Snapshot;
			Snapshot.bIsBridgeActive = true;
			Snapshot.bKineticGateActive = true;
			Snapshot.bAllCriticalBodiesSimulating = true; // Even if simulating, kinetic gate should fail

			const FPhysAnimContinuityValidationResult Result = PhysAnimValidators::ValidateContinuity(Snapshot);
			
			TestFalse(TEXT("Kinetic gate active during bridge must fail continuity"), Result.bPhysicalContinuityValidatorPassed);
			TestEqual(TEXT("Kinetic gate active must map to ActivationContinuousSimulationLost"), 
				static_cast<uint8>(Result.TerminalReason), 
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationContinuousSimulationLost));
		}

		// 2. ActivationPhysicsNotStarted -> ActivationContinuousSimulationLost
		{
			FPhysAnimRunArtifactSnapshot Artifact;
			Artifact.bPhysicalContinuityValidatorPassed = false;
			Artifact.RuntimeSimulatingBodyCount = 0;

			const EPhysAnimTerminalReason Reason = UPhysAnimComponent::TestOnlyResolveStartupPhysicalContinuityTerminalReason(
				EPhysAnimRuntimeState::WaitingForPoseSearch,
				Artifact);

			TestEqual(TEXT("Startup physics not started must map to ActivationContinuousSimulationLost"), 
				static_cast<uint8>(Reason), 
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationContinuousSimulationLost));
		}

		// 3. Precedence Rank Audit (Match Spec)
		{
			// Rank 1: ActivationPhysicsAssetContractViolation
			TestEqual(TEXT("Rank 1: Physics Asset Violation"), 
				PhysAnimFailureArbitration::GetTerminalReasonRank(EPhysAnimTerminalReason::ActivationPhysicsAssetContractViolation), 1);
			
			// Rank 2: ActivationCapsuleContractViolation
			TestEqual(TEXT("Rank 2: Capsule Violation"), 
				PhysAnimFailureArbitration::GetTerminalReasonRank(EPhysAnimTerminalReason::ActivationCapsuleContractViolation), 2);

			// Rank 4: ActivationContinuousSimulationLost
			TestEqual(TEXT("Rank 4: Continuous Simulation Lost"), 
				PhysAnimFailureArbitration::GetTerminalReasonRank(EPhysAnimTerminalReason::ActivationContinuousSimulationLost), 4);
		}

		return true;
	}
}
