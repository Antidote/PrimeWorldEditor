#ifndef SRAYINTERSECTION
#define SRAYINTERSECTION

#include "types.h"
class CSceneNode;

struct SRayIntersection
{
    bool Hit;
    float Distance;
    CSceneNode *pNode;
    u32 AssetIndex;

    SRayIntersection() {}
    SRayIntersection(bool _Hit, float _Distance, CSceneNode *_pNode, u32 _AssetIndex)
        : Hit(_Hit), Distance(_Distance), pNode(_pNode), AssetIndex(_AssetIndex) {}
};

#endif // SRAYINTERSECTION

