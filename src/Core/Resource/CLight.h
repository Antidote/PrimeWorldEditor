#ifndef CLIGHT_H
#define CLIGHT_H

#include <FileIO/IInputStream.h>
#include <Common/CColor.h>
#include <Math/CVector3f.h>

/* CLight is currently heavily based on the lights system from Metroid Prime,
 * including code reverse engineered from the game's executable. Not yet sure
 * how much needs to be modified to properly support DKCR. */
enum ELightType
{
    eLocalAmbient = 0,
    eDirectional = 1,
    eSpot = 3,
    eCustom = 2
};

class CLight
{
    ELightType mType;
    u32 mLayerIndex;
    CVector3f mPosition;
    CVector3f mDirection;
    CColor mColor;
    float mSpotCutoff;
    CVector3f mDistAttenCoefficients;
    CVector3f mAngleAttenCoefficients;

    mutable float mCachedRadius;
    mutable float mCachedIntensity;
    mutable u8 mDirtyFlags;

public:
    CLight();

private:
    // Data Manipulation
    float CalculateRadius() const;
    float CalculateIntensity() const;
    CVector3f CalculateSpotAngleAtten();

public:
    // Accessors
    inline ELightType Type() const              { return mType; }
    inline u32 LayerIndex() const               { return mLayerIndex; }
    inline CVector3f Position() const           { return mPosition; }
    inline CVector3f Direction() const          { return mDirection; }
    inline CColor Color() const                 { return mColor; }
    inline CVector3f DistAttenuation() const    { return mDistAttenCoefficients; }
    inline CVector3f AngleAttenuation() const   { return mAngleAttenCoefficients; }

    inline void SetLayer(u32 Index)                         { mLayerIndex = Index; }
    inline void SetPosition(const CVector3f& rkPosition)    { mPosition = rkPosition; }
    inline void SetDirection(const CVector3f& rkDirection)  { mDirection = rkDirection; }

    float GetRadius() const;
    float GetIntensity() const;

    void SetColor(const CColor& rkColor);
    void SetSpotCutoff(float Cutoff);
    void SetDistAtten(float DistCoefA, float DistCoefB, float DistCoefC);
    void SetAngleAtten(float AngleCoefA, float AngleCoefB, float AngleCoefC);

    // Other
    void Load() const;

    // Static
    static CLight* BuildLocalAmbient(const CVector3f& rkPosition, const CColor& rkColor);
    static CLight* BuildDirectional(const CVector3f& rkPosition, const CVector3f& rkDirection, const CColor& rkColor);
    static CLight* BuildSpot(const CVector3f& rkPosition, const CVector3f& rkDirection, const CColor& rkColor, float Cutoff);
    static CLight* BuildCustom(const CVector3f& rkPosition, const CVector3f& rkDirection, const CColor& rkColor,
                              float DistAttenA, float DistAttenB, float DistAttenC,
                              float AngleAttenA, float AngleAttenB, float AngleAttenC);

    // Constants
    static const CVector3f skDefaultLightPos;
    static const CVector3f skDefaultLightDir;
};

#endif // CLIGHT_H
