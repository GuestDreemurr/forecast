#include "size_t.h"
#include "wchar_t.h"

size_t wcslen(const wchar_t *pStr) {
    size_t len = -1;
    wchar_t* p = (wchar_t*)pStr - 1;

    do {
        len++;
    } while(*++p);

    return len;
}

wchar_t* wcscpy(wchar_t *pDest, const wchar_t *pSrc) {
    const wchar_t* p = (wchar_t*)pSrc - 1;
    wchar_t* q = (wchar_t*)pDest - 1;

    while (*++q = *++p);
    return pDest;
}


wchar_t* wcscat(wchar_t* dst, const wchar_t* src) {
    const wchar_t* p_src = src - 1;
    wchar_t* p_dst = dst - 1;
    wchar_t* original_dst = dst;
    wchar_t current_char;
    do {
        current_char = *++p_dst;
    } while (current_char != 0);

    p_dst--;

    do {
        current_char = *++p_src;
        *++p_dst = current_char;
    } while (current_char != 0);
    return original_dst;
}

wchar_t* wcschr(const wchar_t *pStr, const wchar_t chr) {
    const wchar_t* p = (wchar_t*)pStr - 1;
    wchar_t c = (chr & 0xFFFF);
    wchar_t ch;

    while (ch = (*++p& 0xFFFF)) {
        if (ch == c) {
            return ((wchar_t*)p);
        }
    }
    
    return (c ? 0 : (wchar_t*)p);
}
