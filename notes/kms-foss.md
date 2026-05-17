### Getting pixels on screen on Linux: introduction to Kernel Mode Setting - Simon Ser
[Link](https://www.youtube.com/watch?v=haes4_Xnc5Q)

- apps talk to 'compositor' i.e., X11 or Wayland -> Kernel
- KMS is unified, don't need to write driver-specific code
- write KMS if you work with: display server (x11, wayland), media players, VR, XR, low-level control.
- Otherwise, your app talks to compositor instead.

# Getting the pixel to the screen
```C
int drm_fd = open("dev/drm/card0", O_RDWR | O_NONBLOCK);
/* see its struct def; fb, crtc, connctors, encoders */
drmModRes *resources = drmModeGetResources(drm_fd);
```
- connector: hdmi, display port, usb-c; the list of connectors can change during run-time.
- drmModeInfo (4K mode, 720x400 - something you can change)
```C
drmModeConnector *conn = drmModeGetConnector(drm_fd, conn_id);
/* for loop over all the available modes */
```
- framebuffers (`drmModeFB`); id, width, high, bpp (bit-per-pixel), handle (driver-specific)
- handle is used to import framebuffer to the KMS.
- format of frame buffer (`libdrm/drm_fourcc.h`); XRGB888, ARGB8888; be careful about endian - I guess

```C
// Allocate a buffer and get a driver-specific handle back
struct drm_mode_create_dumb create = {...};
drmIoctl(drm_fd, DRMIOCTL_MODE_CREATE_DUMP, &create);

// Create the DRM framebuffer object
drmModeAddFB2(drm_fd, ...);

// Create a memory mapping
drmIoctl(...);
void *data = mmap();

/* you can paint buffer in red by writing to the data*/
```

- Framebuffer -> Plane -> CRTC -> Connector; why don't we have just fb -> connector.
- (crtc) We may have 2 connectors wired up to a single CRTC "clone screens".
- (plane) cursor plane, desktop plane; separate them leads to better gpu resource + power management.
```C
drmModeCrtc ... 
drmSetClientCap(drm_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
drmModePlaneRes *planes = drmModeGetPlaneResources(drm_fd);
struct drmModePlane // formats supported listed there.
```
- to change these properties `drmModeObjectProperties *props = `
- the pattern: get the id, use `drmModeGetProperty(drm_fd, prop_id);
- iterate crtcs + get the crtc, iterates the planes + get the plane (primary), get the framebuffer.

- Atomic commits
- prevent flickering and bad intermediate state
- userspace set cursor plane framebuffer + move cursor but the monitor may read after the cursor plane fb is set while the cursor has not moved yet.
- complicated rollback when something goes wrong (set plane fb then move plane - kms cannot do that but fb is already set).
```C
drmSetClientCap(drm_fd, DRM_CLIENT_CAP_ATOMIC, 1);
drmModeAtomicReq *req = drmModeAtomicAlloc();
drmModeAtomicAddProperty (req, object_id, prop_id, value);
// add more properties
drmModeAtomicCommit(drm_fd, req, flags, NULL);
```
- diplaying frame buffer: fb_id, src x,y,w,h (offset of buffer), crtc x,y,w,h (offset of screen) ~ memcpy





















