#include "CAnimationParameters.h"
#include "CAnimSet.h"
#include <Core/CResCache.h>
#include <Core/Log.h>
#include <iostream>

CAnimationParameters::CAnimationParameters()
{
    mGame = ePrime;
    mpCharSet = nullptr;
    mNodeIndex = 0;
    mUnknown1 = 0;
    mUnknown2 = 0;
    mUnknown3 = 0;
    mUnknown4 = 0;
}

CAnimationParameters::CAnimationParameters(CInputStream& SCLY, EGame game)
{
    mGame = game;
    mpCharSet = nullptr;
    mNodeIndex = 0;
    mUnknown1 = 0;
    mUnknown2 = 0;
    mUnknown3 = 0;
    mUnknown4 = 0;

    if (game <= eEchoes)
    {
        u32 animSetID = SCLY.ReadLong();
        mNodeIndex = SCLY.ReadLong();
        mUnknown1 = SCLY.ReadLong();

        mpCharSet = gResCache.GetResource(animSetID, "ANCS");
        mResToken = CToken(mpCharSet);
    }

    else if (game <= eCorruption)
    {
        u64 charID = SCLY.ReadLongLong();
        mUnknown1 = SCLY.ReadLong();

        mpCharSet = gResCache.GetResource(charID, "CHAR");
        mResToken = CToken(mpCharSet);
    }

    else if (game == eReturns)
    {
        SCLY.Seek(-6, SEEK_CUR);
        u32 offset = SCLY.Tell();
        u32 propID = SCLY.ReadLong();
        SCLY.Seek(2, SEEK_CUR);

        mUnknown1 = (u32) SCLY.ReadByte();
        mUnknown1 &= 0xFF;

        if (mUnknown1 == 0x60)
        {
            u64 charID = SCLY.ReadLongLong();
            mUnknown2 = SCLY.ReadLong();
            mUnknown3 = SCLY.ReadLong();
            mUnknown4 = SCLY.ReadLong();

            mpCharSet = gResCache.GetResource(charID, "CHAR");
            mResToken = CToken(mpCharSet);
        }

        else if (mUnknown1 != 0x80)
        {
            Log::FileError(SCLY.GetSourceString(), offset,
                           "Unexpected AnimationParameters byte: " + StringUtil::ToHexString(mUnknown1, true, true, 2) + " (property " + StringUtil::ToHexString(propID, true, true, 8) + ")");
        }
    }
}

CModel* CAnimationParameters::GetCurrentModel(s32 nodeIndex)
{
    if (!mpCharSet) return nullptr;
    if (mpCharSet->Type() != eAnimSet) return nullptr;
    if (nodeIndex == -1) nodeIndex = mNodeIndex;

    CAnimSet *pSet = static_cast<CAnimSet*>(mpCharSet);
    if (pSet->getNodeCount() <= nodeIndex) return nullptr;
    return pSet->getNodeModel(nodeIndex);
}

// ************ GETTERS ************
EGame CAnimationParameters::Version()
{
    return mGame;
}

CResource* CAnimationParameters::Resource()
{
    return mpCharSet;
}

u32 CAnimationParameters::CharacterIndex()
{
    return mNodeIndex;
}

u32 CAnimationParameters::Unknown(u32 index)
{
    switch (index)
    {
    case 0: return mUnknown1;
    case 1: return mUnknown2;
    case 2: return mUnknown3;
    case 3: return mUnknown4;
    default: return 0;
    }
}

// ************ SETTERS ************
void CAnimationParameters::SetResource(CResource *pRes)
{
    if ((pRes->Type() == eAnimSet) || (pRes->Type() == eCharacter))
    {
        mpCharSet = pRes;
        mResToken = CToken(pRes);
        mNodeIndex = 0;
    }
    else
        Log::Error("Resource with invalid type passed to CAnimationParameters: " + pRes->Source());
}

void CAnimationParameters::SetNodeIndex(u32 index)
{
    mNodeIndex = index;
}

void CAnimationParameters::SetUnknown(u32 index, u32 value)
{
    switch (index)
    {
    case 0: mUnknown1 = value;
    case 1: mUnknown2 = value;
    case 2: mUnknown3 = value;
    case 3: mUnknown4 = value;
    }
}
