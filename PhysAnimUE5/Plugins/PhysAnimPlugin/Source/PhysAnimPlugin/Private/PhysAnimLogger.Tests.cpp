#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "PhysAnimLogger.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

// Define a log category to intercept in tests if possible, otherwise we just test the logic
DEFINE_LOG_CATEGORY_STATIC(LogTestLogger, Log, All);

// Custom FOutputDevice to capture serialized log entries
class FPhysAnimTestLogDevice : public FOutputDevice
{
public:
	struct FLogEntry
	{
		FString Message;
		ELogVerbosity::Type Verbosity;
		FName Category;
	};

	TArray<FLogEntry> Entries;

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override
	{
		Entries.Add({ FString(V), Verbosity, Category });
	}
};

// RAII Helper to register and deregister the log device automatically
struct FScopedLogCapture
{
	FPhysAnimTestLogDevice Device;

	FScopedLogCapture()
	{
		if (GLog)
		{
			GLog->AddOutputDevice(&Device);
		}
	}

	~FScopedLogCapture()
	{
		if (GLog)
		{
			GLog->RemoveOutputDevice(&Device);
		}
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhysAnimLoggerRateLimitTest, "PhysAnim.Logger.RateLimitTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimLoggerRateLimitTest::RunTest(const FString& Parameters)
{
	FPhysAnimLogger::Reset();

	auto FlushLogs = []()
	{
		if (GLog)
		{
			GLog->Flush();
		}
		FPlatformProcess::Sleep(0.01f);
	};

	// 1. Rate Limit & Suppression Summary Test
	TArray<FPhysAnimTestLogDevice::FLogEntry> CapturedEntries1;
	{
		FScopedLogCapture Capture;

		// Lambda to ensure calls originate from the same line (same __LINE__)
		auto LogMsg = [](float TimeLimit, const FString& Msg)
		{
			PHYSANIM_LOG_RATE_LIMITED(LogTestLogger, Log, TimeLimit, TEXT("%s"), *Msg);
		};

		// Call 1: Should log immediately
		LogMsg(0.05f, TEXT("RateLimitMsg"));
		FlushLogs();

		// Call 2: Within 0.05s, should be suppressed
		LogMsg(0.05f, TEXT("RateLimitMsg"));
		FlushLogs();

		// Call 3: Within 0.05s, should be suppressed
		LogMsg(0.05f, TEXT("RateLimitMsg"));
		FlushLogs();

		// Sleep to exceed the 0.05s limit
		FPlatformProcess::Sleep(0.07f);

		// Call 4: Should log and include the suppression summary
		LogMsg(0.05f, TEXT("RateLimitMsg"));
		FlushLogs();

		CapturedEntries1 = Capture.Device.Entries;
	}

	// Safe diagnostic output (picked up by read_logs.py because of 'PhysA_DIAG')
	for (const auto& Entry : CapturedEntries1)
	{
		FMsg::Logf(nullptr, 0, LogPhysAnim.GetCategoryName(), ELogVerbosity::Log, TEXT("PhysA_DIAG_1: Category=%s Message=%s"), *Entry.Category.ToString(), *Entry.Message);
	}

	// Verify captured entries
	TArray<FPhysAnimTestLogDevice::FLogEntry> Filtered;
	for (const auto& Entry : CapturedEntries1)
	{
		if (Entry.Category == LogTestLogger.GetCategoryName() && Entry.Message.Contains(TEXT("RateLimitMsg")))
		{
			Filtered.Add(Entry);
		}
	}

	// We expect exactly 2 filtered messages:
	// 1. "RateLimitMsg"
	// 2. "[Suppressed 2 times] RateLimitMsg"
	TestEqual(TEXT("Should have logged exactly 2 times"), Filtered.Num(), 2);
	if (Filtered.Num() >= 2)
	{
		TestEqual(TEXT("First log message matches"), Filtered[0].Message, TEXT("RateLimitMsg"));
		TestTrue(TEXT("Second log message contains suppression summary"), Filtered[1].Message.Contains(TEXT("[Suppressed 2 times]")));
	}

	// 2. Reset Behavior Test
	FPhysAnimLogger::Reset();
	TArray<FPhysAnimTestLogDevice::FLogEntry> CapturedEntries2;
	{
		FScopedLogCapture Capture;

		// Log once with a large time limit (e.g., 100.0s)
		FPhysAnimLogger::LogRateLimited(LogTestLogger.GetCategoryName(), ELogVerbosity::Log, __FILE__, 300, 100.0f, TEXT("ResetTestMsg"));
		FlushLogs();
		
		// Logging again immediately should be suppressed
		FPhysAnimLogger::LogRateLimited(LogTestLogger.GetCategoryName(), ELogVerbosity::Log, __FILE__, 300, 100.0f, TEXT("ResetTestMsg"));
		FlushLogs();

		// Reset the logger
		FPhysAnimLogger::Reset();

		// Logging again now should succeed immediately because the history is cleared
		FPhysAnimLogger::LogRateLimited(LogTestLogger.GetCategoryName(), ELogVerbosity::Log, __FILE__, 300, 100.0f, TEXT("ResetTestMsg"));
		FlushLogs();

		CapturedEntries2 = Capture.Device.Entries;
	}

	// Safe diagnostic output
	for (const auto& Entry : CapturedEntries2)
	{
		FMsg::Logf(nullptr, 0, LogPhysAnim.GetCategoryName(), ELogVerbosity::Log, TEXT("PhysA_DIAG_2: Category=%s Message=%s"), *Entry.Category.ToString(), *Entry.Message);
	}

	TArray<FString> ResetMsgs;
	for (const auto& Entry : CapturedEntries2)
	{
		if (Entry.Category == LogTestLogger.GetCategoryName() && Entry.Message.Contains(TEXT("ResetTestMsg")))
		{
			ResetMsgs.Add(Entry.Message);
		}
	}

	// We expect exactly 2 occurrences of "ResetTestMsg" (the first call, and the post-reset call)
	TestEqual(TEXT("Should have logged exactly 2 times due to Reset"), ResetMsgs.Num(), 2);

	FPhysAnimLogger::Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhysAnimLoggerBypassTest, "PhysAnim.Logger.BypassTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimLoggerBypassTest::RunTest(const FString& Parameters)
{
	// Expected error verification patterns (required to avoid Editor test failures when logging Error verbosity)
	AddExpectedError(TEXT("CapOver_Error"), EAutomationExpectedErrorFlags::Contains, 1);

	FPhysAnimLogger::Reset();

	auto FlushLogs = []()
	{
		if (GLog)
		{
			GLog->Flush();
		}
		FPlatformProcess::Sleep(0.01f);
	};

	{
		FScopedLogCapture Capture;

		// 1. Log 10 normal messages on different lines (simulated) to fill the cap
		for (int32 i = 0; i < 10; ++i)
		{
			FPhysAnimLogger::LogRateLimited(LogTestLogger.GetCategoryName(), ELogVerbosity::Log, __FILE__, 100 + i, 0.0f, FString::Printf(TEXT("CapFill_%d"), i));
			FlushLogs();
		}

		// 2. Log 11th normal message (should be dropped by cap)
		FPhysAnimLogger::LogRateLimited(LogTestLogger.GetCategoryName(), ELogVerbosity::Log, __FILE__, 200, 1.0f, TEXT("CapOver_Normal"));
		FlushLogs();

		// 3. Log 12th message that is an Error (should bypass cap)
		FPhysAnimLogger::LogRateLimited(LogTestLogger.GetCategoryName(), ELogVerbosity::Error, __FILE__, 201, 0.0f, TEXT("CapOver_Error"));
		FlushLogs();

		// 4. Log 13th message that is unrated (PHYSANIM_LOG maps to TimeLimit <= 0)
		PHYSANIM_LOG(LogTestLogger, Log, TEXT("CapOver_Unrated"));
		FlushLogs();

		// 5. Log 14th message containing AttemptResult (should bypass cap)
		FPhysAnimLogger::LogRateLimited(LogTestLogger.GetCategoryName(), ELogVerbosity::Log, __FILE__, 202, 0.0f, TEXT("CapOver_AttemptResult"));
		FlushLogs();

		// 6. Log 15th message containing TerminalArtifact (should bypass cap)
		FPhysAnimLogger::LogRateLimited(LogTestLogger.GetCategoryName(), ELogVerbosity::Log, __FILE__, 203, 0.0f, TEXT("CapOver_TerminalArtifact"));
		FlushLogs();

		// Verify captured entries
		TArray<FString> CapturedMsgs;
		for (const auto& Entry : Capture.Device.Entries)
		{
			if (Entry.Category == LogTestLogger.GetCategoryName())
			{
				CapturedMsgs.Add(Entry.Message);
			}
		}

		// We expect the 10 CapFill messages, NOT CapOver_Normal, and YES to CapOver_Error, CapOver_Unrated, CapOver_AttemptResult, CapOver_TerminalArtifact.
		// Total: 10 + 4 = 14 messages.
		TestEqual(TEXT("Total messages logged should match expected bypasses"), CapturedMsgs.Num(), 14);

		TestTrue(TEXT("CapFill_0 was logged"), CapturedMsgs.Contains(TEXT("CapFill_0")));
		TestTrue(TEXT("CapFill_9 was logged"), CapturedMsgs.Contains(TEXT("CapFill_9")));
		TestFalse(TEXT("Normal log exceeding cap was dropped"), CapturedMsgs.Contains(TEXT("CapOver_Normal")));
		TestTrue(TEXT("Error log bypassed cap"), CapturedMsgs.Contains(TEXT("CapOver_Error")));
		TestTrue(TEXT("Unrated log bypassed cap"), CapturedMsgs.Contains(TEXT("CapOver_Unrated")));
		TestTrue(TEXT("AttemptResult log bypassed cap"), CapturedMsgs.Contains(TEXT("CapOver_AttemptResult")));
		TestTrue(TEXT("TerminalArtifact log bypassed cap"), CapturedMsgs.Contains(TEXT("CapOver_TerminalArtifact")));
	}

	// 7. Attempt-Log File Creation Test
	FPhysAnimLogger::Reset();
	{
		const FString AttemptUuid = TEXT("test-attempt-unreal-automation-999");
		FPhysAnimLogger::SetCurrentAttemptUuid(AttemptUuid);

		// Construct expected path
		const FString LogDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT("test-results"), TEXT("logs"));
		const FString LogFilePath = FPaths::Combine(LogDirectory, FString::Printf(TEXT("%s.log"), *AttemptUuid));

		// Make sure the directory does not exist initially (delete recursively)
		if (IFileManager::Get().DirectoryExists(*LogDirectory))
		{
			IFileManager::Get().DeleteDirectory(*LogDirectory, false, true);
		}
		TestFalse(TEXT("Log directory should not exist initially"), IFileManager::Get().DirectoryExists(*LogDirectory));

		// Log a message
		const FString LogMessageText = TEXT("Test log line for attempt file");
		FPhysAnimLogger::LogRateLimited(LogTestLogger.GetCategoryName(), ELogVerbosity::Log, __FILE__, 400, 0.0f, LogMessageText);
		FlushLogs();

		// Verify the directory exists
		bool bDirExists = IFileManager::Get().DirectoryExists(*LogDirectory);
		TestTrue(TEXT("Log directory should be created by the logger"), bDirExists);

		// Verify the file exists
		bool bFileExists = IFileManager::Get().FileExists(*LogFilePath);
		TestTrue(TEXT("Attempt log file should be created"), bFileExists);

		if (bFileExists)
		{
			FString FileContents;
			bool bReadSuccess = FFileHelper::LoadFileToString(FileContents, *LogFilePath);
			TestTrue(TEXT("Should successfully read the log file"), bReadSuccess);
			TestTrue(TEXT("Log file should contain the category name"), FileContents.Contains(LogTestLogger.GetCategoryName().ToString()));
			TestTrue(TEXT("Log file should contain the logged message"), FileContents.Contains(LogMessageText));

			// Clean up the file
			IFileManager::Get().Delete(*LogFilePath);
		}

		// Clear the attempt UUID
		FPhysAnimLogger::SetCurrentAttemptUuid(TEXT(""));
	}

	// 8. Frame Cap Reset Test
	FPhysAnimLogger::Reset();
	{
		FScopedLogCapture Capture;

		// Fill the frame cap (MAX_LOGS_PER_FRAME is 10)
		for (int32 i = 0; i < 10; ++i)
		{
			FPhysAnimLogger::LogRateLimited(LogTestLogger.GetCategoryName(), ELogVerbosity::Log, __FILE__, 500 + i, 1.0f, FString::Printf(TEXT("FrameCapReset_%d"), i));
			FlushLogs();
		}

		// Log 11th message (should be dropped by cap)
		FPhysAnimLogger::LogRateLimited(LogTestLogger.GetCategoryName(), ELogVerbosity::Log, __FILE__, 600, 1.0f, TEXT("FrameCapReset_Dropped"));
		FlushLogs();

		// Reset the logger (should reset the frame cap)
		FPhysAnimLogger::Reset();

		// Log 12th message (should succeed because the frame cap was reset)
		FPhysAnimLogger::LogRateLimited(LogTestLogger.GetCategoryName(), ELogVerbosity::Log, __FILE__, 601, 1.0f, TEXT("FrameCapReset_SucceededAfterReset"));
		FlushLogs();

		// Verify captured entries
		TArray<FString> CapturedMsgs;
		for (const auto& Entry : Capture.Device.Entries)
		{
			if (Entry.Category == LogTestLogger.GetCategoryName() && Entry.Message.Contains(TEXT("FrameCapReset_")))
			{
				CapturedMsgs.Add(Entry.Message);
			}
		}

		TestEqual(TEXT("Total FrameCapReset messages should be 11"), CapturedMsgs.Num(), 11);
		TestFalse(TEXT("Dropped message should not be logged"), CapturedMsgs.Contains(TEXT("FrameCapReset_Dropped")));
		TestTrue(TEXT("Succeeded message after reset should be logged"), CapturedMsgs.Contains(TEXT("FrameCapReset_SucceededAfterReset")));
	}

	FPhysAnimLogger::Reset();
	return true;
}

