#ifndef NW4R_UT_ARCHIVE_FONT_H
#define NW4R_UT_ARCHIVE_FONT_H
#include <nw4r/types_nw4r.h>

#include <nw4r/ut/ut_ResFontBase.h>
#include <nw4r/ut/ut_algorithm.h>
#include <nw4r/ut/ut_binaryFileFormat.h>

#include <revolution/CX.h>

#include <cstring>

namespace nw4r {
namespace ut {

/******************************************************************************
 *
 * Glyph group block (GLGR)
 *
 ******************************************************************************/
struct FontGlyphGroups {
    u32 sheetSize;      // at 0x0
    u16 glyphsPerSheet; // at 0x4
    u16 numSet;         // at 0x6
    u16 numSheet;       // at 0x8
    u16 numCWDH;        // at 0xA
    u16 numCMAP;        // at 0xC
    u16 nameOffsets[];  // at 0xE
};

struct FontGlyphGroupsBlock {
    BinaryBlockHeader header; // at 0x0
    FontGlyphGroups body;     // at 0x8
};

/******************************************************************************
 *
 * ArchiveFontBase
 *
 ******************************************************************************/
class ArchiveFontBase : public detail::ResFontBase {
public:
    static const char LOAD_GLYPH_ALL[1];

    virtual ~ArchiveFontBase(); // at 0x8

    virtual CharWidths GetCharWidths(u16 ch) const; // at 0x4C

protected:
    enum ConstructResult {
        CONSTRUCT_MORE_DATA,
        CONSTRUCT_FINISH,
        CONSTRUCT_ERROR,
        CONSTRUCT_CONTINUE
    };

    enum ConstructOp {
        CONSTRUCT_OP_DISPATCH,
        CONSTRUCT_OP_ANALYZE_FILE_HEADER,
        CONSTRUCT_OP_ANALYZE_GLGR,
        CONSTRUCT_OP_ANALYZE_FINF,
        CONSTRUCT_OP_ANALYZE_CMAP,
        CONSTRUCT_OP_ANALYZE_CWDH,
        CONSTRUCT_OP_ANALYZE_TGLP,
        CONSTRUCT_OP_PREPARE_COPY_SHEET,
        CONSTRUCT_OP_PREPARE_EXPAND_SHEET,
        CONSTRUCT_OP_COPY,
        CONSTRUCT_OP_SKIP,
        CONSTRUCT_OP_EXPAND,
        CONSTRUCT_OP_FATAL_ERROR,
        CONSTRUCT_OP_MAX,
        CONSTRUCT_OP_NONE
    };

    struct ConstructContext;

    // Reads from the input stream, keeping a copy of data that did not fit
    class CachedStreamReader {
    public:
        void Init();
        void Attach(const void* pStream, u32 size);
        bool RequestData(ConstructContext* pContext, u32 size);

        u32 GetRemain() const {
            return (mStreamEnd - mStreamCur) + (mTempEnd - mTempCur);
        }

        u32 GetOffset() const {
            return (mStreamCur - mStreamHead) + (mTempCur - mTempHead);
        }

        void CopyTo(void* pDst, u32 size) {
            u32 tempRemain;
            u8* pDstU8 = static_cast<u8*>(pDst);
            tempRemain = mTempEnd - mTempCur;

            if (tempRemain >= size) {
                std::memcpy(pDstU8, mTempCur, size);
                mTempCur += size;
            } else {
                u32 streamSize = size - tempRemain;
                std::memcpy(pDstU8, mTempCur, tempRemain);
                std::memcpy(pDstU8 + tempRemain, mStreamCur, streamSize);
                mTempCur = mTempEnd;
                mStreamCur += streamSize;
            }
        }

        void MoveTo(void* pDst, u32 size) {
            u32 tempRemain = mTempEnd - mTempCur;

            if (tempRemain >= size) {
                std::memmove(pDst, mTempCur, size);
                mTempCur += size;
            } else {
                u32 streamSize = size - tempRemain;
                std::memmove(pDst, mTempCur, tempRemain);
                std::memmove(static_cast<u8*>(pDst) + tempRemain, mStreamCur,
                             streamSize);
                mTempCur = mTempEnd;
                mStreamCur += streamSize;
            }
        }

        void Skip(u32 size) {
            u32 tempRemain = mTempEnd - mTempCur;

            if (tempRemain > size) {
                mTempCur += size;
            } else {
                u32 streamSize = size - tempRemain;
                mTempCur = mTempEnd;
                mStreamCur += streamSize;
            }
        }

    public:
        const u8* mStreamHead; // at 0x0
        const u8* mStreamCur;  // at 0x4
        const u8* mStreamEnd;  // at 0x8
        const u8* mTempHead;   // at 0xC
        const u8* mTempCur;    // at 0x10
        const u8* mTempEnd;    // at 0x14
        u32 mRequireSize;      // at 0x18
    };

    struct ConstructContext {
        u32 GetRemain() const {
            return pBufferEnd - pCurrent;
        }

        void Advance(u32 size) {
            pCurrent += size;
        }

        void CopyFrom(CachedStreamReader* pStream, u32 size) {
            pStream->CopyTo(pCurrent, size);
            Advance(size);
        }

        void Align(u32 alignment) {
            pCurrent = static_cast<u8*>(RoundUp(pCurrent, alignment));
        }

        void SetupTask(u32 op, u32 size, u32 nextOp) {
            opSize = size;
            this->op = op;
            this->nextOp = nextOp;
        }

        u32 ReduceTaskSize(u32 size) {
            if (size > opSize) {
                size = opSize;
            }

            opSize -= size;
            return opSize;
        }

        FontInformation* pFINF;          // at 0x0
        FontWidth* pPrevCWDH;            // at 0x4
        FontCodeMap* pPrevCMAP;          // at 0x8
        u32 op;                          // at 0xC
        BinaryBlockHeader header;        // at 0x10
        u32 streamOffset;                // at 0x18
        CachedStreamReader stream;       // at 0x1C
        CXUncompContextHuffman* pUncomp; // at 0x38
        const char* pGlyphGroups;        // at 0x3C
        u16* pGlyphIndexAdjustArray;     // at 0x40
        u8* pBufferBegin;                // at 0x44
        u8* pBufferEnd;                  // at 0x48
        u8* pCurrent;                    // at 0x4C
        u32 nextOp;                      // at 0x50
        u32 opSize;                      // at 0x54
        u32 numBlocks;                   // at 0x58
        u32 blockIndex;                  // at 0x5C
        u16 sheetIndex;                  // at 0x60
        u16 numSheets;                   // at 0x62
        u16 glyphsPerSheet;              // at 0x64
    };

protected:
    ArchiveFontBase();

    void SetResourceBuffer(void* pBuffer, FontInformation* pInfo,
                           u16* pAdjustTable);
    void* RemoveResourceBuffer();

    u16 AdjustIndex(u16 index) const;

    static bool IncludeName(const char* pNameList, const char* pName);
    static bool IsValidResource(const void* pBrfnt, u32 size);

    static ConstructResult ConstructOpDispatch(ConstructContext* pContext,
                                               CachedStreamReader* pStream);
    static ConstructResult
    ConstructOpAnalyzeFileHeader(ConstructContext* pContext,
                                 CachedStreamReader* pStream);
    static ConstructResult ConstructOpAnalyzeGLGR(ConstructContext* pContext,
                                                  CachedStreamReader* pStream);
    static ConstructResult ConstructOpAnalyzeFINF(ConstructContext* pContext,
                                                  CachedStreamReader* pStream);
    static ConstructResult ConstructOpAnalyzeCMAP(ConstructContext* pContext,
                                                  CachedStreamReader* pStream);
    static ConstructResult ConstructOpAnalyzeCWDH(ConstructContext* pContext,
                                                  CachedStreamReader* pStream);
    static ConstructResult ConstructOpAnalyzeTGLP(ConstructContext* pContext,
                                                  CachedStreamReader* pStream);
    static ConstructResult
    ConstructOpPrepareCopySheet(ConstructContext* pContext,
                                CachedStreamReader* pStream);
    static ConstructResult
    ConstructOpPrepareExpandSheet(ConstructContext* pContext,
                                  CachedStreamReader* pStream);
    static ConstructResult ConstructOpCopy(ConstructContext* pContext,
                                           CachedStreamReader* pStream);
    static ConstructResult ConstructOpSkip(ConstructContext* pContext,
                                           CachedStreamReader* pStream);
    static ConstructResult ConstructOpExpand(ConstructContext* pContext,
                                             CachedStreamReader* pStream);
    static ConstructResult ConstructOpFatalError(ConstructContext* pContext,
                                                 CachedStreamReader* pStream);

private:
    u16* mGlyphIndexAdjustArray; // at 0x18
};

/******************************************************************************
 *
 * ArchiveFont
 *
 ******************************************************************************/
class ArchiveFont : public ArchiveFontBase {
public:
    ArchiveFont();
    virtual ~ArchiveFont(); // at 0x8

    virtual void GetGlyph(Glyph* pGlyph, u16 ch) const; // at 0x50

    static u32 GetRequireBufferSize(const void* pBrfnt,
                                    const char* pGlyphGroups = LOAD_GLYPH_ALL);

    bool Construct(void* pBuffer, u32 bufferSize, const void* pBrfnt,
                   const char* pGlyphGroups = LOAD_GLYPH_ALL);
    void* Destroy();

private:
    ConstructResult StreamingConstruct(ConstructContext* pContext,
                                       const void* pStream, u32 streamSize);

    void GetGlyphFromIndex(Glyph* pGlyph, u16 index) const;
};

} // namespace ut
} // namespace nw4r

#endif
