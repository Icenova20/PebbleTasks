var protocol = require('./protocol');
var api = require('./google_tasks_api');
var auth = require('./auth_storage');
var messaging = require('./messaging');
var storage = require('./storage');
var timeline = require('./timeline');

var maxListNameForMenu = 64;
var DAY_MS = 86400000;

/** Pebble enforces -2d to +1y on pin.time. */
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

function linesFromListArray(lists) {
  var maxChars = messaging.maxOutboundText;
  var maxLines = protocol.MAX_MENU_LINES;
  var out = [];
  var used = 0;
  for (var i = 0; i < lists.length && out.length < maxLines; i += 1) {
    var title = (lists[i] && (lists[i].title || '')) || '';
    title = storage.truncate(String(title).trim());
    if (!title.length) {
      title = '(untitled list)';
    }
    if (title.length > maxListNameForMenu) {
      title = title.substring(0, maxListNameForMenu - 1) + '…';
    }
    var addLen = title.length + (out.length > 0 ? 1 : 0);
    if (used + addLen > maxChars) {
      break;
    }
    out.push(title);
    used += addLen;
  }
  return out.join('\n');
}

function openAndFlagAsync(accessToken, listIx, done) {
  api.listTaskListsAsync(accessToken, function (lists) {
    var tlId = api.taskListIdFromLists(lists, listIx);
    if (!tlId) {
      done({ text: '', hasComp: 0 });
      return;
    }
    api.listOpenTasksSortedAsync(accessToken, tlId, function (open) {
      var isListTimelineEnabled = localStorage.getItem('timeline_sync_' + tlId) === '1';
      if (isListTimelineEnabled) {
        timeline.getTimelineTokenAsync(function (timelineToken) {
          if (timelineToken) {
            var listName = '';
            for (var li = 0; li < lists.length; li++) {
              if (api.taskListIdFromLists(lists, li) === tlId) {
                listName = (lists[li] && lists[li].title) || '';
                break;
              }
            }
            open.forEach(function (taskP) {
              if (taskP && taskP.due) {
                var dueP = storage.extractDueYmd(taskP.due);
                if (dueP) {
                  var timeIso = dueP + 'T09:00:00Z';
                  var pinId = timeline.newPinId('g');
                  var pin = timeline.buildTaskPin(pinId, taskP.title || '', timeIso, listName);
                  timeline.pushPin(timelineToken, pin);
                }
              }
            });
          }
        });
      }

      var lineStr = open
        .map(function (t) {
          var title = storage.truncate(t.title || '');
          if (t && t.due) {
            var day = storage.extractDueYmd(t.due);
            if (day) {
              title += '\x1F' + day + '\x1F' + storage.formatDueForWatch(day);
            }
          }
          return title;
        })
        .join('\n');
      api.listCompletedTasksSortedAsync(accessToken, tlId, function (comp) {
        done({ text: lineStr, hasComp: comp.length > 0 ? 1 : 0 });
      });
    });
  });
}

function completedLinesAsync(accessToken, listIx, done) {
  api.listTaskListsAsync(accessToken, function (lists) {
    var tlId = api.taskListIdFromLists(lists, listIx);
    if (!tlId) {
      done('');
      return;
    }
    api.listCompletedTasksSortedAsync(accessToken, tlId, function (comp) {
      done(
        comp
          .map(function (t) {
            return storage.truncate(t.title || '');
          })
          .join('\n')
      );
    });
  });
}

function replyEmptyForCmd(cmd, listIx) {
  if (cmd === protocol.CMD_W_ASK_LISTS) {
    messaging.replyListsWithText('');
  } else if (cmd === protocol.CMD_W_ASK_OPEN) {
    messaging.replyOpenTaskLinesForList(listIx, '', 0);
  } else if (cmd === protocol.CMD_W_ASK_COMPLETED) {
    messaging.replyCompletedTaskLinesForList(listIx, '');
  } else if (cmd === protocol.CMD_W_UNCOMPLETE_COMPLETED) {
    messaging.replyOpenTaskLinesForList(listIx, '', 0);
    messaging.replyCompletedTaskLinesForList(listIx, '');
  } else if (cmd === protocol.CMD_W_SET_TASK_DUE || cmd === protocol.CMD_W_CLEAR_TASK_DUE) {
    messaging.replyOpenTaskLinesForList(listIx, '', 0);
  } else if (cmd === protocol.CMD_W_PIN_TASK || cmd === protocol.CMD_W_SET_TIMELINE_SYNC) {
    messaging.replyToast('Sign in to Google');
  } else {
    messaging.replyListsWithText('');
    messaging.replyOpenTaskLinesForList(listIx, '', 0);
  }
}

function dispatch(cmd, listIx, taskIx, text) {
  auth.getValidAccessTokenAsync(function (token) {
    if (!token) {
      if (cmd === protocol.CMD_W_ASK_LISTS) {
        messaging.replyListsWithText("Test List 1\nTest List 2\nTest List 3");
      } else {
        replyEmptyForCmd(cmd, listIx);
      }
      return;
    }

    if (cmd === protocol.CMD_W_ASK_LISTS) {
      api.listTaskListsAsync(token, function (lists) {
        messaging.replyListsWithText(linesFromListArray(lists));
      });
      return;
    }
    if (cmd === protocol.CMD_W_SET_TIMELINE_SYNC) {
      api.listTaskListsAsync(token, function (lists) {
        var tlId = api.taskListIdFromLists(lists, listIx);
        if (tlId) {
          localStorage.setItem('timeline_sync_' + tlId, text);
        }
      });
      return;
    }
    if (cmd === protocol.CMD_W_ASK_OPEN) {
      openAndFlagAsync(token, listIx, function (o0) {
        messaging.replyOpenTaskLinesForList(listIx, o0.text, o0.hasComp);
      });
      return;
    }
    if (cmd === protocol.CMD_W_ASK_COMPLETED) {
      completedLinesAsync(token, listIx, function (lines) {
        messaging.replyCompletedTaskLinesForList(listIx, lines);
      });
      return;
    }

    if (cmd === protocol.CMD_W_ADD_LIST) {
      var name = storage.normalizeLine(text);
      var afterAdd = function () {
        var t2 = auth.getValidAccessToken() || token;
        api.listTaskListsAsync(t2, function (lists) {
          messaging.replyListsWithText(linesFromListArray(lists));
        });
      };
      if (name) {
        api.insertTaskListAsync(token, name, function () {
          afterAdd();
        });
        return;
      }
      afterAdd();
      return;
    }

    if (cmd === protocol.CMD_W_DELETE_LIST) {
      api.listTaskListsAsync(token, function (lists) {
        var tlDel = api.taskListIdFromLists(lists, listIx);
        if (tlDel) {
          api.deleteTaskListAsync(token, tlDel, function () {
            var t2 = auth.getValidAccessToken() || token;
            api.listTaskListsAsync(t2, function (lists2) {
              messaging.replyListsWithText(linesFromListArray(lists2));
            });
          });
          return;
        }
        var t3 = auth.getValidAccessToken() || token;
        api.listTaskListsAsync(t3, function (lists3) {
          messaging.replyListsWithText(linesFromListArray(lists3));
        });
      });
      return;
    }

    api.listTaskListsAsync(token, function (lists) {
      var tlId = api.taskListIdFromLists(lists, listIx);
      var t = auth.getValidAccessToken() || token;

      if (cmd === protocol.CMD_W_ADD_TASK) {
        var taskText = storage.normalizeLine(text);
        if (taskText && tlId) {
          api.insertTaskAsync(t, tlId, taskText, function () {
            var t2 = auth.getValidAccessToken() || token;
            openAndFlagAsync(t2, listIx, function (o1) {
              messaging.replyOpenTaskLinesForList(listIx, o1.text, o1.hasComp);
            });
          });
        } else {
          openAndFlagAsync(t, listIx, function (o1) {
            messaging.replyOpenTaskLinesForList(listIx, o1.text, o1.hasComp);
          });
        }
        return;
      }

      if (cmd === protocol.CMD_W_COMPLETE) {
        if (!tlId) {
          openAndFlagAsync(t, listIx, function (o2) {
            messaging.replyOpenTaskLinesForList(listIx, o2.text, o2.hasComp);
          });
          return;
        }
        api.listOpenTasksSortedAsync(t, tlId, function (openT) {
          if (taskIx < 0 || taskIx >= openT.length) {
            openAndFlagAsync(t, listIx, function (o2) {
              messaging.replyOpenTaskLinesForList(listIx, o2.text, o2.hasComp);
            });
            return;
          }
          api.patchTaskCompleteAsync(t, tlId, openT[taskIx].id, function () {
            var t2 = auth.getValidAccessToken() || token;
            openAndFlagAsync(t2, listIx, function (o2) {
              messaging.replyOpenTaskLinesForList(listIx, o2.text, o2.hasComp);
            });
          });
        });
        return;
      }

      if (cmd === protocol.CMD_W_DELETE_TASK) {
        if (!tlId) {
          openAndFlagAsync(t, listIx, function (o3) {
            messaging.replyOpenTaskLinesForList(listIx, o3.text, o3.hasComp);
          });
          return;
        }
        api.listOpenTasksSortedAsync(t, tlId, function (openD) {
          if (taskIx < 0 || taskIx >= openD.length) {
            openAndFlagAsync(t, listIx, function (o3) {
              messaging.replyOpenTaskLinesForList(listIx, o3.text, o3.hasComp);
            });
            return;
          }
          api.deleteTaskAsync(t, tlId, openD[taskIx].id, function () {
            var t2 = auth.getValidAccessToken() || token;
            openAndFlagAsync(t2, listIx, function (o3) {
              messaging.replyOpenTaskLinesForList(listIx, o3.text, o3.hasComp);
            });
          });
        });
        return;
      }

      if (cmd === protocol.CMD_W_CLEAR_COMPLETED) {
        if (tlId) {
          api.clearCompletedAsync(t, tlId, function () {
            var t2 = auth.getValidAccessToken() || token;
            openAndFlagAsync(t2, listIx, function (o4) {
              completedLinesAsync(t2, listIx, function (compText) {
                messaging.replyOpenTaskLinesForList(listIx, o4.text, o4.hasComp);
                messaging.replyCompletedTaskLinesForList(listIx, compText);
              });
            });
          });
        } else {
          openAndFlagAsync(t, listIx, function (o4) {
            completedLinesAsync(t, listIx, function (compText) {
              messaging.replyOpenTaskLinesForList(listIx, o4.text, o4.hasComp);
              messaging.replyCompletedTaskLinesForList(listIx, compText);
            });
          });
        }
        return;
      }

      if (cmd === protocol.CMD_W_DELETE_COMPLETED_TASK) {
        if (!tlId) {
          openAndFlagAsync(t, listIx, function (o5) {
            completedLinesAsync(t, listIx, function (compL) {
              messaging.replyOpenTaskLinesForList(listIx, o5.text, o5.hasComp);
              messaging.replyCompletedTaskLinesForList(listIx, compL);
            });
          });
          return;
        }
        api.listCompletedTasksSortedAsync(t, tlId, function (compT) {
          if (taskIx < 0 || taskIx >= compT.length) {
            openAndFlagAsync(t, listIx, function (o5) {
              completedLinesAsync(t, listIx, function (compL) {
                messaging.replyOpenTaskLinesForList(listIx, o5.text, o5.hasComp);
                messaging.replyCompletedTaskLinesForList(listIx, compL);
              });
            });
            return;
          }
          api.deleteTaskAsync(t, tlId, compT[taskIx].id, function () {
            var t2 = auth.getValidAccessToken() || token;
            openAndFlagAsync(t2, listIx, function (o5) {
              completedLinesAsync(t2, listIx, function (compL) {
                messaging.replyOpenTaskLinesForList(listIx, o5.text, o5.hasComp);
                messaging.replyCompletedTaskLinesForList(listIx, compL);
              });
            });
          });
        });
        return;
      }

      if (cmd === protocol.CMD_W_UNCOMPLETE_COMPLETED) {
        if (!tlId) {
          openAndFlagAsync(t, listIx, function (o5) {
            completedLinesAsync(t, listIx, function (compL) {
              messaging.replyOpenTaskLinesForList(listIx, o5.text, o5.hasComp);
              messaging.replyCompletedTaskLinesForList(listIx, compL);
            });
          });
          return;
        }
        api.listCompletedTasksSortedAsync(t, tlId, function (compT) {
          if (taskIx < 0 || taskIx >= compT.length) {
            openAndFlagAsync(t, listIx, function (o5) {
              completedLinesAsync(t, listIx, function (compL) {
                messaging.replyOpenTaskLinesForList(listIx, o5.text, o5.hasComp);
                messaging.replyCompletedTaskLinesForList(listIx, compL);
              });
            });
            return;
          }
          api.patchTaskNeedsActionAsync(t, tlId, compT[taskIx].id, function () {
            var t2 = auth.getValidAccessToken() || token;
            openAndFlagAsync(t2, listIx, function (o5) {
              completedLinesAsync(t2, listIx, function (compL) {
                messaging.replyOpenTaskLinesForList(listIx, o5.text, o5.hasComp);
                messaging.replyCompletedTaskLinesForList(listIx, compL);
              });
            });
          });
        });
        return;
      }

      if (cmd === protocol.CMD_W_SET_TASK_DUE) {
        var dueDay = storage.normalizeLine(text);
        if (!tlId || !/^\d{4}-\d{2}-\d{2}$/.test(dueDay)) {
          openAndFlagAsync(t, listIx, function (ox) {
            messaging.replyOpenTaskLinesForList(listIx, ox.text, ox.hasComp);
          });
          return;
        }
        api.listOpenTasksSortedAsync(t, tlId, function (openT) {
          if (taskIx < 0 || taskIx >= openT.length) {
            openAndFlagAsync(t, listIx, function (ox) {
              messaging.replyOpenTaskLinesForList(listIx, ox.text, ox.hasComp);
            });
            return;
          }
          var iso = dueDay + 'T00:00:00.000Z';
          api.patchTaskDueAsync(t, tlId, openT[taskIx].id, iso, function () {
            var t2 = auth.getValidAccessToken() || token;
            openAndFlagAsync(t2, listIx, function (ox) {
              messaging.replyOpenTaskLinesForList(listIx, ox.text, ox.hasComp);
            });
          });
        });
        return;
      }

      if (cmd === protocol.CMD_W_CLEAR_TASK_DUE) {
        if (!tlId) {
          openAndFlagAsync(t, listIx, function (ox) {
            messaging.replyOpenTaskLinesForList(listIx, ox.text, ox.hasComp);
          });
          return;
        }
        api.listOpenTasksSortedAsync(t, tlId, function (openT) {
          if (taskIx < 0 || taskIx >= openT.length) {
            openAndFlagAsync(t, listIx, function (ox) {
              messaging.replyOpenTaskLinesForList(listIx, ox.text, ox.hasComp);
            });
            return;
          }
          api.patchTaskClearDueAsync(t, tlId, openT[taskIx].id, function () {
            var t2 = auth.getValidAccessToken() || token;
            openAndFlagAsync(t2, listIx, function (ox) {
              messaging.replyOpenTaskLinesForList(listIx, ox.text, ox.hasComp);
            });
          });
        });
        return;
      }

      if (cmd === protocol.CMD_W_PIN_TASK) {
        if (!tlId) {
          messaging.replyToast('Task not found');
          return;
        }
        api.listOpenTasksSortedAsync(t, tlId, function (openT) {
          if (taskIx < 0 || taskIx >= openT.length) {
            messaging.replyToast('Task not found');
            return;
          }
          var taskP = openT[taskIx];
          var fromWatch = text != null ? String(text).trim() : '';
          var timeIso = '';
          if (/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$/.test(fromWatch)) {
            timeIso = fromWatch;
          } else {
            var dueP = taskP && taskP.due ? storage.extractDueYmd(taskP.due) : '';
            if (!dueP) {
              messaging.replyToast('Set due first');
              return;
            }
            timeIso = dueP + 'T09:00:00Z';
          }
          if (!isPinInstantWithinPebbleRange(timeIso)) {
            messaging.replyToast('Time out of range');
            return;
          }
          var pinId = timeline.newPinId('g');
          var listName = '';
          for (var li = 0; li < lists.length; li++) {
            if (api.taskListIdFromLists(lists, li) === tlId) {
              listName = (lists[li] && lists[li].title) || '';
              break;
            }
          }
          var pin = timeline.buildTaskPin(pinId, taskP.title || '', timeIso, listName);
          timeline.getTimelineTokenAsync(function (timelineToken) {
            if (!timelineToken) {
              messaging.replyToast('Timeline not enabled');
              return;
            }
            timeline.pushPin(timelineToken, pin, function (ok) {
              if (ok) {
                messaging.replyToast('Pinned');
              } else {
                messaging.replyToast('Timeline error');
              }
            });
          });
        });
        return;
      }

    });
  });
}

module.exports = {
  dispatch: dispatch,
};
