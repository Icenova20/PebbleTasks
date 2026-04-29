#include <pebble.h>
#include <stdio.h>
#include <string.h>

#include "completed_menu.h"
#include "main_menu.h"
#include "messaging.h"
#include "pebble_tasks.h"
#include "protocol.h"
#include "tasks_menu.h"
#include "theme.h"
#include "ui_assets.h"
#include "ui_toast.h"

static DictationSession *s_dictation;
static char s_dictation_text[DICTATION_BUF];

DictationSession *pebble_tasks_dictation_session(void) { return s_dictation; }

bool pebble_tasks_dictation_available(void) { return s_dictation != NULL; }

static void dictation_cb(DictationSession *session, DictationSessionStatus status, char *transcription,
                         void *ctx) {
  (void)session;
  (void)ctx;
  if (status != DictationSessionStatusSuccess) {
    static char s_err[40];
    snprintf(s_err, sizeof(s_err), "Dictation: err %d", (int)status);
    ui_toast_show(s_err);
    return;
  }
  if (!transcription) {
    return;
  }
  {
    const char *t = transcription;
    char *d = s_dictation_text;
    size_t n;
    for (n = 0; n < sizeof(s_dictation_text) - 1 && t && *t; n++) {
      *d++ = *t++;
    }
    *d = '\0';
  }
  if (main_menu_is_adding_list()) {
    messaging_send(CMD_W_ADD_LIST, -1, -1, s_dictation_text);
  } else {
    messaging_send(CMD_W_ADD_TASK, tasks_menu_current_list_index(), -1, s_dictation_text);
  }
}

static void app_init(void) {
  theme_init();
  ui_assets_init();
  main_menu_init();
  app_message_register_inbox_received(messaging_inbox_received);
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
  s_dictation = dictation_session_create(sizeof(s_dictation_text), dictation_cb, NULL);
  window_stack_push(main_menu_get_window(), true);
}

static void app_deinit(void) {
  completed_menu_deinit();
  tasks_menu_deinit();
  if (s_dictation) {
    dictation_session_destroy(s_dictation);
    s_dictation = NULL;
  }
  main_menu_deinit();
  ui_assets_deinit();
}

int main(void) {
  app_init();
  app_event_loop();
  app_deinit();
}
