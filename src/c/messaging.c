#include "messaging.h"
#include "protocol.h"
#include "completed_menu.h"
#include "main_menu.h"
#include "tasks_menu.h"
#include "theme.h"
#include "ui_loading.h"
#include "ui_toast.h"

#include <message_keys.auto.h>
#include <string.h>

void messaging_send(int cmd, int32_t list_index, int32_t task_index, const char *text) {
  DictionaryIterator *it;
  if (app_message_outbox_begin(&it) != APP_MSG_OK) {
    if (!connection_service_peek_pebble_app_connection()) {
      ui_toast_show("Phone disconnected");
    }
    return;
  }
  dict_write_int32(it, MESSAGE_KEY_cmd, cmd);
  if (list_index >= 0) {
    dict_write_int32(it, MESSAGE_KEY_listIndex, list_index);
  }
  if (task_index >= 0) {
    dict_write_int32(it, MESSAGE_KEY_taskIndex, task_index);
  }
  if (text && text[0]) {
    dict_write_cstring(it, MESSAGE_KEY_text, text);
  }
  app_message_outbox_send();
}

void messaging_request_lists(void) { messaging_send(CMD_W_ASK_LISTS, -1, -1, NULL); }

void messaging_request_open_for_list(int list_index) {
  messaging_send(CMD_W_ASK_OPEN, list_index, -1, NULL);
}

void messaging_request_completed_for_list(int list_index) {
  messaging_send(CMD_W_ASK_COMPLETED, list_index, -1, NULL);
}

int32_t messaging_tuple_read_s32(const Tuple *t) {
  if (!t) {
    return 0;
  }
  if (t->type == TUPLE_INT) {
    if (t->length == 1) {
      return t->value[0].int8;
    }
    if (t->length == 2) {
      return t->value[0].int16;
    }
    if (t->length == 4) {
      return t->value[0].int32;
    }
  } else if (t->type == TUPLE_UINT) {
    if (t->length == 1) {
      return t->value[0].uint8;
    }
    if (t->length == 2) {
      return t->value[0].uint16;
    }
    if (t->length == 4) {
      return (int32_t)t->value[0].uint32;
    }
  }
  return 0;
}

const char *messaging_tuple_cstring(const Tuple *t) {
  if (!t || t->type != TUPLE_CSTRING) {
    return "";
  }
  return (const char *)t->value;
}

void messaging_inbox_received(DictionaryIterator *it, void *context) {
  (void)context;
  theme_handle_inbox(it);
  Tuple *tc = dict_find(it, MESSAGE_KEY_cmd);
  if (!tc) {
    return;
  }
  int cmd = (int)messaging_tuple_read_s32(tc);
  Tuple *tt = dict_find(it, MESSAGE_KEY_text);
  const char *text = messaging_tuple_cstring(tt);
  int li = -1;
  Tuple *tl = dict_find(it, MESSAGE_KEY_listIndex);
  if (tl) {
    li = (int)messaging_tuple_read_s32(tl);
  }

  if (cmd == R_LISTS) {
    main_menu_reload_from_payload(text);
    ui_loading_stop();
  } else if (cmd == R_OPEN_TASKS) {
    bool has_completed = false;
    Tuple *th = dict_find(it, MESSAGE_KEY_hasCompleted);
    if (th && messaging_tuple_read_s32(th) != 0) {
      has_completed = true;
    }
    tasks_menu_reload_from_payload_if_visible(text, li, has_completed);
    ui_loading_stop();
  } else if (cmd == R_COMPLETED_TASKS) {
    completed_menu_reload_from_payload_if_visible(text, li);
    ui_loading_stop();
  } else if (cmd == R_TOAST) {
    if (text && text[0]) {
      /* TextLayer does not copy strings; persist the toast message in static storage. */
      static char s_toast_buf[64];
      strncpy(s_toast_buf, text, sizeof(s_toast_buf) - 1);
      s_toast_buf[sizeof(s_toast_buf) - 1] = '\0';
      ui_toast_show(s_toast_buf);
    }
  }
}
