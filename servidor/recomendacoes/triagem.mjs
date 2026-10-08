#!/usr/bin/env node

import { exec } from 'child_process';
import { promisify } from 'util';
import https from 'https';

const execAsync = promisify(exec);

const DB_NAME = 'nuvio-recomendacoes';
const REPO_OWNER = 'iqui27';
const REPO_NAME = 'nuvio-native-legacy';
const TRIAGE_ISSUE = 324;

async function queryD1(sql) {
  const command = `npx --yes wrangler d1 execute ${DB_NAME} --remote --json --command "${sql.replace(/"/g, '\\"')}" 2>&1`;
  const { stdout } = await execAsync(command, { maxBuffer: 50 * 1024 * 1024 });

  // Parse JSON from output - skip warnings and find the first [
  const lines = stdout.split('\n');
  let jsonStart = -1;
  let jsonStr = '';

  for (let i = 0; i < lines.length; i++) {
    if (lines[i].trim().startsWith('[')) {
      jsonStart = i;
      jsonStr = lines.slice(i).join('\n');
      break;
    }
  }

  if (jsonStart === -1) {
    throw new Error('No JSON in output');
  }

  // Extract just the JSON part
  const lastBracket = jsonStr.lastIndexOf(']');
  jsonStr = jsonStr.substring(0, lastBracket + 1);

  const parsed = JSON.parse(jsonStr);
  return parsed[0]?.results || [];
}

async function getLastTriageState() {
  const result = await queryD1('SELECT MAX(ultimo_log) as ultimo_log FROM triagem');
  return result[0]?.ultimo_log || 0;
}

async function getNewLogs(sinceId) {
  const result = await queryD1(`
    SELECT id, pessoa, versao, plataforma, quando, texto, criado
    FROM registro
    WHERE id > ${sinceId} AND pessoa != 'trakt:iqui27'
    ORDER BY id ASC
  `);
  return result;
}

function extractPattern(text) {
  if (!text) return null;

  // Check for known patterns
  if (text.includes('Aborted(')) return 'Aborted(';
  if (text.includes('RuntimeError')) return 'RuntimeError';
  if (text.includes('crash') && text.includes('WASM')) return 'crash-WASM';
  if (text.includes('nao se despediu')) return 'nao-se-despediu';
  if (text.includes('[video]') && text.includes('avplay')) return '[video]-avplay';
  if (text.includes('[mkvass]')) return '[mkvass]';
  if (text.includes('[legenda]')) return '[legenda]';
  if (text.includes('montagem descartada')) return 'montagem-descartada';
  if (text.includes('decode falhou')) return 'decode-falhou';
  if (text.includes('[debrid]') && text.includes('PLAN_RESTRICTED')) return '[debrid]-PLAN_RESTRICTED';
  if (text.includes('longtask-max')) return 'longtask-max';
  if (text.includes('[desc]') && text.includes('fora por cota')) return '[desc]-quota';

  // Extract first meaningful pattern
  const match = text.match(/\[([^\]]+)\]|(\w+-\w+)|([A-Z][a-z]+(?:[A-Z][a-z]+)*)/);
  return match ? match[0] : 'outro';
}

function categorizeLog(log) {
  const pattern = extractPattern(log.texto);
  return {
    ...log,
    pattern,
    isManual: log.quando === 'manual',
    isDiagnostico: log.texto?.startsWith('diagnostico=v2')
  };
}

async function analyzeNewLogs(logs) {
  const grouped = {};

  for (const log of logs) {
    const categorized = categorizeLog(log);
    const key = `${categorized.pattern}|${categorized.versao}|${categorized.plataforma}`;

    if (!grouped[key]) {
      grouped[key] = {
        pattern: categorized.pattern,
        versao: categorized.versao,
        plataforma: categorized.plataforma,
        ocorrencias: 0,
        pessoas: new Set(),
        logs: []
      };
    }

    grouped[key].ocorrencias++;
    grouped[key].pessoas.add(categorized.pessoa);
    grouped[key].logs.push(categorized);
  }

  // Convert to array and finalize
  const analysis = Object.values(grouped).map(g => ({
    pattern: g.pattern,
    versao: g.versao,
    plataforma: g.plataforma,
    ocorrencias: g.ocorrencias,
    pessoas: g.pessoas.size,
    isCritical: ['crash-nativo', 'sem-quadro', 'watchdog', 'pouca-memoria'].includes(g.pattern),
    isRegression: false
  }));

  return analysis;
}

async function updateTriagem(analysis, maxLogId) {
  const now = Math.floor(Date.now() / 1000);

  for (const item of analysis) {
    const escapedPattern = item.pattern.replace(/'/g, "''");
    const sql = `
      INSERT INTO triagem (criado, ultimo_log, padrao, versao, plataforma, ocorrencias, pessoas, estado)
      VALUES (${now}, ${maxLogId}, '${escapedPattern}', '${item.versao}', '${item.plataforma}', ${item.ocorrencias}, ${item.pessoas}, 'novo')
    `;
    try {
      await queryD1(sql);
    } catch (e) {
      console.error(`Failed to insert triagem for ${item.pattern}:`, e.message);
    }
  }
}

function formatGitHubComment(analysis, newCount) {
  const critical = analysis.filter(a => a.isCritical);
  const others = analysis.filter(a => !a.isCritical).sort((a, b) => b.ocorrencias - a.ocorrencias);

  let comment = `## 📊 Triagem Automática de Logs\n\n`;
  comment += `**Período:** ${newCount} novos registros analisados\n`;
  comment += `**Padrões identificados:** ${analysis.length}\n`;
  comment += `**Ocorrências totais:** ${analysis.reduce((sum, a) => sum + a.ocorrencias, 0)}\n`;
  comment += `**Usuários afetados:** ${new Set(analysis.flatMap(a => a.pessoas)).size}\n\n`;

  if (critical.length > 0) {
    comment += `### 🔴 Erros Críticos\n\n`;
    for (const item of critical) {
      comment += `- **${item.pattern}** (${item.versao || 'vários'}/${item.plataforma || 'vários'}) — ${item.ocorrencias} ocorrências em ${item.pessoas} usuários\n`;
    }
    comment += '\n';
  }

  if (others.length > 0) {
    comment += `### ⚠️ Outros Padrões\n\n`;
    for (const item of others.slice(0, 15)) {  // Top 15 patterns
      comment += `- **${item.pattern}** (${item.versao || 'vários'}/${item.plataforma || 'vários'}) — ${item.ocorrencias} ocorrências em ${item.pessoas} usuários\n`;
    }
    if (others.length > 15) {
      comment += `\n_+ ${others.length - 15} padrões com menor frequência_\n`;
    }
  }

  comment += `\n---\n_Triagem automática de logs executada_`;
  return comment;
}

async function postGitHubComment(body) {
  // Note: GitHub commenting is handled by Claude Code via MCP tools
  // This function returns the comment for logging
  console.log('\n✅ GitHub comment ready for posting (use Claude Code MCP tools)');
  return body;
}

async function main() {
  try {
    console.log('📊 Iniciando triagem automática de logs...');

    const lastTriageId = await getLastTriageState();
    console.log(`Last triage state: ${lastTriageId}`);

    const newLogs = await getNewLogs(lastTriageId);
    console.log(`Found ${newLogs.length} new logs since ID ${lastTriageId}`);

    if (newLogs.length === 0) {
      console.log('No new logs to process.');
      return;
    }

    const analysis = await analyzeNewLogs(newLogs);
    console.log(`Analysis complete: ${analysis.length} patterns identified`);

    const maxLogId = Math.max(...newLogs.map(l => l.id));
    await updateTriagem(analysis, maxLogId);
    console.log(`✅ Triagem table updated with max log ID: ${maxLogId}`);

    const comment = formatGitHubComment(analysis, newLogs.length);
    console.log('\n📝 GitHub comment prepared:\n');
    console.log(comment);

    // Post comment to GitHub if token is available
    if (process.env.GITHUB_TOKEN) {
      await postGitHubComment(comment);
    }

  } catch (error) {
    console.error('Error during triage:', error.message);
    process.exit(1);
  }
}

main();
