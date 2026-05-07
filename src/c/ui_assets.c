#include "ui_assets.h"

#include <pebble.h>
#include "src/resource_ids.auto.h"

static GBitmap *s_add_list;
static GBitmap *s_add_task;
static GBitmap *s_done;
static GBitmap *s_trash;

void ui_assets_init(void) {
  s_add_list = gbitmap_create_with_resource(RESOURCE_ID_ICON_ADD_LIST);
  s_add_task = gbitmap_create_with_resource(RESOURCE_ID_ICON_ADD_TASK);
  s_done = gbitmap_create_with_resource(RESOURCE_ID_ICON_COMPLETED);
  s_trash = gbitmap_create_with_resource(RESOURCE_ID_ICON_TRASH);
}

void ui_assets_deinit(void) {
  gbitmap_destroy(s_add_list);
  gbitmap_destroy(s_add_task);
  gbitmap_destroy(s_done);
  gbitmap_destroy(s_trash);
  s_add_list = NULL;
  s_add_task = NULL;
  s_done = NULL;
  s_trash = NULL;
}

const GBitmap *ui_assets_add_list(void) {
  return s_add_list;
}

const GBitmap *ui_assets_add_task(void) {
  return s_add_task;
}

const GBitmap *ui_assets_completed(void) {
  return s_done;
}

const GBitmap *ui_assets_trash(void) {
  return s_trash;
}
