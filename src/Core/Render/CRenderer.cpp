#include "CRenderer.h"

#include "CDrawUtil.h"
#include "CGraphics.h"
#include "Core/GameProject/CResourceStore.h"
#include "Core/Resource/Factory/CTextureDecoder.h"
#include <Math/CTransform4f.h>

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
    : mOptions(eEnableUVScroll | eEnableBackfaceCull)
    , mBloomMode(eNoBloom)
    , mDrawGrid(true)
    , mInitialized(false)
    , mContextIndex(-1)
{
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
        glClearColor(mClearColor.R, mClearColor.G, mClearColor.B, mClearColor.A);
        mContextIndex = CGraphics::GetContextIndex();
        mInitialized = true;
    }
}

// ************ ACCESSORS ************
FRenderOptions CRenderer::RenderOptions() const
{
    return mOptions;
}

void CRenderer::ToggleBackfaceCull(bool Enable)
{
    if (Enable) mOptions |= eEnableBackfaceCull;
    else        mOptions &= ~eEnableBackfaceCull;
}

void CRenderer::ToggleUVAnimation(bool Enable)
{
    if (Enable) mOptions |= eEnableUVScroll;
    else        mOptions &= ~eEnableUVScroll;
}

void CRenderer::ToggleGrid(bool Enable)
{
    mDrawGrid = Enable;
}

void CRenderer::ToggleOccluders(bool Enable)
{
    if (Enable) mOptions |= eEnableOccluders;
    else        mOptions &= ~eEnableOccluders;
}

void CRenderer::ToggleAlphaDisabled(bool Enable)
{
    if (Enable) mOptions |= eNoAlpha;
    else        mOptions &= ~eNoAlpha;
}

void CRenderer::SetBloom(EBloomMode BloomMode)
{
    mBloomMode = BloomMode;

    if (BloomMode != eNoBloom)
        mOptions |= eEnableBloom;
    else
        mOptions &= ~eEnableBloom;
}

void CRenderer::SetClearColor(const CColor& rkClear)
{
    mClearColor = rkClear;
    mClearColor.A = 0.f;
    glClearColor(mClearColor.R, mClearColor.G, mClearColor.B, mClearColor.A);
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
void CRenderer::RenderBuckets(const SViewInfo& rkViewInfo)
{
    if (!mInitialized) Init();
    mSceneFramebuffer.Bind();

    // Set backface culling
    if (mOptions & eEnableBackfaceCull) glEnable(GL_CULL_FACE);
    else glDisable(GL_CULL_FACE);

    // Render scene to texture
    glDepthRange(0.f, 1.f);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    mBackgroundBucket.Draw(rkViewInfo);
    mBackgroundBucket.Clear();
    ClearDepthBuffer();
    mMidgroundBucket.Draw(rkViewInfo);
    mMidgroundBucket.Clear();
    ClearDepthBuffer();
    RenderBloom();
    ClearDepthBuffer();
    rkViewInfo.pCamera->LoadMatrices();
    mForegroundBucket.Draw(rkViewInfo);
    mForegroundBucket.Clear();
    ClearDepthBuffer();
    mUIBucket.Draw(rkViewInfo);
    mUIBucket.Clear();
    ClearDepthBuffer();
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

    static const CColor skTintColors[6] = { CColor::Integral(17, 17, 17),
                                            CColor::Integral(53, 53, 53),
                                            CColor::Integral(89, 89, 89),
                                            CColor::Integral(89, 89, 89),
                                            CColor::Integral(53, 53, 53),
                                            CColor::Integral(17, 17, 17) };

    u32 BloomWidth  = (mBloomMode == eBloom ? mBloomWidth  : mViewportWidth);
    u32 BloomHeight = (mBloomMode == eBloom ? mBloomHeight : mViewportHeight);
    float BloomHScale = (mBloomMode == eBloom ? mBloomHScale : 0);
    float BloomVScale = (mBloomMode == eBloom ? mBloomVScale : 0);

    glDisable(GL_DEPTH_TEST);
    glViewport(0, 0, BloomWidth, BloomHeight);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_FALSE);

    CGraphics::SetIdentityMVP();
    CGraphics::UpdateMVPBlock();

    mPostProcessFramebuffer.Resize(mViewportWidth, mViewportHeight);
    mSceneFramebuffer.Bind(GL_READ_FRAMEBUFFER);
    mPostProcessFramebuffer.Bind(GL_DRAW_FRAMEBUFFER);
    glBlitFramebuffer(0, 0, mViewportWidth, mViewportHeight, 0, 0, mViewportWidth, mViewportHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // Pass 1: Alpha-blend the scene texture on a black background
    mBloomFramebuffers[0].Resize(BloomWidth, BloomHeight);
    mBloomFramebuffers[0].Bind();
    glClear(GL_COLOR_BUFFER_BIT);

    CDrawUtil::UseTextureShader();
    glBlendFunc(GL_SRC_ALPHA, GL_ZERO);
    mPostProcessFramebuffer.Texture()->Bind(0);
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
        CGraphics::sMVPBlock.ModelMatrix = CTransform4f::TranslationMatrix(Translate);
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
        CGraphics::sMVPBlock.ModelMatrix = CTransform4f::TranslationMatrix(Translate);
        CGraphics::UpdateMVPBlock();
        glBlendFunc(GL_ONE, GL_ONE);
        CDrawUtil::DrawSquare();
    }

    // Render result onto main scene framebuffer
    mPostProcessFramebuffer.Bind();
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

    // Copy framebuffer back into the scene framebuffer for more rendering
    mPostProcessFramebuffer.Bind(GL_READ_FRAMEBUFFER);
    mSceneFramebuffer.Bind(GL_DRAW_FRAMEBUFFER);
    glBlitFramebuffer(0, 0, mViewportWidth, mViewportHeight, 0, 0, mViewportWidth, mViewportHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // Clean up
    glEnable(GL_DEPTH_TEST);
}

void CRenderer::RenderSky(CModel *pSkyboxModel, const SViewInfo& rkViewInfo)
{
    if (!mInitialized) Init();
    if (!pSkyboxModel) return;

    glEnable(GL_CULL_FACE);

    CGraphics::sMVPBlock.ModelMatrix = CMatrix4f::skIdentity;
    CGraphics::sVertexBlock.COLOR0_Amb = CColor::skWhite;
    CGraphics::sPixelBlock.TevColor = CColor::skWhite;
    CGraphics::sPixelBlock.TintColor = CColor::skWhite;
    CGraphics::sNumLights = 0;
    CGraphics::UpdateVertexBlock();
    CGraphics::UpdatePixelBlock();
    CGraphics::UpdateLightBlock();

    // Load rotation-only view matrix
    CGraphics::sMVPBlock.ViewMatrix = rkViewInfo.RotationOnlyViewMatrix;
    CGraphics::UpdateMVPBlock();

    glDepthRange(1.f, 1.f);
    pSkyboxModel->Draw(mOptions, 0);
}

void CRenderer::AddMesh(IRenderable *pRenderable, int ComponentIndex, const CAABox& rkAABox, bool Transparent, ERenderCommand Command, EDepthGroup DepthGroup /*= eMidground*/)
{
    SRenderablePtr Ptr;
    Ptr.pRenderable = pRenderable;
    Ptr.ComponentIndex = ComponentIndex;
    Ptr.AABox = rkAABox;
    Ptr.Command = Command;

    switch (DepthGroup)
    {
    case eBackground:
        mBackgroundBucket.Add(Ptr, Transparent);
        break;

    case eMidground:
        mMidgroundBucket.Add(Ptr, Transparent);
        break;

    case eForeground:
        mForegroundBucket.Add(Ptr, Transparent);
        break;

    case eUI:
        mUIBucket.Add(Ptr, Transparent);
        break;
    }
}

void CRenderer::BeginFrame()
{
    if (!mInitialized) Init();

    CGraphics::SetActiveContext(mContextIndex);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &mDefaultFramebuffer);

    mSceneFramebuffer.SetMultisamplingEnabled(true);
    mSceneFramebuffer.Resize(mViewportWidth, mViewportHeight);
    mSceneFramebuffer.Bind();

    glViewport(0, 0, mViewportWidth, mViewportHeight);

    InitFramebuffer();
}

void CRenderer::EndFrame()
{
    // Copy result to the backbuffer
    mSceneFramebuffer.Bind(GL_READ_FRAMEBUFFER);
    CFramebuffer::BindDefaultFramebuffer(GL_DRAW_FRAMEBUFFER);
    glViewport(0, 0, mViewportWidth, mViewportHeight);
    glBlitFramebuffer(0, 0, mViewportWidth, mViewportHeight, 0, 0, mViewportWidth, mViewportHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
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
    glClearColor(mClearColor.R, mClearColor.G, mClearColor.B, mClearColor.A);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

u32 gDrawCount;
