"""All eight diagnostics messages: translated values and printf preservation."""
import pathlib
import re

root = pathlib.Path(__file__).resolve().parents[1]
keys = [
    "Abra este endereço no navegador do computador, na mesma rede. Ele não muda: salve nos favoritos. OK atualiza o registro.",
    "Ativa o cartão de baixar o registro no painel do registro (Azul/F10). Desligado por padrão. O registro normal continua ativo. A cópia temporária só é criada ao abrir.",
    "Direita: enviar o registro para a nuvem.",
    "Enviando o registro para a nuvem…",
    "Mande este link para quem pediu o registro. Ele expira em 72 horas.",
    "Não deu para enviar o registro (%s). OK tenta de novo.",
    "Não foi possível abrir o endereço local (%s). OK envia o registro para a nuvem.",
    "QR de diagnóstico",
]
english = dict(re.findall(r'\{ "([^"]+)", "([^"]+)" \}', (root / "src/idioma_tab.h").read_text()))
files = list((root / "src").glob("idioma_*.h"))
files.remove(root / "src/idioma_tab.h")
assert len(files) == 29
for file in files:
    table = dict(re.findall(r'T\("([^"]+)", "([^"]+)"\)', file.read_text()))
    for key in keys:
        value = table[key]
        assert value and value != english[key], (file.name, key, "English fallback")
        assert value.count("%s") == key.count("%s"), (file.name, key, "printf mismatch")
print("PASS: all eight messages translated in all 29 languages; placeholders intact")
