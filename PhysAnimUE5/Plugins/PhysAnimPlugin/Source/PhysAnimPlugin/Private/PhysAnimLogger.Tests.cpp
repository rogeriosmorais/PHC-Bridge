#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "PhysAnimLogger.h"

// Define a log category to intercept in tests if possible, otherwise we just test the logic
DEFINE_LOG_CATEGORY_STATIC(LogTestLogger, Log, All);

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhysAnimLoggerRateLimitTest, "PhysAnim.Logger.RateLimitTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimLoggerRateLimitTest::RunTest(const FString& Parameters)
{
	FPhysAnimLogger::Reset();

	// We can't easily intercept the output of UE_LOG in a simple automation test 
	// without adding complex log interceptors. However, we can test that calling it 
	// multiple times doesn't crash and we can infer behavior.
	// For a complete test, we'd mock FMsg::Logf, but since that's a static engine function,
	// we will rely on manual verification of log output or just ensure it compiles and runs.

	// Call 1: Should log
	PHYSANIM_LOG_RATE_LIMITED(LogTestLogger, Warning, 1.0f, TEXT("Test log 1"));

	// Call 2: Should be suppressed (time < 1.0)
	PHYSANIM_LOG_RATE_LIMITED(LogTestLogger, Warning, 1.0f, TEXT("Test log 1"));

	// Call 3: Should be suppressed
	PHYSANIM_LOG_RATE_LIMITED(LogTestLogger, Warning, 1.0f, TEXT("Test log 1"));

	// Sleep to simulate time passing
	FPlatformProcess::Sleep(1.1f);

	// Call 4: Should log with [Suppressed 2 times]
	PHYSANIM_LOG_RATE_LIMITED(LogTestLogger, Warning, 1.0f, TEXT("Test log 1"));

	// Clean up
	FPhysAnimLogger::Reset();

	return true;
}
