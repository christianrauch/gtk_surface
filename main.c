#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>

#include <xdg-shell-client-protocol.h>

struct window {
    GtkWidget *gtk_win;

    struct wl_display *display;
    struct wl_registry *registry;
    struct xdg_wm_base *wm_base;

    struct wl_surface *gtk_surface;
    struct xdg_surface *gtk_xdg_surface;
    struct xdg_toplevel *gtk_xdg_toplevel;
};

static void
realise(GtkWidget *widget, void *data)
{
    struct window *win = data;
    GdkWindow *window;

    window = gtk_widget_get_window(widget);
    gdk_wayland_window_set_use_custom_surface(window);
    win->gtk_surface = gdk_wayland_window_get_wl_surface(window);

    // set window as toplevel surface
    if (win->wm_base) {
        win->gtk_xdg_surface = xdg_wm_base_get_xdg_surface(win->wm_base, win->gtk_surface);
        win->gtk_xdg_toplevel = xdg_surface_get_toplevel(win->gtk_xdg_surface);
    }
}

static void
xdg_wm_base_ping(void *data, struct xdg_wm_base *shell, uint32_t serial)
{
    xdg_wm_base_pong(shell, serial);
}

static const struct xdg_wm_base_listener wm_base_listener = {
    xdg_wm_base_ping,
};

static void
registry_global(void *data, struct wl_registry *registry,
                uint32_t name, const char *interface, uint32_t version)
{
    struct window *win = data;

    if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        win->wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(win->wm_base, &wm_base_listener, NULL);
    }
}

void registry_global_remove(void *data,
                            struct wl_registry *wl_registry,
                            uint32_t name)
{

}

static const struct wl_registry_listener registry_listener = {
    registry_global,
    registry_global_remove
};

int main(int argc, char* argv[])
{
    struct window window;

    gtk_init(&argc, &argv);

    window.display = gdk_wayland_display_get_wl_display(gdk_display_get_default());

    window.registry = wl_display_get_registry(window.display);
    wl_registry_add_listener(window.registry, &registry_listener, &window);
    wl_display_dispatch(window.display);

    window.gtk_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(window.gtk_win, "realize", G_CALLBACK(realise), &window);
    g_signal_connect(window.gtk_win, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    gtk_widget_show(window.gtk_win);

    gtk_main();

    return 0;
}
