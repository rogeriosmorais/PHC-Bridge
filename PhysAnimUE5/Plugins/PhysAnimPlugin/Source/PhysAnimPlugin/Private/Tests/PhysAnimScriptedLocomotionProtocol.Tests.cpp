#include "PhysAnimScriptedLocomotionProtocol.h"

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FString ResolveProductGatePath(const TCHAR* FileName)
	{
		return FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPaths::ProjectDir(), TEXT("../product-gates"), FileName));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimScriptedLocomotionProtocolContractTest,
	"PhysAnim.Locomotion.Protocol.AuthoritativeLoading",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimScriptedLocomotionProtocolContractTest::RunTest(const FString& Parameters)
{
	using namespace PhysAnimScriptedLocomotion;

	FProtocol V1;
	FString Error;
	TestTrue(
		TEXT("Locked v1 protocol loads"),
		LoadProtocolFromFile(ResolveProductGatePath(TEXT("scripted-locomotion.v1.json")), V1, Error));
	TestTrue(TEXT("v1 load error is empty"), Error.IsEmpty());
	TestEqual(TEXT("v1 version remains immutable"), V1.Version, 1);
	TestEqual(TEXT("v1 nominal speed remains 60 cm/s"), V1.NominalSpeedCmPerSecond, 60.0);
	TestEqual(TEXT("v1 acceleration end remains 2.0"), V1.Acceleration.EndSeconds, 2.0);
	TestEqual(TEXT("v1 cruise end remains 4.0"), V1.Cruise.EndSeconds, 4.0);
	TestEqual(TEXT("v1 turn end remains 5.0"), V1.MovingTurn.EndSeconds, 5.0);
	TestEqual(TEXT("v1 deceleration end remains 6.0"), V1.Deceleration.EndSeconds, 6.0);
	TestEqual(
		TEXT("v1 bytes retain their locked SHA-256"),
		V1.Sha256,
		FString(TEXT("FCFA625F759BC268A6A9B624A28E4AC2BF518A3FA568586BD02A25B7E23F2F87")));

	FProtocol V2;
	Error.Reset();
	TestTrue(
		TEXT("Locked v2 protocol loads"),
		LoadProtocolFromFile(ResolveProductGatePath(TEXT("scripted-locomotion.v2.json")), V2, Error));
	TestTrue(TEXT("v2 load error is empty"), Error.IsEmpty());
	TestEqual(TEXT("v2 protocol id"), V2.ProtocolId, FString(TEXT("scripted-causal-locomotion")));
	TestEqual(TEXT("v2 version"), V2.Version, 2);
	TestEqual(TEXT("v2 status"), V2.Status, FString(TEXT("LOCKED")));
	TestEqual(TEXT("v2 nominal speed is authoritative"), V2.NominalSpeedCmPerSecond, 160.0);
	TestEqual(TEXT("v2 fixed timestep is authoritative"), V2.FixedDeltaTimeSeconds, 1.0 / 60.0, 1.0e-12);
	TestEqual(TEXT("v2 acceleration end"), V2.Acceleration.EndSeconds, 1.6);
	TestEqual(TEXT("v2 cruise end"), V2.Cruise.EndSeconds, 2.1);
	TestEqual(TEXT("v2 turn end"), V2.MovingTurn.EndSeconds, 2.4);
	TestEqual(TEXT("v2 deceleration end"), V2.Deceleration.EndSeconds, 3.0);
	TestEqual(TEXT("v2 capture window"), V2.CaptureWindowSeconds, 10.0);
	TestEqual(TEXT("v2 minimum shell path threshold"), V2.Acceptance.MinimumShellPathLengthCm, 180.0);
	TestEqual(TEXT("v2 final state threshold"), V2.Acceptance.RequiredFinalRuntimeState, FString(TEXT("BalanceActive_Standing")));
	TestEqual(TEXT("v2 physics minimum sample threshold"), V2.PhysicsMinimumSamples, 590);
	TestEqual(TEXT("v2 policy minimum sample threshold"), V2.PolicyMinimumSamples, 270);
	TestEqual(TEXT("v2 protocol hash has SHA-256 width"), V2.Sha256.Len(), 64);
	TestEqual(
		TEXT("v2 bytes retain their locked SHA-256"),
		V2.Sha256,
		FString(TEXT("DA9830EC14EC659D85A4AF86CDD1CC44A3C7E57A544DD63A4A6C36C56705D381")));

	auto TestPhase = [this, &V2](double TimeSeconds, const TCHAR* ExpectedPhase)
	{
		TestEqual(
			FString::Printf(TEXT("Protocol-derived phase at %.3f seconds"), TimeSeconds),
			V2.ResolveStep(TimeSeconds).Phase,
			FString(ExpectedPhase));
	};
	TestPhase(0.999, TEXT("StandingHold"));
	TestPhase(1.000, TEXT("Acceleration"));
	TestPhase(1.599, TEXT("Acceleration"));
	TestPhase(1.600, TEXT("Cruise"));
	TestPhase(2.099, TEXT("Cruise"));
	TestPhase(2.100, TEXT("MovingTurn"));
	TestPhase(2.399, TEXT("MovingTurn"));
	TestPhase(2.400, TEXT("Deceleration"));
	TestPhase(2.999, TEXT("Deceleration"));
	TestPhase(3.000, TEXT("Settle"));

	double IntegratedYawDegrees = 0.0;
	double PredictedPathCm = 0.0;
	const int32 StepCount = FMath::RoundToInt(V2.CaptureWindowSeconds / V2.FixedDeltaTimeSeconds);
	for (int32 StepIndex = 0; StepIndex < StepCount; ++StepIndex)
	{
		const double TimeSeconds = static_cast<double>(StepIndex) * V2.FixedDeltaTimeSeconds;
		const FStep Step = V2.ResolveStep(TimeSeconds);
		IntegratedYawDegrees += Step.YawDeltaDegrees;
		PredictedPathCm +=
			static_cast<double>(Step.IntentMagnitude) *
			V2.NominalSpeedCmPerSecond *
			V2.FixedDeltaTimeSeconds;
	}
	TestEqual(TEXT("v2 turn integrates to the protocol yaw"), IntegratedYawDegrees, 30.0, 1.0e-4);
	TestTrue(
		TEXT("v2 predicted path satisfies its own product thresholds"),
		PredictedPathCm >= V2.Acceptance.MinimumShellPathLengthCm && PredictedPathCm <= 260.0);

	FString V2Text;
	TestTrue(
		TEXT("Read v2 bytes for negative validation"),
		FFileHelper::LoadFileToString(V2Text, *ResolveProductGatePath(TEXT("scripted-locomotion.v2.json"))));
	V2Text.ReplaceInline(TEXT("\"status\": \"LOCKED\""), TEXT("\"status\": \"DRAFT\""));
	FProtocol DraftProtocol;
	Error.Reset();
	TestFalse(
		TEXT("A non-locked protocol is rejected"),
		LoadProtocolFromJsonText(V2Text, TEXT("memory://draft-scripted-locomotion"), DraftProtocol, Error));
	TestTrue(TEXT("Non-locked rejection names the status"), Error.Contains(TEXT("LOCKED")));

	return true;
}

#endif
