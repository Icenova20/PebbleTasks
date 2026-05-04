var authStorage = require('./auth_storage');
var localHandlers = require('./command_handlers_local');
var googleHandlers = require('./command_handlers_google');

function dispatchCommand(cmd, listIx, taskIx, text) {
  if (authStorage.getMode() === 'google') {
    googleHandlers.dispatch(cmd, listIx, taskIx, text);
  } else {
    localHandlers.dispatch(cmd, listIx, taskIx, text);
  }
}

module.exports = {
  dispatchCommand: dispatchCommand,
};
