Cisivi (cisivi)

Installed files:
  /usr/bin/cisivi                  The cisivi command-line tool.
  /usr/lib/libcisivi.a             The CSV library, with its pkg-config file.
  /usr/include/cisivi.h            The library headers.

Quick start:
  cisivi email users.csv           Print one column, a value per line.
  cisivi -f 'age>30' name,email users.csv
                                   Print the matching rows, as CSV.
  cisivi -t . users.csv            Show the table aligned, for reading.
  cisivi -j . users.csv            Print JSON, an object per row.
  cisivi -w -s done -f id=42 status tasks.csv
                                   Set a value in place.
  cat users.csv | cisivi -c        Count rows from standard input.

Exit status: 0 found, 1 not found, 2 usage or input error.

Run "cisivi -h" for full usage, or see https://lab.partoska.com/cisivi.

Cisivi is licensed under the MIT License.
