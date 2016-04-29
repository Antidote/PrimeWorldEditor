#ifndef CSKIN_H
#define CSKIN_H

#include "CResource.h"
#include "Core/Resource/Model/CVertex.h"

struct SVertexWeights
{
    TBoneIndices Indices;
    TBoneWeights Weights;
};

class CSkin : public CResource
{
    DECLARE_RESOURCE_TYPE(eSkin)
    friend class CSkinLoader;

    struct SVertGroup
    {
        SVertexWeights Weights;
        u32 NumVertices;
    };
    std::vector<SVertGroup> mVertGroups;

public:
    CSkin() {}

    const SVertexWeights& WeightsForVertex(u32 VertIdx)
    {
        // Null weights bind everything to the root bone in case there is no matching vertex group
        static const SVertexWeights skNullWeights = {
            { 3, 0, 0, 0 },
            { 1.f, 0.f, 0.f, 0.f }
        };

        u32 Index = 0;

        for (u32 iGrp = 0; iGrp < mVertGroups.size(); iGrp++)
        {
            if (VertIdx < Index + mVertGroups[iGrp].NumVertices)
                return mVertGroups[iGrp].Weights;

            Index += mVertGroups[iGrp].NumVertices;
        }

        return skNullWeights;
    }
};

#endif // CSKIN_H
