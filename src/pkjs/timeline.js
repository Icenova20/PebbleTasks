/**
 * Pebble timeline web API client (per-user pins only — no shared pins).
 * https://developer.repebble.com/guides/pebble-timeline/timeline-public/
 *
 * Requires the app's UUID to be timeline-enabled in the Rebble Developer
 * Dashboard. Without that, Pebble.getTimelineToken() returns an error.
 */

var BASE_URL = 'https://timeline-api.getpebble.com/v1/user/pins/';
var TITLE_MAX = 64;

/** Wraps Pebble.getTimelineToken; cb(token | null, errOrUndefined). */
function getTimelineTokenAsync(cb) {
  if (typeof cb !== 'function') {
    return;
  }
  if (!Pebble || typeof Pebble.getTimelineToken !== 'function') {
    setTimeout(function () {
      cb(null, 'unsupported');
    }, 0);
    return;
  }
  try {
    Pebble.getTimelineToken(
      function (token) {
        cb(token || null);
      },
      function (err) {
        cb(null, err || 'token error');
      }
    );
  } catch (e) {
    setTimeout(function () {
      cb(null, 'token throw');
    }, 0);
  }
}

/**
 * Build a generic-pin payload for a task.
 * `timeIsoUtc` is a full timeline instant, e.g. 2015-03-19T15:00:00Z (from the
 * watch wizard) or a legacy due-day anchor.
 */
function buildTaskPin(pinId, title, timeIsoUtc, subtitle) {
  var safeTitle = String(title || '').substring(0, TITLE_MAX);
  return {
    id: String(pinId),
    time: String(timeIsoUtc),
    layout: {
      type: 'genericPin',
      title: safeTitle,
      subtitle: String(subtitle || ''),
      tinyIcon: 'system://images/SCHEDULED_EVENT',
    },
  };
}

function sendPinRequest(method, token, pinId, body, cb) {
  if (typeof cb !== 'function') {
    cb = function () {};
  }
  if (!token) {
    setTimeout(function () {
      cb(false, 'no token');
    }, 0);
    return;
  }
  if (!pinId) {
    setTimeout(function () {
      cb(false, 'no pin id');
    }, 0);
    return;
  }
  var url = BASE_URL + encodeURIComponent(String(pinId));
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function () {
    if (xhr.readyState !== 4) {
      return;
    }
    var ok = xhr.status >= 200 && xhr.status < 300;
    cb(ok, ok ? xhr.status : xhr.status + ' ' + (xhr.responseText || ''));
  };
  try {
    xhr.open(method, url, true);
    xhr.setRequestHeader('Content-Type', 'application/json');
    xhr.setRequestHeader('X-User-Token', String(token));
    xhr.send(body || null);
  } catch (e) {
    setTimeout(function () {
      cb(false, 'send throw');
    }, 0);
  }
}

/** PUT pin (create or update by id). cb(ok, statusOrErr). */
function pushPin(token, pin, cb) {
  if (!pin || !pin.id) {
    if (typeof cb === 'function') {
      setTimeout(function () {
        cb(false, 'invalid pin');
      }, 0);
    }
    return;
  }
  sendPinRequest('PUT', token, pin.id, JSON.stringify(pin), cb);
}

/**
 * Pin ids must be unique and ≤64 chars. Pebble forbids reusing a deleted id, so
 * mix Date.now() with a random base36 chunk. modeKey is one of: 'l', 'g'.
 */
function newPinId(modeKey) {
  var t = Date.now().toString(36);
  var r = Math.floor(Math.random() * 0x80000000).toString(36);
  var id = 'pt-' + String(modeKey || 'x') + '-' + t + '-' + r;
  return id.length > 64 ? id.substring(0, 64) : id;
}

module.exports = {
  getTimelineTokenAsync: getTimelineTokenAsync,
  buildTaskPin: buildTaskPin,
  pushPin: pushPin,
  newPinId: newPinId,
};
