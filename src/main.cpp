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
#include <semaphore.h>
#include <unistd.h>
#include <vector>
#include <chrono>
#include <ncurses.h>
#include <random>
#include <tuple>

using namespace std;

// Mutex para sincronizaci�n entre hilos
pthread_mutex_t mutex;

//Semaforo
sem_t sem1;
sem_t sem2;

// Generadores de n�meros aleatorios
random_device rd; 
mt19937 gen(rd()); 
uniform_int_distribution<> randAlto(1, 13);
uniform_int_distribution<> randLargo(1, 29);
uniform_int_distribution<> rand0to1(0, 1);
uniform_int_distribution<> rand1to9(1, 9);
uniform_int_distribution<> randDir(0, 3);

// Estructura del jugador
struct Jugador {
    string name;
    int vidas;
    int x;
    int y;
    int cantidad;
};

// Estructura del enemigo
struct Enemigo {
    string name;
    int vidas;
    int x;
    int y;
    bool tieneLlave;
};

// Estructura del mapa
struct Mapa {
    int alto;
    int largo;
    char** posiciones;
};

// Estructura de la bomba
struct Bomba {
    int distancia; 
    int x;
    int y;
    int autor;
};

// Estructura del muro
struct Muro {
    int x;
    int y;
    bool destructible;
};

// Estructura de la puerta
struct Puerta {
    int x;
    int y;
    bool abierta;
};

struct Juego;

// Estructura para pasar datos a los hilos
struct DataJuego {
    Juego* juego;
    volatile bool* juegoActivo;
};

// Estructura principal del juego
struct Juego {
    Mapa mapa;
    vector<Jugador> jugadores;
    vector<Enemigo> enemigos;
    vector<Bomba> bombas;
    vector<Muro> muros;
    vector<tuple<int, int, char>> powerups;
    int rangoBombas = 1;
    
    int nivel = 1;
    int nivelMaximoDesbloqueado = 1;
    int puntaje = 0;
    int tiempoRestante = 180;
    Puerta puerta = {0, 0, false};
    volatile bool juegoActivo = true;
    
    bool enModoMultijugador = false;
};

// Estructura para datos de explosi�n
struct Explosion {
    Juego* juego;
    Bomba bomba;
};
// Funcion para verificar si una posicion esta dentro de los limites del mapa
inline bool dentroMapa(const Juego& j, int x, int y) {
    return x >= 0 && x < j.mapa.largo && y >= 0 && y < j.mapa.alto;
}


// Inicializa los jugadores segun el modo
void inicializarJugadores(Juego &j, bool modoUnJugador) {
    j.jugadores.clear();
    if (modoUnJugador) {
        Jugador p1 = {"Player1", 3, 1, 1, 0};
        j.jugadores.push_back(p1);
        return;
    }
    if (!modoUnJugador) {
        Jugador p1 = {"Player1", 3, 1, 1, 0};
        Jugador p2 = {"Player2", 3, 29, 13, 0};

        j.jugadores.push_back(p1);
        j.jugadores.push_back(p2);
        return;
    }
};
// Verifica si una posici�n est� en zona de salida segura
bool zonaSalida(int x, int y) {
    if ((x >= 1 && x <= 3) && (y >= 1 && y <= 3)) {
        return true;
    }

    if ((x >= 27 && x <= 29) && (y >= 11 && y <= 13)) {
        return true;
    }

    return false;
}

// Crea el mapa con muros y espacios vac�os
void inicializarMapa(Juego &j) {
    j.mapa.alto = 15;
    j.mapa.largo = 31;
    
    j.mapa.posiciones = new char*[j.mapa.alto];
    for (int i = 0; i < j.mapa.alto; i++) {
        j.mapa.posiciones[i] = new char[j.mapa.largo];
    }

    for (int y = 0; y < j.mapa.alto; y++) {
        for (int x = 0; x < j.mapa.largo; x++) {
            // Bordes siempre son indestructibles
            if (y == 0 || y == j.mapa.alto -1 || x == 0 || x == j.mapa.largo - 1) {
                j.mapa.posiciones[y][x] = '=';
            } 
            // Patr�n de tablero: muros indestructibles en posiciones pares
            else if (y % 2 == 0 && x % 2 == 0) { 
                j.mapa.posiciones[y][x] = '=';
            } 
            // Resto: muros destructibles aleatorios o espacios vac�os
            else {
                if (zonaSalida(x, y)) {
                    j.mapa.posiciones[y][x] = ' ';
                }
                else if (rand1to9(gen) == 3) {
                    j.mapa.posiciones[y][x] = '#';
                }
                else {
                    j.mapa.posiciones[y][x] = ' ';
                }
            }
        }
    }
};
//Coloca powerups aleatoriamente
void colocarPowerups(Juego &j) {
    j.powerups.clear();
    while (j.powerups.size() < 2) {
        int x = randLargo(gen);
        int y = randAlto(gen);
        if (j.mapa.posiciones[y][x] == '#') {
            bool repetido = false;
            for (auto& p : j.powerups)
                if (get<0>(p)==x && get<1>(p)==y)
                    repetido=true;
            if (!repetido) {
                char tipo = (rand1to9(gen) %2 == 0) ? '$' : '+';
                j.powerups.push_back({x, y, tipo});
            }
        }
    }
}

// Dibuja la interfaz de usuario (HUD)
void interfaz_un_jugador (const Juego &j, bool modoUnJugador) {
    int uiy = j.mapa.alto + 1;
    if (modoUnJugador) {
        mvprintw(uiy, 0, "Nivel: %d | Puntaje: %d | Vidas: %d | Tiempo: %d seg | (E) bomba (Y) salir", 
            j.nivel, j.puntaje, j.jugadores[0].vidas, j.tiempoRestante);
        if (j.puerta.abierta) {
            mvprintw(uiy + 1, 0, "Puerta ABIERTA - Ve hasta la puerta para pasar de nivel");
        } else {
            mvprintw(uiy + 1, 0, "Puerta CERRADA - Destruye todos los enemigos E, luego mata al K para abrir puerta");
        }
    } else {
        // HUD multijugador
        mvprintw(uiy, 0, "J1 Vidas: %d  Puntaje: %d     J2 Vidas: %d  Puntaje: %d", 
            j.jugadores[0].vidas, j.jugadores[0].cantidad, 
            j.jugadores[1].vidas, j.jugadores[1].cantidad);
    }
}

// Dibuja el mapa completo en pantalla
void dibujarMapa(const Juego &j, bool modoUnJugador) {
    erase();

    // Dibuja cada celda del mapa
    for (int y = 0; y < j.mapa.alto; y++) {
        for (int x = 0; x < j.mapa.largo; x++) {
            mvaddch(y, x, (unsigned char)j.mapa.posiciones[y][x]);
        }
    }

    // Redibujar puerta encima de todo en modo un jugador
    if (!j.enModoMultijugador && j.puerta.x > 0 && j.puerta.y > 0) {
        char charPuerta = j.puerta.abierta ? '>' : 'X';
        mvaddch(j.puerta.y, j.puerta.x, (unsigned char)charPuerta);
    }

    interfaz_un_jugador(j, modoUnJugador);
    refresh();
};

// Verifica si hay bomba en una posici�n
bool hayBomba(Juego &j, int x, int y) {
    for (int i = 0; i < j.bombas.size(); i++) {
        if (j.bombas[i].x == x && j.bombas[i].y == y) {
            return true;
        }
    }

    return false;
}

// Elimina una bomba del vector
void eliminarBomba(Juego &j, Bomba bomba) {
    for (int i = 0; i < j.bombas.size(); i++) {
        if (j.bombas[i].x == bomba.x && j.bombas[i].y == bomba.y) {
            j.bombas.erase(j.bombas.begin() + i);
            return;
        }
    }
}

//muestra una pantalla de muerte cuando un jugador recibe da�o
void mostrarMuerteJugador(Juego &j, int jugador) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    refresh();
    getch();
}

void reaparecerJugador(Juego &j, int jugador) {
    int spawnX = jugador == 0 ? 1 : 29;
    int spawnY = jugador == 0 ? 1 : 13;

    j.mapa.posiciones[j.jugadores[jugador].y][j.jugadores[jugador].x] = ' ';

    j.jugadores[jugador].x = spawnX;
    j.jugadores[jugador].y = spawnY;

    j.mapa.posiciones[spawnY][spawnX] = '@';
}
//verifica si hay un enemigo en la casilla a donde se mueve el jugador
bool hayEnemigoEn(Juego &j, int x, int y) {
    for (int i = 0; i < j.enemigos.size(); i++) {
        if (j.enemigos[i].x == x && j.enemigos[i].y == y) {
            return true;
        }
    }

    return false;
}

// Mueve un jugador en la direcci�n indicada
void moverJugador(Juego &j, int jugador, int dx, int dy) {
    pthread_mutex_lock(&mutex);

    int oldX = j.jugadores[jugador].x;
    int oldY = j.jugadores[jugador].y;

    int newX = j.jugadores[jugador].x + dx;
    int newY = j.jugadores[jugador].y + dy;
    
    char celda = j.mapa.posiciones[newY][newX];
    //verifica si hay enemigo
    if (hayEnemigoEn(j, newX, newY)) {
        j.mapa.posiciones[oldY][oldX] = ' ';
        j.jugadores[jugador].vidas--;
    
        if (j.jugadores[jugador].vidas > 0) {
            reaparecerJugador(j, jugador);
            pthread_mutex_unlock(&mutex);
            timeout(-1);
            mostrarMuerteJugador(j, jugador);
            timeout(50);
            return;
        }
    
        pthread_mutex_unlock(&mutex);
        return;
    }
    // Puede moverse a espacios vac�os o a la puerta si est� abierta
    if (celda == ' ' || celda == '$' || celda == '+' ||(!j.enModoMultijugador && j.puerta.abierta && j.puerta.x == newX && j.puerta.y == newY)) {
        // Limpiar posicion anterior
        if (hayBomba(j, oldX, oldY)) {
            j.mapa.posiciones[oldY][oldX] = 'O';
        }
        else {
            j.mapa.posiciones[oldY][oldX] = ' ';
        }
        //casilla con powerup
        if (celda == '$') {
            j.rangoBombas++;
        }
        if (celda == '+') {
            j.jugadores[jugador].vidas++;
        }
        // Actualizar posicin
        j.jugadores[jugador].x = newX;
        j.jugadores[jugador].y = newY;
        j.mapa.posiciones[newY][newX] = '@';
    }

    pthread_mutex_unlock(&mutex);
};

// Marca una celda como explosi�n
void marcarExplosion(Juego &j, int x, int y) {
    if (x <= 0 || x >= j.mapa.largo - 1 || y <= 0 || y >= j.mapa.alto - 1) {
        return;
    }

    if (j.mapa.posiciones[y][x] == '=') {
        return;
    }

    j.mapa.posiciones[y][x] = '*';
}

// Calcula distancia de Manhattan entre dos puntos
int distAbsoluta(int x1,int y1,int x2,int y2){
    return abs(x1-x2) + abs(y1-y2);
}

// Verifica si una celda es libre para la puerta
bool esCeldaLibreParaPuerta(const Juego& j, int x, int y) {
    if (x <= 0 || x >= j.mapa.largo - 1 || y <= 0 || y >= j.mapa.alto - 1) return false;
    return j.mapa.posiciones[y][x] == ' ';
}

// Coloca puerta lejos del spawn del jugador
void colocarPuertaLejosDelSpawn(Juego& j, int spawnX, int spawnY) {
    const int minDist = 20;

    // Intenta hasta 2000 veces encontrar posici�n v�lida
    for (int t = 0; t < 2000; t++) {
        int x = randLargo(gen);
        int y = randAlto(gen);  

        if (!esCeldaLibreParaPuerta(j, x, y)) continue;
        if (distAbsoluta(spawnX, spawnY, x, y) < minDist) continue;

        j.puerta = {x, y, false};
        j.mapa.posiciones[y][x] = 'X';
        return;
    }
    // Si no encuentra, coloca en esquina inferior derecha
    int fx = j.mapa.largo - 2;
    int fy = j.mapa.alto - 2;
    if (j.mapa.posiciones[fy][fx] == ' ') {
        j.puerta = {fx, fy, false};
        j.mapa.posiciones[fy][fx] = 'X';
    }
}

//verifica si una pared tiene powerup
bool revelarPowerup(Juego &j, int x, int y) {
    for (size_t i = 0; i < j.powerups.size(); ++i) {
        if (get<0>(j.powerups[i]) == x && get<1>(j.powerups[i]) == y) {
            char tipo = get<2>(j.powerups[i]);
            j.mapa.posiciones[y][x] = tipo; // pone $ o +
            j.powerups.erase(j.powerups.begin() + i);
            return true;
        }
    }
    return false;
}

// Expande explosion de bomba en 4 direcciones
// Destruye muros y mata enemigos
// En un jugador: abre puerta solo si mata K cuando no hay E
void explotarBomba(Juego &j, Bomba bomba) {
    // Centro
    if (!dentroMapa(j, bomba.x, bomba.y)) return;
    marcarExplosion(j, bomba.x, bomba.y);

    auto sumarPuntosMuro = [&]() {
        if (j.enModoMultijugador) {
            if (bomba.autor >= 0 && bomba.autor < (int)j.jugadores.size())
                j.jugadores[bomba.autor].cantidad += 10;
        } else {
            j.puntaje += 10;
        }
    };

    // ---- DERECHA ----
    for (int i = 1; i <= bomba.distancia; ++i) {
        int x = bomba.x + i, y = bomba.y;
        if (!dentroMapa(j, x, y)) break;

        char c = j.mapa.posiciones[y][x];
        if (c == '=') break;

        if (c == '#') {
            if (!revelarPowerup(j, x, y)) j.mapa.posiciones[y][x] = '*';
            sumarPuntosMuro();
            break; // se detiene al romper muro
        }

        marcarExplosion(j, x, y);
    }

    // ---- IZQUIERDA ----
    for (int i = 1; i <= bomba.distancia; ++i) {
        int x = bomba.x - i, y = bomba.y;
        if (!dentroMapa(j, x, y)) break;

        char c = j.mapa.posiciones[y][x];
        if (c == '=') break;

        if (c == '#') {
            if (!revelarPowerup(j, x, y)) j.mapa.posiciones[y][x] = '*';
            sumarPuntosMuro();
            break;
        }

        marcarExplosion(j, x, y);
    }

    // ---- ABAJO ----
    for (int i = 1; i <= bomba.distancia; ++i) {
        int x = bomba.x, y = bomba.y + i;
        if (!dentroMapa(j, x, y)) break;

        char c = j.mapa.posiciones[y][x];
        if (c == '=') break;

        if (c == '#') {
            if (!revelarPowerup(j, x, y)) j.mapa.posiciones[y][x] = '*';
            sumarPuntosMuro();
            break;
        }

        marcarExplosion(j, x, y);
    }

    // ---- ARRIBA ----
    for (int i = 1; i <= bomba.distancia; ++i) {
        int x = bomba.x, y = bomba.y - i;
        if (!dentroMapa(j, x, y)) break;

        char c = j.mapa.posiciones[y][x];
        if (c == '=') break;

        if (c == '#') {
            if (!revelarPowerup(j, x, y)) j.mapa.posiciones[y][x] = '*';
            sumarPuntosMuro();
            break;
        }

        marcarExplosion(j, x, y);
    }
    // Verificar si mata enemigos
    for (int i = 0; i < j.enemigos.size(); i++) {
        int ex = j.enemigos[i].x;
        int ey = j.enemigos[i].y;
        
        if (j.mapa.posiciones[ey][ex] == '*') {
            // Si es enemigo sin llave, puede matarse en cualquier momento
            if (!j.enemigos[i].tieneLlave) {
                if (j.enModoMultijugador) {
                    if (bomba.autor < j.jugadores.size())
                        j.jugadores[bomba.autor].cantidad += 100;
                } else {
                    j.puntaje += 100;
                }
                j.enemigos.erase(j.enemigos.begin() + i);
                i--;
            }
            // Si es enemigo con llave, SOLO puede matarse si no hay otros E
            else if (j.enemigos[i].tieneLlave) {
                bool hayOtrosEnemigos = false;
                for (int k = 0; k < j.enemigos.size(); k++) {
                    if (k != i && !j.enemigos[k].tieneLlave) {
                        hayOtrosEnemigos = true;
                        break;
                    }
                }
                // Solo muere si no hay otros E
                if (!hayOtrosEnemigos) {
                    j.puerta.abierta = true;
                    if (j.enModoMultijugador) {
                        if (bomba.autor < j.jugadores.size())
                            j.jugadores[bomba.autor].cantidad += 100;
                    } else {
                        j.puntaje += 100;
                    }
                    j.enemigos.erase(j.enemigos.begin() + i);
                    i--;
                }
            }
        }
    }
}

// Limpia las marcas de explosion del mapa
void limpiarExplosion(Juego &j, Bomba bomba) {
    // Limpiar marcas de explosion
    for (int y = 0; y < j.mapa.alto; y++) {
        for (int x = 0; x < j.mapa.largo; x++) {
            if (j.mapa.posiciones[y][x] == '*') {
                j.mapa.posiciones[y][x] = ' ';
            }
        }
    }

    // Redibujar jugadores
    for (int i = 0; i < j.jugadores.size(); i++) {
        j.mapa.posiciones[j.jugadores[i].y][j.jugadores[i].x] = '@';
    }

    // Redibujar bombas
    for (int i = 0; i < j.bombas.size(); i++) {
        j.mapa.posiciones[j.bombas[i].y][j.bombas[i].x] = 'O';
    }

    // Redibujar enemigos
    for (int i = 0; i < j.enemigos.size(); i++) {
        char charEnemigo = j.enemigos[i].tieneLlave ? 'K' : 'E';
        j.mapa.posiciones[j.enemigos[i].y][j.enemigos[i].x] = charEnemigo;
    }
    
    if (bomba.autor == 0) {
        sem_post(&sem1);
    }
    else if (bomba.autor == 1) {
        sem_post(&sem2);
    }
}

// Hilo que maneja la explosion de una bomba
void* hiloBomba(void* arg) {
    Explosion* datos = (Explosion*) arg;

    sleep(3);  // Espera 3 segundos antes de explotar

    pthread_mutex_lock(&mutex);

    eliminarBomba(*datos->juego, datos->bomba);
    explotarBomba(*datos->juego, datos->bomba);

    // Quita vida si jugador esta en explosion
    for (int i = 0; i < datos->juego->jugadores.size(); ++i) {
        int px = datos->juego->jugadores[i].x;
        int py = datos->juego->jugadores[i].y;
        if (datos->juego->mapa.posiciones[py][px] == '*') {
            datos->juego->jugadores[i].vidas--;
        
            if (datos->juego->jugadores[i].vidas > 0) {
                reaparecerJugador(*datos->juego, i);
            }
        }   
    }

    pthread_mutex_unlock(&mutex);

    usleep(700000);  // Muestra explosion por 0.7 segundos

    pthread_mutex_lock(&mutex);

    limpiarExplosion(*datos->juego, datos->bomba);

    pthread_mutex_unlock(&mutex);

    delete datos;
    return NULL;
}

// Coloca una bomba en la posici�n del jugador
void colocarBomba(Juego &j, int jugador) {
    pthread_mutex_lock(&mutex);

    int x = j.jugadores[jugador].x;
    int y = j.jugadores[jugador].y;

    // Comprueba si ay una bomba en la casilla
    if (hayBomba(j, x, y)) {
        pthread_mutex_unlock(&mutex);
        return;
    }

    pthread_mutex_unlock(&mutex);

    // semaforos
    if (jugador == 0) {
        if (sem_trywait(&sem1) != 0) {
            return;
        }
    }
    else if (jugador == 1) {
        if (sem_trywait(&sem2) != 0) {
            return;
        }
    }
    
    pthread_mutex_lock(&mutex);

    Bomba bomba;
    bomba.x = j.jugadores[jugador].x;
    bomba.y = j.jugadores[jugador].y;
    bomba.distancia = j.rangoBombas;
    bomba.autor = jugador;

    j.bombas.push_back(bomba);
    j.mapa.posiciones[bomba.y][bomba.x] = 'O';

    // Crear hilo para explosion
    Explosion* datos = new Explosion;
    datos->juego = &j;
    datos->bomba = bomba;

    pthread_t hilo;
    pthread_create(&hilo, NULL, hiloBomba, datos);
    pthread_detach(hilo);

    pthread_mutex_unlock(&mutex);
}

// Limpia elementos del nivel anterior
void limpiarEstadoNivel(Juego& j) {
    j.bombas.clear();
    j.enemigos.clear();
    j.muros.clear();
    j.puerta = {0, 0, false};

    // Reiniciar vidas
    for (int i = 0; i < j.jugadores.size(); i++) {
        j.jugadores[i].vidas = 3;
    }
    // Limpiar powerUp de bomba
    j.rangoBombas = 1;
}

// Crea enemigos en posiciones random
// En multijugador: siempre 4 enemigos sin llave
// En un jugador: varios enemigos, uno con llave
void inicializarEnemigos(Juego& j, int cuantos) {
    j.enemigos.clear();
    
    // En multijugador siempre 4 enemigos
    if (j.enModoMultijugador) {
        cuantos = 4;
    }
    
    // Elegir un enemigo random para tener llave
    int enemigoConLlave = -1;
    
    if (!j.enModoMultijugador) {
        enemigoConLlave = randDir(gen) % max(1, cuantos);
    }
    
    // Crear enemigos
    for (int e = 0; e < cuantos; ++e) {
        int x, y;
        // Encontrar posicion libre
        do {
            x = randLargo(gen);
            y = randAlto(gen);
        } while(j.mapa.posiciones[y][x] != ' '); 
        
        bool tieneLlave = (e == enemigoConLlave);
        Enemigo enemy = {"Enemigo", 1, x, y, tieneLlave};
        j.enemigos.push_back(enemy);
        
        // Dibujar enemigo
        char charEnemigo = tieneLlave ? 'K' : 'E';
        j.mapa.posiciones[y][x] = charEnemigo;
    }
}

// Hilo que controla movimiento de enemigos
// Se mueven aleatoriamente cada 2 segundo
void* hiloMovimientoEnemigos(void* arg) {
    DataJuego* datos = (DataJuego*) arg;
    
    while (*(datos->juegoActivo)) {
        sleep(2);
        
        pthread_mutex_lock(&mutex);
        
        // Mover cada enemigo
        for (int i = 0; i < datos->juego->enemigos.size(); i++) {
            int dir = randDir(gen);  // Direcci�n random
            int newX = datos->juego->enemigos[i].x;
            int newY = datos->juego->enemigos[i].y;
            
            // Calcular nueva posici�n
            switch(dir) {
                case 0: newY--; break;
                case 1: newY++; break;
                case 2: newX--; break;
                case 3: newX++; break;
            }
            
            // Validar movimiento (dentro del mapa y espacio vac�o)
            if (newX > 0 && newX < datos->juego->mapa.largo - 1 && 
                newY > 0 && newY < datos->juego->mapa.alto - 1 &&
                datos->juego->mapa.posiciones[newY][newX] == ' ') {
                
                // Limpiar posici�n anterior
                datos->juego->mapa.posiciones[datos->juego->enemigos[i].y][datos->juego->enemigos[i].x] = ' ';
                
                // Actualizar posici�n
                datos->juego->enemigos[i].x = newX;
                datos->juego->enemigos[i].y = newY;
                
                // Dibujar en nueva posici�n
                char charEnemigo = datos->juego->enemigos[i].tieneLlave ? 'K' : 'E';
                datos->juego->mapa.posiciones[newY][newX] = charEnemigo;
            }
        }
        
        pthread_mutex_unlock(&mutex);
    }
    
    delete datos;
    return NULL;
}

// Hilo del cron�metro que descuenta el tiempo
void* hiloCronometro(void* arg) {
    DataJuego* datos = (DataJuego*) arg;
    
    while (*(datos->juegoActivo) && datos->juego->tiempoRestante > 0) {
        sleep(1);
        
        pthread_mutex_lock(&mutex);
        if (*(datos->juegoActivo)) {
            datos->juego->tiempoRestante--;
        }
        pthread_mutex_unlock(&mutex);
    }
    
    delete datos;
    return NULL;
}

// Hilo que controla ataques de enemigos
// Quita vida si estan en misma posici�n que jugador
void* hiloAtaqueEnemigos(void* arg) {
    DataJuego* datos = (DataJuego*) arg;
    
    while (*(datos->juegoActivo)) {
        sleep(2);  // Atacan cada 2 segundos
        
        pthread_mutex_lock(&mutex);
        
        // Verificar colisi�n enemigo-jugador
        for (int i = 0; i < datos->juego->enemigos.size(); i++) {
            for (int j = 0; j < datos->juego->jugadores.size(); j++) {
                if (datos->juego->enemigos[i].x == datos->juego->jugadores[j].x &&
                    datos->juego->enemigos[i].y == datos->juego->jugadores[j].y) {
                    datos->juego->jugadores[j].vidas--;
                }
            }
        }
        
        pthread_mutex_unlock(&mutex);
    }
    
    delete datos;
    return NULL;
}

// Verifica si un jugador esta muerto
bool jugadorMuerto(const Jugador& jugador) {
    return jugador.vidas <= 0;
}

// Pantalla de game over en modo un jugador
void mostrarGameOverUnJugador(const Juego& j) {
    erase();
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    mvprintw(max_y/2 - 3, max_x/2 - 5, "GAME OVER");
    mvprintw(max_y/2 - 1, max_x/2 - 12, "Tu puntaje final: %d", j.puntaje);
    mvprintw(max_y/2 + 2, max_x/2 - 22, "Presiona cualquier tecla para volver al menu...");
    refresh();
    getch();
}

// Pantalla de fin de juego en modo dos jugadores
void mostrarGanadorMultijugador(const Juego& j) {
    erase();
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    mvprintw(max_y/2 - 5, max_x/2 - 8, "FIN DEL JUEGO");
    mvprintw(max_y/2 - 2, max_x/2 - 15, "========================");
    
    // Determinar ganador por vidas
    if (j.jugadores[0].vidas > j.jugadores[1].vidas) {
        mvprintw(max_y/2, max_x/2 - 15, "GANADOR: Player 1");
        mvprintw(max_y/2 + 2, max_x/2 - 15, "Puntaje: %d", j.jugadores[0].cantidad);
        mvprintw(max_y/2 + 4, max_x/2 - 15, "========================");
        mvprintw(max_y/2 + 6, max_x/2 - 15, "PERDEDOR: Player 2");
        mvprintw(max_y/2 + 8, max_x/2 - 15, "Puntaje: %d", j.jugadores[1].cantidad);
    } else if (j.jugadores[1].vidas > j.jugadores[0].vidas) {
        mvprintw(max_y/2, max_x/2 - 15, "GANADOR: Player 2");
        mvprintw(max_y/2 + 2, max_x/2 - 15, "Puntaje: %d", j.jugadores[1].cantidad);
        mvprintw(max_y/2 + 4, max_x/2 - 15, "========================");
        mvprintw(max_y/2 + 6, max_x/2 - 15, "PERDEDOR: Player 1");
        mvprintw(max_y/2 + 8, max_x/2 - 15, "Puntaje: %d", j.jugadores[0].cantidad);
    } else {
        // Empate
        mvprintw(max_y/2, max_x/2 - 15, "EMPATE");
        mvprintw(max_y/2 + 2, max_x/2 - 15, "Player 1 - Puntaje: %d", j.jugadores[0].cantidad);
        mvprintw(max_y/2 + 4, max_x/2 - 15, "Player 2 - Puntaje: %d", j.jugadores[1].cantidad);
    }
    
    mvprintw(max_y - 2, max_x/2 - 22, "Presiona cualquier tecla para volver al menu...");
    refresh();
    getch();
}


// Menu para seleccionar nivel desde el menu principal
// Muestra TODOS los niveles desbloqueados
int mostrarMenuSeleccionarNivel(const Juego& j) {
    int seleccion = 0;

    while (true) {
        erase();
        int max_y, max_x;
        getmaxyx(stdscr, max_y, max_x);
        mvprintw(max_y/2 - 8, max_x/2 - 15, "SELECCIONAR NIVEL"); // in bold
        mvprintw(max_y/2  - 4, max_x/2 - 20, "Nivel maximo desbloqueado: %d", j.nivelMaximoDesbloqueado);
        mvprintw(max_y/2 - 1, max_x/2 - 23, "Usa flechas y Enter ('Y' para salir)");
        mvprintw(max_y/2 , max_x/2 - 15, "Selecciona un nivel:");

        for (int i = 0; i < 5; i++) {
            bool desbloqueado = (i < j.nivelMaximoDesbloqueado);

            if (i == seleccion) {
                attron(A_REVERSE);
            }
            if (!desbloqueado) {
                attron(A_DIM);

            }

            if (desbloqueado) {
                mvprintw(max_y/2 + 4 + i, max_x/2 - 15, "Nivel %d", i + 1);
            } else {
                mvprintw(max_y/2 + 4 + i, max_x/2 - 15, "Nivel %d (BLOQUEADO)", i + 1);
            }

            if (!desbloqueado) {
                attroff(A_DIM);
            }
            if (i == seleccion) {
                attroff(A_REVERSE);
            }
        }

        refresh();
        int input = getch();

        if (input == KEY_UP && seleccion > 0) {
            seleccion--;
        } else if (input == KEY_DOWN && seleccion < 4) {
            seleccion++;
        } else if (input == 10) {  // Enter
            if (seleccion < j.nivelMaximoDesbloqueado) {
                return seleccion + 1;
            }
            beep();
        } else if (input == 'y' || input == 'Y') {
            return -1;
        }
    }
}
// Pantalla cuando gana un nivel
// Muestra todos los niveles desbloqueados disponibles para jugar
int mostrarVictoriaUnJugador(Juego& j) {
    erase();
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    mvprintw(max_y/2 - 7, max_x/2 - 8, "GANASTE");
    mvprintw(max_y/2 - 5, max_x/2 - 15, "Puntaje acumulado: %d", j.puntaje);
    mvprintw(max_y/2 - 3, max_x/2 - 19, "Tiempo restante: %d segundos", j.tiempoRestante);
    mvprintw(max_y/2 - 1, max_x/2 - 27, "Presiona cualquier tecla para seleccionar nivel...");
    refresh();
    getch();
    return mostrarMenuSeleccionarNivel(j);
}

// Funcion principal
int main() {
    Juego bomberman;
    
    // Inicializar estado global
    inicializarJugadores(bomberman, false);
    bomberman.puntaje = 0;
    bomberman.nivel = 1;
    bomberman.nivelMaximoDesbloqueado = 1;  // Solo nivel 1 al inicio
    bomberman.tiempoRestante = 180;
    bomberman.enModoMultijugador = false;
    
    // Inicializar mutex
    pthread_mutex_init(&mutex, NULL);
    
    //Inicializar semaforo
    sem_init(&sem1, 0, 3);
    sem_init(&sem2, 0, 3);
    
    // Inicializar ncurses
    initscr();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);

    const char* titulo = R"( 
 /$$$$$$$                          /$$                                                            
| $$__  $$                        | $$                                                            
| $$  \ $$  /$$$$$$  /$$$$$$/$$$$ | $$$$$$$   /$$$$$$   /$$$$$$  /$$$$$$/$$$$   /$$$$$$  /$$$$$$$ 
| $$$$$$$  /$$__  $$| $$_  $$_  $$| $$__  $$ /$$__  $$ /$$__  $$| $$_  $$_  $$ |____  $$| $$__  $$
| $$__  $$| $$  \ $$| $$ \ $$ \ $$| $$  \ $$| $$$$$$$$| $$  \__/| $$ \ $$ \ $$  /$$$$$$$| $$  \ $$
| $$  \ $$| $$  | $$| $$ | $$ | $$| $$  | $$| $$_____/| $$      | $$ | $$ | $$ /$$__  $$| $$  | $$
| $$$$$$$/|  $$$$$$/| $$ | $$ | $$| $$$$$$$/|  $$$$$$$| $$      | $$ | $$ | $$|  $$$$$$$| $$  | $$
|_______/  \______/ |__/ |__/ |__/|_______/  \_______/|__/      |__/ |__/ |__/ \_______/|__/  |__/
                                                                                                  
                                                                                                  
                                                                                                  )";
    
    vector<string> opciones = {"Un jugador", "Dos jugadores", "Seleccionar Nivel", "Controles", "Reglas", "Puntajes", "Salir"};
    int input;
    int seleccion = 0;
    bool menu = true;

    // Bucle principal del menu
    while(menu){
    erase();
    mvprintw(0, 12, "%s", titulo);
    
    // Dibujar opciones del menu
    for(int i=13; i<13+opciones.size(); i++){
        if (i == seleccion + 13) {
            attron(A_REVERSE);  // Resaltar opcion seleccionada
            mvprintw(i, 40, "%s", opciones[i - 13].c_str());
            attroff(A_REVERSE);
        } else {
            mvprintw(i, 40, "%s", opciones[i - 13].c_str());
        }
    }
    refresh();
    
    // Obtener entrada del usuario
    input = getch();
    if (input == KEY_UP && seleccion > 0) {
        seleccion--;  // Mover arriba
    } else if (input == KEY_DOWN && seleccion < opciones.size() - 1) {
        seleccion++;  // Mover abajo
    } else if (input == 10) {  // Enter presionado

        bool jugando = true;
        
        // Opci�n: Un jugador
        if (opciones[seleccion] == "Un jugador") {
            limpiarEstadoNivel(bomberman);
            bomberman.enModoMultijugador = false;
            
            inicializarMapa(bomberman);
            colocarPowerups(bomberman);
            inicializarJugadores(bomberman, true);
            inicializarEnemigos(bomberman, bomberman.nivel);
            bomberman.mapa.posiciones[bomberman.jugadores[0].y][bomberman.jugadores[0].x] = '@';
            colocarPuertaLejosDelSpawn(bomberman, bomberman.jugadores[0].x, bomberman.jugadores[0].y);
            
            bomberman.juegoActivo = true;
            
            pthread_t hiloEnems, hiloCrono, hiloAtaques;
            
            // Hilo movimiento enemigos
            DataJuego* datosEnems = new DataJuego;
            datosEnems->juego = &bomberman;
            datosEnems->juegoActivo = &bomberman.juegoActivo;
            pthread_create(&hiloEnems, NULL, hiloMovimientoEnemigos, datosEnems);
            pthread_detach(hiloEnems);
            
            // Hilo cronometro
            DataJuego* datosCrono = new DataJuego;
            datosCrono->juego = &bomberman;
            datosCrono->juegoActivo = &bomberman.juegoActivo;
            pthread_create(&hiloCrono, NULL, hiloCronometro, datosCrono);
            pthread_detach(hiloCrono);
            
            // Hilo ataques enemigos
            DataJuego* datosAtaques = new DataJuego;
            datosAtaques->juego = &bomberman;
            datosAtaques->juegoActivo = &bomberman.juegoActivo;
            pthread_create(&hiloAtaques, NULL, hiloAtaqueEnemigos, datosAtaques);
            pthread_detach(hiloAtaques);

            timeout(50);
            bool nivelActivo = true;
            
            // Bucle de juego
            while(jugando && nivelActivo) {
                dibujarMapa(bomberman, true);

                int ch = getch();
                switch(ch) {
                    // Movimiento
                    case 'w': case 'W': moverJugador(bomberman, 0, 0, -1); break;
                    case 's': case 'S': moverJugador(bomberman, 0, 0, 1); break;
                    case 'a': case 'A': moverJugador(bomberman, 0, -1, 0); break;
                    case 'd': case 'D': moverJugador(bomberman, 0, 1, 0); break;
                    // Bomba
                    case 'e': case 'E': colocarBomba(bomberman, 0); break;
                    // Salir
                    case 'y': case 'Y': 
                        jugando = false;
                        nivelActivo = false;
                        bomberman.juegoActivo = false;
                        break;
                }

                // Verificar si llego a la puerta
                if (bomberman.puerta.abierta &&
                    bomberman.jugadores[0].x == bomberman.puerta.x &&
                    bomberman.jugadores[0].y == bomberman.puerta.y) {
                    
                    timeout(-1);
                    bomberman.juegoActivo = false;
                    sleep(1);
                    
                    // Acumular puntaje
                    bomberman.puntaje += (bomberman.tiempoRestante * 10) + 50;
                    
                    // Desbloquear siguiente nivel si es el actual
                    if (bomberman.nivel == bomberman.nivelMaximoDesbloqueado) {
                        bomberman.nivelMaximoDesbloqueado = bomberman.nivel + 1;
                        if (bomberman.nivelMaximoDesbloqueado > 5) {
                            bomberman.nivelMaximoDesbloqueado = 5;
                        }
                    }
                    
                    // Mostrar pantalla de victoria
                    int resultado = mostrarVictoriaUnJugador(bomberman);
                    
                    // Si seleccion� un nivel
                    if (resultado >= 1 && resultado <= 5) {
                        bomberman.nivel = resultado;
                        bomberman.tiempoRestante = 180;
                        
                        limpiarEstadoNivel(bomberman);
                        inicializarMapa(bomberman);
                        bomberman.jugadores[0].x = 1;
                        bomberman.jugadores[0].y = 1;
                        bomberman.mapa.posiciones[1][1] = '@';
                        inicializarEnemigos(bomberman, bomberman.nivel);
                        colocarPuertaLejosDelSpawn(bomberman, 1, 1);
                        
                        bomberman.juegoActivo = true;
                        
                        // Crear nuevos hilos
                        DataJuego* datosEnems2 = new DataJuego;
                        datosEnems2->juego = &bomberman;
                        datosEnems2->juegoActivo = &bomberman.juegoActivo;
                        pthread_create(&hiloEnems, NULL, hiloMovimientoEnemigos, datosEnems2);
                        pthread_detach(hiloEnems);
                        
                        DataJuego* datosCrono2 = new DataJuego;
                        datosCrono2->juego = &bomberman;
                        datosCrono2->juegoActivo = &bomberman.juegoActivo;
                        pthread_create(&hiloCrono, NULL, hiloCronometro, datosCrono2);
                        pthread_detach(hiloCrono);
                        
                        DataJuego* datosAtaques2 = new DataJuego;
                        datosAtaques2->juego = &bomberman;
                        datosAtaques2->juegoActivo = &bomberman.juegoActivo;
                        pthread_create(&hiloAtaques, NULL, hiloAtaqueEnemigos, datosAtaques2);
                        pthread_detach(hiloAtaques);
                        
                        timeout(50);
                    } 
                    // Si selecciono salir
                    else if (resultado == 6) {
                        jugando = false;
                        nivelActivo = false;
                    }
                }

                // Verificar game over
                if(jugadorMuerto(bomberman.jugadores[0]) || bomberman.tiempoRestante <= 0) {
                    timeout(-1);
                    bomberman.juegoActivo = false;
                    sleep(1);
                    mostrarGameOverUnJugador(bomberman);
                    jugando = false;
                    nivelActivo = false;
                }
            }
        } 
        
        // Opcion: Dos jugadores
        else if (opciones[seleccion] == "Dos jugadores") {
            bomberman.enModoMultijugador = true;
            
            bomberman.jugadores.clear();
            inicializarJugadores(bomberman, false);
            bomberman.jugadores[0].cantidad = 0;
            bomberman.jugadores[1].cantidad = 0;
            
            inicializarMapa(bomberman);
            colocarPowerups(bomberman);
            bomberman.mapa.posiciones[bomberman.jugadores[0].y][bomberman.jugadores[0].x] = '@';
            bomberman.mapa.posiciones[bomberman.jugadores[1].y][bomberman.jugadores[1].x] = '@';
            
            inicializarEnemigos(bomberman, 0);
            
            bomberman.juegoActivo = true;
            pthread_t hiloEnems, hiloAtaques;
            
            DataJuego* datosEnems = new DataJuego;
            datosEnems->juego = &bomberman;
            datosEnems->juegoActivo = &bomberman.juegoActivo;
            pthread_create(&hiloEnems, NULL, hiloMovimientoEnemigos, datosEnems);
            pthread_detach(hiloEnems);
            
            DataJuego* datosAtaques = new DataJuego;
            datosAtaques->juego = &bomberman;
            datosAtaques->juegoActivo = &bomberman.juegoActivo;
            pthread_create(&hiloAtaques, NULL, hiloAtaqueEnemigos, datosAtaques);
            pthread_detach(hiloAtaques);

            timeout(50);
            
            // Bucle de juego multijugador
            while(jugando) {
                dibujarMapa(bomberman, false);

                int ch = getch();
                switch(ch) {
                    // Player 1: WASD
                    case 'w': case 'W': moverJugador(bomberman, 0, 0, -1); break;
                    case 's': case 'S': moverJugador(bomberman, 0, 0, 1); break;
                    case 'a': case 'A': moverJugador(bomberman, 0, -1, 0); break;
                    case 'd': case 'D': moverJugador(bomberman, 0, 1, 0); break;
                    case 'e': case 'E': colocarBomba(bomberman, 0); break;

                    // Player 2: IJKL
                    case 'i': case 'I': moverJugador(bomberman, 1, 0, -1); break;
                    case 'k': case 'K': moverJugador(bomberman, 1, 0, 1); break;
                    case 'j': case 'J': moverJugador(bomberman, 1, -1, 0); break;
                    case 'l': case 'L': moverJugador(bomberman, 1, 1, 0); break;
                    case 'o': case 'O': colocarBomba(bomberman, 1); break;

                    // Salir
                    case 'y': case 'Y': jugando = false; break;
                }
                
                // Verificar si alguien muri�
                if (jugadorMuerto(bomberman.jugadores[0]) || jugadorMuerto(bomberman.jugadores[1])) {
                    timeout(-1);
                    bomberman.juegoActivo = false;
                    sleep(1);
                    mostrarGanadorMultijugador(bomberman);
                    jugando = false;
                }
            }
        }
        
        // Opcion: Seleccionar Nivel
        else if (opciones[seleccion] == "Seleccionar Nivel") {
            int nivelSeleccionado = mostrarMenuSeleccionarNivel(bomberman);
            if (nivelSeleccionado != -1) {
                limpiarEstadoNivel(bomberman);
                bomberman.enModoMultijugador = false;
                bomberman.nivel = nivelSeleccionado;
                bomberman.tiempoRestante = 180;
                
                inicializarMapa(bomberman);
                colocarPowerups(bomberman);
                inicializarJugadores(bomberman, true);
                inicializarEnemigos(bomberman, bomberman.nivel);
                bomberman.mapa.posiciones[bomberman.jugadores[0].y][bomberman.jugadores[0].x] = '@';
                colocarPuertaLejosDelSpawn(bomberman, bomberman.jugadores[0].x, bomberman.jugadores[0].y);
                
                bomberman.juegoActivo = true;
                
                pthread_t hiloEnems, hiloCrono, hiloAtaques;
                
                DataJuego* datosEnems = new DataJuego;
                datosEnems->juego = &bomberman;
                datosEnems->juegoActivo = &bomberman.juegoActivo;
                pthread_create(&hiloEnems, NULL, hiloMovimientoEnemigos, datosEnems);
                pthread_detach(hiloEnems);
                
                DataJuego* datosCrono = new DataJuego;
                datosCrono->juego = &bomberman;
                datosCrono->juegoActivo = &bomberman.juegoActivo;
                pthread_create(&hiloCrono, NULL, hiloCronometro, datosCrono);
                pthread_detach(hiloCrono);
                
                DataJuego* datosAtaques = new DataJuego;
                datosAtaques->juego = &bomberman;
                datosAtaques->juegoActivo = &bomberman.juegoActivo;
                pthread_create(&hiloAtaques, NULL, hiloAtaqueEnemigos, datosAtaques);
                pthread_detach(hiloAtaques);

                timeout(50);
                bool nivelActivo = true;
                bool jugandoNivel = true;
                
                // Bucle del nivel seleccionado
                while(jugandoNivel && nivelActivo) {
                    dibujarMapa(bomberman, true);

                    int ch = getch();
                    switch(ch) {
                        case 'w': case 'W': moverJugador(bomberman, 0, 0, -1); break;
                        case 's': case 'S': moverJugador(bomberman, 0, 0, 1); break;
                        case 'a': case 'A': moverJugador(bomberman, 0, -1, 0); break;
                        case 'd': case 'D': moverJugador(bomberman, 0, 1, 0); break;
                        case 'e': case 'E': colocarBomba(bomberman, 0); break;
                        case 'y': case 'Y': 
                            jugandoNivel = false;
                            nivelActivo = false;
                            bomberman.juegoActivo = false;
                            break;
                    }

                    // Verificar si lleg� a puerta
                    if (bomberman.puerta.abierta &&
                        bomberman.jugadores[0].x == bomberman.puerta.x &&
                        bomberman.jugadores[0].y == bomberman.puerta.y) {
                        
                        timeout(-1);
                        bomberman.juegoActivo = false;
                        sleep(1);
                        
                        bomberman.puntaje += (bomberman.tiempoRestante * 10) + 50;
                        
                        if (bomberman.nivel == bomberman.nivelMaximoDesbloqueado) {
                            bomberman.nivelMaximoDesbloqueado = bomberman.nivel + 1;
                            if (bomberman.nivelMaximoDesbloqueado > 5) {
                                bomberman.nivelMaximoDesbloqueado = 5;
                            }
                        }
                        
                        int resultado = mostrarVictoriaUnJugador(bomberman);
                        
                        if (resultado >= 1 && resultado <= 5) {
                            bomberman.nivel = resultado;
                            bomberman.tiempoRestante = 180;
                            
                            limpiarEstadoNivel(bomberman);
                            inicializarMapa(bomberman);
                            bomberman.jugadores[0].x = 1;
                            bomberman.jugadores[0].y = 1;
                            bomberman.mapa.posiciones[1][1] = '@';
                            inicializarEnemigos(bomberman, bomberman.nivel);
                            colocarPuertaLejosDelSpawn(bomberman, 1, 1);
                            
                            bomberman.juegoActivo = true;
                            
                            DataJuego* datosEnems2 = new DataJuego;
                            datosEnems2->juego = &bomberman;
                            datosEnems2->juegoActivo = &bomberman.juegoActivo;
                            pthread_create(&hiloEnems, NULL, hiloMovimientoEnemigos, datosEnems2);
                            pthread_detach(hiloEnems);
                            
                            DataJuego* datosCrono2 = new DataJuego;
                            datosCrono2->juego = &bomberman;
                            datosCrono2->juegoActivo = &bomberman.juegoActivo;
                            pthread_create(&hiloCrono, NULL, hiloCronometro, datosCrono2);
                            pthread_detach(hiloCrono);
                            
                            DataJuego* datosAtaques2 = new DataJuego;
                            datosAtaques2->juego = &bomberman;
                            datosAtaques2->juegoActivo = &bomberman.juegoActivo;
                            pthread_create(&hiloAtaques, NULL, hiloAtaqueEnemigos, datosAtaques2);
                            pthread_detach(hiloAtaques);
                            
                            timeout(50);
                        } else if (resultado == 6) {
                            jugandoNivel = false;
                            nivelActivo = false;
                        }
                    }

                    // Verificar game over
                    if(jugadorMuerto(bomberman.jugadores[0]) || bomberman.tiempoRestante <= 0) {
                        timeout(-1);
                        bomberman.juegoActivo = false;
                        sleep(1);
                        mostrarGameOverUnJugador(bomberman);
                        jugandoNivel = false;
                        nivelActivo = false;
                    }
                }
            }
        }
        
        // Opcion: Controles
        else if (opciones[seleccion] == "Controles") {
            timeout(-1);
            clear();
            mvprintw(5, 10, "Controles:");
            mvprintw(7, 12, "Jugador 1: W (arriba), A (izquierda), S (abajo), D (derecha), E (colocar bomba)");
            mvprintw(9, 12, "Jugador 2: I (arriba), J (izquierda), K (abajo), L (derecha), O (colocar bomba)");
            mvprintw(11, 12, "Salir del juego: Presiona 'Y' durante el juego");
            mvprintw(13, 12, "Ve hasta la puerta abierta para continuar al siguiente nivel (modo un jugador)");
            mvprintw(15, 12, "Presiona cualquier tecla para volver al menu...");
            refresh();
            getch();
            timeout(50);
        }
        
        // Opcion: Reglas
        else if (opciones[seleccion] == "Reglas") {
            timeout(-1);
            clear();
            int max_y, max_x;
            getmaxyx(stdscr, max_y, max_x);
            mvprintw(2, max_x/2 - 10, "REGLAS DEL JUEGO");
            mvprintw(4, 5, "MODO UN JUGADOR:");
            mvprintw(6, 10, "Destruir muros (#): 10 puntos");
            mvprintw(7, 10, "Eliminar enemigos (E): 100 puntos");
            mvprintw(8, 10, "Enemigo K (con llave): SOLO puede morir si NO hay enemigos E");
            mvprintw(9, 10, "Cuando K muere sin E disponibles -> ABRE LA PUERTA");
            mvprintw(10, 10, "Pasar nivel: 50 puntos mas 10 puntos por segundo restante");
            mvprintw(11, 10, "Tiempo limite: 3 minutos por nivel");
            mvprintw(12, 10, "Vidas: 3 (se pierden al tocar explosion o enemigo)");
            
            mvprintw(14, 5, "MODO DOS JUGADORES:");
            mvprintw(16, 10, "Siempre aparecen 4 enemigos");
            mvprintw(17, 10, "Los enemigos se mueven automaticamente");
            mvprintw(18, 10, "No hay puerta ni limite de tiempo");
            mvprintw(19, 10, "Gana quien tenga mas vidas");
            mvprintw(20, 10, "Destruir muros (#): 10 puntos");
            mvprintw(21, 10, "Eliminar enemigos (E): 100 puntos");
            
            mvprintw(23, 5, "MECANICAS GENERALES:");
            mvprintw(25, 10, "Enemigos se mueven cada 1 segundo");
            mvprintw(26, 10, "Enemigos atacan cada 2 segundos si estan en tu posicion");
            mvprintw(27, 10, "Bombas explotan despues de 3 segundos");
            
            mvprintw(max_y - 2, max_x/2 - 20, "Presiona cualquier tecla para volver al menu...");
            refresh();
            getch();
            timeout(50);
        }
        
        // Opci�n: Puntajes
        else if (opciones[seleccion] == "Puntajes") {
            timeout(-1);
            clear();
            int max_y, max_x;
            getmaxyx(stdscr, max_y, max_x);
            
            mvprintw(max_y/2 - 8, max_x/2 - 10, "PUNTAJES");
            
            mvprintw(max_y/2 - 5, max_x/2 - 20, "MODO UN JUGADOR:");
            mvprintw(max_y/2 - 3, max_x/2 - 20, "Puntaje acumulado: %d", bomberman.puntaje);
            mvprintw(max_y/2 - 1, max_x/2 - 20, "Nivel actual: %d", bomberman.nivel);
            mvprintw(max_y/2 + 1, max_x/2 - 20, "Nivel maximo desbloqueado: %d", bomberman.nivelMaximoDesbloqueado);
            
            mvprintw(max_y/2 + 4, max_x/2 - 20, "MODO MULTIJUGADOR:");
            if (bomberman.enModoMultijugador) {
                mvprintw(max_y/2 + 6, max_x/2 - 20, "Player 1 - Puntaje: %d", bomberman.jugadores[0].cantidad);
                mvprintw(max_y/2 + 8, max_x/2 - 20, "Player 2 - Puntaje: %d", bomberman.jugadores[1].cantidad);
            } else {
                mvprintw(max_y/2 + 6, max_x/2 - 20, "Player 1 - Puntaje: 0");
                mvprintw(max_y/2 + 8, max_x/2 - 20, "Player 2 - Puntaje: 0");
            }
            
            mvprintw(max_y - 2, max_x/2 - 22, "Presiona cualquier tecla para volver al menu...");
            refresh();
            getch();
            timeout(50);
        }
        
        // Opci�n: Salir
        else if (opciones[seleccion] == "Salir") {
            jugando = false;
            menu = false;
        }
    }
    }
    
    // Limpiar ncurses
    endwin();
    
    // Destruir mutex
    pthread_mutex_destroy(&mutex);
    
    //Destruir semaforo
    sem_destroy(&sem1);
    sem_destroy(&sem2);
    
    return 0;
};
