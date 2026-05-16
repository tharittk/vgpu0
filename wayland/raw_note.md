### Wayland Client

wayland.app/protocols
wayland-book.com
[Wayland Components - Youtube](https://www.youtube.com/watch?v=KbryyNrMYl4)
[Tutorial](https://gaultier.github.io/blog/wayland_from_scratch.html)

Communicate with wayland server
- make an application and connect to the wayland socket.
- posix socket; XDG_RUNTIME_DIR; WAYLAND_DISPLAY;
- unix socket is reliable even though it does not have negotiation stuffs like TCP (?)
- header endianness. get_registry; opcode; wayland display id = 1 singleton.
- test read/write to the socket we just connect.
- the event format (message) protocol.
- parser for the response (raw byte -> human-readable).
- get registry gives a lot of stuff (2000+ bytes..) What comes back?
- object id 1, 3 gives ~ 60 bytes but id 2 gives a lot
- display events;error id 1?






