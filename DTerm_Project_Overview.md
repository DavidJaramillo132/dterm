# DTerm — Mobile Linux Workstation

## 1. Descripción del proyecto

**DTerm** será una plataforma de terminal remota que permitirá controlar una computadora Linux desde un teléfono Android.

La idea principal es construir nuestro propio sistema, en lugar de depender exclusivamente de SSH u otras aplicaciones existentes.

El usuario podrá abrir DTerm en su teléfono, conectarse a su computadora Fedora y utilizar una terminal Linux interactiva desde el celular.

La meta final es poder ejecutar herramientas de terminal reales, como:

- Bash / Zsh
- Git
- Docker
- Node.js / npm
- Python
- SSH
- tmux
- Vim / Neovim
- `claude` / Claude Code
- OpenCode
- otras aplicaciones CLI

La aplicación deberá mantener una sesión de terminal persistente y soportar características propias de una terminal real: entrada de teclado, salida de texto, colores ANSI, Ctrl+C, Tab, flechas, resize, etc.

---

# 2. Objetivo general

Construir una plataforma de administración y desarrollo remoto compuesta por:

```text
┌─────────────────────┐
│      Android        │
│                     │
│    DTerm Client     │
│                     │
│  Terminal / Files   │
│  Docker / System    │
└──────────┬──────────┘
           │
       WebSocket
           │
           ▼
┌─────────────────────┐
│     Fedora Linux    │
│                     │
│    DTerm Server     │
│                     │
│       PTY           │
│        │            │
│    Bash / Zsh       │
└─────────────────────┘
```

El proyecto tendrá dos componentes principales:

### DTerm Server

Se ejecutará en Fedora/Linux y será responsable de:

- aceptar conexiones;
- autenticar dispositivos;
- crear y administrar sesiones;
- crear una PTY;
- ejecutar Bash/Zsh;
- transmitir entrada y salida;
- manejar señales;
- gestionar el tamaño de la terminal;
- mantener sesiones;
- registrar eventos;
- aplicar medidas de seguridad.

### DTerm Client

Será una aplicación Android responsable de:

- conectarse al servidor;
- mostrar la terminal;
- enviar entrada de teclado;
- recibir salida;
- gestionar comandos especiales;
- copiar y pegar;
- cambiar el tamaño de la terminal;
- administrar conexiones;
- mostrar información del sistema.

---

# 3. Principio fundamental: PTY real

DTerm no debe limitarse a ejecutar comandos mediante algo como:

```cpp
system(command);
```

La aplicación debe utilizar una **PTY (Pseudo-Terminal)**.

La arquitectura será aproximadamente:

```text
Android
   │
   │ WebSocket
   ▼
DTerm Server
   │
   ▼
PTY
   │
   ▼
Bash / Zsh
   │
   ├── stdin
   ├── stdout
   └── stderr
```

Esto permitirá ejecutar aplicaciones interactivas como:

```bash
claude
opencode
vim
nvim
tmux
htop
top
python
ssh
```

Una sesión podrá mantenerse activa y el cliente podrá reconectarse posteriormente.

---

# 4. Objetivos del proyecto

## Objetivo principal

Crear una terminal Linux remota funcional, segura y multiplataforma en la que un teléfono Android pueda controlar una computadora Fedora.

## Objetivos secundarios

- Aprender programación de sistemas con C++.
- Aprender comunicación cliente-servidor.
- Aprender WebSocket/TCP.
- Aprender PTY y procesos Linux.
- Aprender desarrollo Android con Kotlin.
- Aprender autenticación y seguridad de aplicaciones de red.
- Aprender gestión profesional de proyectos con Git y GitHub.
- Crear documentación técnica.
- Mantener un historial de desarrollo organizado.
- Construir un proyecto presentable para portafolio.

---

# 5. Roadmap por fases

El proyecto se desarrollará por etapas. Cada fase tendrá objetivos claros y dará lugar a un Milestone en GitHub.

```text
v0.1 → Local PTY
v0.2 → Network Server
v0.3 → DTerm Protocol
v0.4 → Android Client
v0.5 → Interactive Terminal
v0.6 → Security
v0.7 → Remote Access
v0.8 → Dashboard
v1.0 → Release
```

---

# 6. Fase 1 — Local PTY

## Objetivo

Crear el núcleo de DTerm y conseguir una terminal Linux interactiva funcionando localmente.

### Tareas

- Crear repositorio.
- Configurar CMake.
- Crear estructura inicial del proyecto.
- Implementar creación de una PTY.
- Crear proceso hijo.
- Ejecutar Bash/Zsh.
- Leer salida.
- Enviar entrada.
- Manejar Ctrl+C.
- Manejar Ctrl+D.
- Manejar señales.
- Manejar resize.
- Probar aplicaciones interactivas.

### Resultado esperado

```bash
./dterm
```

Debe abrir una sesión de terminal real.

### Milestone

**v0.1 — Local PTY**

---

# 7. Fase 2 — Network Server

## Objetivo

Convertir DTerm en un servidor al que otro dispositivo pueda conectarse.

### Tareas

- Crear servidor de red.
- Definir puerto de comunicación.
- Aceptar conexiones.
- Gestionar clientes.
- Crear sesiones.
- Enviar y recibir datos.
- Manejar desconexiones.
- Crear un cliente CLI temporal para pruebas.

Arquitectura:

```text
Client CLI
    │
    │ Network
    ▼
DTerm Server
    │
    ▼
PTY
    │
    ▼
Bash
```

### Milestone

**v0.2 — Network Server**

---

# 8. Fase 3 — DTerm Protocol

## Objetivo

Definir el protocolo utilizado entre el cliente y el servidor.

El protocolo deberá distinguir diferentes tipos de mensajes.

Ejemplo conceptual:

```text
CONNECT
AUTH
INPUT
OUTPUT
RESIZE
SIGNAL
PING
PONG
DISCONNECT
```

También se deberán definir:

- formato de mensajes;
- versionado del protocolo;
- códigos de error;
- identificación de sesión;
- identificación del dispositivo;
- heartbeats;
- reconexión.

Ejemplo:

```text
Client → Server
INPUT: "ls\n"

Server → Client
OUTPUT: "Documents\nDownloads\nProjects\n"
```

### Milestone

**v0.3 — DTerm Protocol**

---

# 9. Fase 4 — Android Client

## Objetivo

Crear la aplicación Android.

### Tecnologías

- Kotlin
- Android Studio
- Jetpack Compose
- WebSocket

La aplicación tendrá inicialmente:

- pantalla de conexión;
- dirección del servidor;
- autenticación;
- terminal;
- estado de conexión;
- desconexión/reconexión.

Arquitectura:

```text
Android
   │
   ├── UI
   ├── Connection Manager
   ├── WebSocket Client
   └── Terminal Emulator
```

### Milestone

**v0.4 — Android Client**

---

# 10. Fase 5 — Terminal interactiva

## Objetivo

Convertir la interfaz Android en una terminal realmente utilizable.

Debe soportar:

- texto;
- colores ANSI;
- scroll;
- selección;
- copiar;
- pegar;
- Tab;
- Ctrl;
- Esc;
- flechas;
- Home/End;
- Ctrl+C;
- Ctrl+D;
- Ctrl+Z;
- resize;
- múltiples sesiones.

Se probarán aplicaciones como:

```bash
claude
opencode
vim
nvim
tmux
htop
docker
git
ssh
```

### Milestone

**v0.5 — Interactive Terminal**

---

# 11. Fase 6 — Seguridad

## Objetivo

Evitar que el servidor exponga una terminal Linux sin protección.

Se implementarán:

- autenticación;
- tokens;
- TLS;
- autorización de dispositivos;
- revocación de dispositivos;
- expiración de sesiones;
- rate limiting;
- logs;
- validación de mensajes;
- protección contra conexiones no autorizadas.

Arquitectura:

```text
Android
   │
   │ TLS
   ▼
Authentication
   │
   ▼
Authorization
   │
   ▼
Session Manager
   │
   ▼
PTY
```

### Importante

Durante las primeras fases, DTerm se utilizará únicamente dentro de una red local o entorno controlado.

No se deberá exponer directamente el servidor a Internet hasta implementar las medidas de seguridad necesarias.

### Milestone

**v0.6 — Security**

---

# 12. Fase 7 — Acceso remoto

## Objetivo

Permitir utilizar DTerm fuera de la red local.

Una posibilidad será utilizar una VPN como Tailscale para conectar los dispositivos sin exponer directamente el servidor.

Arquitectura:

```text
📱 Android
   │
   │ Internet
   ▼
 VPN / Tailscale
   │
   ▼
💻 Fedora
   │
   ▼
DTerm Server
```

Se investigarán y compararán alternativas antes de elegir la solución definitiva.

### Milestone

**v0.7 — Remote Access**

---

# 13. Fase 8 — Dashboard

Una vez que la terminal sea estable, DTerm podrá convertirse en una plataforma de administración Linux.

Posibles módulos:

## Terminal

Terminal interactiva completa.

## System Monitor

Mostrar:

- CPU;
- RAM;
- GPU;
- temperatura;
- almacenamiento;
- batería;
- uptime;
- procesos;
- red.

## Docker

Mostrar:

- contenedores;
- estado;
- logs;
- reiniciar;
- detener;
- iniciar.

## Files

Explorador de archivos remoto:

```text
/home/david/
├── Documents
├── Downloads
├── Projects
└── Videos
```

## Git

Información de:

- branch;
- cambios;
- commits;
- estado;
- repositorios.

## Sessions

Administración de múltiples sesiones:

```text
Session 1
└── bash

Session 2
└── claude

Session 3
└── tmux

Session 4
└── opencode
```

### Milestone

**v0.8 — Dashboard**

---

# 14. Fase 9 — Release

## Objetivo

Preparar DTerm para una primera versión estable.

Se realizará:

- limpieza del código;
- pruebas;
- documentación;
- instalación;
- configuración;
- manejo de errores;
- optimización;
- seguridad;
- empaquetado;
- documentación para usuarios;
- documentación para desarrolladores.

### Milestone

**v1.0 — DTerm Release**

---

# 15. Tecnologías

## Backend / Server

### C++

Lenguaje principal del servidor.

Se utilizará para aprender:

- programación orientada a objetos;
- memoria;
- concurrencia;
- procesos;
- sockets;
- Linux APIs;
- PTY;
- señales;
- manejo de errores.

### CMake

Sistema de construcción del proyecto.

```text
CMake
  ↓
Build
  ↓
DTerm Server
```

### Linux APIs

Especialmente:

- PTY;
- fork;
- exec;
- pipes;
- signals;
- sockets;
- procesos;
- filesystem.

---

# 16. Comunicación

Inicialmente se evaluarán:

- TCP;
- WebSocket.

La opción final dependerá de las necesidades del protocolo y del cliente Android.

El objetivo es mantener una comunicación bidireccional y persistente.

```text
Client ←──────────────→ Server
       bidirectional
       persistent
       connection
```

---

# 17. Android

## Kotlin

Lenguaje principal para la aplicación Android.

## Jetpack Compose

Framework para construir la interfaz.

## Android Studio

IDE para desarrollar y probar el cliente.

La aplicación deberá funcionar como un cliente especializado para DTerm.

---

# 18. Git y GitHub

Git será utilizado desde el primer día.

Git permitirá:

- guardar versiones;
- volver a versiones anteriores;
- crear ramas;
- comparar cambios;
- trabajar por funcionalidades;
- mantener un historial;
- experimentar sin romper la versión principal.

GitHub será utilizado para:

- alojar el código;
- Issues;
- Milestones;
- Pull Requests;
- documentación;
- releases;
- seguimiento del proyecto.

---

# 19. Commits

Los commits representan cambios concretos.

Ejemplos:

```bash
git commit -m "feat: initialize CMake project"

git commit -m "feat: create PTY session"

git commit -m "feat: launch bash process"

git commit -m "fix: handle terminal resize"
```

Se utilizará una convención consistente para los commits.

Posibles tipos:

```text
feat      Nueva funcionalidad
fix       Corrección de error
docs      Documentación
refactor  Refactorización
test      Pruebas
build     Cambios del sistema de build
chore     Mantenimiento
```

---

# 20. Issues

Los Issues representarán tareas o problemas concretos.

Ejemplo:

```text
Issue #1
Create initial CMake project

Issue #2
Implement PTY creation

Issue #3
Launch Bash process

Issue #4
Read PTY output

Issue #5
Handle terminal resize
```

Cada Issue debe tener un objetivo claro y comprobable.

---

# 21. Milestones

Un Milestone agrupa Issues relacionados con un objetivo mayor.

Ejemplo:

```text
Milestone: v0.1 — Local PTY

Issues:
#1 Create CMake project
#2 Implement PTY
#3 Launch Bash
#4 Read output
#5 Handle input
#6 Handle signals
#7 Handle resize
```

Cuando todas las tareas importantes estén completadas:

```text
v0.1 — Local PTY
        ✓ Complete
```

---

# 22. Branches

No todo el desarrollo se realizará directamente sobre `main`.

Ejemplo:

```text
main
 │
 ├── feature/pty
 │
 ├── feature/network-server
 │
 ├── feature/websocket
 │
 └── feature/android-client
```

Una funcionalidad podrá desarrollarse en su propia rama y posteriormente integrarse.

Ejemplo:

```bash
git checkout -b feature/pty
```

Después:

```bash
git add .
git commit -m "feat: implement PTY session"
```

Y finalmente se integrará mediante Pull Request.

---

# 23. Pull Requests

Los Pull Requests permitirán revisar cambios antes de integrarlos.

Flujo:

```text
Issue
  ↓
Branch
  ↓
Development
  ↓
Commits
  ↓
Pull Request
  ↓
Review
  ↓
Merge
```

Aunque el proyecto sea individual, este flujo ayudará a aprender una metodología utilizada en equipos profesionales.

---

# 24. Versionado

Se utilizará versionado progresivo:

```text
v0.1
v0.2
v0.3
...
v1.0
```

Las versiones iniciales serán de desarrollo.

La versión `v1.0` representará la primera versión estable.

---

# 25. Testing

El proyecto deberá incorporar pruebas progresivamente.

Se probarán:

- creación de sesiones;
- conexión;
- desconexión;
- autenticación;
- comandos;
- señales;
- resize;
- reconexión;
- múltiples sesiones;
- errores de red;
- seguridad;
- Android.

Se intentará automatizar la mayor cantidad de pruebas posible.

---

# 26. Documentación

El repositorio tendrá documentación desde el comienzo.

Estructura propuesta:

```text
dterm/
├── README.md
├── LICENSE
├── CONTRIBUTING.md
├── SECURITY.md
├── CHANGELOG.md
├── docs/
│   ├── architecture.md
│   ├── protocol.md
│   ├── development.md
│   └── security.md
│
├── server/
├── android/
├── protocol/
└── tests/
```

---

# 27. Arquitectura final

La arquitectura prevista para la versión completa:

```text
                    DTerm
                      │
       ┌──────────────┴──────────────┐
       │                             │
 Android Client                 Linux Server
       │                             │
       │                         C++ / CMake
       │                             │
       │                        Session Manager
       │                             │
       │                         Auth / TLS
       │                             │
       │                         PTY Manager
       │                             │
       │                             ▼
       │                         Bash / Zsh
       │                             │
       └──────── WebSocket ──────────┘
```

Módulos adicionales:

```text
Linux Server
├── Network
├── Protocol
├── Authentication
├── Session Manager
├── PTY Manager
├── Process Manager
├── System Monitor
├── Docker Manager
├── File Manager
└── Logging
```

---

# 28. Posibles funcionalidades futuras

Después de v1.0 podrían estudiarse:

- Wake-on-LAN;
- notificaciones del PC al celular;
- transferencia de archivos;
- portapapeles bidireccional;
- ejecución de comandos favoritos;
- múltiples computadoras;
- perfiles de conexión;
- sincronización;
- acceso biométrico;
- widgets Android;
- modo tablet;
- soporte para otros clientes;
- aplicación de escritorio;
- API pública.

---

# 29. Filosofía de desarrollo

DTerm no se desarrollará intentando construir todo de una vez.

La estrategia será:

```text
Aprender
   ↓
Implementar
   ↓
Probar
   ↓
Documentar
   ↓
Commit
   ↓
Issue
   ↓
Milestone
   ↓
Siguiente fase
```

Cada fase deberá dejar una versión funcional.

No se avanzará a la siguiente fase simplemente porque el código "parezca funcionar". Se deberán definir criterios de aceptación para cada milestone.

---

# 30. Objetivo educativo

DTerm será también un proyecto de aprendizaje.

Durante su desarrollo se estudiarán:

### C++

- clases;
- RAII;
- smart pointers;
- concurrencia;
- threads;
- sockets;
- manejo de errores;
- CMake.

### Linux

- procesos;
- señales;
- PTY;
- shells;
- permisos;
- filesystem;
- sockets;
- servicios.

### Redes

- TCP;
- WebSocket;
- protocolos;
- autenticación;
- TLS;
- latencia;
- reconexión.

### Android

- Kotlin;
- Compose;
- networking;
- estado;
- arquitectura de aplicaciones.

### Git/GitHub

- commits;
- branches;
- Issues;
- Milestones;
- Pull Requests;
- Releases;
- tags;
- GitHub Actions.

---

# 31. Primera meta

La primera meta no será Android.

Será conseguir esto:

```bash
$ ./dterm

DTerm v0.1
────────────────────────

david@fedora:~$ 
```

Y que detrás de esa interfaz exista una **PTY real ejecutando Bash/Zsh**.

Una vez conseguido:

```bash
david@fedora:~$ claude
```

o:

```bash
david@fedora:~$ opencode
```

deberá funcionar correctamente en la sesión.

Después comenzaremos a llevar esa sesión desde la red hacia Android.

---

# 32. Definición de éxito de v1.0

DTerm podrá considerarse una primera versión estable cuando un usuario pueda:

1. Instalar DTerm Server en Fedora/Linux.
2. Ejecutar el servidor.
3. Registrar/autenticar un dispositivo Android.
4. Conectarse de forma segura.
5. Abrir una sesión de terminal.
6. Ejecutar comandos normales.
7. Ejecutar aplicaciones interactivas.
8. Utilizar herramientas como Git, Docker, Claude Code y OpenCode.
9. Desconectarse y reconectarse a una sesión.
10. Utilizar DTerm remotamente mediante una red segura.
11. Consultar información básica del sistema.
12. Gestionar sesiones.
13. Consultar documentación del proyecto.
14. Utilizar una versión etiquetada y reproducible.

---

# 33. Resultado esperado

Al finalizar el proyecto, DTerm será una plataforma de administración y desarrollo remoto para Linux que permitirá utilizar una computadora Fedora desde un teléfono Android mediante una terminal interactiva propia.

Más allá del producto final, el proyecto permitirá adquirir experiencia práctica en:

**C++ + Linux + redes + PTY + WebSocket + Android + seguridad + Git/GitHub + arquitectura de software.**

La prioridad será construir una base sólida antes de agregar funcionalidades avanzadas.

---

## Roadmap resumido

```text
v0.1  Local PTY
 │
 ├── C++
 ├── Linux
 ├── PTY
 └── Bash/Zsh
       │
       ▼
v0.2  Network Server
 │
 ├── TCP/WebSocket
 └── Sessions
       │
       ▼
v0.3  DTerm Protocol
 │
 ├── Messages
 ├── Authentication
 └── Session protocol
       │
       ▼
v0.4  Android Client
 │
 ├── Kotlin
 ├── Compose
 └── WebSocket
       │
       ▼
v0.5  Interactive Terminal
 │
 ├── ANSI
 ├── Keyboard
 ├── Resize
 └── Interactive CLI
       │
       ▼
v0.6  Security
 │
 ├── TLS
 ├── Authentication
 └── Device management
       │
       ▼
v0.7  Remote Access
 │
 └── VPN / Tailscale
       │
       ▼
v0.8  Dashboard
 │
 ├── System
 ├── Docker
 ├── Files
 └── Git
       │
       ▼
v1.0  DTerm Release
```
