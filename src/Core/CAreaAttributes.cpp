#include "CAreaAttributes.h"
#include "Core/Resource/Script/CMasterTemplate.h"
#include "Core/Resource/Script/CScriptLayer.h"

CAreaAttributes::CAreaAttributes(CScriptObject *pObj)
{
    SetObject(pObj);
}

CAreaAttributes::~CAreaAttributes()
{
}

void CAreaAttributes::SetObject(CScriptObject *pObj)
{
    mpObj = pObj;
    mGame = pObj->Template()->MasterTemplate()->Game();
}

bool CAreaAttributes::IsLayerEnabled() const
{
    return mpObj->Layer()->IsActive();
}

bool CAreaAttributes::IsSkyEnabled() const
{
    CPropertyStruct *pBaseStruct = mpObj->Properties();

    switch (mGame)
    {
    case ePrime:
    case eEchoesDemo:
    case eEchoes:
    case eCorruptionProto:
    case eCorruption:
    case eReturns:
        return static_cast<TBoolProperty*>(pBaseStruct->PropertyByIndex(1))->Get();
    default:
        return false;
    }
}

CModel* CAreaAttributes::SkyModel() const
{
    CPropertyStruct *pBaseStruct = mpObj->Properties();

    switch (mGame)
    {
    case ePrime:
        return gpResourceStore->LoadResource<CModel>( static_cast<TAssetProperty*>(pBaseStruct->PropertyByIndex(7))->Get() );
    case eEchoesDemo:
    case eEchoes:
    case eCorruptionProto:
    case eCorruption:
    case eReturns:
        return gpResourceStore->LoadResource<CModel>( static_cast<TAssetProperty*>(pBaseStruct->PropertyByID(0xD208C9FA))->Get() );
    default:
        return nullptr;
    }
}
