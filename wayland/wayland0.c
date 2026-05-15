// Wayland from scratch for my own educational purpose
// https://wayland-book.com/protocol-design/wire-protocol.html
// Message: object_id [32], size msg [16 upper], event opcode [16 lower], args[..]

#include <assert.h>
#include <errno.h>
#include<fcntl.h>
#include<stddef.h>
#include<stdint.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/mman.h>
#include<sys/socket.h>
#include<sys/time.h>
#include<sys/un.h>
#include<unistd.h>

// clearing the last two bits = round-down to nearest 4
#define roundup_4(x) ((x+3) & -4)

#define cstring_len(s) (sizeof(s) - 1)

static const uint32_t wayland_display_object_id = 1;
//static const uint16_t wayland_wl_registry_event_global = 0;
//static const uint16_t wayland_shm_pool_event_format = 0;
//static const uint16_t wayland_wl_buffer_event_release = 0;
//static const uint16_t wayland_xdg_wm_base_event_ping = 0;
//static const uint16_t wayland_xdg_toplevel_event_configure = 0;
//static const uint16_t wayland_xdg_toplevel_event_close = 1;
//static const uint16_t wayland_xdg_surface_event_configure = 0;
static const uint16_t wayland_wl_display_get_registry_opcode = 1;
//static const uint16_t wayland_wl_registry_bind_opcode = 0;
//static const uint16_t wayland_wl_compositor_create_surface_opcode = 0;
//static const uint16_t wayland_xdg_wm_base_pong_opcode = 3;
//static const uint16_t wayland_xdg_surface_ack_configure_opcode = 4;
//static const uint16_t wayland_wl_shm_create_pool_opcode = 0;
//static const uint16_t wayland_xdg_wm_base_get_xdg_surface_opcode = 2;
//static const uint16_t wayland_wl_shm_pool_create_buffer_opcode = 0;
//static const uint16_t wayland_wl_surface_attach_opcode = 1;
//static const uint16_t wayland_xdg_surface_get_toplevel_opcode = 1;
//static const uint16_t wayland_wl_surface_commit_opcode = 6;
//static const uint16_t wayland_wl_display_error_event = 0;
//static const uint32_t wayland_format_xrgb8888 = 1;
static const uint32_t wayland_header_size = 8;

typedef struct state_t state_t;
struct state_t {
	uint32_t wl_registry;
	uint32_t wl_shm;
	uint32_t wl_shm_pool;
	uint32_t wl_buffer;
	uint32_t wl_compositor;
	uint32_t wl_surface;
	uint32_t xdg_wm_base;
	uint32_t xdg_surface;
	uint32_t xdg_toplevel;
	uint32_t stride;
	uint32_t w;
	uint32_t h;
	uint32_t shm_pool_size;
	int shm_fd;
	uint8_t *shm_pool_data;
};

static int color_channels = 4;
static uint32_t wayland_current_id = 0;

static int wayland_display_connect ()
{
	/* use XDG environment variable and UNIX socket */
	char *xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
	if (xdg_runtime_dir == NULL)
		return EINVAL;

	uint64_t xdg_runtime_dir_len = strlen(xdg_runtime_dir);

	struct sockaddr_un addr = {.sun_family = AF_UNIX};
	uint64_t socket_path_len = 0;

	memcpy(addr.sun_path, xdg_runtime_dir, xdg_runtime_dir_len);
	socket_path_len += xdg_runtime_dir_len;

	// concatenate with wayland
	addr.sun_path[socket_path_len++] = '/';
	char *wayland_display = getenv("WAYLAND_DISPLAY");
	if (wayland_display == NULL) {
		char *wayland_display_default = "wayland-0";
		uint64_t wayland_display_default_len = cstring_len(wayland_display_default);
		memcpy(addr.sun_path + socket_path_len, wayland_display_default, wayland_display_default_len);
		socket_path_len += wayland_display_default_len;
	} else {
		uint64_t wayland_display_len = cstring_len(wayland_display);
		memcpy(addr.sun_path + socket_path_len, wayland_display, wayland_display_len);
		socket_path_len += wayland_display_len;
	}

	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd == -1)
		exit(errno);

	if (connect(fd, (struct sockaddr *)&addr, socket_path_len) == -1)
		exit(errno);

	return fd;
}

/* utilities for writing u16, u32, string to buffer */
static void buf_write_u32 (char *buf, uint64_t *buf_size, uint64_t buf_cap, uint32_t val)
{
	assert (*buf_size + 4 <= buf_cap);

	*(uint32_t *) (buf + *buf_size) = val;
	*buf_size += 4;
}
static void buf_write_u16 (char *buf, uint64_t *buf_size, uint64_t buf_cap, uint16_t val)
{
	assert (*buf_size + 2 <= buf_cap);
	*(uint16_t *) (buf + *buf_size) = val;
	*buf_size += 2;
}
static void buf_write_string(char *buf, uint64_t *buf_size, uint64_t buf_cap, char *src, uint32_t src_len)
{
	assert(*buf_size + src_len <= buf_cap);
	memcpy(buf + *buf_size, src, src_len);
	// could be not 4-byte align. Need round up
	*buf_size += src_len;
}

/* utilities for read u16, u32, string to buffer */
// note: we are going to advance *buf as well
static uint32_t buf_read_u32(char **buf, uint64_t *buf_size)
{
	assert(*buf_size >= 4);
	uint32_t val = *(uint32_t*)(*buf);
	*(buf) += 4;
	*(buf_size) -= 4;
	return val;
}
static uint16_t buf_read_u16(char **buf, uint64_t *buf_size)
{
	assert(*buf_size >= 2);
	uint16_t val = *(uint16_t*)(*buf);
	*(buf) += 2;
	*(buf_size) -= 2;
	return val;
}
static void buf_read_nchar(char **buf, uint64_t *buf_size, char *dst, uint64_t n)
{
	assert(*buf_size >= n);
	memcpy(dst, *buf, n);
	*buf += n;
	*buf_size -= n;
}

/* get registry for the new object id, return the new object id */
static uint32_t wayland_wl_display_get_registry(int fd)
{
	// header
	char msg[128];
	uint64_t msg_size= 0;
	buf_write_u32(msg, &msg_size, sizeof(msg), wayland_display_object_id);
	uint16_t msg_announced_size = wayland_header_size + sizeof(wayland_current_id);

	assert (roundup_4(msg_announced_size) == msg_announced_size);

	buf_write_u16(msg, &msg_size, sizeof(msg), msg_announced_size);
	buf_write_u16(msg, &msg_size, sizeof(msg), wayland_wl_display_get_registry_opcode);

	// content
	wayland_current_id++;
	buf_write_u32(msg, &msg_size, sizeof(msg), wayland_current_id);

	// write to socket
	if (msg_size != send(fd, msg, msg_size, MSG_DONTWAIT))
		exit(errno);

	printf("-> wl_display@%u.get_registry: wl_registry=%u\n", wayland_display_object_id, wayland_current_id);

	return wayland_current_id;
}

// note: the wayland app protocol suggests that there is a new api in which the 
// interface and version are omitted.
static uint32_t wayland_wl_registry_bind (int fd, uint32_t registry, uint32_t name,
		char *interface, uint32_t interface_len, uint32_t version)
{
	/* Current wl_compositor */
	return 0;
}

static uint32_t wayland_wl_compositor_create_surface (int fd, state_t *state);
static uint32_t wayland_xdg_wm_base_get_xdg_surface (int fd, state_t *state);
static uint32_t wayland_xdg_surface_get_top_level (int fd, state_t *state);

/* Create a raw shared memory file - mmap */
static void create_shared_memory_file(uint64_t size, state_t *state) {
	char name[] = "/myshm-wayland15052026";

	int fd = shm_open(name, O_RDWR | O_EXCL | O_TRUNC, 0600);
	if (fd == -1)
		exit(errno);


	assert(shm_unlink(name) != -1);
	if (ftruncate(fd, size) != -1)
		exit(errno);

	state->shm_pool_data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	assert (state->shm_pool_data != MAP_FAILED);

	state->shm_fd = fd;
}

/* Give the mmap file to wayland */
static uint32_t  wayland_wl_shm_create_pool(int fd, state_t *state);

/* Create a buffer from the shared memory pool */
static uint32_t wayland_wl_shm_pool_create_buffer(int fd, state_t *state);

/* Attach wl_buffer to wl_surface - are we missing marked as "damaged"? */
static void wayland_wl_surface_attach (int fd, state_t *state);

static void wayland_wl_surface_commit (int fd, state_t *state);

/* the server tells the client to do something */
static void wayland_handle_message (int fd, state_t *state, char **msg, uint64_t *msg_len);

int main (int argc. char *argv[])
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
		char read_buf[4096] = "";
		uint64_t read_bytes = recv(fd, read_buf, sizeof(read_buf), 0);
		if (read_bytes == 1)
			exit(errno);

		char *msg = read_buf;
		uint64_t msg_len = read_bytes;

		// Server may send a bunch of messages in one go, 
		// our parser parses a message one by one
		while (msg_len > 0)
			wayland_handle_message(fd, &state, &msg, &msg_len);

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



