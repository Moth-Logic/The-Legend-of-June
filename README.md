<p align="center">
  <img src="ASSETS/Dise%C3%B1o%20sin%20t%C3%ADtulo.png" alt="The Legend of June" width="460">
</p>

<p align="center">
  <b>Un juego de supervivencia ASCII para la terminal, inspirado en The Legend of Zelda.</b><br>
  Esquivá proyectiles, cortá con la espada y aguantá oleadas cada vez más grandes.
</p>

<p align="center">
  <img alt="C++17" src="https://img.shields.io/badge/C%2B%2B-17-00599C?logo=c%2B%2B&logoColor=white">
  <img alt="ncurses" src="https://img.shields.io/badge/render-ncurses-2b2b2b">
  <img alt="ARM A32" src="https://img.shields.io/badge/etapa%203-ARM%20A32-0091BD">
  <img alt="Licencia: Unlicense" src="https://img.shields.io/badge/licencia-Unlicense-4caf50">
</p>

---

## ¿De qué va?

Sos `@` en una arena de 60×20 celdas. La primera oleada trae un soldado; la segunda ya
mezcla arqueros y magos, y a partir de ahí la cosa no para de crecer. Los enemigos te
persiguen, te disparan desde lejos y te empujan contra las paredes. Tu única defensa es
una espada de corto alcance — y los corazones `<3` que algunos enemigos sueltan al morir.

Al terminar cada oleada el mapa cambia solo, así que no hay rincón donde esconderse.

| Elemento | En la arena |
| :--- | :--- |
| **Jugador** | `@` — 5 corazones de vida, invulnerable un instante después de cada golpe |
| **Soldado** | `S` — te persigue sin descanso y te aturde al tocarte |
| **Arquero** | `A` — mantiene la distancia y dispara en línea recta cuando te tiene alineado |
| **Mago** | `M` — se queda a media distancia y dispara proyectiles apuntados a vos |
| **Proyectiles** | `-` y `\|` rectos, `*` apuntados — esquivalos; las paredes los frenan |
| **Ataque** | `+` — la zona de golpe se dibuja delante del jugador durante unos cuadros |
| **Corazón** | `<3` — 20 % de probabilidad al matar un enemigo; dura 15 segundos y cura 1 |

## Controles

| Acción | Teclas |
| :--- | :--- |
| Iniciar / volver al menú | `Enter` |
| Mover | Flechas o `W` `A` `S` `D` |
| Atacar | `Espacio` (o `J`) |
| Pausa | `P` |
| Salir | `Q`, `Esc` o `Ctrl+C` (restaura la terminal) |

## Las oleadas

La oleada *n* lanza `2n − 1` enemigos (1, 3, 5, 7…) con un tope de 60. La dificultad sube
por tres vías a la vez:

- **Cantidad** — más enemigos por oleada, hasta el tope.
- **Variedad** — oleada 1 solo soldados; desde la 2 entran arqueros y magos.
- **Resistencia** — los enemigos ganan 1 punto de vida cada 5 oleadas (los magos empiezan con 3).

Cada enemigo derrotado vale **10 puntos**. Sobrevivir es la única forma de subir el récord.

## Mapas

Cinco arenas de 60×20 celdas, en texto plano:

| Archivo | Mapa |
| :--- | :--- |
| `mapa1.txt` | Clásico |
| `mapa2.txt` | Cruz |
| `mapa3.txt` | Cuatro cuartos |
| `mapa4.txt` | Barras |
| `mapa5.txt` | Columnas |

¿Querés uno propio? Copiá cualquiera y respetá el tamaño: `#` es pared y cualquier otro
carácter es piso. El juego rota entre los cinco mapas al azar en cada oleada.

## Compilar y jugar

Necesitás **g++ con C++17**, **ncurses** y una terminal de al menos **80×24**.

```bash
sudo apt install build-essential libncurses-dev
make
./june
```

En Raspberry Pi OS / Debian funciona tal cual (probado en una **Raspberry Pi 4 B**).

```bash
make run            # compila y arranca el juego
make clean          # borra el binario y los objetos
./june --ayuda      # ayuda en la terminal
```

## Rendimiento (bench)

El juego trae un modo de medición pensado para medir el impacto de pasar las rutinas
críticas a ensamblador:

```bash
make bench              # N = 10, 20, 40 y 64, sin pantalla
make bench-pantalla     # igual, midiendo también el refresh() de ncurses
./june --bench 64 --sin-pantalla
```

Reporta el tiempo por cuadro (lógica, pintado del buffer y `refresh()`), el costo aislado
de `mover_entidades`, `colisiones` y `copiar_sprite`, y la fracción *f* del cuadro que
consumen esas rutinas — el dato que pide la **Ley de Amdahl**.

## Cómo está hecho

El juego corre a **30 cuadros por segundo** (33.3 ms por cuadro) y respeta la arquitectura
en módulos del diseño de la Etapa 1: Input Handler, Entity System, Wave Manager,
Render System y Game Engine, con un `StateManager` para menú → juego → pausa → fin.

Algunos detalles que nos gustan:

- **Punto fijo, no flotantes.** Posiciones y velocidades son enteros de 32 bits con 8 bits
  fraccionarios, así el movimiento es idéntico en cualquier ARM.
- **Un solo `refresh()` por cuadro.** Todo se dibuja primero en un buffer de caracteres por
  capas (arena → entidades → HUD) y se manda a ncurses de una sola vez: sin parpadeos.
- **Arreglos contiguos.** Enemigos, proyectiles y corazones viven en arreglos de tamaño fijo
  y `eliminar()` tapa el hueco con la última entidad, para que las rutinas críticas solo
  recorran memoria de corrido.
- **Tres rutinas críticas con interfaz C** — `mover_entidades`, `colisiones` y
  `copiar_sprite` — listas para reemplazarse en la Etapa 3 por ensamblador ARM A32 con
  `make USAR_ASM=1`.

```
main.cpp        juego completo (Etapa 2)
mapa1..5.txt    arenas de 60x20
Makefile        build C++ y switch USAR_ASM=1
ASSETS/         logo del juego
```

## Las tres etapas

| Etapa | Contenido | Estado |
| :--- | :--- | :--- |
| 1 | Diseño y arquitectura por módulos | ✅ |
| 2 | Implementación completa en C++ | ✅ |
| 3 | Rutinas críticas en ensamblador ARM A32 | 🚧 |

Proyecto del curso **Arquitectura de Computadores** — Instituto Tecnológico de Costa Rica,
II Semestre 2026.

## Equipo

| Integrante |
| :--- |
| Andrés Rodríguez Orozco |
| Sebastián Marín Mora |
| Julián Solórzano Hernández |
| Abril González Morales |

## Licencia

Dominio público — [The Unlicense](LICENSE). Hacé lo que quieras con el código.
