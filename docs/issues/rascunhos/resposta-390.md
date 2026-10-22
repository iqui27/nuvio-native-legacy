# Resposta ao #390 (Latvia1234, Samsung QN74F Tizen 9, .tpk 2.0.2)

Nao publicada. Para o dono revisar.

---

Thanks for the report! The custom poster URL field cut anything longer than 299 characters and turned uppercase letters in the key to lowercase, so a long template lost its `{imdb}` and was rejected without saving; the next update accepts templates up to 400 characters with the key intact. If yours is longer than 400 characters, or uses a placeholder other than `{imdb}`, `{tmdb}`, `{type}` or `{tipo_tmdb}` (for example `{imdb_id}`), please tell me which service it is so I can support it.
