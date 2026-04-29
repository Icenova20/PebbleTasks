#pragma once

#include <pebble.h>

void messaging_send(int cmd, int32_t list_index, int32_t task_index, const char *text);
void messaging_request_lists(void);
void messaging_request_open_for_list(int list_index);
void messaging_request_completed_for_list(int list_index);

int32_t messaging_tuple_read_s32(const Tuple *t);
const char *messaging_tuple_cstring(const Tuple *t);

void messaging_inbox_received(DictionaryIterator *iterator, void *context);
