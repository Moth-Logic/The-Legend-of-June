
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

// Jugador: posicion, vida, invulnerabilidad, y la zona de golpe de su ataque.
class Jugador {
public:
    int32_t pos[2], vel[2], prev[2];
    int vida;
    int inmune;
    int espera;
    int golpe;
    Dir mira;
    Dir dirGolpe;

    void reiniciar(int cx, int cy);
    void leerComandos(const std::vector<Comando>& cmds);
    void actualizar();
    bool recibirDano();
    Rect rect() const;
    Rect zonaGolpe() const;
    bool golpeActivo() const { return golpe > 0; }
private:
    int quiereX, quiereY, mantenerX, mantenerY;
    bool quiereAtacar;
};

// FnDisparo: funcion que el Game Engine le entrega a los enemigos para crear
// proyectiles (los modulos no se conocen entre si, todo pasa por el motor).
typedef std::function<void(int32_t x, int32_t y, int32_t vx, int32_t vy)> FnDisparo;

// Enemigos: IA por estados (perseguir, disparar, herido), movimiento y dano.
namespace Enemigos {
    bool agregar(Entidades& e, int tipo, int vida, int32_t x, int32_t y);
    void actualizar(Entidades& e, const int32_t* posJugador, const FnDisparo& disparar);
    void mover(Entidades& e);
    bool recibirDano(Entidades& e, uint32_t i);
    void aturdir(Entidades& e, uint32_t i, int cuadros);
}

// Proyectiles: crean y mueven en linea recta los disparos de Arqueros y Magos.
namespace Proyectiles {
    void crear(Entidades& p, int32_t x, int32_t y, int32_t vx, int32_t vy);
    void mover(Entidades& p);
}

// WAVE MANAGER 
// FnAgregar: funcion que el Game Engine le entrega al Enemy Spawner para agregar
// un enemigo al Entity System.
typedef std::function<bool(int tipo, int vida, int32_t x, int32_t y)> FnAgregar;

// WaveCounter: cuenta las oleadas y detecta cuando termina una (ya no quedan enemigos).
class WaveCounter {
public:
    void reiniciar() { oleada = 0; }
    void subir() { oleada++; }
    int actual() const { return oleada; }
    bool terminada(uint32_t vivos) const { return vivos == 0; }
private:
    int oleada = 0;
};

// DifficultyScaler: cuantos enemigos salen (2n-1), de que tipo y con cuanta vida.
class DifficultyScaler {
public:
    int cantidad(int oleada) const;
    int tipo(int oleada, int i) const;
    int vida(int oleada, int tipo) const;
};

// EnemySpawner: elige un punto libre y lejos del jugador y crea el enemigo.
class EnemySpawner {
public:
    void elegirPunto(const Arena& arena, int jx, int jy, int& cx, int& cy) const;
    bool crear(int tipo, int vida, int cx, int cy, const FnAgregar& agregar) const;
};

// WaveManager: coordina los tres anteriores para lanzar una oleada.
class WaveManager {
public:
    void reiniciar() { contador.reiniciar(); }
    int oleadaActual() const { return contador.actual(); }
    bool terminada(uint32_t vivos) const { return contador.terminada(vivos); }
    void lanzar(const Arena& arena, int jx, int jy, const FnAgregar& agregar);
private:
    WaveCounter contador;
    DifficultyScaler dificultad;
    EnemySpawner spawner;
};

// RENDER SYSTEM 
// Colores (pares de ncurses).
enum Color { C_ROJO = 1, C_VERDE, C_AMARILLO, C_MAGENTA, C_CIAN, C_BLANCO, C_AZUL };

// Sprite: dibujo de caracteres (ancho x alto) guardado fila por fila.
struct Sprite {
    const char* datos;
    int ancho;
    int alto;
};

// ASCIISpriteMgr: guarda los sprites del jugador, enemigos, proyectiles, golpe y corazon.
class ASCIISpriteMgr {
public:
    void cargar();
    Sprite jugador, soldado, arquero, mago;
    Sprite protH, protV, protM;
    Sprite golpe, marca, corazon;
    static bool visible(int parpadeo) { return parpadeo % 2 == 0; }
};

// DatosHUD: datos que el HUD solo LEE para mostrar (vida, oleada, enemigos, puntos, mapa).
struct DatosHUD {
    int vida;
    int oleada;
    int enemigos;
    int puntaje;
    const char* mapa;
    bool esperando;
};

// Renderer: dibuja todo primero en un buffer de caracteres (no en la pantalla) por
// capas: arena, entidades y al final el HUD; luego lo copia a ncurses y llama a
// refresh() una sola vez por cuadro para evitar parpadeos.
class Renderer {
public:
    void iniciar(bool conPantalla = true);
    void limpiar();
    void pintarArena(const Arena& arena);
    void pintarEntidades(const Arena& arena, const Jugador& j, const Entidades& enemigos, const Entidades& proy, const Entidades& corazones);
    void pintarSprite(const Sprite& s, int cx, int cy, int color);
    void texto(int x, int y, const std::string& s, int color);
    void textoCentrado(int y, const std::string& s, int color);
    void refrescar();
    bool terminalSuficiente() const;
private:
    ASCIISpriteMgr sprites;
    char buf[PANT_H][PANT_W];
    unsigned char col[PANT_H][PANT_W];
    bool hayColor = false;
    bool pantalla = true;
};

// HUDManager: vida, oleada, puntos, menu, pausa y pantalla de derrota. Nunca cambia datos del juego.
class HUDManager {
public:
    static void dibujar(Renderer& r, const DatosHUD& d);
    static void menu(Renderer& r);
    static void pausa(Renderer& r);
    static void derrota(Renderer& r, const DatosHUD& d);
};

// GAME ENGINE
// CollisionDetector: usa la rutina colisiones() (parte critica) y ademas frena el
// movimiento contra las paredes devolviendo la entidad a su posicion anterior.
class CollisionDetector {
public:

    const uint8_t* comparar(const Rect* a, uint32_t na, const Rect* b, uint32_t nb);

    static void frenarContraPared(const Arena& arena, int32_t* pos, const int32_t* prev);

    static void desplazarSinPared(const Arena& arena, int32_t* pos, int32_t* prev, int32_t dx, int32_t dy);

private:
    uint8_t res[MAX_ENT * MAX_ENT];
};

// BenchCuadro: resultados del modo de rendimiento (--bench N), en microsegundos por cuadro.
struct BenchCuadro {
    int n = 0;
    int cuadros = 0;
    bool conPantalla = false;
    double total_us = 0;
    double logica_us = 0;
    double pintar_us = 0;
    double refresco_us = 0;
};

void reporteBench(const BenchCuadro& b);

// Estados de la partida: menu, jugando, pausa y fin de partida.
enum EstadoPartida { E_MENU, E_JUGANDO, E_PAUSA, E_FIN };

extern volatile std::sig_atomic_t senalSalir;
void instalarSenales();

// StateManager: maquina de estados. Menu -Enter-> jugando <-P-> pausa; vida 0 -> fin
// de partida -Enter-> menu; Q sale desde cualquier estado. Solo en 'jugando' corre la logica.
class StateManager {
public:
    EstadoPartida actual() const { return estado; }
    void procesar(const Comando& c, bool& iniciarPartida, bool& salir);
    void derrota() { estado = E_FIN; }
    void forzarJugando() { estado = E_JUGANDO; }
private:
    EstadoPartida estado = E_MENU;
};