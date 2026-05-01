#include "../templats_and_constants.h"
#include "gtk/gtk.h"

void aggregate(Battery *b);
void free_entry(gpointer p);
void update_tooltip(Battery *b);
void enumerate_devices(Battery *b);

gboolean draw(GtkWidget *widget, cairo_t *cr, gpointer data);
gboolean tick(gpointer data);
gboolean on_click(GtkWidget *w, GdkEventButton *e, gpointer data);
gboolean on_enter(GtkWidget *w, GdkEventCrossing *e, gpointer data);

#if defined(PLUGIN_IS_XFCE)

void on_plugin_free(XfcePanelPlugin *plugin, gpointer data);
void on_size_changed(XfcePanelPlugin *plugin, gint size, gpointer data);

static void construct(XfcePanelPlugin *plugin) {
  Battery *b = g_new0(Battery, 1);
  b->plugin = plugin;
  b->batteries = g_array_new(FALSE, TRUE, sizeof(BattEntry));
  g_array_set_clear_func(b->batteries, free_entry);

  b->area = gtk_drawing_area_new();
  gtk_widget_set_size_request(b->area, 24, 16);
  gtk_container_add(GTK_CONTAINER(plugin), b->area);

  gtk_widget_add_events(b->area, GDK_ENTER_NOTIFY_MASK | GDK_BUTTON_PRESS_MASK);
  gtk_widget_set_has_tooltip(b->area, TRUE);

  g_signal_connect(b->area, "draw", G_CALLBACK(draw), b);
  g_signal_connect(b->area, "enter-notify-event", G_CALLBACK(on_enter), b);
  g_signal_connect(b->area, "button-press-event", G_CALLBACK(on_click), b);

  g_signal_connect(plugin, "size-changed", G_CALLBACK(on_size_changed), b);
  on_size_changed(plugin, xfce_panel_plugin_get_size(plugin), b);

  b->upower = g_dbus_proxy_new_for_bus_sync(
      G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE, NULL, UPOWER_BUS, UPOWER_PATH,
      UPOWER_IFACE, NULL, NULL);

  enumerate_devices(b);
  aggregate(b);
  b->display_pct = b->percentage;
  update_tooltip(b);

  g_signal_connect(plugin, "free-data", G_CALLBACK(on_plugin_free), b);

  b->tick_id = g_timeout_add(TICK_MS, tick, b);

  gtk_widget_show_all(GTK_WIDGET(plugin));
}

XFCE_PANEL_PLUGIN_REGISTER(construct)

#elif defined(PLUGIN_IS_WAYBAR)

const size_t wbcffi_version = 2;

void *wbcffi_init(const wbcffi_init_info *init_info,
                  const wbcffi_config_entry *config_entries,
                  size_t config_entries_len) {
  for (size_t i = 0; i < config_entries_len; i++)
    printf("  %s = %s\n", config_entries[i].key, config_entries[i].value);

  Battery *b = malloc(sizeof(Battery));
  memset(b, 0, sizeof(Battery));

  b->waybar_module = init_info->obj;
  b->batteries = g_array_new(FALSE, TRUE, sizeof(BattEntry));
  g_array_set_clear_func(b->batteries, free_entry);
  b->click_action = strdup(DEFAULT_CLICK_ACTION);

  for (size_t i = 0; i < config_entries_len; i++) {
    if (strcmp(config_entries[i].key, "click_action") == 0) {
      free(b->click_action);
      b->click_action = strdup(config_entries[i].value);
    }
  }

  GtkContainer *root = init_info->get_root_widget(init_info->obj);

  b->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));

  gtk_widget_set_name(GTK_WIDGET(b->container), "liquid_battery");

  gtk_widget_set_vexpand(GTK_WIDGET(b->container), FALSE);
  gtk_widget_set_hexpand(GTK_WIDGET(b->container), FALSE);

  gtk_widget_set_halign(GTK_WIDGET(b->container), GTK_ALIGN_CENTER);
  gtk_widget_set_valign(GTK_WIDGET(b->container), GTK_ALIGN_CENTER);

  // gtk_widget_set_margin_end(GTK_WIDGET(b->container), 4);

  gtk_container_add(root, GTK_WIDGET(b->container));

  b->area = gtk_drawing_area_new();
  gtk_widget_set_size_request(b->area, 44, 25);
  gtk_widget_add_events(b->area, GDK_ENTER_NOTIFY_MASK | GDK_BUTTON_PRESS_MASK);
  gtk_widget_set_has_tooltip(b->area, TRUE);
  gtk_container_add(GTK_CONTAINER(b->container), b->area);

  g_signal_connect(b->area, "draw", G_CALLBACK(draw), b);
  g_signal_connect(b->area, "enter-notify-event", G_CALLBACK(on_enter), b);
  g_signal_connect(b->area, "button-press-event", G_CALLBACK(on_click), b);

  gtk_widget_show_all(GTK_WIDGET(root));

  b->upower = g_dbus_proxy_new_for_bus_sync(
      G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE, NULL, UPOWER_BUS, UPOWER_PATH,
      UPOWER_IFACE, NULL, NULL);

  enumerate_devices(b);
  aggregate(b);
  b->display_pct = b->percentage;
  update_tooltip(b);

  b->tick_id = g_timeout_add(TICK_MS, tick, b);

  return b;
}

void wbcffi_deinit(void *instance) {
  Battery *b = instance;
  if (b->tick_id)
    g_source_remove(b->tick_id);
  if (b->batteries)
    g_array_free(b->batteries, TRUE);
  if (b->upower)
    g_object_unref(b->upower);
  free(b->click_action);
  free(b);
}

void wbcffi_update(void *instance) {
  Battery *b = instance;
  gtk_widget_queue_draw(b->area);
}

void wbcffi_refresh(void *instance, int signal) {}

void wbcffi_doaction(void *instance, const char *action_name) {}

#endif
