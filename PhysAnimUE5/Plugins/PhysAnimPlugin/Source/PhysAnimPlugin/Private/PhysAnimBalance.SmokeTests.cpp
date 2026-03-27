#if WITH_DEV_AUTOMATION_TESTS

#include "PhysAnimBridge.h"
#include "PhysAnimComponent.h"
#include "PhysAnimBalanceQuietHandoff.h"
#include "PhysAnimBalance.TestHelpers.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealEdGlobals.h"
#endif

namespace
{
#if WITH_EDITOR
	const FString PhysAnimPieSmokeMap = TEXT("/Game/ThirdPerson/Lvl_ThirdPerson");
	constexpr float PhysAnimPieBalanceModeSmokeLeadInSeconds = 1.0f;
	constexpr float PhysAnimPieBalanceModeSmokeDurationSeconds = 15.0f;
	constexpr float PhysAnimPieSmokeStartTimeoutSeconds = 30.0f;
	constexpr float PhysAnimPieSmokeStopTimeoutSeconds = 30.0f;

	using namespace PhysAnimBalanceTestHelpers;

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPieBalanceModeTest,
		"PhysAnim.PIE.BalanceMode",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPieBalanceQuietWindowTest,
		"PhysAnim.PIE.BalanceQuietWindow",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

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

		AddCommand(new FEditorAutomationLogCommand(FString::Printf(
			TEXT("[PhysAnimPieBalanceSmoke] PIE balance smoke opening '%s'."),
			*PhysAnimPieSmokeMap)));
		AddCommand(new FStartPIECommand(false));
		AddCommand(new FUntilCommand(
			[]() -> bool
			{
				return GEditor != nullptr && IsValid(GEditor->PlayWorld);
			},
			[this]() -> bool
			{
				AddError(FString::Printf(
					TEXT("[PhysAnimPieBalanceSmoke] PIE did not start within %.1f seconds."),
					PhysAnimPieSmokeStartTimeoutSeconds));
				return true;
			},
			PhysAnimPieSmokeStartTimeoutSeconds));
		AddCommand(new FWaitLatentCommand(PhysAnimPieBalanceModeSmokeLeadInSeconds));
		AddCommand(new FWaitLatentCommand(PhysAnimPieBalanceModeSmokeDurationSeconds));
		AddCommand(new FValidateBalanceModeSmokeOutcomeCommand(this));
		AddCommand(new FEndPlayMapCommand());
		AddCommand(new FUntilCommand(
			[]() -> bool
			{
				return GEditor == nullptr || !IsValid(GEditor->PlayWorld);
			},
			[this]() -> bool
			{
				AddError(FString::Printf(
					TEXT("[PhysAnimPieBalanceSmoke] PIE did not stop within %.1f seconds."),
					PhysAnimPieSmokeStopTimeoutSeconds));
				return true;
			},
			PhysAnimPieSmokeStopTimeoutSeconds));

		return true;
	}

	bool FPhysAnimPieBalanceQuietWindowTest::RunTest(const FString& Parameters)
	{
		if (!AutomationOpenMap(PhysAnimPieSmokeMap, true))
		{
			AddError(FString::Printf(TEXT("[PhysAnimPieBalanceQuietWindow] Failed to open map '%s'."), *PhysAnimPieSmokeMap));
			return false;
		}

		AddCommand(new FEditorAutomationLogCommand(FString::Printf(
			TEXT("[PhysAnimPieBalanceQuietWindow] PIE balance quiet-window test opening '%s'."),
			*PhysAnimPieSmokeMap)));
		AddCommand(new FStartPIECommand(false));
		AddCommand(new FUntilCommand(
			[]() -> bool
			{
				return GEditor != nullptr && IsValid(GEditor->PlayWorld);
			},
			[this]() -> bool
			{
				AddError(FString::Printf(
					TEXT("[PhysAnimPieBalanceQuietWindow] PIE did not start within %.1f seconds."),
					PhysAnimPieSmokeStartTimeoutSeconds));
				return true;
			},
			PhysAnimPieSmokeStartTimeoutSeconds));
		AddCommand(new FWaitLatentCommand(PhysAnimPieBalanceModeSmokeLeadInSeconds));
		AddCommand(new FWaitLatentCommand(PhysAnimPieBalanceModeSmokeDurationSeconds));
		AddCommand(new FValidateBalanceModeSmokeOutcomeCommand(this));
		AddCommand(new FEndPlayMapCommand());
		AddCommand(new FUntilCommand(
			[]() -> bool
			{
				return GEditor == nullptr || !IsValid(GEditor->PlayWorld);
			},
			[this]() -> bool
			{
				AddError(FString::Printf(
					TEXT("[PhysAnimPieBalanceQuietWindow] PIE did not stop within %.1f seconds."),
					PhysAnimPieSmokeStopTimeoutSeconds));
				return true;
			},
			PhysAnimPieSmokeStopTimeoutSeconds));

		return true;
	}

	bool FPhysAnimPieBalanceModeTest::RunTest(const FString& Parameters)
	{
		if (!AutomationOpenMap(PhysAnimPieSmokeMap, true))
		{
			AddError(FString::Printf(TEXT("%s Failed to open map '%s'."), TEXT("[PhysAnimPieBalance]"), *PhysAnimPieSmokeMap));
			return false;
		}

		AddCommand(new FEditorAutomationLogCommand(FString::Printf(
			TEXT("[PhysAnimPieBalance] PIE balance mode opening '%s'."),
			*PhysAnimPieSmokeMap)));
		AddCommand(new FStartPIECommand(false));
		AddCommand(new FUntilCommand(
			[]() -> bool
			{
				return GEditor != nullptr && IsValid(GEditor->PlayWorld);
			},
			[this]() -> bool
			{
				AddError(FString::Printf(
					TEXT("[PhysAnimPieBalance] PIE did not start within %.1f seconds."),
					PhysAnimPieSmokeStartTimeoutSeconds));
				return true;
			},
			PhysAnimPieSmokeStartTimeoutSeconds));

		AddCommand(new FWaitLatentCommand(1.0f));
		AddCommand(new FUntilCommand(
			[]() -> bool
			{
				if (!GEditor || !IsValid(GEditor->PlayWorld)) return false;
				for (TObjectIterator<UPhysAnimComponent> It; It; ++It)
				{
					if (IsValid(*It) && It->GetWorld() == GEditor->PlayWorld &&
						It->GetRuntimeState() == EPhysAnimRuntimeState::BalanceActive_Recovery)
					{
						return true;
					}
				}
				return false;
			},
			[this]() -> bool
			{
				AddError(TEXT("[PhysAnimPieBalance] Balance mode never started automatically."));
				return true;
			},
			30.0f));

		AddCommand(new FUntilCommand(
			[]() -> bool
			{
				if (!GEditor || !IsValid(GEditor->PlayWorld)) return true;
				for (TObjectIterator<UPhysAnimComponent> It; It; ++It)
				{
					if (IsValid(*It) && It->GetWorld() == GEditor->PlayWorld)
					{
						if (It->GetRuntimeState() == EPhysAnimRuntimeState::BalanceActive_Recovery)
						{
							return false;
						}
					}
				}
				return true;
			},
			[this]() -> bool
			{
				AddError(TEXT("[PhysAnimPieBalance] Scenarios did not complete within timeout."));
				return true;
			},
			120.0f));

		AddCommand(new FEndPlayMapCommand());
		AddCommand(new FUntilCommand(
			[]() -> bool
			{
				return GEditor == nullptr || !IsValid(GEditor->PlayWorld);
			},
			[this]() -> bool
			{
				AddError(FString::Printf(
					TEXT("[PhysAnimPieBalance] PIE did not stop within %.1f seconds."),
					PhysAnimPieSmokeStopTimeoutSeconds));
				return true;
			},
			PhysAnimPieSmokeStopTimeoutSeconds));

		return true;
	}
#endif
}

#endif
