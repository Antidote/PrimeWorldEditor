#ifndef CDRAWUTIL
#define CDRAWUTIL

#include <OpenGL/CVertexBuffer.h>
#include <OpenGL/CDynamicVertexBuffer.h>
#include <OpenGL/CIndexBuffer.h>
#include <Resource/model/CModel.h>
#include <Resource/CLight.h>

// todo: CDrawUtil should work with CRenderer to queue primitives for rendering
// rather than trying to draw them straight away, so that CDrawUtil functions can
// be called from anywhere in the codebase and still function correctly
class CDrawUtil
{
    // 7x7 Grid
    static CVertexBuffer mGridVertices;
    static CIndexBuffer mGridIndices;

    // Square
    static CDynamicVertexBuffer mSquareVertices;
    static CIndexBuffer mSquareIndices;

    // Line
    static CDynamicVertexBuffer mLineVertices;
    static CIndexBuffer mLineIndices;

    // Cube
    static CModel *mpCubeModel;
    static CToken mCubeToken;

    // Wire Cube
    static CVertexBuffer mWireCubeVertices;
    static CIndexBuffer mWireCubeIndices;

    // Sphere
    static CModel *mpSphereModel;
    static CModel *mpDoubleSidedSphereModel;
    static CToken mSphereToken;
    static CToken mDoubleSidedSphereToken;

    // Wire Sphere
    static CModel *mpWireSphereModel;
    static CToken mWireSphereToken;

    // Shaders
    static CShader *mpColorShader;
    static CShader *mpColorShaderLighting;
    static CShader *mpBillboardShader;
    static CShader *mpLightBillboardShader;
    static CShader *mpTextureShader;
    static CShader *mpCollisionShader;
    static CShader *mpTextShader;

    // Textures
    static CTexture *mpCheckerTexture;
    static CToken mCheckerTextureToken;

    static CTexture *mpLightTextures[4];
    static CTexture *mpLightMasks[4];
    static CToken mLightTextureTokens[8];

    // Have all the above members been initialized?
    static bool mDrawUtilInitialized;

public:
    static void DrawGrid();

    static void DrawSquare();
    static void DrawSquare(const CVector2f& TexUL, const CVector2f& TexUR, const CVector2f& TexBR, const CVector2f& TexBL);
    static void DrawSquare(const float *pTexCoords);

    static void DrawLine(const CVector3f& PointA, const CVector3f& PointB);
    static void DrawLine(const CVector2f& PointA, const CVector2f& PointB);
    static void DrawLine(const CVector3f& PointA, const CVector3f& PointB, const CColor& LineColor);
    static void DrawLine(const CVector2f& PointA, const CVector2f& PointB, const CColor& LineColor);

    static void DrawCube();
    static void DrawCube(const CColor& Color);
    static void DrawCube(const CVector3f& Position, const CColor& Color);
    static void DrawShadedCube(const CColor& Color);

    static void DrawWireCube();
    static void DrawWireCube(const CAABox& AABox, const CColor& Color);

    static void DrawSphere(bool DoubleSided = false);
    static void DrawSphere(const CColor& Color);

    static void DrawWireSphere(const CVector3f& Position, float Radius, const CColor& Color = CColor::skWhite);

    static void DrawBillboard(CTexture* pTexture, const CVector3f& Position, const CVector2f& Scale = CVector2f::skOne, const CColor& Tint = CColor::skWhite);

    static void DrawLightBillboard(ELightType Type, const CColor& LightColor, const CVector3f& Position, const CVector2f& Scale = CVector2f::skOne, const CColor& Tint = CColor::skWhite);

    static void UseColorShader(const CColor& Color);
    static void UseColorShaderLighting(const CColor& Color);
    static void UseTextureShader();
    static void UseTextureShader(const CColor& TintColor);
    static void UseCollisionShader(const CColor& TintColor = CColor::skWhite);

    static CShader* GetTextShader();
    static void LoadCheckerboardTexture(u32 GLTextureUnit);
    static CTexture* GetLightTexture(ELightType Type);
    static CTexture* GetLightMask(ELightType Type);
    static CModel* GetCubeModel();

private:
    CDrawUtil(); // Private constructor to prevent class from being instantiated
    static void Init();
    static void InitGrid();
    static void InitSquare();
    static void InitLine();
    static void InitCube();
    static void InitWireCube();
    static void InitSphere();
    static void InitWireSphere();
    static void InitShaders();
    static void InitTextures();

public:
    static void Shutdown();
};

#endif // CDRAWUTIL

