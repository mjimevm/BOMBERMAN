/*
*-----------------------------------------------------------
* main.cpp
*-----------------------------------------------------------
* UNIVERSIDAD DEL VALLE DE GUATEMALA
* FACULTAD DE INGENIERÍA
* DEPARTAMENTO DE CIENCIA DE LA COMPUTACIÓN
*
* CC3086 - Programacion de Microprocesadores
*
*-----------------------------------------------------------
* Descripción: Juego de Bomberman
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
    int cantidadBombas;
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

void inicializarJugadores(Juego &j) {
    Jugador p1 = {"Player1", 3, randLargo(gen), randAlto(gen), 1};
    Jugador p2 = {"Player2", 3, randLargo(gen), randAlto(gen), 1};
    j.jugadores.push_back(p1);
    j.jugadores.push_back(p2);
};

void inicializarMapa(Juego &j) {
    j.mapa.alto = 15;
    j.mapa.largo = 31;
    
    //Asignación de memoria para el mapa
    j.mapa.posiciones = new char*[j.mapa.alto];
    for (int i = 0; i < j.mapa.alto; i++) {
        j.mapa.posiciones[i] = new char[j.mapa.largo];
    }

    for (int y = 0; y < j.mapa.alto; y++) {
        for (int x = 0; x < j.mapa.largo; x++) {
            //Bordes
            if (y == 0 || y == j.mapa.alto -1 || x == 0 || x == j.mapa.largo - 1) {
                j.mapa.posiciones[y][x] = 61; //caracter ASCII =
            } 
            //muros indestructibles en posiciones pares dejando un espacio libre entre el borde y el muro
            else if (y % 2 == 0 && x % 2 == 0) { 
                j.mapa.posiciones[y][x] = 61; //caracter ASCII =
            } 
            // muros destructibles en posiciones randoms libres
            else {
                if (rand1to9(gen) == 3) {
                    j.mapa.posiciones[y][x] = 35; //caracter ASCII #
                }
                else {
                    j.mapa.posiciones[y][x] = 32; //caracter ASCII espacio
                }
            }
        }
    }
};

void dibujarMapa(const Juego &j) {
    for (int y = 0; y < j.mapa.alto; y++) {
        for (int x = 0; x < j.mapa.largo; x++) {
            //mvaddch pone el caracter en la coordenada 
            mvaddch(y, x, (unsigned char)j.mapa.posiciones[y][x]);
        }
    }
    refresh();
};

void moverJugador(Juego &j, int jugador, int dx, int dy) {
    int newX = j.jugadores[jugador].x + dx;
    int newY = j.jugadores[jugador].y + dy;

    j.mapa.posiciones[j.jugadores[jugador].y][j.jugadores[jugador].x] = 32; // Limpiar la posición anterior

    // Actualizar la posición del jugador en el mapa
    j.jugadores[jugador].x = newX;
    j.jugadores[jugador].y = newY;
    j.mapa.posiciones[newY][newX] = 64; // caracter ASCII @
};

int main() {
    Juego bomberman;
    pthread_mutex_init(&mutex, NULL);
    
    //configuración de ncurses
    initscr();             // Inicia ncurses
    noecho();              // ocultar las letras que se presionan
    curs_set(0);           // Oculta el cursor
    keypad(stdscr, TRUE);  // Habilita las flechas del teclado

    // Inicializar el mapa y los jugadores
    inicializarMapa(bomberman);
    inicializarJugadores(bomberman);
    bomberman.mapa.posiciones[bomberman.jugadores[0].y][bomberman.jugadores[0].x] = 64; // caracter ASCII @
    bomberman.mapa.posiciones[bomberman.jugadores[1].y][bomberman.jugadores[1].x] = 64; // caracter ASCII @

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

            // P2: IJKL
            case 'i': case 'I': moverJugador(bomberman, 1, 0, -1); break;
            case 'k': case 'K': moverJugador(bomberman, 1, 0, 1); break;
            case 'j': case 'J': moverJugador(bomberman, 1, -1, 0); break;
            case 'l': case 'L': moverJugador(bomberman, 1, 1, 0); break;

            case '0': jugando = false; break; // Salir
        }
    }

    endwin(); // Termina ncurses
    pthread_mutex_destroy(&mutex);
    return 0;
};