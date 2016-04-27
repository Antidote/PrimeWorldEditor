#include "CModelLoader.h"
#include "CMaterialLoader.h"
#include <Common/Log.h>
#include <map>

CModelLoader::CModelLoader()
    : mFlags(eNoFlags)
    , mNumVertices(0)
{
}

CModelLoader::~CModelLoader()
{
}

void CModelLoader::LoadWorldMeshHeader(IInputStream& rModel)
{
    // I don't really have any need for most of this data, so
    rModel.Seek(0x34, SEEK_CUR);
    mAABox = CAABox(rModel);
    mpSectionMgr->ToNextSection();
}

void CModelLoader::LoadAttribArrays(IInputStream& rModel)
{
    // Positions
    if (mFlags & eShortPositions) // Shorts (DKCR only)
    {
        mPositions.resize(mpSectionMgr->CurrentSectionSize() / 0x6);
        float Divisor = 8192.f; // Might be incorrect! Needs verification via size comparison.

        for (u32 iVtx = 0; iVtx < mPositions.size(); iVtx++)
        {
            float X = rModel.ReadShort() / Divisor;
            float Y = rModel.ReadShort() / Divisor;
            float Z = rModel.ReadShort() / Divisor;
            mPositions[iVtx] = CVector3f(X, Y, Z);
        }
    }

    else // Floats
    {
        mPositions.resize(mpSectionMgr->CurrentSectionSize() / 0xC);

        for (u32 iVtx = 0; iVtx < mPositions.size(); iVtx++)
            mPositions[iVtx] = CVector3f(rModel);
    }

    mpSectionMgr->ToNextSection();

    // Normals
    if (mFlags & eShortNormals) // Shorts
    {
        mNormals.resize(mpSectionMgr->CurrentSectionSize() / 0x6);
        float Divisor = (mVersion < eReturns) ? 32768.f : 16384.f;

        for (u32 iVtx = 0; iVtx < mNormals.size(); iVtx++)
        {
            float X = rModel.ReadShort() / Divisor;
            float Y = rModel.ReadShort() / Divisor;
            float Z = rModel.ReadShort() / Divisor;
            mNormals[iVtx] = CVector3f(X, Y, Z);
        }
    }
    else // Floats
    {
        mNormals.resize(mpSectionMgr->CurrentSectionSize() / 0xC);

        for (u32 iVtx = 0; iVtx < mNormals.size(); iVtx++)
            mNormals[iVtx] = CVector3f(rModel);
    }

    mpSectionMgr->ToNextSection();

    // Colors
    mColors.resize(mpSectionMgr->CurrentSectionSize() / 4);

    for (u32 iVtx = 0; iVtx < mColors.size(); iVtx++)
        mColors[iVtx] = CColor(rModel);

    mpSectionMgr->ToNextSection();


    // Float UVs
    mTex0.resize(mpSectionMgr->CurrentSectionSize() / 0x8);

    for (u32 iVtx = 0; iVtx < mTex0.size(); iVtx++)
        mTex0[iVtx] = CVector2f(rModel);

    mpSectionMgr->ToNextSection();

    // Short UVs
    if (mFlags & eHasTex1)
    {
        mTex1.resize(mpSectionMgr->CurrentSectionSize() / 0x4);
        float Divisor = (mVersion < eReturns) ? 32768.f : 8192.f;

        for (u32 iVtx = 0; iVtx < mTex1.size(); iVtx++)
        {
            float X = rModel.ReadShort() / Divisor;
            float Y = rModel.ReadShort() / Divisor;
            mTex1[iVtx] = CVector2f(X, Y);
        }

        mpSectionMgr->ToNextSection();
    }
}

void CModelLoader::LoadSurfaceOffsets(IInputStream& rModel)
{
    mSurfaceCount = rModel.ReadLong();
    mSurfaceOffsets.resize(mSurfaceCount);

    for (u32 iSurf = 0; iSurf < mSurfaceCount; iSurf++)
        mSurfaceOffsets[iSurf] = rModel.ReadLong();

    mpSectionMgr->ToNextSection();
}

SSurface* CModelLoader::LoadSurface(IInputStream& rModel)
{
    SSurface *pSurf = new SSurface;

    // Surface header
    if (mVersion  < eReturns)
        LoadSurfaceHeaderPrime(rModel, pSurf);
    else
        LoadSurfaceHeaderDKCR(rModel, pSurf);

    bool HasAABB = (pSurf->AABox != CAABox::skInfinite);
    CMaterial *pMat = mMaterials[0]->MaterialByIndex(pSurf->MaterialID);

    // Primitive table
    u8 Flag = rModel.ReadByte();
    u32 NextSurface = mpSectionMgr->NextOffset();

    while ((Flag != 0) && ((u32) rModel.Tell() < NextSurface))
    {
        SSurface::SPrimitive Prim;
        Prim.Type = EGXPrimitiveType(Flag & 0xF8);
        u16 VertexCount = rModel.ReadShort();

        for (u16 iVtx = 0; iVtx < VertexCount; iVtx++)
        {
            CVertex Vtx;
            FVertexDescription VtxDesc = pMat->VtxDesc();

            for (u32 iMtxAttr = 0; iMtxAttr < 8; iMtxAttr++)
                if (VtxDesc & (ePosMtx << iMtxAttr)) rModel.Seek(0x1, SEEK_CUR);

            // Only thing to do here is check whether each attribute is present, and if so, read it.
            // A couple attributes have special considerations; normals can be floats or shorts, as can tex0, depending on vtxfmt.
            // tex0 can also be read from either UV buffer; depends what the material says.

            // Position
            if (VtxDesc & ePosition)
            {
                u16 PosIndex = rModel.ReadShort() & 0xFFFF;
                Vtx.Position = mPositions[PosIndex];
                Vtx.ArrayPosition = PosIndex;

                if (!HasAABB) pSurf->AABox.ExpandBounds(Vtx.Position);
            }

            // Normal
            if (VtxDesc & eNormal)
                Vtx.Normal = mNormals[rModel.ReadShort() & 0xFFFF];

            // Color
            for (u32 iClr = 0; iClr < 2; iClr++)
                if (VtxDesc & (eColor0 << iClr))
                    Vtx.Color[iClr] = mColors[rModel.ReadShort() & 0xFFFF];

            // Tex Coords - these are done a bit differently in DKCR than in the Prime series
            if (mVersion < eReturns)
            {
                // Tex0
                if (VtxDesc & eTex0)
                {
                    if ((mFlags & eHasTex1) && (pMat->Options() & CMaterial::eShortTexCoord))
                        Vtx.Tex[0] = mTex1[rModel.ReadShort() & 0xFFFF];
                    else
                        Vtx.Tex[0] = mTex0[rModel.ReadShort() & 0xFFFF];
                }

                // Tex1-7
                for (u32 iTex = 1; iTex < 7; iTex++)
                    if (VtxDesc & (eTex0 << iTex))
                        Vtx.Tex[iTex] = mTex0[rModel.ReadShort() & 0xFFFF];
            }

            else
            {
                // Tex0-7
                for (u32 iTex = 0; iTex < 7; iTex++)
                {
                    if (VtxDesc & (eTex0 << iTex))
                    {
                        if (!mSurfaceUsingTex1)
                            Vtx.Tex[iTex] = mTex0[rModel.ReadShort() & 0xFFFF];
                        else
                            Vtx.Tex[iTex] = mTex1[rModel.ReadShort() & 0xFFFF];
                    }
                }
            }

            Prim.Vertices.push_back(Vtx);
        } // Vertex array end

        // Update vertex/triangle count
        pSurf->VertexCount += VertexCount;

        switch (Prim.Type)
        {
            case eGX_Triangles:
                pSurf->TriangleCount += VertexCount / 3;
                break;
            case eGX_TriangleFan:
            case eGX_TriangleStrip:
                pSurf->TriangleCount += VertexCount - 2;
                break;
        }

        pSurf->Primitives.push_back(Prim);
        Flag = rModel.ReadByte();
    } // Primitive table end

    mpSectionMgr->ToNextSection();
    return pSurf;
}

void CModelLoader::LoadSurfaceHeaderPrime(IInputStream& rModel, SSurface *pSurf)
{
    pSurf->CenterPoint = CVector3f(rModel);
    pSurf->MaterialID = rModel.ReadLong();

    rModel.Seek(0xC, SEEK_CUR);
    u32 ExtraSize = rModel.ReadLong();
    pSurf->ReflectionDirection = CVector3f(rModel);

    if (mVersion >= eEchoesDemo)
        rModel.Seek(0x4, SEEK_CUR); // Skipping unknown values

    bool HasAABox = (ExtraSize >= 0x18); // MREAs have a set of bounding box coordinates here.

    // If this surface has a bounding box, we can just read it here. Otherwise we'll fill it in manually.
    if (HasAABox)
    {
        ExtraSize -= 0x18;
        pSurf->AABox = CAABox(rModel);
    }
    else
        pSurf->AABox = CAABox::skInfinite;

    rModel.Seek(ExtraSize, SEEK_CUR);
    rModel.SeekToBoundary(32);
}

void CModelLoader::LoadSurfaceHeaderDKCR(IInputStream& rModel, SSurface *pSurf)
{
    pSurf->CenterPoint = CVector3f(rModel);
    rModel.Seek(0xE, SEEK_CUR);
    pSurf->MaterialID = (u32) rModel.ReadShort();
    rModel.Seek(0x2, SEEK_CUR);
    mSurfaceUsingTex1 = (rModel.ReadByte() == 1);
    u32 ExtraSize = rModel.ReadByte();

    if (ExtraSize > 0)
    {
        ExtraSize -= 0x18;
        pSurf->AABox = CAABox(rModel);
    }
    else
        pSurf->AABox = CAABox::skInfinite;

    rModel.Seek(ExtraSize, SEEK_CUR);
    rModel.SeekToBoundary(32);
}

SSurface* CModelLoader::LoadAssimpMesh(const aiMesh *pkMesh, CMaterialSet *pSet)
{
    // Create vertex description and assign it to material
    CMaterial *pMat = pSet->MaterialByIndex(pkMesh->mMaterialIndex);
    FVertexDescription Desc = pMat->VtxDesc();

    if (Desc == eNoAttributes)
    {
        if (pkMesh->HasPositions()) Desc |= ePosition;
        if (pkMesh->HasNormals())   Desc |= eNormal;

        for (u32 iUV = 0; iUV < pkMesh->GetNumUVChannels(); iUV++)
            Desc |= (eTex0 << iUV);

        pMat->SetVertexDescription(Desc);

        // TEMP - disable dynamic lighting on geometry with no normals
        if (!pkMesh->HasNormals())
        {
            pMat->SetLightingEnabled(false);
            pMat->Pass(0)->SetColorInputs(eZeroRGB, eOneRGB, eKonstRGB, eZeroRGB);
            pMat->Pass(0)->SetRasSel(eRasColorNull);
        }
    }

    // Create surface
    SSurface *pSurf = new SSurface();
    pSurf->MaterialID = pkMesh->mMaterialIndex;

    if (pkMesh->mNumFaces > 0)
    {
        pSurf->Primitives.resize(1);
        SSurface::SPrimitive& rPrim = pSurf->Primitives[0];

        // Check primitive type on first face
        u32 NumIndices = pkMesh->mFaces[0].mNumIndices;
        if (NumIndices == 1) rPrim.Type = eGX_Points;
        else if (NumIndices == 2) rPrim.Type = eGX_Lines;
        else if (NumIndices == 3) rPrim.Type = eGX_Triangles;

        // Generate bounding box, center point, and reflection projection
        pSurf->CenterPoint = CVector3f::skZero;
        pSurf->ReflectionDirection = CVector3f::skZero;

        for (u32 iVtx = 0; iVtx < pkMesh->mNumVertices; iVtx++)
        {
            aiVector3D AiPos = pkMesh->mVertices[iVtx];
            pSurf->AABox.ExpandBounds(CVector3f(AiPos.x, AiPos.y, AiPos.z));

            if (pkMesh->HasNormals()) {
                aiVector3D aiNrm = pkMesh->mNormals[iVtx];
                pSurf->ReflectionDirection += CVector3f(aiNrm.x, aiNrm.y, aiNrm.z);
            }
        }
        pSurf->CenterPoint = pSurf->AABox.Center();

        if (pkMesh->HasNormals())
            pSurf->ReflectionDirection /= (float) pkMesh->mNumVertices;
        else
            pSurf->ReflectionDirection = CVector3f(1.f, 0.f, 0.f);

        // Set vertex/triangle count
        pSurf->VertexCount = pkMesh->mNumVertices;
        pSurf->TriangleCount = (rPrim.Type == eGX_Triangles ? pkMesh->mNumFaces : 0);

        // Create primitive
        for (u32 iFace = 0; iFace < pkMesh->mNumFaces; iFace++)
        {
            for (u32 iIndex = 0; iIndex < NumIndices; iIndex++)
            {
                u32 Index = pkMesh->mFaces[iFace].mIndices[iIndex];

                // Create vertex and add it to the primitive
                CVertex Vert;
                Vert.ArrayPosition = Index + mNumVertices;

                if (pkMesh->HasPositions())
                {
                    aiVector3D AiPos = pkMesh->mVertices[Index];
                    Vert.Position = CVector3f(AiPos.x, AiPos.y, AiPos.z);
                }

                if (pkMesh->HasNormals())
                {
                    aiVector3D AiNrm = pkMesh->mNormals[Index];
                    Vert.Normal = CVector3f(AiNrm.x, AiNrm.y, AiNrm.z);
                }

                for (u32 iTex = 0; iTex < pkMesh->GetNumUVChannels(); iTex++)
                {
                    aiVector3D AiTex = pkMesh->mTextureCoords[iTex][Index];
                    Vert.Tex[iTex] = CVector2f(AiTex.x, AiTex.y);
                }

                rPrim.Vertices.push_back(Vert);
            }
        }

        mNumVertices += pkMesh->mNumVertices;
    }

    return pSurf;
}

// ************ STATIC ************
CModel* CModelLoader::LoadCMDL(IInputStream& rCMDL)
{
    CModelLoader Loader;

    // CMDL header - same across the three Primes, but different structure in DKCR
    u32 Magic = rCMDL.ReadLong();

    u32 Version, BlockCount, MatSetCount;
    CAABox AABox;

    // 0xDEADBABE - Metroid Prime seres
    if (Magic == 0xDEADBABE)
    {
        Version = rCMDL.ReadLong();
        u32 Flags = rCMDL.ReadLong();
        AABox = CAABox(rCMDL);
        BlockCount = rCMDL.ReadLong();
        MatSetCount = rCMDL.ReadLong();

        if (Flags & 0x1) Loader.mFlags |= eSkinnedModel;
        if (Flags & 0x2) Loader.mFlags |= eShortNormals;
        if (Flags & 0x4) Loader.mFlags |= eHasTex1;
    }

    // 0x9381000A - Donkey Kong Country Returns
    else if (Magic == 0x9381000A)
    {
        Version = Magic & 0xFFFF;
        u32 Flags = rCMDL.ReadLong();
        AABox = CAABox(rCMDL);
        BlockCount = rCMDL.ReadLong();
        MatSetCount = rCMDL.ReadLong();

        // todo: unknown flags
        Loader.mFlags = eShortNormals | eHasTex1;
        if (Flags & 0x10) Loader.mFlags |= eHasVisGroups;
        if (Flags & 0x20) Loader.mFlags |= eShortPositions;

        // Visibility group data
        // Skipping for now - should read in eventually
        if (Flags & 0x10)
        {
            rCMDL.Seek(0x4, SEEK_CUR);
            u32 VisGroupCount = rCMDL.ReadLong();

            for (u32 iVis = 0; iVis < VisGroupCount; iVis++)
            {
                u32 NameLength = rCMDL.ReadLong();
                rCMDL.Seek(NameLength, SEEK_CUR);
            }

            rCMDL.Seek(0x14, SEEK_CUR); // no clue what any of this is!
        }
    }

    else
    {
        Log::FileError(rCMDL.GetSourceString(), "Invalid CMDL magic: " + TString::HexString(Magic));
        return nullptr;
    }

    // The rest is common to all CMDL versions
    Loader.mVersion = GetFormatVersion(Version);

    if (Loader.mVersion == eUnknownVersion)
    {
        Log::FileError(rCMDL.GetSourceString(), "Unsupported CMDL version: " + TString::HexString(Magic, 0));
        return nullptr;
    }

    CModel *pModel = new CModel();
    Loader.mpModel = pModel;
    Loader.mpSectionMgr = new CSectionMgrIn(BlockCount, &rCMDL);
    rCMDL.SeekToBoundary(32);
    Loader.mpSectionMgr->Init();

    // Materials
    Loader.mMaterials.resize(MatSetCount);
    for (u32 iSet = 0; iSet < MatSetCount; iSet++)
    {
        Loader.mMaterials[iSet] = CMaterialLoader::LoadMaterialSet(rCMDL, Loader.mVersion);

        // Toggle skinning on materials
        if (Loader.mFlags.HasAnyFlags(eSkinnedModel))
        {
            for (u32 iMat = 0; iMat < Loader.mMaterials[iSet]->NumMaterials(); iMat++)
            {
                CMaterial *pMat = Loader.mMaterials[iSet]->MaterialByIndex(iMat);
                CMaterial::FMaterialOptions Options = pMat->Options();
                pMat->SetOptions(Options | CMaterial::eSkinningEnabled);
                pMat->SetVertexDescription(pMat->VtxDesc() | FVertexDescription(eBoneIndices | eBoneWeights));
            }
        }

        if (Loader.mVersion < eCorruptionProto)
            Loader.mpSectionMgr->ToNextSection();
    }

    pModel->mMaterialSets = Loader.mMaterials;
    pModel->mHasOwnMaterials = true;
    if (Loader.mVersion >= eCorruptionProto) Loader.mpSectionMgr->ToNextSection();

    // Mesh
    Loader.LoadAttribArrays(rCMDL);
    Loader.LoadSurfaceOffsets(rCMDL);
    pModel->mSurfaces.reserve(Loader.mSurfaceCount);

    for (u32 iSurf = 0; iSurf < Loader.mSurfaceCount; iSurf++)
    {
        SSurface *pSurf = Loader.LoadSurface(rCMDL);
        pModel->mSurfaces.push_back(pSurf);
        pModel->mVertexCount += pSurf->VertexCount;
        pModel->mTriangleCount += pSurf->TriangleCount;
    }

    pModel->mAABox = AABox;
    pModel->mHasOwnSurfaces = true;

    // Cleanup
    delete Loader.mpSectionMgr;
    return pModel;
}

CModel* CModelLoader::LoadWorldModel(IInputStream& rMREA, CSectionMgrIn& rBlockMgr, CMaterialSet& rMatSet, EGame Version)
{
    CModelLoader Loader;
    Loader.mpSectionMgr = &rBlockMgr;
    Loader.mVersion = Version;
    Loader.mFlags = eShortNormals;
    if (Version != eCorruptionProto) Loader.mFlags |= eHasTex1;
    Loader.mMaterials.resize(1);
    Loader.mMaterials[0] = &rMatSet;

    Loader.LoadWorldMeshHeader(rMREA);
    Loader.LoadAttribArrays(rMREA);
    Loader.LoadSurfaceOffsets(rMREA);

    CModel *pModel = new CModel();
    pModel->mMaterialSets.resize(1);
    pModel->mMaterialSets[0] = &rMatSet;
    pModel->mHasOwnMaterials = false;
    pModel->mSurfaces.reserve(Loader.mSurfaceCount);
    pModel->mHasOwnSurfaces = true;

    for (u32 iSurf = 0; iSurf < Loader.mSurfaceCount; iSurf++)
    {
        SSurface *pSurf = Loader.LoadSurface(rMREA);
        pModel->mSurfaces.push_back(pSurf);
        pModel->mVertexCount += pSurf->VertexCount;
        pModel->mTriangleCount += pSurf->TriangleCount;
    }

    pModel->mAABox = Loader.mAABox;
    return pModel;
}

CModel* CModelLoader::LoadCorruptionWorldModel(IInputStream& rMREA, CSectionMgrIn& rBlockMgr, CMaterialSet& rMatSet, u32 HeaderSecNum, u32 GPUSecNum, EGame Version)
{
    CModelLoader Loader;
    Loader.mpSectionMgr = &rBlockMgr;
    Loader.mVersion = Version;
    Loader.mFlags = eShortNormals;
    Loader.mMaterials.resize(1);
    Loader.mMaterials[0] = &rMatSet;
    if (Version == eReturns) Loader.mFlags |= eHasTex1;

    // Corruption/DKCR MREAs split the mesh header and surface offsets away from the actual geometry data so I need two section numbers to read it
    rBlockMgr.ToSection(HeaderSecNum);
    Loader.LoadWorldMeshHeader(rMREA);
    Loader.LoadSurfaceOffsets(rMREA);
    rBlockMgr.ToSection(GPUSecNum);
    Loader.LoadAttribArrays(rMREA);

    CModel *pModel = new CModel();
    pModel->mMaterialSets.resize(1);
    pModel->mMaterialSets[0] = &rMatSet;
    pModel->mHasOwnMaterials = false;
    pModel->mSurfaces.reserve(Loader.mSurfaceCount);
    pModel->mHasOwnSurfaces = true;

    for (u32 iSurf = 0; iSurf < Loader.mSurfaceCount; iSurf++)
    {
        SSurface *pSurf = Loader.LoadSurface(rMREA);
        pModel->mSurfaces.push_back(pSurf);
        pModel->mVertexCount += pSurf->VertexCount;
        pModel->mTriangleCount += pSurf->TriangleCount;
    }

    pModel->mAABox = Loader.mAABox;
    return pModel;
}

void CModelLoader::BuildWorldMeshes(const std::vector<CModel*>& rkIn, std::vector<CModel*>& rOut, bool DeleteInputModels)
{
    // This function takes the gigantic models with all surfaces combined from MP2/3/DKCR and splits the surfaces to reform the original uncombined meshes.
    std::map<u32, CModel*> OutputMap;

    for (u32 iMdl = 0; iMdl < rkIn.size(); iMdl++)
    {
        CModel *pModel = rkIn[iMdl];
        pModel->mHasOwnSurfaces = false;
        pModel->mHasOwnMaterials = false;

        for (u32 iSurf = 0; iSurf < pModel->mSurfaces.size(); iSurf++)
        {
            SSurface *pSurf = pModel->mSurfaces[iSurf];
            u32 ID = (u32) pSurf->MeshID;
            auto Iter = OutputMap.find(ID);

            // No model for this ID; create one!
            if (Iter == OutputMap.end())
            {
                CModel *pOutMdl = new CModel();
                pOutMdl->mMaterialSets.resize(1);
                pOutMdl->mMaterialSets[0] = pModel->mMaterialSets[0];
                pOutMdl->mHasOwnMaterials = false;
                pOutMdl->mSurfaces.push_back(pSurf);
                pOutMdl->mHasOwnSurfaces = true;
                pOutMdl->mVertexCount = pSurf->VertexCount;
                pOutMdl->mTriangleCount = pSurf->TriangleCount;
                pOutMdl->mAABox.ExpandBounds(pSurf->AABox);

                OutputMap[ID] = pOutMdl;
                rOut.push_back(pOutMdl);
            }

            // Existing model; add this surface to it
            else
            {
                CModel *pOutMdl = Iter->second;
                pOutMdl->mSurfaces.push_back(pSurf);
                pOutMdl->mVertexCount += pSurf->VertexCount;
                pOutMdl->mTriangleCount += pSurf->TriangleCount;
                pOutMdl->mAABox.ExpandBounds(pSurf->AABox);
            }
        }

        // Done with this model, should we delete it?
        if (DeleteInputModels)
            delete pModel;
    }
}

CModel* CModelLoader::ImportAssimpNode(const aiNode *pkNode, const aiScene *pkScene, CMaterialSet& rMatSet)
{
    CModelLoader Loader;
    Loader.mpModel = new CModel(&rMatSet, true);
    Loader.mpModel->mSurfaces.reserve(pkNode->mNumMeshes);

    for (u32 iMesh = 0; iMesh < pkNode->mNumMeshes; iMesh++)
    {
        u32 MeshIndex = pkNode->mMeshes[iMesh];
        const aiMesh *pkMesh = pkScene->mMeshes[MeshIndex];
        SSurface *pSurf = Loader.LoadAssimpMesh(pkMesh, &rMatSet);

        Loader.mpModel->mSurfaces.push_back(pSurf);
        Loader.mpModel->mAABox.ExpandBounds(pSurf->AABox);
        Loader.mpModel->mVertexCount += pSurf->VertexCount;
        Loader.mpModel->mTriangleCount += pSurf->TriangleCount;
    }

    return Loader.mpModel;
}

EGame CModelLoader::GetFormatVersion(u32 Version)
{
    switch (Version)
    {
        case 0x2: return ePrime;
        case 0x3: return eEchoesDemo;
        case 0x4: return eEchoes;
        case 0x5: return eCorruption;
        case 0xA: return eReturns;
        default: return eUnknownVersion;
    }
}
