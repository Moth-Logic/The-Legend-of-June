# The Legend of June

Juego ASCII de supervivencia para la terminal, inspirado en The Legend of Zelda.
Esquivá proyectiles, atacá con la espada y sobreviví oleadas de enemigos cada vez más difíciles.
Tenés 10 puntos de vida, que se muestran como `[##########]` y bajan de a uno por golpe.

Proyecto del curso **Arquitectura de Computadores** (TEC), en tres etapas: diseño, implementación
en C++ y rutinas críticas en ensamblador ARM A32. Por ahora el repositorio tiene la **Etapa 2
completa**: el juego ya se puede ejecutar (probado en Raspberry Pi 4 B).

## Controles

| Acción | Teclas |
| :--- | :--- |
| Iniciar | `Enter` |
| Mover | Flechas o `W` `A` `S` `D` |
| Atacar | `Espacio` |
| Pausa | `P` |
| Salir | `Q` o `Esc` |

## Mapas

Quince mapas de 60×20 celdas en `mapa1.txt` … `mapa15.txt`: Clasico, Cruz, Cuatro cuartos,
Barras, Columnas, Serpiente, Pasillos, Anillos, Zigzag, Gemelos, Aspa, Fortaleza, Puertas,
Espiral y Campos. Para crear uno propio, copiá cualquiera y respetá el tamaño.

## Compilar

Necesitás g++ con C++17, ncurses y una terminal de al menos 80×24.

```bash
sudo apt install build-essential libncurses-dev
make
./june
```

Para medir el rendimiento: `make bench` (o `./june --bench N`).

## Equipo

| Integrante |
| :--- |
| Andrés Rodríguez Orozco |
| Sebastián Marín Mora |
| Julián Solórzano Hernández |
| Abril González Morales |

## Licencia

Dominio público — [The Unlicense](LICENSE).
