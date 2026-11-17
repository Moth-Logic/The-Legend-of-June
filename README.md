<h1 align="center">🏹 The Legend of June</h1>

<p align="center">
  <b>Una aventura ASCII de supervivencia para Raspberry Pi, inspirada en The Legend of Zelda.</b><br>
  Esquivá proyectiles, repartí espadazos y sobreviví oleadas cada vez más bravas…<br>
  todo dibujado con caracteres en tu terminal.
</p>

<p align="center">
  <img alt="C++17" src="https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white">
  <img alt="ncurses" src="https://img.shields.io/badge/terminal-ncurses-4E9A06">
  <img alt="Raspberry Pi" src="https://img.shields.io/badge/target-Raspberry%20Pi-A22846?logo=raspberrypi&logoColor=white">
  <img alt="Ensamblador" src="https://img.shields.io/badge/Etapa%203-ARM%20A32-8A2BE2">
  <img alt="Licencia" src="https://img.shields.io/badge/licencia-Unlicense-brightgreen">
  <img alt="Estado" src="https://img.shields.io/badge/estado-en%20desarrollo-yellow">
</p>

---

## 🎮 ¿Qué es esto?

**The Legend of June** es un juego de acción en ASCII para consola. El jugador se mueve por una
arena de **60×20 celdas**, ataca cuerpo a cuerpo y aguanta rondas de enemigos que lo persiguen y
le disparan. Cada enemigo derrotado puede soltar un corazón, y la dificultad sube ronda tras ronda.

El proyecto se construye en tres etapas para el curso **Arquitectura de Computadores** del TEC:

| Etapa | Enfoque | Estado |
| :--- | :--- | :---: |
| **1** | Diseño de módulos, mapas y reglas de juego | ✅ Listo |
| **2** | Implementación completa en **C++** (sin ensamblador) | 🚧 En progreso |
| **3** | Rutinas críticas reescritas en **ensamblador ARM A32** | ⏳ Pendiente |

> ⚠️ **Nota de estado:** este repositorio contiene por ahora las constantes, los tipos base,
> los mapas y las **declaraciones** de los módulos. Las implementaciones y el bucle principal
> todavía no están escritos, así que **el juego aún no se puede ejecutar**.

---

## 🖥️ Anatomía de la pantalla

La terminal mínima es de **80×24**. La arena va centrada y las filas de arriba y de abajo
quedan libres para el HUD (`ARENA_X = 10`, `ARENA_Y = 2`):

```
          0         1         2         3         4         5         
          012345678901234567890123456789012345678901234567890123456789
0         ♥♥♥♥♥   MAPA: Clasico      RONDA 3      PUNTOS 0140
1         
2         ############################################################
3         #                                                          #
4         #                                                          #
5         #                                                          #
6         #         ####                                ####         #
7         #         ####                                ####         #
8         #                         ########                         #
9         #                                                          #
10        #                   #                  #                   #
11        #                   #                  #                   #
12        #                   #                  #                   #
13        #            @      #         e        #                   #
14        #                                                          #
15        #                         ########                         #
16        #         ####                                ####         #
17        #         ####                                ####         #
18        #                                                          #
19        #                                                          #
20        #                                                          #
21        ############################################################
22        [P] pausa   [Q] salir   [ENTER] confirmar
23        terminal mínima: 80×24
```

*Vista del mapa **Clasico** (`mapa1.txt`) con `@` y `e` dibujados como ejemplo.
Los glifos definitivos de las entidades aún están por definir.*

| Glifo | Elemento |
| :---: | :--- |
| `#` | Pared |
| `@` | Jugador |
| `e` | Enemigo |
| `*` | Proyectil |
| `+` | Corazón |

* **Arena:** 60×20 celdas, con `#` como pared; fuera de la arena cuenta como pared.
* **Movimiento parejo:** las celdas de la terminal son el doble de altas que de anchas,
  por eso la velocidad vertical es la mitad de la horizontal.

---

## ⚔️ Cómo se juega

| Acción | Teclas |
| :--- | :--- |
| Mover | Flechas o `W` `A` `S` `D` |
| Atacar | `Espacio` |
| Pausa | `P` |
| Salir | `Q` o `Esc` |
| Confirmar (menús) | `Enter` |

**Reglas de combate** (valores en cuadros, a 30 FPS):

| Regla | Valor |
| :--- | :--- |
| Vida inicial del jugador | 5 corazones |
| Invulnerabilidad tras recibir golpe | 30 cuadros (1 s) |
| Zona de golpe del ataque | 4 cuadros, con 10 de espera para repetir |
| Enemigo herido | parpadea 6 cuadros, inmune 8 |
| Golpe cuerpo a cuerpo recibido | 35 cuadros de aturdimiento + 4 celdas de empuje |
| Enemigo trabado | rodea el obstáculo durante 45 cuadros |
| Corazón soltado | 20 % de probabilidad; dura 15 s y parpadea al final |

**Los tres enemigos:**

| Enemigo | Comportamiento | Velocidad (celdas/cuadro) |
| :--- | :--- | :--- |
| 🗡️ **Soldado** (`SOLDADO`) | Persigue al jugador para golpearlo de cerca | 0.50 horizontal / 0.25 vertical |
| 🏹 **Arquero** (`ARQUERO`) | Se detiene a disparar flechas | 0.16 / 0.08 |
| 🧙 **Mago** (`MAGO`) | Dispara proyectiles más lentos y erráticos | 0.14 / 0.07 |

Cada enemigo alterna entre **perseguir** (`PERSEGUIR`), **disparar** (`DISPARAR`) y **estar
herido** (`HERIDO`). Los proyectiles se mueven al doble de velocidad que quien los lanza, y no
hay `float` en ninguna parte del juego.

---

## 🗺️ Los cinco mapas

Se cargan desde archivos de texto (`mapa1.txt` … `mapa5.txt`), todos de **60×20** exactos:

| Archivo | Nombre en el HUD | Escenario |
| :--- | :--- | :--- |
| `mapa1.txt` | **Clasico** | Arena abierta con bloques en las esquinas |
| `mapa2.txt` | **Cruz** | Pasillos en cruz con cobertura lateral |
| `mapa3.txt` | **Cuatro cuartos** | Dividido en cuadrantes con puertas |
| `mapa4.txt` | **Barras** | Cuatro barras horizontales alternadas |
| `mapa5.txt` | **Columnas** | Bosque de columnas para esconderse |

¿Querés tu propio mapa? Copiá cualquiera y respetá el tamaño: bordes de `#`, 20 filas y
60 columnas. Si el archivo no existe o no es válido, el juego genera una arena por defecto.

---

## 🧠 Arquitectura

El diseño separa el juego en módulos con responsabilidades claras:

| Módulo | Responsabilidad |
| :--- | :--- |
| **Arena** | Carga el mapa, responde `esPared()`, `lineaLibre()` y busca la celda libre más cercana |
| **Input Handler** | `KeyboardReader` + `InputBuffer` (cola circular de 8 teclas) + `ActionMapper` |
| **Entity System** | Arreglos contiguos de tamaño fijo para enemigos, proyectiles y corazones |
| **Physics / colisiones** | Detección AABB por pares y movimiento en punto fijo |
| **Renderer** | Copia de sprites ASCII recortando bordes y saltando espacios |
| **Game Engine** | Bucle a 30 FPS, oleadas, estados y HUD |

**Decisiones que valen la pena resaltar:**

* 🧮 **Cero punto flotante.** Posiciones y velocidades son enteros Q24.8: 256 unidades = 1 celda.
* 🧱 **Entidades en arreglos planos** de máximo 64 elementos. Al eliminar una, la última se copia
  encima, así nunca quedan huecos y las rutinas pueden recorrer todo de corrido.
* ⌨️ **Latch de teclas.** Las terminales no avisan cuándo se suelta una tecla, así que el jugador
  sigue moviéndose 4 cuadros después de la última pulsación.
* 🐢 **Sin esperas bloqueantes.** `KeyboardReader` lee sin esperar y cada cuadro dura 33.3 ms.

**Interfaz lista para ensamblador.** Tres rutinas se declaran `extern "C"` con firma AAPCS:

```cpp
void mover_entidades(int32_t* pos, const int32_t* vel, uint32_t n);
void colisiones(const Rect* a, uint32_t na, const Rect* b, uint32_t nb, uint8_t* res);
void copiar_sprite(DatosSprite* d);
```

> En la **Etapa 3** estas tres se reescriben en ensamblador ARM A32 **sin tocar el resto del
> juego**: misma firma, mismo comportamiento, mucho más rápido.

---

## 🛠️ Compilar y ejecutar

Requisitos: **g++ con C++17**, **ncurses** y una terminal de al menos **80×24**.

```bash
# Raspberry Pi / Debian
sudo apt install build-essential libncurses-dev

# Compilar
g++ -std=c++17 -O2 main.cpp -o june -lncurses

# Jugar
./june
```

> 🚧 El código todavía no incluye `main()` ni las implementaciones de los módulos, así que esta
> compilación va a fallar hasta que se complete la Etapa 2. Los mapas ya están listos para leerse.

---

## 👥 Equipo

| Integrante | Carné |
| :--- | :--- |
| Andrés Rodríguez Orozco | 2026087874 |
| Sebastián Marín Mora | 2026009715 |
| Julián Solórzano Hernández | 2026086615 |
| Abril González Morales | 2024207854 |

---

## 📜 Licencia

Public domain — [The Unlicense](LICENSE). Usalo, copialo, rompelo y arreglalo como quieras.

<p align="center"><i>Hecho con 🖤 y muchos caracteres en Costa Rica.</i></p>
