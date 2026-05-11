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

#include <stdio.h>
#include <iostream>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <vector>
#include <pthread.h>
#include <windows.h>
#include <conio.h>
#include <time.h>
#include <map>

pthread_mutex_t mutex;

struct Jugador {
    std::string name;
    int vida;
    int x;
    int y;
    int cantidadBombas;
    std::vector<std::pair<int, int>> bombas; // Vector para almacenar las posiciones
};

struct Enemigo {
    std::string name;
    int vida;
    int x;
    int y;
};

struct Mapa {
    int alto;
    int largo;
    char** mapa;
}

struct Bomba {
    int distancia; 
    int x;
    int y;
}

struct Muro {
    int x;
    int y;
    bool destructible;

}

struct Juego {
    Mapa mapa;
    std::vector<Jugador> jugadores;
    std::vector<Enemigo> enemigos;
    std::vector<Bomba> bombas;
    std::vector<Muro> muros;
}

int main() {
    // Inicializar mutex
    pthread_mutex_init(&mutex, NULL);

    pthread_mutex_destroy(&mutex);
    return 0;
}