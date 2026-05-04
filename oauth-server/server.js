/**
 * PebbleTasks settings + Google OAuth + token refresh proxy.
 * Configure via environment (see .env.example).
 * Loads oauth-server/.env when present (not automatic in Node without dotenv).
 */
const fs = require('fs');
const path = require('path');
require('dotenv').config({ path: path.join(__dirname, '.env') });

const crypto = require('crypto');
const express = require('express');
const https = require('https');
const { URL, URLSearchParams } = require('url');

const app = express();
/**
 * Rebble/PebbleKit JS XHR: `Access-Control-Allow-Origin: *` does not satisfy
 * CORS for `Origin: null` (file:/ embedded runtimes) — the browser may hide
 * the response and report xhr.status === 0 even when the server returned 200.
 * Echo the request Origin (or `null`) so the client can read the body.
 */
app.use((req, res, next) => {
  const origin = req.headers.origin;
  if (origin) {
    res.setHeader('Access-Control-Allow-Origin', origin);
    res.setHeader('Vary', 'Origin');
  } else {
    res.setHeader('Access-Control-Allow-Origin', '*');
  }
  res.setHeader('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS');
  res.setHeader(
    'Access-Control-Allow-Headers',
    req.headers['access-control-request-headers'] ||
      'Content-Type, Accept, Authorization, X-Requested-With, Origin, Cache-Control'
  );
  res.setHeader('Access-Control-Max-Age', '7200');
  if (req.method === 'OPTIONS') {
    res.status(204).end();
    return;
  }
  next();
});
app.use(express.json({ limit: '24kb' }));
app.use('/static', express.static(path.join(__dirname, 'static')));

const TEMPLATES_DIR = path.join(__dirname, 'templates');

function readTemplate(name) {
  return fs.readFileSync(path.join(TEMPLATES_DIR, name), 'utf8');
}

function fillTemplate(template, vars) {
  let out = template;
  for (const [key, value] of Object.entries(vars)) {
    out = out.split(`{{${key}}}`).join(value);
  }
  return out;
}

const STATE_TTL_MS = 15 * 60 * 1000;
const pendingStates = new Map();

function escapeHtmlAttr(s) {
  return String(s).replace(/&/g, '&amp;').replace(/"/g, '&quot;').replace(/</g, '&lt;');
}

function escapeHtml(s) {
  return String(s)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;');
}

function env() {
  return {
    port: Number(process.env.PORT || 3847),
    clientId: process.env.GOOGLE_CLIENT_ID || '',
    clientSecret: process.env.GOOGLE_CLIENT_SECRET || '',
    baseUrl: (process.env.BASE_PUBLIC_URL || `http://localhost:${process.env.PORT || 3847}`).replace(
      /\/$/,
      ''
    ),
  };
}

function pruneStates() {
  const now = Date.now();
  for (const [k, v] of pendingStates) {
    if (now - v.at > STATE_TTL_MS) {
      pendingStates.delete(k);
    }
  }
}

/**
 * @param {number} [timeoutMs] — abort outbound request if no response in time (avoids hanging
 *  Express and leaving Pebble sync XHR to time out with HTTP 0 while still waiting on Google).
 */
function httpsPostForm(urlStr, bodyObj, timeoutMs) {
  const u = new URL(urlStr);
  const body = new URLSearchParams(bodyObj).toString();
  const limit = timeoutMs == null || timeoutMs < 0 ? 20000 : timeoutMs;
  return new Promise((resolve, reject) => {
    let timer;
    const req = https.request(
      {
        hostname: u.hostname,
        path: u.pathname + u.search,
        method: 'POST',
        headers: {
          'Content-Type': 'application/x-www-form-urlencoded',
          'Content-Length': Buffer.byteLength(body),
        },
      },
      (res) => {
        let data = '';
        res.on('data', (c) => (data += c));
        res.on('end', () => {
          if (timer) {
            clearTimeout(timer);
            timer = null;
          }
          try {
            resolve({ status: res.statusCode || 0, json: JSON.parse(data || '{}'), raw: data });
          } catch (e) {
            resolve({ status: res.statusCode || 0, json: null, raw: data });
          }
        });
      }
    );
    req.on('error', (e) => {
      if (timer) {
        clearTimeout(timer);
        timer = null;
      }
      reject(e);
    });
    timer = setTimeout(() => {
      timer = null;
      try {
        req.destroy();
      } catch (e) {
        /* ignore */
      }
      reject(new Error('httpsPostForm timeout'));
    }, limit);
    req.write(body);
    req.end();
  });
}

/**
 * Rebble/Pebble static config: return_to is usually "pebblejs://close#"
 * and config JSON is *appended* to that string (one fragment, no second "#").
 * See https://developer.repebble.com/guides/user-interfaces/app-configuration-static/
 */
function redirectFragment(returnTo, payloadObj) {
  const tokenJson = JSON.stringify(payloadObj);
  if (returnTo.indexOf('#') >= 0) {
    return returnTo + encodeURIComponent(tokenJson);
  }
  return `${returnTo}#${encodeURIComponent(tokenJson)}`;
}

function settingsHtml(
  baseUrl,
  clientConfigured,
  initialMode,
  signedIn,
  defaultOauthStart,
  initialThemePreset
) {
  const base = baseUrl.replace(/\/$/, '');
  const oauthInputValueEsc = escapeHtmlAttr(
    defaultOauthStart || base + '/oauth/start?return_to=' + encodeURIComponent('pebblejs://close#')
  );
  const modeGoogle = initialMode === 'google';
  const checkLocal = modeGoogle ? '' : ' checked';
  const checkGoogle = modeGoogle ? ' checked' : '';
  const themeDark = initialThemePreset === 1;
  const themeCheckLight = themeDark ? '' : ' checked';
  const themeCheckDark = themeDark ? ' checked' : '';
  const showLinked = !!signedIn;
  const showNeedToken = !signedIn && modeGoogle;
  const signedHtml = showLinked
    ? '<p class="ok" id="signedStatus"><strong>Google is linked on this phone.</strong> The token is stored only in the Pebble app (not in this page). If you use Google mode, your tasks can sync; you will not see the token text again for security.</p>'
    : '';
  const needTokenHtml = showNeedToken
    ? '<p class="need-token" id="needTokenHint">No Google token on the phone yet — open the sign-in link in your <strong>native browser</strong>, then paste your <strong>PEBBLETASKS1…</strong> line under <strong>Paste Google auth token</strong> and tap <strong>Save to Pebble</strong>.</p>'
    : '';
  const missingCredsHtml = !clientConfigured
    ? '<p class="warn">Server missing GOOGLE_CLIENT_ID / GOOGLE_CLIENT_SECRET. Add them to <code>.env</code> and restart.</p>'
    : '';
  const tpl = readTemplate('settings.html');
  return fillTemplate(tpl, {
    BODY_MODE: initialMode === 'google' ? 'google' : 'local',
    SETTINGS_BASE: escapeHtmlAttr(base),
    SIGNED_STATUS_HTML: signedHtml,
    NEED_TOKEN_HTML: needTokenHtml,
    MISSING_CREDS_HTML: missingCredsHtml,
    CHECK_LOCAL: checkLocal,
    CHECK_GOOGLE: checkGoogle,
    THEME_PRESET_ID: themeDark ? '1' : '0',
    THEME_CHECK_LIGHT: themeCheckLight,
    THEME_CHECK_DARK: themeCheckDark,
    OAUTH_INPUT_VALUE: oauthInputValueEsc,
  });
}

function legalPageHtml(escapedTitle, contentFilename) {
  const shell = readTemplate('legal-shell.html');
  const content = readTemplate(contentFilename);
  return fillTemplate(shell, {
    TITLE: escapedTitle,
    CONTENT: content,
    DATE: new Date().toISOString().slice(0, 10),
  });
}

/** After Google OAuth: one HTML response with a PEBBLETASKS1… paste line — no DB/cache of tokens. */
function oauthCompleteHtml(pasteLine, appUrl, settingsHomeUrl) {
  const tpl = readTemplate('oauth-complete.html');
  return fillTemplate(tpl, {
    PASTE_LINE: pasteLine,
    APP_URL: escapeHtmlAttr(appUrl),
    SETTINGS_HOME: escapeHtmlAttr(settingsHomeUrl),
  });
}

app.get('/', (req, res) => {
  const cfg = env();
  // return_to, current_mode, signed_in, theme_preset are read in the page via getQueryParam (Rebble app-configuration-static pattern).
  const initialMode = req.query.current_mode === 'google' ? 'google' : 'local';
  const initialThemePreset = req.query.theme_preset === '1' ? 1 : 0;
  const signedIn = req.query.signed_in === '1' || req.query.signed_in === 'true';
  const baseNorm = cfg.baseUrl.replace(/\/$/, '');
  const returnToOauth =
    typeof req.query.return_to === 'string' && req.query.return_to.length
      ? req.query.return_to
      : 'pebblejs://close#';
  const defaultOauthStart = `${baseNorm}/oauth/start?return_to=${encodeURIComponent(returnToOauth)}`;
  res
    .set('Cache-Control', 'private, no-store, must-revalidate')
    .type('html')
    .send(
      settingsHtml(
        cfg.baseUrl,
        !!(cfg.clientId && cfg.clientSecret),
        initialMode,
        signedIn,
        defaultOauthStart,
        initialThemePreset
      )
    );
});

app.get('/privacy', (_req, res) => {
  res.type('html').send(
    legalPageHtml(escapeHtml('Privacy Policy'), 'privacy-content.html')
  );
});

app.get('/terms', (_req, res) => {
  res.type('html').send(
    legalPageHtml(escapeHtml('Terms of Service'), 'terms-content.html')
  );
});

app.get('/oauth/start', (req, res) => {
  pruneStates();
  const cfg = env();
  if (!cfg.clientId || !cfg.clientSecret) {
    res.status(500).send('OAuth not configured on server.');
    return;
  }
  const returnTo = typeof req.query.return_to === 'string' ? req.query.return_to : '';
  if (!returnTo) {
    res.status(400).send('Missing return_to query parameter.');
    return;
  }
  const state = crypto.randomBytes(24).toString('hex');
  pendingStates.set(state, { returnTo, at: Date.now() });
  const redirectUri = `${cfg.baseUrl}/oauth/callback`;
  const q = new URLSearchParams({
    client_id: cfg.clientId,
    redirect_uri: redirectUri,
    response_type: 'code',
    scope: 'https://www.googleapis.com/auth/tasks',
    access_type: 'offline',
    prompt: 'consent',
    state,
  });
  res.redirect(`https://accounts.google.com/o/oauth2/v2/auth?${q.toString()}`);
});

app.get('/oauth/callback', async (req, res) => {
  pruneStates();
  const cfg = env();
  const { code, state, error } = req.query;
  if (error) {
    res.status(400).send(`OAuth error: ${error}`);
    return;
  }
  if (!code || !state || typeof state !== 'string') {
    res.status(400).send('Invalid callback.');
    return;
  }
  const meta = pendingStates.get(state);
  pendingStates.delete(state);
  if (!meta) {
    res.status(400).send('Invalid or expired state. Try Connect Google again.');
    return;
  }
  const redirectUri = `${cfg.baseUrl}/oauth/callback`;
  let tokenRes;
  try {
    tokenRes = await httpsPostForm('https://oauth2.googleapis.com/token', {
      code,
      client_id: cfg.clientId,
      client_secret: cfg.clientSecret,
      redirect_uri: redirectUri,
      grant_type: 'authorization_code',
    });
  } catch (e) {
    res.status(502).send('Token exchange failed.');
    return;
  }
  if (tokenRes.status !== 200 || !tokenRes.json || !tokenRes.json.access_token) {
    res
      .status(400)
      .send(
        `Token error: ${tokenRes.raw || JSON.stringify(tokenRes.json)}`
      );
    return;
  }
  const t = tokenRes.json;
  const payload = {
    mode: 'google',
    access_token: t.access_token,
    refresh_token: t.refresh_token || undefined,
    expires_in: t.expires_in,
    token_type: t.token_type,
  };
  const pasteLine =
    'PEBBLETASKS1' +
    Buffer.from(JSON.stringify(payload), 'utf8').toString('base64');
  const appUrl = redirectFragment(meta.returnTo, payload);
  const settingsHomeUrl = `${cfg.baseUrl.replace(/\/$/, '')}/`;
  res
    .set('Cache-Control', 'private, no-store, must-revalidate')
    .type('html')
    .send(oauthCompleteHtml(pasteLine, appUrl, settingsHomeUrl));
});

app.post('/oauth/refresh', async (req, res) => {
  const cfg = env();
  const rt = req.body && req.body.refresh_token;
  if (!rt || typeof rt !== 'string') {
    res.status(400).json({ error: 'refresh_token required' });
    return;
  }
  if (!cfg.clientId || !cfg.clientSecret) {
    res.status(500).json({ error: 'server oauth not configured' });
    return;
  }
  let tokenRes;
  try {
    tokenRes = await httpsPostForm(
      'https://oauth2.googleapis.com/token',
      {
        refresh_token: rt,
        client_id: cfg.clientId,
        client_secret: cfg.clientSecret,
        grant_type: 'refresh_token',
      },
      12000
    );
  } catch (e) {
    res.status(502).json({ error: 'refresh request failed' });
    return;
  }
  if (tokenRes.status !== 200 || !tokenRes.json || !tokenRes.json.access_token) {
    res.status(401).json({
      error: 'refresh_failed',
      detail: tokenRes.json || tokenRes.raw,
    });
    return;
  }
  res.json({
    access_token: tokenRes.json.access_token,
    expires_in: tokenRes.json.expires_in,
    token_type: tokenRes.json.token_type,
  });
});

const listenPort = env().port;
app.listen(listenPort, () => {
  console.log(`PebbleTasks oauth-server listening on ${listenPort}`);
});
