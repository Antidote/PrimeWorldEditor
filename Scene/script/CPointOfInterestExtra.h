#ifndef CPOINTOFINTERESTEXTRA_H
#define CPOINTOFINTERESTEXTRA_H

#include "CScriptExtra.h"
#include <Common/CColor.h>
#include <Resource/CScan.h>

class CPointOfInterestExtra : public CScriptExtra
{
    // Tint POI billboard orange/red depending on scan importance
    CFileProperty *mpScanProperty;
    CScan *mpScanData;
    CToken mScanToken;

public:
    explicit CPointOfInterestExtra(CScriptObject *pInstance, CSceneManager *pScene, CSceneNode *pParent = 0);
    void PropertyModified(CPropertyBase* pProperty);
    void ModifyTintColor(CColor& Color);

    static const CColor skRegularColor;
    static const CColor skImportantColor;
};

#endif // CPOINTOFINTERESTEXTRA_H
