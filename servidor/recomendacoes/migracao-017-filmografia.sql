CREATE TABLE IF NOT EXISTS pessoa_seguida (
  dono TEXT NOT NULL,
  tmdb INTEGER NOT NULL,
  seguindo INTEGER NOT NULL CHECK(seguindo IN (0,1)),
  PRIMARY KEY(dono, tmdb)
);
