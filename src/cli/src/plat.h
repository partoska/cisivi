/*
 * Cisivi - Small, minimal CSV library and query tool.
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

#ifndef CSC_PLAT_H
#define CSC_PLAT_H

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Includes
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "cstypes.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Declarations - Strings
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**
 * Measures a string.
 *
 * @param str NUL-terminated string.
 * @return Number of bytes before the terminating NUL.
 */
CSSize cscStrLen (const CSChar *str);

/**
 * Compares two strings byte by byte, case-sensitively; locale settings play
 * no part.
 *
 * @param a NUL-terminated string.
 * @param b NUL-terminated string.
 * @return Negative, zero or positive as a sorts before, equal to or after
 *         b.
 */
CSInt cscStrCmp (const CSChar *a, const CSChar *b);

/**
 * Compares two strings byte by byte, case-sensitively.
 *
 * @param a NUL-terminated string.
 * @param b NUL-terminated string.
 * @return CS_TRUE when both hold the same bytes.
 */
CSBool cscStrEq (const CSChar *a, const CSChar *b);

/**
 * Finds the first occurrence of a character.
 *
 * @param str NUL-terminated string to search.
 * @param c   Character to find; '\0' finds the terminator.
 * @return Pointer into str at the first c, or NULL when there is none.
 */
CSChar *cscStrFind (const CSChar *str, CSChar c);

/**
 * Finds the first occurrence of a string within another.
 *
 * @param str  NUL-terminated string to search.
 * @param part NUL-terminated string to find; "" is found at the start.
 * @return Pointer into str at the first part, or NULL when there is none.
 */
CSChar *cscStrFindStr (const CSChar *str, const CSChar *part);

/**
 * Copies the start of a string into a new one; release it with
 * cscMemFree().
 *
 * @param str    Bytes to copy; need not be NUL-terminated.
 * @param length Number of bytes to copy.
 * @return New NUL-terminated string of length bytes, or NULL when out of
 *         memory.
 */
CSChar *cscStrDup (const CSChar *str, CSSize length);

/**
 * Reads a decimal number, such as 42, -0.5 or 1e3, taking the whole string
 * apart from surrounding blanks; hexadecimal, infinities and NaN are text.
 *
 * @param str    NUL-terminated string.
 * @param number Receives the number; untouched when str is not one.
 * @return CS_TRUE when str holds a number.
 */
CSBool cscStrNumber (const CSChar *str, CSDouble *number);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Declarations - Memory
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**
 * Allocates uninitialized memory for an array; release it with
 * cscMemFree().
 *
 * @param count Number of items; 0 still gives a valid, unique block.
 * @param size  Size of one item in bytes.
 * @return New block, or NULL when out of memory or when count * size
 *         overflows.
 */
CSVoid *cscMemAlloc (CSSize count, CSSize size);

/**
 * Releases memory from cscMemAlloc() or cscStrDup(). Accepts NULL.
 *
 * @param ptr Block to release.
 */
void cscMemFree (CSVoid *ptr);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Declarations - Streams
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**
 * Prepares the standard streams; call once, before any other stream
 * function. On Windows it switches standard input and output to binary
 * mode, so they carry the same bytes as csSave() and other platforms.
 */
void cscStreamInit (void);

/**
 * Reads standard input; a CSReadFn for csRead().
 *
 * @param user Pointer to a CSInt that receives the system error code
 *             (errno) of a failed read; untouched otherwise.
 * @param data Buffer to fill; not NUL-terminated.
 * @param size Capacity of the buffer in bytes.
 * @return Number of bytes stored, at most size; 0 at the end of the input,
 *         or CS_READ_ERROR on failure.
 */
CSSize cscReadIn (CSVoid *user, CSChar *data, CSSize size);

/**
 * Writes to standard output; a CSWriteFn for csWrite().
 *
 * @param user Ignored.
 * @param data Text to write; not NUL-terminated.
 * @param size Number of bytes.
 * @return Number of bytes written; anything but size is a failure.
 */
CSSize cscWriteOut (CSVoid *user, const CSChar *data, CSSize size);

/**
 * Writes a string to standard output, as is; no newline is added.
 *
 * @param str NUL-terminated string.
 * @return CS_FALSE when the write failed.
 */
CSBool cscPrint (const CSChar *str);

/**
 * Writes one character to standard output, NUL included.
 *
 * @param c Character to write.
 * @return CS_FALSE when the write failed.
 */
CSBool cscPrintChar (CSChar c);

/**
 * Flushes standard output; call before exiting to catch late write
 * failures, e.g. a full disk or a closed pipe.
 *
 * @return CS_FALSE when buffered data could not be written, or an earlier
 *         write to standard output failed.
 */
CSBool cscFlush (void);

/**
 * Formats a message to standard error; no newline is added.
 *
 * @param fmt printf()-style format.
 * @param ... Arguments of fmt.
 */
void cscError (const CSChar *fmt, ...) CS_PRINTF (1, 2);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Declarations - System errors
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**
 * Gets the code of the last system error.
 *
 * @return errno, as left by the last failed system call.
 */
CSInt cscSysError (void);

/**
 * Describes a system error code.
 *
 * @param code System error code, e.g. from cscSysError() or CSError.code.
 * @return Human-readable text owned by the C library; valid until the next
 *         call.
 */
const CSChar *cscSysErrorText (CSInt code);

#endif
