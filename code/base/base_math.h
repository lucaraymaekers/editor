/* date = March 17th 2026 1:10 am */

#ifndef BASE_MATH_H
#define BASE_MATH_H

//~ Types
typedef struct range_s64 range_s64;
struct range_s64
{
    s64 Min;
    s64 Max;
};

typedef struct range_u64 range_u64;
struct range_u64
{
    u64 Min;
    u64 Max;
};

typedef union v2 v2;
union v2
{
    f32 e[2];
    struct { f32 X, Y; };
};
#define V2Arg(Value) Value.X, Value.Y

typedef union v3 v3;
union v3
{
    f32 e[3];
    struct { f32 X, Y, Z; };
};
#define V3Arg(Value) Value.X, Value.Y, Value.Z

typedef union v4 v4;
union v4
{
    f32 e[4];
    v2 eV2[2];
    struct
    {            
        v2 Min;
        v2 Max;
    };
    struct { f32 X, Y, Z, W; };
    struct { f32 R, G, B, A; };
};
#define V4Arg(Value) Value.X, Value.Y, Value.Z, Value.W

#define U32ToV4Arg(Value) \
((f32)(((Value) >> 8*2) & 0xFF)/255.0f), \
((f32)(((Value) >> 8*1) & 0xFF)/255.0f), \
((f32)(((Value) >> 8*0) & 0xFF)/255.0f), \
((f32)(((Value) >> 8*3) & 0xFF)/255.0f)

//~ Functions 
internal inline u64
GetRangeU64Count(range_u64 Range)
{
    u64 Result = Range.Max - Range.Min;
    return Result;
}

internal inline v2 
V2AddV2(v2 A, v2 B)
{
    v2 Result = {0};
    Result.X = A.X + B.X;
    Result.Y = A.Y + B.Y;
    return Result;
}

internal inline v2 
V2AddF32(v2 A, f32 B)
{
    v2 Result = {0};
    Result.X = A.X + B;
    Result.Y = A.Y + B;
    return Result;
}

internal inline v2 
V2SubF32(v2 A, f32 B)
{
    v2 Result = {0};
    Result.X = A.X - B;
    Result.Y = A.Y - B;
    return Result;
}

internal inline v2 
V2MulF32(v2 A, f32 B)
{
    v2 Result = {0};
    Result.X = A.X * B;
    Result.Y = A.Y * B;
    return Result;
}

internal inline v2
V2SubV2(v2 A, v2 B)
{
    v2 Result = {0};
    Result.X = A.X - B.X;
    Result.Y = A.Y - B.Y;
    return Result;
}

internal inline v2
V2S32(s32 X, s32 Y)
{
    v2 Result = {0};
    Result.X = (f32)X;
    Result.Y = (f32)Y;
    return Result;
}

internal inline v2
V2MulV2(v2 A, v2 B)
{
    v2 Result = {0};
    Result.X = A.X * B.X;
    Result.Y = A.Y * B.Y;
    return Result;
}

internal inline v2 
V2(f32 A, f32 B) 
{
    v2 Result = {0};
    Result.X = A;
    Result.Y = B;
    return Result;
}

internal inline f32
Inner(v2 A, v2 B)
{
    f32 Result = A.X*B.X + A.Y*B.Y;
    return Result;
}

internal inline v3 
V3(f32 X, f32 Y, f32 Z) 
{
    v3 Result = {0};
    Result.X = X;
    Result.Y = Y;
    Result.Z = Z;
    return Result;
}

internal inline v4 
V4(f32 X, f32 Y, f32 Z, f32 W) 
{
    v4 Result = {0};
    Result.X = X;
    Result.Y = Y;
    Result.Z = Z;
    Result.W = W;
    return Result;
}

internal inline b32
InBounds(v2 A, v2 Min, v2 Max)
{
    b32 Result = !!((A.X >= Min.X && A.X < Max.X) &&
                    (A.Y >= Min.Y && A.Y < Max.Y));
    return Result;
}

#define E e[_VecMathIdx]
#define V2Math for EachIndex(_VecMathIdx, 2)
#define V3Math for EachIndex(_VecMathIdx, 3)
#define V4Math for EachIndex(_VecMathIdx, 4)
// V3Math { C.E = A.E * B.E; } 

//- 

internal inline v4
Rect(f32 MinX, f32 MinY, f32 MaxX, f32 MaxY)
{
    v4 Result = {0};
    Result.Min = V2(MinX, MinY);
    Result.Max = V2(MaxX, MaxY);
    return Result;
}

internal inline v4
RectV2(v2 Min, v2 Max)
{
    v4 Result = {0};
    Result.Min = Min;
    Result.Max = Max;
    return Result;
}

internal inline v4
RectFromCenterDim(v2 Center, v2 Dim)
{
    v4 Result = {0};
    Result.Min = V2(Center.X - Dim.X, Center.Y - Dim.Y);
    Result.Max = V2(Center.X + Dim.X, Center.Y + Dim.Y);
    return Result;
}

internal inline v4
RectFromSize(v2 TopLeft, v2 Size)
{
    v4 Result = {0};
    Result.Min = TopLeft;
    Result.Max = V2AddV2(TopLeft, Size);
    return Result;
}

internal inline v2
SizeFromRect(v4 Rec)
{
    v2 Result = V2(Rec.Max.X - Rec.Min.X,
                   Rec.Max.Y - Rec.Min.Y);
    return Result;
}

internal inline v4
V4FromRec(v4 Rec)
{
    v4 Result = V4(Rec.Min.X, Rec.Min.Y, Rec.Max.X, Rec.Max.Y);
    return Result;
}

internal inline b32
IsInside(f32 X, f32 Y, v2 Min, v2 Max)
{
    b32 Result = (X >= Min.X && X < Max.X &&
                  Y >= Min.Y && Y < Max.Y);
    return Result;
}

internal inline b32
IsInsideV2(v2 P, v2 Min, v2 Max)
{
    b32 Result = IsInside(P.X, P.Y, Min, Max);
    return Result;
}

internal inline b32
IsInsideV4(f32 X, f32 Y, v4 Rec)
{
    b32 Result = IsInside(X, Y, V2(Rec.X, Rec.Y), V2(Rec.Z, Rec.W));
    return Result;
}

internal inline b32
IsInsideRect(f32 X, f32 Y, v4 Rec)
{
    b32 Result = IsInside(X, Y, Rec.Min, Rec.Max);
    return Result;
}

internal inline b32
IsInsideRectV2(v2 Pos, v4 Rec)
{
    b32 Result = IsInsideRect(Pos.X, Pos.Y, Rec);
    return Result;
}

internal inline v4
RectShrink(v4 Rec, f32 Size)
{
    v4 Result = Rec;
    V2Math Result.Min.E += Size;
    V2Math Result.Max.E -= Size;
    return Result;
}

internal inline v4
RectIntersect(v4 A, v4 B)
{
    v4 Result = {0};
    Result.Min.X = Max(A.Min.X, B.Min.X);
    Result.Min.Y = Max(A.Min.Y, B.Min.Y);
    Result.Max.X = Min(A.Max.X, B.Max.X);
    Result.Max.Y = Min(A.Max.Y, B.Max.Y);
    return Result;
}

internal inline b32
RectValid(v4 A)
{
    b32 Result = (A.Min.X < A.Max.X && 
                  A.Min.Y < A.Max.Y);
    return Result;
}

//- 

internal inline f32
Square(f32 X)
{
    f32 Result = X*X;
    return Result;
}

internal f32
Lerp(f32 A, f32 B, f32 t)
{
    f32 Result = A*t + B*(1-t);
    return Result;
}

#endif //BASE_MATH_H
