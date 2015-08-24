#include "CDrawUtil.h"
#include "CGraphics.h"
#include "CResCache.h"
#include <Common/CTransform4f.h>
#include <iostream>
#include "Log.h"

// ************ MEMBER INITIALIZATION ************
CVertexBuffer CDrawUtil::mGridVertices;
CIndexBuffer CDrawUtil::mGridIndices;

CDynamicVertexBuffer CDrawUtil::mSquareVertices;
CIndexBuffer CDrawUtil::mSquareIndices;

CDynamicVertexBuffer CDrawUtil::mLineVertices;
CIndexBuffer CDrawUtil::mLineIndices;

CModel *CDrawUtil::mpCubeModel;
CToken CDrawUtil::mCubeToken;

CVertexBuffer CDrawUtil::mWireCubeVertices;
CIndexBuffer CDrawUtil::mWireCubeIndices;

CModel *CDrawUtil::mpSphereModel;
CModel *CDrawUtil::mpDoubleSidedSphereModel;
CToken CDrawUtil::mSphereToken;
CToken CDrawUtil::mDoubleSidedSphereToken;

CShader *CDrawUtil::mpColorShader;
CShader *CDrawUtil::mpColorShaderLighting;
CShader *CDrawUtil::mpTextureShader;
CShader *CDrawUtil::mpCollisionShader;
CShader *CDrawUtil::mpTextShader;

CTexture *CDrawUtil::mpCheckerTexture;
CToken CDrawUtil::mCheckerTextureToken;

bool CDrawUtil::mDrawUtilInitialized = false;

// ************ PUBLIC ************
void CDrawUtil::DrawGrid()
{
    Init();

    mGridVertices.Bind();

    CGraphics::sMVPBlock.ModelMatrix = CMatrix4f::skIdentity;
    CGraphics::UpdateMVPBlock();

    glBlendFunc(GL_ONE, GL_ZERO);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);

    glLineWidth(1.0f);
    UseColorShader(CColor(0.6f, 0.6f, 0.6f, 0.f));
    mGridIndices.DrawElements(0, mGridIndices.GetSize() - 4);

    glLineWidth(1.5f);
    UseColorShader(CColor::skTransparentBlack);
    mGridIndices.DrawElements(mGridIndices.GetSize() - 4, 4);
}

void CDrawUtil::DrawSquare()
{
    // Overload with default tex coords
    CVector2f TexCoords[4] = { CVector2f(0.f, 1.f), CVector2f(1.f, 1.f), CVector2f(1.f, 0.f), CVector2f(0.f, 0.f) };
    DrawSquare(&TexCoords[0].x);
}

void CDrawUtil::DrawSquare(const CVector2f& TexUL, const CVector2f& TexUR, const CVector2f& TexBR, const CVector2f& TexBL)
{
    // Overload with tex coords specified via parameters
    // I don't think that parameters are guaranteed to be contiguous in memory, so:
    CVector2f TexCoords[4] = { TexUL, TexUR, TexBR, TexBL };
    DrawSquare(&TexCoords[0].x);
}

void CDrawUtil::DrawSquare(const float *pTexCoords)
{
    Init();

    // Set tex coords
    for (u32 iTex = 0; iTex < 8; iTex++)
    {
        EVertexDescription TexAttrib = (EVertexDescription) (eTex0 << (iTex *2));
        mSquareVertices.BufferAttrib(TexAttrib, pTexCoords);
    }

    // Draw
    mSquareVertices.Bind();
    mSquareIndices.DrawElements();
    mSquareVertices.Unbind();
}

void CDrawUtil::DrawLine(const CVector3f& PointA, const CVector3f& PointB)
{
    DrawLine(PointA, PointB, CColor::skWhite);
}

void CDrawUtil::DrawLine(const CVector2f& PointA, const CVector2f& PointB)
{
    // Overload for 2D lines
    DrawLine(CVector3f(PointA.x, PointA.y, 0.f), CVector3f(PointB.x, PointB.y, 0.f), CColor::skWhite);
}

void CDrawUtil::DrawLine(const CVector3f& PointA, const CVector3f& PointB, const CColor& LineColor)
{
    Init();

    glLineWidth(1.f);
    CGraphics::sMVPBlock.ModelMatrix = CMatrix4f::skIdentity;
    CGraphics::UpdateMVPBlock();

    // Copy vec3s into an array to ensure they are adjacent in memory
    CVector3f Points[2] = { PointA, PointB };
    mLineVertices.BufferAttrib(ePosition, Points);

    // Draw
    UseColorShader(LineColor);
    mLineVertices.Bind();
    mLineIndices.DrawElements();
    mLineVertices.Unbind();
}

void CDrawUtil::DrawLine(const CVector2f& PointA, const CVector2f& PointB, const CColor& LineColor)
{
    // Overload for 2D lines
    DrawLine(CVector3f(PointA.x, PointA.y, 0.f), CVector3f(PointB.x, PointB.y, 0.f), LineColor);
}

void CDrawUtil::DrawCube()
{
    Init();
    mpCubeModel->Draw(eNoMaterialSetup, 0);
}

void CDrawUtil::DrawCube(const CColor& Color)
{
    Init();
    UseColorShader(Color);
    DrawCube();
}

void CDrawUtil::DrawCube(const CVector3f& Position, const CColor& Color)
{
    CGraphics::sMVPBlock.ModelMatrix = CTransform4f::TranslationMatrix(Position).ToMatrix4f();
    CGraphics::UpdateMVPBlock();
    UseColorShader(Color);
    DrawCube();
}

void CDrawUtil::DrawShadedCube(const CColor& Color)
{
    Init();
    UseColorShaderLighting(Color);
    DrawCube();
}

void CDrawUtil::DrawWireCube()
{
    Init();
    glLineWidth(1.f);
    mWireCubeVertices.Bind();
    mWireCubeIndices.DrawElements();
    mWireCubeVertices.Unbind();
}

void CDrawUtil::DrawWireCube(const CAABox& kAABox, const CColor& kColor)
{
    Init();

    // Calculate model matrix
    CTransform4f Transform;
    Transform.Scale(kAABox.Size());
    Transform.Translate(kAABox.Center());
    CGraphics::sMVPBlock.ModelMatrix = Transform.ToMatrix4f();
    CGraphics::UpdateMVPBlock();

    UseColorShader(kColor);
    DrawWireCube();
}

void CDrawUtil::DrawSphere(bool DoubleSided)
{
    Init();

    if (!DoubleSided)
        mpSphereModel->Draw(eNoMaterialSetup, 0);
    else
        mpDoubleSidedSphereModel->Draw(eNoMaterialSetup, 0);
}

void CDrawUtil::DrawSphere(const CColor &kColor)
{
    Init();
    UseColorShader(kColor);
    DrawSphere(false);
}

void CDrawUtil::UseColorShader(const CColor& kColor)
{
    Init();
    mpColorShader->SetCurrent();

    GLuint ColorLoc = mpColorShader->GetUniformLocation("ColorIn");
    CVector4f ColorVec4 = kColor.ToVector4f();
    glUniform4f(ColorLoc, ColorVec4.x, ColorVec4.y, ColorVec4.z, ColorVec4.w);

    CMaterial::KillCachedMaterial();
}

void CDrawUtil::UseColorShaderLighting(const CColor& kColor)
{
    Init();
    mpColorShaderLighting->SetCurrent();

    GLuint NumLightsLoc = mpColorShaderLighting->GetUniformLocation("NumLights");
    glUniform1i(NumLightsLoc, CGraphics::sNumLights);

    GLuint ColorLoc = mpColorShaderLighting->GetUniformLocation("ColorIn");
    CVector4f ColorVec4 = kColor.ToVector4f();
    glUniform4f(ColorLoc, ColorVec4.x, ColorVec4.y, ColorVec4.z, ColorVec4.w);

    CMaterial::KillCachedMaterial();
}

void CDrawUtil::UseTextureShader()
{
    UseTextureShader(CColor::skWhite);
}

void CDrawUtil::UseTextureShader(const CColor& TintColor)
{
    Init();
    mpTextureShader->SetCurrent();

    GLuint TintColorLoc = mpTextureShader->GetUniformLocation("TintColor");
    CVector4f TintVec4 = TintColor.ToVector4f();
    glUniform4f(TintColorLoc, TintVec4.x, TintVec4.y, TintVec4.z, TintVec4.w);

    CMaterial::KillCachedMaterial();
}

void CDrawUtil::UseCollisionShader()
{
    Init();
    mpCollisionShader->SetCurrent();
    LoadCheckerboardTexture(0);

    CMaterial::KillCachedMaterial();
}

CShader* CDrawUtil::GetTextShader()
{
    Init();
    return mpTextShader;
}

void CDrawUtil::LoadCheckerboardTexture(u32 GLTextureUnit)
{
    Init();
    mpCheckerTexture->Bind(GLTextureUnit);
}

CModel* CDrawUtil::GetCubeModel()
{
    Init();
    return mpCubeModel;
}

// ************ PRIVATE ************
CDrawUtil::CDrawUtil()
{
}

void CDrawUtil::Init()
{
    if (!mDrawUtilInitialized)
    {
        Log::Write("Initializing CDrawUtil");
        InitGrid();
        InitSquare();
        InitLine();
        InitCube();
        InitWireCube();
        InitSphere();
        InitShaders();
        InitTextures();
        mDrawUtilInitialized = true;
    }
}

void CDrawUtil::InitGrid()
{
    Log::Write("Creating grid");
    mGridVertices.SetVertexDesc(ePosition);
    mGridVertices.Reserve(64);

     for (s32 i = -7; i < 8; i++)
     {
         if (i == 0) continue;
         mGridVertices.AddVertex(CVector3f(-7.0f, float(i), 0.0f));
         mGridVertices.AddVertex(CVector3f( 7.0f, float(i), 0.0f));
         mGridVertices.AddVertex(CVector3f(float(i), -7.0f, 0.0f));
         mGridVertices.AddVertex(CVector3f(float(i),  7.0f, 0.0f));
     }

     mGridVertices.AddVertex(CVector3f(-7.0f, 0, 0.0f));
     mGridVertices.AddVertex(CVector3f( 7.0f, 0, 0.0f));
     mGridVertices.AddVertex(CVector3f(0, -7.0f, 0.0f));
     mGridVertices.AddVertex(CVector3f(0,  7.0f, 0.0f));

     mGridIndices.Reserve(60);
     for (u16 i = 0; i < 60; i++) mGridIndices.AddIndex(i);
     mGridIndices.SetPrimitiveType(GL_LINES);
}

void CDrawUtil::InitSquare()
{
    Log::Write("Creating square");
    mSquareVertices.SetActiveAttribs(ePosition |  eNormal |
                                     eTex0 | eTex1 | eTex2 | eTex3 |
                                     eTex4 | eTex5 | eTex6 | eTex7);
    mSquareVertices.SetVertexCount(4);

    CVector3f SquareVertices[] = {
        CVector3f(-1.f,  1.f, 0.f),
        CVector3f( 1.f,  1.f, 0.f),
        CVector3f( 1.f, -1.f, 0.f),
        CVector3f(-1.f, -1.f, 0.f)
    };

    CVector3f SquareNormals[] = {
        CVector3f(0.f, 0.f, 1.f),
        CVector3f(0.f, 0.f, 1.f),
        CVector3f(0.f, 0.f, 1.f),
        CVector3f(0.f, 0.f, 1.f)
    };

    CVector2f SquareTexCoords[] = {
        CVector2f(0.f, 1.f),
        CVector2f(1.f, 1.f),
        CVector2f(1.f, 0.f),
        CVector2f(0.f, 0.f)
    };

    mSquareVertices.BufferAttrib(ePosition, SquareVertices);
    mSquareVertices.BufferAttrib(eNormal, SquareNormals);

    for (u32 iTex = 0; iTex < 8; iTex++)
    {
        EVertexDescription Attrib = (EVertexDescription) (eTex0 << (iTex *2));
        mSquareVertices.BufferAttrib(Attrib, SquareTexCoords);
    }

    mSquareIndices.Reserve(4);
    mSquareIndices.SetPrimitiveType(GL_TRIANGLE_STRIP);
    mSquareIndices.AddIndex(3);
    mSquareIndices.AddIndex(2);
    mSquareIndices.AddIndex(0);
    mSquareIndices.AddIndex(1);
}

void CDrawUtil::InitLine()
{
    Log::Write("Creating line");
    mLineVertices.SetActiveAttribs(ePosition);
    mLineVertices.SetVertexCount(2);

    mLineIndices.Reserve(2);
    mLineIndices.SetPrimitiveType(GL_LINES);
    mLineIndices.AddIndex(0);
    mLineIndices.AddIndex(1);
}

void CDrawUtil::InitCube()
{
    Log::Write("Creating cube");
    mpCubeModel = (CModel*) gResCache.GetResource("../resources/Cube.cmdl");
    mCubeToken = CToken(mpCubeModel);
}

void CDrawUtil::InitWireCube()
{
    Log::Write("Creating wire cube");
    mWireCubeVertices.SetVertexDesc(ePosition);
    mWireCubeVertices.Reserve(8);
    mWireCubeVertices.AddVertex(CVector3f(-0.5f, -0.5f, -0.5f));
    mWireCubeVertices.AddVertex(CVector3f(-0.5f,  0.5f, -0.5f));
    mWireCubeVertices.AddVertex(CVector3f( 0.5f,  0.5f, -0.5f));
    mWireCubeVertices.AddVertex(CVector3f( 0.5f, -0.5f, -0.5f));
    mWireCubeVertices.AddVertex(CVector3f(-0.5f, -0.5f,  0.5f));
    mWireCubeVertices.AddVertex(CVector3f( 0.5f, -0.5f,  0.5f));
    mWireCubeVertices.AddVertex(CVector3f( 0.5f,  0.5f,  0.5f));
    mWireCubeVertices.AddVertex(CVector3f(-0.5f,  0.5f,  0.5f));

    u16 Indices[] = {
        0, 1,
        1, 2,
        2, 3,
        3, 0,
        4, 5,
        5, 6,
        6, 7,
        7, 4,
        0, 4,
        1, 7,
        2, 6,
        3, 5
    };
    mWireCubeIndices.AddIndices(Indices, sizeof(Indices) / sizeof(u16));
    mWireCubeIndices.SetPrimitiveType(GL_LINES);
}

void CDrawUtil::InitSphere()
{
    Log::Write("Creating sphere");
    mpSphereModel = (CModel*) gResCache.GetResource("../resources/Sphere.cmdl");
    mpDoubleSidedSphereModel = (CModel*) gResCache.GetResource("../resources/SphereDoubleSided.cmdl");
    mSphereToken = CToken(mpSphereModel);
    mDoubleSidedSphereToken = CToken(mpDoubleSidedSphereModel);
}

void CDrawUtil::InitShaders()
{
    Log::Write("Creating shaders");
    mpColorShader         = CShader::FromResourceFile("ColorShader");
    mpColorShaderLighting = CShader::FromResourceFile("ColorShaderLighting");
    mpTextureShader       = CShader::FromResourceFile("TextureShader");
    mpCollisionShader     = CShader::FromResourceFile("CollisionShader");
    mpTextShader          = CShader::FromResourceFile("TextShader");
}

void CDrawUtil::InitTextures()
{
    Log::Write("Loading checkerboard texture");
    mpCheckerTexture = (CTexture*) gResCache.GetResource("../resources/Checkerboard.txtr");
    mCheckerTextureToken = CToken(mpCheckerTexture);
}

void CDrawUtil::Shutdown()
{
    if (mDrawUtilInitialized)
    {
        Log::Write("Shutting down");
        mCubeToken = CToken();
        mSphereToken = CToken();
        mDoubleSidedSphereToken = CToken();
        delete mpColorShader;
        delete mpColorShaderLighting;
        delete mpTextureShader;
        delete mpCollisionShader;
        delete mpTextShader;
        mDrawUtilInitialized = false;
    }
}
