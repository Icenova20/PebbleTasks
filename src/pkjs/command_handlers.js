var authStorage = require('./auth_storage');
var localHandlers = require('./command_handlers_local');
var googleHandlers = require('./command_handlers_google');

function dispatchCommand(cmd, listIx, taskIx, text) {
  var a = authStorage.getAuthObject();
  if (a && a.access_token && authStorage.getMode() === 'local') {
    /* Token present but mode never switched (fragment edge case). */
    authStorage.setMode('google');
  }
  if (authStorage.getMode() === 'google') {
    googleHandlers.dispatch(cmd, listIx, taskIx, text);
  } else {
    localHandlers.dispatch(cmd, listIx, taskIx, text);
  }
}

module.exports = {
  dispatchCommand: dispatchCommand,
};
