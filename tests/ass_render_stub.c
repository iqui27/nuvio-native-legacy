// Isolated libass rendering fixtures do not collect GPU metrics or set bold overrides.
int perftv_enabled, perftv_uploads;
int plainass_arabic_bold;
// The new engine's "[ass] 10s tv" line reads the platform's clock counters.
void video_ass_leituras(int *n, int *fora, long *somaMs, int *maxMs) { *n = *fora = *maxMs = 0; *somaMs = 0; }
