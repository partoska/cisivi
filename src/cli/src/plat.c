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

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Includes
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "cisivi.h"
#include "plat.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Definitions - Strings
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

CSSize
cscStrLen (const CSChar *str)
{
  return strlen (str);
}

CSInt
cscStrCmp (const CSChar *a, const CSChar *b)
{
  return strcmp (a, b);
}

CSBool
cscStrEq (const CSChar *a, const CSChar *b)
{
  return strcmp (a, b) == 0;
}

CSChar *
cscStrFind (const CSChar *str, CSChar c)
{
  return strchr (str, c);
}

CSChar *
cscStrFindStr (const CSChar *str, const CSChar *part)
{
  return strstr (str, part);
}

CSChar *
cscStrDup (const CSChar *str, CSSize length)
{
  CSChar *copy = malloc (length + 1);
  if (copy)
    {
      memcpy (copy, str, length);
      copy[length] = '\0';
    }

  return copy;
}

CSBool
cscStrNumber (const CSChar *str, CSDouble *number)
{
  // Only digits, signs, the point and exponents: strtod() would also take
  // hexadecimal, inf and nan, which read as words in a table.
  const CSChar *at = str;
  for (; *at == ' ' || *at == '\t'; ++at)
    {
    }
  if (!*at || !strchr ("0123456789+-.", *at))
    {
      return CS_FALSE;
    }
  for (const CSChar *c = at; *c; ++c)
    {
      if (!strchr ("0123456789+-.eE \t", *c))
        {
          return CS_FALSE;
        }
    }

  // The C locale, as nothing calls setlocale(): the point is the decimal
  // separator everywhere.
  CSChar *end = NULL;
  CSDouble value = strtod (at, &end);
  if (end == at)
    {
      return CS_FALSE;
    }
  for (; *end == ' ' || *end == '\t'; ++end)
    {
    }
  if (*end)
    {
      return CS_FALSE;
    }

  *number = value;

  return CS_TRUE;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Definitions - Memory
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

CSVoid *
cscMemAlloc (CSSize count, CSSize size)
{
  if (size && count > (CSSize)-1 / size)
    {
      return NULL;
    }

  CSSize bytes = count * size;

  return malloc (bytes > 0 ? bytes : 1);
}

void
cscMemFree (CSVoid *ptr)
{
  free (ptr);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Definitions - Streams
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
cscStreamInit (void)
{
#ifdef _WIN32
  // Text mode would turn LF into CRLF and stop reading at Ctrl-Z: binary
  // standard streams carry the same bytes as csSave() and other platforms.
  _setmode (_fileno (stdin), _O_BINARY);
  _setmode (_fileno (stdout), _O_BINARY);
#endif
}

CSSize
cscReadIn (CSVoid *user, CSChar *data, CSSize size)
{
  CSSize n = fread (data, 1, size, stdin);
  if (n == 0 && ferror (stdin))
    {
      *(CSInt *)user = errno;
      return CS_READ_ERROR;
    }

  return n;
}

CSSize
cscWriteOut (CSVoid *user, const CSChar *data, CSSize size)
{
  CS_UNUSED (user);

  return fwrite (data, 1, size, stdout);
}

CSBool
cscPrint (const CSChar *str)
{
  return fputs (str, stdout) >= 0;
}

CSBool
cscPrintChar (CSChar c)
{
  return putchar ((CSByte)c) != EOF;
}

CSBool
cscFlush (void)
{
  return fflush (stdout) == 0 && !ferror (stdout);
}

void
cscError (const CSChar *fmt, ...)
{
  va_list args;
  va_start (args, fmt);
  vfprintf (stderr, fmt, args);
  va_end (args);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Definitions - System errors
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

CSInt
cscSysError (void)
{
  return errno;
}

const CSChar *
cscSysErrorText (CSInt code)
{
  return strerror (code);
}
