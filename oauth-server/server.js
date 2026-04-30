/**
 * PebbleTasks settings + Google OAuth + token refresh proxy.
 * Configure via environment (see .env.example).
 * Loads oauth-server/.env when present (not automatic in Node without dotenv).
 */
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

const STATE_TTL_MS = 15 * 60 * 1000;
const pendingStates = new Map();
/** One-time OAuth result for /oauth/complete; deleted after first load. */
const authPickups = new Map();
/** Short id → full config for Pebble webview (avoids iOS pebblejs URL length limits). */
const watchPickups = new Map();
/** Device id (from PKJS) → last POST /oauth/watch-fragment payload, until pulled once (empty webview). */
const deviceStash = new Map();
const DEVICE_STASH_TTL_MS = 30 * 60 * 1000;

function escapeHtmlAttr(s) {
  return String(s).replace(/&/g, '&amp;').replace(/"/g, '&quot;').replace(/</g, '&lt;');
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
  for (const [k, v] of authPickups) {
    if (now - v.at > STATE_TTL_MS) {
      authPickups.delete(k);
    }
  }
  for (const [k, v] of watchPickups) {
    if (now - v.at > STATE_TTL_MS) {
      watchPickups.delete(k);
    }
  }
  for (const [k, v] of deviceStash) {
    if (now - v.at > DEVICE_STASH_TTL_MS) {
      deviceStash.delete(k);
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

const AUTH_PASTE_PREFIX = 'PEBBLETASKS1';

function parsePastedAuthLineToPayload(line) {
  const s = (line && String(line)) || '';
  const t = s.replace(/^\s+|\s+$/g, '');
  if (!t) {
    return null;
  }
  if (t.indexOf(AUTH_PASTE_PREFIX) === 0) {
    try {
      return JSON.parse(Buffer.from(t.slice(AUTH_PASTE_PREFIX.length), 'base64').toString('utf8'));
    } catch (e) {
      return null;
    }
  }
  try {
    return JSON.parse(t);
  } catch (a) {
    /* */
  }
  try {
    return JSON.parse(decodeURIComponent(t));
  } catch (b) {
    return null;
  }
}

/**
 * One line for copy/paste into Pebble app settings (ASCII–safe, no newlines in payload).
 * PKJS and settings page both accept this and raw JSON.
 */
function authPayloadToPasteLine(payloadObj) {
  return AUTH_PASTE_PREFIX + Buffer.from(JSON.stringify(payloadObj), 'utf8').toString('base64');
}

function completeHtml(pastedLine) {
  const lineEsc = (pastedLine || '').replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/"/g, '&quot;');
  return `<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8"/><meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>Google sign-in done — PebbleTasks</title>
<style>
html,body{background:#f7f7f7;color:#111;margin:0;padding:0;-webkit-text-size-adjust:100%}
body{font-family:Helvetica,Arial,sans-serif;max-width:28rem;margin:1.5rem auto;padding:0 1rem 2rem}
h1{font-size:1.1rem}
textarea{width:100%;min-height:5.5rem;box-sizing:border-box;padding:.5rem;font:12px/1.3 Menlo,monospace;word-break:break-all;border:1px solid #aaa}
.copy-btn{margin-top:.6rem;padding:.5rem 1rem;background:#222;color:#fff;border:0;border-radius:4px;font:15px Helvetica,Arial;width:100%;box-sizing:border-box;cursor:pointer;font-weight:600}
ol{padding-left:1.2rem;margin:.6rem 0 1rem 0}
li{margin:.4rem 0}
.hint{color:#333;font-size:14px}
</style></head>
<body>
<h1>Google sign-in complete</h1>
<p class="hint">Ribble’s back-to-app link often does not hand tokens to the watch. Use <strong>copy and paste</strong> instead.</p>
<ol>
  <li>Tap <strong>Copy token</strong> below (or long-press the box → Select All → Copy).</li>
  <li>Switch to the <strong>Rebble</strong> app, open <strong>Settings</strong> for PebbleTasks again (gear / app settings from the watch flow).</li>
  <li>On the PebbleTasks settings page, find <strong>Paste auth token</strong>, tap the field, paste, then <strong>Save to Pebble</strong>.</li>
</ol>
<textarea id="t" readonly="readonly">${lineEsc}</textarea>
<button type="button" class="copy-btn" id="copy">Copy token</button>
<script>
(function(){
  var t = document.getElementById('t');
  document.getElementById('copy').onclick = function() {
    if (!t) return;
    t.focus();
    t.select();
    try {
      if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(t.value);
        alert('Copied. Open Rebble → PebbleTasks settings → paste → Save to Pebble.');
        return;
      }
    } catch (e) {}
    try { document.execCommand('copy'); alert('Copied. Paste in Rebble on the PebbleTasks settings page.'); } catch (e2) { alert('Long-press the box, Select All, Copy.'); }
  };
})();
</script>
</body></html>`;
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
  const oauthInputValueEsc = escapeHtmlAttr(defaultOauthStart || base + '/oauth/start?return_to=' + encodeURIComponent('pebblejs://close#'));
  const modeGoogle = initialMode === 'google';
  const checkLocal = modeGoogle ? '' : ' checked';
  const checkGoogle = modeGoogle ? ' checked' : '';
  const showLinked = !!signedIn;
  const showNeedToken = !signedIn && modeGoogle;
  let t = 0;
  if (typeof initialThemePreset === 'number' && !Number.isNaN(initialThemePreset)) {
    t = initialThemePreset;
  }
  if (t < 0) {
    t = 0;
  }
  if (t > 3) {
    t = 3;
  }
  const checkT0 = t === 0 ? ' checked' : '';
  const checkT1 = t === 1 ? ' checked' : '';
  const checkT2 = t === 2 ? ' checked' : '';
  const checkT3 = t === 3 ? ' checked' : '';
  return `<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8"/><meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>PebbleTasks</title>
<style>
html,body{background-color:#fff;color:#111;margin:0;-webkit-text-size-adjust:100%}
body{font-family:Helvetica,Arial,sans-serif;max-width:28rem;margin:1.5rem auto;padding:0 1rem}
h1{font-size:1.1rem}
label{display:block;margin:.75rem 0}
button{margin-top:1rem;padding:.5rem 1rem;background:#eee;color:#111;border:1px solid #ccc;border-radius:2px;font-size:inherit;cursor:pointer;vertical-align:middle}
button.secondary{margin-top:.5rem;padding:.35rem .75rem;font-size:13px}
body.mode-local #googleBlock{display:none}
body.mode-google #googleBlock{display:block}
#googleBlock p{margin:.5rem 0 0 0}
#oauthUrl{width:100%;box-sizing:border-box;padding:.4rem;font-size:11px;margin-top:.4rem;word-break:break-all;border:1px solid #aaa}
.copy-btn{margin-top:.4rem;padding:.5rem 1rem;background:#222;color:#fff;border:0;border-radius:4px;font-size:15px;font-weight:600;cursor:pointer;width:100%;max-width:20rem;box-sizing:border-box}
#connectBlock{margin-top:1rem;padding-top:1rem;border-top:1px solid #ddd}
#connectBlock.is-hidden,#connectBlock.is-hidden #connectHint{display:none}
.hint{color:#333;font-size:12px;line-height:1.4}
.warn{color:#a50;font-size:.9rem}
.ok{background:#e8f5e9;border:1px solid #81c784;color:#1b5e20;padding:.55rem .65rem;border-radius:4px;font-size:14px;margin:.6rem 0 1rem 0;line-height:1.35}
.need-token{background:#fff8e1;border:1px solid #ffcc80;color:#5d4400;padding:.55rem .65rem;border-radius:4px;font-size:14px;margin:.6rem 0 1rem 0;line-height:1.35}
body.mode-local #noEchoHint{display:none}
body.mode-local #needTokenHint{display:none}
</style></head>
<body class="mode-${initialMode === 'google' ? 'google' : 'local'}">
<h1>PebbleTasks</h1>
${
  showLinked
    ? '<p class="ok" id="signedStatus"><strong>Google is linked on this phone.</strong> The token is stored only in the Ribble app (not in this page). If you use Google mode, your tasks can sync; you will not see the token text again for security.</p>'
    : ''
}
${
  showNeedToken
    ? '<p class="need-token" id="needTokenHint">No sign-in token on the phone yet — use <strong>Copy sign-in link</strong> or paste a token from the “sign-in complete” page, then <strong>Save to Pebble</strong>.</p>'
    : ''
}
<p>Choose where your tasks are stored on the phone.</p>
${
  !clientConfigured
    ? '<p class="warn">Server missing GOOGLE_CLIENT_ID / GOOGLE_CLIENT_SECRET. Add them to <code>.env</code> and restart.</p>'
    : ''
}
<div>
  <label><input type="radio" name="mode" value="local"${checkLocal}/> Stay local (offline, on phone only)</label>
  <label><input type="radio" name="mode" value="google"${checkGoogle}/> Use Google Tasks (sync with your Google account)</label>
</div>
<div style="margin-top:1.25rem;padding-top:1.25rem;border-top:1px solid #ddd">
<h2 style="font-size:1rem;font-weight:600;margin:0 0 .5rem 0">Watch appearance</h2>
<p class="hint" style="margin:0 0 .5rem 0">Color palette on the Pebble (color devices) or contrast style (black &amp; white).</p>
<label><input type="radio" name="theme_preset" value="0"${checkT0}/> Classic (yellow + green)</label>
<label><input type="radio" name="theme_preset" value="1"${checkT1}/> Dark</label>
<label><input type="radio" name="theme_preset" value="2"${checkT2}/> Calm (light gray + blue)</label>
<label><input type="radio" name="theme_preset" value="3"${checkT3}/> High contrast (white + black)</label>
</div>
<button type="button" id="save">Save &amp; return to Pebble</button>
<p class="hint" style="margin:.9rem 0 0 0">
  <a href="/privacy" target="_blank" rel="noopener noreferrer">Privacy Policy</a>
  &nbsp;|&nbsp;
  <a href="/terms" target="_blank" rel="noopener noreferrer">Terms of Service</a>
</p>
<div id="googleBlock">
<p class="warn" id="googleNote">Sign in with Google in your device’s browser, then return to Ribble.</p>
<p class="hint" id="copyHint">1. Copy the link below. 2. Open your browser, paste and go. 3. When Google is done, switch back to Ribble.</p>
<input type="text" id="oauthUrl" readonly="readonly" value="${oauthInputValueEsc}"/>
<button type="button" class="copy-btn" id="copyOauth">Copy sign-in link</button>
<div id="connectBlock">
<p class="hint" id="connectHint">Or try (often works on Android, rarely on iPhone in-app):</p>
<button type="button" class="btnlink" id="connect" style="margin-top:.5rem;padding:.5rem 1rem">Open sign-in in browser (try)</button>
</div>
</div>
<div id="authPasteBlock" style="margin-top:1.5rem;padding-top:1.5rem;border-top:1px solid #ccc">
<h2 style="font-size:1rem;font-weight:600;margin:0 0 .4rem 0">Paste Google auth token</h2>
<p class="hint" id="authPasteHelp">If you finished sign-in in the browser, you will see a <strong>sign-in complete</strong> page with a <strong>token</strong> line. Copy that line, paste it in the field below, then save — this is the most reliable way on iPhone.</p>
<textarea id="authPaste" style="width:100%;min-height:4.5rem;box-sizing:border-box;padding:.4rem;font:12px/1.35 Menlo,monospace;border:1px solid #aaa;border-radius:2px" rows="3" placeholder="PEBBLETASKS1... or { &quot;mode&quot;:&quot;google&quot;,... } "></textarea>
<p class="hint" id="noEchoHint" style="margin:.4rem 0 0 0">This field stays empty on each new visit. After you save, reopen settings from the watch: if the app has a token, a <strong>green “Google is linked”</strong> line appears at the top.</p>
<button type="button" id="authPasteSave" style="margin-top:.5rem;padding:.5rem 1rem;background:#222;color:#fff;border:0;border-radius:4px;font:15px Helvetica,Arial;width:100%;box-sizing:border-box;cursor:pointer;font-weight:600">Save to Pebble</button>
</div>
<script>
(function(){
  function getQueryParam(variable, defaultValue) {
    var query = location.search ? location.search.substring(1) : '';
    var vars = query ? query.split('&') : [];
    for (var i = 0; i < vars.length; i++) {
      var part = vars[i];
      var eq = part.indexOf('=');
      var key = eq >= 0 ? part.slice(0, eq) : part;
      var keyMatch = false;
      try {
        keyMatch = decodeURIComponent(key) === variable;
      } catch (e) {
        keyMatch = key === variable;
      }
      if (!keyMatch) {
        continue;
      }
      if (eq < 0) {
        return '';
      }
      var rawV = part.slice(eq + 1).split('+').join(' ');
      try {
        return decodeURIComponent(rawV);
      } catch (e2) {
        return rawV;
      }
    }
    return defaultValue !== undefined && defaultValue !== null ? defaultValue : false;
  }
  var settingsBase = ${JSON.stringify(base)};
  var rt = getQueryParam('return_to', 'pebblejs://close#');
  if (typeof rt !== 'string' || !rt) {
    rt = 'pebblejs://close#';
  }
  var oauthStartUrl = settingsBase + '/oauth/start?return_to=' + encodeURIComponent(rt);
  var oauthIn = document.getElementById('oauthUrl');
  if (oauthIn) { oauthIn.value = oauthStartUrl; }
  var modes = document.getElementsByName('mode');
  var curMode = getQueryParam('current_mode', 'local');
  if (curMode === 'google' || curMode === 'local') {
    for (var m = 0; m < modes.length; m++) {
      if (modes[m].value === curMode) { modes[m].checked = true; }
    }
  }
  if (document.body) {
    if (curMode === 'google') { document.body.className = 'mode-google'; }
    else if (curMode === 'local') { document.body.className = 'mode-local'; }
  }
  (function () {
    var s = getQueryParam('signed_in', '0');
    var linked = s === '1' || s === 'true';
    var st0 = document.getElementById('signedStatus');
    var nd0 = document.getElementById('needTokenHint');
    if (linked) {
      if (st0) { st0.style.display = ''; }
      if (nd0) { nd0.style.display = 'none'; }
    } else {
      if (st0) { st0.style.display = 'none'; }
      if (nd0) { nd0.style.display = ''; }
    }
  })();
  var noteEl = document.getElementById('googleNote');
  var copyHint = document.getElementById('copyHint');
  var ua = navigator.userAgent || '';
  var isIOS = /iP(hone|ad|od)/.test(ua) || (typeof navigator !== 'undefined' && navigator.maxTouchPoints > 1 && /MacIntel/.test(navigator.platform));
  if (isIOS) {
    if (noteEl) { noteEl.innerHTML = 'On <strong>iPhone or iPad</strong>, sign in with <strong>Safari</strong>, then copy a token and paste it here — the usual “back to Ribble with tokens in the link” does not work reliably.'; }
    if (copyHint) { copyHint.innerHTML = '1. <strong>Copy sign-in link</strong> → 2. Open <strong>Safari</strong>, paste in the address bar, <strong>Go</strong> → 3. Finish Google sign-in → 4. On the <strong>Sign-in complete</strong> page, <strong>Copy token</strong> → 5. Back in <strong>Ribble</strong>, open this app’s <strong>Settings</strong> again → 6. Scroll to <strong>Paste Google auth token</strong> → paste → <strong>Save to Pebble</strong>.'; }
    var connectBlock = document.getElementById('connectBlock');
    if (connectBlock) { connectBlock.className = 'is-hidden'; }
  } else {
    if (copyHint) { copyHint.textContent = 'After Google, open the “sign-in complete” page in the browser, copy the token, then in Ribble open PebbleTasks settings again, paste under “Paste Google auth token”, and tap Save to Pebble (optional if the automatic return works for you).'; }
  }
  function sync(){
    var g = false;
    for (var i=0;i<modes.length;i++){ if(modes[i].checked && modes[i].value==='google') g=true; }
    if (document.body) { document.body.className = g ? 'mode-google' : 'mode-local'; }
  }
  for (var j=0;j<modes.length;j++){ modes[j].addEventListener('change', sync); }
  sync();
  function openInExternalBrowser(href) {
    var a = document.createElement('a');
    a.href = href;
    a.setAttribute('target', '_blank');
    a.setAttribute('rel', 'noopener noreferrer');
    a.style.cssText = 'position:absolute;left:-9999px;';
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
  }
  function tryAndroidExternal(href) {
    if (!/Android/i.test(navigator.userAgent || '')) return;
    try {
      var intent = 'intent://#Intent;action=android.intent.action.VIEW' +
        ';S.browser_fallback_url=' + encodeURIComponent(href) +
        ';end';
      window.location.href = intent;
    } catch (e) {}
  }
  var connectEl = document.getElementById('connect');
  if (connectEl) {
  connectEl.onclick = function() {
    if (!oauthStartUrl) return;
    openInExternalBrowser(oauthStartUrl);
    try { window.open(oauthStartUrl, '_blank', 'noopener,noreferrer'); } catch (e) {}
    tryAndroidExternal(oauthStartUrl);
  };
  }
  document.getElementById('copyOauth').onclick = function() {
    var el = document.getElementById('oauthUrl');
    if (!el) return;
    el.focus();
    el.select();
    try {
      if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(el.value);
        if (isIOS) {
          alert('Copied. Open Safari, tap the address bar, paste, then Go. When Google is finished, return to the Ribble app (App Switcher).');
        } else {
          alert('Link copied. Open Chrome or the browser, paste, complete sign-in, then return to the Pebble app.');
        }
        return;
      }
    } catch (e) {}
    try {
      if (document.execCommand('copy')) {
        alert('Copied. Open your browser, paste, then return to the Pebble app when done.');
        return;
      }
    } catch (e2) {}
    alert('Long-press the field, tap Select All, then Copy. Paste in Safari’s address bar.');
  };
  document.getElementById('save').onclick = function(){
    var mode = 'local';
    for (var i=0;i<modes.length;i++){ if(modes[i].checked) mode = modes[i].value; }
    if(!rt){ alert('Missing return_to — open settings from the Pebble app.'); return; }
    var themePreset = 0;
    var tprs = document.getElementsByName('theme_preset');
    for (var ti=0; ti<tprs.length; ti++) {
      if (tprs[ti].checked) {
        var tv = parseInt(tprs[ti].value, 10);
        themePreset = (isNaN(tv) || tv < 0 || tv > 3) ? 0 : tv;
        break;
      }
    }
    var payload = { mode: mode, themePreset: themePreset };
    if (rt.indexOf('#') >= 0) {
      document.location = rt + encodeURIComponent(JSON.stringify(payload));
    } else {
      document.location = rt + '#' + encodeURIComponent(JSON.stringify(payload));
    }
  };
  function parsePebbleAuthPaste(s) {
    s = (s || '').replace(/^\\s+|\\s+$/g, '');
    if (!s) { return null; }
    if (s.indexOf('PEBBLETASKS1') === 0) {
      try { return JSON.parse(atob(s.slice('PEBBLETASKS1'.length))); } catch (e) { return null; }
    }
    try { return JSON.parse(s); } catch (a) {}
    try { return JSON.parse(decodeURIComponent(s)); } catch (b) {}
    return null;
  }
  var authSave = document.getElementById('authPasteSave');
  if (authSave) {
    authSave.onclick = function() {
      var t = (document.getElementById('authPaste') && document.getElementById('authPaste').value) || '';
      var o = parsePebbleAuthPaste(t);
      if (!o || !o.access_token) { alert('Could not read a Google token. Paste the full line from the sign-in complete page (starts with PEBBLETASKS1), or full JSON with access_token.'); return; }
      if (!rt) { alert('Missing return_to — open settings from the Pebble app.'); return; }
      if (!o.mode) { o.mode = 'google'; }
      var goShort = function (id) {
        var tiny = { w: id, mode: 'google' };
        var enc = encodeURIComponent(JSON.stringify(tiny));
        if (rt.indexOf('#') >= 0) {
          document.location = rt + enc;
        } else {
          document.location = rt + '#' + enc;
        }
      };
      var goLong = function () {
        var enc = encodeURIComponent(JSON.stringify(o));
        if (rt.indexOf('#') >= 0) { document.location = rt + enc; }
        else { document.location = rt + '#' + enc; }
      };
      var x = new XMLHttpRequest();
      x.open('POST', settingsBase + '/oauth/watch-fragment', true);
      x.setRequestHeader('Content-Type', 'application/json');
      x.onreadystatechange = function() {
        if (x.readyState !== 4) { return; }
        if (x.status === 200) {
          try {
            var j = JSON.parse(x.responseText);
            if (j && j.id) { goShort(String(j.id)); return; }
          } catch (e) {}
        }
        try { goLong(); } catch (e2) { alert('Could not send config to the watch. Check that the phone can reach: ' + settingsBase); }
      };
      try {
        x.send(JSON.stringify({ payload: o, d: getQueryParam('d', '') || '' }));
      } catch (e) {
        goLong();
      }
    };
  }
})();
</script>
</body></html>`;
}

function legalPageHtml(title, bodyHtml) {
  return `<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8"/><meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>${escapeHtml(title)} — PebbleTasks</title>
<style>
html,body{background:#fff;color:#111;margin:0;-webkit-text-size-adjust:100%}
body{font-family:Helvetica,Arial,sans-serif;max-width:34rem;margin:1.5rem auto;padding:0 1rem;line-height:1.5}
h1{font-size:1.15rem;margin:.2rem 0 .8rem 0}
h2{font-size:1rem;margin:1rem 0 .4rem 0}
p,li{font-size:.95rem}
a{color:#0b57d0}
.muted{color:#555;font-size:.85rem}
</style></head><body>
<h1>${escapeHtml(title)}</h1>
${bodyHtml}
<p class="muted">Last updated: ${new Date().toISOString().slice(0, 10)}</p>
<p><a href="/">Back to PebbleTasks settings</a></p>
</body></html>`;
}

app.get('/', (req, res) => {
  const cfg = env();
  // return_to, current_mode, signed_in are read in the page via getQueryParam (Rebble app-configuration-static pattern).
  const initialMode = req.query.current_mode === 'google' ? 'google' : 'local';
  const signedIn = req.query.signed_in === '1' || req.query.signed_in === 'true';
  const baseNorm = cfg.baseUrl.replace(/\/$/, '');
  const returnToOauth =
    typeof req.query.return_to === 'string' && req.query.return_to.length
      ? req.query.return_to
      : 'pebblejs://close#';
  const defaultOauthStart = `${baseNorm}/oauth/start?return_to=${encodeURIComponent(returnToOauth)}`;
  let initialTheme = 0;
  const tq = req.query.theme_preset;
  if (typeof tq === 'string' && tq.length) {
    const tn = parseInt(tq, 10);
    if (!Number.isNaN(tn) && tn >= 0 && tn <= 3) {
      initialTheme = tn;
    }
  }
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
        initialTheme
      )
    );
});

app.get('/privacy', (_req, res) => {
  res.type('html').send(
    legalPageHtml(
      'Privacy Policy',
      `
<p>PebbleTasks is a hobby project. This page describes how data is handled.</p>
<h2>What we collect</h2>
<ul>
  <li>Configuration choices you submit in the settings page (for example: local vs Google mode, theme).</li>
  <li>OAuth tokens only when you choose Google mode.</li>
</ul>
<h2>How data is used</h2>
<ul>
  <li>To enable task syncing with Google Tasks when you opt in.</li>
  <li>To return settings and tokens back to the Pebble companion flow.</li>
</ul>
<h2>Data storage</h2>
<ul>
  <li>Primary token storage is intended to be on the phone companion app.</li>
  <li>This server may temporarily hold OAuth payloads during sign-in handoff.</li>
</ul>
<h2>Third-party services</h2>
<p>PebbleTasks depends on third-party services and software (including Google APIs, Rebble/Pebble companion behavior, hosting, DNS, and network providers). Their outages, policy changes, or platform behavior can affect functionality.</p>
<h2>Security limitations</h2>
<p>Reasonable efforts may be used, but no method of transmission or storage is 100% secure. Use at your own risk.</p>
<h2>Google account permissions</h2>
<p>If you connect Google, you can revoke PebbleTasks access at any time from your Google account permissions page.</p>
<h2>Your data ownership</h2>
<p>You retain ownership of your task data. PebbleTasks does not claim ownership of your content.</p>
<h2>No guarantees</h2>
<p>This is an as-is hobby project. No guarantees are made about availability, security, fitness for any purpose, or data retention.</p>
<h2>Policy changes</h2>
<p>This policy may be updated at any time. Continued use after updates means you accept the revised policy.</p>
`
    )
  );
});

app.get('/terms', (_req, res) => {
  res.type('html').send(
    legalPageHtml(
      'Terms of Service',
      `
<p>By using PebbleTasks, you agree to these terms.</p>
<h2>Hobby project</h2>
<p>PebbleTasks is provided as a personal hobby project and may change or stop at any time without notice.</p>
<h2>No liability</h2>
<p>To the maximum extent permitted by law, the developer disclaims all liability for any loss, damage, claim, or consequence arising from use, misuse, inability to use, or reliance on PebbleTasks in any manner whatsoever.</p>
<h2>No warranties</h2>
<p>The service is provided “as is” and “as available”, with no express or implied warranties.</p>
<h2>No affiliation</h2>
<p>PebbleTasks is an independent project and is not affiliated with, endorsed by, or sponsored by Google, Pebble, or Rebble. All trademarks belong to their respective owners.</p>
<h2>Third-party dependency</h2>
<p>Features may rely on third-party platforms, APIs, and network connectivity. These dependencies may fail, change, or be withdrawn at any time.</p>
<h2>No support or service levels</h2>
<p>No service-level agreement, uptime guarantee, update schedule, or support obligation is provided.</p>
<h2>Your responsibility</h2>
<p>You are responsible for how you use the app, your account permissions, and your own backups.</p>
<h2>Changes to terms</h2>
<p>These terms may be updated at any time. Continued use after updates constitutes acceptance of the revised terms.</p>
<h2>Severability</h2>
<p>If any part of these terms is found unenforceable, the remaining terms continue in full effect.</p>
`
    )
  );
});

app.get('/oauth/complete', (req, res) => {
  pruneStates();
  const id = typeof req.query.id === 'string' ? req.query.id : '';
  if (!id) {
    res
      .status(400)
      .type('html')
      .send(
        '<!DOCTYPE html><html lang="en"><head><meta charset="utf-8"/><title>PebbleTasks</title></head><body><p>Missing token id. Open sign-in from <a href="/">PebbleTasks</a> settings, then use Google in the browser.</p></body></html>'
      );
    return;
  }
  const entry = authPickups.get(id);
  if (!entry || !entry.payload) {
    res
      .status(404)
      .type('html')
      .send(
        '<!DOCTYPE html><html lang="en"><head><meta charset="utf-8"/><title>Expired</title></head><body><p>This sign-in page has expired or was already used. Start <strong>Connect / Google</strong> again from PebbleTasks settings in the Ribble app.</p></body></html>'
      );
    return;
  }
  authPickups.delete(id);
  const line = authPayloadToPasteLine(entry.payload);
  res.type('html').send(completeHtml(line));
});

app.post('/oauth/watch-fragment', (req, res) => {
  pruneStates();
  const p = req.body && req.body.payload;
  if (!p || typeof p !== 'object' || !p.access_token) {
    res.status(400).json({ error: 'payload with access_token required' });
    return;
  }
  if (!p.mode) {
    p.mode = 'google';
  }
  const id = crypto.randomBytes(18).toString('hex');
  watchPickups.set(id, { at: Date.now(), payload: p });
  const d = req.body && req.body.d;
  if (typeof d === 'string' && /^[0-9a-f]{32}$/i.test(d)) {
    deviceStash.set(d.toLowerCase(), { at: Date.now(), payload: p });
  }
  res.json({ id });
});

app.get('/oauth/device-pull', (req, res) => {
  pruneStates();
  const did = typeof req.query.d === 'string' ? req.query.d : '';
  if (!/^[0-9a-f]{32}$/i.test(did)) {
    res.status(400).json({ error: 'd required' });
    return;
  }
  const key = did.toLowerCase();
  const entry = deviceStash.get(key);
  if (!entry || !entry.payload) {
    res.status(404).json({ error: 'none' });
    return;
  }
  deviceStash.delete(key);
  res.json(entry.payload);
});

app.get('/oauth/watch-claim', (req, res) => {
  pruneStates();
  const id = typeof req.query.id === 'string' ? req.query.id : '';
  if (!id) {
    res.status(400).json({ error: 'id required' });
    return;
  }
  const entry = watchPickups.get(id);
  if (!entry || !entry.payload) {
    res.status(404).json({ error: 'not_found' });
    return;
  }
  watchPickups.delete(id);
  res.type('json').json(entry.payload);
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
  pruneStates();
  const pickId = crypto.randomBytes(18).toString('hex');
  authPickups.set(pickId, { at: Date.now(), payload });
  const doneUrl = `${cfg.baseUrl}/oauth/complete?id=${encodeURIComponent(pickId)}`;
  res.redirect(302, doneUrl);
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
