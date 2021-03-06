#include "CTextureDecoder.h"
#include <Common/Log.h>
#include <Common/CColor.h>

// A cleanup is warranted at some point. Trying to support both partial + full decode ended up really messy.

// Number of pixels * this = number of bytes
static const float gskPixelsToBytes[] = {
    2.f, 2.f, 2.f, 2.f, 4.f, 4.f, 0.f, 2.f, 4.f, 4.f, 0.5f
};

// Bits per pixel for each GX texture format
static const uint32 gskSourceBpp[] = {
    4, 8, 8, 16, 4, 8, 16, 16, 16, 32, 4
};

// Bits per pixel for each GX texture format when decoded
static const uint32 gskOutputBpp[] = {
    16, 16, 16, 16, 16, 16, 16, 16, 32, 32, 4
};

// Size of one pixel in output data in bytes
static const uint32 gskOutputPixelStride[] = {
    2, 2, 2, 2, 2, 2, 2, 2, 4, 4, 8
};

// Block width for each GX texture format
static const uint32 gskBlockWidth[] = {
    8, 8, 8, 4, 8, 8, 4, 4, 4, 4, 2
};

// Block height for each GX texture format
static const uint32 gskBlockHeight[] = {
    8, 4, 4, 4, 8, 4, 4, 4, 4, 4, 2
};

CTextureDecoder::CTextureDecoder()
{
}

CTextureDecoder::~CTextureDecoder()
{
}

CTexture* CTextureDecoder::CreateTexture()
{
    CTexture *pTex = new CTexture(mpEntry);
    pTex->mSourceTexelFormat = mTexelFormat;
    pTex->mWidth = mWidth;
    pTex->mHeight = mHeight;
    pTex->mNumMipMaps = mNumMipMaps;
    pTex->mLinearSize = (uint32) (mWidth * mHeight * gskPixelsToBytes[(int) mTexelFormat]);
    pTex->mpImgDataBuffer = mpDataBuffer;
    pTex->mImgDataSize = mDataBufferSize;
    pTex->mBufferExists = true;

    switch (mTexelFormat)
    {
        case ETexelFormat::GX_I4:
        case ETexelFormat::GX_I8:
        case ETexelFormat::GX_IA4:
        case ETexelFormat::GX_IA8:
            pTex->mTexelFormat = ETexelFormat::LuminanceAlpha;
            break;
        case ETexelFormat::GX_RGB565:
            pTex->mTexelFormat = ETexelFormat::RGB565;
            break;
        case ETexelFormat::GX_C4:
        case ETexelFormat::GX_C8:
            if (mPaletteFormat == EGXPaletteFormat::IA8)    pTex->mTexelFormat = ETexelFormat::LuminanceAlpha;
            if (mPaletteFormat == EGXPaletteFormat::RGB565) pTex->mTexelFormat = ETexelFormat::RGB565;
            if (mPaletteFormat == EGXPaletteFormat::RGB5A3) pTex->mTexelFormat = ETexelFormat::RGBA8;
            break;
        case ETexelFormat::GX_RGB5A3:
        case ETexelFormat::GX_RGBA8:
            pTex->mTexelFormat = ETexelFormat::RGBA8;
            break;
        case ETexelFormat::GX_CMPR:
            pTex->mTexelFormat = ETexelFormat::DXT1;
            break;
        case ETexelFormat::DXT1:
            pTex->mTexelFormat = ETexelFormat::DXT1;
            pTex->mLinearSize = mWidth * mHeight / 2;
            break;
        default:
            pTex->mTexelFormat = mTexelFormat;
            break;
    }

    return pTex;
}

// ************ STATIC ************
CTexture* CTextureDecoder::LoadTXTR(IInputStream& rTXTR, CResourceEntry *pEntry)
{
    CTextureDecoder Decoder;
    Decoder.mpEntry = pEntry;
    Decoder.ReadTXTR(rTXTR);
    Decoder.PartialDecodeGXTexture(rTXTR);
    return Decoder.CreateTexture();
}

CTexture* CTextureDecoder::DoFullDecode(IInputStream& rTXTR, CResourceEntry *pEntry)
{
    CTextureDecoder Decoder;
    Decoder.mpEntry = pEntry;
    Decoder.ReadTXTR(rTXTR);
    Decoder.FullDecodeGXTexture(rTXTR);

    CTexture *pTexture = Decoder.CreateTexture();
    pTexture->mTexelFormat = ETexelFormat::RGBA8;
    return pTexture;
}

CTexture* CTextureDecoder::LoadDDS(IInputStream& rDDS, CResourceEntry *pEntry)
{
    CTextureDecoder Decoder;
    Decoder.mpEntry = pEntry;
    Decoder.ReadDDS(rDDS);
    Decoder.DecodeDDS(rDDS);
    return Decoder.CreateTexture();
}

CTexture* CTextureDecoder::DoFullDecode(CTexture* /*pTexture*/)
{
    return nullptr;
}

// ************ READ ************
void CTextureDecoder::ReadTXTR(IInputStream& rTXTR)
{
    // Read TXTR header
    mTexelFormat = ETexelFormat(rTXTR.ReadLong());
    mWidth = rTXTR.ReadShort();
    mHeight = rTXTR.ReadShort();
    mNumMipMaps = rTXTR.ReadLong();

    // For C4 and C8 images, read palette
    if ((mTexelFormat == ETexelFormat::GX_C4) || (mTexelFormat == ETexelFormat::GX_C8))
    {
        mHasPalettes = true;
        mPaletteFormat = EGXPaletteFormat(rTXTR.ReadLong());
        rTXTR.Seek(0x4, SEEK_CUR);

        uint32 PaletteEntryCount = (mTexelFormat == ETexelFormat::GX_C4) ? 16 : 256;
        mPalettes.resize(PaletteEntryCount * 2);
        rTXTR.ReadBytes(mPalettes.data(), mPalettes.size());

        mPaletteInput.SetData(mPalettes.data(), mPalettes.size(), EEndian::BigEndian);
    }
    else mHasPalettes = false;
}

void CTextureDecoder::ReadDDS(IInputStream& rDDS)
{
    // Header
    CFourCC Magic(rDDS);
    if (Magic != FOURCC('DDS '))
    {
        errorf("%s: Invalid DDS magic: 0x%08X", *rDDS.GetSourceString(), Magic.ToLong());
        return;
    }

    uint32 ImageDataStart = rDDS.Tell() + rDDS.ReadLong();
    rDDS.Seek(0x4, SEEK_CUR); // Skipping flags
    mHeight = (uint16) rDDS.ReadLong();
    mWidth = (uint16) rDDS.ReadLong();
    rDDS.Seek(0x8, SEEK_CUR); // Skipping linear size + depth
    mNumMipMaps = rDDS.ReadLong() + 1; // DDS doesn't seem to count the first mipmap
    rDDS.Seek(0x2C, SEEK_CUR); // Skipping reserved

    // Pixel Format
    rDDS.Seek(0x4, SEEK_CUR); // Skipping size
    mDDSInfo.Flags = rDDS.ReadLong();
    CFourCC Format(rDDS);

    if (Format == "DXT1")      mDDSInfo.Format = SDDSInfo::DXT1;
    else if (Format == "DXT2") mDDSInfo.Format = SDDSInfo::DXT2;
    else if (Format == "DXT3") mDDSInfo.Format = SDDSInfo::DXT3;
    else if (Format == "DXT4") mDDSInfo.Format = SDDSInfo::DXT4;
    else if (Format == "DXT5") mDDSInfo.Format = SDDSInfo::DXT5;
    else
    {
        mDDSInfo.Format = SDDSInfo::RGBA;
        mDDSInfo.BitCount = rDDS.ReadLong();
        mDDSInfo.RBitMask = rDDS.ReadLong();
        mDDSInfo.GBitMask = rDDS.ReadLong();
        mDDSInfo.BBitMask = rDDS.ReadLong();
        mDDSInfo.ABitMask = rDDS.ReadLong();
        mDDSInfo.RShift = CalculateShiftForMask(mDDSInfo.RBitMask);
        mDDSInfo.GShift = CalculateShiftForMask(mDDSInfo.GBitMask);
        mDDSInfo.BShift = CalculateShiftForMask(mDDSInfo.BBitMask);
        mDDSInfo.AShift = CalculateShiftForMask(mDDSInfo.ABitMask);
        mDDSInfo.RSize = CalculateMaskBitCount(mDDSInfo.RBitMask);
        mDDSInfo.GSize = CalculateMaskBitCount(mDDSInfo.GBitMask);
        mDDSInfo.BSize = CalculateMaskBitCount(mDDSInfo.BBitMask);
        mDDSInfo.ASize = CalculateMaskBitCount(mDDSInfo.ABitMask);
    }

    // Skip the rest
    rDDS.Seek(ImageDataStart, SEEK_SET);
}

// ************ DECODE ************
void CTextureDecoder::PartialDecodeGXTexture(IInputStream& TXTR)
{
    // TODO: This function doesn't handle very small mipmaps correctly.
    // The format applies padding when the size of a mipmap is less than the block size for that format.
    // The decode needs to be adjusted to account for the padding and skip over it (since we don't have padding in OpenGL).

    // Get image data size, create output buffer
    uint32 ImageStart = TXTR.Tell();
    TXTR.Seek(0x0, SEEK_END);
    uint32 ImageSize = TXTR.Tell() - ImageStart;
    TXTR.Seek(ImageStart, SEEK_SET);

    mDataBufferSize = ImageSize * (gskOutputBpp[(int) mTexelFormat] / gskSourceBpp[(int) mTexelFormat]);
    if ((mHasPalettes) && (mPaletteFormat == EGXPaletteFormat::RGB5A3)) mDataBufferSize *= 2;
    mpDataBuffer = new uint8[mDataBufferSize];

    CMemoryOutStream Out(mpDataBuffer, mDataBufferSize, EEndian::SystemEndian);

    // Initializing more stuff before we start the mipmap loop
    uint32 MipW = mWidth, MipH = mHeight;
    uint32 MipOffset = 0;

    uint32 BWidth = gskBlockWidth[(int) mTexelFormat];
    uint32 BHeight = gskBlockHeight[(int) mTexelFormat];

    uint32 PixelStride = gskOutputPixelStride[(int) mTexelFormat];
    if (mHasPalettes && (mPaletteFormat == EGXPaletteFormat::RGB5A3))
        PixelStride = 4;

    // With CMPR, we're using a little trick.
    // CMPR stores pixels in 8x8 blocks, with four 4x4 subblocks.
    // An easy way to convert it is to pretend each block is 2x2 and each subblock is one pixel.
    // So to do that we need to calculate the "new" dimensions of the image, 1/4 the size of the original.
    if (mTexelFormat == ETexelFormat::GX_CMPR) {
        MipW /= 4;
        MipH /= 4;
    }

    // This value set to true if we hit the end of the file earlier than expected.
    // This is necessary due to a mistake Retro made in their cooker for I8 textures where very small mipmaps are cut off early, resulting in an out-of-bounds memory access.
    // This affects one texture that I know of - Echoes 3bb2c034.TXTR
    bool BreakEarly = false;

    for (uint32 iMip = 0; iMip < mNumMipMaps; iMip++)
    {
        if (MipW < BWidth) MipW = BWidth;
        if (MipH < BHeight) MipH = BHeight;

        for (uint32 iBlockY = 0; iBlockY < MipH; iBlockY += BHeight)
        {
            for (uint32 iBlockX = 0; iBlockX < MipW; iBlockX += BWidth)
            {
                for (uint32 iImgY = iBlockY; iImgY < iBlockY + BHeight; iImgY++)
                {
                    for (uint32 iImgX = iBlockX; iImgX < iBlockX + BWidth; iImgX++)
                    {
                        uint32 DstPos = ((iImgY * MipW) + iImgX) * PixelStride;
                        Out.Seek(MipOffset + DstPos, SEEK_SET);

                        if (mTexelFormat == ETexelFormat::GX_I4)          ReadPixelsI4(TXTR, Out);
                        else if (mTexelFormat == ETexelFormat::GX_I8)     ReadPixelI8(TXTR, Out);
                        else if (mTexelFormat == ETexelFormat::GX_IA4)    ReadPixelIA4(TXTR, Out);
                        else if (mTexelFormat == ETexelFormat::GX_IA8)    ReadPixelIA8(TXTR, Out);
                        else if (mTexelFormat == ETexelFormat::GX_C4)     ReadPixelsC4(TXTR, Out);
                        else if (mTexelFormat == ETexelFormat::GX_C8)     ReadPixelC8(TXTR, Out);
                        else if (mTexelFormat == ETexelFormat::GX_RGB565) ReadPixelRGB565(TXTR, Out);
                        else if (mTexelFormat == ETexelFormat::GX_RGB5A3) ReadPixelRGB5A3(TXTR, Out);
                        else if (mTexelFormat == ETexelFormat::GX_RGBA8)  ReadPixelRGBA8(TXTR, Out);
                        else if (mTexelFormat == ETexelFormat::GX_CMPR)   ReadSubBlockCMPR(TXTR, Out);

                        // I4 and C4 have 4bpp images, so I'm forced to read two pixels at a time.
                        if ((mTexelFormat == ETexelFormat::GX_I4) || (mTexelFormat == ETexelFormat::GX_C4)) iImgX++;

                        // Check if we're at the end of the file.
                        if (TXTR.EoF()) BreakEarly = true;
                    }
                    if (BreakEarly) break;
                }
                if (mTexelFormat == ETexelFormat::GX_RGBA8) TXTR.Seek(0x20, SEEK_CUR);
                if (BreakEarly) break;
            }
            if (BreakEarly) break;
        }

        uint32 MipSize = (uint32) (MipW * MipH * gskPixelsToBytes[(int) mTexelFormat]);
        if (mTexelFormat == ETexelFormat::GX_CMPR) MipSize *= 16; // Since we're pretending the image is 1/4 its actual size, we have to multiply the size by 16 to get the correct offset

        MipOffset += MipSize;
        MipW /= 2;
        MipH /= 2;

        if (BreakEarly) break;
    }
}

void CTextureDecoder::FullDecodeGXTexture(IInputStream& rTXTR)
{
    // Get image data size, create output buffer
    uint32 ImageStart = rTXTR.Tell();
    rTXTR.Seek(0x0, SEEK_END);
    uint32 ImageSize = rTXTR.Tell() - ImageStart;
    rTXTR.Seek(ImageStart, SEEK_SET);

    mDataBufferSize = ImageSize * (32 / gskSourceBpp[(int) mTexelFormat]);
    mpDataBuffer = new uint8[mDataBufferSize];

    CMemoryOutStream Out(mpDataBuffer, mDataBufferSize, EEndian::SystemEndian);

    // Initializing more stuff before we start the mipmap loop
    uint32 MipW = mWidth, MipH = mHeight;
    uint32 MipOffset = 0;

    uint32 BWidth = gskBlockWidth[(int) mTexelFormat];
    uint32 BHeight = gskBlockHeight[(int) mTexelFormat];

    // With CMPR, we're using a little trick.
    // CMPR stores pixels in 8x8 blocks, with four 4x4 subblocks.
    // An easy way to convert it is to pretend each block is 2x2 and each subblock is one pixel.
    // So to do that we need to calculate the "new" dimensions of the image, 1/4 the size of the original.
    if (mTexelFormat == ETexelFormat::GX_CMPR)
    {
        MipW /= 4;
        MipH /= 4;
    }

    for (uint32 iMip = 0; iMip < mNumMipMaps; iMip++)
    {
        for (uint32 iBlockY = 0; iBlockY < MipH; iBlockY += BHeight)
            for (uint32 iBlockX = 0; iBlockX < MipW; iBlockX += BWidth) {
                for (uint32 iImgY = iBlockY; iImgY < iBlockY + BHeight; iImgY++) {
                    for (uint32 iImgX = iBlockX; iImgX < iBlockX + BWidth; iImgX++)
                    {
                        uint32 DstPos = (mTexelFormat == ETexelFormat::GX_CMPR) ? ((iImgY * (MipW * 4)) + iImgX) * 16 : ((iImgY * MipW) + iImgX) * 4;
                        Out.Seek(MipOffset + DstPos, SEEK_SET);

                        // I4/C4/CMPR require reading more than one pixel at a time
                        if (mTexelFormat == ETexelFormat::GX_I4)
                        {
                            uint8 Byte = rTXTR.ReadByte();
                            Out.WriteLong( DecodePixelI4(Byte, 0).ToLongARGB() );
                            Out.WriteLong( DecodePixelI4(Byte, 1).ToLongARGB() );
                        }
                        else if (mTexelFormat == ETexelFormat::GX_C4)
                        {
                            uint8 Byte = rTXTR.ReadByte();
                            Out.WriteLong( DecodePixelC4(Byte, 0, mPaletteInput).ToLongARGB() );
                            Out.WriteLong( DecodePixelC4(Byte, 1, mPaletteInput).ToLongARGB() );
                        }
                        else if (mTexelFormat == ETexelFormat::GX_CMPR) DecodeSubBlockCMPR(rTXTR, Out, (uint16) (MipW * 4));

                        else
                        {
                            CColor Pixel;

                            if (mTexelFormat == ETexelFormat::GX_I8)          Pixel = DecodePixelI8(rTXTR.ReadByte());
                            else if (mTexelFormat == ETexelFormat::GX_IA4)    Pixel = DecodePixelIA4(rTXTR.ReadByte());
                            else if (mTexelFormat == ETexelFormat::GX_IA8)    Pixel = DecodePixelIA8(rTXTR.ReadShort());
                            else if (mTexelFormat == ETexelFormat::GX_C8)     Pixel = DecodePixelC8(rTXTR.ReadByte(), mPaletteInput);
                            else if (mTexelFormat == ETexelFormat::GX_RGB565) Pixel = DecodePixelRGB565(rTXTR.ReadShort());
                            else if (mTexelFormat == ETexelFormat::GX_RGB5A3) Pixel = DecodePixelRGB5A3(rTXTR.ReadShort());
                            else if (mTexelFormat == ETexelFormat::GX_RGBA8)  Pixel = CColor(rTXTR, true);

                            Out.WriteLong(Pixel.ToLongARGB());
                        }
                    }
                }
                if (mTexelFormat == ETexelFormat::GX_RGBA8) rTXTR.Seek(0x20, SEEK_CUR);
            }

        uint32 MipSize = MipW * MipH * 4;
        if (mTexelFormat == ETexelFormat::GX_CMPR) MipSize *= 16;

        MipOffset += MipSize;
        MipW /= 2;
        MipH /= 2;
        if (MipW < BWidth) MipW = BWidth;
        if (MipH < BHeight) MipH = BHeight;
    }
}

void CTextureDecoder::DecodeDDS(IInputStream& rDDS)
{
    // Get image data size, create output buffer
    uint32 ImageStart = rDDS.Tell();
    rDDS.Seek(0x0, SEEK_END);
    uint32 ImageSize = rDDS.Tell() - ImageStart;
    rDDS.Seek(ImageStart, SEEK_SET);

    mDataBufferSize = ImageSize;
    if (mDDSInfo.Format == SDDSInfo::DXT1) mDataBufferSize *= 8;
    else if (mDDSInfo.Format == SDDSInfo::RGBA) mDataBufferSize *= (32 / mDDSInfo.BitCount);
    else mDataBufferSize *= 4;
    mpDataBuffer = new uint8[mDataBufferSize];

    CMemoryOutStream Out(mpDataBuffer, mDataBufferSize, EEndian::SystemEndian);

    // Initializing more stuff before we start the mipmap loop
    uint32 MipW = mWidth, MipH = mHeight;
    uint32 MipOffset = 0;

    uint32 BPP;
    switch (mDDSInfo.Format)
    {
    case SDDSInfo::RGBA:
        BPP = mDDSInfo.BitCount;
        break;
    case SDDSInfo::DXT1:
        BPP = 4;
        break;
    case SDDSInfo::DXT2:
    case SDDSInfo::DXT3:
    case SDDSInfo::DXT4:
    case SDDSInfo::DXT5:
        BPP = 8;
        break;
    }

    // For DXT* decodes we can use the same trick as CMPR
    if ((mDDSInfo.Format != SDDSInfo::RGBA) && (mDDSInfo.Format != SDDSInfo::DXT1))
    {
        MipW /= 4;
        MipH /= 4;
    }

    for (uint32 iMip = 0; iMip < mNumMipMaps; iMip++)
    {
        // For DXT1 we can copy the image data as-is to load it
        if (mDDSInfo.Format == SDDSInfo::DXT1)
        {
            Out.Seek(MipOffset, SEEK_SET);
            uint32 MipSize = MipW * MipH / 2;
            std::vector<uint8> MipBuffer(MipSize);
            rDDS.ReadBytes(MipBuffer.data(), MipBuffer.size());
            Out.WriteBytes(MipBuffer.data(), MipBuffer.size());
            MipOffset += MipSize;

            MipW /= 2;
            MipH /= 2;
            if (MipW % 4) MipW += (4 - (MipW % 4));
            if (MipH % 4) MipH += (4 - (MipH % 4));
        }

        // Otherwise we do a full decode to RGBA8
        else
        {
            for (uint32 Y = 0; Y < MipH; Y++)
            {
                for (uint32 X = 0; X < MipW; X++)
                {
                    uint32 OutPos = MipOffset;

                    if (mDDSInfo.Format == SDDSInfo::RGBA)
                    {
                        OutPos += ((Y * MipW) + X) * 4;
                        Out.Seek(OutPos, SEEK_SET);

                        CColor Pixel = DecodeDDSPixel(rDDS);
                        Out.WriteLong(Pixel.ToLongARGB());
                    }

                    else
                    {
                        OutPos += ((Y * (MipW * 4)) + X) * 16;
                        Out.Seek(OutPos, SEEK_SET);

                        if (mDDSInfo.Format == SDDSInfo::DXT1)
                            DecodeBlockBC1(rDDS, Out, MipW * 4);
                        else if ((mDDSInfo.Format == SDDSInfo::DXT2) || (mDDSInfo.Format == SDDSInfo::DXT3))
                            DecodeBlockBC2(rDDS, Out, MipW * 4);
                        else if ((mDDSInfo.Format == SDDSInfo::DXT4) || (mDDSInfo.Format == SDDSInfo::DXT5))
                            DecodeBlockBC3(rDDS, Out, MipW * 4);
                    }
                }
            }

            uint32 MipSize = (mWidth * mHeight) * 4;
            if (mDDSInfo.Format != SDDSInfo::RGBA) MipSize *= 16;
            MipOffset += MipSize;

            MipW /= 2;
            MipH /= 2;
        }
    }

    if (mDDSInfo.Format == SDDSInfo::DXT1)
        mTexelFormat = ETexelFormat::DXT1;
    else
        mTexelFormat = ETexelFormat::GX_RGBA8;
}

// ************ READ PIXELS (PARTIAL DECODE) ************
void CTextureDecoder::ReadPixelsI4(IInputStream& rSrc, IOutputStream& rDst)
{
    uint8 Pixels = rSrc.ReadByte();
    rDst.WriteByte(Extend4to8(Pixels >> 4));
    rDst.WriteByte(Extend4to8(Pixels >> 4));
    rDst.WriteByte(Extend4to8(Pixels));
    rDst.WriteByte(Extend4to8(Pixels));
}

void CTextureDecoder::ReadPixelI8(IInputStream& rSrc, IOutputStream& rDst)
{
    uint8 Pixel = rSrc.ReadByte();
    rDst.WriteByte(Pixel);
    rDst.WriteByte(Pixel);
}

void CTextureDecoder::ReadPixelIA4(IInputStream& rSrc, IOutputStream& rDst)
{
    // this can be left as-is for DDS conversion, but opengl doesn't support two components in one byte...
    uint8 Byte = rSrc.ReadByte();
    uint8 Alpha = Extend4to8(Byte >> 4);
    uint8 Lum = Extend4to8(Byte);
    rDst.WriteShort((Lum << 8) | Alpha);
}

void CTextureDecoder::ReadPixelIA8(IInputStream& rSrc, IOutputStream& rDst)
{
    rDst.WriteShort(rSrc.ReadShort());
}

void CTextureDecoder::ReadPixelsC4(IInputStream& rSrc, IOutputStream& rDst)
{
    // This isn't how C4 works, but due to the way Retro packed font textures (which use C4)
    // this is the only way to get them to decode correctly for now.
    // Commented-out code is proper C4 decoding. Dedicated font texture-decoding function
    // is probably going to be necessary in the future.
    uint8 Byte = rSrc.ReadByte();
    uint8 Indices[2];
    Indices[0] = (Byte >> 4) & 0xF;
    Indices[1] = Byte & 0xF;

    for (uint32 iIdx = 0; iIdx < 2; iIdx++)
    {
        uint8 R, G, B, A;
        ((Indices[iIdx] >> 3) & 0x1) ? R = 0xFF : R = 0x0;
        ((Indices[iIdx] >> 2) & 0x1) ? G = 0xFF : G = 0x0;
        ((Indices[iIdx] >> 1) & 0x1) ? B = 0xFF : B = 0x0;
        ((Indices[iIdx] >> 0) & 0x1) ? A = 0xFF : A = 0x0;
        uint32 RGBA = (R << 24) | (G << 16) | (B << 8) | (A);
        rDst.WriteLong(RGBA);

      /*mPaletteInput.Seek(indices[i] * 2, SEEK_SET);

             if (mPaletteFormat == ePalette_IA8)    readPixelIA8(mPaletteInput, rDst);
        else if (mPaletteFormat == ePalette_RGB565) readPixelRGB565(mPaletteInput, rDst);
        else if (mPaletteFormat == ePalette_RGB5A3) readPixelRGB5A3(mPaletteInput, rDst);*/
    }
}

void CTextureDecoder::ReadPixelC8(IInputStream& rSrc, IOutputStream& rDst)
{
    // DKCR fonts use C8 :|
    uint8 Index = rSrc.ReadByte();

    /*u8 R, G, B, A;
    ((Index >> 3) & 0x1) ? R = 0xFF : R = 0x0;
    ((Index >> 2) & 0x1) ? G = 0xFF : G = 0x0;
    ((Index >> 1) & 0x1) ? B = 0xFF : B = 0x0;
    ((Index >> 0) & 0x1) ? A = 0xFF : A = 0x0;
    uint32 RGBA = (R << 24) | (G << 16) | (B << 8) | (A);
    dst.WriteLong(RGBA);*/

    mPaletteInput.Seek(Index * 2, SEEK_SET);

         if (mPaletteFormat == EGXPaletteFormat::IA8)    ReadPixelIA8(mPaletteInput, rDst);
    else if (mPaletteFormat == EGXPaletteFormat::RGB565) ReadPixelRGB565(mPaletteInput, rDst);
    else if (mPaletteFormat == EGXPaletteFormat::RGB5A3) ReadPixelRGB5A3(mPaletteInput, rDst);
}

void CTextureDecoder::ReadPixelRGB565(IInputStream& rSrc, IOutputStream& rDst)
{
    // RGB565 can be used as-is.
    rDst.WriteShort(rSrc.ReadShort());
}

void CTextureDecoder::ReadPixelRGB5A3(IInputStream& rSrc, IOutputStream& rDst)
{
    uint16 Pixel = rSrc.ReadShort();
    uint8 R, G, B, A;

    if (Pixel & 0x8000) // RGB5
    {
        B = Extend5to8(Pixel >> 10);
        G = Extend5to8(Pixel >>  5);
        R = Extend5to8(Pixel >>  0);
        A = 255;
    }

    else // RGB4A3
    {
        A = Extend3to8(Pixel >> 12);
        B = Extend4to8(Pixel >>  8);
        G = Extend4to8(Pixel >>  4);
        R = Extend4to8(Pixel >>  0);
    }

    uint32 Color = (A << 24) | (R << 16) | (G << 8) | B;
    rDst.WriteLong(Color);
}

void CTextureDecoder::ReadPixelRGBA8(IInputStream& rSrc, IOutputStream& rDst)
{
    uint16 AR = rSrc.ReadShort();
    rSrc.Seek(0x1E, SEEK_CUR);
    uint16 GB = rSrc.ReadShort();
    rSrc.Seek(-0x20, SEEK_CUR);
    uint32 Pixel = (AR << 16) | GB;
    rDst.WriteLong(Pixel);
}

void CTextureDecoder::ReadSubBlockCMPR(IInputStream& rSrc, IOutputStream& rDst)
{
    rDst.WriteShort(rSrc.ReadShort());
    rDst.WriteShort(rSrc.ReadShort());

    for (uint32 iByte = 0; iByte < 4; iByte++)
    {
        uint8 Byte = rSrc.ReadByte();
        Byte = ((Byte & 0x3) << 6) | ((Byte & 0xC) << 2) | ((Byte & 0x30) >> 2) | ((Byte & 0xC0) >> 6);
        rDst.WriteByte(Byte);
    }
}

// ************ DECODE PIXELS (FULL DECODE TO RGBA8) ************
CColor CTextureDecoder::DecodePixelI4(uint8 Byte, uint8 WhichPixel)
{
    if (WhichPixel == 1) Byte >>= 4;
    uint8 Pixel = Extend4to8(Byte);
    return CColor::Integral(Pixel, Pixel, Pixel);
}

CColor CTextureDecoder::DecodePixelI8(uint8 Byte)
{
    return CColor::Integral(Byte, Byte, Byte);
}

CColor CTextureDecoder::DecodePixelIA4(uint8 Byte)
{
    uint8 Alpha = Extend4to8(Byte >> 4);
    uint8 Lum = Extend4to8(Byte);
    return CColor::Integral(Lum, Lum, Lum, Alpha);
}

CColor CTextureDecoder::DecodePixelIA8(uint16 Short)
{
    uint8 Alpha = (Short >> 8) & 0xFF;
    uint8 Lum = Short & 0xFF;
    return CColor::Integral(Lum, Lum, Lum, Alpha);
}

CColor CTextureDecoder::DecodePixelC4(uint8 Byte, uint8 WhichPixel, IInputStream& rPaletteStream)
{
    if (WhichPixel == 1) Byte >>= 4;
    Byte &= 0xF;

    rPaletteStream.Seek(Byte * 2, SEEK_SET);
    if (mPaletteFormat == EGXPaletteFormat::IA8)         return DecodePixelIA8(rPaletteStream.ReadShort());
    else if (mPaletteFormat == EGXPaletteFormat::RGB565) return DecodePixelIA8(rPaletteStream.ReadShort());
    else if (mPaletteFormat == EGXPaletteFormat::RGB5A3) return DecodePixelIA8(rPaletteStream.ReadShort());
    else return CColor::skTransparentBlack;
}

CColor CTextureDecoder::DecodePixelC8(uint8 Byte, IInputStream& rPaletteStream)
{
    rPaletteStream.Seek(Byte * 2, SEEK_SET);
    if (mPaletteFormat == EGXPaletteFormat::IA8)         return DecodePixelIA8(rPaletteStream.ReadShort());
    else if (mPaletteFormat == EGXPaletteFormat::RGB565) return DecodePixelIA8(rPaletteStream.ReadShort());
    else if (mPaletteFormat == EGXPaletteFormat::RGB5A3) return DecodePixelIA8(rPaletteStream.ReadShort());
    else return CColor::skTransparentBlack;
}

CColor CTextureDecoder::DecodePixelRGB565(uint16 Short)
{
    uint8 B = Extend5to8( (uint8) (Short >> 11) );
    uint8 G = Extend6to8( (uint8) (Short >> 5) );
    uint8 R = Extend5to8( (uint8) (Short) );
    return CColor::Integral(R, G, B, 0xFF);
}

CColor CTextureDecoder::DecodePixelRGB5A3(uint16 Short)
{
    if (Short & 0x8000) // RGB5
    {
        uint8 B = Extend5to8( (uint8) (Short >> 10));
        uint8 G = Extend5to8( (uint8) (Short >> 5));
        uint8 R = Extend5to8( (uint8) (Short) );
        return CColor::Integral(R, G, B, 0xFF);
    }

    else // RGB4A3
    {
        uint8 A = Extend3to8( (uint8) (Short >> 12) );
        uint8 B = Extend4to8( (uint8) (Short >> 8) );
        uint8 G = Extend4to8( (uint8) (Short >> 4) );
        uint8 R = Extend4to8( (uint8) (Short) );
        return CColor::Integral(R, G, B, A);
    }
}

void CTextureDecoder::DecodeSubBlockCMPR(IInputStream& rSrc, IOutputStream& rDst, uint16 Width)
{
    CColor Palettes[4];
    uint16 PaletteA = rSrc.ReadShort();
    uint16 PaletteB = rSrc.ReadShort();
    Palettes[0] = DecodePixelRGB565(PaletteA);
    Palettes[1] = DecodePixelRGB565(PaletteB);

    if (PaletteA > PaletteB)
    {
        Palettes[2] = (Palettes[0] * 0.666666666f) + (Palettes[1] * 0.333333333f);
        Palettes[3] = (Palettes[0] * 0.333333333f) + (Palettes[1] * 0.666666666f);
    }
    else
    {
        Palettes[2] = (Palettes[0] * 0.5f) + (Palettes[1] * 0.5f);
        Palettes[3] = CColor::skTransparentBlack;
    }

    for (uint32 iBlockY = 0; iBlockY < 4; iBlockY++)
    {
        uint8 Byte = rSrc.ReadByte();

        for (uint32 iBlockX = 0; iBlockX < 4; iBlockX++)
        {
            uint8 Shift = (uint8) (6 - (iBlockX * 2));
            uint8 PaletteIndex = (Byte >> Shift) & 0x3;
            CColor Pixel = Palettes[PaletteIndex];
            rDst.WriteLong(Pixel.ToLongARGB());
        }

        rDst.Seek((Width - 4) * 4, SEEK_CUR);
    }
}

void CTextureDecoder::DecodeBlockBC1(IInputStream& rSrc, IOutputStream& rDst, uint32 Width)
{
    // Very similar to the CMPR subblock function, but unfortunately a slight
    // difference in the order the pixel indices are read requires a separate function
    CColor Palettes[4];
    uint16 PaletteA = rSrc.ReadShort();
    uint16 PaletteB = rSrc.ReadShort();
    Palettes[0] = DecodePixelRGB565(PaletteA);
    Palettes[1] = DecodePixelRGB565(PaletteB);

    if (PaletteA > PaletteB)
    {
        Palettes[2] = (Palettes[0] * 0.666666666f) + (Palettes[1] * 0.333333333f);
        Palettes[3] = (Palettes[0] * 0.333333333f) + (Palettes[1] * 0.666666666f);
    }
    else
    {
        Palettes[2] = (Palettes[0] * 0.5f) + (Palettes[1] * 0.5f);
        Palettes[3] = CColor::skTransparentBlack;
    }

    for (uint32 iBlockY = 0; iBlockY < 4; iBlockY++)
    {
        uint8 Byte = rSrc.ReadByte();

        for (uint32 iBlockX = 0; iBlockX < 4; iBlockX++)
        {
            uint8 Shift = (uint8) (iBlockX * 2);
            uint8 PaletteIndex = (Byte >> Shift) & 0x3;
            CColor Pixel = Palettes[PaletteIndex];
            rDst.WriteLong(Pixel.ToLongARGB());
        }

        rDst.Seek((Width - 4) * 4, SEEK_CUR);
    }
}

void CTextureDecoder::DecodeBlockBC2(IInputStream& rSrc, IOutputStream& rDst, uint32 Width)
{
    CColor CPalettes[4];
    uint16 PaletteA = rSrc.ReadShort();
    uint16 PaletteB = rSrc.ReadShort();
    CPalettes[0] = DecodePixelRGB565(PaletteA);
    CPalettes[1] = DecodePixelRGB565(PaletteB);

    if (PaletteA > PaletteB)
    {
        CPalettes[2] = (CPalettes[0] * 0.666666666f) + (CPalettes[1] * 0.333333333f);
        CPalettes[3] = (CPalettes[0] * 0.333333333f) + (CPalettes[1] * 0.666666666f);
    }
    else
    {
        CPalettes[2] = (CPalettes[0] * 0.5f) + (CPalettes[1] * 0.5f);
        CPalettes[3] = CColor::skTransparentBlack;
    }

    for (uint32 iBlockY = 0; iBlockY < 4; iBlockY++)
    {
        uint8 Byte = rSrc.ReadByte();

        for (uint32 iBlockX = 0; iBlockX < 4; iBlockX++)
        {
            uint8 Shift = (uint8) (iBlockX * 2);
            uint8 PaletteIndex = (Byte >> Shift) & 0x3;
            CColor Pixel = CPalettes[PaletteIndex];
            rDst.WriteLong(Pixel.ToLongARGB());
        }

        rDst.Seek((Width - 4) * 4, SEEK_CUR);
    }
}

void CTextureDecoder::DecodeBlockBC3(IInputStream& rSrc, IOutputStream& rDst, uint32 Width)
{
    CColor Palettes[4];
    uint16 PaletteA = rSrc.ReadShort();
    uint16 PaletteB = rSrc.ReadShort();
    Palettes[0] = DecodePixelRGB565(PaletteA);
    Palettes[1] = DecodePixelRGB565(PaletteB);

    if (PaletteA > PaletteB)
    {
        Palettes[2] = (Palettes[0] * 0.666666666f) + (Palettes[1] * 0.333333333f);
        Palettes[3] = (Palettes[0] * 0.333333333f) + (Palettes[1] * 0.666666666f);
    }
    else
    {
        Palettes[2] = (Palettes[0] * 0.5f) + (Palettes[1] * 0.5f);
        Palettes[3] = CColor::skTransparentBlack;
    }

    for (uint32 iBlockY = 0; iBlockY < 4; iBlockY++)
    {
        uint8 Byte = rSrc.ReadByte();

        for (uint32 iBlockX = 0; iBlockX < 4; iBlockX++)
        {
            uint8 Shift = (uint8) (iBlockX * 2);
            uint8 PaletteIndex = (Byte >> Shift) & 0x3;
            CColor Pixel = Palettes[PaletteIndex];
            rDst.WriteLong(Pixel.ToLongARGB());
        }

        rDst.Seek((Width - 4) * 4, SEEK_CUR);
    }
}

CColor CTextureDecoder::DecodeDDSPixel(IInputStream& /*rDDS*/)
{
    return CColor::skWhite;
}

// ************ UTILITY ************
uint8 CTextureDecoder::Extend3to8(uint8 In)
{
    In &= 0x7;
    return (In << 5) | (In << 2) | (In >> 1);
}

uint8 CTextureDecoder::Extend4to8(uint8 In)
{
    In &= 0xF;
    return (In << 4) | In;
}

uint8 CTextureDecoder::Extend5to8(uint8 In)
{
    In &= 0x1F;
    return (In << 3) | (In >> 2);
}

uint8 CTextureDecoder::Extend6to8(uint8 In)
{
    In &= 0x3F;
    return (In << 2) | (In >> 4);
}

uint32 CTextureDecoder::CalculateShiftForMask(uint32 BitMask)
{
    uint32 Shift = 32;

    while (BitMask)
    {
        BitMask <<= 1;
        Shift--;
    }
    return Shift;
}

uint32 CTextureDecoder::CalculateMaskBitCount(uint32 BitMask)
{
    uint32 Count = 0;

    while (BitMask)
    {
        if (BitMask & 0x1) Count++;
        BitMask >>= 1;
    }
    return Count;
}
