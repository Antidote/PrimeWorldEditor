#ifndef CENUMPROPERTY_H
#define CENUMPROPERTY_H

#include "IProperty.h"

/** There are two types of enum properties in the game data: enum and choice.
 *
 *  In the game, the difference is that choice properties are index-based, while
 *  enum properties are stored as a hash of the name of the enum value.
 *
 *  In PWE, however, they are both implemented the same way under the hood.
 */
template<EPropertyType TypeEnum>
class TEnumPropertyBase : public TSerializeableTypedProperty<s32, TypeEnum>
{
    friend class CTemplateLoader;
    friend class IProperty;

    struct SEnumValue
    {
        TString Name;
        u32 ID;

        SEnumValue()
            : ID(0)
        {}

        SEnumValue(const TString& rkInName, u32 InID)
            : Name(rkInName), ID(InID) {}


        inline bool operator==(const SEnumValue& rkOther) const
        {
            return( Name == rkOther.Name && ID == rkOther.ID );
        }

        void Serialize(IArchive& rArc)
        {
            rArc << SerialParameter("Name", Name, SH_Attribute)
                 << SerialParameter("ID", ID, SH_Attribute | SH_HexDisplay);
        }
    };
    std::vector<SEnumValue> mValues;

    /** XML template file that this enum originated from; for archetypes */
    TString mSourceFile;

protected:
    /** Constructor */
    TEnumPropertyBase(EGame Game)
        : TSerializeableTypedProperty(Game)
    {}

public:
    virtual const char* GetHashableTypeName() const
    {
        if (TypeEnum == EPropertyType::Enum)
            return "enum";
        else
            return "choice";
    }

    virtual void Serialize(IArchive& rArc)
    {
        // Skip TSerializeableTypedProperty, serialize default value ourselves so we can set SH_HexDisplay
        TTypedProperty::Serialize(rArc);

        TEnumPropertyBase* pArchetype = static_cast<TEnumPropertyBase*>(mpArchetype);
        u32 DefaultValueFlags = SH_HexDisplay | (pArchetype || Game() <= ePrime ? SH_Optional : 0);
        rArc << SerialParameter("DefaultValue", mDefaultValue, DefaultValueFlags, pArchetype ? pArchetype->mDefaultValue : 0);

        if (!pArchetype || !rArc.CanSkipParameters() || mValues != pArchetype->mValues)
        {
            rArc << SerialParameter("Values", mValues);
        }
    }

    virtual void SerializeValue(void* pData, IArchive& Arc) const
    {
        Arc.SerializePrimitive( (u32&) ValueRef(pData), 0 );
    }

    virtual void InitFromArchetype(IProperty* pOther)
    {
        TTypedProperty::InitFromArchetype(pOther);
        TEnumPropertyBase* pOtherEnum = static_cast<TEnumPropertyBase*>(pOther);
        mValues = pOtherEnum->mValues;
    }

    virtual TString GetTemplateFileName()
    {
        ASSERT(IsArchetype() || mpArchetype);
        return IsArchetype() ? mSourceFile : mpArchetype->GetTemplateFileName();
    }

    void AddValue(TString ValueName, u32 ValueID)
    {
        mValues.push_back( SEnumValue(ValueName, ValueID) );
    }

    inline u32 NumPossibleValues() const { return mValues.size(); }

    u32 ValueIndex(u32 ID) const
    {
        for (u32 ValueIdx = 0; ValueIdx < mValues.size(); ValueIdx++)
        {
            if (mValues[ValueIdx].ID == ID)
            {
                return ValueIdx;
            }
        }
        return -1;
    }

    u32 ValueID(u32 Index) const
    {
        ASSERT(Index >= 0 && Index < mValues.size());
        return mValues[Index].ID;
    }

    TString ValueName(u32 Index) const
    {
        ASSERT(Index >= 0 && Index < mValues.size());
        return mValues[Index].Name;
    }

    bool HasValidValue(void* pPropertyData)
    {
        int ID = ValueRef(pPropertyData);
        u32 Index = ValueIndex(ID);
        return Index >= 0 && Index < mValues.size();
    }
};

typedef TEnumPropertyBase<EPropertyType::Choice> CChoiceProperty;
typedef TEnumPropertyBase<EPropertyType::Enum>   CEnumProperty;

// Specialization of TPropCast to allow interchangeable casting, as both types are the same thing
template<>
inline CEnumProperty* TPropCast(IProperty* pProperty)
{
    if (pProperty)
    {
        EPropertyType InType = pProperty->Type();

        if (InType == EPropertyType::Enum || InType == EPropertyType::Choice)
        {
            return static_cast<CEnumProperty*>(pProperty);
        }
    }

    return nullptr;
}

template<>
inline CChoiceProperty* TPropCast(IProperty* pProperty)
{
    if (pProperty)
    {
        EPropertyType InType = pProperty->Type();

        if (InType == EPropertyType::Enum || InType == EPropertyType::Choice)
        {
            return static_cast<CChoiceProperty*>(pProperty);
        }
    }

    return nullptr;
}

#endif // CENUMPROPERTY_H
