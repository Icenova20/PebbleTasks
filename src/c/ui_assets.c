#include "ui_assets.h"
#include "theme.h"

#include <pebble.h>
#include "src/resource_ids.auto.h"

static GBitmap *s_add_list[2];
static GBitmap *s_add_task[2];
static GBitmap *s_done[2];
static GBitmap *s_trash[2];

void ui_assets_init(void) {
  s_add_list[0] = gbitmap_create_with_resource(RESOURCE_ID_ICON_ADD_LIST);
  s_add_list[1] = gbitmap_create_with_resource(RESOURCE_ID_ICON_ADD_LIST_W);
  s_add_task[0] = gbitmap_create_with_resource(RESOURCE_ID_ICON_ADD_TASK);
  s_add_task[1] = gbitmap_create_with_resource(RESOURCE_ID_ICON_ADD_TASK_W);
  s_done[0] = gbitmap_create_with_resource(RESOURCE_ID_ICON_COMPLETED);
  s_done[1] = gbitmap_create_with_resource(RESOURCE_ID_ICON_COMPLETED_W);
  s_trash[0] = gbitmap_create_with_resource(RESOURCE_ID_ICON_TRASH);
  s_trash[1] = gbitmap_create_with_resource(RESOURCE_ID_ICON_TRASH_W);
}

void ui_assets_deinit(void) {
  for (int i = 0; i < 2; i++) {
    gbitmap_destroy(s_add_list[i]);
    gbitmap_destroy(s_add_task[i]);
    gbitmap_destroy(s_done[i]);
    gbitmap_destroy(s_trash[i]);
    s_add_list[i] = NULL;
    s_add_task[i] = NULL;
    s_done[i] = NULL;
    s_trash[i] = NULL;
  }
}

const GBitmap *ui_assets_add_list(void) {
  return s_add_list[theme_icons_use_light_variant() ? 1 : 0];
}

const GBitmap *ui_assets_add_task(void) {
  return s_add_task[theme_icons_use_light_variant() ? 1 : 0];
}

const GBitmap *ui_assets_completed(void) {
  return s_done[theme_icons_use_light_variant() ? 1 : 0];
}

const GBitmap *ui_assets_trash(void) {
  return s_trash[theme_icons_use_light_variant() ? 1 : 0];
}
