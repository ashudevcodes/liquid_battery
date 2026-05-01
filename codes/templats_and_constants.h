#pragma once

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <math.h>

#if defined(PLUGIN_IS_XFCE)
#include <libxfce4panel/libxfce4panel.h>

#elif defined(PLUGIN_IS_WAYBAR)
#include "./waybar_cffi_module.h"

#endif

#define UPOWER_BUS "org.freedesktop.UPower"
#define UPOWER_PATH "/org/freedesktop/UPower"
#define UPOWER_IFACE "org.freedesktop.UPower"
#define UPOWER_DEV_IFACE "org.freedesktop.UPower.Device"

#define UPOWER_STATE_CHARGING 1
#define UPOWER_STATE_DISCHARGING 2
#define UPOWER_STATE_FULL 4

#define NUB_RATIO 0.18
#define NUB_W_RATIO 0.11
#define PAD_RATIO 0.14

#define CORNER_OUTER_RATIO 0.25
#define CORNER_INNER_RATIO 0.25
#define LINE_WIDTH 1

#define WAVE_PHASE_STEP 0.15
#define TICK_MS 33
#define FORCE_MAX 3.0

#define LOW_BATTERY_PCT 15.0

#define DEFAULT_CLICK_ACTION "gnome-power-statistics"

typedef struct {
  GDBusProxy *proxy;
  double percentage;
  double display_pct;
  gboolean charging;
  gint64 time_to_empty;
  gint64 time_to_full;
} BattEntry;

typedef struct {
  GArray *batteries;
  GDBusProxy *upower;
  double percentage;
  double display_pct;
  gboolean charging;
  double phase;
  double interaction_force;
  double ripple_strength;
  guint tick_id;
  gboolean low_notified;
  GtkWidget *area;

#if defined(PLUGIN_IS_XFCE)
  XfcePanelPlugin *plugin;

#elif defined(PLUGIN_IS_WAYBAR)
  wbcffi_module *waybar_module;
  GtkBox *container;
  char *click_action;

#endif
} Battery;
