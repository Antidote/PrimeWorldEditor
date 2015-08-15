#ifndef CRENDERBUCKET_H
#define CRENDERBUCKET_H

#include "CCamera.h"
#include "ERenderOptions.h"
#include "SRenderablePtr.h"
#include <Common/types.h>
#include <vector>

class CRenderBucket
{
public:
    enum ESortType
    {
        BackToFront,
        FrontToBack
    };
private:
    ESortType mSortType;
    std::vector<SRenderablePtr> mRenderables;
    u32 mEstSize;
    u32 mSize;

public:
    CRenderBucket();
    void SetSortType(ESortType Type);
    void Add(const SRenderablePtr& ptr);
    void Sort(CCamera& Camera);
    void Clear();
    void Draw(ERenderOptions Options);
};

#endif // CRENDERBUCKET_H
