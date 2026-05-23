#!/usr/bin/env node
/**
 * Decode a PEBBLETASKS1… line from the oauth-server and write src/pkjs/oauth_config.local.js.
 *
 * Usage:
 *   npm run decode-token -- 'PEBBLETASKS1...'
 *   node scripts/pebbletasks-decode-token.js 'PEBBLETASKS1...'
 *   echo PEBBLETASKS1… | node scripts/pebbletasks-decode-token.js
 */

const fs = require('fs');
const path = require('path');

const PREFIX = 'PEBBLETASKS1';
const LOCAL_CONFIG = path.join(__dirname, '../src/pkjs/oauth_config.local.js');

function parsePastedAuthLine(s) {
  s = typeof s === 'string' ? s.trim() : '';
  if (!s) {
    return null;
  }
  if (s.startsWith(PREFIX)) {
    try {
      return JSON.parse(Buffer.from(s.slice(PREFIX.length), 'base64').toString('utf8'));
    } catch {
      return null;
    }
  }
  try {
    return JSON.parse(s);
  } catch {
    /* */
  }
  try {
    return JSON.parse(decodeURIComponent(s));
  } catch {
    return null;
  }
}

function readExistingSettingsBaseUrl() {
  if (!fs.existsSync(LOCAL_CONFIG)) {
    return null;
  }
  const text = fs.readFileSync(LOCAL_CONFIG, 'utf8');
  const m = text.match(/^\s*settingsBaseUrl:\s*['"]([^'"]+)['"]/m);
  return m ? m[1] : null;
}

function writeLocalConfig(hardcodeValue, settingsBaseUrl) {
  const lines = [
    '/**',
    ' * Auto-written by scripts/pebbletasks-decode-token.js — gitignored.',
    ' * Rebuild/install the Pebble app after updating. Delete before shipping.',
    ' */',
    'module.exports = {',
    '  hardcodedAccessToken: ' + JSON.stringify(hardcodeValue) + ',',
  ];
  if (settingsBaseUrl) {
    lines.push('  settingsBaseUrl: ' + JSON.stringify(settingsBaseUrl) + ',');
  } else {
    lines.push('  // settingsBaseUrl: \'http://192.168.1.10:3847\',  // LAN oauth-server for phone testing');
  }
  lines.push('};', '');
  fs.writeFileSync(LOCAL_CONFIG, lines.join('\n'), 'utf8');
}

function readInput() {
  const arg = process.argv[2];
  if (arg && arg !== '-') {
    return arg;
  }
  if (process.stdin.isTTY) {
    return null;
  }
  return fs.readFileSync(0, 'utf8').trim();
}

function main() {
  const raw = readInput();
  if (!raw) {
    console.error(
      'Usage: npm run decode-token -- <PEBBLETASKS1… or JSON>\n' +
        '       echo PEBBLETASKS1… | npm run decode-token'
    );
    process.exit(1);
  }

  const payload = parsePastedAuthLine(raw);
  if (!payload || !payload.access_token) {
    console.error('Could not parse token. Expected PEBBLETASKS1 + base64(JSON) or JSON with access_token.');
    process.exit(1);
  }

  const hardcodeValue = raw.trim().startsWith(PREFIX) ? raw.trim() : JSON.stringify(payload);
  const settingsBaseUrl = readExistingSettingsBaseUrl();
  writeLocalConfig(hardcodeValue, settingsBaseUrl);

  console.log('Wrote ' + path.relative(process.cwd(), LOCAL_CONFIG));
  if (settingsBaseUrl) {
    console.log('Kept settingsBaseUrl: ' + settingsBaseUrl);
  } else {
    console.log('Uncomment settingsBaseUrl in that file if your phone needs a LAN oauth-server URL.');
  }
  console.log('Rebuild/install the Pebble app. Delete oauth_config.local.js before shipping.');
}

main();
