#include "CCharacterEditorViewport.h"

CCharacterEditorViewport::CCharacterEditorViewport(QWidget *pParent /*= 0*/)
    : CBasicViewport(pParent)
    , mpCharNode(nullptr)
{
    mpRenderer = new CRenderer();
    mpRenderer->SetViewportSize(width(), height());
    mpRenderer->SetClearColor(CColor(0.3f, 0.3f, 0.3f));
    mpRenderer->ToggleGrid(true);

    mViewInfo.ShowFlags = eShowNone; // The mesh doesn't check any show flags so this just disables the skeleton.
    mViewInfo.pRenderer = mpRenderer;
    mViewInfo.pScene = nullptr;
    mViewInfo.GameMode = false;
}

CCharacterEditorViewport::~CCharacterEditorViewport()
{
    delete mpRenderer;
}

void CCharacterEditorViewport::SetNode(CCharacterNode *pNode)
{
    mpCharNode = pNode;
}

void CCharacterEditorViewport::CheckUserInput()
{
    u32 HoverBoneID = -1;

    if (underMouse() && !IsMouseInputActive())
    {
        CRay Ray = CastRay();
        SRayIntersection Intersect = mpCharNode->RayNodeIntersectTest(Ray, 0, mViewInfo);

        if (Intersect.Hit)
            HoverBoneID = Intersect.ComponentIndex;
    }

    if (HoverBoneID != mHoverBone)
    {
        mHoverBone = HoverBoneID;
        emit HoverBoneChanged(mHoverBone);
    }
}

void CCharacterEditorViewport::Paint()
{
    mpRenderer->BeginFrame();
    mCamera.LoadMatrices();
    mGrid.AddToRenderer(mpRenderer, mViewInfo);

    if (mpCharNode)
    {
        mpCharNode->AddToRenderer(mpRenderer, mViewInfo);
    }

    mpRenderer->RenderBuckets(mViewInfo);
    mpRenderer->EndFrame();
}

void CCharacterEditorViewport::OnResize()
{
    mpRenderer->SetViewportSize(width(), height());
}
