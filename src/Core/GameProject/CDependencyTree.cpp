#include "CDependencyTree.h"
#include "Core/GameProject/CGameProject.h"
#include "Core/Resource/Animation/CAnimSet.h"
#include "Core/Resource/Script/CMasterTemplate.h"
#include "Core/Resource/Script/CScriptLayer.h"
#include "Core/Resource/Script/CScriptObject.h"

// ************ IDependencyNode ************
IDependencyNode::~IDependencyNode()
{
    for (u32 iChild = 0; iChild < mChildren.size(); iChild++)
        delete mChildren[iChild];
}

bool IDependencyNode::HasDependency(const CAssetID& rkID) const
{
    for (u32 iChild = 0; iChild < mChildren.size(); iChild++)
    {
        if (mChildren[iChild]->HasDependency(rkID))
            return true;
    }

    return false;
}

void IDependencyNode::GetAllResourceReferences(std::set<CAssetID>& rOutSet) const
{
    for (u32 iChild = 0; iChild < mChildren.size(); iChild++)
        mChildren[iChild]->GetAllResourceReferences(rOutSet);
}

// Serialization constructor
IDependencyNode* IDependencyNode::ArchiveConstructor(EDependencyNodeType Type)
{
    switch (Type)
    {
    case eDNT_DependencyTree:       return new CDependencyTree;
    case eDNT_ResourceDependency:   return new CResourceDependency;
    case eDNT_ScriptInstance:       return new CScriptInstanceDependency;
    case eDNT_ScriptProperty:       return new CPropertyDependency;
    case eDNT_CharacterProperty:    return new CCharPropertyDependency;
    case eDNT_SetCharacter:         return new CSetCharacterDependency;
    case eDNT_SetAnimation:         return new CSetAnimationDependency;
    case eDNT_AnimEvent:            return new CAnimEventDependency;
    case eDNT_Area:                 return new CAreaDependencyTree;
    default:                        ASSERT(false); return nullptr;
    }
}

// ************ CDependencyTree ************
EDependencyNodeType CDependencyTree::Type() const
{
    return eDNT_DependencyTree;
}

void CDependencyTree::Serialize(IArchive& rArc)
{
    rArc << SerialParameter("Children", mChildren);
}

void CDependencyTree::AddChild(IDependencyNode *pNode)
{
    ASSERT(pNode);
    mChildren.push_back(pNode);
}

void CDependencyTree::AddDependency(const CAssetID& rkID, bool AvoidDuplicates /*= true*/)
{
    if (!rkID.IsValid() || (AvoidDuplicates && HasDependency(rkID))) return;
    CResourceDependency *pDepend = new CResourceDependency(rkID);
    mChildren.push_back(pDepend);
}

void CDependencyTree::AddDependency(CResource *pRes, bool AvoidDuplicates /*= true*/)
{
    if (!pRes) return;
    AddDependency(pRes->ID(), AvoidDuplicates);
}

void CDependencyTree::AddCharacterDependency(const CAnimationParameters& rkAnimParams)
{
    // This is for formats other than MREA that use AnimationParameters (such as SCAN).
    CAnimSet *pSet = rkAnimParams.AnimSet();
    if (!pSet || rkAnimParams.CharacterIndex() == -1) return;
    CCharPropertyDependency *pChar = new CCharPropertyDependency("NULL", pSet->ID(), rkAnimParams.CharacterIndex());
    mChildren.push_back(pChar);
}

// ************ CResourceDependency ************
EDependencyNodeType CResourceDependency::Type() const
{
    return eDNT_ResourceDependency;
}

void CResourceDependency::Serialize(IArchive& rArc)
{
    rArc << SerialParameter("ID", mID);
}

void CResourceDependency::GetAllResourceReferences(std::set<CAssetID>& rOutSet) const
{
    rOutSet.insert(mID);
}

bool CResourceDependency::HasDependency(const CAssetID& rkID) const
{
    return (mID == rkID);
}

// ************ CPropertyDependency ************
EDependencyNodeType CPropertyDependency::Type() const
{
    return eDNT_ScriptProperty;
}

void CPropertyDependency::Serialize(IArchive& rArc)
{
    rArc << SerialParameter("PropertyID", mIDString);
    CResourceDependency::Serialize(rArc);
}

// ************ CCharacterPropertyDependency ************
EDependencyNodeType CCharPropertyDependency::Type() const
{
    return eDNT_CharacterProperty;
}

void CCharPropertyDependency::Serialize(IArchive& rArc)
{
    CPropertyDependency::Serialize(rArc);
    rArc << SerialParameter("CharIndex", mUsedChar);
}

// ************ CScriptInstanceDependency ************
EDependencyNodeType CScriptInstanceDependency::Type() const
{
    return eDNT_ScriptInstance;
}

void CScriptInstanceDependency::Serialize(IArchive& rArc)
{
    rArc << SerialParameter("ObjectType", mObjectType)
         << SerialParameter("Properties", mChildren);
}

// Static
CScriptInstanceDependency* CScriptInstanceDependency::BuildTree(CScriptObject *pInstance)
{
    CScriptInstanceDependency *pInst = new CScriptInstanceDependency();
    pInst->mObjectType = pInstance->ObjectTypeID();
    ParseStructDependencies(pInst, pInstance, pInstance->Template()->Properties());
    return pInst;
}

void CScriptInstanceDependency::ParseStructDependencies(CScriptInstanceDependency* pInst, CScriptObject* pInstance, CStructProperty *pStruct)
{
    // Recursive function for parsing script dependencies and loading them into the script instance dependency
    void* pPropertyData = pInstance->PropertyData();

    for (u32 PropertyIdx = 0; PropertyIdx < pStruct->NumChildren(); PropertyIdx++)
    {
        IProperty *pProp = pStruct->ChildByIndex(PropertyIdx);
        EPropertyType Type = pProp->Type();

        // Technically we aren't parsing array children, but it's not really worth refactoring this function
        // to support it when there aren't any array properties that contain any asset references anyway...
        if (Type == EPropertyType::Struct)
            ParseStructDependencies(pInst, pInstance, TPropCast<CStructProperty>(pProp));

        else if (Type == EPropertyType::Sound)
        {
            u32 SoundID = TPropCast<CSoundProperty>(pProp)->Value(pPropertyData);

            if (SoundID != -1)
            {
                CGameProject *pProj = pInstance->Area()->Entry()->Project();
                SSoundInfo Info = pProj->AudioManager()->GetSoundInfo(SoundID);

                if (Info.pAudioGroup)
                {
                    CPropertyDependency *pDep = new CPropertyDependency(pProp->IDString(true), Info.pAudioGroup->ID());
                    pInst->mChildren.push_back(pDep);
                }
            }
        }

        else if (Type == EPropertyType::Asset)
        {
            CAssetID ID = TPropCast<CAssetProperty>(pProp)->Value(pPropertyData);

            if (ID.IsValid())
            {
                CPropertyDependency *pDep = new CPropertyDependency(pProp->IDString(true), ID);
                pInst->mChildren.push_back(pDep);
            }
        }

        else if (Type == EPropertyType::AnimationSet)
        {
            CAnimationParameters Params = TPropCast<CAnimationSetProperty>(pProp)->Value(pPropertyData);
            CAssetID ID = Params.ID();

            if (ID.IsValid())
            {
                // Character sets are removed starting in MP3, so we only need char property dependencies in Echoes and earlier
                if (pStruct->Game() <= eEchoes)
                {
                    CCharPropertyDependency *pDep = new CCharPropertyDependency(pProp->IDString(true), ID, Params.CharacterIndex());
                    pInst->mChildren.push_back(pDep);
                }
                else
                {
                    CPropertyDependency *pDep = new CPropertyDependency(pProp->IDString(true), ID);
                    pInst->mChildren.push_back(pDep);
                }
            }
        }
    }
}

// ************ CSetCharacterDependency ************
EDependencyNodeType CSetCharacterDependency::Type() const
{
    return eDNT_SetCharacter;
}

void CSetCharacterDependency::Serialize(IArchive& rArc)
{
    rArc << SerialParameter("CharSetIndex", mCharSetIndex)
         << SerialParameter("Children", mChildren);
}

CSetCharacterDependency* CSetCharacterDependency::BuildTree(const SSetCharacter& rkChar)
{
    CSetCharacterDependency *pTree = new CSetCharacterDependency(rkChar.ID);
    pTree->AddDependency(rkChar.pModel);
    pTree->AddDependency(rkChar.pSkeleton);
    pTree->AddDependency(rkChar.pSkin);
    pTree->AddDependency(rkChar.AnimDataID);
    pTree->AddDependency(rkChar.CollisionPrimitivesID);

    const std::vector<CAssetID> *pkParticleVectors[5] = {
        &rkChar.GenericParticles, &rkChar.ElectricParticles,
        &rkChar.SwooshParticles, &rkChar.SpawnParticles,
        &rkChar.EffectParticles
    };

    for (u32 iVec = 0; iVec < 5; iVec++)
    {
        for (u32 iPart = 0; iPart < pkParticleVectors[iVec]->size(); iPart++)
            pTree->AddDependency(pkParticleVectors[iVec]->at(iPart));
    }

    for (u32 iOverlay = 0; iOverlay < rkChar.OverlayModels.size(); iOverlay++)
    {
        const SOverlayModel& rkOverlay = rkChar.OverlayModels[iOverlay];
        pTree->AddDependency(rkOverlay.ModelID);
        pTree->AddDependency(rkOverlay.SkinID);
    }

    pTree->AddDependency(rkChar.SpatialPrimitives);

    return pTree;
}

// ************ CSetAnimationDependency ************
EDependencyNodeType CSetAnimationDependency::Type() const
{
    return eDNT_SetAnimation;
}

void CSetAnimationDependency::Serialize(IArchive& rArc)
{
    rArc << SerialParameter("CharacterIndices", mCharacterIndices)
         << SerialParameter("Children", mChildren);
}

CSetAnimationDependency* CSetAnimationDependency::BuildTree(const CAnimSet *pkOwnerSet, u32 AnimIndex)
{
    CSetAnimationDependency *pTree = new CSetAnimationDependency;
    const SAnimation *pkAnim = pkOwnerSet->Animation(AnimIndex);

    // Find relevant character indices
    for (u32 iChar = 0; iChar < pkOwnerSet->NumCharacters(); iChar++)
    {
        const SSetCharacter *pkChar = pkOwnerSet->Character(iChar);

        if ( pkChar->UsedAnimationIndices.find(AnimIndex) != pkChar->UsedAnimationIndices.end() )
            pTree->mCharacterIndices.insert(iChar);
    }

    // Add primitive dependencies. In MP2 animation event data is not a standalone resource.
    std::set<CAnimPrimitive> UsedPrimitives;
    pkAnim->pMetaAnim->GetUniquePrimitives(UsedPrimitives);

    for (auto Iter = UsedPrimitives.begin(); Iter != UsedPrimitives.end(); Iter++)
    {
        const CAnimPrimitive& rkPrim = *Iter;
        pTree->AddDependency(rkPrim.Animation());

        if (pkOwnerSet->Game() >= eEchoesDemo)
        {
            CAnimEventData *pEvents = pkOwnerSet->AnimationEventData(rkPrim.ID());
            ASSERT(pEvents && !pEvents->Entry());
            pEvents->AddDependenciesToTree(pTree);
        }
    }

    return pTree;
}

// ************ CAnimEventDependency ************
EDependencyNodeType CAnimEventDependency::Type() const
{
    return eDNT_AnimEvent;
}

void CAnimEventDependency::Serialize(IArchive& rArc)
{
    CResourceDependency::Serialize(rArc);
    rArc << SerialParameter("CharacterIndex", mCharIndex);
}

// ************ CAreaDependencyTree ************
EDependencyNodeType CAreaDependencyTree::Type() const
{
    return eDNT_Area;
}

void CAreaDependencyTree::Serialize(IArchive& rArc)
{
    CDependencyTree::Serialize(rArc);
    rArc << SerialParameter("LayerOffsets", mLayerOffsets);
}

void CAreaDependencyTree::AddScriptLayer(CScriptLayer *pLayer, const std::vector<CAssetID>& rkExtraDeps)
{
    if (!pLayer) return;
    mLayerOffsets.push_back(mChildren.size());
    std::set<CAssetID> UsedIDs;

    for (u32 iInst = 0; iInst < pLayer->NumInstances(); iInst++)
    {
        CScriptInstanceDependency *pTree = CScriptInstanceDependency::BuildTree( pLayer->InstanceByIndex(iInst) );
        ASSERT(pTree != nullptr);

        // Note: MP2+ need to track all instances (not just instances with dependencies) to be able to build the layer module list
        if (pTree->NumChildren() > 0 || pLayer->Area()->Game() >= eEchoesDemo)
        {
            mChildren.push_back(pTree);
            pTree->GetAllResourceReferences(UsedIDs);
        }
        else
            delete pTree;
    }

    for (u32 iDep = 0; iDep < rkExtraDeps.size(); iDep++)
        AddDependency(rkExtraDeps[iDep]);
}

void CAreaDependencyTree::GetModuleDependencies(EGame Game, std::vector<TString>& rModuleDepsOut, std::vector<u32>& rModuleLayerOffsetsOut) const
{
    CMasterTemplate *pMaster = CMasterTemplate::MasterForGame(Game);

    // Output module list will be split per-script layer
    // The output offset list contains two offsets per layer - start index and end index
    for (u32 iLayer = 0; iLayer < mLayerOffsets.size(); iLayer++)
    {
        u32 StartIdx = mLayerOffsets[iLayer];
        u32 EndIdx = (iLayer == mLayerOffsets.size() - 1 ? mChildren.size() : mLayerOffsets[iLayer + 1]);

        u32 ModuleStartIdx = rModuleDepsOut.size();
        rModuleLayerOffsetsOut.push_back(ModuleStartIdx);

        // Keep track of which types we've already checked on this layer to speed things up a little...
        std::set<u32> UsedObjectTypes;

        for (u32 iInst = StartIdx; iInst < EndIdx; iInst++)
        {
            IDependencyNode *pNode = mChildren[iInst];
            if (pNode->Type() != eDNT_ScriptInstance) continue;

            CScriptInstanceDependency *pInst = static_cast<CScriptInstanceDependency*>(pNode);
            u32 ObjType = pInst->ObjectType();

            if (UsedObjectTypes.find(ObjType) == UsedObjectTypes.end())
            {
                // Get the module list for this object type and check whether any of them are new before adding them to the output list
                CScriptTemplate *pTemplate = pMaster->TemplateByID(ObjType);
                const std::vector<TString>& rkModules = pTemplate->RequiredModules();

                for (u32 iMod = 0; iMod < rkModules.size(); iMod++)
                {
                    TString ModuleName = rkModules[iMod];
                    bool NewModule = true;

                    for (u32 iUsed = ModuleStartIdx; iUsed < rModuleDepsOut.size(); iUsed++)
                    {
                        if (rModuleDepsOut[iUsed] == ModuleName)
                        {
                            NewModule = false;
                            break;
                        }
                    }

                    if (NewModule)
                        rModuleDepsOut.push_back(ModuleName);
                }

                UsedObjectTypes.insert(ObjType);
            }
        }

        rModuleLayerOffsetsOut.push_back(rModuleDepsOut.size());
    }
}
