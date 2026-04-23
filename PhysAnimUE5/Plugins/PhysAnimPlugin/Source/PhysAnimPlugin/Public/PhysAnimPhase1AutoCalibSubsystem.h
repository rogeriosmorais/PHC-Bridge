#pragma once

#include "PhysAnimComponent.h"
#include "Subsystems/WorldSubsystem.h"

#include "PhysAnimPhase1AutoCalibSubsystem.generated.h"

UCLASS()
class PHYSANIMPLUGIN_API UPhysAnimPhase1AutoCalibSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;

	bool StartPhase1AutoCalib(const FPhase1AutoCalibRequest& Request, FString& OutError);
	void StopPhase1AutoCalib(const FString& Reason = TEXT("manual_stop"));
	bool IsPhase1AutoCalibActive() const { return bRunActive; }
	const FPhase1AutoCalibReport& GetLatestReport() const { return LatestReport; }
	const FString& GetLastError() const { return LastError; }

	static void BuildStageACandidates(const FPhase1AutoCalibRequest& Request, TArray<FPhase1AutoCalibParams>& OutCandidates);
	static void BuildStageBRefinementCandidates(const TArray<FPhase1AutoCalibTrialResult>& StageAResults, const FPhase1AutoCalibRequest& Request, TArray<FPhase1AutoCalibParams>& OutCandidates);
	static void BuildStageCReproCandidates(const TArray<FPhase1AutoCalibTrialResult>& StageBResults, const FPhase1AutoCalibRequest& Request, TArray<FPhase1AutoCalibParams>& OutCandidates);
	static bool AreTrialResultsReproducible(const TArray<FPhase1AutoCalibTrialResult>& Trials, float Epsilon = 1.0e-3f);
	static bool AreDeterminismFingerprintsNear(
		const FPhase1AutoCalibDeterminismFingerprint& A,
		const FPhase1AutoCalibDeterminismFingerprint& B,
		float Tolerance = 1.0e-3f);
	static bool IsActiveTrialTimeoutReached(bool bTrialStarted, double TrialStartTimeSeconds, double CurrentTimeSeconds, double TimeoutSeconds);
	static bool ShouldAccumulateActiveTrialMetrics(bool bTrialStarted);
	static bool ShouldFinalizeActiveTrial(
		EBalanceReadyTransitionPhase Phase,
		double BalanceActiveStandingHoldSeconds,
		bool bTransitionFailed,
		bool bSafeDenied);
	static void FinalizeReportData(FPhase1AutoCalibReport& InOutReport, TArray<FPhase1AutoCalibTrialResult>* StageCTrials = nullptr);
#if WITH_DEV_AUTOMATION_TESTS
	static EBalanceReadyTransitionPhase TestOnlyMergeObservedTransitionPhase(
		EBalanceReadyTransitionPhase PeakPhase,
		EBalanceReadyTransitionPhase ObservedPhase);
	static bool TestOnlyDidTrialReachRootOn(
		EBalanceReadyTransitionPhase CurrentPhase,
		EBalanceReadyTransitionPhase PeakPhase);
	static void TestOnlyResetActiveTrialTrackingState(
		double& InOutActiveTrialStartTimeSeconds,
		double& InOutActiveTrialFirstRootOnTimeSeconds,
		double& InOutActiveTrialFirstNoCouplingProofTimeSeconds,
		double& InOutActiveTrialFirstBalanceActiveStandingTimeSeconds,
		double& InOutActiveTrialStandingHoldStartTimeSeconds,
		double& InOutActiveTrialMaxBalanceActiveStandingHoldSeconds,
		FPhase1AutoCalibLiveMetrics& InOutActiveTrialPeakMetrics);
	static bool TestOnlyWriteArtifacts(FPhase1AutoCalibReport& InOutReport);
#endif

private:
	enum class EAutoCalibStage : uint8
	{
		Inactive,
		AwaitingReadiness,
		StageA,
		StageB,
		StageC,
		Completed,
		Failed
	};

	struct FPendingTrial
	{
		FPhase1AutoCalibParams Params;
		FString StageName;
		int32 RepetitionIndex = 0;
	};

	bool ResolveTargetComponent(const FString& OwnerFilter, UPhysAnimComponent*& OutComponent, FString& OutError) const;
	bool RunDeterminismPreflight(UPhysAnimComponent& Component, const FPhase1AutoCalibBaselineSnapshot& Baseline, FString& OutError) const;
	bool BeginNextTrial();
	void TickAwaitingReadiness();
	void TickActiveTrial();
	void FinalizeActiveTrial(bool bTimedOut);
	void AdvanceStageOrFinish();
	void ResetActiveTrialTrackingState();
	void UpdatePeakMetrics(const FPhase1AutoCalibLiveMetrics& Metrics);
	FPhase1AutoCalibTrialResult BuildTrialResult(bool bTimedOut) const;
	void FinalizeReport();
	void WriteArtifacts();
	FString BuildOutputDirectory() const;

	bool bRunActive = false;
	bool bTrialActive = false;
	bool bActiveTrialStarted = false;
	EAutoCalibStage CurrentStage = EAutoCalibStage::Inactive;
	FPhase1AutoCalibRequest ActiveRequest;
	TWeakObjectPtr<UPhysAnimComponent> TargetComponent;
	FPhase1AutoCalibBaselineSnapshot BaselineSnapshot;
	TArray<FPendingTrial> PendingTrials;
	TArray<FPhase1AutoCalibTrialResult> StageAResults;
	TArray<FPhase1AutoCalibTrialResult> StageBResults;
	TArray<FPhase1AutoCalibTrialResult> StageCResults;
	FPendingTrial ActiveTrial;
	double ActiveTrialStartTimeSeconds = -1.0;
	double ActiveTrialFirstRootOnTimeSeconds = -1.0;
	double ActiveTrialFirstNoCouplingProofTimeSeconds = -1.0;
	double ActiveTrialFirstBalanceActiveStandingTimeSeconds = -1.0;
	double ActiveTrialStandingHoldStartTimeSeconds = -1.0;
	double ActiveTrialMaxBalanceActiveStandingHoldSeconds = 0.0;
	double CurrentStageStartTimeSeconds = -1.0;
	double LastReadinessLogTimeSeconds = -1.0;
	FPhase1AutoCalibLiveMetrics ActiveTrialPeakMetrics;
	FString LastError;
	FPhase1AutoCalibReport LatestReport;
	FPhase1AutoCalibDeterminismFingerprint BaselineFingerprint;
	int32 NextTrialId = 0;
};
