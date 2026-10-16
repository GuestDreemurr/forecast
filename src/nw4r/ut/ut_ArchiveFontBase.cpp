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

const char ArchiveFontBase::LOAD_GLYPH_ALL[1] = "";

/******************************************************************************
 *
 * ArchiveFontBase
 *
 ******************************************************************************/
ArchiveFontBase::ArchiveFontBase() : mGlyphIndexAdjustArray(NULL) {}

ArchiveFontBase::~ArchiveFontBase() {}

CharWidths ArchiveFontBase::GetCharWidths(u16 ch) const {
    u16 index = GetGlyphIndex(ch);

    if (AdjustIndex(index) == GLYPH_INDEX_NOT_FOUND) {
        index = mFontInfo->alterCharIndex;
    }

    return GetCharWidthsFromIndex(index);
}

void ArchiveFontBase::SetResourceBuffer(void* pBuffer, FontInformation* pInfo,
                                        u16* pAdjustTable) {
    ResFontBase::SetResourceBuffer(pBuffer, pInfo);
    mGlyphIndexAdjustArray = pAdjustTable;
}

void* ArchiveFontBase::RemoveResourceBuffer() {
    mGlyphIndexAdjustArray = NULL;
    return ResFontBase::RemoveResourceBuffer();
}

u16 ArchiveFontBase::AdjustIndex(u16 index) const {
    const FontTextureGlyph* pGlyph = mFontInfo->pGlyph;
    int glyphsPerSheet = pGlyph->sheetRow * pGlyph->sheetLine;

    u16 ofs = mGlyphIndexAdjustArray[index / glyphsPerSheet];
    if (ofs == 0xFFFF) {
        return GLYPH_INDEX_NOT_FOUND;
    }

    return index - ofs;
}

bool ArchiveFontBase::IncludeName(const char* pNameList, const char* pName) {
    u32 nameLen = std::strlen(pName);
    const char* pFound = pNameList - 1;

    while (true) {
        pFound = std::strstr(pFound + 1, pName);
        if (pFound == NULL) {
            return false;
        }

        if (pFound != pNameList) {
            const char* pPrev;
            for (pPrev = pFound - 1; pPrev > pNameList && *pPrev == ' ';
                 pPrev--) {
                ;
            }

            if (*pPrev != ',') {
                continue;
            }
        }

        const char* pComma = std::strchr(pFound, ',');
        u32 len = pComma != NULL ? pComma - pFound : std::strlen(pFound);

        const char* pNext = pFound + nameLen;
        const char* pEnd = pFound + len;

        for (; pNext < pEnd && *pNext == ' '; pNext++) {
            ;
        }

        if (pNext == pEnd) {
            return true;
        }
    }
}

bool ArchiveFontBase::IsValidResource(const void* pBrfnt, u32 size) {
    const BinaryFileHeader* pHeader =
        static_cast<const BinaryFileHeader*>(pBrfnt);

    if (!IsValidBinaryFile(pHeader, 'RFNA', NW4R_VERSION(1, 4), 2)) {
        return false;
    }

    const FontGlyphGroupsBlock* pGLGR =
        reinterpret_cast<const FontGlyphGroupsBlock*>(pHeader + 1);

    if (pGLGR->header.kind != 'GLGR') {
        return false;
    }

    return pGLGR->header.size + sizeof(BinaryFileHeader) <= size;
}

ArchiveFontBase::ConstructResult
ArchiveFontBase::ConstructOpDispatch(ConstructContext* pContext,
                                     CachedStreamReader* pStream) {
    if (pContext->blockIndex >= pContext->numBlocks) {
        return CONSTRUCT_FINISH;
    }

    if (pStream->GetRemain() < sizeof(BinaryBlockHeader)) {
        pContext->streamOffset += pStream->GetOffset();
        return pStream->RequestData(pContext, sizeof(BinaryBlockHeader))
                   ? CONSTRUCT_MORE_DATA
                   : CONSTRUCT_ERROR;
    }

    pStream->CopyTo(&pContext->header, sizeof(BinaryBlockHeader));

    switch (pContext->header.kind) {
    case 'GLGR': {
        pContext->op = CONSTRUCT_OP_ANALYZE_GLGR;
        break;
    }

    case 'FINF': {
        pContext->op = CONSTRUCT_OP_ANALYZE_FINF;
        break;
    }

    case 'CMAP': {
        pContext->op = CONSTRUCT_OP_ANALYZE_CMAP;
        break;
    }

    case 'CWDH': {
        pContext->op = CONSTRUCT_OP_ANALYZE_CWDH;
        break;
    }

    case 'TGLP': {
        pContext->op = CONSTRUCT_OP_ANALYZE_TGLP;
        break;
    }

    default: {
        pContext->op = CONSTRUCT_OP_FATAL_ERROR;
        return CONSTRUCT_ERROR;
    }
    }

    pContext->blockIndex++;
    pContext->Align(4);

    return CONSTRUCT_CONTINUE;
}

ArchiveFontBase::ConstructResult
ArchiveFontBase::ConstructOpAnalyzeFileHeader(ConstructContext* pContext,
                                              CachedStreamReader* pStream) {
    if (pStream->GetRemain() < sizeof(BinaryFileHeader)) {
        pContext->streamOffset += pStream->GetOffset();
        return pStream->RequestData(pContext, sizeof(BinaryFileHeader))
                   ? CONSTRUCT_MORE_DATA
                   : CONSTRUCT_ERROR;
    }

    if (pContext->GetRemain() < sizeof(BinaryFileHeader)) {
        return CONSTRUCT_ERROR;
    }

    pStream->CopyTo(pContext->pCurrent, sizeof(BinaryFileHeader));
    pContext->op = CONSTRUCT_OP_DISPATCH;

    return CONSTRUCT_CONTINUE;
}

ArchiveFontBase::ConstructResult
ArchiveFontBase::ConstructOpAnalyzeGLGR(ConstructContext* pContext,
                                        CachedStreamReader* pStream) {
    BinaryFileHeader* pHeader =
        reinterpret_cast<BinaryFileHeader*>(pContext->pCurrent);
    u32 bodySize = pContext->header.size - sizeof(BinaryBlockHeader);

    if (pHeader->signature != 'RFNA') {
        return CONSTRUCT_ERROR;
    }

    if (pStream->GetRemain() < bodySize) {
        pContext->streamOffset += pStream->GetOffset();
        return pStream->RequestData(pContext, bodySize) ? CONSTRUCT_MORE_DATA
                                                        : CONSTRUCT_ERROR;
    }

    if (pContext->GetRemain() < bodySize) {
        return CONSTRUCT_ERROR;
    }

    u8* pBlock = reinterpret_cast<u8*>(pHeader + 1);
    u32 totalSize = sizeof(BinaryFileHeader) + pContext->header.size;
    u8* pBlockEnd = pBlock + pContext->header.size;

    std::memcpy(pBlock, &pContext->header, sizeof(BinaryBlockHeader));
    pStream->CopyTo(pBlock + sizeof(BinaryBlockHeader), bodySize);

    if (!IsValidResource(pHeader, totalSize)) {
        return CONSTRUCT_ERROR;
    }

    const FontGlyphGroupsBlock* pGLGR =
        reinterpret_cast<FontGlyphGroupsBlock*>(pHeader + 1);

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
    const u32* pUseSheets =
        static_cast<const u32*>(AddOffsetToPtr(pHeader, useSheetsOfs));

    u16 numSheet = pGLGR->body.numSheet;
    u16 glyphsPerSheet = pGLGR->body.glyphsPerSheet;
    u16 numBlocks = pHeader->dataBlocks;

    u32 adjustSize = RoundUp(numSheet * sizeof(u16), 4);

    u16* pAdjust = static_cast<u16*>(RoundDown(
        reinterpret_cast<u8*>(pHeader) + pContext->GetRemain() - adjustSize,
        2));

    if (pContext->GetRemain() <
        adjustSize + (pBlockEnd - reinterpret_cast<u8*>(pHeader))) {
        return CONSTRUCT_ERROR;
    }

    for (int i = 0; i < pGLGR->body.numSheet; i++) {
        pAdjust[i] = 0;
    }

    for (int set = 0; set < pGLGR->body.numSet; set++) {
        const char* pSetName = static_cast<const char*>(
            AddOffsetToPtr(pHeader, pGLGR->body.nameOffsets[set]));

        if (IsNullString(pContext->pGlyphGroups) ||
            IncludeName(pContext->pGlyphGroups, pSetName)) {

            for (int sheet = 0; sheet < pGLGR->body.numSheet; sheet++) {
                if (IsBitOn(pUseSheets, sheet + set * sheetBytes * 8)) {
                    pAdjust[sheet] = 1;
                }
            }
        }
    }

    u16 adjust = 0;
    for (int i = 0; i < pGLGR->body.numSheet; i++) {
        if (pAdjust[i] == 1) {
            pAdjust[i] = adjust;
        } else {
            pAdjust[i] = 0xFFFF;
            adjust += pGLGR->body.glyphsPerSheet;
        }
    }

    u16* pDst = reinterpret_cast<u16*>(pContext->pCurrent);
    pContext->Advance(adjustSize);
    u16* pAdjustEnd = pAdjust + numSheet;
    std::memmove(pDst, pAdjust, (pAdjustEnd - pAdjust) * sizeof(u16));

    pContext->pGlyphIndexAdjustArray = pDst;
    pContext->numSheets = numSheet;
    pContext->glyphsPerSheet = glyphsPerSheet;
    pContext->numBlocks = numBlocks;
    pContext->op = CONSTRUCT_OP_DISPATCH;

    return CONSTRUCT_CONTINUE;
}

ArchiveFontBase::ConstructResult
ArchiveFontBase::ConstructOpAnalyzeFINF(ConstructContext* pContext,
                                        CachedStreamReader* /* pStream */) {
    u32 size = pContext->header.size - sizeof(BinaryBlockHeader);

    if (pContext->GetRemain() < size) {
        return CONSTRUCT_ERROR;
    }

    pContext->pFINF = reinterpret_cast<FontInformation*>(pContext->pCurrent);
    pContext->SetupTask(CONSTRUCT_OP_COPY, size, CONSTRUCT_OP_DISPATCH);

    return CONSTRUCT_CONTINUE;
}

ArchiveFontBase::ConstructResult
ArchiveFontBase::ConstructOpAnalyzeCMAP(ConstructContext* pContext,
                                        CachedStreamReader* /* pStream */) {
    FontCodeMap* pMap = reinterpret_cast<FontCodeMap*>(pContext->pCurrent);

    if (pContext->pPrevCMAP != NULL) {
        pContext->pPrevCMAP->pNext = pMap;
    } else {
        pContext->pFINF->pMap = pMap;
    }

    pContext->pPrevCMAP = pMap;

    u32 size = pContext->header.size - sizeof(BinaryBlockHeader);

    if (pContext->GetRemain() < size) {
        return CONSTRUCT_ERROR;
    }

    pContext->SetupTask(CONSTRUCT_OP_COPY, size, CONSTRUCT_OP_DISPATCH);

    return CONSTRUCT_CONTINUE;
}

ArchiveFontBase::ConstructResult
ArchiveFontBase::ConstructOpAnalyzeCWDH(ConstructContext* pContext,
                                        CachedStreamReader* /* pStream */) {
    FontWidth* pWidth = reinterpret_cast<FontWidth*>(pContext->pCurrent);

    if (pContext->pPrevCWDH != NULL) {
        pContext->pPrevCWDH->pNext = pWidth;
    } else {
        pContext->pFINF->pWidth = pWidth;
    }

    pContext->pPrevCWDH = pWidth;

    u32 size = pContext->header.size - sizeof(BinaryBlockHeader);

    if (pContext->GetRemain() < size) {
        return CONSTRUCT_ERROR;
    }

    pContext->SetupTask(CONSTRUCT_OP_COPY, size, CONSTRUCT_OP_DISPATCH);

    return CONSTRUCT_CONTINUE;
}

ArchiveFontBase::ConstructResult
ArchiveFontBase::ConstructOpAnalyzeTGLP(ConstructContext* pContext,
                                        CachedStreamReader* pStream) {
    if (pStream->GetRemain() < sizeof(FontTextureGlyph)) {
        pContext->streamOffset += pStream->GetOffset();
        return pStream->RequestData(pContext, sizeof(FontTextureGlyph))
                   ? CONSTRUCT_MORE_DATA
                   : CONSTRUCT_ERROR;
    }

    if (pContext->GetRemain() < sizeof(FontTextureGlyph)) {
        return CONSTRUCT_ERROR;
    }

    pContext->pFINF->pGlyph =
        reinterpret_cast<FontTextureGlyph*>(pContext->pCurrent);

    pContext->CopyFrom(pStream, sizeof(FontTextureGlyph));

    bool compressed = (pContext->pFINF->pGlyph->sheetFormat & 0x8000) != 0;
    pContext->pFINF->pGlyph->sheetFormat &= 0x7FFF;

    u16 numSheet = 0;
    for (int i = 0; i < pContext->numSheets; i++) {
        if (pContext->pGlyphIndexAdjustArray[i] != 0xFFFF) {
            numSheet++;
        }
    }

    pContext->pFINF->pGlyph->sheetNum = numSheet;

    u32 sheetOffset = reinterpret_cast<u32>(pContext->pFINF->pGlyph->sheetImage);

    pContext->Align(32);
    pContext->pFINF->pGlyph->sheetImage = pContext->pCurrent;

    u32 nextOp = compressed ? CONSTRUCT_OP_PREPARE_EXPAND_SHEET
                            : CONSTRUCT_OP_PREPARE_COPY_SHEET;

    pContext->SetupTask(CONSTRUCT_OP_SKIP,
                        sheetOffset -
                            (pContext->streamOffset + pStream->GetOffset()),
                        nextOp);

    return CONSTRUCT_CONTINUE;
}

ArchiveFontBase::ConstructResult
ArchiveFontBase::ConstructOpPrepareCopySheet(
    ConstructContext* pContext, CachedStreamReader* /* pStream */) {

    if (pContext->sheetIndex >= pContext->numSheets) {
        pContext->op = CONSTRUCT_OP_DISPATCH;
        return CONSTRUCT_CONTINUE;
    }

    u32 sheetSize = pContext->pFINF->pGlyph->sheetSize;

    if (pContext->pGlyphIndexAdjustArray[pContext->sheetIndex] != 0xFFFF) {
        if (pContext->GetRemain() < sheetSize) {
            return CONSTRUCT_ERROR;
        }

        pContext->SetupTask(CONSTRUCT_OP_COPY, sheetSize,
                            CONSTRUCT_OP_PREPARE_COPY_SHEET);
    } else {
        pContext->SetupTask(CONSTRUCT_OP_SKIP, sheetSize,
                            CONSTRUCT_OP_PREPARE_COPY_SHEET);
    }

    pContext->sheetIndex++;

    return CONSTRUCT_CONTINUE;
}

ArchiveFontBase::ConstructResult
ArchiveFontBase::ConstructOpPrepareExpandSheet(ConstructContext* pContext,
                                               CachedStreamReader* pStream) {
    if (pContext->sheetIndex >= pContext->numSheets) {
        pContext->op = CONSTRUCT_OP_DISPATCH;
        return CONSTRUCT_CONTINUE;
    }

    if (pStream->GetRemain() < sizeof(u32) * 2) {
        pContext->streamOffset += pStream->GetOffset();
        return pStream->RequestData(pContext, sizeof(u32) * 2)
                   ? CONSTRUCT_MORE_DATA
                   : CONSTRUCT_ERROR;
    }

    u32 compSizeBuf;
    u32 compHeader;

    pStream->CopyTo(&compSizeBuf, sizeof(u32));
    std::memcpy(&compHeader, pStream->mStreamCur, sizeof(u32));

    u32 compSize = compSizeBuf;
    u32 uncompSize = CXGetUncompressedSize(&compHeader);

    if (pContext->pGlyphIndexAdjustArray[pContext->sheetIndex] != 0xFFFF) {
        if (pContext->GetRemain() < compSize + sizeof(CXUncompContextHuffman)) {
            return CONSTRUCT_ERROR;
        }

        CXInitUncompContextHuffman(pContext->pUncomp, pContext->pCurrent);
        pContext->SetupTask(CONSTRUCT_OP_EXPAND, compSize,
                            CONSTRUCT_OP_PREPARE_EXPAND_SHEET);
        pContext->Advance(uncompSize);
    } else {
        pContext->SetupTask(CONSTRUCT_OP_SKIP, compSize,
                            CONSTRUCT_OP_PREPARE_EXPAND_SHEET);
    }

    pContext->sheetIndex++;

    return CONSTRUCT_CONTINUE;
}

ArchiveFontBase::ConstructResult
ArchiveFontBase::ConstructOpCopy(ConstructContext* pContext,
                                 CachedStreamReader* pStream) {
    u32 copySize = pStream->mStreamEnd - pStream->mStreamCur;
    if (copySize > pContext->opSize) {
        copySize = pContext->opSize;
    }

    std::memcpy(pContext->pCurrent, pStream->mStreamCur, copySize);
    pStream->mStreamCur += copySize;
    pContext->Advance(copySize);

    if (pContext->ReduceTaskSize(copySize) == 0) {
        pContext->op = pContext->nextOp;
    } else {
        pContext->streamOffset += pStream->GetOffset();
        return pStream->RequestData(pContext, pContext->opSize)
                   ? CONSTRUCT_MORE_DATA
                   : CONSTRUCT_ERROR;
    }

    return CONSTRUCT_CONTINUE;
}

ArchiveFontBase::ConstructResult
ArchiveFontBase::ConstructOpSkip(ConstructContext* pContext,
                                 CachedStreamReader* pStream) {
    u32 skipSize = Min(pContext->opSize, pStream->GetRemain());

    pStream->Skip(skipSize);

    if (pContext->ReduceTaskSize(skipSize) == 0) {
        pContext->op = pContext->nextOp;
    } else {
        pContext->streamOffset += pStream->GetOffset();
        return pStream->RequestData(pContext, pContext->opSize)
                   ? CONSTRUCT_MORE_DATA
                   : CONSTRUCT_ERROR;
    }

    return CONSTRUCT_CONTINUE;
}

ArchiveFontBase::ConstructResult
ArchiveFontBase::ConstructOpExpand(ConstructContext* pContext,
                                   CachedStreamReader* pStream) {
    u32 expandSize = Min(pContext->opSize, pStream->GetRemain());

    const u8* pSrc = pStream->mStreamCur;
    pStream->mStreamCur += expandSize;
    CXReadUncompHuffman(pContext->pUncomp, pSrc, expandSize);

    if (pContext->ReduceTaskSize(expandSize) == 0) {
        pContext->op = pContext->nextOp;
    } else {
        pContext->streamOffset += pStream->GetOffset();
        return pStream->RequestData(pContext, pContext->opSize)
                   ? CONSTRUCT_MORE_DATA
                   : CONSTRUCT_ERROR;
    }

    return CONSTRUCT_CONTINUE;
}

ArchiveFontBase::ConstructResult
ArchiveFontBase::ConstructOpFatalError(ConstructContext* pContext,
                                       CachedStreamReader* /* pStream */) {
    pContext->op = CONSTRUCT_OP_FATAL_ERROR;
    return CONSTRUCT_ERROR;
}

/******************************************************************************
 *
 * CachedStreamReader
 *
 ******************************************************************************/
void ArchiveFontBase::CachedStreamReader::Init() {
    mStreamHead = NULL;
    mStreamCur = NULL;
    mStreamEnd = NULL;
    mTempHead = NULL;
    mTempCur = NULL;
    mTempEnd = NULL;
    mRequireSize = 0;
}

void ArchiveFontBase::CachedStreamReader::Attach(const void* pStream,
                                                 u32 size) {
    mStreamHead = static_cast<const u8*>(pStream);
    mStreamCur = static_cast<const u8*>(pStream);
    mStreamEnd = static_cast<const u8*>(pStream) + size;
}

bool ArchiveFontBase::CachedStreamReader::RequestData(
    ConstructContext* pContext, u32 size) {

    u32 remain = GetRemain();

    if (remain == 0) {
        mTempHead = NULL;
        mTempCur = NULL;
        mTempEnd = NULL;
        mRequireSize = 0;
    } else {
        if (pContext->GetRemain() < size * 2) {
            return false;
        }

        u8* pTemp = pContext->pCurrent + size;
        MoveTo(pTemp, remain);

        mTempHead = pTemp;
        mTempCur = pTemp;
        mTempEnd = pTemp + remain;
        mRequireSize = size;
    }

    return true;
}

} // namespace ut
} // namespace nw4r
