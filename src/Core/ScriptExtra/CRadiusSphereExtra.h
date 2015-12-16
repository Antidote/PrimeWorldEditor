#ifndef CRADIUSSPHEREEXTRA_H
#define CRADIUSSPHEREEXTRA_H

#include "CScriptExtra.h"

class CRadiusSphereExtra : public CScriptExtra
{
    // Sphere visualization for objects that have a float radius property.
    u32 mObjectType;
    CFloatProperty *mpRadius;

public:
    explicit CRadiusSphereExtra(CScriptObject *pInstance, CSceneManager *pScene, CSceneNode *pParent = 0);
    void AddToRenderer(CRenderer *pRenderer, const SViewInfo& rkViewInfo);
    void Draw(ERenderOptions Options, int ComponentIndex, const SViewInfo& rkViewInfo);
    CColor Color() const;
    CAABox Bounds() const;
};

#endif // CRADIUSSPHEREEXTRA_H
