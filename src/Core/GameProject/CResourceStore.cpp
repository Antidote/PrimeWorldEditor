#include "CResourceStore.h"
#include "CGameExporter.h"
#include "CGameProject.h"
#include "CResourceIterator.h"
#include "Core/IUIRelay.h"
#include "Core/Resource/CResource.h"
#include <Common/AssertMacro.h>
#include <Common/FileUtil.h>
#include <Common/Log.h>
#include <Common/Serialization/Binary.h>
#include <Common/Serialization/XML.h>
#include <tinyxml2.h>

using namespace tinyxml2;
CResourceStore *gpResourceStore = nullptr;
CResourceStore *gpEditorStore = nullptr;

// Constructor for editor store
CResourceStore::CResourceStore(const TString& rkDatabasePath)
    : mpProj(nullptr)
    , mGame(ePrime)
    , mDatabaseCacheDirty(false)
{
    mpDatabaseRoot = new CVirtualDirectory(this);
    mDatabasePath = FileUtil::MakeAbsolute(rkDatabasePath.GetFileDirectory());
    LoadDatabaseCache();
}

// Main constructor for game projects and game exporter
CResourceStore::CResourceStore(CGameProject *pProject)
    : mpProj(nullptr)
    , mGame(eUnknownGame)
    , mpDatabaseRoot(nullptr)
    , mDatabaseCacheDirty(false)
{
    SetProject(pProject);
}

CResourceStore::~CResourceStore()
{
    CloseProject();
    DestroyUnreferencedResources();

    for (auto It = mResourceEntries.begin(); It != mResourceEntries.end(); It++)
        delete It->second;
}

void RecursiveGetListOfEmptyDirectories(CVirtualDirectory *pDir, TStringList& rOutList)
{
    // Helper function for SerializeResourceDatabase
    if (pDir->IsEmpty())
    {
        rOutList.push_back(pDir->FullPath());
    }
    else
    {
        for (u32 SubIdx = 0; SubIdx < pDir->NumSubdirectories(); SubIdx++)
            RecursiveGetListOfEmptyDirectories(pDir->SubdirectoryByIndex(SubIdx), rOutList);
    }
}

bool CResourceStore::SerializeDatabaseCache(IArchive& rArc)
{
    // Serialize resources
    if (rArc.ParamBegin("Resources"))
    {
        // Serialize resources
        u32 ResourceCount = mResourceEntries.size();
        rArc << SERIAL_AUTO(ResourceCount);

        if (rArc.IsReader())
        {
            for (u32 ResIdx = 0; ResIdx < ResourceCount; ResIdx++)
            {
                if (rArc.ParamBegin("Resource"))
                {
                    CResourceEntry *pEntry = CResourceEntry::BuildFromArchive(this, rArc);
                    ASSERT( FindEntry(pEntry->ID()) == nullptr );
                    mResourceEntries[pEntry->ID()] = pEntry;
                    rArc.ParamEnd();
                }
            }
        }
        else
        {
            for (CResourceIterator It(this); It; ++It)
            {
                if (rArc.ParamBegin("Resource"))
                {
                    It->SerializeEntryInfo(rArc, false);
                    rArc.ParamEnd();
                }
            }
        }
        rArc.ParamEnd();
    }

    // Serialize empty directory list
    TStringList EmptyDirectories;

    if (!rArc.IsReader())
        RecursiveGetListOfEmptyDirectories(mpDatabaseRoot, EmptyDirectories);

    rArc << SERIAL_CONTAINER_AUTO(EmptyDirectories, "Directory");

    if (rArc.IsReader())
    {
        for (auto Iter = EmptyDirectories.begin(); Iter != EmptyDirectories.end(); Iter++)
            CreateVirtualDirectory(*Iter);
    }

    return true;
}

bool CResourceStore::LoadDatabaseCache()
{
    ASSERT(!mDatabasePath.IsEmpty());
    TString Path = DatabasePath();

    if (!mpDatabaseRoot)
        mpDatabaseRoot = new CVirtualDirectory(this);

    // Load the resource database
    CBinaryReader Reader(Path, FOURCC('CACH'));

    if (!Reader.IsValid() || !SerializeDatabaseCache(Reader))
    {
        if (gpUIRelay->AskYesNoQuestion("Error", "Failed to load the resource database. Attempt to build from the directory? (This may take a while.)"))
        {
            if (!BuildFromDirectory(true))
                return false;
        }
        else return false;
    }
    else
    {
        // Database is succesfully loaded at this point
        if (mpProj)
            ASSERT(mpProj->Game() == Reader.Game());
    }

    mGame = Reader.Game();
    return true;
}

bool CResourceStore::SaveDatabaseCache()
{
    TString Path = DatabasePath();

    CBinaryWriter Writer(Path, FOURCC('CACH'), 0, mGame);

    if (!Writer.IsValid())
        return false;

    SerializeDatabaseCache(Writer);
    mDatabaseCacheDirty = false;
    return true;
}

void CResourceStore::ConditionalSaveStore()
{
    if (mDatabaseCacheDirty)  SaveDatabaseCache();
}

void CResourceStore::SetProject(CGameProject *pProj)
{
    if (mpProj == pProj) return;

    if (mpProj)
        CloseProject();

    mpProj = pProj;

    if (mpProj)
    {
        TString DatabasePath = mpProj->ResourceDBPath(false);
        mDatabasePath = DatabasePath.GetFileDirectory();
        mpDatabaseRoot = new CVirtualDirectory(this);
        mGame = mpProj->Game();
    }
}

void CResourceStore::CloseProject()
{
    // Destroy unreferenced resources first. (This is necessary to avoid invalid memory accesses when
    // various TResPtrs are destroyed. There might be a cleaner solution than this.)
    DestroyUnreferencedResources();

    // There should be no loaded resources!!!
    // If there are, that means something didn't clean up resource references properly on project close!!!
    if (!mLoadedResources.empty())
    {
        Log::Error(TString::FromInt32(mLoadedResources.size(), 0, 10) + " resources still loaded on project close:");

        for (auto Iter = mLoadedResources.begin(); Iter != mLoadedResources.end(); Iter++)
        {
            CResourceEntry *pEntry = Iter->second;
            Log::Write("\t" + pEntry->Name() + "." + pEntry->CookedExtension().ToString());
        }

        ASSERT(false);
    }

    // Delete all entries from old project
    auto It = mResourceEntries.begin();

    while (It != mResourceEntries.end())
    {
        delete It->second;
        It = mResourceEntries.erase(It);
    }

    delete mpDatabaseRoot;
    mpDatabaseRoot = nullptr;
    mpProj = nullptr;
    mGame = eUnknownGame;
}

CVirtualDirectory* CResourceStore::GetVirtualDirectory(const TString& rkPath, bool AllowCreate)
{
    if (rkPath.IsEmpty())
        return mpDatabaseRoot;
    else if (mpDatabaseRoot)
        return mpDatabaseRoot->FindChildDirectory(rkPath, AllowCreate);
    else
        return nullptr;
}

void CResourceStore::CreateVirtualDirectory(const TString& rkPath)
{
    if (!rkPath.IsEmpty())
        mpDatabaseRoot->FindChildDirectory(rkPath, true);
}

void CResourceStore::ConditionalDeleteDirectory(CVirtualDirectory *pDir, bool Recurse)
{
    if (pDir->IsEmpty() && !pDir->IsRoot())
    {
        CVirtualDirectory *pParent = pDir->Parent();
        pParent->RemoveChildDirectory(pDir);

        if (Recurse)
        {
            ConditionalDeleteDirectory(pParent, true);
        }
    }
}

CResourceEntry* CResourceStore::FindEntry(const CAssetID& rkID) const
{
    if (!rkID.IsValid()) return nullptr;
    auto Found = mResourceEntries.find(rkID);
    if (Found == mResourceEntries.end()) return nullptr;
    else return Found->second;
}

CResourceEntry* CResourceStore::FindEntry(const TString& rkPath) const
{
    return (mpDatabaseRoot ? mpDatabaseRoot->FindChildResource(rkPath) : nullptr);
}

bool CResourceStore::AreAllEntriesValid() const
{
    for (CResourceIterator Iter(this); Iter; ++Iter)
    {
        if (!Iter->HasCookedVersion() && !Iter->HasRawVersion())
            return false;
    }

    return true;
}

void CResourceStore::ClearDatabase()
{
    // THIS OPERATION REQUIRES THAT ALL RESOURCES ARE UNREFERENCED
    DestroyUnreferencedResources();
    ASSERT(mLoadedResources.empty());

    // Clear out existing resource entries and directories
    for (auto Iter = mResourceEntries.begin(); Iter != mResourceEntries.end(); Iter++)
        delete Iter->second;
    mResourceEntries.clear();

    delete mpDatabaseRoot;
    mpDatabaseRoot = new CVirtualDirectory(this);

    mDatabaseCacheDirty = true;
}

bool CResourceStore::BuildFromDirectory(bool ShouldGenerateCacheFile)
{
    ASSERT(mResourceEntries.empty());

    // Get list of resources
    TString ResDir = ResourcesDir();
    TStringList ResourceList;
    FileUtil::GetDirectoryContents(ResDir, ResourceList);

    for (auto Iter = ResourceList.begin(); Iter != ResourceList.end(); Iter++)
    {
        TString Path = *Iter;
        TString RelPath = FileUtil::MakeRelative(Path, ResDir);

        if (FileUtil::IsFile(Path) && Path.GetFileExtension() == "rsmeta")
        {
            // Determine resource name
            TString DirPath = RelPath.GetFileDirectory();
            TString CookedFilename = RelPath.GetFileName(false); // This call removes the .rsmeta extension
            TString ResName = CookedFilename.GetFileName(false); // This call removes the cooked extension
            ASSERT( IsValidResourcePath(DirPath, ResName) );

            // Determine resource type
            TString CookedExtension = CookedFilename.GetFileExtension();
            CResTypeInfo *pTypeInfo = CResTypeInfo::TypeForCookedExtension( Game(), CFourCC(CookedExtension) );

            if (!pTypeInfo)
            {
                Log::Error("Found resource but couldn't register because failed to identify resource type: " + RelPath);
                continue;
            }

            // Create resource entry
            CResourceEntry *pEntry = CResourceEntry::BuildFromDirectory(this, pTypeInfo, DirPath, ResName);

            // Validate the entry
            CAssetID ID = pEntry->ID();
            ASSERT( mResourceEntries.find(ID) == mResourceEntries.end() );
            ASSERT( ID.Length() == CAssetID::GameIDLength(mGame) );

            mResourceEntries[ID] = pEntry;
        }

        else if (FileUtil::IsDirectory(Path))
            CreateVirtualDirectory(RelPath);
    }

    // Generate new cache file
    if (ShouldGenerateCacheFile)
    {
        // Make sure gpResourceStore points to this store
        CResourceStore *pOldStore = gpResourceStore;
        gpResourceStore = this;

        // Make sure audio manager is loaded correctly so AGSC dependencies can be looked up
        if (mpProj)
            mpProj->AudioManager()->LoadAssets();

        // Update dependencies
        for (CResourceIterator It(this); It; ++It)
            It->UpdateDependencies();

        // Update database file
        mDatabaseCacheDirty = true;
        ConditionalSaveStore();

        // Restore old gpResourceStore
        gpResourceStore = pOldStore;
    }

    return true;
}

void CResourceStore::RebuildFromDirectory()
{
    ASSERT(mpProj != nullptr);
    mpProj->AudioManager()->ClearAssets();
    ClearDatabase();
    BuildFromDirectory(true);
}

bool CResourceStore::IsResourceRegistered(const CAssetID& rkID) const
{
    return FindEntry(rkID) != nullptr;
}

CResourceEntry* CResourceStore::RegisterResource(const CAssetID& rkID, EResType Type, const TString& rkDir, const TString& rkName)
{
    CResourceEntry *pEntry = FindEntry(rkID);

    if (pEntry)
        Log::Error("Attempted to register resource that's already tracked in the database: " + rkID.ToString() + " / " + rkDir + " / " + rkName);

    else
    {
        // Validate directory
        if (IsValidResourcePath(rkDir, rkName))
        {
            pEntry = CResourceEntry::CreateNewResource(this, rkID, rkDir, rkName, Type);
            mResourceEntries[rkID] = pEntry;
        }

        else
            Log::Error("Invalid resource path, failed to register: " + rkDir + rkName);
    }

    return pEntry;
}

CResource* CResourceStore::LoadResource(const CAssetID& rkID)
{
    if (!rkID.IsValid()) return nullptr;

    CResourceEntry *pEntry = FindEntry(rkID);

    if (pEntry)
        return pEntry->Load();

    else
    {
        // Resource doesn't seem to exist
        Log::Error("Can't find requested resource with ID \"" + rkID.ToString() + "\".");;
        return nullptr;
    }
}

CResource* CResourceStore::LoadResource(const CAssetID& rkID, EResType Type)
{
    CResource *pRes = LoadResource(rkID);

    if (pRes)
    {
        if (pRes->Type() == Type)
        {
            return pRes;
        }
        else
        {
            CResTypeInfo *pExpectedType = CResTypeInfo::FindTypeInfo(Type);
            CResTypeInfo *pGotType = pRes->TypeInfo();
            ASSERT(pExpectedType && pGotType);

            Log::Error("Resource with ID \"" + rkID.ToString() + "\" requested with the wrong type; expected " + pExpectedType->TypeName() + " asset, got " + pGotType->TypeName() + " asset");
            return nullptr;
        }
    }

    else
        return nullptr;
}

CResource* CResourceStore::LoadResource(const TString& rkPath)
{
    // If this is a relative path then load via the resource DB
    CResourceEntry *pEntry = FindEntry(rkPath);

    if (pEntry)
    {
        // Verify extension matches the entry + load resource
        TString Ext = rkPath.GetFileExtension();

        if (!Ext.IsEmpty())
        {
            if (Ext.Length() == 4)
            {
                ASSERT( Ext.CaseInsensitiveCompare(pEntry->CookedExtension().ToString()) );
            }
            else
            {
                ASSERT( rkPath.EndsWith(pEntry->RawExtension()) );
            }
        }

        return pEntry->Load();
    }

    else return nullptr;
}

void CResourceStore::TrackLoadedResource(CResourceEntry *pEntry)
{
    ASSERT(pEntry->IsLoaded());
    ASSERT(mLoadedResources.find(pEntry->ID()) == mLoadedResources.end());
    mLoadedResources[pEntry->ID()] = pEntry;
}

void CResourceStore::DestroyUnreferencedResources()
{
    // This can be updated to avoid the do-while loop when reference lookup is implemented.
    u32 NumDeleted;

    do
    {
        NumDeleted = 0;
        auto It = mLoadedResources.begin();

        while (It != mLoadedResources.end())
        {
            CResourceEntry *pEntry = It->second;

            if (!pEntry->Resource()->IsReferenced() && pEntry->Unload())
            {
                It = mLoadedResources.erase(It);
                NumDeleted++;
            }

            else It++;
        }
    } while (NumDeleted > 0);
}

bool CResourceStore::DeleteResourceEntry(CResourceEntry *pEntry)
{
    CAssetID ID = pEntry->ID();

    if (pEntry->IsLoaded())
    {
        if (!pEntry->Unload())
            return false;

        auto It = mLoadedResources.find(ID);
        ASSERT(It != mLoadedResources.end());
        mLoadedResources.erase(It);
    }

    if (pEntry->Directory())
        pEntry->Directory()->RemoveChildResource(pEntry);

    auto It = mResourceEntries.find(ID);
    ASSERT(It != mResourceEntries.end());
    mResourceEntries.erase(It);

    delete pEntry;
    return true;
}

void CResourceStore::ImportNamesFromPakContentsTxt(const TString& rkTxtPath, bool UnnamedOnly)
{
    // Read file contents -first- then move assets -after-; this
    // 1. avoids anything fucking up if the contents file is badly formatted and we crash, and
    // 2. avoids extra redundant moves (since there are redundant entries in the file)
    // todo: move to CAssetNameMap?
    std::map<CResourceEntry*, TString> PathMap;
    FILE *pContentsFile;
    fopen_s(&pContentsFile, *rkTxtPath, "r");

    if (!pContentsFile)
    {
        Log::Error("Failed to open .contents.txt file: " + rkTxtPath);
        return;
    }

    while (!feof(pContentsFile))
    {
        // Get new line, parse to extract the ID/path
        char LineBuffer[512];
        fgets(LineBuffer, 512, pContentsFile);

        TString Line(LineBuffer);
        if (Line.IsEmpty()) break;

        u32 IDStart = Line.IndexOfPhrase("0x") + 2;
        if (IDStart == 1) continue;

        u32 IDEnd = Line.IndexOf(" \t", IDStart);
        u32 PathStart = IDEnd + 1;
        u32 PathEnd = Line.Size() - 5;

        TString IDStr = Line.SubString(IDStart, IDEnd - IDStart);
        TString Path = Line.SubString(PathStart, PathEnd - PathStart);

        CAssetID ID = CAssetID::FromString(IDStr);
        CResourceEntry *pEntry = FindEntry(ID);

        // Only process this entry if the ID exists
        if (pEntry)
        {
            // Chop name to just after "x_rep"
            u32 RepStart = Path.IndexOfPhrase("_rep");

            if (RepStart != -1)
                Path = Path.ChopFront(RepStart + 5);

            // If the "x_rep" folder doesn't exist in this path for some reason, but this is still a path, then just chop off the drive letter.
            // Otherwise, this is most likely just a standalone name, so use the full name as-is.
            else if (Path[1] == ':')
                Path = Path.ChopFront(3);

            PathMap[pEntry] = Path;
        }
    }

    fclose(pContentsFile);

    // Assign names
    for (auto Iter = PathMap.begin(); Iter != PathMap.end(); Iter++)
    {
        CResourceEntry *pEntry = Iter->first;
        if (UnnamedOnly && pEntry->IsNamed()) continue;

        TString Path = Iter->second;
        TString Dir = Path.GetFileDirectory();
        TString Name = Path.GetFileName(false);
        if (Dir.IsEmpty()) Dir = pEntry->DirectoryPath();

        pEntry->Move(Dir, Name);
    }

    // Save
    ConditionalSaveStore();
}

bool CResourceStore::IsValidResourcePath(const TString& rkPath, const TString& rkName)
{
    // Path must not be an absolute path and must not go outside the project structure.
    // Name must not be a path.
    return ( CVirtualDirectory::IsValidDirectoryPath(rkPath) &&
             FileUtil::IsValidName(rkName, false) &&
             !rkName.Contains('/') &&
             !rkName.Contains('\\') );
}
