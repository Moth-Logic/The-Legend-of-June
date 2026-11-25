
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
#include <cerrno>
#include <unistd.h>
#ifdef __unix__
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif


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

// Musica: reproduce en bucle la pista del menu o de la partida con un reproductor
// externo (aplay, paplay, ffplay, mpv o play) en un proceso hijo, que se mata al
// cambiar de pista o al salir. Si no hay reproductor o falta el WAV, no suena nada.
class Musica {
public:
    ~Musica() { detener(); }
    void actualizar(EstadoPartida e);
    void detener();
private:
    enum Pista { SIN_PISTA, PISTA_MENU, PISTA_BATALLA };
    void iniciar(const char* archivo);
    Pista actual = SIN_PISTA;
    int pid = -1;
};

// GameEngine: director del juego. Es el unico que conecta los modulos entre si:
// en cada cuadro lee comandos, actualiza entidades, revisa colisiones, pide la
// siguiente oleada 
class GameEngine {
public:
    GameEngine() {}
    int ejecutar();

    BenchCuadro benchmark(int n, bool conPantalla);
private:
    int mapa = 0;
    Arena arena;
    InputHandler input;
    StateManager estados;
    Renderer render;
    WaveManager oleadas;
        CollisionDetector colisionador;
    Musica musica;
    Jugador jugador;
    Entidades enemigos;
    Entidades proyectiles;
    Entidades corazones;
    int puntaje = 0;
    bool esperando = true;
    int pausaOleada = 0;
    bool salir = false;

    void cargarMapa(int i);
    void siguienteMapa();
    void empujarEnemigo(uint32_t j);
    void inicializar();
    void nuevaPartida(int mapaFijo = -1);
    void cuadro();
    void logica(const std::vector<Comando>& cmds);
    void actualizarEntidades();
    bool revisarColisiones();
    void limpiarMuertos();
    void soltarCorazon(int32_t x, int32_t y);
    void recogerCorazones();
    void pedirOleada();
    void pintar();
    void dibujar();
    void controlarTiempo(const timespec& inicio);
    void celdaLibreAleatoria(int& cx, int& cy) const;
};


#ifndef USAR_ASM

extern "C" {

// Suma la velocidad a la posicion: n es la cantidad de enteros (2 por entidad: x, y).
void mover_entidades(int32_t* pos, const int32_t* vel, uint32_t n) {
    for (uint32_t i = 0; i < n; i++)
        pos[i] += vel[i];
}

// Para cada pareja (i, j) escribe en res[i*nb + j] un 1 si los rectangulos a[i] y
// b[j] se traslapan (prueba de ejes alineados) y un 0 si no.
void colisiones(const Rect* a, uint32_t na, const Rect* b, uint32_t nb, uint8_t* res) {
    for (uint32_t i = 0; i < na; i++) {
        for (uint32_t j = 0; j < nb; j++) {
            bool toca = a[i].x < b[j].x + b[j].ancho && a[i].x + a[i].ancho > b[j].x &&
                        a[i].y < b[j].y + b[j].alto && a[i].y + a[i].alto > b[j].y;
            res[i * nb + j] = toca ? 1 : 0;
        }
    }
}

// Copia el sprite al buffer en (d->x, d->y): salta los espacios (para no borrar lo
// que esta debajo) y recorta lo que se salga de los bordes del buffer.
void copiar_sprite(DatosSprite* d) {
    for (int32_t f = 0; f < d->spAlto; f++) {
        int32_t by = d->y + f;
        if (by < 0 || by >= d->bufAlto) continue;
        for (int32_t c = 0; c < d->spAncho; c++) {
            int32_t bx = d->x + c;
            if (bx < 0 || bx >= d->bufAncho) continue;
            char ch = d->sprite[f * d->spAncho + c];
            if (ch == ' ') continue;
            d->buffer[by * d->bufAncho + bx] = ch;
        }
    }
}

}

#endif

// IMPLEMENTACION

// Arena 
Arena::Arena() { 
    generarPorDefecto(); 
}

// Rellena de paredes el rectangulo de celdas (x0,y0)-(x1,y1) (ambos inclusive).
void Arena::bloque(int x0, int y0, int x1, int y1) {
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            celdas[y][x] = '#';
}

// Arena de respaldo: borde de paredes mas 8 obstaculos, equivalente a mapa1.txt.
void Arena::generarPorDefecto() {
    memset(celdas, ' ', sizeof(celdas));
    bloque(0, 0, ARENA_W - 1, 0);
    bloque(0, ARENA_H - 1, ARENA_W - 1, ARENA_H - 1);
    bloque(0, 0, 0, ARENA_H - 1);
    bloque(ARENA_W - 1, 0, ARENA_W - 1, ARENA_H - 1);
    bloque(10, 4, 13, 5);
    bloque(46, 4, 49, 5);
    bloque(10, 14, 13, 15);
    bloque(46, 14, 49, 15);
    bloque(26, 6, 33, 6);
    bloque(26, 13, 33, 13);
    bloque(20, 8, 20, 11);
    bloque(39, 8, 39, 11);
}

// Carga una arena desde un archivo de texto de 20 filas: '#' es pared y cualquier
// otro caracter es piso. Devuelve false (sin modificar la arena) si faltan filas.
bool Arena::cargar(const char* ruta) {
    std::ifstream f(ruta);
    if (!f) return false;
    char tmp[ARENA_H][ARENA_W];
    std::string linea;
    int fila = 0;
    while (fila < ARENA_H && std::getline(f, linea)) {
        for (int c = 0; c < ARENA_W; c++)
            tmp[fila][c] = (c < (int)linea.size() && linea[c] == '#') ? '#' : ' ';
        fila++;
    }
    if (fila < ARENA_H) return false;
    memcpy(celdas, tmp, sizeof(celdas));
    return true;
}

// true si la celda es pared; todo lo que queda fuera de la arena tambien cuenta como pared.
bool Arena::esPared(int cx, int cy) const {
    if (cx < 0 || cy < 0 || cx >= ARENA_W || cy >= ARENA_H) return true;
    return celdas[cy][cx] == '#';
}

// Linea de Bresenham entre dos celdas: true si no hay paredes en el camino (tampoco
// se puede pasar por una esquina diagonal cerrada). Se usa para que el golpe no atraviese paredes.
bool Arena::lineaLibre(int x0, int y0, int x1, int y1) const {
    int dx = std::abs(x1 - x0), dy = std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    int x = x0, y = y0;
    while (x != x1 || y != y1) {
        int e2 = 2 * err;
        int ox = x, oy = y;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx) { err += dx; y += sy; }
        if (x != ox && y != oy && esPared(ox + sx, oy) && esPared(ox, oy + sy)) return false;
        if (esPared(x, y)) return false;
    }
    return true;
}

// Mueve (cx, cy) a la celda libre mas cercana (busqueda en anillos crecientes).
void Arena::libreCerca(int& cx, int& cy) const {
    for (int r = 0; r < ARENA_W; r++) {
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                if (!esPared(cx + dx, cy + dy)) {
                    cx += dx;
                    cy += dy;
                    return;
                }
            }
        }
    }
}

// Input Handler 
// Configura la consola una sola vez: sin esperar Enter, sin eco, teclas especiales,
// lectura que no bloquea y cursor oculto.
void KeyboardReader::iniciarNcurses() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);
    set_escdelay(25);
}

// Lee una tecla con getch(); si no se apreto nada devuelve NINGUNA.
int KeyboardReader::leerTecla() {
    int t = getch();
    return t == ERR ? NINGUNA : t;
}

void KeyboardReader::cerrarNcurses() { 
    endwin(); 
}

// Agrega la tecla al final de la cola circular; si esta llena descarta la mas vieja.
void InputBuffer::guardar(int tecla) {
    if (cantidad == 8) {
        inicio = (inicio + 1) % 8;
        cantidad--;
    }
    datos[(inicio + cantidad) % 8] = tecla;
    cantidad++;
}
// Saca la tecla mas antigua devuelve NINGUNA si la cola esta vacia.
int InputBuffer::siguiente() {
    if (cantidad == 0) return NINGUNA;
    int t = datos[inicio];
    inicio = (inicio + 1) % 8;
    cantidad--;
    return t;
}

// Busca la tecla en la tabla de controles devuelve false si la tecla no hace nada
bool ActionMapper::traducir(int tecla, Comando& cmd) const {
    switch (tecla) {
    case KEY_UP: case 'w': case 'W': cmd = Comando{ MOVER, ARRIBA }; return true;
    case KEY_DOWN: case 's': case 'S': cmd = Comando{ MOVER, ABAJO }; return true;
    case KEY_LEFT: case 'a': case 'A': cmd = Comando{ MOVER, IZQUIERDA }; return true;
    case KEY_RIGHT: case 'd': case 'D': cmd = Comando{ MOVER, DERECHA }; return true;
    case ' ': case 'j': case 'J': cmd = Comando{ ATACAR, ARRIBA }; return true;
    case 'p': case 'P': cmd = Comando{ PAUSA, ARRIBA }; return true;
    case 'q': case 'Q': case TECLA_ESC: cmd = Comando{ SALIR, ARRIBA }; return true;
    case '\n': case '\r': case KEY_ENTER: cmd = Comando{ ENTER, ARRIBA }; return true;
    default: return false;
    }
}

void InputHandler::iniciar() { teclado.iniciarNcurses(); }
void InputHandler::cerrar() { teclado.cerrarNcurses(); }

// Lee todas las teclas pendientes del cuadro las pasa por la cola y devuelve los
// comandos resultantes al Game Engine
std::vector<Comando> InputHandler::leerComandos() {
    int t;
    while ((t = teclado.leerTecla()) != NINGUNA)
        cola.guardar(t);
    std::vector<Comando> cmds;
    while ((t = cola.siguiente()) != NINGUNA) {
        Comando c;
        if (mapa.traducir(t, c)) cmds.push_back(c);
    }
    return cmds;
}

// Entity System 
// Agrega una entidad al final de los arreglos devuelve false si ya hay MAX_ENT
bool Entidades::agregar(int t, int v, int32_t x, int32_t y, int32_t vx, int32_t vy) {
    if (n >= MAX_ENT) return false;
    uint32_t i = n++;
    pos[2 * i] = prev[2 * i] = x;
    pos[2 * i + 1] = prev[2 * i + 1] = y;
    vel[2 * i] = vx;
    vel[2 * i + 1] = vy;
    vida[i] = v;
    estado[i] = 0;
    tipo[i] = t;
    espera[i] = 0;
    parpadeo[i] = 0;
    inmune[i] = 0;
    aturdido[i] = 0;
    desvio[i] = 0;
    desvioVel[2 * i] = desvioVel[2 * i + 1] = 0;
    return true;
}

// Borra la entidad i copiando la ultima sobre ella y bajando la cantidad (sin huecos)
void Entidades::eliminar(uint32_t i) {
    uint32_t u = n - 1;
    if (i != u) {
        pos[2 * i] = pos[2 * u];
        pos[2 * i + 1] = pos[2 * u + 1];
        vel[2 * i] = vel[2 * u];
        vel[2 * i + 1] = vel[2 * u + 1];
        prev[2 * i] = prev[2 * u];
        prev[2 * i + 1] = prev[2 * u + 1];
        vida[i] = vida[u];
        estado[i] = estado[u];
        tipo[i] = tipo[u];
        espera[i] = espera[u];
        parpadeo[i] = parpadeo[u];
        inmune[i] = inmune[u];
        aturdido[i] = aturdido[u];
        desvio[i] = desvio[u];
        desvioVel[2 * i] = desvioVel[2 * u];
        desvioVel[2 * i + 1] = desvioVel[2 * u + 1];
    }
    n--;
}

// Deja al jugador en la celda dada con vida completa y sin ataque ni invulnerabilidad
void Jugador::reiniciar(int cx, int cy) {
    pos[0] = prev[0] = aFijo(cx);
    pos[1] = prev[1] = aFijo(cy);
    vel[0] = vel[1] = 0;
    vida = VIDA_JUGADOR;
    inmune = espera = golpe = 0;
    mira = ABAJO;
    dirGolpe = ABAJO;
    quiereX = quiereY = mantenerX = mantenerY = 0;
    quiereAtacar = false;
}

// Guarda lo que el jugador quiere hacer este cuadro (direccion de movimiento y/o ataque)
void Jugador::leerComandos(const std::vector<Comando>& cmds) {
    quiereX = quiereY = 0;
    quiereAtacar = false;
    for (size_t i = 0; i < cmds.size(); i++) {
        const Comando& c = cmds[i];
        if (c.accion == ATACAR) {
            quiereAtacar = true;
        }
        else if (c.accion == MOVER) {
            mira = c.dir;
            if (c.dir == IZQUIERDA) quiereX = -1;
            else if (c.dir == DERECHA) quiereX = 1;
            else if (c.dir == ARRIBA) quiereY = -1;
            else quiereY = 1;
        }
    }
}

// Un cuadro del jugador: baja los contadores, fija la velocidad segun la direccion
// (se mantiene MANTENER_TECLA cuadros tras soltar), guarda la posicion anterior
// mueve con mover_entidades() y si ya paso la espera inicia el ataque
void Jugador::actualizar() {
    if (inmune > 0) inmune--;
    if (espera > 0) espera--;
    if (golpe > 0) golpe--;

    if (quiereX != 0) {
        vel[0] = quiereX * VEL_JUG_X;
        mantenerX = MANTENER_TECLA;
    }
    else {
        if (mantenerX > 0) mantenerX--;
        if (mantenerX == 0) vel[0] = 0;
    }
    if (quiereY != 0) {
        vel[1] = quiereY * VEL_JUG_Y;
        mantenerY = MANTENER_TECLA;
    }
    else {
        if (mantenerY > 0) mantenerY--;
        if (mantenerY == 0) vel[1] = 0;
    }

    prev[0] = pos[0];
    prev[1] = pos[1];
    mover_entidades(pos, vel, 2);

    if (quiereAtacar && espera == 0) {
        golpe = GOLPE_DURACION;
        dirGolpe = mira;
        espera = GOLPE_ESPERA;
    }
    quiereAtacar = false;
}

// Resta 1 de vida y activa la invulnerabilidad (si no la tenia) devuelves true si la vida llego a 0
bool Jugador::recibirDano() {
    if (inmune > 0) return false;
    vida--;
    inmune = INMUNE_JUGADOR;
    return vida <= 0;
}

Rect Jugador::rect() const { return Rect{ aCelda(pos[0]), aCelda(pos[1]), 1, 1 }; }

// Zona de golpe: 3 celdas de ancho por 3 de profundidad delante del jugador segun hacia donde miro al atacar
Rect Jugador::zonaGolpe() const {
    int cx = aCelda(pos[0]), cy = aCelda(pos[1]);
    switch (dirGolpe) {
    case ARRIBA: return Rect{ cx - 1, cy - 3, 3, 3 };
    case ABAJO: return Rect{ cx - 1, cy + 1, 3, 3 };
    case IZQUIERDA: return Rect{ cx - 3, cy - 1, 3, 3 };
    default: return Rect{ cx + 1, cy - 1, 3, 3 };
    }
}

// Auxiliares de la IA: limitar()recorta d al rango [-v, v]; signo() devuelve -1, 0 o 1
static int32_t limitar(int32_t d, int32_t v) { return d > v ? v : (d < -v ? -v : d); }
static int32_t signo(int32_t d) { return d > 0 ? 1 : (d < 0 ? -1 : 0); }

// Agrega un enemigo; los de ataque a distancia empiezan con una espera aleatoria antes del primer disparo.
bool Enemigos::agregar(Entidades& e, int tipo, int vida, int32_t x, int32_t y) {
    if (!e.agregar(tipo, vida, x, y, 0, 0)) return false;
    if (tipo != SOLDADO) e.espera[e.n - 1] = 20 + std::rand() % 40;
    return true;
}

//  - Herido o aturdido: se queda quieto
//  - Soldado: persigue al jugador
//  - Arquero: mantiene distancia y dispara en linea recta en la direccion cardinal
//    donde la diferencia con el jugador es mayor cuando esta alineado y en rango
//  - Mago: mantiene distancia y dispara un proyectil apuntando al jugador
//  - Si quedo trabado contra un obstaculo, rodea durante DESVIO_CUADROS cuadros
// 'disparar' es la funcion que entrega el Game Engine para crear proyectiles
void Enemigos::actualizar(Entidades& e, const int32_t* jug, const FnDisparo& disparar) {
    for (uint32_t i = 0; i < e.n; i++) {
        int32_t* p = &e.pos[2 * i];
        int32_t* v = &e.vel[2 * i];
        if (e.espera[i] > 0) e.espera[i]--;
        if (e.inmune[i] > 0) e.inmune[i]--;
        if (e.parpadeo[i] > 0) {
            e.parpadeo[i]--;
            e.estado[i] = HERIDO;
            v[0] = v[1] = 0;
            continue;
        }
        if (e.aturdido[i] > 0) {
            e.aturdido[i]--;
            e.estado[i] = HERIDO;
            v[0] = v[1] = 0;
            continue;
        }
        
        int32_t ovx = v[0], ovy = v[1];
        bool trabado = (ovx != 0 || ovy != 0) && p[0] == e.prev[2 * i] && p[1] == e.prev[2 * i + 1];

        int32_t dx = jug[0] - p[0], dy = jug[1] - p[1];
        int adx = std::abs(dx) >> FP_BITS, ady = std::abs(dy) >> FP_BITS;
        int dist = std::max(adx, 2 * ady);

        if (e.tipo[i] == SOLDADO) {
            e.estado[i] = PERSEGUIR;
            v[0] = limitar(dx, VEL_SOL_X);
            v[1] = limitar(dy, VEL_SOL_Y);
        }
        else if (e.tipo[i] == ARQUERO) {
            bool horiz = std::abs(dx) >= std::abs(dy);
            if (horiz) {
                v[1] = limitar(dy, VEL_ARQ_Y);
                if (adx > 14) v[0] = signo(dx) * VEL_ARQ_X;
                else if (adx < 8) v[0] = -signo(dx) * VEL_ARQ_X;
                else v[0] = 0;
            }
            else {
                v[0] = limitar(dx, VEL_ARQ_X);
                if (ady > 7) v[1] = signo(dy) * VEL_ARQ_Y;
                else if (ady < 3) v[1] = -signo(dy) * VEL_ARQ_Y;
                else v[1] = 0;
            }
            e.estado[i] = (v[0] == 0 && v[1] == 0) ? DISPARAR : PERSEGUIR;

            bool alineado = horiz ? aCelda(p[1]) == aCelda(jug[1]) : aCelda(p[0]) == aCelda(jug[0]);
            bool enRango = horiz ? adx <= 28 : ady <= 14;
            if (e.espera[i] == 0 && alineado && enRango) {
                int32_t vx = 0, vy = 0;
                if (horiz) vx = signo(dx) * VEL_PROY_X;
                else vy = signo(dy) * VEL_PROY_Y;
                disparar(p[0], p[1], vx, vy);
                e.espera[i] = 90;
                e.estado[i] = DISPARAR;
            }
        }
        else {
            if (dist > 18) {
                v[0] = signo(dx) * VEL_MAG_X;
                v[1] = signo(dy) * VEL_MAG_Y;
            }
            else if (dist < 10) {
                v[0] = -signo(dx) * VEL_MAG_X;
                v[1] = -signo(dy) * VEL_MAG_Y;
            }
            else {
                v[0] = v[1] = 0;
            }
            e.estado[i] = (v[0] == 0 && v[1] == 0) ? DISPARAR : PERSEGUIR;

            if (e.espera[i] == 0 && dist <= 30) {
                int64_t m = std::max<int64_t>(std::abs(dx), 2 * (int64_t)std::abs(dy));
                if (m == 0) m = 1;
                int32_t vx = (int32_t)((int64_t)dx * VEL_PROY_X / m);
                int32_t vy = (int32_t)((int64_t)dy * VEL_PROY_X / m);
                disparar(p[0], p[1], vx, vy);
                e.espera[i] = 120;
                e.estado[i] = DISPARAR;
            }
        }

        if (e.desvio[i] > 0) {
            e.desvio[i]--;
            v[0] = e.desvioVel[2 * i];
            v[1] = e.desvioVel[2 * i + 1];
        }
        else if (trabado) {
            int s = (std::rand() & 1) ? 1 : -1;
            e.desvio[i] = DESVIO_CUADROS;
            if (std::abs(ovx) >= 2 * std::abs(ovy)) {
                e.desvioVel[2 * i] = 0;
                e.desvioVel[2 * i + 1] = s * std::max(std::abs(ovx) / 2, 1);
            }
            else {
                e.desvioVel[2 * i] = s * std::max(std::abs(ovy) * 2, 1);
                e.desvioVel[2 * i + 1] = 0;
            }
            v[0] = e.desvioVel[2 * i];
            v[1] = e.desvioVel[2 * i + 1];
        }
    }
}

// Guarda las posiciones actuales como anteriores y mueve TODOS los enemigos con una sola llamada a mover_entidades().
void Enemigos::mover(Entidades& e) {
    memcpy(e.prev, e.pos, sizeof(int32_t) * 2 * e.n);
    mover_entidades(e.pos, e.vel, 2 * e.n);
}

// Resta vida, activa parpadeo e inmunidad corta. Devuelve true si la vida llego a 0 (se elimina al final del cuadro).
bool Enemigos::recibirDano(Entidades& e, uint32_t i) {
    if (e.inmune[i] > 0 || e.vida[i] <= 0) return false;
    e.vida[i]--;
    e.parpadeo[i] = PARPADEO_CUADROS;
    e.inmune[i] = INMUNE_ENEMIGO;
    e.estado[i] = HERIDO;
    return e.vida[i] <= 0;
}

// Detiene al enemigo durante 'cuadros' cuadros (despues de tocar al jugador).
void Enemigos::aturdir(Entidades& e, uint32_t i, int cuadros) {
    e.aturdido[i] = cuadros;
    e.vel[2 * i] = e.vel[2 * i + 1] = 0;
}

// Crea un proyectil en la posicion del enemigo que disparo, con la velocidad dada.
void Proyectiles::crear(Entidades& p, int32_t x, int32_t y, int32_t vx, int32_t vy) {
    p.agregar(0, 1, x, y, vx, vy);
}

// Avanza todos los proyectiles en linea recta con una sola llamada a mover_entidades().
void Proyectiles::mover(Entidades& p) {
    memcpy(p.prev, p.pos, sizeof(int32_t) * 2 * p.n);
    mover_entidades(p.pos, p.vel, 2 * p.n);
}

// Wave Manager 
// En la oleada n salen 2n-1 enemigos (1, 3, 5, 7...), con tope de 60.
int DifficultyScaler::cantidad(int oleada) const {
    return std::min(2 * oleada - 1, 60);
}

// Tipo del enemigo numero i de la oleada. Oleada 1: solo Soldado. Oleada 2: Soldado,
// Arquero y Mago. Desde la 3: uno de cada tres es de distancia (Arqueros; desde la 5
// alternan con Magos).
int DifficultyScaler::tipo(int oleada, int i) const {
    if (oleada == 2) return i == 1 ? ARQUERO : (i == 2 ? MAGO : SOLDADO);   // 2da oleada: Soldado + Arquero + Mago
    if (oleada < 3 || i % 3 != 2) return SOLDADO;
    if (oleada < 5) return ARQUERO;
    return ((i / 3) % 2 == 0) ? ARQUERO : MAGO;
}

// Vida base 2 (Mago 3), mas 1 cada 5 oleadas.
int DifficultyScaler::vida(int oleada, int tipo) const {
    int base = (tipo == MAGO) ? 3 : 2;
    return base + (oleada - 1) / 5;
}

// Elige una celda libre al azar y lejos del jugador (si no encuentra, usa la primera libre).
void EnemySpawner::elegirPunto(const Arena& arena, int jx, int jy, int& cx, int& cy) const {
    for (int intento = 0; intento < 300; intento++) {
        int x = 1 + std::rand() % (ARENA_W - 2);
        int y = 1 + std::rand() % (ARENA_H - 2);
        if (arena.esPared(x, y)) continue;
        if (std::max(std::abs(x - jx), 2 * std::abs(y - jy)) < 16) continue;
        cx = x;
        cy = y;
        return;
    }
    for (int y = 1; y < ARENA_H - 1; y++) {
        for (int x = 1; x < ARENA_W - 1; x++) {
            if (!arena.esPared(x, y)) {
                cx = x;
                cy = y;
                return;
            }
        }
    }
}

// Crea el enemigo en la celda indicada usando la funcion de agregar que dio el Game Engine.
bool EnemySpawner::crear(int tipo, int vida, int cx, int cy, const FnAgregar& agregar) const {
    return agregar(tipo, vida, aFijo(cx), aFijo(cy));
}

// Sube el numero de oleada y crea todos sus enemigos.
void WaveManager::lanzar(const Arena& arena, int jx, int jy, const FnAgregar& agregar) {
    contador.subir();
    int o = contador.actual();
    int n = dificultad.cantidad(o);
    for (int i = 0; i < n; i++) {
        int t = dificultad.tipo(o, i);
        int cx = 1, cy = 1;
        spawner.elegirPunto(arena, jx, jy, cx, cy);
        spawner.crear(t, dificultad.vida(o, t), cx, cy, agregar);
    }
}

// Render System 
// Define los dibujos: @ jugador, S soldado, A arquero, M mago, - | * proyectiles, + golpe, <3 corazon.
void ASCIISpriteMgr::cargar() {
    jugador = Sprite{ "@", 1, 1 };
    soldado = Sprite{ "S", 1, 1 };
    arquero = Sprite{ "A", 1, 1 };
    mago = Sprite{ "M", 1, 1 };
    protH = Sprite{ "-", 1, 1 };
    protV = Sprite{ "|", 1, 1 };
    protM = Sprite{ "*", 1, 1 };
    golpe = Sprite{ "+", 1, 1 };
    marca = Sprite{ ".", 1, 1 };
    corazon = Sprite{ "<3", 2, 1 };
}

// Inicia los colores de ncurses (si la terminal los soporta). Con conPantalla=false no toca ncurses (modo bench).
void Renderer::iniciar(bool conPantalla) {
    sprites.cargar();
    pantalla = conPantalla;
    if (!pantalla) return;
    hayColor = has_colors();
    if (hayColor) {
        start_color();
        init_pair(C_ROJO, COLOR_RED, COLOR_BLACK);
        init_pair(C_VERDE, COLOR_GREEN, COLOR_BLACK);
        init_pair(C_AMARILLO, COLOR_YELLOW, COLOR_BLACK);
        init_pair(C_MAGENTA, COLOR_MAGENTA, COLOR_BLACK);
        init_pair(C_CIAN, COLOR_CYAN, COLOR_BLACK);
        init_pair(C_BLANCO, COLOR_WHITE, COLOR_BLACK);
        init_pair(C_AZUL, COLOR_BLUE, COLOR_BLACK);
    }
}

// Llena el buffer de espacios; todavia no toca la pantalla.
void Renderer::limpiar() {
    memset(buf, ' ', sizeof(buf));
    memset(col, C_BLANCO, sizeof(col));
}

// Capa 1: paredes de la arena.
void Renderer::pintarArena(const Arena& arena) {
    for (int y = 0; y < ARENA_H; y++) {
        for (int x = 0; x < ARENA_W; x++) {
            if (arena.esPared(x, y)) {
                buf[ARENA_Y + y][ARENA_X + x] = '#';
                col[ARENA_Y + y][ARENA_X + x] = C_AZUL;
            }
        }
    }
}

// Copia un sprite al buffer con la rutina copiar_sprite() y le pone su color.
void Renderer::pintarSprite(const Sprite& s, int cx, int cy, int color) {
    DatosSprite d;
    d.buffer = &buf[0][0];
    d.bufAncho = PANT_W;
    d.bufAlto = PANT_H;
    d.sprite = s.datos;
    d.spAncho = s.ancho;
    d.spAlto = s.alto;
    d.x = ARENA_X + cx;
    d.y = ARENA_Y + cy;
    copiar_sprite(&d);
    for (int f = 0; f < s.alto; f++) {
        for (int c = 0; c < s.ancho; c++) {
            int bx = d.x + c, by = d.y + f;
            if (bx < 0 || by < 0 || bx >= PANT_W || by >= PANT_H) continue;
            if (s.datos[f * s.ancho + c] != ' ') col[by][bx] = (unsigned char)color;
        }
    }
}

// Capa 2: zona de golpe (o la marca de hacia donde mira), corazones, enemigos (los
// heridos parpadean), proyectiles y por ultimo el jugador (parpadea si es invulnerable).
void Renderer::pintarEntidades(const Arena& arena, const Jugador& j, const Entidades& enemigos, const Entidades& proy, const Entidades& corazones) {
    int jx = aCelda(j.pos[0]), jy = aCelda(j.pos[1]);

    if (j.golpeActivo()) {
        Rect z = j.zonaGolpe();
        for (int fy = z.y; fy < z.y + z.alto; fy++)
            for (int fx = z.x; fx < z.x + z.ancho; fx++)
                if (!arena.esPared(fx, fy) && arena.lineaLibre(jx, jy, fx, fy))
                    pintarSprite(sprites.golpe, fx, fy, C_BLANCO);
    }
    else {
        int mx = jx, my = jy;
        if (j.mira == ARRIBA) my--;
        else if (j.mira == ABAJO) my++;
        else if (j.mira == IZQUIERDA) mx--;
        else mx++;
        if (!arena.esPared(mx, my)) pintarSprite(sprites.marca, mx, my, C_CIAN);
    }

    for (uint32_t i = 0; i < corazones.n; i++) {
        if (corazones.espera[i] < CORAZON_PARPADEA && (corazones.espera[i] / 5) % 2 == 1) continue;
        pintarSprite(sprites.corazon, aCelda(corazones.pos[2 * i]), aCelda(corazones.pos[2 * i + 1]), C_ROJO);
    }

    for (uint32_t i = 0; i < enemigos.n; i++) {
        if (!ASCIISpriteMgr::visible(enemigos.parpadeo[i])) continue;
        int cx = aCelda(enemigos.pos[2 * i]), cy = aCelda(enemigos.pos[2 * i + 1]);
        if (enemigos.tipo[i] == SOLDADO) pintarSprite(sprites.soldado, cx, cy, C_ROJO);
        else if (enemigos.tipo[i] == ARQUERO) pintarSprite(sprites.arquero, cx, cy, C_AMARILLO);
        else pintarSprite(sprites.mago, cx, cy, C_MAGENTA);
    }

    for (uint32_t i = 0; i < proy.n; i++) {
        int cx = aCelda(proy.pos[2 * i]), cy = aCelda(proy.pos[2 * i + 1]);
        int32_t vx = proy.vel[2 * i], vy = proy.vel[2 * i + 1];
        const Sprite& s = (vy == 0) ? sprites.protH : (vx == 0 ? sprites.protV : sprites.protM);
        pintarSprite(s, cx, cy, C_CIAN);
    }

    if (j.inmune == 0 || (j.inmune / 3) % 2 == 0)
        pintarSprite(sprites.jugador, jx, jy, C_VERDE);
}

// Escribe texto en el buffer en (x, y), recortando lo que se salga de la pantalla.
void Renderer::texto(int x, int y, const std::string& s, int color) {
    if (y < 0 || y >= PANT_H) return;
    for (size_t i = 0; i < s.size(); i++) {
        int cx = x + (int)i;
        if (cx < 0 || cx >= PANT_W) continue;
        buf[y][cx] = s[i];
        col[y][cx] = (unsigned char)color;
    }
}

// Igual que texto(), centrado horizontalmente.
void Renderer::textoCentrado(int y, const std::string& s, int color) {
    texto((PANT_W - (int)s.size()) / 2, y, s, color);
}

// La terminal debe ser de al menos 80x24.
bool Renderer::terminalSuficiente() const { return LINES >= PANT_H && COLS >= PANT_W; }

// Copia el buffer a ncurses y llama a refresh() UNA sola vez por cuadro.
void Renderer::refrescar() {
    if (!pantalla) return;
    if (!terminalSuficiente()) {
        erase();
        mvprintw(0, 0, "Terminal muy pequena (%dx%d). Se necesita 80x24.", COLS, LINES);
        refresh();
        return;
    }
    for (int y = 0; y < PANT_H; y++) {
        for (int x = 0; x < PANT_W; x++) {
            int c = col[y][x];
            if (hayColor) attrset(COLOR_PAIR(c) | (c == C_AZUL ? A_NORMAL : A_BOLD));
            else attrset(A_NORMAL);
            mvaddch(y, x, (chtype)(unsigned char)buf[y][x]);
        }
    }
    refresh();
}

// Capa 3 (HUD): vida con corazones, oleada, enemigos que quedan, puntos, mapa y controles.
void HUDManager::dibujar(Renderer& r, const DatosHUD& d) {
    std::string v = "VIDA ";
    for (int i = 0; i < VIDA_JUGADOR; i++) v += (i < d.vida) ? "<3 " : "-- ";
    r.texto(1, 0, v, C_ROJO);
    std::string o = "OLEADA " + std::to_string(d.oleada) + "   ENEMIGOS " + std::to_string(d.enemigos) +
        "   PUNTOS " + std::to_string(d.puntaje);
    r.texto(PANT_W - (int)o.size() - 1, 0, o, C_AMARILLO);
    r.texto(1, 1, std::string("Mapa: ") + d.mapa, C_CIAN);
    if (d.esperando) r.textoCentrado(1, "Preparate... oleada " + std::to_string(d.oleada + 1), C_CIAN);
    r.textoCentrado(PANT_H - 1, "Mover: WASD/flechas   Atacar: ESPACIO   Pausa: P   Salir: Q", C_BLANCO);
}

// Pantalla de inicio: creditos del grupo, titulo, instrucciones y leyenda de enemigos.
void HUDManager::menu(Renderer& r) {
    r.textoCentrado(0, "Andres Rodriguez Orozco - 2026087874", C_CIAN);
    r.textoCentrado(1, "Sebastian Marin Mora - 2026009715", C_CIAN);
    r.textoCentrado(2, "Julian Solorzano Hernandez - 2026086615", C_CIAN);
    r.textoCentrado(3, "Abril Gonzalez Morales - 2024207854", C_CIAN);
    r.textoCentrado(5, "=== THE LEGEND OF JUNE ===", C_AMARILLO);
    r.textoCentrado(7, "Sobrevive a oleadas de enemigos cada vez mas grandes", C_BLANCO);
    r.textoCentrado(10, "Mover: WASD o flechas", C_BLANCO);
    r.textoCentrado(11, "Atacar: ESPACIO (o J)", C_BLANCO);
    r.textoCentrado(12, "Pausa: P     Salir: Q", C_BLANCO);
    r.textoCentrado(14, "S = Soldado   A = Arquero   M = Mago", C_ROJO);
    r.textoCentrado(15, "Los arqueros y magos disparan: esquiva los proyectiles", C_CIAN);
    r.textoCentrado(16, "El mapa cambia al azar en cada oleada", C_AMARILLO);
    r.textoCentrado(17, "A veces los enemigos sueltan corazones <3 : recogelos para curarte", C_ROJO);
    r.textoCentrado(19, "Presiona ENTER para jugar", C_VERDE);
}

// Aviso de pausa.
void HUDManager::pausa(Renderer& r) {
    r.textoCentrado(11, "  PAUSA  ", C_AMARILLO);
    r.textoCentrado(12, " P para continuar ", C_BLANCO);
}

// Pantalla de fin de partida con la oleada alcanzada y los puntos.
void HUDManager::derrota(Renderer& r, const DatosHUD& d) {
    r.textoCentrado(9, "  FIN DE PARTIDA  ", C_ROJO);
    r.textoCentrado(11, " Llegaste a la oleada " + std::to_string(d.oleada) + " ", C_BLANCO);
    r.textoCentrado(12, " Puntos: " + std::to_string(d.puntaje) + " ", C_BLANCO);
    r.textoCentrado(14, " ENTER para volver al menu ", C_VERDE);
}

// Collision Detector 
// Llama a colisiones() y devuelve la matriz de resultados (res[i*nb + j]).
const uint8_t* CollisionDetector::comparar(const Rect* a, uint32_t na, const Rect* b, uint32_t nb) {
    if (na > (uint32_t)MAX_ENT) na = MAX_ENT;
    if (nb > (uint32_t)MAX_ENT) nb = MAX_ENT;
    colisiones(a, na, b, nb, res);
    return res;
}


// Si la entidad quedo dentro de una pared, la regresa a su posicion anterior; primero
// intenta conservar un eje para que pueda deslizarse a lo largo de la pared.
void CollisionDetector::frenarContraPared(const Arena& arena, int32_t* p, const int32_t* q) {
    int cx = aCelda(p[0]), cy = aCelda(p[1]);
    if (!arena.esPared(cx, cy)) return;
    int qx = aCelda(q[0]), qy = aCelda(q[1]);
    if (!arena.esPared(cx, qy)) {
        p[1] = q[1];
    } else if (!arena.esPared(qx, cy)) {
        p[0] = q[0];
    } else {
        p[0] = q[0];
        p[1] = q[1];
    }
}

// Desplaza una entidad (empuje) en 8 pasos, deteniendose antes de entrar en una pared.
void CollisionDetector::desplazarSinPared(const Arena& arena, int32_t* pos, int32_t* prev, int32_t dx, int32_t dy) {
    const int PASOS = 8;
    for (int k = 0; k < PASOS; k++) {
        int32_t nx = pos[0] + dx / PASOS, ny = pos[1] + dy / PASOS;
        if (arena.esPared(aCelda(nx), aCelda(ny))) break;
        pos[0] = nx;
        pos[1] = ny;
    }
    prev[0] = pos[0];
    prev[1] = pos[1];
}

// Modo de rendimiento (--bench) 
// Tiempo actual en nanosegundos (CLOCK_MONOTONIC).
static long long ahoraNs() {
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long long)t.tv_sec * 1000000000LL + t.tv_nsec;
}

// Mide (en microsegundos) el trabajo de mover_entidades() de un cuadro: jugador + n enemigos + n proyectiles.
static double medirMover(int n, int reps) {
    std::vector<int32_t> pj(2, 0), vj(2, 3), pe(2 * n, 100), ve(2 * n, 7), pp(2 * n, 50), vp(2 * n, 5);
    long long t0 = ahoraNs();
    for (int r = 0; r < reps; r++) {
        mover_entidades(pj.data(), vj.data(), 2);
        mover_entidades(pe.data(), ve.data(), 2 * n);
        mover_entidades(pp.data(), vp.data(), 2 * n);
    }
    return (ahoraNs() - t0) / 1000.0 / reps;
}

// Mide las tres comparaciones de un cuadro: jugador-enemigos, golpe-enemigos y proyectiles-jugador.
static double medirColisiones(int n, int reps) {
    std::vector<Rect> re(n), rp(n);
    for (int i = 0; i < n; i++) {
        re[i] = Rect{(i * 7) % 58, (i * 3) % 18, 1, 1};
        rp[i] = Rect{(i * 5) % 58, (i * 11) % 18, 1, 1};
    }
    Rect rj{30, 10, 1, 1}, z{31, 9, 3, 2};
    std::vector<uint8_t> res(MAX_ENT * MAX_ENT);
    long long t0 = ahoraNs();
    for (int r = 0; r < reps; r++) {
        colisiones(&rj, 1, re.data(), n, res.data());
        colisiones(&z, 1, re.data(), n, res.data());
        colisiones(rp.data(), n, &rj, 1, res.data());
    }
    return (ahoraNs() - t0) / 1000.0 / reps;
}

// Mide copiar_sprite() para los 2n+1 sprites de un cuadro.
static double medirSprites(int n, int reps) {
    std::vector<char> buf(PANT_W * PANT_H, ' ');
    char simbolo = 'S';
    DatosSprite d;
    d.buffer = buf.data();
    d.bufAncho = PANT_W;
    d.bufAlto = PANT_H;
    d.sprite = &simbolo;
    d.spAncho = 1;
    d.spAlto = 1;
    long long t0 = ahoraNs();
    for (int r = 0; r < reps; r++) {
        for (int i = 0; i < 2 * n + 1; i++) {
            d.x = 10 + (i * 7) % 60;
            d.y = 2 + (i * 3) % 20;
            copiar_sprite(&d);
        }
    }
    return (ahoraNs() - t0) / 1000.0 / reps;
}

// Imprime el tiempo por cuadro, el de cada rutina y la fraccion f del cuadro que
// ocupan las rutinas (dato para la Ley de Amdahl). Para medir tambien el refresh de
// ncurses hay que correrlo en una terminal real, sin --sin-pantalla.
void reporteBench(const BenchCuadro& b) {
    if (b.cuadros == 0) return;
#ifdef USAR_ASM
    const char* version = "ensamblador AA32";
#else
    const char* version = "C++";
#endif
    printf("=== BENCH  N=%d  (%d cuadros, rutinas en %s) ===\n", b.n, b.cuadros, version);
    if (!b.conPantalla)
        printf("(sin pantalla: el refresco de ncurses NO se midio)\n");
    printf("Tiempo promedio por cuadro:\n");
    printf("  cuadro completo : %10.2f us\n", b.total_us);
    printf("    logica        : %10.2f us\n", b.logica_us);
    printf("    pintar buffer : %10.2f us\n", b.pintar_us);
    printf("    refresh()     : %10.2f us\n", b.refresco_us);

    const int reps = 20000;
    double tm = medirMover(b.n, reps);
    double tc = medirColisiones(b.n, reps);
    double ts = medirSprites(b.n, reps);
    printf("Rutinas (trabajo de un cuadro, medido aislado, %d repeticiones):\n", reps);
    printf("  mover_entidades x3 : %10.3f us\n", tm);
    printf("  colisiones x3      : %10.3f us\n", tc);
    printf("  copiar_sprite x%-3d : %10.3f us\n", 2 * b.n + 1, ts);
    double suma = tm + tc + ts;
    printf("  suma rutinas       : %10.3f us\n", suma);
    if (b.total_us > 0)
        printf("Fraccion f del cuadro en las rutinas (para Amdahl): %.4f\n", suma / b.total_us);
}

// Game Engine 
// Ctrl+C o SIGTERM piden salir de forma ordenada (se restaura la terminal).
volatile std::sig_atomic_t senalSalir = 0;

static void alSenal(int) { senalSalir = 1; }

void instalarSenales() {
    std::signal(SIGINT, alSenal);
    std::signal(SIGTERM, alSenal);
}

// Aplica un comando a la maquina de estados (ver la descripcion de StateManager).
void StateManager::procesar(const Comando& c, bool& iniciarPartida, bool& salir) {
    if (c.accion == SALIR) {
        salir = true;
        return;
    }
    switch (estado) {
        case E_MENU:
            if (c.accion == ENTER) {
                estado = E_JUGANDO;
                iniciarPartida = true;
            }
            break;
        case E_JUGANDO:
            if (c.accion == PAUSA) estado = E_PAUSA;
            break;
        case E_PAUSA:
            if (c.accion == PAUSA) estado = E_JUGANDO;
            break;
        case E_FIN:
            if (c.accion == ENTER) estado = E_MENU;
            break;
    }
}

// Inicia ncurses, el renderer, la semilla aleatoria y carga el primer mapa.
void GameEngine::inicializar() {
    std::srand((unsigned)std::time(nullptr));
    input.iniciar();
    render.iniciar();
    cargarMapa(0);
}

// Carga mapaN.txt (N = i+1); si falta el archivo usa la arena por defecto.
void GameEngine::cargarMapa(int i) {
    if (i < 0 || i >= NUM_MAPAS) i = 0;
    mapa = i;
    char ruta[32];
    std::snprintf(ruta, sizeof(ruta), "mapa%d.txt", i + 1);
    if (!arena.cargar(ruta)) arena.generarPorDefecto();
}

// Cambia a otro mapa al azar entre oleadas y reubica al jugador y los corazones si quedaron en una pared.
void GameEngine::siguienteMapa() {
    int r = std::rand() % (NUM_MAPAS - 1);
    if (r >= mapa) r++;
    cargarMapa(r);
    int cx = aCelda(jugador.pos[0]), cy = aCelda(jugador.pos[1]);
    arena.libreCerca(cx, cy);
    jugador.pos[0] = jugador.prev[0] = aFijo(cx);
    jugador.pos[1] = jugador.prev[1] = aFijo(cy);
    jugador.vel[0] = jugador.vel[1] = 0;
    for (uint32_t i = 0; i < corazones.n; i++) {
        int hx = aCelda(corazones.pos[2 * i]), hy = aCelda(corazones.pos[2 * i + 1]);
        if (arena.esPared(hx, hy) || arena.esPared(hx + 1, hy)) {
            arena.libreCerca(hx, hy);
            if (arena.esPared(hx + 1, hy)) hx--;
            corazones.pos[2 * i] = corazones.prev[2 * i] = aFijo(hx);
            corazones.pos[2 * i + 1] = corazones.prev[2 * i + 1] = aFijo(hy);
        }
    }
}

// Reinicia todo para una partida nueva: mapa, jugador, enemigos, proyectiles, corazones, oleada y puntos.
void GameEngine::nuevaPartida(int mapaFijo) {
    cargarMapa(mapaFijo >= 0 ? mapaFijo : std::rand() % NUM_MAPAS);
    int cx = ARENA_W / 2, cy = ARENA_H / 2;
    arena.libreCerca(cx, cy);
    jugador.reiniciar(cx, cy);
    enemigos.n = 0;
    proyectiles.n = 0;
    corazones.n = 0;
    oleadas.reiniciar();
    puntaje = 0;
    esperando = true;
    pausaOleada = 20;
}
// Ciclo principal: ejecuta un cuadro y espera lo que falte hasta completar 33 ms.
int GameEngine::ejecutar() {
    inicializar();
    while (!salir && !senalSalir) {
        timespec inicio;
        clock_gettime(CLOCK_MONOTONIC, &inicio);
        cuadro();
        controlarTiempo(inicio);
    }
    musica.detener();
    input.cerrar();
    return 0;
}

// Un cuadro: procesa comandos y estados corre la logica solo si se esta jugando, y dibuja.
void GameEngine::cuadro() {
    std::vector<Comando> cmds = input.leerComandos();
    bool iniciar = false;
    for (size_t i = 0; i < cmds.size(); i++)
        estados.procesar(cmds[i], iniciar, salir);
    if (salir) return;
    if (iniciar) nuevaPartida();
    musica.actualizar(estados.actual());
    if (estados.actual() == E_JUGANDO) logica(cmds);
    dibujar();
}

// Logica de un cuadro, siempre en este orden: (1) comandos al jugador, (2) actualizar
// y mover entidades (3) colisiones (4) eliminar muertos y recoger corazones,
// (5) pedir la siguiente oleada. Si el jugador murio pasa a fin de partida.
void GameEngine::logica(const std::vector<Comando>& cmds) {
    jugador.leerComandos(cmds);
    actualizarEntidades();
    bool murio = revisarColisiones();
    limpiarMuertos();
    recogerCorazones();
    pedirOleada();
    if (murio) estados.derrota();
}

// Jugador movimientos y acciones de los enemigos (con la funcion para crear proyectiles) y movimiento de enemigos y proyectiles.
void GameEngine::actualizarEntidades() {
    jugador.actualizar();
    Enemigos::actualizar(enemigos, jugador.pos, [this](int32_t x, int32_t y, int32_t vx, int32_t vy) {
        Proyectiles::crear(proyectiles, x, y, vx, vy);
        });
    Enemigos::mover(enemigos);
    Proyectiles::mover(proyectiles);
}

// Aleja y aturde a un enemigo que acaba de tocar al jugador.
void GameEngine::empujarEnemigo(uint32_t j) {
    int32_t dx = enemigos.pos[2 * j] - jugador.pos[0];
    int32_t dy = enemigos.pos[2 * j + 1] - jugador.pos[1];
    int64_t m = std::max<int64_t>(std::abs(dx), 2 * (int64_t)std::abs(dy));
    if (m == 0) {
        dx = (std::rand() & 1) ? 1 : -1;
        dy = 0;
        m = 1;
    }
    int32_t px = (int32_t)((int64_t)dx * aFijo(EMPUJE_CELDAS) / m);
    int32_t py = (int32_t)((int64_t)dy * aFijo(EMPUJE_CELDAS) / m);
    CollisionDetector::desplazarSinPared(arena, &enemigos.pos[2 * j], &enemigos.prev[2 * j], px, py);
    Enemigos::aturdir(enemigos, j, ATURDIR_CUADROS);
}

// Paredes (frenan al jugador y a los enemigos, destruyen proyectiles) y tres
// comparaciones de rectangulos: jugador-enemigos (dano al jugador si no es
// invulnerable), zona de golpe-enemigos (dano y parpadeo) y proyectiles-jugador
// (dano y el proyectil desaparece) Devuelve true si el jugador murio.
bool GameEngine::revisarColisiones() {

    CollisionDetector::frenarContraPared(arena, jugador.pos, jugador.prev);
    for (uint32_t i = 0; i < enemigos.n; i++)
        CollisionDetector::frenarContraPared(arena, &enemigos.pos[2 * i], &enemigos.prev[2 * i]);
    for (uint32_t i = 0; i < proyectiles.n; i++) {
        if (arena.esPared(aCelda(proyectiles.pos[2 * i]), aCelda(proyectiles.pos[2 * i + 1])))
            proyectiles.vida[i] = 0;
    }

    bool murio = false;
    Rect rj = jugador.rect();
    Rect re[MAX_ENT];
    Rect rp[MAX_ENT];
    for (uint32_t i = 0; i < enemigos.n; i++) re[i] = enemigos.rect(i);
    for (uint32_t i = 0; i < proyectiles.n; i++) rp[i] = proyectiles.rect(i);

    if (enemigos.n > 0) {
        const uint8_t* res = colisionador.comparar(&rj, 1, re, enemigos.n);
        bool hayContacto = false;
        for (uint32_t j = 0; j < enemigos.n; j++)
            if (res[j] && enemigos.vida[j] > 0) hayContacto = true;
        if (hayContacto && jugador.inmune == 0) {
            if (jugador.recibirDano()) murio = true;
            for (uint32_t j = 0; j < enemigos.n; j++)
                if (res[j] && enemigos.vida[j] > 0) empujarEnemigo(j);
            for (uint32_t i = 0; i < enemigos.n; i++) re[i] = enemigos.rect(i);
        }
    }

    if (jugador.golpeActivo() && enemigos.n > 0) {
        Rect z = jugador.zonaGolpe();
        const uint8_t* res = colisionador.comparar(&z, 1, re, enemigos.n);
        for (uint32_t j = 0; j < enemigos.n; j++)
            if (res[j] && arena.lineaLibre(aCelda(jugador.pos[0]), aCelda(jugador.pos[1]),
                aCelda(enemigos.pos[2 * j]), aCelda(enemigos.pos[2 * j + 1])))
                Enemigos::recibirDano(enemigos, j);
    }

    if (proyectiles.n > 0) {
        const uint8_t* res = colisionador.comparar(rp, proyectiles.n, &rj, 1);
        for (uint32_t i = 0; i < proyectiles.n; i++) {
            if (res[i] && proyectiles.vida[i] > 0) {
                if (jugador.recibirDano()) murio = true;
                proyectiles.vida[i] = 0;
            }
        }
    }
    return murio;
}

// Elimina enemigos y proyectiles sin vida al final del cuadro cada enemigo da 10 puntos y puede soltar un corazon.
void GameEngine::limpiarMuertos() {
    for (uint32_t i = 0; i < enemigos.n;) {
        if (enemigos.vida[i] <= 0) {
            puntaje += 10;
            if (std::rand() % 100 < PROB_CORAZON)
                soltarCorazon(enemigos.pos[2 * i], enemigos.pos[2 * i + 1]);
            enemigos.eliminar(i);
        }
        else {
            i++;
        }
    }
    for (uint32_t i = 0; i < proyectiles.n;) {
        if (proyectiles.vida[i] <= 0) proyectiles.eliminar(i);
        else i++;
    }
}

// Crea un corazon en la celda del enemigo (ocupa 2 columnas: "<3") su vida util es VIDA_CORAZON cuadros.
void GameEngine::soltarCorazon(int32_t x, int32_t y) {
    int cx = aCelda(x), cy = aCelda(y);
    if (arena.esPared(cx, cy)) arena.libreCerca(cx, cy);
    if (arena.esPared(cx + 1, cy)) cx--;          // el corazon ocupa 2 columnas ("<3")
    if (corazones.agregar(0, 1, aFijo(cx), aFijo(cy), 0, 0))
        corazones.espera[corazones.n - 1] = VIDA_CORAZON;
}

// Cuenta regresiva de cada corazon si el jugador lo toca y tiene la vida incompleta
// le suma 1 de vida y el corazon desaparece Tambien desaparece al agotarse su tiempo.
void GameEngine::recogerCorazones() {
    for (uint32_t i = 0; i < corazones.n;) {
        if (corazones.espera[i] > 0) corazones.espera[i]--;
        bool tomado = false;
        if (corazones.espera[i] > 0 && jugador.vida < VIDA_JUGADOR) {
            Rect rc{ aCelda(corazones.pos[2 * i]), aCelda(corazones.pos[2 * i + 1]), 2, 1 };
            Rect rj = jugador.rect();
            uint8_t toca = 0;
            colisiones(&rj, 1, &rc, 1, &toca);     // misma rutina de colisiones del juego
            if (toca) {
                jugador.vida++;
                tomado = true;
            }
        }
        if (tomado || corazones.espera[i] <= 0) corazones.eliminar(i);
        else i++;
    }
}

// Si no quedan enemigos: la primera vez cambia de mapa y espera 45 cuadros despues
// lanza la siguiente oleada.
void GameEngine::pedirOleada() {
    if (!oleadas.terminada(enemigos.n)) return;
    if (!esperando) {
        esperando = true;
        pausaOleada = 45;
        proyectiles.n = 0;
        siguienteMapa();
    }
    else if (--pausaOleada <= 0) {
        oleadas.lanzar(arena, aCelda(jugador.pos[0]), aCelda(jugador.pos[1]),
            [this](int tipo, int vida, int32_t x, int32_t y) {
                return Enemigos::agregar(enemigos, tipo, vida, x, y);
            });
        esperando = false;
    }
}

// Arma el cuadro completo en el buffer: menu, o arena + entidades + HUD (y pausa/derrota encima).
void GameEngine::pintar() {
    render.limpiar();
    EstadoPartida e = estados.actual();
    if (e == E_MENU) {
        HUDManager::menu(render);
    }
    else {
        DatosHUD d;
        d.vida = jugador.vida < 0 ? 0 : jugador.vida;
        d.oleada = oleadas.oleadaActual();
        d.enemigos = (int)enemigos.n;
        d.puntaje = puntaje;
        d.mapa = nombreMapa(mapa);
        d.esperando = esperando && enemigos.n == 0;
        render.pintarArena(arena);
        render.pintarEntidades(arena, jugador, enemigos, proyectiles, corazones);
        HUDManager::dibujar(render, d);
        if (e == E_PAUSA) HUDManager::pausa(render);
        if (e == E_FIN) HUDManager::derrota(render, d);
    }
}
// Pinta el buffer y lo manda a la pantalla.
void GameEngine::dibujar() {
    pintar();
    render.refrescar();
}

// Duerme lo que falte para que el cuadro dure 33 ms, asi el juego va a velocidad pareja.
void GameEngine::controlarTiempo(const timespec& inicio) {
    timespec ahora;
    clock_gettime(CLOCK_MONOTONIC, &ahora);
    long usado = (ahora.tv_sec - inicio.tv_sec) * 1000000000L + (ahora.tv_nsec - inicio.tv_nsec);
    if (usado < NS_CUADRO) {
        timespec dormir = { 0, NS_CUADRO - usado };
        nanosleep(&dormir, nullptr);
    }
}

void GameEngine::celdaLibreAleatoria(int& cx, int& cy) const {
    do {
        cx = 1 + std::rand() % (ARENA_W - 2);
        cy = 1 + std::rand() % (ARENA_H - 2);
    } while (arena.esPared(cx, cy));
}

// Modo de rendimiento: N enemigos (invencibles) y N proyectiles, 1000 cuadros sin
// esperar entre ellos y con semilla fija; mide logica, pintar y refresh por separado.
BenchCuadro GameEngine::benchmark(int n, bool conPantalla) {
    BenchCuadro r;
    r.n = n;
    r.conPantalla = conPantalla;
    if (n < 1) n = 1;
    if (n > MAX_ENT) n = MAX_ENT;

    std::srand(12345);
    if (conPantalla) input.iniciar();
    render.iniciar(conPantalla);
    if (conPantalla && !render.terminalSuficiente()) {
        input.cerrar();
        std::fprintf(stderr, "Terminal muy pequena. Se necesita 80x24.\n");
        return r;
    }
    nuevaPartida(0);
    estados.forzarJugando();
    esperando = false;
    jugador.vida = 1000000000;
    const int BIG = 1000000000;
    for (int i = 0; i < n; i++) {
        int t = (i % 3 != 2) ? SOLDADO : (((i / 3) % 2 == 0) ? ARQUERO : MAGO);
        int cx, cy;
        celdaLibreAleatoria(cx, cy);
        Enemigos::agregar(enemigos, t, BIG, aFijo(cx), aFijo(cy));
    }

    const int CUADROS = 1000;
    const std::vector<Comando> sinComandos;
    long long tTotal = 0, tLogica = 0, tPintar = 0, tRefresco = 0;
    for (int c = 0; c < CUADROS; c++) {
        while (proyectiles.n < (uint32_t)n) {
            int cx, cy;
            celdaLibreAleatoria(cx, cy);
            int d = std::rand() % 4;
            int32_t vx = (d == 2) ? -VEL_PROY_X : (d == 3 ? VEL_PROY_X : 0);
            int32_t vy = (d == 0) ? -VEL_PROY_Y : (d == 1 ? VEL_PROY_Y : 0);
            Proyectiles::crear(proyectiles, aFijo(cx), aFijo(cy), vx, vy);
        }
        long long t0 = ahoraNs();
        logica(sinComandos);
        long long t1 = ahoraNs();
        pintar();
        long long t2 = ahoraNs();
        render.refrescar();
        long long t3 = ahoraNs();
        tLogica += t1 - t0;
        tPintar += t2 - t1;
        tRefresco += t3 - t2;
        tTotal += t3 - t0;
    }
    if (conPantalla) input.cerrar();

    r.cuadros = CUADROS;
    r.total_us = tTotal / 1000.0 / CUADROS;
    r.logica_us = tLogica / 1000.0 / CUADROS;
    r.pintar_us = tPintar / 1000.0 / CUADROS;
    r.refresco_us = tRefresco / 1000.0 / CUADROS;
    return r;
}

// MUSICA
#ifdef __unix__
// Busca un ejecutable en el PATH (igual que la shell).
static bool programaDisponible(const char* nombre) {
    const char* ruta = std::getenv("PATH");
    if (!ruta) return false;
    const char* p = ruta;
    while (true) {
        const char* fin = std::strchr(p, ':');
        size_t n = fin ? (size_t)(fin - p) : std::strlen(p);
        char cand[512];
        if (n + std::strlen(nombre) + 2 < sizeof(cand)) {
            std::memcpy(cand, p, n);
            cand[n] = '/';
            std::strcpy(cand + n + 1, nombre);
            if (access(cand, X_OK) == 0) return true;
        }
        if (!fin) return false;
        p = fin + 1;
    }
}

// Formato de comando del primer reproductor disponible (se busca una sola vez).
static const char* reproductorDisponible() {
    if (programaDisponible("aplay"))  return "aplay -q '%s'";
    if (programaDisponible("paplay")) return "paplay '%s'";
    if (programaDisponible("ffplay")) return "ffplay -nodisp -autoexit -loglevel quiet '%s'";
    if (programaDisponible("mpv"))    return "mpv --no-video --really-quiet --no-terminal '%s'";
    if (programaDisponible("play"))   return "play -q '%s'";
    return nullptr;
}
#endif

// Lanza en segundo plano un bucle que repite el WAV hasta que se mate el proceso.
void Musica::iniciar(const char* archivo) {
#ifdef __unix__
    static const char* formato = nullptr;
    static bool buscado = false;
    if (!buscado) {
        buscado = true;
        formato = reproductorDisponible();
    }
    if (!formato || access(archivo, R_OK) != 0) return;

    char pista[600];
    char comando[700];
    std::snprintf(pista, sizeof(pista), formato, archivo);
    std::snprintf(comando, sizeof(comando), "while :; do %s || sleep 1; done", pista);

    pid_t p = fork();
    if (p < 0) return;
    if (p == 0) {
        setpgid(0, 0);
        int fd = open("/dev/null", O_RDWR);
        if (fd >= 0) {
            dup2(fd, STDIN_FILENO);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            if (fd > 2) close(fd);
        }
        execl("/bin/sh", "sh", "-c", comando, (char*)nullptr);
        _exit(127);
    }
    setpgid(p, p);
    pid = (int)p;
#else
    (void)archivo;
#endif
}

// Corta la pista en curso (shell y reproductor, en su propio grupo de procesos).
void Musica::detener() {
#ifdef __unix__
    if (pid <= 0) return;
    kill(-pid, SIGTERM);
    kill(pid, SIGTERM);
    while (waitpid(pid, nullptr, 0) < 0 && errno == EINTR) {}
    pid = -1;
#endif
}

// Menu -> pista del menu; jugando, pausa y fin -> pista de batalla.
void Musica::actualizar(EstadoPartida e) {
    Pista deseada = (e == E_MENU) ? PISTA_MENU : PISTA_BATALLA;
    if (deseada == actual) return;
    detener();
    actual = deseada;
    if (deseada == PISTA_MENU) iniciar("MUSIC/JUNE_MENU.wav");
    else iniciar("MUSIC/JUNE_BATTLE.wav");
}

// Ayuda de linea de comandos.
static void uso(const char* prog) {
    std::printf("Uso:\n");
    std::printf("  %s                                   jugar (el mapa cambia al azar en cada oleada)\n", prog);
    std::printf("  %s --bench N [--sin-pantalla]\n", prog);
    std::printf("      mide el rendimiento con N enemigos y N proyectiles (1 <= N <= %d)\n", MAX_ENT);
}

// Sin argumentos juega; con --bench N [--sin-pantalla] corre el modo de rendimiento.
int main(int argc, char** argv) {
    int bench = 0;
    bool sinPantalla = false;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--bench") && i + 1 < argc) {
            bench = std::atoi(argv[++i]);
            if (bench < 1 || bench > MAX_ENT) {
                uso(argv[0]);
                return 1;
            }
        }
        else if (!std::strcmp(argv[i], "--sin-pantalla")) {
            sinPantalla = true;
        }
        else if (!std::strcmp(argv[i], "--ayuda") || !std::strcmp(argv[i], "-h")) {
            uso(argv[0]);
            return 0;
        }
        else {
            uso(argv[0]);
            return 1;
        }
    }

    instalarSenales();
    GameEngine juego;

    if (bench > 0) {
        bool conPantalla = !sinPantalla && isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
        BenchCuadro b = juego.benchmark(bench, conPantalla);
        if (b.cuadros == 0) return 1;
        reporteBench(b);
        return 0;
    }

    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        std::fprintf(stderr, "El juego necesita una terminal interactiva.\n");
        return 1;
    }
    return juego.ejecutar();
}
