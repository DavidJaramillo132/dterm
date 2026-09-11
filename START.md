# Starting the server

Everything here runs on the machine that owns the shell. For the phone side,
see [Dterm_Movil/START.md](https://github.com/DavidJaramillo132/Dterm_Movil/blob/main/START.md).

## Once, ever

The server refuses to start without a shared secret, and refuses again if the
file can be read by anyone else. Both sides must hold the same value.

```sh
mkdir -p ~/.dterm
openssl rand -hex 32 > ~/.dterm/secret
chmod 600 ~/.dterm/secret
```

## Build

```sh
cmake -B build          # first time only
cmake --build build
```

## Run

```sh
./build/dterm 4243
```

The port is the only argument and defaults to 4242. The process stays in the
foreground and prints a line whenever a session starts.

| Variable | Default | What it does |
| --- | --- | --- |
| `DTERM_SECRET` | — | Overrides `~/.dterm/secret`. Useful for a quick test. |
| `DTERM_START_DIR` | your home directory | Where a **new** session's shell begins. Never applied on reattach. |
| `DTERM_PING_SECONDS` | 30 | How long a silent client is given before it is pinged. |

To start new shells somewhere other than home:

```sh
DTERM_START_DIR=~/Projects ./build/dterm 4243
```

## Which address to give the phone

```sh
tailscale ip -4      # works from anywhere, and does not change
ip -4 -br addr       # local network only, and DHCP will change it
```

Prefer the Tailscale address. The Wi-Fi address on this machine has already
changed between one day and the next, and traffic through the tunnel is
encrypted — the DTerm protocol itself is not.

## Check it is actually listening

```sh
ss -ltn | grep 4243
```

`0.0.0.0:4243` means every interface, tunnel included.

## Sessions

```sh
ls ~/.dterm/sessions/          # one socket per live session
```

A socket left over from a reboot is harmless: the first connection gets
`ECONNREFUSED` from it, unlinks it, and starts a fresh session.

## Stopping

Ctrl-C in the terminal running the server stops **the listener only**. Sessions
are separate processes that survive on purpose — that is the whole point of the
project. Your shells and their jobs keep running, and the next server you start
finds them again.

To end a session, exit its shell from inside. A background job still holding the
PTY keeps it alive, exactly like `tmux`.

To see what is actually running:

```sh
ps -eo pid,ppid,args | grep '[d]term'
```

A session process shows `PPID 1`: it calls `setsid()` and is adopted by init, so
it belongs to no terminal and no longer descends from the listener.

> **Do not reach for `pkill -f 'dterm 4243'`.** `-f` matches whole command
> lines, including the one you are typing, so the command kills its own shell.
> This has bitten this project more than once.

## When it will not connect

| What the phone shows | What it means |
| --- | --- |
| Refused, instantly | Nothing is listening. The server is not running, or the port is wrong. |
| `EHOSTUNREACH`, instantly | A firewall rejected it. Check `firewall-cmd --list-ports`. |
| Timeout after 5 s, silently | The packet arrived and the reply never got back. See below. |

That last one is the subtle case. If the machine has **two interfaces on the
same subnet**, the reply can leave by a different interface than the request
arrived on, and something in between drops it:

```sh
ip -4 -br addr                    # two addresses in one /24 is the symptom
ip route get <the phone's IP>     # shows which interface the reply will take
```

Connecting to the other interface's address is the quick way to confirm it.
Going through Tailscale sidesteps the problem entirely.
