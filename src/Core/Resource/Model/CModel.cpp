#include "CModel.h"
#include "Core/Render/CDrawUtil.h"
#include "Core/Render/CRenderer.h"
#include "Core/OpenGL/GLCommon.h"

CModel::CModel()
    : CBasicModel()
{
    mHasOwnMaterials = true;
    mHasOwnSurfaces = true;
}

CModel::CModel(CMaterialSet *pSet, bool OwnsMatSet)
    : CBasicModel()
{
    mHasOwnMaterials = OwnsMatSet;
    mHasOwnSurfaces = true;

    mMaterialSets.resize(1);
    mMaterialSets[0] = pSet;
}

CModel::~CModel()
{
    if (mHasOwnMaterials)
        for (u32 iMat = 0; iMat < mMaterialSets.size(); iMat++)
            delete mMaterialSets[iMat];
}

void CModel::BufferGL()
{
    if (!mBuffered)
    {
        mVBO.Clear();
        mSurfaceIndexBuffers.clear();

        mSurfaceIndexBuffers.resize(mSurfaces.size());

        for (u32 iSurf = 0; iSurf < mSurfaces.size(); iSurf++)
        {
            SSurface *pSurf = mSurfaces[iSurf];

            u16 VBOStartOffset = (u16) mVBO.Size();
            mVBO.Reserve((u16) pSurf->VertexCount);

            for (u32 iPrim = 0; iPrim < pSurf->Primitives.size(); iPrim++)
            {
                SSurface::SPrimitive *pPrim = &pSurf->Primitives[iPrim];
                CIndexBuffer *pIBO = InternalGetIBO(iSurf, pPrim->Type);
                pIBO->Reserve(pPrim->Vertices.size() + 1); // Allocate enough space for this primitive, plus the restart index

                std::vector<u16> Indices(pPrim->Vertices.size());
                for (u32 iVert = 0; iVert < pPrim->Vertices.size(); iVert++)
                    Indices[iVert] = mVBO.AddIfUnique(pPrim->Vertices[iVert], VBOStartOffset);

                // then add the indices to the IBO. We convert some primitives to strips to minimize draw calls.
                switch (pPrim->Type)
                {
                    case eGX_Triangles:
                        pIBO->TrianglesToStrips(Indices.data(), Indices.size());
                        break;
                    case eGX_TriangleFan:
                        pIBO->FansToStrips(Indices.data(), Indices.size());
                        break;
                    case eGX_Quads:
                        pIBO->QuadsToStrips(Indices.data(), Indices.size());
                        break;
                    default:
                        pIBO->AddIndices(Indices.data(), Indices.size());
                        pIBO->AddIndex(0xFFFF); // primitive restart
                        break;
                }
            }

            for (u32 iIBO = 0; iIBO < mSurfaceIndexBuffers[iSurf].size(); iIBO++)
                mSurfaceIndexBuffers[iSurf][iIBO].Buffer();
        }

        mBuffered = true;
    }
}

void CModel::GenerateMaterialShaders()
{
    for (u32 iSet = 0; iSet < mMaterialSets.size(); iSet++)
    {
        CMaterialSet *pSet = mMaterialSets[iSet];

        for (u32 iMat = 0; iMat < pSet->NumMaterials(); iMat++)
        {
            CMaterial *pMat = pSet->MaterialByIndex(iMat);
            pMat->GenerateShader(false);
        }
    }
}

void CModel::ClearGLBuffer()
{
    mVBO.Clear();
    mSurfaceIndexBuffers.clear();
    mBuffered = false;
}

void CModel::Draw(FRenderOptions Options, u32 MatSet)
{
    if (!mBuffered) BufferGL();
    for (u32 iSurf = 0; iSurf < mSurfaces.size(); iSurf++)
        DrawSurface(Options, iSurf, MatSet);
}

void CModel::DrawSurface(FRenderOptions Options, u32 Surface, u32 MatSet)
{
    if (!mBuffered) BufferGL();

    // Check that mat set index is valid
    if (MatSet >= mMaterialSets.size())
        MatSet = mMaterialSets.size() - 1;

    // Bind material
    if ((Options & eNoMaterialSetup) == 0)
    {
        SSurface *pSurf = mSurfaces[Surface];
        CMaterial *pMat = mMaterialSets[MatSet]->MaterialByIndex(pSurf->MaterialID);

        if ((!(Options & eEnableOccluders)) && (pMat->Options() & CMaterial::eOccluder))
            return;

        pMat->SetCurrent(Options);
    }

    // Draw IBOs
    mVBO.Bind();
    glLineWidth(1.f);

    for (u32 iIBO = 0; iIBO < mSurfaceIndexBuffers[Surface].size(); iIBO++)
    {
        CIndexBuffer *pIBO = &mSurfaceIndexBuffers[Surface][iIBO];
        pIBO->DrawElements();
    }

    mVBO.Unbind();
}

void CModel::DrawWireframe(FRenderOptions Options, CColor WireColor /*= CColor::skWhite*/)
{
    if (!mBuffered) BufferGL();

    // Set up wireframe
    WireColor.A = 0;
    CDrawUtil::UseColorShader(WireColor);
    Options |= eNoMaterialSetup;
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glBlendFunc(GL_ONE, GL_ZERO);

    // Draw surfaces
    for (u32 iSurf = 0; iSurf < mSurfaces.size(); iSurf++)
        DrawSurface(Options, iSurf, 0);

    // Cleanup
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void CModel::SetSkin(CSkin *pSkin)
{
    if (mpSkin != pSkin)
    {
        const FVertexDescription kBoneFlags = (eBoneIndices | eBoneWeights);

        mpSkin = pSkin;
        mVBO.SetSkin(pSkin);
        ClearGLBuffer();

        if (pSkin && !mVBO.VertexDesc().HasAllFlags(kBoneFlags))
            mVBO.SetVertexDesc(mVBO.VertexDesc() | kBoneFlags);
        else if (!pSkin && mVBO.VertexDesc().HasAnyFlags(kBoneFlags))
            mVBO.SetVertexDesc(mVBO.VertexDesc() & ~kBoneFlags);

        for (u32 iSet = 0; iSet < mMaterialSets.size(); iSet++)
        {
            CMaterialSet *pSet = mMaterialSets[iSet];

            for (u32 iMat = 0; iMat < pSet->NumMaterials(); iMat++)
            {
                CMaterial *pMat = pSet->MaterialByIndex(iMat);
                FVertexDescription VtxDesc = pMat->VtxDesc();

                if (pSkin && !VtxDesc.HasAllFlags(kBoneFlags))
                {
                    VtxDesc |= kBoneFlags;
                    pMat->SetVertexDescription(VtxDesc);
                }

                else if (!pSkin && VtxDesc.HasAnyFlags(kBoneFlags))
                {
                    VtxDesc &= ~kBoneFlags;
                    pMat->SetVertexDescription(VtxDesc);
                }
            }
        }
    }
}

u32 CModel::GetMatSetCount()
{
    return mMaterialSets.size();
}

u32 CModel::GetMatCount()
{
    if (mMaterialSets.empty()) return 0;
    else return mMaterialSets[0]->NumMaterials();
}

CMaterialSet* CModel::GetMatSet(u32 MatSet)
{
    return mMaterialSets[MatSet];
}

CMaterial* CModel::GetMaterialByIndex(u32 MatSet, u32 Index)
{
    if (MatSet >= mMaterialSets.size())
        MatSet = mMaterialSets.size() - 1;

    if (GetMatCount() == 0)
        return nullptr;

    return mMaterialSets[MatSet]->MaterialByIndex(Index);
}

CMaterial* CModel::GetMaterialBySurface(u32 MatSet, u32 Surface)
{
    return GetMaterialByIndex(MatSet, mSurfaces[Surface]->MaterialID);
}

bool CModel::HasTransparency(u32 MatSet)
{
    if (MatSet >= mMaterialSets.size())
        MatSet = mMaterialSets.size() - 1;

    for (u32 iMat = 0; iMat < mMaterialSets[MatSet]->NumMaterials(); iMat++)
        if (mMaterialSets[MatSet]->MaterialByIndex(iMat)->Options() & CMaterial::eTransparent ) return true;

    return false;
}

bool CModel::IsSurfaceTransparent(u32 Surface, u32 MatSet)
{
    if (MatSet >= mMaterialSets.size())
        MatSet = mMaterialSets.size() - 1;

    u32 matID = mSurfaces[Surface]->MaterialID;
    return (mMaterialSets[MatSet]->MaterialByIndex(matID)->Options() & CMaterial::eTransparent) != 0;
}

CIndexBuffer* CModel::InternalGetIBO(u32 Surface, EGXPrimitiveType Primitive)
{
    std::vector<CIndexBuffer> *pIBOs = &mSurfaceIndexBuffers[Surface];
    GLenum Type = GXPrimToGLPrim(Primitive);

    for (u32 iIBO = 0; iIBO < pIBOs->size(); iIBO++)
    {
        if ((*pIBOs)[iIBO].GetPrimitiveType() == Type)
            return &(*pIBOs)[iIBO];
    }

    pIBOs->emplace_back(CIndexBuffer(Type));
    return &pIBOs->back();
}
