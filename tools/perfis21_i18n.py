#!/usr/bin/env python3
# Insere as chaves novas da 2.1 (perfil: adicionar/gerenciar/tamanho) em
# idioma_tab.h e nas 28 irmas, cada uma na posicao da ordem strcmp.
# Uso: python3 tools/perfis21_i18n.py
import pathlib, re, sys

RAIZ = pathlib.Path(__file__).resolve().parent.parent
SRC = RAIZ / "src"

NOVAS = {
  "Adicionar perfil": {
    "en": "Add profile",
    "ro": "Adaugă profil", "uk": "Додати профіль", "ru": "Добавить профиль",
    "fr": "Ajouter un profil", "de": "Profil hinzufügen", "es": "Añadir perfil",
    "it": "Aggiungi profilo", "nl": "Profiel toevoegen", "pl": "Dodaj profil",
    "tr": "Profil ekle", "ptpt": "Adicionar perfil", "sv": "Lägg till profil",
    "da": "Tilføj profil", "no": "Legg til profil", "cs": "Přidat profil",
    "sk": "Pridať profil", "sl": "Dodaj profil", "hu": "Profil hozzáadása",
    "lt": "Pridėti profilį", "bs": "Dodaj profil", "sr": "Додај профил",
    "bg": "Добави профил", "el": "Προσθήκη προφίλ", "id": "Tambah profil",
    "vi": "Thêm hồ sơ", "ja": "プロフィールを追加", "zhcn": "添加用户",
    "zhtw": "新增個人檔",
  },
  "Gerenciar perfis": {
    "en": "Manage profiles",
    "ro": "Gestionează profilurile", "uk": "Керувати профілями",
    "ru": "Управление профилями", "fr": "Gérer les profils",
    "de": "Profile verwalten", "es": "Gestionar perfiles",
    "it": "Gestisci profili", "nl": "Profielen beheren",
    "pl": "Zarządzaj profilami", "tr": "Profilleri yönet",
    "ptpt": "Gerir perfis", "sv": "Hantera profiler", "da": "Administrér profiler",
    "no": "Administrer profiler", "cs": "Spravovat profily",
    "sk": "Spravovať profily", "sl": "Upravljaj profile",
    "hu": "Profilok kezelése", "lt": "Tvarkyti profilius",
    "bs": "Upravljaj profilima", "sr": "Управљај профилима",
    "bg": "Управление на профилите", "el": "Διαχείριση προφίλ",
    "id": "Kelola profil", "vi": "Quản lý hồ sơ", "ja": "プロフィールを管理",
    "zhcn": "管理用户", "zhtw": "管理個人檔",
  },
  "Remover %s? O progresso dele sai da conta.": {
    "en": "Remove %s? Their progress leaves the account.",
    "ro": "Ștergi %s? Progresul lui pleacă din cont.",
    "uk": "Видалити %s? Його прогрес зникає з акаунта.",
    "ru": "Удалить %s? Его прогресс исчезнет из аккаунта.",
    "fr": "Supprimer %s ? Sa progression quitte le compte.",
    "de": "%s entfernen? Sein Fortschritt verlässt das Konto.",
    "es": "¿Eliminar %s? Su progreso sale de la cuenta.",
    "it": "Rimuovere %s? I suoi progressi escono dall'account.",
    "nl": "%s verwijderen? De voortgang verlaat het account.",
    "pl": "Usunąć %s? Jego postęp znika z konta.",
    "tr": "%s kaldırılsın mı? İzleme durumu hesaptan çıkar.",
    "ptpt": "Remover %s? O progresso dele sai da conta.",
    "sv": "Ta bort %s? Deras förlopp lämnar kontot.",
    "da": "Fjern %s? Deres fremskridt forlader kontoen.",
    "no": "Fjerne %s? Fremdriften deres forlater kontoen.",
    "cs": "Odebrat %s? Jeho postup zmizí z účtu.",
    "sk": "Odstrániť %s? Jeho priebeh zmizne z účtu.",
    "sl": "Odstrani %s? Njegov napredek izgine z računa.",
    "hu": "%s eltávolítása? Az előrehaladása kikerül a fiókból.",
    "lt": "Pašalinti %s? Jo pažanga dingsta iš paskyros.",
    "bs": "Ukloniti %s? Njegov napredak izlazi iz računa.",
    "sr": "Уклонити %s? Његов напредак излази из налога.",
    "bg": "Премахване на %s? Напредъкът му излиза от акаунта.",
    "el": "Αφαίρεση του %s; Η πρόοδός του φεύγει από τον λογαριασμό.",
    "id": "Hapus %s? Progresnya keluar dari akun.",
    "vi": "Xóa %s? Tiến trình của họ rời khỏi tài khoản.",
    "ja": "%sを削除しますか？視聴履歴はアカウントから消えます。",
    "zhcn": "移除%s？其进度将从此账户移除。",
    "zhtw": "移除%s？其進度將離開帳號。",
  },
  "Tamanho dos perfis": {
    "en": "Profile size",
    "ro": "Mărimea profilurilor", "uk": "Розмір профілів",
    "ru": "Размер профилей", "fr": "Taille des profils",
    "de": "Profilgröße", "es": "Tamaño de los perfiles",
    "it": "Dimensione dei profili", "nl": "Profielgrootte",
    "pl": "Rozmiar profili", "tr": "Profil boyutu", "ptpt": "Tamanho dos perfis",
    "sv": "Profilstorlek", "da": "Profilstørrelse", "no": "Profilstørrelse",
    "cs": "Velikost profilů", "sk": "Veľkosť profilov",
    "sl": "Velikost profilov", "hu": "Profilok mérete",
    "lt": "Profilių dydis", "bs": "Veličina profila", "sr": "Величина профила",
    "bg": "Размер на профилите", "el": "Μέγεθος προφίλ",
    "id": "Ukuran profil", "vi": "Kích thước hồ sơ", "ja": "プロフィールのサイズ",
    "zhcn": "用户尺寸", "zhtw": "個人檔大小",
  },
  "Tamanho dos avatares na escolha de perfil: Pequeno, Médio (o de sempre) ou Grande.": {
    "en": "Size of the avatars on the profile screen: Small, Medium (the usual) or Large.",
    "ro": "Mărimea avatarurilor la alegerea profilului: mic, mediu (cel de dintotdeauna) sau mare.",
    "uk": "Розмір аватарів на екрані вибору профілю: маленький, середній (як завжди) чи великий.",
    "ru": "Размер аватаров на экране выбора профиля: маленький, средний (как всегда) или большой.",
    "fr": "Taille des avatars sur l'écran de profil : petit, moyen (comme toujours) ou grand.",
    "de": "Größe der Avatare im Profilbildschirm: Klein, Mittel (wie immer) oder Groß.",
    "es": "Tamaño de los avatares en la pantalla de perfiles: pequeño, mediano (el de siempre) o grande.",
    "it": "Dimensione degli avatar nella schermata dei profili: piccolo, medio (come sempre) o grande.",
    "nl": "Grootte van de avatars in het profielscherm: klein, middel (zoals altijd) of groot.",
    "pl": "Rozmiar avatarów na ekranie wyboru profilu: mały, średni (jak zawsze) lub duży.",
    "tr": "Profil ekranındaki avatar boyutu: küçük, orta (her zamanki) veya büyük.",
    "ptpt": "Tamanho dos avatares na escolha de perfil: pequeno, médio (o de sempre) ou grande.",
    "sv": "Storlek på avatarerna på profilerskärmen: liten, medium (som vanligt) eller stor.",
    "da": "Størrelse på avatarerne på proforskærmen: lille, medium (som altid) eller stor.",
    "no": "Størrelse på avatarane på profilskjermen: liten, medium (som alltid) eller stor.",
    "cs": "Velikost avatarů na obrazovce výběru profilu: malý, střední (jako vždy) nebo velký.",
    "sk": "Veľkosť avatarov na obrazovke výberu profilu: malý, stredný (ako vždy) alebo veľký.",
    "sl": "Velikost avatarjev na zaslonu izbire profila: majhen, srednji (kot vedno) ali velik.",
    "hu": "Az avatarok mérete a profilkiválasztó képernyőn: kicsi, közepes (a megszokott) vagy nagy.",
    "lt": "Avatarų dydis profilio pasirinkimo ekrane: mažas, vidutinis (kaip visada) arba didelis.",
    "bs": "Veličina avatara na ekranu izbora profila: mali, srednji (kao uvijek) ili veliki.",
    "sr": "Величина аватара на екрану избора профила: мали, средњи (као увек) или велики.",
    "bg": "Размер на аватарите на екрана за избор на профил: малък, среден (както винаги) или голям.",
    "el": "Μέγεθος των avatar στην οθόνη επιλογής προφίλ: μικρό, μεσαίο (ως συνήθως) ή μεγάλο.",
    "id": "Ukuran avatar di layar profil: kecil, sedang (seperti biasa) atau besar.",
    "vi": "Kích thước đại diện trên màn hình chọn hồ sơ: nhỏ, vừa (như luôn) hoặc lớn.",
    "ja": "プロフィール選択画面のアバターのサイズ：小、中（いつも通り）、大。",
    "zhcn": "个人资料选择界面的头像大小：小、中（默认）或大。",
    "zhtw": "個人檔選擇畫面的頭像大小：小、中（預設）或大。",
  },
}

IDIOMAS = ("ro", "uk", "ru", "fr", "de", "es", "it", "nl", "pl", "tr", "ptpt", "sv",
           "da", "no", "cs", "sk", "sl", "hu", "lt", "bs", "sr", "bg", "el", "id",
           "vi", "ja", "zhcn", "zhtw")

LINHA_TAB = '  {{ "{k}", "{v}" }},\n'
LINHA_IRMA = '  T("{k}", "{v}"),\n'

def esc(s):
    return s.replace("\\", "\\\\").replace('"', '\\"')

def inserir(caminho, chave, valor, irma):
    linhas = caminho.read_text(encoding="utf-8").splitlines(keepends=True)
    fmt = LINHA_IRMA if irma else LINHA_TAB
    nova = fmt.format(k=esc(chave), v=esc(valor))
    # acha a posicao: a primeira linha cuja chave decodificada > chave nova
    pat = re.compile(r'^\s*(?:\{\s*|T\(\s*)"((?:[^"\\]|\\.)*)"')
    pos = None
    for i, ln in enumerate(linhas):
        m = pat.match(ln)
        if not m:
            continue
        k = m.group(1).encode().decode("unicode_escape").encode("latin-1", "ignore").decode("utf-8", "ignore")
        if k == chave:
            print(f"  ja existe: {chave!r} em {caminho.name}")
            return False
        if k > chave:
            pos = i
            break
    if pos is None:
        print(f"  ERRO: fim da tabela sem ponto de insercao para {chave!r} em {caminho.name}")
        sys.exit(1)
    linhas.insert(pos, nova)
    caminho.write_text("".join(linhas), encoding="utf-8")
    return True

for chave, trads in NOVAS.items():
    inserir(SRC / "idioma_tab.h", chave, trads["en"], False)
    for cod in IDIOMAS:
        inserir(SRC / f"idioma_{cod}.h", chave, trads[cod], True)
    print(f"inserida: {chave!r}")
print("ok")
