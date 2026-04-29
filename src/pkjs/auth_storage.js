var oauthConfig = require('./oauth_config');

var MODE_KEY = 'pebbletasks_mode';
var AUTH_KEY = 'pebbletasks_google_auth';

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

function getAuthObject() {
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
      callback(null);
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
};
