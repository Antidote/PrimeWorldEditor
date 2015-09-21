#ifndef CSCRIPTTEMPLATE_H
#define CSCRIPTTEMPLATE_H

#include "CPropertyTemplate.h"
#include "CProperty.h"
#include "EPropertyType.h"
#include "EVolumeShape.h"
#include <Common/CFourCC.h>
#include <Common/types.h>
#include <list>
#include <vector>
#include <tinyxml2.h>
#include <Resource/model/CModel.h>

class CMasterTemplate;
class CScriptObject;

typedef std::string TIDString;

/*
 * CScriptTemplate is a class that encases the data contained in one of the XML templates.
 * It essentially sets the layout of any given script object.
 *
 * It contains any data that applies globally to every instance of the object, such as
 * property names, editor attribute properties, etc.
 */
class CScriptTemplate
{
    friend class CTemplateLoader;
    friend class CTemplateWriter;

public:
    enum ERotationType {
        eRotationEnabled, eRotationDisabled
    };

    enum EScaleType {
        eScaleEnabled, eScaleDisabled, eScaleVolume
    };

private:
    struct SPropertySet {
        std::string SetName;
        CStructTemplate *pBaseStruct;
    };

    struct SEditorAsset
    {
        enum {
            eModel, eAnimParams
        } AssetType;

        enum {
            eProperty, eFile
        } AssetSource;

        TIDString AssetLocation;
        s32 ForceNodeIndex; // Force animsets to use specific node instead of one from property
    };

    CMasterTemplate *mpMaster;
    std::vector<SPropertySet> mPropertySets;
    std::list<CScriptObject*> mObjectList;
    ERotationType mRotationType;
    EScaleType mScaleType;
    std::string mTemplateName;
    std::string mSourceFile;
    u32 mObjectID;
    bool mVisible;

    // Editor Properties
    TIDString mNameIDString;
    TIDString mPositionIDString;
    TIDString mRotationIDString;
    TIDString mScaleIDString;
    TIDString mActiveIDString;
    TIDString mLightParametersIDString;
    std::vector<SEditorAsset> mAssets;

    // Preview Volume
    EVolumeShape mVolumeShape;
    TIDString mVolumeConditionIDString;

    struct SVolumeCondition {
        int Value;
        EVolumeShape Shape;
    };
    std::vector<SVolumeCondition> mVolumeConditions;

public:
    CScriptTemplate(CMasterTemplate *pMaster);
    ~CScriptTemplate();

    CMasterTemplate* MasterTemplate();
    std::string TemplateName(s32 propCount = -1) const;
    std::string PropertySetNameByCount(s32 propCount) const;
    std::string PropertySetNameByIndex(u32 index) const;
    u32 NumPropertySets() const;
    ERotationType RotationType() const;
    EScaleType ScaleType() const;
    u32 ObjectID() const;
    void SetVisible(bool visible);
    bool IsVisible() const;
    void DebugPrintProperties(int propCount = -1);

    // Property Fetching
    CStructTemplate* BaseStructByCount(s32 propCount);
    CStructTemplate* BaseStructByIndex(u32 index);
    EVolumeShape VolumeShape(CScriptObject *pObj);
    CStringProperty* FindInstanceName(CPropertyStruct *pProperties);
    CVector3Property* FindPosition(CPropertyStruct *pProperties);
    CVector3Property* FindRotation(CPropertyStruct *pProperties);
    CVector3Property* FindScale(CPropertyStruct *pProperties);
    CBoolProperty* FindActive(CPropertyStruct *pProperties);
    CPropertyStruct* FindLightParameters(CPropertyStruct *pProperties);
    CModel* FindDisplayModel(CPropertyStruct *pProperties);
    bool HasPosition();

    // Object Tracking
    u32 NumObjects() const;
    const std::list<CScriptObject*>& ObjectList() const;
    void AddObject(CScriptObject *pObject);
    void RemoveObject(CScriptObject *pObject);
    void SortObjects();
};

#endif // CSCRIPTTEMPLATE_H
