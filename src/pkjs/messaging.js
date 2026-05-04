var protocol = require('./protocol');
var storage = require('./storage');

var lineSep = '\n';

/** PebbleKit often delivers tuples as strings; payload keys may be 0 or "0". */
function payloadPick(p, numericKey, namedKey) {
  if (p[numericKey] !== undefined && p[numericKey] !== null) {
    return p[numericKey];
  }
  var s = String(numericKey);
  if (p[s] !== undefined && p[s] !== null) {
    return p[s];
  }
  if (namedKey !== undefined && p[namedKey] !== undefined && p[namedKey] !== null) {
    return p[namedKey];
  }
  return undefined;
}

/** C side TEXT_BUF is 512; AppMessage cstrings are tight on some firmware — stay under this. */
var maxOutboundText = 500;

function sendToWatch(dict) {
  if (dict && dict[3] != null) {
    var t = String(dict[3]);
    if (t.length > maxOutboundText) {
      dict[3] = t.substring(0, maxOutboundText);
    }
  }
  Pebble.sendAppMessage(
    dict,
    function () {},
    function () {}
  );
}

/** Single dark theme on watch (THEME_NUM_PRESETS 1); kept for API compatibility. */
function pushThemePreset(presetId) {
  var n = parseInt(presetId, 10);
  if (isNaN(n) || n !== 0) {
    return;
  }
  var d = {};
  d[protocol.THEME_PRESET] = n;
  d.themePreset = n;
  Pebble.sendAppMessage(d, function () {}, function () {});
}

function replyLists() {
  var d = storage.loadData();
  var lines = [];
  for (var i = 0; i < d.lists.length; i++) {
    lines.push(storage.truncate(d.lists[i].name));
  }
  var payload = { 0: protocol.R_LISTS, 3: lines.join(lineSep) };
  sendToWatch(payload);
}

function replyOpenTaskLines(listIndex) {
  var d = storage.loadData();
  if (listIndex < 0 || listIndex >= d.lists.length) {
    sendToWatch({ 0: protocol.R_OPEN_TASKS, 1: listIndex, 3: '', 4: 0 });
    return;
  }
  var list = d.lists[listIndex];
  var open = storage.getOpenTasksInOrder(list);
  var isoDateRe = /^\d{4}-\d{2}-\d{2}$/;
  var lines = open.map(function (t) {
    var line = storage.truncate(t.text);
    if (t && t.due && isoDateRe.test(String(t.due))) {
      line += '\x1F' + storage.formatDueForWatch(String(t.due));
    }
    return line;
  });
  var hasComp = storage.countCompletedTasks(list) > 0 ? 1 : 0;
  sendToWatch({ 0: protocol.R_OPEN_TASKS, 1: listIndex, 3: lines.join(lineSep), 4: hasComp });
}

/** Explicit payloads for Google mode (no local storage read). */
function replyListsWithText(lineText) {
  sendToWatch({ 0: protocol.R_LISTS, 3: lineText || '' });
}

function replyOpenTaskLinesForList(listIndex, lineText, hasComp) {
  sendToWatch({
    0: protocol.R_OPEN_TASKS,
    1: listIndex,
    3: lineText || '',
    4: hasComp ? 1 : 0,
  });
}

function replyCompletedTaskLines(listIndex) {
  var d = storage.loadData();
  if (listIndex < 0 || listIndex >= d.lists.length) {
    sendToWatch({ 0: protocol.R_COMPLETED_TASKS, 1: listIndex, 3: '' });
    return;
  }
  var list = d.lists[listIndex];
  var comp = storage.getCompletedTasksInOrder(list);
  var lines = comp.map(function (t) {
    return storage.truncate(t.text);
  });
  sendToWatch({ 0: protocol.R_COMPLETED_TASKS, 1: listIndex, 3: lines.join(lineSep) });
}

function replyCompletedTaskLinesForList(listIndex, lineText) {
  sendToWatch({ 0: protocol.R_COMPLETED_TASKS, 1: listIndex, 3: lineText || '' });
}

module.exports = {
  payloadPick: payloadPick,
  sendToWatch: sendToWatch,
  pushThemePreset: pushThemePreset,
  replyLists: replyLists,
  replyOpenTaskLines: replyOpenTaskLines,
  replyListsWithText: replyListsWithText,
  replyOpenTaskLinesForList: replyOpenTaskLinesForList,
  replyCompletedTaskLines: replyCompletedTaskLines,
  replyCompletedTaskLinesForList: replyCompletedTaskLinesForList,
};
