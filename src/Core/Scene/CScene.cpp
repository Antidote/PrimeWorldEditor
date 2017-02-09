#include "CScene.h"
#include "CSceneIterator.h"
#include "Core/Render/CGraphics.h"
#include "Core/Resource/CPoiToWorld.h"
#include "Core/Resource/Script/CScriptLayer.h"
#include "Core/CRayCollisionTester.h"

#include <FileIO/CFileInStream.h>
#include <Common/TString.h>
#include <Math/CRay.h>

#include <list>
#include <string>

CScene::CScene()
    : mSplitTerrain(true)
    , mRanPostLoad(false)
    , mNumNodes(0)
    , mpSceneRootNode(new CRootNode(this, -1, nullptr))
    , mpArea(nullptr)
    , mpWorld(nullptr)
    , mpAreaRootNode(nullptr)
{
}

CScene::~CScene()
{
    ClearScene();
    delete mpSceneRootNode;
}

bool CScene::IsNodeIDUsed(u32 ID) const
{
    return (mNodeMap.find(ID) != mNodeMap.end());
}

u32 CScene::CreateNodeID(u32 SuggestedID /*= -1*/) const
{
    if (SuggestedID != -1)
    {
        if (IsNodeIDUsed(SuggestedID))
            Log::Error("Suggested node ID is already being used! New ID will be created.");
        else
            return SuggestedID;
    }

    u32 ID = 0;

    while (IsNodeIDUsed(ID))
        ID++;

    return ID;
}

CModelNode* CScene::CreateModelNode(CModel *pModel, u32 NodeID /*= -1*/)
{
    if (pModel == nullptr) return nullptr;

    u32 ID = CreateNodeID(NodeID);
    CModelNode *pNode = new CModelNode(this, ID, mpAreaRootNode, pModel);
    mNodes[eModelNode].push_back(pNode);
    mNodeMap[ID] = pNode;
    mNumNodes++;
    return pNode;
}

CStaticNode* CScene::CreateStaticNode(CStaticModel *pModel, u32 NodeID /*= -1*/)
{
    if (pModel == nullptr) return nullptr;

    u32 ID = CreateNodeID(NodeID);
    CStaticNode *pNode = new CStaticNode(this, ID, mpAreaRootNode, pModel);
    mNodes[eStaticNode].push_back(pNode);
    mNodeMap[ID] = pNode;
    mNumNodes++;
    return pNode;
}

CCollisionNode* CScene::CreateCollisionNode(CCollisionMeshGroup *pMesh, u32 NodeID /*= -1*/)
{
    if (pMesh == nullptr) return nullptr;

    u32 ID = CreateNodeID(NodeID);
    CCollisionNode *pNode = new CCollisionNode(this, ID, mpAreaRootNode, pMesh);
    mNodes[eCollisionNode].push_back(pNode);
    mNodeMap[ID] = pNode;
    mNumNodes++;
    return pNode;
}

CScriptNode* CScene::CreateScriptNode(CScriptObject *pObj, u32 NodeID /*= -1*/)
{
    if (pObj == nullptr) return nullptr;

    u32 ID = CreateNodeID(NodeID);
    u32 InstanceID = pObj->InstanceID();

    CScriptNode *pNode = new CScriptNode(this, ID, mpAreaRootNode, pObj);
    mNodes[eScriptNode].push_back(pNode);
    mNodeMap[ID] = pNode;
    mScriptMap[InstanceID] = pNode;
    pNode->BuildLightList(mpArea);

    // AreaAttributes check
    switch (pObj->ObjectTypeID())
    {
    case 0x4E:       // MP1 AreaAttributes ID
    case 0x52454141: // MP2/MP3/DKCR AreaAttributes ID ("REAA")
        mAreaAttributesObjects.emplace_back( CAreaAttributes(pObj) );
        break;
    }

    mNumNodes++;
    return pNode;
}

CLightNode* CScene::CreateLightNode(CLight *pLight, u32 NodeID /*= -1*/)
{
    if (pLight == nullptr) return nullptr;

    u32 ID = CreateNodeID(NodeID);
    CLightNode *pNode = new CLightNode(this, ID, mpAreaRootNode, pLight);
    mNodes[eLightNode].push_back(pNode);
    mNodeMap[ID] = pNode;
    mNumNodes++;
    return pNode;
}

void CScene::DeleteNode(CSceneNode *pNode)
{
    ENodeType Type = pNode->NodeType();

    for (auto it = mNodes[Type].begin(); it != mNodes[Type].end(); it++)
    {
        if (*it == pNode)
        {
            mNodes[Type].erase(it);
            break;
        }
    }

    auto MapIt = mNodeMap.find(pNode->ID());
    if (MapIt != mNodeMap.end())
        mNodeMap.erase(MapIt);

    if (Type == eScriptNode)
    {
        CScriptNode *pScript = static_cast<CScriptNode*>(pNode);

        auto it = mScriptMap.find(pScript->Instance()->InstanceID());
        if (it != mScriptMap.end())
            mScriptMap.erase(it);

        switch (pScript->Instance()->ObjectTypeID())
        {
        case 0x4E:
        case 0x52454141:
            for (auto it = mAreaAttributesObjects.begin(); it != mAreaAttributesObjects.end(); it++)
            {
                if ((*it).Instance() == pScript->Instance())
                {
                    mAreaAttributesObjects.erase(it);
                    break;
                }
            }
            break;
        }
    }

    pNode->Unparent();
    delete pNode;
    mNumNodes--;
}

void CScene::SetActiveArea(CGameArea *pArea)
{
    // Clear existing area
    ClearScene();

    // Create nodes for new area
    mpArea = pArea;
    mpAreaRootNode = new CRootNode(this, -1, mpSceneRootNode);

    // Create static nodes
    u32 Count = mpArea->NumStaticModels();

    for (u32 iMdl = 0; iMdl < Count; iMdl++)
    {
        CStaticNode *pNode = CreateStaticNode(mpArea->StaticModel(iMdl));
        pNode->SetName("Static World Model " + TString::FromInt32(iMdl, 0, 10));
    }

    // Create model nodes
    Count = mpArea->NumWorldModels();

    for (u32 iMdl = 0; iMdl < Count; iMdl++)
    {
        CModel *pModel = mpArea->TerrainModel(iMdl);
        CModelNode *pNode = CreateModelNode(pModel);
        pNode->SetName("World Model " + TString::FromInt32(iMdl, 0, 10));
        pNode->SetWorldModel(true);
    }

    CreateCollisionNode(mpArea->Collision());

    u32 NumLayers = mpArea->NumScriptLayers();

    for (u32 iLyr = 0; iLyr < NumLayers; iLyr++)
    {
        CScriptLayer *pLayer = mpArea->ScriptLayer(iLyr);
        u32 NumObjects = pLayer->NumInstances();
        mNodes[eScriptNode].reserve(mNodes[eScriptNode].size() + NumObjects);

        for (u32 iObj = 0; iObj < NumObjects; iObj++)
        {
            CScriptObject *pObj = pLayer->InstanceByIndex(iObj);
            CreateScriptNode(pObj);
        }
    }

    // Ensure script nodes have valid positions + build light lists
    for (CSceneIterator It(this, eScriptNode, true); It; ++It)
    {
        CScriptNode *pScript = static_cast<CScriptNode*>(*It);
        pScript->GeneratePosition();
        pScript->BuildLightList(mpArea);
    }

    u32 NumLightLayers = mpArea->NumLightLayers();
    CGraphics::sAreaAmbientColor = CColor::skBlack;

    for (u32 iLyr = 0; iLyr < NumLightLayers; iLyr++)
    {
        u32 NumLights = mpArea->NumLights(iLyr);

        for (u32 iLit = 0; iLit < NumLights; iLit++)
        {
            CLight *pLight = mpArea->Light(iLyr, iLit);

            if (pLight->Type() == eLocalAmbient)
                CGraphics::sAreaAmbientColor += pLight->Color();

            CreateLightNode(pLight);
        }
    }

    mRanPostLoad = false;
    Log::Write( TString::FromInt32(CSceneNode::NumNodes(), 0, 10) + " nodes" );
}

void CScene::SetActiveWorld(CWorld* pWorld)
{
    mpWorld = pWorld;
}

void CScene::PostLoad()
{
    mpSceneRootNode->OnLoadFinished();
    mRanPostLoad = true;
}

void CScene::ClearScene()
{
    if (mpAreaRootNode)
    {
        mpAreaRootNode->Unparent();
        delete mpAreaRootNode;
    }

    mNodes.clear();
    mAreaAttributesObjects.clear();
    mNodeMap.clear();
    mScriptMap.clear();
    mNumNodes = 0;

    mpArea = nullptr;
}

void CScene::AddSceneToRenderer(CRenderer *pRenderer, const SViewInfo& rkViewInfo)
{
    // Call PostLoad the first time the scene is rendered to ensure the OpenGL context has been created before it runs.
    if (!mRanPostLoad)
        PostLoad();

    // Override show flags in game mode
    FShowFlags ShowFlags = (rkViewInfo.GameMode ? gkGameModeShowFlags : rkViewInfo.ShowFlags);
    FNodeFlags NodeFlags = NodeFlagsForShowFlags(ShowFlags);

    for (CSceneIterator It(this, NodeFlags, false); It; ++It)
    {
        if (rkViewInfo.GameMode || It->IsVisible())
            It->AddToRenderer(pRenderer, rkViewInfo);
    }
}

SRayIntersection CScene::SceneRayCast(const CRay& rkRay, const SViewInfo& rkViewInfo)
{
    FShowFlags ShowFlags = (rkViewInfo.GameMode ? gkGameModeShowFlags : rkViewInfo.ShowFlags);
    FNodeFlags NodeFlags = NodeFlagsForShowFlags(ShowFlags);
    CRayCollisionTester Tester(rkRay);

    for (CSceneIterator It(this, NodeFlags, false); It; ++It)
    {
        if (It->IsVisible())
            It->RayAABoxIntersectTest(Tester, rkViewInfo);
    }

    return Tester.TestNodes(rkViewInfo);
}

CSceneNode* CScene::NodeByID(u32 NodeID)
{
    auto it = mNodeMap.find(NodeID);

    if (it != mNodeMap.end()) return it->second;
    else return nullptr;
}

CScriptNode* CScene::NodeForInstanceID(u32 InstanceID)
{
    auto it = mScriptMap.find(InstanceID);

    if (it != mScriptMap.end()) return it->second;
    else return nullptr;
}

CScriptNode* CScene::NodeForInstance(CScriptObject *pObj)
{
    return (pObj ? NodeForInstanceID(pObj->InstanceID()) : nullptr);
}

CLightNode* CScene::NodeForLight(CLight *pLight)
{
    // Slow. Is there a better way to do this?
    std::vector<CSceneNode*>& rLights = mNodes[eLightNode];

    for (auto it = rLights.begin(); it != rLights.end(); it++)
    {
        CLightNode *pNode = static_cast<CLightNode*>(*it);
        if (pNode->Light() == pLight) return pNode;
    }

    return nullptr;
}

CModel* CScene::ActiveSkybox()
{
    bool SkyEnabled = false;

    for (u32 iAtt = 0; iAtt < mAreaAttributesObjects.size(); iAtt++)
    {
        const CAreaAttributes& rkAttributes = mAreaAttributesObjects[iAtt];
        if (rkAttributes.IsSkyEnabled()) SkyEnabled = true;

        if (rkAttributes.IsLayerEnabled())
        {
            if (rkAttributes.IsSkyEnabled())
            {
                SkyEnabled = true;
                CModel *pSky = rkAttributes.SkyModel();
                if (pSky) return pSky;
            }
        }
    }

    if (SkyEnabled) return mpWorld->DefaultSkybox();
    else return nullptr;
}

CGameArea* CScene::ActiveArea()
{
    return mpArea;
}

// ************ STATIC ************
FShowFlags CScene::ShowFlagsForNodeFlags(FNodeFlags NodeFlags)
{
    FShowFlags Out;
    if (NodeFlags & eModelNode) Out |= eShowSplitWorld;
    if (NodeFlags & eStaticNode) Out |= eShowMergedWorld;
    if (NodeFlags & eScriptNode) Out |= eShowObjects;
    if (NodeFlags & eCollisionNode) Out |= eShowWorldCollision;
    if (NodeFlags & eLightNode) Out |= eShowLights;
    return Out;
}

FNodeFlags CScene::NodeFlagsForShowFlags(FShowFlags ShowFlags)
{
    FNodeFlags Out = eRootNode;
    if (ShowFlags & eShowSplitWorld) Out |= eModelNode;
    if (ShowFlags & eShowMergedWorld) Out |= eStaticNode;
    if (ShowFlags & eShowWorldCollision) Out |= eCollisionNode;
    if (ShowFlags & eShowObjects) Out |= eScriptNode | eScriptExtraNode;
    if (ShowFlags & eShowLights) Out |= eLightNode;
    return Out;
}
