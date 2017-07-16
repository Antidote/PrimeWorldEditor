#include "CEditorApplication.h"
#include "IEditor.h"
#include "CBasicViewport.h"
#include "CProgressDialog.h"
#include "CProjectSettingsDialog.h"
#include "Editor/CharacterEditor/CCharacterEditor.h"
#include "Editor/ModelEditor/CModelEditorWindow.h"
#include "Editor/ResourceBrowser/CResourceBrowser.h"
#include "Editor/WorldEditor/CWorldEditor.h"
#include <Common/AssertMacro.h>
#include <Common/CTimer.h>
#include <Core/GameProject/CGameProject.h>

#include <QFuture>
#include <QtConcurrent/QtConcurrentRun>

CEditorApplication::CEditorApplication(int& rArgc, char **ppArgv)
    : QApplication(rArgc, ppArgv)
    , mpActiveProject(nullptr)
    , mpWorldEditor(nullptr)
    , mpProjectDialog(nullptr)
{
    mLastUpdate = CTimer::GlobalTime();

    connect(&mRefreshTimer, SIGNAL(timeout()), this, SLOT(TickEditors()));
    mRefreshTimer.start(8);
}

CEditorApplication::~CEditorApplication()
{
    delete mpWorldEditor;
    delete mpProjectDialog;
}

void CEditorApplication::InitEditor()
{
    mpWorldEditor = new CWorldEditor();
    mpProjectDialog = new CProjectSettingsDialog(mpWorldEditor);
    mpWorldEditor->showMaximized();
}

bool CEditorApplication::CloseAllEditors()
{
    // Close active editor windows.
    foreach (IEditor *pEditor, mEditorWindows)
    {
        if (pEditor != mpWorldEditor && !pEditor->close())
            return false;
    }

    // Close world
    if (!mpWorldEditor->CloseWorld())
        return false;

    mpProjectDialog->close();
    return true;
}

bool CEditorApplication::CloseProject()
{
    if (mpActiveProject && CloseAllEditors())
    {
        // Emit before actually deleting the project to allow editor references to clean up
        CGameProject *pOldProj = mpActiveProject;
        mpActiveProject = nullptr;
        emit ActiveProjectChanged(nullptr);
        delete pOldProj;
    }

    return true;
}

bool CEditorApplication::OpenProject(const QString& rkProjPath)
{
    // Close existing project
    if (!CloseProject())
        return false;

    // Load new project
    TString Path = TO_TSTRING(rkProjPath);

    CProgressDialog Dialog("Opening " + TO_QSTRING(Path.GetFileName()), true, true, mpWorldEditor);
    Dialog.DisallowCanceling();
    QFuture<CGameProject*> Future = QtConcurrent::run(&CGameProject::LoadProject, Path, &Dialog);
    mpActiveProject = Dialog.WaitForResults(Future);
    Dialog.close();

    if (mpActiveProject)
    {
        gpResourceStore = mpActiveProject->ResourceStore();
        emit ActiveProjectChanged(mpActiveProject);
        return true;
    }
    else
    {
        UICommon::ErrorMsg(mpWorldEditor, "Failed to open project! Is it already open in another Prime World Editor instance?");
        return false;
    }
}

void CEditorApplication::EditResource(CResourceEntry *pEntry)
{
    ASSERT(pEntry != nullptr);

    // Check if we're already editing this resource
    if (mEditingMap.contains(pEntry))
    {
        IEditor *pEd = mEditingMap[pEntry];
        pEd->show();
        pEd->raise();
    }

    else
    {
        // Attempt to load asset
        CResource *pRes = pEntry->Load();

        if (!pRes)
        {
            UICommon::ErrorMsg(mpWorldEditor, "Failed to load resource!");
            return;
        }

        // Launch editor window
        IEditor *pEd = nullptr;

        switch (pEntry->ResourceType())
        {
        case eModel:
            pEd = new CModelEditorWindow((CModel*) pRes, mpWorldEditor);
            break;

        case eAnimSet:
            pEd = new CCharacterEditor((CAnimSet*) pRes, mpWorldEditor);
            break;
        }

        if (pEd)
        {
            pEd->show();
            mEditingMap[pEntry] = pEd;
        }
        else
            UICommon::InfoMsg(mpWorldEditor, "Unsupported Resource", "This resource type is currently unsupported for editing.");
    }
}

void CEditorApplication::NotifyAssetsModified()
{
    emit AssetsModified();
}

bool CEditorApplication::CookPackage(CPackage *pPkg)
{
    return CookPackageList(QList<CPackage*>() << pPkg);
}

bool CEditorApplication::CookAllDirtyPackages()
{
    ASSERT(mpActiveProject != nullptr);
    QList<CPackage*> PackageList;

    for (u32 iPkg = 0; iPkg < mpActiveProject->NumPackages(); iPkg++)
    {
        CPackage *pPackage = mpActiveProject->PackageByIndex(iPkg);

        if (pPackage->NeedsRecook())
            PackageList << pPackage;
    }

    return CookPackageList(PackageList);
}

bool CEditorApplication::CookPackageList(QList<CPackage*> PackageList)
{
    if (!PackageList.isEmpty())
    {
        CProgressDialog Dialog("Cooking package" + QString(PackageList.size() > 1  ? "s" : ""), false, true, mpWorldEditor);

        QFuture<void> Future = QtConcurrent::run([&]()
        {
            Dialog.SetNumTasks(PackageList.size());

            for (int PkgIdx = 0; PkgIdx < PackageList.size() && !Dialog.ShouldCancel(); PkgIdx++)
            {
                CPackage *pPkg = PackageList[PkgIdx];
                Dialog.SetTask(PkgIdx, "Cooking " + pPkg->Name() + ".pak...");
                pPkg->Cook(&Dialog);
            }
        });

        Dialog.WaitForResults(Future);

        emit PackagesCooked();
        return !Dialog.ShouldCancel();
    }
    else return true;
}

bool CEditorApplication::RebuildResourceDatabase()
{
    // Make sure all editors are closed
    if (mpActiveProject && CloseAllEditors())
    {
        // Fake-close the project, but keep it in memory so we can modify the resource store
        CGameProject *pProj = mpActiveProject;
        mpActiveProject = nullptr;
        emit ActiveProjectChanged(nullptr);

        // Rebuild
        CProgressDialog Dialog("Rebuilding resource database", true, false, mpWorldEditor);
        Dialog.SetOneShotTask("Rebuilding resource database");
        Dialog.DisallowCanceling();

        QFuture<void> Future = QtConcurrent::run(pProj->ResourceStore(), &CResourceStore::RebuildFromDirectory);
        Dialog.WaitForResults(Future);
        Dialog.close();

        // Set project to active again
        mpActiveProject = pProj;
        emit ActiveProjectChanged(pProj);

        UICommon::InfoMsg(mpWorldEditor, "Success", "Resource database rebuilt successfully!");
        return true;
    }

    return false;
}

// ************ SLOTS ************
void CEditorApplication::AddEditor(IEditor *pEditor)
{
    mEditorWindows << pEditor;
    connect(pEditor, SIGNAL(Closed()), this, SLOT(OnEditorClose()));
}

void CEditorApplication::TickEditors()
{
    double LastUpdate = mLastUpdate;
    mLastUpdate = CTimer::GlobalTime();
    double DeltaTime = mLastUpdate - LastUpdate;

    // Make sure the resource store caches are up-to-date
    if (gpEditorStore)
        gpEditorStore->ConditionalSaveStore();

    if (gpResourceStore)
        gpResourceStore->ConditionalSaveStore();

    // Tick each editor window and redraw their viewports
    foreach(IEditor *pEditor, mEditorWindows)
    {
        if (pEditor->isVisible())
        {
            CBasicViewport *pViewport = pEditor->Viewport();
            bool ViewportVisible = (pViewport && pViewport->isVisible() && !pEditor->isMinimized());

            if (ViewportVisible) pViewport->ProcessInput();
            pEditor->EditorTick((float) DeltaTime);
            if (ViewportVisible) pViewport->Render();
        }
    }
}

void CEditorApplication::OnEditorClose()
{
    IEditor *pEditor = qobject_cast<IEditor*>(sender());
    ASSERT(pEditor);

    if (pEditor == mpWorldEditor)
    {
        mpWorldEditor = nullptr;
        quit();
    }
    else
    {
        for (auto Iter = mEditingMap.begin(); Iter != mEditingMap.end(); Iter++)
        {
            if (Iter.value() == pEditor)
            {
                mEditingMap.erase(Iter);
                break;
            }
        }

        mEditorWindows.removeOne(pEditor);
        delete pEditor;

        if (mpActiveProject)
        {
            mpActiveProject->ResourceStore()->DestroyUnreferencedResources();
        }
    }
}

CResourceBrowser* CEditorApplication::ResourceBrowser() const
{
    return mpWorldEditor->ResourceBrowser();
}
