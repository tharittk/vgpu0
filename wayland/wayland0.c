// Wayland from scratch for my own educational purpose
// https://wayland-book.com/protocol-design/wire-protocol.html
// Message: object_id [32], size msg [16 upper], event opcode [16 lower], args[..]
//

static int wayland_display_connect ()
{
	/* use XDG environment variable and UNIX socket */

	/* return a file descriptor */
	return 0;
}

/* utilities for writing u16, u32, string to buffer */
static void buf_write_u32 (char *buf, uint64_t *buf_size, uint64_t *buf_cap, uint32_t val);
static void buf_write_u16 (char *buf, uint64_t *buf_size, uint64_t *buf_cap, uint16_t val);
static void buf_write_string(char *buf, uint64_t *buf_size, uint64_t *buf_cap, char *src, uint32_t src_len);

/* utilities for read u16, u32, string to buffer */
// note: we are going to advance *buf as well
static uint32_t buf_read_u32(char **buf, uint64_t *buf_size);
static uint16_t buf_read_u32(char **buf, uint64_t *buf_size);
static uint16_t buf_read_nchar(char **buf, uint64_t *buf_size, char *dst, uint64_t n);

/* get registry for the new object id, return the new object id */
static uint32_t wayland_wl_display_get_registry(int fd)
{
	return 0;
}

// note: the wayland app protocol suggests that there is a new api in which the 
// interface and version are omitted.
static uint32_t wayland_wl_registry_bind (int fd, uint32_t registry, uint32_t name,
		char *interface, uint32_t interface_len, uint32_t version)
{
	/* Current wl_compositor */
	return 0;
}

static uint32_t wayland_wl_compositor_create_surface (int fd, state* state);
static uint32_t wayland_xdg_wm_base_get_xdg_surface (int fd, state* state);
static uint32_t wayland_xdg_surface_get_top_level (int fd, state* state);

/* Create a raw shared memory file - mmap */
static void create_shared_memory_file(uint64_t size, state* state);

/* Give the mmap file to wayland */
static uint32_t  wayland_wl_shm_create_pool(int fd, state* state);

/* Create a buffer from the shared memory pool */
static uint32_t wayland_wl_shm_pool_create_buffer(fd, &state);

/* Attach wl_buffer to wl_surface - are we missing marked as "damaged"? */
static void wayland_wl_surface_attach (int fd, state* state);

static void wayland_wl_surface_commit (int fd, state* state);

/* the server tells the client to do something */
static void wayland_handle_message (int fd, state_t *state, char **msg, uint64_t *msg_len);

int main (int argc. char **argv)
{
	/* Connect to the wayland display */
	int fd = wayland_display_connect();

	/* Get wayland display registry */
	state_t state = {
		.wl_registry= wayland_wl_display_get_registry (fd),
		.w = 117,
		.h = 150,
		// one row of rgb
		.stride = 117 * color_channels,
	};

	/* Create a shared memory file - single buffering*/
	state.shm_pool_size = state.stride * state.h;

	/* Underlying file & mmap - not yet binds with wayland */
	create_shared_memory_file(state.shm_pool_size, &state);

	/* event loop */
	while (1) {
		/* read the event from server */

		// TODO: server may send a bunch of messages in one go, 
		// our parser parses a message one by one
		while (msg_len > 0)
			wayland_handle_message();

		// Bind phase completes, need to create surfaces
		if (0){

			// wl_surface: raw pixels
			state.wl_surface = wayland_wl_compositor_create_surface(fd, &state);
			// XDG shell basic
			// xdg_surface: additional functionality for desktop
			// (?), wrap wl_surface with XDG
			state.xdg_surface = wayland_xdg_wm_base_get_xdg_surface(fd, &state);
			// xdg_toplevel: dealing with requests (think resize,
			// drag ..)
			state.xdg_toplevel = wayland_xdg_surface_get_top_level(fd, &state);
			wayland_wl_surface_commit(fd, &state);
		}

		// Render a frame
		if (state.state = STATE_SURFACE_ACKED_CONFIGURE) {
			
			// assigned shared memory pool to wayland (if not already)
			state.wl_shm_pool = wayland_wl_shm_create_pool(fd, &state);

			// create a buffer out of the shared memory pool (if not
			// already)
			state.wl_buffer = wayland_wl_shm_pool_create_buffer(fd, &state);

			// redering logic
			//
			// rendering done
			wayland_wl_surface_attach(fd, &state);
			wayland_wl_surface_commit(fd, &state);

			state.state = STATE_SURFACE_ATTACHED;
		}
	}
}



