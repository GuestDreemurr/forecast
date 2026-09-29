#ifndef NW4R_UT_ARCHIVE_FONT_H
#define NW4R_UT_ARCHIVE_FONT_H
#include <nw4r/types_nw4r.h>

#include <nw4r/ut/ut_ResFontBase.h>

namespace nw4r {
namespace ut {

class ArchiveFontBase : public detail::ResFontBase {
public:
    static const char LOAD_GLYPH_ALL[1];

protected:
    ArchiveFontBase();
    virtual ~ArchiveFontBase(); // at 0x8

    void* RemoveResourceBuffer();

    u8 unk18[0x4]; // at 0x18
};

class ArchiveFont : public ArchiveFontBase {
public:
    ArchiveFont();
    virtual ~ArchiveFont(); // at 0x8

    static u32 GetRequireBufferSize(const void* pBrfnt, const char* pGlyphGroups = LOAD_GLYPH_ALL);
    bool Construct(void* pBuffer, u32 bufferSize, const void* pBrfnt,
                   const char* pGlyphGroups = LOAD_GLYPH_ALL);
    void* Destroy();
};

} // namespace ut
} // namespace nw4r

#endif
