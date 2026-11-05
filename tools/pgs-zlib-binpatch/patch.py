#!/usr/bin/env python3
"""Redirect the single pgs_decodificar call in the Nuvio 2.0.8 webOS binary to hook.c."""
import hashlib, os, struct, subprocess, sys

ORIG_SHA = "7d5c996190c00e76cd3aa37d5760f7a4aba4dd1a882718cd5accdc1d0950180b"
LOAD_DELTA = 0x10000            # RX PT_LOAD: vaddr - file offset
HOOK_VADDR = 0x01a42340         # just past .eh_frame, inside the RX segment's last page
ROOM_END_OFF = 0x01a32958       # file offset where the RW segment starts
CALL_VADDR = 0x0030c5a8         # bl pgs_decodificar in entregarImagens
CALL_OLD = 0xeb01bff1
RX_PHDR = 52 + 3 * 32           # program header 3 is the RX PT_LOAD
SYMS = {"pgs_orig": 0x0037c574, "malloc": 0x00018a4c, "free": 0x0001868c,
        "inflate": 0x00018848, "inflateInit2_": 0x00018158, "inflateEnd": 0x000195d4,
        "printf": 0x0001802c, "puts": 0x000183f8}

src, dst, tc, sysroot = sys.argv[1:5]
here = os.path.dirname(os.path.abspath(__file__))
build = os.path.join(here, "build")
data = bytearray(open(src, "rb").read())
if hashlib.sha256(data).hexdigest() != ORIG_SHA:
    sys.exit("refusing: input is not the Nuvio 2.0.8 arm binary")

obj, elf, blob = (os.path.join(build, n) for n in ("hook.o", "hook.elf", "hook.bin"))
subprocess.check_call([tc + "-gcc", "-c", "-O2", "-marm", "-fno-stack-protector", "-fno-pic",
    "-fno-tree-loop-distribute-patterns", "-fno-asynchronous-unwind-tables", "-fno-unwind-tables",
    "--sysroot=" + sysroot, os.path.join(here, "hook.c"), "-o", obj])
subprocess.check_call([tc + "-ld", "-T", os.path.join(here, "hook.ld"), "-o", elf, obj] +
    ["--defsym=%s=0x%x" % kv for kv in SYMS.items()])
subprocess.check_call([tc + "-objcopy", "-O", "binary", elf, blob])
code = open(blob, "rb").read()
nm = subprocess.check_output([tc + "-nm", elf], text=True)
if not any(l.split()[-1] == "pgs_zlib_hook" and int(l.split()[0], 16) == HOOK_VADDR for l in nm.splitlines()):
    sys.exit("hook entry is not at the expected address")

off = HOOK_VADDR - LOAD_DELTA
if off + len(code) > ROOM_END_OFF or any(data[off:off + len(code)]):
    sys.exit("spare space is too small or not empty")
data[off:off + len(code)] = code

call_off = CALL_VADDR - LOAD_DELTA
if struct.unpack_from("<I", data, call_off)[0] != CALL_OLD:
    sys.exit("call site does not match")
rel = HOOK_VADDR - (CALL_VADDR + 8)
if not -(1 << 25) <= rel < (1 << 25) or rel & 3:
    sys.exit("hook is out of branch range")
struct.pack_into("<I", data, call_off, 0xeb000000 | ((rel >> 2) & 0xffffff))

ptype, poff, pv, _, fsz, msz, flags = struct.unpack_from("<7I", data, RX_PHDR)
if ptype != 1 or poff != 0 or pv != LOAD_DELTA or flags != 5 or fsz > off or msz != fsz:
    sys.exit("RX program header does not match")
new_size = off + len(code)
struct.pack_into("<II", data, RX_PHDR + 16, new_size, new_size)

with open(dst, "wb") as f:
    f.write(data)
os.chmod(dst, 0o755)
print("hook bytes", len(code), "of", ROOM_END_OFF - off, "available")
print("sha256", hashlib.sha256(data).hexdigest())
