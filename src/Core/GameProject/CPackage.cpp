#include "CPackage.h"
#include "DependencyListBuilders.h"
#include "CGameProject.h"
#include "Core/CompressionUtil.h"
#include "Core/Resource/Cooker/CWorldCooker.h"
#include <Common/AssertMacro.h>
#include <Common/FileIO.h>
#include <Common/FileUtil.h>
#include <Common/Serialization/XML.h>

using namespace tinyxml2;

bool CPackage::Load()
{
    TString DefPath = DefinitionPath(false);
    CXMLReader Reader(DefPath);

    if (Reader.IsValid())
    {
        Serialize(Reader);
        mCacheDirty = true;
        return true;
    }
    else return false;
}

bool CPackage::Save()
{
    TString DefPath = DefinitionPath(false);
    FileUtil::MakeDirectory(DefPath.GetFileDirectory());

    CXMLWriter Writer(DefPath, "PackageDefinition", 0, mpProject ? mpProject->Game() : eUnknownGame);
    Serialize(Writer);
    return Writer.Save();
}

void CPackage::Serialize(IArchive& rArc)
{
    rArc << SERIAL("NeedsRecook", mNeedsRecook)
         << SERIAL_CONTAINER("NamedResources", mResources, "Resource");
}

void CPackage::AddResource(const TString& rkName, const CAssetID& rkID, const CFourCC& rkType)
{
    mResources.push_back( SNamedResource { rkName, rkID, rkType } );
    mCacheDirty = true;
}

void CPackage::UpdateDependencyCache() const
{
    CPackageDependencyListBuilder Builder(this);
    std::list<CAssetID> AssetList;
    Builder.BuildDependencyList(false, AssetList);

    mCachedDependencies.clear();
    for (auto Iter = AssetList.begin(); Iter != AssetList.end(); Iter++)
        mCachedDependencies.insert(*Iter);

    mCacheDirty = false;
}

void CPackage::Cook(IProgressNotifier *pProgress)
{
    SCOPED_TIMER(CookPackage);

    // Build asset list
    pProgress->Report(-1, -1, "Building dependency list");

    CPackageDependencyListBuilder Builder(this);
    std::list<CAssetID> AssetList;
    Builder.BuildDependencyList(true, AssetList);
    Log::Write(TString::FromInt32(AssetList.size(), 0, 10) + " assets in " + Name() + ".pak");

    // Write new pak
    TString PakPath = CookedPackagePath(false);
    CFileOutStream Pak(PakPath, IOUtil::eBigEndian);

    if (!Pak.IsValid())
    {
        Log::Error("Couldn't cook package " + CookedPackagePath(true) + "; unable to open package for writing");
        return;
    }

    EGame Game = mpProject->Game();
    u32 Alignment = (Game <= eCorruptionProto ? 0x20 : 0x40);
    u32 AlignmentMinusOne = Alignment - 1;

    u32 TocOffset = 0;
    u32 NamesSize = 0;
    u32 ResTableOffset = 0;
    u32 ResTableSize = 0;
    u32 ResDataSize = 0;

    // Write MP1 pak header
    if (Game <= eCorruptionProto)
    {
        Pak.WriteLong(0x00030005); // Major/Minor Version
        Pak.WriteLong(0); // Unknown

        // Named Resources
        Pak.WriteLong(mResources.size());

        for (auto Iter = mResources.begin(); Iter != mResources.end(); Iter++)
        {
            const SNamedResource& rkRes = *Iter;
            rkRes.Type.Write(Pak);
            rkRes.ID.Write(Pak);
            Pak.WriteSizedString(rkRes.Name);
        }
    }

    // Write MP3 pak header
    else
    {
        // Header
        Pak.WriteLong(2); // Version
        Pak.WriteLong(0x40); // Header size
        Pak.WriteToBoundary(0x40, 0); // We don't care about the MD5 hash; the game doesn't use it

        // PAK table of contents; write later
        TocOffset = Pak.Tell();
        Pak.WriteLong(0);
        Pak.WriteToBoundary(0x40, 0);

        // Named Resources
        u32 NamesStart = Pak.Tell();
        Pak.WriteLong(mResources.size());

        for (auto Iter = mResources.begin(); Iter != mResources.end(); Iter++)
        {
            const SNamedResource& rkRes = *Iter;
            Pak.WriteString(rkRes.Name);
            rkRes.Type.Write(Pak);
            rkRes.ID.Write(Pak);
        }

        Pak.WriteToBoundary(0x40, 0);
        NamesSize = Pak.Tell() - NamesStart;
    }

    // Fill in resource table with junk, write later
    ResTableOffset = Pak.Tell();
    Pak.WriteLong(AssetList.size());
    CAssetID Dummy = CAssetID::InvalidID(Game);

    for (u32 iRes = 0; iRes < AssetList.size(); iRes++)
    {
        Pak.WriteLongLong(0);
        Dummy.Write(Pak);
        Pak.WriteLongLong(0);
    }

    Pak.WriteToBoundary(Alignment, 0);
    ResTableSize = Pak.Tell() - ResTableOffset;

    // Start writing resources
    struct SResourceTableInfo
    {
        CResourceEntry *pEntry;
        u32 Offset;
        u32 Size;
        bool Compressed;
    };
    std::vector<SResourceTableInfo> ResourceTableData(AssetList.size());
    u32 ResIdx = 0;
    u32 ResDataOffset = Pak.Tell();

    for (auto Iter = AssetList.begin(); Iter != AssetList.end() && !pProgress->ShouldCancel(); Iter++, ResIdx++)
    {
        // Initialize entry, recook assets if needed
        u32 AssetOffset = Pak.Tell();
        CAssetID ID = *Iter;
        CResourceEntry *pEntry = gpResourceStore->FindEntry(ID);
        ASSERT(pEntry != nullptr);

        if (pEntry->NeedsRecook())
        {
            pProgress->Report(ResIdx, AssetList.size(), "Cooking asset: " + pEntry->Name() + "." + pEntry->CookedExtension());
            pEntry->Cook();
        }

        // Update progress bar
        if (ResIdx & 0x1 || ResIdx == AssetList.size() - 1)
        {
            pProgress->Report(ResIdx, AssetList.size(), TString::Format("Writing asset %d/%d: %s", ResIdx+1, AssetList.size(), *(pEntry->Name() + "." + pEntry->CookedExtension())));
        }

        // Update table info
        SResourceTableInfo& rTableInfo = ResourceTableData[ResIdx];
        rTableInfo.pEntry = pEntry;
        rTableInfo.Offset = (Game <= eEchoes ? AssetOffset : AssetOffset - ResDataOffset);

        // Load resource data
        CFileInStream CookedAsset(pEntry->CookedAssetPath(), IOUtil::eBigEndian);
        ASSERT(CookedAsset.IsValid());
        u32 ResourceSize = CookedAsset.Size();

        std::vector<u8> ResourceData(ResourceSize);
        CookedAsset.ReadBytes(ResourceData.data(), ResourceData.size());

        // Check if this asset should be compressed; there are a few resource types that are
        // always compressed, and some types that are compressed if they're over a certain size
        EResType Type = pEntry->ResourceType();
        u32 CompressThreshold = (Game <= eCorruptionProto ? 0x400 : 0x80);

        bool ShouldAlwaysCompress = (Type == eTexture || Type == eModel || Type == eSkin ||
                                     Type == eAnimSet || Type == eAnimation || Type == eFont);

        if (Game >= eCorruption)
        {
            ShouldAlwaysCompress = ShouldAlwaysCompress ||
                                   (Type == eCharacter || Type == eSourceAnimData || Type == eScan ||
                                    Type == eAudioSample || Type == eStringTable || Type == eAudioAmplitudeData ||
                                    Type == eDynamicCollision);
        }

        bool ShouldCompressConditional = !ShouldAlwaysCompress &&
                (Type == eParticle || Type == eParticleElectric || Type == eParticleSwoosh ||
                 Type == eParticleWeapon || Type == eParticleDecal || Type == eParticleCollisionResponse ||
                 Type == eParticleSpawn || Type == eParticleSorted || Type == eBurstFireData);

        bool ShouldCompress = ShouldAlwaysCompress || (ShouldCompressConditional && ResourceSize >= CompressThreshold);

        // Write resource data to pak
        if (!ShouldCompress)
        {
            Pak.WriteBytes(ResourceData.data(), ResourceSize);
            rTableInfo.Compressed = false;
        }

        else
        {
            u32 CompressedSize;
            std::vector<u8> CompressedData(ResourceData.size() * 2);
            bool Success = false;

            if (Game <= eEchoesDemo || Game == eReturns)
                Success = CompressionUtil::CompressZlib(ResourceData.data(), ResourceData.size(), CompressedData.data(), CompressedData.size(), CompressedSize);
            else
                Success = CompressionUtil::CompressLZOSegmented(ResourceData.data(), ResourceData.size(), CompressedData.data(), CompressedSize, false);

            // Make sure that the compressed data is actually smaller, accounting for padding + uncompressed size value
            if (Success)
            {
                u32 CompressionHeaderSize = (Game <= eCorruptionProto ? 4 : 0x10);
                u32 PaddedUncompressedSize = (ResourceSize + AlignmentMinusOne) & ~AlignmentMinusOne;
                u32 PaddedCompressedSize = (CompressedSize + CompressionHeaderSize + AlignmentMinusOne) & ~AlignmentMinusOne;
                Success = (PaddedCompressedSize < PaddedUncompressedSize);
            }

            // Write file to pak
            if (Success)
            {
                // Write MP1/2 compressed asset
                if (Game <= eCorruptionProto)
                {
                    Pak.WriteLong(ResourceSize);
                }
                // Write MP3/DKCR compressed asset
                else
                {
                    // Note: Compressed asset data can be stored in multiple blocks. Normally, the only assets that make use of this are textures,
                    // which can store each separate component of the file (header, palette, image data) in separate blocks. However, some textures
                    // are stored in one block, and I've had no luck figuring out why. The game doesn't generally seem to care whether textures use
                    // multiple blocks or not, so for the sake of complicity we compress everything to one block.
                    Pak.WriteFourCC( FOURCC('CMPD') );
                    Pak.WriteLong(1);
                    Pak.WriteLong(0xA0000000 | CompressedSize);
                    Pak.WriteLong(ResourceSize);
                }
                Pak.WriteBytes(CompressedData.data(), CompressedSize);
            }
            else
                Pak.WriteBytes(ResourceData.data(), ResourceSize);

            rTableInfo.Compressed = Success;
        }

        Pak.WriteToBoundary(Alignment, 0xFF);
        rTableInfo.Size = Pak.Tell() - AssetOffset;
    }
    ResDataSize = Pak.Tell() - ResDataOffset;

    // If we cancelled, don't finish writing the pak; delete the file instead and make sure the package is flagged for recook
    if (pProgress->ShouldCancel())
    {
        Pak.Close();
        FileUtil::DeleteFile(PakPath);
        mNeedsRecook = true;
    }

    else
    {
        // Write table of contents for real
        if (Game >= eCorruption)
        {
            Pak.Seek(TocOffset, SEEK_SET);
            Pak.WriteLong(3); // Always 3 pak sections
            Pak.WriteFourCC( FOURCC('STRG') );
            Pak.WriteLong(NamesSize);
            Pak.WriteFourCC( FOURCC('RSHD') );
            Pak.WriteLong(ResTableSize);
            Pak.WriteFourCC( FOURCC('DATA') );
            Pak.WriteLong(ResDataSize);
        }

        // Write resource table for real
        Pak.Seek(ResTableOffset+4, SEEK_SET);

        for (u32 iRes = 0; iRes < AssetList.size(); iRes++)
        {
            const SResourceTableInfo& rkInfo = ResourceTableData[iRes];
            CResourceEntry *pEntry = rkInfo.pEntry;

            Pak.WriteLong( rkInfo.Compressed ? 1 : 0 );
            pEntry->CookedExtension().Write(Pak);
            pEntry->ID().Write(Pak);
            Pak.WriteLong(rkInfo.Size);
            Pak.WriteLong(rkInfo.Offset);
        }

        // Clear recook flag
        mNeedsRecook = false;
        Log::Write("Finished writing " + PakPath);
    }

    Save();

    // Update resource store in case we recooked any assets
    mpProject->ResourceStore()->ConditionalSaveStore();
}

void CPackage::CompareOriginalAssetList(const std::list<CAssetID>& rkNewList)
{
    // Debug - take the newly generated rkNewList and compare it with the asset list
    // from the original pak, and print info about any extra or missing resources
    // Build a set out of the generated list
    std::set<CAssetID> NewListSet;

    for (auto Iter = rkNewList.begin(); Iter != rkNewList.end(); Iter++)
        NewListSet.insert(*Iter);

    // Read the original pak
    TString CookedPath = CookedPackagePath(false);
    CFileInStream Pak(CookedPath, IOUtil::eBigEndian);

    if (!Pak.IsValid() || Pak.Size() == 0)
    {
        Log::Error("Failed to compare to original asset list; couldn't open the original pak");
        return;
    }

    // Determine pak version
    u32 PakVersion = Pak.ReadLong();
    std::set<CAssetID> OldListSet;

    // Read MP1/2 pak
    if (PakVersion == 0x00030005)
    {
        Pak.Seek(0x4, SEEK_CUR);
        u32 NumNamedResources = Pak.ReadLong();

        for (u32 iName = 0; iName < NumNamedResources; iName++)
        {
            Pak.Seek(0x8, SEEK_CUR);
            u32 NameLen = Pak.ReadLong();
            Pak.Seek(NameLen, SEEK_CUR);
        }

        // Build a set out of the original pak resource list
        u32 NumResources = Pak.ReadLong();

        for (u32 iRes = 0; iRes < NumResources; iRes++)
        {
            Pak.Seek(0x8, SEEK_CUR);
            OldListSet.insert( CAssetID(Pak, e32Bit) );
            Pak.Seek(0x8, SEEK_CUR);
        }
    }

    // Read MP3/DKCR pak
    else
    {
        ASSERT(PakVersion == 0x2);

        // Skip named resources
        Pak.Seek(0x44, SEEK_SET);
        CFourCC StringSecType = Pak.ReadLong();
        u32 StringSecSize = Pak.ReadLong();
        ASSERT(StringSecType == "STRG");

        Pak.Seek(0x80 + StringSecSize, SEEK_SET);

        // Read resource table
        u32 NumResources = Pak.ReadLong();

        for (u32 iRes = 0; iRes < NumResources; iRes++)
        {
            Pak.Seek(0x8, SEEK_CUR);
            OldListSet.insert( CAssetID(Pak, e64Bit) );
            Pak.Seek(0x8, SEEK_CUR);
        }
    }

    // Check for missing resources in the new list
    for (auto Iter = OldListSet.begin(); Iter != OldListSet.end(); Iter++)
    {
        CAssetID ID = *Iter;

        if (NewListSet.find(ID) == NewListSet.end())
        {
            CResourceEntry *pEntry = gpResourceStore->FindEntry(ID);
            TString Extension = (pEntry ? "." + pEntry->CookedExtension() : "");
            Log::Error("Missing resource: " + ID.ToString() + Extension);
        }
    }

    // Check for extra resources in the new list
    for (auto Iter = NewListSet.begin(); Iter != NewListSet.end(); Iter++)
    {
        CAssetID ID = *Iter;

        if (OldListSet.find(ID) == OldListSet.end())
        {
            CResourceEntry *pEntry = gpResourceStore->FindEntry(ID);
            TString Extension = (pEntry ? "." + pEntry->CookedExtension() : "");
            Log::Error("Extra resource: " + ID.ToString() + Extension);
        }
    }
}

bool CPackage::ContainsAsset(const CAssetID& rkID) const
{
    if (mCacheDirty)
        UpdateDependencyCache();

    return mCachedDependencies.find(rkID) != mCachedDependencies.end();
}

TString CPackage::DefinitionPath(bool Relative) const
{
    TString RelPath = mPakPath + mPakName + ".pkd";
    return Relative ? RelPath : mpProject->PackagesDir(false) + RelPath;
}

TString CPackage::CookedPackagePath(bool Relative) const
{
    TString RelPath = mPakPath + mPakName + ".pak";
    return Relative ? RelPath : mpProject->DiscDir(false) + RelPath;
}
