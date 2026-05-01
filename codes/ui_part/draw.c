#include "../templats_and_constants.h"
#include "gtk/gtk.h"

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

gboolean draw(GtkWidget *widget, cairo_t *cr, gpointer data) {
  Battery *b = data;

  int height = gtk_widget_get_allocated_height(widget);
  int width = gtk_widget_get_allocated_width(widget);

  double batt_h = height;
  double batt_w = width;

  double nub_h = batt_h * NUB_RATIO;
  double nub_w = batt_w * NUB_W_RATIO;

  double pad = 5;

  double x = 6;
  double y = 5;

  double corner_in = batt_h * CORNER_INNER_RATIO;
  double corner_out = batt_h * CORNER_OUTER_RATIO;

  cairo_set_line_width(cr, LINE_WIDTH);
  cairo_set_source_rgb(cr, 0.62, 0.62, 0.62);

  /* batt_h = height * 0.70;
  batt_w = height * 1.1; */

  /* Inner fill */
  double inner_w = batt_w - pad * 2;
  double inner_h = batt_h - pad * 2;
  double fill_h = inner_h * (b->display_pct / 100.0);

  /* Outer outline */
  /* cairo_save(cr);
  cairo_translate(cr, x, y);
  rounded_rect(cr, inner_w, inner_h, corner_out);
  cairo_stroke(cr);
  cairo_restore(cr); */

  /* Nub */
  /* cairo_set_source_rgb(cr, 1, 1, 1);
  cairo_rectangle(cr, inner_w + 7, inner_h - 5, nub_w, nub_h);
  cairo_fill(cr); */

  if (fill_h <= 0.0)
    return TRUE;

  cairo_save(cr);
  cairo_translate(cr, x, y);
  rounded_rect(cr, inner_w, inner_h, corner_in);
  cairo_clip(cr);

  set_fill_color(cr, b->charging, b->percentage);

  /* Wave */
  double wave_amp = 0.0;
  // b->percentage = 100;
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
