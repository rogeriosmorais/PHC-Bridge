#pragma once

#include "CoreMinimal.h"
#include "PhysAnimBridge.h"

enum class EPhysAnimBridgeTraceMode : uint8
{
	Off = 0,
	MetadataAndEvents = 1,
	Full = 2,
};

struct FPhysAnimBridgeTraceSessionMetadata
{
	int32 TraceVersion = 1;
	FString SessionId;
	FString TimestampUtc;
	FString MapName;
	FString ActorName;
	FString RuntimeState;
	FString NneRuntimeName;
	FString ModelAssetPath;
	FString PoseSearchDatabasePath;
	int32 TraceMode = 0;
	int32 SampleEveryNthFrame = 1;
	float FlushIntervalSeconds = 0.5f;
	bool bForceZeroActions = false;
	float ActionScale = 0.0f;
	float ActionClampAbs = 0.0f;
	float ActionSmoothingAlpha = 0.0f;
	float StartupRampSeconds = 0.0f;
	float PolicyControlRateHz = 0.0f;
	float MaxAngularStepDegreesPerSecond = 0.0f;
	float AngularStrengthMultiplier = 0.0f;
	float AngularDampingRatioMultiplier = 0.0f;
	float AngularExtraDampingMultiplier = 0.0f;
	bool bEnableInstabilityFailStop = false;
	float MaxRootHeightDeltaCm = 0.0f;
	float MaxRootLinearSpeedCmPerSecond = 0.0f;
	float MaxRootAngularSpeedDegPerSecond = 0.0f;
	float InstabilityGracePeriodSeconds = 0.0f;
	FString StabilizationSummary;
};

struct FPhysAnimBridgeTraceFrame
{
	FString SessionId;
	int32 TraceVersion = 1;
	int64 FrameIndex = 0;
	double WorldTimeSeconds = 0.0;
	float DeltaTimeSeconds = 0.0f;
	bool bSampledPolicyStep = false;

	FString RuntimeState;
	FString NneRuntimeName;
	bool bPoseSearchValid = false;
	bool bRunSyncSucceeded = false;
	bool bUpdateControlsSucceeded = false;
	bool bPolicyInfluenceActive = false;
	bool bFirstPolicyEnabledFrame = false;
	int32 NumPolicyTargetsWritten = 0;

	float ResolveContextMs = 0.0f;
	float PoseSearchQueryMs = 0.0f;
	float FuturePoseSampleMs = 0.0f;
	float BodySampleMs = 0.0f;
	float ObservationPackMs = 0.0f;
	float InferenceMs = 0.0f;
	float ActionConditionMs = 0.0f;
	float ControlTargetMs = 0.0f;
	float UpdateControlsMs = 0.0f;
	float InstabilityCheckMs = 0.0f;
	float BridgeTickTotalMs = 0.0f;

	FPhysAnimActionDiagnostics ActionDiagnostics;
	FPhysAnimControlTargetDiagnostics ControlTargetDiagnostics;
	FPhysAnimRuntimeInstabilityDiagnostics InstabilityDiagnostics;
};

struct FPhysAnimBridgeTraceEvent
{
	FString SessionId;
	double EventTimeSeconds = 0.0;
	FString EventType;
	FString RuntimeState;
	FString Message;
	FString Error;
	FString PreviousRuntimeState;
	FString NewRuntimeState;
	FString NneRuntimeName;
	FString ModelAssetPath;
	FString MapName;
	FString ActorName;
};

class FPhysAnimBridgeTraceWriter
{
public:
	explicit FPhysAnimBridgeTraceWriter(EPhysAnimBridgeTraceMode InMode = EPhysAnimBridgeTraceMode::Off);

	bool StartSession(const FString& TraceRootPath, const FPhysAnimBridgeTraceSessionMetadata& Metadata, FString& OutError);
	void AppendEvent(const FPhysAnimBridgeTraceEvent& Event);
	void AppendFrame(const FPhysAnimBridgeTraceFrame& Frame);
	void Flush(bool bForce);
	void Shutdown();

	bool IsEnabled() const;
	bool CanWriteFrames() const;
	bool HasWriteFailed() const { return bWriteFailed; }
	const FString& GetSessionId() const { return SessionId; }
	const FString& GetSessionFolderPath() const { return SessionFolderPath; }

	void SetFailWritesAfterCountForTesting(int32 InFailWritesAfterCount);

	static FString BuildSessionMetadataJson(const FPhysAnimBridgeTraceSessionMetadata& Metadata);
	static FString BuildEventJsonLine(const FPhysAnimBridgeTraceEvent& Event);
	static FString BuildFrameCsvHeader();
	static FString BuildFrameCsvRow(const FPhysAnimBridgeTraceFrame& Frame);

private:
	bool WriteBytes(IFileHandle& Handle, const FString& Text);
	bool WriteWholeFile(const FString& FilePath, const FString& Text);
	void HandleWriteFailure(const FString& Context);

	EPhysAnimBridgeTraceMode Mode = EPhysAnimBridgeTraceMode::Off;
	bool bSessionStarted = false;
	bool bWriteFailed = false;
	FString SessionId;
	FString SessionFolderPath;
	FString EventsBuffer;
	FString FramesBuffer;
	TUniquePtr<IFileHandle> EventsFileHandle;
	TUniquePtr<IFileHandle> FramesFileHandle;
	int32 WriteCount = 0;
	int32 FailWritesAfterCount = INDEX_NONE;
};
