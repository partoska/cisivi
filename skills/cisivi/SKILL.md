---
name: cisivi
description: Use the cisivi CLI to read, filter, sort, count, convert, join, set, append, and delete data in CSV files from shell scripts and pipelines. Use when the user wants to query or edit CSV, TSV, or semicolon-separated files, extract columns or values, print a table as JSON or aligned text, or script against tabular data.
license: MIT
metadata:
  author: Partoska Laboratory
  version: "1.0.0"
---

# Cisivi (cisivi)

Cisivi queries and edits CSV files from the command line. It parses quoted fields properly (commas, quotes and line breaks inside values) and prints exactly what a query selects, so shell scripts can work with tables without splitting lines by hand.

## Mental model

- The first row names the **columns** (`-n` makes it data; columns are then `#0`, `#1`, ...). Data **rows** are counted from `0`, the header excluded.
- A **query** is the first operand and selects columns, rows, or both. The default query is `.`, the whole table. **Always put the query before the files**: `cisivi . data.csv`, never `cisivi data.csv` (that reads standard input with the query `data.csv`).
- **Output**: a query naming exactly one column prints its raw values, one per line. Anything else prints CSV with the header, which can be piped into another `cisivi`. `-j` prints JSON, `-t` an aligned table.
- **Selection order**: filters (`-f`, all must hold), then sorting (`--sort`), then the rows of the query (`[N]`, `[N:M]`). So `[0]` is the first match after sorting.
- **Input** is every `FILE` given, or standard input when there is none or the file is `-`. Several files are joined in order with columns matched by name; a column missing from a file is left empty.
- **Exit status** carries the answer: `0` found, `1` not found (missing column, or no row selected), `2` usage or input error. Errors go to standard error, never to standard output.
- **Edits** (`-s`, `-a`, `-d`) print the whole edited table, or write it back to its single file with `-w`.

## Installation

Cisivi is available for macOS, Linux, and Windows. If the environment cannot run installers, portable binaries or building from source are the fallbacks.

### Step 1: Install cisivi

#### Option A: Package manager (recommended)

Homebrew on macOS or Linux (x86_64 / arm64):

```bash
brew install partoska/tap/cisivi
```

Scoop on Windows (x64 / arm64):

```powershell
scoop bucket add partoska https://github.com/partoska/scoop-bucket
scoop install partoska/cisivi
```

npm on macOS, Linux (x64, arm64), or Windows (x64, arm64):

```bash
npm install -g @partoska/cisivi
```

Prefer npm when Node.js is already available; prefer Homebrew or Scoop when it is already the system's package manager.

#### Option B: Platform-specific binary from GitHub Releases

Download the latest release from GitHub:

**https://github.com/partoska/cisivi/releases**

If network access is available and you want exact commands, resolve the latest release via the GitHub Releases API rather than inventing versioned URLs:

```bash
curl -L https://api.github.com/repos/partoska/cisivi/releases/latest
```

Select the asset from `assets[].name`, use its `browser_download_url`, and verify against its `digest` when available. Do **not** hard-code a version or construct a release URL by guessing.

#### Choose the right asset

| Platform | Preferred interactive install | Portable/headless fallback |
|---|---|---|
| macOS | `cisivi_<version>_darwin_universal.pkg` | `cisivi_<version>_darwin_universal.tar.gz` or bare `cisivi_<version>_darwin_universal` |
| Debian/Ubuntu amd64 | `cisivi_<version>_linux_amd64.deb` | `cisivi_<version>_linux_amd64.tar.gz` or bare `cisivi_<version>_linux_amd64` |
| Debian/Ubuntu arm64 | `cisivi_<version>_linux_arm64.deb` | `cisivi_<version>_linux_arm64.tar.gz` or bare `cisivi_<version>_linux_arm64` |
| Debian/Ubuntu armhf | `cisivi_<version>_linux_armhf.deb` | `cisivi_<version>_linux_armhf.tar.gz` or bare `cisivi_<version>_linux_armhf` |
| Fedora/RHEL amd64 | `cisivi_<version>_linux_amd64.rpm` | `cisivi_<version>_linux_amd64.tar.gz` or bare `cisivi_<version>_linux_amd64` |
| Fedora/RHEL arm64 | `cisivi_<version>_linux_arm64.rpm` | `cisivi_<version>_linux_arm64.tar.gz` or bare `cisivi_<version>_linux_arm64` |
| Windows x64 | `cisivi_<version>_windows_amd64_setup.exe` or `.msi` | `cisivi_<version>_windows_amd64.zip` or bare `cisivi_<version>_windows_amd64.exe` |
| Windows arm64 | `cisivi_<version>_windows_arm64_setup.exe` | `cisivi_<version>_windows_arm64.zip` or bare `cisivi_<version>_windows_arm64.exe` |

Use `uname -s` for OS and `uname -m` for CPU architecture. Map `x86_64` to `amd64`; map `aarch64` or `arm64` to `arm64`; map `armv6l` or `armv7l` to `armhf`. On Linux, inspect `/etc/os-release` to choose `.deb` for Debian/Ubuntu-family systems and `.rpm` for Fedora/RHEL-family systems. On Windows, `$env:PROCESSOR_ARCHITECTURE` is `AMD64` or `ARM64`.

#### Install examples

macOS package:

```bash
sudo installer -pkg ./cisivi_<version>_darwin_universal.pkg -target /
```

Debian/Ubuntu:

```bash
sudo apt install ./cisivi_<version>_linux_amd64.deb
```

Fedora/RHEL:

```bash
sudo dnf install ./cisivi_<version>_linux_amd64.rpm
```

Portable tarball (Linux or macOS). The archive extracts into a `cisivi_<version>_<platform>/` directory with the binary somewhere under it in `bin/`, so locate it instead of assuming a path:

```bash
tar -xzf ./cisivi_<version>_<platform>.tar.gz
mkdir -p ~/.local/bin
install -m 755 "$(find ./cisivi_<version>_<platform> -type f -path '*/bin/cisivi')" ~/.local/bin/cisivi
```

Portable bare binary:

```bash
chmod +x ./cisivi_<version>_<platform>
mkdir -p ~/.local/bin
mv ./cisivi_<version>_<platform> ~/.local/bin/cisivi
```

Make sure `~/.local/bin` is on `PATH` when using portable installs. On Windows, put the bare `.exe` (renamed to `cisivi.exe`) in a directory on `PATH`.

#### Verify downloaded assets

When the release API provides a `digest` such as `sha256:<hex>`, verify the downloaded file before installing:

```bash
shasum -a 256 ./cisivi_<version>_<platform>.<ext>
```

Compare the output hash to the asset digest from the release API.

### Step 2: Verify the install

```bash
cisivi --version
```

## Quick reference

```
cisivi [-f EXPR]... [--sort COLUMN | --sort-desc COLUMN] [-k | -c | -s VALUE | -a ROW | -d] [-w]
       [-j | -t] [-0] [-F C] [--output-delimiter C] [--crlf] [-n] [-i] [QUERY] [FILE...]
cisivi -h                                       # full help
cisivi -V                                       # print version
```

### Queries

| Query | Selects |
|---|---|
| `.` (default) | The whole table. |
| `name` | One column: prints its raw values, one per line. |
| `name,email` | Several columns: prints CSV with the header. |
| `#2` | A column by position, from 0. |
| `'"last, first"'` | A quoted column name (with commas or quotes); `\"` and `\\` escape. |
| `'[3]'` | Data row 3 (from 0), as CSV with the header. |
| `'[-1]'` | The last row; `[-N]` counts from the end. |
| `'[2:5]'`, `'[:10]'`, `'[-3:]'`, `'[*]'` | Row ranges, end excluded; `[*]` is every row. |
| `'[0].name'` | One value, raw. |
| `'[-3:].name,email'` | Rows and columns together. |

- A leading dot is optional: `.[0].name` equals `[0].name`.
- Unquoted, the whole column text is tried as one name first, so a column literally named `b,c` is found without quotes; otherwise it splits at commas.
- Quote `[...]` queries for the shell (`'[0].name'`), since brackets are glob characters.

### Filters (`-f`)

`-f 'COLUMN OPERATOR VALUE'`, repeatable (all must hold); the column may be quoted or `#N`.

| Operator | Meaning |
|---|---|
| `=` (or `==`), `!=` | Equal or not, exact text. |
| `<`, `<=`, `>`, `>=` | Ordered: numbers by value when both sides are numbers, other text byte by byte. |
| `~`, `!~` | Contains the value, or not. |

`-f 'note='` keeps rows with an empty `note`. Values are literal; no quoting inside the value.

### Options

| Flag | Long | Meaning |
|---|---|---|
| `-f EXPR` | `--filter EXPR` | Keep rows where `EXPR` holds. |
| | `--sort COLUMN` | Order rows by `COLUMN` (stable; numbers by value). |
| | `--sort-desc COLUMN` | The same, descending. |
| `-k` | `--keys` | List column names (of the query, or all), one per line. |
| `-c` | `--count` | Print the number of selected rows (exit `1` when `0`). |
| `-s VALUE` | `--set VALUE` | Set one `COLUMN` of the selected rows (all by default) to `VALUE`; adds the column when missing. Prints the table. |
| `-a ROW` | `--add ROW` | Append one record, given as CSV text (`'6,"Picard, J.L.",59'`), with query `.`; or insert it as data row `N` with query `[N]` (`[-1]`: before the last row; beyond the end exits `1`). Prints the table. |
| `-d` | `--delete` | Delete the selected rows (needs `[ROWS]` or `-f`), or the named columns (without rows or filters). Prints the table. |
| `-w` | `--write` | With `-s`, `-a`, or `-d`: write the table back to `FILE` instead of printing. Needs exactly one `FILE`; CSV only. |
| `-j` | `--json` | Print a JSON array of objects (arrays with `-n`); all values are strings. |
| `-t` | `--table` | Print an aligned table for reading; line breaks show as `\n`. |
| `-0` | `--null` | End raw values and `-k` names with NUL instead of newline. |
| `-F C` | `--delimiter C` | Read fields separated by `C`: one character, or `tab`. |
| | `--output-delimiter C` | Write fields separated by `C` (default: as read). |
| | `--crlf` | End written rows with CRLF (default LF). |
| `-n` | `--no-header` | The first row is data; address columns as `#N`. |
| `-i` | `--ignore-case` | Match column names ASCII case-insensitively. |

`-k`, `-c`, `-s`, `-a`, and `-d` exclude each other; so do `-j` and `-t`.

### Exit codes

| Code | Meaning |
|---|---|
| `0` | Found (or edit done). |
| `1` | Not found: a column that does not exist (in the query, a filter, or `--sort`), or no row selected. Nothing on standard error. When no row is selected the header (or `[]`) is still printed; an edit still prints the unchanged table but `-w` leaves the file alone. |
| `2` | Usage or input error: bad option or query, unreadable file, CSV syntax error (reported with its line), rows of different widths, empty input, `-w` without exactly one file. |

## Common workflows

One value, with a fallback when it is missing:

```bash
email=$(cisivi -f id=42 '[0].email' users.csv) || email=unknown
```

Distinguish "missing" from "broken":

```bash
if names=$(cisivi -f 'status=failed' name jobs.csv); then
  echo "$names"
else
  case $? in
    1) echo "nothing failed" ;;
    *) echo "cannot read jobs.csv" >&2; exit 2 ;;
  esac
fi
```

Loop over values (use `-0` when values may contain newlines or spaces):

```bash
cisivi -0 email users.csv | xargs -0 -n1 ./notify.sh
cisivi -k . users.csv            # column names
```

Count, top N, and views:

```bash
cisivi -c -f 'age>=18' . users.csv                       # rows, not lines
cisivi --sort-desc score '[:3].name,score' scores.csv    # top three
cisivi -t -f country=CZ name,city users.csv              # aligned, for reading
cisivi -j . users.csv | some-json-tool                   # JSON for other tools
```

Convert delimiters and join files:

```bash
cisivi -F ';' --output-delimiter , . export.csv > clean.csv
cisivi --output-delimiter tab . data.csv > data.tsv
cisivi . january.csv february.csv > quarter.csv          # columns matched by name
```

Edit in place, or write the result elsewhere:

```bash
cisivi -w -s done -f id=42 status tasks.csv              # set one cell
cisivi -w -s pending status tasks.csv                    # set (or add) a column for every row
cisivi -w -a '43,"write docs",open' . tasks.csv           # append a row
cisivi -w -a '1,"read docs",open' '[0]' tasks.csv         # insert as the first data row
cisivi -w -d -f status=done . tasks.csv                  # delete matching rows
cisivi -w -d notes,draft tasks.csv                       # delete columns
cisivi -d '[0]' tasks.csv > rest.csv                     # drop the first data row
```

Pipelines (CSV output is valid CSV with its header):

```bash
cisivi -f country=CZ . users.csv | cisivi --sort age name
curl -s https://example.com/data.csv | cisivi -c
```

## CSV format

```csv
id,name              Fields are separated by commas (or -F); spaces count.
1,"a, ""b"""         Quoted fields may hold the delimiter, doubled quotes
2,"two               and line breaks.
lines"
3,""                 A quoted empty string is one field.
```

- CRLF, LF and CR all end records; blank lines are skipped; a UTF-8 byte order mark is skipped.
- Every row must have as many fields as the header; header names must be non-empty and unique.
- Quotes inside unquoted fields, unterminated quotes, and text after a closing quote are errors (exit `2`, with the line).
- No type conversion: values stay text; only filters and sorting compare numbers by value.

## Guidance for the agent

- **Always give a query before the file**, `.` when you want everything: `cisivi -c . file.csv`, `cisivi -t . file.csv`, `cisivi -a ROW . file.csv`, `cisivi -d -f x=1 . file.csv`.
- **Check the exit status, not the output**, to decide whether something exists: an empty value prints an empty line and exits `0`; a missing column prints nothing and exits `1`; no matching row prints the header (CSV) and exits `1`.
- Quote filters and bracket queries with single quotes in POSIX shells (`-f 'age>30'`, `'[0].name'`): `>` and `<` are redirections, `[` is a glob. On Windows `cmd`, use double quotes.
- **Normalization on write**: `-w` writes LF line endings (pass `--crlf` to keep CRLF), keeps the input delimiter, quotes only fields that need it, and drops blank lines. Warn the user before rewriting a hand-maintained file in place, or write to a new file first and diff.
- `-w` rewrites the file in place, keeping its permissions and links. An edit that selects nothing exits `1` and leaves the file untouched.
- Prefer `cisivi` over `cut`/`awk`/`grep` for CSV: those split quoted commas and multi-line values wrongly.
- For C code, the same parser is available as `libcisivi` (`#include <cisivi.h>`, `pkg-config --cflags --libs cisivi`); see `cisivi.h` in the release packages for the API.
