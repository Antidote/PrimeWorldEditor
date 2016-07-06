#include "CStartWindow.h"
#include "ui_CStartWindow.h"
#include "CAboutDialog.h"
#include "CErrorLogDialog.h"
#include "CPakToolDialog.h"
#include "UICommon.h"

#include "Editor/ModelEditor/CModelEditorWindow.h"
#include "Editor/WorldEditor/CWorldEditor.h"
#include <Core/GameProject/CGameExporter.h>
#include <Core/GameProject/CResourceStore.h>

#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>

CStartWindow::CStartWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::CStartWindow)
{
    ui->setupUi(this);
    REPLACE_WINDOWTITLE_APPVARS;

    mpWorld = nullptr;
    mpWorldEditor = new CWorldEditor(0);
    mpModelEditor = new CModelEditorWindow(this);
    mpCharEditor = new CCharacterEditor(this);

    connect(ui->ActionAbout, SIGNAL(triggered()), this, SLOT(About()));
    connect(ui->ActionCharacterEditor, SIGNAL(triggered()), this, SLOT(LaunchCharacterEditor()));
    connect(ui->ActionExportGame, SIGNAL(triggered()), this, SLOT(ExportGame()));
}

CStartWindow::~CStartWindow()
{
    delete ui;
    delete mpWorldEditor;
    delete mpModelEditor;
    delete mpCharEditor;
}

void CStartWindow::closeEvent(QCloseEvent *pEvent)
{
    if (mpWorldEditor->close())
        qApp->quit();
    else
        pEvent->ignore();
}

void CStartWindow::on_actionOpen_MLVL_triggered()
{
    QString WorldFile = QFileDialog::getOpenFileName(this, "Open MLVL", "", "Metroid Prime World (*.MLVL)");
    if (WorldFile.isEmpty()) return;

    if (mpWorldEditor->close())
    {
        TString Dir = TO_TSTRING(WorldFile).GetFileDirectory();
        gpResourceStore->SetTransientLoadDir(Dir);
        mpWorld = gpResourceStore->LoadResource(WorldFile.toStdString());

        QString QStrDir = TO_QSTRING(Dir);
        mpWorldEditor->SetWorldDir(QStrDir);
        mpWorldEditor->SetPakFileList(CPakToolDialog::TargetListForFolder(QStrDir));
        mpWorldEditor->SetPakTarget(CPakToolDialog::TargetPakForFolder(QStrDir));

        FillWorldUI();
    }
}

void CStartWindow::FillWorldUI()
{
    CStringTable *pWorldName = mpWorld->WorldName();
    if (pWorldName)
    {
        TWideString WorldName = pWorldName->String("ENGL", 0);
        ui->WorldNameLabel->setText( QString("<font size=5><b>") + TO_QSTRING(WorldName) + QString("</b></font>") );
        ui->WorldNameSTRGLineEdit->setText(TO_QSTRING(pWorldName->Source()));

        // hack to get around lack of UTF16 -> UTF8 support
        TString WorldNameTStr = TO_TSTRING(QString::fromStdWString(WorldName.ToStdString()));
        Log::Write("Loaded " + mpWorld->Source() + " (" + WorldNameTStr + ")");
    }
    else
    {
        ui->WorldNameLabel->clear();
        ui->WorldNameSTRGLineEdit->clear();
        Log::Write("Loaded " + mpWorld->Source() + " (unnamed world)");
    }

    CStringTable *pDarkWorldName = mpWorld->DarkWorldName();
    if (pDarkWorldName)
        ui->DarkWorldNameSTRGLineEdit->setText(TO_QSTRING(pDarkWorldName->Source()));
    else
        ui->DarkWorldNameSTRGLineEdit->clear();

    CModel *pDefaultSkybox = mpWorld->DefaultSkybox();
    if (pDefaultSkybox)
        ui->DefaultSkyboxCMDLLineEdit->setText(TO_QSTRING(pDefaultSkybox->Source()));
    else
        ui->DefaultSkyboxCMDLLineEdit->clear();

    CResource *pSaveWorld = mpWorld->SaveWorld();
    if (pSaveWorld)
        ui->WorldSAVWLineEdit->setText(TO_QSTRING(pSaveWorld->Source()));
    else
        ui->WorldSAVWLineEdit->clear();

    CResource *pMapWorld = mpWorld->MapWorld();
    if (pMapWorld)
        ui->WorldMAPWLineEdit->setText(TO_QSTRING(pMapWorld->Source()));
    else
        ui->WorldMAPWLineEdit->clear();

    u32 NumAreas = mpWorld->NumAreas();
    ui->AreaSelectComboBox->blockSignals(true);
    ui->AreaSelectComboBox->clear();
    ui->AreaSelectComboBox->blockSignals(false);
    ui->AreaSelectComboBox->setDisabled(false);

    for (u32 iArea = 0; iArea < NumAreas; iArea++)
    {
        CStringTable *pAreaName = mpWorld->AreaName(iArea);
        QString AreaName = (pAreaName != nullptr) ? TO_QSTRING(pAreaName->String("ENGL", 0)) : QString("!!") + TO_QSTRING(mpWorld->AreaInternalName(iArea));
        ui->AreaSelectComboBox->addItem(AreaName);
    }
}

void CStartWindow::FillAreaUI()
{
    ui->AreaNameLineEdit->setDisabled(false);
    ui->AreaNameSTRGLineEdit->setDisabled(false);
    ui->AreaMREALineEdit->setDisabled(false);
    ui->AttachedAreasList->setDisabled(false);

    ui->AreaSelectComboBox->blockSignals(true);
    ui->AreaSelectComboBox->setCurrentIndex(mSelectedAreaIndex);
    ui->AreaSelectComboBox->blockSignals(false);

    ui->AreaNameLineEdit->setText(TO_QSTRING(mpWorld->AreaInternalName(mSelectedAreaIndex)));

    CStringTable *pAreaName = mpWorld->AreaName(mSelectedAreaIndex);
    if (pAreaName)
        ui->AreaNameSTRGLineEdit->setText(TO_QSTRING(pAreaName->Source()));
    else
        ui->AreaNameSTRGLineEdit->clear();

    u64 MREA = mpWorld->AreaResourceID(mSelectedAreaIndex);
    TString MREAStr;
    if (MREA & 0xFFFFFFFF00000000)
        MREAStr = TString::FromInt64(MREA, 16);
    else
        MREAStr = TString::FromInt32(MREA, 8);

    ui->AreaMREALineEdit->setText(TO_QSTRING(MREAStr) + QString(".MREA"));

    u32 NumAttachedAreas = mpWorld->AreaAttachedCount(mSelectedAreaIndex);
    ui->AttachedAreasList->clear();

    for (u32 iArea = 0; iArea < NumAttachedAreas; iArea++)
    {
        u32 AttachedAreaIndex = mpWorld->AreaAttachedID(mSelectedAreaIndex, iArea);

        CStringTable *AttachedAreaSTRG = mpWorld->AreaName(AttachedAreaIndex);
        QString AttachedStr;

        if (AttachedAreaSTRG)
            AttachedStr = TO_QSTRING(AttachedAreaSTRG->String("ENGL", 0));
        else
            AttachedStr = QString("!") + TO_QSTRING(mpWorld->AreaInternalName(AttachedAreaIndex));

        ui->AttachedAreasList->addItem(AttachedStr);
    }

    ui->LaunchWorldEditorButton->setDisabled(false);
}

void CStartWindow::on_AreaSelectComboBox_currentIndexChanged(int Index)
{
    mSelectedAreaIndex = Index;
    FillAreaUI();
}

void CStartWindow::on_AttachedAreasList_doubleClicked(const QModelIndex& rkIndex)
{
    mSelectedAreaIndex = mpWorld->AreaAttachedID(mSelectedAreaIndex, rkIndex.row());
    FillAreaUI();
}

void CStartWindow::on_LaunchWorldEditorButton_clicked()
{
    if (mpWorldEditor->CheckUnsavedChanges())
    {
        Log::ClearErrorLog();

        CUniqueID AreaID = mpWorld->AreaResourceID(mSelectedAreaIndex);
        TString AreaPath = mpWorld->Entry()->CookedAssetPath().GetFileDirectory() + AreaID.ToString() + ".MREA";
        TResPtr<CGameArea> pArea = gpResourceStore->LoadResource(AreaPath);

        if (!pArea)
        {
            QMessageBox::warning(this, "Error", "Couldn't load area!");
            return;
        }

        pArea->SetWorldIndex(mSelectedAreaIndex);
        mpWorld->SetAreaLayerInfo(pArea);
        mpWorldEditor->SetArea(mpWorld, pArea);
        gpResourceStore->DestroyUnreferencedResources();

        mpWorldEditor->setWindowModality(Qt::WindowModal);
        mpWorldEditor->showMaximized();

        // Display errors
        CErrorLogDialog ErrorDialog(mpWorldEditor);
        bool HasErrors = ErrorDialog.GatherErrors();

        if (HasErrors)
            ErrorDialog.exec();
    }
}

void CStartWindow::on_actionLaunch_model_viewer_triggered()
{
    mpModelEditor->setWindowModality(Qt::ApplicationModal);
    mpModelEditor->show();
}

void CStartWindow::on_actionExtract_PAK_triggered()
{
    QString Pak = QFileDialog::getOpenFileName(this, "Select pak", "", "Package (*.pak)");

    if (!Pak.isEmpty())
    {
        CPakToolDialog::EResult Result = CPakToolDialog::Extract(Pak, 0, this);

        if (Result == CPakToolDialog::eSuccess)
            Result = CPakToolDialog::DumpList(Pak, 0, this);

        if (Result == CPakToolDialog::eSuccess)
            QMessageBox::information(this, "Success", "Extracted pak successfully!");
        else if (Result == CPakToolDialog::eError)
            QMessageBox::warning(this, "Error", "Unable to extract pak.");
    }
}

void CStartWindow::LaunchCharacterEditor()
{
    mpCharEditor->show();
}

void CStartWindow::About()
{
    CAboutDialog Dialog(this);
    Dialog.exec();
}

void CStartWindow::ExportGame()
{
    // TEMP - hardcoded names for convenience. will remove later!
#define USE_HARDCODED_GAME_ROOT 0
#define USE_HARDCODED_EXPORT_DIR 1

#if USE_HARDCODED_GAME_ROOT
    QString GameRoot = "E:/Unpacked/Metroid Prime 2";
#else
    QString GameRoot = QFileDialog::getExistingDirectory(this, "Select game root directory");
    if (GameRoot.isEmpty()) return;
#endif

#if USE_HARDCODED_EXPORT_DIR
    QString ExportDir = "E:/Unpacked/ExportTest";
#else
    QString ExportDir = QFileDialog::getExistingDirectory(this, "Select output export directory");
    if (ExportDir.isEmpty()) return;
#endif

    // Verify valid game root by checking if opening.bnr exists
    TString OpeningBNR = TO_TSTRING(GameRoot) + "/opening.bnr";
    if (!FileUtil::Exists(OpeningBNR.ToUTF16()))
    {
        QMessageBox::warning(this, "Error", "Error; this is not a valid game root directory!");
        return;
    }

    CGameExporter Exporter(TO_TSTRING(GameRoot), TO_TSTRING(ExportDir));
    Exporter.Export();
}
