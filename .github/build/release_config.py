#!/usr/bin/env python3
"""Reuse only application configuration embedded in a verified public release.
Never copies profile files or prints application configuration values.
"""
import argparse, base64, hashlib, io, json, pathlib, re, struct, subprocess, tarfile

def executable(package):
    data = subprocess.check_output(["ar", "p", str(package.resolve()), "data.tar.gz"])
    with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as archive:
        matches = [m for m in archive.getmembers() if m.isfile() and m.name.endswith("/nuvio-proto")]
        assert len(matches) == 1, "Expected one application executable"
        return archive.extractfile(matches[0]).read()

def symbols(blob):
    assert blob[:6] == b"\x7fELF\x01\x01", "Expected little-endian ELF32"
    header = struct.unpack_from("<16sHHIIIIIHHHHHH", blob)
    sections = [struct.unpack_from("<IIIIIIIIII", blob, header[6] + i * header[11])
                for i in range(header[12])]
    result = {}
    for section in sections:
        if section[1] != 2:
            continue
        names_section = sections[section[6]]
        names = blob[names_section[4]:names_section[4]+names_section[5]]
        context = ""
        for offset in range(section[4], section[4]+section[5], section[9]):
            name, value, size, info, _, index = struct.unpack_from("<IIIBBH", blob, offset)
            name = names[name:names.find(b"\0", name)].decode("utf-8", "replace")
            if info & 15 == 4:
                context = name
            if info & 15 != 1 or not size or index >= len(sections):
                continue
            target = sections[index]
            if target[1] == 8:
                continue
            start = target[4] + value - target[3]
            raw = blob[start:start+size].split(b"\0", 1)[0]
            try:
                result[(context, name)] = raw.decode("utf-8")
            except UnicodeDecodeError:
                pass
    return result

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("package", type=pathlib.Path)
    parser.add_argument("checksums", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    args = parser.parse_args()
    expected = [line.split()[0] for line in args.checksums.read_text().splitlines()
                if line.split()[-1].lstrip("*") == "space.nuvio.native.legacy_2.0.8_arm.ipk"]
    assert len(expected) == 1, "Missing official checksum"
    assert hashlib.sha256(args.package.read_bytes()).hexdigest() == expected[0], "Release checksum mismatch"
    blob = executable(args.package)
    table = symbols(blob)
    mapping = {
        "NUVIO_SUPABASE_URL": "url", "NUVIO_SUPABASE_ANON_KEY": "anon",
        "TV_LOGIN_WEB_BASE_URL": "baseLogin", "TRAKT_CLIENT_ID": "traktCliente",
        "TRAKT_CLIENT_SECRET": "traktSegredo", "SIMKL_CLIENT_ID": "simklCliente",
        "SIMKL_APP_NAME": "simklApp",
    }
    config = {key: table[("nuvem.c", symbol)] for key, symbol in mapping.items()}
    assert all(config.values()), "Missing required application configuration"
    anon = config["NUVIO_SUPABASE_ANON_KEY"].split(".")
    payload = json.loads(base64.urlsafe_b64decode(anon[1] + "=" * (-len(anon[1]) % 4)))
    assert payload.get("role") == "anon", "Only the public anonymous application key is allowed"
    marker = blob.index(b"[desc] tmdb: chave do pacote")
    candidates = re.findall(rb"(?<=\x00)[0-9a-fA-F]{32}(?=\x00)", blob[max(0, marker-160):marker])
    assert len(candidates) == 1, "Cannot identify the package TMDB application key"
    config["TMDB_API_KEY"] = candidates[0].decode()
    discord = re.findall(rb"(?<=\x00)[0-9]{17,20}(?=\x00)", blob)
    if len(discord) == 1:
        config["DISCORD_CLIENT_ID"] = discord[0].decode()
    recommendation = table.get(("recomenda.c", "url"), "")
    if recommendation.startswith("https://"):
        config["NUVIO_REC_URL"] = recommendation
    assert all("\n" not in value and "\r" not in value for value in config.values())
    args.output.write_text("".join(key+"="+value+"\n" for key,value in config.items()))
    args.output.chmod(0o600)
    print("Verified official 2.0.8 release; restored application build configuration without profile data.")

if __name__ == "__main__":
    main()
