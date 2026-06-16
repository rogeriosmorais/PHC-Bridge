#include "PhysAnimLogger.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY(LogPhysAnim);

TMap<uint32, FPhysAnimLogger::FLogRecord> FPhysAnimLogger::LogHistory;
FCriticalSection FPhysAnimLogger::Mutex;
uint64 FPhysAnimLogger::LastFrameLogged = 0;
int32 FPhysAnimLogger::LogsThisFrame = 0;
FString FPhysAnimLogger::CurrentAttemptUuid;

void FPhysAnimLogger::LogRateLimited(const FName& CategoryName, ELogVerbosity::Type Verbosity, const char* File, int32 Line, float TimeLimit, const FString& Message)
{
	FScopeLock Lock(&Mutex);

	// Per-frame global limit
	uint64 CurrentFrame = GFrameCounter;
	if (CurrentFrame != LastFrameLogged)
	{
		LastFrameLogged = CurrentFrame;
		LogsThisFrame = 0;
	}

	if (LogsThisFrame >= MAX_LOGS_PER_FRAME)
	{
		return;
	}

	// Generate a unique key for the log statement based on file and line
	uint32 Key = HashCombine(GetTypeHash(FString(File)), GetTypeHash(Line));

	double CurrentTime = FPlatformTime::Seconds();

	FLogRecord& Record = LogHistory.FindOrAdd(Key);

	if (CurrentTime - Record.LastLogTime >= TimeLimit)
	{
		FString FinalMessage;
		if (Record.SuppressedCount > 0)
		{
			// Emit a summary of suppressed logs before the new one
			FinalMessage = FString::Printf(TEXT("[Suppressed %d times] %s"), Record.SuppressedCount, *Message);
		}
		else
		{
			FinalMessage = Message;
		}

		FMsg::Logf(File, Line, CategoryName, Verbosity, TEXT("%s"), *FinalMessage);

		// If an attempt UUID is active, also write to the attempt log file
		if (!CurrentAttemptUuid.IsEmpty())
		{
			const FString LogDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT("test-results"), TEXT("logs"));
			const FString LogFilePath = FPaths::Combine(LogDirectory, FString::Printf(TEXT("%s.log"), *CurrentAttemptUuid));
			const FString Timestamp = FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S.%f"));
			const FString LogLine = FString::Printf(TEXT("[%s][%s] %s\n"), *Timestamp, *CategoryName.ToString(), *FinalMessage);
			FFileHelper::SaveStringToFile(LogLine, *LogFilePath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
		}

		Record.LastLogTime = CurrentTime;
		Record.SuppressedCount = 0;
		LogsThisFrame++;
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

void FPhysAnimLogger::SetCurrentAttemptUuid(const FString& InAttemptUuid)
{
	FScopeLock Lock(&Mutex);
	CurrentAttemptUuid = InAttemptUuid;
}
