"""Exercise the production cache key with synthetic editions."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
text = (root / 'src/miniatura_android.c').read_text()
start = text.index('static void chaveArquivo(')
end = text.index('\n}', start) + 2
source = r'''
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
typedef struct { const char *imdb; int t,e; long durMs; const char *url,*cabecalhos; unsigned long long tamanho; const char *videoHash,*arquivo; } SeekrFonte;
static int dados_caminho(char *out, size_t n, const char *x) { (void)x; snprintf(out,n,"/tmp"); return 1; }
''' + text[start:end] + r'''
int main(void) {
  char a[700], b[700];
  SeekrFonte x = { .imdb="ttsynthetic", .durMs=7200000, .url="synthetic-edition-a" };
  SeekrFonte y = x; y.url="synthetic-edition-b";
  chaveArquivo(&x,a,sizeof a,""); chaveArquivo(&y,b,sizeof b,"");
  assert(a[0] && b[0] && strcmp(a,b));
  x.cabecalhos="Authorization: synthetic-a"; chaveArquivo(&x,a,sizeof a,""); assert(!a[0]);
  x.url=NULL; chaveArquivo(&x,a,sizeof a,""); assert(!a[0]);
  return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
    c = Path(d) / 'test.c'
    c.write_text(source)
    subprocess.run(['cc', '-std=c11', str(c), '-o', str(Path(d) / 'test')], check=True)
    subprocess.run([str(Path(d) / 'test')], check=True)
print('PASS cache identity')
