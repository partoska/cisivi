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

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Includes
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plat.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Definitions - Memory
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

CSVoid *
csMemZalloc (CSSize size)
{
  return calloc (1, size ? size : 1);
}

CSVoid *
csMemRealloc (CSVoid *ptr, CSSize size)
{
  return realloc (ptr, size ? size : 1);
}

void
csMemFree (CSVoid *ptr)
{
  free (ptr);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Definitions - Strings
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static CSInt
csLower (CSChar c)
{
  return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : (CSByte)c;
}

CSSize
csStrLen (const CSChar *str)
{
  return strlen (str);
}

CSInt
csStrCompare (const CSChar *a, const CSChar *b, CSBool icase)
{
  if (!icase)
    {
      return strcmp (a, b);
    }

  // Portable ASCII folding: no strcasecmp() on Windows, no locale surprises.
  while (*a && csLower (*a) == csLower (*b))
    {
      ++a;
      ++b;
    }

  return csLower (*a) - csLower (*b);
}

CSInt
csStrCompareN (const CSChar *a, const CSChar *b, CSSize n)
{
  return strncmp (a, b, n);
}

CSChar *
csStrFindAny (const CSChar *str, const CSChar *set)
{
  return strpbrk (str, set);
}

CSChar *
csStrDup (const CSChar *str, CSSize length)
{
  CSChar *copy = malloc (length + 1);
  if (copy)
    {
      memcpy (copy, str, length);
      copy[length] = '\0';
    }

  return copy;
}

CSInt
csStrFormat (CSChar *dst, CSSize size, const CSChar *fmt, ...)
{
  va_list args;
  va_start (args, fmt);
  CSInt n = vsnprintf (dst, size, fmt, args);
  va_end (args);

  return n;
}

CSInt
csStrFormatV (CSChar *dst, CSSize size, const CSChar *fmt, CSVaList args)
{
  return vsnprintf (dst, size, fmt, args);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Definitions - Files
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

CSFile *
csFileOpen (const CSChar *path, CSBool write)
{
  return (CSFile *)fopen (path, write ? "wb" : "rb");
}

CSSize
csFileRead (CSFile *file, CSVoid *data, CSSize size)
{
  return fread (data, 1, size, (FILE *)file);
}

CSSize
csFileWrite (CSFile *file, const CSVoid *data, CSSize size)
{
  return fwrite (data, 1, size, (FILE *)file);
}

CSBool
csFileFailed (CSFile *file)
{
  return ferror ((FILE *)file) != 0;
}

CSBool
csFileClose (CSFile *file)
{
  return fclose ((FILE *)file) == 0;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Definitions - System errors
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

CSInt
csSysError (void)
{
  return errno;
}

const CSChar *
csSysErrorText (CSInt code)
{
  return strerror (code);
}
