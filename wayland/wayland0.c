// Wayland from scratch for my own educational purpose
// https://wayland-book.com/protocol-design/wire-protocol.html
// Message: object_id [32], size msg [16 upper], event opcode [16 lower], args[..]

#define _POSIX_C_SOURCE 200112L
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

// clearing the last two bits = round-down to nearest 4
#define roundup_4(x) ((x + 3) & -4)

#define cstring_len(s) (sizeof(s) - 1)

static const uint32_t wayland_display_object_id = 1;
static const uint16_t wayland_wl_registry_event_global = 0;
static const uint16_t wayland_shm_pool_event_format = 0;
static const uint16_t wayland_wl_buffer_event_release = 0;
static const uint16_t wayland_xdg_wm_base_event_ping = 0;
static const uint16_t wayland_xdg_toplevel_event_configure = 0;
static const uint16_t wayland_xdg_toplevel_event_close = 1;
static const uint16_t wayland_xdg_surface_event_configure = 0;
static const uint16_t wayland_wl_display_get_registry_opcode = 1;
static const uint16_t wayland_wl_registry_bind_opcode = 0;
static const uint16_t wayland_wl_compositor_create_surface_opcode = 0;
static const uint16_t wayland_xdg_wm_base_pong_opcode = 3;
static const uint16_t wayland_xdg_surface_ack_configure_opcode = 4;
static const uint16_t wayland_wl_shm_create_pool_opcode = 0;
static const uint16_t wayland_xdg_wm_base_get_xdg_surface_opcode = 2;
static const uint16_t wayland_wl_shm_pool_create_buffer_opcode = 0;
static const uint16_t wayland_wl_surface_attach_opcode = 1;
static const uint16_t wayland_xdg_surface_get_toplevel_opcode = 1;
static const uint16_t wayland_wl_surface_commit_opcode = 6;
static const uint16_t wayland_wl_display_error_event = 0;
static const uint32_t wayland_format_xrgb8888 = 1;
static const uint32_t wayland_header_size = 8;

typedef enum state_state_t state_state_t;
enum state_state_t
{
	STATE_NONE,
	STATE_SURFACE_ACKED_CONFIGURE,
	STATE_SURFACE_ATTACHED
};

typedef struct state_t state_t;
struct state_t
{
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
	state_state_t state;
};

static int color_channels = 4;
static uint32_t wayland_current_id = 1;

static int wayland_display_connect()
{
	/* use XDG environment variable and UNIX socket */
	char *xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
	printf("xdg run time: %s \n", xdg_runtime_dir);
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
	if (wayland_display == NULL)
	{
		char wayland_display_default[] = "wayland-0";
		uint64_t wayland_display_default_len = cstring_len(wayland_display_default);

		printf("No wayland server, use default: %s \n", wayland_display_default);
		memcpy(addr.sun_path + socket_path_len, wayland_display_default, wayland_display_default_len);
		socket_path_len += wayland_display_default_len;
	}
	else
	{
		uint64_t wayland_display_len = strlen(wayland_display);

		printf("wayland server: %s \n", wayland_display);
		memcpy(addr.sun_path + socket_path_len, wayland_display, wayland_display_len);
		socket_path_len += wayland_display_len;
	}

	printf("addr.sun_apth: %s \n", addr.sun_path);

	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd == -1)
	{
		perror("error creating socket\n");
		exit(errno);
	}

	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		perror("error connecting to fd");
		exit(errno);
	}

	printf("Connected to wayland server ok. \n");
	return fd;
}

/* utilities for writing u16, u32, string to buffer */
static void buf_write_u32(char *buf, uint64_t *buf_size, uint64_t buf_cap, uint32_t val)
{
	assert(*buf_size + 4 <= buf_cap);

	*(uint32_t *)(buf + *buf_size) = val;
	*buf_size += 4;
}
static void buf_write_u16(char *buf, uint64_t *buf_size, uint64_t buf_cap, uint16_t val)
{
	assert(*buf_size + 2 <= buf_cap);
	*(uint16_t *)(buf + *buf_size) = val;
	*buf_size += 2;
}
static void buf_write_string(char *buf, uint64_t *buf_size, uint64_t buf_cap, char *src, uint32_t src_len)
{
	assert(*buf_size + sizeof(uint32_t) + roundup_4(src_len) <= buf_cap);
	buf_write_u32(buf, buf_size, buf_cap, src_len);
	memcpy(buf + *buf_size, src, roundup_4(src_len));
	*buf_size += roundup_4(src_len);
}

/* utilities for read u16, u32, string to buffer */
// note: we are going to advance *buf as well
static uint32_t buf_read_u32(char **buf, uint64_t *buf_size)
{
	assert(*buf_size >= 4);
	uint32_t val = *(uint32_t *)(*buf);
	*(buf) += 4;
	*(buf_size) -= 4;
	return val;
}
static uint16_t buf_read_u16(char **buf, uint64_t *buf_size)
{
	assert(*buf_size >= 2);
	uint16_t val = *(uint16_t *)(*buf);
	*(buf) += 2;
	*(buf_size) -= 2;
	return val;
}
static void buf_read_n(char **buf, uint64_t *buf_size, char *dst, uint64_t n)
{
	assert(*buf_size >= n);
	memcpy(dst, *buf, n);
	*buf += n;
	*buf_size -= n;
}

/* get registry for the new object id, return the new object id */
static uint32_t wayland_wl_display_get_registry(int fd)
{
	char msg[128];
	uint64_t msg_size = 0;
	buf_write_u32(msg, &msg_size, sizeof(msg), wayland_display_object_id);
	uint16_t msg_announced_size = wayland_header_size + sizeof(wayland_current_id);

	assert(roundup_4(msg_announced_size) == msg_announced_size);

	buf_write_u16(msg, &msg_size, sizeof(msg), wayland_wl_display_get_registry_opcode);
	buf_write_u16(msg, &msg_size, sizeof(msg), msg_announced_size);

	// content
	wayland_current_id++;
	buf_write_u32(msg, &msg_size, sizeof(msg), wayland_current_id);

	assert(msg_size == roundup_4(msg_size));

	if ((int64_t)msg_size != send(fd, msg, msg_size, 0))
		exit(errno);

	printf("-> wl_display@%u.get_registry: wl_registry=%u\n", wayland_display_object_id, wayland_current_id);

	return wayland_current_id;
}

// note: the wayland app protocol suggests that there is a new api in which the
// interface and version are omitted.
static uint32_t wayland_wl_registry_bind(int fd, uint32_t registry, uint32_t name,
										 char *interface, uint32_t interface_len, uint32_t version)
{
	char msg[128];
	uint64_t msg_size = 0;
	buf_write_u32(msg, &msg_size, sizeof(msg), registry);
	uint16_t msg_announced_size = wayland_header_size + sizeof(name) +
								  sizeof(interface_len) + roundup_4(interface_len) + sizeof(version) + sizeof(wayland_current_id);

	assert(roundup_4(msg_announced_size) == msg_announced_size);

	buf_write_u16(msg, &msg_size, sizeof(msg), wayland_wl_registry_bind_opcode);
	buf_write_u16(msg, &msg_size, sizeof(msg), msg_announced_size);

	// content
	buf_write_u32(msg, &msg_size, sizeof(msg), name);
	buf_write_string(msg, &msg_size, sizeof(msg), interface, interface_len);
	buf_write_u32(msg, &msg_size, sizeof(msg), version);

	wayland_current_id++;
	buf_write_u32(msg, &msg_size, sizeof(msg), wayland_current_id);

	assert(msg_size == roundup_4(msg_size));

	if ((int64_t)msg_size != send(fd, msg, msg_size, 0))
		exit(errno);

	printf("-> wl_registry@%u.bind: name=%u interface=%.*s version=%u\n", registry, name, interface_len, interface, version);

	/* Current wl_compositor */
	return wayland_current_id;
}

static uint32_t wayland_wl_compositor_create_surface(int fd, state_t *state)
{
	char msg[512];
	uint64_t msg_size = 0;
	buf_write_u32(msg, &msg_size, sizeof(msg), state->wl_compositor);
	uint16_t msg_announced_size = wayland_header_size + sizeof(wayland_current_id);

	assert(roundup_4(msg_announced_size) == msg_announced_size);

	buf_write_u16(msg, &msg_size, sizeof(msg), wayland_wl_compositor_create_surface_opcode);
	buf_write_u16(msg, &msg_size, sizeof(msg), msg_announced_size);

	wayland_current_id++;
	buf_write_u32(msg, &msg_size, sizeof(msg), wayland_current_id);

	assert(msg_size == roundup_4(msg_size));

	if ((int64_t)msg_size != send(fd, msg, msg_size, 0))
		exit(errno);

	printf("-> wl_compositor@%u.wl_compositor_create_surface: wl_surface=%u\n", wayland_display_object_id, wayland_current_id);

	return wayland_current_id;
}

static uint32_t wayland_xdg_wm_base_get_xdg_surface(int fd, state_t *state)
{
	char msg[512];
	uint64_t msg_size = 0;
	buf_write_u32(msg, &msg_size, sizeof(msg), state->xdg_wm_base);
	uint16_t msg_announced_size = wayland_header_size + sizeof(wayland_current_id) + sizeof(state->wl_surface);

	assert(roundup_4(msg_announced_size) == msg_announced_size);

	buf_write_u16(msg, &msg_size, sizeof(msg), wayland_xdg_wm_base_get_xdg_surface_opcode);
	buf_write_u16(msg, &msg_size, sizeof(msg), msg_announced_size);

	wayland_current_id++;
	buf_write_u32(msg, &msg_size, sizeof(msg), wayland_current_id);
	buf_write_u32(msg, &msg_size, sizeof(msg), state->wl_surface);

	assert(msg_size == roundup_4(msg_size));

	if ((int64_t)msg_size != send(fd, msg, msg_size, 0))
		exit(errno);
	printf("-> xdg_wm_base@%u.get_xdg_surface: xdg_surface=%u wl_surface=%u\n", state->xdg_wm_base, wayland_current_id, state->wl_surface);

	return wayland_current_id;
}

static uint32_t wayland_xdg_surface_get_toplevel(int fd, state_t *state)
{
	char msg[512];
	uint64_t msg_size = 0;
	buf_write_u32(msg, &msg_size, sizeof(msg), state->xdg_surface);
	uint16_t msg_announced_size = wayland_header_size + sizeof(wayland_current_id);

	assert(roundup_4(msg_announced_size) == msg_announced_size);

	buf_write_u16(msg, &msg_size, sizeof(msg), wayland_xdg_surface_get_toplevel_opcode);
	buf_write_u16(msg, &msg_size, sizeof(msg), msg_announced_size);

	wayland_current_id++;
	buf_write_u32(msg, &msg_size, sizeof(msg), wayland_current_id);

	assert(msg_size == roundup_4(msg_size));

	if ((int64_t)msg_size != send(fd, msg, msg_size, 0))
		exit(errno);

	printf("-> xdg_surface@%u.get_toplevel: xdg_toplevel=%u\n", state->xdg_surface, wayland_current_id);
	return wayland_current_id;
}

/* Create a raw shared memory file - mmap */
static void create_shared_memory_file(uint64_t size, state_t *state)
{
	char name[] = "/myshm-wayland15052026";

	// Unlink first in case it exists from a previous run
	shm_unlink(name);

	int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);

	if (fd == -1)
	{
		perror("shm_open failed");
		exit(errno);
	}

	assert(shm_unlink(name) != -1);
	if (ftruncate(fd, size) == -1)
	{
		perror("ftruncate fiailed");
		exit(errno);
	}

	state->shm_pool_data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	assert(state->shm_pool_data != MAP_FAILED);

	printf("Create shared memory file ok\n");

	state->shm_fd = fd;
}

/* Give the mmap file to wayland */
static uint32_t wayland_wl_shm_create_pool(int fd, state_t *state)
{
	char msg[512];
	uint64_t msg_size = 0;
	buf_write_u32(msg, &msg_size, sizeof(msg), state->wl_shm);
	/* the tutorial shows that the file descriptor must be sent with some UNIX shinanegans */
	uint16_t msg_announced_size = wayland_header_size + sizeof(wayland_current_id) + sizeof(state->shm_pool_size);

	assert(roundup_4(msg_announced_size) == msg_announced_size);

	buf_write_u16(msg, &msg_size, sizeof(msg), wayland_wl_shm_create_pool_opcode);
	buf_write_u16(msg, &msg_size, sizeof(msg), msg_announced_size);

	wayland_current_id++;
	buf_write_u32(msg, &msg_size, sizeof(msg), wayland_current_id);
	buf_write_u32(msg, &msg_size, sizeof(msg), state->shm_pool_size);

	assert(msg_size == roundup_4(msg_size));

	// Send the file descriptor as ancillary data (what I refer to above)
	char buf[CMSG_SPACE(sizeof(state->shm_fd))] = "";

	struct iovec io = {.iov_base = msg, .iov_len = msg_size};
	struct msghdr socket_msg = {
		.msg_iov = &io,
		.msg_iovlen = 1,
		.msg_control = buf,
		.msg_controllen = sizeof(buf),
	};

	struct cmsghdr *cmsg = CMSG_FIRSTHDR(&socket_msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(state->shm_fd));

	*((int *)CMSG_DATA(cmsg)) = state->shm_fd;
	socket_msg.msg_controllen = CMSG_SPACE(sizeof(state->shm_fd));

	if (sendmsg(fd, &socket_msg, 0) == -1)
		exit(errno);

	printf("-> wl_shm@%u.create_pool: wl_shm_pool=%u\n", state->wl_shm, wayland_current_id);

	return wayland_current_id;
}

/* Create a buffer from the shared memory pool */
static uint32_t wayland_wl_shm_pool_create_buffer(int fd, state_t *state)
{
	char msg[512];
	uint64_t msg_size = 0;
	buf_write_u32(msg, &msg_size, sizeof(msg), state->wl_shm_pool);
	uint16_t msg_announced_size = wayland_header_size + sizeof(wayland_current_id) + sizeof(int) * 5;

	assert(roundup_4(msg_announced_size) == msg_announced_size);

	buf_write_u16(msg, &msg_size, sizeof(msg), wayland_wl_shm_pool_create_buffer_opcode);
	buf_write_u16(msg, &msg_size, sizeof(msg), msg_announced_size);

	wayland_current_id++;
	buf_write_u32(msg, &msg_size, sizeof(msg), wayland_current_id);
	int offset = 0;
	buf_write_u32(msg, &msg_size, sizeof(msg), offset);
	buf_write_u32(msg, &msg_size, sizeof(msg), state->w);
	buf_write_u32(msg, &msg_size, sizeof(msg), state->h);
	buf_write_u32(msg, &msg_size, sizeof(msg), state->stride);
	buf_write_u32(msg, &msg_size, sizeof(msg), wayland_format_xrgb8888);

	assert(msg_size == roundup_4(msg_size));

	if ((int64_t)msg_size != send(fd, msg, msg_size, 0))
		exit(errno);
	printf("-> wl_shm_pool@%u.create_buffer: wl_buffer=%u\n", state->wl_shm_pool, wayland_current_id);

	return wayland_current_id;
}

/* Attach wl_buffer to wl_surface - are we missing marked as "damaged"? */
static void wayland_wl_surface_attach(int fd, state_t *state)
{
	char msg[512];
	uint64_t msg_size = 0;
	buf_write_u32(msg, &msg_size, sizeof(msg), state->wl_surface);
	uint16_t msg_announced_size = wayland_header_size + sizeof(state->wl_buffer) + sizeof(int) * 2;

	assert(roundup_4(msg_announced_size) == msg_announced_size);

	buf_write_u16(msg, &msg_size, sizeof(msg), wayland_wl_surface_attach_opcode);
	buf_write_u16(msg, &msg_size, sizeof(msg), msg_announced_size);

	buf_write_u32(msg, &msg_size, sizeof(msg), state->wl_buffer);
	/* document says setting x, y not zero is discourage */
	int x = 0, y = 0;
	buf_write_u32(msg, &msg_size, sizeof(msg), x);
	buf_write_u32(msg, &msg_size, sizeof(msg), y);

	assert(msg_size == roundup_4(msg_size));

	if ((int64_t)msg_size != send(fd, msg, msg_size, 0))
		exit(errno);
	printf("-> wl_surface@%u.attach: wl_buffer=%u\n", state->wl_surface, state->wl_buffer);
}

static void wayland_wl_surface_commit(int fd, state_t *state)
{
	char msg[128];
	uint64_t msg_size = 0;
	buf_write_u32(msg, &msg_size, sizeof(msg), state->wl_surface);
	uint16_t msg_announced_size = wayland_header_size;

	assert(roundup_4(msg_announced_size) == msg_announced_size);

	buf_write_u16(msg, &msg_size, sizeof(msg), wayland_wl_surface_commit_opcode);
	buf_write_u16(msg, &msg_size, sizeof(msg), msg_announced_size);

	assert(msg_size == roundup_4(msg_size));

	if ((int64_t)msg_size != send(fd, msg, msg_size, 0))
		exit(errno);
	printf("-> wl_surface@%u.commit: \n", state->wl_surface);
}

static void wayland_xdg_surface_ack_configure(int fd, state_t *state, uint32_t configure)
{
	char msg[128] = "";
	uint64_t msg_size = 0;
	buf_write_u32(msg, &msg_size, sizeof(msg), state->xdg_surface);

	buf_write_u16(msg, &msg_size, sizeof(msg), wayland_xdg_surface_ack_configure_opcode);

	uint16_t msg_announced_size = wayland_header_size + sizeof(configure);
	assert(roundup_4(msg_announced_size) == msg_announced_size);
	buf_write_u16(msg, &msg_size, sizeof(msg), msg_announced_size);

	buf_write_u32(msg, &msg_size, sizeof(msg), configure);

	if ((int64_t)msg_size != send(fd, msg, msg_size, 0))
		exit(errno);

	printf("-> xdg_surface@%u.ack_configure: configure=%u\n", state->xdg_surface, configure);
}

static void wayland_xdg_wm_base_pong(int fd, state_t *state, uint32_t ping)
{
	char msg[128] = "";
	uint64_t msg_size = 0;
	buf_write_u32(msg, &msg_size, sizeof(msg), state->xdg_wm_base);

	buf_write_u16(msg, &msg_size, sizeof(msg), wayland_xdg_wm_base_pong_opcode);

	uint16_t msg_announced_size = wayland_header_size + sizeof(ping);
	assert(roundup_4(msg_announced_size) == msg_announced_size);
	buf_write_u16(msg, &msg_size, sizeof(msg), msg_announced_size);

	buf_write_u32(msg, &msg_size, sizeof(msg), ping);

	if ((int64_t)msg_size != send(fd, msg, msg_size, 0))
		exit(errno);

	printf("-> xdg_wm_base@%u.pong: ping=%u\n", state->xdg_wm_base, ping);
}

/* the server tells the client to do something */
static void wayland_handle_message(int fd, state_t *state, char **msg, uint64_t *msg_len)
{
	/* Gigantic switch-case style message parsing */
	assert(*msg_len >= 8);

	uint32_t object_id = buf_read_u32(msg, msg_len);
	assert(object_id <= wayland_current_id);

	uint16_t opcode = buf_read_u16(msg, msg_len);

	uint16_t announced_size = buf_read_u16(msg, msg_len);
	assert(roundup_4(announced_size) <= announced_size);

	uint32_t header_size = sizeof(object_id) + sizeof(opcode) + sizeof(announced_size);
	assert(announced_size <= header_size + *msg_len);

	if (object_id == state->wl_registry && opcode == wayland_wl_registry_event_global)
	{
		uint32_t name = buf_read_u32(msg, msg_len);
		uint32_t interface_len = buf_read_u32(msg, msg_len);
		uint32_t padded_interface_len = roundup_4(interface_len);

		char interface[512] = "";

		buf_read_n(msg, msg_len, interface, padded_interface_len);

		assert(padded_interface_len <= cstring_len(interface));
		// The length includes the NULL terminator.
		assert(interface[interface_len - 1] == 0);

		uint32_t version = buf_read_u32(msg, msg_len);

		printf("<- wl_registry@%u.global: name=%u interface=%.*s version=%u\n", state->wl_registry, name, interface_len, interface, version);

		assert(announced_size == sizeof(object_id) + sizeof(announced_size) +
									 sizeof(opcode) + sizeof(name) +
									 sizeof(interface_len) + padded_interface_len +
									 sizeof(version));

		char wl_shm_interface[] = "wl_shm";
		if (strcmp(wl_shm_interface, interface) == 0)
		{
			state->wl_shm = wayland_wl_registry_bind(fd, state->wl_registry, name, interface, interface_len, version);
		}

		char xdg_wm_base_interface[] = "xdg_wm_base";
		if (strcmp(xdg_wm_base_interface, interface) == 0)
		{
			state->xdg_wm_base = wayland_wl_registry_bind(fd, state->wl_registry, name, interface, interface_len, version);
		}

		char wl_compositor_interface[] = "wl_compositor";
		if (strcmp(wl_compositor_interface, interface) == 0)
		{
			state->wl_compositor = wayland_wl_registry_bind(fd, state->wl_registry, name, interface, interface_len, version);
		}

		return;
	}
	else if (object_id == wayland_display_object_id && opcode == wayland_wl_display_error_event)
	{
		uint32_t target_object_id = buf_read_u32(msg, msg_len);
		uint32_t code = buf_read_u32(msg, msg_len);
		char error[512] = "";
		uint32_t error_len = buf_read_u32(msg, msg_len);
		buf_read_n(msg, msg_len, error, roundup_4(error_len));

		fprintf(stderr, "fatal error: target_object_id=%u code=%u error=%s\n",
				target_object_id, code, error);
		exit(EINVAL);
	}
	else if (object_id == state->wl_shm && opcode == wayland_shm_pool_event_format)
	{

		uint32_t format = buf_read_u32(msg, msg_len);
		printf("<- wl_shm: format=%#x\n", format);

		return;
	}
	else if (object_id == state->wl_buffer && opcode == wayland_wl_buffer_event_release)
	{
		// No-op, for now.
		printf("<- xdg_wl_buffer@%u.release\n", state->wl_buffer);

		return;
	}
	else if (object_id == state->xdg_wm_base && opcode == wayland_xdg_wm_base_event_ping)
	{
		uint32_t ping = buf_read_u32(msg, msg_len);
		printf("<- xdg_wm_base@%u.ping: ping=%u\n", state->xdg_wm_base, ping);
		wayland_xdg_wm_base_pong(fd, state, ping);

		return;
	}
	else if (object_id == state->xdg_toplevel &&
			 opcode == wayland_xdg_toplevel_event_configure)
	{
		uint32_t w = buf_read_u32(msg, msg_len);
		uint32_t h = buf_read_u32(msg, msg_len);
		uint32_t len = buf_read_u32(msg, msg_len);
		char buf[256] = "";
		assert(len <= sizeof(buf));
		buf_read_n(msg, msg_len, buf, len);

		printf("<- xdg_toplevel@%u.configure: w=%u h=%u states[%u]\n",
			   state->xdg_toplevel, w, h, len);

		return;
	}
	else if (object_id == state->xdg_surface &&
			 opcode == wayland_xdg_surface_event_configure)
	{
		uint32_t configure = buf_read_u32(msg, msg_len);
		printf("<- xdg_surface@%u.configure: configure=%u\n", state->xdg_surface,
			   configure);
		wayland_xdg_surface_ack_configure(fd, state, configure);
		state->state = STATE_SURFACE_ACKED_CONFIGURE;

		return;
	}
	else if (object_id == state->xdg_toplevel && opcode == wayland_xdg_toplevel_event_close)
	{
		printf("<- xdg_toplevel@%u.close\n", state->xdg_toplevel);
		exit(0);
	}

	fprintf(stderr, "unhandled: object_id=%u opcode=%u announced_size=%u\n", object_id, opcode, announced_size);
	// Skip unhandled event payload
	uint32_t payload_size = announced_size - (sizeof(object_id) + sizeof(opcode) + sizeof(announced_size));
	if (payload_size > 0)
	{
		char skip[512];
		assert(payload_size <= sizeof(skip));
		buf_read_n(msg, msg_len, skip, payload_size);
	}
}

int main(int argc, char *argv[])
{
	/* Connect to the wayland display */
	int fd = wayland_display_connect();

	/* Get wayland display registry */
	state_t state = {
		.wl_registry = wayland_wl_display_get_registry(fd),
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
	while (1)
	{
		/* read the event from server */
		char read_buf[4096] = "";
		int64_t read_bytes = recv(fd, read_buf, sizeof(read_buf), 0);
		if (read_bytes == -1)
			exit(errno);
		if (read_bytes == 0)
			exit(0);

		char *msg = read_buf;
		uint64_t msg_len = (uint64_t)read_bytes;

		// Server may send a bunch of messages in one go,
		// our parser parses a message one by one
		while (msg_len > 0)
			wayland_handle_message(fd, &state, &msg, &msg_len);

		// Bind phase completes, need to create surfaces
		if (state.wl_compositor != 0 && state.shm_fd != 0 && state.xdg_wm_base != 0 && state.wl_surface == 0)
		{
			assert(state.state == STATE_NONE);

			// wl_surface: raw pixels
			state.wl_surface = wayland_wl_compositor_create_surface(fd, &state);
			// XDG shell basic
			// xdg_surface: additional functionality for desktop
			// (?), wrap wl_surface with XDG
			state.xdg_surface = wayland_xdg_wm_base_get_xdg_surface(fd, &state);
			// xdg_toplevel: dealing with requests (think resize,
			// drag ..)
			state.xdg_toplevel = wayland_xdg_surface_get_toplevel(fd, &state);
			wayland_wl_surface_commit(fd, &state);
		}

		// Render a frame
		if (state.state == STATE_SURFACE_ACKED_CONFIGURE)
		{
			// assigned shared memory pool to wayland (if not already)
			if (state.wl_shm_pool == 0)
				state.wl_shm_pool = wayland_wl_shm_create_pool(fd, &state);

			// create a buffer out of the shared memory pool (if not
			// already)
			if (state.wl_buffer == 0)
				state.wl_buffer = wayland_wl_shm_pool_create_buffer(fd, &state);

			// redering logic
			//
			uint32_t *pixels = (uint32_t *)state.shm_pool_data;
			for (uint32_t i = 0; i < state.w * state.h; i++)
			{
				uint8_t r = (i * 3 + 0) % 128;
				uint8_t g = (i * 3 + 1) % 128;
				uint8_t b = (i * 3 + 2) % 128;
				pixels[i] = (r << 16) | (g << 8) | b;
			}
			// rendering done
			wayland_wl_surface_attach(fd, &state);
			wayland_wl_surface_commit(fd, &state);

			state.state = STATE_SURFACE_ATTACHED;
		}
	}
}
