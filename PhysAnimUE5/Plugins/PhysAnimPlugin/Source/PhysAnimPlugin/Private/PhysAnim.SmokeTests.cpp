#include "PhysAnimComponent.h"
#include "PhysAnimBalanceQuietHandoff.h"
#include "PhysAnimBalance.TestHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"

namespace
{
	using namespace PhysAnimBridge;
	using namespace PhysAnimBalanceTestHelpers;

	const FString PhysAnimPieSmokeMap = TEXT("/Game/ThirdPerson/Lvl_ThirdPerson");

	const TCHAR* PhysAnimPieSmokePrefix = TEXT("[PhysAnimPieSmoke]");
	const TCHAR* PhysAnimPieMovementSmokePrefix = TEXT("[PhysAnimPieMovementSmoke]");
	const TCHAR* PhysAnimPieMovementTraceSmokePrefix = TEXT("[PhysAnimPieMovementTraceSmoke]");
	const TCHAR* PhysAnimPieMovementSoakPrefix = TEXT("[PhysAnimPieMovementSoak]");
	const TCHAR* PhysAnimPieG2PresentationPrefix = TEXT("[PhysAnimPieG2Presentation]");

	constexpr float PhysAnimPieSmokeStartTimeoutSeconds = 30.0f;
	constexpr float PhysAnimPieSmokeStopTimeoutSeconds = 30.0f;
	constexpr float PhysAnimPieSmokeDurationSeconds = 2.0f;
	constexpr float PhysAnimPieMovementSmokeDurationSeconds = 5.0f;
	constexpr float PhysAnimPieMovementSoakDurationSeconds = 30.0f;
	constexpr int32 PhysAnimPieMovementSoakLoopCount = 5;
	constexpr float PhysAnimPieG2PresentationLeadInSeconds = 1.0f;
	constexpr float PhysAnimPieG2PresentationDurationSeconds = 4.0f;
	constexpr float PhysAnimPieBalanceModeSmokeLeadInSeconds = 1.0f;
	constexpr float PhysAnimPieBalanceModeSmokeDurationSeconds = 15.0f;

	DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FSetIntConsoleVariableCommand, FString, Name, int32, Value);
	bool FSetIntConsoleVariableCommand::Update()
	{
		if (IConsoleVariable* const ConsoleVariable = IConsoleManager::Get().FindConsoleVariable(*Name))
		{
			ConsoleVariable->Set(Value);
		}
		return true;
	}

	DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FExecPieConsoleCommand, FString, Command);
	bool FExecPieConsoleCommand::Update()
	{
		if (GEditor && GEditor->PlayWorld)
		{
			GEditor->PlayWorld->Exec(GEditor->PlayWorld, *Command);
		}
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPieSmokeTest,
		"PhysAnim.PIE.Smoke",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimPieSmokeTest::RunTest(const FString& Parameters)
	{
		if (!AutomationOpenMap(PhysAnimPieSmokeMap, true))
		{
			AddError(FString::Printf(TEXT("%s Failed to open map '%s'."), PhysAnimPieSmokePrefix, *PhysAnimPieSmokeMap));
			return false;
		}

		AddCommand(new FStartPIECommand(false));
		AddCommand(new FUntilCommand(
			[]() -> bool { return GEditor != nullptr && IsValid(GEditor->PlayWorld); },
			[this]() -> bool { AddError(TEXT("PIE did not start")); return true; },
			PhysAnimPieSmokeStartTimeoutSeconds));
		AddCommand(new FWaitLatentCommand(PhysAnimPieSmokeDurationSeconds));
		AddCommand(new FEndPlayMapCommand());
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPieMovementSmokeTest,
		"PhysAnim.PIE.MovementSmoke",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimPieMovementSmokeTest::RunTest(const FString& Parameters)
	{
		if (!AutomationOpenMap(PhysAnimPieSmokeMap, true))
		{
			AddError(FString::Printf(TEXT("%s Failed to open map '%s'."), PhysAnimPieMovementSmokePrefix, *PhysAnimPieSmokeMap));
			return false;
		}

		AddCommand(new FSetIntConsoleVariableCommand(TEXT("physanim.MovementSmokeMode"), 1));
		AddCommand(new FStartPIECommand(false));
		AddCommand(new FWaitLatentCommand(PhysAnimPieMovementSmokeDurationSeconds));
		AddCommand(new FEndPlayMapCommand());
		AddCommand(new FSetIntConsoleVariableCommand(TEXT("physanim.MovementSmokeMode"), 0));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPieBalanceModeSmokeTest,
		"PhysAnim.PIE.BalanceModeSmoke",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimPieBalanceModeSmokeTest::RunTest(const FString& Parameters)
	{
		if (!AutomationOpenMap(PhysAnimPieSmokeMap, true))
		{
			AddError(FString::Printf(TEXT("[PhysAnimPieBalanceSmoke] Failed to open map '%s'."), *PhysAnimPieSmokeMap));
			return false;
		}

		AddCommand(new FStartPIECommand(false));
		AddCommand(new FUntilCommand(
			[]() -> bool { return GEditor != nullptr && IsValid(GEditor->PlayWorld); },
			[this]() -> bool { AddError(TEXT("PIE did not start")); return true; },
			PhysAnimPieSmokeStartTimeoutSeconds));
		AddCommand(new FWaitLatentCommand(PhysAnimPieBalanceModeSmokeLeadInSeconds));
		AddCommand(new FWaitLatentCommand(PhysAnimPieBalanceModeSmokeDurationSeconds));
		AddCommand(new FValidateBalanceModeSmokeOutcomeCommand(this));
		AddCommand(new FEndPlayMapCommand());
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPieG2PresentationTest,
		"PhysAnim.PIE.G2Presentation",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimPieG2PresentationTest::RunTest(const FString& Parameters)
	{
		if (!AutomationOpenMap(PhysAnimPieSmokeMap, true))
		{
			AddError(FString::Printf(TEXT("%s Failed to open map '%s'."), PhysAnimPieG2PresentationPrefix, *PhysAnimPieSmokeMap));
			return false;
		}

		AddCommand(new FStartPIECommand(false));
		AddCommand(new FWaitLatentCommand(PhysAnimPieG2PresentationLeadInSeconds));
		AddCommand(new FExecPieConsoleCommand(TEXT("PhysAnim.G2.StartPresentation")));
		AddCommand(new FWaitLatentCommand(PhysAnimPieG2PresentationDurationSeconds));
		AddCommand(new FEndPlayMapCommand());
		return true;
	}
}
