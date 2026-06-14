#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"
#include "PhysAnimLogger.h"
#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhysAnimStage2AWalkSmokeTest, "PhysAnim.PIE.Stage2A.WalkSmoke", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

static constexpr float Stage2AWalkSmokeDurationSeconds = 3.0f;
static constexpr float Stage2AWalkSmokeTickSeconds = 0.05f;
static constexpr float Stage2AWalkSmokeMinForwardDisplacementCm = 90.0f;
static constexpr float Stage2AWalkSmokeMaxLateralDriftCm = 30.0f;
static constexpr float Stage2AWalkSmokeMaxVerticalDriftCm = 15.0f;

bool FPhysAnimStage2AWalkSmokeTest::RunTest(const FString& Parameters)
{
	// Setup: Minimal transient actor/component in GWorld
	UWorld* World = GWorld;
	if (!World)
	{
		return false;
	}

	AActor* TestActor = World->SpawnActor<AActor>();
	USceneComponent* Root = NewObject<USceneComponent>(TestActor);
	TestActor->SetRootComponent(Root);
	Root->RegisterComponent();

	UPhysAnimComponent* Component = NewObject<UPhysAnimComponent>(TestActor);
	Component->RegisterComponent();

	// Force the component into the implemented Stage2A-ready state
	Component->RuntimeState = EPhysAnimRuntimeState::BalanceActive_Standing;
	Component->BalanceTransitionShellAuthorityMode = EBalanceTransitionShellAuthorityMode::TransitionOwnedShellLocked;
	Component->BridgeShellState.bInitialized = true;
	Component->bPolicyTargetsAppliedLastFrame = true;
	Component->PolicyControlTicksExecuted = 1;
	Component->bStage2APhase3SimRootAttempted = false;
	Component->Stage2AConsecutivePolicyActiveFrames = 3;
	Component->LastPolicyControlUpdateTimeSeconds = Component->GetPhysAnimClockTime();

	// Record StartLocation and StartForward from OwnerActor
	const FVector StartLocation = TestActor->GetActorLocation();
	const FVector StartForward = TestActor->GetActorForwardVector();

	// Run exactly 60 iterations: 3.0 seconds / 0.05 seconds
	const int32 Iterations = FMath::RoundToInt(Stage2AWalkSmokeDurationSeconds / Stage2AWalkSmokeTickSeconds);
	for (int32 i = 0; i < Iterations; ++i)
	{
		Component->TryOpenStage2ALocomotionRequestGate(TEXT("WalkSmoke"));
		Component->TryActivateStage2AWalkIntent(Stage2AWalkSmokeTickSeconds);
		
		// Mock watchdog update to keep it live
		Component->LastPolicyControlUpdateTimeSeconds = Component->GetPhysAnimClockTime();
	}

	const FVector EndLocation = TestActor->GetActorLocation();

	// After the loop, compute metrics
	const FVector Delta = EndLocation - StartLocation;
	const float ForwardDisplacementCm = FVector::DotProduct(Delta, StartForward);
	
	const FVector ForwardNormalized = StartForward.GetSafeNormal();
	const FVector RightNormalized = FVector::CrossProduct(FVector::UpVector, ForwardNormalized).GetSafeNormal();
	const float LateralDriftCm = FMath::Abs(FVector::DotProduct(Delta, RightNormalized));
	const float VerticalDriftCm = FMath::Abs(Delta.Z);

	// Assertions
	TestTrue(TEXT("SMOKE-01 ForwardDisplacementCm >= 90.0f"), ForwardDisplacementCm >= Stage2AWalkSmokeMinForwardDisplacementCm);
	TestTrue(TEXT("SMOKE-02 LateralDriftCm <= 30.0f"), LateralDriftCm <= Stage2AWalkSmokeMaxLateralDriftCm);
	TestTrue(TEXT("SMOKE-03 VerticalDriftCm <= 15.0f"), VerticalDriftCm <= Stage2AWalkSmokeMaxVerticalDriftCm);
	
	TestEqual(TEXT("SMOKE-04 RuntimeState is LocomotionActiveShell"), (int32)Component->RuntimeState, (int32)EPhysAnimRuntimeState::LocomotionActiveShell);
	TestEqual(TEXT("SMOKE-05 AuthorityState is Locomoting"), (int32)Component->BridgeLocomotionAuthorityState, (int32)EBridgeLocomotionAuthorityState::Locomoting);
	TestEqual(TEXT("SMOKE-06 TerminalState is Allowed"), (int32)Component->Stage2ALastLocomotionTerminalState, (int32)EStage2ALocomotionTerminalState::Allowed);

	// Telemetry assertions
	TestTrue(TEXT("SMOKE-07 Telemetry contains root_mode=Stage1_KinematicRoot"), Component->LastStage2ALocomotionTelemetryLine.Contains(TEXT("root_mode=Stage1_KinematicRoot")));
	TestTrue(TEXT("SMOKE-08 Telemetry contains locomotion_intent=WalkForward"), Component->LastStage2ALocomotionTelemetryLine.Contains(TEXT("locomotion_intent=WalkForward")));
	TestTrue(TEXT("SMOKE-09 Telemetry contains capsule_or_shell_motion_source=Stage2A_KinematicShell"), Component->LastStage2ALocomotionTelemetryLine.Contains(TEXT("capsule_or_shell_motion_source=Stage2A_KinematicShell")));
	TestTrue(TEXT("SMOKE-10 Telemetry contains phase3_simroot_attempted=false"), Component->LastStage2ALocomotionTelemetryLine.Contains(TEXT("phase3_simroot_attempted=false")));
	TestTrue(TEXT("SMOKE-11 Telemetry contains terminal_state=Allowed"), Component->LastStage2ALocomotionTelemetryLine.Contains(TEXT("terminal_state=Allowed")));

	// Emit final log line
	PHYSANIM_LOG(LogPhysAnimBridge, Display, TEXT("PASS_STAGE2A_WALK_SMOKE duration_seconds=3.0 forward_displacement_cm=%.3f lateral_drift_cm=%.3f vertical_drift_cm=%.3f terminal_state=%s"), 
		ForwardDisplacementCm, LateralDriftCm, VerticalDriftCm, UPhysAnimComponent::Stage2ATerminalStateToString(Component->Stage2ALastLocomotionTerminalState));

	// Cleanup
	TestActor->Destroy();

	return true;
}
