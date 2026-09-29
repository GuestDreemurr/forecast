#ifndef NW4R_UT_TEXT_WRITER_BASE_H
#define NW4R_UT_TEXT_WRITER_BASE_H
#include <nw4r/types_nw4r.h>

#include <nw4r/ut/ut_CharWriter.h>
#include <nw4r/ut/ut_TagProcessorBase.h>

#include <nw4r/math.h>

#include <cstdio>
#include <cstring>
#include <cwchar>

namespace nw4r {
namespace ut {

template <typename T> class TextWriterBase : public CharWriter {
public:
    typedef TagProcessorBase<T> TagProcessorType;

public:
    enum DrawFlag {
        // Align text lines
        DRAWFLAG_ALIGN_TEXT_BASELINE = 0,
        DRAWFLAG_ALIGN_TEXT_CENTER = (1 << 0),
        DRAWFLAG_ALIGN_TEXT_RIGHT = (1 << 1),

        // Align text block (horizontal)
        DRAWFLAG_ALIGN_H_BASELINE = 0,
        DRAWFLAG_ALIGN_H_CENTER = (1 << 4),
        DRAWFLAG_ALIGN_H_RIGHT = (1 << 5),

        // Align text block (vertical)
        DRAWFLAG_ALIGN_V_BASELINE = 0,
        DRAWFLAG_ALIGN_V_CENTER = (1 << 8),
        DRAWFLAG_ALIGN_V_TOP = (1 << 9),

        // Mask constants
        DRAWFLAG_MASK_ALIGN_TEXT = DRAWFLAG_ALIGN_TEXT_BASELINE |
                                   DRAWFLAG_ALIGN_TEXT_CENTER |
                                   DRAWFLAG_ALIGN_TEXT_RIGHT,

        DRAWFLAG_MASK_ALIGN_H = DRAWFLAG_ALIGN_H_BASELINE |
                                DRAWFLAG_ALIGN_H_CENTER |
                                DRAWFLAG_ALIGN_H_RIGHT,

        DRAWFLAG_MASK_ALIGN_V = DRAWFLAG_ALIGN_V_BASELINE |
                                DRAWFLAG_ALIGN_V_CENTER | DRAWFLAG_ALIGN_V_TOP,
    };

public:
    TextWriterBase();
    ~TextWriterBase();

    // No width limit in this version of nw4r
    f32 GetWidthLimit() const {
        return NW4R_MATH_FLT_MAX;
    }

    void SetLineSpace(f32 space);
    void SetCharSpace(f32 space);
    f32 GetCharSpace() const;
    int GetTabWidth() const;
    void SetDrawFlag(u32 flag);
    void SetTagProcessor(TagProcessorBase<T>* pProcessor);
    TagProcessorBase<T>* GetTagProcessor() const;

    f32 GetLineSpace() const {
        return mLineSpace;
    }
    void SetTabWidth(int width) {
        mTabWidth = width;
    }
    u32 GetDrawFlag() const {
        return mDrawFlag;
    }
    void ResetTagProcessor() {
        mTagProcessor = &mDefaultTagProcessor;
    }

    f32 GetLineHeight() const;

    f32 CalcLineWidth(const T* pStr, int len);
    f32 CalcStringWidth(const T* pStr) const;
    f32 CalcStringWidth(const T* pStr, int len) const;
    void CalcStringRect(Rect* pRect, const T* pStr, int len) const;

    int VSNPrintf(T* buffer, u32 count, const T* pStr, std::va_list args);
    f32 VPrintf(const T* pStr, std::va_list args);
    f32 Print(const T* pStr, int len);
    f32 Print(const T* pStr);

    static T* GetBuffer() {
        return mFormatBuffer;
    }
    static T* SetBuffer(T* pBuffer, u32 size) {
        T* pOldBuffer = mFormatBuffer;
        mFormatBuffer = pBuffer;
        mFormatBufferSize = size;
        return pOldBuffer;
    }

    static u32 GetBufferSize() {
        return mFormatBufferSize;
    }

private:
    static const int DEFAULT_FORMAT_BUFFER_SIZE = 256;

    static const u32 DRAWFLAG_MASK_ALL = DRAWFLAG_MASK_ALIGN_TEXT |
                                         DRAWFLAG_MASK_ALIGN_H |
                                         DRAWFLAG_MASK_ALIGN_V;

private:
    static int StrLen(const T* pStr);

    bool IsDrawFlagSet(u32 mask, u32 flag) const {
        return (mDrawFlag & mask) == flag;
    }

    bool CalcLineRectImpl(Rect* pRect, const T** ppStr, int len);
    void CalcStringRectImpl(Rect* pRect, const T* pStr, int len);

    f32 PrintImpl(const T* pStr, int len);
    f32 AdjustCursor(f32* pX, f32* pY, const T* pStr, int len);

private:
    f32 mCharSpace;                     // at 0x4C
    f32 mLineSpace;                     // at 0x50
    int mTabWidth;                      // at 0x54
    u32 mDrawFlag;                      // at 0x58
    TagProcessorBase<T>* mTagProcessor; // at 0x5C

    static T* mFormatBuffer;
    static u32 mFormatBufferSize;
    static TagProcessorBase<T> mDefaultTagProcessor;
};

template <> inline int TextWriterBase<char>::StrLen(const char* pStr) {
    return std::strlen(pStr);
}

template <> inline int TextWriterBase<wchar_t>::StrLen(const wchar_t* pStr) {
    return std::wcslen(pStr);
}

template <>
inline int TextWriterBase<char>::VSNPrintf(char* pBuffer, u32 count,
                                           const char* pStr,
                                           std::va_list args) {

    return std::vsnprintf(pBuffer, count, pStr, args);
}

template <>
inline int TextWriterBase<wchar_t>::VSNPrintf(wchar_t* pBuffer, u32 count,
                                              const wchar_t* pStr,
                                              std::va_list args) {

    return std::vswprintf(pBuffer, count, pStr, args);
}

} // namespace ut
} // namespace nw4r

#endif
