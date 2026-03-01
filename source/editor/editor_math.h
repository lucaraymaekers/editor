/* date = January 11th 2026 11:03 pm */

#ifndef EDITOR_MATH_H
#define EDITOR_MATH_H

internal inline v2 
V2AddV2(v2 A, v2 B)
{
    v2 Result = {};
    Result.X = A.X + B.X;
    Result.Y = A.Y + B.Y;
    return Result;
}

internal inline v2 
V2AddF32(v2 A, f32 B)
{
    v2 Result = {};
    Result.X = A.X + B;
    Result.Y = A.Y + B;
    return Result;
}

internal inline v2 
V2SubF32(v2 A, f32 B)
{
    v2 Result = {};
    Result.X = A.X - B;
    Result.Y = A.Y - B;
    return Result;
}

internal inline v2 
V2MulF32(v2 A, f32 B)
{
    v2 Result = {};
    Result.X = A.X * B;
    Result.Y = A.Y * B;
    return Result;
}

internal inline v2
V2SubV2(v2 A, v2 B)
{
    v2 Result = {};
    Result.X = A.X - B.X;
    Result.Y = A.Y - B.Y;
    return Result;
}

internal inline v2
V2S32(s32 X, s32 Y)
{
    v2 Result = {};
    Result.X = (f32)X;
    Result.Y = (f32)Y;
    return Result;
}

internal inline v2
V2MulV2(v2 A, v2 B)
{
    v2 Result = {};
    Result.X = A.X * B.X;
    Result.Y = A.Y * B.Y;
    return Result;
}

internal inline v2 
V2(f32 A, f32 B) 
{
    v2 Result = {};
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
    v3 Result = {};
    Result.X = X;
    Result.Y = Y;
    Result.Z = Z;
    return Result;
}

internal inline v4 
V4(f32 X, f32 Y, f32 Z, f32 W) 
{
    v4 Result = {};
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

// TODO(luca): Metadesk

#define ProvokingFuncs \
SET(V4, v4) \
SET(V3, v3) \
SET(V2, v2) \
SET(F32, f32)

#define SET(Name, type) \
internal inline void \
SetProvoking##Name(type *Quad, type Value) \
{ \
Quad[2] = Value; Quad[5] = Value; \
}
ProvokingFuncs
#undef SET

internal inline void
MakeQuadV2(v2 *Quad, v2 Min, v2 Max)
{
    Quad[0] = V2(Min.X, Min.Y); // BL
    Quad[1] = V2(Max.X, Min.Y); // BR
    Quad[2] = V2(Min.X, Max.Y); // TL
    Quad[3] = V2(Min.X, Max.Y); // TL
    Quad[4] = V2(Max.X, Max.Y); // TR
    Quad[5] = V2(Max.X, Min.Y); // BR
}

internal inline void
MakeQuadV3(v3 *Quad, v2 Min, v2 Max, f32 Z)
{
    Quad[0] = V3(Min.X, Min.Y, Z); // BL
    Quad[1] = V3(Max.X, Min.Y, Z); // BR
    Quad[2] = V3(Min.X, Max.Y, Z); // TL
    Quad[3] = V3(Min.X, Max.Y, Z); // TL
    Quad[4] = V3(Max.X, Max.Y, Z); // TR
    Quad[5] = V3(Max.X, Min.Y, Z); // BR
}

#define E e[_VecMathIdx]
#define V2Math for EachIndex(_VecMathIdx, 2)
#define V3Math for EachIndex(_VecMathIdx, 3)
#define V4Math for EachIndex(_VecMathIdx, 4)
// V3Math { C.E = A.E * B.E; } 

//- 
typedef union rect rect;
union rect
{
    f32 e[4];
    v2 eV2[2];
    struct
    {            
        v2 Min;
        v2 Max;
    };
    struct
    {
        f32 X, Y, W, Z;
    };
};

#define RectArg(Value) (Value).Min.X, (Value).Min.Y, (Value).Max.X, (Value).Max.Y

internal inline rect
Rect(f32 MinX, f32 MinY, f32 MaxX, f32 MaxY)
{
    rect Result = {0};
    Result.Min = V2(MinX, MinY);
    Result.Max = V2(MaxX, MaxY);
    return Result;
}

internal inline rect
RectV2(v2 Min, v2 Max)
{
    rect Result = {0};
    Result.Min = Min;
    Result.Max = Max;
    return Result;
}

internal inline rect
RectFromCenterDim(v2 Center, v2 Dim)
{
    rect Result = {0};
    Result.Min = V2(Center.X - Dim.X, Center.Y - Dim.Y);
    Result.Max = V2(Center.X + Dim.X, Center.Y + Dim.Y);
    return Result;
}

internal inline rect
RectFromSize(v2 TopLeft, v2 Size)
{
    rect Result = {0};
    Result.Min = TopLeft;
    Result.Max = V2AddV2(TopLeft, Size);
    return Result;
}

internal inline v2
SizeFromRect(rect Rec)
{
    v2 Result = V2(Rec.Max.X - Rec.Min.X,
                   Rec.Max.Y - Rec.Min.Y);
    return Result;
}

internal inline v4
V4FromRec(rect Rec)
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
IsInsideRec(f32 X, f32 Y, rect Rec)
{
    b32 Result = IsInside(X, Y, Rec.Min, Rec.Max);
    return Result;
}

internal inline b32
IsInsideRecV2(v2 Pos, rect Rec)
{
    b32 Result = IsInsideRec(Pos.X, Pos.Y, Rec);
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

#endif //EDITOR_MATH_H
