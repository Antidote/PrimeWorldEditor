#ifndef CTYPESINSTANCEMODEL_H
#define CTYPESINSTANCEMODEL_H

#include "CWorldEditor.h"
#include <Core/Resource/Script/CMasterTemplate.h>
#include <Core/Resource/Script/CScriptTemplate.h>
#include <Core/Scene/CSceneNode.h>

#include <QAbstractItemModel>
#include <QList>

class CInstancesModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum EIndexType
    {
        eRootIndex, eNodeTypeIndex, eObjectTypeIndex, eInstanceIndex
    };

    enum ENodeType
    {
        eScriptType = 0x0,
        eLightType = 0x1,
        eInvalidType = 0xFF
    };

    enum EInstanceModelType
    {
        eLayers, eTypes
    };

private:
    CWorldEditor *mpEditor;
    CScene *mpScene;
    TResPtr<CGameArea> mpArea;
    CMasterTemplate *mpCurrentMaster;
    EInstanceModelType mModelType;
    QList<CScriptTemplate*> mTemplateList;
    QStringList mBaseItems;
    bool mShowColumnEnabled;
    bool mChangingLayout;

public:
    explicit CInstancesModel(QObject *pParent = 0);
    ~CInstancesModel();
    QVariant headerData(int Section, Qt::Orientation Orientation, int Role) const;
    QModelIndex index(int Row, int Column, const QModelIndex& rkParent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex& rkChild) const;
    int rowCount(const QModelIndex& rkParent) const;
    int columnCount(const QModelIndex& rkParent) const;
    QVariant data(const QModelIndex& rkIndex, int Role) const;

    void SetEditor(CWorldEditor *pEditor);
    void SetMaster(CMasterTemplate *pMaster);
    void SetArea(CGameArea *pArea);
    void SetModelType(EInstanceModelType Type);
    void SetShowColumnEnabled(bool Enabled);
    CScriptLayer* IndexLayer(const QModelIndex& rkIndex) const;
    CScriptTemplate* IndexTemplate(const QModelIndex& rkIndex) const;
    CScriptObject* IndexObject(const QModelIndex& rkIndex) const;

public slots:
    void NodeAboutToBeCreated();
    void NodeCreated(CSceneNode *pNode);
    void NodeAboutToBeDeleted(CSceneNode *pNode);
    void NodeDeleted();

    void PropertyModified(CScriptObject *pInst, IProperty *pProp);
    void InstancesLayerPreChange();
    void InstancesLayerPostChange(const QList<CScriptNode*>& rkInstanceList);

    // Static
    static EIndexType IndexType(const QModelIndex& rkIndex);
    static ENodeType IndexNodeType(const QModelIndex& rkIndex);

private:
    void GenerateList();
};

#endif // CTYPESINSTANCEMODEL_H
