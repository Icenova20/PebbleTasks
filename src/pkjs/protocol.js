// Mirror src/c/protocol.h and package.json message keys

/** Matches package.json messageKeys.themePreset */
module.exports = {
  THEME_PRESET: 5,

  CMD_W_ASK_LISTS: 1,
  CMD_W_ASK_OPEN: 2,
  CMD_W_ADD_LIST: 3,
  CMD_W_ADD_TASK: 4,
  CMD_W_COMPLETE: 5,
  CMD_W_DELETE_TASK: 6,
  CMD_W_CLEAR_COMPLETED: 7,
  CMD_W_DELETE_LIST: 8,
  CMD_W_ASK_COMPLETED: 9,
  CMD_W_DELETE_COMPLETED_TASK: 10,
  CMD_W_UNCOMPLETE_COMPLETED: 11,

  R_LISTS: 20,
  R_OPEN_TASKS: 21,
  R_COMPLETED_TASKS: 22,
};
