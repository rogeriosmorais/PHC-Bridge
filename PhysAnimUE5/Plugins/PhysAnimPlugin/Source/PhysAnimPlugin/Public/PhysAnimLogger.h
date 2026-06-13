#pragma once

#include "CoreMinimal.h"

// Custom log category for the PhysAnim logger
DECLARE_LOG_CATEGORY_EXTERN(LogPhysAnim, Log, All);

/**
 * FPhysAnimLogger
 * A rate-limited logging wrapper designed to prevent log spam in high-frequency loops (like Tick).
 * It uses the file name and line number to uniquely identify a log statement and restricts
 * its output frequency based on a specified time limit.
 */
class PHYSANIMPLUGIN_API FPhysAnimLogger
{
public:
	/**
	 * Rate-limited logging function.
	 * @param CategoryName   The log category (e.g., LogTemp, LogPhysAnim)
	 * @param Verbosity      The verbosity level (e.g., Warning, Error)
	 * @param File           The source file name (__FILE__)
	 * @param Line           The source line number (__LINE__)
	 * @param TimeLimit      Minimum time in seconds between identical logs
	 * @param Message        The formatted message to log
	 */
	static void LogRateLimited(const FName& CategoryName, ELogVerbosity::Type Verbosity, const char* File, int32 Line, float TimeLimit, const FString& Message);

	/**
	 * Resets all rate-limiting state (useful for tests or level restarts).
	 */
	static void Reset();

private:
	struct FLogRecord
	{
		double LastLogTime = 0.0;
		int32 SuppressedCount = 0;
	};

	// Hash of (File, Line) -> Log Record
	static TMap<uint32, FLogRecord> LogHistory;
	
	// Critical section for thread-safety (logs can come from different threads)
	static FCriticalSection Mutex;
};

// Macro to easily use the rate-limited logger.
// Usage: PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("Something happened: %d"), SomeValue);
#define PHYSANIM_LOG_RATE_LIMITED(CategoryName, Verbosity, TimeLimit, Format, ...) \
	{ \
		FString __LogMsg = FString::Printf(Format, ##__VA_ARGS__); \
		FPhysAnimLogger::LogRateLimited(CategoryName.GetCategoryName(), ELogVerbosity::Verbosity, __FILE__, __LINE__, TimeLimit, __LogMsg); \
	}
