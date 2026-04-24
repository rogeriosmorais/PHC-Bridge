#include "PhysAnimSupportTruth.h"
#include "Misc/AutomationTest.h"

namespace
{
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimSupportTruthHarnessTest,
		"PhysAnim.SupportTruth.Harness.CompilesAndRuns",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimSupportTruthHarnessTest::RunTest(const FString& Parameters)
	{
		TestTrue(TEXT("Support truth harness compiles and runs"), true);
		return true;
	}
}
