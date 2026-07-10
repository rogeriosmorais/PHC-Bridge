#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "PhysAnimProductGateFacts.h"
#include "PhysAnimProofArtifactEmitter.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimStandingWindowRequiresContinuousActiveStateTest,
	"PhysAnim.ProductGate.StandingWindowRequiresContinuousActiveState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimStandingWindowRequiresContinuousActiveStateTest::RunTest(const FString& Parameters)
{
	FPhysAnimStandingWindowAccumulator Window;

	Window.Advance(false, 100.0);
	TestEqual(TEXT("Startup time is not standing time"), Window.ContinuousSeconds, 0.0);

	Window.Advance(true, 2.99);
	TestTrue(TEXT("2.99 seconds of active standing is below the gate"), Window.ContinuousSeconds < 3.0);

	Window.Advance(false, 0.01);
	TestEqual(TEXT("Leaving standing resets the consecutive window"), Window.ContinuousSeconds, 0.0);
	TestEqual(TEXT("Leaving a started window is counted"), Window.ExitCount, 1);

	Window.Advance(true, 3.0);
	TestFalse(
		TEXT("One oversized sample cannot establish a valid standing cadence"),
		Window.HasValidCadence(2, 0.25));

	Window.Advance(false, 0.01);
	for (int32 SampleIndex = 0; SampleIndex < 180; ++SampleIndex)
	{
		Window.Advance(true, 1.0 / 60.0);
	}
	TestTrue(TEXT("A sampled three-second standing window is measured"), Window.ContinuousSeconds >= 3.0 - KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Standing sample count is factual"), Window.SampleCount, 180);
	TestTrue(TEXT("Regular samples satisfy caller-provided cadence bounds"), Window.HasValidCadence(180, 0.02));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimBodyContinuityRequiresEveryFrameTest,
	"PhysAnim.ProductGate.BodyContinuityRequiresEveryFrame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimBodyContinuityRequiresEveryFrameTest::RunTest(const FString& Parameters)
{
	FPhysAnimBodyContinuityAccumulator Continuity;
	Continuity.RecordSample(10, 0x3f, 0x3f, 0x0f, 0x0f);
	Continuity.RecordSample(9, 0x3f, 0x3e, 0x0f, 0x0d);

	TestEqual(TEXT("Minimum simultaneous body count is retained"), Continuity.MinSimulatingBodyCount, 9);
	TestEqual(TEXT("Critical validity must hold in every frame"), Continuity.CriticalBodyValidAllFramesMask, 0x3f);
	TestEqual(TEXT("A missing critical body remains missing"), Continuity.CriticalBodySimulatingAllFramesMask, 0x3e);
	TestEqual(TEXT("Support validity must hold in every frame"), Continuity.SupportBodyValidAllFramesMask, 0x0f);
	TestEqual(TEXT("A missing support body remains missing"), Continuity.SupportBodySimulatingAllFramesMask, 0x0d);
	TestEqual(TEXT("Both samples are counted"), Continuity.SampleCount, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimProductGateArtifactContainsFactsOnlyTest,
	"PhysAnim.ProductGate.ArtifactContainsFactsOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimProductGateArtifactContainsFactsOnlyTest::RunTest(const FString& Parameters)
{
	FPhysAnimProofArtifactEmitInput Input;
	Input.AttemptUuid = TEXT("product-gate-facts-only");
	Input.AttemptNonce = TEXT("nonce-123");
	Input.SourceCommit = TEXT("0123456789abcdef0123456789abcdef01234567");
	Input.bSourceTreeDirty = false;
	Input.FinalRuntimeOutcome = TEXT("BalanceActive_Standing");
	Input.StandingWindowSampleCount = 180;
	Input.StandingWindowMaxDeltaSec = 1.0 / 60.0;

	FPhysAnimRunArtifactSnapshot& Artifact = Input.PipelineResult.StateApplyResult.State.TerminalArtifact;
	Artifact.AttemptUuid = Input.AttemptUuid;
	Artifact.BalanceActiveStandingContinuousSec = 3.1;
	Artifact.BalanceActiveStandingExitCount = 0;
	Artifact.RuntimeMinSimulatingBodyCount = 10;
	Artifact.CriticalBodyValidAllFramesMask = 0x3f;
	Artifact.CriticalBodySimulatingAllFramesMask = 0x3f;
	Artifact.SupportBodyValidAllFramesMask = 0x0f;
	Artifact.SupportBodySimulatingAllFramesMask = 0x0f;

	const FString OutputPath = PhysAnimProofArtifactEmitter::BuildTerminalArtifactJsonPath(Input.AttemptUuid);
	IFileManager::Get().Delete(*OutputPath);
	TestTrue(TEXT("Raw product-gate artifact writes"), PhysAnimProofArtifactEmitter::WriteTerminalArtifactJson(OutputPath, Input));

	FString JsonText;
	TestTrue(TEXT("Raw product-gate artifact is readable"), FFileHelper::LoadFileToString(JsonText, *OutputPath));
	TSharedPtr<FJsonObject> Json;
	TestTrue(TEXT("Raw product-gate artifact parses"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(JsonText), Json));
	if (Json.IsValid())
	{
		TestEqual(TEXT("Schema is versioned"), Json->GetStringField(TEXT("schema_version")), FString(TEXT("physanim-runtime-facts/v1")));
		TestEqual(TEXT("Nonce is factual provenance"), Json->GetStringField(TEXT("attempt_nonce")), Input.AttemptNonce);
		TestEqual(TEXT("Source commit is factual provenance"), Json->GetStringField(TEXT("source_commit")), Input.SourceCommit);
		TestTrue(TEXT("Unknown setup override count fails closed as null"), Json->HasTypedField<EJson::Null>(TEXT("setup_override_count")));
		TestEqual(TEXT("Standing time is the active-state window"), Json->GetNumberField(TEXT("balance_active_standing_continuous_sec")), 3.1);
		TestEqual(TEXT("Standing cadence sample count is serialized"), Json->GetNumberField(TEXT("standing_window_sample_count")), 180.0);
		TestTrue(TEXT("Standing cadence max delta is serialized"), FMath::IsNearlyEqual(Json->GetNumberField(TEXT("standing_window_max_delta_sec")), 1.0 / 60.0));
		TestEqual(TEXT("Minimum simultaneous bodies are serialized"), Json->GetNumberField(TEXT("runtime_min_simulating_body_count")), 10.0);
		TestFalse(TEXT("Runtime artifact has no strict verdict"), Json->HasField(TEXT("strict_verdict")));
		TestFalse(TEXT("Runtime artifact has no product success field"), Json->HasField(TEXT("product_success")));
		TestFalse(TEXT("Runtime artifact has no continuity pass boolean"), Json->HasField(TEXT("physical_continuity_validator_passed")));
		TestFalse(TEXT("Runtime artifact has no policy-output pass boolean"), Json->HasField(TEXT("policy_output_active")));
	}

	IFileManager::Get().Delete(*OutputPath);
	return true;
}

#endif
