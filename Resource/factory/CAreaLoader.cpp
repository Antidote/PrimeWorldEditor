#include "CAreaLoader.h"
#include "CCollisionLoader.h"
#include "CModelLoader.h"
#include "CMaterialLoader.h"
#include "CScriptLoader.h"
#include <Common/CFourCC.h>
#include <Common/CompressionUtil.h>
#include <iostream>
#include <Core/Log.h>

CAreaLoader::CAreaLoader()
{
    mpMREA = nullptr;
    mHasDecompressedBuffer = false;
    mGeometryBlockNum      = -1;
    mScriptLayerBlockNum   = -1;
    mCollisionBlockNum     = -1;
    mUnknownBlockNum       = -1;
    mLightsBlockNum        = -1;
    mEmptyBlockNum         = -1;
    mPathBlockNum          = -1;
    mOctreeBlockNum        = -1;
    mScriptGeneratorBlockNum          = -1;
    mFFFFBlockNum          = -1;
    mUnknown2BlockNum      = -1;
    mEGMCBlockNum          = -1;
    mBoundingBoxesBlockNum = -1;
    mDependenciesBlockNum  = -1;
    mGPUBlockNum           = -1;
    mPVSBlockNum           = -1;
    mRSOBlockNum           = -1;
}

CAreaLoader::~CAreaLoader()
{
    if (mHasDecompressedBuffer)
    {
        delete mpMREA;
        delete[] mDecmpBuffer;
    }
}

// ************ PRIME ************
void CAreaLoader::ReadHeaderPrime()
{
    Log::FileWrite(mpMREA->GetSourceString(), "Reading MREA header (MP1)");
    mpArea->mTransform = CTransform4f(*mpMREA);
    mNumMeshes = mpMREA->ReadLong();
    u32 mNumBlocks = mpMREA->ReadLong();

    mGeometryBlockNum = mpMREA->ReadLong();
    mScriptLayerBlockNum = mpMREA->ReadLong();
    mCollisionBlockNum = mpMREA->ReadLong();
    mUnknownBlockNum = mpMREA->ReadLong();
    mLightsBlockNum = mpMREA->ReadLong();
    mEmptyBlockNum = mpMREA->ReadLong();
    mPathBlockNum = mpMREA->ReadLong();
    mOctreeBlockNum = mpMREA->ReadLong();

    mBlockMgr = new CBlockMgrIn(mNumBlocks, mpMREA);
    mpMREA->SeekToBoundary(32);
    mBlockMgr->Init();
}

void CAreaLoader::ReadGeometryPrime()
{
    Log::FileWrite(mpMREA->GetSourceString(), "Reading MREA world geometry (MP1/MP2)");
    mBlockMgr->ToBlock(mGeometryBlockNum);

    // Materials
    mpArea->mMaterialSet = CMaterialLoader::LoadMaterialSet(*mpMREA, mVersion);
    mBlockMgr->ToNextBlock();

    // Geometry
    for (u32 m = 0; m < mNumMeshes; m++) {
        std::cout << "\rLoading mesh " << std::dec << m + 1 << "/" << mNumMeshes;

        CModel *pTerrainModel = CModelLoader::LoadWorldModel(*mpMREA, *mBlockMgr, *mpArea->mMaterialSet, mVersion);
        mpArea->AddWorldModel(pTerrainModel);

        if (mVersion >= eEchoes) {
            mBlockMgr->ToNextBlock();
            mBlockMgr->ToNextBlock();
        }
    }
    mpArea->MergeTerrain();
    std::cout << "\n";
}

void CAreaLoader::ReadSCLYPrime()
{
    Log::FileWrite(mpMREA->GetSourceString(), "Reading MREA script layers (MP1)");
    mBlockMgr->ToBlock(mScriptLayerBlockNum);

    CFourCC SCLY(*mpMREA);
    if (SCLY != "SCLY")
    {
        Log::Error(mpMREA->GetSourceString() + " - Invalid SCLY magic: " + SCLY.ToString());
        return;
    }

    mpMREA->Seek(0x4, SEEK_CUR);
    mNumLayers = mpMREA->ReadLong();
    mpArea->mScriptLayers.reserve(mNumLayers);

    std::vector<u32> LayerSizes(mNumLayers);
    for (u32 l = 0; l < mNumLayers; l++)
        LayerSizes[l] = mpMREA->ReadLong();

    for (u32 l = 0; l < mNumLayers; l++)
    {
        u32 next = mpMREA->Tell() + LayerSizes[l];

        CScriptLayer *layer = CScriptLoader::LoadLayer(*mpMREA, mpArea, mVersion);
        if (layer)
            mpArea->mScriptLayers.push_back(layer);

        mpMREA->Seek(next, SEEK_SET);
    }

    SetUpObjects();
}

void CAreaLoader::ReadLightsPrime()
{
    Log::FileWrite(mpMREA->GetSourceString(), "Reading MREA dynamic lights (MP1/MP2)");
    mBlockMgr->ToBlock(mLightsBlockNum);

    u32 babedead = mpMREA->ReadLong();
    if (babedead != 0xbabedead) return;

    mpArea->mLightLayers.resize(2);

    for (u32 ly = 0; ly < 2; ly++)
    {
        u32 NumLights = mpMREA->ReadLong();
        mpArea->mLightLayers[ly].resize(NumLights);

        for (u32 l = 0; l < NumLights; l++)
        {
            ELightType Type = ELightType(mpMREA->ReadLong());
            CVector3f Color(*mpMREA);
            CVector3f Position(*mpMREA);
            CVector3f Direction(*mpMREA);
            float Multiplier = mpMREA->ReadFloat();
            float SpotCutoff = mpMREA->ReadFloat();
            mpMREA->Seek(0x9, SEEK_CUR);
            u32 FalloffType = mpMREA->ReadLong();
            mpMREA->Seek(0x4, SEEK_CUR);

            // Relevant data is read - now we process and form a CLight out of it
            CLight *Light;

            CColor LightColor = CColor(Color.x, Color.y, Color.z, 0.f);
            if (Multiplier < FLT_EPSILON)
                Multiplier = FLT_EPSILON;

            // Local Ambient
            if (Type == eLocalAmbient)
            {
                Color *= Multiplier;

                // Clamp
                if (Color.x > 1.f) Color.x = 1.f;
                if (Color.y > 1.f) Color.y = 1.f;
                if (Color.z > 1.f) Color.z = 1.f;
                CColor MultColor(Color.x, Color.y, Color.z, 1.f);

                Light = CLight::BuildLocalAmbient(Position, MultColor);
            }

            // Directional
            else if (Type == eDirectional)
            {
                Light = CLight::BuildDirectional(Position, Direction, LightColor);
            }

            // Spot
            else if (Type == eSpot)
            {
                Light = CLight::BuildSpot(Position, Direction.Normalized(), LightColor, SpotCutoff);

                float DistAttenA = (FalloffType == 0) ? (2.f / Multiplier) : 0.f;
                float DistAttenB = (FalloffType == 1) ? (250.f / Multiplier) : 0.f;
                float DistAttenC = (FalloffType == 2) ? (25000.f / Multiplier) : 0.f;
                Light->SetDistAtten(DistAttenA, DistAttenB, DistAttenC);
            }

            // Custom
            else
            {
                float DistAttenA = (FalloffType == 0) ? (2.f / Multiplier) : 0.f;
                float DistAttenB = (FalloffType == 1) ? (249.9998f / Multiplier) : 0.f;
                float DistAttenC = (FalloffType == 2) ? (25000.f / Multiplier) : 0.f;

                Light = CLight::BuildCustom(Position, Direction, LightColor,
                                            DistAttenA, DistAttenB, DistAttenC,
                                            1.f, 0.f, 0.f);
            }

            mpArea->mLightLayers[ly][l] = Light;
        }
    }
}

// ************ ECHOES ************
void CAreaLoader::ReadHeaderEchoes()
{
    // This function reads the header for Echoes and the Echoes demo disc
    Log::FileWrite(mpMREA->GetSourceString(), "Reading MREA header (MP2)");
    mpArea->mTransform = CTransform4f(*mpMREA);
    mNumMeshes = mpMREA->ReadLong();
    if (mVersion == eEchoes) mNumLayers = mpMREA->ReadLong();
    u32 numBlocks = mpMREA->ReadLong();

    mGeometryBlockNum = mpMREA->ReadLong();
    mScriptLayerBlockNum = mpMREA->ReadLong();
    mScriptGeneratorBlockNum = mpMREA->ReadLong();
    mCollisionBlockNum = mpMREA->ReadLong();
    mUnknownBlockNum = mpMREA->ReadLong();
    mLightsBlockNum = mpMREA->ReadLong();
    mEmptyBlockNum = mpMREA->ReadLong();
    mPathBlockNum = mpMREA->ReadLong();
    mFFFFBlockNum = mpMREA->ReadLong();
    mUnknown2BlockNum = mpMREA->ReadLong();
    mEGMCBlockNum = mpMREA->ReadLong();
    if (mVersion == eEchoes) mClusters.resize(mpMREA->ReadLong());
    mpMREA->SeekToBoundary(32);

    mBlockMgr = new CBlockMgrIn(numBlocks, mpMREA);
    mpMREA->SeekToBoundary(32);

    if (mVersion == eEchoes)
    {
        ReadCompressedBlocks();
        Decompress();
    }

    mBlockMgr->Init();
}

void CAreaLoader::ReadSCLYEchoes()
{
    Log::FileWrite(mpMREA->GetSourceString(), "Reading MREA script layers (MP2/MP3/DKCR)");
    mBlockMgr->ToBlock(mScriptLayerBlockNum);

    // SCLY
    for (u32 l = 0; l < mNumLayers; l++)
    {
        CScriptLayer *pLayer = CScriptLoader::LoadLayer(*mpMREA, mpArea, mVersion);

        if (pLayer)
            mpArea->mScriptLayers.push_back(pLayer);

        mBlockMgr->ToNextBlock();
    }

    // SCGN
    mBlockMgr->ToBlock(mScriptGeneratorBlockNum);
    CScriptLayer *pLayer = CScriptLoader::LoadLayer(*mpMREA, mpArea, mVersion);

    if (pLayer)
        mpArea->mpGeneratorLayer = pLayer;

    SetUpObjects();
}

// ************ CORRUPTION ************
void CAreaLoader::ReadHeaderCorruption()
{
    // This function reads the header for MP3, the MP3 prototype, and DKCR
    Log::FileWrite(mpMREA->GetSourceString(), "Reading MREA header (MP3/DKCR)");
    mpArea->mTransform = CTransform4f(*mpMREA);
    mNumMeshes = mpMREA->ReadLong();
    mNumLayers = mpMREA->ReadLong();
    u32 NumSections = mpMREA->ReadLong();
    mClusters.resize(mpMREA->ReadLong());
    u32 SectionNumberCount = mpMREA->ReadLong();
    mpMREA->SeekToBoundary(32);

    mBlockMgr = new CBlockMgrIn(NumSections, mpMREA);
    mpMREA->SeekToBoundary(32);

    ReadCompressedBlocks();

    for (u32 iNum = 0; iNum < SectionNumberCount; iNum++)
    {
        CFourCC Type(*mpMREA);
        u32 Num = mpMREA->ReadLong();

        if      (Type == "AABB") mBoundingBoxesBlockNum = Num;
        else if (Type == "COLI") mCollisionBlockNum = Num;
        else if (Type == "DEPS") mDependenciesBlockNum = Num;
        else if (Type == "EGMC") mEGMCBlockNum = Num;
        else if (Type == "GPUD") mGPUBlockNum = Num;
        else if (Type == "LITE") mLightsBlockNum = Num;
        else if (Type == "PFL2") mPathBlockNum = Num;
        else if (Type == "PVS!") mPVSBlockNum = Num;
        else if (Type == "ROCT") mOctreeBlockNum = Num;
        else if (Type == "RSOS") mRSOBlockNum = Num;
        else if (Type == "SOBJ") mScriptLayerBlockNum = Num;
        else if (Type == "SGEN") mScriptGeneratorBlockNum = Num;
        else if (Type == "WOBJ") mGeometryBlockNum = Num; // note WOBJ can show up multiple times, but is always 0
    }

    mpMREA->SeekToBoundary(32);
    Decompress();
    mBlockMgr->Init();
}

void CAreaLoader::ReadGeometryCorruption()
{
    Log::FileWrite(mpMREA->GetSourceString(), "Reading MREA world geometry (MP3)");
    mBlockMgr->ToBlock(mGeometryBlockNum);

    // Materials
    mpArea->mMaterialSet = CMaterialLoader::LoadMaterialSet(*mpMREA, mVersion);
    mBlockMgr->ToNextBlock();

    // Geometry
    u32 CurWOBJSection = 1;
    u32 CurGPUSection = mGPUBlockNum;

    for (u32 iMesh = 0; iMesh < mNumMeshes; iMesh++)
    {
        std::cout << "\rLoading mesh " << std::dec << iMesh + 1 << "/" << mNumMeshes;

        CModel *pWorldModel = CModelLoader::LoadCorruptionWorldModel(*mpMREA, *mBlockMgr, *mpArea->mMaterialSet, CurWOBJSection, CurGPUSection, mVersion);
        mpArea->AddWorldModel(pWorldModel);

        CurWOBJSection += 4;
        CurGPUSection = mBlockMgr->CurrentBlock();
    }

    mpArea->MergeTerrain();
    std::cout << "\n";
}

// ************ RETURNS ************

// ************ COMMON ************
void CAreaLoader::ReadCompressedBlocks()
{
    mTotalDecmpSize = 0;

    for (u32 c = 0; c < mClusters.size(); c++)
    {
        mClusters[c].BufferSize = mpMREA->ReadLong();
        mClusters[c].DecompressedSize = mpMREA->ReadLong();
        mClusters[c].CompressedSize = mpMREA->ReadLong();
        mClusters[c].NumSections = mpMREA->ReadLong();
        mTotalDecmpSize += mClusters[c].DecompressedSize;
    }

    mpMREA->SeekToBoundary(32);
}

void CAreaLoader::Decompress()
{
    // This function decompresses compressed clusters into a buffer.
    // It should be called at the beginning of the first compressed cluster.
    Log::FileWrite(mpMREA->GetSourceString(), "Decompressing MREA data");
    if (mVersion < eEchoes) return;

    // Decompress clusters
    mDecmpBuffer = new u8[mTotalDecmpSize];
    u32 Offset = 0;

    for (u32 c = 0; c < mClusters.size(); c++)
    {
        SCompressedCluster *cc = &mClusters[c];

        // Is it decompressed already?
        if (mClusters[c].CompressedSize == 0)
        {
            mpMREA->ReadBytes(mDecmpBuffer + Offset, cc->DecompressedSize);
            Offset += cc->DecompressedSize;
        }

        else
        {
            u32 StartOffset = 32 - (mClusters[c].CompressedSize % 32); // For some reason they pad the beginning instead of the end
            if (StartOffset != 32)
                mpMREA->Seek(StartOffset, SEEK_CUR);

            std::vector<u8> cmp(mClusters[c].CompressedSize);
            mpMREA->ReadBytes(cmp.data(), cmp.size());

            bool Success = CompressionUtil::DecompressAreaLZO(cmp.data(), cmp.size(), mDecmpBuffer + Offset, cc->DecompressedSize);
            if (!Success)
                throw "Failed to decompress MREA!";

            Offset += cc->DecompressedSize;
        }
    }

    std::string Source = mpMREA->GetSourceString();
    mpMREA = new CMemoryInStream(mDecmpBuffer, mTotalDecmpSize, IOUtil::BigEndian);
    mpMREA->SetSourceString(Source);
    mBlockMgr->SetInputStream(mpMREA);
    mHasDecompressedBuffer = true;
}

void CAreaLoader::ReadCollision()
{
    Log::FileWrite(mpMREA->GetSourceString(), "Reading collision (MP1/MP2/MP3)");
    mBlockMgr->ToBlock(mCollisionBlockNum);
    mpArea->mCollision = CCollisionLoader::LoadAreaCollision(*mpMREA);
}

void CAreaLoader::SetUpObjects()
{
    // Iterate over all objects
    for (u32 iLyr = 0; iLyr < mpArea->GetScriptLayerCount() + 1; iLyr++)
    {
        CScriptLayer *pLayer;
        if (iLyr < mpArea->GetScriptLayerCount()) pLayer = mpArea->mScriptLayers[iLyr];

        else
        {
            pLayer = mpArea->GetGeneratorLayer();
            if (!pLayer) break;
        }

        for (u32 iObj = 0; iObj < pLayer->GetNumObjects(); iObj++)
        {
            // Add object to object map
            CScriptObject *pObj = (*pLayer)[iObj];
            mpArea->mObjectMap[pObj->InstanceID()] = pObj;

            // Store outgoing connections
            for (u32 iCon = 0; iCon < pObj->NumOutLinks(); iCon++)
            {
                SLink Connection = pObj->OutLink(iCon);

                SLink NewConnection;
                NewConnection.State = Connection.State;
                NewConnection.Message = Connection.Message;
                NewConnection.ObjectID = pObj->InstanceID();
                mConnectionMap[Connection.ObjectID].push_back(NewConnection);
            }
        }
    }

    // Store connections
    for (auto it = mpArea->mObjectMap.begin(); it != mpArea->mObjectMap.end(); it++)
    {
        u32 InstanceID = it->first;
        auto iConMap = mConnectionMap.find(InstanceID);

        if (iConMap != mConnectionMap.end())
        {
            CScriptObject *pObj = mpArea->GetInstanceByID(InstanceID);
            pObj->mInConnections = iConMap->second;
        }
    }
}

// ************ STATIC ************
CGameArea* CAreaLoader::LoadMREA(CInputStream& MREA)
{
    CAreaLoader Loader;

    // Validation
    if (!MREA.IsValid()) return nullptr;
    Log::Write("Loading " + MREA.GetSourceString());

    u32 deadbeef = MREA.ReadLong();
    if (deadbeef != 0xdeadbeef)
    {
        Log::FileError(MREA.GetSourceString(), "Invalid MREA magic: " + StringUtil::ToHexString(deadbeef));
        return nullptr;
    }

    // Header
    Loader.mpArea = new CGameArea;
    u32 version = MREA.ReadLong();
    Loader.mVersion = GetFormatVersion(version);
    Loader.mpMREA = &MREA;

    switch (Loader.mVersion)
    {
        case ePrimeDemo:
        case ePrime:
            Loader.ReadHeaderPrime();
            Loader.ReadGeometryPrime();
            Loader.ReadSCLYPrime();
            Loader.ReadCollision();
            Loader.ReadLightsPrime();
            break;
        case eEchoesDemo:
        case eEchoes:
            Loader.ReadHeaderEchoes();
            Loader.ReadGeometryPrime();
            Loader.ReadSCLYEchoes();
            Loader.ReadCollision();
            Loader.ReadLightsPrime();
            break;
        case eCorruptionProto:
            Loader.ReadHeaderCorruption();
            Loader.ReadGeometryPrime();
            Loader.ReadSCLYEchoes();
            Loader.ReadCollision();
            break;
        case eCorruption:
        case eReturns:
            Loader.ReadHeaderCorruption();
            Loader.ReadGeometryCorruption();
            Loader.ReadSCLYEchoes();
            if (Loader.mVersion != eReturns) Loader.ReadCollision();
            break;
        default:
            Log::FileError(MREA.GetSourceString(), "Unsupported MREA version: " + StringUtil::ToHexString(version));
            delete Loader.mpArea;
            return nullptr;
    }

    // Cleanup
    delete Loader.mBlockMgr;
    return Loader.mpArea;
}

EGame CAreaLoader::GetFormatVersion(u32 version)
{
    switch (version)
    {
        case 0xC: return ePrimeDemo;
        case 0xF: return ePrime;
        case 0x15: return eEchoesDemo;
        case 0x19: return eEchoes;
        case 0x1D: return eCorruptionProto;
        case 0x1E: return eCorruption;
        case 0x20: return eReturns;
        default: return eUnknownVersion;
    }
}
