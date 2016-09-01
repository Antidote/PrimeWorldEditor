#ifndef CPOINTOFINTERESTEXTRA_H
#define CPOINTOFINTERESTEXTRA_H

#include "CScriptExtra.h"
#include "Core/Resource/CScan.h"
#include <Common/CColor.h>

class CPointOfInterestExtra : public CScriptExtra
{
    // Tint POI billboard orange/red depending on scan importance
    TAssetProperty *mpScanProperty;
    TResPtr<CScan> mpScanData;

public:
    explicit CPointOfInterestExtra(CScriptObject *pInstance, CScene *pScene, CScriptNode *pParent = 0);
    void PropertyModified(IProperty* pProperty);
    void ModifyTintColor(CColor& Color);
    CScan* GetScan() const { return mpScanData; }

    static const CColor skRegularColor;
    static const CColor skImportantColor;
};

#endif // CPOINTOFINTERESTEXTRA_H
