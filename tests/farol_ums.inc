static double numeroDe(const char *p, const char *chave) {
  const char *q = p;
  size_t n = strlen(chave);
  while ((q = strstr(q, chave)) != NULL) {
    const char *v = q + n;
    while (*v == ' ') v++;
    if ((*v >= '0' && *v <= '9') || *v == '-' || *v == '.') return atof(v);
    q += n;
  }
  return -1.0;
}
static void farolUmsErro(const char *p, double posicaoS, char *erroTxt, size_t nErro) {
  double cod = numeroDe(p, "\"errorCode\":");
  const char *t = strstr(p, "errorText\":\"");
  char txt[72] = "";
  FarolErro fe;
  if (t) {
    const char *f = strchr(t + 12, '"');
    if (f && f - (t + 12) < (int)sizeof txt) {
      memcpy(txt, t + 12, (size_t)(f - (t + 12)));
      txt[f - (t + 12)] = 0;
    }
  }
  snprintf(erroTxt, nErro, "%.0f %s", cod >= 0 ? cod : 0.0, txt);
  farolerro_ler(FP_WEBOS, erroTxt, (int)(cod >= 0 ? cod : 0.0), &fe);
  // Ao anel so vai cod da lista fechada (farolcod_anel): um token montado de
  // texto livre nao passa daqui.
  { char cs[24];
    const char *tok = farolcod_anel(&fe, FP_WEBOS, cs, sizeof cs);
    if (fe.classe == FC_AUDIO)
      farol_ev(FM_AUDIO, FE_AUDIO_RECUSADO, 2, fe.nativo, 0, tok);
    else if (!strcmp(fe.cod, "ums_pipeline"))
      farol_ev(FM_VIDEO, FE_PIPELINE_MORREU, 2, (int32_t)posicaoS, 0, tok);
    else
      farol_ev(FM_VIDEO, FE_ERRO_NATIVO, 2, fe.nativo, 0, tok); }
}
static void farolTvModeloLer(const char *json, char *out, size_t nOut) {
  const char *k = json ? strstr(json, "\"tv.model.modelname\"") : NULL;
  char v[64] = "";
  size_t i, n = 0;
  if (!out || !nOut) return;
  out[0] = 0;
  if (k) sscanf(k + 20, " : \"%63[^\"]", v);
  if (nOut > 33) nOut = 33;
  for (i = 0; v[i] && n + 1 < nOut; i++) {
    unsigned char c = (unsigned char)v[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_' || c == ' ')
      out[n++] = (char)c;
  }
  out[n] = 0;
  while (n && out[n - 1] == ' ') out[--n] = 0;
}
static char tvModelo[33];
static pthread_mutex_t tvModeloTrava = PTHREAD_MUTEX_INITIALIZER;
const char *video_tv_modelo(void) {
  static _Thread_local char copia[33];
  pthread_mutex_lock(&tvModeloTrava);
  memcpy(copia, tvModelo, sizeof copia);
  pthread_mutex_unlock(&tvModeloTrava);
  return copia[0] ? copia : "?";
}
static int edidResposta(LSHandle *h, LSMessage *m, void *u) {
  const char *r;
  (void)h;
  (void)u;
  r = lsPayload(m);
  __atomic_store_n(&edidDts, nv_dts_edid(r) ? 1 : 0, __ATOMIC_RELEASE);
  pthread_mutex_lock(&tvModeloTrava);
  farolTvModeloLer(r, tvModelo, sizeof tvModelo);
  pthread_mutex_unlock(&tvModeloTrava);
  printf("[dts] %.160s\n", r ? r : "");
  fflush(stdout);
  return 1;
}
