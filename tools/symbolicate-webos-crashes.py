#!/usr/bin/env python3
"""Summarize crash data without emitting identities, timestamps or request text."""
import argparse
import collections
import json
import pathlib
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--reports", required=True, type=pathlib.Path)
parser.add_argument("--binary", required=True, type=pathlib.Path)
parser.add_argument("--output", required=True, type=pathlib.Path)
args = parser.parse_args()
rows = json.loads(args.reports.read_text())[0]["results"]
# queda.c emits offset relative to the first mapping, including for ET_EXEC.
# addr2line needs the virtual address; establish the load base from ELF.
binary = args.binary.resolve()
mount = ["container", "run", "--rm", "-v", f"{binary.parent}:/symbols:ro",
         "nuvio-webos-sdk"]
elf = subprocess.check_output(mount + ["arm-webos-linux-gnueabi-readelf", "-lW",
                                     f"/symbols/{binary.name}"], text=True)
loads = re.findall(r"^\s*LOAD\s+(0x[0-9a-f]+)\s+(0x[0-9a-f]+)", elf, re.M)
base = min(int(v, 16) - int(o, 16) for o, v in loads)
offsets = sorted({int(v, 16) for r in rows for v in
                  re.findall(r"nuvio-proto\+0x([0-9a-f]+)", r["texto"])})
addresses = [hex((v & ~1) + base) for v in offsets]
symbols = subprocess.check_output(mount + ["arm-webos-linux-gnueabi-addr2line",
    "-f", "-e", f"/symbols/{binary.name}"] + addresses, text=True).splitlines()
assert len(symbols) == 2 * len(offsets)
lookup = {v: {"function": symbols[2*i], "location": symbols[2*i+1]}
          for i, v in enumerate(offsets)}
groups = collections.Counter()
sites = collections.Counter()
application_groups = collections.Counter()
pcs = collections.Counter()
faults = collections.Counter()
for row in rows:
    text = row["texto"]
    # Only fixed-format module/offset lines can leave the input boundary.
    frames = re.findall(r"^\[queda\] pilha ((?:libcrypto|libcurl)[\w.]*\+0x[0-9a-f]+)$", text, re.M)
    groups[tuple(frames[:4])] += 1
    pc = re.search(r"^\[queda\] pc ([\w.]+\+0x[0-9a-f]+)$", text, re.M)
    pcs[pc[1] if pc else "unmapped"] += 1
    fault = re.search(r"endereco (0x[0-9a-f]+)", text)
    faults[fault[1] if fault else "unknown"] += 1
    app_offsets = [int(v, 16) for v in re.findall(r"nuvio-proto\+0x([0-9a-f]+)", text)]
    application_groups[tuple(lookup[v]["function"] for v in app_offsets)] += 1
    for v in set(app_offsets):
        sites[lookup[v]["function"]] += 1
result = {
    "reports": len(rows), "elf_load_base": hex(base),
    "warning": "Stack scan candidates, not unwound frames. Rebuilt layout is not a verified match to the distributed binary. Firmware libraries are not symbolicated with a different SDK copy.",
    "fault_addresses": dict(faults), "pc_groups": dict(pcs.most_common()),
    "library_stack_groups": [{"count": n, "candidates": list(k)} for k, n in groups.most_common()],
    "application_stack_groups": [{"count": n, "candidates": list(k)} for k, n in application_groups.most_common()],
    "symbol_candidates_by_report": dict(sites.most_common()),
    "offsets": {hex(v): lookup[v] for v in offsets},
}
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(result, indent=2) + "\n")
print(f"Symbolicated {len(offsets)} distinct offsets from {len(rows)} reports; sanitized summary written.")
