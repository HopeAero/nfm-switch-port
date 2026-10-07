# Need for Madness — port nativo para Nintendo Switch y Linux

[English](README.md) · **Español**

![icon](data/icon.png)

Un port no oficial de **Need for Madness** (Radicalplay, 2015) para
**Nintendo Switch** (homebrew) y **Linux**, escrito en C.

| Plataforma | Estado |
|---|---|
| Nintendo Switch | Probado en hardware (Switch v1) y en el emulador Eden |
| Linux | Compila y corre; se usa para probar cada cambio |
| PS Vita | Heredado del port original. El código del juego cambió en este fork, pero **estos cambios no se probaron en la Vita** |

Este repositorio es un fork de
[PedrelliMath/nfm-psvita-port](https://github.com/PedrelliMath/nfm-psvita-port),
el port nativo para PS Vita / Linux. Ese port parte a su vez de
[radicalarchive/nfm](https://github.com/radicalarchive/nfm), el port en
JavaScript/WebGL. Este fork agrega el target de Switch
(`native/platform/switch/`) y los arreglos y funciones que se listan abajo. Los
tres targets comparten el mismo código del juego, así que los cambios también
llegan a Linux y a la Vita.

El código fuente original nunca se publicó. El juego se descompiló y se
reescribió línea por línea, primero en JavaScript/WebGL (`web/`) y después en C
(`native/`), con la meta de un port fiel 1:1. Los assets del juego (`data/`,
`stages/`, `mycars/`, `mystages/`, `music/`) son los originales, byte por byte.

---

## Descarga e instalación

Necesitas una Switch con custom firmware (Atmosphère) y el Homebrew Menu.

1. Descarga `nfm_switch.nro` del [último release](../../releases/latest).
2. Cópialo a `sdmc:/switch/` en la tarjeta SD.
3. Abre el Homebrew Menu **desde un juego, manteniendo R mientras abre**
   (title override). No lo abras desde el Álbum: desde ahí el homebrew corre
   como applet, con mucha menos memoria.
4. Elige **Need for Madness**.

Todos los assets van dentro del `.nro`; no hay que copiar nada más. El progreso
y los ajustes se guardan en `sdmc:/switch/nfm/`.

### Si el juego se congela o crashea

El juego escribe un reporte junto a la partida para poder encontrar el
problema:

- `sdmc:/switch/nfm/freeze.txt`: el juego dejó de avanzar durante 8 segundos.
  Guarda qué estaba haciendo (pantalla, pista, tick, auto).
- `sdmc:/switch/nfm/crash.txt`: el juego tuvo un fallo y se cerró solo. Guarda
  los registros, la dirección del fallo y la cadena de llamadas como offsets
  dentro del `.nro`; `aarch64-none-elf-addr2line -e nfm_switch.elf 0x…` los
  traduce a archivo y línea. Hace falta el `.elf` del mismo build.

Adjunta el archivo cuando abras un issue.

---

## Controles

Joy-Cons (en modo portátil o separados) o Pro Controller.

| Botón | Acción |
|---|---|
| ZR | acelerar |
| ZL | frenar / reversa |
| Stick izquierdo | girar (menús: stick o cruceta) |
| B | freno de mano; mantenido en el aire, la tecla de acrobacias |
| B + stick izquierdo, en el aire | acrobacias en 8 direcciones: adelante/atrás = loops, a los lados = giros, diagonales = acrobacias combinadas |
| Stick derecho | mirar alrededor |
| A / B (menús) | confirmar / volver |
| X | cambiar cámara |
| Y | silenciar música |
| − | silenciar efectos |
| Cruceta arriba | flecha guía: pista ↔ autos |
| Cruceta abajo | radar (minimapa + velocímetro) |
| + | pausa |

---

## Qué agrega el port de Switch

**El target de Switch**
- `.nro` con todos los assets en su RomFS, partidas guardadas en la SD,
  controles con libnx y vibración (Joy-Cons y Pro Controller).
- 1920×1080 en el dock, 1280×720 en portátil.
- Contexto OpenGL 2.1 clásico con SDL2 y Mesa. Las 33 funciones de GL se cargan
  en tiempo de ejecución.
- La pantalla de Instrucciones y las tarjetas de pista muestran los botones de
  la Switch.

**60 fps fluidos**
- **Smooth Frames** (activado por defecto): la carrera sigue avanzando a
  18,9 Hz como el original, pero cada cuadro dibuja los autos y la cámara
  mezclados entre los dos últimos ticks. Eso incluye el ángulo de giro y de
  volteretas de cada auto, con fracciones de grado. La simulación no cambia.
- Limitador a 60 Hz que duerme solo lo que falta de cada cuadro de 16,7 ms.
- **Settings › Performance Test**: una carrera de 60 segundos en la pista 9 que
  maneja la IA. Muestra un resumen en pantalla y escribe un reporte completo en
  `sdmc:/switch/nfm/benchmark.txt` (fps promedio y 1% low, percentiles,
  cuadros lentos, trabajo por cuadro).

**Ajustes** (desde el menú principal y desde la pausa)
- **Graphics**: calidad de imagen (Original / Smooth / HD), distancia de
  dibujo, detalle del escenario, sombras, partículas, motion blur, Smooth
  Frames.
- **Audio**: volumen de música y de efectos.
- **Interface**: contador de FPS (apagado / FPS / detallado); nombres de los
  autos en la tabla de posiciones (apagado por defecto: el original en un
  jugador los deja en blanco).
- **Gameplay**: sacudida de pantalla, vibración.

**Cosas del original que al port de Vita le faltaban**
- El **texto** del original: Arial negrita/normal en los tamaños del original
  (como Liberation Sans, su gemela libre con las mismas métricas), con
  antialias, mayúsculas y minúsculas y todos los símbolos, y cada texto, fuente
  y posición sacados del Java. El port de Vita usaba una fuente vectorial de
  5×7 solo en mayúsculas.
- El fondo de las pistas: nubes, montañas, las estrellas de las pistas de
  noche y los parches de suelo alrededor de la pista (`clouds(`, `mountains(`,
  `density(`, `fadefrom(`, `lightson`).
- Las **colinas de tierra** (`pile(`) que ponen 31 de las 32 pistas, de 47 a
  187 cada una, dibujadas y con colisión.
- Los **puntos de ruta** de la IA (`set(...)p`): los bots siguen la pista en vez
  de apuntar de un checkpoint al siguiente.
- Acrobacias en **8 direcciones** (la Vita leía solo 4, así que las diagonales
  no hacían nada).

**Choques entre autos como en el original**
- Después de un choque, el auto se mueve con la velocidad de sus ruedas
  *después* de recortarla, como hace Java. El port en C usaba la velocidad de
  antes del recorte, así que un auto recién chocado seguía de largo dentro del
  otro; en pruebas de choque de frente, los autos quedaban encimados un tercio
  menos de veces tras el arreglo.
- El rumbo, la inclinación, el bamboleo en terreno irregular y el rebote usan
  la aritmética en double de Java donde el port en C redondeaba a float. Los
  choques de frente ahora coinciden exactos con el original, actualización por
  actualización.

**Arreglos traídos del port web** ([HopeAero/nfm](https://github.com/HopeAero/nfm))
- El freno y los cambios de marcha dividen enteros como Java (`handb / 2`,
  `swits / 2`). Los valores impares frenaban y daban una velocidad tope 0,5 más
  alta en cada tick.
- El polvo de las ruedas envejece por tick y no por cuadro. A 60 fps
  desaparecía 3 veces más rápido.
- Un congelamiento con autos muy dañados, y un tope al bucle de puntos de ruta
  de la IA.
- Validación de pistas como en Java: un modelo desconocido, demasiados objetos
  o menos de 2 checkpoints muestran *ERROR LOADING STAGE* en vez de colgar la
  carrera. Las caras de modelo demasiado grandes ya no desbordan la pila, y una
  división por cero que venga de los datos de una pista da 0.
- HUD legible en cielos oscuros: se recolorea con contraste 4.5:1 contra el
  cielo, como en el port web, en vez de dibujar recuadros.
- Volver de la pausa ya no corre 3 ticks de física de golpe.
- Proyección en 64 bits (sin desbordes con signo) y `-ffp-contract=off`, para
  que las cuentas con decimales de la Switch den lo mismo que en Java.

**Diagnóstico**
- Reportes de congelamiento y crasheo (ver arriba).

---

## Compilar

### Nintendo Switch

Con Docker y la imagen oficial de devkitPro, desde la raíz del repositorio:

```sh
docker run --rm -v "$PWD:/src" -w /src devkitpro/devkita64 bash -c \
  'cmake -S native -B native/build-switch -DNFM_PLATFORM=switch \
     -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake && \
   cmake --build native/build-switch -j'
```

En Windows (Git Bash), antepone `MSYS_NO_PATHCONV=1` al comando y monta la
ruta con la unidad, por ejemplo `-v "D:/ruta/a/nfm-switch-port:/src"`.

El resultado es `native/build-switch/platform/switch/nfm_switch.nro` (unos
13 MB), con `nfm_switch.elf` al lado.

Opciones:
- `-DNFM_SVCLOG=ON` manda stderr al log del emulador (Eden, yuzu). Ese build
  también lee `sdmc:/switch/nfm/debug_env.txt` (líneas `CLAVE=VALOR`) para los
  hooks de prueba sin pantalla.
- `-DNFM_NXLINK=ON` manda `printf`/stderr a la PC con `nxlink -s`.

### Linux

El target de Linux corre el mismo juego con SDL2 y OpenGL, con teclado
(teclas en [`CONTROLES.txt`](CONTROLES.txt)):

```sh
sudo apt install build-essential cmake libsdl2-dev libgl-dev zlib1g-dev
cmake -S native -B native/build-linux -DNFM_PLATFORM=linux -DCMAKE_BUILD_TYPE=Release
cmake --build native/build-linux -j
native/build-linux/platform/linux/nfm_linux   # ejecutar desde la raíz del repositorio
```

Tests del núcleo (en la PC, sin ventana):

```sh
cmake -S native/tests -B native/tests/build && cmake --build native/tests/build -j
cd native/tests/build && ctest
```

### PS Vita

El target de Vita es el del repositorio original: ve
[PedrelliMath/nfm-psvita-port](https://github.com/PedrelliMath/nfm-psvita-port)
para VitaSDK, vitaGL y cómo instalar el `.vpk`. **No probado en este fork:** el
código compartido del juego cambió (todo lo listado arriba), y no se verificó ni
el build de Vita ni el juego en la Vita.

---

## Lo que falta / diferencias conocidas

**Respecto al juego original**
- **Sin multijugador en línea**: el lobby del original no está portado. Solo
  existe en el port web.
- **Sin contenido de usuario**: solo las 32 pistas y los autos originales. Las
  pistas y autos hechos con el Stage Maker y el Car Maker del original
  (`mystages/`, `mycars/`) todavía no se pueden cargar.
- **Pequeñas diferencias numéricas**: unos 15 lugares todavía redondean en
  `float` donde Java usa `double`, o al revés. Producen diferencias raras de una
  unidad en la inclinación del auto, los rebotes, los tiempos de la IA y si dos
  autos se tocan o no.
- **Pistas 28 a 32**: algunas piezas de pista giradas ±90° proyectan una sombra
  un poco distinta (esas pistas no ponen `loadnew`).
- **Dos ajustes cambian el juego, no solo la imagen.** Los valores por defecto
  son fieles al original.
  - *Scenery Detail: Low* es el modo de bajo detalle del propio original, que
    también quita los choques con la decoración.
  - *Draw Distance: Far/Max* puede cambiar si la reparación de un auto lejano se
    anima o no.

**Plataformas**
- Conectar o sacar del dock en medio del juego mantiene el tamaño de pantalla
  con el que arrancó.
- Todavía no hay pantalla táctil en los menús.
- Hasta ahora lo probó una sola persona en una Switch v1. La vibración y las
  cifras finales de rendimiento no están confirmadas en hardware; el reporte del
  Performance Test ayuda con eso.
- PS Vita: ninguno de los cambios de este fork se probó en la Vita.

**Prioridad**: un port lo más fiel posible al juego original. El contenido
extra del port web (Extended Mode, los editores, autos propios …) no forma
parte del plan de este repositorio; si se pide, podría llegar más adelante en
otra rama u otro repositorio.

---

## Estructura del repositorio

| Carpeta | Contenido |
|---|---|
| `native/` | **El port nativo.** `core/` es el juego independiente de la plataforma (física, render, audio, decodificadores). `platform/common/game.c` es el bucle del juego, los menús y los ajustes; `platform/common/diag.c` tiene los reportes de congelamiento y crasheo. `platform/switch/`, `platform/vita/` y `platform/linux/` son la capa delgada de cada target. `tests/` tiene los tests del núcleo. |
| `web/` | El port JavaScript/WebGL del que se tradujo el C. |
| `decompilation/` | El Java descompilado (`java-src/`). Referencia de lectura, no se compila. |
| `java/` | El `Game.jar` original parcheado para Java moderno (`./start.sh`), junto con el original sin tocar. |
| `data/`, `stages/`, `mycars/`, `mystages/`, `music/` | Los assets originales: **no los modifiques**. `data/switch/` y `data/vita/` tienen el arte de los botones; `data/port/` tiene los textos de menú propios del port. |
| `tools/` | Scripts de apoyo (arte de botones, textos de menú, música). |

## Documentación

- [`TASKS_SWITCH.md`](TASKS_SWITCH.md): el port de Switch, qué se hizo, cómo se
  verificó y qué falta.
- [`native/PORT_SPEC.md`](native/PORT_SPEC.md): las reglas del port nativo.
- [`native/TASKS_NATIVE.md`](native/TASKS_NATIVE.md): la historia del port nativo.
- [`WORK.md`](WORK.md): descubrimientos y trampas, uno por línea.
- [`AGENTS.md`](AGENTS.md): cómo correr, medir y verificar, y los invariantes
  que no se pueden romper (sin depth buffer; una sola llamada de dibujo en orden
  de envío).
- [`web/TRANSPILE_SPEC.md`](web/TRANSPILE_SPEC.md): el contrato de traducción
  Java → código (desborde de enteros, float32).
- [`CONTROLES.txt`](CONTROLES.txt): los controles de Vita y Linux.

---

## Créditos

- **Need for Madness**: Radicalplay (Omar Waly), el juego original.
- [**radicalarchive/nfm**](https://github.com/radicalarchive/nfm): la
  descompilación y el port JavaScript/WebGL en el que se apoya todo esto.
- [**PedrelliMath/nfm-psvita-port**](https://github.com/PedrelliMath/nfm-psvita-port):
  el port nativo en C para PS Vita y Linux del que este repositorio es fork.
- [**HopeAero/nfm**](https://github.com/HopeAero/nfm): el port web del que se
  trajeron los arreglos posteriores.
- [devkitPro](https://devkitpro.org), libnx, SDL2 y Mesa por la toolchain y las
  librerías de Switch.
- [Liberation Sans](https://github.com/liberationfonts/liberation-fonts)
  (SIL Open Font License 1.1, `data/port/LiberationSans-OFL.txt`) para el texto.

*Need for Madness © Radicalplay. Un proyecto de fans sin fines comerciales, sin
relación con Radicalplay ni con Nintendo.*
