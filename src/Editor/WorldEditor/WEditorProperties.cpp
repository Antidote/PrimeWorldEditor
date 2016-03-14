#include "WEditorProperties.h"
#include "Editor/Undo/CEditScriptPropertyCommand.h"
#include <Core/Resource/Script/CScriptLayer.h>

WEditorProperties::WEditorProperties(QWidget *pParent /*= 0*/)
    : QWidget(pParent)
    , mpEditor(nullptr)
    , mpDisplayNode(nullptr)
    , mHasEditedName(false)
{
    mpInstanceInfoLabel = new QLabel;
    mpInstanceInfoLabel->setText("<i>No selection</i>");
    mpInstanceInfoLayout = new QHBoxLayout;
    mpInstanceInfoLayout->addWidget(mpInstanceInfoLabel);

    mpActiveCheckBox = new QCheckBox;
    mpActiveCheckBox->setToolTip("Active");
    mpInstanceNameLineEdit = new QLineEdit;
    mpInstanceNameLineEdit->setToolTip("Instance Name");
    mpNameLayout = new QHBoxLayout;
    mpNameLayout->addWidget(mpActiveCheckBox);
    mpNameLayout->addWidget(mpInstanceNameLineEdit);

    mpLayersLabel = new QLabel;
    mpLayersLabel->setText("Layer:");
    mpLayersLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    mpLayersComboBox = new QComboBox;
    mpLayersComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    mpLayersLayout = new QHBoxLayout;
    mpLayersLayout->addWidget(mpLayersLabel);
    mpLayersLayout->addWidget(mpLayersComboBox);

    mpMainLayout = new QVBoxLayout;
    mpMainLayout->addLayout(mpInstanceInfoLayout);
    mpMainLayout->addLayout(mpNameLayout);
    mpMainLayout->addLayout(mpLayersLayout);
    mpMainLayout->setContentsMargins(6, 6, 6, 0);
    mpMainLayout->setSpacing(3);
    setLayout(mpMainLayout);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    QFont Font = font();
    Font.setPointSize(10);
    setFont(Font);
    mpInstanceInfoLabel->setFont(Font);
    mpInstanceNameLineEdit->setFont(Font);
    mpLayersLabel->setFont(Font);

    QFont ComboFont = mpLayersComboBox->font();
    ComboFont.setPointSize(10);
    mpLayersComboBox->setFont(ComboFont);

    connect(mpActiveCheckBox, SIGNAL(clicked()), this, SLOT(OnActiveChanged()));
    connect(mpInstanceNameLineEdit, SIGNAL(textEdited(QString)), this, SLOT(OnInstanceNameEdited()));
    connect(mpInstanceNameLineEdit, SIGNAL(editingFinished()), this, SLOT(OnInstanceNameEditFinished()));
    connect(mpLayersComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(OnLayerChanged()));
}

void WEditorProperties::SyncToEditor(CWorldEditor *pEditor)
{
    if (mpEditor)
        disconnect(mpEditor, 0, this, 0);

    mpEditor = pEditor;
    connect(mpEditor, SIGNAL(SelectionModified()), this, SLOT(OnSelectionModified()));
    connect(mpEditor, SIGNAL(LayersModified()), this, SLOT(OnLayersModified()));
    connect(mpEditor, SIGNAL(InstancesLayerChanged(QList<CScriptNode*>)), this, SLOT(OnInstancesLayerChanged(QList<CScriptNode*>)));
    connect(mpEditor, SIGNAL(PropertyModified(CScriptObject*,IProperty*)), this, SLOT(OnPropertyModified(CScriptObject*,IProperty*)));

    OnLayersModified();
}

void WEditorProperties::SetLayerComboBox()
{
    mpLayersComboBox->blockSignals(true);

    if (mpDisplayNode && mpDisplayNode->NodeType() == eScriptNode)
    {
        CScriptNode *pScript = static_cast<CScriptNode*>(mpDisplayNode);
        CScriptLayer *pLayer = pScript->Object()->Layer();
        for (u32 iLyr = 0; iLyr < mpEditor->ActiveArea()->GetScriptLayerCount(); iLyr++)
        {
            if (mpEditor->ActiveArea()->GetScriptLayer(iLyr) == pLayer)
            {
                mpLayersComboBox->setCurrentIndex(iLyr);
                break;
            }
        }
        mpLayersComboBox->setEnabled(true);
    }
    else
    {
        mpLayersComboBox->setCurrentIndex(-1);
        mpLayersComboBox->setEnabled(false);
    }

    mpLayersComboBox->blockSignals(false);
}

// ************ PUBLIC SLOTS ************
void WEditorProperties::OnSelectionModified()
{
    CNodeSelection *pSelection = mpEditor->Selection();
    mpDisplayNode = (pSelection->Size() == 1 ? pSelection->Front() : nullptr);

    if (pSelection->IsEmpty() || pSelection->Size() != 1 || mpDisplayNode->NodeType() != eScriptNode)
    {
        mpActiveCheckBox->setChecked(false);
        mpActiveCheckBox->setEnabled(false);
        mpInstanceNameLineEdit->setEnabled(false);

        if (pSelection->IsEmpty())
        {
            mpInstanceInfoLabel->setText("<i>[No selection]</i>");
            mpInstanceNameLineEdit->clear();
        }
        else if (mpDisplayNode)
        {
            mpInstanceInfoLabel->setText("[Light]");
            mpInstanceNameLineEdit->setText(TO_QSTRING(mpDisplayNode->Name()));
        }
        else
        {
            mpInstanceInfoLabel->setText(QString("<i>[%1 objects selected]</i>").arg(pSelection->Size()));
            mpInstanceNameLineEdit->clear();
        }
    }

    else
    {
        CScriptNode *pScript = static_cast<CScriptNode*>(mpDisplayNode);
        TString InstanceID = TString::HexString(pScript->Object()->InstanceID(), false, true, 8);
        TString ObjectType = pScript->Template()->Name();
        mpInstanceInfoLabel->setText(QString("[%1] [%2]").arg( TO_QSTRING(ObjectType) ).arg( TO_QSTRING(InstanceID) ));

        UpdatePropertyValues();
    }

    SetLayerComboBox();
}

void WEditorProperties::OnPropertyModified(CScriptObject *pInstance, IProperty *pProp)
{
    if (!mpInstanceNameLineEdit->hasFocus())
    {
        if (mpDisplayNode->NodeType() == eScriptNode && pInstance->IsEditorProperty(pProp))
            UpdatePropertyValues();
    }
}

void WEditorProperties::OnInstancesLayerChanged(const QList<CScriptNode*>& rkNodeList)
{
    if (rkNodeList.contains((CScriptNode*) mpDisplayNode))
        SetLayerComboBox();
}

void WEditorProperties::OnLayersModified()
{
    CGameArea *pArea = mpEditor->ActiveArea();
    mpLayersComboBox->clear();

     if (pArea)
     {
         for (u32 iLyr = 0; iLyr < pArea->GetScriptLayerCount(); iLyr++)
             mpLayersComboBox->addItem(TO_QSTRING(pArea->GetScriptLayer(iLyr)->Name()));
     }

     SetLayerComboBox();
}

void WEditorProperties::UpdatePropertyValues()
{
    CScriptNode *pScript = static_cast<CScriptNode*>(mpDisplayNode);
    CScriptObject *pInst = pScript->Object();

    mpActiveCheckBox->setChecked(pInst->IsActive());
    mpActiveCheckBox->setEnabled(pInst->ActiveProperty() != nullptr);

    mpInstanceNameLineEdit->blockSignals(true);
    mpInstanceNameLineEdit->setText(TO_QSTRING(pInst->InstanceName()));
    mpInstanceNameLineEdit->setEnabled(pInst->InstanceNameProperty() != nullptr);
    mpInstanceNameLineEdit->blockSignals(false);
}

// ************ PROTECTED SLOTS ************
void WEditorProperties::OnActiveChanged()
{
    mpEditor->SetSelectionActive(mpActiveCheckBox->isChecked());
}

void WEditorProperties::OnInstanceNameEdited()
{
    // This function triggers when the user is actively editing the Instance Name line edit.
    mHasEditedName = true;
    mpEditor->SetSelectionInstanceNames(mpInstanceNameLineEdit->text(), false);
}

void WEditorProperties::OnInstanceNameEditFinished()
{
    // This function triggers when the user is finished editing the Instance Name line edit.
    if (mHasEditedName)
        mpEditor->SetSelectionInstanceNames(mpInstanceNameLineEdit->text(), true);

    mHasEditedName = false;
}

void WEditorProperties::OnLayerChanged()
{
    int Index = mpLayersComboBox->currentIndex();

    if (Index >= 0)
    {
        CScriptLayer *pLayer = mpEditor->ActiveArea()->GetScriptLayer(Index);
        mpEditor->SetSelectionLayer(pLayer);
    }
}

