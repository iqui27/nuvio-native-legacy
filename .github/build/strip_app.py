#!/usr/bin/env python3
"""Strip non-runtime data and verify every allocated ELF section is unchanged."""
import os, pathlib, struct, subprocess
def elf(data):
    h=struct.unpack_from("<16sHHIIIIIHHHHHH",data)
    assert data[:6]==b"\x7fELF\x01\x01" and h[2]==40
    sections=[struct.unpack_from("<IIIIIIIIII",data,h[6]+i*h[11]) for i in range(h[12])]
    names=sections[h[13]]; names=data[names[4]:names[4]+names[5]]
    result={}
    all_names=[]
    for s in sections:
        name=names[s[0]:names.find(b"\0",s[0])].decode()
        all_names.append((name,s[1]))
        if s[2]&2:
            result[name]=(s[1],s[2],s[3],s[5],s[8],s[9],data[s[4]:s[4]+s[5]] if s[1]!=8 else b"")
    return (h[2],h[4],h[7]),result,all_names
p=pathlib.Path("nuvio-proto.arm")
before=p.read_bytes(); identity,allocated,_=elf(before)
subprocess.run([os.environ.get("NUVIO_CONTAINER_RUNTIME","docker"),"run","--rm",
    "--platform",os.environ.get("NUVIO_BUILD_PLATFORM","linux/amd64"),
    "-v",str(pathlib.Path.cwd())+":/work",os.environ.get("NUVIO_SDK_IMAGE","nuvio-webos-sdk"),
    "arm-webos-linux-gnueabi-strip","--strip-all","/work/nuvio-proto.arm"],check=True)
after=p.read_bytes(); identity_after,allocated_after,names=elf(after)
assert identity==identity_after and allocated==allocated_after,"Runtime ELF sections changed"
assert not any(n.startswith(".debug") or t==2 for n,t in names),"Debug/static symbols remain"
print(f"Stripped {len(before)} -> {len(after)} bytes; {len(allocated)} runtime sections identical.")
