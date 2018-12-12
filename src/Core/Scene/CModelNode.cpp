#include "CModelNode.h"
#include "Core/Render/CDrawUtil.h"
#include "Core/Render/CRenderer.h"
#include "Core/Render/CGraphics.h"
#include <Common/Math/MathUtil.h>

CModelNode::CModelNode(CScene *pScene, uint32 NodeID, CSceneNode *pParent, CModel *pModel)
    : CSceneNode(pScene, NodeID, pParent)
    , mWorldModel(false)
    , mForceAlphaOn(false)
    , mEnableScanOverlay(false)
    , mTintColor(CColor::skWhite)
{
    mScale = CVector3f::skOne;
    SetModel(pModel);
}

ENodeType CModelNode::NodeType()
{
    return eModelNode;
}

void CModelNode::PostLoad()
{
    if (mpModel)
    {
        mpModel->BufferGL();
        mpModel->GenerateMaterialShaders();
    }
}

void CModelNode::AddToRenderer(CRenderer *pRenderer, const SViewInfo& rkViewInfo)
{
    if (!mpModel) return;
    if (!rkViewInfo.ViewFrustum.BoxInFrustum(AABox())) return;
    if (rkViewInfo.GameMode) return;

    // Transparent world models should have each surface processed separately
    if (mWorldModel && mpModel->HasTransparency(mActiveMatSet))
    {
        pRenderer->AddMesh(this, -1, AABox(), false, eDrawOpaqueParts);

        for (uint32 iSurf = 0; iSurf < mpModel->GetSurfaceCount(); iSurf++)
        {
            if (mpModel->IsSurfaceTransparent(iSurf, mActiveMatSet))
                pRenderer->AddMesh(this, iSurf, mpModel->GetSurfaceAABox(iSurf).Transformed(Transform()), true, eDrawTransparentParts);
        }
    }

    // Other models should just draw all transparent surfaces sequentially
    else
        AddModelToRenderer(pRenderer, mpModel, mActiveMatSet);

    if (mSelected)
        pRenderer->AddMesh(this, -1, AABox(), false, eDrawSelection);
}

void CModelNode::Draw(FRenderOptions Options, int ComponentIndex, ERenderCommand Command, const SViewInfo& rkViewInfo)
{
    if (!mpModel) return;
    if (mForceAlphaOn) Options = (FRenderOptions) (Options & ~eNoAlpha);

    if (!mWorldModel)
    {
        CGraphics::SetDefaultLighting();
        CGraphics::UpdateLightBlock();
        CGraphics::sVertexBlock.COLOR0_Amb = CGraphics::skDefaultAmbientColor;
        CGraphics::sPixelBlock.LightmapMultiplier = 1.f;
        CGraphics::sPixelBlock.TevColor = CColor::skWhite;
    }
    else
    {
        bool IsLightingEnabled = CGraphics::sLightMode == CGraphics::eWorldLighting || rkViewInfo.GameMode;

        if (IsLightingEnabled)
        {
            CGraphics::sNumLights = 0;
            CGraphics::sVertexBlock.COLOR0_Amb = CColor::skBlack;
            CGraphics::sPixelBlock.LightmapMultiplier = 1.f;
            CGraphics::UpdateLightBlock();
        }

        else
        {
            LoadLights(rkViewInfo);
            if (CGraphics::sLightMode == CGraphics::eNoLighting)
                CGraphics::sVertexBlock.COLOR0_Amb = CColor::skWhite;
        }

        float Mul = CGraphics::sWorldLightMultiplier;
        CGraphics::sPixelBlock.TevColor = CColor(Mul,Mul,Mul);
    }

    CGraphics::sPixelBlock.TintColor = TintColor(rkViewInfo);
    LoadModelMatrix();

    if (mpModel->IsSkinned())
        CGraphics::LoadIdentityBoneTransforms();

    if (ComponentIndex == -1)
        DrawModelParts(mpModel, Options, mActiveMatSet, Command);
    else
        mpModel->DrawSurface(Options, ComponentIndex, mActiveMatSet);

    if (mEnableScanOverlay)
    {
        CDrawUtil::UseColorShader(mScanOverlayColor);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ZERO);
        Options |= eNoMaterialSetup;
        DrawModelParts(mpModel, Options, 0, Command);
    }
}

void CModelNode::DrawSelection()
{
    if (!mpModel) return;
    LoadModelMatrix();
    mpModel->DrawWireframe(eNoRenderOptions, WireframeColor());
}

void CModelNode::RayAABoxIntersectTest(CRayCollisionTester& rTester, const SViewInfo& /*rkViewInfo*/)
{
    if (!mpModel) return;

    const CRay& rkRay = rTester.Ray();
    std::pair<bool,float> BoxResult = AABox().IntersectsRay(rkRay);

    if (BoxResult.first)
        rTester.AddNodeModel(this, mpModel);
}

SRayIntersection CModelNode::RayNodeIntersectTest(const CRay& rkRay, uint32 AssetID, const SViewInfo& rkViewInfo)
{
    SRayIntersection Out;
    Out.pNode = this;
    Out.ComponentIndex = AssetID;

    CRay TransformedRay = rkRay.Transformed(Transform().Inverse());
    FRenderOptions Options = rkViewInfo.pRenderer->RenderOptions();
    std::pair<bool,float> Result = mpModel->GetSurface(AssetID)->IntersectsRay(TransformedRay, ((Options & eEnableBackfaceCull) == 0));

    if (Result.first)
    {
        Out.Hit = true;

        CVector3f HitPoint = TransformedRay.PointOnRay(Result.second);
        CVector3f WorldHitPoint = Transform() * HitPoint;
        Out.Distance = Math::Distance(rkRay.Origin(), WorldHitPoint);
    }

    else
        Out.Hit = false;

    return Out;
}

CColor CModelNode::TintColor(const SViewInfo& /*rkViewInfo*/) const
{
    return mTintColor;
}

void CModelNode::SetModel(CModel *pModel)
{
    mpModel = pModel;
    mActiveMatSet = 0;

    if (pModel)
    {
        SetName(pModel->Source());
        mLocalAABox = mpModel->AABox();
    }

    MarkTransformChanged();
}
