#ifndef CSANDWORMEXTRA_H
#define CSANDWORMEXTRA_H

#include "CScriptExtra.h"

class CSandwormExtra : public CScriptExtra
{
    // Transform adjustments to Sandworm attachments.
    TFloatProperty *mpPincersScaleProperty;

public:
    explicit CSandwormExtra(CScriptObject *pInstance, CScene *pScene, CScriptNode *pParent);
    void PropertyModified(IProperty *pProp);
};

#endif // CSANDWORMEXTRA_H
