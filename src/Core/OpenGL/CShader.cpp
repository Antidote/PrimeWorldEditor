#include "CShader.h"
#include "Core/Render/CGraphics.h"
#include <Common/Log.h>
#include <Common/TString.h>
#include <Common/types.h>
#include <FileIO/CTextInStream.h>

#include <fstream>
#include <sstream>

bool gDebugDumpShaders = false;
u64 gFailedCompileCount = 0;
u64 gSuccessfulCompileCount = 0;

CShader* CShader::spCurrentShader = nullptr;

CShader::CShader()
{
    mVertexShaderExists = false;
    mPixelShaderExists = false;
    mProgramExists = false;
}

CShader::CShader(const char *pkVertexSource, const char *pkPixelSource)
{
    mVertexShaderExists = false;
    mPixelShaderExists = false;
    mProgramExists = false;

    CompileVertexSource(pkVertexSource);
    CompilePixelSource(pkPixelSource);
    LinkShaders();
}

CShader::~CShader()
{
    if (mVertexShaderExists) glDeleteShader(mVertexShader);
    if (mPixelShaderExists)  glDeleteShader(mPixelShader);
    if (mProgramExists)      glDeleteProgram(mProgram);

    if (spCurrentShader == this) spCurrentShader = 0;
}

bool CShader::CompileVertexSource(const char* pkSource)
{
    mVertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(mVertexShader, 1, (const GLchar**) &pkSource, NULL);
    glCompileShader(mVertexShader);

    // Shader should be compiled - check for errors
    GLint CompileStatus;
    glGetShaderiv(mVertexShader, GL_COMPILE_STATUS, &CompileStatus);

    if (CompileStatus == GL_FALSE)
    {
        TString Out = "dump/BadVS_" + std::to_string(gFailedCompileCount) + ".txt";
        Log::Error("Unable to compile vertex shader; dumped to " + Out);
        DumpShaderSource(mVertexShader, Out);

        gFailedCompileCount++;
        glDeleteShader(mVertexShader);
        return false;
    }

    // Debug dump
    else if (gDebugDumpShaders == true)
    {
        TString Out = "dump/VS_" + TString::FromInt64(gSuccessfulCompileCount, 8, 10) + ".txt";
        Log::Write("Debug shader dumping enabled; dumped to " + Out);
        DumpShaderSource(mVertexShader, Out);

        gSuccessfulCompileCount++;
    }

    mVertexShaderExists = true;
    return true;
}

bool CShader::CompilePixelSource(const char* pkSource)
{
    mPixelShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(mPixelShader, 1, (const GLchar**) &pkSource, NULL);
    glCompileShader(mPixelShader);

    // Shader should be compiled - check for errors
    GLint CompileStatus;
    glGetShaderiv(mPixelShader, GL_COMPILE_STATUS, &CompileStatus);

    if (CompileStatus == GL_FALSE)
    {
        TString Out = "dump/BadPS_" + TString::FromInt64(gFailedCompileCount, 8, 10) + ".txt";
        Log::Error("Unable to compile pixel shader; dumped to " + Out);
        DumpShaderSource(mPixelShader, Out);

        gFailedCompileCount++;
        glDeleteShader(mPixelShader);
        return false;
    }

    // Debug dump
    else if (gDebugDumpShaders == true)
    {
        TString Out = "dump/PS_" + TString::FromInt64(gSuccessfulCompileCount, 8, 10) + ".txt";
        Log::Write("Debug shader dumping enabled; dumped to " + Out);
        DumpShaderSource(mPixelShader, Out);

        gSuccessfulCompileCount++;
    }

    mPixelShaderExists = true;
    return true;
}

bool CShader::LinkShaders()
{
    if ((!mVertexShaderExists) || (!mPixelShaderExists)) return false;

    mProgram = glCreateProgram();
    glAttachShader(mProgram, mVertexShader);
    glAttachShader(mProgram, mPixelShader);
    glLinkProgram(mProgram);

    glDeleteShader(mVertexShader);
    glDeleteShader(mPixelShader);
    mVertexShaderExists = false;
    mPixelShaderExists = false;

    // Shader should be linked - check for errors
    GLint LinkStatus;
    glGetProgramiv(mProgram, GL_LINK_STATUS, &LinkStatus);

    if (LinkStatus == GL_FALSE)
    {
        TString Out = "dump/BadLink_" + TString::FromInt64(gFailedCompileCount, 8, 10) + ".txt";
        Log::Error("Unable to link shaders. Dumped error log to " + Out);

        GLint LogLen;
        glGetProgramiv(mProgram, GL_INFO_LOG_LENGTH, &LogLen);
        GLchar *pInfoLog = new GLchar[LogLen];
        glGetProgramInfoLog(mProgram, LogLen, NULL, pInfoLog);

        std::ofstream LinkOut;
        LinkOut.open(*Out);

        if (LogLen > 0)
            LinkOut << pInfoLog;

        LinkOut.close();
        delete[] pInfoLog;

        gFailedCompileCount++;
        glDeleteProgram(mProgram);
        return false;
    }

    mMVPBlockIndex = GetUniformBlockIndex("MVPBlock");
    mVertexBlockIndex = GetUniformBlockIndex("VertexBlock");
    mPixelBlockIndex = GetUniformBlockIndex("PixelBlock");
    mLightBlockIndex = GetUniformBlockIndex("LightBlock");
    mBoneTransformBlockIndex = GetUniformBlockIndex("BoneTransformBlock");

    CacheCommonUniforms();
    mProgramExists = true;
    return true;
}

bool CShader::IsValidProgram()
{
    return mProgramExists;
}

GLuint CShader::GetProgramID()
{
    return mProgram;
}

GLuint CShader::GetUniformLocation(const char* pkUniform)
{
    return glGetUniformLocation(mProgram, pkUniform);
}

GLuint CShader::GetUniformBlockIndex(const char* pkUniformBlock)
{
    return glGetUniformBlockIndex(mProgram, pkUniformBlock);
}

void CShader::SetTextureUniforms(u32 NumTextures)
{
    for (u32 iTex = 0; iTex < NumTextures; iTex++)
        glUniform1i(mTextureUniforms[iTex], iTex);
}

void CShader::SetNumLights(u32 NumLights)
{
    glUniform1i(mNumLightsUniform, NumLights);
}

void CShader::SetCurrent()
{
    if (spCurrentShader != this)
    {
        glUseProgram(mProgram);
        spCurrentShader = this;

        glUniformBlockBinding(mProgram, mMVPBlockIndex, CGraphics::MVPBlockBindingPoint());
        glUniformBlockBinding(mProgram, mVertexBlockIndex, CGraphics::VertexBlockBindingPoint());
        glUniformBlockBinding(mProgram, mPixelBlockIndex, CGraphics::PixelBlockBindingPoint());
        glUniformBlockBinding(mProgram, mLightBlockIndex, CGraphics::LightBlockBindingPoint());
        glUniformBlockBinding(mProgram, mBoneTransformBlockIndex, CGraphics::BoneTransformBlockBindingPoint());
    }
}

// ************ STATIC ************
CShader* CShader::FromResourceFile(const TString& rkShaderName)
{
    TString VertexShaderFilename = "../resources/shaders/" + rkShaderName + ".vs";
    TString PixelShaderFilename = "../resources/shaders/" + rkShaderName + ".ps";
    CTextInStream VertexShaderFile(VertexShaderFilename.ToStdString());
    CTextInStream PixelShaderFile(PixelShaderFilename.ToStdString());

    if (!VertexShaderFile.IsValid())
        Log::Error("Couldn't load vertex shader file for " + rkShaderName);
    if (!PixelShaderFile.IsValid())
        Log::Error("Error: Couldn't load pixel shader file for " + rkShaderName);
    if ((!VertexShaderFile.IsValid()) || (!PixelShaderFile.IsValid())) return nullptr;

    std::stringstream VertexShader;
    while (!VertexShaderFile.EoF())
        VertexShader << VertexShaderFile.GetString();

    std::stringstream PixelShader;
    while (!PixelShaderFile.EoF())
        PixelShader << PixelShaderFile.GetString();

    CShader *pShader = new CShader();
    pShader->CompileVertexSource(VertexShader.str().c_str());
    pShader->CompilePixelSource(PixelShader.str().c_str());
    pShader->LinkShaders();
    return pShader;
}

CShader* CShader::CurrentShader()
{
    return spCurrentShader;
}

void CShader::KillCachedShader()
{
    spCurrentShader = 0;
}

// ************ PRIVATE ************
void CShader::CacheCommonUniforms()
{
    for (u32 iTex = 0; iTex < 8; iTex++)
    {
        TString TexUniform = "Texture" + TString::FromInt32(iTex);
        mTextureUniforms[iTex] = glGetUniformLocation(mProgram, *TexUniform);
    }

    mNumLightsUniform = glGetUniformLocation(mProgram, "NumLights");
}

void CShader::DumpShaderSource(GLuint Shader, const TString& rkOut)
{
    GLint SourceLen;
    glGetShaderiv(Shader, GL_SHADER_SOURCE_LENGTH, &SourceLen);
    GLchar *Source = new GLchar[SourceLen];
    glGetShaderSource(Shader, SourceLen, NULL, Source);

    GLint LogLen;
    glGetShaderiv(Shader, GL_INFO_LOG_LENGTH, &LogLen);
    GLchar *pInfoLog = new GLchar[LogLen];
    glGetShaderInfoLog(Shader, LogLen, NULL, pInfoLog);

    std::ofstream ShaderOut;
    ShaderOut.open(*rkOut);

    if (SourceLen > 0)
        ShaderOut << Source;
    if (LogLen > 0)
        ShaderOut << pInfoLog;

    ShaderOut.close();

    delete[] Source;
    delete[] pInfoLog;
}
