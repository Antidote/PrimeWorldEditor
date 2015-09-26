#ifndef CWORLDEDITOR_H
#define CWORLDEDITOR_H

#include "INodeEditor.h"

#include <QComboBox>
#include <QList>
#include <QMainWindow>
#include <QTimer>
#include <QUndoStack>

#include "CGizmo.h"
#include <Common/CRay.h>
#include <Common/CTimer.h>
#include <Common/EKeyInputs.h>
#include <Common/SRayIntersection.h>
#include <Common/ETransformSpace.h>
#include <Core/CRenderer.h>
#include <Core/CSceneManager.h>
#include <Core/CToken.h>
#include <Resource/CGameArea.h>
#include <Resource/CWorld.h>

namespace Ui {
class CWorldEditor;
}

class CWorldEditor : public INodeEditor
{
    Q_OBJECT
    Ui::CWorldEditor *ui;

    CWorld *mpWorld;
    CGameArea *mpArea;
    CToken mAreaToken;
    CToken mWorldToken;
    QTimer mRefreshTimer;

public:
    explicit CWorldEditor(QWidget *parent = 0);
    ~CWorldEditor();
    bool eventFilter(QObject *pObj, QEvent *pEvent);
    void SetArea(CWorld *pWorld, CGameArea *pArea);
    CGameArea* ActiveArea();

    // Update UI
    void UpdateGizmoUI();
    void UpdateSelectionUI();
    void UpdateStatusBar();

protected:
    void GizmoModeChanged(CGizmo::EGizmoMode mode);

private:
    void UpdateCursor();
    void OnSidebarResize();

private slots:
    void RefreshViewport();
    void OnCameraSpeedChange(double speed);
    void OnTransformSpinBoxModified(CVector3f value);
    void OnTransformSpinBoxEdited(CVector3f value);
    void on_ActionDrawWorld_triggered();
    void on_ActionDrawCollision_triggered();
    void on_ActionDrawObjects_triggered();
    void on_ActionDrawLights_triggered();
    void on_ActionDrawSky_triggered();
    void on_ActionNoLighting_triggered();
    void on_ActionBasicLighting_triggered();
    void on_ActionWorldLighting_triggered();
    void on_ActionNoBloom_triggered();
    void on_ActionBloomMaps_triggered();
    void on_ActionFakeBloom_triggered();
    void on_ActionBloom_triggered();
    void on_ActionZoomOnSelection_triggered();
    void on_ActionDisableBackfaceCull_triggered();
    void on_ActionDisableAlpha_triggered();
    void on_ActionEditLayers_triggered();
    void on_ActionIncrementGizmo_triggered();
    void on_ActionDecrementGizmo_triggered();
    void on_ActionDrawObjectCollision_triggered();
};

#endif // CWORLDEDITOR_H
