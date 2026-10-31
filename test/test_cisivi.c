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

#include "cisivi.h"
#include "cstest.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Helpers
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

typedef struct CSBuffer
{
  CSChar data[65536];
  CSSize length;
  CSSize limit;
} CSBuffer;

static CSSize
writeBuffer (CSVoid *user, const CSChar *data, CSSize size)
{
  CSBuffer *buffer = user;
  CSSize available = buffer->limit - buffer->length;
  CSSize n = size < available ? size : available;
  memcpy (buffer->data + buffer->length, data, n);
  buffer->length += n;
  buffer->data[buffer->length] = '\0';
  return n;
}

/* Supplies text in pieces of at most step bytes, or fails after limit. */
typedef struct Source
{
  const CSChar *data;
  CSSize length;
  CSSize offset;
  CSSize step;
  CSSize limit;   /* Fail once this many bytes were supplied; 0 for never. */
  CSBool overrun; /* Claim more bytes than asked for. */
  CSInt calls;
} Source;

static CSSize
readSource (CSVoid *user, CSChar *data, CSSize size)
{
  Source *source = user;
  ++source->calls;
  if (source->overrun)
    {
      return size + 1;
    }
  if (source->limit && source->offset >= source->limit)
    {
      return CS_READ_ERROR;
    }

  CSSize n = source->length - source->offset;
  n = n < size ? n : size;
  n = source->step && n > source->step ? source->step : n;
  memcpy (data, source->data + source->offset, n);
  source->offset += n;

  return n;
}

static Source
source (const CSChar *data, CSSize length)
{
  Source result = { data, length, 0, 0, 0, CS_FALSE, 0 };
  return result;
}

/* Serializes a document into a buffer, NUL-terminated. */
static const CSChar *
written (CSCsv *csv, CSBuffer *buffer)
{
  buffer->length = 0;
  buffer->limit = sizeof (buffer->data) - 1;
  buffer->data[0] = '\0';
  CS_ASSERT_EQ (csWrite (csv, writeBuffer, buffer), CS_OK);
  return buffer->data;
}

static void
compare (const CSCsv *a, const CSCsv *b)
{
  CS_ASSERT_EQ (csRowCount (a), csRowCount (b));
  CS_ASSERT_EQ (csColumnCount (a), csColumnCount (b));
  for (CSSize i = 0; i < csRowCount (a); ++i)
    {
      for (CSSize j = 0; j < csColumnCount (a); ++j)
        {
          CS_ASSERT_STR_EQ (csGet (a, i, j), csGet (b, i, j));
        }
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Tests
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static void
testParse (void)
{
  CSCsv *csv = csInit (CS_FHEADER);
  CS_ASSERT (csv != NULL);
  CS_ASSERT_EQ (csParse (csv, "\xef\xbb\xbf"
                              "extra,name,id\r\n"
                              "\"x,\"\"y\"\"\",\"line\r\nbreak\",abc\r\n"
                              "\r\n, plain ,"),
                CS_OK);
  CS_ASSERT_EQ (csRowCount (csv), 3);
  CS_ASSERT_EQ (csColumnCount (csv), 3);
  CS_ASSERT_EQ (csRowSize (csRowAt (csv, 1)), 3);
  CS_ASSERT_STR_EQ (csRowValue (csRowAt (csv, 1), 0), "x,\"y\"");
  CS_ASSERT_STR_EQ (csGet (csv, 1, 1), "line\r\nbreak");
  CS_ASSERT_STR_EQ (csGet (csv, 1, 2), "abc");
  CS_ASSERT_STR_EQ (csGet (csv, 2, 0), "");
  CS_ASSERT_STR_EQ (csGet (csv, 2, 1), " plain ");
  CS_ASSERT_STR_EQ (csGet (csv, 2, 2), "");
  CS_ASSERT_EQ (csColumnFind (csv, "id"), 2);
  CS_ASSERT_EQ (csColumnFind (csv, "ID"), CS_COLUMN_NONE);
  CS_ASSERT_EQ (csColumnFind (csv, "missing"), CS_COLUMN_NONE);
  CS_ASSERT_STR_EQ (csColumnName (csv, 1), "name");
  CS_ASSERT (csColumnName (csv, 3) == NULL);
  CS_ASSERT (csGet (csv, 0, 3) == NULL);
  CS_ASSERT (csGet (csv, 3, 0) == NULL);
  CS_ASSERT (csRowAt (csv, 3) == NULL);
  CS_ASSERT_EQ (csError (csv)->status, CS_OK);
  CS_ASSERT_EQ (csError (csv)->code, 0);
  CS_ASSERT_EQ (csError (csv)->line, 0);
  CS_ASSERT_STR_EQ (csError (csv)->message, "");
  csDestroy (csv);
}

static void
testFlagsAndEmpty (void)
{
  CSCsv *csv = csInit (CS_FNONE);
  CS_ASSERT_EQ (csParse (csv, "\xef\xbb\xbf\r\n\n\r"), CS_OK);
  CS_ASSERT_EQ (csRowCount (csv), 0);
  CS_ASSERT_EQ (csColumnCount (csv), 0);
  CS_ASSERT_EQ (csParse (csv, "\"\"\n\"a\rb\nc\r\nd\"\r"), CS_OK);
  CS_ASSERT_EQ (csRowCount (csv), 2);
  CS_ASSERT_STR_EQ (csGet (csv, 0, 0), "");
  CS_ASSERT_STR_EQ (csGet (csv, 1, 0), "a\rb\nc\r\nd");
  CS_ASSERT (csColumnName (csv, 0) == NULL);
  CS_ASSERT_EQ (csColumnFind (csv, ""), CS_COLUMN_NONE);
  csDestroy (csv);

  csv = csInit (CS_FHEADER | CS_FICASE | CS_FLF);
  CS_ASSERT_EQ (csParse (csv, "ID,Name\r1,\"a\rb\nc\r\nd\""), CS_OK);
  CS_ASSERT_EQ (csColumnFind (csv, "name"), 1);
  CS_ASSERT_STR_EQ (csColumnName (csv, 1), "Name");
  CS_ASSERT_STR_EQ (csGet (csv, 1, 1), "a\nb\nc\nd");
  CS_ASSERT_EQ (csSet (csv, 0, 1, "id"), CS_EDUPLICATE);
  CS_ASSERT_EQ (csSet (csv, 0, 1, "NAME"), CS_OK);
  CS_ASSERT_EQ (csSet (csv, 1, 1, "\r\n"), CS_OK);
  CS_ASSERT_STR_EQ (csGet (csv, 1, 1), "\r\n");
  csDestroy (csv);

  csv = csInit (CS_FHEADER | CS_FICASE);
  CS_ASSERT_EQ (csParse (csv, "ID,id"), CS_EDUPLICATE);
  CS_ASSERT_EQ (csRowCount (csv), 0);
  csDestroy (csv);
  csv = csInit (CS_FHEADER);
  CS_ASSERT_EQ (csParse (csv, "ID,id"), CS_OK);
  CS_ASSERT_EQ (csRowCount (csv), 1);
  csDestroy (csv);
}

static void
testMalformed (void)
{
  static const struct
  {
    const CSChar *text;
    CSStatus status;
    CSSize line;
    CSSize rows;
    const CSChar *message;
  } cases[] = {
    { "", CS_ESYNTAX, 1, 0, "missing CSV header" },
    { "\r\n\r\n", CS_ESYNTAX, 1, 0, "missing CSV header" },
    { "a,b\n1\n", CS_ESYNTAX, 2, 1, "expected 2 fields, got 1" },
    { "a,b\r\n1,2,3", CS_ESYNTAX, 2, 1, "expected 2 fields, got 3" },
    { "a,b\n\"open,2\n", CS_ESYNTAX, 2, 1, "unterminated quoted field" },
    { "a,b\n\"x\"y,2", CS_ESYNTAX, 2, 1, "text after closing quote" },
    { "a,b\n\"x\" ,2", CS_ESYNTAX, 2, 1, "text after closing quote" },
    { "a,b\nxx\"y,2", CS_ESYNTAX, 2, 1, "quote in unquoted field" },
    { "a,a\n1,2", CS_EDUPLICATE, 1, 0, "duplicate column name 'a'" },
    { "a,,b", CS_ESYNTAX, 1, 0, "empty column name" },
    { "\"\"", CS_ESYNTAX, 1, 0, "empty column name" },
    { "a,b\r\n\"x\r\ny\",z\r\n\r\nshort", CS_ESYNTAX, 5, 2,
      "expected 2 fields, got 1" },
    { "a,b\n\"x\ny\"z,2", CS_ESYNTAX, 3, 1, "text after closing quote" },
    { "a,b\r\"x\ry\",2\rshort", CS_ESYNTAX, 4, 2, "expected 2 fields, got 1" },
  };
  for (CSSize i = 0; i < CS_COUNTOF (cases); ++i)
    {
      CSCsv *csv = csInit (CS_FHEADER);
      CS_ASSERT_EQ (csParse (csv, cases[i].text), cases[i].status);
      CS_ASSERT_EQ (csError (csv)->status, cases[i].status);
      CS_ASSERT_EQ (csError (csv)->line, cases[i].line);
      CS_ASSERT_EQ (csError (csv)->code, 0);
      CSChar message[CS_ERROR_MAX];
      snprintf (message, sizeof (message), "line %zu: %s", cases[i].line,
                cases[i].message);
      CS_ASSERT_STR_EQ (csError (csv)->message, message);
      CS_ASSERT_EQ (csRowCount (csv), cases[i].rows);
      csDestroy (csv);
    }
}

static void
testAppendAndEdit (void)
{
  CSCsv *csv = csInit (CS_FHEADER);
  CSChar name[] = "name";
  const CSChar *header[] = { "id", name };
  const CSChar *row[] = { "1", "first" };
  CS_ASSERT_EQ (csRowAdd (csv, header, 2), CS_OK);
  name[0] = 'X';
  CS_ASSERT_STR_EQ (csColumnName (csv, 1), "name");
  CS_ASSERT_EQ (csRowAdd (csv, row, 2), CS_OK);
  const CSRow *saved = csRowAt (csv, 1);
  const CSChar *value = csGet (csv, 1, 1);
  for (CSSize i = 0; i < 40; ++i)
    {
      CS_ASSERT_EQ (csParse (csv, "2,next\n"), CS_OK);
    }
  CS_ASSERT (csRowAt (csv, 1) == saved);
  CS_ASSERT (csGet (csv, 1, 1) == value);
  CS_ASSERT_EQ (csRowCount (csv), 42);
  CS_ASSERT_EQ (csParse (csv, "3,last\nshort"), CS_ESYNTAX);
  CS_ASSERT_EQ (csRowCount (csv), 43);
  CS_ASSERT_EQ (csError (csv)->line, 2);
  CS_ASSERT_EQ (csSet (csv, 1, 1, csGet (csv, 1, 1)), CS_OK);
  CS_ASSERT_EQ (csError (csv)->line, 0);
  CS_ASSERT_STR_EQ (csError (csv)->message, "");
  CS_ASSERT_STR_EQ (csRowValue (saved, 1), "first");
  CS_ASSERT_EQ (csSet (csv, 1, 1, "new, \"value\""), CS_OK);
  CS_ASSERT_STR_EQ (csRowValue (saved, 1), "new, \"value\"");
  CS_ASSERT_EQ (csSet (csv, 0, 1, "id"), CS_EDUPLICATE);
  CS_ASSERT_EQ (csSet (csv, 0, 1, ""), CS_EARG);
  CS_ASSERT_STR_EQ (csColumnName (csv, 1), "name");
  CS_ASSERT_EQ (csSet (csv, 0, 1, "title"), CS_OK);
  CS_ASSERT_EQ (csColumnFind (csv, "name"), CS_COLUMN_NONE);
  CS_ASSERT_EQ (csColumnFind (csv, "title"), 1);
  CS_ASSERT_EQ (csRowAdd (csv, row, 1), CS_EARG);
  const CSChar *bad[] = { "valid", NULL };
  CS_ASSERT_EQ (csRowAdd (csv, bad, 2), CS_EARG);
  CS_ASSERT_EQ (csRowCount (csv), 43);
  CS_ASSERT_EQ (csParse (csv, ""), CS_OK);
  csDestroy (csv);
}

static void
testInsert (void)
{
  CSCsv *csv = csInit (CS_FHEADER);
  const CSChar *header[] = { "id", "name" };
  const CSChar *first[] = { "1", "first" };
  const CSChar *middle[] = { "2", "middle" };
  const CSChar *last[] = { "3", "last" };

  // Position 0 starts the header in an empty document, and only there.
  CS_ASSERT_EQ (csRowInsert (csv, 1, header, 2), CS_EARG);
  CS_ASSERT_EQ (csRowInsert (csv, 0, header, 2), CS_OK);
  CS_ASSERT_EQ (csRowInsert (csv, 0, first, 2), CS_EARG);
  CS_ASSERT_STR_EQ (csError (csv)->message, "the header stays the first row");
  CS_ASSERT_EQ (csRowInsert (csv, 1, last, 2), CS_OK);
  CS_ASSERT_EQ (csRowInsert (csv, 1, first, 2), CS_OK);
  const CSRow *saved = csRowAt (csv, 2);
  CS_ASSERT_EQ (csRowInsert (csv, 2, middle, 2), CS_OK);
  CS_ASSERT (csRowAt (csv, 3) == saved);
  CS_ASSERT_EQ (csRowCount (csv), 4);
  for (CSSize i = 1; i < 4; ++i)
    {
      CS_ASSERT_STR_EQ (csGet (csv, i, 0), i == 1 ? "1" : i == 2 ? "2" : "3");
    }

  // Out of range, the wrong width or a NULL field add nothing.
  CS_ASSERT_EQ (csRowInsert (csv, 5, last, 2), CS_EARG);
  CS_ASSERT_EQ (csRowInsert (csv, 1, last, 1), CS_EARG);
  const CSChar *bad[] = { "valid", NULL };
  CS_ASSERT_EQ (csRowInsert (csv, 1, bad, 2), CS_EARG);
  CS_ASSERT_EQ (csRowInsert (csv, 1, NULL, 2), CS_EARG);
  CS_ASSERT_EQ (csRowCount (csv), 4);
  CS_ASSERT (csRowAt (csv, 3) == saved);

  // At the row count, inserting appends.
  CS_ASSERT_EQ (csRowInsert (csv, 4, last, 2), CS_OK);
  CS_ASSERT_STR_EQ (csGet (csv, 4, 1), "last");
  csDestroy (csv);

  // Without a header any position goes, the first included.
  csv = csInit (CS_FNONE);
  CS_ASSERT_EQ (csParse (csv, "b\n"), CS_OK);
  const CSChar *a[] = { "a" };
  CS_ASSERT_EQ (csRowInsert (csv, 0, a, 1), CS_OK);
  CS_ASSERT_STR_EQ (csGet (csv, 0, 0), "a");
  CS_ASSERT_STR_EQ (csGet (csv, 1, 0), "b");
  CS_ASSERT_EQ (csRowInsert (NULL, 0, a, 1), CS_EARG);
  csDestroy (csv);
}

static void
testWriting (void)
{
  CSCsv *csv = csInit (CS_FNONE);
  const CSChar *row[] = { "plain", "", "comma,", "\"quote\"", "\r\n", " " };
  CS_ASSERT_EQ (csRowAdd (csv, row, CS_COUNTOF (row)), CS_OK);
  CSBuffer buffer = { { 0 }, 0, sizeof (buffer.data) - 1 };
  CS_ASSERT_EQ (csWrite (csv, writeBuffer, &buffer), CS_OK);
  CS_ASSERT_STR_EQ (buffer.data,
                    "plain,,\"comma,\",\"\"\"quote\"\"\",\"\r\n\", \r\n");
  CSSize length = buffer.length;
  for (CSSize limit = 0; limit < length; ++limit)
    {
      buffer.length = 0;
      buffer.limit = limit;
      errno = EINVAL;
      CS_ASSERT_EQ (csWrite (csv, writeBuffer, &buffer), CS_EIO);
      CS_ASSERT_EQ (csError (csv)->code, 0);
      CS_ASSERT_EQ (buffer.length, limit);
    }
  buffer.length = 0;
  buffer.limit = length;
  CS_ASSERT_EQ (csWrite (csv, writeBuffer, &buffer), CS_OK);
  CS_ASSERT_EQ (csError (csv)->status, CS_OK);
  csDestroy (csv);

  csv = csInit (CS_FNONE);
  const CSChar *empty[] = { "" };
  CS_ASSERT_EQ (csRowAdd (csv, empty, 1), CS_OK);
  buffer.length = 0;
  CS_ASSERT_EQ (csWrite (csv, writeBuffer, &buffer), CS_OK);
  CS_ASSERT_STR_EQ (buffer.data, "\"\"\r\n");
  csDestroy (csv);

  // A literal BOM in the first cell must not become a file-level BOM.
  csv = csInit (CS_FNONE);
  const CSChar *bom[] = { "\xef\xbb\xbf" };
  CS_ASSERT_EQ (csRowAdd (csv, bom, 1), CS_OK);
  buffer.length = 0;
  CS_ASSERT_EQ (csWrite (csv, writeBuffer, &buffer), CS_OK);
  CS_ASSERT_STR_EQ (buffer.data, "\"\xef\xbb\xbf\"\r\n");
  CSCsv *copy = csInit (CS_FNONE);
  CS_ASSERT_EQ (csParse (copy, buffer.data), CS_OK);
  compare (csv, copy);
  csDestroy (copy);
  csDestroy (csv);
}

static void
testWriteRow (void)
{
  // Rows written alone match the document's own serialization, in its
  // settings, and leave it empty.
  CSCsv *csv = csInit (CS_FNONE);
  CS_ASSERT_EQ (csSetDelimiter (csv, ';'), CS_OK);
  CS_ASSERT_EQ (csSetNewline (csv, "\n"), CS_OK);
  const CSChar *row[] = { "plain", "semi;colon", "\"quote\"", "comma," };
  const CSChar *empty[] = { "" };
  CSBuffer buffer = { { 0 }, 0, sizeof (buffer.data) - 1 };
  CS_ASSERT_EQ (csWriteRow (csv, row, CS_COUNTOF (row), writeBuffer, &buffer),
                CS_OK);
  CS_ASSERT_EQ (csWriteRow (csv, empty, 1, writeBuffer, &buffer), CS_OK);
  CS_ASSERT_STR_EQ (buffer.data,
                    "plain;\"semi;colon\";\"\"\"quote\"\"\";comma,\n\"\"\n");
  CS_ASSERT_EQ (csRowCount (csv), 0);

  const CSChar *blank[] = { "", "", "", "" };
  CS_ASSERT_EQ (csRowAdd (csv, row, CS_COUNTOF (row)), CS_OK);
  CS_ASSERT_EQ (csRowAdd (csv, blank, CS_COUNTOF (blank)), CS_OK);
  CSBuffer whole = { { 0 }, 0, sizeof (whole.data) - 1 };
  CSBuffer rows = { { 0 }, 0, sizeof (rows.data) - 1 };
  CS_ASSERT_EQ (csWrite (csv, writeBuffer, &whole), CS_OK);
  for (CSSize i = 0; i < csRowCount (csv); ++i)
    {
      const CSChar *fields[4];
      for (CSSize j = 0; j < 4; ++j)
        {
          fields[j] = csGet (csv, i, j);
        }
      CS_ASSERT_EQ (csWriteRow (csv, fields, 4, writeBuffer, &rows), CS_OK);
    }
  CS_ASSERT_STR_EQ (rows.data, whole.data);

  // A short write fails, and so does an invalid row.
  buffer.length = 0;
  buffer.limit = 3;
  CS_ASSERT_EQ (csWriteRow (csv, row, CS_COUNTOF (row), writeBuffer, &buffer),
                CS_EIO);
  CS_ASSERT_EQ (csError (csv)->status, CS_EIO);
  const CSChar *hole[] = { "a", NULL };
  CS_ASSERT_EQ (csWriteRow (csv, hole, 2, writeBuffer, &buffer), CS_EARG);
  CS_ASSERT_EQ (csWriteRow (csv, row, 0, writeBuffer, &buffer), CS_EARG);
  CS_ASSERT_EQ (csWriteRow (csv, NULL, 1, writeBuffer, &buffer), CS_EARG);
  CS_ASSERT_EQ (csWriteRow (csv, row, 1, NULL, NULL), CS_EARG);
  CS_ASSERT_EQ (csWriteRow (NULL, row, 1, writeBuffer, &buffer), CS_EARG);
  csDestroy (csv);
}

static void
testRoundTrips (void)
{
  static const CSChar *const values[]
      = { "",         "x",  ",",     "\"",           "\"\"",
          "a\"b",     "\n", "\r",    "\r\n",         "\n\r",
          " spaces ", "\t", "český", "a,\"b\"\r\nc", "\xef\xbb\xbf" };
  CSCsv *csv = csInit (CS_FNONE);
  for (CSSize i = 0; i < CS_COUNTOF (values); ++i)
    {
      for (CSSize j = 0; j < CS_COUNTOF (values); ++j)
        {
          const CSChar *row[] = { values[i], values[j] };
          CS_ASSERT_EQ (csRowAdd (csv, row, 2), CS_OK);
        }
    }
  CSBuffer buffer = { { 0 }, 0, sizeof (buffer.data) - 1 };
  CS_ASSERT_EQ (csWrite (csv, writeBuffer, &buffer), CS_OK);
  CSCsv *copy = csInit (CS_FNONE);
  CS_ASSERT_EQ (csParse (copy, buffer.data), CS_OK);
  compare (csv, copy);
  csDestroy (copy);
  csDestroy (csv);
}

static void
testFiles (void)
{
  // Written into the current directory, as every platform has one.
  const CSChar *path = "test_cisivi_file.csv";
  CSCsv *csv = csInit (CS_FHEADER);
  CSCsv *copy = csInit (CS_FHEADER);
  CS_ASSERT_EQ (csParse (csv, "id,name\n1,\"a, b\"\n"), CS_OK);
  CSChar large[9000];
  memset (large, 'x', sizeof (large) - 1);
  large[sizeof (large) - 1] = '\0';
  const CSChar *row[] = { "2", large };
  CS_ASSERT_EQ (csRowAdd (csv, row, 2), CS_OK);
  CS_ASSERT_EQ (csSave (csv, path), CS_OK);
  CS_ASSERT_EQ (csLoad (copy, path), CS_OK);
  compare (csv, copy);
  csDestroy (copy);

  // Saving replaces the entire file, including a formerly longer value.
  CS_ASSERT_EQ (csSet (csv, 2, 1, "short"), CS_OK);
  CS_ASSERT_EQ (csSave (csv, path), CS_OK);
  copy = csInit (CS_FHEADER);
  CS_ASSERT_EQ (csLoad (copy, path), CS_OK);
  compare (csv, copy);
  csDestroy (copy);

  CSChar badPath[128];
  snprintf (badPath, sizeof (badPath), "%s/child", path);
  CS_ASSERT_EQ (csSave (csv, badPath), CS_EIO);
  CS_ASSERT (csError (csv)->code != 0);
  CS_ASSERT (strstr (csError (csv)->message, badPath) != NULL);
  FILE *file = fopen (path, "wb");
  CS_ASSERT (file != NULL);
  if (file)
    {
      const CSChar nul[] = "id,name\n1,ok\0hidden";
      CS_ASSERT_EQ (fwrite (nul, 1, sizeof (nul) - 1, file), sizeof (nul) - 1);
      CS_ASSERT_EQ (fclose (file), 0);
      copy = csInit (CS_FHEADER);
      CS_ASSERT_EQ (csLoad (copy, path), CS_ESYNTAX);
      CS_ASSERT_EQ (csRowCount (copy), 0);
      CS_ASSERT_STR_EQ (csError (copy)->message, "NUL byte in CSV text");
      csDestroy (copy);
    }
  remove (path);
  CS_ASSERT_EQ (csLoad (csv, path), CS_EIO);
  CS_ASSERT_EQ (csError (csv)->code, ENOENT);
  CS_ASSERT_EQ (csRowCount (csv), 3);
  CS_ASSERT_EQ (csParse (csv, ""), CS_OK);
  CS_ASSERT_EQ (csError (csv)->code, 0);
  csDestroy (csv);
}

static void
testRead (void)
{
  // Small pieces, a BOM and a quoted line break split across them.
  const CSChar text[] = "\xef\xbb\xbfship,captain\n"
                        "Enterprise,\"Kirk\r\nPicard\"\nVoyager,Janeway\n";
  Source in = source (text, sizeof (text) - 1);
  in.step = 3;
  CSCsv *csv = csInit (CS_FHEADER);
  CS_ASSERT_EQ (csRead (csv, readSource, &in), CS_OK);
  CS_ASSERT_EQ (csRowCount (csv), 3);
  CS_ASSERT_STR_EQ (csColumnName (csv, 0), "ship");
  CS_ASSERT_STR_EQ (csGet (csv, 1, 1), "Kirk\r\nPicard");
  CS_ASSERT_STR_EQ (csGet (csv, 2, 1), "Janeway");
  CS_ASSERT_EQ (csError (csv)->status, CS_OK);
  CS_ASSERT (in.calls > 10);

  // Appends like csParse(), with the same width.
  const CSChar more[] = "Defiant,Sisko\n";
  in = source (more, sizeof (more) - 1);
  CS_ASSERT_EQ (csRead (csv, readSource, &in), CS_OK);
  CS_ASSERT_EQ (csRowCount (csv), 4);
  in = source ("Discovery\n", 10);
  CS_ASSERT_EQ (csRead (csv, readSource, &in), CS_ESYNTAX);
  CS_ASSERT_EQ (csError (csv)->line, 1);
  csDestroy (csv);

  // Round trip through csWrite(), larger than one read chunk.
  static CSChar large[9000];
  memset (large, 'x', sizeof (large) - 1);
  csv = csInit (CS_FNONE);
  const CSChar *row[] = { large, "a,\"b\"\nc" };
  CS_ASSERT_EQ (csRowAdd (csv, row, 2), CS_OK);
  static CSBuffer buffer;
  written (csv, &buffer);
  csDestroy (csv);
  in = source (buffer.data, buffer.length);
  csv = csInit (CS_FNONE);
  CS_ASSERT_EQ (csRead (csv, readSource, &in), CS_OK);
  CS_ASSERT_STR_EQ (csGet (csv, 0, 0), large);
  CS_ASSERT_STR_EQ (csGet (csv, 0, 1), "a,\"b\"\nc");
  csDestroy (csv);

  // Empty input is an empty document, without a header too.
  in = source ("", 0);
  csv = csInit (CS_FNONE);
  CS_ASSERT_EQ (csRead (csv, readSource, &in), CS_OK);
  CS_ASSERT_EQ (csRowCount (csv), 0);
  CS_ASSERT_EQ (in.calls, 1);
  csDestroy (csv);
}

static void
testReadErrors (void)
{
  CSCsv *csv = csInit (CS_FNONE);

  // A failing reader leaves the document unchanged, even after good text.
  const CSChar text[] = "Enterprise,Kirk\nVoyager,Janeway\n";
  Source in = source (text, sizeof (text) - 1);
  in.step = 4;
  in.limit = 4;
  CS_ASSERT_EQ (csRead (csv, readSource, &in), CS_EIO);
  CS_ASSERT_EQ (csRowCount (csv), 0);
  CS_ASSERT_STR_EQ (csError (csv)->message, "cannot read CSV text");
  CS_ASSERT_EQ (csError (csv)->code, 0);

  in = source (text, sizeof (text) - 1);
  in.overrun = CS_TRUE;
  CS_ASSERT_EQ (csRead (csv, readSource, &in), CS_EIO);
  CS_ASSERT_EQ (csRowCount (csv), 0);

  // An embedded NUL byte is an error, not the end of the text.
  const CSChar nul[] = "Enterprise,Kirk\0Borg,cube\n";
  in = source (nul, sizeof (nul) - 1);
  CS_ASSERT_EQ (csRead (csv, readSource, &in), CS_ESYNTAX);
  CS_ASSERT_EQ (csRowCount (csv), 0);
  CS_ASSERT_STR_EQ (csError (csv)->message, "NUL byte in CSV text");

  in = source ("\"unterminated\n", 14);
  CS_ASSERT_EQ (csRead (csv, readSource, &in), CS_ESYNTAX);
  CS_ASSERT_EQ (csError (csv)->line, 1);
  CS_ASSERT_EQ (csRead (csv, NULL, NULL), CS_EARG);
  CS_ASSERT_EQ (csRead (NULL, readSource, &in), CS_EARG);
  csDestroy (csv);
}

static void
testDelimiter (void)
{
  CSCsv *csv = csInit (CS_FHEADER);
  CS_ASSERT_EQ (csSetDelimiter (csv, ';'), CS_OK);
  CS_ASSERT_EQ (csParse (csv, "ship;crew,size\nEnterprise;\"430; or so\"\n"),
                CS_OK);
  CS_ASSERT_EQ (csColumnCount (csv), 2);
  CS_ASSERT_STR_EQ (csColumnName (csv, 1), "crew,size");
  CS_ASSERT_STR_EQ (csGet (csv, 1, 1), "430; or so");

  // Written with the same delimiter: commas need no quotes, semicolons do.
  CSBuffer buffer;
  CS_ASSERT_STR_EQ (written (csv, &buffer),
                    "ship;crew,size\r\nEnterprise;\"430; or so\"\r\n");
  CS_ASSERT_EQ (csSetDelimiter (csv, '\t'), CS_OK);
  CS_ASSERT_STR_EQ (written (csv, &buffer),
                    "ship\tcrew,size\r\nEnterprise\t430; or so\r\n");
  CS_ASSERT_EQ (csSetDelimiter (csv, '"'), CS_EARG);
  CS_ASSERT_STR_EQ (csError (csv)->message, "invalid delimiter");
  CS_ASSERT_EQ (csSetDelimiter (csv, '\n'), CS_EARG);
  CS_ASSERT_EQ (csSetDelimiter (csv, '\r'), CS_EARG);
  CS_ASSERT_EQ (csSetDelimiter (csv, '\0'), CS_EARG);
  CS_ASSERT_EQ (csSetDelimiter (NULL, ';'), CS_EARG);

  // The tab still separates, a comma is part of a value.
  CS_ASSERT_EQ (csParse (csv, "Voyager\tJaneway, Kathryn\n"), CS_OK);
  CS_ASSERT_STR_EQ (csGet (csv, 2, 1), "Janeway, Kathryn");
  CS_ASSERT_EQ (csParse (csv, "Defiant\t\"Sisko\"x\n"), CS_ESYNTAX);
  csDestroy (csv);
}

static void
testNewline (void)
{
  CSCsv *csv = csInit (CS_FNONE);
  const CSChar *row[] = { "warp", "9" };
  CS_ASSERT_EQ (csRowAdd (csv, row, 2), CS_OK);
  CS_ASSERT_EQ (csSetNewline (csv, "\n"), CS_OK);
  CSBuffer buffer;
  CS_ASSERT_STR_EQ (written (csv, &buffer), "warp,9\n");
  CS_ASSERT_EQ (csSetNewline (csv, "\r\n"), CS_OK);
  CS_ASSERT_STR_EQ (written (csv, &buffer), "warp,9\r\n");
  CS_ASSERT_EQ (csSetNewline (csv, "\r"), CS_EARG);
  CS_ASSERT_STR_EQ (csError (csv)->message, "invalid newline");
  CS_ASSERT_EQ (csSetNewline (csv, NULL), CS_EARG);
  CS_ASSERT_EQ (csSetNewline (NULL, "\n"), CS_EARG);
  CS_ASSERT_STR_EQ (written (csv, &buffer), "warp,9\r\n");
  csDestroy (csv);
}

static void
testRemove (void)
{
  CSCsv *csv = csInit (CS_FHEADER);
  CS_ASSERT_EQ (csParse (csv, "ship,captain\nEnterprise,Kirk\n"
                              "Voyager,Janeway\nDefiant,Sisko\n"),
                CS_OK);
  const CSRow *last = csRowAt (csv, 3);
  CS_ASSERT (csRowRemove (csv, 2));
  CS_ASSERT_EQ (csRowCount (csv), 3);
  CS_ASSERT (csRowAt (csv, 2) == last);
  CS_ASSERT (!csRowRemove (csv, 3));
  CS_ASSERT (!csRowRemove (csv, 0));
  CS_ASSERT_STR_EQ (csColumnName (csv, 0), "ship");
  CS_ASSERT (csRowRemove (csv, 1));
  CS_ASSERT (csRowRemove (csv, 1));

  // The header goes last, and then the next row is a header again.
  CS_ASSERT (csRowRemove (csv, 0));
  CS_ASSERT_EQ (csRowCount (csv), 0);
  CS_ASSERT_EQ (csParse (csv, "id\n1\n"), CS_OK);
  CS_ASSERT_EQ (csColumnFind (csv, "id"), 0);
  csDestroy (csv);

  csv = csInit (CS_FNONE);
  CS_ASSERT_EQ (csParse (csv, "a\nb\n"), CS_OK);
  CS_ASSERT (csRowRemove (csv, 0));
  CS_ASSERT_STR_EQ (csGet (csv, 0, 0), "b");
  CS_ASSERT (!csRowRemove (NULL, 0));
  csDestroy (csv);
}

static void
testRemoveMany (void)
{
  CSCsv *csv = csInit (CS_FHEADER);
  CS_ASSERT_EQ (csParse (csv, "n\n1\n2\n3\n4\n5\n6\n"), CS_OK);
  const CSRow *four = csRowAt (csv, 4);

  // Out of order, repeated, out of range or the header alone: refused,
  // nothing changes.
  const CSSize unordered[] = { 3, 1 };
  const CSSize repeated[] = { 2, 2 };
  const CSSize beyond[] = { 1, 7 };
  const CSSize header[] = { 0, 1 };
  CS_ASSERT (!csRowRemoveMany (csv, unordered, 2));
  CS_ASSERT (!csRowRemoveMany (csv, repeated, 2));
  CS_ASSERT (!csRowRemoveMany (csv, beyond, 2));
  CS_ASSERT (!csRowRemoveMany (csv, header, 2));
  CS_ASSERT (!csRowRemoveMany (csv, NULL, 1));
  CS_ASSERT (!csRowRemoveMany (NULL, NULL, 0));
  CS_ASSERT_EQ (csRowCount (csv), 7);
  CS_ASSERT (csRowRemoveMany (csv, NULL, 0));
  CS_ASSERT_EQ (csRowCount (csv), 7);

  const CSSize some[] = { 1, 3, 5, 6 };
  CS_ASSERT (csRowRemoveMany (csv, some, CS_COUNTOF (some)));
  CS_ASSERT_EQ (csRowCount (csv), 3);
  CS_ASSERT_STR_EQ (csGet (csv, 0, 0), "n");
  CS_ASSERT_STR_EQ (csGet (csv, 1, 0), "2");
  CS_ASSERT_STR_EQ (csGet (csv, 2, 0), "4");
  CS_ASSERT (csRowAt (csv, 2) == four);

  // The header goes with every other row.
  const CSSize all[] = { 0, 1, 2 };
  CS_ASSERT (csRowRemoveMany (csv, all, CS_COUNTOF (all)));
  CS_ASSERT_EQ (csRowCount (csv), 0);
  csDestroy (csv);

  csv = csInit (CS_FNONE);
  CS_ASSERT_EQ (csParse (csv, "a\nb\nc\n"), CS_OK);
  const CSSize first[] = { 0, 2 };
  CS_ASSERT (csRowRemoveMany (csv, first, CS_COUNTOF (first)));
  CS_ASSERT_EQ (csRowCount (csv), 1);
  CS_ASSERT_STR_EQ (csGet (csv, 0, 0), "b");
  csDestroy (csv);
}

static void
testColumns (void)
{
  CSCsv *csv = csInit (CS_FHEADER | CS_FICASE);
  CS_ASSERT_EQ (csColumnAdd (csv, "ship", NULL), CS_OK);
  CS_ASSERT_EQ (csRowCount (csv), 1);
  CS_ASSERT_EQ (csParse (csv, "Enterprise\nVoyager\n"), CS_OK);
  CS_ASSERT_EQ (csColumnAdd (csv, "captain", NULL), CS_OK);
  CS_ASSERT_EQ (csColumnCount (csv), 2);
  CS_ASSERT_EQ (csColumnFind (csv, "Captain"), 1);
  CS_ASSERT_STR_EQ (csGet (csv, 2, 1), "");
  CS_ASSERT_EQ (csSet (csv, 2, 1, "Janeway"), CS_OK);
  CS_ASSERT_EQ (csColumnAdd (csv, "SHIP", NULL), CS_EDUPLICATE);
  CS_ASSERT_EQ (csColumnAdd (csv, "", NULL), CS_EARG);
  CS_ASSERT_EQ (csColumnAdd (csv, NULL, NULL), CS_EARG);
  CS_ASSERT_EQ (csColumnCount (csv), 2);

  // Later rows keep the new width.
  CS_ASSERT_EQ (csParse (csv, "Defiant\n"), CS_ESYNTAX);
  CS_ASSERT_EQ (csParse (csv, "Defiant,Sisko\n"), CS_OK);
  CSBuffer buffer;
  CS_ASSERT_STR_EQ (written (csv, &buffer),
                    "ship,captain\r\nEnterprise,\r\nVoyager,Janeway\r\n"
                    "Defiant,Sisko\r\n");

  CS_ASSERT (csColumnRemove (csv, 0));
  CS_ASSERT_EQ (csColumnCount (csv), 1);
  CS_ASSERT_STR_EQ (csColumnName (csv, 0), "captain");
  CS_ASSERT_STR_EQ (written (csv, &buffer),
                    "captain\r\n\"\"\r\nJaneway\r\nSisko\r\n");
  CS_ASSERT (!csColumnRemove (csv, 1));

  // Without columns no row is left.
  CS_ASSERT (csColumnRemove (csv, 0));
  CS_ASSERT_EQ (csRowCount (csv), 0);
  CS_ASSERT (!csColumnRemove (csv, 0));
  CS_ASSERT (!csColumnRemove (NULL, 0));
  csDestroy (csv);

  csv = csInit (CS_FNONE);
  CS_ASSERT_EQ (csColumnAdd (csv, "ship", NULL), CS_EARG);
  CS_ASSERT_EQ (csParse (csv, "a,b\n1,2\n"), CS_OK);
  CS_ASSERT (csColumnRemove (csv, 1));
  CS_ASSERT_STR_EQ (csGet (csv, 1, 0), "1");
  CS_ASSERT_EQ (csColumnAdd (NULL, "x", NULL), CS_EARG);
  CS_ASSERT_EQ (csColumnInsert (csv, 0, "ship", NULL), CS_EARG);
  csDestroy (csv);
}

static void
testColumnInsert (void)
{
  CSCsv *csv = csInit (CS_FHEADER);

  // An empty document takes position 0 only; the column starts the header.
  CS_ASSERT_EQ (csColumnInsert (csv, 1, "ship", NULL), CS_EARG);
  CS_ASSERT_EQ (csColumnInsert (csv, 0, "ship", "ignored"), CS_OK);
  CS_ASSERT_EQ (csRowCount (csv), 1);
  CS_ASSERT_EQ (csParse (csv, "Enterprise\nVoyager\n"), CS_OK);

  // The value fills every row but the header, as its own copy.
  CSChar fill[] = "Starfleet";
  CS_ASSERT_EQ (csColumnAdd (csv, "fleet", fill), CS_OK);
  fill[0] = 'X';
  CS_ASSERT_STR_EQ (csGet (csv, 1, 1), "Starfleet");
  CS_ASSERT_EQ (csSet (csv, 2, 1, "Federation"), CS_OK);
  CS_ASSERT_STR_EQ (csGet (csv, 1, 1), "Starfleet");

  // Inserted first, in the middle and last; the others move right.
  const CSRow *saved = csRowAt (csv, 1);
  CS_ASSERT_EQ (csColumnInsert (csv, 0, "id", "0"), CS_OK);
  CS_ASSERT_EQ (csColumnInsert (csv, 2, "captain", NULL), CS_OK);
  CS_ASSERT_EQ (csColumnInsert (csv, 4, "notes", "-"), CS_OK);
  CS_ASSERT (csRowAt (csv, 1) == saved);
  CS_ASSERT_EQ (csColumnFind (csv, "fleet"), 3);
  CSBuffer buffer;
  CS_ASSERT_STR_EQ (written (csv, &buffer), "id,ship,captain,fleet,notes\r\n"
                                            "0,Enterprise,,Starfleet,-\r\n"
                                            "0,Voyager,,Federation,-\r\n");

  // Out of range, duplicate or empty names change nothing.
  CS_ASSERT_EQ (csColumnInsert (csv, 6, "extra", NULL), CS_EARG);
  CS_ASSERT_STR_EQ (csError (csv)->message, "invalid column position");
  CS_ASSERT_EQ (csColumnInsert (csv, 1, "ship", NULL), CS_EDUPLICATE);
  CS_ASSERT_EQ (csColumnInsert (csv, 1, "", NULL), CS_EARG);
  CS_ASSERT_EQ (csColumnInsert (csv, 1, NULL, NULL), CS_EARG);
  CS_ASSERT_EQ (csColumnCount (csv), 5);

  // Later rows keep the new width.
  CS_ASSERT_EQ (csParse (csv, "1,Defiant,Sisko,Starfleet,-\n"), CS_OK);
  CS_ASSERT_EQ (csParse (csv, "Defiant\n"), CS_ESYNTAX);
  CS_ASSERT_EQ (csColumnInsert (NULL, 0, "x", NULL), CS_EARG);
  csDestroy (csv);
}

static void
testArguments (void)
{
  csDestroy (NULL);
  CS_ASSERT (csInit ((CSDword)-1) == NULL);
  CS_ASSERT (csError (NULL) == NULL);
  CS_ASSERT_EQ (csParse (NULL, ""), CS_EARG);
  CS_ASSERT_EQ (csLoad (NULL, ""), CS_EARG);
  CS_ASSERT_EQ (csWrite (NULL, NULL, NULL), CS_EARG);
  CS_ASSERT_EQ (csSave (NULL, ""), CS_EARG);
  CS_ASSERT_EQ (csSet (NULL, 0, 0, ""), CS_EARG);
  CS_ASSERT_EQ (csRowAdd (NULL, NULL, 0), CS_EARG);
  CS_ASSERT_EQ (csRowCount (NULL), 0);
  CS_ASSERT_EQ (csRowSize (NULL), 0);
  CS_ASSERT_EQ (csColumnCount (NULL), 0);
  CS_ASSERT_EQ (csColumnFind (NULL, NULL), CS_COLUMN_NONE);
  CS_ASSERT (csGet (NULL, 0, 0) == NULL);
  CS_ASSERT (csRowAt (NULL, 0) == NULL);
  CS_ASSERT (csRowValue (NULL, 0) == NULL);
  CS_ASSERT (csColumnName (NULL, 0) == NULL);
  CSCsv *csv = csInit (CS_FNONE);
  CS_ASSERT_EQ (csParse (csv, NULL), CS_EARG);
  CS_ASSERT_EQ (csError (csv)->status, CS_EARG);
  CS_ASSERT_EQ (csLoad (csv, NULL), CS_EARG);
  CS_ASSERT_EQ (csSave (csv, NULL), CS_EARG);
  CS_ASSERT_EQ (csWrite (csv, NULL, NULL), CS_EARG);
  CS_ASSERT_EQ (csSet (csv, 0, 0, ""), CS_EARG);
  CS_ASSERT_EQ (csRowAdd (csv, NULL, 0), CS_EARG);
  const CSChar *row[] = { "value" };
  CS_ASSERT_EQ (csRowAdd (csv, row, 0), CS_EARG);
  CS_ASSERT_EQ (csRowAdd (csv, row, (CSSize)-1), CS_EARG);
  CS_ASSERT_EQ (csRowAdd (csv, row, 1), CS_OK);
  CS_ASSERT_EQ (csSet (csv, 0, 0, NULL), CS_EARG);
  CS_ASSERT_EQ (csSet (csv, 0, 1, ""), CS_EARG);
  CS_ASSERT_EQ (csSet (csv, 1, 0, ""), CS_EARG);
  CS_ASSERT_EQ (csColumnFind (csv, NULL), CS_COLUMN_NONE);
  CS_ASSERT_EQ (csError (csv)->status, CS_EARG);
  CS_ASSERT_EQ (csParse (csv, ""), CS_OK);
  CS_ASSERT_EQ (csError (csv)->status, CS_OK);
  csDestroy (csv);
  CS_ASSERT_STR_EQ (csStatusText (CS_OK), "Success");
  CS_ASSERT_STR_EQ (csStatusText (CS_EARG), "Invalid argument");
  CS_ASSERT_STR_EQ (csStatusText (CS_EMEM), "Out of memory");
  CS_ASSERT_STR_EQ (csStatusText (CS_EIO), "Input/output error");
  CS_ASSERT_STR_EQ (csStatusText (CS_ESYNTAX), "Syntax error");
  CS_ASSERT_STR_EQ (csStatusText (CS_EDUPLICATE), "Duplicate column name");
  CS_ASSERT_STR_EQ (csStatusText ((CSStatus)1), "Unknown error");
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Main
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

CSInt
main (void)
{
  CS_SUITE ("cisivi");
  CS_RUN (testParse);
  CS_RUN (testFlagsAndEmpty);
  CS_RUN (testMalformed);
  CS_RUN (testAppendAndEdit);
  CS_RUN (testInsert);
  CS_RUN (testWriting);
  CS_RUN (testWriteRow);
  CS_RUN (testRoundTrips);
  CS_RUN (testFiles);
  CS_RUN (testRead);
  CS_RUN (testReadErrors);
  CS_RUN (testDelimiter);
  CS_RUN (testNewline);
  CS_RUN (testRemove);
  CS_RUN (testRemoveMany);
  CS_RUN (testColumns);
  CS_RUN (testColumnInsert);
  CS_RUN (testArguments);
  CS_SUMMARY ();
}
