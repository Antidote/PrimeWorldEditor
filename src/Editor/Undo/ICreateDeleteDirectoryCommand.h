#ifndef CCREATEDIRECTORYCOMMAND_H
#define CCREATEDIRECTORYCOMMAND_H

#include "IUndoCommand.h"
#include "Editor/CEditorApplication.h"
#include "Editor/ResourceBrowser/CResourceBrowser.h"
#include <Core/GameProject/CResourceStore.h>
#include <Core/GameProject/CVirtualDirectory.h>

class ICreateDeleteDirectoryCommand : public IUndoCommand
{
protected:
    CResourceStore *mpStore;
    TString mParentPath;
    TString mDirName;
    CVirtualDirectory *mpDir;

public:
    ICreateDeleteDirectoryCommand(CResourceStore *pStore, TString ParentPath, TString DirName)
        : IUndoCommand("Create Directory")
        , mpStore(pStore)
        , mParentPath(ParentPath)
        , mDirName(DirName)
        , mpDir(nullptr)
    {}

protected:
    void DoCreate()
    {
        CVirtualDirectory *pParent = mpStore->GetVirtualDirectory(mParentPath, false);

        if (pParent)
        {
            gpEdApp->ResourceBrowser()->DirectoryAboutToBeCreated( TO_QSTRING(mParentPath + mDirName) );
            mpDir = pParent->FindChildDirectory(mDirName, true);
            gpEdApp->ResourceBrowser()->DirectoryCreated(mpDir);
        }
    }

    void DoDelete()
    {
        if (mpDir && !mpDir->IsRoot())
        {
            if (mpDir->IsEmpty(true))
            {
                gpEdApp->ResourceBrowser()->DirectoryAboutToBeDeleted(mpDir);
                bool DeleteSuccess = mpDir->Delete();
                ASSERT(DeleteSuccess);
                gpEdApp->ResourceBrowser()->DirectoryDeleted();

                mpDir = nullptr;
            }
            else
            {
                Log::Write("Directory delete failed, directory is not empty: " + mParentPath + mDirName);
            }
        }
    }

    bool AffectsCleanState() const { return false; }
};

class CCreateDirectoryCommand : public ICreateDeleteDirectoryCommand
{
public:
    CCreateDirectoryCommand(CResourceStore *pStore, TString ParentPath, TString DirName)
        : ICreateDeleteDirectoryCommand(pStore, ParentPath, DirName)
    {}

    void undo() { DoDelete(); }
    void redo() { DoCreate(); }
};

class CDeleteDirectoryCommand : public ICreateDeleteDirectoryCommand
{
public:
    CDeleteDirectoryCommand(CResourceStore *pStore, TString ParentPath, TString DirName)
        : ICreateDeleteDirectoryCommand(pStore, ParentPath, DirName)
    {
        mpDir = pStore->GetVirtualDirectory(ParentPath + DirName, false);
        ASSERT(mpDir);
        ASSERT(!mpDir->IsRoot());
    }

    void undo() { DoCreate(); }
    void redo() { DoDelete(); }
};

#endif // CCREATEDIRECTORYCOMMAND_H
