#!/usr/bin/env node

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

"use strict";

const { spawnSync } = require("child_process");

// Map `${process.platform}-${process.arch}` to the package + relative binary.
const TARGETS = {
  "darwin-x64": ["@partoska/cisivi-darwin", "bin/cisivi"],
  "darwin-arm64": ["@partoska/cisivi-darwin", "bin/cisivi"],
  "linux-x64": ["@partoska/cisivi-linux-x64", "bin/cisivi"],
  "linux-arm64": ["@partoska/cisivi-linux-arm64", "bin/cisivi"],
  "win32-x64": ["@partoska/cisivi-win32-x64", "bin/cisivi.exe"],
  "win32-arm64": ["@partoska/cisivi-win32-arm64", "bin/cisivi.exe"],
};

const key = `${process.platform}-${process.arch}`;
const target = TARGETS[key];
if (!target) {
  console.error(
    `cisivi: unsupported platform/architecture "${key}".\n` +
      `Supported: ${Object.keys(TARGETS).join(", ")}.`,
  );
  process.exit(2);
}

const [pkg, relPath] = target;
let binary;
try {
  binary = require.resolve(`${pkg}/${relPath}`);
} catch {
  console.error(
    `cisivi: could not find the native binary for "${key}".\n` +
      `The optional dependency "${pkg}" does not seem to be installed.\n` +
      `Try reinstalling cisivi (e.g. "npm install -g @partoska/cisivi"), and\n` +
      `make sure optional dependencies are not disabled.`,
  );
  process.exit(2);
}

// cisivi is short-lived: run it to completion with the standard streams
// passed through, so pipes and redirections behave as with the binary.
const result = spawnSync(binary, process.argv.slice(2), { stdio: "inherit" });
if (result.error) {
  console.error(`cisivi: failed to launch native binary: ${result.error.message}`);
  process.exit(2);
}

// Mirror the child's exit; 0, 1 and 2 are meaningful to scripts.
if (result.signal) {
  process.kill(process.pid, result.signal);
} else {
  process.exit(result.status === null ? 2 : result.status);
}
