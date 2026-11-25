<p align="center">
  <img src="ASSETS/Dise%C3%B1o%20sin%20t%C3%ADtulo.png" alt="The Legend of June" width="460">
</p>

<p align="center">
  <b>Juego de supervivencia ASCII para la terminal, inspirado en The Legend of Zelda.</b><br>
  Esquivá proyectiles, cortá con la espada y aguantá oleadas cada vez más grandes.
</p>

<p align="center">
  <img alt="C++17" src="https://img.shields.io/badge/C%2B%2B-17-00599C?logo=c%2B%2B&logoColor=white">
  <img alt="ncurses" src="https://img.shields.io/badge/render-ncurses-2b2b2b">
  <img alt="Licencia: Unlicense" src="https://img.shields.io/badge/licencia-Unlicense-4caf50">
</p>

---

## El juego

Sos `@` en una arena de 60×20. Los enemigos (`S` soldado, `A` arquero, `M` mago) te
persiguen y disparan; vos tenés una espada de corto alcance y los corazones `<3` que
suelen soltar al morir. Cada oleada trae más enemigos y cambia el mapa.

Oleada *n* = `2n − 1` enemigos (tope 60). Cada baja vale 10 puntos. Sobrevivir es todo.

## Controles

**Enter** inicia partida, **flechas** o **WASD** mueven, **Espacio** (o **J**) ataca,
**P** pausa y **Q** / **Esc** sale.

## Música

El menú y la partida tienen su propia pista en `MUSIC/` (`JUNE_MENU.wav` y
`JUNE_BATTLE.wav`), en bucle mientras dure cada estado. Se reproduce con el primer
reproductor que aparezca en el `PATH` (`aplay`, `paplay`, `ffplay`, `mpv` o `play`);
si no hay ninguno, el juego funciona igual, en silencio. En Raspberry Pi OS alcanza
con `sudo apt install alsa-utils`.

## Compilar

Necesitás g++ con C++17, ncurses y una terminal de 80×24 o más.

```bash
sudo apt install build-essential libncurses-dev
make
./june
```

También sirven `make run` (compila y arranca) y `make clean`.

En Raspberry Pi OS / Debian funciona tal cual (probado en una Raspberry Pi 4 B).

## Mapas

Cinco arenas de 60×20 en texto plano (`mapa1.txt` … `mapa5.txt`), que rotan al azar
en cada oleada. Para hacer uno propio, copiá cualquiera: `#` es pared y cualquier otro
carácter es piso.

## Rendimiento

Modo de medición para comparar la versión en C++ con las rutinas en ensamblador:

```bash
make bench              # N = 10, 20, 40 y 64, sin pantalla
make bench-pantalla     # igual, midiendo también refresh()
./june --bench 64 --sin-pantalla
```

Reporta el tiempo por cuadro (lógica, pintado y `refresh()`), el costo de
`mover_entidades`, `colisiones` y `copiar_sprite`, y la fracción del cuadro que
consumen esas rutinas (el dato de la Ley de Amdahl).

## Cómo está hecho

Corre a 30 cuadros por segundo y sigue los módulos de la Etapa 1: Input Handler,
Entity System, Wave Manager, Render System y Game Engine, con un `StateManager` para
menú → juego → pausa → fin.

- **Punto fijo:** posiciones y velocidades son enteros de 32 bits con 8 bits
  fraccionarios, así el movimiento es idéntico en cualquier ARM.
- **Un `refresh()` por cuadro:** todo se dibuja en un buffer por capas (arena,
  entidades, HUD) y se manda a ncurses de una sola vez.
- **Arreglos contiguos:** enemigos, proyectiles y corazones viven en arreglos de tamaño
  fijo, sin huecos.
- **Rutinas críticas con interfaz C** (`mover_entidades`, `colisiones`,
  `copiar_sprite`), listas para reemplazarse por ensamblador ARM A32 con
  `make USAR_ASM=1`.

Archivos principales: `main.cpp` (el juego), `mapa1..5.txt` (arenas), `Makefile`
(compilación y switch `USAR_ASM=1`) y `ASSETS/` (logo).

## Estado

Etapa 1 (diseño) y Etapa 2 (C++ completo) terminadas; la Etapa 3 (rutinas en
ensamblador ARM A32) está en curso.

Proyecto del curso **Arquitectura de Computadores** — Instituto Tecnológico de Costa
Rica, II Semestre 2026. Equipo: Andrés Rodríguez Orozco, Sebastián Marín Mora,
Julián Solórzano Hernández y Abril González Morales.

## Licencia

Dominio público — [The Unlicense](LICENSE). Hacé lo que quieras con el código.
