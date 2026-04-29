#pragma once

#include <pebble.h>
#include <stdbool.h>

/* Provided by main.c after dictation_session_create */
DictationSession *pebble_tasks_dictation_session(void);
bool pebble_tasks_dictation_available(void);
