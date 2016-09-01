#ifndef CSTRINGLIST
#define CSTRINGLIST

#include "CResource.h"

class CStringList : public CResource
{
    DECLARE_RESOURCE_TYPE(eStringList)
    friend class CAudioGroupLoader;
    std::vector<TString> mStringList;

public:
    CStringList(CResourceEntry *pEntry = 0)
        : CResource(pEntry)
    {}

    inline u32 NumStrings() const
    {
        return mStringList.size();
    }

    inline TString StringByIndex(u32 Index) const
    {
        ASSERT(Index >= 0 && Index < mStringList.size());
        return mStringList[Index];
    }
};

#endif // CSTRINGLIST

