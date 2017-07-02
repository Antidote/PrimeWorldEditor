#include "CAssetNameMap.h"

bool CAssetNameMap::LoadAssetNames(TString Path /*= ""*/)
{
    if (Path.IsEmpty())
        Path = DefaultNameMapPath(mIDLength);

    CXMLReader Reader(Path);

    if (Reader.IsValid())
    {
        EIDLength FileIDLength = CAssetID::GameIDLength(Reader.Game());

        if (FileIDLength == mIDLength)
        {
            Serialize(Reader);
            return true;
        }
        else
        {
            TString ExpectedIDLength = (mIDLength == e32Bit ? "32-bit" : "64-bit");
            TString GotIDLength = (FileIDLength == e32Bit ? "32-bit" : "64-bit");
            Log::Error("Failed to load asset names; expected " + ExpectedIDLength + " IDs, got " + GotIDLength);
            return false;
        }
    }
    else
    {
        Log::Error("Failed to load asset names; couldn't open XML.");
        return false;
    }
}

bool CAssetNameMap::SaveAssetNames(TString Path /*= ""*/)
{
    if (Path.IsEmpty())
        Path = DefaultNameMapPath(mIDLength);

    EGame Game = (mIDLength == e32Bit ? ePrime : eCorruption);
    CXMLWriter Writer(Path, "AssetNameMap", 0, Game);
    Serialize(Writer);
    return Writer.Save();
}

bool CAssetNameMap::GetNameInfo(CAssetID ID, TString& rOutDirectory, TString& rOutName, bool& rOutAutoGenDir, bool& rOutAutoGenName)
{
    auto It = mMap.find(ID);

    if (It != mMap.end())
    {
        SAssetNameInfo& rInfo = It->second;
        rOutName = rInfo.Name;
        rOutDirectory = rInfo.Directory;
        rOutAutoGenDir = rInfo.AutoGenDir;
        rOutAutoGenName = rInfo.AutoGenName;
        return true;
    }

    else
    {
        rOutDirectory = "Uncategorized/";
        rOutName = ID.ToString();
        rOutAutoGenDir = true;
        rOutAutoGenName = true;
        return false;
    }
}

void CAssetNameMap::CopyFromStore(CResourceStore *pStore /*= gpResourceStore*/)
{
    // Do a first pass to remove old paths from used set to prevent false positives from eg. if two resources switch places
    ASSERT( CAssetID::GameIDLength(pStore->Game()) == mIDLength );

    for (CResourceIterator It(pStore); It; ++It)
    {
        if (It->IsCategorized() || It->IsNamed())
        {
            auto Find = mMap.find(It->ID());

            if (Find != mMap.end())
            {
                SAssetNameInfo& rInfo = Find->second;
                auto UsedFind = mUsedSet.find(rInfo);
                ASSERT(UsedFind != mUsedSet.end());
                mUsedSet.erase(UsedFind);
            }
        }
    }

    // Do a second pass to add the new paths to the map
    for (CResourceIterator It(pStore); It; ++It)
    {
        if (It->IsCategorized() || It->IsNamed())
        {
            CAssetID ID = It->ID();
            ASSERT(ID.Length() == mIDLength);

            TString Name = It->Name();
            TString Directory = It->Directory()->FullPath();
            CFourCC Type = It->CookedExtension();
            bool AutoName = It->HasFlag(eREF_AutoResName);
            bool AutoDir = It->HasFlag(eREF_AutoResDir);
            SAssetNameInfo NameInfo { Name, Directory, Type, AutoName, AutoDir };

            // Check for conflicts with new name
            if (mUsedSet.find(NameInfo) != mUsedSet.end())
            {
                SAssetNameInfo NewNameInfo = NameInfo;
                int NumConflicted = 0;

                while (mUsedSet.find(NewNameInfo) != mUsedSet.end())
                {
                    NewNameInfo.Name = NameInfo.Name + '_' + TString::FromInt32(NumConflicted, 0, 10);
                    NumConflicted++;
                }

                TString OldPath = NameInfo.FullPath();
                TString NewPath = NewNameInfo.FullPath();
                Log::Warning("Detected name conflict when copying asset name from the resource store; renaming.");
                Log::Warning("\tOld Path: " + OldPath);
                Log::Warning("\tNew Path: " + NewPath);
                NameInfo.Name = NewNameInfo.Name;
            }

            // Assign to map
            mMap[ID] = NameInfo;
            mUsedSet.insert(NameInfo);
        }
    }
}

// ************ PRIVATE ************
void CAssetNameMap::Serialize(IArchive& rArc)
{
    rArc << SERIAL_CONTAINER("AssetNameMap", mMap, "Asset");

    if (rArc.IsReader())
        PostLoadValidate();
}

void CAssetNameMap::PostLoadValidate()
{
    // Make sure the newly loaded map doesn't contain any errors or name conflicts.
    bool FoundErrors = false;
    mIsValid = false;
    std::set<SAssetNameInfo> Dupes;

    for (auto Iter = mMap.begin(); Iter != mMap.end(); Iter++)
    {
        const SAssetNameInfo& rkInfo = Iter->second;

        if (mUsedSet.find(rkInfo) != mUsedSet.end())
            Dupes.insert(rkInfo);

        else
        {
            mUsedSet.insert(rkInfo);

            // Verify the name/path is valid
            if (!CResourceStore::IsValidResourcePath(rkInfo.Directory, rkInfo.Name))
            {
                Log::Error("Invalid resource path in asset name map: " + rkInfo.Directory + rkInfo.Name + "." + rkInfo.Type.ToString());
                Iter = mMap.erase(Iter);
                FoundErrors = true;
            }

            // Verify correct ID length
            if (Iter->first.Length() != mIDLength)
            {
                Log::Error("Incorrect asset ID length in asset name map: " + Iter->first.ToString());
                Iter = mMap.erase(Iter);
                FoundErrors = true;
            }
        }
    }

    // If we detected any dupes, then this map can't be used
    if (!Dupes.empty())
    {
        Log::Error("Asset name map is invalid and cannot be used! Duplicate asset entries detected:");

        for (auto Iter = Dupes.begin(); Iter != Dupes.end(); Iter++)
        {
            const SAssetNameInfo& rkInfo = *Iter;
            TString FullPath = rkInfo.FullPath();
            Log::Error("\t" + FullPath);
        }

        mMap.clear();
    }
    else
        mIsValid = !FoundErrors;
}

TString CAssetNameMap::DefaultNameMapPath(EIDLength IDLength)
{
    ASSERT(IDLength != eInvalidIDLength);
    TString Suffix = (IDLength == e32Bit ? "32" : "64");
    return gkAssetMapPath + Suffix + "." + gkAssetMapExt;
}

TString CAssetNameMap::DefaultNameMapPath(EGame Game)
{
    return DefaultNameMapPath( CAssetID::GameIDLength(Game) );
}
