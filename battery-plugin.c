#include <gio/gio.h>
#include <gtk/gtk.h>
#include <libxfce4panel/libxfce4panel.h>
#include <math.h>
#include <stdio.h>

#define UPOWER_BUS "org.freedesktop.UPower"
#define UPOWER_PATH "/org/freedesktop/UPower"
#define UPOWER_IFACE "org.freedesktop.UPower"
#define UPOWER_DEV_IFACE "org.freedesktop.UPower.Device"

#define UPOWER_STATE_CHARGING 1
#define UPOWER_STATE_DISCHARGING 2
#define UPOWER_STATE_FULL 4

/* ── Drawing constants (scaled at runtime) ──────────────────────────────── */
#define NUB_RATIO 0.35   /* nub_h  = battery_h * NUB_RATIO  */
#define NUB_W_RATIO 0.12 /* nub_w  = battery_h * NUB_W_RATIO */
#define PAD_RATIO 0.14   /* pad    = battery_h * PAD_RATIO   */
#define CORNER_OUTER_RATIO 0.22
#define CORNER_INNER_RATIO 0.16
#define LINE_WIDTH 1.8

#define WAVE_PHASE_STEP 0.15
#define TICK_MS 33 /* ~30 fps */
#define FORCE_MAX 3.0

#define LOW_BATTERY_PCT 15.0

typedef struct {
  GDBusProxy *proxy;
  double percentage;
  double display_pct;
  gboolean charging;
  gint64 time_to_empty; /* seconds, 0 = unknown */
  gint64 time_to_full;
} BattEntry;

typedef struct {
  XfcePanelPlugin *plugin;
  GtkWidget *area;

  /* batteries */
  GArray *batteries;  /* GArray of BattEntry */
  GDBusProxy *upower; /* org.freedesktop.UPower (for device enumeration) */

  /* aggregated display values */
  double percentage;
  double display_pct;
  gboolean charging;

  /* animation */
  double phase;
  double interaction_force;
  double ripple_strength;
  guint tick_id;

  /* low-battery guard */
  gboolean low_notified;
} Battery;

static gboolean get_double(GDBusProxy *proxy, const char *name, double *out) {
  GVariant *v = g_dbus_proxy_get_cached_property(proxy, name);
  if (!v)
    return FALSE;
  gboolean ok = g_variant_is_of_type(v, G_VARIANT_TYPE_DOUBLE);
  if (ok)
    *out = g_variant_get_double(v);
  g_variant_unref(v);
  return ok;
}

static gboolean get_uint(GDBusProxy *proxy, const char *name, guint *out) {
  GVariant *v = g_dbus_proxy_get_cached_property(proxy, name);
  if (!v)
    return FALSE;
  gboolean ok = g_variant_is_of_type(v, G_VARIANT_TYPE_UINT32);
  if (ok)
    *out = g_variant_get_uint32(v);
  g_variant_unref(v);
  return ok;
}

static gboolean get_int64(GDBusProxy *proxy, const char *name, gint64 *out) {
  GVariant *v = g_dbus_proxy_get_cached_property(proxy, name);
  if (!v)
    return FALSE;
  gboolean ok = g_variant_is_of_type(v, G_VARIANT_TYPE_INT64);
  if (ok)
    *out = g_variant_get_int64(v);
  g_variant_unref(v);
  return ok;
}

static void aggregate(Battery *b) {
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

static void update_tooltip(Battery *b) {
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
        g_string_append_printf(tip, " — charging (%dh:%02dm remaining)", h, m);
      } else {
        g_string_append(tip, " — charging");
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

static void rounded_rect(cairo_t *cr, double w, double h, double r) {
  cairo_new_sub_path(cr);
  cairo_arc(cr, w - r, r, r, -M_PI / 2, 0);
  cairo_arc(cr, w - r, h - r, r, 0, M_PI / 2);
  cairo_arc(cr, r, h - r, r, M_PI / 2, M_PI);
  cairo_arc(cr, r, r, r, M_PI, 3 * M_PI / 2);
  cairo_close_path(cr);
}

/* Lerp fill colour: gray(100%) → orange(20%) → red(0%), ignoring charging */
static void set_fill_color(cairo_t *cr, gboolean charging, double pct) {
  if (charging) {
    /* green, slightly dimmer when almost full */
    double v = 0.75 + 0.1 * (pct / 100.0);
    cairo_set_source_rgb(cr, 0.2, v, 0.35);
    return;
  }

  /* clamp 0..100 */
  double t = CLAMP(pct, 0.0, 100.0) / 100.0;

  double r, g, bl;
  if (t >= 0.20) {
    /* gray(1.0) → gray(0.2): just desaturate smoothly */
    double s = (t - 0.20) / 0.80; /* 0=20%, 1=100% */
    r = 0.52 + 0.10 * s;
    g = 0.52 + 0.10 * s;
    bl = 0.52 + 0.10 * s;
  } else if (t >= 0.10) {
    /* orange → gray */
    double s = (t - 0.10) / 0.10;
    r = 1.00 - (1.00 - 0.52) * s; /* 1.0 → 0.52 */
    g = 0.60 - (0.60 - 0.52) * s;
    bl = 0.20 - (0.20 - 0.52) * s;
    bl = CLAMP(bl, 0.0, 1.0);
  } else {
    /* red → orange */
    double s = t / 0.10;
    r = 1.00;
    g = 0.30 + 0.30 * s;
    bl = 0.30 * (1.0 - s);
  }

  cairo_set_source_rgb(cr, r, g, bl);
}

static gboolean draw(GtkWidget *widget, cairo_t *cr, gpointer data) {
  Battery *b = data;

  int height = gtk_widget_get_allocated_height(widget);

  /* Scale geometry to widget height */
  double batt_h = height * 0.65;
  double batt_h_min = 8.0;
  if (batt_h < batt_h_min)
    batt_h = batt_h_min;
  double batt_w = height / 2;
  double nub_h = batt_h * NUB_RATIO;
  double nub_w = batt_h * NUB_W_RATIO;
  double pad = batt_h * PAD_RATIO;
  double corner_out = batt_h * CORNER_OUTER_RATIO;
  double corner_in = batt_h * CORNER_INNER_RATIO;

  double total_w = batt_w + nub_w + 1.0;
  double x = total_w / 2.0;
  double y = (height - batt_h) / 2.0;

  cairo_set_line_width(cr, LINE_WIDTH);
  cairo_set_source_rgb(cr, 1, 1, 1);

  batt_h = height * 0.65;
  batt_w = height * 1.1;

  /* Outer outline */
  cairo_save(cr);
  cairo_translate(cr, x, y);
  rounded_rect(cr, batt_w, batt_h, corner_out);
  cairo_stroke(cr);
  cairo_restore(cr);

  /* Nub */
  cairo_set_source_rgb(cr, 1, 1, 1);
  cairo_rectangle(cr, x + batt_w + 1.0, y + (batt_h - nub_h) / 2.0, nub_w,
                  nub_h);
  cairo_fill(cr);

  /* Inner fill */
  double inner_w = batt_w - pad * 2;
  double inner_h = batt_h - pad * 2;
  double fill_h = inner_h * (b->display_pct / 100.0);

  if (fill_h <= 0.0)
    return TRUE;

  cairo_save(cr);
  cairo_translate(cr, x + pad, y + pad);
  rounded_rect(cr, inner_w, inner_h, corner_in);
  cairo_clip(cr);

  set_fill_color(cr, b->charging, b->percentage);

  /* Wave */
  double wave_amp = 0.0;
  if (b->charging && b->percentage < 100.0)
    wave_amp = 2.0 * (1.0 - b->percentage / 100.0);
  wave_amp += b->interaction_force * 2.0;
  wave_amp += b->ripple_strength * 2.0;

  cairo_new_path(cr);
  cairo_move_to(cr, 0, inner_h + 0.5);

  int steps = (int)inner_w;
  for (int i = 0; i <= steps; i++) {
    double nx = (double)i / inner_w;
    double wave = sin(nx * 2.0 * M_PI + b->phase);
    double y_coord = inner_h - fill_h + wave * wave_amp;
    cairo_line_to(cr, i, y_coord);
  }

  cairo_line_to(cr, inner_w, inner_h);
  cairo_close_path(cr);
  cairo_fill(cr);

  cairo_restore(cr);

  return TRUE;
}

static gboolean tick(gpointer data) {
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

static gboolean on_enter(GtkWidget *w, GdkEventCrossing *e, gpointer data) {
  Battery *b = data;
  b->interaction_force = MIN(b->interaction_force + 0.6, FORCE_MAX);
  return FALSE;
}

static gboolean on_click(GtkWidget *w, GdkEventButton *e, gpointer data) {
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

static void enumerate_devices(Battery *b) {
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

static void free_entry(gpointer p) {
  BattEntry *e = p;
  if (e->proxy)
    g_object_unref(e->proxy);
}

static void on_plugin_free(XfcePanelPlugin *plugin, gpointer data) {
  Battery *b = data;

  if (b->tick_id) {
    g_source_remove(b->tick_id);
    b->tick_id = 0;
  }

  if (b->batteries) {
    g_array_free(b->batteries, TRUE);
    b->batteries = NULL;
  }

  if (b->upower) {
    g_object_unref(b->upower);
    b->upower = NULL;
  }

  g_free(b);
}

static void on_size_changed(XfcePanelPlugin *plugin, gint size, gpointer data) {
  Battery *b = data;
  /* width = ~2× battery body + nub + margin; height = panel size */
  int w = (int)(size * 1.6);
  gtk_widget_set_size_request(b->area, w, size);
}

static void construct(XfcePanelPlugin *plugin) {
  Battery *b = g_new0(Battery, 1);
  b->plugin = plugin;
  b->batteries = g_array_new(FALSE, TRUE, sizeof(BattEntry));
  g_array_set_clear_func(b->batteries, free_entry);

  /* Drawing area */
  b->area = gtk_drawing_area_new();
  gtk_widget_set_size_request(b->area, 24, 16); /* initial fallback */
  gtk_container_add(GTK_CONTAINER(plugin), b->area);

  gtk_widget_add_events(b->area, GDK_ENTER_NOTIFY_MASK | GDK_BUTTON_PRESS_MASK);
  gtk_widget_set_has_tooltip(b->area, TRUE);

  g_signal_connect(b->area, "draw", G_CALLBACK(draw), b);
  g_signal_connect(b->area, "enter-notify-event", G_CALLBACK(on_enter), b);
  g_signal_connect(b->area, "button-press-event", G_CALLBACK(on_click), b);

  /* Panel size tracking */
  g_signal_connect(plugin, "size-changed", G_CALLBACK(on_size_changed), b);
  on_size_changed(plugin, xfce_panel_plugin_get_size(plugin), b);

  /* UPower master proxy */
  b->upower = g_dbus_proxy_new_for_bus_sync(
      G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE, NULL, UPOWER_BUS, UPOWER_PATH,
      UPOWER_IFACE, NULL, NULL);

  enumerate_devices(b);
  aggregate(b);
  b->display_pct = b->percentage; /* no lerp jump on startup */
  update_tooltip(b);

  g_signal_connect(plugin, "free-data", G_CALLBACK(on_plugin_free), b);

  b->tick_id = g_timeout_add(TICK_MS, tick, b);

  gtk_widget_show_all(GTK_WIDGET(plugin));
}

XFCE_PANEL_PLUGIN_REGISTER(construct)
