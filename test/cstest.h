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

/*
 * Minimal in-process test framework: assertions count failures instead of
 * aborting, so one run reports every broken expectation.
 */

#ifndef CSTEST_H
#define CSTEST_H

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Includes
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cstypes.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Helpers
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static CSInt csAssertions = 0;
static CSInt csFailures = 0;

static inline void
csAssertImpl (CSBool cond, const CSChar *expr, const CSChar *file, CSInt line)
{
  ++csAssertions;
  if (!cond)
    {
      fprintf (stderr, "\n    assertion failed: %s (%s:%d)", expr, file, line);
      ++csFailures;
    }
}

static inline CSBool
csStrEq (const CSChar *a, const CSChar *b)
{
  return a && b && strcmp (a, b) == 0;
}

#define CS_ASSERT(cond) csAssertImpl (!!(cond), #cond, __FILE__, __LINE__)
#define CS_ASSERT_EQ(a, b) CS_ASSERT ((a) == (b))
#define CS_ASSERT_STR_EQ(a, b) CS_ASSERT (csStrEq ((a), (b)))

#define CS_RUN(fn)                                                            \
  do                                                                          \
    {                                                                         \
      CSInt failuresBefore = csFailures;                                      \
      printf ("  %-48s", #fn);                                                \
      fflush (stdout);                                                        \
      fn ();                                                                  \
      printf ("%s\n", csFailures == failuresBefore ? "ok" : "FAIL");          \
    }                                                                         \
  while (0)

#define CS_SUITE(name) printf ("\n%s\n", name)

#define CS_SUMMARY()                                                          \
  do                                                                          \
    {                                                                         \
      printf ("\n%d assertions, %d failed\n", csAssertions, csFailures);      \
      return csFailures > 0 ? EXIT_FAILURE : EXIT_SUCCESS;                    \
    }                                                                         \
  while (0)

#endif
