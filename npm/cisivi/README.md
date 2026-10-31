# @partoska/cisivi

Cisivi — query, filter and edit CSV files from the command line. Shell
scripts and pipelines read and change tables safely, quoted commas and line
breaks included.

[![npm version](https://img.shields.io/npm/v/@partoska/cisivi.svg)](https://www.npmjs.com/package/@partoska/cisivi)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](https://github.com/partoska/cisivi/blob/main/LICENSE.txt)

## What is this?

`cisivi` reads CSV files and prints what a query selects: the whole table,
some columns, some rows, or single values.

**Key features:**

- Simple queries: `.`, `COLUMN,...`, `#N`, `[ROWS]`, `[ROWS].COLUMN`, with
  rows counted from the start or the end.
- Filters (`=`, `!=`, `<`, `>=`, `~`, ...) and sorting, numbers by value.
- Output as CSV, raw values, JSON, or an aligned table.
- Counts and column lists that line breaks inside values do not upset.
- Joins several files by column name.
- Sets, appends and deletes, printing the result or writing it back in
  place.
- Any delimiter: comma, semicolon, tab.
- Meaningful exit codes: `0` found, `1` not found, `2` usage or input error.
- Works on macOS, Linux (x64, arm64) and Windows (x64, arm64).

## Install

```bash
npm install -g @partoska/cisivi
```

This installs a small launcher plus the prebuilt native binary for your
platform, selected automatically. No compiler or build tools are required — see
[How this package works](#how-this-package-works) for the details.

### Supported platforms

| OS      | Architectures              |
| ------- | -------------------------- |
| macOS   | Intel & Apple Silicon      |
| Linux   | x64, arm64                 |
| Windows | x64, arm64                 |

## Quick Start

```bash
cisivi email users.csv                    # Print one column, a value per line.
cisivi '[0].email' users.csv              # Print one value.
cisivi -f 'age>30' name,email users.csv   # Print the matching rows, as CSV.
cisivi --sort-desc age '[:3].name' users.csv
cisivi -t . users.csv                     # Show the table aligned.
cisivi -j . users.csv                     # Print JSON, an object per row.
cat users.csv | cisivi -c                 # Count rows from standard input.
cisivi -w -s done -f id=42 status tasks.csv
```

Run `cisivi -h` at any time for full usage.

## Usage

```bash
cisivi [OPTIONS] [QUERY] [FILE...]
```

Reads `FILE`, or standard input when `FILE` is missing or `-`. The query
always comes first: `cisivi . users.csv` for the whole table.

| Query               | Prints                                       |
| ------------------- | -------------------------------------------- |
| `.` (default)       | the whole table, as CSV                      |
| `COLUMN`            | the values of one column, one per line       |
| `COLUMN,COLUMN,...` | those columns, as CSV                        |
| `#N`                | a column by position, from 0                 |
| `[ROWS]`            | `[N]`, `[-N]`, `[N:M]`, `[N:]`, `[:M]`, `[*]` |
| `[ROWS].COLUMN,...` | both, such as `[0].name`                     |

| Flag      | Long                   | Meaning                                                  |
| --------- | ---------------------- | -------------------------------------------------------- |
| `-f`      | `--filter EXPR`        | Keep rows where `COLUMN OP VALUE` holds; repeatable.     |
|           | `--sort COLUMN`        | Order rows by `COLUMN`; `--sort-desc` descending.        |
| `-k`      | `--keys`               | List the column names.                                   |
| `-c`      | `--count`              | Print the number of rows selected.                       |
| `-s`      | `--set VALUE`          | Set `COLUMN` in the selected rows; print the table.      |
| `-a`      | `--add ROW`            | Append one CSV record, or insert it at `[N]`; print.     |
| `-d`      | `--delete`             | Delete the selected rows or columns; print the table.    |
| `-w`      | `--write`              | Save the edit back to the single `FILE` instead.         |
| `-j`      | `--json`               | Print JSON.                                              |
| `-t`      | `--table`              | Print an aligned table.                                  |
| `-0`      | `--null`               | End values and names with NUL.                           |
| `-F`      | `--delimiter C`        | Read fields separated by `C` (`;`, `tab`, ...).          |
|           | `--output-delimiter C` | Write fields separated by `C`.                           |
|           | `--crlf`               | End written rows with CRLF.                              |
| `-n`      | `--no-header`          | The first row is data; columns are `#N`.                 |
| `-i`      | `--ignore-case`        | Match column names case-insensitively.                   |
| `-h`      | `--help`               | Show the help.                                           |
| `-V`      | `--version`            | Show the version.                                        |

Exit status: `0` found, `1` not found, `2` usage or input error (including
syntax errors in the CSV text, reported with their line on standard error).

## How this package works

`@partoska/cisivi` follows the per-platform `optionalDependencies` model. The
package you install ships only a tiny Node launcher; the actual native binary
comes from one of these platform packages, and npm installs only the one
matching your OS and CPU:

- `@partoska/cisivi-darwin` — macOS universal (Intel + Apple Silicon)
- `@partoska/cisivi-linux-x64`
- `@partoska/cisivi-linux-arm64`
- `@partoska/cisivi-win32-x64`
- `@partoska/cisivi-win32-arm64`

When you run `cisivi`, the launcher runs the matching binary with the standard
streams passed through and exits with its status, so pipes, redirections and
exit codes behave exactly as with the binary itself.

## Troubleshooting

### "Unsupported platform" or "could not find the native binary"

This usually means optional dependencies were skipped during install (for
example with `npm install --omit=optional`, or behind a strict CI mirror).
Reinstall with optional dependencies enabled:

```bash
npm install -g @partoska/cisivi
```

## License

MIT. Copyright (C) 2026 Fabrika Charvat s.r.o. Developed by the
[Partoska Laboratory](https://lab.partoska.com) team.

## Links

- **Project website:** <https://lab.partoska.com/cisivi>
- **Source & issues:** <https://github.com/partoska/cisivi>
