# The Legend of June

Juego ASCII de supervivencia para la terminal, inspirado en The Legend of Zelda.
Esquivá proyectiles, atacá con la espada y sobreviví oleadas de enemigos cada vez más difíciles.

Proyecto del curso **Arquitectura de Computadores** (TEC), en tres etapas: diseño, implementación
en C++ y rutinas críticas en ensamblador ARM A32. Por ahora el repositorio tiene los tipos, los
mapas y las declaraciones de los módulos: **el juego todavía no se puede ejecutar**.

## Controles

| Acción | Teclas |
| :--- | :--- |
| Mover | Flechas o `W` `A` `S` `D` |
| Atacar | `Espacio` |
| Pausa | `P` |
| Salir | `Q` o `Esc` |

## Mapas

Cinco mapas de 60×20 celdas en `mapa1.txt` … `mapa5.txt`: Clasico, Cruz, Cuatro cuartos,
Barras y Columnas. Para crear uno propio, copiá cualquiera y respetá el tamaño.

## Compilar

Necesitás g++ con C++17, ncurses y una terminal de al menos 80×24.

```bash
sudo apt install build-essential libncurses-dev
g++ -std=c++17 -O2 main.cpp -o june -lncurses
./june
```

## Equipo

| Integrante | Carné |
| :--- | :--- |
| Andrés Rodríguez Orozco | 2026087874 |
| Sebastián Marín Mora | 2026009715 |
| Julián Solórzano Hernández | 2026086615 |
| Abril González Morales | 2024207854 |

## Licencia

Dominio público — [The Unlicense](LICENSE).
