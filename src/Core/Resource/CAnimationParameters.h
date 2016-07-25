#ifndef CANIMATIONPARAMETERS_H
#define CANIMATIONPARAMETERS_H

#include "CAnimSet.h"
#include "CResourceInfo.h"
#include "EGame.h"
#include "TResPtr.h"
#include "Core/Resource/Model/CModel.h"

class CAnimationParameters
{
    EGame mGame;
    CResourceInfo mCharacter;

    u32 mCharIndex;
    u32 mAnimIndex;
    u32 mUnknown2;
    u32 mUnknown3;

public:
    CAnimationParameters();
    CAnimationParameters(EGame Game);
    CAnimationParameters(IInputStream& rSCLY, EGame Game);
    void Write(IOutputStream& rSCLY);

    CModel* GetCurrentModel(s32 NodeIndex = -1);
    TString GetCurrentCharacterName(s32 NodeIndex = -1);

    // Accessors
    inline EGame Version() const        { return mGame; }
    inline CUniqueID ID() const         { return mCharacter.ID(); }
    inline CAnimSet* AnimSet() const    { return (CAnimSet*) mCharacter.Load(); }
    inline u32 CharacterIndex() const   { return mCharIndex; }
    inline u32 AnimIndex() const        { return mAnimIndex; }
    inline void SetCharIndex(u32 Index) { mCharIndex = Index; }
    inline void SetAnimIndex(u32 Index) { mAnimIndex = Index; }

    u32 Unknown(u32 Index);
    void SetResource(CResourceInfo Res);
    void SetUnknown(u32 Index, u32 Value);

    // Operators
    inline bool operator==(const CAnimationParameters& rkOther) const
    {
        return ( (mGame == rkOther.mGame) &&
                 (mCharacter == rkOther.mCharacter) &&
                 (mCharIndex == rkOther.mCharIndex) &&
                 (mAnimIndex == rkOther.mAnimIndex) &&
                 (mUnknown2 == rkOther.mUnknown2) &&
                 (mUnknown3 == rkOther.mUnknown3) );
    }
};

#endif // CANIMATIONPARAMETERS_H
