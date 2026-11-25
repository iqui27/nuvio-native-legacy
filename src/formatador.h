#ifndef NV_FORMATADOR_H
#define NV_FORMATADOR_H

#include <stddef.h>

// ICONES DOS FORMATADORES DE FONTE (folha de Fontes, "Texto das fontes: Do
// addon"; dono, 17/11/2026).
//
// Os formatadores (AIOStreams, Tamtaro, Core Builds, Torrentio, Comet...) poem
// emoji e simbolos no nome e na descricao de cada fonte. Antes so 10 viravam
// icone, e so no comeco de uma peca da descricao; o resto sumia em
// nv_limpar_texto. A lista e tools/icones-formatador.tsv (300 sequencias, de
// claude/formatter-icons-TODO.md), e a tabela compilada e formatador_icones.inc,
// gerada por tools/icones-formatador.py junto com os PNG.
//
// Dois estilos (Ajustes > Estilo dos icones):
//   Padrao       emoji -> Twemoji em cores (icones/formatador/tw_<seq>.png);
//                simbolo monocromatico (☆ ⯪ ⛊ ✦ ♬ ◈...) -> o glifo Noto, na cor do
//                texto (icones/formatador/sy_<cp>.png)
//   Minimalista  os dois -> Lucide na cor do texto (icones/aj_<nome>.png); as
//                estrelas de nota (★ ⯪ ☆), sem estrela cheia no Lucide, ficam com o
//                glifo Noto nos dois estilos
// Bandeiras (par de indicadores regionais, e 🏴 Inglaterra/Escocia/Gales) ->
// flagpack no Padrao (icones/formatador/fl_<CC>.png); no Minimalista, e sem o
// PNG, o codigo de idioma de sempre (nv_limpar_texto).
// Emoji que nao esta na lista: some, como antes (nv_limpar_texto).
//
// Funcoes PURAS (sem SDL), testadas em tests/formatador.c.

// Quantas sequencias a tabela tem.
int formatador_icones_n(void);

// O icone que comeca em `p`, ou -1. `*len` recebe quantos bytes ele ocupa,
// com o seletor de variacao (FE0E/FE0F), tom de pele e o resto de uma
// sequencia ZWJ que a tabela nao conhece ("🕵️‍♀️" cai no 🕵 e leva o "‍♀️").
int formatador_icone_em(const char *p, size_t *len);

// O PNG (sem ".png", relativo a icones/) do icone `idx` no estilo pedido.
// `*cor` = 1 quando o PNG tem cores proprias (desenhar sem tinta).
const char *formatador_icone_nome(int idx, int minimalista, int *cor);

// O emoji em UTF-8, para log e teste.
const char *formatador_icone_utf8(int idx);

// UMA LINHA CRUA -> PECAS. Acrescenta em `out` (a partir de out[*k]):
//   'T' texto '\0'   texto ja limpo (nv_limpar_texto + nv_aparar_separadores)
//   'I' d d d        icone: indice em 3 digitos decimais
//   'B' CC '\0'      bandeira: "GB", "BR", "GB-ENG"...
//   'S' ... 'M' ... 'E'   o que muda com o estilo: entre S e M o Padrao, entre M
//                    e E o Minimalista. Bandeira = S B M T"EN" E (o Minimalista
//                    nao desenha bandeira: escreve o codigo de idioma). Lista de
//                    idiomas em texto depois de um icone de legenda/audio
//                    (💬 📝 🔤 🦻 🗣 🌐 🌎 🌍 🎙 👂 ⚐ ⚑ ☰) = S bandeiras M texto E.
// Quebras de linha dentro de `in` viram espaco. `bandeira_existe` (pode ser
// NULL = todas existem) diz se ha PNG para o codigo; sem ele a bandeira volta
// ao texto e vira o codigo de idioma. Devolve o numero de pecas escritas; para
// quando `out` enche (nunca corta uma peca no meio).
int formatador_pecas(const char *in, size_t n, char *out, size_t cap, size_t *k,
                     int (*bandeira_existe)(const char *cc));

#endif
