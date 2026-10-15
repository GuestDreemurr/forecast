#include <nw4r/lyt.h>
#include <nw4r/ut.h>

#include <cstring>
#include <cwchar>

/******************************************************************************
 *
 * Utility functions
 *
 ******************************************************************************/
namespace {

using namespace nw4r;
using namespace nw4r::lyt;

inline u8 ClampColor(s16 value) {
    return value < 0 ? 0 : (value > 255 ? 255 : value);
}

ut::Color GetColor(const GXColorS10& rColor16) {
    GXColor color8 = {ClampColor(rColor16.r), ClampColor(rColor16.g),
                      ClampColor(rColor16.b), ClampColor(rColor16.a)};

    return ut::Color(color8);
}

int CalcLineRectImpl(ut::Rect* pRect, ut::WideTextWriter* pWriter,
                     const wchar_t* pStr, int len, f32 maxWidth, bool* pOver);

void CalcStringRectImpl(ut::Rect* pRect, ut::WideTextWriter writer,
                        const wchar_t* pStr, int len, f32 maxWidth);

} // namespace

namespace nw4r {
namespace lyt {

NW4R_UT_RTTI_DEF_DERIVED(TextBox, Pane);

/******************************************************************************
 *
 * TextBox
 *
 ******************************************************************************/
TextBox::TextBox(const res::TextBox* pRes, const ResBlockSet& rBlockSet)
    : Pane(pRes) {

    u16 len = pRes->textBufBytes / sizeof(wchar_t);
    if (len > 0) {
        len--;
    }

    Init(len);

    if (pRes->textStrBytes >= sizeof(wchar_t) && mTextBuf != NULL) {
        const wchar_t* const pResStr =
            detail::ConvertOffsToPtr<wchar_t>(pRes, pRes->textStrOffset);

        u16 resLen = pRes->textStrBytes / sizeof(wchar_t) - 1;
        SetString(pResStr, 0, resLen);
    }

    for (int i = 0; i < TEXTCOLOR_MAX; i++) {
        mTextColors[i] = pRes->textCols[i];
    }

    mFontSize = pRes->fontSize;
    mTextPosition = pRes->textPosition;
    mCharSpace = pRes->charSpace;
    mLineSpace = pRes->lineSpace;

    const res::Font* const pResFonts = detail::ConvertOffsToPtr<res::Font>(
        rBlockSet.pFontList, sizeof(res::FontList));

    const char* const pFontName = detail::ConvertOffsToPtr<char>(
        pResFonts, pResFonts[pRes->fontIdx].nameStrOffset);

    ut::Font* pFont = rBlockSet.pResAccessor->GetFont(pFontName);

    if (pFont != NULL) {
        mpFont = pFont;
    } else {
        void* pFontBinary = rBlockSet.pResAccessor->GetResource(
            ArcResourceAccessor::RES_TYPE_FONT, pFontName, NULL);

        if (pFontBinary != NULL) {
            void* pFontBuf = Layout::AllocMemory(sizeof(ut::ResFont));

            if (pFontBuf != NULL) {
                ut::ResFont* pResFont = new (pFontBuf) ut::ResFont();
                pResFont->SetResource(pFontBinary);

                mpFont = pResFont;
                mBits.bAllocFont = true;
            }
        }
    }

    void* pMaterialBuf = Layout::AllocMemory(sizeof(Material));

    if (pMaterialBuf != NULL) {
        const u32* const pMatOffsetTbl = detail::ConvertOffsToPtr<u32>(
            rBlockSet.pMaterialList, sizeof(res::MaterialList));

        const res::Material* const pResMaterial =
            detail::ConvertOffsToPtr<res::Material>(
                rBlockSet.pMaterialList, pMatOffsetTbl[pRes->materialIdx]);

        Material* pMaterial =
            new (pMaterialBuf) Material(pResMaterial, rBlockSet);

        mpMaterial = pMaterial;
    }
}

void TextBox::Init(u16 len) {
    mTextBuf = NULL;
    mTextBufBytes = 0;
    mTextLen = 0;

    mpFont = NULL;
    mFontSize = Size(0.0f, 0.0f);

    SetTextPositionH(HORIZONTALPOSITION_CENTER);
    SetTextPositionV(VERTICALPOSITION_CENTER);

    mLineSpace = 0.0f;
    mCharSpace = 0.0f;

    mpTagProcessor = NULL;

    std::memset(&mBits, 0, sizeof(mBits));

    if (len > 0) {
        AllocStringBuffer(len);
    }
}

TextBox::~TextBox() {
    SetFont(NULL);

    if (mpMaterial != NULL && !mpMaterial->IsUserAllocated()) {
        mpMaterial->~Material();
        Layout::FreeMemory(mpMaterial);
        mpMaterial = NULL;
    }

    FreeStringBuffer();
}

ut::Color TextBox::GetVtxColor(u32 idx) const {
    return GetTextColor(idx / TEXTCOLOR_MAX);
}

void TextBox::SetVtxColor(u32 idx, ut::Color color) {
    SetTextColor(idx / TEXTCOLOR_MAX, color);
}

u8 TextBox::GetVtxColorElement(u32 idx) const {
    return reinterpret_cast<const u8*>(
        &mTextColors[idx / (4 * TEXTCOLOR_MAX)])[idx % 4];
}

void TextBox::SetVtxColorElement(u32 idx, u8 value) {
    reinterpret_cast<u8*>(&mTextColors[idx / (4 * TEXTCOLOR_MAX)])[idx % 4] =
        value;
}

ut::Rect TextBox::GetTextDrawRect(const DrawInfo& rInfo) const {
    ut::WideTextWriter writer;
    ut::Rect rect = GetTextDrawRect(&writer);

    if (rInfo.IsYAxisUp()) {
        rect.top = -rect.top;
        rect.bottom = -rect.bottom;
    }

    return rect;
}

void TextBox::DrawSelf(const DrawInfo& rInfo) {
    if (mTextBuf == NULL || mpFont == NULL || mpMaterial == NULL) {
        return;
    }

    LoadMtx(rInfo);

    ut::WideTextWriter writer;
    writer.SetFont(*mpFont);
    writer.SetFontSize(mFontSize.width, mFontSize.height);
    writer.SetLineSpace(mLineSpace);
    writer.SetCharSpace(mCharSpace);

    ut::Color top =
        detail::MultipleAlpha(mTextColors[TEXTCOLOR_TOP], mGlbAlpha);
    ut::Color bottom =
        detail::MultipleAlpha(mTextColors[TEXTCOLOR_BOTTOM], mGlbAlpha);

    writer.SetGradationMode(top != bottom ? ut::CharWriter::GRADMODE_V
                                          : ut::CharWriter::GRADMODE_NONE);
    writer.SetTextColor(top, bottom);

    ut::Color min = GetColor(mpMaterial->GetTevColor(TEVCOLOR_REG0));
    ut::Color max = GetColor(mpMaterial->GetTevColor(TEVCOLOR_REG1));
    writer.SetColorMapping(min, max);

    if (mpTagProcessor != NULL) {
        writer.SetTagProcessor(mpTagProcessor);
    }

    writer.SetupGX();

    f32 width;
    ut::Rect rect = GetTextDrawRect(&writer);

    f32 magH;
    switch (GetTextPositionH()) {
    default:
    case HORIZONTALPOSITION_LEFT: {
        magH = 0.0f;
        break;
    }

    case HORIZONTALPOSITION_CENTER: {
        magH = 0.5f;
        break;
    }

    case HORIZONTALPOSITION_RIGHT: {
        magH = 1.0f;
        break;
    }
    }

    const wchar_t* pStr = mTextBuf;
    const ut::Rect& rRect = rect;
    width = rRect.right - rRect.left;

    writer.SetCursor(rect.left, rect.top);

    for (int remain = mTextLen; remain > 0;) {
        f32 maxWidth = mSize.width;
        ut::Rect lineRect;
        bool over;
        int lineLen;
        f32 lineWidth;

        {
            ut::WideTextWriter lineWriter = writer;
            lineWriter.SetCursor(0.0f, 0.0f);

            lineLen = CalcLineRectImpl(&lineRect, &lineWriter, pStr, remain,
                                       maxWidth, &over);

            lineWidth = lineRect.GetWidth();
        }

        writer.SetCursorX(rect.left + magH * (width - lineWidth));
        writer.Print(pStr, lineLen);

        if (over) {
            writer.Print(L"\n");
        }

        pStr += lineLen;
        remain -= lineLen;
    }
}

u16 TextBox::GetStringBufferLength() const {
    if (mTextBufBytes == 0) {
        return 0;
    }

    return mTextBufBytes / sizeof(wchar_t) - 1;
}

void TextBox::AllocStringBuffer(u16 len) {
    if (len == 0) {
        return;
    }

    u16 chars = len + 1;
    u16 bytes = chars * sizeof(wchar_t);

    if (bytes > mTextBufBytes) {
        FreeStringBuffer();
        mTextBuf = static_cast<wchar_t*>(Layout::AllocMemory(bytes));

        if (mTextBuf != NULL) {
            mTextBufBytes = bytes;
        }
    }
}

void TextBox::FreeStringBuffer() {
    if (mTextBuf == NULL) {
        return;
    }

    Layout::FreeMemory(mTextBuf);
    mTextBuf = NULL;
    mTextBufBytes = 0;
}

u16 TextBox::SetString(const wchar_t* pStr, u16 pos) {
    return SetString(pStr, pos, std::wcslen(pStr));
}

u16 TextBox::SetString(const wchar_t* pStr, u16 pos, u16 len) {
    if (mTextBuf == NULL) {
        return 0;
    }

    const u16 maxlen = GetStringBufferLength();
    if (pos >= maxlen) {
        return 0;
    }

    const u16 chars = ut::Min<u16>(len, maxlen - pos);
    memcpy(&mTextBuf[pos], pStr, chars * sizeof(wchar_t));

    mTextLen = pos + chars;
    mTextBuf[mTextLen] = L'\0';

    return chars;
}

const ut::Font* TextBox::GetFont() const {
    return mpFont;
}

void TextBox::SetFont(const ut::Font* pFont) {
    if (mBits.bAllocFont) {
        mpFont->~Font();
        Layout::FreeMemory(const_cast<ut::Font*>(mpFont));
        mBits.bAllocFont = false;
    }

    mpFont = pFont;

    if (mpFont != NULL) {
        SetFontSize(Size(static_cast<f32>(mpFont->GetWidth()),
                         static_cast<f32>(mpFont->GetHeight())));
    } else {
        SetFontSize(Size(0.0f, 0.0f));
    }
}

ut::Rect TextBox::GetTextDrawRect(ut::WideTextWriter* pWriter) const {
    ut::Rect rect;

    pWriter->SetCursor(0.0f, 0.0f);
    f32 maxWidth = mSize.width;
    int len = mTextLen;
    const wchar_t* pStr = mTextBuf;
    CalcStringRectImpl(&rect, *pWriter, pStr, len, maxWidth);

    math::VEC2 base = GetVtxPos();
    rect.MoveTo(base.x + (mSize.width - rect.GetWidth()) * GetTextMagH(),
                base.y + (mSize.height - rect.GetHeight()) * GetTextMagV());

    return rect;
}

f32 TextBox::GetTextMagH() const {
    f32 mag = 0.0f;

    switch (GetTextPositionH()) {
    default:
    case HORIZONTALPOSITION_LEFT: {
        mag = 0.0f;
        break;
    }

    case HORIZONTALPOSITION_CENTER: {
        mag = 0.5f;
        break;
    }

    case HORIZONTALPOSITION_RIGHT: {
        mag = 1.0f;
        break;
    }
    }

    return mag;
}

f32 TextBox::GetTextMagV() const {
    f32 mag = 0.0f;

    switch (GetTextPositionV()) {
    default:
    case VERTICALPOSITION_TOP: {
        mag = 0.0f;
        break;
    }

    case VERTICALPOSITION_CENTER: {
        mag = 0.5f;
        break;
    }

    case VERTICALPOSITION_BOTTOM: {
        mag = 1.0f;
        break;
    }
    }

    return mag;
}

u32 TextBox::MakeDrawFlag() const {
    u32 flag = 0;

    switch (GetTextPositionH()) {
    case HORIZONTALPOSITION_CENTER: {
        flag |= ut::WideTextWriter::DRAWFLAG_ALIGN_TEXT_CENTER;
        break;
    }

    case HORIZONTALPOSITION_RIGHT: {
        flag |= ut::WideTextWriter::DRAWFLAG_ALIGN_TEXT_RIGHT;
        break;
    }

    default: {
        break;
    }
    }

    return flag;
}

} // namespace lyt
} // namespace nw4r

/******************************************************************************
 *
 * Text measurement with a width limit
 *
 ******************************************************************************/
namespace {

int CalcLineRectImpl(ut::Rect* pRect, ut::WideTextWriter* pWriter,
                     const wchar_t* pStr, int len, f32 maxWidth, bool* pOver) {

    ut::PrintContext<wchar_t> context = {
        pWriter, // writer
        pStr     // str
    };

    const ut::Font* pFont = pWriter->GetFont();

    f32 x = 0.0f;
    bool charSpace = false;

    ut::CharStrmReader reader = pFont->GetCharStrmReader();
    const wchar_t* pPrevPos = static_cast<const wchar_t*>(reader.GetCurrentPos());

    pRect->left = 0.0f;
    pRect->right = 0.0f;
    pRect->top = ut::Min(0.0f, pWriter->GetLineHeight());
    pRect->bottom = ut::Max(0.0f, pWriter->GetLineHeight());

    *pOver = false;

    reader.Set(pStr);
    ut::Rect prevRect = *pRect;

    u16 ch = reader.Next();

    while (static_cast<const wchar_t*>(reader.GetCurrentPos()) - pStr <= len) {
        if (ch < ' ') {
            ut::Rect r(x, 0.0f, 0.0f, 0.0f);
            context.str = static_cast<const wchar_t*>(reader.GetCurrentPos());
            context.flags =
                charSpace ? 0 : ut::PrintContext<wchar_t>::FLAGS_CHARSPACE;

            pWriter->SetCursorX(x);

            ut::WideTagProcessor::Operation oper =
                pWriter->GetTagProcessor()->CalcRect(&r, ch, &context);

            reader.Set(context.str);

            pRect->left = ut::Min(pRect->left, r.left);
            pRect->top = ut::Min(pRect->top, r.top);
            pRect->right = ut::Max(pRect->right, r.right);
            pRect->bottom = ut::Max(pRect->bottom, r.bottom);

            x = pWriter->GetCursorX();

            if (pRect->GetWidth() > maxWidth) {
                *pOver = true;
                break;
            }

            if (oper == ut::WideTagProcessor::OPERATION_END_DRAW) {
                return len;
            }

            if (oper == ut::WideTagProcessor::OPERATION_NO_CHAR_SPACE) {
                charSpace = false;
            } else if (oper == ut::WideTagProcessor::OPERATION_CHAR_SPACE) {
                charSpace = true;
            } else if (oper == ut::WideTagProcessor::OPERATION_NEXT_LINE) {
                break;
            }
        } else {
            if (charSpace) {
                x += pWriter->GetCharSpace();
            }

            charSpace = true;

            if (pWriter->IsWidthFixed()) {
                x += pWriter->GetFixedWidth();
            } else {
                x += pWriter->GetFont()->GetCharWidth(ch) * pWriter->GetScaleH();
            }

            pRect->left = ut::Min(pRect->left, x);
            pRect->right = ut::Max(pRect->right, x);

            if (pRect->GetWidth() > maxWidth) {
                *pOver = true;
                break;
            }
        }

        pPrevPos = static_cast<const wchar_t*>(reader.GetCurrentPos());
        ch = reader.Next();
        prevRect = *pRect;
    }

    if (*pOver && pPrevPos != NULL) {
        *pRect = prevRect;
        return pPrevPos - pStr;
    }

    return static_cast<const wchar_t*>(reader.GetCurrentPos()) - pStr;
}

void CalcStringRectImpl(ut::Rect* pRect, ut::WideTextWriter writer,
                        const wchar_t* pStr, int len, f32 maxWidth) {

    int remain = len;

    pRect->left = 0.0f;
    pRect->right = 0.0f;
    pRect->top = 0.0f;
    pRect->bottom = 0.0f;

    writer.SetCursor(0.0f, 0.0f);

    do {
        ut::Rect r;
        bool over;

        int read = CalcLineRectImpl(&r, &writer, pStr, remain, maxWidth, &over);
        if (over) {
            CalcLineRectImpl(&r, &writer, L"\n", 1, maxWidth, &over);
        }

        pStr += read;
        remain -= read;

        pRect->left = ut::Min(pRect->left, r.left);
        pRect->top = ut::Min(pRect->top, r.top);
        pRect->right = ut::Max(pRect->right, r.right);
        pRect->bottom = ut::Max(pRect->bottom, r.bottom);
    } while (remain > 0);
}

} // namespace
