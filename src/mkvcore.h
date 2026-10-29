#pragma once
// Rust core of src/mkv.c (rust/mkvcore), compiled only with -DNV_RUST_MKVCORE.
// Off by default; docs/plans/rust-mapa.md, phase 1. No state lives on the Rust
// side: everything comes in and goes out through these arguments.
#include "mkv.h"

// Layouts mirrored in rust/mkvcore/src/lib.rs (const asserts there).
_Static_assert(sizeof(MkvFaixa) == (sizeof(long) == 8 ? 176 : 168), "MkvFaixa layout");
_Static_assert(sizeof(MkvCap) == 72, "MkvCap layout");

// What the C acharTracks left in file-scope statics.
typedef struct {
  int tracks_cortado;
  int tk_achado;
  int n_caps;
  int tracks_nao_visto;    // SeekHead said where Tracks is and it wasn't reached
  long long tk_tam;
  long long tk_disp;      // bytes left after the Tracks header (valid when cortado)
  long long seg_ini;
  long long pos_chapters; // -1 = the SeekHead did not say
  long long pos_tracks;   // idem, for Tracks (#483)
} NvMkvScan;

// Cluster scan result (the MkvDiag fields it fills).
typedef struct {
  int rpu, quadros, rpu_tipo, rpu_perfil;
  long long bloco_ini, bloco_fim;
} NvDiagOut;

_Static_assert(sizeof(NvMkvScan) == 56, "NvMkvScan layout");
_Static_assert(sizeof(NvDiagOut) == 32, "NvDiagOut layout");

int nv_mkvcore_contentor(const unsigned char *p, long n);
int nv_mkvcore_quadro_tem_rpu(const unsigned char *p, long n, int nalTam,
                              int *rpuTipo, int *rpuPerfil);
int nv_mkvcore_achar_tracks(const unsigned char *p, long n, MkvFaixa *saida, int max,
                            MkvCap *caps, int maxCaps, NvMkvScan *sc);
// Data size of the element at p when it is `quer` (-1: not it / unknown);
// *hdr = header length. Generic sibling of nv_mkvcore_cap_cabecalho.
long long nv_mkvcore_elem_cabecalho(const unsigned char *p, long n,
                                    unsigned long quer, int *hdr);
// `lerTracks`: TrackEntry loop on the data of a Tracks element.
int nv_mkvcore_ler_tracks(const unsigned char *p, long n, MkvFaixa *saida, int max);
void nv_mkvcore_diag_trecho(const unsigned char *p, long n, int faixa, int nalTam,
                            NvDiagOut *out);
// Data size of the Chapters element at p (-1: not one / unknown size); *hdr = header length.
long long nv_mkvcore_cap_cabecalho(const unsigned char *p, long n, int *hdr);
int nv_mkvcore_capitulos(const unsigned char *p, long n, MkvCap *caps, int max);
