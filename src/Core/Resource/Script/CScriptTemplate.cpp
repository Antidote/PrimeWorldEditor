#include "CScriptTemplate.h"
#include "CScriptObject.h"
#include "CMasterTemplate.h"
#include "Core/GameProject/CResourceStore.h"
#include "Core/Resource/CAnimSet.h"
#include <Common/Log.h>

#include <iostream>
#include <string>

CScriptTemplate::CScriptTemplate(CMasterTemplate *pMaster)
    : mpMaster(pMaster)
    , mpBaseStruct(nullptr)
    , mVisible(true)
    , mPreviewScale(1.f)
    , mVolumeShape(eNoShape)
    , mVolumeScale(1.f)
{
}

CScriptTemplate::~CScriptTemplate()
{
    delete mpBaseStruct;
}

EGame CScriptTemplate::Game() const
{
    return mpMaster->Game();
}

// ************ PROPERTY FETCHING ************
template<typename PropType, EPropertyType PropEnum>
PropType TFetchProperty(CPropertyStruct *pProperties, const TIDString& rkID)
{
    if (rkID.IsEmpty()) return nullptr;
    IProperty *pProp = pProperties->PropertyByIDString(rkID);

    if (pProp && (pProp->Type() == PropEnum))
        return static_cast<PropType>(pProp);
    else
        return nullptr;
}

EVolumeShape CScriptTemplate::VolumeShape(CScriptObject *pObj)
{
    if (pObj->Template() != this)
    {
        Log::Error(pObj->Template()->Name() + " instance somehow called VolumeShape() on " + Name() + " template");
        return eInvalidShape;
    }

    if (mVolumeShape == eConditionalShape)
    {
        s32 Index = CheckVolumeConditions(pObj, true);
        if (Index == -1) return eInvalidShape;
        else return mVolumeConditions[Index].Shape;
    }
    else return mVolumeShape;
}

float CScriptTemplate::VolumeScale(CScriptObject *pObj)
{
    if (pObj->Template() != this)
    {
        Log::Error(pObj->Template()->Name() + " instance somehow called VolumeScale() on " + Name() + " template");
        return -1;
    }

    if (mVolumeShape == eConditionalShape)
    {
        s32 Index = CheckVolumeConditions(pObj, false);
        if (Index == -1) return mVolumeScale;
        else return mVolumeConditions[Index].Scale;
    }
    else return mVolumeScale;
}

s32 CScriptTemplate::CheckVolumeConditions(CScriptObject *pObj, bool LogErrors)
{
    // Private function
    if (mVolumeShape == eConditionalShape)
    {
        IProperty *pProp = pObj->Properties()->PropertyByIDString(mVolumeConditionIDString);

        // Get value of the condition test property (only boolean, integral, and enum types supported)
        int Val;

        switch (pProp->Type())
        {
        case eBoolProperty:
            Val = (static_cast<TBoolProperty*>(pProp)->Get() ? 1 : 0);
            break;

        case eByteProperty:
            Val = (int) static_cast<TByteProperty*>(pProp)->Get();
            break;

        case eShortProperty:
            Val = (int) static_cast<TShortProperty*>(pProp)->Get();
            break;

        case eLongProperty:
            Val = (int) static_cast<TLongProperty*>(pProp)->Get();
            break;

        case eEnumProperty:
            Val = (int) static_cast<TEnumProperty*>(pProp)->Get();
            break;
        }

        // Test and check whether any of the conditions are true
        for (u32 iCon = 0; iCon < mVolumeConditions.size(); iCon++)
        {
            if (mVolumeConditions[iCon].Value == Val)
                return iCon;
        }

        if (LogErrors)
            Log::Error(pObj->Template()->Name() + " instance " + TString::HexString(pObj->InstanceID()) + " has unexpected volume shape value of " + TString::HexString((u32) Val, 0));
    }

    return -1;
}

TStringProperty* CScriptTemplate::FindInstanceName(CPropertyStruct *pProperties)
{
    return TFetchProperty<TStringProperty*, eStringProperty>(pProperties, mNameIDString);
}

TVector3Property* CScriptTemplate::FindPosition(CPropertyStruct *pProperties)
{
    return TFetchProperty<TVector3Property*, eVector3Property>(pProperties, mPositionIDString);
}

TVector3Property* CScriptTemplate::FindRotation(CPropertyStruct *pProperties)
{
    return TFetchProperty<TVector3Property*, eVector3Property>(pProperties, mRotationIDString);
}

TVector3Property* CScriptTemplate::FindScale(CPropertyStruct *pProperties)
{
    return TFetchProperty<TVector3Property*, eVector3Property>(pProperties, mScaleIDString);
}

TBoolProperty* CScriptTemplate::FindActive(CPropertyStruct *pProperties)
{
    return TFetchProperty<TBoolProperty*, eBoolProperty>(pProperties, mActiveIDString);
}

CPropertyStruct* CScriptTemplate::FindLightParameters(CPropertyStruct *pProperties)
{
    return TFetchProperty<CPropertyStruct*, eStructProperty>(pProperties, mLightParametersIDString);
}

CResource* CScriptTemplate::FindDisplayAsset(CPropertyStruct *pProperties, u32& rOutCharIndex, u32& rOutAnimIndex, bool& rOutIsInGame)
{
    rOutCharIndex = -1;
    rOutAnimIndex = -1;
    rOutIsInGame = false;

    for (auto it = mAssets.begin(); it != mAssets.end(); it++)
    {
        CResource *pRes = nullptr;

        // File
        if (it->AssetSource == SEditorAsset::eFile)
        {
            TString Path = "../resources/" + it->AssetLocation;
            pRes = gpResourceStore->LoadResource(Path);
        }

        // Property
        else
        {
            IProperty *pProp = pProperties->PropertyByIDString(it->AssetLocation);

            if (it->AssetType == SEditorAsset::eAnimParams && pProp->Type() == eCharacterProperty)
            {
                TCharacterProperty *pChar = static_cast<TCharacterProperty*>(pProp);
                pRes = pChar->Get().AnimSet();

                if (pRes)
                {
                    rOutCharIndex = (it->ForceNodeIndex >= 0 ? it->ForceNodeIndex : pChar->Get().CharacterIndex());
                    rOutAnimIndex = pChar->Get().AnimIndex();
                }
            }

            else
            {
                TFileProperty *pFile = static_cast<TFileProperty*>(pProp);
                pRes = pFile->Get().Load();
            }
        }

        // If we have a valid resource, return
        if (pRes)
        {
            rOutIsInGame = (pRes->Type() != eTexture && it->AssetSource == SEditorAsset::eProperty);
            return pRes;
        }
    }

    // None are valid - no display asset
    return nullptr;
}

CCollisionMeshGroup* CScriptTemplate::FindCollision(CPropertyStruct *pProperties)
{
    for (auto it = mAssets.begin(); it != mAssets.end(); it++)
    {
        if (it->AssetType != SEditorAsset::eCollision) continue;
        CResource *pRes = nullptr;

        // File
        if (it->AssetSource == SEditorAsset::eFile)
        {
            TString path = "../resources/" + it->AssetLocation;
            pRes = gpResourceStore->LoadResource(path);
        }

        // Property
        else
        {
            IProperty *pProp = pProperties->PropertyByIDString(it->AssetLocation);

            if (pProp->Type() == eFileProperty)
            {
                TFileProperty *pFile = static_cast<TFileProperty*>(pProp);
                pRes = pFile->Get().Load();
            }
        }

        // Verify resource exists + is correct type
        if (pRes && (pRes->Type() == eDynamicCollision))
            return static_cast<CCollisionMeshGroup*>(pRes);
    }

    return nullptr;
}


// ************ OBJECT TRACKING ************
u32 CScriptTemplate::NumObjects() const
{
    return mObjectList.size();
}

const std::list<CScriptObject*>& CScriptTemplate::ObjectList() const
{
    return mObjectList;
}

void CScriptTemplate::AddObject(CScriptObject *pObject)
{
    mObjectList.push_back(pObject);
}

void CScriptTemplate::RemoveObject(CScriptObject *pObject)
{
    for (auto it = mObjectList.begin(); it != mObjectList.end(); it++)
    {
        if (*it == pObject)
        {
            mObjectList.erase(it);
            break;
        }
    }
}

void CScriptTemplate::SortObjects()
{
    // todo: make this function take layer names into account
    mObjectList.sort([](CScriptObject *pA, CScriptObject *pB) -> bool {
        return (pA->InstanceID() < pB->InstanceID());
    });
}
