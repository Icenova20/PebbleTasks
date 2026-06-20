var oauthConfig = require('./oauth_config');

var MODE_KEY = 'pebbletasks_mode';
var AUTH_KEY = 'pebbletasks_google_auth';
var AUTO_TIMELINE_KEY = 'pebbletasks_auto_timeline';

/** PebbleKit JS may not define `atob`; payload is ASCII JSON after decode. */
function base64DecodeToBinaryString(b64) {
  var alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=';
  var input = String(b64).replace(/[^A-Za-z0-9+/=]/g, '');
  var out = '';
  var bc = 0;
  var bs = 0;
  var buffer;
  var idx = 0;
  while ((buffer = input.charAt(idx++))) {
    buffer = alphabet.indexOf(buffer);
    if (buffer === -1) {
      continue;
    }
    bs = bc % 4 ? bs * 64 + buffer : buffer;
    if (bc++ % 4) {
      out += String.fromCharCode(255 & (bs >> ((-2 * bc) & 6)));
    }
  }
  return out;
}

function decodePebbleTasksPayloadBase64(b64) {
  var txt;
  try {
    txt = typeof atob === 'function' ? atob(b64) : base64DecodeToBinaryString(b64);
  } catch (e) {
    return null;
  }
  try {
    return JSON.parse(txt);
  } catch (e2) {
    return null;
  }
}

/** Same shapes as oauth-server parsePastedAuthLineToPayload — PEBBLETASKS1 + base64(JSON) or raw JSON. */
function parsePastedAuthLine(s) {
  s = typeof s === 'string' ? s.replace(/^\s+|\s+$/g, '') : '';
  if (!s) {
    return null;
  }
  var PREFIX = 'PEBBLETASKS1';
  if (s.indexOf(PREFIX) === 0) {
    return decodePebbleTasksPayloadBase64(s.slice(PREFIX.length));
  }
  try {
    return JSON.parse(s);
  } catch (a) {
    /* */
  }
  try {
    return JSON.parse(decodeURIComponent(s));
  } catch (b) {
    return null;
  }
}

/**
 * @returns {{ access_token: string, refresh_token: string | null, expires_at_ms: number } | null}
 */
function authObjectFromHardcoded(raw) {
  if (typeof raw !== 'string' || !raw.length) {
    return null;
  }
  var o = parsePastedAuthLine(raw);
  if (o && o.access_token) {
    var rt = o.refresh_token ? String(o.refresh_token) : null;
    /*
     * OAuth `expires_in` is relative to token issue time, which we do not have in a static paste.
     * Treating Date.now()+expires_in wrongly marks stale access tokens as “still fresh” and skips
     * refresh — Google returns 401 and list APIs yield empty. If we have a refresh_token, omit
     * expires_at_ms so the client refreshes before first use.
     */
    var expMs;
    if (rt) {
      expMs = undefined;
    } else if (o.expires_in != null && !isNaN(Number(o.expires_in))) {
      expMs = Date.now() + Number(o.expires_in) * 1000;
    } else {
      expMs = Date.now() + 10 * 365 * 24 * 60 * 60 * 1000;
    }
    return {
      access_token: String(o.access_token),
      refresh_token: rt,
      expires_at_ms: expMs,
    };
  }
  var plain = raw.replace(/^\s+|\s+$/g, '');
  if (plain.length) {
    return {
      access_token: plain,
      refresh_token: null,
      expires_at_ms: Date.now() + 10 * 365 * 24 * 60 * 60 * 1000,
    };
  }
  return null;
}

function getMode() {
  var m = localStorage.getItem(MODE_KEY);
  if (m === 'google') {
    return 'google';
  }
  return 'local';
}

function setMode(mode) {
  if (mode === 'google' || mode === 'local') {
    localStorage.setItem(MODE_KEY, mode);
  }
}

function getAutoTimeline() {
  return localStorage.getItem(AUTO_TIMELINE_KEY) === '1';
}

function setAutoTimeline(val) {
  if (val) {
    localStorage.setItem(AUTO_TIMELINE_KEY, '1');
  } else {
    localStorage.setItem(AUTO_TIMELINE_KEY, '0');
  }
}

function getAuthObject() {
  var hc = oauthConfig.hardcodedAccessToken;
  if (typeof hc === 'string' && hc.length > 0) {
    var hard = authObjectFromHardcoded(hc);
    if (hard) {
      return hard;
    }
  }
  var raw = localStorage.getItem(AUTH_KEY);
  if (!raw) {
    return null;
  }
  try {
    var o = JSON.parse(raw);
    if (o && o.access_token) {
      return o;
    }
  } catch (e) {
    /* invalid auth JSON in storage */
  }
  return null;
}

function setAuthObject(o) {
  if (!o) {
    localStorage.removeItem(AUTH_KEY);
    return;
  }
  localStorage.setItem(AUTH_KEY, JSON.stringify(o));
}

function clearGoogleAuth() {
  localStorage.removeItem(AUTH_KEY);
}

/**
 * Returns a valid access token only if it is not expired (no network).
 * After refresh, this returns the new token — use getValidAccessTokenAsync to refresh.
 */
function getValidAccessToken() {
  var auth = getAuthObject();
  if (!auth || !auth.access_token) {
    return null;
  }
  var now = Date.now();
  var skew = 60000;
  if (auth.expires_at_ms && now < auth.expires_at_ms - skew) {
    return auth.access_token;
  }
  return null;
}

/**
 * Resolves a usable access token, refreshing via async POST /oauth/refresh when needed.
 * Refreshes with async POST /oauth/refresh (sync XHR is unreliable in Rebble).
 * @param {function (string | null) : void} callback
 */
function getValidAccessTokenAsync(callback) {
  if (typeof callback !== 'function') {
    return;
  }
  var auth = getAuthObject();
  if (!auth || !auth.access_token) {
    setTimeout(function () {
      callback(null);
    }, 0);
    return;
  }
  var now = Date.now();
  var skew = 60000;
  if (auth.expires_at_ms && now < auth.expires_at_ms - skew) {
    setTimeout(function () {
      callback(auth.access_token);
    }, 0);
    return;
  }
  if (!auth.refresh_token) {
    setTimeout(function () {
      callback(auth.access_token);
    }, 0);
    return;
  }
  var url = oauthConfig.settingsBaseUrl.replace(/\/$/, '') + '/oauth/refresh';
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function () {
    if (xhr.readyState !== 4) {
      return;
    }
    if (xhr.status !== 200) {
      callback(null);
      return;
    }
    var body;
    try {
      body = JSON.parse(xhr.responseText);
    } catch (e) {
      callback(null);
      return;
    }
    if (!body.access_token) {
      callback(null);
      return;
    }
    var expiresIn = body.expires_in || 3600;
    var next = {
      access_token: body.access_token,
      refresh_token: auth.refresh_token,
      expires_at_ms: Date.now() + expiresIn * 1000,
    };
    setAuthObject(next);
    callback(next.access_token);
  };
  try {
    xhr.open('POST', url, true);
    xhr.setRequestHeader('Content-Type', 'application/json');
    xhr.send(JSON.stringify({ refresh_token: auth.refresh_token }));
  } catch (e) {
    setTimeout(function () {
      callback(null);
    }, 0);
  }
}

module.exports = {
  getMode: getMode,
  setMode: setMode,
  getAuthObject: getAuthObject,
  setAuthObject: setAuthObject,
  clearGoogleAuth: clearGoogleAuth,
  getValidAccessToken: getValidAccessToken,
  getValidAccessTokenAsync: getValidAccessTokenAsync,
  getAutoTimeline: getAutoTimeline,
  setAutoTimeline: setAutoTimeline,
};
