#include "PhysAnimComponent.h"
#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

namespace
{
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimWatchdogRobustnessTest,
		"PhysAnim.Contract.Watchdog.Robustness",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimWatchdogRobustnessTest::RunTest(const FString& Parameters)
	{
		// Fixture
		AActor* const TestActor = GEditor->GetEditorWorldContext().World()->SpawnActor<AActor>();
		UPhysAnimComponent* const TestComp = NewObject<UPhysAnimComponent>(TestActor);
		TestComp->RegisterComponent();

		// 1. Initial State (T=0, No ticks)
		{
			TestFalse(TEXT("WATCH-01 Watchdog must be inactive initially"), TestComp->TestOnlyIsStage2APolicyOutputActive());
		}

		// 2. Ticks > 0 but no update time set (simulating missed initialization)
		{
			TestComp->TestOnlySetPolicyControlTicksExecuted(1);
			// LastPolicyControlUpdateTimeSeconds is -1.0 by default
			TestFalse(TEXT("WATCH-02 Watchdog must be inactive if update time is -1.0 even if ticks > 0"), TestComp->TestOnlyIsStage2APolicyOutputActive());
		}

		// 3. Normal operation
		{
			TestComp->TestOnlySetPolicyControlTicksExecuted(1);
			TestComp->TestOnlySetLastPolicyControlUpdateTimeSeconds(TestComp->TestOnlyGetPhysAnimClockTime());
			TestTrue(TEXT("WATCH-03 Watchdog must be active after update"), TestComp->TestOnlyIsStage2APolicyOutputActive());
		}

		// 4. Timeout
		{
			TestComp->TestOnlySetPolicyControlTicksExecuted(1);
			// Manually set update time to the past
			const double Now = TestComp->TestOnlyGetPhysAnimClockTime();
			TestComp->TestOnlySetLastPolicyControlUpdateTimeSeconds(Now - 2.0); // Timeout is usually 0.1s
			TestFalse(TEXT("WATCH-04 Watchdog must be inactive after timeout"), TestComp->TestOnlyIsStage2APolicyOutputActive());
		}

		// Cleanup
		TestActor->Destroy();
		return true;
	}
}
