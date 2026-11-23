// CODIGO DO REGISTRO (Ajustes > Enviar registro, 03/10): seis caracteres que a
// pessoa dita no grupo ou cola na issue. E o id da linha numa bijecao
// afim mod 2^30 em base32 de Crockford (sem I, L, O, U): nao e sequencial na
// tela, nao precisa de coluna nova, e volta ao id com
// codigo-registro.mjs.
// A TV calcula o MESMO codigo a partir do registro_id (src/regcodigo.c) quando
// o servidor ainda nao manda este campo; mudar as constantes aqui exige mudar
// la tambem, senao suporte e TV leem codigos diferentes.
const COD_ALFA = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
const COD_A = 0x2c5e1b3dn, COD_A_INV = 0x39331415n, COD_B = 0x15a4e3c7n, COD_M = 1n << 30n;
export const FAROL_ID_MIN = 1 << 29;

export function codigoRegistro(id) {
  const n = BigInt(id);
  if (n < 1n || n >= COD_M) return null;
  let v = (n * COD_A + COD_B) % COD_M, s = "";
  for (let i = 0; i < 6; i++) { s = COD_ALFA[Number(v & 31n)] + s; v >>= 5n; }
  return s;
}

// Inverso da bijecao em base32 de Crockford (aceita minusculas e O->0, I/L->1).
// Exige exatamente 6 caracteres sem espacos; devolve id em 1..2^30-1 ou null.
export function decodificarCodigo(cod) {
  if (typeof cod !== "string" || !/^[0-9A-Za-z]{6}$/.test(cod)) return null;
  const norm = cod.toUpperCase().replace(/O/g, "0").replace(/[IL]/g, "1");
  if (!/^[0-9A-HJKMNP-TV-Z]{6}$/.test(norm)) return null;
  let v = 0n;
  for (const c of norm) v = v * 32n + BigInt(COD_ALFA.indexOf(c));
  const id = Number((((v - COD_B) % COD_M + COD_M) % COD_M * COD_A_INV) % COD_M);
  return id >= 1 ? id : null;
}
