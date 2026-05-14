/*
*-----------------------------------------------------------
* main.cpp
*-----------------------------------------------------------
* UNIVERSIDAD DEL VALLE DE GUATEMALA
* FACULTAD DE INGENIERIA
* DEPARTAMENTO DE CIENCIA DE LA COMPUTACION
*
* CC3086 - Programacion de Microprocesadores
*
*-----------------------------------------------------------
* Descripcion: Juego de Bomberman
*-----------------------------------------------------------
*/

#include <iostream>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <vector>
#include <chrono>
#include <ncurses.h>
#include <random>

using namespace std;

pthread_mutex_t mutex;

random_device rd; 
mt19937 gen(rd()); 
uniform_int_distribution<> randAlto(1, 13);
uniform_int_distribution<> randLargo(1, 29);
uniform_int_distribution<> rand1to9(1, 9);

struct Jugador {
    string name;
    int vida;
    int x;
    int y;
    int cantidad;
};

struct Enemigo {
    string name;
    int vida;
    int x;
    int y;
};

struct Mapa {
    int alto;
    int largo;
    char** posiciones;
};

struct Bomba {
    int distancia; 
    int x;
    int y;
};

struct Muro {
    int x;
    int y;
    bool destructible;
};

struct Juego {
    Mapa mapa;
    vector<Jugador> jugadores;
    vector<Enemigo> enemigos;
    vector<Bomba> bombas;
    vector<Muro> muros;
};

//Estructura para enviar los datos de la bomba al hilo
struct Explosion {
    Juego* juego;
    Bomba bomba;
};

void inicializarJugadores(Juego &j) {
    Jugador p1 = {"Player1", 3, 1, 1, 1};
    Jugador p2 = {"Player2", 3, 29, 13, 1};

    j.jugadores.push_back(p1);
    j.jugadores.push_back(p2);
};

//Funcion de lugar de inicio de jugadores
bool zonaSalida(int x, int y) {
    // esquina superior izquierda
    if ((x >= 1 && x <= 3) && (y >= 1 && y <= 3)) {
        return true;
    }

    // esquina inferior derecha
    if ((x >= 27 && x <= 29) && (y >= 11 && y <= 13)) {
        return true;
    }

    return false;
}

void inicializarMapa(Juego &j) {
    j.mapa.alto = 15;
    j.mapa.largo = 31;
    
    //Asignacion de memoria para el mapa
    j.mapa.posiciones = new char*[j.mapa.alto];
    for (int i = 0; i < j.mapa.alto; i++) {
        j.mapa.posiciones[i] = new char[j.mapa.largo];
    }

    for (int y = 0; y < j.mapa.alto; y++) {
        for (int x = 0; x < j.mapa.largo; x++) {
            //Bordes
            if (y == 0 || y == j.mapa.alto -1 || x == 0 || x == j.mapa.largo - 1) {
                j.mapa.posiciones[y][x] = '='; //caracter ASCII =
            } 
            //muros indestructibles en posiciones pares dejando un espacio libre entre el borde y el muro
            else if (y % 2 == 0 && x % 2 == 0) { 
                j.mapa.posiciones[y][x] = '='; //caracter ASCII =
            } 
            // muros destructibles en posiciones randoms libres
           else {
                if (zonaSalida(x, y)) {
                    j.mapa.posiciones[y][x] = ' ';
                }
                else if (rand1to9(gen) == 3) {
                    j.mapa.posiciones[y][x] = '#'; //caracter ASCII #
                }
                else {
                    j.mapa.posiciones[y][x] = ' '; //caracter ASCII espacio
                }
            }
        }
    }
};

void dibujarMapa(const Juego &j) {
    clear();

    for (int y = 0; y < j.mapa.alto; y++) {
        for (int x = 0; x < j.mapa.largo; x++) {
            //mvaddch pone el caracter en la coordenada 
            mvaddch(y, x, (unsigned char)j.mapa.posiciones[y][x]);
        }
    }

    refresh();
};

bool hayBomba(Juego &j, int x, int y) {
    for (int i = 0; i < j.bombas.size(); i++) {
        if (j.bombas[i].x == x && j.bombas[i].y == y) {
            return true;
        }
    }

    return false;
}

void eliminarBomba(Juego &j, Bomba bomba) {
    for (int i = 0; i < j.bombas.size(); i++) {
        if (j.bombas[i].x == bomba.x && j.bombas[i].y == bomba.y) {
            j.bombas.erase(j.bombas.begin() + i);
            return;
        }
    }
}

void moverJugador(Juego &j, int jugador, int dx, int dy) {
    pthread_mutex_lock(&mutex);

    int oldX = j.jugadores[jugador].x;
    int oldY = j.jugadores[jugador].y;

    int newX = j.jugadores[jugador].x + dx;
    int newY = j.jugadores[jugador].y + dy;

    if (j.mapa.posiciones[newY][newX] == ' ') {
        if (hayBomba(j, oldX, oldY)) {
            j.mapa.posiciones[oldY][oldX] = 'O'; // caracter ASCII O
        }
        else {
            j.mapa.posiciones[oldY][oldX] = ' '; // Limpiar la posicion anterior
        }

        // Actualizar la posicion del jugador en el mapa
        j.jugadores[jugador].x = newX;
        j.jugadores[jugador].y = newY;
        j.mapa.posiciones[newY][newX] = '@'; // caracter ASCII @
    }

    pthread_mutex_unlock(&mutex);
};

//Marca una parte de la explosion en el mapa
void marcarExplosion(Juego &j, int x, int y) {
    if (x <= 0 || x >= j.mapa.largo - 1 || y <= 0 || y >= j.mapa.alto - 1) {
        return;
    }

    if (j.mapa.posiciones[y][x] == '=') {
        return;
    }

    j.mapa.posiciones[y][x] = '*';
}

//Expande la explosion de la bomba hacia arriba, abajo, izquierda y derecha
void explotarBomba(Juego &j, Bomba bomba) {
    marcarExplosion(j, bomba.x, bomba.y);

    for (int i = 1; i <= bomba.distancia; i++) {

        // derecha
        if (j.mapa.posiciones[bomba.y][bomba.x + i] != '=') {

            if (j.mapa.posiciones[bomba.y][bomba.x + i] == '#') {
                j.mapa.posiciones[bomba.y][bomba.x + i] = '*';
            }
            else {
                marcarExplosion(j, bomba.x + i, bomba.y);
            }
        }

        // izquierda
        if (j.mapa.posiciones[bomba.y][bomba.x - i] != '=') {

            if (j.mapa.posiciones[bomba.y][bomba.x - i] == '#') {
                j.mapa.posiciones[bomba.y][bomba.x - i] = '*';
            }
            else {
                marcarExplosion(j, bomba.x - i, bomba.y);
            }
        }

        // abajo
        if (j.mapa.posiciones[bomba.y + i][bomba.x] != '=') {

            if (j.mapa.posiciones[bomba.y + i][bomba.x] == '#') {
                j.mapa.posiciones[bomba.y + i][bomba.x] = '*';
            }
            else {
                marcarExplosion(j, bomba.x, bomba.y + i);
            }
        }

        // arriba
        if (j.mapa.posiciones[bomba.y - i][bomba.x] != '=') {

            if (j.mapa.posiciones[bomba.y - i][bomba.x] == '#') {
                j.mapa.posiciones[bomba.y - i][bomba.x] = '*';
            }
            else {
                marcarExplosion(j, bomba.x, bomba.y - i);
            }
        }
    }
}

//Limpia la explosion despues de mostrarse un momento
void limpiarExplosion(Juego &j) {
    for (int y = 0; y < j.mapa.alto; y++) {
        for (int x = 0; x < j.mapa.largo; x++) {
            if (j.mapa.posiciones[y][x] == '*') {
                j.mapa.posiciones[y][x] = ' ';
            }
        }
    }

    for (int i = 0; i < j.jugadores.size(); i++) {
        j.mapa.posiciones[j.jugadores[i].y][j.jugadores[i].x] = '@';
    }

    for (int i = 0; i < j.bombas.size(); i++) {
        j.mapa.posiciones[j.bombas[i].y][j.bombas[i].x] = 'O';
    }
}

//Hilo temporizado para que la bomba explote despues de unos segundos
void* hiloBomba(void* arg) {
    Explosion* datos = (Explosion*) arg;

    sleep(3);

    pthread_mutex_lock(&mutex);

    eliminarBomba(*datos->juego, datos->bomba);
    explotarBomba(*datos->juego, datos->bomba);
    dibujarMapa(*datos->juego);

    pthread_mutex_unlock(&mutex);

    usleep(700000);

    pthread_mutex_lock(&mutex);

    limpiarExplosion(*datos->juego);
    dibujarMapa(*datos->juego);

    pthread_mutex_unlock(&mutex);

    delete datos;
    return NULL;
}

//Coloca una bomba en la posicion actual del jugador
void colocarBomba(Juego &j, int jugador) {
    pthread_mutex_lock(&mutex);

    Bomba bomba;
    bomba.x = j.jugadores[jugador].x;
    bomba.y = j.jugadores[jugador].y;
    bomba.distancia = 1;

    j.bombas.push_back(bomba);
    j.mapa.posiciones[bomba.y][bomba.x] = 'O'; // caracter ASCII O

    Explosion* datos = new Explosion;
    datos->juego = &j;
    datos->bomba = bomba;

    pthread_t hilo;
    pthread_create(&hilo, NULL, hiloBomba, datos);
    pthread_detach(hilo);

    pthread_mutex_unlock(&mutex);
}

int main() {
    Juego bomberman;
    pthread_mutex_init(&mutex, NULL);
    
    //configuracion de ncurses
    initscr();             // Inicia ncurses
    noecho();              // ocultar las letras que se presionan
    curs_set(0);           // Oculta el cursor
    keypad(stdscr, TRUE);  // Habilita las flechas del teclado

    // Inicializar el mapa y los jugadores
    inicializarMapa(bomberman);
    inicializarJugadores(bomberman);
    bomberman.mapa.posiciones[bomberman.jugadores[0].y][bomberman.jugadores[0].x] = '@';
    bomberman.mapa.posiciones[bomberman.jugadores[1].y][bomberman.jugadores[1].x] = '@';

    // Bucle principal del juego
    bool jugando = true;
    while(jugando) {
        dibujarMapa(bomberman);

        int ch = getch(); //Detecta la tecla presionada
        switch(ch) {
            // P1: WASD
            case 'w': case 'W': moverJugador(bomberman, 0, 0, -1); break;
            case 's': case 'S': moverJugador(bomberman, 0, 0, 1); break;
            case 'a': case 'A': moverJugador(bomberman, 0, -1, 0); break;
            case 'd': case 'D': moverJugador(bomberman, 0, 1, 0); break;
            case 'e': case 'E': colocarBomba(bomberman, 0); break; // bomba jugador 1

            // P2: IJKL
            case 'i': case 'I': moverJugador(bomberman, 1, 0, -1); break;
            case 'k': case 'K': moverJugador(bomberman, 1, 0, 1); break;
            case 'j': case 'J': moverJugador(bomberman, 1, -1, 0); break;
            case 'l': case 'L': moverJugador(bomberman, 1, 1, 0); break;
            case 'o': case 'O': colocarBomba(bomberman, 1); break; // bomba jugador 2

            case '0': jugando = false; break; // Salir
        }
    }

    endwin(); // Termina ncurses
    pthread_mutex_destroy(&mutex);
    return 0;
};
