-- JWKS publica e verificacao ocasional por CONTA, nunca por token.
CREATE TABLE IF NOT EXISTS nuvio_jwks (
  url TEXT PRIMARY KEY,
  chaves TEXT NOT NULL DEFAULT '',
  expira INTEGER NOT NULL DEFAULT 0,
  rebusca INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS nuvio_conta (
  sub TEXT PRIMARY KEY,
  ultima_verificacao INTEGER NOT NULL,
  nome TEXT NOT NULL DEFAULT '',
  revogado INTEGER NOT NULL DEFAULT 0
);
ALTER TABLE ident_hora ADD COLUMN assinatura INTEGER NOT NULL DEFAULT 0;
