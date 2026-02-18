/* date = January 26th 2026 11:00 am */

#ifndef EDITOR_GL_H
#define EDITOR_GL_H

#include "editor/editor_app.h"

typedef unsigned int gl_handle;

//- GL helpers 

internal void
gl_ErrorStatus(gl_handle Handle, b32 IsShader)
{
    b32 Success = true;
    
    char InfoLog[KB(2)] = {};
    if(IsShader)
    {
        glGetShaderiv(Handle, GL_COMPILE_STATUS, &Success);
        glGetShaderInfoLog(Handle, sizeof(InfoLog), NULL, InfoLog);
    }
    else
    {
        glGetProgramiv(Handle, GL_LINK_STATUS, &Success);
        glGetProgramInfoLog(Handle, sizeof(InfoLog), NULL, InfoLog);
    }
    
    if(!Success)
    {
        ErrorLog("%s", InfoLog);
    }
}

internal gl_handle
gl_CompileShaderFromSource(arena *Arena, str8 ExeDirPath, str8 FileNameAfterExe, s32 Type)
{
    gl_handle Handle = glCreateShader(Type);
    
    char *FileName = PathFromExe(Arena, ExeDirPath, FileNameAfterExe);
    str8 Source = OS_ReadEntireFileIntoMemory(FileName);
    
    if(Source.Size)
    {    
        glShaderSource(Handle, 1, (const char **)&Source.Data, NULL);
        glCompileShader(Handle);
        gl_ErrorStatus(Handle, true);
    }
    
    OS_FreeFileMemory(Source);
    
    return Handle;
}

internal gl_handle
gl_ProgramFromShaders(arena *Arena, str8 ExeDirPath, str8 VertPath, str8 FragPath)
{
    gl_handle Program = 0;
    
    gl_handle VertexShader, FragmentShader;
    VertexShader = gl_CompileShaderFromSource(Arena, ExeDirPath, VertPath, GL_VERTEX_SHADER);
    FragmentShader = gl_CompileShaderFromSource(Arena, ExeDirPath, FragPath, GL_FRAGMENT_SHADER);
    
    Program = glCreateProgram();
    glAttachShader(Program, VertexShader);
    glAttachShader(Program, FragmentShader);
    glLinkProgram(Program);
    gl_ErrorStatus(Program, false);
    
    glDeleteShader(FragmentShader); 
    glDeleteShader(VertexShader);
    
    return Program;
}

internal void
gl_LoadFloatsIntoBuffer(gl_handle BufferHandle, gl_handle ShaderHandle, char *AttributeName, u64 Count, s32 VecSize, void *Buffer)
{
    gl_handle AttribHandle;
    
    s32 SizeOfVec = sizeof(f32)*VecSize;
    
    glBindBuffer(GL_ARRAY_BUFFER, BufferHandle);
    
    AttribHandle = glGetAttribLocation(ShaderHandle, AttributeName);
    Assert(AttribHandle != -1);
    glEnableVertexAttribArray(AttribHandle);
    glVertexAttribPointer(AttribHandle, VecSize, GL_FLOAT, GL_FALSE, SizeOfVec, 0);
    
    glBufferData(GL_ARRAY_BUFFER, SizeOfVec*Count, Buffer, GL_STATIC_DRAW);
}

internal void
gl_LoadTextureFromImage(gl_handle Texture, s32 Width, s32 Height, u8 *Image, s32 Format, gl_handle ShaderProgram)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, Texture);
    
    s32 InternalFormat = GL_RGBA;
    glTexImage2D(GL_TEXTURE_2D, 0, InternalFormat, Width, Height, 0, Format, GL_UNSIGNED_BYTE, Image);
    
    // NOTE(luca): Border might show up sometimes, so we use it to debug, but it isn't really desired.
#if 0    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#else
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
#endif
    
    // TODO(luca): Use mipmap
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
#if 1    
    f32 Color[] = { 1.0f, 0.0f, 0.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, Color);
#endif
    
    gl_handle UTexture = glGetUniformLocation(ShaderProgram, "Texture"); 
    glUniform1i(UTexture, 0);
}

void
gl_SetQuadAttribute(s32 Index, s32 Count, u64 *Offset)
{
    glEnableVertexAttribArray(Index);
    glVertexAttribDivisor(Index, 1);
    glVertexAttribPointer(Index, Count, GL_FLOAT, false, sizeof(rect_instance), (void *)((*Offset)*sizeof(f32)));
    *Offset += Count;
}

#endif //EDITOR_GL_H
