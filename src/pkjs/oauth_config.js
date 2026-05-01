/**
 * Pebble app settings: base URL of the OAuth server (settings + token refresh in Rebble).
 * Do not use http://127.0.0.1 or http://localhost on the phone — that is the device itself.
 *
 * TEMPORARY: paste the full line from “Google sign-in complete” (starts with PEBBLETASKS1),
 * or paste JSON with access_token — same as “Paste auth token” on settingsBaseUrl. Then rebuild.
 * Leave '' for normal stored OAuth only. Remove before shipping.
 */
module.exports = {
  settingsBaseUrl: 'https://pebbletasks.shansrini.com',
  hardcodedAccessToken: '',
};
