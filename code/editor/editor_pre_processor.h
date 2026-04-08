#ifndef PRE_PROCESSOR_H
#define PRE_PROCESSOR_H

typedef struct translation_unit translation_unit;
struct translation_unit
{
    s32                  *FileHandle;
    concrete_syntax_tree *Tree;
};

typedef struct File File;
struct File
{
};

#endif /* PRE_PROCESSOR_H */
