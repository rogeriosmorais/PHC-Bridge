#include "PhysAnimScriptedLocomotionProtocol.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace PhysAnimScriptedLocomotion
{
	namespace
	{
		bool Fail(FString& OutError, const FString& Message)
		{
			OutError = Message;
			return false;
		}

		bool GetRequiredObject(
			const TSharedPtr<FJsonObject>& Parent,
			const TCHAR* FieldName,
			const TSharedPtr<FJsonObject>*& OutObject,
			FString& OutError,
			const FString& Path)
		{
			if (!Parent.IsValid() ||
				!Parent->TryGetObjectField(FieldName, OutObject) ||
				!OutObject ||
				!OutObject->IsValid())
			{
				return Fail(OutError, FString::Printf(TEXT("Missing object field %s.%s"), *Path, FieldName));
			}
			return true;
		}

		bool GetRequiredString(
			const TSharedPtr<FJsonObject>& Object,
			const TCHAR* FieldName,
			FString& OutValue,
			FString& OutError,
			const FString& Path)
		{
			if (!Object.IsValid() || !Object->TryGetStringField(FieldName, OutValue))
			{
				return Fail(OutError, FString::Printf(TEXT("Missing string field %s.%s"), *Path, FieldName));
			}
			return true;
		}

		bool GetRequiredNumber(
			const TSharedPtr<FJsonObject>& Object,
			const TCHAR* FieldName,
			double& OutValue,
			FString& OutError,
			const FString& Path)
		{
			if (!Object.IsValid() || !Object->TryGetNumberField(FieldName, OutValue))
			{
				return Fail(OutError, FString::Printf(TEXT("Missing number field %s.%s"), *Path, FieldName));
			}
			return true;
		}

		bool GetRequiredInt(
			const TSharedPtr<FJsonObject>& Object,
			const TCHAR* FieldName,
			int32& OutValue,
			FString& OutError,
			const FString& Path)
		{
			if (!Object.IsValid() || !Object->TryGetNumberField(FieldName, OutValue))
			{
				return Fail(OutError, FString::Printf(TEXT("Missing integer field %s.%s"), *Path, FieldName));
			}
			return true;
		}

		bool GetRequiredBool(
			const TSharedPtr<FJsonObject>& Object,
			const TCHAR* FieldName,
			bool& OutValue,
			FString& OutError,
			const FString& Path)
		{
			if (!Object.IsValid() || !Object->TryGetBoolField(FieldName, OutValue))
			{
				return Fail(OutError, FString::Printf(TEXT("Missing bool field %s.%s"), *Path, FieldName));
			}
			return true;
		}

		bool ParseRange(
			const TSharedPtr<FJsonObject>& Object,
			FPhaseRange& OutRange,
			FString& OutError,
			const FString& Path)
		{
			return GetRequiredNumber(Object, TEXT("start_sec"), OutRange.StartSeconds, OutError, Path) &&
				GetRequiredNumber(Object, TEXT("end_sec"), OutRange.EndSeconds, OutError, Path);
		}

		bool ParseIntentRamp(
			const TSharedPtr<FJsonObject>& Object,
			FIntentRampPhase& OutPhase,
			FString& OutError,
			const FString& Path)
		{
			double IntentStart = 0.0;
			double IntentEnd = 0.0;
			if (!ParseRange(Object, OutPhase, OutError, Path) ||
				!GetRequiredNumber(Object, TEXT("intent_start"), IntentStart, OutError, Path) ||
				!GetRequiredNumber(Object, TEXT("intent_end"), IntentEnd, OutError, Path))
			{
				return false;
			}
			OutPhase.IntentStart = IntentStart;
			OutPhase.IntentEnd = IntentEnd;
			return true;
		}

		bool ParseConstantIntent(
			const TSharedPtr<FJsonObject>& Object,
			FConstantIntentPhase& OutPhase,
			FString& OutError,
			const FString& Path)
		{
			double Intent = 0.0;
			if (!ParseRange(Object, OutPhase, OutError, Path) ||
				!GetRequiredNumber(Object, TEXT("intent"), Intent, OutError, Path))
			{
				return false;
			}
			OutPhase.Intent = Intent;
			return true;
		}

		bool ParseMovingTurn(
			const TSharedPtr<FJsonObject>& Object,
			FMovingTurnPhase& OutPhase,
			FString& OutError,
			const FString& Path)
		{
			double TotalYawDegrees = 0.0;
			if (!ParseConstantIntent(Object, OutPhase, OutError, Path) ||
				!GetRequiredNumber(Object, TEXT("total_yaw_deg"), TotalYawDegrees, OutError, Path))
			{
				return false;
			}
			OutPhase.TotalYawDegrees = TotalYawDegrees;
			return true;
		}

		bool NearlyEqual(double A, double B, double Tolerance = 1.0e-9)
		{
			return FMath::Abs(A - B) <= Tolerance;
		}

		bool InUnitRange(double Value)
		{
			return Value >= 0.0f && Value <= 1.0f;
		}

		uint32 RotateRight(uint32 Value, uint32 Bits)
		{
			return (Value >> Bits) | (Value << (32u - Bits));
		}

		FString HashBytes(const TArray<uint8>& InputBytes)
		{
			static constexpr uint32 RoundConstants[64] =
			{
				0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
				0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
				0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
				0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
				0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
				0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
				0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
				0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
				0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
				0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
				0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
				0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
				0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
				0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
				0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
				0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
			};

			TArray<uint8> Bytes = InputBytes;
			const uint64 BitLength = static_cast<uint64>(Bytes.Num()) * 8ull;
			Bytes.Add(0x80u);
			while ((Bytes.Num() % 64) != 56)
			{
				Bytes.Add(0u);
			}
			for (int32 Shift = 56; Shift >= 0; Shift -= 8)
			{
				Bytes.Add(static_cast<uint8>((BitLength >> Shift) & 0xffull));
			}

			uint32 Hash[8] =
			{
				0x6a09e667u,
				0xbb67ae85u,
				0x3c6ef372u,
				0xa54ff53au,
				0x510e527fu,
				0x9b05688cu,
				0x1f83d9abu,
				0x5be0cd19u,
			};

			for (int32 ChunkOffset = 0; ChunkOffset < Bytes.Num(); ChunkOffset += 64)
			{
				uint32 Words[64] = {};
				for (int32 Index = 0; Index < 16; ++Index)
				{
					const int32 Offset = ChunkOffset + Index * 4;
					Words[Index] =
						(static_cast<uint32>(Bytes[Offset]) << 24u) |
						(static_cast<uint32>(Bytes[Offset + 1]) << 16u) |
						(static_cast<uint32>(Bytes[Offset + 2]) << 8u) |
						static_cast<uint32>(Bytes[Offset + 3]);
				}
				for (int32 Index = 16; Index < 64; ++Index)
				{
					const uint32 S0 =
						RotateRight(Words[Index - 15], 7u) ^
						RotateRight(Words[Index - 15], 18u) ^
						(Words[Index - 15] >> 3u);
					const uint32 S1 =
						RotateRight(Words[Index - 2], 17u) ^
						RotateRight(Words[Index - 2], 19u) ^
						(Words[Index - 2] >> 10u);
					Words[Index] = Words[Index - 16] + S0 + Words[Index - 7] + S1;
				}

				uint32 A = Hash[0];
				uint32 B = Hash[1];
				uint32 C = Hash[2];
				uint32 D = Hash[3];
				uint32 E = Hash[4];
				uint32 F = Hash[5];
				uint32 G = Hash[6];
				uint32 H = Hash[7];

				for (int32 Index = 0; Index < 64; ++Index)
				{
					const uint32 Sum1 =
						RotateRight(E, 6u) ^
						RotateRight(E, 11u) ^
						RotateRight(E, 25u);
					const uint32 Choice = (E & F) ^ ((~E) & G);
					const uint32 Temp1 = H + Sum1 + Choice + RoundConstants[Index] + Words[Index];
					const uint32 Sum0 =
						RotateRight(A, 2u) ^
						RotateRight(A, 13u) ^
						RotateRight(A, 22u);
					const uint32 Majority = (A & B) ^ (A & C) ^ (B & C);
					const uint32 Temp2 = Sum0 + Majority;

					H = G;
					G = F;
					F = E;
					E = D + Temp1;
					D = C;
					C = B;
					B = A;
					A = Temp1 + Temp2;
				}

				Hash[0] += A;
				Hash[1] += B;
				Hash[2] += C;
				Hash[3] += D;
				Hash[4] += E;
				Hash[5] += F;
				Hash[6] += G;
				Hash[7] += H;
			}

			return FString::Printf(
				TEXT("%08X%08X%08X%08X%08X%08X%08X%08X"),
				Hash[0], Hash[1], Hash[2], Hash[3],
				Hash[4], Hash[5], Hash[6], Hash[7]);
		}

		FString HashUtf8Text(const FString& Text)
		{
			const FTCHARToUTF8 Utf8(*Text);
			TArray<uint8> Bytes;
			Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
			return HashBytes(Bytes);
		}
	}

	bool FProtocol::Validate(FString& OutError) const
	{
		OutError.Reset();
		if (SchemaVersion != TEXT("physanim-product-protocol/v1"))
		{
			return Fail(OutError, TEXT("Unsupported scripted-locomotion protocol schema"));
		}
		if (ProtocolId != TEXT("scripted-causal-locomotion"))
		{
			return Fail(OutError, TEXT("Unexpected scripted-locomotion protocol_id"));
		}
		if (Version <= 0)
		{
			return Fail(OutError, TEXT("Protocol version must be positive"));
		}
		if (Status != TEXT("LOCKED"))
		{
			return Fail(OutError, TEXT("Product protocol status must be LOCKED"));
		}
		if (Map.IsEmpty() || ActorClass.IsEmpty() || ModelAsset.IsEmpty() || TestFamily.IsEmpty())
		{
			return Fail(OutError, TEXT("Protocol identity fields are incomplete"));
		}
		if (bHumanInput)
		{
			return Fail(OutError, TEXT("Scripted locomotion product protocol must set human_input=false"));
		}
		if (CaptureWindowSeconds <= 0.0 || FixedDeltaTimeSeconds <= 0.0 || NominalSpeedCmPerSecond <= 0.0)
		{
			return Fail(OutError, TEXT("Script timing and nominal speed must be positive"));
		}

		const FPhaseRange* Ranges[] =
		{
			&StandingHold,
			&Acceleration,
			&Cruise,
			&MovingTurn,
			&Deceleration,
			&Settle,
		};
		for (const FPhaseRange* Range : Ranges)
		{
			if (!Range || Range->EndSeconds <= Range->StartSeconds)
			{
				return Fail(OutError, TEXT("Each scripted phase must have positive duration"));
			}
		}
		if (!NearlyEqual(StandingHold.StartSeconds, 0.0) ||
			!NearlyEqual(StandingHold.EndSeconds, Acceleration.StartSeconds) ||
			!NearlyEqual(Acceleration.EndSeconds, Cruise.StartSeconds) ||
			!NearlyEqual(Cruise.EndSeconds, MovingTurn.StartSeconds) ||
			!NearlyEqual(MovingTurn.EndSeconds, Deceleration.StartSeconds) ||
			!NearlyEqual(Deceleration.EndSeconds, Settle.StartSeconds) ||
			!NearlyEqual(Settle.EndSeconds, CaptureWindowSeconds))
		{
			return Fail(OutError, TEXT("Scripted phase boundaries must be contiguous and cover the capture window"));
		}
		if (!InUnitRange(Acceleration.IntentStart) ||
			!InUnitRange(Acceleration.IntentEnd) ||
			!InUnitRange(Cruise.Intent) ||
			!InUnitRange(MovingTurn.Intent) ||
			!InUnitRange(Deceleration.IntentStart) ||
			!InUnitRange(Deceleration.IntentEnd))
		{
			return Fail(OutError, TEXT("Script intent magnitudes must be in [0,1]"));
		}
		if (FMath::IsNearlyZero(MovingTurn.TotalYawDegrees))
		{
			return Fail(OutError, TEXT("Moving turn must request nonzero yaw"));
		}
		if (Variants.IsEmpty() || Repetitions.IsEmpty())
		{
			return Fail(OutError, TEXT("Protocol variants and repetitions are required"));
		}
		for (const FString& Variant : Variants)
		{
			const int32* Count = Repetitions.Find(Variant);
			if (!Count || *Count <= 0)
			{
				return Fail(OutError, FString::Printf(TEXT("Protocol repetition count is missing for %s"), *Variant));
			}
		}
		if (PhysicsMinimumSamples <= 0 || PolicyMinimumSamples <= 0)
		{
			return Fail(OutError, TEXT("Protocol minimum sample counts must be positive"));
		}
		if (Acceptance.MinimumShellPathLengthCm <= 0.0 ||
			Acceptance.MinimumNetPlanarDisplacementCm <= 0.0 ||
			Acceptance.MaximumAbsYawDeltaDegrees < Acceptance.MinimumAbsYawDeltaDegrees ||
			Acceptance.MinimumPelvisHeightRatio <= 0.0 ||
			Acceptance.MinimumTargetReadbackMatchRatio <= 0.0 ||
			Acceptance.RequiredFinalRuntimeState.IsEmpty())
		{
			return Fail(OutError, TEXT("Protocol acceptance thresholds are incomplete or inconsistent"));
		}
		if (StabilityCost.WindowStartSeconds < 0.0 ||
			StabilityCost.WindowEndSeconds > CaptureWindowSeconds ||
			StabilityCost.WindowEndSeconds <= StabilityCost.WindowStartSeconds ||
			StabilityCost.Formula.IsEmpty() ||
			StabilityCost.Integration.IsEmpty())
		{
			return Fail(OutError, TEXT("Protocol stability-cost contract is incomplete or outside the capture window"));
		}
		if (Sha256.Len() != 64)
		{
			return Fail(OutError, TEXT("Protocol SHA-256 is missing or malformed"));
		}
		return true;
	}

	FStep FProtocol::ResolveStep(double TimeSeconds) const
	{
		FStep Step;
		if (TimeSeconds < Acceleration.StartSeconds)
		{
			return Step;
		}
		if (TimeSeconds < Acceleration.EndSeconds)
		{
			Step.Phase = TEXT("Acceleration");
			const double Duration = Acceleration.EndSeconds - Acceleration.StartSeconds;
			const float Alpha = static_cast<float>(FMath::Clamp(
				(TimeSeconds - Acceleration.StartSeconds) / Duration,
				0.0,
				1.0));
			Step.IntentMagnitude = static_cast<float>(
				FMath::Lerp(Acceleration.IntentStart, Acceleration.IntentEnd, static_cast<double>(Alpha)));
			Step.bMove = true;
			return Step;
		}
		if (TimeSeconds < Cruise.EndSeconds)
		{
			Step.Phase = TEXT("Cruise");
			Step.IntentMagnitude = static_cast<float>(Cruise.Intent);
			Step.bMove = true;
			return Step;
		}
		if (TimeSeconds < MovingTurn.EndSeconds)
		{
			Step.Phase = TEXT("MovingTurn");
			Step.IntentMagnitude = static_cast<float>(MovingTurn.Intent);
			const double Duration = MovingTurn.EndSeconds - MovingTurn.StartSeconds;
			Step.YawDeltaDegrees = static_cast<float>(
				MovingTurn.TotalYawDegrees * (FixedDeltaTimeSeconds / Duration));
			Step.bMove = true;
			return Step;
		}
		if (TimeSeconds < Deceleration.EndSeconds)
		{
			Step.Phase = TEXT("Deceleration");
			const double Duration = Deceleration.EndSeconds - Deceleration.StartSeconds;
			const float Alpha = static_cast<float>(FMath::Clamp(
				(TimeSeconds - Deceleration.StartSeconds) / Duration,
				0.0,
				1.0));
			Step.IntentMagnitude = static_cast<float>(
				FMath::Lerp(Deceleration.IntentStart, Deceleration.IntentEnd, static_cast<double>(Alpha)));
			Step.bMove = true;
			return Step;
		}
		Step.Phase = TEXT("Settle");
		Step.bStop = true;
		return Step;
	}

	bool LoadProtocolFromJsonText(
		const FString& JsonText,
		const FString& SourcePath,
		FProtocol& OutProtocol,
		FString& OutError)
	{
		OutProtocol = FProtocol();
		OutError.Reset();

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return Fail(OutError, FString::Printf(TEXT("Unable to parse scripted-locomotion protocol JSON: %s"), *SourcePath));
		}

		OutProtocol.SourcePath = SourcePath;
		FString NormalizedJsonText = JsonText.Replace(TEXT("\r\n"), TEXT("\n"));
		NormalizedJsonText.ReplaceInline(TEXT("\r"), TEXT("\n"));
		OutProtocol.Sha256 = HashUtf8Text(NormalizedJsonText);
		if (OutProtocol.Sha256.IsEmpty())
		{
			return Fail(OutError, TEXT("Unable to compute protocol SHA-256"));
		}

		if (!GetRequiredString(Root, TEXT("schema_version"), OutProtocol.SchemaVersion, OutError, TEXT("root")) ||
			!GetRequiredString(Root, TEXT("protocol_id"), OutProtocol.ProtocolId, OutError, TEXT("root")) ||
			!GetRequiredInt(Root, TEXT("version"), OutProtocol.Version, OutError, TEXT("root")) ||
			!GetRequiredString(Root, TEXT("status"), OutProtocol.Status, OutError, TEXT("root")) ||
			!GetRequiredString(Root, TEXT("map"), OutProtocol.Map, OutError, TEXT("root")) ||
			!GetRequiredString(Root, TEXT("actor_class"), OutProtocol.ActorClass, OutError, TEXT("root")) ||
			!GetRequiredString(Root, TEXT("skeleton"), OutProtocol.Skeleton, OutError, TEXT("root")) ||
			!GetRequiredString(Root, TEXT("model_asset"), OutProtocol.ModelAsset, OutError, TEXT("root")) ||
			!GetRequiredString(Root, TEXT("root_authority"), OutProtocol.RootAuthority, OutError, TEXT("root")) ||
			!GetRequiredString(Root, TEXT("motion_source"), OutProtocol.MotionSource, OutError, TEXT("root")) ||
			!GetRequiredBool(Root, TEXT("human_input"), OutProtocol.bHumanInput, OutError, TEXT("root")) ||
			!GetRequiredString(Root, TEXT("test_family"), OutProtocol.TestFamily, OutError, TEXT("root")) ||
			!GetRequiredString(Root, TEXT("renderer_mode"), OutProtocol.RendererMode, OutError, TEXT("root")))
		{
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* VariantValues = nullptr;
		if (!Root->TryGetArrayField(TEXT("variants"), VariantValues) || !VariantValues)
		{
			return Fail(OutError, TEXT("Missing array field root.variants"));
		}
		for (const TSharedPtr<FJsonValue>& Value : *VariantValues)
		{
			FString Variant;
			if (!Value.IsValid() || !Value->TryGetString(Variant) || Variant.IsEmpty())
			{
				return Fail(OutError, TEXT("root.variants must contain non-empty strings"));
			}
			OutProtocol.Variants.Add(Variant);
		}

		const TSharedPtr<FJsonObject>* RepetitionsObject = nullptr;
		if (!GetRequiredObject(Root, TEXT("repetitions"), RepetitionsObject, OutError, TEXT("root")))
		{
			return false;
		}
		for (const FString& Variant : OutProtocol.Variants)
		{
			int32 Count = 0;
			if (!(*RepetitionsObject)->TryGetNumberField(Variant, Count))
			{
				return Fail(OutError, FString::Printf(TEXT("Missing repetition count for %s"), *Variant));
			}
			OutProtocol.Repetitions.Add(Variant, Count);
		}

		const TSharedPtr<FJsonObject>* ScriptObject = nullptr;
		if (!GetRequiredObject(Root, TEXT("script"), ScriptObject, OutError, TEXT("root")) ||
			!GetRequiredNumber(*ScriptObject, TEXT("capture_window_sec"), OutProtocol.CaptureWindowSeconds, OutError, TEXT("root.script")) ||
			!GetRequiredNumber(*ScriptObject, TEXT("fixed_delta_time_sec"), OutProtocol.FixedDeltaTimeSeconds, OutError, TEXT("root.script")) ||
			!GetRequiredNumber(*ScriptObject, TEXT("nominal_speed_cm_per_sec"), OutProtocol.NominalSpeedCmPerSecond, OutError, TEXT("root.script")))
		{
			return false;
		}

		const TSharedPtr<FJsonObject>* StandingObject = nullptr;
		const TSharedPtr<FJsonObject>* AccelerationObject = nullptr;
		const TSharedPtr<FJsonObject>* CruiseObject = nullptr;
		const TSharedPtr<FJsonObject>* MovingTurnObject = nullptr;
		const TSharedPtr<FJsonObject>* DecelerationObject = nullptr;
		const TSharedPtr<FJsonObject>* SettleObject = nullptr;
		if (!GetRequiredObject(*ScriptObject, TEXT("standing_hold"), StandingObject, OutError, TEXT("root.script")) ||
			!GetRequiredObject(*ScriptObject, TEXT("acceleration"), AccelerationObject, OutError, TEXT("root.script")) ||
			!GetRequiredObject(*ScriptObject, TEXT("cruise"), CruiseObject, OutError, TEXT("root.script")) ||
			!GetRequiredObject(*ScriptObject, TEXT("moving_turn"), MovingTurnObject, OutError, TEXT("root.script")) ||
			!GetRequiredObject(*ScriptObject, TEXT("deceleration"), DecelerationObject, OutError, TEXT("root.script")) ||
			!GetRequiredObject(*ScriptObject, TEXT("settle"), SettleObject, OutError, TEXT("root.script")) ||
			!ParseRange(*StandingObject, OutProtocol.StandingHold, OutError, TEXT("root.script.standing_hold")) ||
			!ParseIntentRamp(*AccelerationObject, OutProtocol.Acceleration, OutError, TEXT("root.script.acceleration")) ||
			!ParseConstantIntent(*CruiseObject, OutProtocol.Cruise, OutError, TEXT("root.script.cruise")) ||
			!ParseMovingTurn(*MovingTurnObject, OutProtocol.MovingTurn, OutError, TEXT("root.script.moving_turn")) ||
			!ParseIntentRamp(*DecelerationObject, OutProtocol.Deceleration, OutError, TEXT("root.script.deceleration")) ||
			!ParseRange(*SettleObject, OutProtocol.Settle, OutError, TEXT("root.script.settle")))
		{
			return false;
		}

		const TSharedPtr<FJsonObject>* StreamsObject = nullptr;
		const TSharedPtr<FJsonObject>* PhysicsStreamObject = nullptr;
		const TSharedPtr<FJsonObject>* PolicyStreamObject = nullptr;
		if (!GetRequiredObject(Root, TEXT("sample_streams"), StreamsObject, OutError, TEXT("root")) ||
			!GetRequiredObject(*StreamsObject, TEXT("physics"), PhysicsStreamObject, OutError, TEXT("root.sample_streams")) ||
			!GetRequiredObject(*StreamsObject, TEXT("policy"), PolicyStreamObject, OutError, TEXT("root.sample_streams")) ||
			!GetRequiredInt(*PhysicsStreamObject, TEXT("minimum_samples"), OutProtocol.PhysicsMinimumSamples, OutError, TEXT("root.sample_streams.physics")) ||
			!GetRequiredInt(*PolicyStreamObject, TEXT("minimum_samples"), OutProtocol.PolicyMinimumSamples, OutError, TEXT("root.sample_streams.policy")))
		{
			return false;
		}

		const TSharedPtr<FJsonObject>* AcceptanceObject = nullptr;
		if (!GetRequiredObject(Root, TEXT("acceptance"), AcceptanceObject, OutError, TEXT("root")) ||
			!GetRequiredNumber(*AcceptanceObject, TEXT("startup_timeout_sec"), OutProtocol.Acceptance.StartupTimeoutSeconds, OutError, TEXT("root.acceptance")) ||
			!GetRequiredNumber(*AcceptanceObject, TEXT("minimum_shell_path_length_cm"), OutProtocol.Acceptance.MinimumShellPathLengthCm, OutError, TEXT("root.acceptance")) ||
			!GetRequiredNumber(*AcceptanceObject, TEXT("minimum_net_planar_displacement_cm"), OutProtocol.Acceptance.MinimumNetPlanarDisplacementCm, OutError, TEXT("root.acceptance")) ||
			!GetRequiredNumber(*AcceptanceObject, TEXT("minimum_abs_yaw_delta_deg"), OutProtocol.Acceptance.MinimumAbsYawDeltaDegrees, OutError, TEXT("root.acceptance")) ||
			!GetRequiredNumber(*AcceptanceObject, TEXT("maximum_abs_yaw_delta_deg"), OutProtocol.Acceptance.MaximumAbsYawDeltaDegrees, OutError, TEXT("root.acceptance")) ||
			!GetRequiredNumber(*AcceptanceObject, TEXT("minimum_pelvis_height_ratio"), OutProtocol.Acceptance.MinimumPelvisHeightRatio, OutError, TEXT("root.acceptance")) ||
			!GetRequiredNumber(*AcceptanceObject, TEXT("maximum_root_tilt_deg"), OutProtocol.Acceptance.MaximumRootTiltDegrees, OutError, TEXT("root.acceptance")) ||
			!GetRequiredNumber(*AcceptanceObject, TEXT("maximum_penetration_cm"), OutProtocol.Acceptance.MaximumPenetrationCm, OutError, TEXT("root.acceptance")) ||
			!GetRequiredNumber(*AcceptanceObject, TEXT("maximum_support_gap_ms"), OutProtocol.Acceptance.MaximumSupportGapMs, OutError, TEXT("root.acceptance")) ||
			!GetRequiredNumber(*AcceptanceObject, TEXT("minimum_policy_step_coverage"), OutProtocol.Acceptance.MinimumPolicyStepCoverage, OutError, TEXT("root.acceptance")) ||
			!GetRequiredNumber(*AcceptanceObject, TEXT("maximum_target_readback_error_deg"), OutProtocol.Acceptance.MaximumTargetReadbackErrorDegrees, OutError, TEXT("root.acceptance")) ||
			!GetRequiredNumber(*AcceptanceObject, TEXT("minimum_target_readback_match_ratio"), OutProtocol.Acceptance.MinimumTargetReadbackMatchRatio, OutError, TEXT("root.acceptance")) ||
			!GetRequiredNumber(*AcceptanceObject, TEXT("settle_window_start_sec"), OutProtocol.Acceptance.SettleWindowStartSeconds, OutError, TEXT("root.acceptance")) ||
			!GetRequiredNumber(*AcceptanceObject, TEXT("maximum_settle_planar_speed_cm_per_sec"), OutProtocol.Acceptance.MaximumSettlePlanarSpeedCmPerSecond, OutError, TEXT("root.acceptance")) ||
			!GetRequiredString(*AcceptanceObject, TEXT("required_final_runtime_state"), OutProtocol.Acceptance.RequiredFinalRuntimeState, OutError, TEXT("root.acceptance")) ||
			!GetRequiredNumber(*AcceptanceObject, TEXT("normal_to_zero_stability_cost_ratio_max"), OutProtocol.Acceptance.NormalToZeroStabilityCostRatioMax, OutError, TEXT("root.acceptance")) ||
			!GetRequiredBool(*AcceptanceObject, TEXT("require_all_script_phases"), OutProtocol.Acceptance.bRequireAllScriptPhases, OutError, TEXT("root.acceptance")) ||
			!GetRequiredBool(*AcceptanceObject, TEXT("require_zero_inference_failures"), OutProtocol.Acceptance.bRequireZeroInferenceFailures, OutError, TEXT("root.acceptance")) ||
			!GetRequiredBool(*AcceptanceObject, TEXT("require_cmc_inactive"), OutProtocol.Acceptance.bRequireCmcInactive, OutError, TEXT("root.acceptance")) ||
			!GetRequiredBool(*AcceptanceObject, TEXT("require_no_simroot"), OutProtocol.Acceptance.bRequireNoSimRoot, OutError, TEXT("root.acceptance")) ||
			!GetRequiredBool(*AcceptanceObject, TEXT("require_nonblank_render_capture"), OutProtocol.Acceptance.bRequireNonblankRenderCapture, OutError, TEXT("root.acceptance")))
		{
			return false;
		}

		const TSharedPtr<FJsonObject>* StabilityObject = nullptr;
		if (!GetRequiredObject(Root, TEXT("stability_cost"), StabilityObject, OutError, TEXT("root")) ||
			!GetRequiredNumber(*StabilityObject, TEXT("window_start_sec"), OutProtocol.StabilityCost.WindowStartSeconds, OutError, TEXT("root.stability_cost")) ||
			!GetRequiredNumber(*StabilityObject, TEXT("window_end_sec"), OutProtocol.StabilityCost.WindowEndSeconds, OutError, TEXT("root.stability_cost")) ||
			!GetRequiredString(*StabilityObject, TEXT("formula"), OutProtocol.StabilityCost.Formula, OutError, TEXT("root.stability_cost")) ||
			!GetRequiredString(*StabilityObject, TEXT("integration"), OutProtocol.StabilityCost.Integration, OutError, TEXT("root.stability_cost")) ||
			!GetRequiredBool(*StabilityObject, TEXT("lower_is_better"), OutProtocol.StabilityCost.bLowerIsBetter, OutError, TEXT("root.stability_cost")))
		{
			return false;
		}

		return OutProtocol.Validate(OutError);
	}

	bool LoadProtocolFromFile(const FString& ProtocolPath, FProtocol& OutProtocol, FString& OutError)
	{
		FString JsonText;
		if (!FFileHelper::LoadFileToString(JsonText, *ProtocolPath))
		{
			return Fail(OutError, FString::Printf(TEXT("Unable to read scripted-locomotion protocol: %s"), *ProtocolPath));
		}
		if (!LoadProtocolFromJsonText(JsonText, FPaths::ConvertRelativePathToFull(ProtocolPath), OutProtocol, OutError))
		{
			return false;
		}
		return OutProtocol.Validate(OutError);
	}
}
