#ifndef CLIGHTPARAMETERS_H
#define CLIGHTPARAMETERS_H

#include "Core/Resource/Area/CGameArea.h"
#include "Core/Resource/Script/IProperty.h"

enum EWorldLightingOptions
{
    eUnknown1 = 0,
    eNormalLighting = 1,
    eUnknown2 = 2,
    eDisableWorldLighting = 3
};

class CLightParameters
{
    CPropertyStruct *mpStruct;
    EGame mGame;

    TLongProperty *mpLightLayer;
    TEnumProperty *mpWorldLightingOptions;

public:
    CLightParameters(CPropertyStruct *pStruct, EGame Game)
        : mpStruct(pStruct)
        , mGame(Game)
        , mpLightLayer(nullptr)
        , mpWorldLightingOptions(nullptr)
    {
        if (mpStruct)
        {
            if (mGame <= ePrime)
            {
                mpWorldLightingOptions = TPropCast<TEnumProperty>(mpStruct->PropertyByIndex(0x7));
                mpLightLayer = TPropCast<TLongProperty>(mpStruct->PropertyByIndex(0xD));
            }
            else
            {
                mpWorldLightingOptions = TPropCast<TEnumProperty>(mpStruct->PropertyByID(0x6B5E7509));
                mpLightLayer = TPropCast<TLongProperty>(mpStruct->PropertyByID(0x1F715FD3));
            }
        }
    }

    inline int LightLayerIndex() const
    {
        if (!mpLightLayer)
            return 0;
        else
            return mpLightLayer->Get();
    }

    inline EWorldLightingOptions WorldLightingOptions() const
    {
        if (mpWorldLightingOptions)
            return (EWorldLightingOptions) mpWorldLightingOptions->Get();
        else
            return eNormalLighting;
    }
};

#endif // CLIGHTPARAMETERS_H
