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

#include "cisivi.h"
#include "plat.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Types
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

typedef enum CSCExit
{
  CSC_EXIT_OK = 0,
  CSC_EXIT_MISSING = 1,
  CSC_EXIT_ERROR = 2
} CSCExit;

typedef enum CSCMode
{
  CSC_MODE_QUERY,
  CSC_MODE_KEYS,
  CSC_MODE_COUNT,
  CSC_MODE_SET,
  CSC_MODE_ADD,
  CSC_MODE_DELETE
} CSCMode;

typedef enum CSCFormat
{
  CSC_FORMAT_CSV,
  CSC_FORMAT_JSON,
  CSC_FORMAT_TABLE
} CSCFormat;

typedef enum CSCOp
{
  CSC_OP_EQ,
  CSC_OP_NE,
  CSC_OP_LT,
  CSC_OP_LE,
  CSC_OP_GT,
  CSC_OP_GE,
  CSC_OP_HAS,
  CSC_OP_LACKS
} CSCOp;

typedef struct CSCArgs
{
  CSDword flags;
  CSCMode mode;
  CSCFormat format;
  CSBool write;
  CSBool null; /* -0: values and names end with NUL. */
  CSBool crlf;
  CSChar delimiter;
  CSChar output; /* Delimiter of written CSV, '\0' for the one read. */
  const CSChar *value;
  const CSChar *query;
  const CSChar *sort;
  CSBool descending;
  const CSChar **filters;
  CSSize nfilters;
  const CSChar **paths;
  CSSize count;
} CSCArgs;

/* A column as written in a query or filter: by name, or by position when
 * name is NULL. */
typedef struct CSCColumn
{
  const CSChar *name;
  CSSize position;
} CSCColumn;

typedef struct CSCFilter
{
  CSCColumn column;
  CSCOp op;
  const CSChar *value;
  CSSize index; /* The column in the table, once resolved. */
  CSChar *buffer;
} CSCFilter;

/* A row index of [ROWS]: a position from the start, or from the end. */
typedef struct CSCIndex
{
  CSBool given;
  CSBool end;
  CSSize value;
} CSCIndex;

typedef struct CSCQuery
{
  const CSChar *text;
  CSBool rows;         /* [ROWS] given. */
  CSBool slice;        /* A range rather than a single row. */
  CSCIndex from;       /* The single row, or the start of the range, */
  CSCIndex to;         /* and its end, excluded. */
  const CSChar *whole; /* Unquoted columns, tried as one name first. */
  CSCColumn *columns;  /* The columns named; none for every column. */
  CSSize ncolumns;
  CSCColumn order; /* The column of --sort, */
  CSBool ordered;  /* when given. */
  CSChar *buffer;
} CSCQuery;

/* What a query selects: data rows and columns of the table, by index. */
typedef struct CSCSelection
{
  CSSize *rows;
  CSSize nrows;
  CSSize *columns;
  CSSize ncolumns;
} CSCSelection;

/* Tells whether row a goes before row b. */
typedef CSBool (*CSCBefore) (const CSVoid *context, CSSize a, CSSize b);

/* A sort value, its number read once. */
typedef struct CSCKey
{
  CSDouble number;
  CSBool numeric;
} CSCKey;

/* Orders positions in rows by the values of a column. */
typedef struct CSCOrder
{
  const CSCsv *csv;
  CSSize column;
  CSBool descending;
  const CSSize *rows;
  const CSCKey *keys;
} CSCOrder;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Definitions - Private
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static void
cscLogo (void)
{
  cscPrint ("      _       _ _      _\n"
            " ____(_) ___ (_|_)  __(_)\n"
            "/ __/ / (_-</ / | |/ / /\n"
            "\\__/_( )___/_( )|___/_/\n"
            "     |/      |/\n"
            "\n"
            "Cisivi v" CS_VERSION ", <https://lab.partoska.com/cisivi>\n"
            "Copyright (C) 2026 Fabrika Charvat s.r.o. All rights reserved.\n"
            "Licensed under the MIT License.\n"
            "\n");
}

static void
cscUsage (void)
{
  cscLogo ();
  cscPrint (
      "Usage: cisivi [OPTIONS] [QUERY] [FILE...]\n"
      "\n"
      "Queries a CSV table read from FILE, or from standard input when\n"
      "FILE is missing or -. Several files are joined in order, their\n"
      "columns matched by name. The first row names the columns.\n"
      "\n"
      "Query:\n"
      "  .                    The whole table (default)\n"
      "  COLUMN,...           Columns by name; one column prints its\n"
      "                       values, one per line, more print CSV\n"
      "  #N                   A column by position, from 0\n"
      "  \"NAME\"               A quoted column name, which may contain\n"
      "                       commas or quotes; \\\" and \\\\ escape\n"
      "  [ROWS]               Rows by position, from 0: [N], [-N] from\n"
      "                       the end, [N:M] from N up to M, [N:],\n"
      "                       [:M], [*] for all\n"
      "  [ROWS].COLUMN,...    Both, such as [0].name or [-3:].name,id\n"
      "\n"
      "Filter, -f COLUMN OPERATOR VALUE, such as -f 'age>=18':\n"
      "  = !=                 Equal or not, as text\n"
      "  < <= > >=            Ordered: numbers by value, other text\n"
      "                       byte by byte\n"
      "  ~ !~                 Containing VALUE or not\n"
      "\n"
      "Options:\n"
      "  -f, --filter EXPR    Keep the rows where EXPR holds; repeat\n"
      "                       to require all\n"
      "  --sort COLUMN        Order the rows by COLUMN, numbers by\n"
      "                       value\n"
      "  --sort-desc COLUMN   The same, in descending order\n"
      "  -k, --keys           List the column names, one per line\n"
      "  -c, --count          Print the number of rows selected\n"
      "  -s, --set VALUE      Set COLUMN to VALUE in the selected rows,\n"
      "                       adding the column when missing, and\n"
      "                       print the table\n"
      "  -a, --add ROW        Append ROW, one CSV record, or with [N]\n"
      "                       insert it as row N, and print the table\n"
      "  -d, --delete         Delete the selected rows, or the\n"
      "                       columns of the query, and print the\n"
      "                       table\n"
      "  -w, --write          Save the edit back to FILE instead\n"
      "  -j, --json           Print JSON, an array of rows\n"
      "  -t, --table          Print an aligned table, for reading\n"
      "  -0, --null           End values and names with NUL, not a\n"
      "                       newline\n"
      "  -F, --delimiter C    Read fields separated by C, such as ;\n"
      "                       or tab; a comma by default\n"
      "  --output-delimiter C Write fields separated by C; as read by\n"
      "                       default\n"
      "  --crlf               End written rows with CRLF, not LF\n"
      "  -n, --no-header      Read the first row as data; columns are\n"
      "                       #N then\n"
      "  -i, --ignore-case    Match column names case-insensitively\n"
      "  -h, --help           Show this help\n"
      "  -V, --version        Show the version\n"
      "\n"
      "Exit status: 0 found, 1 not found, 2 usage or input error.\n");
}

static CSCExit
cscFailMem (void)
{
  cscError ("cisivi: %s\n", csStatusText (CS_EMEM));

  return CSC_EXIT_ERROR;
}

static const CSChar *
cscInputName (const CSChar *path)
{
  return path ? path : "standard input";
}

static CSBool
cscHeader (const CSCArgs *args)
{
  return (args->flags & CS_FHEADER) != 0;
}

static CSCExit
cscSetMode (CSCArgs *args, CSCMode mode)
{
  if (args->mode != CSC_MODE_QUERY && args->mode != mode)
    {
      cscError ("cisivi: options -k, -c, -s, -a and -d exclude each other\n");
      return CSC_EXIT_ERROR;
    }
  args->mode = mode;

  return CSC_EXIT_OK;
}

static CSCExit
cscSetFormat (CSCArgs *args, CSCFormat format)
{
  if (args->format != CSC_FORMAT_CSV && args->format != format)
    {
      cscError ("cisivi: options -j and -t exclude each other\n");
      return CSC_EXIT_ERROR;
    }
  args->format = format;

  return CSC_EXIT_OK;
}

/* Reads a delimiter: one character, or tab for a tab. */
static CSCExit
cscParseDelimiter (const CSChar *text, CSChar *delimiter)
{
  CSChar c = cscStrEq (text, "tab") || cscStrEq (text, "\\t") ? '\t' : text[0];
  if ((c != '\t' && (text[0] == '\0' || text[1] != '\0')) || c == '"'
      || c == '\r' || c == '\n')
    {
      cscError ("cisivi: invalid delimiter '%s'\n", text);
      return CSC_EXIT_ERROR;
    }
  *delimiter = c;

  return CSC_EXIT_OK;
}

/* Takes the value of an option, the next argument. */
static const CSChar *
cscOptionValue (CSInt argc, CSChar *argv[], CSInt *i)
{
  if (*i + 1 == argc)
    {
      cscError ("cisivi: option '%s' needs a value\n", argv[*i]);
      return NULL;
    }

  return argv[++*i];
}

static CSBool
cscOption (const CSChar *arg, const CSChar *brief, const CSChar *full)
{
  return (brief && cscStrEq (arg, brief)) || cscStrEq (arg, full);
}

static CSCExit
cscParseArgs (CSInt argc, CSChar *argv[], CSCArgs *args, CSBool *done)
{
  CSBool query = CS_FALSE;
  CSBool options = CS_TRUE;
  for (CSInt i = 1; i < argc; ++i)
    {
      const CSChar *arg = argv[i];
      if (options && arg[0] == '-' && arg[1] != '\0')
        {
          CSCExit result = CSC_EXIT_OK;
          const CSChar *value = NULL;
          if (cscStrEq (arg, "--"))
            {
              options = CS_FALSE;
            }
          else if (cscOption (arg, "-f", "--filter"))
            {
              if (!(value = cscOptionValue (argc, argv, &i)))
                {
                  return CSC_EXIT_ERROR;
                }
              args->filters[args->nfilters++] = value;
            }
          else if (cscOption (arg, NULL, "--sort")
                   || cscOption (arg, NULL, "--sort-desc"))
            {
              if (!(value = cscOptionValue (argc, argv, &i)))
                {
                  return CSC_EXIT_ERROR;
                }
              args->sort = value;
              args->descending = cscStrEq (arg, "--sort-desc");
            }
          else if (cscOption (arg, "-k", "--keys"))
            {
              result = cscSetMode (args, CSC_MODE_KEYS);
            }
          else if (cscOption (arg, "-c", "--count"))
            {
              result = cscSetMode (args, CSC_MODE_COUNT);
            }
          else if (cscOption (arg, "-s", "--set")
                   || cscOption (arg, "-a", "--add"))
            {
              if (!(args->value = cscOptionValue (argc, argv, &i)))
                {
                  return CSC_EXIT_ERROR;
                }
              result = cscSetMode (args, cscOption (arg, "-s", "--set")
                                             ? CSC_MODE_SET
                                             : CSC_MODE_ADD);
            }
          else if (cscOption (arg, "-d", "--delete"))
            {
              result = cscSetMode (args, CSC_MODE_DELETE);
            }
          else if (cscOption (arg, "-w", "--write"))
            {
              args->write = CS_TRUE;
            }
          else if (cscOption (arg, "-j", "--json"))
            {
              result = cscSetFormat (args, CSC_FORMAT_JSON);
            }
          else if (cscOption (arg, "-t", "--table"))
            {
              result = cscSetFormat (args, CSC_FORMAT_TABLE);
            }
          else if (cscOption (arg, "-0", "--null"))
            {
              args->null = CS_TRUE;
            }
          else if (cscOption (arg, "-F", "--delimiter")
                   || cscOption (arg, NULL, "--output-delimiter"))
            {
              if (!(value = cscOptionValue (argc, argv, &i)))
                {
                  return CSC_EXIT_ERROR;
                }
              result = cscParseDelimiter (value,
                                          cscStrEq (arg, "--output-delimiter")
                                              ? &args->output
                                              : &args->delimiter);
            }
          else if (cscOption (arg, NULL, "--crlf"))
            {
              args->crlf = CS_TRUE;
            }
          else if (cscOption (arg, "-n", "--no-header"))
            {
              args->flags &= ~(CSDword)CS_FHEADER;
            }
          else if (cscOption (arg, "-i", "--ignore-case"))
            {
              args->flags |= CS_FICASE;
            }
          else if (cscOption (arg, "-h", "--help"))
            {
              cscUsage ();
              *done = CS_TRUE;
            }
          else if (cscOption (arg, "-V", "--version"))
            {
              cscPrint ("cisivi " CS_VERSION "\n");
              *done = CS_TRUE;
            }
          else
            {
              cscError ("cisivi: unknown option '%s'\n", arg);
              result = CSC_EXIT_ERROR;
            }
          if (result != CSC_EXIT_OK || *done)
            {
              return result;
            }
          continue;
        }

      if (!query)
        {
          args->query = arg;
          query = CS_TRUE;
        }
      else
        {
          args->paths[args->count++] = cscStrEq (arg, "-") ? NULL : arg;
        }
    }

  // No file reads standard input.
  if (args->count == 0)
    {
      args->paths[args->count++] = NULL;
    }
  if (!args->output)
    {
      args->output = args->delimiter;
    }

  return CSC_EXIT_OK;
}

static CSCExit
cscSyntaxErr (const CSChar *what, const CSChar *text, const CSChar *reason)
{
  cscError ("cisivi: invalid %s '%s': %s\n", what, text, reason);

  return CSC_EXIT_ERROR;
}

static CSBool
cscIsStop (CSChar c, const CSChar *stops)
{
  return c == '\0' || cscStrFind (stops, c) != NULL;
}

/* Reads decimal digits into value, and moves past them. */
static CSBool
cscParseDigits (const CSChar **in, CSSize *value)
{
  const CSChar *at = *in;
  if (*at < '0' || *at > '9')
    {
      return CS_FALSE;
    }

  *value = 0;
  for (; *at >= '0' && *at <= '9'; ++at)
    {
      CSSize digit = (CSSize)(*at - '0');
      if (*value > ((CSSize)-1 - digit) / 10)
        {
          return CS_FALSE;
        }
      *value = *value * 10 + digit;
    }
  *in = at;

  return CS_TRUE;
}

/* Reads a column at *in, quoted, #N or a bare name up to a stop character,
 * and moves past it. Names are unescaped into *out, which never needs more
 * room than the text they come from. */
static CSCExit
cscParseColumn (const CSChar *what, const CSChar *text, const CSChar **in,
                CSChar **out, const CSChar *stops, CSCColumn *column)
{
  const CSChar *at = *in;
  CSChar *name = *out;
  column->name = NULL;
  column->position = 0;

  if (*at == '#')
    {
      const CSChar *digits = at + 1;
      if (cscParseDigits (&digits, &column->position)
          && cscIsStop (*digits, stops))
        {
          *in = digits;
          return CSC_EXIT_OK;
        }
    }

  if (*at == '"')
    {
      for (++at; *at != '"'; ++at)
        {
          if (*at == '\0')
            {
              return cscSyntaxErr (what, text, "unterminated quote");
            }
          if (*at == '\\' && (at[1] == '"' || at[1] == '\\'))
            {
              ++at;
            }
          *(*out)++ = *at;
        }
      if (!cscIsStop (*++at, stops))
        {
          return cscSyntaxErr (what, text, "text after a quoted name");
        }
    }
  else
    {
      for (; !cscIsStop (*at, stops); ++at)
        {
          if (*at == '"')
            {
              return cscSyntaxErr (what, text, "quote inside a name");
            }
          *(*out)++ = *at;
        }
    }

  if (*out == name)
    {
      return cscSyntaxErr (what, text, "empty name");
    }
  *(*out)++ = '\0';
  column->name = name;
  *in = at;

  return CSC_EXIT_OK;
}

/* Reads a row index, a number or -number counted from the end. */
static CSBool
cscParseIndex (const CSChar **in, CSCIndex *index)
{
  const CSChar *at = *in;
  index->end = *at == '-';
  if (index->end)
    {
      ++at;
    }
  if (!cscParseDigits (&at, &index->value))
    {
      return CS_FALSE;
    }

  // -0 is the first row, as 0 is.
  index->end = index->end && index->value > 0;
  index->given = CS_TRUE;
  *in = at;

  return CS_TRUE;
}

/* Reads [ROWS]: [N], [N:M] with either end left out, [*] or []. */
static CSCExit
cscParseRows (const CSChar **in, CSCQuery *query)
{
  const CSChar *at = *in + 1;
  query->rows = CS_TRUE;
  query->slice = CS_TRUE;
  if (*at == '*')
    {
      ++at;
    }
  else if (*at != ']')
    {
      query->slice = CS_FALSE;
      if (*at != ':' && !cscParseIndex (&at, &query->from))
        {
          return cscSyntaxErr ("query", query->text, "invalid row index");
        }
      if (*at == ':')
        {
          query->slice = CS_TRUE;
          ++at;
          if (*at != ']' && !cscParseIndex (&at, &query->to))
            {
              return cscSyntaxErr ("query", query->text, "invalid row index");
            }
        }
    }
  if (*at++ != ']')
    {
      return cscSyntaxErr ("query", query->text, "expected ] after rows");
    }
  *in = at;

  return CSC_EXIT_OK;
}

/* Splits a query into [ROWS] and the columns, separated by commas. */
static CSCExit
cscParseQuery (const CSChar *text, CSCQuery *query)
{
  query->text = text;
  if (cscStrEq (text, "."))
    {
      return CSC_EXIT_OK;
    }

  CSSize length = cscStrLen (text);
  CSSize commas = 0;
  for (const CSChar *at = text; *at; ++at)
    {
      commas += *at == ',';
    }
  query->buffer = cscStrDup (text, length);
  query->columns = cscMemAlloc (commas + 1, sizeof (*query->columns));
  if (!query->buffer || !query->columns)
    {
      return cscFailMem ();
    }

  // A leading dot is optional: .[0].name is [0].name.
  const CSChar *in = text;
  if (*in == '.')
    {
      ++in;
    }
  if (*in == '[')
    {
      CSCExit result = cscParseRows (&in, query);
      if (result != CSC_EXIT_OK || *in == '\0')
        {
          return result;
        }
      if (*in++ != '.')
        {
          return cscSyntaxErr ("query", text, "expected . after rows");
        }
    }

  // Unquoted, the columns are tried as one name first, so that names may
  // contain commas.
  query->whole = cscStrFind (in, '"') ? NULL : in;
  CSChar *out = query->buffer;
  for (;;)
    {
      CSCExit result = cscParseColumn ("query", text, &in, &out, ",",
                                       &query->columns[query->ncolumns++]);
      if (result != CSC_EXIT_OK)
        {
          return result;
        }
      if (*in++ == '\0')
        {
          break;
        }
    }

  return CSC_EXIT_OK;
}

/* Splits a filter into column, operator and value. */
static CSCExit
cscParseFilter (const CSChar *text, CSCFilter *filter)
{
  static const struct
  {
    const CSChar *text;
    CSCOp op;
  } ops[] = { { "==", CSC_OP_EQ }, { "!=", CSC_OP_NE }, { "!~", CSC_OP_LACKS },
              { "<=", CSC_OP_LE }, { ">=", CSC_OP_GE }, { "=", CSC_OP_EQ },
              { "<", CSC_OP_LT },  { ">", CSC_OP_GT },  { "~", CSC_OP_HAS } };

  filter->buffer = cscStrDup (text, cscStrLen (text));
  if (!filter->buffer)
    {
      return cscFailMem ();
    }

  const CSChar *in = text;
  CSChar *out = filter->buffer;
  CSCExit result
      = cscParseColumn ("filter", text, &in, &out, "=!<>~", &filter->column);
  if (result != CSC_EXIT_OK)
    {
      return result;
    }

  for (CSSize i = 0; i < CS_COUNTOF (ops); ++i)
    {
      CSSize n = cscStrLen (ops[i].text);
      CSSize j = 0;
      while (j < n && in[j] == ops[i].text[j])
        {
          ++j;
        }
      if (j == n)
        {
          filter->op = ops[i].op;
          filter->value = in + n;
          return CSC_EXIT_OK;
        }
    }

  return cscSyntaxErr ("filter", text, "expected an operator");
}

/* Reads the column of --sort, which may be quoted or #N. */
static CSCExit
cscParseOrder (const CSChar *text, CSCQuery *query, CSChar **buffer)
{
  *buffer = cscStrDup (text, cscStrLen (text));
  if (!*buffer)
    {
      return cscFailMem ();
    }

  const CSChar *in = text;
  CSChar *out = *buffer;
  query->ordered = CS_TRUE;

  return cscParseColumn ("sort column", text, &in, &out, "", &query->order);
}

/* Rejects combinations of options and query that make no sense. */
static CSCExit
cscCheck (const CSCArgs *args, const CSCQuery *query)
{
  CSBool rows = query->rows || args->nfilters > 0;
  if (args->mode == CSC_MODE_SET && query->ncolumns == 0)
    {
      cscError ("cisivi: --set needs a COLUMN\n");
      return CSC_EXIT_ERROR;
    }

  if (args->mode == CSC_MODE_ADD
      && ((query->rows && query->slice) || args->nfilters > 0
          || query->ncolumns > 0))
    {
      cscError ("cisivi: --add appends to the whole table, or inserts at "
                "one [N], without COLUMNS or filter\n");
      return CSC_EXIT_ERROR;
    }

  if (args->mode == CSC_MODE_DELETE && rows == (query->ncolumns > 0))
    {
      cscError ("cisivi: --delete needs [ROWS] or a filter, or else "
                "COLUMNS\n");
      return CSC_EXIT_ERROR;
    }

  if (!args->write)
    {
      return CSC_EXIT_OK;
    }

  if (args->mode != CSC_MODE_SET && args->mode != CSC_MODE_ADD
      && args->mode != CSC_MODE_DELETE)
    {
      cscError ("cisivi: --write needs --set, --add or --delete\n");
      return CSC_EXIT_ERROR;
    }

  if (args->count != 1 || !args->paths[0])
    {
      cscError ("cisivi: --write needs exactly one FILE\n");
      return CSC_EXIT_ERROR;
    }

  if (args->format != CSC_FORMAT_CSV)
    {
      cscError ("cisivi: --write writes CSV, not -j or -t\n");
      return CSC_EXIT_ERROR;
    }

  return CSC_EXIT_OK;
}

static CSCExit
cscLoad (CSCsv *csv, const CSChar *path)
{
  if (path)
    {
      CSStatus status = csLoad (csv, path);
      if (status == CS_EIO)
        {
          // The message names the file already.
          cscError ("cisivi: %s\n", csError (csv)->message);
          return CSC_EXIT_ERROR;
        }

      if (status != CS_OK)
        {
          cscError ("cisivi: %s: %s\n", path, csError (csv)->message);
          return CSC_EXIT_ERROR;
        }

      return CSC_EXIT_OK;
    }

  CSInt code = 0;
  if (csRead (csv, cscReadIn, &code) != CS_OK)
    {
      cscError ("cisivi: standard input: %s\n",
                code ? cscSysErrorText (code) : csError (csv)->message);
      return CSC_EXIT_ERROR;
    }

  return CSC_EXIT_OK;
}

/* Appends the rows of another table, matching columns by name: columns new
 * to the table are added, those a row lacks stay empty. */
static CSCExit
cscMerge (CSCsv *csv, const CSCsv *more, const CSChar *path)
{
  CSSize width = csColumnCount (more);
  CSSize *map = cscMemAlloc (width, sizeof (*map));
  if (!map)
    {
      return cscFailMem ();
    }

  CSStatus status = CS_OK;
  for (CSSize j = 0; status == CS_OK && j < width; ++j)
    {
      map[j] = csColumnFind (csv, csColumnName (more, j));
      if (map[j] == CS_COLUMN_NONE)
        {
          status = csColumnAdd (csv, csColumnName (more, j), NULL);
          map[j] = csColumnCount (csv) - 1;
        }
    }

  CSSize total = csColumnCount (csv);
  const CSChar **fields = cscMemAlloc (total, sizeof (*fields));
  if (!fields)
    {
      cscMemFree (map);
      return cscFailMem ();
    }
  for (CSSize i = 1; status == CS_OK && i < csRowCount (more); ++i)
    {
      for (CSSize j = 0; j < total; ++j)
        {
          fields[j] = "";
        }
      for (CSSize j = 0; j < width; ++j)
        {
          fields[map[j]] = csGet (more, i, j);
        }
      status = csRowAdd (csv, fields, total);
    }
  cscMemFree (fields);
  cscMemFree (map);

  if (status != CS_OK)
    {
      cscError ("cisivi: %s: %s\n", cscInputName (path),
                csError (csv)->message);
      return CSC_EXIT_ERROR;
    }

  return CSC_EXIT_OK;
}

/* Loads the inputs in order: with a header, each joins the table by column
 * name; without, rows are appended and must be as wide as the first. */
static CSCExit
cscLoadAll (CSCsv *csv, const CSCArgs *args)
{
  CSCExit result = cscLoad (csv, args->paths[0]);
  for (CSSize i = 1; result == CSC_EXIT_OK && i < args->count; ++i)
    {
      if (!cscHeader (args))
        {
          result = cscLoad (csv, args->paths[i]);
          continue;
        }

      CSCsv *more = csInit (args->flags);
      if (!more)
        {
          return cscFailMem ();
        }

      csSetDelimiter (more, args->delimiter);
      result = cscLoad (more, args->paths[i]);
      if (result == CSC_EXIT_OK)
        {
          result = cscMerge (csv, more, args->paths[i]);
        }
      csDestroy (more);
    }

  return result;
}

/* Gets the index of a column in the table, or CS_COLUMN_NONE. */
static CSSize
cscFind (const CSCsv *csv, const CSCColumn *column)
{
  if (column->name)
    {
      return csColumnFind (csv, column->name);
    }

  return column->position < csColumnCount (csv) ? column->position
                                                : CS_COLUMN_NONE;
}

/* Compares two values: numbers by value, other text byte by byte. */
static CSInt
cscCompare (const CSChar *a, const CSChar *b)
{
  CSDouble x = 0;
  CSDouble y = 0;
  if (cscStrNumber (a, &x) && cscStrNumber (b, &y))
    {
      return x < y ? -1 : x > y ? 1 : 0;
    }

  return cscStrCmp (a, b);
}

static CSBool
cscMatch (const CSCFilter *filter, const CSChar *value)
{
  switch (filter->op)
    {
    case CSC_OP_EQ:
      return cscStrEq (value, filter->value);
    case CSC_OP_NE:
      return !cscStrEq (value, filter->value);
    case CSC_OP_HAS:
      return cscStrFindStr (value, filter->value) != NULL;
    case CSC_OP_LACKS:
      return cscStrFindStr (value, filter->value) == NULL;
    case CSC_OP_LT:
      return cscCompare (value, filter->value) < 0;
    case CSC_OP_LE:
      return cscCompare (value, filter->value) <= 0;
    case CSC_OP_GT:
      return cscCompare (value, filter->value) > 0;
    case CSC_OP_GE:
      return cscCompare (value, filter->value) >= 0;
    }

  return CS_FALSE;
}

/* Sorts rows, stably: a bottom-up merge sort between rows and spare. */
static void
cscSort (CSSize *rows, CSSize *spare, CSSize count, CSCBefore before,
         const CSVoid *context)
{
  CSSize *from = rows;
  CSSize *to = spare;
  for (CSSize width = 1;
       width<count; width = width> count / 2 ? count : width * 2)
    {
      for (CSSize lo = 0; lo < count;
           lo = count - lo > 2 * width ? lo + 2 * width : count)
        {
          CSSize mid = count - lo > width ? lo + width : count;
          CSSize hi = count - mid > width ? mid + width : count;
          CSSize i = lo;
          CSSize j = mid;
          CSSize k = lo;
          while (i < mid && j < hi)
            {
              // Ties keep the earlier row first.
              to[k++]
                  = before (context, from[j], from[i]) ? from[j++] : from[i++];
            }
          while (i < mid)
            {
              to[k++] = from[i++];
            }
          while (j < hi)
            {
              to[k++] = from[j++];
            }
        }

      CSSize *swap = from;
      from = to;
      to = swap;
    }

  for (CSSize i = 0; from != rows && i < count; ++i)
    {
      rows[i] = from[i];
    }
}

/* Compares as cscCompare() does, with the numbers read beforehand. */
static CSBool
cscBeforeValue (const CSVoid *context, CSSize a, CSSize b)
{
  const CSCOrder *order = context;
  const CSCKey *x = &order->keys[a];
  const CSCKey *y = &order->keys[b];
  CSInt c;
  if (x->numeric && y->numeric)
    {
      c = x->number < y->number ? -1 : x->number > y->number ? 1 : 0;
    }
  else
    {
      c = cscStrCmp (csGet (order->csv, order->rows[a], order->column),
                     csGet (order->csv, order->rows[b], order->column));
    }

  return order->descending ? c > 0 : c < 0;
}

static CSBool
cscBeforeAscending (const CSVoid *context, CSSize a, CSSize b)
{
  CS_UNUSED (context);

  return a < b;
}

static CSBool
cscBeforeDescending (const CSVoid *context, CSSize a, CSSize b)
{
  CS_UNUSED (context);

  return a > b;
}

/* Sorts with a spare array of the same size. */
static CSCExit
cscOrder (CSSize *rows, CSSize count, CSCBefore before, const CSVoid *context)
{
  CSSize *spare = cscMemAlloc (count, sizeof (*spare));
  if (!spare)
    {
      return cscFailMem ();
    }
  cscSort (rows, spare, count, before, context);
  cscMemFree (spare);

  return CSC_EXIT_OK;
}

/* Sorts rows by the values of a column. Each value is read as a number
 * once, and positions into rows are sorted against those keys. */
static CSCExit
cscOrderValues (const CSCsv *csv, CSSize column, CSBool descending,
                CSSize *rows, CSSize count)
{
  CSCKey *keys = cscMemAlloc (count, sizeof (*keys));
  CSSize *positions = cscMemAlloc (count, sizeof (*positions));
  if (!keys || !positions)
    {
      cscMemFree (keys);
      cscMemFree (positions);
      return cscFailMem ();
    }

  for (CSSize i = 0; i < count; ++i)
    {
      keys[i].numeric
          = cscStrNumber (csGet (csv, rows[i], column), &keys[i].number);
      positions[i] = i;
    }

  CSCOrder order = { csv, column, descending, rows, keys };
  CSCExit result = cscOrder (positions, count, cscBeforeValue, &order);
  cscMemFree (keys);
  if (result == CSC_EXIT_OK)
    {
      for (CSSize i = 0; i < count; ++i)
        {
          positions[i] = rows[positions[i]];
        }
      for (CSSize i = 0; i < count; ++i)
        {
          rows[i] = positions[i];
        }
    }
  cscMemFree (positions);

  return result;
}

/* Resolves a row index against count rows. */
static CSSize
cscPosition (const CSCIndex *index, CSSize count)
{
  if (index->end)
    {
      return index->value > count ? 0 : count - index->value;
    }

  return index->value > count ? count : index->value;
}

/* Narrows the selected rows to what [ROWS] picks among them. */
static void
cscSlice (const CSCQuery *query, CSCSelection *selection)
{
  CSSize count = selection->nrows;
  if (!query->rows)
    {
      return;
    }

  CSSize first = query->from.given ? cscPosition (&query->from, count) : 0;
  CSSize last = query->to.given ? cscPosition (&query->to, count) : count;
  if (!query->slice)
    {
      // A single row exists, or nothing is selected.
      CSBool exists = query->from.end ? query->from.value <= count
                                      : query->from.value < count;
      last = exists ? first + 1 : first;
    }

  selection->nrows = first < last ? last - first : 0;
  for (CSSize i = 0; i < selection->nrows; ++i)
    {
      selection->rows[i] = selection->rows[first + i];
    }
}

/* Selects the data rows: those passing every filter, in the order asked
 * for, narrowed by [ROWS]. A filter or sort column that does not exist is
 * not found. */
static CSCExit
cscSelectRows (const CSCsv *csv, const CSCArgs *args, CSCFilter *filters,
               const CSCQuery *query, CSCSelection *selection)
{
  for (CSSize i = 0; i < args->nfilters; ++i)
    {
      filters[i].index = cscFind (csv, &filters[i].column);
      if (filters[i].index == CS_COLUMN_NONE)
        {
          return CSC_EXIT_MISSING;
        }
    }

  CSSize column = CS_COLUMN_NONE;
  if (query->ordered)
    {
      column = cscFind (csv, &query->order);
      if (column == CS_COLUMN_NONE)
        {
          return CSC_EXIT_MISSING;
        }
    }

  CSSize base = cscHeader (args) ? 1 : 0;
  CSSize count = csRowCount (csv) > base ? csRowCount (csv) - base : 0;
  selection->rows = cscMemAlloc (count, sizeof (*selection->rows));
  if (!selection->rows)
    {
      return cscFailMem ();
    }

  for (CSSize i = base; i < csRowCount (csv); ++i)
    {
      CSBool keep = CS_TRUE;
      for (CSSize j = 0; keep && j < args->nfilters; ++j)
        {
          keep = cscMatch (&filters[j], csGet (csv, i, filters[j].index));
        }
      if (keep)
        {
          selection->rows[selection->nrows++] = i;
        }
    }

  if (query->ordered
      && cscOrderValues (csv, column, args->descending, selection->rows,
                         selection->nrows)
             != CSC_EXIT_OK)
    {
      return CSC_EXIT_ERROR;
    }

  cscSlice (query, selection);

  return CSC_EXIT_OK;
}

/* Selects the columns a query names, or every column. A column that does
 * not exist is not found; one named twice is an error. */
static CSCExit
cscSelectColumns (const CSCsv *csv, const CSCQuery *query,
                  CSCSelection *selection)
{
  CSSize width = csColumnCount (csv);
  CSSize whole = query->whole && query->ncolumns > 1
                     ? csColumnFind (csv, query->whole)
                     : CS_COLUMN_NONE;
  CSSize count = query->ncolumns == 0      ? width
                 : whole != CS_COLUMN_NONE ? 1
                                           : query->ncolumns;
  selection->columns = cscMemAlloc (count, sizeof (*selection->columns));
  if (!selection->columns)
    {
      return cscFailMem ();
    }

  for (CSSize i = 0; i < count; ++i)
    {
      CSSize index = query->ncolumns == 0 ? i
                     : whole != CS_COLUMN_NONE
                         ? whole
                         : cscFind (csv, &query->columns[i]);
      if (index == CS_COLUMN_NONE)
        {
          return CSC_EXIT_MISSING;
        }
      for (CSSize j = 0; j < i; ++j)
        {
          if (selection->columns[j] == index)
            {
              cscError ("cisivi: invalid query '%s': a column named twice\n",
                        query->text);
              return CSC_EXIT_ERROR;
            }
        }
      selection->columns[selection->ncolumns++] = index;
    }

  return CSC_EXIT_OK;
}

/* Formats a number into digits, which must hold 32 characters. */
static CSChar *
cscFormatSize (CSSize n, CSChar *digits)
{
  CSChar *at = digits + 31;
  *at = '\0';
  do
    {
      *--at = (CSChar)('0' + n % 10);
      n /= 10;
    }
  while (n > 0);

  return at;
}

/* Ends a value or name printed on its own, with a newline or NUL. */
static CSBool
cscPrintEnd (const CSCArgs *args)
{
  return cscPrintChar (args->null ? '\0' : '\n');
}

/* Gets the name of a column; without a header, its position as #N. */
static const CSChar *
cscColumnLabel (const CSCsv *csv, const CSCArgs *args, CSSize column,
                CSChar *digits)
{
  if (cscHeader (args))
    {
      return csColumnName (csv, column);
    }

  CSChar *at = cscFormatSize (column, digits + 1);
  *--at = '#';

  return at;
}

static CSCExit
cscWriteErr (CSCsv *csv)
{
  cscError ("cisivi: %s\n", csError (csv)->message);

  return CSC_EXIT_ERROR;
}

/* Sets the delimiter and line ending of written CSV. */
static CSCExit
cscPrepare (CSCsv *csv, const CSCArgs *args)
{
  if (csSetDelimiter (csv, args->output) != CS_OK
      || csSetNewline (csv, args->crlf ? "\r\n" : "\n") != CS_OK)
    {
      return cscWriteErr (csv);
    }

  return CSC_EXIT_OK;
}

/* Prints the values of one column, each on its own. */
static CSCExit
cscPrintRaw (const CSCsv *csv, const CSCArgs *args,
             const CSCSelection *selection)
{
  for (CSSize i = 0; i < selection->nrows; ++i)
    {
      if (!cscPrint (csGet (csv, selection->rows[i], selection->columns[0]))
          || !cscPrintEnd (args))
        {
          return CSC_EXIT_ERROR;
        }
    }

  return CSC_EXIT_OK;
}

/* Prints the selection as CSV, row by row, with the output settings kept
 * in an empty table of its own. */
static CSCExit
cscPrintCsv (const CSCsv *csv, const CSCArgs *args,
             const CSCSelection *selection)
{
  CSCsv *out = csInit (CS_FNONE);
  const CSChar **fields = cscMemAlloc (selection->ncolumns, sizeof (*fields));
  if (!out || !fields)
    {
      csDestroy (out);
      cscMemFree (fields);
      return cscFailMem ();
    }

  CSCExit result = cscPrepare (out, args);
  CSSize first = cscHeader (args) ? 0 : 1;
  for (CSSize i = first; result == CSC_EXIT_OK && selection->ncolumns > 0
                         && i <= selection->nrows;
       ++i)
    {
      for (CSSize j = 0; j < selection->ncolumns; ++j)
        {
          fields[j] = i == 0 ? csColumnName (csv, selection->columns[j])
                             : csGet (csv, selection->rows[i - 1],
                                      selection->columns[j]);
        }
      if (csWriteRow (out, fields, selection->ncolumns, cscWriteOut, NULL)
          != CS_OK)
        {
          result = cscWriteErr (out);
        }
    }

  cscMemFree (fields);
  csDestroy (out);

  return result;
}

/* Prints a JSON string, escaping quotes, backslashes and control
 * characters. */
static CSBool
cscPrintJsonText (const CSChar *text)
{
  static const CSChar hex[] = "0123456789abcdef";
  CSBool ok = cscPrintChar ('"');
  for (; ok && *text; ++text)
    {
      CSByte c = (CSByte)*text;
      CSChar escape[7] = { '\\', '\0', '\0', '\0', '\0', '\0', '\0' };
      switch (c)
        {
        case '"':
        case '\\':
          escape[1] = (CSChar)c;
          break;
        case '\n':
          escape[1] = 'n';
          break;
        case '\r':
          escape[1] = 'r';
          break;
        case '\t':
          escape[1] = 't';
          break;
        default:
          if (c < 0x20)
            {
              escape[1] = 'u';
              escape[2] = '0';
              escape[3] = '0';
              escape[4] = hex[c >> 4];
              escape[5] = hex[c & 0xf];
            }
        }
      ok = escape[1] ? cscPrint (escape) : cscPrintChar ((CSChar)c);
    }

  return ok && cscPrintChar ('"');
}

/* Prints the selection as a JSON array: an object per row, keyed by the
 * column names, or without a header an array per row. */
static CSCExit
cscPrintJson (const CSCsv *csv, const CSCArgs *args,
              const CSCSelection *selection)
{
  CSBool header = cscHeader (args);
  CSBool ok = cscPrint (selection->nrows ? "[\n" : "[");
  for (CSSize i = 0; ok && i < selection->nrows; ++i)
    {
      ok = cscPrint (header ? "  {" : "  [");
      for (CSSize j = 0; ok && j < selection->ncolumns; ++j)
        {
          CSSize column = selection->columns[j];
          ok = (j == 0 || cscPrint (", "))
               && (!header
                   || (cscPrintJsonText (csColumnName (csv, column))
                       && cscPrint (": ")))
               && cscPrintJsonText (csGet (csv, selection->rows[i], column));
        }
      ok = ok && cscPrint (header ? "}" : "]")
           && cscPrint (i + 1 < selection->nrows ? ",\n" : "\n");
    }

  return ok && cscPrint ("]\n") ? CSC_EXIT_OK : CSC_EXIT_ERROR;
}

/* Gets the escape a table shows for a control character, or NULL. */
static const CSChar *
cscCellEscape (CSChar c)
{
  return c == '\n' ? "\\n" : c == '\r' ? "\\r" : c == '\t' ? "\\t" : NULL;
}

/* Measures a value as a table shows it, in characters of UTF-8. */
static CSSize
cscCellWidth (const CSChar *value)
{
  CSSize width = 0;
  for (; *value; ++value)
    {
      if (cscCellEscape (*value))
        {
          width += 2;
        }
      else if (((CSByte)*value & 0xc0) != 0x80)
        {
          ++width;
        }
    }

  return width;
}

/* Prints a value as a table shows it, padded to width unless last. */
static CSBool
cscPrintCell (const CSChar *value, CSSize width, CSBool last)
{
  CSBool ok = CS_TRUE;
  for (const CSChar *at = value; ok && *at; ++at)
    {
      const CSChar *escape = cscCellEscape (*at);
      ok = escape ? cscPrint (escape) : cscPrintChar (*at);
    }
  for (CSSize n = cscCellWidth (value); ok && !last && n < width; ++n)
    {
      ok = cscPrintChar (' ');
    }

  return ok && cscPrint (last ? "\n" : "  ");
}

/* Prints the selection as an aligned table: the names, a rule, the rows. */
static CSCExit
cscPrintTable (const CSCsv *csv, const CSCArgs *args,
               const CSCSelection *selection)
{
  CSSize count = selection->ncolumns;
  CSSize *widths = cscMemAlloc (count, sizeof (*widths));
  if (!widths)
    {
      return cscFailMem ();
    }

  CSChar digits[33];
  for (CSSize j = 0; j < count; ++j)
    {
      CSSize column = selection->columns[j];
      widths[j] = cscCellWidth (cscColumnLabel (csv, args, column, digits));
      for (CSSize i = 0; i < selection->nrows; ++i)
        {
          CSSize width
              = cscCellWidth (csGet (csv, selection->rows[i], column));
          widths[j] = width > widths[j] ? width : widths[j];
        }
    }

  CSBool ok = CS_TRUE;
  for (CSSize j = 0; ok && j < count; ++j)
    {
      ok = cscPrintCell (
          cscColumnLabel (csv, args, selection->columns[j], digits), widths[j],
          j + 1 == count);
    }
  for (CSSize j = 0; ok && j < count; ++j)
    {
      for (CSSize n = 0; ok && n < widths[j]; ++n)
        {
          ok = cscPrintChar ('-');
        }
      ok = ok && cscPrint (j + 1 == count ? "\n" : "  ");
    }
  for (CSSize i = 0; ok && i < selection->nrows; ++i)
    {
      for (CSSize j = 0; ok && j < count; ++j)
        {
          ok = cscPrintCell (
              csGet (csv, selection->rows[i], selection->columns[j]),
              widths[j], j + 1 == count);
        }
    }
  cscMemFree (widths);

  return ok ? CSC_EXIT_OK : CSC_EXIT_ERROR;
}

/* Prints a selection in the format asked for. */
static CSCExit
cscPrintSelection (const CSCsv *csv, const CSCArgs *args,
                   const CSCSelection *selection)
{
  switch (args->format)
    {
    case CSC_FORMAT_JSON:
      return cscPrintJson (csv, args, selection);
    case CSC_FORMAT_TABLE:
      return cscPrintTable (csv, args, selection);
    case CSC_FORMAT_CSV:
      break;
    }

  return cscPrintCsv (csv, args, selection);
}

static void
cscSelectionFree (CSCSelection *selection)
{
  cscMemFree (selection->rows);
  cscMemFree (selection->columns);
}

/* Prints the whole table, as after an edit. */
static CSCExit
cscPrintAll (CSCsv *csv, const CSCArgs *args)
{
  if (args->format == CSC_FORMAT_CSV)
    {
      CSCExit result = cscPrepare (csv, args);
      if (result == CSC_EXIT_OK && csWrite (csv, cscWriteOut, NULL) != CS_OK)
        {
          result = cscWriteErr (csv);
        }
      return result;
    }

  CSCQuery all = { ".",
                   CS_FALSE,
                   CS_FALSE,
                   { CS_FALSE, CS_FALSE, 0 },
                   { CS_FALSE, CS_FALSE, 0 },
                   NULL,
                   NULL,
                   0,
                   { NULL, 0 },
                   CS_FALSE,
                   NULL };
  CSCArgs every = *args;
  every.nfilters = 0;
  CSCSelection selection = { NULL, 0, NULL, 0 };
  CSCExit result = cscSelectRows (csv, &every, NULL, &all, &selection);
  if (result == CSC_EXIT_OK)
    {
      result = cscSelectColumns (csv, &all, &selection);
    }
  if (result == CSC_EXIT_OK)
    {
      result = cscPrintSelection (csv, args, &selection);
    }
  cscSelectionFree (&selection);

  return result;
}

/* Prints the names of the columns, one per line. */
static CSCExit
cscKeys (const CSCsv *csv, const CSCArgs *args, const CSCQuery *query)
{
  CSCSelection selection = { NULL, 0, NULL, 0 };
  CSCExit result = cscSelectColumns (csv, query, &selection);
  CSChar digits[33];
  for (CSSize j = 0; result == CSC_EXIT_OK && j < selection.ncolumns; ++j)
    {
      if (!cscPrint (cscColumnLabel (csv, args, selection.columns[j], digits))
          || !cscPrintEnd (args))
        {
          result = CSC_EXIT_ERROR;
        }
    }
  cscSelectionFree (&selection);

  return result;
}

/* Prints what a query selects; nothing selected is not found, though the
 * header is printed all the same. */
static CSCExit
cscQuery (const CSCsv *csv, const CSCArgs *args, CSCFilter *filters,
          const CSCQuery *query)
{
  CSCSelection selection = { NULL, 0, NULL, 0 };
  CSCExit result = cscSelectRows (csv, args, filters, query, &selection);
  if (result == CSC_EXIT_OK)
    {
      result = cscSelectColumns (csv, query, &selection);
    }

  if (result == CSC_EXIT_OK && args->mode == CSC_MODE_COUNT)
    {
      CSChar digits[32];
      result = cscPrint (cscFormatSize (selection.nrows, digits))
                       && cscPrint ("\n")
                   ? CSC_EXIT_OK
                   : CSC_EXIT_ERROR;
    }
  else if (result == CSC_EXIT_OK)
    {
      // One column named prints its values as they are.
      result = query->ncolumns > 0 && selection.ncolumns == 1
                       && args->format == CSC_FORMAT_CSV
                   ? cscPrintRaw (csv, args, &selection)
                   : cscPrintSelection (csv, args, &selection);
    }
  if (result == CSC_EXIT_OK && selection.nrows == 0)
    {
      result = CSC_EXIT_MISSING;
    }
  cscSelectionFree (&selection);

  return result;
}

/* Sets one column in the selected rows, adding it when missing. */
static CSCExit
cscSetCells (CSCsv *csv, const CSCArgs *args, const CSCSelection *selection,
             const CSCQuery *query)
{
  CSSize whole = query->whole && query->ncolumns > 1
                     ? csColumnFind (csv, query->whole)
                     : CS_COLUMN_NONE;
  if (whole == CS_COLUMN_NONE && query->ncolumns > 1)
    {
      cscError ("cisivi: --set needs one COLUMN\n");
      return CSC_EXIT_ERROR;
    }

  CSSize column
      = whole != CS_COLUMN_NONE ? whole : cscFind (csv, &query->columns[0]);
  if (selection->nrows == 0
      || (column == CS_COLUMN_NONE
          && (!query->columns[0].name || !cscHeader (args))))
    {
      return CSC_EXIT_MISSING;
    }

  if (column == CS_COLUMN_NONE)
    {
      if (csColumnAdd (csv, query->columns[0].name, NULL) != CS_OK)
        {
          return cscWriteErr (csv);
        }
      column = csColumnCount (csv) - 1;
    }

  for (CSSize i = 0; i < selection->nrows; ++i)
    {
      if (csSet (csv, selection->rows[i], column, args->value) != CS_OK)
        {
          return cscWriteErr (csv);
        }
    }

  return CSC_EXIT_OK;
}

/* Appends one record, parsed as CSV with the delimiter of the input, or
 * inserts it as data row [N]: N may be the row count, the end, and [-N]
 * goes before the last N rows. A position beyond is not found. */
static CSCExit
cscAddRow (CSCsv *csv, const CSCArgs *args, const CSCQuery *query)
{
  CSCsv *row = csInit (CS_FNONE);
  if (!row)
    {
      return cscFailMem ();
    }

  CSCExit result = CSC_EXIT_OK;
  csSetDelimiter (row, args->delimiter);
  if (csParse (row, args->value) != CS_OK)
    {
      cscError ("cisivi: invalid row '%s': %s\n", args->value,
                csError (row)->message);
      result = CSC_EXIT_ERROR;
    }
  else if (csRowCount (row) != 1)
    {
      cscError ("cisivi: invalid row '%s': not one record\n", args->value);
      result = CSC_EXIT_ERROR;
    }

  const CSChar **fields = NULL;
  CSSize count = csColumnCount (row);
  if (result == CSC_EXIT_OK
      && !(fields = cscMemAlloc (count, sizeof (*fields))))
    {
      result = cscFailMem ();
    }
  for (CSSize j = 0; result == CSC_EXIT_OK && j < count; ++j)
    {
      fields[j] = csGet (row, 0, j);
    }

  CSSize index = csRowCount (csv);
  if (query->rows)
    {
      CSSize base = cscHeader (args) && index > 0 ? 1 : 0;
      CSSize data = index - base;
      index = base + cscPosition (&query->from, data);
      if (result == CSC_EXIT_OK && query->from.value > data)
        {
          result = CSC_EXIT_MISSING;
        }
    }
  if (result == CSC_EXIT_OK
      && csRowInsert (csv, index, fields, count) != CS_OK)
    {
      result = cscWriteErr (csv);
    }

  cscMemFree (fields);
  csDestroy (row);

  return result;
}

/* Deletes the selected rows, or the columns of the query. */
static CSCExit
cscDelete (CSCsv *csv, CSCSelection *selection, const CSCQuery *query)
{
  if (query->ncolumns == 0)
    {
      // In document order, all in one pass: --sort may have mixed them.
      if (selection->nrows == 0)
        {
          return CSC_EXIT_MISSING;
        }
      if (cscOrder (selection->rows, selection->nrows, cscBeforeAscending,
                    NULL)
          != CSC_EXIT_OK)
        {
          return CSC_EXIT_ERROR;
        }
      csRowRemoveMany (csv, selection->rows, selection->nrows);
      return CSC_EXIT_OK;
    }

  CSCExit result = cscSelectColumns (csv, query, selection);
  if (result == CSC_EXIT_OK
      && cscOrder (selection->columns, selection->ncolumns,
                   cscBeforeDescending, NULL)
             != CSC_EXIT_OK)
    {
      result = CSC_EXIT_ERROR;
    }
  for (CSSize j = 0; result == CSC_EXIT_OK && j < selection->ncolumns; ++j)
    {
      csColumnRemove (csv, selection->columns[j]);
    }

  return result;
}

/* Edits, then prints the table or saves it to its file. A missing target
 * leaves the file as it is. */
static CSCExit
cscEdit (CSCsv *csv, const CSCArgs *args, CSCFilter *filters,
         const CSCQuery *query)
{
  CSCSelection selection = { NULL, 0, NULL, 0 };
  CSCExit result = CSC_EXIT_OK;
  if (args->mode == CSC_MODE_ADD)
    {
      result = cscAddRow (csv, args, query);
    }
  else
    {
      result = cscSelectRows (csv, args, filters, query, &selection);
      if (result == CSC_EXIT_OK)
        {
          result = args->mode == CSC_MODE_SET
                       ? cscSetCells (csv, args, &selection, query)
                       : cscDelete (csv, &selection, query);
        }
    }
  cscSelectionFree (&selection);

  if (result == CSC_EXIT_ERROR)
    {
      return result;
    }

  if (!args->write)
    {
      CSCExit printed = cscPrintAll (csv, args);
      return printed == CSC_EXIT_OK ? result : printed;
    }

  if (result != CSC_EXIT_OK)
    {
      return result;
    }

  result = cscPrepare (csv, args);
  if (result == CSC_EXIT_OK && csSave (csv, args->paths[0]) != CS_OK)
    {
      // The message names the file already.
      result = cscWriteErr (csv);
    }

  return result;
}

static CSCExit
cscRun (const CSCArgs *args)
{
  CSCQuery query = { NULL,
                     CS_FALSE,
                     CS_FALSE,
                     { CS_FALSE, CS_FALSE, 0 },
                     { CS_FALSE, CS_FALSE, 0 },
                     NULL,
                     NULL,
                     0,
                     { NULL, 0 },
                     CS_FALSE,
                     NULL };
  CSChar *order = NULL;
  CSCFilter *filters = cscMemAlloc (args->nfilters, sizeof (*filters));
  for (CSSize i = 0; filters && i < args->nfilters; ++i)
    {
      filters[i].buffer = NULL;
    }
  CSCExit result
      = filters ? cscParseQuery (args->query, &query) : cscFailMem ();
  for (CSSize i = 0; result == CSC_EXIT_OK && i < args->nfilters; ++i)
    {
      result = cscParseFilter (args->filters[i], &filters[i]);
    }
  if (result == CSC_EXIT_OK && args->sort)
    {
      result = cscParseOrder (args->sort, &query, &order);
    }
  if (result == CSC_EXIT_OK)
    {
      result = cscCheck (args, &query);
    }

  CSCsv *csv = NULL;
  if (result == CSC_EXIT_OK && !(csv = csInit (args->flags)))
    {
      result = cscFailMem ();
    }
  if (result == CSC_EXIT_OK)
    {
      csSetDelimiter (csv, args->delimiter);
      result = cscLoadAll (csv, args);
    }
  if (result == CSC_EXIT_OK)
    {
      switch (args->mode)
        {
        case CSC_MODE_QUERY:
        case CSC_MODE_COUNT:
          result = cscQuery (csv, args, filters, &query);
          break;
        case CSC_MODE_KEYS:
          result = cscKeys (csv, args, &query);
          break;
        case CSC_MODE_SET:
        case CSC_MODE_ADD:
        case CSC_MODE_DELETE:
          result = cscEdit (csv, args, filters, &query);
          break;
        }
    }

  csDestroy (csv);
  for (CSSize i = 0; filters && i < args->nfilters; ++i)
    {
      cscMemFree (filters[i].buffer);
    }
  cscMemFree (filters);
  cscMemFree (order);
  cscMemFree (query.buffer);
  cscMemFree (query.columns);

  return result;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Definitions - Public
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

CSInt
main (CSInt argc, CSChar *argv[])
{
  cscStreamInit ();

  CSCArgs args = { CS_FHEADER,
                   CSC_MODE_QUERY,
                   CSC_FORMAT_CSV,
                   CS_FALSE,
                   CS_FALSE,
                   CS_FALSE,
                   ',',
                   '\0',
                   NULL,
                   ".",
                   NULL,
                   CS_FALSE,
                   NULL,
                   0,
                   NULL,
                   0 };
  args.paths = cscMemAlloc ((CSSize)argc + 1, sizeof (*args.paths));
  args.filters = cscMemAlloc ((CSSize)argc, sizeof (*args.filters));
  if (!args.paths || !args.filters)
    {
      cscMemFree (args.paths);
      cscMemFree (args.filters);
      return (CSInt)cscFailMem ();
    }

  CSBool done = CS_FALSE;
  CSCExit result = cscParseArgs (argc, argv, &args, &done);
  if (result == CSC_EXIT_OK && !done)
    {
      result = cscRun (&args);
    }
  cscMemFree (args.paths);
  cscMemFree (args.filters);

  if (!cscFlush ())
    {
      cscError ("cisivi: standard output: %s\n",
                cscSysErrorText (cscSysError ()));
      return CSC_EXIT_ERROR;
    }

  return (CSInt)result;
}
