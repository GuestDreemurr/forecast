#include "ctype.h"
#include "file_struct.h"
#include "locale.h"

#pragma exceptions on

int fflush(FILE*);
void free(void*);
int __flush_all(void);
long ftell(FILE*);
int __flush_buffer(FILE*, size_t*);

inline int tolower_inline(int c) {
    return ((c < 0) || (c >= 0x100)) ? c : (int)(_current_locale.ctype_cmpt_ptr->lower_map_ptr[c]);
}

int fclose(FILE* pFile) {
    int flush_res, close_res;

    if (pFile == 0) {
        return -1;
    }

    if (pFile->mode.file == 0) {
        return 0;
    }

    flush_res = fflush(pFile);
    close_res = (*pFile->close_proc)(pFile->handle);
    pFile->mode.file = 0;
    pFile->handle = 0;

    if (pFile->state.free_buffer) {
        free(pFile->buffer);
    }

    return ((flush_res || close_res) ? -1 : 0);
}

int fflush(FILE* pFile) {
    long pos;

    if (pFile == 0) {
        return __flush_all();
    }

    if (pFile->state.error || pFile->mode.file == 0) {
        return -1;
    }

    if (pFile->mode.io == 1) {
        return 0;
    }

    if (pFile->state.io_state >= 3) {
        pFile->state.io_state = 2;
    }

    if (pFile->state.io_state == 2) {
        pFile->buffer_len = 0;
    }

    if (pFile->state.io_state != 1) {
        pFile->state.io_state = 0;
        return 0;
    }

    if (pFile->mode.file != 1) {
        pos = 0;
    } else {
        pos = ftell(pFile);
    }

    if (__flush_buffer(pFile, 0)) {
        pFile->state.error = 1;
        pFile->buffer_len = 0;
        return -1;
    }

    pFile->state.io_state = 0;
    pFile->pos = pos;
    pFile->buffer_len = 0;
    return 0;
}

/* tolower is inlined here, but for some reason CW won't inline the one in ctype */
int __msl_strnicmp(const char* pStr1, const char* pStr2, int n) {
    int i;
    char c1, c2;

    for (i = 0; i < n; i++) {
        c1 = tolower_inline(*pStr1++);
        c2 = tolower_inline(*pStr2++);
        if (c1 < c2) {
            return -1;
        }

        if (c1 > c2) {
            return 1;
        }

        if (!c1) {
            return 0;
        }
    }

    return 0;
}

size_t strlen(const char*);

// Writes num in the given radix (digits above 9 are capital letters), returns str
char* __msl_itoa(int num, char* str, int radix) {
    char c;
    int sign = 0;
    int i = 0;
    int j;
    int k;
    unsigned int n = num;

    if (num < 0) {
        n = -num;
        sign = 1;
    }

    do {
        int rem = n % radix;

        if (rem > 9) {
            str[i++] = rem + 'A' - 10;
        }
        else {
            str[i++] = rem + '0';
        }
    } while ((n /= radix) != 0);

    if (sign) {
        str[i++] = '-';
    }
    str[i] = '\0';

    for (j = 0, k = strlen(str) - 1; j < k; j++, k--) {
        c = str[j];
        str[j] = str[k];
        str[k] = c;
    }

    return str;
}
