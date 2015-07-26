#ifndef CMATERIALPASS_H
#define CMATERIALPASS_H

#include <Common/CFourCC.h>
#include <Common/CHashFNV1A.h>
#include <Core/CToken.h>
#include <Core/ERenderOptions.h>
#include <Resource/CTexture.h>
#include "ETevEnums.h"

class CMaterial;

class CMaterialPass
{
    friend class CMaterialLoader;
    friend class CMaterialCooker;

public:
    enum EPassSettings
    {
        eEmissiveBloom    = 0x4,
        eInvertOpacityMap = 0x10
    };

private:
    CMaterial *mpParentMat;
    CFourCC mPassType;
    EPassSettings mSettings;

    ETevColorInput mColorInputs[4];
    ETevAlphaInput mAlphaInputs[4];
    ETevOutput mColorOutput;
    ETevOutput mAlphaOutput;
    ETevKSel mKColorSel;
    ETevKSel mKAlphaSel;
    ETevRasSel mRasSel;
    u32 mTexCoordSource; // Should maybe be an enum but worried about conflicts with EVertexDescriptionn
    CTexture *mpTexture;
    CToken mTexToken;
    EUVAnimMode mAnimMode;
    float mAnimParams[4];
    bool mEnabled;

public:
    CMaterialPass(CMaterial *pParent);
    ~CMaterialPass();
    void HashParameters(CHashFNV1A& Hash);
    void LoadTexture(u32 PassIndex);
    void SetAnimCurrent(ERenderOptions Options, u32 PassIndex);

    // Setters
    void SetType(CFourCC Type);
    void SetColorInputs(ETevColorInput InputA, ETevColorInput InputB, ETevColorInput InputC, ETevColorInput InputD);
    void SetAlphaInputs(ETevAlphaInput InputA, ETevAlphaInput InputB, ETevAlphaInput InputC, ETevAlphaInput InputD);
    void SetColorOutput(ETevOutput OutputReg);
    void SetAlphaOutput(ETevOutput OutputReg);
    void SetKColorSel(ETevKSel Sel);
    void SetKAlphaSel(ETevKSel Sel);
    void SetRasSel(ETevRasSel Sel);
    void SetTexCoordSource(u32 Source);
    void SetTexture(CTexture *pTex);
    void SetAnimMode(EUVAnimMode Mode);
    void SetAnimParam(u32 ParamIndex, float Value);
    void SetEnabled(bool Enabled);

    // Getters
    inline CFourCC Type() const {
        return mPassType;
    }

    inline std::string NamedType() const {
        return PassTypeName(mPassType);
    }

    inline ETevColorInput ColorInput(u32 Input) const {
        return mColorInputs[Input];
    }

    inline ETevAlphaInput AlphaInput(u32 Input) const {
        return mAlphaInputs[Input];
    }

    inline ETevOutput ColorOutput() const {
        return mColorOutput;
    }

    inline ETevOutput AlphaOutput() const {
        return mAlphaOutput;
    }

    inline ETevKSel KColorSel() const {
        return mKColorSel;
    }

    inline ETevKSel KAlphaSel() const {
        return mKAlphaSel;
    }

    inline ETevRasSel RasSel() const {
        return mRasSel;
    }

    inline u32 TexCoordSource() const {
        return mTexCoordSource;
    }

    inline CTexture* Texture() const {
        return mpTexture;
    }

    inline EUVAnimMode AnimMode() const {
        return mAnimMode;
    }

    inline float AnimParam(u32 ParamIndex) const {
        return mAnimParams[ParamIndex];
    }

    inline bool IsEnabled() const {
        return mEnabled;
    }

    // Static
    static std::string PassTypeName(CFourCC Type);
};

#endif // CMATERIALPASS_H
