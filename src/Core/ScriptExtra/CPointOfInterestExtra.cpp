#include "CPointOfInterestExtra.h"

const CColor CPointOfInterestExtra::skRegularColor   = CColor::Integral(0xFF,0x70,0x00);
const CColor CPointOfInterestExtra::skImportantColor = CColor::Integral(0xFF,0x00,0x00);

CPointOfInterestExtra::CPointOfInterestExtra(CScriptObject *pInstance, CScene *pScene, CScriptNode *pParent)
    : CScriptExtra(pInstance, pScene, pParent)
    , mpScanProperty(nullptr)
    , mpScanData(nullptr)
{
    // Fetch scan data property
    CPropertyStruct *pBaseProp = pInstance->Properties();

    if (mGame <= ePrime)    mpScanProperty = TPropCast<TAssetProperty>(pBaseProp->PropertyByIDString("0x04:0x00"));
    else                    mpScanProperty = (TAssetProperty*) pBaseProp->PropertyByIDString("0xBDBEC295:0xB94E9BE7");
    if (mpScanProperty) PropertyModified(mpScanProperty);
}

void CPointOfInterestExtra::PropertyModified(IProperty* pProperty)
{
    if (mpScanProperty == pProperty)
        mpScanData = gpResourceStore->LoadResource( mpScanProperty->Get(), "SCAN" );
}

void CPointOfInterestExtra::ModifyTintColor(CColor& Color)
{
    if (mpScanData)
    {
        if (mpScanData->IsImportant()) Color *= skImportantColor;
        else Color *= skRegularColor;
    }
}
