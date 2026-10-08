#!/usr/bin/env node
/**
 * Automatic log triage routine
 * Queries D1 database, aggregates patterns, and posts to GitHub
 */

import { spawn } from 'child_process';
import { readFileSync } from 'fs';
import { execSync } from 'child_process';

const ACCOUNT_ID = process.env.CLOUDFLARE_ACCOUNT_ID;
const API_TOKEN = process.env.CLOUDFLARE_API_TOKEN;
const DB_NAME = 'nuvio-recomendacoes';
const OWNER_ID = 'trakt:iqui27';
const TRIAGEM_ISSUE_LABEL = 'triagem';

// Known error patterns to detect
const PATTERNS = {
  'crash-wasm': /Aborted.*RuntimeError|crash WASM/i,
  'sessao-morta': /\[avisos\]\s+a\s+sessao\s+anterior.*nao\s+se\s+despediu|nao\s+se\s+despediu/i,
  'video-avplay-erro': /\[video\]\s+avplay\s+erro.*errorText(?!.*No Error)/i,
  'legenda-ass': /(\[mkvass\]|\[legenda\]).*(?:falha|fallback)/i,
  'montagem-descartada': /montagem\s+descartada/i,
  'ajuste-zerado': /padrao\s+fora\s+da\s+lista.*ajuste\s+zerado/i,
  'decode-falhou': /decode\s+falhou\s+com\s+caminho\s+(?!\/|http)/i,
  'debrid-tfs': /\[debrid\].*(?:TorBox|P2P).*PLAN_RESTRICTED/i,
  'longtask-samsung': /longtask-max=\s*([2-9]\d{3,}|[1-9]\d{4,})\s*ms/i,
  'catalogo-cortado': /\[desc\].*de\s+fora\s+por\s+cota|#126/i,
};

async function queryD1(command) {
  return new Promise((resolve, reject) => {
    const args = [
      '--yes', 'wrangler', 'd1', 'execute',
      DB_NAME, '--remote', '--json',
      '--command', command
    ];

    const proc = spawn('npx', args, {
      env: {
        ...process.env,
        CLOUDFLARE_API_TOKEN: API_TOKEN,
        CLOUDFLARE_ACCOUNT_ID: ACCOUNT_ID,
      },
    });

    let stdout = '';
    let stderr = '';

    proc.stdout.on('data', (data) => {
      stdout += data.toString();
    });

    proc.stderr.on('data', (data) => {
      stderr += data.toString();
    });

    proc.on('close', (code) => {
      if (code !== 0) {
        reject(new Error(`wrangler exited with ${code}: ${stderr}`));
        return;
      }

      try {
        // Find JSON - look for [ at the start of a line
        let jsonStr = stdout;
        const jsonStart = stdout.indexOf('\n[');
        if (jsonStart !== -1) {
          jsonStr = stdout.substring(jsonStart + 1);
        } else if (!stdout.trim().startsWith('[')) {
          // Try to find [ anywhere
          const start = stdout.indexOf('[');
          if (start === -1) {
            reject(new Error('No JSON found in wrangler output'));
            return;
          }
          jsonStr = stdout.substring(start);
        }

        const data = JSON.parse(jsonStr);
        // Extract the actual results from the wrapped response
        if (Array.isArray(data) && data[0] && data[0].results) {
          resolve(data[0].results);
        } else if (Array.isArray(data)) {
          resolve(data);
        } else {
          resolve(data);
        }
      } catch (e) {
        reject(new Error(`Failed to parse wrangler output: ${e.message}`));
      }
    });
  });
}

async function getLastTriagemTimestamp() {
  try {
    const result = await queryD1('SELECT MAX(ultimo_log) as max_id FROM triagem;');
    if (result && result.length > 0 && result[0].max_id !== null) {
      return result[0].max_id;
    }
    return 0;
  } catch (e) {
    console.warn('Could not get last triagem timestamp:', e.message);
    return 0;
  }
}

async function getNewRegistros(lastId) {
  const query = `
    SELECT id, pessoa, versao, plataforma, quando, texto, criado
    FROM registro
    WHERE id > ${lastId} AND pessoa != '${OWNER_ID}'
    ORDER BY id ASC;
  `;

  try {
    return await queryD1(query);
  } catch (e) {
    console.error('Error fetching registros:', e.message);
    return [];
  }
}

function detectPatterns(texto) {
  const detected = [];
  for (const [pattern, regex] of Object.entries(PATTERNS)) {
    if (regex.test(texto)) {
      detected.push(pattern);
    }
  }
  return detected;
}

function aggregatePatterns(registros) {
  const aggregated = {};

  for (const reg of registros) {
    const patterns = detectPatterns(reg.texto);

    for (const pattern of patterns) {
      const key = `${pattern}|${reg.versao || 'unknown'}|${reg.plataforma || 'unknown'}`;

      if (!aggregated[key]) {
        aggregated[key] = {
          pattern,
          versao: reg.versao || 'unknown',
          plataforma: reg.plataforma || 'unknown',
          ocorrencias: 0,
          pessoas: new Set(),
          exemplo: reg.texto.substring(0, 100),
        };
      }

      aggregated[key].ocorrencias++;
      aggregated[key].pessoas.add(reg.pessoa);
    }
  }

  // Convert Sets to counts
  return Object.values(aggregated).map((item) => ({
    ...item,
    pessoas: item.pessoas.size,
  }));
}

async function findRelatedIssues(pattern) {
  // Simple mapping of patterns to issue numbers (would need GitHub search in real scenario)
  const issueMap = {
    'crash-wasm': 350,
    'sessao-morta': 349,
    'video-avplay-erro': 345,
    'legenda-ass': 92,
    'montagem-descartada': 348,
    'ajuste-zerado': 129,
    'longtask-samsung': 347,
    'catalogo-cortado': 126,
  };

  return issueMap[pattern] || null;
}

async function saveTriagem(patterns, lastRegistroId) {
  const timestamp = Math.floor(Date.now() / 1000);
  const queries = [];

  for (const p of patterns) {
    const issue = await findRelatedIssues(p.pattern);
    const estado = 'novo'; // Could compare with previous to detect regressions

    const query = `
      INSERT INTO triagem (criado, ultimo_log, padrao, versao, plataforma, ocorrencias, pessoas, issue, estado)
      VALUES (${timestamp}, ${lastRegistroId}, '${p.pattern}', '${p.versao}', '${p.plataforma}', ${p.ocorrencias}, ${p.pessoas}, ${issue || 'NULL'}, '${estado}');
    `;

    queries.push(query);
  }

  for (const query of queries) {
    try {
      await queryD1(query);
    } catch (e) {
      console.warn('Could not save triagem:', e.message);
    }
  }
}

async function findTriagemIssue() {
  try {
    // Search for any open issue with "Relatório" in title (most likely the triage report)
    const result = execSync(
      'gh api repos/iqui27/nuvio-native-legacy/issues -f state=open -f per_page=50',
      { encoding: 'utf8' }
    );

    const issues = JSON.parse(result || '[]');
    const reportIssue = issues.find(i => i.title.includes('Relatório'));
    if (reportIssue) {
      console.log(`Found triagem issue: #${reportIssue.number} - ${reportIssue.title}`);
      return reportIssue.number;
    }

    // Otherwise, look for any issue with triagem label
    for (const issue of issues) {
      if (issue.labels && issue.labels.some(l => l.name === 'triagem')) {
        console.log(`Found triagem labeled issue: #${issue.number}`);
        return issue.number;
      }
    }

    // Last resort: use issue #353 from the webhook
    return 353;
  } catch (e) {
    console.error('Failed to find triagem issue:', e.message);
    // Fallback to issue #353 which was just opened
    return 353;
  }
}

async function postGitHubComment(patterns) {
  // Format the aggregated patterns for GitHub comment
  const newPatterns = patterns.filter((p) => p.pattern !== 'unknown');
  const hasContent = newPatterns.length > 0;

  if (!hasContent) {
    console.log('No new patterns detected. Skipping GitHub comment.');
    return;
  }

  const issueNumber = await findTriagemIssue();
  if (!issueNumber) {
    console.error('Could not determine triagem issue number');
    return;
  }

  let comment = '## 📊 Log Triage Report\n\n';
  comment += `**Updated:** ${new Date().toISOString()}\n\n`;

  comment += '### New Patterns Detected\n\n';
  for (const p of newPatterns) {
    comment += `- **${p.pattern}** (v${p.versao} / ${p.plataforma})\n`;
    comment += `  - Occurrences: ${p.ocorrencias}\n`;
    comment += `  - Affected users: ${p.pessoas}\n`;
    if (p.issue) {
      comment += `  - Related: #${p.issue}\n`;
    }
    comment += '\n';
  }

  try {
    execSync(
      `gh api repos/iqui27/nuvio-native-legacy/issues/${issueNumber}/comments --input -`,
      {
        input: JSON.stringify({ body: comment }),
        encoding: 'utf8'
      }
    );
    console.log(`✓ Posted comment to issue #${issueNumber}`);
  } catch (e) {
    console.error('Failed to post GitHub comment:', e.message);
  }
}

async function main() {
  try {
    console.log('🔍 Starting log triage routine...');

    // Get the last processed log ID
    const lastId = await getLastTriagemTimestamp();
    console.log(`Last processed log ID: ${lastId}`);

    // Fetch new registros
    const registros = await getNewRegistros(lastId);
    console.log(`Found ${registros.length} new registros`);

    if (registros.length === 0) {
      console.log('✓ No new logs to process');
      return;
    }

    // Aggregate patterns
    const patterns = aggregatePatterns(registros);
    console.log(`Detected ${patterns.length} pattern occurrences`);

    // Save triagem results
    const maxId = Math.max(...registros.map((r) => r.id));
    await saveTriagem(patterns, maxId);
    console.log(`✓ Saved triagem for log IDs up to ${maxId}`);

    // Post to GitHub
    await postGitHubComment(patterns);

    console.log('✓ Triage routine completed');
  } catch (e) {
    console.error('❌ Error in triage routine:', e);
    process.exit(1);
  }
}

main();
