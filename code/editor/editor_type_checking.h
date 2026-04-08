#ifndef TYPE_CHECKING_H
#define TYPE_CHECKING_H

/* NOTE(nasr): defining the primitve types */
enum type_kind
{

    TypeFloat32,
    TypeFloat64

    TypeSInt8
    TypeSInt16
    TypeSInt32,
    TypeSInt64,

    TypeUInt8
    TypeUInt16
    TypeUInt32,
    TypeUInt64,

    TypeChar,

    TypeVoid,

    TypePointer,
    TypeArray,
    TypeEnum,

    TypeUnion,
    TypeFuncion

    TypeNull,

    TypeConst,

    TypeStruct,
    TypeBool,

};

typedef struct Type Type;
struct Type
{
    str8 Type;
    str8 TypeKind;
};

// NOTE(nasr): C primitve types used to define other types?
global_variable const str8
PrimitiveTypes[] =
{
{.Data = "int", .Size = 3},
{.Data = "float", .Size = 5},
{.Data = "double", .Size = 6},
{.Data = "char", .Size = 4},
{.Data = "bool", .Size = 4},
{.Data = "void", .Size = 4},
{.Data = "long", .Size = 4},
{.Data = "short", .Size = 5},
};

#endif
