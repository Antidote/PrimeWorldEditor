#ifndef CLIGHTNODE_H
#define CLIGHTNODE_H

#include "CSceneNode.h"
#include "Core/Resource/CLight.h"

class CLightNode : public CSceneNode
{
    CLight *mpLight;
public:
    CLightNode(CSceneManager *pScene, CSceneNode *pParent = 0, CLight *Light = 0);
    ENodeType NodeType();
    void AddToRenderer(CRenderer *pRenderer, const SViewInfo& ViewInfo);
    void Draw(FRenderOptions Options, int ComponentIndex, const SViewInfo& ViewInfo);
    void DrawSelection();
    void RayAABoxIntersectTest(CRayCollisionTester& Tester, const SViewInfo& ViewInfo);
    SRayIntersection RayNodeIntersectTest(const CRay &Ray, u32 AssetID, const SViewInfo& ViewInfo);
    bool AllowsRotate() const { return false; }
    CLight* Light();
    CVector2f BillboardScale();

protected:
    void CalculateTransform(CTransform4f& rOut) const;
};

#endif // CLIGHTNODE_H
