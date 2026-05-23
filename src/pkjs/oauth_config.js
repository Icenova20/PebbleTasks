/**
 * Pebble app settings: base URL of the OAuth server (settings + token refresh in Rebble).
 * Do not use http://127.0.0.1 or http://localhost on the phone — that is the device itself.
 *
 * Local testing: `npm run decode-token -- 'PEBBLETASKS1…'` writes oauth_config.local.js (gitignored), then rebuild.
 * Leave hardcodedAccessToken '' for normal stored OAuth only. Remove local file before shipping.
 */
var base = {
  settingsBaseUrl: 'https://pebbletasks.shansrini.com',
  hardcodedAccessToken: '',
};

try {
  var local = require('./oauth_config.local');
  if (local && typeof local === 'object') {
    if (typeof local.settingsBaseUrl === 'string' && local.settingsBaseUrl.length) {
      base.settingsBaseUrl = local.settingsBaseUrl;
    }
    if (typeof local.hardcodedAccessToken === 'string' && local.hardcodedAccessToken.length) {
      base.hardcodedAccessToken = local.hardcodedAccessToken;
    }
  }
} catch (e) {
  /* oauth_config.local.js not present — normal production build */
}

module.exports = base;
