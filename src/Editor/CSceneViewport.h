#ifndef CSCENEVIEWPORT_H
#define CSCENEVIEWPORT_H

#include "CBasicViewport.h"
#include "CGridRenderable.h"
#include "CLineRenderable.h"
#include "INodeEditor.h"

class CSceneViewport : public CBasicViewport
{
    Q_OBJECT

    INodeEditor *mpEditor;
    CScene *mpScene;
    CRenderer *mpRenderer;
    bool mRenderingMergedWorld;

    // Scene interaction
    bool mGizmoHovering;
    bool mGizmoTransforming;
    SRayIntersection mRayIntersection;
    CSceneNode *mpHoverNode;
    CVector3f mHoverPoint;

    // Context Menu
    QMenu *mpContextMenu;
    QAction *mpToggleSelectAction;
    QAction *mpHideSelectionSeparator;
    QAction *mpHideSelectionAction;
    QAction *mpHideUnselectedAction;
    QAction *mpHideHoverSeparator;
    QAction *mpHideHoverNodeAction;
    QAction *mpHideHoverTypeAction;
    QAction *mpHideHoverLayerAction;
    QAction *mpUnhideSeparator;
    QAction *mpUnhideAllAction;
    CSceneNode *mpMenuNode;

    QMenu *mpSelectConnectedMenu;
    QAction *mpSelectConnectedOutgoingAction;
    QAction *mpSelectConnectedIncomingAction;
    QAction *mpSelectConnectedAllAction;

    // Grid
    CGridRenderable mGrid;

    // Link Line
    bool mLinkLineEnabled;
    CLineRenderable mLinkLine;

public:
    CSceneViewport(QWidget *pParent = 0);
    ~CSceneViewport();
    void SetScene(INodeEditor *pEditor, CScene *pScene);
    void SetShowWorld(bool Visible);
    void SetRenderMergedWorld(bool RenderMerged);
    FShowFlags ShowFlags() const;
    CRenderer* Renderer();
    CSceneNode* HoverNode();
    CVector3f HoverPoint();
    void CheckGizmoInput(const CRay& rkRay);
    SRayIntersection SceneRayCast(const CRay& rkRay);
    void ResetHover();
    bool IsHoveringGizmo();

    void keyPressEvent(QKeyEvent* pEvent);
    void keyReleaseEvent(QKeyEvent* pEvent);

    inline void SetLinkLineEnabled(bool Enable)                                     { mLinkLineEnabled = Enable; }
    inline void SetLinkLine(const CVector3f& rkPointA, const CVector3f& rkPointB)   { mLinkLine.SetPoints(rkPointA, rkPointB); }

protected:
    void CreateContextMenu();
    QMouseEvent CreateMouseEvent();
    void FindConnectedObjects(uint32 InstanceID, bool SearchOutgoing, bool SearchIncoming, QList<uint32>& rIDList);

signals:
    void InputProcessed(const SRayIntersection& rkIntersect, QMouseEvent *pEvent);
    void ViewportClick(const SRayIntersection& rkIntersect, QMouseEvent *pEvent);
    void GizmoMoved();
    void CameraOrbit();

protected slots:
    void CheckUserInput();
    void Paint();
    void ContextMenu(QContextMenuEvent *pEvent);
    void OnResize();
    void OnMouseClick(QMouseEvent *pEvent);
    void OnMouseRelease(QMouseEvent *pEvent);

    // Menu Actions
    void OnToggleSelect();
    void OnSelectConnected();
    void OnHideSelection();
    void OnHideUnselected();
    void OnHideNode();
    void OnHideType();
    void OnHideLayer();
    void OnUnhideAll();
    void OnContextMenuClose();
};

#endif // CSCENEVIEWPORT_H
