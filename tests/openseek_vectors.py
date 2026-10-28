#!/usr/bin/env python3
"""OSP1 test vectors for the Nuvio C client, derived ONLY from the frozen OpenSeek
contract (docs/api/CONTRACT-v1.md, "Pairing"), with the Python stdlib.

  python3 -I tests/openseek_vectors.py > tests/openseek_vectors.json

  code_norm    = uppercase(code) with spaces/dashes removed
  K            = PBKDF2-HMAC-SHA256(code_norm, "OSP1" + pair_id + salt + spki_pin, iterations, 32)
  client_proof = hex(HMAC-SHA256(K, "client" + pair_id + device_name))
  server_proof = hex(HMAC-SHA256(K, "server" + pair_id + hex(SHA-256(token))))
All strings UTF-8, hex lowercase. The values are fixed (no randomness) so the
file is reproducible.
"""
import base64
import hashlib
import hmac
import json


def norm(code):
    return code.upper().replace(" ", "").replace("-", "")


def case(name, code, pair_id, salt, spki_der_seed, iterations, device, token):
    pin = "sha256//" + base64.b64encode(hashlib.sha256(spki_der_seed.encode()).digest()).decode()
    cn = norm(code)
    k = hashlib.pbkdf2_hmac("sha256", cn.encode(), ("OSP1" + pair_id + salt + pin).encode(), iterations, 32)
    cp = hmac.new(k, ("client" + pair_id + device).encode(), hashlib.sha256).hexdigest()
    th = hashlib.sha256(token.encode()).hexdigest()
    sp = hmac.new(k, ("server" + pair_id + th).encode(), hashlib.sha256).hexdigest()
    return {"name": name, "code": code, "code_norm": cn, "pair_id": pair_id, "salt": salt,
            "spki_pin": pin, "iterations": iterations, "device_name": device, "token": token,
            "K_hex": k.hex(), "client_proof": cp, "server_proof": sp}


def main():
    tok = base64.urlsafe_b64encode(bytes(range(32))).rstrip(b"=").decode()  # 43 chars
    cases = [
        case("contract_600k", "K7QF-M2XD", "00112233445566778899aabbccddeeff",
             "0f1e2d3c4b5a69788796a5b4c3d2e1f0", "test-spki", 600000, "Shield", tok),
        case("lowercase_spaces_200k", "k7qf m2xd", "ffeeddccbbaa99887766554433221100",
             "00000000000000000000000000000001", "other-spki", 200000, "SHIELD Android TV", tok[::-1]),
        case("fast_1000", "2345-6789", "0123456789abcdef0123456789abcdef",
             "fedcba9876543210fedcba9876543210", "x", 1000, "Shield", "tok"),
    ]
    prims = {
        "sha256_empty": hashlib.sha256(b"").hexdigest(),
        "sha256_abc": hashlib.sha256(b"abc").hexdigest(),
        "sha256_448": hashlib.sha256(b"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq").hexdigest(),
        "sha256_million_a": hashlib.sha256(b"a" * 1000000).hexdigest(),
        "hmac_rfc4231_1": hmac.new(b"\x0b" * 20, b"Hi There", hashlib.sha256).hexdigest(),
        "hmac_long_key": hmac.new(b"\xaa" * 131, b"Test Using Larger Than Block-Size Key - Hash Key First",
                                  hashlib.sha256).hexdigest(),
        "pbkdf2_password_salt_1": hashlib.pbkdf2_hmac("sha256", b"password", b"salt", 1, 32).hex(),
        "pbkdf2_password_salt_4096": hashlib.pbkdf2_hmac("sha256", b"password", b"salt", 4096, 32).hex(),
    }
    print(json.dumps({"source": "tests/openseek_vectors.py (contract v1, stdlib)",
                      "primitives": prims, "osp1": cases}, indent=1))


if __name__ == "__main__":
    main()
