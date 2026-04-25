#include "PhysAnimSupportTruth.h"

namespace
{
	double Cross(const FVector2D& Origin, const FVector2D& A, const FVector2D& B)
	{
		return (A.X - Origin.X) * (B.Y - Origin.Y) - (A.Y - Origin.Y) * (B.X - Origin.X);
	}

	double CalculatePolygonAreaCm2(const TArray<FVector2D>& Points)
	{
		double TwiceArea = 0.0;

		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			const FVector2D& Current = Points[Index];
			const FVector2D& Next = Points[(Index + 1) % Points.Num()];
			TwiceArea += Current.X * Next.Y - Next.X * Current.Y;
		}

		return FMath::Abs(TwiceArea) * 0.5;
	}
}

namespace PhysAnimSupportTruth
{
	FPhysAnimSupportPatch ExtractPatchHull(const TArray<FPhysAnimSupportPoint2D>& Points)
	{
		FPhysAnimSupportPatch Result;

		if (Points.IsEmpty())
		{
			return Result;
		}

		Result.BodyName = Points[0].BodyName;
		Result.SupportSide = Points[0].SupportSide;

		TArray<FVector2D> SortedPoints;
		SortedPoints.Reserve(Points.Num());

		for (const FPhysAnimSupportPoint2D& Point : Points)
		{
			if (Point.BodyName != Result.BodyName || Point.SupportSide != Result.SupportSide)
			{
				Result.HullPointsCm.Reset();
				Result.PatchAreaCm2 = 0.0;
				Result.bValidInput = false;
				return Result;
			}

			SortedPoints.Add(Point.PositionCm);
		}

		SortedPoints.Sort(
			[](const FVector2D& Left, const FVector2D& Right)
			{
				if (!FMath::IsNearlyEqual(Left.X, Right.X))
				{
					return Left.X < Right.X;
				}

				return Left.Y < Right.Y;
			});

		for (int32 Index = SortedPoints.Num() - 1; Index > 0; --Index)
		{
			if (SortedPoints[Index].Equals(SortedPoints[Index - 1], UE_SMALL_NUMBER))
			{
				SortedPoints.RemoveAt(Index);
			}
		}

		if (SortedPoints.Num() < 3)
		{
			return Result;
		}

		TArray<FVector2D> Hull;
		Hull.Reserve(SortedPoints.Num() * 2);

		for (const FVector2D& Point : SortedPoints)
		{
			while (Hull.Num() >= 2 && Cross(Hull[Hull.Num() - 2], Hull.Last(), Point) <= UE_SMALL_NUMBER)
			{
				Hull.Pop(EAllowShrinking::No);
			}

			Hull.Add(Point);
		}

		const int32 LowerHullCount = Hull.Num();

		for (int32 Index = SortedPoints.Num() - 2; Index >= 0; --Index)
		{
			const FVector2D& Point = SortedPoints[Index];

			while (Hull.Num() > LowerHullCount && Cross(Hull[Hull.Num() - 2], Hull.Last(), Point) <= UE_SMALL_NUMBER)
			{
				Hull.Pop(EAllowShrinking::No);
			}

			Hull.Add(Point);
		}

		if (!Hull.IsEmpty())
		{
			Hull.Pop(EAllowShrinking::No);
		}

		const double AreaCm2 = Hull.Num() >= 3 ? CalculatePolygonAreaCm2(Hull) : 0.0;
		if (AreaCm2 <= UE_SMALL_NUMBER)
		{
			return Result;
		}

		Result.HullPointsCm = MoveTemp(Hull);
		Result.PatchAreaCm2 = AreaCm2;
		return Result;
	}
}
