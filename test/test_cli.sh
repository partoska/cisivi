#!/bin/sh
#
# Cisivi - Small, minimal CSV library and query tool.
# Copyright (C) 2026 Fabrika Charvat s.r.o. All rights reserved.
# Developed by Partoska Laboratory team, <https://lab.partoska.com>
#
# MIT License
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#
# You can contact the author(s) via email at ask <at> partoska.com.
#

CISIVI="$1"
FAILURES=0
COUNT=0

if [ -z "$CISIVI" ] || [ ! -x "$CISIVI" ]; then
    echo "Usage: $0 PATH/TO/cisivi" >&2
    exit 2
fi

TMP=$(mktemp -d) || exit 2
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/crew.csv" <<'CSV'
id,name,rank,age,ship
1,Kirk,Captain,34,Enterprise
2,Spock,Commander,35,Enterprise
3,"McCoy, Leonard",Doctor,52,Enterprise
4,Janeway,Captain,41,Voyager
5,Seven,"Astro, metrics",9,Voyager
CSV

cat > "$TMP/more.csv" <<'CSV'
ship,name,species
Defiant,Worf,Klingon
CSV

printf 'ship;captain\r\nDefiant;Sisko\r\n"Rio Grande";"Dax; Jadzia"\r\n' > "$TMP/semi.csv"
printf 'log,"stardate, local"\n"first\nsecond",41153.7\n' > "$TMP/log.csv"

cat > "$TMP/emoji.csv" <<'CSV'
name,mood
smile 😀,happy
family 👨‍👩‍👧,"busy, 🎉"
heart ❤️,warm
CSV

# edit NAME [SOURCE]: a fresh copy of crew.csv, or SOURCE, to edit in place
edit() {
    cp "$TMP/${2:-crew}.csv" "$TMP/$1.csv"
    echo "$TMP/$1.csv"
}

# expect NAME STATUS EXPECTED-OUTPUT COMMAND...
expect() {
    name="$1"
    status="$2"
    expected="$3"
    shift 3
    COUNT=$((COUNT + 1))
    actual=$("$@" 2>/dev/null)
    code=$?
    if [ "$code" -ne "$status" ] || [ "$actual" != "$expected" ]; then
        echo "  $name: FAIL (status $code, expected $status)"
        echo "    expected: $expected"
        echo "    actual:   $actual"
        FAILURES=$((FAILURES + 1))
    else
        echo "  $name: ok"
    fi
}

CREW="$TMP/crew.csv"

expect "column" 0 "Kirk
Spock
McCoy, Leonard
Janeway
Seven" "$CISIVI" name "$CREW"
expect "column from stdin" 0 "Enterprise
Enterprise
Enterprise
Voyager
Voyager" sh -c "\"$CISIVI\" ship < \"$CREW\""
expect "column from pipe" 0 "34" sh -c "cat \"$CREW\" | \"$CISIVI\" '[0].age' -"
expect "columns" 0 "name,ship
Kirk,Enterprise
Spock,Enterprise
\"McCoy, Leonard\",Enterprise
Janeway,Voyager
Seven,Voyager" "$CISIVI" name,ship "$CREW"
expect "whole table" 0 "id,name,rank,age,ship
1,Kirk,Captain,34,Enterprise
2,Spock,Commander,35,Enterprise
3,\"McCoy, Leonard\",Doctor,52,Enterprise
4,Janeway,Captain,41,Voyager
5,Seven,\"Astro, metrics\",9,Voyager" "$CISIVI" . "$CREW"
expect "default query" 0 "4" sh -c "\"$CISIVI\" < \"$CREW\" | \"$CISIVI\" '[3].id'"
expect "position" 0 "Captain" "$CISIVI" '[0].#2' "$CREW"
expect "leading dot" 0 "Spock" "$CISIVI" '.[1].name' "$CREW"
expect "ignore case" 0 "Kirk" "$CISIVI" -i '[0].NAME' "$CREW"
expect "case matters" 1 "" "$CISIVI" '[0].NAME' "$CREW"
expect "missing column" 1 "" "$CISIVI" captain "$CREW"
expect "missing file" 2 "" "$CISIVI" name "$TMP/borg.csv"
expect "syntax error" 2 "" sh -c "printf 'a,b\n1\n' | \"$CISIVI\" ."
expect "empty input" 2 "" sh -c "printf '' | \"$CISIVI\" ."
expect "NUL from stdin" 2 "" sh -c "printf 'a,b\\n1,2\\000\\n' | \"$CISIVI\" ."
expect "unreadable stdin" 2 "" sh -c "\"$CISIVI\" . < /"
expect "multi-line value" 0 "first
second" "$CISIVI" '[0].log' "$TMP/log.csv"
expect "multi-line quoted" 0 "log,\"stardate, local\"
\"first
second\",41153.7" "$CISIVI" . "$TMP/log.csv"

# Rows
expect "row" 0 "id,name,rank,age,ship
3,\"McCoy, Leonard\",Doctor,52,Enterprise" "$CISIVI" '[2]' "$CREW"
expect "row from end" 0 "Janeway" "$CISIVI" '[-2].name' "$CREW"
expect "row range" 0 "2
3" "$CISIVI" '[1:3].id' "$CREW"
expect "row range open end" 0 "4
5" "$CISIVI" '[3:].id' "$CREW"
expect "row range open start" 0 "1
2" "$CISIVI" '[:2].id' "$CREW"
expect "row range from end" 0 "4
5" "$CISIVI" '[-2:].id' "$CREW"
expect "all rows" 0 "1
2
3
4
5" "$CISIVI" '[*].id' "$CREW"
expect "row out of range" 1 "" "$CISIVI" '[5].id' "$CREW"
expect "row out of range prints header" 1 "id,name" "$CISIVI" '[-6].id,name' "$CREW"
expect "empty range" 1 "" "$CISIVI" '[3:1].id' "$CREW"
expect "invalid row" 2 "" "$CISIVI" '[x]' "$CREW"
expect "text after rows" 2 "" "$CISIVI" '[0]name' "$CREW"

# Quoting
printf 'a,"b,c",d\n1,2,3\n' > "$TMP/comma.csv"
expect "whole name wins" 0 "2" "$CISIVI" 'b,c' "$TMP/comma.csv"
expect "quoted name" 0 "\"b,c\",a
2,1" "$CISIVI" '"b,c",a' "$TMP/comma.csv"
expect "escaped quote" 0 "9" sh -c "printf '\"say \"\"engage\"\"\",warp\n1,9\n' | \"$CISIVI\" -f '\"say \\\"engage\\\"\"=1' warp"
expect "unterminated quote" 2 "" "$CISIVI" '"name' "$CREW"
expect "quote inside a name" 2 "" "$CISIVI" 'na"me' "$CREW"
expect "text after a quote" 2 "" "$CISIVI" '"name"x' "$CREW"
expect "empty name" 2 "" "$CISIVI" 'name,' "$CREW"
expect "column twice" 2 "" "$CISIVI" 'name,#1' "$CREW"

# Filters and sorting
expect "filter equal" 0 "Janeway
Seven" "$CISIVI" -f ship=Voyager name "$CREW"
expect "filter double equal" 0 "Kirk
Janeway" "$CISIVI" -f rank==Captain name "$CREW"
expect "filter not equal" 0 "Spock
McCoy, Leonard
Seven" "$CISIVI" -f 'rank!=Captain' name "$CREW"
expect "filter numbers" 0 "Seven" "$CISIVI" -f 'age<10' name "$CREW"
expect "filter at least" 0 "Spock
McCoy, Leonard
Janeway" "$CISIVI" -f 'age>=35' name "$CREW"
expect "filter text order" 0 "Spock
Seven" "$CISIVI" -f 'name>R' name "$CREW"
expect "filter contains" 0 "Spock
McCoy, Leonard" "$CISIVI" -f 'name~o' -f 'name!~J' -f 'ship~prise' name "$CREW"
expect "filter empty value" 0 "1" sh -c "printf 'a,b\n1,\n2,x\n' | \"$CISIVI\" -f 'b=' a"
expect "filter by position" 0 "Seven" "$CISIVI" -f '#3<10' name "$CREW"
expect "filter then rows" 0 "Seven" "$CISIVI" -f ship=Voyager '[-1].name' "$CREW"
expect "filter matches nothing" 1 "id,name,rank,age,ship" "$CISIVI" -f ship=Defiant . "$CREW"
expect "filter missing column" 1 "" "$CISIVI" -f 'species=Vulcan' name "$CREW"
expect "filter no operator" 2 "" "$CISIVI" -f 'age' name "$CREW"
expect "sort" 0 "Seven
Kirk
Spock
Janeway
McCoy, Leonard" "$CISIVI" --sort age name "$CREW"
expect "sort descending" 0 "McCoy, Leonard" "$CISIVI" --sort-desc age '[0].name' "$CREW"
expect "sort text, stable" 0 "5
1
4
2
3" "$CISIVI" --sort rank id "$CREW"
expect "sort by position" 0 "Spock" "$CISIVI" --sort '#1' '[-1].name' "$CREW"
expect "sort filtered" 0 "Seven,Janeway" sh -c "\"$CISIVI\" -f ship=Voyager --sort age name \"$CREW\" | paste -sd, -"
expect "sort mixed" 0 "-1,9,10,1e1,B,a,b" sh -c "printf 'v\n10\nb\n9\na\n-1\n1e1\nB\n' | \"$CISIVI\" --sort v v | paste -sd, -"
expect "sort missing" 1 "" "$CISIVI" --sort species name "$CREW"

# Listing and counting
expect "keys" 0 "id
name
rank
age
ship" "$CISIVI" -k . "$CREW"
expect "keys of columns" 0 "ship
id" "$CISIVI" --keys 'ship,#0' "$CREW"
expect "keys without header" 0 "#0
#1" sh -c "printf 'a,b\n' | \"$CISIVI\" -n -k"
expect "count" 0 "2" "$CISIVI" -c -f ship=Voyager . "$CREW"
expect "count nothing" 1 "0" "$CISIVI" --count -f ship=Defiant . "$CREW"
expect "count multi-line" 0 "1" "$CISIVI" -c . "$TMP/log.csv"

# Output formats
expect "json" 0 "[
  {\"name\": \"Janeway\", \"rank\": \"Captain\"},
  {\"name\": \"Seven\", \"rank\": \"Astro, metrics\"}
]" "$CISIVI" -j '[-2:].name,rank' "$CREW"
expect "json one column" 0 "[
  {\"id\": \"1\"}
]" "$CISIVI" --json '[0].id' "$CREW"
expect "json escapes" 0 "[
  {\"log\": \"first\\nsecond\", \"stardate, local\": \"41153.7\"}
]" "$CISIVI" -j . "$TMP/log.csv"
expect "json empty" 1 "[]" "$CISIVI" -j -f ship=Defiant . "$CREW"
expect "json without header" 0 "[
  [\"a\", \"b\"]
]" sh -c "printf 'a,b\n' | \"$CISIVI\" -n -j"
expect "table" 0 "id  name            age
--  --------------  ---
3   McCoy, Leonard  52
4   Janeway         41" "$CISIVI" -t '[2:4].id,name,age' "$CREW"
expect "table escapes" 0 "log            stardate, local
-------------  ---------------
first\\nsecond  41153.7" "$CISIVI" -t . "$TMP/log.csv"
expect "table without header" 0 "#0  #1
--  --
a   b" sh -c "printf 'a,b\n' | \"$CISIVI\" -n -t"
# Emoji pass through as bytes, given in files and pipes; -t aligns UTF-8
# characters, not terminal cells, so only its cells are compared.
EMOJI="$TMP/emoji.csv"
expect "emoji column" 0 "smile 😀
family 👨‍👩‍👧
heart ❤️" "$CISIVI" name "$EMOJI"
expect "emoji round trip" 0 "$(cat "$EMOJI")" "$CISIVI" . "$EMOJI"
expect "emoji from stdin" 0 "busy, 🎉" sh -c "\"$CISIVI\" '[1].mood' < \"$EMOJI\""
expect "emoji filtered" 0 "heart ❤️" "$CISIVI" -f mood=warm name "$EMOJI"
expect "emoji json" 0 "[
  {\"name\": \"family 👨‍👩‍👧\", \"mood\": \"busy, 🎉\"}
]" "$CISIVI" -j '[1]' "$EMOJI"
expect "emoji table" 0 "smile 😀
family 👨‍👩‍👧
heart ❤️" sh -c "\"$CISIVI\" -t name \"$EMOJI\" | tail -n +3"
expect "exclusive formats" 2 "" "$CISIVI" -j -t . "$CREW"
expect "null" 0 "Kirk|Spock|" sh -c "\"$CISIVI\" -0 '[:2].name' \"$CREW\" | tr '\\000' '|'"

# Delimiters and line endings
expect "delimiter" 0 "Dax; Jadzia" "$CISIVI" -F ';' '[1].captain' "$TMP/semi.csv"
expect "delimiter kept" 0 "ship;captain
Defiant;Sisko
Rio Grande;\"Dax; Jadzia\"" "$CISIVI" -F ';' . "$TMP/semi.csv"
expect "output delimiter" 0 "ship,captain
Defiant,Sisko
Rio Grande,Dax; Jadzia" "$CISIVI" -F ';' --output-delimiter , . "$TMP/semi.csv"
expect "tab" 0 "id	name
1	Kirk" "$CISIVI" --output-delimiter tab '[0].id,name' "$CREW"
expect "crlf" 0 "id,name|1,Kirk|" sh -c "\"$CISIVI\" --crlf '[0].id,name' \"$CREW\" | tr '\\r\\n' '|' | tr -s '|'"
expect "invalid delimiter" 2 "" "$CISIVI" -F '"' . "$CREW"
expect "long delimiter" 2 "" "$CISIVI" -F ';;' . "$CREW"

# Without a header
expect "no header" 0 "id
1" "$CISIVI" -n '[:2].#0' "$CREW"
expect "no header names" 1 "" "$CISIVI" -n name "$CREW"

# Several files
expect "joined" 0 "id,name,rank,age,ship,species
4,Janeway,Captain,41,Voyager,
5,Seven,\"Astro, metrics\",9,Voyager,
,Worf,,,Defiant,Klingon" "$CISIVI" '[-3:]' "$CREW" "$TMP/more.csv"
expect "joined stdin" 0 "Klingon" sh -c "\"$CISIVI\" -f name=Worf species \"$CREW\" - < \"$TMP/more.csv\""
expect "joined without header" 0 "4" sh -c "printf 'a;b\n' | \"$CISIVI\" -n -c . - \"$TMP/semi.csv\" -F ';'"
expect "joined width" 2 "" "$CISIVI" -n . "$CREW" "$TMP/more.csv"
expect "joined missing file" 2 "" "$CISIVI" . "$CREW" "$TMP/borg.csv"

# Editing to standard output
expect "set" 0 "Admiral" sh -c "\"$CISIVI\" -s Admiral -f name=Kirk rank \"$CREW\" | \"$CISIVI\" '[0].rank'"
expect "set rows" 0 "x
x
Doctor" sh -c "\"$CISIVI\" --set x '[:2].rank' \"$CREW\" | \"$CISIVI\" '[:3].rank'"
expect "set every row" 0 "1" sh -c "\"$CISIVI\" -s Starfleet ship \"$CREW\" | \"$CISIVI\" -f ship=Starfleet -c -f id=3"
expect "set new column" 0 ",,,yes,yes" sh -c "\"$CISIVI\" -s yes -f ship=Voyager delta \"$CREW\" | \"$CISIVI\" delta | tr '\\n' ',' | sed 's/,\$//'"
expect "set quoted value" 0 "\"a, \"\"b\"\"\"" sh -c "\"$CISIVI\" -s 'a, \"b\"' '[0].name' \"$CREW\" | sed -n 2p | cut -d, -f2-3"
expect "set value like option" 0 "-1" sh -c "\"$CISIVI\" -s -1 '[0].age' \"$CREW\" | \"$CISIVI\" '[0].age'"
expect "set nothing selected" 1 "" sh -c "\"$CISIVI\" -s x -f ship=Defiant rank \"$CREW\" > /dev/null"
expect "set no column" 2 "" "$CISIVI" -s x '[0]' "$CREW"
expect "set two columns" 2 "" "$CISIVI" -s x name,rank "$CREW"
expect "set missing position" 1 "" sh -c "\"$CISIVI\" -s x '#9' \"$CREW\" > /dev/null"
expect "set no value" 2 "" "$CISIVI" name "$CREW" -s
expect "add" 0 "Picard, J.L." sh -c "\"$CISIVI\" -a '6,\"Picard, J.L.\",Captain,59,Enterprise' . \"$CREW\" | \"$CISIVI\" '[-1].name'"
expect "add with delimiter" 0 "Kira" sh -c "\"$CISIVI\" -F ';' --add 'Deep Space Nine;Kira' . \"$TMP/semi.csv\" | \"$CISIVI\" -F ';' '[-1].captain'"
expect "add wrong width" 2 "" "$CISIVI" -a '6,Picard' . "$CREW"
expect "add nothing" 2 "" "$CISIVI" -a '' . "$CREW"
expect "add two records" 2 "" "$CISIVI" -a '6
7' . "$CREW"
expect "add with query" 2 "" "$CISIVI" -a 'x' name "$CREW"
expect "insert first" 0 "6,1,2,3,4,5" sh -c "\"$CISIVI\" -a '6,Picard,Captain,59,Enterprise' '[0]' \"$CREW\" | \"$CISIVI\" id | paste -sd, -"
expect "insert at the end" 0 "1,2,3,4,5,6" sh -c "\"$CISIVI\" -a '6,Picard,Captain,59,Enterprise' '[5]' \"$CREW\" | \"$CISIVI\" id | paste -sd, -"
expect "insert from the end" 0 "1,2,3,4,6,5" sh -c "\"$CISIVI\" -a '6,Picard,Captain,59,Enterprise' '[-1]' \"$CREW\" | \"$CISIVI\" id | paste -sd, -"
expect "insert beyond" 1 "" sh -c "\"$CISIVI\" -a '6,Picard,Captain,59,Enterprise' '[6]' \"$CREW\" > /dev/null"
expect "insert beyond prints" 0 "5" sh -c "\"$CISIVI\" -a '6,Picard,Captain,59,Enterprise' '[6]' \"$CREW\" | \"$CISIVI\" -c ."
expect "insert beyond the start" 1 "" sh -c "\"$CISIVI\" -a '6,Picard,Captain,59,Enterprise' '[-6]' \"$CREW\" > /dev/null"
expect "insert wrong width" 2 "" "$CISIVI" -a '6,Picard' '[0]' "$CREW"
expect "insert rows" 2 "" "$CISIVI" -a '6,Picard,Captain,59,Enterprise' '[1:2]' "$CREW"
expect "insert column" 2 "" "$CISIVI" -a '6,Picard,Captain,59,Enterprise' '[1].name' "$CREW"
expect "insert filtered" 2 "" "$CISIVI" -a '6,Picard,Captain,59,Enterprise' -f id=1 '[0]' "$CREW"
expect "insert without header" 0 "a,b
x,y" sh -c "printf 'x,y\n' | \"$CISIVI\" -n -a 'a,b' '[0]'"
expect "delete row" 0 "Spock" sh -c "\"$CISIVI\" -d '[0]' \"$CREW\" | \"$CISIVI\" '[0].name'"
expect "delete filtered" 0 "Enterprise
Enterprise
Enterprise" sh -c "\"$CISIVI\" -d -f ship=Voyager . \"$CREW\" | \"$CISIVI\" ship"
expect "delete sorted" 0 "Kirk,Spock,McCoy, Leonard,Janeway" sh -c "\"$CISIVI\" -d --sort age '[0]' \"$CREW\" | \"$CISIVI\" name | paste -sd, -"
expect "delete sorted rows" 0 "Kirk,Seven" sh -c "\"$CISIVI\" -d --sort-desc age '[:3]' \"$CREW\" | \"$CISIVI\" name | paste -sd, -"
expect "delete columns" 0 "id,ship
1,Enterprise" sh -c "\"$CISIVI\" -d name,rank,age \"$CREW\" | \"$CISIVI\" '[0]'"
expect "delete missing row" 1 "id,name,rank,age,ship
1,Kirk,Captain,34,Enterprise
2,Spock,Commander,35,Enterprise
3,\"McCoy, Leonard\",Doctor,52,Enterprise
4,Janeway,Captain,41,Voyager
5,Seven,\"Astro, metrics\",9,Voyager" "$CISIVI" -d -f ship=Defiant . "$CREW"
expect "delete missing column" 1 "" sh -c "\"$CISIVI\" -d species \"$CREW\" > /dev/null"
expect "delete everything" 2 "" "$CISIVI" -d . "$CREW"
expect "delete rows and columns" 2 "" "$CISIVI" -d '[0].name' "$CREW"
expect "edit as json" 0 "[
  {\"id\": \"5\"}
]" sh -c "\"$CISIVI\" -j -d -f 'id<5' '[*]' \"$CREW\" | sed 's/, \"name.*\"}/}/'"
expect "exclusive modes" 2 "" "$CISIVI" -k -d name "$CREW"

# Editing in place
FILE=$(edit set)
expect "write set" 0 "99" sh -c "\"$CISIVI\" -w -s 99 -f id=1 age \"$FILE\" && \"$CISIVI\" '[0].age' \"$FILE\""
FILE=$(edit add)
expect "write add" 0 "6" sh -c "\"$CISIVI\" -w -a '6,Picard,Captain,59,Enterprise' . \"$FILE\" && \"$CISIVI\" -c . \"$FILE\""
FILE=$(edit insert)
expect "write insert" 0 "Picard" sh -c "\"$CISIVI\" -w -a '6,Picard,Captain,59,Enterprise' '[1]' \"$FILE\" && \"$CISIVI\" '[1].name' \"$FILE\""
FILE=$(edit delete)
expect "write delete" 0 "id,name" sh -c "\"$CISIVI\" -w -d rank,age,ship \"$FILE\" && sed -n 1p \"$FILE\""
FILE=$(edit semi-write semi)
expect "write keeps delimiter" 0 "ship;captain
Defiant;Worf
Rio Grande;\"Dax; Jadzia\"" sh -c "\"$CISIVI\" -F ';' -w -s Worf '[0].captain' \"$FILE\" && cat \"$FILE\""
FILE=$(edit crlf semi)
expect "write crlf" 0 "3" sh -c "\"$CISIVI\" -F ';' --crlf -w -s Worf '[0].captain' \"$FILE\" && tr -cd '\\r' < \"$FILE\" | wc -c | tr -d ' '"
FILE=$(edit missing)
expect "write missing" 1 "" "$CISIVI" -w -d -f ship=Defiant . "$FILE"
COUNT=$((COUNT + 1))
# cksum, not cmp: Git for Windows on arm64 has no cmp
if [ "$(cksum < "$CREW")" = "$(cksum < "$FILE")" ]; then
    echo "  write missing leaves file: ok"
else
    echo "  write missing leaves file: FAIL"
    FAILURES=$((FAILURES + 1))
fi
expect "write without edit" 2 "" "$CISIVI" -w name "$CREW"
expect "write stdin" 2 "" sh -c "\"$CISIVI\" -w -s 1 age < \"$CREW\""
expect "write two files" 2 "" "$CISIVI" -w -s 1 age "$CREW" "$TMP/more.csv"
expect "write json" 2 "" "$CISIVI" -w -j -s 1 age "$CREW"
expect "write unwritable" 2 "" "$CISIVI" -w -s 1 age /

expect "unknown option" 2 "" "$CISIVI" --nope
expect "version" 0 "cisivi 1.0.1" "$CISIVI" --version
expect "help banner" 0 "Cisivi v1.0.1, <https://lab.partoska.com/cisivi>
Copyright (C) 2026 Fabrika Charvat s.r.o. All rights reserved.
Licensed under the MIT License." sh -c "\"$CISIVI\" --help | sed -n '6,8p'"

echo
echo "$COUNT tests, $FAILURES failed"
[ "$FAILURES" -eq 0 ]
