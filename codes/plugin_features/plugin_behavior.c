#include "../templats_and_constants.h"

gboolean get_double(GDBusProxy *proxy, const char *name, double *out);
gboolean get_uint(GDBusProxy *proxy, const char *name, guint *out);
gboolean get_int64(GDBusProxy *proxy, const char *name, gint64 *out);

void aggregate(Battery *b) {
  if (b->batteries->len == 0)
    return;

  double total = 0.0;
  gboolean any_charging = FALSE;

  for (guint i = 0; i < b->batteries->len; i++) {
    BattEntry *e = &g_array_index(b->batteries, BattEntry, i);
    total += e->percentage;
    if (e->charging)
      any_charging = TRUE;
  }

  double old_pct = b->percentage;
  b->percentage = total / b->batteries->len;
  b->charging = any_charging;

  if (b->percentage < old_pct)
    b->ripple_strength += 1.0;
}

void update_tooltip(Battery *b) {
  GString *tip = g_string_new(NULL);

  for (guint i = 0; i < b->batteries->len; i++) {
    BattEntry *e = &g_array_index(b->batteries, BattEntry, i);

    if (b->batteries->len > 1)
      g_string_append_printf(tip, "Battery %u: ", i + 1);

    g_string_append_printf(tip, "%.0f%%", e->percentage);

    if (e->charging) {
      if (e->time_to_full > 0) {
        int h = e->time_to_full / 3600;
        int m = (e->time_to_full % 3600) / 60;
        g_string_append_printf(tip, " — full charging in (%dh:%02dm)", h, m);
      } else {
        g_string_append(tip, " — fully charged");
      }
    } else {
      if (e->time_to_empty > 0) {
        int h = e->time_to_empty / 3600;
        int m = (e->time_to_empty % 3600) / 60;
        g_string_append_printf(tip, " — %dh:%02dm remaining", h, m);
      } else {
        g_string_append(tip, " — discharging");
      }
    }

    if (i + 1 < b->batteries->len)
      g_string_append_c(tip, '\n');
  }

  gtk_widget_set_tooltip_text(b->area, tip->str);
  g_string_free(tip, TRUE);
}

gboolean tick(gpointer data) {
  Battery *b = data;

  gboolean charging_anim = b->charging && b->percentage < 100.0;
  gboolean forces = b->interaction_force > 0.01 || b->ripple_strength > 0.01;
  gboolean level_drift = fabs(b->display_pct - b->percentage) > 0.1;

  if (!charging_anim && !forces && !level_drift)
    return G_SOURCE_CONTINUE;

  b->phase += WAVE_PHASE_STEP;
  b->interaction_force *= 0.90;
  b->ripple_strength *= 0.92;
  b->display_pct += (b->percentage - b->display_pct) * 0.08;

  gtk_widget_queue_draw(b->area);
  return G_SOURCE_CONTINUE;
}

gboolean on_enter(GtkWidget *w, GdkEventCrossing *e, gpointer data) {
  Battery *b = data;
  b->interaction_force = MIN(b->interaction_force + 1, FORCE_MAX);
  return FALSE;
}

gboolean on_click(GtkWidget *w, GdkEventButton *e, gpointer data) {
  if (e->button != 1)
    return FALSE;
  g_spawn_command_line_async("xfce4-power-manager-settings", NULL);
  Battery *b = data;
  b->interaction_force = MIN(b->interaction_force + 1.3, FORCE_MAX);
  return FALSE;
}

static void update_entry(Battery *b, BattEntry *e) {
  double pct;
  if (get_double(e->proxy, "Percentage", &pct)) {
    if (pct < e->percentage)
      b->ripple_strength += 0.6;
    e->percentage = pct;
  }

  guint state;
  if (get_uint(e->proxy, "State", &state)) {
    gboolean charging =
        (state == UPOWER_STATE_CHARGING || state == UPOWER_STATE_FULL);
    if (charging != e->charging) {
      e->charging = charging;
      b->ripple_strength += 1.2;
    }
  }

  gint64 t;
  if (get_int64(e->proxy, "TimeToEmpty", &t))
    e->time_to_empty = t;
  if (get_int64(e->proxy, "TimeToFull", &t))
    e->time_to_full = t;
}

static void on_entry_props_changed(GDBusProxy *proxy, GVariant *changed,
                                   const gchar *const *inv, gpointer data) {
  Battery *b = data;
  /* find which entry owns this proxy */
  for (guint i = 0; i < b->batteries->len; i++) {
    BattEntry *e = &g_array_index(b->batteries, BattEntry, i);
    if (e->proxy == proxy) {
      update_entry(b, e);
      break;
    }
  }
  aggregate(b);
  update_tooltip(b);
}

static void add_device(Battery *b, const gchar *path) {
  GError *err = NULL;
  GDBusProxy *proxy = g_dbus_proxy_new_for_bus_sync(
      G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE, NULL, UPOWER_BUS, path,
      UPOWER_DEV_IFACE, NULL, &err);

  if (!proxy) {
    g_warning("battery-plugin: proxy for %s: %s", path, err->message);
    g_error_free(err);
    return;
  }

  /* Only real batteries (Type == 2) */
  GVariant *type_v = g_dbus_proxy_get_cached_property(proxy, "Type");
  if (type_v) {
    guint type = g_variant_get_uint32(type_v);
    g_variant_unref(type_v);
    if (type != 2) { /* 2 = Battery */
      g_object_unref(proxy);
      return;
    }
  }

  BattEntry e = {0};
  e.proxy = proxy;

  g_array_append_val(b->batteries, e);
  BattEntry *ep =
      &g_array_index(b->batteries, BattEntry, b->batteries->len - 1);
  update_entry(b, ep);

  g_signal_connect(proxy, "g-properties-changed",
                   G_CALLBACK(on_entry_props_changed), b);
}

void enumerate_devices(Battery *b) {
  if (!b->upower)
    return;

  GVariant *result =
      g_dbus_proxy_call_sync(b->upower, "EnumerateDevices", NULL,
                             G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL);

  if (!result)
    return;

  GVariant *paths_v;
  g_variant_get(result, "(@ao)", &paths_v);
  gsize n = g_variant_n_children(paths_v);

  for (gsize i = 0; i < n; i++) {
    const gchar *path;
    g_variant_get_child(paths_v, i, "&o", &path);
    add_device(b, path);
  }

  g_variant_unref(paths_v);
  g_variant_unref(result);

  /* Fallback: no real batteries found → use DisplayDevice */
  if (b->batteries->len == 0) {
    g_warning(
        "battery-plugin: no batteries found, falling back to DisplayDevice");
    add_device(b, "/org/freedesktop/UPower/devices/DisplayDevice");
  }
}
