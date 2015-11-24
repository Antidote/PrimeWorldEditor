#ifndef CMODEL_H
#define CMODEL_H

#include "CBasicModel.h"
#include "SSurface.h"
#include "../CMaterialSet.h"

#include <Core/ERenderOptions.h>
#include <OpenGL/CIndexBuffer.h>
#include <OpenGL/GLCommon.h>

class CModel : public CBasicModel
{
    friend class CModelLoader;
    friend class CModelCooker;

    std::vector<CMaterialSet*> mMaterialSets;
    std::vector<std::vector<CIndexBuffer>> mSubmeshIndexBuffers;
    bool mHasOwnMaterials;
    
public:
    CModel();
    CModel(CMaterialSet *pSet, bool ownsMatSet);
    ~CModel();

    void BufferGL();
    void ClearGLBuffer();
    void Draw(ERenderOptions Options, u32 MatSet);
    void DrawSurface(ERenderOptions Options, u32 Surface, u32 MatSet);
    void DrawWireframe(ERenderOptions Options, const CColor& WireColor = CColor::skWhite);

    u32 GetMatSetCount();
    u32 GetMatCount();
    CMaterialSet* GetMatSet(u32 MatSet);
    CMaterial* GetMaterialByIndex(u32 MatSet, u32 Index);
    CMaterial* GetMaterialBySurface(u32 MatSet, u32 Surface);
    bool HasTransparency(u32 MatSet);
    bool IsSurfaceTransparent(u32 Surface, u32 MatSet);

private:
    CIndexBuffer* InternalGetIBO(u32 Surface, EGXPrimitiveType Primitive);
};

#endif // MODEL_H
