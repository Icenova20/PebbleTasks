var storageKey = 'pebbletasks_data';
var maxLineLen = 200;

function loadData() {
  var raw = localStorage.getItem(storageKey);
  if (!raw) {
    return { lists: [] };
  }
  try {
    var d = JSON.parse(raw);
    if (d && Array.isArray(d.lists)) {
      return d;
    }
  } catch (e) {
    /* corrupt local storage */
  }
  return { lists: [] };
}

function saveData(data) {
  localStorage.setItem(storageKey, JSON.stringify(data));
}

function normalizeLine(s) {
  if (!s || typeof s !== 'string') {
    return '';
  }
  return s.replace(/[\n\r\x1E\x1F]/g, ' ').trim();
}

function truncate(s) {
  s = String(s);
  if (s.length > maxLineLen) {
    return s.substring(0, maxLineLen - 1) + '…';
  }
  return s;
}

var ShortMonths = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];

/** YYYY-MM-DD → "mon dd, yyyy" for watch subtitle (pkjs only; watch shows string as-is). */
function formatDueForWatch(isoYmd) {
  if (!isoYmd || typeof isoYmd !== 'string') {
    return '';
  }
  var m = /^(\d{4})-(\d{2})-(\d{2})$/.exec(isoYmd);
  if (!m) {
    return isoYmd;
  }
  var year = m[1];
  var mon = parseInt(m[2], 10) - 1;
  var day = parseInt(m[3], 10);
  if (mon < 0 || mon > 11) {
    return isoYmd;
  }
  return ShortMonths[mon] + ' ' + day + ', ' + year;
}

function getOpenTasksInOrder(list) {
  var out = [];
  if (!list || !Array.isArray(list.tasks)) {
    return out;
  }
  for (var i = 0; i < list.tasks.length; i++) {
    var t = list.tasks[i];
    if (t && t.open) {
      out.push(t);
    }
  }
  return out;
}

function countCompletedTasks(list) {
  var n = 0;
  if (!list || !Array.isArray(list.tasks)) {
    return 0;
  }
  for (var i = 0; i < list.tasks.length; i++) {
    var t = list.tasks[i];
    if (t && !t.open) {
      n++;
    }
  }
  return n;
}

function getCompletedTasksInOrder(list) {
  var out = [];
  if (!list || !Array.isArray(list.tasks)) {
    return out;
  }
  for (var i = 0; i < list.tasks.length; i++) {
    var t = list.tasks[i];
    if (t && !t.open) {
      out.push(t);
    }
  }
  return out;
}

module.exports = {
  loadData: loadData,
  saveData: saveData,
  normalizeLine: normalizeLine,
  truncate: truncate,
  formatDueForWatch: formatDueForWatch,
  getOpenTasksInOrder: getOpenTasksInOrder,
  getCompletedTasksInOrder: getCompletedTasksInOrder,
  countCompletedTasks: countCompletedTasks,
};
