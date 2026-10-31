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

#ifndef CISIVI_H
#define CISIVI_H

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Includes
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "cstypes.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Macros
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define CS_VERSION "1.0.0"
#define CS_ERROR_MAX (256)
#define CS_READ_ERROR (~(CSSize)0)
#define CS_COLUMN_NONE ((CSSize) - 1)

#ifdef __cplusplus
extern "C"
{
#endif

  /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
   * Types
   * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

  /** Result of an operation: CS_OK, or a negative error code. */
  typedef enum CSStatus
  {
    CS_OK = 0, /**< Success. */

    CS_EARG = -1,      /**< Invalid argument, index or row. */
    CS_EMEM = -2,      /**< Out of memory. */
    CS_EIO = -3,       /**< File or stream failure; CSError.code is errno. */
    CS_ESYNTAX = -4,   /**< Malformed CSV or row width; CSError.line too. */
    CS_EDUPLICATE = -5 /**< Repeated column name. */
  } CSStatus;

  /** Options of a document, combined with | and given to csInit(). */
  typedef enum CSFlag
  {
    CS_FNONE = 0, /**< Default behavior. */

    /** Treat the first row as unique, non-empty column names. */
    CS_FHEADER = 1 << 0,

    /** Match column names case-insensitively (ASCII), in lookups and
     * duplicate checks alike. Names are kept as written. */
    CS_FICASE = 1 << 1,

    /** Turn CRLF and CR inside parsed fields into LF. */
    CS_FLF = 1 << 2
  } CSFlag;

  /** Details of the most recent operation on a document; see csError(). */
  typedef struct CSError
  {
    CSStatus status;              /**< CS_OK when the last operation worked. */
    CSSize line;                  /**< Offending line, or 0. */
    CSInt code;                   /**< System error code (errno), or 0. */
    CSChar message[CS_ERROR_MAX]; /**< Readable, e.g. "line 3: expected 2
                                       fields, got 3". */
  } CSError;

  /**
   * Receives serialized CSV text from csWrite().
   *
   * @param user Pointer given to csWrite().
   * @param data Text to write; not NUL-terminated.
   * @param size Number of bytes.
   * @return Number of bytes written; anything but size is a failure.
   */
  typedef CSSize (*CSWriteFn) (CSVoid *user, const CSChar *data, CSSize size);

  /**
   * Supplies CSV text to csRead().
   *
   * @param user Pointer given to csRead().
   * @param data Buffer to fill; not NUL-terminated.
   * @param size Capacity of the buffer in bytes.
   * @return Number of bytes stored, at most size; 0 at the end of the input,
   *         or CS_READ_ERROR on failure.
   */
  typedef CSSize (*CSReadFn) (CSVoid *user, CSChar *data, CSSize size);

  /** Opaque CSV document: its rows in order, and the last error. */
  typedef struct CSCsv CSCsv;

  /** Opaque row of a document: its fields in order; owned by the document. */
  typedef struct CSRow CSRow;

  /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
   * Declarations - Document
   * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

  /**
   * Allocates an empty document.
   *
   * @param flags Combination of CSFlag values.
   * @return New document, or NULL when out of memory or for unknown flags.
   */
  CSCsv *csInit (CSDword flags);

  /**
   * Releases a document and every string it owns. Accepts NULL.
   *
   * @param csv Document to destroy.
   */
  void csDestroy (CSCsv *csv);

  /**
   * Gets the details of the most recent operation on a document.
   *
   * @param csv Document to inspect.
   * @return Error record owned by the document, NULL for NULL; status is
   *         CS_OK when the last operation succeeded.
   */
  const CSError *csError (const CSCsv *csv);

  /**
   * Describes a status code.
   *
   * @param status Status to describe.
   * @return Static, human-readable text.
   */
  const CSChar *csStatusText (CSStatus status);

  /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
   * Declarations - Reading and writing
   * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

  /**
   * Parses CSV text into a document. Rows are added to those already
   * present; the document's first row sets the width and, with CS_FHEADER,
   * the column names. On failure the document holds the rows before the
   * offending one.
   *
   * @param csv  Document to populate.
   * @param text NUL-terminated CSV text; a leading UTF-8 BOM is skipped.
   * @return CS_OK on success, or a CS_E* error code.
   */
  CSStatus csParse (CSCsv *csv, const CSChar *text);

  /**
   * Reads and parses a CSV file, as csParse() does. A NUL byte in the file
   * is CS_ESYNTAX; it and read failures leave the document unchanged.
   *
   * @param csv  Document to populate.
   * @param path File to read.
   * @return CS_OK on success, or a CS_E* error code.
   */
  CSStatus csLoad (CSCsv *csv, const CSChar *path);

  /**
   * Reads CSV text from a stream to its end and parses it, as csParse()
   * does; the counterpart of csWrite() for standard input, sockets or
   * memory. A NUL byte is CS_ESYNTAX; it and read failures leave the
   * document unchanged.
   *
   * @param csv  Document to populate.
   * @param read Supplies the text, in pieces, until it returns 0.
   * @param user Passed to read.
   * @return CS_OK on success, or a CS_E* error code; CS_EIO when read
   *         fails or returns more than asked.
   */
  CSStatus csRead (CSCsv *csv, CSReadFn read, CSVoid *user);

  /**
   * Serializes a document as CSV text, row by row, each ended as
   * csSetNewline() chose. Fields are quoted when they hold the delimiter, a
   * quote or a line break, or start with a UTF-8 BOM; an empty field when it
   * is the only one of its row, which would otherwise be a blank line.
   *
   * @param csv   Document to serialize.
   * @param write Receives the text, in pieces.
   * @param user  Passed to write.
   * @return CS_OK on success, or CS_EIO when write fails.
   */
  CSStatus csWrite (CSCsv *csv, CSWriteFn write, CSVoid *user);

  /**
   * Serializes one record as csWrite() serializes a row, with the document's
   * delimiter, line ending and quoting, without adding it to the document:
   * for writing rows as they are produced.
   *
   * @param csv    Document whose settings apply.
   * @param fields Values to write.
   * @param count  Number of fields, at least 1.
   * @param write  Receives the text, in pieces.
   * @param user   Passed to write.
   * @return CS_OK on success, CS_EARG for an invalid row or no writer, or
   *         CS_EIO when write fails.
   */
  CSStatus csWriteRow (CSCsv *csv, const CSChar *const fields[], CSSize count,
                       CSWriteFn write, CSVoid *user);

  /**
   * Writes a document to a file, replacing its contents. Callers that need
   * an atomic replacement write a temporary file with csWrite() and rename
   * it.
   *
   * @param csv  Document to serialize.
   * @param path Destination file.
   * @return CS_OK on success, or a CS_E* error code.
   */
  CSStatus csSave (CSCsv *csv, const CSChar *path);

  /**
   * Chooses the character that separates fields, in parsing and writing
   * alike; a comma by default.
   *
   * @param csv       Document to modify.
   * @param delimiter Any byte but a quote, CR, LF or NUL: e.g. ';', '\t'
   *                  or '|'.
   * @return CS_OK on success, or CS_EARG for an invalid delimiter.
   */
  CSStatus csSetDelimiter (CSCsv *csv, CSChar delimiter);

  /**
   * Chooses what csWrite() and csSave() put after each row; parsing takes
   * either.
   *
   * @param csv     Document to modify.
   * @param newline "\r\n", the default, or "\n".
   * @return CS_OK on success, or CS_EARG for anything else.
   */
  CSStatus csSetNewline (CSCsv *csv, const CSChar *newline);

  /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
   * Declarations - Values and rows
   * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

  /**
   * Looks up a field.
   *
   * @param csv    Document to search.
   * @param row    Row index; 0 is the header with CS_FHEADER.
   * @param column Column index.
   * @return The value, or NULL when out of range.
   */
  const CSChar *csGet (const CSCsv *csv, CSSize row, CSSize column);

  /**
   * Replaces an existing field; the table does not grow. Header names stay
   * non-empty and unique. On failure the field is unchanged.
   *
   * @param csv    Document to modify.
   * @param row    Row index below csRowCount().
   * @param column Column index below csColumnCount().
   * @param value  Value to store; the document keeps a copy.
   * @return CS_OK on success, or a CS_E* error code.
   */
  CSStatus csSet (CSCsv *csv, CSSize row, CSSize column, const CSChar *value);

  /**
   * Appends a row. The first row sets the width and, with CS_FHEADER, the
   * column names; later rows must match it. On failure nothing is added.
   *
   * @param csv    Document to modify.
   * @param fields Values to store; the document keeps copies.
   * @param count  Number of fields, at least 1.
   * @return CS_OK on success, or a CS_E* error code.
   */
  CSStatus csRowAdd (CSCsv *csv, const CSChar *const fields[], CSSize count);

  /**
   * Inserts a row at a position, as csRowAdd() appends one; later rows move
   * down one position. With CS_FHEADER the header stays row 0, so position 0
   * starts the header only in an empty document. On failure nothing is
   * added.
   *
   * @param csv    Document to modify.
   * @param index  Position up to csRowCount(), which appends.
   * @param fields Values to store; the document keeps copies.
   * @param count  Number of fields, at least 1.
   * @return CS_OK on success, or a CS_E* error code.
   */
  CSStatus csRowInsert (CSCsv *csv, CSSize index, const CSChar *const fields[],
                        CSSize count);

  /**
   * Removes a row; later rows move up one position. With CS_FHEADER the
   * header, row 0, goes only as the last row.
   *
   * @param csv   Document to modify.
   * @param index Position below csRowCount().
   * @return CS_TRUE when the row was removed, CS_FALSE when out of range or
   *         for the header of a document with more rows.
   */
  CSBool csRowRemove (CSCsv *csv, CSSize index);

  /**
   * Removes several rows at once, in a single pass over the document; the
   * others keep their order. With CS_FHEADER the header, row 0, goes only
   * together with every other row. A refusal changes nothing.
   *
   * @param csv     Document to modify.
   * @param indexes Positions below csRowCount(), strictly ascending.
   * @param count   Number of positions; 0 removes nothing.
   * @return CS_TRUE when the rows were removed, CS_FALSE for NULL, positions
   *         out of range or out of order, or the header of a document with
   *         more rows.
   */
  CSBool csRowRemoveMany (CSCsv *csv, const CSSize indexes[], CSSize count);

  /**
   * Gets the number of rows, including the header.
   *
   * @param csv Document to inspect.
   * @return Row count, 0 for NULL.
   */
  CSSize csRowCount (const CSCsv *csv);

  /**
   * Gets a row by position, in document order.
   *
   * @param csv   Document to inspect.
   * @param index Position below csRowCount().
   * @return The row, or NULL when out of range. Valid until the row is
   *         removed or the document destroyed.
   */
  const CSRow *csRowAt (const CSCsv *csv, CSSize index);

  /**
   * Gets the number of fields in a row.
   *
   * @param row Row to inspect.
   * @return Field count, 0 for NULL.
   */
  CSSize csRowSize (const CSRow *row);

  /**
   * Gets a field by position within a row.
   *
   * @param row   Row to inspect.
   * @param index Position below csRowSize().
   * @return The value, or NULL when out of range.
   */
  const CSChar *csRowValue (const CSRow *row, CSSize index);

  /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
   * Declarations - Columns
   * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

  /**
   * Gets the number of columns, set by the first row.
   *
   * @param csv Document to inspect.
   * @return Column count, 0 for NULL or an empty document.
   */
  CSSize csColumnCount (const CSCsv *csv);

  /**
   * Gets a column name from the header.
   *
   * @param csv   Document to inspect; needs CS_FHEADER.
   * @param index Position below csColumnCount().
   * @return Column name, or NULL without a header or when out of range.
   */
  const CSChar *csColumnName (const CSCsv *csv, CSSize index);

  /**
   * Finds a column by header name.
   *
   * @param csv  Document to search; needs CS_FHEADER.
   * @param name Column name.
   * @return Column index, or CS_COLUMN_NONE when missing.
   */
  CSSize csColumnFind (const CSCsv *csv, const CSChar *name);

  /**
   * Appends a column: its name to the header, a value to every other row.
   * In an empty document it starts the header. On failure nothing changes.
   *
   * @param csv   Document to modify; needs CS_FHEADER.
   * @param name  Column name, non-empty and unique; the document keeps a
   *              copy.
   * @param value Value of the column in every other row, copied for each;
   *              NULL for an empty field.
   * @return CS_OK on success, or a CS_E* error code.
   */
  CSStatus csColumnAdd (CSCsv *csv, const CSChar *name, const CSChar *value);

  /**
   * Inserts a column at a position, as csColumnAdd() appends one; later
   * columns move right one position. On failure nothing changes.
   *
   * @param csv   Document to modify; needs CS_FHEADER.
   * @param index Position up to csColumnCount(), which appends; 0 in an
   *              empty document.
   * @param name  Column name, non-empty and unique; the document keeps a
   *              copy.
   * @param value Value of the column in every other row, copied for each;
   *              NULL for an empty field.
   * @return CS_OK on success, or a CS_E* error code.
   */
  CSStatus csColumnInsert (CSCsv *csv, CSSize index, const CSChar *name,
                           const CSChar *value);

  /**
   * Removes a column from every row; later columns move left one position.
   * Removing the only column empties the document.
   *
   * @param csv   Document to modify.
   * @param index Position below csColumnCount().
   * @return CS_TRUE when the column was removed, CS_FALSE when out of range.
   */
  CSBool csColumnRemove (CSCsv *csv, CSSize index);

#ifdef __cplusplus
}
#endif

#endif
