#!/usr/bin/env python3
"""Lower Emscripten wasm2js glue without feeding its huge asm body to Babel."""
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

PLUGINS = [
    "class-static-block", "class-properties", "async-to-generator",
    "async-generator-functions", "regenerator", "arrow-functions",
    "block-scoping", "classes", "function-name", "template-literals",
    "shorthand-properties", "computed-properties", "destructuring", "spread",
    "for-of", "parameters", "object-rest-spread", "optional-chaining",
    "nullish-coalescing-operator", "logical-assignment-operators",
    "numeric-separator", "exponentiation-operator",
]
PLACEHOLDER = "__NUVIO_WASM2JS_BODY__"


def lower(src, dst):
    data = Path(src).read_text()
    # Emscripten 6.0.9 embeds the wasm2js expression in its local API shim.
    # Fail closed if that contract changes. Do not count braces inside JS strings.
    start = re.search(r"this\.exports\s*=\s*(?:\(\s*)?(?=function)", data)
    end = re.search(r"\)?\s*\(info\)\s*;\s*\},\s*instantiate\s*:", data[start.end():]) if start else None
    if not start or not end or PLACEHOLDER in data:
        raise SystemExit("tizen3-es5: unsupported wasm2js embedding")
    a, b = start.end(), start.end() + end.start()
    body = data[a:b]
    if "wasm2js_trap" not in body:
        raise SystemExit("tizen3-es5: wasm2js body missing")
    glue = data[:a] + PLACEHOLDER + data[b:]
    env = os.environ.copy()
    env["NODE_PATH"] = os.environ.get("NUVIO_BABEL_NODE_PATH", str(Path.home() / "node_modules"))
    script = """
const babel = require('@babel/core');
const fs = require('fs');
const plugins = JSON.parse(process.argv[3]).map(p => require('@babel/plugin-transform-' + p));
// Optional catch binding needs no dependency: give each omitted binding a local name.
plugins.push(function ({types:t}) { return {visitor:{CatchClause(p) {
  if (!p.node.param) p.node.param = p.scope.generateUidIdentifier('error');
}, NumericLiteral(p) {
  if (p.node.extra) delete p.node.extra; // Print binary/octal literals as ES5 decimal.
}}}; });
const result = babel.transformFileSync(process.argv[1], {
  babelrc: false, configFile: false, plugins, comments: false, compact: false
});
fs.writeFileSync(process.argv[2], result.code);
"""
    import json
    with tempfile.TemporaryDirectory(prefix="nuvio-tizen3-") as tmp:
        source, target = Path(tmp) / "glue.js", Path(tmp) / "es5.js"
        source.write_text(glue)
        subprocess.run(["node", "-e", script, str(source), str(target), json.dumps(PLUGINS)],
                       env=env, check=True)
        result = target.read_text()
    if result.count(PLACEHOLDER) != 1:
        raise SystemExit("tizen3-es5: placeholder changed during transpilation")
    Path(dst).write_text(result.replace(PLACEHOLDER, '(' + body + ')'))
    print("tizen3-es5: glue lowered; wasm2js body preserved")


if __name__ == "__main__":
    lower(*sys.argv[1:])
