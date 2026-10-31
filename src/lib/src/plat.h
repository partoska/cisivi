/*
 * Cisivi Library - Small, minimal CSV reader and writer.
 * Copyright (C) 2026 Fabrika Charvat s.r.o. All rights reserved.
 * Developed by Partoska Laboratory team, <https://lab.partoska.com>
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * You can contact the author(s) via email at ask <at> partoska.com.
 */

#ifndef CS_PLAT_H
#define CS_PLAT_H

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Includes
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "cstypes.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Types
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/** Opaque open file, from csFileOpen(); released by csFileClose(). */
typedef struct CSFile CSFile;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Declarations - Memory
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**
 * Allocates zeroed memory; release it with csMemFree().
 *
 * @param size Number of bytes; 0 still gives a valid, unique block.
 * @return New block, or NULL when out of memory.
 */
CSVoid *csMemZalloc (CSSize size);

/**
 * Resizes a block, keeping its contents up to the smaller size; bytes
 * beyond the old size are uninitialized.
 *
 * @param ptr  Block from csMemZalloc() or csMemRealloc(), or NULL for a new
 *             one.
 * @param size New size in bytes; 0 still gives a valid, unique block.
 * @return Resized block, possibly moved, or NULL when out of memory; ptr
 *         then stays valid and unchanged.
 */
CSVoid *csMemRealloc (CSVoid *ptr, CSSize size);

/**
 * Releases memory from csMemZalloc(), csMemRealloc() or csStrDup().
 * Accepts NULL.
 *
 * @param ptr Block to release.
 */
void csMemFree (CSVoid *ptr);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Declarations - Strings
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**
 * Measures a string.
 *
 * @param str NUL-terminated string.
 * @return Number of bytes before the terminating NUL.
 */
CSSize csStrLen (const CSChar *str);

/**
 * Compares two strings, byte by byte or ASCII case-insensitively; locale
 * settings play no part.
 *
 * @param a     NUL-terminated string.
 * @param b     NUL-terminated string.
 * @param icase CS_TRUE to treat A to Z as a to z.
 * @return Negative, zero or positive as a sorts before, equal to or after
 *         b.
 */
CSInt csStrCompare (const CSChar *a, const CSChar *b, CSBool icase);

/**
 * Compares at most the first n bytes of two strings, case-sensitively.
 *
 * @param a String; stops at its NUL if shorter than n.
 * @param b String; stops at its NUL if shorter than n.
 * @param n Maximum number of bytes to compare.
 * @return Negative, zero or positive as a sorts before, equal to or after
 *         b.
 */
CSInt csStrCompareN (const CSChar *a, const CSChar *b, CSSize n);

/**
 * Finds the first occurrence of any character from a set.
 *
 * @param str NUL-terminated string to search.
 * @param set NUL-terminated characters to find.
 * @return Pointer into str at the first match, or NULL when there is none.
 */
CSChar *csStrFindAny (const CSChar *str, const CSChar *set);

/**
 * Copies the start of a string into a new one; release it with
 * csMemFree().
 *
 * @param str    Bytes to copy; need not be NUL-terminated.
 * @param length Number of bytes to copy.
 * @return New NUL-terminated string of length bytes, or NULL when out of
 *         memory.
 */
CSChar *csStrDup (const CSChar *str, CSSize length);

/**
 * Formats into a buffer like snprintf(), truncating to fit.
 *
 * @param dst  Buffer; always NUL-terminated when size is not 0.
 * @param size Capacity of dst in bytes.
 * @param fmt  printf()-style format.
 * @param ...  Arguments of fmt.
 * @return Length of the full result without the NUL, which is size or more
 *         when truncated; negative on an encoding error.
 */
CSInt csStrFormat (CSChar *dst, CSSize size, const CSChar *fmt, ...)
    CS_PRINTF (3, 4);

/**
 * Formats into a buffer like vsnprintf(), as csStrFormat() does.
 *
 * @param dst  Buffer; always NUL-terminated when size is not 0.
 * @param size Capacity of dst in bytes.
 * @param fmt  printf()-style format.
 * @param args Arguments of fmt.
 * @return Length of the full result without the NUL, which is size or more
 *         when truncated; negative on an encoding error.
 */
CSInt csStrFormatV (CSChar *dst, CSSize size, const CSChar *fmt,
                    CSVaList args);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Declarations - Files
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**
 * Opens a file in binary mode, so bytes pass unchanged on every platform.
 *
 * @param path  File to open.
 * @param write CS_FALSE to read; CS_TRUE to write, creating the file or
 *              truncating it.
 * @return Open file, or NULL on failure, with csSysError() telling why.
 */
CSFile *csFileOpen (const CSChar *path, CSBool write);

/**
 * Reads from a file.
 *
 * @param file File opened for reading.
 * @param data Buffer to fill.
 * @param size Capacity of the buffer in bytes.
 * @return Number of bytes stored; less than size at the end of the file or
 *         on failure, which csFileFailed() tells apart.
 */
CSSize csFileRead (CSFile *file, CSVoid *data, CSSize size);

/**
 * Writes to a file.
 *
 * @param file File opened for writing.
 * @param data Bytes to write.
 * @param size Number of bytes.
 * @return Number of bytes written; anything but size is a failure.
 */
CSSize csFileWrite (CSFile *file, const CSVoid *data, CSSize size);

/**
 * Reports whether a read or write on a file failed.
 *
 * @param file Open file.
 * @return CS_TRUE once any read or write on file has failed.
 */
CSBool csFileFailed (CSFile *file);

/**
 * Closes a file, writing out buffered data; the file is released even when
 * that fails.
 *
 * @param file Open file; not NULL.
 * @return CS_FALSE when buffered data could not be written.
 */
CSBool csFileClose (CSFile *file);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Declarations - System errors
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**
 * Gets the code of the last system error.
 *
 * @return errno, as left by the last failed system call.
 */
CSInt csSysError (void);

/**
 * Describes a system error code.
 *
 * @param code System error code, e.g. from csSysError().
 * @return Human-readable text owned by the C library; valid until the next
 *         call.
 */
const CSChar *csSysErrorText (CSInt code);

#endif
