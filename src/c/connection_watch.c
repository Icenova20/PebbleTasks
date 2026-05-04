#include "connection_watch.h"
#include "ui_toast.h"

#include <pebble.h>

static void handle_pebble_connection(bool connected) {
  if (connected) {
    return;
  }
  ui_toast_show("Phone disconnected");
}

void connection_watch_init(void) {
  connection_service_subscribe((ConnectionHandlers){
      .pebble_app_connection_handler = handle_pebble_connection,
  });
  if (!connection_service_peek_pebble_app_connection()) {
    ui_toast_show("Phone disconnected");
  }
}

void connection_watch_deinit(void) {
  connection_service_unsubscribe();
}
