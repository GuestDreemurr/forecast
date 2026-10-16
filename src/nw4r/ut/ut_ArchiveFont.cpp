#include <nw4r/math.h>
#include <nw4r/ut.h>

#include <revolution/CX.h>
#include <revolution/OS.h>

#include <cstring>

namespace nw4r {
namespace ut {
namespace {

inline bool IsNullString(const char* pStr) {
    return *pStr == '\0';
}

inline bool IsBitOn(const u32* pBits, u32 index) {
    return (pBits[index / 32] << (index % 32)) & 0x80000000;
}

} // namespace

/******************************************************************************
 *
 * ArchiveFont
 *
 ******************************************************************************/
ArchiveFont::ArchiveFont() {}

ArchiveFont::~ArchiveFont() {}

u32 ArchiveFont::GetRequireBufferSize(const void* pBrfnt,
                                      const char* pGlyphGroups) {
    if (!IsValidResource(pBrfnt, 0x4000)) {
        return 0;
    }

    const BinaryFileHeader* pHeader =
        static_cast<const BinaryFileHeader*>(pBrfnt);
    const FontGlyphGroupsBlock* pGLGR =
        reinterpret_cast<const FontGlyphGroupsBlock*>(pHeader + 1);

    u32 numUseSheet = 0;

    u32 sizeSheetsOfs = RoundUp(sizeof(BinaryFileHeader) +
                                    offsetof(FontGlyphGroupsBlock, body) +
                                    offsetof(FontGlyphGroups, nameOffsets) +
                                    pGLGR->body.numSet * sizeof(u16),
                                4);
    u32 sizeCWDHOfs =
        RoundUp(sizeSheetsOfs + pGLGR->body.numSheet * sizeof(u32), 4);
    u32 sizeCMAPOfs =
        RoundUp(sizeCWDHOfs + pGLGR->body.numCWDH * sizeof(u32), 4);
    u32 useSheetsOfs =
        RoundUp(sizeCMAPOfs + pGLGR->body.numCMAP * sizeof(u32), 4);

    u32 sheetBytes = ((pGLGR->body.numSheet + 31) / 32) * sizeof(u32);
    u32 cwdhBytes = ((pGLGR->body.numCWDH + 31) / 32) * sizeof(u32);
    u32 cmapBytes = ((pGLGR->body.numCMAP + 31) / 32) * sizeof(u32);

    u32 useCWDHOfs =
        RoundUp(useSheetsOfs + sheetBytes * pGLGR->body.numSet, 4);
    u32 useCMAPOfs = RoundUp(useCWDHOfs + cwdhBytes * pGLGR->body.numSet, 4);

    const u32* pSizeCWDH =
        static_cast<const u32*>(AddOffsetToPtr(pHeader, sizeCWDHOfs));
    const u32* pSizeCMAP =
        static_cast<const u32*>(AddOffsetToPtr(pHeader, sizeCMAPOfs));

    u32 totalCWDH = 0;
    u32 totalCMAP = 0;

    const u32* pUseSheets =
        static_cast<const u32*>(AddOffsetToPtr(pHeader, useSheetsOfs));
    const u32* pUseCWDH =
        static_cast<const u32*>(AddOffsetToPtr(pHeader, useCWDHOfs));
    const u32* pUseCMAP =
        static_cast<const u32*>(AddOffsetToPtr(pHeader, useCMAPOfs));

    for (int base = 0; base < pGLGR->body.numSheet; base += 32) {
        const u32* pBits = pUseSheets + base / 32;
        u32 useBits = 0;

        for (int set = 0; set < pGLGR->body.numSet; set++) {
            const char* pSetName = static_cast<const char*>(
                AddOffsetToPtr(pHeader, pGLGR->body.nameOffsets[set]));

            if (IsNullString(pGlyphGroups) ||
                IncludeName(pGlyphGroups, pSetName)) {

                useBits |= pBits[set * sheetBytes / sizeof(u32)];
            }
        }

        numUseSheet += math::CntBit1(useBits);
    }

    for (int base = 0; base < pGLGR->body.numCWDH; base += 32) {
        const u32* pBits = pUseCWDH + base / 32;
        u32 useBits = 0;

        for (int set = 0; set < pGLGR->body.numSet; set++) {
            const char* pSetName = static_cast<const char*>(
                AddOffsetToPtr(pHeader, pGLGR->body.nameOffsets[set]));

            if (IsNullString(pGlyphGroups) ||
                IncludeName(pGlyphGroups, pSetName)) {

                useBits |= pBits[set * cwdhBytes / sizeof(u32)];
            }
        }

        for (int bit = 0; bit < 32; bit++) {
            if ((useBits << bit) & 0x80000000) {
                totalCWDH += pSizeCWDH[base + bit] - sizeof(BinaryBlockHeader);
            }
        }
    }

    for (int base = 0; base < pGLGR->body.numCMAP; base += 32) {
        const u32* pBits = pUseCMAP + base / 32;
        u32 useBits = 0;

        for (int set = 0; set < pGLGR->body.numSet; set++) {
            const char* pSetName = static_cast<const char*>(
                AddOffsetToPtr(pHeader, pGLGR->body.nameOffsets[set]));

            if (IsNullString(pGlyphGroups) ||
                IncludeName(pGlyphGroups, pSetName)) {

                useBits |= pBits[set * cmapBytes / sizeof(u32)];
            }
        }

        for (int bit = 0; bit < 32; bit++) {
            if ((useBits << bit) & 0x80000000) {
                totalCMAP += pSizeCMAP[base + bit] - sizeof(BinaryBlockHeader);
            }
        }
    }

    u32 sizeSheets = numUseSheet * pGLGR->body.sheetSize;
    u32 sizeCWDHCMAP = totalCWDH + totalCMAP;

    u32 sizeAdjust = RoundUp(pGLGR->body.numSheet * sizeof(u16), 4);
    u32 sizeWork = Max<u32>(sizeCWDHCMAP, sizeof(CXUncompContextHuffman));

    return RoundUp(sizeAdjust + sizeof(FontInformation) +
                       sizeof(FontTextureGlyph),
                   32) +
           (sizeWork + RoundUp(sizeSheets, 4));
}

ArchiveFontBase::ConstructResult
ArchiveFont::StreamingConstruct(ConstructContext* pContext,
                                const void* pStream, u32 streamSize) {
    if (mFontInfo != NULL) {
        return pContext->blockIndex < pContext->numBlocks ? CONSTRUCT_ERROR
                                                          : CONSTRUCT_FINISH;
    }

    ConstructResult result;
    CachedStreamReader& rReader = pContext->stream;
    CachedStreamReader* pReader = &rReader;

    result = CONSTRUCT_CONTINUE;
    rReader.Attach(pStream, streamSize);

    while (result == CONSTRUCT_CONTINUE) {
        switch (pContext->op) {
        case CONSTRUCT_OP_DISPATCH: {
            result = ConstructOpDispatch(pContext, pReader);
            break;
        }

        case CONSTRUCT_OP_ANALYZE_FILE_HEADER: {
            result = ConstructOpAnalyzeFileHeader(pContext, pReader);
            break;
        }

        case CONSTRUCT_OP_ANALYZE_GLGR: {
            result = ConstructOpAnalyzeGLGR(pContext, pReader);
            break;
        }

        case CONSTRUCT_OP_ANALYZE_FINF: {
            result = ConstructOpAnalyzeFINF(pContext, pReader);
            break;
        }

        case CONSTRUCT_OP_ANALYZE_CMAP: {
            result = ConstructOpAnalyzeCMAP(pContext, pReader);
            break;
        }

        case CONSTRUCT_OP_ANALYZE_CWDH: {
            result = ConstructOpAnalyzeCWDH(pContext, pReader);
            break;
        }

        case CONSTRUCT_OP_ANALYZE_TGLP: {
            result = ConstructOpAnalyzeTGLP(pContext, pReader);
            break;
        }

        case CONSTRUCT_OP_PREPARE_COPY_SHEET: {
            result = ConstructOpPrepareCopySheet(pContext, pReader);
            break;
        }

        case CONSTRUCT_OP_PREPARE_EXPAND_SHEET: {
            result = ConstructOpPrepareExpandSheet(pContext, pReader);
            break;
        }

        case CONSTRUCT_OP_COPY: {
            result = ConstructOpCopy(pContext, pReader);
            break;
        }

        case CONSTRUCT_OP_SKIP: {
            result = ConstructOpSkip(pContext, pReader);
            break;
        }

        case CONSTRUCT_OP_EXPAND: {
            result = ConstructOpExpand(pContext, pReader);
            break;
        }

        case CONSTRUCT_OP_FATAL_ERROR: {
            result = ConstructOpFatalError(pContext, pReader);
            break;
        }

        default: {
            pContext->op = CONSTRUCT_OP_FATAL_ERROR;
            result = CONSTRUCT_ERROR;
            break;
        }
        }
    }

    if (result == CONSTRUCT_FINISH && mFontInfo == NULL) {
        DCFlushRange(pContext->pBufferBegin,
                     pContext->pBufferEnd - pContext->pBufferBegin);

        SetResourceBuffer(pContext->pBufferBegin, pContext->pFINF,
                          pContext->pGlyphIndexAdjustArray);

        if (AdjustIndex(mFontInfo->alterCharIndex) == GLYPH_INDEX_NOT_FOUND) {
            mFontInfo->alterCharIndex = 0;
        }

        InitReaderFunc(GetEncoding());
    }

    return result;
}

bool ArchiveFont::Construct(void* pBuffer, u32 bufferSize, const void* pBrfnt,
                            const char* pGlyphGroups) {
    ConstructContext context;
    u32 fileSize = static_cast<const BinaryFileHeader*>(pBrfnt)->fileSize;

    context.stream.Init();

    context.pGlyphGroups = pGlyphGroups;
    context.pGlyphIndexAdjustArray = NULL;
    context.pBufferBegin = static_cast<u8*>(pBuffer);
    context.pBufferEnd = static_cast<u8*>(pBuffer) + bufferSize;
    context.pCurrent = static_cast<u8*>(pBuffer);
    context.pUncomp = static_cast<CXUncompContextHuffman*>(RoundDown(
        context.pBufferEnd - sizeof(CXUncompContextHuffman), 4));
    context.nextOp = CONSTRUCT_OP_NONE;
    context.opSize = 0;
    context.numBlocks = 1;
    context.blockIndex = 0;
    context.sheetIndex = 0;
    context.numSheets = 0;
    context.glyphsPerSheet = 0;
    context.streamOffset = 0;
    context.pFINF = NULL;
    context.pPrevCWDH = NULL;
    context.pPrevCMAP = NULL;
    context.op = CONSTRUCT_OP_ANALYZE_FILE_HEADER;

    return StreamingConstruct(&context, pBrfnt, fileSize) == CONSTRUCT_FINISH;
}

void* ArchiveFont::Destroy() {
    return RemoveResourceBuffer();
}

void ArchiveFont::GetGlyph(Glyph* pGlyph, u16 ch) const {
    GetGlyphFromIndex(pGlyph, GetGlyphIndex(ch));
}

void ArchiveFont::GetGlyphFromIndex(Glyph* pGlyph, u16 index) const {
    const FontTextureGlyph* pTexGlyph = mFontInfo->pGlyph;

    u16 adjusted = AdjustIndex(index);
    if (adjusted == GLYPH_INDEX_NOT_FOUND) {
        index = mFontInfo->alterCharIndex;
        adjusted = AdjustIndex(index);
    }

    u32 cellsPerSheet = pTexGlyph->sheetRow * pTexGlyph->sheetLine;
    u32 sheetNo = adjusted / cellsPerSheet;
    u32 cellNo = adjusted % cellsPerSheet;
    u32 cellY = cellNo / pTexGlyph->sheetRow;
    u32 cellX = cellNo % pTexGlyph->sheetRow;

    u32 cellW = pTexGlyph->cellWidth + 1;
    u32 cellH = pTexGlyph->cellHeight + 1;

    u32 pixelX = cellX * cellW;
    u32 pixelY = cellY * cellH;

    pGlyph->pTexture = pTexGlyph->sheetImage + sheetNo * pTexGlyph->sheetSize;

    pGlyph->widths = GetCharWidthsFromIndex(index);
    pGlyph->height = pTexGlyph->cellHeight;
    pGlyph->texFormat = static_cast<GXTexFmt>(pTexGlyph->sheetFormat);
    pGlyph->texWidth = pTexGlyph->sheetWidth;
    pGlyph->texHeight = pTexGlyph->sheetHeight;
    pGlyph->cellX = pixelX + 1;
    pGlyph->cellY = pixelY + 1;
}

} // namespace ut
} // namespace nw4r
