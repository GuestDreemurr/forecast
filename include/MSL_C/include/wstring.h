#ifndef WSTRING_H
#define WSTRING_H

#include "size_t.h"
#include "wchar_t.h"

#ifdef __cplusplus
extern "C" {
#endif

wchar_t* wcschr(const wchar_t*, const wchar_t);

wchar_t* wcscat(wchar_t* dst, const wchar_t* src);
size_t wcslen(const wchar_t*);

#ifdef __cplusplus
}
#endif

#endif  // WSTRING_H
