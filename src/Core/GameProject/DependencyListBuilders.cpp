#include "DependencyListBuilders.h"

// ************ CCharacterUsageMap ************
bool CCharacterUsageMap::IsCharacterUsed(const CAssetID& rkID, u32 CharacterIndex) const
{
    auto Find = mUsageMap.find(rkID);
    if (Find == mUsageMap.end()) return false;

    const std::vector<bool>& rkUsageList = Find->second;
    if (CharacterIndex >= rkUsageList.size()) return false;
    else return rkUsageList[CharacterIndex];
}

bool CCharacterUsageMap::IsAnimationUsed(const CAssetID& rkID, CSetAnimationDependency *pAnim) const
{
    auto Find = mUsageMap.find(rkID);
    if (Find == mUsageMap.end()) return false;
    const std::vector<bool>& rkUsageList = Find->second;

    for (u32 iChar = 0; iChar < rkUsageList.size(); iChar++)
    {
        if (rkUsageList[iChar] && pAnim->IsUsedByCharacter(iChar))
            return true;
    }

    return false;
}

void CCharacterUsageMap::FindUsagesForArea(CWorld *pWorld, CResourceEntry *pEntry)
{
    ASSERT(pEntry->ResourceType() == eArea);

    for (u32 iArea = 0; iArea < pWorld->NumAreas(); iArea++)
    {
        if (pWorld->AreaResourceID(iArea) == pEntry->ID())
        {
            FindUsagesForArea(pWorld, iArea);
            return;
        }
    }
}

void CCharacterUsageMap::FindUsagesForArea(CWorld *pWorld, u32 AreaIndex)
{
    // We only need to search forward from this area to other areas that both use the same character(s) + have duplicates enabled
    mUsageMap.clear();
    mStillLookingIDs.clear();
    mLayerIndex = -1;
    mIsInitialArea = true;

    for (u32 iArea = AreaIndex; iArea < pWorld->NumAreas(); iArea++)
    {
        if (!mIsInitialArea && mStillLookingIDs.empty()) break;
        mCurrentAreaAllowsDupes = pWorld->DoesAreaAllowPakDuplicates(iArea);

        CAssetID AreaID = pWorld->AreaResourceID(iArea);
        CResourceEntry *pEntry = gpResourceStore->FindEntry(AreaID);
        ASSERT(pEntry && pEntry->ResourceType() == eArea);

        ParseDependencyNode(pEntry->Dependencies());
        mIsInitialArea = false;
    }
}

void CCharacterUsageMap::FindUsagesForLayer(CResourceEntry *pAreaEntry, u32 LayerIndex)
{
    mUsageMap.clear();
    mStillLookingIDs.clear();
    mLayerIndex = LayerIndex;
    mIsInitialArea = true;

    CAreaDependencyTree *pTree = static_cast<CAreaDependencyTree*>(pAreaEntry->Dependencies());
    ASSERT(pTree->Type() == eDNT_Area);

    // Only examine dependencies of the particular layer specified by the caller
    bool IsLastLayer = (mLayerIndex == pTree->NumScriptLayers() - 1);
    u32 StartIdx = pTree->ScriptLayerOffset(mLayerIndex);
    u32 EndIdx = (IsLastLayer ? pTree->NumChildren() : pTree->ScriptLayerOffset(mLayerIndex + 1));

    for (u32 iInst = StartIdx; iInst < EndIdx; iInst++)
        ParseDependencyNode(pTree->ChildByIndex(iInst));
}

#include "Core/Resource/Animation/CAnimSet.h"

void CCharacterUsageMap::DebugPrintContents()
{
    for (auto Iter = mUsageMap.begin(); Iter != mUsageMap.end(); Iter++)
    {
        CAssetID ID = Iter->first;
        std::vector<bool>& rUsedList = Iter->second;
        CAnimSet *pSet = (CAnimSet*) gpResourceStore->LoadResource(ID, "ANCS");

        for (u32 iChar = 0; iChar < pSet->NumCharacters(); iChar++)
        {
            bool Used = (rUsedList.size() > iChar && rUsedList[iChar]);
            TString CharName = pSet->Character(iChar)->Name;
            Log::Write(ID.ToString() + " : Char " + TString::FromInt32(iChar, 0, 10) + " : " + CharName + " : " + (Used ? "USED" : "UNUSED"));
        }
    }
}

// ************ PROTECTED ************
void CCharacterUsageMap::ParseDependencyNode(IDependencyNode *pNode)
{
    EDependencyNodeType Type = pNode->Type();

    if (Type == eDNT_CharacterProperty)
    {
        CCharPropertyDependency *pDep = static_cast<CCharPropertyDependency*>(pNode);
        CAssetID ResID = pDep->ID();
        auto Find = mUsageMap.find(ResID);

        if (!mIsInitialArea && mStillLookingIDs.find(ResID) == mStillLookingIDs.end())
            return;

        if (Find != mUsageMap.end())
        {
            if (!mIsInitialArea && mCurrentAreaAllowsDupes)
            {
                mStillLookingIDs.erase( mStillLookingIDs.find(ResID) );
                return;
            }
        }

        else
        {
            if (!mIsInitialArea) return;
            mUsageMap[ResID] = std::vector<bool>();
            mStillLookingIDs.insert(ResID);
        }

        std::vector<bool>& rUsageList = mUsageMap[ResID];
        u32 UsedChar = pDep->UsedChar();

        if (rUsageList.size() <= UsedChar)
            rUsageList.resize(UsedChar + 1, false);

        rUsageList[UsedChar] = true;
    }

    // Parse dependencies of the referenced resource if it's a type that can reference animsets
    else if (Type == eDNT_ResourceDependency || Type == eDNT_ScriptProperty)
    {
        CResourceDependency *pDep = static_cast<CResourceDependency*>(pNode);
        CResourceEntry *pEntry = gpResourceStore->FindEntry(pDep->ID());

        if (pEntry)
        {
            EResType ResType = pEntry->ResourceType();

            if (ResType == eScan)
                ParseDependencyNode(pEntry->Dependencies());
        }
    }

    // Look for sub-dependencies of the current node
    else
    {
        for (u32 iChild = 0; iChild < pNode->NumChildren(); iChild++)
            ParseDependencyNode(pNode->ChildByIndex(iChild));
    }
}

// ************ CPackageDependencyListBuilder ************
void CPackageDependencyListBuilder::BuildDependencyList(bool AllowDuplicates, std::list<CAssetID>& rOut)
{
    mEnableDuplicates = AllowDuplicates;

    // Iterate over all resource collections and resources and parse their dependencies
    for (u32 iCol = 0; iCol < mpPackage->NumCollections(); iCol++)
    {
        CResourceCollection *pCollection = mpPackage->CollectionByIndex(iCol);

        for (u32 iRes = 0; iRes < pCollection->NumResources(); iRes++)
        {
            const SNamedResource& rkRes = pCollection->ResourceByIndex(iRes);
            CResourceEntry *pEntry = gpResourceStore->FindEntry(rkRes.ID);
            if (!pEntry) continue;

            if (rkRes.Name.EndsWith("NODEPEND"))
            {
                rOut.push_back(rkRes.ID);
                continue;
            }

            if (rkRes.Type == "MLVL")
            {
                CResourceEntry *pWorldEntry = gpResourceStore->FindEntry(rkRes.ID);
                ASSERT(pWorldEntry);
                mpWorld = (CWorld*) pWorldEntry->Load();
                ASSERT(mpWorld);
            }

            AddDependency(nullptr, rkRes.ID, rOut);
        }
    }
}

void CPackageDependencyListBuilder::AddDependency(CResourceEntry *pCurEntry, const CAssetID& rkID, std::list<CAssetID>& rOut)
{
    if (pCurEntry && pCurEntry->ResourceType() == eDependencyGroup) return;
    CResourceEntry *pEntry = gpResourceStore->FindEntry(rkID);
    if (!pEntry) return;

    EResType ResType = pEntry->ResourceType();

    // Is this entry valid?
    bool IsValid =  ResType != eMidi &&
                   (ResType != eAudioGroup || mGame >= eEchoesDemo) &&
                   (ResType != eWorld || !pCurEntry) &&
                   (ResType != eArea || !pCurEntry || pCurEntry->ResourceType() == eWorld);

    if (!IsValid) return;

    if ( ( mCurrentAreaHasDuplicates && mAreaUsedAssets.find(rkID) != mAreaUsedAssets.end()) ||
         (!mCurrentAreaHasDuplicates && mPackageUsedAssets.find(rkID) != mPackageUsedAssets.end()) )
        return;

    // Entry is valid, parse its sub-dependencies
    mPackageUsedAssets.insert(rkID);
    mAreaUsedAssets.insert(rkID);

    // New area - toggle duplicates and find character usages
    if (ResType == eArea)
    {
        if (mGame <= eEchoes)
            mCharacterUsageMap.FindUsagesForArea(mpWorld, pEntry);

        mAreaUsedAssets.clear();
        mCurrentAreaHasDuplicates = false;

        if (mEnableDuplicates)
        {
            for (u32 iArea = 0; iArea < mpWorld->NumAreas(); iArea++)
            {
                if (mpWorld->AreaResourceID(iArea) == rkID)
                {
                    mCurrentAreaHasDuplicates = mpWorld->DoesAreaAllowPakDuplicates(iArea);
                    break;
                }
            }
        }
    }

    // Animset - keep track of the current animset ID
    else if (ResType == eAnimSet)
        mCurrentAnimSetID = rkID;

    // Evaluate dependencies of this entry
    CDependencyTree *pTree = pEntry->Dependencies();
    EvaluateDependencyNode(pEntry, pTree, rOut);
    rOut.push_back(rkID);

    // Revert current animset ID
    if (ResType == eAnimSet)
        mCurrentAnimSetID = CAssetID::InvalidID(mGame);
}

void CPackageDependencyListBuilder::EvaluateDependencyNode(CResourceEntry *pCurEntry, IDependencyNode *pNode, std::list<CAssetID>& rOut)
{
    EDependencyNodeType Type = pNode->Type();
    bool ParseChildren = false;

    // Straight resource dependencies should just be added to the tree directly
    if (Type == eDNT_ResourceDependency || Type == eDNT_ScriptProperty || Type == eDNT_CharacterProperty)
    {
        CResourceDependency *pDep = static_cast<CResourceDependency*>(pNode);
        AddDependency(pCurEntry, pDep->ID(), rOut);
    }

    // Anim events should be added if either they apply to characters, or their character index is used
    else if (Type == eDNT_AnimEvent)
    {
        CAnimEventDependency *pDep = static_cast<CAnimEventDependency*>(pNode);
        u32 CharIndex = pDep->CharIndex();

        if (CharIndex == -1 || mCharacterUsageMap.IsCharacterUsed(mCurrentAnimSetID, CharIndex))
            AddDependency(pCurEntry, pDep->ID(), rOut);
    }

    // Set characters should only be added if their character index is used
    else if (Type == eDNT_SetCharacter)
    {
        CSetCharacterDependency *pChar = static_cast<CSetCharacterDependency*>(pNode);
        ParseChildren = mCharacterUsageMap.IsCharacterUsed(mCurrentAnimSetID, pChar->CharSetIndex()) || mIsPlayerActor;
    }

    // Set animations should only be added if they're being used by at least one used character
    else if (Type == eDNT_SetAnimation)
    {
        CSetAnimationDependency *pAnim = static_cast<CSetAnimationDependency*>(pNode);
        ParseChildren = mCharacterUsageMap.IsAnimationUsed(mCurrentAnimSetID, pAnim) || (mIsPlayerActor && pAnim->IsUsedByAnyCharacter());
    }

    else
        ParseChildren = true;

    // Analyze this node's children
    if (ParseChildren)
    {
        if (Type == eDNT_ScriptInstance)
        {
            u32 ObjType = static_cast<CScriptInstanceDependency*>(pNode)->ObjectType();
            mIsPlayerActor = (ObjType == 0x4C || ObjType == FOURCC("PLAC"));
        }

        for (u32 iChild = 0; iChild < pNode->NumChildren(); iChild++)
            EvaluateDependencyNode(pCurEntry, pNode->ChildByIndex(iChild), rOut);

        if (Type == eDNT_ScriptInstance)
            mIsPlayerActor = false;
    }
}

// ************ CAreaDependencyListBuilder ************
void CAreaDependencyListBuilder::BuildDependencyList(std::list<CAssetID>& rAssetsOut, std::list<u32>& rLayerOffsetsOut, std::set<CAssetID> *pAudioGroupsOut)
{
    CAreaDependencyTree *pTree = static_cast<CAreaDependencyTree*>(mpAreaEntry->Dependencies());

    // Fill area base used assets set (don't actually add to list yet)
    u32 BaseEndIndex = (pTree->NumScriptLayers() > 0 ? pTree->ScriptLayerOffset(0) : pTree->NumChildren());

    for (u32 iDep = 0; iDep < BaseEndIndex; iDep++)
    {
        CResourceDependency *pRes = static_cast<CResourceDependency*>(pTree->ChildByIndex(iDep));
        ASSERT(pRes->Type() == eDNT_ResourceDependency);
        mBaseUsedAssets.insert(pRes->ID());
    }

    // Get dependencies of each layer
    for (u32 iLyr = 0; iLyr < pTree->NumScriptLayers(); iLyr++)
    {
        mLayerUsedAssets.clear();
        mCharacterUsageMap.FindUsagesForLayer(mpAreaEntry, iLyr);
        rLayerOffsetsOut.push_back(rAssetsOut.size());

        bool IsLastLayer = (iLyr == pTree->NumScriptLayers() - 1);
        u32 StartIdx = pTree->ScriptLayerOffset(iLyr);
        u32 EndIdx = (IsLastLayer ? pTree->NumChildren() : pTree->ScriptLayerOffset(iLyr + 1));

        for (u32 iChild = StartIdx; iChild < EndIdx; iChild++)
        {
            CScriptInstanceDependency *pInst = static_cast<CScriptInstanceDependency*>(pTree->ChildByIndex(iChild));
            ASSERT(pInst->Type() == eDNT_ScriptInstance);
            mIsPlayerActor = (pInst->ObjectType() == 0x4C || pInst->ObjectType() == FOURCC("PLAC"));

            for (u32 iDep = 0; iDep < pInst->NumChildren(); iDep++)
            {
                CPropertyDependency *pDep = static_cast<CPropertyDependency*>(pInst->ChildByIndex(iDep));

                // For MP3, exclude the CMDL/CSKR properties for the suit assets - only include default character assets
                if (mGame == eCorruption && mIsPlayerActor)
                {
                    TString PropID = pDep->PropertyID();

                    if (    PropID == "0x846397A8" || PropID == "0x685A4C01" ||
                            PropID == "0x9834ECC9" || PropID == "0x188B8960" ||
                            PropID == "0x134A81E3" || PropID == "0x4ABF030C" ||
                            PropID == "0x9BF030DC" || PropID == "0x981263D3" ||
                            PropID == "0x8A8D5AA5" || PropID == "0xE4734608" ||
                            PropID == "0x3376814D" || PropID == "0x797CA77E" ||
                            PropID == "0x0EBEC440" || PropID == "0xBC0952D8" ||
                            PropID == "0xA8778E57" || PropID == "0x1CB10DBE"    )
                        continue;
                }

                AddDependency(pDep->ID(), rAssetsOut, pAudioGroupsOut);
            }
        }
    }

    // Add base assets
    mBaseUsedAssets.clear();
    mLayerUsedAssets.clear();
    rLayerOffsetsOut.push_back(rAssetsOut.size());

    for (u32 iDep = 0; iDep < BaseEndIndex; iDep++)
    {
        CResourceDependency *pDep = static_cast<CResourceDependency*>(pTree->ChildByIndex(iDep));
        AddDependency(pDep->ID(), rAssetsOut, pAudioGroupsOut);
    }
}

void CAreaDependencyListBuilder::AddDependency(const CAssetID& rkID, std::list<CAssetID>& rOut, std::set<CAssetID> *pAudioGroupsOut)
{
    CResourceEntry *pEntry = gpResourceStore->FindEntry(rkID);
    if (!pEntry) return;

    EResType ResType = pEntry->ResourceType();

    // If this is an audio group, for MP1, save it in the output set. For MP2, treat audio groups as a normal dependency.
    if (mGame <= ePrime && ResType == eAudioGroup)
    {
        if (pAudioGroupsOut)
            pAudioGroupsOut->insert(rkID);
        return;
    }

    // Check to ensure this is a valid/new dependency
    if (ResType == eWorld || ResType == eArea)
        return;

    if (mBaseUsedAssets.find(rkID) != mBaseUsedAssets.end() || mLayerUsedAssets.find(rkID) != mLayerUsedAssets.end())
        return;

    // Dependency is valid! Evaluate the node tree (except for SCAN and DGRP)
    if (ResType != eScan && ResType != eDependencyGroup)
    {
        if (ResType == eAnimSet)
        {
            ASSERT(!mCurrentAnimSetID.IsValid());
            mCurrentAnimSetID = pEntry->ID();
        }

        EvaluateDependencyNode(pEntry, pEntry->Dependencies(), rOut, pAudioGroupsOut);

        if (ResType == eAnimSet)
        {
            ASSERT(mCurrentAnimSetID.IsValid());
            mCurrentAnimSetID = CAssetID::InvalidID(mGame);
        }
    }

    // Don't add CSNGs to the output dependency list (we parse them because we need their AGSC dependencies in the output AudioGroup set)
    if (ResType != eMidi)
    {
        rOut.push_back(rkID);
        mLayerUsedAssets.insert(rkID);
    }
}

void CAreaDependencyListBuilder::EvaluateDependencyNode(CResourceEntry *pCurEntry, IDependencyNode *pNode, std::list<CAssetID>& rOut, std::set<CAssetID> *pAudioGroupsOut)
{
    EDependencyNodeType Type = pNode->Type();
    bool ParseChildren = false;

    if (Type == eDNT_ResourceDependency || Type == eDNT_ScriptProperty || Type == eDNT_CharacterProperty)
    {
        CResourceDependency *pDep = static_cast<CResourceDependency*>(pNode);
        AddDependency(pDep->ID(), rOut, pAudioGroupsOut);
    }

    else if (Type == eDNT_AnimEvent)
    {
        CAnimEventDependency *pDep = static_cast<CAnimEventDependency*>(pNode);
        u32 CharIndex = pDep->CharIndex();

        if (CharIndex == -1 || mCharacterUsageMap.IsCharacterUsed(mCurrentAnimSetID, CharIndex))
            AddDependency(pDep->ID(), rOut, pAudioGroupsOut);
    }

    else if (Type == eDNT_SetCharacter)
    {
        // Note: For MP1/2 PlayerActor, always treat as if Empty Suit is the only used one
        const u32 kEmptySuitIndex = (mGame >= eEchoesDemo ? 3 : 5);

        CSetCharacterDependency *pChar = static_cast<CSetCharacterDependency*>(pNode);
        u32 SetIndex = pChar->CharSetIndex();
        ParseChildren = mCharacterUsageMap.IsCharacterUsed(mCurrentAnimSetID, pChar->CharSetIndex()) || (mIsPlayerActor && SetIndex == kEmptySuitIndex);
    }

    else if (Type == eDNT_SetAnimation)
    {
        CSetAnimationDependency *pAnim = static_cast<CSetAnimationDependency*>(pNode);
        ParseChildren = mCharacterUsageMap.IsAnimationUsed(mCurrentAnimSetID, pAnim) || (mIsPlayerActor && pAnim->IsUsedByAnyCharacter());
    }

    else
        ParseChildren = true;

    if (ParseChildren)
    {
        for (u32 iChild = 0; iChild < pNode->NumChildren(); iChild++)
            EvaluateDependencyNode(pCurEntry, pNode->ChildByIndex(iChild), rOut, pAudioGroupsOut);
    }
}
