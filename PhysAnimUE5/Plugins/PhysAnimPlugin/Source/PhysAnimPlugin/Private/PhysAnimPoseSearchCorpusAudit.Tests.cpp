#include "Animation/AnimSequence.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FLocomotionAssetExpectation
	{
		const TCHAR* Gait;
		const TCHAR* AssetName;
		FVector ExpectedDataDirection;
	};

	static const FLocomotionAssetExpectation Expectations[] =
	{
		{ TEXT("Walk"), TEXT("MF_Unarmed_Walk_Fwd"), FVector(0.0f, 1.0f, 0.0f) },
		{ TEXT("Walk"), TEXT("MF_Unarmed_Walk_Fwd_Left"), FVector(1.0f, 1.0f, 0.0f).GetSafeNormal() },
		{ TEXT("Walk"), TEXT("MF_Unarmed_Walk_Left"), FVector(1.0f, 0.0f, 0.0f) },
		{ TEXT("Walk"), TEXT("MF_Unarmed_Walk_Bwd_Left"), FVector(1.0f, -1.0f, 0.0f).GetSafeNormal() },
		{ TEXT("Walk"), TEXT("MF_Unarmed_Walk_Bwd"), FVector(0.0f, -1.0f, 0.0f) },
		{ TEXT("Walk"), TEXT("MF_Unarmed_Walk_Bwd_Right"), FVector(-1.0f, -1.0f, 0.0f).GetSafeNormal() },
		{ TEXT("Walk"), TEXT("MF_Unarmed_Walk_Right"), FVector(-1.0f, 0.0f, 0.0f) },
		{ TEXT("Walk"), TEXT("MF_Unarmed_Walk_Fwd_Right"), FVector(-1.0f, 1.0f, 0.0f).GetSafeNormal() },
		{ TEXT("Jog"), TEXT("MF_Unarmed_Jog_Fwd"), FVector(0.0f, 1.0f, 0.0f) },
		{ TEXT("Jog"), TEXT("MF_Unarmed_Jog_Fwd_Left"), FVector(1.0f, 1.0f, 0.0f).GetSafeNormal() },
		{ TEXT("Jog"), TEXT("MF_Unarmed_Jog_Left"), FVector(1.0f, 0.0f, 0.0f) },
		{ TEXT("Jog"), TEXT("MF_Unarmed_Jog_Bwd_Left"), FVector(1.0f, -1.0f, 0.0f).GetSafeNormal() },
		{ TEXT("Jog"), TEXT("MF_Unarmed_Jog_Bwd"), FVector(0.0f, -1.0f, 0.0f) },
		{ TEXT("Jog"), TEXT("MF_Unarmed_Jog_Bwd_Right"), FVector(-1.0f, -1.0f, 0.0f).GetSafeNormal() },
		{ TEXT("Jog"), TEXT("MF_Unarmed_Jog_Right"), FVector(-1.0f, 0.0f, 0.0f) },
		{ TEXT("Jog"), TEXT("MF_Unarmed_Jog_Fwd_Right"), FVector(-1.0f, 1.0f, 0.0f).GetSafeNormal() },
	};

	TArray<TSharedPtr<FJsonValue>> VectorToJson(const FVector& Value)
	{
		return {
			MakeShared<FJsonValueNumber>(Value.X),
			MakeShared<FJsonValueNumber>(Value.Y),
			MakeShared<FJsonValueNumber>(Value.Z)
		};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimPoseSearchCorpusAuditTest,
	"PhysAnim.Development.PoseSearchCorpusAudit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimPoseSearchCorpusAuditTest::RunTest(const FString& Parameters)
{
	TMap<FString, double> WalkSpeedsByDirection;
	TArray<TSharedPtr<FJsonValue>> AssetRows;
	const FAnimExtractContext ExtractionContext;

	for (const FLocomotionAssetExpectation& Expectation : Expectations)
	{
		const FString AssetPath = FString::Printf(
			TEXT("/Game/Characters/Mannequins/Anims/Unarmed/%s/%s.%s"),
			Expectation.Gait,
			Expectation.AssetName,
			Expectation.AssetName);
		UAnimSequence* const Sequence = LoadObject<UAnimSequence>(nullptr, *AssetPath);
		TestNotNull(FString::Printf(TEXT("Load %s"), Expectation.AssetName), Sequence);
		if (!Sequence)
		{
			continue;
		}

		const double DurationSeconds = Sequence->GetPlayLength();
		const double AuditWindowSeconds = FMath::Min(0.5, DurationSeconds);
		const FTransform WindowRootMotion = Sequence->ExtractRootMotionFromRange(
			0.0,
			AuditWindowSeconds,
			ExtractionContext);
		const FTransform FullRootMotion = Sequence->ExtractRootMotionFromRange(
			0.0,
			DurationSeconds,
			ExtractionContext);
		const FVector WindowDeltaCm = WindowRootMotion.GetTranslation();
		const FVector FullDeltaCm = FullRootMotion.GetTranslation();
		const double WindowSpeedCmPerSecond = AuditWindowSeconds > 0.0
			? WindowDeltaCm.Size2D() / AuditWindowSeconds
			: 0.0;
		const double DirectionDot = FVector::DotProduct(
			WindowDeltaCm.GetSafeNormal2D(),
			Expectation.ExpectedDataDirection);
		const double FullYawDeltaDegrees = FullRootMotion.Rotator().Yaw;

		TestTrue(
			FString::Printf(TEXT("%s has nonzero planar root motion"), Expectation.AssetName),
			WindowDeltaCm.SizeSquared2D() > 1.0);
		TestTrue(
			FString::Printf(TEXT("%s matches its locked authored data direction"), Expectation.AssetName),
			DirectionDot > 0.999);
		TestTrue(
			FString::Printf(TEXT("%s is a translation clip rather than an unintended turn"), Expectation.AssetName),
			FMath::Abs(FullYawDeltaDegrees) < 2.0);

		const FString DirectionKey = FString(Expectation.AssetName)
			.Replace(TEXT("MF_Unarmed_Walk_"), TEXT(""))
			.Replace(TEXT("MF_Unarmed_Jog_"), TEXT(""));
		if (FCString::Strcmp(Expectation.Gait, TEXT("Walk")) == 0)
		{
			WalkSpeedsByDirection.Add(DirectionKey, WindowSpeedCmPerSecond);
		}
		else if (const double* const WalkSpeed = WalkSpeedsByDirection.Find(DirectionKey))
		{
			TestTrue(
				FString::Printf(TEXT("%s is faster than the matching walk clip"), Expectation.AssetName),
				WindowSpeedCmPerSecond > *WalkSpeed * 1.10);
		}

		const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("gait"), Expectation.Gait);
		Row->SetStringField(TEXT("asset"), Expectation.AssetName);
		Row->SetStringField(TEXT("asset_path"), AssetPath);
		Row->SetNumberField(TEXT("duration_sec"), DurationSeconds);
		Row->SetNumberField(TEXT("audit_window_sec"), AuditWindowSeconds);
		Row->SetArrayField(TEXT("window_root_delta_cm"), VectorToJson(WindowDeltaCm));
		Row->SetArrayField(TEXT("full_root_delta_cm"), VectorToJson(FullDeltaCm));
		Row->SetArrayField(TEXT("expected_data_direction"), VectorToJson(Expectation.ExpectedDataDirection));
		Row->SetNumberField(TEXT("direction_dot"), DirectionDot);
		Row->SetNumberField(TEXT("window_speed_cm_per_sec"), WindowSpeedCmPerSecond);
		Row->SetNumberField(TEXT("full_yaw_delta_deg"), FullYawDeltaDegrees);
		Row->SetBoolField(TEXT("direction_contract_pass"), DirectionDot > 0.999);
		AssetRows.Add(MakeShared<FJsonValueObject>(Row));
	}

	const TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetStringField(TEXT("schema_version"), TEXT("physanim-pose-search-corpus-audit/v1"));
	Report->SetNumberField(TEXT("expected_asset_count"), UE_ARRAY_COUNT(Expectations));
	Report->SetNumberField(TEXT("loaded_asset_count"), AssetRows.Num());
	Report->SetStringField(TEXT("data_forward_axis"), TEXT("+Y"));
	Report->SetStringField(TEXT("standard_manny_mesh_relative_yaw"), TEXT("-90 degrees"));
	Report->SetArrayField(TEXT("assets"), AssetRows);

	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Report, Writer);
	const FString OutputPath = FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("PhysAnim"),
		TEXT("pose-search-corpus-audit.json"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutputPath), true);
	TestTrue(TEXT("Pose Search corpus audit report is written"), FFileHelper::SaveStringToFile(Json + TEXT("\n"), *OutputPath));
	TestEqual(
		TEXT("All locked locomotion assets were audited"),
		AssetRows.Num(),
		static_cast<int32>(UE_ARRAY_COUNT(Expectations)));
	AddInfo(FString::Printf(TEXT("Pose Search corpus audit: %s"), *OutputPath));
	return true;
}

#endif
