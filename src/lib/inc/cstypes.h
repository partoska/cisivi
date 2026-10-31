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

#ifndef CSTYPES_H
#define CSTYPES_H

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Includes
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <stdarg.h>
#include <stddef.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Types
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifdef __cplusplus
typedef bool CSBool;
#else
typedef _Bool CSBool;
#endif
typedef char CSChar;
typedef signed char CSSChar;
typedef unsigned char CSByte;
typedef short CSShort;
typedef unsigned short CSWord;
typedef int CSInt;
typedef unsigned int CSDword;
typedef long CSLong;
typedef unsigned long CSULong;
typedef double CSDouble;

typedef void CSVoid;
typedef size_t CSSize;
typedef va_list CSVaList;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Macros
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define CS_TRUE ((CSBool)1)
#define CS_FALSE ((CSBool)0)

#define CS_UNUSED(x) ((void)(x))
#define CS_COUNTOF(a) (sizeof (a) / sizeof ((a)[0]))

#if defined(__GNUC__) || defined(__clang__)
#define CS_PRINTF(fmt, args) __attribute__ ((format (printf, fmt, args)))
#else
#define CS_PRINTF(fmt, args)
#endif

#endif
