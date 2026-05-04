var messaging = require('./messaging');
var commandHandlers = require('./command_handlers');
var oauthConfig = require('./oauth_config');
var authStorage = require('./auth_storage');

function applyConfigObject(cfg) {
  if (!cfg) {
    return;
  }
  if (cfg.themePreset !== undefined && cfg.themePreset !== null) {
    var tp = parseInt(cfg.themePreset, 10);
    if (!isNaN(tp) && (tp === 0 || tp === 1)) {
      authStorage.setThemePreset(tp);
      messaging.pushThemePreset(tp);
    }
  }
  if (cfg.access_token) {
    var ex = cfg.expires_in || 3600;
    authStorage.setAuthObject({
      access_token: cfg.access_token,
      refresh_token: cfg.refresh_token || null,
      expires_at_ms: Date.now() + ex * 1000,
    });
    authStorage.setMode('google');
  } else if (cfg.mode === 'local' || cfg.mode === 'google') {
    authStorage.setMode(cfg.mode);
  }
}

Pebble.addEventListener('ready', function () {
  var hcRaw = oauthConfig.hardcodedAccessToken;
  var hc =
    typeof hcRaw === 'string' ? hcRaw.replace(/^\s+|\s+$/g, '') : '';
  if (hc.length > 0 && authStorage.getAuthObject()) {
    authStorage.setMode('google');
  }
  messaging.pushThemePreset(authStorage.getThemePreset());
});

function settingsUrlUnreachableFromPhone(url) {
  if (!url || typeof url !== 'string') {
    return true;
  }
  var u = url.replace(/\/$/, '').toLowerCase();
  if (u === '') {
    return true;
  }
  if (u.indexOf('http://127.0.0.1') === 0 || u.indexOf('http://localhost') === 0) {
    return true;
  }
  return false;
}

/** Opens when oauth URL is missing or points at localhost (broken from the phone). */
function openSettingsUrlHelpPage() {
  var html =
    '<!DOCTYPE html><html><head><meta charset="utf-8"/>' +
    '<meta name="viewport" content="width=device-width,initial-scale=1"/>' +
    '<style>html,body{background:#fff;color:#111;margin:0;padding:16px;font:16px Helvetica,Arial,sans-serif}' +
    'code{font-size:14px;word-break:break-all}</style></head><body>' +
    '<h1 style="font-size:18px">PebbleTasks settings</h1>' +
    '<p><strong>This page is blank or black</strong> when the URL points at <code>127.0.0.1</code> or ' +
    '<code>localhost</code> — on your phone that is the phone itself, not your PC.</p>' +
    '<p>Edit <code>src/pkjs/oauth_config.js</code>: set <code>settingsBaseUrl</code> to the machine ' +
    'running the oauth-server, using its <strong>LAN IP</strong>, e.g. ' +
    '<code>http://192.168.1.10:3847</code>, then rebuild and install the app. For production use HTTPS.</p>' +
    '<p>Android may block HTTP to non-localhost; if the page still fails, use HTTPS or allow cleartext for the dev app.</p>' +
    '</body></html>';
  Pebble.openURL('data:text/html;charset=utf-8,' + encodeURIComponent(html));
}

/**
 * PebbleKit does not pass return_to on the showConfiguration event object.
 * Config pages must use a default close URL; see Rebble app configuration guide.
 * https://developer.rebble.io/guides/user-interfaces/app-configuration-static/
 */
function getConfigurationReturnTo(e) {
  var def = 'pebblejs://close#';
  if (!e || typeof e !== 'object') {
    return def;
  }
  if (e.returnTo != null && String(e.returnTo).length) {
    return String(e.returnTo);
  }
  if (e.return_to != null && String(e.return_to).length) {
    return String(e.return_to);
  }
  if (e.data) {
    if (e.data.returnTo != null && String(e.data.returnTo).length) {
      return String(e.data.returnTo);
    }
    if (e.data.return_to != null && String(e.data.return_to).length) {
      return String(e.data.return_to);
    }
  }
  return def;
}

Pebble.addEventListener('showConfiguration', function (e) {
  var base = (oauthConfig.settingsBaseUrl || '').replace(/\/$/, '');
  if (settingsUrlUnreachableFromPhone(base)) {
    openSettingsUrlHelpPage();
    return;
  }
  var ret = getConfigurationReturnTo(e);
  var modeQ = 'current_mode=' + encodeURIComponent(authStorage.getMode() === 'google' ? 'google' : 'local');
  var auth = authStorage.getAuthObject();
  var hasGoogleAuth = !!(auth && (auth.access_token || auth.refresh_token));
  var signedQ = 'signed_in=' + (hasGoogleAuth ? '1' : '0');
  var themeQ = 'theme_preset=' + encodeURIComponent(String(authStorage.getThemePreset()));
  var startUrl =
    base +
    '/?return_to=' +
    encodeURIComponent(ret) +
    '&' +
    modeQ +
    '&' +
    signedQ +
    '&' +
    themeQ;
  Pebble.openURL(startUrl);
});

/**
 * Fragment from config close URL (return_to + encodeURIComponent(JSON)).
 * https://developer.rebble.io/guides/user-interfaces/app-configuration-static/
 */
function tryParseConfigFragment(s) {
  s = typeof s === 'string' ? s : String(s);
  if (s.length === 0) {
    return null;
  }
  try {
    return JSON.parse(decodeURIComponent(s));
  } catch (a) {
    /* Some runtimes hand back decoded JSON. */
  }
  try {
    return JSON.parse(s);
  } catch (b) {
    return null;
  }
}

/** Some Pebble/iOS builds pass the full close URL; only the part after # is JSON. */
function normalizeWebviewConfigResponse(raw) {
  var s = typeof raw === 'string' ? raw : String(raw);
  if (s.length === 0) {
    return s;
  }
  var hash = s.indexOf('#');
  if (hash >= 0) {
    s = s.substring(hash + 1);
  }
  return s;
}

Pebble.addEventListener('webviewclosed', function (e) {
  if (e && e.response === -1) {
    return;
  }
  if (!e || e.response === undefined || e.response === null) {
    return;
  }
  try {
    var s = typeof e.response === 'string' ? e.response : String(e.response);
    s = normalizeWebviewConfigResponse(s);
    if (s.length === 0) {
      return;
    }
    var cfg = tryParseConfigFragment(s);
    if (!cfg) {
      return;
    }
    applyConfigObject(cfg);
  } catch (err) {
    /* ignore */
  }
});

Pebble.addEventListener('appmessage', function (e) {
  var p = e.payload;
  if (!p) {
    return;
  }
  var cmdRaw = messaging.payloadPick(p, 0, 'cmd');
  if (cmdRaw === undefined || cmdRaw === null) {
    return;
  }
  var cmd = Number(cmdRaw);
  if (isNaN(cmd)) {
    return;
  }

  var listIx = Number(messaging.payloadPick(p, 1, 'listIndex'));
  if (isNaN(listIx)) {
    listIx = 0;
  }
  var taskIx = Number(messaging.payloadPick(p, 2, 'taskIndex'));
  if (isNaN(taskIx)) {
    taskIx = 0;
  }
  var textRaw = messaging.payloadPick(p, 3, 'text');
  var text = textRaw !== undefined && textRaw !== null ? String(textRaw) : '';

  commandHandlers.dispatchCommand(cmd, listIx, taskIx, text);
});
