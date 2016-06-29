#include "CDrawUtil.h"
#include "CGraphics.h"
#include "Core/GameProject/CResourceStore.h"
#include <Common/Log.h>
#include <Math/CTransform4f.h>
#include <iostream>

// ************ MEMBER INITIALIZATION ************
CVertexBuffer CDrawUtil::mGridVertices;
CIndexBuffer CDrawUtil::mGridIndices;

CDynamicVertexBuffer CDrawUtil::mSquareVertices;
CIndexBuffer CDrawUtil::mSquareIndices;

CDynamicVertexBuffer CDrawUtil::mLineVertices;
CIndexBuffer CDrawUtil::mLineIndices;

TResPtr<CModel> CDrawUtil::mpCubeModel;

CVertexBuffer CDrawUtil::mWireCubeVertices;
CIndexBuffer CDrawUtil::mWireCubeIndices;

TResPtr<CModel> CDrawUtil::mpSphereModel;
TResPtr<CModel> CDrawUtil::mpDoubleSidedSphereModel;

TResPtr<CModel> CDrawUtil::mpWireSphereModel;

CShader *CDrawUtil::mpColorShader;
CShader *CDrawUtil::mpColorShaderLighting;
CShader *CDrawUtil::mpBillboardShader;
CShader *CDrawUtil::mpLightBillboardShader;
CShader *CDrawUtil::mpTextureShader;
CShader *CDrawUtil::mpCollisionShader;
CShader *CDrawUtil::mpTextShader;

TResPtr<CTexture> CDrawUtil::mpCheckerTexture;

TResPtr<CTexture> CDrawUtil::mpLightTextures[4];
TResPtr<CTexture> CDrawUtil::mpLightMasks[4];

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
    DrawSquare(&TexCoords[0].X);
}

void CDrawUtil::DrawSquare(const CVector2f& TexUL, const CVector2f& TexUR, const CVector2f& TexBR, const CVector2f& TexBL)
{
    // Overload with tex coords specified via parameters
    // I don't think that parameters are guaranteed to be contiguous in memory, so:
    CVector2f TexCoords[4] = { TexUL, TexUR, TexBR, TexBL };
    DrawSquare(&TexCoords[0].X);
}

void CDrawUtil::DrawSquare(const float *pTexCoords)
{
    Init();

    // Set tex coords
    for (u32 iTex = 0; iTex < 8; iTex++)
    {
        EVertexAttribute TexAttrib = (EVertexAttribute) (eTex0 << (iTex *2));
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
    DrawLine(CVector3f(PointA.X, PointA.Y, 0.f), CVector3f(PointB.X, PointB.Y, 0.f), CColor::skWhite);
}

void CDrawUtil::DrawLine(const CVector3f& PointA, const CVector3f& PointB, const CColor& LineColor)
{
    Init();

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
    DrawLine(CVector3f(PointA.X, PointA.Y, 0.f), CVector3f(PointB.X, PointB.Y, 0.f), LineColor);
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
    CGraphics::sMVPBlock.ModelMatrix = CTransform4f::TranslationMatrix(Position);
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
    CGraphics::sMVPBlock.ModelMatrix = Transform;
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

void CDrawUtil::DrawWireSphere(const CVector3f& Position, float Radius, const CColor& Color /*= CColor::skWhite*/)
{
    Init();

    // Create model matrix
    CTransform4f Transform;
    Transform.Scale(Radius);
    Transform.Translate(Position);
    CGraphics::sMVPBlock.ModelMatrix = Transform;
    CGraphics::UpdateMVPBlock();

    // Set other render params
    UseColorShader(Color);
    CMaterial::KillCachedMaterial();
    glBlendFunc(GL_ONE, GL_ZERO);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);

    // Draw
    mpWireSphereModel->Draw(eNoMaterialSetup, 0);
}

void CDrawUtil::DrawBillboard(CTexture* pTexture, const CVector3f& Position, const CVector2f& Scale /*= CVector2f::skOne*/, const CColor& Tint /*= CColor::skWhite*/)
{
    Init();

    // Create translation-only model matrix
    CGraphics::sMVPBlock.ModelMatrix = CTransform4f::TranslationMatrix(Position);
    CGraphics::UpdateMVPBlock();

    // Set uniforms
    mpBillboardShader->SetCurrent();

    static GLuint ScaleLoc = mpBillboardShader->GetUniformLocation("BillboardScale");
    glUniform2f(ScaleLoc, Scale.X, Scale.Y);

    static GLuint TintLoc = mpBillboardShader->GetUniformLocation("TintColor");
    glUniform4f(TintLoc, Tint.R, Tint.G, Tint.B, Tint.A);

    pTexture->Bind(0);

    // Set other properties
    CMaterial::KillCachedMaterial();
    glBlendFunc(GL_ONE, GL_ZERO);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);

    // Draw
    DrawSquare();
}

void CDrawUtil::DrawLightBillboard(ELightType Type, const CColor& LightColor, const CVector3f& Position, const CVector2f& Scale /*= CVector2f::skOne*/, const CColor& Tint /*= CColor::skWhite*/)
{
    Init();

    // Create translation-only model matrix
    CGraphics::sMVPBlock.ModelMatrix = CTransform4f::TranslationMatrix(Position);
    CGraphics::UpdateMVPBlock();

    // Set uniforms
    mpLightBillboardShader->SetCurrent();

    static GLuint ScaleLoc = mpLightBillboardShader->GetUniformLocation("BillboardScale");
    glUniform2f(ScaleLoc, Scale.X, Scale.Y);

    static GLuint ColorLoc = mpLightBillboardShader->GetUniformLocation("LightColor");
    glUniform4f(ColorLoc, LightColor.R, LightColor.G, LightColor.B, LightColor.A);

    static GLuint TintLoc = mpLightBillboardShader->GetUniformLocation("TintColor");
    glUniform4f(TintLoc, Tint.R, Tint.G, Tint.B, Tint.A);

    CTexture *pTexA = GetLightTexture(Type);
    CTexture *pTexB = GetLightMask(Type);
    pTexA->Bind(0);
    pTexB->Bind(1);

    static GLuint TextureLoc = mpLightBillboardShader->GetUniformLocation("Texture");
    static GLuint MaskLoc    = mpLightBillboardShader->GetUniformLocation("LightMask");
    glUniform1i(TextureLoc, 0);
    glUniform1i(MaskLoc, 1);

    // Set other properties
    CMaterial::KillCachedMaterial();
    glBlendFunc(GL_ONE, GL_ZERO);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);

    // Draw
    DrawSquare();

}

void CDrawUtil::UseColorShader(const CColor& kColor)
{
    Init();
    mpColorShader->SetCurrent();

    static GLuint ColorLoc = mpColorShader->GetUniformLocation("ColorIn");
    glUniform4f(ColorLoc, kColor.R, kColor.G, kColor.B, kColor.A);

    CMaterial::KillCachedMaterial();
}

void CDrawUtil::UseColorShaderLighting(const CColor& kColor)
{
    Init();
    mpColorShaderLighting->SetCurrent();

    static GLuint NumLightsLoc = mpColorShaderLighting->GetUniformLocation("NumLights");
    glUniform1i(NumLightsLoc, CGraphics::sNumLights);

    static GLuint ColorLoc = mpColorShaderLighting->GetUniformLocation("ColorIn");
    glUniform4f(ColorLoc, kColor.R, kColor.G, kColor.B, kColor.A);

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

    static GLuint TintColorLoc = mpTextureShader->GetUniformLocation("TintColor");
    glUniform4f(TintColorLoc, TintColor.R, TintColor.G, TintColor.B, TintColor.A);

    CMaterial::KillCachedMaterial();
}

void CDrawUtil::UseCollisionShader(const CColor& TintColor /*= CColor::skWhite*/)
{
    Init();
    mpCollisionShader->SetCurrent();
    LoadCheckerboardTexture(0);

    static GLuint TintColorLoc = mpCollisionShader->GetUniformLocation("TintColor");
    glUniform4f(TintColorLoc, TintColor.R, TintColor.G, TintColor.B, TintColor.A);

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

CTexture* CDrawUtil::GetLightTexture(ELightType Type)
{
    Init();
    return mpLightTextures[Type];
}

CTexture* CDrawUtil::GetLightMask(ELightType Type)
{
    Init();
    return mpLightMasks[Type];
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
        InitWireSphere();
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
        EVertexAttribute Attrib = (EVertexAttribute) (eTex0 << (iTex *2));
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
    mpCubeModel = gResourceStore.LoadResource("../resources/Cube.cmdl");
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
    mpSphereModel = gResourceStore.LoadResource("../resources/Sphere.cmdl");
    mpDoubleSidedSphereModel = gResourceStore.LoadResource("../resources/SphereDoubleSided.cmdl");
}

void CDrawUtil::InitWireSphere()
{
    Log::Write("Creating wire sphere");
    mpWireSphereModel = gResourceStore.LoadResource("../resources/WireSphere.cmdl");
}

void CDrawUtil::InitShaders()
{
    Log::Write("Creating shaders");
    mpColorShader          = CShader::FromResourceFile("ColorShader");
    mpColorShaderLighting  = CShader::FromResourceFile("ColorShaderLighting");
    mpBillboardShader      = CShader::FromResourceFile("BillboardShader");
    mpLightBillboardShader = CShader::FromResourceFile("LightBillboardShader");
    mpTextureShader        = CShader::FromResourceFile("TextureShader");
    mpCollisionShader      = CShader::FromResourceFile("CollisionShader");
    mpTextShader           = CShader::FromResourceFile("TextShader");
}

void CDrawUtil::InitTextures()
{
    Log::Write("Loading textures");
    mpCheckerTexture = gResourceStore.LoadResource("../resources/Checkerboard.txtr");

    mpLightTextures[0] = gResourceStore.LoadResource("../resources/LightAmbient.txtr");
    mpLightTextures[1] = gResourceStore.LoadResource("../resources/LightDirectional.txtr");
    mpLightTextures[2] = gResourceStore.LoadResource("../resources/LightCustom.txtr");
    mpLightTextures[3] = gResourceStore.LoadResource("../resources/LightSpot.txtr");

    mpLightMasks[0] = gResourceStore.LoadResource("../resources/LightAmbientMask.txtr");
    mpLightMasks[1] = gResourceStore.LoadResource("../resources/LightDirectionalMask.txtr");
    mpLightMasks[2] = gResourceStore.LoadResource("../resources/LightCustomMask.txtr");
    mpLightMasks[3] = gResourceStore.LoadResource("../resources/LightSpotMask.txtr");
}

void CDrawUtil::Shutdown()
{
    if (mDrawUtilInitialized)
    {
        Log::Write("Shutting down");
        delete mpColorShader;
        delete mpColorShaderLighting;
        delete mpTextureShader;
        delete mpCollisionShader;
        delete mpTextShader;
        mDrawUtilInitialized = false;
    }
}
