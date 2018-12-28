#ifndef CPROPERTYVIEW_H
#define CPROPERTYVIEW_H

#include "CPropertyModel.h"
#include "CPropertyDelegate.h"
#include <Core/Resource/Script/CScriptObject.h>
#include <QTreeView>

class CPropertyView : public QTreeView
{
    Q_OBJECT

    CWorldEditor *mpEditor;
    CPropertyModel *mpModel;
    CPropertyDelegate *mpDelegate;
    CScriptObject *mpObject;

    IProperty *mpMenuProperty;
    QAction *mpShowNameValidityAction;
    QAction *mpEditTemplateAction;
    QAction *mpGenNamesForPropertyAction;
    QAction *mpGenNamesForSiblingsAction;
    QAction *mpGenNamesForChildrenAction;

public:
    CPropertyView(QWidget *pParent = 0);
    void setModel(QAbstractItemModel *pModel);
    bool event(QEvent *pEvent);
    void InitColumnWidths(float NameColumnPercentage, float ValueColumnPercentage);
    void ClearProperties();
    void SetIntrinsicProperties(CStructRef InProperties);
    void SetInstance(CScriptObject *pObj);
    void UpdateEditorProperties(const QModelIndex& rkParent);

    inline CPropertyModel* PropertyModel() const { return mpModel; }

public slots:
    void SetPersistentEditors(const QModelIndex& rkIndex);
    void ClosePersistentEditors(const QModelIndex& rkIndex);
    void OnPropertyModified(const QModelIndex& rkIndex);

    void RefreshView();
    void CreateContextMenu(const QPoint& rkPos);
    void ToggleShowNameValidity(bool ShouldShow);
    void EditPropertyTemplate();

    void GenerateNamesForProperty();
    void GenerateNamesForSiblings();
    void GenerateNamesForChildren();
};

#endif // CPROPERTYVIEW_H
