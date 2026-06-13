#include "PhysAnimLogger.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"

DEFINE_LOG_CATEGORY(LogPhysAnim);

TMap<uint32, FPhysAnimLogger::FLogRecord> FPhysAnimLogger::LogHistory;
FCriticalSection FPhysAnimLogger::Mutex;

void FPhysAnimLogger::LogRateLimited(const FName& CategoryName, ELogVerbosity::Type Verbosity, const char* File, int32 Line, float TimeLimit, const FString& Message)
{
	FScopeLock Lock(&Mutex);

	// Generate a unique key for the log statement based on file and line
	uint32 Key = HashCombine(GetTypeHash(FString(File)), GetTypeHash(Line));

	double CurrentTime = FPlatformTime::Seconds();

	FLogRecord& Record = LogHistory.FindOrAdd(Key);

	if (CurrentTime - Record.LastLogTime >= TimeLimit)
	{
		if (Record.SuppressedCount > 0)
		{
			// Emit a summary of suppressed logs before the new one
			FString SuppressedMsg = FString::Printf(TEXT("[Suppressed %d times] %s"), Record.SuppressedCount, *Message);
			FMsg::Logf(File, Line, CategoryName, Verbosity, TEXT("%s"), *SuppressedMsg);
		}
		else
		{
			FMsg::Logf(File, Line, CategoryName, Verbosity, TEXT("%s"), *Message);
		}

		Record.LastLogTime = CurrentTime;
		Record.SuppressedCount = 0;
	}
	else
	{
		Record.SuppressedCount++;
	}
}

void FPhysAnimLogger::Reset()
{
	FScopeLock Lock(&Mutex);
	LogHistory.Empty();
}
