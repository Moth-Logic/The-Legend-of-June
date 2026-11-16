
// The Legend of June  -  Etapa 2 (solo C++)
// Arquitectura de Computadores, TEC, II Semestre 2026
//
// Andres Rodriguez Orozco   - 2026087874
// Sebastian Marin Mora      - 2026009715
// Julian Solorzano Hernandez - 2026086615
// Abril Gonzalez Morales    - 2024207854

#include <cstdint>
#include <vector>
#include <functional>
#include <string>
#include <csignal>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <ncurses.h>
#include <algorithm>
#include <cstdio>
#include <unistd.h>


// CONSTANTES DEL JUEGO
// Pantalla: el juego necesita una terminal de al menos 80x24 (PANT_W x PANT_H).
// La arena (60x20 celdas) se dibuja con su esquina en (ARENA_X, ARENA_Y); las
// filas de arriba y de abajo quedan libres para el HUD.
// FP_BITS: posiciones y velocidades son enteros de 32 bits en punto fijo con
// 8 bits fraccionarios (256 = 1 celda). No se usa punto flotante.
// MAX_ENT: tope de enemigos, de proyectiles y de corazones (arreglos fijos).
// NS_CUADRO: 33.3 ms por cuadro = 30 cuadros por segundo.
const int ARENA_W = 60;
const int ARENA_H = 20;
const int PANT_W = 80;
const int PANT_H = 24;
const int ARENA_X = 10;
const int ARENA_Y = 2;
const int FP_BITS = 8;
const int MAX_ENT = 64;
const int NUM_MAPAS = 5;
const long NS_CUADRO = 33333333L;

// Valores de combate (en cuadros salvo que se diga otra cosa):
//  - VIDA_JUGADOR / INMUNE_JUGADOR: vida inicial y cuadros de invulnerabilidad tras un golpe.
//  - GOLPE_DURACION / GOLPE_ESPERA: cuanto dura la zona de golpe y la espera para volver a atacar.
//  - PARPADEO_CUADROS / INMUNE_ENEMIGO: parpadeo e inmunidad corta del enemigo herido.
//  - MANTENER_TECLA: cuadros que se sigue moviendo el jugador tras soltar una tecla
//    (la terminal no avisa cuando se suelta, solo repite la tecla).
//  - ATURDIR_CUADROS / EMPUJE_CELDAS: aturdimiento y empuje del enemigo que toca al jugador.
//  - DESVIO_CUADROS: cuadros que un enemigo trabado rodea un obstaculo.
const int VIDA_JUGADOR = 5;
const int INMUNE_JUGADOR = 30;
const int GOLPE_DURACION = 4;
const int GOLPE_ESPERA = 10;
const int PARPADEO_CUADROS = 6;
const int INMUNE_ENEMIGO = 8;
const int MANTENER_TECLA = 4;
const int ATURDIR_CUADROS = 35;
const int EMPUJE_CELDAS = 4;
const int TECLA_ESC = 27;
const int DESVIO_CUADROS = 45;
// Corazones: probabilidad (%) de que un enemigo muerto suelte uno, cuadros que
// dura en el suelo y cuadros finales en los que parpadea.
const int PROB_CORAZON = 20;      // % de probabilidad de que un enemigo suelte un corazon
const int VIDA_CORAZON = 450;     // cuadros que dura un corazon en el suelo (15 s)
const int CORAZON_PARPADEA = 90;  // en los ultimos cuadros el corazon parpadea

// Velocidades en punto fijo (celdas por cuadro x 256). Las celdas de la consola
// son aprox. el doble de altas que de anchas, por eso la velocidad vertical es
// la mitad de la horizontal: asi el movimiento se ve parejo en pantalla.
const int32_t VEL_JUG_X = 128, VEL_JUG_Y = 64;
const int32_t VEL_SOL_X = 48, VEL_SOL_Y = 24;
const int32_t VEL_ARQ_X = 40, VEL_ARQ_Y = 20;
const int32_t VEL_MAG_X = 36, VEL_MAG_Y = 18;
const int32_t VEL_PROY_X = 96, VEL_PROY_Y = 48;

// Direcciones, acciones que produce el Input Handler, tipos de enemigo y
// estados de los enemigos.
enum Dir { ARRIBA, ABAJO, IZQUIERDA, DERECHA };
enum Accion { NADA, MOVER, ATACAR, PAUSA, SALIR, ENTER };
enum TipoEnemigo { SOLDADO, ARQUERO, MAGO };
enum EstadoEnemigo { PERSEGUIR, DISPARAR, HERIDO };

// Comando: lo que el Input Handler le entrega al Game Engine (accion + direccion).
// Rect: rectangulo alineado a los ejes en CELDAS (x, y, ancho, alto); es la
// estructura que recibe la rutina colisiones().
struct Comando { Accion accion; Dir dir; };
struct Rect { int32_t x, y, ancho, alto; };

// Nombre de cada uno de los 5 mapas (mapa1.txt ... mapa5.txt) para mostrarlo en el HUD.
inline const char* nombreMapa(int i) {
    static const char* nombres[NUM_MAPAS] = { "Clasico", "Cruz", "Cuatro cuartos", "Barras", "Columnas" };
    return nombres[i];
}

// Conversion celda <-> punto fijo. Para dibujar o comparar se usa la posicion
// desplazada FP_BITS a la derecha (la celda).
inline int32_t aFijo(int celda) { return celda << FP_BITS; }
inline int aCelda(int32_t fijo) { return fijo >> FP_BITS; }

// Parametros de copiar_sprite(): buffer de pantalla (con su ancho y alto), el
// sprite (con su ancho y alto) y la celda del buffer donde se dibuja.
struct DatosSprite {
    char* buffer;
    int32_t bufAncho;
    int32_t bufAlto;
    const char* sprite;
    int32_t spAncho;
    int32_t spAlto;
    int32_t x;
    int32_t y;
};

// RUTINAS CRITICAS (interfaz C). En la Etapa 3 se reemplazan por ensamblador AA32
// con exactamente esta misma firma (AAPCS: r0-r3 y el quinto parametro por pila).
//  - mover_entidades(pos, vel, n): pos[i] += vel[i] para n enteros (pares x,y).
//  - colisiones(a, na, b, nb, res): res[i*nb + j] = 1 si a[i] y b[j] se traslapan, 0 si no.
//  - copiar_sprite(d): copia el sprite al buffer saltando espacios y recortando bordes.
extern "C" {
    void mover_entidades(int32_t* pos, const int32_t* vel, uint32_t n);
    void colisiones(const Rect* a, uint32_t na, const Rect* b, uint32_t nb, uint8_t* res);
    void copiar_sprite(DatosSprite* d);
}


// DECLARACIONES DE LOS MODULOS (mismos bloques del diseno de la Etapa 1)

// ARENA: cuadricula de 60x20 con paredes ('#'). Se carga de un archivo de texto;
// si no existe, se genera una arena por defecto. Fuera de la arena cuenta como pared.
class Arena {
public:
    Arena();
    bool cargar(const char* ruta);
    void generarPorDefecto();
    bool esPared(int cx, int cy) const;
    bool lineaLibre(int x0, int y0, int x1, int y1) const;
    void libreCerca(int& cx, int& cy) const;
private:
    char celdas[ARENA_H][ARENA_W];
    void bloque(int x0, int y0, int x1, int y1);
};

const int NINGUNA = -1;

//INPUT HANDLER
// KeyboardReader: configura ncurses y lee una tecla sin esperar.
class KeyboardReader {
public:
    void iniciarNcurses();
    int leerTecla();
    void cerrarNcurses();
};

// InputBuffer: cola circular de 8 teclas; si se llena descarta la mas vieja.
class InputBuffer {
public:
    void guardar(int tecla);
    int siguiente();
private:
    int datos[8];
    int inicio = 0;
    int cantidad = 0;
};

// ActionMapper: traduce una tecla a un Comando (flechas/WASD, espacio, P, Q, Enter).
class ActionMapper {
public:
    bool traducir(int tecla, Comando& cmd) const;
};

// InputHandler: junta los tres anteriores; leerComandos() entrega los comandos del cuadro.
class InputHandler {
public:
    void iniciar();
    void cerrar();
    std::vector<Comando> leerComandos();
private:
    KeyboardReader teclado;
    InputBuffer cola;
    ActionMapper mapa;
};

//ENTITY SYSTEM
// Entidades: arreglos contiguos de tamano fijo (maximo MAX_ENT) con la posicion,
// velocidad, posicion anterior, vida, estado, etc. de cada entidad. Se usa para
// enemigos, proyectiles y corazones. eliminar() copia la ultima entidad sobre la
// borrada, asi nunca quedan huecos y las rutinas recorren los arreglos de corrido.
struct Entidades {
    int32_t pos[2 * MAX_ENT] = {};
    int32_t vel[2 * MAX_ENT] = {};
    int32_t prev[2 * MAX_ENT] = {};
    int32_t vida[MAX_ENT] = {};
    int32_t estado[MAX_ENT] = {};
    int32_t tipo[MAX_ENT] = {};
    int32_t espera[MAX_ENT] = {};
    int32_t parpadeo[MAX_ENT] = {};
    int32_t inmune[MAX_ENT] = {};
    int32_t aturdido[MAX_ENT] = {};
    int32_t desvio[MAX_ENT] = {};
    int32_t desvioVel[2 * MAX_ENT] = {};
    uint32_t n = 0;

    bool agregar(int t, int v, int32_t x, int32_t y, int32_t vx, int32_t vy);
    void eliminar(uint32_t i);
    Rect rect(uint32_t i) const { return Rect{ aCelda(pos[2 * i]), aCelda(pos[2 * i + 1]), 1, 1 }; }
};