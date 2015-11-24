#ifndef INODEEDITOR_H
#define INODEEDITOR_H

#include <QMainWindow>
#include <QAction>
#include <QActionGroup>
#include <QComboBox>
#include <QList>
#include <QUndoStack>

#include "CGizmo.h"
#include <Common/ETransformSpace.h>
#include <Core/CSceneManager.h>

class INodeEditor : public QMainWindow
{
    Q_OBJECT

protected:
    // Undo stack
    QUndoStack mUndoStack;
    QList<QAction*> mUndoActions;

    // Node management
    CSceneManager mScene;
    QList<CSceneNode*> mSelection;
    CAABox mSelectionBounds;

    // Gizmo
    CGizmo mGizmo;
    bool mShowGizmo;
    bool mGizmoHovering;
    bool mGizmoTransforming;
    ETransformSpace mTranslateSpace;
    ETransformSpace mRotateSpace;

    // Gizmo widgets
    QActionGroup *mpGizmoGroup;
    QList<QAction*> mGizmoActions;
    QComboBox *mpTransformCombo;

public:
    explicit INodeEditor(QWidget *pParent = 0);
    virtual ~INodeEditor();
    QUndoStack* UndoStack();
    CSceneManager* Scene();
    CGizmo* Gizmo();
    bool IsGizmoVisible();
    void BeginGizmoTransform();
    void EndGizmoTransform();

    ETransformSpace CurrentTransformSpace();
    void RecalculateSelectionBounds();
    void ExpandSelectionBounds(CSceneNode *pNode);
    void SelectNode(CSceneNode *pNode);
    void DeselectNode(CSceneNode *pNode);
    void ClearSelection();
    void ClearAndSelectNode(CSceneNode *pNode);

    virtual void UpdateGizmoUI() = 0;
    virtual void UpdateSelectionUI() = 0;

public slots:
    void OnGizmoMoved();

protected:
    virtual void GizmoModeChanged(CGizmo::EGizmoMode /*mode*/) {}

private slots:
    void OnSelectObjectsTriggered();
    void OnTranslateTriggered();
    void OnRotateTriggered();
    void OnScaleTriggered();
    void OnTransformSpaceChanged(int spaceIndex);
};

#endif // INODEEDITOR_H
