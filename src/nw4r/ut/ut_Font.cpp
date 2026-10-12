// HACK: the original emits Font's destructor and vtable in this file, right
// after InitReaderFunc, although every other file inlines the destructor.
// No natural form reproduces that (Font is abstract and nothing here uses
// it), so this file alone sees the destructor as out of line.
#define NW4R_UT_FONT_DTOR_OUT_OF_LINE
#include <nw4r/ut.h>

namespace nw4r {
namespace ut {

void Font::InitReaderFunc(FontEncoding encode) {
    switch (encode) {
    case FONT_ENCODING_UTF8: {
        mReadFunc = &CharStrmReader::ReadNextCharUTF8;
        break;
    }

    case FONT_ENCODING_UTF16: {
        mReadFunc = &CharStrmReader::ReadNextCharUTF16;
        break;
    }

    case FONT_ENCODING_SJIS: {
        mReadFunc = &CharStrmReader::ReadNextCharSJIS;
        break;
    }

    case FONT_ENCODING_CP1252:
    default: {
        mReadFunc = &CharStrmReader::ReadNextCharCP1252;
        break;
    }
    }
}

Font::~Font() {}

} // namespace ut
} // namespace nw4r
