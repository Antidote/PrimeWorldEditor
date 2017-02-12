#include "IMetaAnimation.h"

// ************ CMetaAnimFactory ************
CMetaAnimFactory gMetaAnimFactory;

IMetaAnimation* CMetaAnimFactory::LoadFromStream(IInputStream& rInput)
{
    EMetaAnimationType Type = (EMetaAnimationType) rInput.ReadLong();

    switch (Type)
    {
    case eMAT_Play:
        return new CMetaAnimPlay(rInput);

    case eMAT_Blend:
    case eMAT_PhaseBlend:
        return new CMetaAnimBlend(Type, rInput);

    case eMAT_Random:
        return new CMetaAnimRandom(rInput);

    case eMAT_Sequence:
        return new CMetaAnimSequence(rInput);

    default:
        Log::Error("Unrecognized meta-animation type: " + TString::FromInt32(Type, 0, 10));
        return nullptr;
    }
}

// ************ CMetaAnimationPlay ************
CMetaAnimPlay::CMetaAnimPlay(IInputStream& rInput)
{
    mPrimitive = CAnimPrimitive(rInput);
    mUnknownA = rInput.ReadFloat();
    mUnknownB = rInput.ReadLong();
}

EMetaAnimationType CMetaAnimPlay::Type() const
{
    return eMAT_Play;
}

void CMetaAnimPlay::GetUniquePrimitives(std::set<CAnimPrimitive>& rPrimSet) const
{
    rPrimSet.insert(mPrimitive);
}

// ************ CMetaAnimBlend ************
CMetaAnimBlend::CMetaAnimBlend(EMetaAnimationType Type, IInputStream& rInput)
{
    ASSERT(Type == eMAT_Blend || Type == eMAT_PhaseBlend);
    mType = Type;
    mpMetaAnimA = gMetaAnimFactory.LoadFromStream(rInput);
    mpMetaAnimB = gMetaAnimFactory.LoadFromStream(rInput);
    mUnknownA = rInput.ReadFloat();
    mUnknownB = rInput.ReadBool();
}

CMetaAnimBlend::~CMetaAnimBlend()
{
    delete mpMetaAnimA;
    delete mpMetaAnimB;
}

EMetaAnimationType CMetaAnimBlend::Type() const
{
    return mType;
}

void CMetaAnimBlend::GetUniquePrimitives(std::set<CAnimPrimitive>& rPrimSet) const
{
    mpMetaAnimA->GetUniquePrimitives(rPrimSet);
    mpMetaAnimB->GetUniquePrimitives(rPrimSet);
}

// ************ CMetaAnimRandom ************
CMetaAnimRandom::CMetaAnimRandom(IInputStream& rInput)
{
    u32 NumPairs = rInput.ReadLong();
    mProbabilityPairs.reserve(NumPairs);

    for (u32 iAnim = 0; iAnim < NumPairs; iAnim++)
    {
        SAnimProbabilityPair Pair;
        Pair.pAnim = gMetaAnimFactory.LoadFromStream(rInput);
        Pair.Probability = rInput.ReadLong();
        mProbabilityPairs.push_back(Pair);
    }
}

CMetaAnimRandom::~CMetaAnimRandom()
{
    for (u32 iPair = 0; iPair < mProbabilityPairs.size(); iPair++)
        delete mProbabilityPairs[iPair].pAnim;
}

EMetaAnimationType CMetaAnimRandom::Type() const
{
    return eMAT_Random;
}

void CMetaAnimRandom::GetUniquePrimitives(std::set<CAnimPrimitive>& rPrimSet) const
{
    for (u32 iPair = 0; iPair < mProbabilityPairs.size(); iPair++)
        mProbabilityPairs[iPair].pAnim->GetUniquePrimitives(rPrimSet);
}

// ************ CMetaAnimSequence ************
CMetaAnimSequence::CMetaAnimSequence(IInputStream& rInput)
{
    u32 NumAnims = rInput.ReadLong();
    mAnimations.reserve(NumAnims);

    for (u32 iAnim = 0; iAnim < NumAnims; iAnim++)
    {
        IMetaAnimation *pAnim = gMetaAnimFactory.LoadFromStream(rInput);
        mAnimations.push_back(pAnim);
    }
}

CMetaAnimSequence::~CMetaAnimSequence()
{
    for (u32 iAnim = 0; iAnim < mAnimations.size(); iAnim++)
        delete mAnimations[iAnim];
}

EMetaAnimationType CMetaAnimSequence::Type() const
{
    return eMAT_Sequence;
}

void CMetaAnimSequence::GetUniquePrimitives(std::set<CAnimPrimitive>& rPrimSet) const
{
    for (u32 iAnim = 0; iAnim < mAnimations.size(); iAnim++)
        mAnimations[iAnim]->GetUniquePrimitives(rPrimSet);
}