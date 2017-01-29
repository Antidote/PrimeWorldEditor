#include "CWorldEditor.h"
#include "ui_CWorldEditor.h"
#include "CConfirmUnlinkDialog.h"
#include "CLayerEditor.h"
#include "CRepackInfoDialog.h"
#include "CTemplateMimeData.h"
#include "WModifyTab.h"
#include "WInstancesTab.h"

#include "Editor/CBasicViewport.h"
#include "Editor/CNodeCopyMimeData.h"
#include "Editor/CPakToolDialog.h"
#include "Editor/CSelectionIterator.h"
#include "Editor/UICommon.h"
#include "Editor/PropertyEdit/CPropertyView.h"
#include "Editor/Widgets/WDraggableSpinBox.h"
#include "Editor/Widgets/WVectorEditor.h"
#include "Editor/Undo/UndoCommands.h"

#include <Core/Render/CDrawUtil.h>
#include <Core/Resource/Cooker/CAreaCooker.h>
#include <Core/Scene/CSceneIterator.h>
#include <Common/Log.h>

#include <iostream>
#include <QClipboard>
#include <QComboBox>
#include <QFontMetrics>
#include <QMessageBox>
#include <QOpenGLContext>

CWorldEditor::CWorldEditor(QWidget *parent)
    : INodeEditor(parent)
    , ui(new Ui::CWorldEditor)
    , mpArea(nullptr)
    , mpWorld(nullptr)
    , mpLinkDialog(new CLinkDialog(this, this))
    , mpPoiDialog(nullptr)
    , mIsMakingLink(false)
    , mpNewLinkSender(nullptr)
    , mpNewLinkReceiver(nullptr)
{
    Log::Write("Creating World Editor");
    ui->setupUi(this);
    REPLACE_WINDOWTITLE_APPVARS;

    mpSelection->SetAllowedNodeTypes(eScriptNode | eLightNode);

    // Initialize splitter
    QList<int> SplitterSizes;
    SplitterSizes << width() * 0.775 << width() * 0.225;
    ui->splitter->setSizes(SplitterSizes);

    // Initialize UI stuff
    ui->MainViewport->SetScene(this, &mScene);
    ui->CreateTabContents->SetEditor(this);
    ui->ModifyTabContents->SetEditor(this);
    ui->InstancesTabContents->SetEditor(this, &mScene);
    ui->TransformSpinBox->SetOrientation(Qt::Horizontal);
    ui->TransformSpinBox->layout()->setContentsMargins(0,0,0,0);
    ui->CamSpeedSpinBox->SetDefaultValue(1.0);

    mpTransformCombo->setMinimumWidth(75);
    ui->MainToolBar->addActions(mGizmoActions);
    ui->MainToolBar->addWidget(mpTransformCombo);
    ui->menuEdit->insertActions(ui->ActionCut, mUndoActions);
    ui->menuEdit->insertSeparator(ui->ActionCut);

    // Initialize actions
    addAction(ui->ActionIncrementGizmo);
    addAction(ui->ActionDecrementGizmo);

    QAction *pToolBarUndo = mUndoStack.createUndoAction(this);
    pToolBarUndo->setIcon(QIcon(":/icons/Undo.png"));
    ui->MainToolBar->insertAction(ui->ActionLink, pToolBarUndo);

    QAction *pToolBarRedo = mUndoStack.createRedoAction(this);
    pToolBarRedo->setIcon(QIcon(":/icons/Redo.png"));
    ui->MainToolBar->insertAction(ui->ActionLink, pToolBarRedo);
    ui->MainToolBar->insertSeparator(ui->ActionLink);

    ui->ActionCut->setAutoRepeat(false);
    ui->ActionCut->setShortcut(QKeySequence::Cut);
    ui->ActionCopy->setAutoRepeat(false);
    ui->ActionCopy->setShortcut(QKeySequence::Copy);
    ui->ActionPaste->setAutoRepeat(false);
    ui->ActionPaste->setShortcut(QKeySequence::Paste);
    ui->ActionDelete->setAutoRepeat(false);
    ui->ActionDelete->setShortcut(QKeySequence::Delete);

    mpCollisionDialog = new CCollisionRenderSettingsDialog(this, this);

    // Connect signals and slots
    connect(ui->MainViewport, SIGNAL(ViewportClick(SRayIntersection,QMouseEvent*)), this, SLOT(OnViewportClick(SRayIntersection,QMouseEvent*)));
    connect(ui->MainViewport, SIGNAL(InputProcessed(SRayIntersection,QMouseEvent*)), this, SLOT(OnViewportInputProcessed(SRayIntersection,QMouseEvent*)));
    connect(ui->MainViewport, SIGNAL(InputProcessed(SRayIntersection,QMouseEvent*)), this, SLOT(UpdateGizmoUI()) );
    connect(ui->MainViewport, SIGNAL(InputProcessed(SRayIntersection,QMouseEvent*)), this, SLOT(UpdateStatusBar()) );
    connect(ui->MainViewport, SIGNAL(InputProcessed(SRayIntersection,QMouseEvent*)), this, SLOT(UpdateCursor()) );
    connect(ui->MainViewport, SIGNAL(GizmoMoved()), this, SLOT(OnGizmoMoved()));
    connect(ui->MainViewport, SIGNAL(CameraOrbit()), this, SLOT(UpdateCameraOrbit()));
    connect(this, SIGNAL(SelectionModified()), this, SLOT(OnSelectionModified()));
    connect(this, SIGNAL(SelectionTransformed()), this, SLOT(UpdateCameraOrbit()));
    connect(this, SIGNAL(PickModeEntered(QCursor)), this, SLOT(OnPickModeEnter(QCursor)));
    connect(this, SIGNAL(PickModeExited()), this, SLOT(OnPickModeExit()));
    connect(ui->TransformSpinBox, SIGNAL(ValueChanged(CVector3f)), this, SLOT(OnTransformSpinBoxModified(CVector3f)));
    connect(ui->TransformSpinBox, SIGNAL(EditingDone(CVector3f)), this, SLOT(OnTransformSpinBoxEdited(CVector3f)));
    connect(ui->CamSpeedSpinBox, SIGNAL(valueChanged(double)), this, SLOT(OnCameraSpeedChange(double)));
    connect(qApp->clipboard(), SIGNAL(dataChanged()), this, SLOT(OnClipboardDataModified()));
    connect(&mUndoStack, SIGNAL(indexChanged(int)), this, SLOT(OnUndoStackIndexChanged()));

    connect(ui->ActionSave, SIGNAL(triggered()), this, SLOT(Save()));
    connect(ui->ActionSaveAndRepack, SIGNAL(triggered()), this, SLOT(SaveAndRepack()));
    connect(ui->ActionCut, SIGNAL(triggered()), this, SLOT(Cut()));
    connect(ui->ActionCopy, SIGNAL(triggered()), this, SLOT(Copy()));
    connect(ui->ActionPaste, SIGNAL(triggered()), this, SLOT(Paste()));
    connect(ui->ActionDelete, SIGNAL(triggered()), this, SLOT(DeleteSelection()));
    connect(ui->ActionSelectAll, SIGNAL(triggered()), this, SLOT(SelectAllTriggered()));
    connect(ui->ActionInvertSelection, SIGNAL(triggered()), this, SLOT(InvertSelectionTriggered()));
    connect(ui->ActionLink, SIGNAL(toggled(bool)), this, SLOT(OnLinkButtonToggled(bool)));
    connect(ui->ActionUnlink, SIGNAL(triggered()), this, SLOT(OnUnlinkClicked()));

    connect(ui->ActionDrawWorld, SIGNAL(triggered()), this, SLOT(ToggleDrawWorld()));
    connect(ui->ActionDrawObjects, SIGNAL(triggered()), this, SLOT(ToggleDrawObjects()));
    connect(ui->ActionDrawCollision, SIGNAL(triggered()), this, SLOT(ToggleDrawCollision()));
    connect(ui->ActionDrawObjectCollision, SIGNAL(triggered()), this, SLOT(ToggleDrawObjectCollision()));
    connect(ui->ActionDrawLights, SIGNAL(triggered()), this, SLOT(ToggleDrawLights()));
    connect(ui->ActionDrawSky, SIGNAL(triggered()), this, SLOT(ToggleDrawSky()));
    connect(ui->ActionGameMode, SIGNAL(triggered()), this, SLOT(ToggleGameMode()));
    connect(ui->ActionDisableAlpha, SIGNAL(triggered()), this, SLOT(ToggleDisableAlpha()));
    connect(ui->ActionNoLighting, SIGNAL(triggered()), this, SLOT(SetNoLighting()));
    connect(ui->ActionBasicLighting, SIGNAL(triggered()), this, SLOT(SetBasicLighting()));
    connect(ui->ActionWorldLighting, SIGNAL(triggered()), this, SLOT(SetWorldLighting()));
    connect(ui->ActionNoBloom, SIGNAL(triggered()), this, SLOT(SetNoBloom()));
    connect(ui->ActionBloomMaps, SIGNAL(triggered()), this, SLOT(SetBloomMaps()));
    connect(ui->ActionFakeBloom, SIGNAL(triggered()), this, SLOT(SetFakeBloom()));
    connect(ui->ActionBloom, SIGNAL(triggered()), this, SLOT(SetBloom()));
    connect(ui->ActionIncrementGizmo, SIGNAL(triggered()), this, SLOT(IncrementGizmo()));
    connect(ui->ActionDecrementGizmo, SIGNAL(triggered()), this, SLOT(DecrementGizmo()));
    connect(ui->ActionCollisionRenderSettings, SIGNAL(triggered()), this, SLOT(EditCollisionRenderSettings()));
    connect(ui->ActionEditLayers, SIGNAL(triggered()), this, SLOT(EditLayers()));
    connect(ui->ActionEditPoiToWorldMap, SIGNAL(triggered()), this, SLOT(EditPoiToWorldMap()));

    ui->CreateTabEditorProperties->SyncToEditor(this);
    ui->ModifyTabEditorProperties->SyncToEditor(this);
    ui->InstancesTabEditorProperties->SyncToEditor(this);
    ui->MainViewport->setAcceptDrops(true);
}

CWorldEditor::~CWorldEditor()
{
    delete ui;
}

void CWorldEditor::closeEvent(QCloseEvent *pEvent)
{
    bool ShouldClose = CheckUnsavedChanges();

    if (ShouldClose)
    {
        mUndoStack.clear();
        mpCollisionDialog->close();
        mpLinkDialog->close();

        if (mpPoiDialog)
            mpPoiDialog->close();

        IEditor::closeEvent(pEvent);
    }
    else
    {
        pEvent->ignore();
    }
}

void CWorldEditor::SetArea(CWorld *pWorld, CGameArea *pArea)
{
    ExitPickMode();
    ui->MainViewport->ResetHover();
    ClearSelection();
    ui->ModifyTabContents->ClearUI();
    ui->InstancesTabContents->SetMaster(nullptr);
    ui->InstancesTabContents->SetArea(pArea);
    mUndoStack.clear();

    if (mpPoiDialog)
    {
        delete mpPoiDialog;
        mpPoiDialog = nullptr;
    }

    // Clear old area - hack until better world/area loader is implemented
    if ((mpArea) && (pArea != mpArea))
        mpArea->ClearScriptLayers();

    // Load new area
    mpArea = pArea;
    mpWorld = pWorld;
    mScene.SetActiveWorld(pWorld);
    mScene.SetActiveArea(pArea);

    // Snap camera to new area
    CCamera *pCamera = &ui->MainViewport->Camera();

    if (pCamera->MoveMode() == eFreeCamera)
    {
        CTransform4f AreaTransform = pArea->Transform();
        CVector3f AreaPosition(AreaTransform[0][3], AreaTransform[1][3], AreaTransform[2][3]);
        pCamera->Snap(AreaPosition);
    }

    UpdateCameraOrbit();

    // Default bloom to Fake Bloom for Metroid Prime 3; disable for other games
    bool AllowBloom = (mpWorld->Game() == eCorruptionProto || mpWorld->Game() == eCorruption);
    AllowBloom ? SetFakeBloom() : SetNoBloom();
    ui->menuBloom->setEnabled(AllowBloom);

    // Disable EGMC editing for Prime 1 and DKCR
    bool AllowEGMC = ( (mpWorld->Game() >= eEchoesDemo) && (mpWorld->Game() <= eCorruption) );
    ui->ActionEditPoiToWorldMap->setEnabled(AllowEGMC);

    // Set up sidebar tabs
    CMasterTemplate *pMaster = CMasterTemplate::MasterForGame(mpArea->Game());
    ui->CreateTabContents->SetMaster(pMaster);
    ui->InstancesTabContents->SetMaster(pMaster);

    // Set up dialogs
    mpCollisionDialog->SetupWidgets(); // Won't modify any settings but will update widget visibility status if we've changed games
    mpLinkDialog->SetMaster(pMaster);

    // Set window title
    CStringTable *pWorldNameTable = mpWorld->WorldName();
    TWideString WorldName = pWorldNameTable ? pWorldNameTable->String("ENGL", 0) : "[Untitled World]";

    if (CurrentGame() < eReturns)
    {
        CStringTable *pAreaNameTable = mpWorld->AreaName(mpArea->WorldIndex());
        TWideString AreaName = pAreaNameTable ? pAreaNameTable->String("ENGL", 0) : (TWideString("!") + mpWorld->AreaInternalName(mpArea->WorldIndex()).ToUTF16());

        if (AreaName.IsEmpty())
            AreaName = "[Untitled Area]";

        SET_WINDOWTITLE_APPVARS( QString("%APP_FULL_NAME% - %1 - %2[*]").arg(TO_QSTRING(WorldName)).arg(TO_QSTRING(AreaName)) );
        Log::Write("Loaded area: " + mpArea->Source() + " (" + TO_TSTRING(TO_QSTRING(AreaName)) + ")");
    }

    else
    {
        QString LevelName;
        if (pWorldNameTable) LevelName = TO_QSTRING(WorldName);
        else LevelName = "!" + TO_QSTRING(mpWorld->AreaInternalName(mpArea->WorldIndex()));

        SET_WINDOWTITLE_APPVARS( QString("%APP_FULL_NAME% - %1[*]").arg(LevelName) );
        Log::Write("Loaded level: World " + mpWorld->Source() + " / Area " + mpArea->Source() + " (" + TO_TSTRING(LevelName) + ")");
    }

    // Update paste action
    OnClipboardDataModified();

    // Emit signals
    emit LayersModified();
}

bool CWorldEditor::CheckUnsavedChanges()
{
    // Check whether the user has unsaved changes, return whether it's okay to clear the scene
    bool OkToClear = !isWindowModified();

    if (!OkToClear)
    {
        int Result = QMessageBox::warning(this, "Save", "You have unsaved changes. Save?", QMessageBox::Yes, QMessageBox::No, QMessageBox::Cancel);

        if (Result == QMessageBox::Yes)
            OkToClear = Save();

        else if (Result == QMessageBox::No)
            OkToClear = true;

        else if (Result == QMessageBox::Cancel)
            OkToClear = false;
    }

    return OkToClear;
}

bool CWorldEditor::HasAnyScriptNodesSelected() const
{
    for (CSelectionIterator It(mpSelection); It; ++It)
    {
        if (It->NodeType() == eScriptNode)
            return true;
    }

    return false;
}

CSceneViewport* CWorldEditor::Viewport() const
{
    return ui->MainViewport;
}

// ************ PUBLIC SLOTS ************
void CWorldEditor::NotifyNodeAboutToBeDeleted(CSceneNode *pNode)
{
    INodeEditor::NotifyNodeAboutToBeDeleted(pNode);

    if (ui->MainViewport->HoverNode() == pNode)
        ui->MainViewport->ResetHover();
}

void CWorldEditor::Cut()
{
    if (!mpSelection->IsEmpty())
    {
        Copy();
        mUndoStack.push(new CDeleteSelectionCommand(this, "Cut"));
    }
}

void CWorldEditor::Copy()
{
    if (!mpSelection->IsEmpty())
    {
        CNodeCopyMimeData *pMimeData = new CNodeCopyMimeData(this);
        qApp->clipboard()->setMimeData(pMimeData);
    }
}

void CWorldEditor::Paste()
{
    if (const CNodeCopyMimeData *pkMimeData =
            qobject_cast<const CNodeCopyMimeData*>(qApp->clipboard()->mimeData()))
    {
        if (pkMimeData->Game() == CurrentGame())
        {
            CVector3f PastePoint;

            if (ui->MainViewport->underMouse() && !ui->MainViewport->IsMouseInputActive())
                PastePoint = ui->MainViewport->HoverPoint();

            else
            {
                CRay Ray = ui->MainViewport->Camera().CastRay(CVector2f(0.f, 0.f));
                SRayIntersection Intersect = ui->MainViewport->SceneRayCast(Ray);

                if (Intersect.Hit)
                    PastePoint = Intersect.HitPoint;
                else
                    PastePoint = Ray.PointOnRay(10.f);
            }

            CPasteNodesCommand *pCmd = new CPasteNodesCommand(this, ui->CreateTabContents->SpawnLayer(), PastePoint);
            mUndoStack.push(pCmd);
        }
    }
}

bool CWorldEditor::Save()
{
    TString Out = mpArea->FullSource();
    CFileOutStream MREA(Out.ToStdString(), IOUtil::eBigEndian);

    if (MREA.IsValid())
    {
        CAreaCooker::WriteCookedArea(mpArea, MREA);
        mUndoStack.setClean();
        setWindowModified(false);
        return true;
    }
    else
    {
        QMessageBox::warning(this, "Error", "Unable to save error; couldn't open output file " + TO_QSTRING(Out));
        return false;
    }
}

bool CWorldEditor::SaveAndRepack()
{
    if (!Save()) return false;

    if (!CanRepack())
    {
        CRepackInfoDialog Dialog(mWorldDir, mPakFileList, mPakTarget, this);
        Dialog.exec();

        if (Dialog.result() == QDialog::Accepted)
        {
            SetWorldDir(Dialog.TargetFolder());
            SetPakFileList(Dialog.ListFile());
            SetPakTarget(Dialog.OutputPak());
            if (!CanRepack()) return false;
        }

        else return false;
    }

    QString PakOut;
    CPakToolDialog::EResult Result = CPakToolDialog::Repack(CurrentGame(), mPakTarget, mPakFileList, mWorldDir, &PakOut, this);

    if (Result == CPakToolDialog::eError)
        QMessageBox::warning(this, "Error", "Failed to repack!");
    else if (Result == CPakToolDialog::eSuccess)
        QMessageBox::information(this, "Success", "Successfully saved pak: " + PakOut);

    return (Result == CPakToolDialog::eSuccess);
}

void CWorldEditor::OnLinksModified(const QList<CScriptObject*>& rkInstances)
{
    foreach (CScriptObject *pInstance, rkInstances)
    {
        CScriptNode *pNode = mScene.NodeForInstance(pInstance);
        pNode->LinksModified();
    }

    if (!rkInstances.isEmpty())
        emit InstanceLinksModified(rkInstances);
}

void CWorldEditor::OnPropertyModified(IProperty *pProp)
{
    bool EditorProperty = false;

    if (!mpSelection->IsEmpty() && mpSelection->Front()->NodeType() == eScriptNode)
    {
        CScriptNode *pScript = static_cast<CScriptNode*>(mpSelection->Front());
        pScript->PropertyModified(pProp);

        // Check editor property
        if (pScript->Instance()->IsEditorProperty(pProp))
            EditorProperty = true;

        // If this is an editor property, update other parts of the UI to reflect the new value.
        if (EditorProperty)
        {
            UpdateStatusBar();
            UpdateSelectionUI();
        }

        // If this is a model/character, then we'll treat this as a modified selection. This is to make sure the selection bounds updates.
        if (pProp->Type() == eAssetProperty)
        {
            CAssetTemplate *pAsset = static_cast<CAssetTemplate*>(pProp->Template());

            if (pAsset->AcceptsExtension("CMDL") || pAsset->AcceptsExtension("ANCS") || pAsset->AcceptsExtension("CHAR"))
                SelectionModified();
        }
        else if (pProp->Type() == eCharacterProperty)
            SelectionModified();

        // Emit signal so other widgets can react to the property change
        emit PropertyModified(pScript->Instance(), pProp);
    }
}

void CWorldEditor::SetSelectionActive(bool Active)
{
    // Gather list of selected objects that actually have Active properties
    QVector<CScriptObject*> Objects;

    for (CSelectionIterator It(mpSelection); It; ++It)
    {
        if (It->NodeType() == eScriptNode)
        {
            CScriptNode *pScript = static_cast<CScriptNode*>(*It);
            CScriptObject *pInst = pScript->Instance();
            IProperty *pActive = pInst->ActiveProperty();

            if (pActive)
                Objects << pInst;
        }
    }

    if (!Objects.isEmpty())
    {
        mUndoStack.beginMacro("Toggle Active");

        foreach (CScriptObject *pInst, Objects)
        {
            IProperty *pActive = pInst->ActiveProperty();
            IPropertyValue *pOld = pActive->RawValue()->Clone();
            pInst->SetActive(Active);
            mUndoStack.push(new CEditScriptPropertyCommand(pActive, this, pOld, true));
        }

        mUndoStack.endMacro();
    }
}

void CWorldEditor::SetSelectionInstanceNames(const QString& rkNewName, bool IsDone)
{
    // todo: this only supports one node at a time because a macro prevents us from merging undo commands
    // this is fine right now because this function is only ever called with a selection of one node, but probably want to fix in the future
    if (mpSelection->Size() == 1 && mpSelection->Front()->NodeType() == eScriptNode)
    {
        CScriptNode *pNode = static_cast<CScriptNode*>(mpSelection->Front());
        CScriptObject *pInst = pNode->Instance();
        IProperty *pName = pInst->InstanceNameProperty();

        if (pName)
        {
            TString NewName = TO_TSTRING(rkNewName);
            IPropertyValue *pOld = pName->RawValue()->Clone();
            pInst->SetName(NewName);
            mUndoStack.push(new CEditScriptPropertyCommand(pName, this, pOld, IsDone, "Edit Instance Name"));
        }
    }
}

void CWorldEditor::SetSelectionLayer(CScriptLayer *pLayer)
{
    QList<CScriptNode*> ScriptNodes;

    for (CSelectionIterator It(mpSelection); It; ++It)
    {
        if (It->NodeType() == eScriptNode)
            ScriptNodes << static_cast<CScriptNode*>(*It);
    }

    if (!ScriptNodes.isEmpty())
        mUndoStack.push(new CChangeLayerCommand(this, ScriptNodes, pLayer));
}

void CWorldEditor::DeleteSelection()
{
    if (HasAnyScriptNodesSelected())
    {
        CDeleteSelectionCommand *pCmd = new CDeleteSelectionCommand(this);
        mUndoStack.push(pCmd);
    }
}

void CWorldEditor::UpdateStatusBar()
{
    // Would be cool to do more frequent status bar updates with more info. Unfortunately, this causes lag.
    QString StatusText = "";

    if (!mGizmoHovering)
    {
        if (ui->MainViewport->underMouse())
        {
            CSceneNode *pHoverNode = ui->MainViewport->HoverNode();

            if (pHoverNode && mpSelection->IsAllowedType(pHoverNode))
                StatusText = TO_QSTRING(pHoverNode->Name());
        }
    }

    if (ui->statusbar->currentMessage() != StatusText)
        ui->statusbar->showMessage(StatusText);
}

void CWorldEditor::UpdateGizmoUI()
{
    // Update transform XYZ spin boxes
    if (!ui->TransformSpinBox->IsBeingEdited())
    {
        CVector3f SpinBoxValue = CVector3f::skZero;

        // If the gizmo is transforming, use the total transform amount
        // Otherwise, use the first selected node transform, or 0 if no selection
        if (mShowGizmo)
        {
            switch (mGizmo.Mode())
            {
            case CGizmo::eTranslate:
                if (mGizmoTransforming && mGizmo.HasTransformed())
                    SpinBoxValue = mGizmo.TotalTranslation();
                else if (!mpSelection->IsEmpty())
                    SpinBoxValue = mpSelection->Front()->AbsolutePosition();
                break;

            case CGizmo::eRotate:
                if (mGizmoTransforming && mGizmo.HasTransformed())
                    SpinBoxValue = mGizmo.TotalRotation();
                else if (!mpSelection->IsEmpty())
                    SpinBoxValue = mpSelection->Front()->AbsoluteRotation().ToEuler();
                break;

            case CGizmo::eScale:
                if (mGizmoTransforming && mGizmo.HasTransformed())
                    SpinBoxValue = mGizmo.TotalScale();
                else if (!mpSelection->IsEmpty())
                    SpinBoxValue = mpSelection->Front()->AbsoluteScale();
                break;
            }
        }
        else if (!mpSelection->IsEmpty()) SpinBoxValue = mpSelection->Front()->AbsolutePosition();

        ui->TransformSpinBox->blockSignals(true);
        ui->TransformSpinBox->SetValue(SpinBoxValue);
        ui->TransformSpinBox->blockSignals(false);
    }

    // Update gizmo
    if (!mGizmoTransforming)
    {
        // Set gizmo transform
        if (!mpSelection->IsEmpty())
        {
            mGizmo.SetPosition(mpSelection->Front()->AbsolutePosition());
            mGizmo.SetLocalRotation(mpSelection->Front()->AbsoluteRotation());
        }
    }
}

void CWorldEditor::UpdateSelectionUI()
{
    // Update selection info text
    QString SelectionText;

    if (mpSelection->Size() == 1)
        SelectionText = TO_QSTRING(mpSelection->Front()->Name());
    else if (mpSelection->Size() > 1)
        SelectionText = QString("%1 objects selected").arg(mpSelection->Size());

    QFontMetrics Metrics(ui->SelectionInfoLabel->font());
    SelectionText = Metrics.elidedText(SelectionText, Qt::ElideRight, ui->SelectionInfoFrame->width() - 10);

    if (ui->SelectionInfoLabel->text() != SelectionText)
        ui->SelectionInfoLabel->setText(SelectionText);

    // Update gizmo stuff
    UpdateGizmoUI();
}

void CWorldEditor::UpdateCursor()
{
    if (ui->MainViewport->IsCursorVisible() && !mPickMode)
    {
        CSceneNode *pHoverNode = ui->MainViewport->HoverNode();

        if (ui->MainViewport->IsHoveringGizmo())
            ui->MainViewport->SetCursorState(Qt::SizeAllCursor);
        else if ((pHoverNode) && mpSelection->IsAllowedType(pHoverNode))
            ui->MainViewport->SetCursorState(Qt::PointingHandCursor);
        else
            ui->MainViewport->SetCursorState(Qt::ArrowCursor);
    }
}

void CWorldEditor::UpdateNewLinkLine()
{
    // Check if there is a sender+receiver
    if (mpLinkDialog->isVisible() && mpLinkDialog->Sender() && mpLinkDialog->Receiver() && !mpLinkDialog->IsPicking())
    {
        CVector3f Start = mScene.NodeForInstance(mpLinkDialog->Sender())->CenterPoint();
        CVector3f End = mScene.NodeForInstance(mpLinkDialog->Receiver())->CenterPoint();
        ui->MainViewport->SetLinkLineEnabled(true);
        ui->MainViewport->SetLinkLine(Start, End);
    }

    // Otherwise check whether there's just a sender or just a receiver
    else
    {
        CScriptObject *pSender = nullptr;
        CScriptObject *pReceiver = nullptr;

        if (mpLinkDialog->isVisible())
        {
            if (mpLinkDialog->Sender() && !mpLinkDialog->IsPickingSender())
                pSender = mpLinkDialog->Sender();
            if (mpLinkDialog->Receiver() && !mpLinkDialog->IsPickingReceiver())
                pReceiver = mpLinkDialog->Receiver();
        }
        else if (mIsMakingLink && mpNewLinkSender)
            pSender = mpNewLinkSender;
        else if (ui->ModifyTabContents->IsPicking() && ui->ModifyTabContents->EditNode()->NodeType() == eScriptNode)
            pSender = static_cast<CScriptNode*>(ui->ModifyTabContents->EditNode())->Instance();

        // No sender and no receiver = no line
        if (!pSender && !pReceiver)
            ui->MainViewport->SetLinkLineEnabled(false);

        // Yes sender and yes receiver = yes line
        else if (pSender && pReceiver)
        {
            ui->MainViewport->SetLinkLineEnabled(true);
            ui->MainViewport->SetLinkLine( mScene.NodeForInstance(pSender)->CenterPoint(), mScene.NodeForInstance(pReceiver)->CenterPoint() );
        }

        // Compensate for missing sender or missing receiver
        else
        {
            bool IsPicking = (mIsMakingLink || mpLinkDialog->IsPicking() || ui->ModifyTabContents->IsPicking());

            if (ui->MainViewport->underMouse() && !ui->MainViewport->IsMouseInputActive() && IsPicking)
            {
                CSceneNode *pHoverNode = ui->MainViewport->HoverNode();
                CScriptObject *pInst = (pSender ? pSender : pReceiver);

                CVector3f Start = mScene.NodeForInstance(pInst)->CenterPoint();
                CVector3f End = (pHoverNode && pHoverNode->NodeType() == eScriptNode ? pHoverNode->CenterPoint() : ui->MainViewport->HoverPoint());
                ui->MainViewport->SetLinkLineEnabled(true);
                ui->MainViewport->SetLinkLine(Start, End);
            }

            else
                ui->MainViewport->SetLinkLineEnabled(false);
        }
    }
}

// ************ PROTECTED ************
void CWorldEditor::GizmoModeChanged(CGizmo::EGizmoMode mode)
{
    ui->TransformSpinBox->SetSingleStep( (mode == CGizmo::eRotate ? 1.0 : 0.1) );
    ui->TransformSpinBox->SetDefaultValue( (mode == CGizmo::eScale ? 1.0 : 0.0) );
}

// ************ PRIVATE SLOTS ************
void CWorldEditor::OnClipboardDataModified()
{
    const QMimeData *pkClipboardMimeData = qApp->clipboard()->mimeData();
    const CNodeCopyMimeData *pkMimeData = qobject_cast<const CNodeCopyMimeData*>(pkClipboardMimeData);
    bool ValidMimeData = (pkMimeData && pkMimeData->Game() == CurrentGame());
    ui->ActionPaste->setEnabled(ValidMimeData);
}

void CWorldEditor::OnSelectionModified()
{
    ui->TransformSpinBox->setEnabled(!mpSelection->IsEmpty());

    bool HasScriptNode = HasAnyScriptNodesSelected();
    ui->ActionCut->setEnabled(HasScriptNode);
    ui->ActionCopy->setEnabled(HasScriptNode);
    ui->ActionDelete->setEnabled(HasScriptNode);

    UpdateCameraOrbit();
}

void CWorldEditor::OnLinkButtonToggled(bool Enabled)
{
    if (Enabled)
    {
        EnterPickMode(eScriptNode, true, false, false);
        connect(this, SIGNAL(PickModeClick(SRayIntersection,QMouseEvent*)), this, SLOT(OnLinkClick(SRayIntersection)));
        connect(this, SIGNAL(PickModeExited()), this, SLOT(OnLinkEnd()));
        mIsMakingLink = true;
        mpNewLinkSender = nullptr;
        mpNewLinkReceiver = nullptr;
    }

    else
    {
        if (mIsMakingLink)
            ExitPickMode();
    }
}

void CWorldEditor::OnLinkClick(const SRayIntersection& rkIntersect)
{
    if (!mpNewLinkSender)
    {
        mpNewLinkSender = static_cast<CScriptNode*>(rkIntersect.pNode)->Instance();
    }

    else
    {
        mpNewLinkReceiver = static_cast<CScriptNode*>(rkIntersect.pNode)->Instance();
        mpLinkDialog->NewLink(mpNewLinkSender, mpNewLinkReceiver);
        mpLinkDialog->show();
        ExitPickMode();
    }
}

void CWorldEditor::OnLinkEnd()
{
    disconnect(this, SIGNAL(PickModeClick(SRayIntersection,QMouseEvent*)), this, SLOT(OnLinkClick(SRayIntersection)));
    disconnect(this, SIGNAL(PickModeExited()), this, SLOT(OnLinkEnd()));
    ui->ActionLink->setChecked(false);
    mIsMakingLink = false;
    mpNewLinkSender = nullptr;
    mpNewLinkReceiver = nullptr;
}

void CWorldEditor::OnUnlinkClicked()
{
    QList<CScriptNode*> SelectedScriptNodes;

    for (CSelectionIterator It(mpSelection); It; ++It)
    {
        if (It->NodeType() == eScriptNode)
            SelectedScriptNodes << static_cast<CScriptNode*>(*It);
    }

    if (!SelectedScriptNodes.isEmpty())
    {
        CConfirmUnlinkDialog Dialog(this);
        Dialog.exec();

        if (Dialog.UserChoice() != CConfirmUnlinkDialog::eCancel)
        {
            mUndoStack.beginMacro("Unlink");
            bool UnlinkIncoming = (Dialog.UserChoice() != CConfirmUnlinkDialog::eOutgoingOnly);
            bool UnlinkOutgoing = (Dialog.UserChoice() != CConfirmUnlinkDialog::eIncomingOnly);

            foreach (CScriptNode *pNode, SelectedScriptNodes)
            {
                CScriptObject *pInst = pNode->Instance();

                if (UnlinkIncoming)
                {
                    QVector<u32> LinkIndices;
                    for (u32 iLink = 0; iLink < pInst->NumLinks(eIncoming); iLink++)
                        LinkIndices << iLink;

                    CDeleteLinksCommand *pCmd = new CDeleteLinksCommand(this, pInst, eIncoming, LinkIndices);
                    mUndoStack.push(pCmd);
                }

                if (UnlinkOutgoing)
                {
                    QVector<u32> LinkIndices;
                    for (u32 iLink = 0; iLink < pInst->NumLinks(eOutgoing); iLink++)
                        LinkIndices << iLink;

                    CDeleteLinksCommand *pCmd = new CDeleteLinksCommand(this, pInst, eOutgoing, LinkIndices);
                    mUndoStack.push(pCmd);
                }
            }

            mUndoStack.endMacro();
        }
    }
}

void CWorldEditor::OnUndoStackIndexChanged()
{
    // Check the commands that have been executed on the undo stack and find out whether any of them affect the clean state.
    // This is to prevent commands like select/deselect from altering the clean state.
    int CurrentIndex = mUndoStack.index();
    int CleanIndex = mUndoStack.cleanIndex();

    if (CleanIndex == -1)
    {
        if (!isWindowModified())
            mUndoStack.setClean();

        return;
    }

    if (CurrentIndex == CleanIndex)
        setWindowModified(false);

    else
    {
        bool IsClean = true;
        int LowIndex = (CurrentIndex > CleanIndex ? CleanIndex : CurrentIndex);
        int HighIndex = (CurrentIndex > CleanIndex ? CurrentIndex - 1 : CleanIndex - 1);

        for (int iIdx = LowIndex; iIdx <= HighIndex; iIdx++)
        {
            const QUndoCommand *pkQCmd = mUndoStack.command(iIdx);

            if (const IUndoCommand *pkCmd = dynamic_cast<const IUndoCommand*>(pkQCmd))
            {
                if (pkCmd->AffectsCleanState())
                    IsClean = false;
            }

            else if (pkQCmd->childCount() > 0)
            {
                for (int iChild = 0; iChild < pkQCmd->childCount(); iChild++)
                {
                    const IUndoCommand *pkCmd = static_cast<const IUndoCommand*>(pkQCmd->child(iChild));

                    if (pkCmd->AffectsCleanState())
                    {
                        IsClean = false;
                        break;
                    }
                }
            }

            if (!IsClean) break;
        }

        setWindowModified(!IsClean);
    }
}

void CWorldEditor::OnPickModeEnter(QCursor Cursor)
{
    ui->MainViewport->SetCursorState(Cursor);
}

void CWorldEditor::OnPickModeExit()
{
    UpdateCursor();
}

void CWorldEditor::RefreshViewport()
{
    if (!mGizmo.IsTransforming())
        mGizmo.ResetSelectedAxes();

    // Process input
    ui->MainViewport->ProcessInput();

    // Update new link line
    UpdateNewLinkLine();

    // Render
    ui->MainViewport->Render();
}

void CWorldEditor::UpdateCameraOrbit()
{
    CCamera *pCamera = &ui->MainViewport->Camera();

    if (!mpSelection->IsEmpty())
        pCamera->SetOrbit(mpSelection->Bounds());
    else if (mpArea)
        pCamera->SetOrbit(mpArea->AABox(), 1.2f);
}

void CWorldEditor::OnCameraSpeedChange(double Speed)
{
    static const double skDefaultSpeed = 1.0;
    ui->MainViewport->Camera().SetMoveSpeed(skDefaultSpeed * Speed);

    ui->CamSpeedSpinBox->blockSignals(true);
    ui->CamSpeedSpinBox->setValue(Speed);
    ui->CamSpeedSpinBox->blockSignals(false);
}

void CWorldEditor::OnTransformSpinBoxModified(CVector3f Value)
{
    if (mpSelection->IsEmpty()) return;

    switch (mGizmo.Mode())
    {
        // Use absolute position/rotation, but relative scale. (This way spinbox doesn't show preview multiplier)
        case CGizmo::eTranslate:
        {
            CVector3f Delta = Value - mpSelection->Front()->AbsolutePosition();
            mUndoStack.push(new CTranslateNodeCommand(this, mpSelection->SelectedNodeList(), Delta, mTranslateSpace));
            break;
        }

        case CGizmo::eRotate:
        {
            CQuaternion Delta = CQuaternion::FromEuler(Value) * mpSelection->Front()->AbsoluteRotation().Inverse();
            mUndoStack.push(new CRotateNodeCommand(this, mpSelection->SelectedNodeList(), CVector3f::skZero, Delta, mRotateSpace));
            break;
        }

        case CGizmo::eScale:
        {
            CVector3f Delta = Value / mpSelection->Front()->AbsoluteScale();
            mUndoStack.push(new CScaleNodeCommand(this, mpSelection->SelectedNodeList(), CVector3f::skZero, Delta));
            break;
        }
    }

    UpdateGizmoUI();
}

void CWorldEditor::OnTransformSpinBoxEdited(CVector3f)
{
    if (mpSelection->IsEmpty()) return;

    if (mGizmo.Mode() == CGizmo::eTranslate)   mUndoStack.push(CTranslateNodeCommand::End());
    else if (mGizmo.Mode() == CGizmo::eRotate) mUndoStack.push(CRotateNodeCommand::End());
    else if (mGizmo.Mode() == CGizmo::eScale)  mUndoStack.push(CScaleNodeCommand::End());

    UpdateGizmoUI();
}

void CWorldEditor::OnClosePoiEditDialog()
{
    delete mpPoiDialog;
    mpPoiDialog = nullptr;
    ui->MainViewport->SetRenderMergedWorld(true);
}

void CWorldEditor::SelectAllTriggered()
{
    FNodeFlags NodeFlags = CScene::NodeFlagsForShowFlags(ui->MainViewport->ShowFlags());
    NodeFlags &= ~(eModelNode | eStaticNode | eCollisionNode);
    SelectAll(NodeFlags);
}

void CWorldEditor::InvertSelectionTriggered()
{
    FNodeFlags NodeFlags = CScene::NodeFlagsForShowFlags(ui->MainViewport->ShowFlags());
    NodeFlags &= ~(eModelNode | eStaticNode | eCollisionNode);
    InvertSelection(NodeFlags);
}

void CWorldEditor::ToggleDrawWorld()
{
    ui->MainViewport->SetShowWorld(ui->ActionDrawWorld->isChecked());
}

void CWorldEditor::ToggleDrawObjects()
{
    ui->MainViewport->SetShowFlag(eShowObjectGeometry, ui->ActionDrawObjects->isChecked());
}

void CWorldEditor::ToggleDrawCollision()
{
    ui->MainViewport->SetShowFlag(eShowWorldCollision, ui->ActionDrawCollision->isChecked());
}

void CWorldEditor::ToggleDrawObjectCollision()
{
    ui->MainViewport->SetShowFlag(eShowObjectCollision, ui->ActionDrawObjectCollision->isChecked());
}

void CWorldEditor::ToggleDrawLights()
{
    ui->MainViewport->SetShowFlag(eShowLights, ui->ActionDrawLights->isChecked());
}

void CWorldEditor::ToggleDrawSky()
{
    ui->MainViewport->SetShowFlag(eShowSky, ui->ActionDrawSky->isChecked());
}

void CWorldEditor::ToggleGameMode()
{
    ui->MainViewport->SetGameMode(ui->ActionGameMode->isChecked());
}

void CWorldEditor::ToggleDisableAlpha()
{
    ui->MainViewport->Renderer()->ToggleAlphaDisabled(ui->ActionDisableAlpha->isChecked());
}

void CWorldEditor::SetNoLighting()
{
    CGraphics::sLightMode = CGraphics::eNoLighting;
    ui->ActionNoLighting->setChecked(true);
    ui->ActionBasicLighting->setChecked(false);
    ui->ActionWorldLighting->setChecked(false);
}

void CWorldEditor::SetBasicLighting()
{
    CGraphics::sLightMode = CGraphics::eBasicLighting;
    ui->ActionNoLighting->setChecked(false);
    ui->ActionBasicLighting->setChecked(true);
    ui->ActionWorldLighting->setChecked(false);
}

void CWorldEditor::SetWorldLighting()
{
    CGraphics::sLightMode = CGraphics::eWorldLighting;
    ui->ActionNoLighting->setChecked(false);
    ui->ActionBasicLighting->setChecked(false);
    ui->ActionWorldLighting->setChecked(true);
}

void CWorldEditor::SetNoBloom()
{
    ui->MainViewport->Renderer()->SetBloom(CRenderer::eNoBloom);
    ui->ActionNoBloom->setChecked(true);
    ui->ActionBloomMaps->setChecked(false);
    ui->ActionFakeBloom->setChecked(false);
    ui->ActionBloom->setChecked(false);
}

void CWorldEditor::SetBloomMaps()
{
    ui->MainViewport->Renderer()->SetBloom(CRenderer::eBloomMaps);
    ui->ActionNoBloom->setChecked(false);
    ui->ActionBloomMaps->setChecked(true);
    ui->ActionFakeBloom->setChecked(false);
    ui->ActionBloom->setChecked(false);
}

void CWorldEditor::SetFakeBloom()
{
    ui->MainViewport->Renderer()->SetBloom(CRenderer::eFakeBloom);
    ui->ActionNoBloom->setChecked(false);
    ui->ActionBloomMaps->setChecked(false);
    ui->ActionFakeBloom->setChecked(true);
    ui->ActionBloom->setChecked(false);
}

void CWorldEditor::SetBloom()
{
    ui->MainViewport->Renderer()->SetBloom(CRenderer::eBloom);
    ui->ActionNoBloom->setChecked(false);
    ui->ActionBloomMaps->setChecked(false);
    ui->ActionFakeBloom->setChecked(false);
    ui->ActionBloom->setChecked(true);
}

void CWorldEditor::IncrementGizmo()
{
    mGizmo.IncrementSize();
}

void CWorldEditor::DecrementGizmo()
{
    mGizmo.DecrementSize();
}

void CWorldEditor::EditCollisionRenderSettings()
{
    mpCollisionDialog->show();
}

void CWorldEditor::EditLayers()
{
    // Launch layer editor
    CLayerEditor Editor(this);
    Editor.SetArea(mpArea);
    Editor.exec();
}

void CWorldEditor::EditPoiToWorldMap()
{
    if (!mpPoiDialog)
    {
        mpPoiDialog = new CPoiMapEditDialog(this, this);
        mpPoiDialog->show();
        ui->MainViewport->SetRenderMergedWorld(false);
        connect(mpPoiDialog, SIGNAL(Closed()), this, SLOT(OnClosePoiEditDialog()));
    }
    else
    {
        mpPoiDialog->show();
    }
}
