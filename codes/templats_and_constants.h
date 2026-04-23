#include <gio/gio.h>
#include <libxfce4panel/libxfce4panel.h>

#define UPOWER_BUS "org.freedesktop.UPower"
#define UPOWER_PATH "/org/freedesktop/UPower"
#define UPOWER_IFACE "org.freedesktop.UPower"
#define UPOWER_DEV_IFACE "org.freedesktop.UPower.Device"

#define UPOWER_STATE_CHARGING 1
#define UPOWER_STATE_DISCHARGING 2
#define UPOWER_STATE_FULL 4

#define NUB_RATIO 0.18   /* nub_h  = battery_h * NUB_RATIO  */
#define NUB_W_RATIO 0.12 /* nub_w  = battery_h * NUB_W_RATIO */
#define PAD_RATIO 0.14   /* pad    = battery_h * PAD_RATIO   */

#define CORNER_OUTER_RATIO 0
#define CORNER_INNER_RATIO 0.20
#define LINE_WIDTH 0

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
