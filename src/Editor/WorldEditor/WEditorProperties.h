#ifndef WEDITORPROPERTIES_H
#define WEDITORPROPERTIES_H

#include <QWidget>
#include "CWorldEditor.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

class WEditorProperties : public QWidget
{
    Q_OBJECT
    CWorldEditor *mpEditor;
    CSceneNode *mpDisplayNode;

    QVBoxLayout *mpMainLayout;

    QLabel *mpInstanceInfoLabel;
    QHBoxLayout *mpInstanceInfoLayout;

    QCheckBox *mpActiveCheckBox;
    QLineEdit *mpInstanceNameLineEdit;
    QHBoxLayout *mpNameLayout;

    QLabel *mpLayersLabel;
    QComboBox *mpLayersComboBox;
    QHBoxLayout *mpLayersLayout;

    bool mHasEditedName;

public:
    WEditorProperties(QWidget *pParent = 0);
    void SyncToEditor(CWorldEditor *pEditor);
    void SetLayerComboBox();

public slots:
    void OnSelectionModified();
    void OnPropertyModified(CScriptObject *pInst, IPropertyNew *pProp);
    void OnInstancesLayerChanged(const QList<CScriptNode*>& rkNodeList);
    void OnLayersModified();
    void UpdatePropertyValues();

protected slots:
    void OnActiveChanged();
    void OnInstanceNameEdited();
    void OnInstanceNameEditFinished();
    void OnLayerChanged();
};

#endif // WEDITORPROPERTIES_H
