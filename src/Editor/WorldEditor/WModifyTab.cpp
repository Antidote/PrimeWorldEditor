#include "WModifyTab.h"
#include "ui_WModifyTab.h"

#include "CLinkDialog.h"
#include "CSelectInstanceDialog.h"
#include "CWorldEditor.h"
#include "Editor/Undo/UndoCommands.h"
#include <Core/Scene/CScriptNode.h>

#include <QScrollArea>
#include <QScrollBar>

WModifyTab::WModifyTab(QWidget *pParent)
    : QWidget(pParent)
    , ui(new Ui::WModifyTab)
    , mIsPicking(false)
{
    ui->setupUi(this);

    int PropViewWidth = ui->PropertyView->width();
    ui->PropertyView->header()->resizeSection(0, PropViewWidth * 0.3);
    ui->PropertyView->header()->resizeSection(1, PropViewWidth * 0.3);
    ui->PropertyView->header()->setSectionResizeMode(1, QHeaderView::Fixed);

    mpInLinkModel = new CLinkModel(this);
    mpInLinkModel->SetConnectionType(eIncoming);
    mpOutLinkModel = new CLinkModel(this);
    mpOutLinkModel->SetConnectionType(eOutgoing);

    mpAddFromViewportAction = new QAction("Choose from viewport", this);
    mpAddFromListAction = new QAction("Choose from list", this);
    mpAddLinkMenu = new QMenu(this);
    mpAddLinkMenu->addAction(mpAddFromViewportAction);
    mpAddLinkMenu->addAction(mpAddFromListAction);
    ui->AddOutgoingConnectionToolButton->setMenu(mpAddLinkMenu);
    ui->AddIncomingConnectionToolButton->setMenu(mpAddLinkMenu);

    ui->InLinksTableView->setModel(mpInLinkModel);
    ui->OutLinksTableView->setModel(mpOutLinkModel);
    ui->InLinksTableView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->OutLinksTableView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    connect(ui->InLinksTableView, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(OnLinkTableDoubleClick(QModelIndex)));
    connect(ui->OutLinksTableView, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(OnLinkTableDoubleClick(QModelIndex)));
    connect(ui->InLinksTableView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(OnLinksSelectionModified()));
    connect(ui->OutLinksTableView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(OnLinksSelectionModified()));
    connect(ui->AddOutgoingConnectionToolButton, SIGNAL(triggered(QAction*)), this, SLOT(OnAddLinkActionClicked(QAction*)));
    connect(ui->AddIncomingConnectionToolButton, SIGNAL(triggered(QAction*)), this, SLOT(OnAddLinkActionClicked(QAction*)));
    connect(ui->DeleteOutgoingConnectionButton, SIGNAL(clicked()), this, SLOT(OnDeleteLinksClicked()));
    connect(ui->DeleteIncomingConnectionButton, SIGNAL(clicked()), this, SLOT(OnDeleteLinksClicked()));
    connect(ui->EditOutgoingConnectionButton, SIGNAL(clicked()), this, SLOT(OnEditLinkClicked()));
    connect(ui->EditIncomingConnectionButton, SIGNAL(clicked()), this, SLOT(OnEditLinkClicked()));

    ClearUI();
}

WModifyTab::~WModifyTab()
{
    delete ui;
}

void WModifyTab::SetEditor(CWorldEditor *pEditor)
{
    mpWorldEditor = pEditor;
    ui->PropertyView->SetEditor(mpWorldEditor);
    connect(mpWorldEditor, SIGNAL(SelectionTransformed()), this, SLOT(OnWorldSelectionTransformed()));
    connect(mpWorldEditor, SIGNAL(InstanceLinksModified(const QList<CScriptObject*>&)), this, SLOT(OnInstanceLinksModified(const QList<CScriptObject*>&)));
    connect(mpWorldEditor->Selection(), SIGNAL(Modified()), this, SLOT(GenerateUI()));
}

void WModifyTab::ClearUI()
{
    ui->ObjectsTabWidget->hide();
    ui->PropertyView->SetInstance(nullptr);
    ui->LightGroupBox->hide();
    mpSelectedNode = nullptr;
}

// ************ PUBLIC SLOTS ************
void WModifyTab::GenerateUI()
{
    if (mIsPicking)
        mpWorldEditor->ExitPickMode();

    if (mpWorldEditor->Selection()->Size() == 1)
    {
        if (mpSelectedNode != mpWorldEditor->Selection()->Front())
        {
            mpSelectedNode = mpWorldEditor->Selection()->Front();

            // todo: set up editing UI for Light Nodes
            if (mpSelectedNode->NodeType() == eScriptNode)
            {
                ui->ObjectsTabWidget->show();
                CScriptNode *pScriptNode = static_cast<CScriptNode*>(mpSelectedNode);
                CScriptObject *pObj = pScriptNode->Instance();

                // Set up UI
                ui->PropertyView->SetInstance(pObj);
                ui->LightGroupBox->hide();

                ui->InLinksTableView->clearSelection();
                ui->OutLinksTableView->clearSelection();
                mpInLinkModel->SetObject(pObj);
                mpOutLinkModel->SetObject(pObj);
            }
        }
    }

    else
        ClearUI();
}

void WModifyTab::OnInstanceLinksModified(const QList<CScriptObject*>& rkInstances)
{
    if (mpSelectedNode && mpSelectedNode->NodeType() == eScriptNode)
    {
        CScriptObject *pInstance = static_cast<CScriptNode*>(mpSelectedNode)->Instance();

        if (pInstance && rkInstances.contains(pInstance))
        {
            mpInLinkModel->layoutChanged();
            mpOutLinkModel->layoutChanged();
            ui->InLinksTableView->clearSelection();
            ui->OutLinksTableView->clearSelection();
        }
    }
}

void WModifyTab::OnWorldSelectionTransformed()
{
    ui->PropertyView->UpdateEditorProperties(QModelIndex());
}

void WModifyTab::OnLinksSelectionModified()
{
    if (sender() == ui->InLinksTableView->selectionModel())
    {
        u32 NumSelectedRows = ui->InLinksTableView->selectionModel()->selectedRows().size();
        ui->EditIncomingConnectionButton->setEnabled(NumSelectedRows == 1);
        ui->DeleteIncomingConnectionButton->setEnabled(NumSelectedRows > 0);
    }
    else
    {
        u32 NumSelectedRows = ui->OutLinksTableView->selectionModel()->selectedRows().size();
        ui->EditOutgoingConnectionButton->setEnabled(NumSelectedRows == 1);
        ui->DeleteOutgoingConnectionButton->setEnabled(NumSelectedRows > 0);
    }
}

void WModifyTab::OnAddLinkActionClicked(QAction *pAction)
{
    if (mpSelectedNode && mpSelectedNode->NodeType() == eScriptNode)
    {
        mAddLinkType = (sender() == ui->AddOutgoingConnectionToolButton ? eOutgoing : eIncoming);

        if (pAction == mpAddFromViewportAction)
        {
            mpWorldEditor->EnterPickMode(eScriptNode, true, false, false);
            connect(mpWorldEditor, SIGNAL(PickModeClick(SRayIntersection,QMouseEvent*)), this, SLOT(OnPickModeClick(SRayIntersection)));
            connect(mpWorldEditor, SIGNAL(PickModeExited()), this, SLOT(OnPickModeExit()));
            mIsPicking = true;
        }

        else if (pAction == mpAddFromListAction)
        {
            if (mIsPicking)
                mpWorldEditor->ExitPickMode();

            CSelectInstanceDialog Dialog(mpWorldEditor, this);
            Dialog.exec();
            CScriptObject *pTarget = Dialog.SelectedInstance();

            if (pTarget)
            {
                CLinkDialog *pLinkDialog = mpWorldEditor->LinkDialog();
                CScriptObject *pSelected = static_cast<CScriptNode*>(mpSelectedNode)->Instance();

                CScriptObject *pSender      = (mAddLinkType == eOutgoing ? pSelected : pTarget);
                CScriptObject *pReceiver    = (mAddLinkType == eOutgoing ? pTarget : pSelected);
                pLinkDialog->NewLink(pSender, pReceiver);
                pLinkDialog->show();
            }
        }
    }
}

void WModifyTab::OnPickModeClick(const SRayIntersection& rkIntersect)
{
    mpWorldEditor->ExitPickMode();
    CScriptObject *pTarget = static_cast<CScriptNode*>(rkIntersect.pNode)->Instance();

    if (pTarget)
    {
        CLinkDialog *pDialog = mpWorldEditor->LinkDialog();
        CScriptObject *pSelected = static_cast<CScriptNode*>(mpSelectedNode)->Instance();

        CScriptObject *pSender      = (mAddLinkType == eOutgoing ? pSelected : pTarget);
        CScriptObject *pReceiver    = (mAddLinkType == eOutgoing ? pTarget : pSelected);
        pDialog->NewLink(pSender, pReceiver);
        pDialog->show();
    }
}

void WModifyTab::OnPickModeExit()
{
    disconnect(mpWorldEditor, SIGNAL(PickModeClick(SRayIntersection,QMouseEvent*)), this, 0);
    disconnect(mpWorldEditor, SIGNAL(PickModeExited()), this, 0);
    mIsPicking = false;
}

void WModifyTab::OnDeleteLinksClicked()
{
    if (mpSelectedNode && mpSelectedNode->NodeType() == eScriptNode)
    {
        ELinkType Type = (sender() == ui->DeleteOutgoingConnectionButton ? eOutgoing : eIncoming);
        QModelIndexList SelectedIndices = (Type == eOutgoing ? ui->OutLinksTableView->selectionModel()->selectedRows() : ui->InLinksTableView->selectionModel()->selectedRows());

        if (!SelectedIndices.isEmpty())
        {
            QVector<u32> Indices;

            for (int iIdx = 0; iIdx < SelectedIndices.size(); iIdx++)
                Indices << SelectedIndices[iIdx].row();

            CScriptObject *pInst = static_cast<CScriptNode*>(mpSelectedNode)->Instance();
            CDeleteLinksCommand *pCmd = new CDeleteLinksCommand(mpWorldEditor, pInst, Type, Indices);
            mpWorldEditor->UndoStack()->push(pCmd);
        }
    }
}

void WModifyTab::OnEditLinkClicked()
{
    if (mpSelectedNode && mpSelectedNode->NodeType() == eScriptNode)
    {
        ELinkType Type = (sender() == ui->EditOutgoingConnectionButton ? eOutgoing : eIncoming);
        QModelIndexList SelectedIndices = (Type == eOutgoing ? ui->OutLinksTableView->selectionModel()->selectedRows() : ui->InLinksTableView->selectionModel()->selectedRows());

        if (SelectedIndices.size() == 1)
        {
            CScriptObject *pInst = static_cast<CScriptNode*>(mpSelectedNode)->Instance();
            CLinkDialog *pDialog = mpWorldEditor->LinkDialog();
            pDialog->EditLink(pInst->Link(Type, SelectedIndices.front().row()));
            pDialog->show();
        }
    }
}

// ************ PRIVATE SLOTS ************
void WModifyTab::OnLinkTableDoubleClick(QModelIndex Index)
{
    if (Index.column() == 0)
    {
        // The link table will only be visible if the selected node is a script node
        CScriptNode *pNode = static_cast<CScriptNode*>(mpSelectedNode);
        u32 InstanceID;

        if (sender() == ui->InLinksTableView)
            InstanceID = pNode->Instance()->Link(eIncoming, Index.row())->SenderID();
        else if (sender() == ui->OutLinksTableView)
            InstanceID = pNode->Instance()->Link(eOutgoing, Index.row())->ReceiverID();

        CScriptNode *pLinkedNode = pNode->Scene()->NodeForInstanceID(InstanceID);

        if (pLinkedNode)
            mpWorldEditor->ClearAndSelectNode(pLinkedNode);

        ui->InLinksTableView->clearSelection();
        ui->OutLinksTableView->clearSelection();
    }
}
