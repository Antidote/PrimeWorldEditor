#include "CRenderer.h"

#include "CDrawUtil.h"
#include "CGraphics.h"
#include "CResCache.h"
#include <Common/AnimUtil.h>
#include <Common/CTransform4f.h>
#include <Resource/factory/CTextureDecoder.h>

#include <algorithm>
#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <gtx/transform.hpp>

// ************ STATIC MEMBER INITIALIZATION ************
u32 CRenderer::sNumRenderers = 0;

// ************ INITIALIZATION ************
CRenderer::CRenderer()
{
    mOptions = eDrawWorld | eDrawObjects | eDrawLights | eDrawSky |
               eEnableUVScroll | eEnableBackfaceCull;
    mBloomMode = eNoBloom;
    mDrawGrid = true;
    mInitialized = false;
    mContextIndex = -1;
    mOpaqueBucket.SetSortType(CRenderBucket::FrontToBack);
    mTransparentBucket.SetSortType(CRenderBucket::BackToFront);
    sNumRenderers++;
}

CRenderer::~CRenderer()
{
    sNumRenderers--;

    if (sNumRenderers == 0)
    {
        CGraphics::Shutdown();
        CDrawUtil::Shutdown();
    }
}

void CRenderer::Init()
{
    if (!mInitialized)
    {
        CVector4f ClearVec = mClearColor.ToVector4f();
        glClearColor(ClearVec.x, ClearVec.y, ClearVec.z, ClearVec.w);
        mContextIndex = CGraphics::GetContextIndex();
        mInitialized = true;
    }
}

// ************ GETTERS/SETTERS ************
ERenderOptions CRenderer::RenderOptions()
{
    return mOptions;
}

void CRenderer::ToggleWorld(bool b)
{
    if (b) mOptions |= eDrawWorld;
    else   mOptions &= ~eDrawWorld;
}

void CRenderer::ToggleWorldCollision(bool b)
{
    if (b) mOptions |= eDrawWorldCollision;
    else   mOptions &= ~eDrawWorldCollision;
}

void CRenderer::ToggleObjects(bool b)
{
    if (b) mOptions |= eDrawObjects;
    else   mOptions &= ~eDrawObjects;
}

void CRenderer::ToggleObjectCollision(bool b)
{
    if (b) mOptions |= eDrawObjectCollision;
    else   mOptions &= ~eDrawObjectCollision;
}

void CRenderer::ToggleLights(bool b)
{
    if (b) mOptions |= eDrawLights;
    else   mOptions &= ~eDrawLights;
}

void CRenderer::ToggleSky(bool b)
{
    if (b) mOptions |= eDrawSky;
    else   mOptions &= ~eDrawSky;
}

void CRenderer::ToggleBackfaceCull(bool b)
{
    if (b) mOptions |= eEnableBackfaceCull;
    else   mOptions &= ~eEnableBackfaceCull;
}

void CRenderer::ToggleUVAnimation(bool b)
{
    if (b) mOptions |= eEnableUVScroll;
    else   mOptions &= ~eEnableUVScroll;
}

void CRenderer::ToggleGrid(bool b)
{
    mDrawGrid = b;
}

void CRenderer::ToggleOccluders(bool b)
{
    if (b) mOptions |= eEnableOccluders;
    else   mOptions &= ~eEnableOccluders;
}

void CRenderer::ToggleAlphaDisabled(bool b)
{
    if (b) mOptions |= eNoAlpha;
    else   mOptions &= ~eNoAlpha;
}

void CRenderer::SetBloom(EBloomMode BloomMode)
{
    mBloomMode = BloomMode;

    if (BloomMode != eNoBloom)
        mOptions |= eEnableBloom;
    else
        mOptions &= ~eEnableBloom;
}

void CRenderer::SetFont(CFont *pFont)
{
}

void CRenderer::SetClearColor(CColor Clear)
{
    mClearColor = Clear;
    CVector4f ClearVec = Clear.ToVector4f();
    ClearVec.w = 0.f;
    glClearColor(ClearVec.x, ClearVec.y, ClearVec.z, ClearVec.w);
}

void CRenderer::SetViewportSize(u32 Width, u32 Height)
{
    mViewportWidth = Width;
    mViewportHeight = Height;
    mBloomHScale = ((float) Width / 640);
    mBloomVScale = ((float) Height / 528);
    mBloomWidth  = (u32) (320 * mBloomHScale);
    mBloomHeight = (u32) (224 * mBloomVScale);
    mBloomHScale = 1.f / mBloomHScale;
    mBloomVScale = 1.f / mBloomVScale;
}

// ************ RENDER ************
void CRenderer::RenderBuckets(CCamera& Camera)
{
    if (!mInitialized) Init();
    mSceneFramebuffer.Bind();
    //Camera.LoadMatrices();

    // Set backface culling
    if (mOptions & eEnableBackfaceCull) glEnable(GL_CULL_FACE);
    else glDisable(GL_CULL_FACE);

    // Render scene to texture
    glDepthRange(0.f, 1.f);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    mOpaqueBucket.Draw(mOptions);
    mOpaqueBucket.Clear();
    mTransparentBucket.Sort(Camera);
    mTransparentBucket.Draw(mOptions);
    mTransparentBucket.Clear();
}

void CRenderer::RenderBloom()
{
    // Check to ensure bloom is enabled
    if (mBloomMode == eNoBloom) return;

    // Setup
    static const float skHOffset[6] = { -0.008595f, -0.005470f, -0.002345f,
                                         0.002345f,  0.005470f,  0.008595f };

    static const float skVOffset[6] = { -0.012275f, -0.007815f, -0.003350f,
                                         0.003350f,  0.007815f,  0.012275f };

    static const CColor skTintColors[6] = { CColor((u8) 17, 17, 17, 255),
                                            CColor((u8) 53, 53, 53, 255),
                                            CColor((u8) 89, 89, 89, 255),
                                            CColor((u8) 89, 89, 89, 255),
                                            CColor((u8) 53, 53, 53, 255),
                                            CColor((u8) 17, 17, 17, 255) };

    float BloomWidth = (mBloomMode == eBloom ? mBloomWidth : mViewportWidth);
    float BloomHeight = (mBloomMode == eBloom ? mBloomHeight : mViewportHeight);
    float BloomHScale = (mBloomMode == eBloom ? mBloomHScale : 0);
    float BloomVScale = (mBloomMode == eBloom ? mBloomVScale : 0);

    glDisable(GL_DEPTH_TEST);
    glViewport(0, 0, BloomWidth, BloomHeight);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_FALSE);

    CGraphics::SetIdentityMVP();
    CGraphics::UpdateMVPBlock();

    // Pass 1: Alpha-blend the scene texture on a black background
    mBloomFramebuffers[0].Resize(BloomWidth, BloomHeight);
    mBloomFramebuffers[0].Bind();
    glClear(GL_COLOR_BUFFER_BIT);

    CDrawUtil::UseTextureShader();
    glBlendFunc(GL_SRC_ALPHA, GL_ZERO);
    mSceneFramebuffer.Texture()->Bind(0);
    CDrawUtil::DrawSquare();

    // Pass 2: Horizontal blur
    mBloomFramebuffers[1].Resize(BloomWidth, BloomHeight);
    mBloomFramebuffers[1].Bind();

    CDrawUtil::UseTextureShader(CColor::skGray);
    glBlendFunc(GL_ONE, GL_ZERO);
    mBloomFramebuffers[0].Texture()->Bind(0);
    CDrawUtil::DrawSquare();

    for (u32 iPass = 0; iPass < 6; iPass++)
    {
        CDrawUtil::UseTextureShader(skTintColors[iPass]);
        CVector3f Translate(skHOffset[iPass] * BloomHScale, 0.f, 0.f);
        CGraphics::sMVPBlock.ModelMatrix = CTransform4f::TranslationMatrix(Translate).ToMatrix4f();
        CGraphics::UpdateMVPBlock();
        glBlendFunc(GL_ONE, GL_ONE);
        CDrawUtil::DrawSquare();
    }

    // Pass 3: Vertical blur
    mBloomFramebuffers[2].Resize(BloomWidth, BloomHeight);
    mBloomFramebuffers[2].Bind();
    glClear(GL_COLOR_BUFFER_BIT);

    CDrawUtil::UseTextureShader(CColor::skGray);
    glBlendFunc(GL_ONE, GL_ZERO);
    mBloomFramebuffers[1].Texture()->Bind(0);
    CDrawUtil::DrawSquare();

    for (u32 iPass = 0; iPass < 6; iPass++)
    {
        CDrawUtil::UseTextureShader(skTintColors[iPass]);
        CVector3f Translate(0.f, skVOffset[iPass] * BloomVScale, 0.f);
        CGraphics::sMVPBlock.ModelMatrix = CTransform4f::TranslationMatrix(Translate).ToMatrix4f();
        CGraphics::UpdateMVPBlock();
        glBlendFunc(GL_ONE, GL_ONE);
        CDrawUtil::DrawSquare();
    }

    // Render result onto main scene framebuffer
    mSceneFramebuffer.Bind();
    glViewport(0, 0, mViewportWidth, mViewportHeight);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);

    CGraphics::SetIdentityMVP();
    CGraphics::UpdateMVPBlock();

    CDrawUtil::UseTextureShader();
    glBlendFunc(GL_ONE, GL_ONE);
    mBloomFramebuffers[2].Texture()->Bind(0);
    CDrawUtil::DrawSquare();

    if (mBloomMode == eBloomMaps)
    {
        // Bloom maps are in the framebuffer alpha channel.
        // White * dst alpha = bloom map colors
        CDrawUtil::UseColorShader(CColor::skWhite);
        glBlendFunc(GL_DST_ALPHA, GL_ZERO);
        CDrawUtil::DrawSquare();
    }

    // Clean up
    glEnable(GL_DEPTH_TEST);
}

void CRenderer::RenderSky(CModel *pSkyboxModel, CCamera& Camera)
{
    if (!mInitialized) Init();
    if (!pSkyboxModel) return;

    glEnable(GL_CULL_FACE);

    CGraphics::sMVPBlock.ModelMatrix = CMatrix4f::skIdentity;
    CGraphics::sVertexBlock.COLOR0_Amb = CVector4f(1.f, 1.f, 1.f, 1.f);
    CGraphics::sPixelBlock.TevColor = CVector4f(1.f, 1.f, 1.f, 1.f);
    CGraphics::sNumLights = 0;
    CGraphics::UpdateVertexBlock();
    CGraphics::UpdatePixelBlock();
    CGraphics::UpdateLightBlock();
    Camera.LoadRotationOnlyMatrices();

    glDepthRange(1.f, 1.f);
    pSkyboxModel->Draw(mOptions, 0);
}

void CRenderer::AddOpaqueMesh(IRenderable *pRenderable, u32 AssetID, CAABox& AABox, ERenderCommand Command)
{
    SRenderablePtr ptr;
    ptr.pRenderable = pRenderable;
    ptr.Asset = AssetID;
    ptr.AABox = AABox;
    ptr.Command = Command;
    mOpaqueBucket.Add(ptr);
}

void CRenderer::AddTransparentMesh(IRenderable *pRenderable, u32 AssetID, CAABox& AABox, ERenderCommand Command)
{
    SRenderablePtr ptr;
    ptr.pRenderable = pRenderable;
    ptr.Asset = AssetID;
    ptr.AABox = AABox;
    ptr.Command = Command;
    mTransparentBucket.Add(ptr);
}

void CRenderer::BeginFrame()
{
    if (!mInitialized) Init();

    CGraphics::SetActiveContext(mContextIndex);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &mDefaultFramebuffer);

    mSceneFramebuffer.Resize(mViewportWidth, mViewportHeight);
    mSceneFramebuffer.Bind();

    glViewport(0, 0, mViewportWidth, mViewportHeight);

    InitFramebuffer();
}

void CRenderer::EndFrame()
{
    // Render result to screen
    glBindFramebuffer(GL_FRAMEBUFFER, mDefaultFramebuffer);
    InitFramebuffer();
    glViewport(0, 0, mViewportWidth, mViewportHeight);

    CGraphics::SetIdentityMVP();
    CGraphics::UpdateMVPBlock();

    glDisable(GL_DEPTH_TEST);

    CDrawUtil::UseTextureShader();
    glBlendFunc(GL_ONE, GL_ZERO);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    mSceneFramebuffer.Texture()->Bind(0);
    CDrawUtil::DrawSquare();

    glEnable(GL_DEPTH_TEST);
    gDrawCount = 0;
}

void CRenderer::ClearDepthBuffer()
{
    glDepthMask(GL_TRUE);
    glClear(GL_DEPTH_BUFFER_BIT);
}

// ************ PRIVATE ************
void CRenderer::InitFramebuffer()
{
    CVector4f Clear = mClearColor.ToVector4f();
    Clear.w = 0.f;
    glClearColor(Clear.x, Clear.y, Clear.z, Clear.w);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

u32 gDrawCount;
