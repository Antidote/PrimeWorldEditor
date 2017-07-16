#include "CResourceBrowser.h"
#include "ui_CResourceBrowser.h"
#include "CProgressDialog.h"
#include "CResourceDelegate.h"
#include "CResourceTableContextMenu.h"
#include "Editor/CEditorApplication.h"
#include "Editor/Undo/CMoveDirectoryCommand.h"
#include "Editor/Undo/CMoveResourceCommand.h"
#include "Editor/Undo/CRenameDirectoryCommand.h"
#include "Editor/Undo/CRenameResourceCommand.h"
#include <Core/GameProject/AssetNameGeneration.h>
#include <Core/GameProject/CAssetNameMap.h>

#include <QCheckBox>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QtConcurrent/QtConcurrentRun>

CResourceBrowser::CResourceBrowser(QWidget *pParent)
    : QWidget(pParent)
    , mpUI(new Ui::CResourceBrowser)
    , mpSelectedEntry(nullptr)
    , mpStore(nullptr)
    , mpSelectedDir(nullptr)
    , mEditorStore(false)
    , mAssetListMode(false)
    , mSearching(false)
{
    mpUI->setupUi(this);

    // Hide sorting combo box for now. The size isn't displayed on the UI so this isn't really useful for the end user.
    mpUI->SortComboBox->hide();

    // Create undo/redo actions
    mpUndoAction = new QAction("Undo", this);
    mpRedoAction = new QAction("Redo", this);
    mpUndoAction->setShortcut( QKeySequence::Undo );
    mpRedoAction->setShortcut( QKeySequence::Redo );
    addAction(mpUndoAction);
    addAction(mpRedoAction);

    connect(mpUndoAction, SIGNAL(triggered(bool)), this, SLOT(Undo()));
    connect(mpRedoAction, SIGNAL(triggered(bool)), this, SLOT(Redo()));
    connect(&mUndoStack, SIGNAL(canUndoChanged(bool)), this, SLOT(UpdateUndoActionStates()));
    connect(&mUndoStack, SIGNAL(canRedoChanged(bool)), this, SLOT(UpdateUndoActionStates()));

    // Configure display mode buttons
    QButtonGroup *pModeGroup = new QButtonGroup(this);
    pModeGroup->addButton(mpUI->ResourceTreeButton);
    pModeGroup->addButton(mpUI->ResourceListButton);
    pModeGroup->setExclusive(true);

    // Set up table models
    mpModel = new CResourceTableModel(this, this);
    mpProxyModel = new CResourceProxyModel(this);
    mpProxyModel->setSourceModel(mpModel);
    mpUI->ResourceTableView->setModel(mpProxyModel);

    QHeaderView *pHeader = mpUI->ResourceTableView->horizontalHeader();
    pHeader->setSectionResizeMode(0, QHeaderView::Stretch);

    mpDelegate = new CResourceBrowserDelegate(this);
    mpUI->ResourceTableView->setItemDelegate(mpDelegate);
    mpUI->ResourceTableView->installEventFilter(this);

    // Set up directory tree model
    mpDirectoryModel = new CVirtualDirectoryModel(this);
    mpUI->DirectoryTreeView->setModel(mpDirectoryModel);

    RefreshResources();

    // Set up filter checkboxes
    mpFilterBoxesLayout = new QVBoxLayout();
    mpFilterBoxesLayout->setContentsMargins(2, 2, 2, 2);
    mpFilterBoxesLayout->setSpacing(1);

    mpFilterBoxesContainerWidget = new QWidget(this);
    mpFilterBoxesContainerWidget->setLayout(mpFilterBoxesLayout);

    mpUI->FilterCheckboxScrollArea->setWidget(mpFilterBoxesContainerWidget);
    mpUI->FilterCheckboxScrollArea->setBackgroundRole(QPalette::AlternateBase);

    mFilterBoxFont = font();
    mFilterBoxFont.setPointSize(mFilterBoxFont.pointSize() - 1);

    QFont AllBoxFont = mFilterBoxFont;
    AllBoxFont.setBold(true);

    mpFilterAllBox = new QCheckBox(this);
    mpFilterAllBox->setChecked(true);
    mpFilterAllBox->setFont(AllBoxFont);
    mpFilterAllBox->setText("All");
    mpFilterBoxesLayout->addWidget(mpFilterAllBox);

    CreateFilterCheckboxes();

    // Set up the options menu
    QMenu *pOptionsMenu = new QMenu(this);
    QMenu *pImportMenu = pOptionsMenu->addMenu("Import Names");
    pOptionsMenu->addAction("Export Names", this, SLOT(ExportAssetNames()));
    pOptionsMenu->addSeparator();

    pImportMenu->addAction("Asset Name Map", this, SLOT(ImportAssetNameMap()));
    pImportMenu->addAction("Package Contents List", this, SLOT(ImportPackageContentsList()));
    pImportMenu->addAction("Generate Asset Names", this, SLOT(GenerateAssetNames()));

    QAction *pDisplayAssetIDsAction = new QAction("Display Asset IDs", this);
    pDisplayAssetIDsAction->setCheckable(true);
    connect(pDisplayAssetIDsAction, SIGNAL(toggled(bool)), this, SLOT(SetAssetIdDisplayEnabled(bool)));
    pOptionsMenu->addAction(pDisplayAssetIDsAction);

    pOptionsMenu->addAction("Rebuild Database", this, SLOT(RebuildResourceDB()));
    mpUI->OptionsToolButton->setMenu(pOptionsMenu);

#if !PUBLIC_RELEASE
    // Only add the store menu in debug builds. We don't want end users editing the editor store.
    pOptionsMenu->addSeparator();
    QMenu *pStoreMenu = pOptionsMenu->addMenu("Set Store");
    QAction *pProjStoreAction = pStoreMenu->addAction("Project Store", this, SLOT(SetProjectStore()));
    QAction *pEdStoreAction = pStoreMenu->addAction("Editor Store", this, SLOT(SetEditorStore()));

    pProjStoreAction->setCheckable(true);
    pProjStoreAction->setChecked(true);
    pEdStoreAction->setCheckable(true);

    QActionGroup *pGroup = new QActionGroup(this);
    pGroup->addAction(pProjStoreAction);
    pGroup->addAction(pEdStoreAction);
#endif

    // Create context menu for the resource table
    new CResourceTableContextMenu(this, mpUI->ResourceTableView, mpModel, mpProxyModel);

    // Set up connections
    connect(mpUI->SearchBar, SIGNAL(StoppedTyping(QString)), this, SLOT(OnSearchStringChanged(QString)));
    connect(mpUI->ResourceTreeButton, SIGNAL(pressed()), this, SLOT(SetResourceTreeView()));
    connect(mpUI->ResourceListButton, SIGNAL(pressed()), this, SLOT(SetResourceListView()));
    connect(mpUI->SortComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(OnSortModeChanged(int)));
    connect(mpUI->DirectoryTreeView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)), this, SLOT(OnDirectorySelectionChanged(QModelIndex,QModelIndex)));
    connect(mpUI->ResourceTableView, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(OnDoubleClickTable(QModelIndex)));
    connect(mpUI->ResourceTableView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)), this, SLOT(OnResourceSelectionChanged(QModelIndex, QModelIndex)));
    connect(mpProxyModel, SIGNAL(rowsInserted(QModelIndex,int,int)), mpUI->ResourceTableView, SLOT(resizeRowsToContents()));
    connect(mpProxyModel, SIGNAL(layoutChanged(QList<QPersistentModelIndex>,QAbstractItemModel::LayoutChangeHint)), mpUI->ResourceTableView, SLOT(resizeRowsToContents()));
    connect(mpFilterAllBox, SIGNAL(toggled(bool)), this, SLOT(OnFilterTypeBoxTicked(bool)));
    connect(gpEdApp, SIGNAL(ActiveProjectChanged(CGameProject*)), this, SLOT(UpdateStore()));
}

CResourceBrowser::~CResourceBrowser()
{
    delete mpUI;
}

void CResourceBrowser::SelectResource(CResourceEntry *pEntry)
{
    ASSERT(pEntry);

    // Select target directory
    SelectDirectory(pEntry->Directory());

    // Clear search
    if (!mpUI->SearchBar->text().isEmpty())
    {
        mpUI->SearchBar->clear();
        UpdateFilter();
    }

    // Select resource
    QModelIndex SourceIndex = mpModel->GetIndexForEntry(pEntry);
    QModelIndex ProxyIndex = mpProxyModel->mapFromSource(SourceIndex);
    mpUI->ResourceTableView->selectionModel()->select(ProxyIndex, QItemSelectionModel::ClearAndSelect);
    mpUI->ResourceTableView->scrollTo(ProxyIndex, QAbstractItemView::PositionAtCenter);
}

void CResourceBrowser::SelectDirectory(CVirtualDirectory *pDir)
{
    QModelIndex Index = mpDirectoryModel->GetIndexForDirectory(pDir);
    mpUI->DirectoryTreeView->selectionModel()->setCurrentIndex(Index, QItemSelectionModel::ClearAndSelect);
}

void CResourceBrowser::CreateFilterCheckboxes()
{
    // Delete existing checkboxes
    foreach (const SResourceType& rkType, mTypeList)
        delete rkType.pFilterCheckBox;

    mTypeList.clear();

    if (mpStore)
    {
        // Get new type list
        std::list<CResTypeInfo*> TypeList;
        CResTypeInfo::GetAllTypesInGame(mpStore->Game(), TypeList);

        for (auto Iter = TypeList.begin(); Iter != TypeList.end(); Iter++)
        {
            CResTypeInfo *pType = *Iter;

            if (pType->IsVisibleInBrowser())
            {
                QCheckBox *pCheck = new QCheckBox(this);
                pCheck->setFont(mFilterBoxFont);
                pCheck->setText(TO_QSTRING(pType->TypeName()));
                mTypeList << SResourceType { pType, pCheck };
            }
        }

        qSort(mTypeList.begin(), mTypeList.end(), [](const SResourceType& rkLeft, const SResourceType& rkRight) -> bool {
            return rkLeft.pTypeInfo->TypeName().ToUpper() < rkRight.pTypeInfo->TypeName().ToUpper();
        });

        // Add sorted checkboxes to the UI
        foreach (const SResourceType& rkType, mTypeList)
        {
            QCheckBox *pCheck = rkType.pFilterCheckBox;
            mpFilterBoxesLayout->addWidget(rkType.pFilterCheckBox);
            connect(pCheck, SIGNAL(toggled(bool)), this, SLOT(OnFilterTypeBoxTicked(bool)));
        }
    }

    QSpacerItem *pSpacer = new QSpacerItem(0, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);
    mpFilterBoxesLayout->addSpacerItem(pSpacer);
}

bool CResourceBrowser::RenameResource(CResourceEntry *pEntry, const TString& rkNewName)
{
    if (pEntry->Name() == rkNewName)
        return false;

    // Check if the move is valid
    if (!pEntry->CanMoveTo(pEntry->DirectoryPath(), rkNewName))
    {
        if (pEntry->Directory()->FindChildResource(rkNewName, pEntry->ResourceType()) != nullptr)
        {
            UICommon::ErrorMsg(this, "Failed to rename; the destination directory has conflicting files!");
            return false;
        }
        else
        {
            UICommon::ErrorMsg(this, "Failed to rename; filename is invalid!");
            return false;
        }
    }

    // Everything seems to be valid; proceed with the rename
    mUndoStack.push( new CRenameResourceCommand(pEntry, rkNewName) );
    return true;
}

bool CResourceBrowser::RenameDirectory(CVirtualDirectory *pDir, const TString& rkNewName)
{
    if (pDir->Name() == rkNewName)
        return false;

    if (!CVirtualDirectory::IsValidDirectoryName(rkNewName))
    {
        UICommon::ErrorMsg(this, "Failed to rename; directory name is invalid!");
        return false;
    }

    // Check for conflicts
    if (pDir->Parent()->FindChildDirectory(rkNewName, false) != nullptr)
    {
        UICommon::ErrorMsg(this, "Failed to rename; the destination directory has a conflicting directory!");
        return false;
    }

    // No conflicts, proceed with the rename
    mUndoStack.push( new CRenameDirectoryCommand(pDir, rkNewName) );
    return true;
}

bool CResourceBrowser::MoveResources(const QList<CResourceEntry*>& rkResources, const QList<CVirtualDirectory*>& rkDirectories, CVirtualDirectory *pNewDir)
{
    // Check for any conflicts
    QList<CResourceEntry*> ConflictingResources;

    foreach (CResourceEntry *pEntry, rkResources)
    {
        if (pNewDir->FindChildResource(pEntry->Name(), pEntry->ResourceType()) != nullptr)
            ConflictingResources << pEntry;
    }

    QList<CVirtualDirectory*> ConflictingDirs;

    foreach (CVirtualDirectory *pDir, rkDirectories)
    {
        if (pNewDir->FindChildDirectory(pDir->Name(), false) != nullptr)
            ConflictingDirs << pDir;
    }

    // If there were conflicts, notify the user of them
    if (!ConflictingResources.isEmpty() || !ConflictingDirs.isEmpty())
    {
        QString ErrorMsg = "Failed to move; the destination directory has conflicting files.\n\n";

        foreach (CVirtualDirectory *pDir, ConflictingDirs)
        {
            ErrorMsg += QString("* %1").arg( TO_QSTRING(pDir->Name()) );
        }

        foreach (CResourceEntry *pEntry, ConflictingResources)
        {
            ErrorMsg += QString("* %1.%2\n").arg( TO_QSTRING(pEntry->Name()) ).arg( TO_QSTRING(pEntry->CookedExtension().ToString()) );
        }

        UICommon::ErrorMsg(this, ErrorMsg);
        return false;
    }

    // Create undo actions to actually perform the moves
    mUndoStack.beginMacro("Move Resources");

    foreach (CVirtualDirectory *pDir, rkDirectories)
        mUndoStack.push( new CMoveDirectoryCommand(mpStore, pDir, pNewDir) );

    foreach (CResourceEntry *pEntry, rkResources)
        mUndoStack.push( new CMoveResourceCommand(pEntry, pNewDir) );

    mUndoStack.endMacro();
    return true;
}

bool CResourceBrowser::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    if (pWatched == mpUI->ResourceTableView)
    {
        if (pEvent->type() == QEvent::FocusIn || pEvent->type() == QEvent::FocusOut)
        {
            UpdateUndoActionStates();
        }
    }

    return false;
}

void CResourceBrowser::RefreshResources()
{
    // Fill resource table
    mpModel->FillEntryList(mpSelectedDir, mAssetListMode || mSearching);
}

void CResourceBrowser::RefreshDirectories()
{
    mpDirectoryModel->SetRoot(mpStore->RootDirectory());

    // Clear selection. This function is called when directories are created/deleted and our current selection might not be valid anymore
    QModelIndex RootIndex = mpDirectoryModel->index(0, 0, QModelIndex());
    mpUI->DirectoryTreeView->selectionModel()->setCurrentIndex(RootIndex, QItemSelectionModel::ClearAndSelect);
    mpUI->DirectoryTreeView->setExpanded(RootIndex, true);
}

void CResourceBrowser::UpdateDescriptionLabel()
{
    QString Desc;
    Desc += (mAssetListMode ? "[Assets]" : "[Filesystem]");
    Desc += " ";

    bool ValidDir = mpSelectedDir && !mpSelectedDir->IsRoot();
    QString Path = (ValidDir ? '/' + TO_QSTRING(mpSelectedDir->FullPath()) : "");

    if (mSearching)
    {
        QString SearchText = mpUI->SearchBar->text();
        Desc += QString("Searching \"%1\"").arg(SearchText);

        if (ValidDir)
            Desc += QString(" in %1").arg(Path);
    }
    else
    {
        if (ValidDir)
            Desc += Path;
        else
            Desc += "Root";
    }

    mpUI->TableDescriptionLabel->setText(Desc);
}

void CResourceBrowser::SetResourceTreeView()
{
    mAssetListMode = false;
    RefreshResources();
}

void CResourceBrowser::SetResourceListView()
{
    mAssetListMode = true;
    RefreshResources();
}

void CResourceBrowser::OnSortModeChanged(int Index)
{
    CResourceProxyModel::ESortMode Mode = (Index == 0 ? CResourceProxyModel::eSortByName : CResourceProxyModel::eSortBySize);
    mpProxyModel->SetSortMode(Mode);
}

void CResourceBrowser::OnSearchStringChanged(QString SearchString)
{
    bool WasSearching = mSearching;
    mSearching = !SearchString.isEmpty();

    // Check if we need to change to/from asset list mode to display/stop displaying search results
    if (!mAssetListMode)
    {
        if ( (mSearching && !WasSearching) ||
             (!mSearching && WasSearching) )
        {
            RefreshResources();
        }
    }

    UpdateFilter();
}

void CResourceBrowser::OnDirectorySelectionChanged(const QModelIndex& rkNewIndex, const QModelIndex& /*rkPrevIndex*/)
{
    if (rkNewIndex.isValid())
        mpSelectedDir = mpDirectoryModel->IndexDirectory(rkNewIndex);
    else
        mpSelectedDir = mpStore ? mpStore->RootDirectory() : nullptr;

    UpdateDescriptionLabel();
    RefreshResources();
}

void CResourceBrowser::OnDoubleClickTable(QModelIndex Index)
{
    QModelIndex SourceIndex = mpProxyModel->mapToSource(Index);

    // Directory - switch to the selected directory
    if (mpModel->IsIndexDirectory(SourceIndex))
    {
        CVirtualDirectory *pDir = mpModel->IndexDirectory(SourceIndex);
        SelectDirectory(pDir);
    }

    // Resource - open resource for editing
    else
    {
        CResourceEntry *pEntry = mpModel->IndexEntry(SourceIndex);
        gpEdApp->EditResource(pEntry);
    }
}

void CResourceBrowser::OnResourceSelectionChanged(const QModelIndex& rkNewIndex, const QModelIndex& /*rkPrevIndex*/)
{
    QModelIndex SourceIndex = mpProxyModel->mapToSource(rkNewIndex);
    mpSelectedEntry = mpModel->IndexEntry(SourceIndex);
    emit SelectedResourceChanged(mpSelectedEntry);
}

void CResourceBrowser::SetAssetIdDisplayEnabled(bool Enable)
{
    mpDelegate->SetDisplayAssetIDs(Enable);
    mpUI->ResourceTableView->repaint();
}

void CResourceBrowser::UpdateStore()
{
    CGameProject *pProj = gpEdApp->ActiveProject();
    CResourceStore *pProjStore = (pProj ? pProj->ResourceStore() : nullptr);
    CResourceStore *pNewStore = (mEditorStore ? gpEditorStore : pProjStore);

    if (mpStore != pNewStore)
    {
        mpStore = pNewStore;

        // Clear search
        mpUI->SearchBar->clear();
        mSearching = false;

        // Refresh type filter list
        CreateFilterCheckboxes();

        // Refresh directory tree
        mpDirectoryModel->SetRoot(mpStore ? mpStore->RootDirectory() : nullptr);
        QModelIndex RootIndex = mpDirectoryModel->index(0, 0, QModelIndex());
        mpUI->DirectoryTreeView->expand(RootIndex);
        mpUI->DirectoryTreeView->clearSelection();
        OnDirectorySelectionChanged(QModelIndex(), QModelIndex());
    }
}

void CResourceBrowser::SetProjectStore()
{
    mEditorStore = false;
    UpdateStore();
}

void CResourceBrowser::SetEditorStore()
{
    mEditorStore = true;
    UpdateStore();
}

void CResourceBrowser::ImportPackageContentsList()
{
    QStringList PathList = UICommon::OpenFilesDialog(this, "Open package contents list", "*.pak.contents.txt");
    if (PathList.isEmpty()) return;
    SelectDirectory(nullptr);

    foreach(const QString& rkPath, PathList)
        mpStore->ImportNamesFromPakContentsTxt(TO_TSTRING(rkPath), false);

    RefreshResources();
    RefreshDirectories();
}

void CResourceBrowser::GenerateAssetNames()
{
    SelectDirectory(nullptr);

    CProgressDialog Dialog("Generating asset names", true, true, this);
    Dialog.DisallowCanceling();
    Dialog.SetOneShotTask("Generating asset names");

    // Temporarily set root to null to ensure the window doesn't access the resource store while we're running.
    mpDirectoryModel->SetRoot(mpStore->RootDirectory());

    QFuture<void> Future = QtConcurrent::run(&::GenerateAssetNames, mpStore->Project());
    Dialog.WaitForResults(Future);

    RefreshResources();
    RefreshDirectories();

    UICommon::InfoMsg(this, "Complete", "Asset name generation complete!");
}

void CResourceBrowser::ImportAssetNameMap()
{
    CAssetNameMap Map( mpStore->Game() );
    bool LoadSuccess = Map.LoadAssetNames();

    if (!LoadSuccess)
    {
        UICommon::ErrorMsg(this, "Import failed; couldn't load asset name map!");
        return;
    }
    else if (!Map.IsValid())
    {
        UICommon::ErrorMsg(this, "Import failed; the input asset name map is invalid! See the log for details.");
        return;
    }

    SelectDirectory(nullptr);

    for (CResourceIterator It(mpStore); It; ++It)
    {
        TString Dir, Name;
        bool AutoDir, AutoName;

        if (Map.GetNameInfo(It->ID(), Dir, Name, AutoDir, AutoName))
            It->MoveAndRename(Dir, Name, AutoDir, AutoName);
    }

    mpStore->ConditionalSaveStore();
    RefreshResources();
    RefreshDirectories();
    UICommon::InfoMsg(this, "Success", "New asset names imported successfully!");
}

void CResourceBrowser::ExportAssetNames()
{
    QString OutFile = UICommon::SaveFileDialog(this, "Export asset name map", "*.xml", "../resources/gameinfo/");
    if (OutFile.isEmpty()) return;
    TString OutFileStr = TO_TSTRING(OutFile);

    CAssetNameMap NameMap(mpStore->Game());

    if (FileUtil::Exists(OutFileStr))
    {
        bool LoadSuccess = NameMap.LoadAssetNames(OutFileStr);

        if (!LoadSuccess || !NameMap.IsValid())
        {
            UICommon::ErrorMsg(this, "Unable to export; failed to load existing names from the original asset name map file! See the log for details.");
            return;
        }
    }

    NameMap.CopyFromStore(mpStore);
    bool SaveSuccess = NameMap.SaveAssetNames(OutFileStr);

    if (!SaveSuccess)
        UICommon::ErrorMsg(this, "Failed to export asset names!");
    else
        UICommon::InfoMsg(this, "Success", "Asset names exported successfully!");
}

void CResourceBrowser::RebuildResourceDB()
{
    if (UICommon::YesNoQuestion(this, "Rebuild resource database", "Are you sure you want to rebuild the resource database? This will take a while."))
    {
        gpEdApp->RebuildResourceDatabase();
    }
}

void CResourceBrowser::UpdateFilter()
{
    QString SearchText = mpUI->SearchBar->text();
    mSearching = !SearchText.isEmpty();

    UpdateDescriptionLabel();
    mpProxyModel->SetSearchString( TO_TSTRING(mpUI->SearchBar->text()) );
    mpProxyModel->invalidate();
}

void CResourceBrowser::ResetTypeFilter()
{
    mpFilterAllBox->setChecked(true);
}

void CResourceBrowser::OnFilterTypeBoxTicked(bool Checked)
{
    // NOTE: there should only be one CResourceBrowser; if that ever changes for some reason change this to a member
    static bool ReentrantGuard = false;
    if (ReentrantGuard) return;
    ReentrantGuard = true;

    if (sender() == mpFilterAllBox)
    {
        if (!Checked && !mpProxyModel->HasTypeFilter())
            mpFilterAllBox->setChecked(true);

        else if (Checked)
        {
            foreach (const SResourceType& rkType, mTypeList)
            {
                rkType.pFilterCheckBox->setChecked(false);
                mpProxyModel->SetTypeFilter(rkType.pTypeInfo, false);
            }
        }
    }

    else
    {
        foreach (const SResourceType& rkType, mTypeList)
        {
            if (rkType.pFilterCheckBox == sender())
            {
                mpProxyModel->SetTypeFilter(rkType.pTypeInfo, Checked);
                break;
            }
        }

        mpFilterAllBox->setChecked(!mpProxyModel->HasTypeFilter());
    }

    mpProxyModel->invalidate();
    ReentrantGuard = false;
}

void CResourceBrowser::UpdateUndoActionStates()
{
    // Make sure that the undo actions are only enabled when the table view has focus.
    // This is to prevent them from conflicting with world editor undo/redo actions.
    bool HasFocus = (mpUI->ResourceTableView->hasFocus());
    mpUndoAction->setEnabled( HasFocus && mUndoStack.canUndo() );
    mpRedoAction->setEnabled( HasFocus && mUndoStack.canRedo() );
}

void CResourceBrowser::Undo()
{
    mUndoStack.undo();
    UpdateUndoActionStates();
}

void CResourceBrowser::Redo()
{
    mUndoStack.redo();
    UpdateUndoActionStates();
}
