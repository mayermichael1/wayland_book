#include <limits.h>
#include <stdbool.h>
#include <string.h>
#include <wayland-client.h>

#include "include/xdg-shell-client-protocol.h"
#include "include/types.h"

#include "src/xdg-shell-protocol.c"
#include "src/shm_alloc.cpp"

/* Wayland code */
struct client_state {
    /* Globals */
    struct wl_display *wl_display;
    struct wl_registry *wl_registry;
    struct wl_shm *wl_shm;
    struct wl_compositor *wl_compositor;
    struct xdg_wm_base *xdg_wm_base;
    /* Objects */
    struct wl_surface *wl_surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
};

static void
wl_buffer_release(void *data, wl_buffer *wl_buffer)
{
    (void)data;
    wl_buffer_destroy(wl_buffer);
}

static const wl_buffer_listener wl_buffer_listener = {
    .release = wl_buffer_release,
};

static wl_buffer *
draw_frame(client_state *state)
{
    const int width = 640, height = 480;
    int stride = width * 4;
    int size = stride * height;

    int fd = allocate_shm_file(size);
    if (fd == -1) {
        return NULL;
    }

    u32 *data = (u32*)mmap(
        NULL,
        size,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        0
    );

    if (data == MAP_FAILED)
    {
        close(fd);
        return NULL;
    }

    wl_shm_pool *pool = wl_shm_create_pool(state->wl_shm, fd, size);
    wl_buffer *buffer = wl_shm_pool_create_buffer(
        pool,
        0,
        width,
        height,
        stride,
        WL_SHM_FORMAT_XRGB8888
    );

    //TODO: why is this destroyed and closed here
    wl_shm_pool_destroy(pool); 
    close(fd);

    /* Draw checkerboxed background */
    for (int y = 0; y < height; ++y) 
    {
        for (int x = 0; x < width; ++x) 
        {
            if ((x + y / 8 * 8) % 16 < 8)
                data[y * width + x] = 0xFF666666;
            else
                data[y * width + x] = 0xFFEEEEEE;
        }
    }

    munmap(data, size);
    wl_buffer_add_listener(buffer, &wl_buffer_listener, NULL);
    return buffer;
}

static void
xdg_surface_configure(void *data, xdg_surface *xdg_surface, u32 serial)
{
    client_state *state = (client_state*)data;
    xdg_surface_ack_configure(xdg_surface, serial);

    wl_buffer *buffer = draw_frame(state);
    wl_surface_attach(state->wl_surface, buffer, 0, 0);
    wl_surface_commit(state->wl_surface);
}

static xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

void
xdg_wm_base_ping(void *data, xdg_wm_base *xdg_wm_base, u32 serial)
{
    (void)data;
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

void
registry_global(void *data, wl_registry *wl_registry, u32 name, 
    const char *interface, u32 version)
{
    (void)version;
    client_state *state = (client_state*)data;

    if (strcmp(interface, wl_shm_interface.name) == 0) 
    {
        state->wl_shm = (wl_shm*)wl_registry_bind( 
            wl_registry, 
            name,
            &wl_shm_interface,
            1
        );
    } 
    else if (strcmp(interface, wl_compositor_interface.name) == 0) 
    {
        state->wl_compositor = (wl_compositor*)wl_registry_bind( 
            wl_registry,
            name,
            &wl_compositor_interface,
            4
        );
    } 
    else if (strcmp(interface, xdg_wm_base_interface.name) == 0) 
    {
        state->xdg_wm_base = (xdg_wm_base*)wl_registry_bind(
            wl_registry,
            name,
            &xdg_wm_base_interface,
            1
        );
        xdg_wm_base_add_listener(
            state->xdg_wm_base, 
            &xdg_wm_base_listener,
            state
        );
    }
}

void
registry_global_remove(void *data, wl_registry *wl_registry, u32 name)
{
    (void)data;
    (void)wl_registry;
    (void)name;
}

static wl_registry_listener wl_registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

int
main()
{
    client_state state = { };
    state.wl_display = wl_display_connect(NULL);
    state.wl_registry = wl_display_get_registry(state.wl_display);
    wl_registry_add_listener(state.wl_registry, &wl_registry_listener, &state);
    wl_display_roundtrip(state.wl_display);

    state.wl_surface = wl_compositor_create_surface(state.wl_compositor);
    state.xdg_surface = xdg_wm_base_get_xdg_surface(
        state.xdg_wm_base,
        state.wl_surface
    );
    xdg_surface_add_listener(state.xdg_surface, &xdg_surface_listener, &state);
    state.xdg_toplevel = xdg_surface_get_toplevel(state.xdg_surface);
    xdg_toplevel_set_title(state.xdg_toplevel, "Example client");
    wl_surface_commit(state.wl_surface);

    while (wl_display_dispatch(state.wl_display)) 
    {

    }

    return 0;
}
