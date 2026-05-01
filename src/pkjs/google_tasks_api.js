/**
 * Google Tasks API v1 — async XMLHttpRequest (open(..., true)) for PebbleKit JS.
 * See https://developer.google.com/workspace/tasks/reference/rest
 */
var API = 'https://tasks.googleapis.com/tasks/v1';

function httpJsonDirectAsync(method, url, accessToken, bodyObj, callback) {
  if (typeof callback !== 'function') {
    return;
  }
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function () {
    if (xhr.readyState !== 4) {
      return;
    }
    var text = xhr.responseText || '';
    var json = null;
    try {
      json = text ? JSON.parse(text) : null;
    } catch (e) {
      /* leave null */
    }
    callback({ status: xhr.status, json: json, raw: text });
  };
  try {
    xhr.open(method, url, true);
    xhr.setRequestHeader('Authorization', 'Bearer ' + accessToken);
    if (bodyObj !== undefined && bodyObj !== null) {
      xhr.setRequestHeader('Content-Type', 'application/json');
      xhr.send(JSON.stringify(bodyObj));
    } else {
      xhr.send();
    }
  } catch (e) {
    callback({ status: 0, json: null, raw: String(e) });
  }
}

function httpJsonAsync(method, url, accessToken, bodyObj, callback) {
  httpJsonDirectAsync(method, url, accessToken, bodyObj, callback);
}

/** PebbleKit JS: localeCompare / ICU options throw RangeError — use plain UTF-16 order. */
function cmpString16(a, b) {
  var s = String(a || '');
  var t = String(b || '');
  if (s < t) {
    return -1;
  }
  if (s > t) {
    return 1;
  }
  return 0;
}

function cmpPosition(a, b) {
  if (a === b) {
    return 0;
  }
  if (!a) {
    return -1;
  }
  if (!b) {
    return 1;
  }
  return cmpString16(a, b);
}

function sortTasksByPosition(items) {
  return items.slice().sort(function (x, y) {
    return cmpPosition(x.position, y.position);
  });
}

function listTaskListsSortedAsync(accessToken, callback) {
  httpJsonAsync('GET', API + '/users/@me/lists', accessToken, null, function (res) {
    if (res.status !== 200 || !res.json) {
      callback([]);
      return;
    }
    var items = res.json.items || [];
    callback(
      items.slice().sort(function (a, b) {
        return cmpString16(a.title, b.title);
      })
    );
  });
}

function taskListIdFromLists(lists, listIndex) {
  if (listIndex < 0 || listIndex >= lists.length) {
    return null;
  }
  return lists[listIndex] && lists[listIndex].id ? lists[listIndex].id : null;
}

function fetchTasksPagesAsync(accessToken, taskListId, showCompleted, callback) {
  var all = [];
  var sc = showCompleted ? 'true' : 'false';
  var numPages = 0;
  function onePage(pageToken) {
    if (numPages >= 10) {
      callback(all);
      return;
    }
    numPages += 1;
    var q =
      '?maxResults=100&showCompleted=' +
      sc +
      '&showHidden=false' +
      (pageToken ? '&pageToken=' + encodeURIComponent(pageToken) : '');
    var url = API + '/lists/' + encodeURIComponent(taskListId) + '/tasks' + q;
    httpJsonAsync('GET', url, accessToken, null, function (res) {
      if (res.status !== 200 || !res.json) {
        callback(all);
        return;
      }
      var items = res.json.items || [];
      for (var i = 0; i < items.length; i += 1) {
        all.push(items[i]);
      }
      var npt = res.json.nextPageToken;
      if (npt) {
        onePage(npt);
      } else {
        callback(all);
      }
    });
  }
  onePage(null);
}

function listOpenTasksSortedAsync(accessToken, taskListId, callback) {
  fetchTasksPagesAsync(accessToken, taskListId, false, function (raw) {
    var open = [];
    for (var i = 0; i < raw.length; i += 1) {
      var t = raw[i];
      if (t && t.status !== 'completed') {
        open.push(t);
      }
    }
    callback(sortTasksByPosition(open));
  });
}

function listCompletedTasksSortedAsync(accessToken, taskListId, callback) {
  fetchTasksPagesAsync(accessToken, taskListId, true, function (raw) {
    var done = [];
    for (var j = 0; j < raw.length; j += 1) {
      var u = raw[j];
      if (u && u.status === 'completed') {
        done.push(u);
      }
    }
    callback(sortTasksByPosition(done));
  });
}

function insertTaskListAsync(accessToken, title, callback) {
  httpJsonAsync('POST', API + '/users/@me/lists', accessToken, { title: title }, callback);
}

function deleteTaskListAsync(accessToken, taskListId, callback) {
  httpJsonAsync(
    'DELETE',
    API + '/users/@me/lists/' + encodeURIComponent(taskListId),
    accessToken,
    null,
    callback
  );
}

function insertTaskAsync(accessToken, taskListId, title, callback) {
  var url = API + '/lists/' + encodeURIComponent(taskListId) + '/tasks';
  httpJsonAsync('POST', url, accessToken, { title: title }, callback);
}

function patchTaskCompleteAsync(accessToken, taskListId, taskId, callback) {
  var url =
    API +
    '/lists/' +
    encodeURIComponent(taskListId) +
    '/tasks/' +
    encodeURIComponent(taskId);
  var now = new Date().toISOString();
  httpJsonAsync('PATCH', url, accessToken, { status: 'completed', completed: now }, callback);
}

function patchTaskNeedsActionAsync(accessToken, taskListId, taskId, callback) {
  var url =
    API +
    '/lists/' +
    encodeURIComponent(taskListId) +
    '/tasks/' +
    encodeURIComponent(taskId);
  httpJsonAsync('PATCH', url, accessToken, { status: 'needsAction' }, callback);
}

function deleteTaskAsync(accessToken, taskListId, taskId, callback) {
  var url =
    API +
    '/lists/' +
    encodeURIComponent(taskListId) +
    '/tasks/' +
    encodeURIComponent(taskId);
  httpJsonAsync('DELETE', url, accessToken, null, callback);
}

function clearCompletedAsync(accessToken, taskListId, callback) {
  var url = API + '/lists/' + encodeURIComponent(taskListId) + '/clear';
  httpJsonAsync('POST', url, accessToken, null, callback);
}

module.exports = {
  listTaskListsSortedAsync: listTaskListsSortedAsync,
  taskListIdFromLists: taskListIdFromLists,
  listOpenTasksSortedAsync: listOpenTasksSortedAsync,
  listCompletedTasksSortedAsync: listCompletedTasksSortedAsync,
  insertTaskListAsync: insertTaskListAsync,
  deleteTaskListAsync: deleteTaskListAsync,
  insertTaskAsync: insertTaskAsync,
  patchTaskCompleteAsync: patchTaskCompleteAsync,
  patchTaskNeedsActionAsync: patchTaskNeedsActionAsync,
  deleteTaskAsync: deleteTaskAsync,
  clearCompletedAsync: clearCompletedAsync,
};
