//! EBML / HEVC-NAL core of `src/mkv.c` (docs/plans/rust-mapa.md, phase 1).
//!
//! `no_std`, no dependencies, no global state: every function gets its input
//! and its output by argument. Network and diagnostics stay in C (`src/mkv.c`).
//!
//! Rules this file keeps:
//!  * no panic crosses FFI: `panic = "abort"` plus a panic handler that calls C
//!    `abort()`, and no indexing/slicing/unwrap (checked access only — the
//!    Cargo.toml lints make clippy fail on a violation);
//!  * offsets are `i64` (the C code uses `long`, 32 bits on the ARM TVs);
//!  * recursion is depth-limited (the C code was not);
//!  * the output structs mirror `MkvFaixa`/`MkvCap` and are written with
//!    `ptr::write`, never through a reference to caller memory.
#![no_std]

use core::ffi::{c_char, c_int, c_long, c_ulong};
use core::ptr;
use core::slice;

#[cfg(not(test))]
extern "C" {
    fn abort() -> !;
}

#[cfg(not(test))]
#[panic_handler]
fn on_panic(_: &core::panic::PanicInfo) -> ! {
    // SAFETY: C abort(), never returns.
    unsafe { abort() }
}

/// Link-only personality symbol for compiler_builtins' ARM32 EHABI and
/// aarch64 Android unwind tables. With panic=abort it is never called.
#[cfg(all(not(test), not(feature = "external-personality")))]
#[no_mangle]
pub extern "C" fn rust_eh_personality() {}

// --- C ABI ----------------------------------------------------------------------

/// `MkvFaixa` of src/mkv.h, field for field.
#[repr(C)]
pub struct MkvFaixa {
    numero: c_int,
    tipo: c_int,
    idioma: [c_char; 8],
    nome: [c_char; 48],
    codec: [c_char; 24],
    forcado: c_int,
    sdh: c_int,
    canais: c_int,
    dv_perfil: c_int,
    dv_nivel: c_int,
    dv_rpu: c_int,
    dv_el: c_int,
    dv_bl: c_int,
    dv_compat: c_int,
    cp_n: c_long,
    hvcc_nal62: c_int,
    bam_n: c_int,
    nal_tam: c_int,
    entrada_inteira: c_int,
    bam_tipo: c_ulong,
}

/// `MkvCap` of src/mkv.h.
#[repr(C)]
pub struct MkvCap {
    inicio: f64,
    nome: [c_char; 64],
}

/// What `acharTracks` used to leave in file-scope statics (src/mkv.c keeps them).
#[repr(C)]
pub struct NvMkvScan {
    tracks_cortado: c_int,
    tk_achado: c_int,
    n_caps: c_int,
    tracks_nao_visto: c_int,
    tk_tam: i64,
    tk_disp: i64,
    seg_ini: i64,
    pos_chapters: i64,
    pos_tracks: i64,
}

/// The fields of `MkvDiag` that come from scanning the first Cluster.
#[repr(C)]
pub struct NvDiagOut {
    rpu: c_int,
    quadros: c_int,
    rpu_tipo: c_int,
    rpu_perfil: c_int,
    bloco_ini: i64,
    bloco_fim: i64,
}

// Layout contract with src/mkvcore.h (which has the same numbers as
// _Static_assert): LP64 vs ILP32 `long`.
const LONG8: bool = core::mem::size_of::<c_long>() == 8;
const _: () = assert!(core::mem::size_of::<MkvFaixa>() == if LONG8 { 160 } else { 148 });
const _: () = assert!(core::mem::size_of::<MkvCap>() == 72);
const _: () = assert!(core::mem::size_of::<NvMkvScan>() == 56);
const _: () = assert!(core::mem::size_of::<NvDiagOut>() == 32);

impl MkvFaixa {
    const fn zero() -> Self {
        MkvFaixa {
            numero: 0,
            tipo: 0,
            idioma: [0; 8],
            nome: [0; 48],
            codec: [0; 24],
            forcado: 0,
            sdh: 0,
            canais: 0,
            dv_perfil: 0,
            dv_nivel: 0,
            dv_rpu: 0,
            dv_el: 0,
            dv_bl: 0,
            dv_compat: 0,
            cp_n: 0,
            hvcc_nal62: 0,
            bam_n: 0,
            nal_tam: 0,
            entrada_inteira: 0,
            bam_tipo: 0,
        }
    }
}

// --- checked access ---------------------------------------------------------------

/// Deepest nesting followed (Edition in Edition, BlockGroup in BlockGroup). Real
/// files use 1; the C code recursed without a limit.
const PROFUNDIDADE_MAX: u32 = 8;

fn idx(i: i64) -> Option<usize> {
    usize::try_from(i).ok()
}

fn at(p: &[u8], i: i64) -> u8 {
    idx(i).and_then(|k| p.get(k)).copied().unwrap_or(0)
}

/// `p[off .. off+len]`, or empty when it does not fit.
fn sub(p: &[u8], off: i64, len: i64) -> &[u8] {
    let r = (|| {
        let s = idx(off)?;
        let e = idx(off.checked_add(len)?)?;
        p.get(s..e)
    })();
    r.unwrap_or(&[])
}

fn tail(p: &[u8], off: i64) -> &[u8] {
    idx(off).and_then(|s| p.get(s..)).unwrap_or(&[])
}

/// The caller's `(ptr, n)` as a slice; empty for NULL or a negative length.
///
/// # Safety
/// For positive `n`, `p` must denote one readable allocation of `n` bytes,
/// remain valid and immutable for `'a`, and its address plus `n` must not wrap.
unsafe fn buf<'a>(p: *const u8, n: c_long) -> &'a [u8] {
    if p.is_null() || n <= 0 {
        return &[];
    }
    let n = n as usize;
    if n > isize::MAX as usize || (p as usize).checked_add(n).is_none() {
        return &[];
    }
    // SAFETY: length and address arithmetic checked; the caller owns one
    // readable allocation at `p`, immutable for the borrow.
    unsafe { slice::from_raw_parts(p, n) }
}

// --- EBML ------------------------------------------------------------------------

fn largura_de(b: u8) -> i64 {
    let mut i = 0;
    while i < 8 {
        if b & (0x80u8 >> i) != 0 {
            return i64::from(i) + 1;
        }
        i += 1;
    }
    0
}

/// ID with the marker bit kept. `(id, bytes used)`; id 0 = end or invalid.
fn ler_id(p: &[u8]) -> (u32, i64) {
    let w = largura_de(at(p, 0));
    if !(1..=4).contains(&w) || (p.len() as i64) < w {
        return (0, 0);
    }
    let mut v: u32 = 0;
    let mut i = 0;
    while i < w {
        v = (v << 8) | u32::from(at(p, i));
        i += 1;
    }
    (v, w)
}

const TAM_INVALIDO: i64 = -1;
const TAM_DESCONHECIDO: i64 = -2;

/// Size with the marker bit removed. `(size, bytes used)`; -1 invalid, -2 unknown.
fn ler_tam(p: &[u8]) -> (i64, i64) {
    if p.is_empty() {
        return (TAM_INVALIDO, 0);
    }
    let b0 = at(p, 0);
    let w = largura_de(b0);
    if !(1..=8).contains(&w) || (p.len() as i64) < w {
        return (TAM_INVALIDO, 0);
    }
    // u32: with w == 8 a u8 shift would overflow (the C side does it as int -> 0).
    let mascara = (0xFFu32 >> w) as u8;
    let mut todos_um = (b0 & mascara) == mascara;
    let mut v = u64::from(b0 & mascara);
    let mut i = 1;
    while i < w {
        let b = at(p, i);
        if b != 0xFF {
            todos_um = false;
        }
        v = (v << 8) | u64::from(b);
        i += 1;
    }
    if todos_um {
        return (TAM_DESCONHECIDO, w);
    }
    // At most 56 bits of data (w <= 8, marker removed): fits.
    (v as i64, w)
}

/// Element header at `o`: `(id, offset of the data, size)`. None for an invalid
/// id, an invalid size or the unknown size (callers that care about the last
/// one use `cabeca_bruta`).
fn cabeca(p: &[u8], o: i64) -> Option<(u32, i64, i64)> {
    let (id, d, tam, _) = cabeca_bruta(p, o)?;
    if tam < 0 {
        return None;
    }
    Some((id, d, tam))
}

/// Like `cabeca`, keeps -1/-2 sizes: `(id, data offset, size, header length)`.
fn cabeca_bruta(p: &[u8], o: i64) -> Option<(u32, i64, i64, i64)> {
    let r = tail(p, o);
    let (id, ui) = ler_id(r);
    if id == 0 {
        return None;
    }
    let (tam, ut) = ler_tam(tail(r, ui));
    Some((id, o + ui + ut, tam, ui + ut))
}

/// Big-endian unsigned of 1..=8 bytes; anything else is 0. 64 bits on purpose.
fn ler_uint(v: &[u8]) -> u64 {
    if v.is_empty() || v.len() > 8 {
        return 0;
    }
    v.iter().fold(0u64, |a, &b| (a << 8) | u64::from(b))
}

/// Copies like the C `lerTexto`: up to `dst.len()-1` bytes, then a NUL right
/// after them. Bytes past that keep whatever they held.
fn ler_texto(v: &[u8], dst: &mut [c_char]) {
    let cap = dst.len().saturating_sub(1);
    let k = v.len().min(cap);
    for (d, s) in dst.iter_mut().zip(v.iter().take(k)) {
        *d = *s as c_char;
    }
    if let Some(z) = dst.get_mut(k) {
        *z = 0;
    }
}

// --- ids --------------------------------------------------------------------------

const ID_SEGMENT: u32 = 0x1853_8067;
const ID_TRACKS: u32 = 0x1654_AE6B;
const ID_TRACKENTRY: u32 = 0xAE;
const ID_TRACKNUMBER: u32 = 0xD7;
const ID_TRACKTYPE: u32 = 0x83;
const ID_LANGUAGE: u32 = 0x22_B59C;
const ID_LANG_BCP47: u32 = 0x22_B59D;
const ID_NAME: u32 = 0x536E;
const ID_CODECID: u32 = 0x86;
const ID_FLAGFORCED: u32 = 0x55AA;
const ID_FLAGHEARING: u32 = 0x55AB;
const ID_AUDIO: u32 = 0xE1;
const ID_CHANNELS: u32 = 0x9F;
const ID_BLOCKADDMAP: u32 = 0x41E4;
const ID_BLOCKADDTYPE: u32 = 0x41E7;
const ID_BLOCKADDDATA: u32 = 0x41ED;
const ID_CODECPRIV: u32 = 0x63A2;
const ID_CHAPTERS: u32 = 0x1043_A770;
const ID_EDITION: u32 = 0x45B9;
const ID_CHAPATOM: u32 = 0xB6;
const ID_CHAPSTART: u32 = 0x91;
const ID_CHAPDISPLAY: u32 = 0x80;
const ID_CHAPSTRING: u32 = 0x85;
const ID_SEEKHEAD: u32 = 0x114D_9B74;
const ID_SEEK: u32 = 0x4DBB;
const ID_SEEKID: u32 = 0x53AB;
const ID_SEEKPOS: u32 = 0x53AC;
const ID_CLUSTER: u32 = 0x1F43_B675;
const ID_SIMPLEBLOCK: u32 = 0xA3;
const ID_BLOCKGROUP: u32 = 0xA0;
const ID_BLOCK: u32 = 0xA1;

/// 'dvcC' / 'dvvC' as BlockAddIDType.
const BAM_DVCC: c_ulong = 0x6476_6343;
const BAM_DVVC: c_ulong = 0x6476_7643;

// --- Tracks -----------------------------------------------------------------------

/// hvcC: lengthSizeMinusOne at byte 21, numOfArrays at 22, then arrays
/// `{type&0x3F, u16 count, {u16 len, NAL}...}`. Flags an array of NAL type 62.
fn ler_hvcc(v: &[u8], f: &mut MkvFaixa) {
    let n = v.len() as i64;
    if n < 23 || at(v, 0) != 1 {
        return;
    }
    f.nal_tam = c_int::from(at(v, 21) & 3) + 1;
    let n_arrays = i64::from(at(v, 22));
    let mut o: i64 = 23;
    let mut a = 0;
    while a < n_arrays && o + 3 <= n {
        let tipo = at(v, o) & 0x3F;
        let n_nal = (i64::from(at(v, o + 1)) << 8) | i64::from(at(v, o + 2));
        o += 3;
        if tipo == 62 {
            f.hvcc_nal62 = 1;
        }
        let mut k = 0;
        while k < n_nal && o + 2 <= n {
            o += 2 + ((i64::from(at(v, o)) << 8) | i64::from(at(v, o + 1)));
            k += 1;
        }
        a += 1;
    }
}

/// Children of a master element, handed to `f` as `(id, data)`. Stops where the C
/// loops did: invalid id or size, or a child that does not fit. Returns the
/// offset reached (what the TrackEntry loop compares with its size).
fn filhos<'a, F: FnMut(u32, &'a [u8])>(p: &'a [u8], mut f: F) -> i64 {
    let n = p.len() as i64;
    let mut q: i64 = 0;
    while q < n {
        let Some((id, d, tam)) = cabeca(p, q) else { break };
        q = d;
        if q.saturating_add(tam) > n {
            break;
        }
        f(id, sub(p, q, tam));
        q += tam;
    }
    q
}

fn ler_entrada(e: &[u8]) -> MkvFaixa {
    let mut f = MkvFaixa::zero();
    let q = filhos(e, |fid, v| match fid {
        ID_TRACKNUMBER => f.numero = ler_uint(v) as u32 as c_int,
        ID_TRACKTYPE => f.tipo = ler_uint(v) as u32 as c_int,
        ID_LANGUAGE | ID_LANG_BCP47 => {
            // BCP47 beats ISO 639-2 when both exist.
            if fid == ID_LANG_BCP47 || f.idioma.first() == Some(&0) {
                ler_texto(v, &mut f.idioma);
            }
        }
        ID_NAME => ler_texto(v, &mut f.nome),
        ID_CODECID => ler_texto(v, &mut f.codec),
        ID_CODECPRIV => {
            f.cp_n = v.len() as c_long;
            ler_hvcc(v, &mut f);
        }
        ID_FLAGFORCED => f.forcado = c_int::from(ler_uint(v) != 0),
        ID_FLAGHEARING => f.sdh = c_int::from(ler_uint(v) != 0),
        ID_AUDIO => {
            filhos(v, |aid, av| {
                if aid == ID_CHANNELS {
                    f.canais = ler_uint(av) as u32 as c_int;
                }
            });
        }
        ID_BLOCKADDMAP => {
            let mut tipo: c_ulong = 0;
            let mut dado: &[u8] = &[];
            filhos(v, |aid, av| {
                if aid == ID_BLOCKADDTYPE {
                    tipo = ler_uint(av) as c_ulong;
                } else if aid == ID_BLOCKADDDATA {
                    dado = av;
                }
            });
            f.bam_n = f.bam_n.wrapping_add(1);
            f.bam_tipo = tipo;
            if (tipo == BAM_DVCC || tipo == BAM_DVVC) && dado.len() >= 5 {
                let (b2, b3, b4) = (at(dado, 2), at(dado, 3), at(dado, 4));
                f.dv_perfil = c_int::from(b2 >> 1);
                f.dv_nivel = (c_int::from(b2 & 1) << 5) | c_int::from(b3 >> 3);
                f.dv_rpu = c_int::from((b3 >> 2) & 1);
                f.dv_el = c_int::from((b3 >> 1) & 1);
                f.dv_bl = c_int::from(b3 & 1);
                f.dv_compat = c_int::from(b4 >> 4);
            }
        }
        _ => {}
    });
    f.entrada_inteira = c_int::from(q == e.len() as i64);
    f
}

/// TrackEntry loop. Writes up to `max` faixas (numero > 0) to `saida`.
fn ler_tracks(p: &[u8], saida: *mut MkvFaixa, max: c_int) -> c_int {
    let n = p.len() as i64;
    let mut o: i64 = 0;
    let mut achou: c_int = 0;
    if saida.is_null() {
        return 0;
    }
    while o < n && achou < max {
        let Some((id, d, tam)) = cabeca(p, o) else { break };
        o = d;
        if o.saturating_add(tam) > n {
            break;
        }
        if id == ID_TRACKENTRY {
            let f = ler_entrada(sub(p, o, tam));
            if f.numero > 0 {
                // SAFETY: achou < max and the caller sized `saida` for `max`.
                unsafe { ptr::write(saida.add(achou as usize), f) };
                achou += 1;
            }
        }
        o += tam;
    }
    achou
}

// --- Chapters ---------------------------------------------------------------------

fn ler_capitulos(p: &[u8], saida: *mut MkvCap, max: c_int, prof: u32) -> c_int {
    let n = p.len() as i64;
    let mut o: i64 = 0;
    let mut achou: c_int = 0;
    if saida.is_null() {
        return 0;
    }
    while o < n && achou < max {
        let Some((id, d, tam)) = cabeca(p, o) else { break };
        o = d;
        if o.saturating_add(tam) > n {
            break;
        }
        if id == ID_EDITION {
            // Only the FIRST edition counts: several would scramble the order.
            if achou == 0 && prof < PROFUNDIDADE_MAX {
                achou += ler_capitulos(sub(p, o, tam), saida, max, prof + 1);
            }
        } else if id == ID_CHAPATOM {
            let mut c = MkvCap { inicio: 0.0, nome: [0; 64] };
            let mut tem_inicio = false;
            filhos(sub(p, o, tam), |fid, v| {
                if fid == ID_CHAPSTART {
                    // Absolute NANOSECONDS, not TimecodeScale units.
                    c.inicio = ler_uint(v) as f64 / 1_000_000_000.0;
                    tem_inicio = true;
                } else if fid == ID_CHAPDISPLAY {
                    filhos(v, |did, dv| {
                        if did == ID_CHAPSTRING {
                            ler_texto(dv, &mut c.nome);
                        }
                    });
                }
            });
            if tem_inicio {
                // SAFETY: achou < max and the caller sized `saida` for `max`.
                unsafe { ptr::write(saida.add(achou as usize), c) };
                achou += 1;
            }
        }
        o += tam;
    }
    achou
}

// --- SeekHead / walk to Tracks ----------------------------------------------------

fn ler_seek_head(p: &[u8], seg_ini: i64, pos_chapters: &mut i64, pos_tracks: &mut i64) {
    filhos(p, |id, v| {
        if id != ID_SEEK {
            return;
        }
        let mut alvo: u64 = 0;
        let mut pos: i64 = -1;
        filhos(v, |fid, fv| {
            if fid == ID_SEEKID {
                alvo = ler_uint(fv);
            } else if fid == ID_SEEKPOS {
                pos = ler_uint(fv) as i64;
            }
        });
        if pos < 0 {
            return;
        }
        // Absolute position; an overflowing one is "not said".
        if alvo == u64::from(ID_CHAPTERS) {
            *pos_chapters = seg_ini.checked_add(pos).unwrap_or(-1);
        } else if alvo == u64::from(ID_TRACKS) {
            *pos_tracks = seg_ini.checked_add(pos).unwrap_or(-1);
        }
    });
}

fn achar_tracks(
    p: &[u8],
    saida: *mut MkvFaixa,
    max: c_int,
    caps: *mut MkvCap,
    max_caps: c_int,
    sc: &mut NvMkvScan,
) -> c_int {
    let n = p.len() as i64;
    let mut o: i64 = 0;
    let mut n_faixas: c_int = 0;
    while o < n {
        let Some((id, d, tam, hdr)) = cabeca_bruta(p, o) else { break };
        if tam == TAM_INVALIDO {
            break;
        }
        o = d;
        if id == ID_SEGMENT || tam == TAM_DESCONHECIDO {
            // Segment: descend. Unknown size elsewhere: nowhere to skip to.
            if id == ID_SEGMENT {
                sc.seg_ini = o;
                continue;
            }
            break;
        }
        if id == ID_SEEKHEAD && o.saturating_add(tam) <= n {
            ler_seek_head(sub(p, o, tam), sc.seg_ini, &mut sc.pos_chapters, &mut sc.pos_tracks);
        }
        if id == ID_TRACKS {
            let disp = n - o;
            let t = tam.min(disp);
            sc.tk_achado = 1;
            sc.tk_tam = tam;
            n_faixas = ler_tracks(sub(p, o, t), saida, max);
            if tam > disp {
                sc.tracks_cortado = 1;
                sc.tk_disp = disp;
            }
            // Chapters come AFTER Tracks: do not return on the first hit.
            if caps.is_null() || tam > disp {
                break;
            }
            o += tam;
            continue;
        }
        if id == ID_CHAPTERS && !caps.is_null() && max_caps > 0 {
            let disp = n - o;
            // Cut by the window: a half list gets "last chapter" wrong. Left to
            // the SeekHead read.
            if tam > disp {
                sc.pos_chapters = o - hdr;
                break;
            }
            sc.n_caps = ler_capitulos(sub(p, o, tam), caps, max_caps, 0);
            sc.pos_chapters = -1;
            break;
        }
        if o.saturating_add(tam) > n {
            break;
        }
        o += tam;
    }
    // #483: Tracks past the window (remux edited by mkvpropedit): the walk
    // stopped at a big element (Attachments, first Cluster) with n_faixas == 0,
    // or Tracks came truncated — and every faixa stayed "codec=?", which left
    // the embedded ASS to the TV (plain text, lines dropped). The SeekHead
    // knows where Tracks is; the C side fetches it in one Range.
    sc.tracks_nao_visto =
        c_int::from(sc.pos_tracks >= 0 && (n_faixas == 0 || sc.tracks_cortado != 0));
    n_faixas
}

// --- RPU in band ------------------------------------------------------------------

/// `mkv_quadro_tem_rpu`: 1 = NAL 62, 0 = none, -1 = malformed. The pair is the
/// `(rpu_type, vdr_rpu_profile)` written when a NAL 62 was met.
fn quadro_rpu(p: &[u8], nal_tam: i64) -> (c_int, Option<(c_int, c_int)>) {
    // diag_trecho also reaches this helper; match the C frame parser's guard.
    if !(1..=4).contains(&nal_tam) {
        return (-1, None);
    }
    let n = p.len() as i64;
    let mut o: i64 = 0;
    while o + nal_tam + 2 <= n {
        let mut len: i64 = 0;
        let mut i = 0;
        while i < nal_tam {
            len = (len << 8) | i64::from(at(p, o + i));
            i += 1;
        }
        o += nal_tam;
        if len < 2 || o.saturating_add(len) > n {
            return (-1, None);
        }
        if (at(p, o) >> 1) & 0x3F == 62 {
            // rbsp without the emulation-prevention bytes (00 00 03): prefix
            // 0x19, rpu_type u(6), rpu_format u(11), and for type 2 the
            // vdr_rpu_profile u(4).
            let mut b = [0u8; 8];
            let mut k = o + 2;
            let mut m = 0usize;
            let mut zeros = 0;
            while k < o + len && m < 8 {
                let c = at(p, k);
                if zeros >= 2 && c == 3 {
                    zeros = 0;
                    k += 1;
                    continue;
                }
                zeros = if c == 0 { zeros + 1 } else { 0 };
                if let Some(slot) = b.get_mut(m) {
                    *slot = c;
                }
                m += 1;
                k += 1;
            }
            let mut tipo = -1;
            let mut perfil = -1;
            if m >= 5 && b.first() == Some(&0x19) {
                let bits = (u32::from(at(&b, 1)) << 24)
                    | (u32::from(at(&b, 2)) << 16)
                    | (u32::from(at(&b, 3)) << 8)
                    | u32::from(at(&b, 4));
                tipo = (bits >> 26) as c_int;
                if tipo == 2 {
                    perfil = ((bits >> 11) & 0xF) as c_int;
                }
            }
            return (1, Some((tipo, perfil)));
        }
        o += len;
    }
    (if o == n { 0 } else { -1 }, None)
}

/// A Block/SimpleBlock header: track number (vint), 2 bytes of time, 1 of flags.
/// Offset of the frame data, or -1 (other track, lacing, too short).
fn cab_bloco(p: &[u8], faixa: i64) -> i64 {
    let (num, u) = ler_tam(p);
    if num != faixa || (p.len() as i64) < u + 3 {
        return -1;
    }
    if (at(p, u + 2) >> 1) & 3 != 0 {
        return -1; // lacing: video does not use it
    }
    u + 3
}

fn rpu_no_contentor(
    p: &[u8],
    base: i64,
    faixa: i64,
    nal_tam: i64,
    d: &mut NvDiagOut,
    prof: u32,
) -> bool {
    let n = p.len() as i64;
    let mut o: i64 = 0;
    while o < n {
        let Some((id, dd, tam)) = cabeca(p, o) else { return false };
        o = dd;
        let resta = n - o;
        if id == ID_BLOCKGROUP {
            if prof < PROFUNDIDADE_MAX
                && rpu_no_contentor(sub(p, o, tam.min(resta)), base + o, faixa, nal_tam, d, prof + 1)
            {
                return true;
            }
        } else if id == ID_SIMPLEBLOCK || id == ID_BLOCK {
            let disp = tam.min(resta);
            let h = cab_bloco(sub(p, o, disp), faixa);
            if h >= 0 {
                if tam > resta {
                    // Frame cut by the window.
                    d.bloco_ini = base + o + h;
                    d.bloco_fim = base + o + tam;
                    return true;
                }
                let (r, par) = quadro_rpu(sub(p, o + h, tam - h), nal_tam);
                if let Some((t, pf)) = par {
                    d.rpu_tipo = t;
                    d.rpu_perfil = pf;
                }
                if r >= 0 {
                    d.quadros = d.quadros.wrapping_add(1);
                }
                if r == 1 {
                    d.rpu = 1;
                    return true;
                }
                if r == 0 {
                    d.rpu = 0; // DV puts an RPU in EVERY frame
                    return true;
                }
            }
        }
        if tam > resta {
            return false;
        }
        o += tam;
    }
    false
}

fn diag_trecho(p: &[u8], faixa: i64, nal_tam: i64) -> NvDiagOut {
    let mut d = NvDiagOut { rpu: -1, quadros: 0, rpu_tipo: -1, rpu_perfil: -1, bloco_ini: -1, bloco_fim: -1 };
    let n = p.len() as i64;
    let mut o: i64 = 0;
    if faixa == 0 || nal_tam == 0 {
        return d;
    }
    while o < n {
        let Some((id, dd, tam, _)) = cabeca_bruta(p, o) else { return d };
        if tam == TAM_INVALIDO {
            return d;
        }
        o = dd;
        if id == ID_SEGMENT {
            continue;
        }
        if id == ID_CLUSTER {
            let resta = n - o;
            let t = if tam < 0 || tam > resta { resta } else { tam };
            if !rpu_no_contentor(sub(p, o, t), o, faixa, nal_tam, &mut d, 0)
                && d.quadros > 0
                && tam >= 0
                && tam <= resta
            {
                d.rpu = 0; // whole Cluster scanned, frames without RPU
            }
            return d;
        }
        if tam < 0 || tam > n - o {
            return d;
        }
        o += tam;
    }
    d
}

// --- container sniff --------------------------------------------------------------

fn caixa_iso(t: &[u8]) -> bool {
    const TIPOS: [&[u8; 4]; 8] = [b"ftyp", b"styp", b"moov", b"mdat", b"free", b"skip", b"wide", b"pdin"];
    TIPOS.iter().any(|x| t.get(..4) == Some(x.as_slice()))
}

/// Same numbers as MKV_CONT_* in src/mkv.h.
fn contentor(p: &[u8]) -> c_int {
    const NADA: c_int = 0;
    const MKV: c_int = 1;
    const MP4: c_int = 2;
    const TS: c_int = 3;
    const TEXTO: c_int = 4;
    const OUTRO: c_int = 5;
    let n = p.len() as i64;
    if n < 4 {
        return NADA;
    }
    if p.get(..4) == Some([0x1A, 0x45, 0xDF, 0xA3].as_slice()) {
        return MKV;
    }
    if n >= 8 && caixa_iso(tail(p, 4)) {
        return MP4;
    }
    if at(p, 0) == 0x47 && (n <= 188 || at(p, 188) == 0x47) {
        return TS;
    }
    if n >= 5 && at(p, 4) == 0x47 && (n <= 196 || at(p, 196) == 0x47) {
        return TS;
    }
    let mut i: i64 = 0;
    if n >= 3 && at(p, 0) == 0xEF && at(p, 1) == 0xBB && at(p, 2) == 0xBF {
        i = 3; // UTF-8 BOM
    }
    while i < n && i < 64 && matches!(at(p, i), b' ' | b'\t' | b'\r' | b'\n') {
        i += 1;
    }
    if i < n && matches!(at(p, i), b'<' | b'{' | b'[') {
        return TEXTO;
    }
    OUTRO
}

// --- exports ----------------------------------------------------------------------

/// `mkv_contentor`.
///
/// # Safety
/// For positive `n`, non-NULL `p` must denote one readable allocation of `n`
/// bytes, immutable for the call; its address plus `n` must not wrap.
#[no_mangle]
pub unsafe extern "C" fn nv_mkvcore_contentor(p: *const u8, n: c_long) -> c_int {
    if p.is_null() {
        return 0;
    }
    // SAFETY: forwarded contract.
    contentor(unsafe { buf(p, n) })
}

/// `mkv_quadro_tem_rpu`.
///
/// # Safety
/// For positive `n`, non-NULL `p` must denote one readable allocation of `n`
/// bytes, immutable for the call; its address plus `n` must not wrap.
/// `tipo`/`perfil` must be NULL or aligned writable ints, disjoint from `p`.
#[no_mangle]
pub unsafe extern "C" fn nv_mkvcore_quadro_tem_rpu(
    p: *const u8,
    n: c_long,
    nal_tam: c_int,
    tipo: *mut c_int,
    perfil: *mut c_int,
) -> c_int {
    if p.is_null() || !(1..=4).contains(&nal_tam) || n < 0 {
        return -1;
    }
    // SAFETY: forwarded contract.
    let (r, par) = quadro_rpu(unsafe { buf(p, n) }, i64::from(nal_tam));
    if let Some((t, pf)) = par {
        // SAFETY: NULL checked; the caller gave writable ints.
        unsafe {
            if !tipo.is_null() {
                ptr::write(tipo, t);
            }
            if !perfil.is_null() {
                ptr::write(perfil, pf);
            }
        }
    }
    r
}

/// The walk of `acharTracks`. State goes to `sc`; faixas to `saida` (up to
/// `max`), chapters to `caps` (up to `max_caps`, `caps` may be NULL).
///
/// # Safety
/// For positive `n`, non-NULL `p` must denote one readable allocation of `n`
/// bytes, immutable for the call; its address plus `n` must not wrap.
/// Non-NULL outputs must be aligned and writable for their stated positive
/// capacities (`sc`: one struct), fit within one allocation and `isize::MAX`
/// bytes, and be disjoint from the input and each other.
#[no_mangle]
pub unsafe extern "C" fn nv_mkvcore_achar_tracks(
    p: *const u8,
    n: c_long,
    saida: *mut MkvFaixa,
    max: c_int,
    caps: *mut MkvCap,
    max_caps: c_int,
    sc: *mut NvMkvScan,
) -> c_int {
    if sc.is_null() {
        return 0;
    }
    let mut s = NvMkvScan {
        tracks_cortado: 0,
        tk_achado: 0,
        n_caps: 0,
        tracks_nao_visto: 0,
        tk_tam: 0,
        tk_disp: 0,
        seg_ini: 0,
        pos_chapters: -1,
        pos_tracks: -1,
    };
    let r = if p.is_null() || saida.is_null() {
        0
    } else {
        // SAFETY: forwarded contract.
        achar_tracks(unsafe { buf(p, n) }, saida, max, caps, max_caps, &mut s)
    };
    // SAFETY: NULL checked above.
    unsafe { ptr::write(sc, s) };
    r
}

/// The Cluster scan of `diagTrecho`: `faixa` = first video track, `nal_tam` its
/// hvcC NAL length size. `out` receives the whole result.
///
/// # Safety
/// For positive `n`, non-NULL `p` must denote one readable allocation of `n`
/// bytes, immutable for the call; its address plus `n` must not wrap.
/// Non-NULL `out` must be aligned, writable for one struct and disjoint from `p`.
#[no_mangle]
pub unsafe extern "C" fn nv_mkvcore_diag_trecho(
    p: *const u8,
    n: c_long,
    faixa: c_int,
    nal_tam: c_int,
    out: *mut NvDiagOut,
) {
    if out.is_null() {
        return;
    }
    // SAFETY: forwarded contract.
    let d = diag_trecho(unsafe { buf(p, n) }, i64::from(faixa), i64::from(nal_tam));
    // SAFETY: NULL checked above.
    unsafe { ptr::write(out, d) };
}

/// Header of a Chapters element at the start of `p` (`capsPorPosicao`): its data
/// size, or -1 when `p` does not start with a Chapters element of known size.
/// `hdr` receives the header length.
///
/// # Safety
/// For positive `n`, non-NULL `p` must denote one readable allocation of `n`
/// bytes, immutable for the call; its address plus `n` must not wrap.
/// `hdr` must be NULL or an aligned writable int, disjoint from `p`.
#[no_mangle]
pub unsafe extern "C" fn nv_mkvcore_cap_cabecalho(p: *const u8, n: c_long, hdr: *mut c_int) -> i64 {
    // SAFETY: forwarded contract.
    let b = unsafe { buf(p, n) };
    let Some((id, d, tam)) = cabeca(b, 0) else { return -1 };
    if id != ID_CHAPTERS {
        return -1;
    }
    if !hdr.is_null() {
        // SAFETY: NULL checked.
        unsafe { ptr::write(hdr, d as c_int) };
    }
    tam
}

/// Data size of the element at `p` when it is `quer` (-1: not it / unknown
/// size); `hdr` receives the header length. Generic `nv_mkvcore_cap_cabecalho`.
///
/// # Safety
/// For positive `n`, non-NULL `p` must denote one readable allocation of `n`
/// bytes, immutable for the call; its address plus `n` must not wrap.
/// `hdr` must be NULL or an aligned writable int, disjoint from `p`.
#[no_mangle]
pub unsafe extern "C" fn nv_mkvcore_elem_cabecalho(
    p: *const u8,
    n: c_long,
    quer: c_ulong,
    hdr: *mut c_int,
) -> i64 {
    // SAFETY: forwarded contract.
    let b = unsafe { buf(p, n) };
    let Some((id, d, tam)) = cabeca(b, 0) else { return -1 };
    if u64::from(id) != quer as u64 {
        return -1;
    }
    if !hdr.is_null() {
        // SAFETY: NULL checked.
        unsafe { ptr::write(hdr, d as c_int) };
    }
    tam
}

/// `lerTracks` on the data of a Tracks element.
///
/// # Safety
/// For positive `n`, non-NULL `p` must denote one readable allocation of `n`
/// bytes, immutable for the call; its address plus `n` must not wrap.
/// Non-NULL `saida` must be aligned, writable for `max` `MkvFaixa` when
/// positive, and disjoint from `p`.
#[no_mangle]
pub unsafe extern "C" fn nv_mkvcore_ler_tracks(
    p: *const u8,
    n: c_long,
    saida: *mut MkvFaixa,
    max: c_int,
) -> c_int {
    // SAFETY: forwarded contract.
    ler_tracks(unsafe { buf(p, n) }, saida, max)
}

/// `lerCapitulos` on the data of a Chapters element.
///
/// # Safety
/// For positive `n`, non-NULL `p` must denote one readable allocation of `n`
/// bytes, immutable for the call; its address plus `n` must not wrap.
/// Non-NULL `caps` must be aligned, writable for `max` structs when positive,
/// fit within one allocation and `isize::MAX` bytes, and be disjoint from `p`.
#[no_mangle]
pub unsafe extern "C" fn nv_mkvcore_capitulos(
    p: *const u8,
    n: c_long,
    caps: *mut MkvCap,
    max: c_int,
) -> c_int {
    // SAFETY: forwarded contract.
    ler_capitulos(unsafe { buf(p, n) }, caps, max, 0)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn diag_should_return_error_when_nal_length_invalid() {
        // Cluster / SimpleBlock / track 1 / timestamp / flags / 5-byte NAL size / NAL.
        let p = [0x1f, 0x43, 0xb6, 0x75, 0x8d, 0xa3, 0x8b, 0x81, 0, 0, 0,
                 0, 0, 0, 0, 2, 2, 1];
        assert_eq!(diag_trecho(&p, 1, 5).rpu, -1);
    }

    #[test]
    fn quadro_should_return_error_when_nal_length_invalid() {
        for n in [i64::MAX, i64::MIN, -1, 0, 5] {
            assert_eq!(quadro_rpu(&[], n), (-1, None));
        }
    }

    #[test]
    fn buf_should_return_empty_when_address_wraps() {
        // SAFETY: the impossible address range must be rejected before access.
        assert!(unsafe { buf(usize::MAX as *const u8, 1) }.is_empty());
    }

    #[test]
    fn parse_should_return_error_when_input_truncated() {
        let header = [0x10, 0x43, 0xa7, 0x70, 0x40, 0x01];
        for n in 0..header.len() {
            let p = header.get(..n).unwrap_or(&[]);
            assert_eq!(cabeca(p, 0), None);
        }
    }

    #[test]
    fn quadro_should_return_error_when_input_truncated() {
        let frame = [0, 0, 0, 2, 2, 1];
        for n in 1..frame.len() {
            let p = frame.get(..n).unwrap_or(&[]);
            assert_eq!(quadro_rpu(p, 4), (-1, None));
        }
    }
}
