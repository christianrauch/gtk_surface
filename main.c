#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>

#include <xdg-shell-client-protocol.h>

#include <stdbool.h>
#include <EGL/egl.h>
#include <wayland-egl.h>
#include <GL/gl.h>

struct window {
    GtkWidget *gtk_win;

    struct wl_display *display;
    struct wl_compositor *compositor;
    struct wl_registry *registry;
    struct xdg_wm_base *wm_base;

    struct wl_surface *gtk_surface;
    struct xdg_surface *gtk_xdg_surface;
    struct xdg_toplevel *gtk_xdg_toplevel;

    EGLDisplay egl_display;
    EGLContext egl_context;
    struct wl_surface *surface;
    struct wl_egl_window *egl_window;
    EGLSurface egl_surface;
    struct wl_subcompositor *subcompositor;
    struct wl_subsurface *frame_subsurface;
};

static bool
setup(struct window *window)
{
    static const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_NONE
    };

    EGLint major, minor;
    EGLint n;
    EGLConfig config;

    window->egl_display =
        eglGetDisplay((EGLNativeDisplayType)window->display);

    if (eglInitialize(window->egl_display, &major, &minor) == EGL_FALSE) {
        fprintf(stderr, "Cannot initialise EGL!\n");
        return false;
    }

    if (eglBindAPI(EGL_OPENGL_API) == EGL_FALSE) {
        fprintf(stderr, "Cannot bind EGL API!\n");
        return false;
    }

    if (eglChooseConfig(window->egl_display,
                config_attribs,
                &config, 1, &n) == EGL_FALSE) {
        fprintf(stderr, "No matching EGL configurations!\n");
        return false;
    }

    window->egl_context = eglCreateContext(window->egl_display,
                               config, EGL_NO_CONTEXT, NULL);

    if (window->egl_context == EGL_NO_CONTEXT) {
        fprintf(stderr, "No EGL context!\n");
        return false;
    }

    window->surface = wl_compositor_create_surface(window->compositor);

    window->egl_window = wl_egl_window_create(window->surface,
                          200, 200);

    window->egl_surface = eglCreateWindowSurface(
                      window->egl_display, config,
                      (EGLNativeWindowType)window->egl_window,
                      NULL);

    eglMakeCurrent(window->egl_display, window->egl_surface,
               window->egl_surface, window->egl_context);

    return true;
}

static float
hue_to_channel(const float *const hue, const int n)
{
    /* convert hue to rgb channels with saturation and value equal to 1
     * https://en.wikipedia.org/wiki/HSL_and_HSV#HSV_to_RGB_alternative
     */
    const float k = fmod(n + ((*hue) * 3 / M_PI), 6);
    return 1 - MAX(0, MIN(MIN(k, 4 - k), 1));
}

static void
hue_to_rgb(const float *const hue, float (*rgb)[3])
{
    (*rgb)[0] = hue_to_channel(hue, 5);
    (*rgb)[1] = hue_to_channel(hue, 3);
    (*rgb)[2] = hue_to_channel(hue, 1);
}

static void
draw(struct window *window)
{
    struct timespec tv;
    double time;

    /* change of colour hue (HSV space) in rad/sec */
    static const float hue_change = (2 * M_PI) / 10;
    float hue;
    float rgb[3] = {0,0,0};

    clock_gettime(CLOCK_REALTIME, &tv);
    time = tv.tv_sec + tv.tv_nsec * 1e-9;

    hue = fmod(time * hue_change, 2 * M_PI);

    hue_to_rgb(&hue, &rgb);

    glClearColor(rgb[0], rgb[1], rgb[2], 1);
    glClear(GL_COLOR_BUFFER_BIT);

    eglSwapBuffers(window->egl_display, window->egl_surface);
}

static void
xdg_toplevel_configure(void *data, struct xdg_toplevel *toplevel,
              int32_t width, int32_t height,
              struct wl_array *states)
{
    struct window *window = data;
    uint32_t *p;

    wl_array_for_each(p, states) {
        uint32_t state = *p;
        switch (state) {
        case XDG_TOPLEVEL_STATE_FULLSCREEN:
            printf("XDG FULLSCREEN\n");
//            gtk_window_fullscreen(GTK_WINDOW(window->gtk_win));
            break;
        case XDG_TOPLEVEL_STATE_MAXIMIZED:
            printf("XDG MAXIMIZED\n");
//            gtk_window_maximize(GTK_WINDOW(window->gtk_win));
            break;
        }
    }

    printf("configure w/h %ix%i\n", width, height);
    fflush(stdout);
}

static void
xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel)
{
    struct window *window = data;
    gtk_window_close(GTK_WINDOW(window->gtk_win));
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    xdg_toplevel_configure,
    xdg_toplevel_close,
};

static void
surface_configure(void *data, struct xdg_surface *surface,
                  uint32_t serial)
{
    xdg_surface_ack_configure(surface, serial);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    surface_configure
};

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
    else if (strcmp(interface, wl_subcompositor_interface.name) == 0) {
        win->subcompositor = wl_registry_bind(registry, name, &wl_subcompositor_interface, 1);
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

static void
realise(GtkWidget *widget, void *data)
{
    struct window *win = data;
    GdkWindow *window;

    window = gtk_widget_get_window(widget);
//    gdk_wayland_window_set_use_custom_surface(window);
    win->gtk_surface = gdk_wayland_window_get_wl_surface(window);

    // set window as toplevel surface
//    if (win->wm_base) {
//        win->gtk_xdg_surface = xdg_wm_base_get_xdg_surface(win->wm_base, win->gtk_surface);
//        xdg_surface_add_listener(win->gtk_xdg_surface, &xdg_surface_listener, NULL);

//        win->gtk_xdg_toplevel = xdg_surface_get_toplevel(win->gtk_xdg_surface);
//        xdg_toplevel_add_listener(win->gtk_xdg_toplevel, &xdg_toplevel_listener, win);
//    }

    win->frame_subsurface = wl_subcompositor_get_subsurface(
                    win->subcompositor,
                    win->surface, win->gtk_surface);

    wl_subsurface_place_above(win->frame_subsurface, win->gtk_surface);
}

static gboolean
state_changed(GtkWidget *widget, GdkEventWindowState *event, gpointer user_data)
{
    struct window *win = user_data;

    if(event->new_window_state & GDK_WINDOW_STATE_ICONIFIED){
        printf("GDK ICONIFIED\n");
        if (win->gtk_xdg_toplevel)
            xdg_toplevel_set_minimized(win->gtk_xdg_toplevel);
    }
    else if (event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) {
        printf("GDK MAXIMIZED\n");
        if (win->gtk_xdg_toplevel)
            xdg_toplevel_set_maximized(win->gtk_xdg_toplevel);
    }
    fflush(stdout);

    return FALSE;
}

void
quit()
{
    exit(EXIT_SUCCESS);
}

int main(int argc, char* argv[])
{
    struct window window;
    window.gtk_xdg_surface = NULL;
    window.gtk_xdg_toplevel = NULL;
    window.subcompositor = NULL;

    gtk_init(&argc, &argv);

    window.display = gdk_wayland_display_get_wl_display(gdk_display_get_default());
    window.compositor = gdk_wayland_display_get_wl_compositor(gdk_display_get_default());

    setup(&window);

    window.registry = wl_display_get_registry(window.display);
    wl_registry_add_listener(window.registry, &registry_listener, &window);
    wl_display_dispatch(window.display);
    wl_display_dispatch(window.display);

    window.gtk_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(window.gtk_win, "realize", G_CALLBACK(realise), &window);
//    g_signal_connect(window.gtk_win, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(window.gtk_win, "destroy", G_CALLBACK(quit), NULL);
    g_signal_connect(window.gtk_win, "window-state-event", G_CALLBACK(state_changed), &window);
    gtk_widget_show(window.gtk_win);


//    gtk_main();

    while (true) {
        gtk_main_iteration();
        printf("loop\n"); fflush(stdout);
        draw(&window);
    };

    return 0;
}
