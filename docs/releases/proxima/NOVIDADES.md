# Próxima versão

As novidades candidatas foram consolidadas em [1.7.4](../1.7.4/NOTAS.md).

## Checklist

- **A partir da 2.0.6, pare de gerar e anexar o `_arm-highcache.ipk`** (`tools/arm.sh --high-cache`) e tire a linha de cache grande da tabela "Which file do I need?". A 2.0.5 é a última highcache: ela migra quem a instalou para Ajustes › Memória para imagens = 300 MB, uma vez, e o atualizador da highcache passa a pegar o `.ipk` normal.
