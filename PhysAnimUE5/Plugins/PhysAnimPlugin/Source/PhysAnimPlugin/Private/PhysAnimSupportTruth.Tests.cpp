#include "PhysAnimSupportTruth.h"
#include "Misc/AutomationTest.h"

namespace
{
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimSupportTruthHarnessTest,
		"PhysAnim.SupportTruth.Harness.CompilesAndRuns",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimSupportTruthHarnessTest::RunTest(const FString& Parameters)
	{
		TestTrue(TEXT("Support truth harness compiles and runs"), true);
		return true;
	}

	FPhysAnimSupportPoint2D MakeSupportPoint(const double X, const double Y, const FName BodyName = TEXT("LeftFoot"), const EPhysAnimSupportSide SupportSide = EPhysAnimSupportSide::Left)
	{
		FPhysAnimSupportPoint2D Point;
		Point.PositionCm = FVector2D(X, Y);
		Point.BodyName = BodyName;
		Point.SupportSide = SupportSide;
		return Point;
	}

	bool ContainsPoint(const TArray<FVector2D>& Points, const FVector2D& Expected)
	{
		return Points.ContainsByPredicate(
			[Expected](const FVector2D& Point)
			{
				return Point.Equals(Expected, UE_SMALL_NUMBER);
			});
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimSupportTruthExtractPatchHullTest,
		"PhysAnim.SupportTruth.ExtractPatchHull",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimSupportTruthExtractPatchHullTest::RunTest(const FString& Parameters)
	{
		{
			TArray<FPhysAnimSupportPoint2D> Points;
			Points.Add(MakeSupportPoint(0.0, 0.0));
			Points.Add(MakeSupportPoint(10.0, 0.0));
			Points.Add(MakeSupportPoint(10.0, 10.0));
			Points.Add(MakeSupportPoint(0.0, 10.0));
			Points.Add(MakeSupportPoint(5.0, 5.0));

			const FPhysAnimSupportPatch Patch = PhysAnimSupportTruth::ExtractPatchHull(Points);

			TestTrue(TEXT("LOGIC-01 valid input remains valid"), Patch.bValidInput);
			TestEqual(TEXT("LOGIC-01 body name is preserved"), Patch.BodyName, FName(TEXT("LeftFoot")));
			TestEqual(TEXT("LOGIC-01 support side is preserved"), static_cast<uint8>(Patch.SupportSide), static_cast<uint8>(EPhysAnimSupportSide::Left));
			TestEqual(TEXT("LOGIC-01 convex hull has four points"), Patch.HullPointsCm.Num(), 4);
			TestEqual(TEXT("LOGIC-01 area is positive square area"), Patch.PatchAreaCm2, 100.0);
			TestTrue(TEXT("LOGIC-01 hull contains lower-left"), ContainsPoint(Patch.HullPointsCm, FVector2D(0.0, 0.0)));
			TestTrue(TEXT("LOGIC-01 hull contains lower-right"), ContainsPoint(Patch.HullPointsCm, FVector2D(10.0, 0.0)));
			TestTrue(TEXT("LOGIC-01 hull contains upper-right"), ContainsPoint(Patch.HullPointsCm, FVector2D(10.0, 10.0)));
			TestTrue(TEXT("LOGIC-01 hull contains upper-left"), ContainsPoint(Patch.HullPointsCm, FVector2D(0.0, 10.0)));
		}

		{
			TArray<FPhysAnimSupportPoint2D> Points;
			Points.Add(MakeSupportPoint(0.0, 0.0));
			Points.Add(MakeSupportPoint(5.0, 0.0));
			Points.Add(MakeSupportPoint(10.0, 0.0));

			const FPhysAnimSupportPatch Patch = PhysAnimSupportTruth::ExtractPatchHull(Points);

			TestTrue(TEXT("LOGIC-02 collinear input remains valid"), Patch.bValidInput);
			TestEqual(TEXT("LOGIC-02 collinear input has empty hull"), Patch.HullPointsCm.Num(), 0);
			TestEqual(TEXT("LOGIC-02 collinear area is zero"), Patch.PatchAreaCm2, 0.0);
		}

		{
			const FPhysAnimSupportPatch Patch = PhysAnimSupportTruth::ExtractPatchHull(TArray<FPhysAnimSupportPoint2D>());

			TestTrue(TEXT("LOGIC-03 empty input remains valid"), Patch.bValidInput);
			TestEqual(TEXT("LOGIC-03 empty input has empty hull"), Patch.HullPointsCm.Num(), 0);
			TestEqual(TEXT("LOGIC-03 empty input area is zero"), Patch.PatchAreaCm2, 0.0);
		}

		{
			TArray<FPhysAnimSupportPoint2D> MixedBodyPoints;
			MixedBodyPoints.Add(MakeSupportPoint(0.0, 0.0, TEXT("LeftFoot"), EPhysAnimSupportSide::Left));
			MixedBodyPoints.Add(MakeSupportPoint(10.0, 0.0, TEXT("LeftBall"), EPhysAnimSupportSide::Left));
			MixedBodyPoints.Add(MakeSupportPoint(0.0, 10.0, TEXT("LeftFoot"), EPhysAnimSupportSide::Left));

			const FPhysAnimSupportPatch MixedBodyPatch = PhysAnimSupportTruth::ExtractPatchHull(MixedBodyPoints);

			TestFalse(TEXT("LOGIC-03A mixed body input is invalid"), MixedBodyPatch.bValidInput);
			TestEqual(TEXT("LOGIC-03A mixed body has empty hull"), MixedBodyPatch.HullPointsCm.Num(), 0);
			TestEqual(TEXT("LOGIC-03A mixed body area is zero"), MixedBodyPatch.PatchAreaCm2, 0.0);

			TArray<FPhysAnimSupportPoint2D> MixedSidePoints;
			MixedSidePoints.Add(MakeSupportPoint(0.0, 0.0, TEXT("Foot"), EPhysAnimSupportSide::Left));
			MixedSidePoints.Add(MakeSupportPoint(10.0, 0.0, TEXT("Foot"), EPhysAnimSupportSide::Right));
			MixedSidePoints.Add(MakeSupportPoint(0.0, 10.0, TEXT("Foot"), EPhysAnimSupportSide::Left));

			const FPhysAnimSupportPatch MixedSidePatch = PhysAnimSupportTruth::ExtractPatchHull(MixedSidePoints);

			TestFalse(TEXT("LOGIC-03A mixed side input is invalid"), MixedSidePatch.bValidInput);
			TestEqual(TEXT("LOGIC-03A mixed side has empty hull"), MixedSidePatch.HullPointsCm.Num(), 0);
			TestEqual(TEXT("LOGIC-03A mixed side area is zero"), MixedSidePatch.PatchAreaCm2, 0.0);
		}

		return true;
	}
}
