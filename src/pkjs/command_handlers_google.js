var protocol = require('./protocol');
var api = require('./google_tasks_api');
var auth = require('./auth_storage');
var messaging = require('./messaging');
var storage = require('./storage');

var maxListNameForMenu = 64;
var maxListLines = 7; /* keep joined text under app message + TEXT_BUF(512) limits */

function linesFromListArray(lists) {
  var out = [];
  for (var i = 0; i < lists.length && i < maxListLines; i += 1) {
    var title = (lists[i] && (lists[i].title || '')) || '';
    title = storage.truncate(String(title).trim());
    if (!title.length) {
      title = '(untitled list)';
    }
    if (title.length > maxListNameForMenu) {
      title = title.substring(0, maxListNameForMenu - 1) + '…';
    }
    out.push(title);
  }
  return out.join('\n');
}

function openAndFlagAsync(accessToken, listIx, done) {
  api.listTaskListsSortedAsync(accessToken, function (lists) {
    var tlId = api.taskListIdFromLists(lists, listIx);
    if (!tlId) {
      done({ text: '', hasComp: 0 });
      return;
    }
    api.listOpenTasksSortedAsync(accessToken, tlId, function (open) {
      var isoDateRe = /^\d{4}-\d{2}-\d{2}$/;
      var lineStr = open
        .map(function (t) {
          var title = storage.truncate(t.title || '');
          if (t && t.due) {
            var day = String(t.due).substring(0, 10);
            if (isoDateRe.test(day)) {
              title += '\x1F' + storage.formatDueForWatch(day);
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
  api.listTaskListsSortedAsync(accessToken, function (lists) {
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
  } else {
    messaging.replyListsWithText('');
    messaging.replyOpenTaskLinesForList(listIx, '', 0);
  }
}

function dispatch(cmd, listIx, taskIx, text) {
  auth.getValidAccessTokenAsync(function (token) {
    if (!token) {
      replyEmptyForCmd(cmd, listIx);
      return;
    }

    if (cmd === protocol.CMD_W_ASK_LISTS) {
      api.listTaskListsSortedAsync(token, function (lists) {
        messaging.replyListsWithText(linesFromListArray(lists));
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
        api.listTaskListsSortedAsync(t2, function (lists) {
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
      api.listTaskListsSortedAsync(token, function (lists) {
        var tlDel = api.taskListIdFromLists(lists, listIx);
        if (tlDel) {
          api.deleteTaskListAsync(token, tlDel, function () {
            var t2 = auth.getValidAccessToken() || token;
            api.listTaskListsSortedAsync(t2, function (lists2) {
              messaging.replyListsWithText(linesFromListArray(lists2));
            });
          });
          return;
        }
        var t3 = auth.getValidAccessToken() || token;
        api.listTaskListsSortedAsync(t3, function (lists3) {
          messaging.replyListsWithText(linesFromListArray(lists3));
        });
      });
      return;
    }

    api.listTaskListsSortedAsync(token, function (lists) {
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

    });
  });
}

module.exports = {
  dispatch: dispatch,
};
