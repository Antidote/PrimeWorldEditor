#ifndef CCOLLISIONLOADER_H
#define CCOLLISIONLOADER_H

#include "../CCollisionMesh.h"
#include "../CCollisionMeshGroup.h"
#include "../EFormatVersion.h"

class CCollisionLoader
{
    CCollisionMeshGroup *mpGroup;
    CCollisionMesh *mpMesh;
    EGame mVersion;
    std::vector<CCollisionMesh::SCollisionProperties> mProperties;

    CCollisionLoader();
    CCollisionMesh::CCollisionOctree* ParseOctree(CInputStream& src);
    CCollisionMesh::CCollisionOctree::SBranch* ParseOctreeBranch(CInputStream& src);
    CCollisionMesh::CCollisionOctree::SLeaf* ParseOctreeLeaf(CInputStream& src);
    void ParseOBBNode(CInputStream& DCLN);
    void ReadPropertyFlags(CInputStream& src);
    void LoadCollisionIndices(CInputStream& file, bool buildAABox);

public:
    static CCollisionMeshGroup* LoadAreaCollision(CInputStream& MREA);
    static CCollisionMeshGroup* LoadDCLN(CInputStream& DCLN);
    static EGame GetFormatVersion(u32 version);
};

#endif // CCOLLISIONLOADER_H
