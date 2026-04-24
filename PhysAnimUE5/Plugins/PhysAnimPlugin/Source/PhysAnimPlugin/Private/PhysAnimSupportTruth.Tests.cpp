#include "PhysAnimSupportTruth.h"
#include "Misc/AutomationTest.h"

namespace
{
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimSupportReportWindowTest,
		"PhysAnim.SupportTruth.ReduceSupportModeForReportWindow",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimSupportReportWindowTest::RunTest(const FString& Parameters)
	{
		// LOGIC-14: 30 Hz tie-break (Equal durations)
		{
			FPhysAnimSupportReportWindowInput Input;
			Input.Modes = { EPhysAnimSupportMode::TwoFootStable, EPhysAnimSupportMode::Airborne };
			Input.DurationsMs = { 33.3, 33.3 };
			
			FPhysAnimSupportReportWindowResult Result = PhysAnimSupportTruth::ReduceSupportModeForReportWindow(Input);
			TestEqual(TEXT("LOGIC-14: Equal duration tie-break chooses most severe (Airborne)"), Result.SupportMode, EPhysAnimSupportMode::Airborne);
			TestEqual(TEXT("LOGIC-14: Total duration is correct"), Result.TotalWindowDurationMs, 66.6);
		}

		// LOGIC-14A: Empty or zero input
		{
			FPhysAnimSupportReportWindowInput Input;
			FPhysAnimSupportReportWindowResult Result = PhysAnimSupportTruth::ReduceSupportModeForReportWindow(Input);
			TestEqual(TEXT("LOGIC-14A: Empty input returns Airborne"), Result.SupportMode, EPhysAnimSupportMode::Airborne);
			TestEqual(TEXT("LOGIC-14A: Empty input returns 0.0 duration"), Result.TotalWindowDurationMs, 0.0);
			TestTrue(TEXT("LOGIC-14A: Empty input is valid"), Result.bValidInput);
		}

		// LOGIC-14B: Array length mismatch
		{
			FPhysAnimSupportReportWindowInput Input;
			Input.Modes = { EPhysAnimSupportMode::TwoFootStable };
			Input.DurationsMs = { 33.3, 33.3 };
			
			FPhysAnimSupportReportWindowResult Result = PhysAnimSupportTruth::ReduceSupportModeForReportWindow(Input);
			TestEqual(TEXT("LOGIC-14B: Mismatched length returns Airborne"), Result.SupportMode, EPhysAnimSupportMode::Airborne);
			TestEqual(TEXT("LOGIC-14B: Mismatched length returns 0.0 duration"), Result.TotalWindowDurationMs, 0.0);
			TestFalse(TEXT("LOGIC-14B: Mismatched length is invalid"), Result.bValidInput);
		}

		// LOGIC-14C: Negative duration
		{
			FPhysAnimSupportReportWindowInput Input;
			Input.Modes = { EPhysAnimSupportMode::TwoFootStable, EPhysAnimSupportMode::SingleFootSurvival };
			Input.DurationsMs = { 100.0, -50.0 };
			
			FPhysAnimSupportReportWindowResult Result = PhysAnimSupportTruth::ReduceSupportModeForReportWindow(Input);
			TestEqual(TEXT("LOGIC-14C: Negative duration clamped to 0.0, making other mode dominant"), Result.SupportMode, EPhysAnimSupportMode::TwoFootStable);
			TestEqual(TEXT("LOGIC-14C: Total duration only counts non-negative"), Result.TotalWindowDurationMs, 100.0);
			TestTrue(TEXT("LOGIC-14C: Negative duration input is technically valid but clamped"), Result.bValidInput);
		}

		return true;
	}
}
