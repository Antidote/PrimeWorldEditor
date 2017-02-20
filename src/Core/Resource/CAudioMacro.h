#ifndef CAUDIOMACRO_H
#define CAUDIOMACRO_H

#include "CResource.h"

class CAudioMacro : public CResource
{
    DECLARE_RESOURCE_TYPE(eAudioMacro)
    friend class CUnsupportedFormatLoader;

    TString mMacroName;
    std::vector<CAssetID> mSamples;

public:
    CAudioMacro(CResourceEntry *pEntry = 0)
        : CResource(pEntry)
    {}

    virtual CDependencyTree* BuildDependencyTree() const
    {
        CDependencyTree *pTree = new CDependencyTree();

        for (u32 iSamp = 0; iSamp < mSamples.size(); iSamp++)
            pTree->AddDependency(mSamples[iSamp]);

        return pTree;
    }

    // Accessors
    inline TString MacroName() const                { return mMacroName; }
    inline u32 NumSamples() const                   { return mSamples.size(); }
    inline CAssetID SampleByIndex(u32 Index) const  { return mSamples[Index]; }
};

#endif // CAUDIOMACRO_H
