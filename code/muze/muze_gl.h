/* date = January 26th 2026 11:00 am */

#ifndef MUZE_GL_H
#define MUZE_GL_H

//~ Types

typedef GLuint gl_uint;
typedef GLint gl_int;
typedef GLenum gl_enum;

typedef struct gl_render_state gl_render_state;
struct gl_render_state
{
    gl_uint VAOs[1];
    gl_uint VBOs[1];
    gl_uint Textures[2];
    gl_uint RectShader;
    b32 ShadersCompiled;
};

//~ Globals
global_variable arena *GL_FrameArena = 0;

//~ Functions 
internal void
GL_ErrorStatus(gl_uint Handle, b32 IsShader)
{
    b32 Valid = true;
    
    char InfoLog[KB(2)] = {0};
    if(IsShader)
    {
        glGetShaderiv(Handle, GL_COMPILE_STATUS, &Valid);
        glGetShaderInfoLog(Handle, sizeof(InfoLog), NULL, InfoLog);
    }
    else
    {
        glGetProgramiv(Handle, GL_LINK_STATUS, &Valid);
        glGetProgramInfoLog(Handle, sizeof(InfoLog), NULL, InfoLog);
    }
    
    if(!Valid)
    {
        ErrorLog("%s", InfoLog);
    }
}

internal gl_uint
GL_CompileShaderFromSource(str8 FileNameAfterExe, u32 Type)
{
    gl_uint Shader = (gl_uint)glCreateShader(Type);
    
    char *FileName = PathFromExe(GL_FrameArena, FileNameAfterExe);
    str8 Source = OS_ReadEntireFileIntoMemory(FileName);
    
    if(Source.Size)
    {    
        glShaderSource(Shader, 1, (const char **)&Source.Data, NULL);
        glCompileShader(Shader);
        GL_ErrorStatus(Shader, true);
    }
    
    OS_FreeFileMemory(Source);
    
    return Shader;
}

internal gl_uint
GL_ProgramFromShaders(str8 VertPath, str8 FragPath)
{
    gl_uint Program = 0;
    
    gl_uint VertexShader, FragmentShader;
    VertexShader = GL_CompileShaderFromSource(VertPath, GL_VERTEX_SHADER);
    FragmentShader = GL_CompileShaderFromSource(FragPath, GL_FRAGMENT_SHADER);
    
    Program = glCreateProgram();
    glAttachShader(Program, VertexShader);
    glAttachShader(Program, FragmentShader);
    glLinkProgram(Program);
    GL_ErrorStatus(Program, false);
    
    glDeleteShader(VertexShader);
    glDeleteShader(FragmentShader); 
    
    return Program;
}

internal void
GL_LoadFloatsIntoBuffer(gl_uint BufferHandle, gl_uint ShaderHandle, char *AttributeName, s32 Count, s32 VecSize, void *Buffer)
{
    gl_int AttribHandle;
    
    s32 SizeOfVec = (smm)(sizeof(f32))*VecSize;
    
    glBindBuffer(GL_ARRAY_BUFFER, BufferHandle);
    
    AttribHandle = glGetAttribLocation(ShaderHandle, AttributeName);
    Assert(AttribHandle != -1);
    glEnableVertexAttribArray((gl_uint)AttribHandle);
    glVertexAttribPointer((gl_uint)AttribHandle, VecSize, GL_FLOAT, GL_FALSE, SizeOfVec, 0);
    
    glBufferData(GL_ARRAY_BUFFER, (SizeOfVec*Count), Buffer, GL_STATIC_DRAW);
}

internal void
GL_LoadTextureFromImage(gl_uint Texture, s32 Width, s32 Height, u8 *Image, gl_enum Format, gl_uint ShaderProgram, char *TextureHandle)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, Texture);
    
    s32 InternalFormat = GL_RGBA;
    
    glTexImage2D(GL_TEXTURE_2D, 0, InternalFormat, Width, Height, 0, Format, GL_UNSIGNED_BYTE, Image);
    
    // NOTE(luca): Border might show up sometimes, so we use it to debug, but it isn't really desired.
#if 1
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
    
    gl_int UTexture = glGetUniformLocation(ShaderProgram, TextureHandle); 
    glUniform1i(UTexture, 0);
}

#endif //MUZE_GL_H
