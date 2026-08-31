# DTerm

A remote Linux terminal server written in C++, meant to be driven from an
Android phone. It is a from-scratch alternative to SSH built as a systems
programming exercise: PTYs, sockets, signals, framing and authentication are
all hand-written rather than delegated to a library.

## Status

| Version | What it added |
| --- | --- |
| v0.1 | Local PTY: `forkpty`, raw mode, `poll` loop |
| v0.2 | TCP server and CLI client |
| v0.3 | Framed wire protocol with window-size negotiation |
| v0.4 | HMAC-SHA256 authentication and keepalive pings |
| v0.5 | One forked process per client |
| v0.6 | Sessions that survive disconnection |

Not implemented yet: transport encryption and the Android application.

## Requirements

- Linux, a C++20 compiler, CMake 3.20+
- OpenSSL development headers (`openssl-devel` on Fedora, `libssl-dev` on Debian)

## Build

```bash
cmake -S . -B build
cmake --build build
```

Three binaries land in `build/`:

- `dterm` — the server, runs on the machine you want to reach
- `dterm-client` — the terminal client
- `protocol-test` — frame parser tests

## The shared secret

The server refuses to start without one, and both machines need the **same**
value. Generate it once and copy it to the other side:

```bash
mkdir -p ~/.dterm
openssl rand -hex 32 > ~/.dterm/secret
chmod 600 ~/.dterm/secret
```

The file must not be readable by other users; the server checks and refuses a
world-readable one. `$DTERM_SECRET` overrides the file when it is set.

The secret authenticates the client — it is never sent over the wire. The
server issues a random challenge and the client answers with
`HMAC-SHA256(secret, challenge)`.

## Run

On the machine hosting the shell:

```bash
./build/dterm            # listens on 0.0.0.0:4242
```

From anywhere on the same network:

```bash
./build/dterm-client 192.168.1.50 4242
```

With no arguments the client connects to `127.0.0.1:4242`.

## Sessions

A session is a shell with its own PTY that lives in its own process, **separate
from the network connection**. Losing the connection detaches you; it does not
kill your work. Reconnecting puts you back in the same shell, with the same
environment, the same running programs, and a replay of the last 64 KiB of
output so the screen is not blank.

```bash
./build/dterm-client 192.168.1.50 4242            # the "default" session
./build/dterm-client 192.168.1.50 4242 deploy     # a session named "deploy"
```

Session names may contain letters, digits, `-` and `_`, up to 32 characters,
because they become file names under `~/.dterm/sessions/`.

A session ends when every process holding its PTY is gone — exiting the shell
is usually enough, but a background job keeps it alive, exactly like `tmux`.
Sessions outlive the server too: restarting `dterm` does not disturb them.

Only one client is attached at a time. A second client attaching to the same
session takes it over and the previous one is detached.

## Security

Authentication is solid; **the transport is not encrypted**. Everything you
type, including passwords typed into `sudo`, travels in cleartext. Treat DTerm
as safe only on a network you trust, and tunnel it through a VPN otherwise.

## Wire protocol

Every message is a length-prefixed binary frame:

```text
 0        1                                5
 +--------+--------+--------+--------+--------+---------------+
 |  type  |          length (uint32 BE)       |    payload    |
 +--------+--------+--------+--------+--------+---------------+
```

| Type | Name | Direction | Payload |
| --- | --- | --- | --- |
| `0x00` | HELLO | client → server | `"DTRM"` + version byte |
| `0x01` | INPUT | client → server | keystrokes |
| `0x02` | OUTPUT | server → client | PTY output |
| `0x03` | RESIZE | client → server | rows, cols (uint16 BE each) |
| `0x04` | PING | both | empty |
| `0x05` | PONG | both | empty |
| `0x06` | CHALLENGE | server → client | 32 random bytes |
| `0x07` | AUTH | client → server | HMAC-SHA256 of the challenge |
| `0x08` | AUTH_OK | server → client | empty |
| `0x09` | AUTH_FAIL | server → client | empty |
| `0x0A` | ATTACH | client → server | session name, empty means `default` |

Integers are big-endian and written by hand rather than through `htonl`, so a
JVM client can read them without depending on host byte order.

A connection runs HELLO → CHALLENGE → AUTH → AUTH_OK → ATTACH before the server
spawns anything. An unauthenticated client never costs a shell process. The
current protocol version is 2.

## Layout

```text
protocol/   frame encoding and authentication, shared by both sides
server/     listener, handshake, and the session process that owns the PTY
client/     CLI client and terminal raw mode
tests/      frame parser tests
```

## Testing

```bash
./build/protocol-test
```

The parser is exercised over the same byte stream delivered whole, one byte at
a time, and in seven-byte slices, because TCP is free to split a frame anywhere.

## How a connection is served

```text
dterm (listener)
  └── fork per connection ── handshake, then copies bytes both ways
                               │
                               └── Unix socket ──> session process
                                                     └── PTY ──> bash
```

The connection process is disposable and parses nothing beyond the handshake.
The session process owns the PTY and outlives it.
