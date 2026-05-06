var protocol = require('./protocol');
var storage = require('./storage');
var messaging = require('./messaging');
var timeline = require('./timeline');

var DAY_MS = 86400000;

/** Pebble enforces -2d to +1y on pin.time (see timeline public API). */
function isPinInstantWithinPebbleRange(timeIsoZ) {
  var pinT = Date.parse(String(timeIsoZ));
  if (isNaN(pinT)) {
    return false;
  }
  var now = Date.now();
  if (pinT < now - 2 * DAY_MS) {
    return false;
  }
  if (pinT > now + 365 * DAY_MS) {
    return false;
  }
  return true;
}

function dispatch(cmd, listIx, taskIx, text) {
  var d;

  if (cmd === protocol.CMD_W_ASK_LISTS) {
    messaging.replyLists();
  } else if (cmd === protocol.CMD_W_ASK_OPEN) {
    messaging.replyOpenTaskLines(listIx);
  } else if (cmd === protocol.CMD_W_ASK_COMPLETED) {
    messaging.replyCompletedTaskLines(listIx);
  } else if (cmd === protocol.CMD_W_ADD_LIST) {
    var name = storage.normalizeLine(text);
    if (name) {
      d = storage.loadData();
      d.lists.push({ name: name, tasks: [] });
      storage.saveData(d);
    }
    messaging.replyLists();
  } else if (cmd === protocol.CMD_W_ADD_TASK) {
    var taskText = storage.normalizeLine(text);
    if (taskText) {
      d = storage.loadData();
      if (listIx >= 0 && listIx < d.lists.length) {
        if (!Array.isArray(d.lists[listIx].tasks)) {
          d.lists[listIx].tasks = [];
        }
        d.lists[listIx].tasks.push({ text: taskText, open: true });
        storage.saveData(d);
      }
    }
    messaging.replyOpenTaskLines(listIx);
  } else if (cmd === protocol.CMD_W_COMPLETE) {
    d = storage.loadData();
    if (listIx >= 0 && listIx < d.lists.length) {
      var list = d.lists[listIx];
      var open = storage.getOpenTasksInOrder(list);
      if (taskIx >= 0 && taskIx < open.length) {
        var target = open[taskIx];
        target.open = false;
        storage.saveData(d);
      }
    }
    messaging.replyOpenTaskLines(listIx);
  } else if (cmd === protocol.CMD_W_DELETE_TASK) {
    d = storage.loadData();
    if (listIx >= 0 && listIx < d.lists.length) {
      var listD = d.lists[listIx];
      var openD = storage.getOpenTasksInOrder(listD);
      if (taskIx >= 0 && taskIx < openD.length) {
        var target = openD[taskIx];
        for (var j = 0; j < listD.tasks.length; j++) {
          if (listD.tasks[j] === target) {
            listD.tasks.splice(j, 1);
            storage.saveData(d);
            break;
          }
        }
      }
    }
    messaging.replyOpenTaskLines(listIx);
  } else if (cmd === protocol.CMD_W_DELETE_COMPLETED_TASK) {
    d = storage.loadData();
    if (listIx >= 0 && listIx < d.lists.length) {
      var listDc = d.lists[listIx];
      var comp = storage.getCompletedTasksInOrder(listDc);
      if (taskIx >= 0 && taskIx < comp.length) {
        var tgt = comp[taskIx];
        for (var k = 0; k < listDc.tasks.length; k++) {
          if (listDc.tasks[k] === tgt) {
            listDc.tasks.splice(k, 1);
            storage.saveData(d);
            break;
          }
        }
      }
    }
    messaging.replyOpenTaskLines(listIx);
    messaging.replyCompletedTaskLines(listIx);
  } else if (cmd === protocol.CMD_W_UNCOMPLETE_COMPLETED) {
    d = storage.loadData();
    if (listIx >= 0 && listIx < d.lists.length) {
      var listUc = d.lists[listIx];
      var compU = storage.getCompletedTasksInOrder(listUc);
      if (taskIx >= 0 && taskIx < compU.length) {
        compU[taskIx].open = true;
        storage.saveData(d);
      }
    }
    messaging.replyOpenTaskLines(listIx);
    messaging.replyCompletedTaskLines(listIx);
  } else if (cmd === protocol.CMD_W_CLEAR_COMPLETED) {
    d = storage.loadData();
    if (listIx >= 0 && listIx < d.lists.length) {
      var listC = d.lists[listIx];
      if (Array.isArray(listC.tasks)) {
        listC.tasks = listC.tasks.filter(function (t) {
          return t && t.open;
        });
        storage.saveData(d);
      }
    }
    messaging.replyOpenTaskLines(listIx);
    messaging.replyCompletedTaskLines(listIx);
  } else if (cmd === protocol.CMD_W_SET_TASK_DUE) {
    var dueLocal = storage.normalizeLine(text);
    if (/^\d{4}-\d{2}-\d{2}$/.test(dueLocal)) {
      d = storage.loadData();
      if (listIx >= 0 && listIx < d.lists.length) {
        var listDue = d.lists[listIx];
        var openDue = storage.getOpenTasksInOrder(listDue);
        if (taskIx >= 0 && taskIx < openDue.length) {
          openDue[taskIx].due = dueLocal;
          storage.saveData(d);
        }
      }
    }
    messaging.replyOpenTaskLines(listIx);
  } else if (cmd === protocol.CMD_W_CLEAR_TASK_DUE) {
    d = storage.loadData();
    if (listIx >= 0 && listIx < d.lists.length) {
      var listClr = d.lists[listIx];
      var openClr = storage.getOpenTasksInOrder(listClr);
      if (taskIx >= 0 && taskIx < openClr.length) {
        delete openClr[taskIx].due;
        storage.saveData(d);
      }
    }
    messaging.replyOpenTaskLines(listIx);
  } else if (cmd === protocol.CMD_W_DELETE_LIST) {
    d = storage.loadData();
    if (listIx >= 0 && listIx < d.lists.length) {
      d.lists.splice(listIx, 1);
      storage.saveData(d);
    }
    messaging.replyLists();
  } else if (cmd === protocol.CMD_W_PIN_TASK) {
    d = storage.loadData();
    if (listIx < 0 || listIx >= d.lists.length) {
      messaging.replyToast('Task not found');
      return;
    }
    var listPin = d.lists[listIx];
    var openPin = storage.getOpenTasksInOrder(listPin);
    if (taskIx < 0 || taskIx >= openPin.length) {
      messaging.replyToast('Task not found');
      return;
    }
    var taskPin = openPin[taskIx];
    var fromWatch = text != null ? String(text).trim() : '';
    var timeIso = '';
    if (/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$/.test(fromWatch)) {
      timeIso = fromWatch;
    } else {
      var duePin = taskPin && taskPin.due ? storage.extractDueYmd(taskPin.due) : '';
      if (!duePin) {
        messaging.replyToast('Set due first');
        return;
      }
      timeIso = duePin + 'T09:00:00Z';
    }
    if (!isPinInstantWithinPebbleRange(timeIso)) {
      messaging.replyToast('Time out of range');
      return;
    }
    var pinId = timeline.newPinId('l');
    var subtitle = listPin && listPin.name ? String(listPin.name) : '';
    var pin = timeline.buildTaskPin(pinId, taskPin.text, timeIso, subtitle);
    timeline.getTimelineTokenAsync(function (token) {
      if (!token) {
        messaging.replyToast('Timeline not enabled');
        return;
      }
      timeline.pushPin(token, pin, function (ok) {
        if (ok) {
          messaging.replyToast('Pinned');
        } else {
          messaging.replyToast('Timeline error');
        }
      });
    });
  }
}

module.exports = {
  dispatch: dispatch,
};
