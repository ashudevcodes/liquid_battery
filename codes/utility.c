#include "./templats_and_constants.h"

gboolean get_double(GDBusProxy *proxy, const char *name, double *out) {
  GVariant *v = g_dbus_proxy_get_cached_property(proxy, name);
  if (!v)
    return FALSE;
  gboolean ok = g_variant_is_of_type(v, G_VARIANT_TYPE_DOUBLE);
  if (ok)
    *out = g_variant_get_double(v);
  g_variant_unref(v);
  return ok;
}

gboolean get_uint(GDBusProxy *proxy, const char *name, guint *out) {
  GVariant *v = g_dbus_proxy_get_cached_property(proxy, name);
  if (!v)
    return FALSE;
  gboolean ok = g_variant_is_of_type(v, G_VARIANT_TYPE_UINT32);
  if (ok)
    *out = g_variant_get_uint32(v);
  g_variant_unref(v);
  return ok;
}

/*
 * this function is for extracting TimeToEmpty for UPOWER_BUS
 * it also return bool values which tells is TimeToEmpty is there or not in
 * UPOWER_BUS
 * */
gboolean get_int64(GDBusProxy *proxy, const char *name, gint64 *out) {
  GVariant *v = g_dbus_proxy_get_cached_property(proxy, name);
  if (!v)
    return FALSE;
  gboolean ok = g_variant_is_of_type(v, G_VARIANT_TYPE_INT64);
  if (ok)
    *out = g_variant_get_int64(v);
  g_variant_unref(v);
  return ok;
}

void free_entry(gpointer p) {
  BattEntry *e = p;
  if (e->proxy)
    g_object_unref(e->proxy);
}

#if defined(PLUGIN_IS_XFCE)

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
  int w = (int)(size * 1.6);
  gtk_widget_set_size_request(b->area, w, size);
}

#endif
