// Motor P2P embutido: ciclo de vida, cancelamento, tetos e vigia. Ver p2pmotor.h.
//
// Este arquivo NAO inclui o nuvio-engine: fala com ele pela tabela P2pmOps
// (p2pmotor_motor.c com -DNV_P2P_MOTOR; motor falso nos testes). Assim toda a
// logica de concorrencia roda sob ASan/TSan no Mac, com ou sem o motor real.
//
// Sem nftw e sem strcasestr/usleep: no .tpk o -include src/tpk.h chega antes
// de qualquer _GNU_SOURCE e essas declaracoes somem (medido no p2p2).
#include "p2pmotor.h"
#include "p2p.h"
#include "dados.h"
#include "androidcache.h"
#include "ajustes.h"
#include "rede.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <time.h>
#include <unistd.h>
#ifdef __ANDROID__
#include "android.h"
#include <sys/vfs.h>
#endif

// ------------------------------------------------------------------ estado
// TUDO abaixo da trava e lido/escrito so com ela, e ela so e presa por
// trechos curtos sem I/O nem chamada ao motor.
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sinal = PTHREAD_COND_INITIALIZER;
static void *motor;            // o motor de pe (NULL subindo/parado)
static int usuarios;           // fios usando `motor` FORA da trava (pedido, vigia)
static int resolvendo;         // um pedido em curso (um por vez)
static int parando;            // destroy em curso num fio solto
static int vigiaEventos;       // a vigia esta lendo a fila de eventos
static int vigiaViva, vigiaSair;
static int ondeFoiPendente;    // vigia pediu o inventario; pararFio roda depois dos escritores (#297)
static pthread_t fioVigia;
static char tidAtual[65], sidAtual[65], hashAtual[41], prefixoUrl[96];
static int metaPronta;
static char pastaRaiz[600], pastaMae[600];
static int pastaFat;           // File limit bound to the active motor root.
static uint64_t livreSubida;   // livre medido quando o motor subiu (#297)
static uint64_t tamArquivo;    // tamanho do arquivo do pedido atual (#334); 0 = nao sei
static int limpando;           // p2pmotor_limpar_sobra apagando a pasta (#334)
static P2pmEstado estado;
static _Atomic unsigned geracao;
static _Atomic int motivoParada;
static _Atomic unsigned descartados;
static _Atomic int segurado;
static _Atomic unsigned tetoUltimoMb;
static _Atomic unsigned limiteMb;    // escolha de Ajustes (#334); 0 = Automatico

// Injecao (testes). Escrita so sem motor de pe.
static const P2pmOps *opsInjetado;
static int temOpsInjetado;
static char raizInjetada[600];
// #205: usbPreferido e atomico — o fio do app o muda a cada quadro
// (vigiarMotorP2p) enquanto os fios do motor o leem em pastas(). usbRaiz,
// usbResolvido, usbQuando e usbExtra ficam sob usbTrava: usb_presente() (fio
// da tela) e pastas() (fio do pedido/limpeza) podem se cruzar. O scan faz
// I/O curto com a trava presa; quem espera e a sonda da UI, nunca o motor.
static _Atomic int usbPreferido;
static pthread_mutex_t usbTrava = PTHREAD_MUTEX_INITIALIZER;
static char usbRaiz[600];        // <usb>/p2p escolhida; "" = nenhuma
static struct stat usbVolume;    // identity of the allocated mount root
static int usbResolvido;         // scan ja rodou nesta configuracao
static _Atomic int usbTem;       // resposta cacheada de usb_presente
static double usbQuando;         // quando o cache acima foi medido
static char usbExtra[600];
#define USB_MAX_VISTOS 64
#define USB_DONO ".nuvio"   // marca: so apaga p2p/ que a gente criou
static void usbMarcarDono(const char *raiz);
static int usbENossa(const char *p2p);
static struct stat usbVistos[USB_MAX_VISTOS];   // (dev,ino) ja olhados no scan
static unsigned usbNVistos;
static _Atomic int usbMotivo;
static char usbAndroidInj[1400]; // teste: lista no lugar de getExternalFilesDirs
static char usbMounts[200];      // teste: arquivo no formato /proc/mounts
static int usbApp;               // sondando o dir do app que E a raiz do pendrive (Android)
static int usbFat;               // raiz escolhida esta em FAT32: arquivo < 4 GiB
static char usbAndroidLogado[700];
static char usbRecusado[700];    // last rejected candidate path (for the modal)
static char usbTizenLogado[700];
// Shared by discovery and stale-cache cleanup; only mount roots qualify.
static const char *const usbBases[] = {
  "/tmp/usb", "/media/usb", "/mnt/media_rw",
  "/opt/storage/usb"
};
// Tizen mounts each drive at /opt/media/USBDrive<letter><n> (A1, B1, ...).
static char usbTizenDir[600] = "/opt/media";
#define USB_MAX_BASES 16
// Candidate roots: the test base alone, else static bases + enumerated Tizen drives.
static unsigned usbTodasBases(const char *extra, char out[][700]) {
  unsigned n = 0;
  DIR *d;
  struct dirent *e;
  if (extra[0]) { snprintf(out[n++], 700, "%s", extra); return n; }
  if (usbAndroidInj[0]) return 0;
  if (!strcmp(usbTizenDir, "/opt/media"))   // a test dir replaces the real bases
    for (; n < sizeof usbBases / sizeof usbBases[0]; n++) snprintf(out[n], 700, "%s", usbBases[n]);
  if ((d = opendir(usbTizenDir))) {
    while ((e = readdir(d)) && n < USB_MAX_BASES)
      if (!strncmp(e->d_name, "USBDrive", 8) &&
          snprintf(out[n], 700, "%s/%s", usbTizenDir, e->d_name) < 700) n++;
    closedir(d);
  }
  return n;
}
#define USB_SCAN_SECONDS 0.25
// ponytail: linear lookup within the bounded scan; index only if profiling warrants it.
typedef struct UsbLog {
  struct UsbLog *next;
  char path[700];
  P2pmUsbMotivo reason, previous;
  int error, previousError, seen;
} UsbLog;
static UsbLog *usbLogs;

// With usbTrava held. Retain per-path outcomes across scans, never file contents.
static void usbResultado(const char *path, P2pmUsbMotivo reason, int error) {
  UsbLog *entry;
  if ((int)reason > atomic_load(&usbMotivo)) {
    atomic_store(&usbMotivo, reason);
    snprintf(usbRecusado, sizeof usbRecusado, "%s", path);
  }
  for (entry = usbLogs; entry; entry = entry->next)
    if (!strcmp(entry->path, path)) break;
  if (!entry) {
    entry = calloc(1, sizeof *entry);
    if (!entry) return;
    snprintf(entry->path, sizeof entry->path, "%s", path);
    entry->previous = P2PM_USB_DISABLED; // Ensure the first result is logged.
    entry->next = usbLogs;
    usbLogs = entry;
  }
  entry->reason = reason;
  entry->error = error;
  entry->seen = 1;
}
static int usbRejeitar(const char *path, P2pmUsbMotivo reason, int error, int result) {
  usbResultado(path, reason, error);
  return result;
}
static unsigned vigiaMs = P2PM_VIGIA_MS;

static const P2pmOps *ops(void) {
  return temOpsInjetado ? opsInjetado : p2pmotor_ops_reais();
}

// ------------------------------------------------------------------ tempo
static double agora(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}
static void dormirMs(unsigned ms) {
  struct timespec t = { (time_t)(ms / 1000), (long)(ms % 1000) * 1000000L };
  nanosleep(&t, NULL);
}
// Espera no `sinal` por ate `ms` (relogio de parede: pthread_condattr_setclock
// nao existe no Mac). Chamar com a trava presa; ela e solta durante a espera.
static void esperarSinal(unsigned ms) {
  struct timespec t;
  clock_gettime(CLOCK_REALTIME, &t);
  t.tv_sec += (time_t)(ms / 1000);
  t.tv_nsec += (long)(ms % 1000) * 1000000L;
  if (t.tv_nsec >= 1000000000L) { t.tv_sec++; t.tv_nsec -= 1000000000L; }
  pthread_cond_timedwait(&sinal, &trava, &t);
}

// ------------------------------------------------------------------ disco
static int livreReal(const char *pasta, uint64_t *livre) {
  struct statvfs v;
  if (statvfs(pasta, &v) != 0) return -1;
  *livre = (uint64_t)v.f_bavail * (uint64_t)v.f_frsize;
  return 0;
}
static int sondaReal(const char *url, int (*parar)(void *), void *u);
static P2pmLivreFn livreFn = livreReal;
static P2pmSondaFn sondaFn = sondaReal;

// Percorre `p` sem seguir links. apagar=1 remove tudo (inclusive `p`);
// apagar=0 so soma os blocos REAIS (o arquivo do torrent e esparso: o
// tamanho mente).
// #297: no ARM de 32 bits sem _FILE_OFFSET_BITS=64 o lstat de um arquivo
// acima de 2 GB falha (EOVERFLOW): o video do torrent nao era contado nem
// apagado, e o disco da TV enchia sessao apos sessao. O tpk.sh compila este
// arquivo com 64 bits; se o lstat falhar mesmo assim, apagar tenta o unlink.
// #297: o webOS (tools/arm.sh) e o mesmo caso. No Android 32 bits o bionic ja
// usa struct stat 64 (st_size long long), entao la nao precisa.
#if (defined(NV_TPK) || defined(NV_WEBOS)) && defined(__arm__)
_Static_assert(sizeof(off_t) == 8, "p2pmotor.c precisa de -D_FILE_OFFSET_BITS=64 (#297)");
#endif
// Limite de trabalho (so para medir, nunca para apagar): entradas e prazo.
typedef struct { unsigned n, max; double ate; int estourou; } LimTrab;
static uint64_t percorrerL(const char *p, int apagar, int fundo, LimTrab *lim) {
  struct stat st;
  uint64_t soma = 0;
  if (fundo > 16) return 0;
  if (lim) {
    if (lim->estourou || ++lim->n > lim->max || agora() > lim->ate) { lim->estourou = 1; return 0; }
  }
  if (lstat(p, &st) != 0) {
    if (apagar && fundo > 0) unlink(p);
    return 0;
  }
  if (S_ISDIR(st.st_mode)) {
    DIR *d = opendir(p);
    struct dirent *e;
    char f[1200];
    if (d) {
      while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        if (snprintf(f, sizeof f, "%s/%s", p, e->d_name) >= (int)sizeof f) continue;
        soma += percorrerL(f, apagar, fundo + 1, lim);
      }
      closedir(d);
    }
    if (apagar) rmdir(p);
  } else {
    soma = (uint64_t)st.st_blocks * 512u;
    if (apagar) unlink(p);
  }
  return soma;
}
static int pastasInternas(char *raiz, unsigned nr, char *mae, unsigned nm);
static uint64_t percorrer(const char *p, int apagar, int fundo) {
  if (apagar && !usbENossa(p)) {
    char interna[600], mae[600];
    // Internal storage predates markers; external paths always require ownership.
    if (!pastasInternas(interna, sizeof interna, mae, sizeof mae) || strcmp(p, interna)) return 0;
  }
  return percorrerL(p, apagar, fundo, NULL);
}

// Returns 1 for a usable volume, -1 for a rejected volume, 0 for a container.
// Sonda de um candidato a raiz USB. criar=1 cria <cand>/p2p e prova escrita
// nela (raiz de verdade do motor); criar=0 so prova escrita no volume, sem
// deixar pasta nenhuma — e o modo da UI perguntando "tem pendrive?". Na
// falha, a pasta que a propria sonda criou e removida: sondar nao pode
// deixar lixo na midia da pessoa.
static int usbServe(const char *candidato, char *raiz, unsigned nr, int criar) {
  char teste[700], p2p[700];
  int fd, gravou, fechou;
  int nosso = 0;
  unsigned i;
  struct stat st;
  const char *dados = dados_dir();
  if (!candidato || !candidato[0]) return 0;
  // Um lugar ja olhado NAO entra de novo: diretorios com varios hardlinks,
  // .tpk montando o mesmo disco em dois pontos e o usbExtra de teste (um
  // diretorio no mesmo disco de dados_dir) gerariam lojas ou leitura dupla.
  if (stat(candidato, &st) != 0)
    return usbRejeitar(candidato, P2PM_USB_IO, errno, 0);
  if (!S_ISDIR(st.st_mode)) return usbRejeitar(candidato, P2PM_USB_IO, ENOTDIR, -1);
  if (!usbExtra[0] && !usbApp) {
    struct stat parent;
    // Only mount roots qualify; writable containers live on the parent device.
    if (snprintf(teste, sizeof teste, "%s/..", candidato) >= (int)sizeof teste)
      return usbRejeitar(candidato, P2PM_USB_IO, ENAMETOOLONG, -1);
    if (stat(teste, &parent) != 0) return usbRejeitar(candidato, P2PM_USB_IO, errno, -1);
    if (parent.st_dev == st.st_dev)
      return usbRejeitar(candidato, P2PM_USB_NOT_MOUNT, 0, 0);
  }
  for (i = 0; i < usbNVistos; i++)
    if (usbVistos[i].st_dev == st.st_dev && usbVistos[i].st_ino == st.st_ino)
      return usbRejeitar(candidato, P2PM_USB_ALREADY_SEEN, 0, -1);
  if (usbNVistos < USB_MAX_VISTOS) usbVistos[usbNVistos++] = st;
  // dados_dir e os subdiretorios dele nao sao pendrive. Comparacao por
  // prefixo de caminho, nao st_dev: o pendrive e o disco interno tem dev
  // diferente, mas o usbExtra de teste mora no mesmo disco — parentesco de
  // caminho cobre os dois casos.
  if (dados && dados[0]) {
    size_t nc = strlen(candidato), nd = strlen(dados);
    if ((!strncmp(candidato, dados, nd) && (candidato[nd] == 0 || candidato[nd] == '/')) ||
        (!strncmp(dados, candidato, nc) && (dados[nc] == 0 || dados[nc] == '/')))
      return usbRejeitar(candidato, P2PM_USB_DATA_DIR, 0, -1);
  }
  // Injected read-only roots model mount containers on the same device.
  if (usbExtra[0] && access(candidato, W_OK) != 0)
    return usbRejeitar(candidato, P2PM_USB_READONLY, errno, 0);
  if (snprintf(p2p, sizeof p2p, "%s/p2p", candidato) >= (int)sizeof p2p)
    return usbRejeitar(candidato, P2PM_USB_IO, ENAMETOOLONG, -1);
  if (criar) {
    nosso = mkdir(p2p, 0700) == 0;
    if (!nosso && errno != EEXIST)
      return usbRejeitar(candidato, errno == EROFS || errno == EACCES ? P2PM_USB_READONLY : P2PM_USB_IO, errno, -1);
    if (!nosso && (lstat(p2p, &st) != 0 || !S_ISDIR(st.st_mode) || !usbENossa(p2p)))
      return usbRejeitar(candidato, P2PM_USB_DATA_DIR, 0, -1);
  }
  if (snprintf(teste, sizeof teste, "%s/.escrita-XXXXXX", criar ? p2p : candidato) >= (int)sizeof teste)
    { if (nosso) rmdir(p2p); return usbRejeitar(candidato, P2PM_USB_IO, ENAMETOOLONG, -1); }
  fd = mkstemp(teste);
  if (fd < 0) {
    int error = errno;
    if (nosso) rmdir(p2p);
    return usbRejeitar(candidato, error == EROFS || error == EACCES ? P2PM_USB_READONLY : P2PM_USB_IO, error, -1);
  }
  gravou = write(fd, "ok\n", 3) == 3;
  int error = gravou ? 0 : errno;
  fechou = close(fd) == 0;
  if (!fechou && !error) error = errno;
  if (unlink(teste) != 0 && !error) error = errno;
  if (!gravou || !fechou || error) {
    if (nosso) rmdir(p2p);
    return usbRejeitar(candidato, P2PM_USB_IO, error, -1);
  }
  if (snprintf(raiz, nr, "%s", p2p) >= (int)nr)
    { if (nosso) rmdir(p2p); return usbRejeitar(candidato, P2PM_USB_IO, ENAMETOOLONG, -1); }
  if (nosso) usbMarcarDono(p2p);   // sem a marca, limpar_sobra nao apaga
  if (criar && !usbENossa(p2p))
    { if (nosso) rmdir(p2p); return usbRejeitar(candidato, P2PM_USB_IO, EACCES, -1); }
  usbResultado(candidato, P2PM_USB_NONE, 0);
  return 1;
}

// Com usbTrava presa.
static int usbEscanearDirL(const char *base, char *raiz, unsigned nr, int fundo, int criar, LimTrab *lim) {
  DIR *d;
  struct dirent *e;
  char f[700];
  struct stat st;
  if (!base || !base[0]) return 0;
  int volume = usbServe(base, raiz, nr, criar);
  if (volume) return volume > 0; // Rejected volumes are leaves, never containers.
  if (fundo > 3 || lim->estourou)
    return usbRejeitar(base, P2PM_USB_TIMEOUT, 0, 0);
  d = opendir(base);
  if (!d) return usbRejeitar(base, P2PM_USB_IO, errno, 0);
  while (!lim->estourou && (e = readdir(d))) {
    if (++lim->n > lim->max || agora() > lim->ate) {
      lim->estourou = 1;
      usbResultado(base, P2PM_USB_TIMEOUT, 0);
      break;
    }
    if (!strcmp(e->d_name, "p2p") || e->d_name[0] == '.') continue;
    if (snprintf(f, sizeof f, "%s/%s", base, e->d_name) >= (int)sizeof f) continue;
    // Links nao sao ponto de montagem: um link dentro de /tmp/usb poderia
    // apontar para o disco interno e furar a rejeicao por caminho acima.
    if (lstat(f, &st) != 0 || S_ISLNK(st.st_mode)) continue;
    if (S_ISDIR(st.st_mode)) {
      double start = agora();
      LimTrab child = { .max = 256, .ate = start + USB_SCAN_SECONDS };
      int found = usbEscanearDirL(f, raiz, nr, fundo + 1, criar, &child);
      lim->ate += agora() - start; // A slow volume cannot spend its siblings' time.
      if (found) { closedir(d); return 1; }
    }
  }
  closedir(d);
  return 0;
}

static int usbEscanearDir(const char *base, char *raiz, unsigned nr, int fundo, int criar) {
  // Only containers consume traversal budget; volumes get one independent probe.
  LimTrab lim = { .max = 256, .ate = agora() + USB_SCAN_SECONDS };
  return usbEscanearDirL(base, raiz, nr, fundo, criar, &lim);
}


// #205 Android: app-specific dir of each removable volume (getExternalFilesDirs
// minus index 0, the primary/emulated one). Writable with no permission, so
// it stands in for the unwritable /storage/<id> root.
static unsigned usbAndroidDirs(char dirs[][600], unsigned max) {
  char lista[1400], *p, *fim;
  unsigned n = 0, linha = 0;
  lista[0] = 0;
  if (usbAndroidInj[0]) snprintf(lista, sizeof lista, "%s", usbAndroidInj);
#ifdef __ANDROID__
  else android_volumes_externos(lista, sizeof lista);
#endif
  for (p = lista; p && n < max; p = fim ? fim + 1 : NULL, linha++) {
    fim = strchr(p, '\n');
    if (fim) *fim = 0;
    if (linha && *p && strlen(p) < 600) snprintf(dirs[n++], 600, "%s", p);
  }
  return n;
}

// Filesystem type of the mount holding `path`. A /storage/<id> fuse view is
// backed by /mnt/media_rw/<id>, which carries the real type when visible.
static void usbFs(const char *path, char *fs, size_t nfs) {
  char linha[1024], dev[512], mp[512], tipo[64], best[512] = "", alvo[600] = "";
  const char *id;
  FILE *f = fopen(usbMounts[0] ? usbMounts : "/proc/mounts", "r");
  snprintf(fs, nfs, "?");
  if (!strncmp(path, "/storage/", 9) && (id = path + 9)[0]) {
    size_t l = strcspn(id, "/");
    snprintf(alvo, sizeof alvo, "/mnt/media_rw/%.*s", (int)l, id);
  }
  while (f && fgets(linha, sizeof linha, f)) {
    size_t l;
    if (sscanf(linha, "%511s %511s %63s", dev, mp, tipo) != 3) continue;
    l = strlen(mp);
    if (alvo[0] && !strcmp(mp, alvo)) { snprintf(fs, nfs, "%s", tipo); fclose(f); return; }
    if (!strncmp(path, mp, l) && (path[l] == 0 || path[l] == '/' || l == 1) && l >= strlen(best)) {
      snprintf(best, sizeof best, "%s", mp);
      snprintf(fs, nfs, "%s", tipo);
    }
  }
  if (f) fclose(f);
#ifdef __ANDROID__
  // FUSE may hide media_rw in the app namespace. statfs can expose the
  // backing type; if it still reports FUSE, keep the conservative limit.
  if (!strcmp(fs, "?") || !strncmp(fs, "fuse", 4) || !strcmp(fs, "sdcardfs")) {
    struct statfs st;
    if (statfs(path, &st) == 0) {
      if (st.f_type == 0x4d44) snprintf(fs, nfs, "vfat");
      else if (st.f_type == 0xef53) snprintf(fs, nfs, "ext4");
      else if ((unsigned long)st.f_type == 0x2011bab0) snprintf(fs, nfs, "exfat");
    }
  }
#endif
}

// Only known large-file filesystems bypass the FAT32/unknown backing cap.
static int usbFsLimitado(const char *fs) {
  return strcmp(fs, "exfat") && strcmp(fs, "ext4") && strcmp(fs, "ext3") &&
         strcmp(fs, "ext2") && strcmp(fs, "ntfs") && strcmp(fs, "ntfs3") &&
         strcmp(fs, "xfs") && strcmp(fs, "btrfs") && strcmp(fs, "f2fs") &&
         strcmp(fs, "apfs") && strcmp(fs, "hfsplus") && strcmp(fs, "tmpfs") &&
         strcmp(fs, "overlay");
}

// FAT32 and unknown backing types hold files below 4 GiB. The limit belongs
// to the active root, regardless of later USB preference changes.
static int usbFatExcede(uint64_t tam) {
  int r;
  pthread_mutex_lock(&trava);
  r = pastaFat && tam >= (4ull << 30);
  pthread_mutex_unlock(&trava);
  return r;
}

// Com usbTrava presa. Com usbExtra (teste) o scan olha SO ele: as bases de
// verdade variam por maquina e tornariam o teste indeterminado.
static int usbEscanear(char *raiz, unsigned nr, int criar) {
  char bases[USB_MAX_BASES][700];
  unsigned n = usbTodasBases(usbExtra, bases);
  int found = 0, exists = 0;
  char dirs[4][600], fs[64];
  unsigned k, i = 0;
  UsbLog *entry;
  usbNVistos = 0;
  atomic_store(&usbMotivo, P2PM_USB_NONE);
  for (entry = usbLogs; entry; entry = entry->next) entry->seen = 0;
  for (i = 0; i < n; i++) {
    const char *base = bases[i];
    struct stat st;
    if (lstat(base, &st) != 0) {
      if (errno != ENOENT) { exists = 1; usbResultado(base, P2PM_USB_IO, errno); }
      continue;
    }
    exists = 1;
    if (!S_ISDIR(st.st_mode)) { usbResultado(base, P2PM_USB_NOT_MOUNT, 0); continue; }
    if (usbEscanearDir(base, raiz, nr, 0, criar)) { found = 1; break; }
  }
  if (!found && !usbExtra[0]) {
    k = usbAndroidDirs(dirs, 4);
    usbApp = 1;
    for (i = 0; i < k && !found; i++) {
      exists = 1;
      if (usbServe(dirs[i], raiz, nr, criar) > 0) {
        uint64_t livre = 0;
        found = 1;
        usbFs(dirs[i], fs, sizeof fs);
        if (livreReal(dirs[i], &livre) != 0) livre = 0;
        if (strcmp(usbAndroidLogado, dirs[i])) {
          snprintf(usbAndroidLogado, sizeof usbAndroidLogado, "%s", dirs[i]);
          printf("[p2p] usb android: %s %s livre=%.1f\n", dirs[i], fs, (double)livre / 1073741824.0);
        }
      }
    }
    usbApp = 0;
  }
  if (!exists) usbResultado("", P2PM_USB_NO_BASE, 0);
  if (found) {
    atomic_store(&usbMotivo, P2PM_USB_NONE);
    usbFs(raiz, fs, sizeof fs);
    if (!usbApp && !strncmp(raiz, usbTizenDir, strlen(usbTizenDir)) && strcmp(usbTizenLogado, raiz)) {
      uint64_t livre = 0;
      if (livreReal(raiz, &livre) != 0) livre = 0;
      snprintf(usbTizenLogado, sizeof usbTizenLogado, "%s", raiz);
      printf("[p2p] usb tizen: %s %s free=%.1f\n", raiz, fs, (double)livre / 1073741824.0);
    }
    int fat = usbFsLimitado(fs);
    if (fat && !usbFat) printf("[p2p] usb: %s is FAT32 or unknown backing (%s), files >= 4 GiB refused\n", raiz, fs);
    usbFat = fat;
  }
  for (UsbLog **p = &usbLogs; *p;) {
    static const char *const names[] = {
      "accepted", "no base", "not a mount root", "already seen",
      "timeout budget", "data dir", "I/O failure", "read-only", "P2P disabled"
    };
    entry = *p;
    if (!entry->seen) { *p = entry->next; free(entry); continue; }
    if (entry->reason != entry->previous || entry->error != entry->previousError) {
      if (entry->reason == P2PM_USB_NO_BASE) printf("[p2p] usb: no base\n");
      else if (entry->reason == P2PM_USB_NONE) printf("[p2p] usb: %s accepted\n", entry->path);
      else printf("[p2p] usb: %s rejeitado: %s (errno=%d)\n",
                  entry->path, names[entry->reason], entry->error);
      entry->previous = entry->reason;
      entry->previousError = entry->error;
    }
    p = &entry->next;
  }
  fflush(stdout);
  return found;
}

static int pastasInternas(char *raiz, unsigned nr, char *mae, unsigned nm) {
  const char *d;
  char *b;
  if (raizInjetada[0]) snprintf(raiz, nr, "%s", raizInjetada);
  else {
    d = dados_dir();
#ifdef NV_ANDROID
    d = getenv("NUVIO_P2P_CACHE");
#endif
    if (!d || !d[0]) return 0;
    if (snprintf(raiz, nr, "%s/p2p", d) >= (int)nr) return 0;
  }
  snprintf(mae, nm, "%s", raiz);
  b = strrchr(mae, '/');
  if (b && b != mae) *b = 0;
  else snprintf(mae, nm, ".");
  return 1;
}

// With usbTrava held: a writable pathname alone does not identify a drive.
static int usbCacheValido(void) {
  char volume[640];
  struct stat st;
  if (snprintf(volume, sizeof volume, "%s/..", usbRaiz) >= (int)sizeof volume ||
      stat(volume, &st) != 0 || st.st_dev != usbVolume.st_dev ||
      st.st_ino != usbVolume.st_ino || lstat(usbRaiz, &st) != 0 ||
      !S_ISDIR(st.st_mode)) return 0;
  return usbENossa(usbRaiz) && access(usbRaiz, W_OK) == 0;
}

// Pasta raiz do motor (<dados>/p2p ou <usb>/p2p) e a mae dela (statvfs). 0 sem pasta.
// Com USB preferida a raiz escolhida e LEMBRADA em usbRaiz: sem isto cada
// chamada re-sondava e um pendrive arrancado no meio da sessao trocava a raiz
// em silencio. Raiz lembrada que sumiu (access falha): re-sonda uma vez e,
// nao achando, cai para o interno — a promessa do ajuste.
static int pastas(char *raiz, unsigned nr, char *mae, unsigned nm, int *fat) {
  if (fat) *fat = 0;
  if (!raizInjetada[0] && ajustes_p2p_ligado() && atomic_load(&usbPreferido)) {
    int ok;
    pthread_mutex_lock(&usbTrava);
    if (usbResolvido && usbRaiz[0] && !usbCacheValido()) {
      usbRaiz[0] = 0;
      usbResolvido = 0;
    }
    if (!usbResolvido || (!usbRaiz[0] && agora() - usbQuando >= 2.0)) {
      usbResolvido = 1;
      if (usbEscanear(raiz, nr, 1)) {
        char volume[640];
        snprintf(usbRaiz, sizeof usbRaiz, "%s", raiz);
        snprintf(volume, sizeof volume, "%s/..", usbRaiz);
        if (stat(volume, &usbVolume) != 0 || !usbCacheValido()) usbRaiz[0] = 0;
      } else usbRaiz[0] = 0;
      atomic_store(&usbTem, usbRaiz[0] != 0);
      usbQuando = agora();
    }
    if (usbRaiz[0]) {
      snprintf(raiz, nr, "%s", usbRaiz);
      if (fat) *fat = usbFat;
      ok = 1;
    }
    else ok = 0;
    pthread_mutex_unlock(&usbTrava);
    if (!ok) return pastasInternas(raiz, nr, mae, nm);
    snprintf(mae, nm, "%s", raiz);
    { char *b = strrchr(mae, '/'); if (b && b != mae) *b = 0; else snprintf(mae, nm, "."); }
    return 1;
  }
  return pastasInternas(raiz, nr, mae, nm);
}

// #334: teto DURO da pasta. Automatico = o de sempre (metade do livre, no
// maximo P2PM_DURO_MAX_MB). Fixo = ate o pedido, deixando P2PM_RESERVA_MB
// livres na TV, e nunca menos que o Automatico (escolher "mais" nao pode dar
// menos). Na TV do #334 (1,37 GB livres) o Automatico da 685 MB e qualquer
// fixo da 857 MB: o limite nao salva um 4K de 15 GB; so a janela (ver
// docs/plans/p2p/janela-334.md) salva.
uint64_t p2pmotor_teto_duro(uint64_t livre, unsigned escolhaMb) {
  uint64_t auto_ = (uint64_t)P2PM_DURO_MAX_MB << 20, fixo, reserva;
  if (livre / 2 < auto_) auto_ = livre / 2;
  if (!escolhaMb) return auto_;
  reserva = (uint64_t)P2PM_RESERVA_MB << 20;
  fixo = (uint64_t)escolhaMb << 20;
  if (livre <= reserva) fixo = 0;
  else if (livre - reserva < fixo) fixo = livre - reserva;
  return fixo > auto_ ? fixo : auto_;
}
void p2pmotor_definir_limite_mb(unsigned mb) { atomic_store(&limiteMb, mb); }

// Junta uma pasta de sobra na fila de apagamento em fio solto, recusando
// quando o motor esta de pe — a parada normal ja apaga pastaRaiz. UMA fila
// (nao uma por pasta): limpar_sobra acha a interna e as de pendrive na mesma
// passada e um segundo agendar() nao pode ser recusado pela primeira limpeza
// — era o que deixava a sobra interna viva quando uma USB existia (revisao).
typedef struct SobraNo { struct SobraNo *prox; char *raiz; struct stat identidade; } SobraNo;
static SobraNo *sobraFila;      // so com a trava
static int sobraUsb;           // USB discovery pending on the cleanup worker
static void usbVarreSobra(const char *raizAtual);
static void sobraAgendar(const char *raiz);
static void sobraLancar(void);

void p2pmotor_definir_usb(int usar) {
  char velha[600];
  // Chamada a cada quadro (vigiarMotorP2p) e quase sempre com a MESMA
  // escolha: so transicao faz trabalho. Sem isto, com o ajuste em Interno,
  // usbResolvido/usbQuando zeravam a cada quadro, o cache de ~2 s de
  // usb_presente nunca vingava e a UI re-escaneava /tmp/usb inteiro por
  // quadro (mkdir + sonda de escrita) — travada com volume lento ou ausente.
  if (atomic_exchange(&usbPreferido, usar ? 1 : 0) == (usar ? 1 : 0)) return;
  if (usar) return;
  // Desligou: esquece a raiz e agenda apagar a pasta que ficou no pendrive —
  // so se o motor nao estiver de pe nela (a parada normal ja apaga pastaRaiz).
  pthread_mutex_lock(&usbTrava);
  snprintf(velha, sizeof velha, "%s", usbResolvido && usbCacheValido() ? usbRaiz : "");
  usbRaiz[0] = 0;
  usbResolvido = 0;
  atomic_store(&usbTem, 0);
  usbQuando = 0;
  pthread_mutex_unlock(&usbTrava);
  if (velha[0] && usbENossa(velha)) sobraAgendar(velha);
}

int p2pmotor_usb_presente(void) {
  char tmp[600];
  int r;
  if (!ajustes_p2p_ligado()) return 0;
  pthread_mutex_lock(&usbTrava);
  // A UI pergunta a cada quadro: resposta vale por ~2 s. O scan daqui NAO
  // cria pasta (criar=0): espiar nao pode sujar o pendrive de ninguem.
  if (agora() - usbQuando >= 2.0) {
    if (usbResolvido && usbRaiz[0] && !usbCacheValido()) { usbRaiz[0] = 0; usbResolvido = 0; }
    if (!usbResolvido || !usbRaiz[0]) {
      usbResolvido = 0;
      // Presence alone does not authorize allocation of the cached path.
      if (usbEscanear(tmp, sizeof tmp, 0)) snprintf(usbRaiz, sizeof usbRaiz, "%s", tmp);
      else usbRaiz[0] = 0;
    }
    atomic_store(&usbTem, usbRaiz[0] != 0);
    usbQuando = agora();
  }
  r = atomic_load(&usbTem);
  pthread_mutex_unlock(&usbTrava);
  return r;
}

P2pmUsbMotivo p2pmotor_usb_motivo(void) {
  return ajustes_p2p_ligado() ? (P2pmUsbMotivo)atomic_load(&usbMotivo) : P2PM_USB_DISABLED;
}

// Drive label: the mount-root component (/storage/<id>, <base>/<id>) or the last one.
static void usbRotulo(const char *dir, char *nome, size_t n) {
  const char *p = NULL, *fim;
  unsigned i;
  if (!strncmp(dir, "/storage/", 9)) p = dir + 9;
  for (i = 0; !p && i < sizeof usbBases / sizeof usbBases[0]; i++) {
    size_t l = strlen(usbBases[i]);
    if (!strncmp(dir, usbBases[i], l) && dir[l] == '/') p = dir + l + 1;
  }
  if (!p || !*p) {
    p = strrchr(dir, '/');
    p = p ? p + 1 : dir;
  }
  fim = p + strcspn(p, "/");
  snprintf(nome, n, "%.*s", (int)(fim - p), p);
}

// What the USB cache modal shows. Refreshed at most every 2 s: the cache walk
// and /proc/mounts read are I/O, and the UI asks every frame. No writes.
void p2pmotor_usb_info(P2pmUsbInfo *o) {
  static P2pmUsbInfo memo;
  static double quando = -10.0;
  char dir[700] = "";
  int presente = p2pmotor_usb_presente();
  P2pmUsbMotivo motivo = p2pmotor_usb_motivo();
  memset(o, 0, sizeof *o);
  pthread_mutex_lock(&usbTrava);
  if (agora() - quando < 2.0 && (memo.estado == 1) == presente && memo.motivo == motivo) {
    *o = memo;
    pthread_mutex_unlock(&usbTrava);
    return;
  }
  o->motivo = motivo;
  if (presente && usbRaiz[0]) {
    o->estado = 1;
    snprintf(dir, sizeof dir, "%s", usbRaiz);
  } else if (o->motivo > P2PM_USB_NO_BASE && o->motivo < P2PM_USB_DISABLED && usbRecusado[0]) {
    o->estado = 2;
    snprintf(dir, sizeof dir, "%s", usbRecusado);
  }
  if (o->estado) {
    struct statvfs v;
    LimTrab lim = { .max = 256, .ate = agora() + 0.05 };
    char mae[700];
    snprintf(mae, sizeof mae, "%s", dir);
    if (o->estado == 1) { char *b = strrchr(mae, '/'); if (b && b != mae) *b = 0; }
    snprintf(o->caminho, sizeof o->caminho, "%s", mae);
    usbRotulo(mae, o->nome, sizeof o->nome);
    usbFs(mae, o->fs, sizeof o->fs);
    if (statvfs(mae, &v) == 0) {
      o->total = (uint64_t)v.f_blocks * (uint64_t)v.f_frsize;
      o->livre = (uint64_t)v.f_bavail * (uint64_t)v.f_frsize;
    }
    o->fat = usbFsLimitado(o->fs);
    if (o->estado == 1) {
      o->gravavel = access(mae, W_OK) == 0;
      o->cache = percorrerL(usbRaiz, 0, 0, &lim);
      o->limite = p2pmotor_teto_duro(o->livre, atomic_load(&limiteMb));
      o->emUso = atomic_load(&usbPreferido) != 0;
    }
  }
  memo = *o;
  quando = agora();
  pthread_mutex_unlock(&usbTrava);
}

// #334: a janela fica um pouco abaixo do teto duro (a vigia para em > duro e a
// pasta tem tambem o resume e as pecas de borda com outros arquivos). O motor
// so usa a janela para arquivo MAIOR que ela; menor fica inteiro, como antes.
uint64_t p2pmotor_janela(uint64_t duro) {
  uint64_t folga = (uint64_t)P2PM_JANELA_FOLGA_MB << 20;
  return duro > folga ? duro - folga : 0;
}
int p2pmotor_cabe(uint64_t tam, uint64_t duro) {
  if (tam <= duro) return 1;
  return p2pmotor_janela(duro) >= ((uint64_t)P2PM_JANELA_MIN_MB << 20);
}

// Orcamento ao subir. statvfs falhou: recusa (sem medir, nao ha teto honesto).
static int orcamento(const char *mae, uint64_t *mole, uint64_t *duro, uint64_t *livre) {
  uint64_t l = 0;
  if (livreFn(mae, &l) != 0) return P2P_ERR_DISCO;
  *livre = l;
  *mole = (uint64_t)P2PM_DISCO_MB << 20;
  if (l / 4 < *mole) *mole = l / 4;
  *duro = p2pmotor_teto_duro(l, atomic_load(&limiteMb));
  if (l < ((uint64_t)P2PM_LIVRE_MIN_MB << 20)) return P2P_ERR_SEM_ESPACO;
  return P2P_OK;
}

// #297: o livre caiu ~1,2 GB num 4K com a pasta do motor em 16 MB, e nao
// voltou depois do motor desligado. Sem saber onde o espaco foi, lista o que
// pesa na pasta do app (pai de <dados>) e no /tmp. So na TV (.tpk), e so se o
// pai for mesmo a sandbox do app (.../apps_rw/<pacote>): no desktop <dados>
// pode ser $HOME/.nuvio e o pai seria a home inteira. Roda DEPOIS que os
// escritores pararam, com limite de entradas e de tempo. Fio de parada ou do
// pedido, nunca o da tela nem o da vigia.
#define ONDE_MAX_ENTRADAS 5000u
#define ONDE_MAX_MS 200
#ifdef NV_TPK
static int ehSandboxApp(const char *app) {
  const char *p = strstr(app, "/apps_rw/");
  if (!p) return 0;
  p += 9;
  return *p && !strchr(p, '/');
}
static void ondeFoi(const char *mae) {
  char app[600], f[1200], linha[900];
  const char *outros[3];
  size_t n, k = 0;
  DIR *d;
  struct dirent *e;
  unsigned i;
  LimTrab lim;
  if (raizInjetada[0]) return;
  snprintf(app, sizeof app, "%s", mae);
  n = strlen(app);
  while (n > 1 && app[n - 1] == '/') app[--n] = 0;
  { char *b = strrchr(app, '/'); if (!b || b == app) return; *b = 0; }
  if (!ehSandboxApp(app)) return;
  memset(&lim, 0, sizeof lim);
  lim.max = ONDE_MAX_ENTRADAS;
  lim.ate = agora() + ONDE_MAX_MS / 1000.0;
  linha[0] = 0;
  if ((d = opendir(app))) {
    while ((e = readdir(d)) && !lim.estourou) {
      uint64_t mb;
      if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
      if (snprintf(f, sizeof f, "%s/%s", app, e->d_name) >= (int)sizeof f) continue;
      mb = percorrerL(f, 0, 0, &lim) >> 20;
      if (mb && k < sizeof linha - 80)
        k += (size_t)snprintf(linha + k, sizeof linha - k, " %s=%llu", e->d_name, (unsigned long long)mb);
    }
    closedir(d);
  }
  outros[0] = "/tmp"; outros[1] = getenv("TMPDIR"); outros[2] = "/opt/usr/home/owner/share";
  for (i = 0; i < 3 && !lim.estourou; i++) {
    uint64_t mb;
    if (!outros[i] || !outros[i][0]) continue;
    mb = percorrerL(outros[i], 0, 0, &lim) >> 20;
    if (mb && k < sizeof linha - 80)
      k += (size_t)snprintf(linha + k, sizeof linha - k, " %s=%llu", outros[i], (unsigned long long)mb);
  }
  printf("[p2p-motor] onde (MB) em %s:%s%s\n", app, linha[0] ? linha : " nada acima de 1 MB",
         lim.estourou ? " (parcial: limite)" : "");
  fflush(stdout);
}
#else
static void ondeFoi(const char *mae) { (void)mae; }   // so a TV Samsung tem sandbox conhecida
#endif

static int contem(const char *s, const char *agulha) {   // sem strcasestr
  size_t n = strlen(agulha);
  for (; s && *s; s++) {
    size_t i = 0;
    while (i < n && s[i] && tolower((unsigned char)s[i]) == agulha[i]) i++;
    if (i == n) return 1;
  }
  return 0;
}
static int ehEspaco(const char *msg) {
  return contem(msg, "no space") || contem(msg, "enospc") || contem(msg, "disk full");
}

// ------------------------------------------------------------------ API simples
int p2pmotor_disponivel(void) { return ops() != NULL; }

const char *p2pmotor_versao(void) {
  const P2pmOps *o = ops();
  return o && o->versao ? o->versao() : "";
}

const char *p2pmotor_motivo_indisponivel(void) {
  // So para o log (a tela diz "sem motor neste pacote", com i18n).
  if (ops()) return "";
  if (p2pmotor_motor_erro()[0]) return p2pmotor_motor_erro();
#ifdef __EMSCRIPTEN__
  return "browser has no TCP/UDP sockets (.wgt): no P2P engine";
#else
  return "package built without the P2P engine (NV_P2P_MOTOR)";
#endif
}
int p2pmotor_resumo(char *detalhe, unsigned n) {
  char raiz[600], mae[600];
  uint64_t mole = 0, duro = 0, livre = 0;
  int e;
  if (detalhe && n) detalhe[0] = 0;
  if (!ops()) return P2P_ERR_DESLIGADO;
  if (!pastas(raiz, sizeof raiz, mae, sizeof mae, NULL)) return P2P_ERR_DISCO;
  e = orcamento(mae, &mole, &duro, &livre);
  atomic_store(&tetoUltimoMb, e == P2P_OK ? (unsigned)(duro >> 20) : 0u);
  if (detalhe && n) snprintf(detalhe, n, "%s", p2pmotor_versao());
  return e;
}
unsigned p2pmotor_teto_mb(void) { return atomic_load(&tetoUltimoMb); }

void p2pmotor_arquivo_atual(uint64_t *tam, uint64_t *teto, uint64_t *livre) {
  pthread_mutex_lock(&trava);
  if (tam) *tam = tamArquivo;
  if (teto) *teto = (uint64_t)atomic_load(&tetoUltimoMb) << 20;
  if (livre) *livre = livreSubida;
  pthread_mutex_unlock(&trava);
}

// #334: a pasta so era apagada ao PARAR o motor e no proximo SUBIR. TV
// desligada com o filme tocando = ate o teto duro preso no disco ate a pessoa
// abrir outro torrent. Agora o inicio do app tambem apaga.
static void *limparFio(void *x) {
  SobraNo *no, *fila = x;
  int scan;
  pthread_mutex_lock(&trava);
  scan = sobraUsb;
  sobraUsb = 0;
  pthread_mutex_unlock(&trava);
  if (scan && ajustes_p2p_ligado()) usbVarreSobra("");
  for (no = fila; no; no = no->prox) {
    uint64_t b = percorrer(no->raiz, 0, 0);
    struct stat st;
    if (stat(no->raiz, &st) == 0 && st.st_dev == no->identidade.st_dev &&
        st.st_ino == no->identidade.st_ino) percorrer(no->raiz, 1, 0);
    printf("[p2p-motor] inicio: sobra de %.0f MB apagada em %s\n", (double)b / 1048576.0, no->raiz);
    fflush(stdout);
    free(no->raiz);
  }
  while (fila) { no = fila->prox; free(fila); fila = no; }
  pthread_mutex_lock(&trava);
  limpando = 0;
  sobraLancar();   // ninguem agendou durante a varredura? sobraFila vazia: sai
  pthread_cond_broadcast(&sinal);
  pthread_mutex_unlock(&trava);
  return NULL;
}
static void sobraLancar(void) {
  pthread_t t;
  pthread_attr_t a;
  SobraNo *fila;
  if ((!sobraFila && !sobraUsb) || motor || resolvendo || parando || limpando) return;
  fila = sobraFila;
  sobraFila = NULL;
  limpando = 1;
  pthread_attr_init(&a);
  pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);
  if (pthread_create(&t, &a, limparFio, fila) != 0) { limpando = 0; sobraFila = fila; }
  pthread_attr_destroy(&a);
}
static void sobraAgendar(const char *raiz) {
  SobraNo *no;
  if (!raiz || !raiz[0]) return;
  no = malloc(sizeof *no);
  if (!no) return;
  no->raiz = strdup(raiz);
  if (!no->raiz) { free(no); return; }
  if (stat(raiz, &no->identidade) != 0) { free(no->raiz); free(no); return; }
  pthread_mutex_lock(&trava);
  no->prox = sobraFila;
  sobraFila = no;
  sobraLancar();
  pthread_mutex_unlock(&trava);
}
// .nuvio dentro da pasta: o unico sinal de que p2p/ e nossa. Sem ela,
// Movies/p2p ou um p2p/ na raiz do volume da pessoa nao e sobra.
static void usbMarcarDono(const char *raiz) {
  char m[720];
  FILE *f;
  if (!raiz || !raiz[0]) return;
  if (snprintf(m, sizeof m, "%s/%s", raiz, USB_DONO) >= (int)sizeof m) return;
  f = fopen(m, "w");
  if (!f) return;
  fputs("nuvio\n", f);
  fclose(f);
}
static int usbENossa(const char *p2p) {
  char m[720];
  struct stat st;
  if (snprintf(m, sizeof m, "%s/%s", p2p, USB_DONO) >= (int)sizeof m) return 0;
  return lstat(m, &st) == 0 && S_ISREG(st.st_mode);
}
// #205: pasta p2p/ que sobrou num pendrive de uma sessao anterior (raiz
// escolhida mudou ou o ajuste foi desligado). So le (stat), nunca cria.
// So agenda se a marca .nuvio estiver la: um p2p/ da pessoa (Movies/p2p,
// p2p/ na raiz do volume) nao e sobra. Chamar com o motor parado.
static void usbVarreSobraDir(const char *base, int fundo, const char *raizAtual) {
  DIR *d;
  struct dirent *e;
  char f[700], p2p[720];
  struct stat st, parent;
  LimTrab lim = { .max = 256, .ate = agora() + 0.05 };
  int volume;
  if (!base || !base[0] || fundo > 3 || stat(base, &st) != 0 || !S_ISDIR(st.st_mode)) return;
  if (snprintf(f, sizeof f, "%s/..", base) >= (int)sizeof f || stat(f, &parent) != 0) return;
  volume = st.st_dev != parent.st_dev || (usbExtra[0] && access(base, W_OK) == 0);
  if (volume && snprintf(p2p, sizeof p2p, "%s/p2p", base) < (int)sizeof p2p &&
      lstat(p2p, &st) == 0 && S_ISDIR(st.st_mode) &&
      strcmp(p2p, raizAtual) && usbENossa(p2p))
    sobraAgendar(p2p);
  if (volume) return; // Never descend into an accepted or rejected mounted volume.
  if (!(d = opendir(base))) return;
  while ((e = readdir(d))) {
    if (++lim.n > lim.max || agora() > lim.ate) break;
    if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..") || e->d_name[0] == '.') continue;
    if (snprintf(f, sizeof f, "%s/%s", base, e->d_name) >= (int)sizeof f) continue;
    if (lstat(f, &st) != 0 || S_ISLNK(st.st_mode)) continue;   // links: ver o scan
    double start = agora();
    usbVarreSobraDir(f, fundo + 1, raizAtual);
    lim.ate += agora() - start;
  }
  closedir(d);
}
static void usbVarreSobra(const char *raizAtual) {
  unsigned n, i;
  char extra[600], bases[USB_MAX_BASES][700];
  extra[0] = 0;
  pthread_mutex_lock(&usbTrava);
  snprintf(extra, sizeof extra, "%s", usbExtra);
  n = usbTodasBases(extra, bases);
  for (i = 0; i < n; i++) {
    const char *base = bases[i];
    struct stat st;
    if (lstat(base, &st) == 0 && S_ISDIR(st.st_mode)) usbVarreSobraDir(base, 0, raizAtual);
  }
  if (!extra[0]) {
    char dirs[4][600], p2p[720];
    struct stat st;
    unsigned k = usbAndroidDirs(dirs, 4);
    for (i = 0; i < k; i++)   // each app dir is a drive root: only <dir>/p2p can be ours
      if (snprintf(p2p, sizeof p2p, "%s/p2p", dirs[i]) < (int)sizeof p2p &&
          lstat(p2p, &st) == 0 && S_ISDIR(st.st_mode) && strcmp(p2p, raizAtual) && usbENossa(p2p))
        sobraAgendar(p2p);
  }
  pthread_mutex_unlock(&usbTrava);
}
void p2pmotor_limpar_sobra(void) {
  char raiz[600], mae[600];
  struct stat st;
  if (p2pmotor_ativo()) return;          // motor de pe cuida da pasta dele
  if (!pastasInternas(raiz, sizeof raiz, mae, sizeof mae)) return;
  pthread_mutex_lock(&trava);
  if (ajustes_p2p_ligado()) sobraUsb = 1;
  sobraLancar();
  pthread_mutex_unlock(&trava);
  if (lstat(raiz, &st) == 0) sobraAgendar(raiz);
}

void p2pmotor_cancelar(void) {
  atomic_fetch_add(&geracao, 1u);
  pthread_mutex_lock(&trava);
  pthread_cond_broadcast(&sinal);
  pthread_mutex_unlock(&trava);
}

int p2pmotor_e_url(const char *url) {
  int r;
  if (!url) return 0;
  pthread_mutex_lock(&trava);
  r = prefixoUrl[0] && !strncmp(url, prefixoUrl, strlen(prefixoUrl));
  pthread_mutex_unlock(&trava);
  return r;
}

int p2pmotor_ativo(void) {
  int r;
  pthread_mutex_lock(&trava);
  r = motor != NULL || parando || resolvendo;
  pthread_mutex_unlock(&trava);
  return r;
}

int p2pmotor_motivo_parada(void) { return atomic_exchange(&motivoParada, 0); }

void p2pmotor_estado(P2pmEstado *e) {
  if (!e) return;
  pthread_mutex_lock(&trava);
  *e = estado;
  e->ativo = motor != NULL;
  pthread_mutex_unlock(&trava);
}

void p2pmotor_segurar(int s) { atomic_store(&segurado, s); }
int p2pmotor_segurado(void) { return atomic_load(&segurado); }
unsigned p2pmotor_teste_descartados(void) { return atomic_load(&descartados); }

void p2pmotor_teste_injetar(const P2pmOps *o, P2pmLivreFn livre, P2pmSondaFn sonda,
                            const char *raiz, unsigned vms) {
  pthread_mutex_lock(&trava);
  if (o) { opsInjetado = o; temOpsInjetado = 1; }
  if (livre) livreFn = livre;
  if (sonda) sondaFn = sonda;
  if (raiz) snprintf(raizInjetada, sizeof raizInjetada, "%s", raiz);
  if (vms) vigiaMs = vms;
  pthread_mutex_unlock(&trava);
}

void p2pmotor_teste_usb_android(const char *lista, const char *mounts) {
  pthread_mutex_lock(&usbTrava);
  snprintf(usbAndroidInj, sizeof usbAndroidInj, "%s", lista ? lista : "");
  snprintf(usbMounts, sizeof usbMounts, "%s", mounts ? mounts : "");
  usbAndroidLogado[0] = 0;
  usbResolvido = 0;
  usbQuando = 0;
  pthread_mutex_unlock(&usbTrava);
}

unsigned p2pmotor_teste_usb_tizen(const char *dir, char *out, unsigned nout) {
  char bases[USB_MAX_BASES][700];
  unsigned n, i, len = 0;
  pthread_mutex_lock(&usbTrava);
  snprintf(usbTizenDir, sizeof usbTizenDir, "%s", dir);
  usbTizenLogado[0] = 0;
  usbResolvido = 0;
  usbQuando = 0;
  n = usbTodasBases("", bases);
  pthread_mutex_unlock(&usbTrava);
  out[0] = 0;
  for (i = 0; i < n && len < nout; i++) len += snprintf(out + len, nout - len, "%s\n", bases[i]);
  return n;
}

void p2pmotor_teste_usb_base(const char *base) {
  pthread_mutex_lock(&usbTrava);
  if (base) snprintf(usbExtra, sizeof usbExtra, "%s", base);
  else usbExtra[0] = 0;
  usbResolvido = 0;   // novo ponto de montagem: scan na proxima consulta
  usbQuando = 0;
  pthread_mutex_unlock(&usbTrava);
}

// ------------------------------------------------------------------ parar
static void *pararFio(void *x) {
  const P2pmOps *o = x;
  void *m;
  int jv;
  pthread_t v;
  char tid[65], sid[65], raiz[600], mae[600];
  int inventario;
  double t0 = agora();
  pthread_mutex_lock(&trava);
  // ORDEM 1: ninguem mais usa o ponteiro (o pedido sai em <= 20 ms depois da
  // geracao subir; a sonda e cancelada pelo mesmo atomico; a vigia solta a
  // referencia ao fim da volta).
  while (usuarios > 0) esperarSinal(200);
  m = motor; motor = NULL;
  jv = vigiaViva; vigiaViva = 0; v = fioVigia;
  snprintf(tid, sizeof tid, "%s", tidAtual);
  snprintf(sid, sizeof sid, "%s", sidAtual);
  snprintf(raiz, sizeof raiz, "%s", pastaRaiz);
  snprintf(mae, sizeof mae, "%s", pastaMae);
  inventario = ondeFoiPendente; ondeFoiPendente = 0;
  prefixoUrl[0] = 0;
  pthread_mutex_unlock(&trava);
  // ORDEM 2: a vigia saiu do laco (vigiaSair foi posto por quem pediu a parada).
  if (jv) pthread_join(v, NULL);
  // ORDEM 3: destroy (para os escritores do libtorrent e o servidor HTTP).
  if (m) {
    P2pmStats st;
    memset(&st, 0, sizeof st);
    if (o->stats && o->stats(m, &st) == 0)
      printf("[p2p-motor] parando: disco do motor %.0f MB, RAM %.1f MB, %u pares\n",
             (double)st.disco_motor / 1048576.0, (double)st.ram_usada / 1048576.0, st.pares);
    if (sid[0] && o->parar_stream) o->parar_stream(m, sid);
    if (tid[0] && o->remover) o->remover(m, tid);
    o->destruir(m);
  }
  // Escritores parados: inventario (limitado) antes de apagar o cache.
  if (inventario) ondeFoi(mae);
  // ORDEM 4: so agora a pasta, com nenhum escritor vivo.
  if (raiz[0]) percorrer(raiz, 1, 0);
  pthread_mutex_lock(&trava);
  tidAtual[0] = sidAtual[0] = hashAtual[0] = 0;
  metaPronta = 0;
  memset(&estado, 0, sizeof estado);
  parando = 0;   // tamArquivo/livreSubida ficam: a tela le depois da parada (#334)
  pthread_cond_broadcast(&sinal);
  pthread_mutex_unlock(&trava);
  printf("[p2p-motor] desligado e cache apagado em %.1f s\n", agora() - t0);
  fflush(stdout);
  return NULL;
}

// Com a trava presa. 1 se a parada comecou (ou ja estava em curso).
static int pedirParadaTravado(void) {
  pthread_t t;
  pthread_attr_t a;
  if (parando) return 1;
  if (!motor) return 0;
  parando = 1;
  vigiaSair = 1;
  atomic_fetch_add(&geracao, 1u);
  pthread_cond_broadcast(&sinal);
  pthread_attr_init(&a);
  pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);
  if (pthread_create(&t, &a, pararFio, (void *)ops()) != 0) {
    // Sem fio agora: tenta de novo no proximo quadro. A vigia continua.
    parando = 0;
    vigiaSair = 0;
    pthread_attr_destroy(&a);
    return 0;
  }
  pthread_attr_destroy(&a);
  return 1;
}

void p2pmotor_parar_fundo(void) {
  pthread_mutex_lock(&trava);
  pedirParadaTravado();
  pthread_mutex_unlock(&trava);
}

void p2pmotor_parar(void) {
  p2pmotor_cancelar();
  pthread_mutex_lock(&trava);
  while (resolvendo) esperarSinal(100);
  pedirParadaTravado();
  while (parando) esperarSinal(100);
  pthread_mutex_unlock(&trava);
}

void p2pmotor_saida(void) {
  double fim = agora() + P2PM_SAIDA_MS / 1000.0;
  int pronto;
  p2pmotor_cancelar();
  pthread_mutex_lock(&trava);
  while (resolvendo && agora() < fim) esperarSinal(50);
  pedirParadaTravado();
  while (parando && agora() < fim) esperarSinal(50);
  pronto = !parando && !motor && !resolvendo;
  pthread_mutex_unlock(&trava);
  // Nao terminou: NAO apaga a pasta com o libtorrent escrevendo nela. O
  // proximo subir (sem motor vivo) apaga a sobra.
  if (!pronto) printf("[p2p-motor] saida: destroy ainda em curso; a sobra fica para o proximo inicio\n");
}

// ------------------------------------------------------------------ vigia
static void *vigiaFio(void *x) {
  const P2pmOps *o = x;
  unsigned volta = 0;
  pthread_mutex_lock(&trava);
  for (;;) {
    double fim = agora() + vigiaMs / 1000.0;
    void *m;
    int lerEventos, motivo = 0, espaco = 0, okLivre;
    char tid[65], raiz[600], mae[600];
    uint64_t duro, usado, livre = 0, subida;
    P2pmStats st;
    while (!vigiaSair && agora() < fim) esperarSinal(vigiaMs);
    if (vigiaSair) break;
    if (!motor || parando) continue;
    m = motor;
    usuarios++;
    lerEventos = !resolvendo;
    if (lerEventos) vigiaEventos = 1;
    snprintf(tid, sizeof tid, "%s", tidAtual);
    snprintf(raiz, sizeof raiz, "%s", pastaRaiz);
    snprintf(mae, sizeof mae, "%s", pastaMae);
    duro = estado.disco_duro;
    subida = livreSubida;
    pthread_mutex_unlock(&trava);

    // Tudo isto e I/O ou chamada ao motor: sem a trava.
    usado = percorrer(raiz, 0, 0);
    okLivre = livreFn(mae, &livre);
    memset(&st, 0, sizeof st);
    if (o->stats) o->stats(m, &st);
    if (lerEventos) {
      P2pmEvento ev;
      int k;
      for (k = 0; k < 64; k++) {
        memset(&ev, 0, sizeof ev);
        if (!o->poll(m, &ev)) break;
        if (ev.tipo == P2PM_EV_ERRO && ehEspaco(ev.msg)) espaco = 1;
      }
    }
    if (okLivre != 0) motivo = P2P_ERR_DISCO;
    else if (espaco || usado > duro || livre < ((uint64_t)P2PM_LIVRE_PISO_MB << 20))
      motivo = P2P_ERR_SEM_ESPACO;
    else if (st.ram_usada > ((uint64_t)P2PM_RAM_DURO_MB << 20)) motivo = P2P_ERR_RAM;

    // #297: a cada ~60 s, quem gasta o disco. O livre caindo com a pasta
    // parada aponta para fora do motor.
    if (++volta * vigiaMs >= 60000u) {
      volta = 0;
      printf("[p2p-motor] disco: pasta %.0f MB, motor %.0f MB, baixado %.0f MB, livre %.0f MB (ao subir %.0f MB), janela apagou %.0f MB, rebaixou %llu pecas\n",
             (double)usado / 1048576.0, (double)st.disco_motor / 1048576.0,
             (double)st.baixado / 1048576.0, (double)livre / 1048576.0,
             (double)subida / 1048576.0, (double)st.janela_apagado / 1048576.0,
             (unsigned long long)st.janela_rebaixadas);
      fflush(stdout);
    }
    if (motivo == P2P_ERR_SEM_ESPACO) {
      printf("[p2p-motor] sem espaco: baixado %.0f MB, livre ao subir %.0f MB, janela apagou %.0f MB%s\n",
             (double)st.baixado / 1048576.0, (double)subida / 1048576.0,
             (double)st.janela_apagado / 1048576.0,
             st.janela_sem_suporte ? " (sistema de arquivos sem buraco: janela parada)" : "");
    }

    pthread_mutex_lock(&trava);
    if (lerEventos) vigiaEventos = 0;
    usuarios--;
    estado.disco_usado = usado;
    estado.livre = livre;
    estado.ram_usada = st.ram_usada;
    estado.baixando_bps = st.baixando_bps;
    estado.pares = st.pares;
    pthread_cond_broadcast(&sinal);
    if (motivo && !parando) {
      atomic_store(&motivoParada, motivo);
      if (motivo == P2P_ERR_SEM_ESPACO) ondeFoiPendente = 1;   // roda no pararFio, sem escritores
      printf("[p2p-motor] vigia: erro %d (disco %.0f/%.0f MB, livre %.0f MB, RAM %.1f MB): parando\n",
             motivo, (double)usado / 1048576.0, (double)duro / 1048576.0,
             (double)livre / 1048576.0, (double)st.ram_usada / 1048576.0);
      fflush(stdout);
      pedirParadaTravado();    // poe vigiaSair: este laco sai na proxima volta
    }
  }
  pthread_mutex_unlock(&trava);
  return NULL;
}

// ------------------------------------------------------------------ subir
// Chamado pelo pedido (resolvendo=1, motor NULL, parando 0), SEM a trava.
// Ninguem mais cria motor (so o pedido, e ha um por vez) e nenhum destroy esta
// em curso: apagar a sobra da pasta aqui e seguro.
static int subir(const P2pmOps *o, unsigned g) {
  char raiz[600], mae[600], dad[640], cac[640];
  uint64_t mole = 0, duro = 0, livre = 0;
  P2pmConfig c;
  void *m = NULL;
  int e, fat = 0;
  if (!pastas(raiz, sizeof raiz, mae, sizeof mae, &fat)) return P2P_ERR_DISCO;
  percorrer(raiz, 1, 0);   // sobra de uma sessao que caiu (ou da saida apressada)
  e = orcamento(mae, &mole, &duro, &livre);
  if (e != P2P_OK && atomic_load(&usbPreferido) && !raizInjetada[0]) {
    int eraUsb;
    // A raiz USB escolhida nao serviu (pendrive arrancado, sem espaco):
    // Retry internal storage directly so the same full USB cannot win again.
    pthread_mutex_lock(&usbTrava);
    eraUsb = usbRaiz[0] && !strcmp(raiz, usbRaiz);
    if (eraUsb) { usbRaiz[0] = 0; usbResolvido = 0; usbQuando = 0; atomic_store(&usbTem, 0); }
    pthread_mutex_unlock(&usbTrava);
    if (eraUsb) {
      if (!pastasInternas(raiz, sizeof raiz, mae, sizeof mae)) return P2P_ERR_DISCO;
      fat = 0;
      percorrer(raiz, 1, 0);
      e = orcamento(mae, &mole, &duro, &livre);
      printf("[p2p-motor] USB indisponivel; caiu para interno\n");
    }
  }
  if (e != P2P_OK) {
    printf("[p2p-motor] recusado (erro %d): livre %.0f MB\n", e, (double)livre / 1048576.0);
    if (e == P2P_ERR_SEM_ESPACO) ondeFoi(mae);
    return e;
  }
  snprintf(dad, sizeof dad, "%s/dados", raiz);
  snprintf(cac, sizeof cac, "%s/cache", raiz);
  if ((mkdir(raiz, 0700) != 0 && access(raiz, W_OK) != 0) ||
      (mkdir(dad, 0700) != 0 && access(dad, W_OK) != 0) ||
      (mkdir(cac, 0700) != 0 && access(cac, W_OK) != 0)) {
    printf("[p2p-motor] nao criou a pasta do cache\n");
    return P2P_ERR_DISCO;
  }
  usbMarcarDono(raiz);   // percorrer acima apagou a marca; sem ela a sobra fica
  memset(&c, 0, sizeof c);
  c.dados = dad;
  c.cache = cac;
  c.ram = (uint64_t)P2PM_RAM_MB << 20;
  c.disco = mole;
  c.upload_bps = 256u * 1024u;
  c.janela = p2pmotor_janela(duro);
  if (atomic_load(&geracao) != g) { percorrer(raiz, 1, 0); return P2P_ERR_CANCELADO; }
  if (o->criar(&c, &m) != 0 || !m) {
    printf("[p2p-motor] nao subiu\n");
    percorrer(raiz, 1, 0);
    return P2P_ERR_MOTOR;
  }
  pthread_mutex_lock(&trava);
  if (atomic_load(&geracao) != g) {
    pthread_mutex_unlock(&trava);
    o->destruir(m);
    percorrer(raiz, 1, 0);
    return P2P_ERR_CANCELADO;
  }
  // Sem vigia nao ha teto duro: sem fio para ela, o motor nao fica de pe.
  vigiaSair = 0;
  if (pthread_create(&fioVigia, NULL, vigiaFio, (void *)o) != 0) {
    pthread_mutex_unlock(&trava);
    o->destruir(m);
    percorrer(raiz, 1, 0);
    return P2P_ERR_MOTOR;
  }
  vigiaViva = 1;
  motor = m;
  snprintf(pastaRaiz, sizeof pastaRaiz, "%s", raiz);
  pastaFat = fat;
  snprintf(pastaMae, sizeof pastaMae, "%s", mae);
  memset(&estado, 0, sizeof estado);
  estado.disco_teto = mole;
  estado.disco_duro = duro;
  estado.livre = livre;
  livreSubida = livre;
  estado.ram_teto = (uint64_t)P2PM_RAM_MB << 20;
  estado.ram_duro = (uint64_t)P2PM_RAM_DURO_MB << 20;
  atomic_store(&tetoUltimoMb, (unsigned)(duro >> 20));
  pthread_mutex_unlock(&trava);
  printf("[p2p-motor] subiu %s: disco %.0f MB (duro %.0f MB, janela %.0f MB, limite %u MB, livre %.0f MB), RAM %d MB (duro %d MB)\n",
         p2pmotor_versao(), (double)mole / 1048576.0, (double)duro / 1048576.0,
         (double)c.janela / 1048576.0,
         atomic_load(&limiteMb), (double)livre / 1048576.0, P2PM_RAM_MB, P2PM_RAM_DURO_MB);
  return P2P_OK;
}

// ------------------------------------------------------------------ pedido
typedef struct {
  const P2pmOps *o;
  void *m;
  unsigned g;
  char metaTid[65];   // METADATA que chegou antes do ADDED casar
} Pedido;

static int cancelado(void *u) { return atomic_load(&geracao) != ((Pedido *)u)->g; }

// 1 achou, 0 prazo, -1 erro do torrent (`*espaco` se foi falta de espaco),
// -2 cancelado. Eventos de outro pedido sao descartados (e contados).
static int esperar(Pedido *p, int tipo, uint64_t rid, const char *tid, double prazo,
                   P2pmEvento *ev, int *espaco) {
  double fim = agora() + prazo;
  for (;;) {
    if (cancelado(p)) return -2;
    memset(ev, 0, sizeof *ev);
    while (p->o->poll(p->m, ev)) {
      int nosso;
      switch (ev->tipo) {
        case P2PM_EV_ADDED:
          if (tipo == P2PM_EV_ADDED && ev->rid == rid) return 1;
          atomic_fetch_add(&descartados, 1u);
          break;
        case P2PM_EV_META:
          if (tid && tid[0] && !strcmp(ev->tid, tid)) { if (tipo == P2PM_EV_META) return 1; }
          else if (tipo == P2PM_EV_ADDED) snprintf(p->metaTid, sizeof p->metaTid, "%s", ev->tid);
          else atomic_fetch_add(&descartados, 1u);
          break;
        case P2PM_EV_ERRO:
          nosso = (tid && tid[0] && !strcmp(ev->tid, tid)) || (rid && ev->rid == rid);
          if (nosso) {
            printf("[p2p-motor] erro do torrent: %.120s\n", ev->msg);
            if (espaco) *espaco = ehEspaco(ev->msg);
            return -1;
          }
          atomic_fetch_add(&descartados, 1u);
          break;
        case P2PM_EV_PREPARADO:
          if (tipo == P2PM_EV_PREPARADO && ev->rid == rid) return 1;
          atomic_fetch_add(&descartados, 1u);
          break;
        default: break;
      }
      memset(ev, 0, sizeof *ev);
    }
    if (agora() >= fim) return 0;
    dormirMs(20);
  }
}

// URL local do motor: so http://127.0.0.1 ou localhost. Qualquer outra
// coisa e recusada (o player nunca e mandado para fora por esta rota).
static int urlLocal(const char *u) {
  return !strncmp(u, "http://127.0.0.1:", 17) || !strncmp(u, "http://localhost:", 17);
}

static void soltarTorrent(Pedido *p) {
  char tid[65], sid[65];
  pthread_mutex_lock(&trava);
  snprintf(tid, sizeof tid, "%s", tidAtual);
  snprintf(sid, sizeof sid, "%s", sidAtual);
  tidAtual[0] = sidAtual[0] = hashAtual[0] = 0;
  metaPronta = 0;
  prefixoUrl[0] = 0;
  pthread_mutex_unlock(&trava);
  if (sid[0] && p->o->parar_stream) p->o->parar_stream(p->m, sid);
  if (tid[0] && p->o->remover) p->o->remover(p->m, tid);
}

#define P2PM_MAX_ARQ 4096

static int resolverCom(Pedido *p, const char *h, int fileIdx, const char *fontes,
                       int temporada, int episodio, char *url, unsigned n) {
  char magnet[2400], tid[65], sidVelho[65];
  P2pmEvento ev;
  uint64_t rid = 0;
  size_t qtd = 0, i;
  int mesmo, r, idx, espaco = 0;
  uint64_t tamEscolhido = 0;
  pthread_mutex_lock(&trava);
  tamArquivo = 0;
  mesmo = hashAtual[0] && !strcmp(hashAtual, h) && metaPronta && tidAtual[0];
  snprintf(tid, sizeof tid, "%s", mesmo ? tidAtual : "");
  snprintf(sidVelho, sizeof sidVelho, "%s", sidAtual);
  sidAtual[0] = 0;
  prefixoUrl[0] = 0;
  pthread_mutex_unlock(&trava);
  if (sidVelho[0] && p->o->parar_stream) p->o->parar_stream(p->m, sidVelho);
  if (!mesmo) {
    double t0 = agora();
    // Um torrent por vez: outro hash solta o anterior (pecas, pares e cache).
    soltarTorrent(p);
    if (!p2p_magnet(h, fontes, magnet, sizeof magnet)) return P2P_ERR_HASH;
    p->metaTid[0] = 0;
    if (p->o->add_magnet(p->m, magnet, &rid) != 0) return P2P_ERR_RECUSOU;
    r = esperar(p, P2PM_EV_ADDED, rid, NULL, 10, &ev, &espaco);
    if (r != 1) goto falhou;
    snprintf(tid, sizeof tid, "%s", ev.tid);
    pthread_mutex_lock(&trava);
    snprintf(tidAtual, sizeof tidAtual, "%s", tid);
    snprintf(hashAtual, sizeof hashAtual, "%s", h);
    pthread_mutex_unlock(&trava);
    if (strcmp(p->metaTid, tid)) {
      if (p->metaTid[0]) atomic_fetch_add(&descartados, 1u);   // META de outro torrent
      r = esperar(p, P2PM_EV_META, 0, tid, P2P_PRAZO_METADADOS, &ev, &espaco);
      if (r != 1) {
        if (r == 0) {
          P2pmStats st;
          memset(&st, 0, sizeof st);
          if (p->o->stats) p->o->stats(p->m, &st);
          printf("[p2p-motor] %.8s: metadados nao chegaram em %d s (%u pares)\n", h, P2P_PRAZO_METADADOS, st.pares);
        }
        goto falhou;
      }
    }
    pthread_mutex_lock(&trava);
    metaPronta = 1;
    pthread_mutex_unlock(&trava);
    // #486: "lento" sem log nao separa metadados (peers) de primeiros bytes.
    printf("[p2p-motor] %.8s: metadados em %.1f s\n", h, agora() - t0);
  }
  if (cancelado(p)) { r = -2; goto falhou; }
  // Arquivos. O teto vem ANTES de alocar: torrent com 100 mil arquivos nao
  // vira 100 mil mallocs; olhamos os primeiros P2PM_MAX_ARQ.
  if (p->o->qtd_arquivos(p->m, tid, &qtd) != 0 || qtd == 0) { soltarTorrent(p); return P2P_ERR_SEM_VIDEO; }
  if (qtd > P2PM_MAX_ARQ) {
    printf("[p2p-motor] %.8s: %zu arquivos, olhando os primeiros %d\n", h, qtd, P2PM_MAX_ARQ);
    qtd = P2PM_MAX_ARQ;
  }
  {
    char **nomes = calloc(qtd, sizeof *nomes);
    double *tams = calloc(qtd, sizeof *tams);
    char caminho[1024];
    if (!nomes || !tams) { free(nomes); free(tams); soltarTorrent(p); return P2P_ERR_MOTOR; }
    for (i = 0; i < qtd; i++) {
      uint64_t t = 0;
      if (p->o->arquivo(p->m, tid, i, caminho, sizeof caminho, &t) != 0) continue;
      nomes[i] = strdup(caminho);
      tams[i] = (double)t;
    }
    idx = p2p_escolher_lista((const char *const *)nomes, tams, (int)qtd, fileIdx, temporada, episodio);
    if (idx >= 0 && (size_t)idx < qtd) tamEscolhido = (uint64_t)tams[idx];
    for (i = 0; i < qtd; i++) free(nomes[i]);
    free(nomes); free(tams);
  }
  if (idx >= 0 && usbFatExcede(tamEscolhido)) {
    printf("[p2p-motor] %.8s: arquivo %d tem %.0f MB, FAT32/unknown backing accepts < 4096 MB: refused\n",
           h, idx, (double)tamEscolhido / 1048576.0);
    soltarTorrent(p);
    return P2P_ERR_SEM_ESPACO;
  }
  if (idx >= 0) {
    // #334: arquivo maior que o teto toca pela janela (o motor apaga o que ja
    // tocou). Nem a janela minima cabe: recusa AGORA, com os numeros para a
    // tela, em vez de tocar e parar no meio.
    uint64_t teto;
    pthread_mutex_lock(&trava);
    tamArquivo = tamEscolhido;
    teto = estado.disco_duro;
    pthread_mutex_unlock(&trava);
    if (teto && tamEscolhido > teto) {
      if (!p2pmotor_cabe(tamEscolhido, teto)) {
        printf("[p2p-motor] %.8s: arquivo %d tem %.0f MB, teto %.0f MB, janela %.0f MB < %d MB: recusado\n",
               h, idx, (double)tamEscolhido / 1048576.0, (double)teto / 1048576.0,
               (double)p2pmotor_janela(teto) / 1048576.0, P2PM_JANELA_MIN_MB);
        soltarTorrent(p);
        return P2P_ERR_SEM_ESPACO;
      }
      printf("[p2p-motor] %.8s: arquivo %d tem %.0f MB, teto %.0f MB: janela de %.0f MB\n", h, idx,
             (double)tamEscolhido / 1048576.0, (double)teto / 1048576.0,
             (double)p2pmotor_janela(teto) / 1048576.0);
    }
  }
  if (idx < 0) {
    printf("[p2p-motor] %.8s: sem arquivo de video\n", h);
    soltarTorrent(p);
    return P2P_ERR_SEM_VIDEO;
  }
  if (p->o->preparar(p->m, tid, (uint32_t)idx, &rid) != 0) { soltarTorrent(p); return P2P_ERR_RECUSOU; }
  r = esperar(p, P2PM_EV_PREPARADO, rid, tid, 10, &ev, &espaco);
  if (r != 1) goto falhou;
  if (!ev.url[0] || !urlLocal(ev.url) || strlen(ev.url) >= n) {
    printf("[p2p-motor] %.8s: url do stream recusada\n", h);
    if (ev.sid[0] && p->o->parar_stream) p->o->parar_stream(p->m, ev.sid);
    soltarTorrent(p);
    return P2P_ERR_RECUSOU;
  }
  pthread_mutex_lock(&trava);
  snprintf(sidAtual, sizeof sidAtual, "%s", ev.sid);
  pthread_mutex_unlock(&trava);
  // Os primeiros KB de VIDEO tem de chegar (a mesma prova do servidor
  // Stremio): metadados sem ninguem semeando prenderiam o player em
  // "carregando". A sonda e cancelada pela mesma geracao.
  r = sondaFn(ev.url, cancelado, p);
  if (cancelado(p)) { r = -2; goto falhou; }
  if (r != 1) {
    { P2pmStats st;
      memset(&st, 0, sizeof st);
      if (p->o->stats) p->o->stats(p->m, &st);
      printf("[p2p-motor] %.8s: arquivo %d sem bytes (%u pares)\n", h, idx, st.pares); }
    soltarTorrent(p);
    return r == 0 ? P2P_ERR_SEM_PEERS : P2P_ERR_RECUSOU;
  }
  // Publica so se ainda e a mesma geracao, sob a trava: um cancelamento que
  // chegou durante a sonda nunca vira URL.
  pthread_mutex_lock(&trava);
  if (atomic_load(&geracao) != p->g) {
    pthread_mutex_unlock(&trava);
    r = -2;
    goto falhou;
  }
  snprintf(url, n, "%s", ev.url);
  {
    const char *b = strchr(ev.url + 7, '/');
    size_t L = b ? (size_t)(b - ev.url + 1) : strlen(ev.url);
    if (L < sizeof prefixoUrl) { memcpy(prefixoUrl, ev.url, L); prefixoUrl[L] = 0; }
  }
  pthread_mutex_unlock(&trava);
  printf("[p2p-motor] %.8s: arquivo %d pronto\n", h, idx);
  return P2P_OK;
falhou:
  soltarTorrent(p);
  if (r == -2) return P2P_ERR_CANCELADO;
  if (r == -1) return espaco ? P2P_ERR_SEM_ESPACO : P2P_ERR_RECUSOU;
  return P2P_ERR_SEM_PEERS;
}

int p2pmotor_resolver(const char *hash, int fileIdx, const char *fontes,
                      int temporada, int episodio, char *url, unsigned n) {
  const P2pmOps *o = ops();
  unsigned g = atomic_load(&geracao);
  Pedido p;
  char h[41];
  double t0 = agora(), fimEspera;
  int e, i;
  if (url && n) url[0] = 0;
  if (!url || n < 32 || !o) return P2P_ERR_DESLIGADO;
  if (!p2p_hash_valido(hash)) return P2P_ERR_HASH;
  for (i = 0; i < 40; i++) h[i] = (char)tolower((unsigned char)hash[i]);
  h[40] = 0;
  pthread_mutex_lock(&trava);
  if (resolvendo) { pthread_mutex_unlock(&trava); return P2P_ERR_OCUPADO; }
  resolvendo = 1;
  // Uma parada em curso (destroy de segundos) ou a vigia lendo eventos: espera
  // no sinal (a trava fica solta), acordando com cancelamento.
  fimEspera = agora() + 15;
  while ((parando || vigiaEventos || limpando) && atomic_load(&geracao) == g && agora() < fimEspera)
    esperarSinal(200);
  if (atomic_load(&geracao) != g) e = P2P_ERR_CANCELADO;
  else if (parando || vigiaEventos || limpando) e = P2P_ERR_OCUPADO;
  else e = P2P_OK;
  if (e == P2P_OK && !motor) {
    pthread_mutex_unlock(&trava);
    e = subir(o, g);
    pthread_mutex_lock(&trava);
    // #486: subida (inclui apagar sobra de cache) e espera de parada anterior.
    printf("[p2p-motor] %.8s: motor de pe em %.1f s (erro %d)\n", h, agora() - t0, e);
  }
  if (e != P2P_OK) {
    resolvendo = 0;
    pthread_cond_broadcast(&sinal);
    pthread_mutex_unlock(&trava);
    return e;
  }
  memset(&p, 0, sizeof p);
  p.o = o; p.m = motor; p.g = g;
  usuarios++;
  pthread_mutex_unlock(&trava);

  e = resolverCom(&p, h, fileIdx, fontes, temporada, episodio, url, n);

  pthread_mutex_lock(&trava);
  usuarios--;
  resolvendo = 0;
  pthread_cond_broadcast(&sinal);
  pthread_mutex_unlock(&trava);
  if (e != P2P_OK) url[0] = 0;
  printf("[p2p-motor] %.8s: %s em %.1f s (erro %d)\n", h, e == P2P_OK ? "pronto" : "falhou",
         agora() - t0, e);
  return e;
}

// ------------------------------------------------------------------ sonda real
// GET com Range dos primeiros 64 KB pelo N01 (rede_pedir): prazo unico,
// descarte em streaming (nada do video e alocado) e cancelamento por `parar`.
static int sondaReal(const char *url, int (*parar)(void *), void *u) {
  static const char *const cab[] = { "Range: bytes=0-65535", NULL };
  RedePedido q;
  RedeResposta r;
  int ok;
  memset(&q, 0, sizeof q);
  memset(&r, 0, sizeof r);
  q.url = url;
  q.cabecalhos = cab;
  q.prazo_ms = P2P_PRAZO_BYTES * 1000u;
  q.janela_corpo_ms = P2P_PRAZO_BYTES * 1000u;
  q.max_descartado = 65536;
  q.parar = parar;
  q.parar_usuario = u;
  rede_pedir(&q, &r);
  if (r.erro == REDE_OK && r.status >= 200 && r.status < 300 && (r.n_prefixo > 0 || r.bytes_fio > 0))
    ok = 1;
  else if (r.erro == REDE_OK && r.status >= 400) ok = -1;
  else ok = 0;
  rede_resposta_limpar(&r);
  return ok;
}
