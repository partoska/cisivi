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

#include "cisivi.h"
#include "plat.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Macros
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define CS_READ_CHUNK (4096)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Types - Private
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct CSRow
{
  CSChar **fields;
  CSSize count;
  CSSize capacity;
};

struct CSCsv
{
  CSRow **rows;
  CSSize count;
  CSSize capacity;
  CSDword flags;
  CSChar delimiter;
  const CSChar *newline;
  CSError error;
};

/* A file being read by csLoad(), with the errno of a failed read. */
typedef struct CSFileReader
{
  CSFile *file;
  CSInt code;
} CSFileReader;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Definitions - Errors
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static void
csSucceed (CSCsv *csv)
{
  csv->error.status = CS_OK;
  csv->error.line = 0;
  csv->error.code = 0;
  csv->error.message[0] = '\0';
}

/* Records a failure; a line number prefixes the message. */
static CSStatus csFail (CSCsv *csv, CSStatus status, CSSize line,
                        const CSChar *fmt, ...) CS_PRINTF (4, 5);

static CSStatus
csFail (CSCsv *csv, CSStatus status, CSSize line, const CSChar *fmt, ...)
{
  CSChar text[CS_ERROR_MAX];
  CSVaList args;
  va_start (args, fmt);
  csStrFormatV (text, sizeof (text), fmt, args);
  va_end (args);

  CSError *error = &csv->error;
  error->status = status;
  error->line = line;
  error->code = 0;
  if (line > 0)
    {
      // %zu is unknown to the msvcrt of MinGW, so the line goes as %lu
      csStrFormat (error->message, sizeof (error->message), "line %lu: %s",
                   (CSULong)line, text);
    }
  else
    {
      csStrFormat (error->message, sizeof (error->message), "%s", text);
    }

  return status;
}

/* Records a failed system call on a path, keeping errno. */
static CSStatus
csFailSys (CSCsv *csv, const CSChar *path, CSInt code)
{
  csFail (csv, CS_EIO, 0, "%s: %s", path, csSysErrorText (code));
  csv->error.code = code;

  return CS_EIO;
}

static CSStatus
csFailMem (CSCsv *csv)
{
  return csFail (csv, CS_EMEM, 0, "out of memory");
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Definitions - Private
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* Makes room for at least needed items, doubling the capacity. */
static CSBool
csGrow (CSVoid **items, CSSize *capacity, CSSize needed, CSSize size)
{
  if (needed <= *capacity)
    {
      return CS_TRUE;
    }

  CSSize maximum = (CSSize)-1 / size;
  CSSize next = *capacity ? *capacity : 8;
  while (next < needed)
    {
      if (next > maximum / 2)
        {
          return CS_FALSE;
        }
      next *= 2;
    }
  if (next > maximum)
    {
      return CS_FALSE;
    }

  CSVoid *grown = csMemRealloc (*items, next * size);
  if (!grown)
    {
      return CS_FALSE;
    }

  *items = grown;
  *capacity = next;

  return CS_TRUE;
}

static void
csRowFree (CSRow *row)
{
  for (CSSize i = 0; i < row->count; ++i)
    {
      csMemFree (row->fields[i]);
    }

  csMemFree (row->fields);
  csMemFree (row);
}

/* Appends a copy of a value to a row. */
static CSBool
csFieldAdd (CSRow *row, const CSChar *value)
{
  if (!csGrow ((CSVoid **)&row->fields, &row->capacity, row->count + 1,
               sizeof (*row->fields)))
    {
      return CS_FALSE;
    }

  CSChar *copy = csStrDup (value, csStrLen (value));
  if (!copy)
    {
      return CS_FALSE;
    }

  row->fields[row->count++] = copy;

  return CS_TRUE;
}

/* Checks that a header name at index is non-empty and unique within row.
 * Parsing errors (line > 0) are syntax errors, API errors bad arguments. */
static CSStatus
csHeaderCheck (CSCsv *csv, const CSRow *row, CSSize index, const CSChar *name,
               CSSize line)
{
  if (!*name)
    {
      return csFail (csv, line ? CS_ESYNTAX : CS_EARG, line,
                     "empty column name");
    }

  CSBool icase = (csv->flags & CS_FICASE) != 0;
  for (CSSize i = 0; i < row->count; ++i)
    {
      if (i != index && csStrCompare (row->fields[i], name, icase) == 0)
        {
          return csFail (csv, CS_EDUPLICATE, line,
                         "duplicate column name '%s'", name);
        }
    }

  return CS_OK;
}

/* Appends a validated row to the document; takes ownership only on
 * success. */
static CSStatus
csAppend (CSCsv *csv, CSRow *row, CSSize line)
{
  if (csv->count && row->count != csColumnCount (csv))
    {
      return csFail (csv, line ? CS_ESYNTAX : CS_EARG, line,
                     "expected %lu fields, got %lu",
                     (CSULong)csColumnCount (csv), (CSULong)row->count);
    }

  if (!csv->count && (csv->flags & CS_FHEADER))
    {
      for (CSSize i = 0; i < row->count; ++i)
        {
          CSStatus status = csHeaderCheck (csv, row, i, row->fields[i], line);
          if (status != CS_OK)
            {
              return status;
            }
        }
    }

  if (!csGrow ((CSVoid **)&csv->rows, &csv->capacity, csv->count + 1,
               sizeof (*csv->rows)))
    {
      return csFailMem (csv);
    }

  csv->rows[csv->count++] = row;

  return CS_OK;
}

static CSBool
csEol (CSChar c)
{
  return c == '\r' || c == '\n';
}

/* Parses one field, compacting it in place with the writer always behind
 * the reader. Consumes the separator after the field and reports it in
 * end: the delimiter, a line break or NUL at the end of the text. */
static CSStatus
csParseField (CSCsv *csv, CSChar **source, CSSize *line, CSChar *end)
{
  CSChar *src = *source;
  CSChar *dst = src;
  CSSize start = *line;
  CSChar delimiter = csv->delimiter;

  if (*src == '"')
    {
      ++src;
      for (;;)
        {
          if (!*src)
            {
              return csFail (csv, CS_ESYNTAX, start,
                             "unterminated quoted field");
            }
          if (*src == '"')
            {
              ++src;
              if (*src != '"')
                {
                  break;
                }
            }

          if (csEol (*src))
            {
              CSChar ending = *src++;
              ++*line;
              *dst++ = (csv->flags & CS_FLF) ? '\n' : ending;
              if (ending == '\r' && *src == '\n')
                {
                  ++src;
                  if (!(csv->flags & CS_FLF))
                    {
                      *dst++ = '\n';
                    }
                }
            }
          else
            {
              *dst++ = *src++;
            }
        }

      if (*src && *src != delimiter && !csEol (*src))
        {
          return csFail (csv, CS_ESYNTAX, *line, "text after closing quote");
        }
    }
  else
    {
      while (*src && *src != delimiter && !csEol (*src))
        {
          if (*src == '"')
            {
              return csFail (csv, CS_ESYNTAX, *line,
                             "quote in unquoted field");
            }
          *dst++ = *src++;
        }
    }

  *end = *src;
  if (*src)
    {
      ++src;
      if (csEol (*end))
        {
          ++*line;
          if (*end == '\r' && *src == '\n')
            {
              ++src;
            }
        }
    }

  *dst = '\0';
  *source = src;

  return CS_OK;
}

static CSStatus
csParseRows (CSCsv *csv, CSChar *src)
{
  CSSize line = 1;
  while (*src)
    {
      // Blank lines separate nothing and are skipped.
      if (csEol (*src))
        {
          CSChar ending = *src++;
          if (ending == '\r' && *src == '\n')
            {
              ++src;
            }
          ++line;
          continue;
        }

      CSSize start = line;
      CSRow *row = csMemZalloc (sizeof (*row));
      if (!row)
        {
          return csFailMem (csv);
        }

      CSStatus status;
      CSChar end = '\0';
      do
        {
          CSChar *field = src;
          status = csParseField (csv, &src, &line, &end);
          if (status == CS_OK && !csFieldAdd (row, field))
            {
              status = csFailMem (csv);
            }
        }
      while (status == CS_OK && end == csv->delimiter);

      if (status == CS_OK)
        {
          status = csAppend (csv, row, start);
        }
      if (status != CS_OK)
        {
          csRowFree (row);
          return status;
        }
    }

  if ((csv->flags & CS_FHEADER) && !csv->count)
    {
      return csFail (csv, CS_ESYNTAX, 1, "missing CSV header");
    }

  return CS_OK;
}

/* Parses text the document owns, past a leading UTF-8 BOM. */
static CSStatus
csParseOwned (CSCsv *csv, CSChar *text)
{
  if (csStrCompareN (text, "\xef\xbb\xbf", 3) == 0)
    {
      text += 3;
    }

  return csParseRows (csv, text);
}

/* Collects everything a reader supplies into a new NUL-terminated buffer. */
static CSStatus
csReadAll (CSCsv *csv, CSReadFn read, CSVoid *user, CSChar **text,
           CSSize *length)
{
  CSChar *buffer = NULL;
  CSSize used = 0;
  CSSize capacity = 0;
  for (;;)
    {
      if (!csGrow ((CSVoid **)&buffer, &capacity, used + CS_READ_CHUNK + 1, 1))
        {
          csMemFree (buffer);
          return csFailMem (csv);
        }

      CSSize n = read (user, buffer + used, CS_READ_CHUNK);
      if (n == 0)
        {
          break;
        }
      // Covers CS_READ_ERROR as well as readers overrunning the buffer.
      if (n > CS_READ_CHUNK)
        {
          csMemFree (buffer);
          return csFail (csv, CS_EIO, 0, "cannot read CSV text");
        }
      used += n;
    }

  buffer[used] = '\0';
  *text = buffer;
  *length = used;

  return CS_OK;
}

/* Parses read text unless it holds a NUL byte, which would end it early. */
static CSStatus
csParseRead (CSCsv *csv, CSChar *text, CSSize length)
{
  if (csStrLen (text) != length)
    {
      return csFail (csv, CS_ESYNTAX, 0, "NUL byte in CSV text");
    }

  return csParseOwned (csv, text);
}

/* Passes text to the writer, failing on a short write. */
static CSBool
csEmit (CSWriteFn write, CSVoid *user, const CSChar *text, CSSize size)
{
  return size == 0 || write (user, text, size) == size;
}

/* Writes a field, quoting it when it holds the delimiter, a quote or a line
 * break, starts with a BOM, or is empty and alone in its row, and doubling
 * embedded quotes. */
static CSBool
csEmitField (const CSCsv *csv, CSWriteFn write, CSVoid *user,
             const CSChar *value, CSBool alone)
{
  const CSChar special[] = { csv->delimiter, '"', '\r', '\n', '\0' };
  if ((*value || !alone) && !csStrFindAny (value, special)
      && csStrCompareN (value, "\xef\xbb\xbf", 3) != 0)
    {
      return csEmit (write, user, value, csStrLen (value));
    }

  if (!csEmit (write, user, "\"", 1))
    {
      return CS_FALSE;
    }

  const CSChar *start = value;
  for (; *value; ++value)
    {
      if (*value == '"')
        {
          if (!csEmit (write, user, start, (CSSize)(value - start))
              || !csEmit (write, user, "\"\"", 2))
            {
              return CS_FALSE;
            }
          start = value + 1;
        }
    }

  return csEmit (write, user, start, (CSSize)(value - start))
         && csEmit (write, user, "\"", 1);
}

/* Writes a record and its line ending. */
static CSBool
csEmitRow (const CSCsv *csv, const CSChar *const fields[], CSSize count,
           CSWriteFn write, CSVoid *user)
{
  CSBool ok = CS_TRUE;
  for (CSSize j = 0; ok && j < count; ++j)
    {
      ok = (j == 0 || csEmit (write, user, &csv->delimiter, 1))
           && csEmitField (csv, write, user, fields[j], count == 1);
    }

  return ok && csEmit (write, user, csv->newline, csStrLen (csv->newline));
}

static CSSize
csReadFile (CSVoid *user, CSChar *data, CSSize size)
{
  CSFileReader *reader = user;
  CSSize n = csFileRead (reader->file, data, size);
  if (n == 0 && csFileFailed (reader->file))
    {
      reader->code = csSysError ();
      return CS_READ_ERROR;
    }

  return n;
}

static CSSize
csWriteFile (CSVoid *user, const CSChar *data, CSSize size)
{
  return csFileWrite (user, data, size);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Definitions - Public
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

CSCsv *
csInit (CSDword flags)
{
  if (flags & ~(CSDword)(CS_FHEADER | CS_FICASE | CS_FLF))
    {
      return NULL;
    }

  CSCsv *csv = csMemZalloc (sizeof (CSCsv));
  if (csv)
    {
      csv->flags = flags;
      csv->delimiter = ',';
      csv->newline = "\r\n";
    }

  return csv;
}

void
csDestroy (CSCsv *csv)
{
  if (!csv)
    {
      return;
    }

  for (CSSize i = 0; i < csv->count; ++i)
    {
      csRowFree (csv->rows[i]);
    }

  csMemFree (csv->rows);
  csMemFree (csv);
}

const CSError *
csError (const CSCsv *csv)
{
  return csv ? &csv->error : NULL;
}

const CSChar *
csStatusText (CSStatus status)
{
  switch (status)
    {
    case CS_OK:
      return "Success";
    case CS_EARG:
      return "Invalid argument";
    case CS_EMEM:
      return "Out of memory";
    case CS_EIO:
      return "Input/output error";
    case CS_ESYNTAX:
      return "Syntax error";
    case CS_EDUPLICATE:
      return "Duplicate column name";
    }

  return "Unknown error";
}

CSStatus
csParse (CSCsv *csv, const CSChar *text)
{
  if (!csv)
    {
      return CS_EARG;
    }

  csSucceed (csv);
  if (!text)
    {
      return csFail (csv, CS_EARG, 0, "no text to parse");
    }

  CSChar *copy = csStrDup (text, csStrLen (text));
  if (!copy)
    {
      return csFailMem (csv);
    }

  CSStatus status = csParseOwned (csv, copy);
  csMemFree (copy);

  return status;
}

CSStatus
csLoad (CSCsv *csv, const CSChar *path)
{
  if (!csv)
    {
      return CS_EARG;
    }

  csSucceed (csv);
  if (!path)
    {
      return csFail (csv, CS_EARG, 0, "no file to load");
    }

  CSFileReader reader = { csFileOpen (path, CS_FALSE), 0 };
  if (!reader.file)
    {
      return csFailSys (csv, path, csSysError ());
    }

  // The file is closed before parsing, so that a failing close leaves the
  // document unchanged.
  CSChar *text = NULL;
  CSSize length = 0;
  CSStatus status = csReadAll (csv, csReadFile, &reader, &text, &length);
  if (status == CS_EIO)
    {
      status = csFailSys (csv, path, reader.code);
    }

  if (!csFileClose (reader.file) && status == CS_OK)
    {
      status = csFailSys (csv, path, csSysError ());
    }

  if (status == CS_OK)
    {
      status = csParseRead (csv, text, length);
    }

  csMemFree (text);

  return status;
}

CSStatus
csRead (CSCsv *csv, CSReadFn read, CSVoid *user)
{
  if (!csv)
    {
      return CS_EARG;
    }

  csSucceed (csv);
  if (!read)
    {
      return csFail (csv, CS_EARG, 0, "no reader");
    }

  CSChar *text = NULL;
  CSSize length = 0;
  CSStatus status = csReadAll (csv, read, user, &text, &length);
  if (status == CS_OK)
    {
      status = csParseRead (csv, text, length);
    }

  csMemFree (text);

  return status;
}

CSStatus
csWrite (CSCsv *csv, CSWriteFn write, CSVoid *user)
{
  if (!csv)
    {
      return CS_EARG;
    }

  csSucceed (csv);
  if (!write)
    {
      return csFail (csv, CS_EARG, 0, "no writer");
    }

  for (CSSize i = 0; i < csv->count; ++i)
    {
      const CSRow *row = csv->rows[i];
      if (!csEmitRow (csv, (const CSChar *const *)row->fields, row->count,
                      write, user))
        {
          return csFail (csv, CS_EIO, 0, "cannot write CSV text");
        }
    }

  return CS_OK;
}

CSStatus
csWriteRow (CSCsv *csv, const CSChar *const fields[], CSSize count,
            CSWriteFn write, CSVoid *user)
{
  if (!csv)
    {
      return CS_EARG;
    }

  csSucceed (csv);
  if (!write)
    {
      return csFail (csv, CS_EARG, 0, "no writer");
    }
  if (!fields || !count)
    {
      return csFail (csv, CS_EARG, 0, "invalid row");
    }
  for (CSSize i = 0; i < count; ++i)
    {
      if (!fields[i])
        {
          return csFail (csv, CS_EARG, 0, "NULL field in row");
        }
    }

  if (!csEmitRow (csv, fields, count, write, user))
    {
      return csFail (csv, CS_EIO, 0, "cannot write CSV text");
    }

  return CS_OK;
}

CSStatus
csSave (CSCsv *csv, const CSChar *path)
{
  if (!csv)
    {
      return CS_EARG;
    }

  csSucceed (csv);
  if (!path)
    {
      return csFail (csv, CS_EARG, 0, "no file to save");
    }

  CSFile *file = csFileOpen (path, CS_TRUE);
  if (!file)
    {
      return csFailSys (csv, path, csSysError ());
    }

  CSStatus status = csWrite (csv, csWriteFile, file);
  CSInt code = status == CS_EIO ? csSysError () : 0;
  if (!csFileClose (file) && status == CS_OK)
    {
      status = CS_EIO;
      code = csSysError ();
    }

  return status == CS_EIO ? csFailSys (csv, path, code) : status;
}

CSStatus
csSetDelimiter (CSCsv *csv, CSChar delimiter)
{
  if (!csv)
    {
      return CS_EARG;
    }

  csSucceed (csv);
  if (delimiter == '\0' || delimiter == '"' || csEol (delimiter))
    {
      return csFail (csv, CS_EARG, 0, "invalid delimiter");
    }

  csv->delimiter = delimiter;

  return CS_OK;
}

CSStatus
csSetNewline (CSCsv *csv, const CSChar *newline)
{
  if (!csv)
    {
      return CS_EARG;
    }

  csSucceed (csv);
  if (!newline
      || (csStrCompare (newline, "\r\n", CS_FALSE) != 0
          && csStrCompare (newline, "\n", CS_FALSE) != 0))
    {
      return csFail (csv, CS_EARG, 0, "invalid newline");
    }

  // Both are literals, so the document never owns a copy.
  csv->newline = newline[0] == '\r' ? "\r\n" : "\n";

  return CS_OK;
}

const CSChar *
csGet (const CSCsv *csv, CSSize row, CSSize column)
{
  return csRowValue (csRowAt (csv, row), column);
}

CSStatus
csSet (CSCsv *csv, CSSize row, CSSize column, const CSChar *value)
{
  if (!csv)
    {
      return CS_EARG;
    }

  csSucceed (csv);
  if (!value || row >= csv->count || column >= csColumnCount (csv))
    {
      return csFail (csv, CS_EARG, 0, "invalid cell or value");
    }

  CSRow *target = csv->rows[row];
  if (row == 0 && (csv->flags & CS_FHEADER))
    {
      CSStatus status = csHeaderCheck (csv, target, column, value, 0);
      if (status != CS_OK)
        {
          return status;
        }
    }

  CSChar *copy = csStrDup (value, csStrLen (value));
  if (!copy)
    {
      return csFailMem (csv);
    }

  csMemFree (target->fields[column]);
  target->fields[column] = copy;

  return CS_OK;
}

CSStatus
csRowAdd (CSCsv *csv, const CSChar *const fields[], CSSize count)
{
  return csRowInsert (csv, csv ? csv->count : 0, fields, count);
}

CSStatus
csRowInsert (CSCsv *csv, CSSize index, const CSChar *const fields[],
             CSSize count)
{
  if (!csv)
    {
      return CS_EARG;
    }

  csSucceed (csv);
  if (!fields || !count || count > (CSSize)-1 / sizeof (*fields))
    {
      return csFail (csv, CS_EARG, 0, "invalid row");
    }
  if (index > csv->count)
    {
      return csFail (csv, CS_EARG, 0, "invalid row position");
    }
  if (index == 0 && csv->count && (csv->flags & CS_FHEADER))
    {
      return csFail (csv, CS_EARG, 0, "the header stays the first row");
    }

  CSRow *row = csMemZalloc (sizeof (CSRow));
  if (!row)
    {
      return csFailMem (csv);
    }

  CSStatus status = CS_OK;
  for (CSSize i = 0; i < count && status == CS_OK; ++i)
    {
      if (!fields[i])
        {
          status = csFail (csv, CS_EARG, 0, "NULL field in row");
        }
      else if (!csFieldAdd (row, fields[i]))
        {
          status = csFailMem (csv);
        }
    }

  if (status == CS_OK)
    {
      status = csAppend (csv, row, 0);
    }
  if (status != CS_OK)
    {
      csRowFree (row);
      return status;
    }

  // Appended and checked, the row moves up to its place.
  for (CSSize i = csv->count - 1; i > index; --i)
    {
      csv->rows[i] = csv->rows[i - 1];
    }
  csv->rows[index] = row;

  return CS_OK;
}

CSBool
csRowRemove (CSCsv *csv, CSSize index)
{
  return csRowRemoveMany (csv, &index, 1);
}

CSBool
csRowRemoveMany (CSCsv *csv, const CSSize indexes[], CSSize count)
{
  if (!csv || (count && !indexes))
    {
      return CS_FALSE;
    }

  // Every position is checked first, so that a refusal changes nothing.
  for (CSSize i = 0; i < count; ++i)
    {
      if (indexes[i] >= csv->count || (i > 0 && indexes[i] <= indexes[i - 1]))
        {
          return CS_FALSE;
        }
    }
  if (count && indexes[0] == 0 && count < csv->count
      && (csv->flags & CS_FHEADER))
    {
      return CS_FALSE;
    }

  CSSize kept = 0;
  CSSize next = 0;
  for (CSSize i = 0; i < csv->count; ++i)
    {
      if (next < count && indexes[next] == i)
        {
          csRowFree (csv->rows[i]);
          ++next;
        }
      else
        {
          csv->rows[kept++] = csv->rows[i];
        }
    }
  csv->count = kept;

  return CS_TRUE;
}

CSSize
csRowCount (const CSCsv *csv)
{
  return csv ? csv->count : 0;
}

const CSRow *
csRowAt (const CSCsv *csv, CSSize index)
{
  return csv && index < csv->count ? csv->rows[index] : NULL;
}

CSSize
csRowSize (const CSRow *row)
{
  return row ? row->count : 0;
}

const CSChar *
csRowValue (const CSRow *row, CSSize index)
{
  return row && index < row->count ? row->fields[index] : NULL;
}

CSSize
csColumnCount (const CSCsv *csv)
{
  return csRowSize (csRowAt (csv, 0));
}

const CSChar *
csColumnName (const CSCsv *csv, CSSize index)
{
  return csv && (csv->flags & CS_FHEADER) ? csGet (csv, 0, index) : NULL;
}

CSSize
csColumnFind (const CSCsv *csv, const CSChar *name)
{
  if (!csv || !name || !(csv->flags & CS_FHEADER))
    {
      return CS_COLUMN_NONE;
    }

  CSBool icase = (csv->flags & CS_FICASE) != 0;
  for (CSSize i = 0; i < csColumnCount (csv); ++i)
    {
      if (csStrCompare (csColumnName (csv, i), name, icase) == 0)
        {
          return i;
        }
    }

  return CS_COLUMN_NONE;
}

CSStatus
csColumnAdd (CSCsv *csv, const CSChar *name, const CSChar *value)
{
  return csColumnInsert (csv, csColumnCount (csv), name, value);
}

CSStatus
csColumnInsert (CSCsv *csv, CSSize index, const CSChar *name,
                const CSChar *value)
{
  if (!csv)
    {
      return CS_EARG;
    }

  csSucceed (csv);
  if (!(csv->flags & CS_FHEADER))
    {
      return csFail (csv, CS_EARG, 0, "columns are added by header name");
    }
  if (!name)
    {
      return csFail (csv, CS_EARG, 0, "no column name");
    }
  if (index > csColumnCount (csv))
    {
      return csFail (csv, CS_EARG, 0, "invalid column position");
    }
  if (!csv->count)
    {
      return csRowAdd (csv, &name, 1);
    }

  CSStatus status = csHeaderCheck (csv, csv->rows[0], CS_COLUMN_NONE, name, 0);
  if (status != CS_OK)
    {
      return status;
    }

  // Every allocation comes first, so that a failure changes nothing: spare
  // capacity in the rows is harmless, the new fields are released.
  CSChar **fields = csMemZalloc (csv->count * sizeof (*fields));
  if (!fields)
    {
      return csFailMem (csv);
    }
  const CSChar *fill = value ? value : "";
  CSSize length = csStrLen (fill);
  for (CSSize i = 0; status == CS_OK && i < csv->count; ++i)
    {
      CSRow *row = csv->rows[i];
      fields[i] = i == 0 ? csStrDup (name, csStrLen (name))
                         : csStrDup (fill, length);
      if (!fields[i]
          || !csGrow ((CSVoid **)&row->fields, &row->capacity, row->count + 1,
                      sizeof (*row->fields)))
        {
          status = csFailMem (csv);
        }
    }

  for (CSSize i = 0; i < csv->count; ++i)
    {
      if (status == CS_OK)
        {
          CSRow *row = csv->rows[i];
          for (CSSize j = row->count; j > index; --j)
            {
              row->fields[j] = row->fields[j - 1];
            }
          row->fields[index] = fields[i];
          ++row->count;
        }
      else
        {
          csMemFree (fields[i]);
        }
    }
  csMemFree (fields);

  return status;
}

CSBool
csColumnRemove (CSCsv *csv, CSSize index)
{
  if (!csv || index >= csColumnCount (csv))
    {
      return CS_FALSE;
    }

  // Rows without fields would break the width every row shares.
  if (csColumnCount (csv) == 1)
    {
      while (csv->count > 0)
        {
          csRowFree (csv->rows[--csv->count]);
        }
      return CS_TRUE;
    }

  for (CSSize i = 0; i < csv->count; ++i)
    {
      CSRow *row = csv->rows[i];
      csMemFree (row->fields[index]);
      for (CSSize j = index + 1; j < row->count; ++j)
        {
          row->fields[j - 1] = row->fields[j];
        }
      --row->count;
    }

  return CS_TRUE;
}
