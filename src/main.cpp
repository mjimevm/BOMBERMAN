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
#include <locale.h>
#include <cwchar>
#include <fstream>
#include <sstream>
#include <string>
#include <limits>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>

using namespace std;
using namespace std::chrono;

// Mutex para sincronizacion entre hilos
pthread_mutex_t mutex;

//Semaforo
sem_t sem1;
sem_t sem2;

// Generadores de numeros aleatorios
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
    int runID = 0;
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
    int runID = 0;
};

// Estructura para datos de explosion
struct Explosion {
    Juego* juego;
    Bomba bomba;
};

struct Puntajes {
    string fecha;
    string nombre;
    int puntaje;
};
// Funcion para verificar si una posicion esta dentro de los limites del mapa
inline bool dentroMapa(const Juego& j, int x, int y) {
    return x >= 0 && x < j.mapa.largo && y >= 0 && y < j.mapa.alto;
}



// Verifica si una posicion esta en zona de salida segura
bool zonaSalida(int x, int y) {
    if ((x >= 1 && x <= 3) && (y >= 1 && y <= 3)) {
        return true;
    }

    if ((x >= 27 && x <= 29) && (y >= 11 && y <= 13)) {
        return true;
    }

    return false;
}

// Crea el mapa con muros y espacios vacios
void inicializarMapa(Juego &j) {
    j.mapa.alto = 15;
    j.mapa.largo = 31;
    
    int probMuro = min(2 + j.nivel, 7);

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
            // Patron de tablero: muros indestructibles en posiciones pares
            else if (y % 2 == 0 && x % 2 == 0) { 
                j.mapa.posiciones[y][x] = '=';
            } 
            // Resto: muros destructibles aleatorios o espacios vacios
            else {
                if (zonaSalida(x, y)) {
                    j.mapa.posiciones[y][x] = ' ';
                }
                else if (rand1to9(gen) <= probMuro) {
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

// Dibuja un panel para la informacion del jugador
void dibujarRanking(int inicioY);

// Prototipo
int obtenerOffsetMapa(const Juego& j);

// Se mantiene para compatibilidad
void interfaz_un_jugador(const Juego &j, bool modoUnJugador) {
}

void dibujarPanelHUD(const Juego& j, bool modoUnJugador) {

    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    int panelY = j.mapa.alto + 2;

    int panelAncho = 110;
    int panelAlto  = 5;

    int panelX = (max_x - panelAncho) / 2;

    attron(COLOR_PAIR(6));

    mvaddwstr(panelY, panelX, L"╔");

    for(int i = 1; i < panelAncho - 1; i++)
        mvaddwstr(panelY, panelX + i, L"═");

    mvaddwstr(panelY, panelX + panelAncho - 1, L"╗");

    for(int i = 1; i < panelAlto - 1; i++) {

        mvaddwstr(panelY + i, panelX, L"║");

        for(int j = 1; j < panelAncho - 1; j++)
            mvaddch(panelY + i, panelX + j, ' ');

        mvaddwstr(panelY + i, panelX + panelAncho - 1, L"║");
    }

    mvaddwstr(panelY + panelAlto - 1, panelX, L"╚");

    for(int i = 1; i < panelAncho - 1; i++)
        mvaddwstr(panelY + panelAlto - 1, panelX + i, L"═");

    mvaddwstr(panelY + panelAlto - 1,
              panelX + panelAncho - 1,
              L"╝");

    attroff(COLOR_PAIR(6));

if(modoUnJugador) {
        
        // Separadores estructurales '|' posicionados simétricamente
        attron(COLOR_PAIR(7));
        mvprintw(panelY + 1, panelX + 14, "|");
        mvprintw(panelY + 1, panelX + 33, "|");
        mvprintw(panelY + 1, panelX + 47, "|");
        mvprintw(panelY + 1, panelX + 67, "|");
        attroff(COLOR_PAIR(7));

        // Nivel (Cian) - Columna 1
        attron(COLOR_PAIR(6) | A_BOLD);
        mvprintw(panelY + 1, panelX + 4, "Nivel: %d", j.nivel);
        attroff(COLOR_PAIR(6) | A_BOLD);

        // Puntaje (Amarillo) - Columna 2
        attron(COLOR_PAIR(2) | A_BOLD);
        mvprintw(panelY + 1, panelX + 17, "Puntaje: %d", j.puntaje);
        attroff(COLOR_PAIR(2) | A_BOLD);

        // Vidas (Verde) - Columna 3
        attron(COLOR_PAIR(3) | A_BOLD);
        mvprintw(panelY + 1, panelX + 36, "Vidas: %d", j.jugadores[0].vidas);
        attroff(COLOR_PAIR(3) | A_BOLD);

        // Tiempo (Magenta) - Columna 4
        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(panelY + 1, panelX + 50, "Tiempo: %d seg", j.tiempoRestante);
        attroff(COLOR_PAIR(5) | A_BOLD);

        // Acciones (Blanco) - Columna 5
        attron(COLOR_PAIR(7));
        mvprintw(panelY + 1, panelX + 70, "(E) bomba  (Y) salir");
        attroff(COLOR_PAIR(7));

        char linea2[128];
        if(j.puerta.abierta) {
            snprintf(linea2, sizeof(linea2), "Puerta ABIERTA - Ve hasta la puerta para pasar de nivel");
            int x2 = panelX + (panelAncho - (int)strlen(linea2)) / 2;
            attron(COLOR_PAIR(3) | A_BOLD);
            mvprintw(panelY + 2, x2, "%s", linea2);
            attroff(COLOR_PAIR(3) | A_BOLD);
        } else {
            snprintf(linea2, sizeof(linea2), "Puerta CERRADA - Destruye todos los enemigos, luego mata al jefe para abrir puerta");
            int x2 = panelX + (panelAncho - (int)strlen(linea2)) / 2;
            attron(COLOR_PAIR(6) | A_BOLD); 
            mvprintw(panelY + 2, x2, "%s", linea2);
            attroff(COLOR_PAIR(6) | A_BOLD);
        }

        const char* linea3 = "Usa (WASD) para moverte y (E) para colocar bomba";
        int x3 = panelX + (panelAncho - (int)strlen(linea3)) / 2;
        attron(COLOR_PAIR(7));
        mvprintw(panelY + 3, x3, "%s", linea3);
        attroff(COLOR_PAIR(7));
    }
    else {

        // Línea 1: Estadísticas de Jugador 1 (Verde)
        char lp1[128];
        snprintf(lp1, sizeof(lp1), "JUGADOR 1 (Verde)    |   Vidas: %d   |   Controles: [WASD] + [E] Bomba", j.jugadores[0].vidas);
        int xp1 = panelX + (panelAncho - (int)strlen(lp1)) / 2;
        attron(COLOR_PAIR(3) | A_BOLD);
        mvprintw(panelY + 1, xp1, "%s", lp1);
        attroff(COLOR_PAIR(3) | A_BOLD);

        // Línea 2: Estadísticas de Jugador 2 (Amarillo)
        char lp2[128];
        snprintf(lp2, sizeof(lp2), "JUGADOR 2 (Amarillo) |   Vidas: %d   |   Controles: [J,I,K,L] + [o] Bomba", j.jugadores[1].vidas);
        int xp2 = panelX + (panelAncho - (int)strlen(lp2)) / 2;
        attron(COLOR_PAIR(2) | A_BOLD);
        mvprintw(panelY + 2, xp2, "%s", lp2);
        attroff(COLOR_PAIR(2) | A_BOLD);

        // Línea 3: Opción de salida global (Blanco)
        const char* lsalir = "Presiona (Y) en cualquier momento para salir al Menú Principal";
        int xsalir = panelX + (panelAncho - (int)strlen(lsalir)) / 2;
        attron(COLOR_PAIR(7));
        mvprintw(panelY + 3, xsalir, "%s", lsalir);
        attroff(COLOR_PAIR(7));
    }
}


// Dibuja el mapa completo en pantalla
void dibujarMapa(const Juego &j, bool modoUnJugador) {
    erase();

    int offsetX = obtenerOffsetMapa(j);

    bool parpadeo = (duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count() / 250) % 2;
    
    // Dibuja cada celda del mapa cambiando a los simbolos con mejor dise;o
    for (int y = 0; y < j.mapa.alto; y++) {
        for (int x = 0; x < j.mapa.largo; x++) {
            char c = j.mapa.posiciones[y][x];
            
            switch(c) {
                case '=':
                    attron(COLOR_PAIR(1));
                    if (y == 0 && x == 0) {
                        mvaddwstr(y, x + offsetX, L"╔"); // esquina superior izquierda
                    } else if (y == 0 && x == j.mapa.largo - 1) {
                        mvaddwstr(y, x + offsetX, L"╗"); // esquina superior derecha
                    } else if (y == j.mapa.alto - 1 && x == 0) {
                        mvaddwstr(y, x + offsetX, L"╚"); // esquina inferior izquierda
                    } else if (y == j.mapa.alto - 1 && x == j.mapa.largo - 1) {
                        mvaddwstr(y, x + offsetX, L"╝"); // esquina inferior derecha
                    } else if (y == 0 || y == j.mapa.alto - 1) {
                        mvaddwstr(y, x + offsetX, L"═"); // bordes horizontales
                    } else if (x == 0 || x == j.mapa.largo - 1) {
                        mvaddwstr(y, x + offsetX, L"║"); // bordes verticales
                    } else {
                        mvaddwstr(y, x + offsetX, L"="); // interior
                    }
                    attroff(COLOR_PAIR(1));
                    break;

                case '#':
                    attron(COLOR_PAIR(2));
                    mvaddwstr(y, x + offsetX, L"#");   // muro rompible
                    attroff(COLOR_PAIR(2));
                    break;

                case '@':
                    attron(COLOR_PAIR(3) | A_BOLD);
                    mvaddwstr(y, x + offsetX, L"●");   // jugador 1
                    attroff(COLOR_PAIR(3) | A_BOLD);
                    break;
                case '&':
                    attron(COLOR_PAIR(3) | A_BOLD);
                    mvaddwstr(y, x + offsetX, L"⚬");   // jugador 2
                    attroff(COLOR_PAIR(3) | A_BOLD);
                    break;
                case 'O':
                    if (parpadeo) { // bomba parpadeando
                        attron(COLOR_PAIR(11) | A_BOLD);
                        mvaddwstr(y, x + offsetX, L"¤"); 
                        attroff(COLOR_PAIR(11) | A_BOLD);
                    } else {
                        attron(COLOR_PAIR(7) | A_BOLD);
                        mvaddwstr(y, x + offsetX, L"¤");
                        attroff(COLOR_PAIR(7) | A_BOLD);
                    }
                    break;

                case '*':
                    attron(COLOR_PAIR(8) | A_BOLD);
                    mvaddwstr(y, x + offsetX, L"✦");   // explosión
                    attroff(COLOR_PAIR(8) | A_BOLD);
                    break;

                case 'E':
                    attron(COLOR_PAIR(4) | A_BOLD);
                    mvaddwstr(y, x + offsetX, L"♣");   // enemigo
                    attroff(COLOR_PAIR(4) | A_BOLD);
                    break;

                case 'K':
                    attron(COLOR_PAIR(5) | A_BOLD);
                    mvaddwstr(y, x + offsetX, L"♠");   // jefe
                    attroff(COLOR_PAIR(5) | A_BOLD);
                    break;

                case '>':
                    attron(COLOR_PAIR(6) | A_BOLD);
                    mvaddwstr(y, x + offsetX, L"▣");   // puerta abierta
                    attroff(COLOR_PAIR(6) | A_BOLD);
                    break;

                case 'X':
                    attron(COLOR_PAIR(6) | A_BOLD);
                    mvaddwstr(y, x + offsetX, L"□");   // puerta cerrada
                    attroff(COLOR_PAIR(6) | A_BOLD);
                    break;

                case '$':
                    if (parpadeo) { // power up bomba  parpadeando
                        attron(COLOR_PAIR(10) | A_BOLD);
                        mvaddwstr(y, x + offsetX, L"♦");
                        attroff(COLOR_PAIR(10) | A_BOLD);
                    } else {
                        attron(COLOR_PAIR(7) | A_BOLD);
                        mvaddwstr(y, x + offsetX, L"♦");
                        attroff(COLOR_PAIR(7) | A_BOLD);
                    }
                    break;

                case '+':
                    if (parpadeo) { // vida parpadeando
                        attron(COLOR_PAIR(9) | A_BOLD);
                        mvaddwstr(y, x + offsetX, L"♥");
                        attroff(COLOR_PAIR(9) | A_BOLD);
                    } else {
                        attron(COLOR_PAIR(7) | A_BOLD);
                        mvaddwstr(y, x + offsetX, L"♥");
                        attroff(COLOR_PAIR(7) | A_BOLD);
                    }
                    break;
                default:
                    mvaddch(y, x + offsetX, c);
            }
        }
    }

    // Redibujar puerta encima de todo en modo un jugador
    if (!j.enModoMultijugador && j.puerta.x > 0 && j.puerta.y > 0) {
        if (j.puerta.abierta) {
            if (parpadeo) { // puerta abierta parpadeando
                attron(COLOR_PAIR(6) | A_BOLD);
                mvaddwstr(j.puerta.y, j.puerta.x + offsetX, L"▣");
                attroff(COLOR_PAIR(6) | A_BOLD);
            } else {
                attron(COLOR_PAIR(7) | A_BOLD);
                mvaddwstr(j.puerta.y, j.puerta.x + offsetX, L"▣");
                attroff(COLOR_PAIR(7) | A_BOLD);
            }
        }
        else {
            attron(COLOR_PAIR(6) | A_BOLD);
            mvaddwstr(j.puerta.y, j.puerta.x + offsetX, L"□");
            attroff(COLOR_PAIR(6) | A_BOLD);
        }
    }

    dibujarPanelHUD(j, modoUnJugador);

    if(modoUnJugador)
        dibujarRanking(j.mapa.alto + 9);

    refresh();
};

// Verifica si hay bomba en una posicion
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

//muestra una pantalla de muerte cuando un jugador recibe daño
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
    if(jugador == 0) {
        j.mapa.posiciones[spawnY][spawnX] = '@';
    }
    else {
        j.mapa.posiciones[spawnY][spawnX] = '&';
    }
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

// Mueve un jugador en la dirección indicada
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
            nodelay(stdscr, FALSE);
            mostrarMuerteJugador(j, jugador);
            nodelay(stdscr, TRUE);
            return;
        }
    
        pthread_mutex_unlock(&mutex);
        return;
    }
    // Puede moverse a espacios vacios o a la puerta si esta abierta
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
        if(jugador == 0) {
            j.mapa.posiciones[newY][newX] = '@';
        }
        else {
            j.mapa.posiciones[newY][newX] = '&';
        }
    }

    pthread_mutex_unlock(&mutex);
};

// Marca una celda como explosion
void marcarExplosion(Juego &j, int x, int y) {
    if (x <= 0 || x >= j.mapa.largo - 1 || y <= 0 || y >= j.mapa.alto - 1) {
        return;
    }

    if (j.mapa.posiciones[y][x] == '=') {
        return;
    }

    j.mapa.posiciones[y][x] = '*';
}

// Calcula la distancia absoluta entre dos puntos 
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

    // Intenta hasta 2000 veces encontrar posicion valida
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
        if (i == 0) {
            j.mapa.posiciones[j.jugadores[i].y][j.jugadores[i].x] = '@';
        }
        else {
            j.mapa.posiciones[j.jugadores[i].y][j.jugadores[i].x] = '&';
        }
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

// Coloca una bomba en la posicion del jugador
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
            int dir = randDir(gen);  // Direccion random
            int newX = datos->juego->enemigos[i].x;
            int newY = datos->juego->enemigos[i].y;
            
            // Calcular nueva posicion
            switch(dir) {
                case 0: newY--; break;
                case 1: newY++; break;
                case 2: newX--; break;
                case 3: newX++; break;
            }
            
            // Validar movimiento 
            if (newX > 0 && newX < datos->juego->mapa.largo - 1 && 
                newY > 0 && newY < datos->juego->mapa.alto - 1 &&
                datos->juego->mapa.posiciones[newY][newX] == ' ') {
                
                // Limpiar posicion anterior
                datos->juego->mapa.posiciones[datos->juego->enemigos[i].y][datos->juego->enemigos[i].x] = ' ';
                
                // Actualizar posicion
                datos->juego->enemigos[i].x = newX;
                datos->juego->enemigos[i].y = newY;
                
                // Dibujar en nueva posicion
                char charEnemigo = datos->juego->enemigos[i].tieneLlave ? 'K' : 'E';
                datos->juego->mapa.posiciones[newY][newX] = charEnemigo;
            }
        }
        
        pthread_mutex_unlock(&mutex);
    }
    
    delete datos;
    return NULL;
}

// Hilo del cronometro que descuenta el tiempo
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
// Quita vida si estan en misma posicion que jugador
void* hiloAtaqueEnemigos(void* arg) {
    DataJuego* datos = (DataJuego*) arg;
    
    while (*(datos->juegoActivo)) {
        sleep(2);  // Atacan cada 2 segundos
        
        pthread_mutex_lock(&mutex);
        
        // Verificar colision enemigo-jugador
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
    nodelay(stdscr, TRUE);
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
    nodelay(stdscr, TRUE);
}


// Menu para seleccionar nivel desde el menu principal
// Muestra TODOS los niveles desbloqueados
int mostrarMenuSeleccionarNivel(const Juego& j) {
    int seleccion = 0;

    while (true) {
        erase();
        int max_y, max_x;
        getmaxyx(stdscr, max_y, max_x);

        mvprintw(max_y/2 - 8, max_x/2 - 15, "SELECCIONAR NIVEL");
        mvprintw(max_y/2 - 4, max_x/2 - 20,
                 "Nivel maximo desbloqueado: %d",
                 j.nivelMaximoDesbloqueado);
        mvprintw(max_y/2 - 1, max_x/2 - 23,
                 "Usa flechas y Enter ('Y' para salir)");
        mvprintw(max_y/2, max_x/2 - 15,
                 "Selecciona un nivel:");

        for (int i = 0; i < 5; i++) {
            bool desbloqueado = (i < j.nivelMaximoDesbloqueado);

            if (i == seleccion) attron(A_REVERSE);
            if (!desbloqueado) attron(A_DIM);

            if (desbloqueado)
                mvprintw(max_y/2 + 4 + i, max_x/2 - 15, "Nivel %d", i + 1);
            else
                mvprintw(max_y/2 + 4 + i, max_x/2 - 15, "Nivel %d (BLOQUEADO)", i + 1);

            if (!desbloqueado) attroff(A_DIM);
            if (i == seleccion) attroff(A_REVERSE);
        }

        refresh();

        int input = getch();

        if (input == KEY_UP && seleccion > 0) {
            seleccion--;
        } else if (input == KEY_DOWN && seleccion < 4) {
            seleccion++;
        } else if (input == 10) {
            if (seleccion < j.nivelMaximoDesbloqueado)
                return seleccion + 1;
            beep();
        } else if (input == 'y' || input == 'Y') {
            flushinp();
            return -1;
        }
    }
}
// Pantalla cuando gana un nivel// Pantalla cuando gana un nivel
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
    nodelay(stdscr, FALSE);
    return mostrarMenuSeleccionarNivel(j);
}

static const char* PUNTAJES_1P_FILE = "puntajes.csv";

string ahoraISO() {
    time_t t = time(nullptr);
    tm *lt = localtime(&t);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", lt);
    return string(buf);
}

void asegurarCSV() {
    ifstream in(PUNTAJES_1P_FILE);
    if (in.good()) return;
    ofstream out(PUNTAJES_1P_FILE);
    out << "fecha,nombre,puntaje\n";
}

string pedirNombre(const string& titulo) {
    echo();
    nodelay(stdscr, FALSE);
    curs_set(1);

    erase();
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    mvprintw(max_y/2 - 1, max_x/2 - (int)titulo.size()/2, "%s", titulo.c_str());
    mvprintw(max_y/2 + 1, max_x/2 - 18, "Nombre: ");
    refresh();

    char buf[64];
    getnstr(buf, 63);

    noecho();
    nodelay(stdscr, TRUE);
    curs_set(0);

    string nombre(buf);
    if (nombre.empty()) nombre = "Player";
    return nombre;
}


void asegurarCSV1P() {
    ifstream in(PUNTAJES_1P_FILE);
    if (in.good()) return;
    ofstream out(PUNTAJES_1P_FILE);
    out << "fecha,nombre,puntaje\n";
}

void guardarPuntajesCSV(const string& nombreJugador, int puntaje) {
    asegurarCSV1P();

    vector<Puntajes> puntajes;

    // leer existentes
    {
        ifstream in(PUNTAJES_1P_FILE);
        string line;
        getline(in, line); // header
        while (getline(in, line)) {
            if (line.empty()) continue;
            stringstream ss(line);
            vector<string> c; string p;
            while (getline(ss, p, ',')) c.push_back(p);
            if (c.size() < 3) continue;

            Puntajes s;
            s.fecha = c[0];
            s.nombre = c[1];
            s.puntaje = stoi(c[2]);
            puntajes.push_back(s);
        }
    }

    // agregar nuevo (TOP 5)
    Puntajes nuevo;
    nuevo.fecha = ahoraISO();
    nuevo.nombre = nombreJugador;
    nuevo.puntaje = puntaje;
    puntajes.push_back(nuevo);

    // ordenar desc por puntaje (si empata, más reciente primero por fecha string ISO)
    sort(puntajes.begin(), puntajes.end(), [](const Puntajes& a, const Puntajes& b) {
        if (a.puntaje != b.puntaje) return a.puntaje > b.puntaje;
        return a.fecha > b.fecha;
    });

    if ((int)puntajes.size() > 5) puntajes.resize(5);

    // escribir
    ofstream out(PUNTAJES_1P_FILE, ios::trunc);
    out << "fecha,nombre,puntaje\n";
    for (auto &s : puntajes) {
        out << s.fecha << "," << s.nombre << "," << s.puntaje << "\n";
    }
}

void mostrarPuntajes() {
    nodelay(stdscr, FALSE);
    erase();

    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    // Leer CSV
    ifstream in(PUNTAJES_1P_FILE);
    string line;
    vector<vector<string>> rows;

    if (in.good()) {
        getline(in, line); // header
        while (getline(in, line)) {
            if (line.empty()) continue;
            stringstream ss(line);
            vector<string> c;
            string p;
            while (getline(ss, p, ',')) c.push_back(p);
            if (c.size() >= 3) rows.push_back(c); // fecha,nombre,puntaje
        }
    }


    // Coordenadas centradas
    int tableW = 19 + 3 + 18 + 3 + 8; // FECHA + " | " + NOMBRE + " | " + PUNTAJE
    int x0 = (max_x - tableW) / 2;
    if (x0 < 0) x0 = 0;

    // Título
    attron(A_BOLD);
    mvprintw(2, (max_x - (int)strlen("TOP 5 PUNTAJES"))/2, "TOP 5 PUNTAJES");
    attroff(A_BOLD);

    // Encabezado tabla
    int y = 5;
    attron(A_BOLD);
    mvprintw(y, x0, "%-19s | %-18s | %-8s", "FECHA", "NOMBRE", "PUNTAJE");
    attroff(A_BOLD);
    y++;

    mvprintw(y, x0, "--------------------+--------------------+----------");
    y++;

    if (rows.empty()) {
        mvprintw(y, x0, "No hay puntajes registrados.");
        y += 2;
    } else {
        // Mostrar (máximo 5)
        for (int i = 0; i < (int)rows.size() && i < 5; i++) {
            string fecha = rows[i][0];
            string nombre = rows[i][1];
            string puntaje = rows[i][2];

            mvprintw(y + i, x0, "%-19.19s | %-18.18s | %8s",
                     fecha.c_str(), nombre.c_str(), puntaje.c_str());
        }
        y += min(5, (int)rows.size()) + 1;
    }

    mvprintw(max_y - 2, (max_x - (int)strlen("Presiona cualquier tecla para volver al menu..."))/2,
             "Presiona cualquier tecla para volver al menu...");
    refresh();
    getch();
    nodelay(stdscr, TRUE);
}

void dibujarRanking(int inicioY) {

    ifstream in(PUNTAJES_1P_FILE);

    string line;
    vector<Puntajes> ranking;

    if (!in.good()) return;

    getline(in, line); // header

    while (getline(in, line)) {

        if (line.empty()) continue;

        stringstream ss(line);

        string fecha, nombre, puntajeStr;

        getline(ss, fecha, ',');
        getline(ss, nombre, ',');
        getline(ss, puntajeStr, ',');

        Puntajes p;
        p.fecha = fecha;
        p.nombre = nombre;
        p.puntaje = stoi(puntajeStr);

        ranking.push_back(p);
    }

    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    int xRank = max_x / 2 - 15;

    attron(COLOR_PAIR(6) | A_BOLD);
    mvprintw(inicioY, xRank, "TOP 5 PUNTAJES");
    attroff(COLOR_PAIR(6) | A_BOLD);

    for (int i = 0; i < ranking.size() && i < 5; i++) {

        mvprintw(
            inicioY + 1 + i,
            xRank,
            "%d. %-15s %6d",
            i + 1,
            ranking[i].nombre.c_str(),
            ranking[i].puntaje
        );
    }
}

void inicializarJugadores(Juego &j, bool modoUnJugador) {
    // Si ya existen nombres, solo resetear stats/posiciones
    if (modoUnJugador) {
        if (j.jugadores.size() >= 1 && !j.jugadores[0].name.empty()) {
            j.jugadores[0].vidas = 3;
            j.jugadores[0].x = 1;
            j.jugadores[0].y = 1;
            j.jugadores[0].cantidad = 0;
            return;
        }
    } else {
        if (j.jugadores.size() >= 2 &&
            !j.jugadores[0].name.empty() &&
            !j.jugadores[1].name.empty()) {
            j.jugadores[0].vidas = 3; j.jugadores[0].x = 1;  j.jugadores[0].y = 1;  j.jugadores[0].cantidad = 0;
            j.jugadores[1].vidas = 3; j.jugadores[1].x = 29; j.jugadores[1].y = 13; j.jugadores[1].cantidad = 0;
            return;
        }
    }

    // Si no existían, crearlos y pedir nombres
    j.jugadores.clear();

    if (modoUnJugador) {
        string n1 = pedirNombre("Ingresa tu nombre (Un jugador)");
        Jugador p1 = {n1, 3, 1, 1, 0};
        j.jugadores.push_back(p1);
    } else {
        string n1 = pedirNombre("Nombre Jugador 1");
        string n2 = pedirNombre("Nombre Jugador 2");
        Jugador p1 = {n1, 3, 1, 1, 0};
        Jugador p2 = {n2, 3, 29, 13, 0};
        j.jugadores.push_back(p1);
        j.jugadores.push_back(p2);
    }
}
int xCentrado(int max_x, int len) {
    int x = (max_x - len) / 2;
    return (x < 0) ? 0 : x;
}

// Calcula el desplazamiento horizontal para centrar el mapa
int obtenerOffsetMapa(const Juego& j) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    int offsetX = (max_x - j.mapa.largo) / 2;

    if (offsetX < 0) {
        offsetX = 0;
    }

    return offsetX;
}

void imprimirCentrado(int y, const string& s) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    mvprintw(y, xCentrado(max_x, (int)s.size()), "%s", s.c_str());
}


void imprimirBloqueCentrado(int y0, const string& block) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    string line;
    stringstream ss(block);
    int y = y0;

    while (getline(ss, line)) {

        mvprintw(y, xCentrado(max_x, (int)line.size()), "%s", line.c_str());
        y++;
    }
}

int main() {
    setlocale(LC_ALL, "");

    pthread_mutex_init(&mutex, NULL);
    

    sem_init(&sem1, 0, 3);
    sem_init(&sem2, 0, 3);
    

    initscr();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    start_color();
    use_default_colors();


    init_pair(1, COLOR_BLUE,    -1);
    init_pair(2, COLOR_YELLOW,  -1);
    init_pair(3, COLOR_GREEN,   -1);
    init_pair(4, COLOR_RED,     -1);
    init_pair(5, COLOR_MAGENTA, -1);
    init_pair(6, COLOR_CYAN,    -1);
    init_pair(7, COLOR_WHITE,   -1);
    init_pair(8, COLOR_RED,  -1);
    init_pair(9, COLOR_GREEN,   -1);
    init_pair(10,COLOR_CYAN,    -1);
    init_pair(11, COLOR_RED, -1);
    
    Juego bomberman;
    bomberman.jugadores.clear(); 
    bomberman.puntaje = 0;
    bomberman.nivel = 1;
    bomberman.nivelMaximoDesbloqueado = 1;
    bomberman.tiempoRestante = 10;
    bomberman.enModoMultijugador = false;
    bomberman.juegoActivo = false; 
    

    string titulo = R"( 
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
    nodelay(stdscr, FALSE);

    while(menu){
        flushinp();
        nodelay(stdscr, FALSE);
        keypad(stdscr, TRUE);

        erase();

        int max_y, max_x;
        getmaxyx(stdscr, max_y, max_x);

        int marcoAncho = 104; 
        int marcoAlto  = 25;  
        int marcoX = (max_x - marcoAncho) / 2;
        int marcoY = 1; 

        // Dibujar borde superior (Color Cian)
        attron(COLOR_PAIR(6));
        mvaddwstr(marcoY, marcoX, L"╔");
        for(int i = 1; i < marcoAncho - 1; i++) mvaddwstr(marcoY, marcoX + i, L"═");
        mvaddwstr(marcoY, marcoX + marcoAncho - 1, L"╗");

        // Dibujar bordes laterales
        for(int i = 1; i < marcoAlto - 1; i++) {
            mvaddwstr(marcoY + i, marcoX, L"║");
            mvaddwstr(marcoY + i, marcoX + marcoAncho - 1, L"║");
        }

        // Dibujar borde inferior
        mvaddwstr(marcoY + marcoAlto - 1, marcoX, L"╚");
        for(int i = 1; i < marcoAncho - 1; i++) mvaddwstr(marcoY + marcoAlto - 1, marcoX + i, L"═");
        mvaddwstr(marcoY + marcoAlto - 1, marcoX + marcoAncho - 1, L"╝");
        attroff(COLOR_PAIR(6));

        // 2. LOGO ASCII CON COLORES (Rojo Brillante y Negrita)
        attron(COLOR_PAIR(4) | A_BOLD);
        imprimirBloqueCentrado(marcoY + 2, titulo);
        attroff(COLOR_PAIR(4) | A_BOLD);

        // Línea decorativa interna extendida (Color Amarillo)
        attron(COLOR_PAIR(2));
        int decoracionX = marcoX + 4;
        mvaddwstr(marcoY + 10, decoracionX, L"╠");
        for(int i = 1; i < marcoAncho - 9; i++) mvaddwstr(marcoY + 10, decoracionX + i, L"═");
        mvaddwstr(marcoY + 10, marcoX + marcoAncho - 5, L"╣");
        attroff(COLOR_PAIR(2));

        // 3. RENDERIZADO DE OPCIONES CON SELECCIONADOR EN BLANCO ESTÁNDAR
        int startY = marcoY + 13;

        for (int idx = 0; idx < (int)opciones.size(); idx++) {
            int y = startY + idx;
            int x = xCentrado(max_x, (int)opciones[idx].size());

            if (idx == seleccion) {
                // Seleccionador original en blanco (solo A_REVERSE sin color verde)
                attron(A_REVERSE);
                mvprintw(y, x, "%s", opciones[idx].c_str());
                attroff(A_REVERSE);
            } else {
                // Opciones normales en blanco 
                attron(COLOR_PAIR(7));
                mvprintw(y, x, "%s", opciones[idx].c_str());
                attroff(COLOR_PAIR(7));
            }
        }

        // 4. INSTRUCCIONES DE NAVEGACIÓN (Color Magenta)
        attron(COLOR_PAIR(5));
        imprimirCentrado(marcoY + marcoAlto - 3, "Usa ▲ y ▼ para navegar y Enter para seleccionar");
        attroff(COLOR_PAIR(5));

        refresh();
    

    input = getch();
    if (input == KEY_UP && seleccion > 0) {
        seleccion--;
    } else if (input == KEY_DOWN && seleccion < opciones.size() - 1) {
        seleccion++;
    } else if (input == ERR) {
        continue;
    } else if (input == 10) {

        bool jugando = true;
        

        if (opciones[seleccion] == "Un jugador") {

            bomberman.juegoActivo = false;
            usleep(500000); // Esperar medio segundo para que terminen hilos anteriores

            limpiarEstadoNivel(bomberman);
            
            bomberman.tiempoRestante = 10;
            bomberman.puntaje = 0;

            bomberman.enModoMultijugador = false;
            bomberman.tiempoRestante = 10;
            
            inicializarMapa(bomberman);
            colocarPowerups(bomberman);
            inicializarJugadores(bomberman, true);
            inicializarEnemigos(bomberman, bomberman.nivel);
            bomberman.mapa.posiciones[bomberman.jugadores[0].y][bomberman.jugadores[0].x] = '@';
            colocarPuertaLejosDelSpawn(bomberman, bomberman.jugadores[0].x, bomberman.jugadores[0].y);
            
            bomberman.juegoActivo = true;
            bomberman.runID++;
            int miRun = bomberman.runID;
            
            pthread_t hiloEnems, hiloCrono, hiloAtaques;
            

            DataJuego* datosEnems = new DataJuego;
            datosEnems->juego = &bomberman;
            datosEnems->juegoActivo = &bomberman.juegoActivo;
            datosEnems->runID = miRun;
            pthread_create(&hiloEnems, NULL, hiloMovimientoEnemigos, datosEnems);
            pthread_detach(hiloEnems);
            

            DataJuego* datosCrono = new DataJuego;
            datosCrono->juego = &bomberman;
            datosCrono->juegoActivo = &bomberman.juegoActivo;
            datosCrono->runID = miRun;
            pthread_create(&hiloCrono, NULL, hiloCronometro, datosCrono);
            pthread_detach(hiloCrono);
            

            DataJuego* datosAtaques = new DataJuego;
            datosAtaques->juego = &bomberman;
            datosAtaques->juegoActivo = &bomberman.juegoActivo;
            datosAtaques->runID = miRun;
            pthread_create(&hiloAtaques, NULL, hiloAtaqueEnemigos, datosAtaques);
            pthread_detach(hiloAtaques);

            
            bool nivelActivo = true;
            nodelay(stdscr, TRUE);

            while(jugando && nivelActivo) {
                dibujarMapa(bomberman, true);

                int ch = getch();
                if (ch != ERR) {
                switch(ch) {

                    case 'w': case 'W': moverJugador(bomberman, 0, 0, -1); break;
                    case 's': case 'S': moverJugador(bomberman, 0, 0, 1); break;
                    case 'a': case 'A': moverJugador(bomberman, 0, -1, 0); break;
                    case 'd': case 'D': moverJugador(bomberman, 0, 1, 0); break;

                    case 'e': case 'E': colocarBomba(bomberman, 0); break;

                    case 'y': case 'Y': 
                        jugando = false;
                        nivelActivo = false;
                        bomberman.juegoActivo = false;
                        break;
                }
                }


                if (bomberman.puerta.abierta &&
                    bomberman.jugadores[0].x == bomberman.puerta.x &&
                    bomberman.jugadores[0].y == bomberman.puerta.y) {
                    
                    nodelay(stdscr, FALSE);
                    bomberman.juegoActivo = false;
                    sleep(1);
                    

                    bomberman.puntaje += (bomberman.tiempoRestante * 10) + 50;
                    guardarPuntajesCSV(bomberman.jugadores[0].name, bomberman.puntaje);
                    

                    if (bomberman.nivel == bomberman.nivelMaximoDesbloqueado) {
                        bomberman.nivelMaximoDesbloqueado = bomberman.nivel + 1;
                        if (bomberman.nivelMaximoDesbloqueado > 5) {
                            bomberman.nivelMaximoDesbloqueado = 5;
                        }
                    }
                    

                    int resultado = mostrarVictoriaUnJugador(bomberman);
                    

                    if (resultado >= 1 && resultado <= 5) {
                        bomberman.nivel = resultado;
                        bomberman.tiempoRestante = 10;
                        
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
                        
                    } else if (resultado == -1) {
                        jugando = false;
                        nivelActivo = false;
                        bomberman.puerta.abierta = false;
                        bomberman.puerta.x = 0;
                        bomberman.puerta.y = 0;
                        bomberman.bombas.clear();
                        limpiarEstadoNivel(bomberman);
                        break; 
                }
                nodelay(stdscr, TRUE);
            }
                pthread_mutex_lock(&mutex);

                bool tiempoAgotado = (bomberman.tiempoRestante <= 0);

                pthread_mutex_unlock(&mutex);

                if (jugadorMuerto(bomberman.jugadores[0]) ||
                    tiempoAgotado)
                {
                    bomberman.juegoActivo = false;

                    usleep(200000);

                    nodelay(stdscr, FALSE);

                    mostrarGameOverUnJugador(bomberman);

                    flushinp();

                    bomberman.tiempoRestante = 10;
                    bomberman.juegoActivo = false;

                    guardarPuntajesCSV(
                        bomberman.jugadores[0].name,
                        bomberman.puntaje
                    );

                    jugando = false;
                    nivelActivo = false;
                }
            }
        } 

        else if (opciones[seleccion] == "Dos jugadores") {
            bomberman.enModoMultijugador = true;
            
            bomberman.jugadores.clear();
            inicializarJugadores(bomberman, false);
            bomberman.jugadores[0].cantidad = 0;
            bomberman.jugadores[1].cantidad = 0;
            
            inicializarMapa(bomberman);
            colocarPowerups(bomberman);
            bomberman.mapa.posiciones[bomberman.jugadores[0].y][bomberman.jugadores[0].x] = '@';
            bomberman.mapa.posiciones[bomberman.jugadores[1].y][bomberman.jugadores[1].x] = '&';
            
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

            
            

            while(jugando) {
                dibujarMapa(bomberman, false);

                int ch = getch();
                switch(ch) {

                    case 'w': case 'W': moverJugador(bomberman, 0, 0, -1); break;
                    case 's': case 'S': moverJugador(bomberman, 0, 0, 1); break;
                    case 'a': case 'A': moverJugador(bomberman, 0, -1, 0); break;
                    case 'd': case 'D': moverJugador(bomberman, 0, 1, 0); break;
                    case 'e': case 'E': colocarBomba(bomberman, 0); break;


                    case 'i': case 'I': moverJugador(bomberman, 1, 0, -1); break;
                    case 'k': case 'K': moverJugador(bomberman, 1, 0, 1); break;
                    case 'j': case 'J': moverJugador(bomberman, 1, -1, 0); break;
                    case 'l': case 'L': moverJugador(bomberman, 1, 1, 0); break;
                    case 'o': case 'O': colocarBomba(bomberman, 1); break;


                    case 'y': case 'Y': jugando = false; break;
                }
                

                if (jugadorMuerto(bomberman.jugadores[0]) || jugadorMuerto(bomberman.jugadores[1])) {
                    nodelay(stdscr, FALSE);
                    bomberman.juegoActivo = false;
                    sleep(1);
                    mostrarGanadorMultijugador(bomberman);
                    jugando = false;
                }
            }
        }
        

        else if (opciones[seleccion] == "Seleccionar Nivel") {
            int nivelSeleccionado = mostrarMenuSeleccionarNivel(bomberman);
            if (nivelSeleccionado != -1) {
                limpiarEstadoNivel(bomberman);
                bomberman.enModoMultijugador = false;
                bomberman.nivel = nivelSeleccionado;
                bomberman.tiempoRestante = 10;
                
                inicializarMapa(bomberman);
                colocarPowerups(bomberman);
                bomberman.jugadores.clear();
                inicializarJugadores(bomberman, true);
                inicializarEnemigos(bomberman, bomberman.nivel);
                bomberman.mapa.posiciones[bomberman.jugadores[0].y][bomberman.jugadores[0].x] = '@';
                colocarPuertaLejosDelSpawn(bomberman, bomberman.jugadores[0].x, bomberman.jugadores[0].y);
                
                bomberman.juegoActivo = true;
                bomberman.runID++;
                int miRun = bomberman.runID;
                
                pthread_t hiloEnems, hiloCrono, hiloAtaques;
                
                DataJuego* datosEnems = new DataJuego;
                datosEnems->juego = &bomberman;
                datosEnems->juegoActivo = &bomberman.juegoActivo;
                datosEnems->runID = miRun;
                pthread_create(&hiloEnems, NULL, hiloMovimientoEnemigos, datosEnems);
                pthread_detach(hiloEnems);
                
                DataJuego* datosCrono = new DataJuego;
                datosCrono->juego = &bomberman;
                datosCrono->juegoActivo = &bomberman.juegoActivo;
                datosCrono->runID = miRun;
                pthread_create(&hiloCrono, NULL, hiloCronometro, datosCrono);
                pthread_detach(hiloCrono);
                
                DataJuego* datosAtaques = new DataJuego;
                datosAtaques->juego = &bomberman;
                datosAtaques->juegoActivo = &bomberman.juegoActivo;
                datosAtaques->runID = miRun;
                pthread_create(&hiloAtaques, NULL, hiloAtaqueEnemigos, datosAtaques);
                pthread_detach(hiloAtaques);

                
                bool nivelActivo = true;
                bool jugandoNivel = true;
                

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
                            nodelay(stdscr, FALSE);
                            bomberman.juegoActivo = false;
                            usleep(300000); 
                            bomberman.puerta.abierta = false;
                            bomberman.puerta.x = 0;
                            bomberman.puerta.y = 0;
                            bomberman.bombas.clear();
                            limpiarEstadoNivel(bomberman); 
                            break;
                    }


                    if (bomberman.puerta.abierta && bomberman.jugadores[0].x == bomberman.puerta.x && bomberman.jugadores[0].y == bomberman.puerta.y) 
                    {
                        
                        nodelay(stdscr, FALSE);
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
                            bomberman.tiempoRestante = 10;
                            
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
                            
                            
                        } else if (resultado == -1) {
                            jugando = false;
                            nivelActivo = false;
                            bomberman.puerta.abierta = false;
                            bomberman.puerta.x = 0;
                            bomberman.puerta.y = 0;
                            bomberman.bombas.clear();
                            limpiarEstadoNivel(bomberman);
                            break;
                        }
                        nodelay(stdscr, TRUE);
                    }


                    bool tiempoAgotado = false;

                    pthread_mutex_lock(&mutex);
                    tiempoAgotado = (bomberman.tiempoRestante <= 0);
                    pthread_mutex_unlock(&mutex);


                    if (jugadorMuerto(bomberman.jugadores[0]) ||
                        tiempoAgotado)
                    {
                        bomberman.juegoActivo = false;

                        usleep(200000);

                        nodelay(stdscr, FALSE);

                        mostrarGameOverUnJugador(bomberman);

                        guardarPuntajesCSV(
                            bomberman.jugadores[0].name,
                            bomberman.puntaje
                        );

                        jugandoNivel = false;
                        nivelActivo = false;
                    }
                }
            }
        }
        

        else if (opciones[seleccion] == "Controles") {
            nodelay(stdscr, FALSE);
            clear();
            mvprintw(5, 10, "Controles:");
            mvprintw(7, 12, "Jugador 1: W (arriba), A (izquierda), S (abajo), D (derecha), E (colocar bomba)");
            mvprintw(9, 12, "Jugador 2: I (arriba), J (izquierda), K (abajo), L (derecha), O (colocar bomba)");
            mvprintw(11, 12, "Salir del juego: Presiona 'Y' durante el juego");
            mvprintw(13, 12, "Ve hasta la puerta abierta para continuar al siguiente nivel (modo un jugador)");
            mvprintw(15, 12, "Presiona cualquier tecla para volver al menu...");
            refresh();
            getch();
            nodelay(stdscr, TRUE);
        }
        

        else if (opciones[seleccion] == "Reglas") {
            nodelay(stdscr, FALSE);
            clear();
            int max_y, max_x;
            getmaxyx(stdscr, max_y, max_x);
            mvprintw(2, max_x/2 - 10, "REGLAS DEL JUEGO");
            mvprintw(4, 5, "MODO UN JUGADOR:");
            mvprintw(6, 10, "Destruir muros (#): 10 puntos");
            mvprintw(7, 10, "Eliminar enemigos (♣): 100 puntos");
            mvprintw(8, 10, "Enemigo ♠ (con llave): SOLO puede morir si NO hay enemigos ♣");
            mvprintw(9, 10, "Cuando ♠ muere sin ♣ disponibles -> ABRE LA PUERTA");
            mvprintw(10, 10, "Pasar nivel: 50 puntos mas 10 puntos por segundo restante");
            mvprintw(11, 10, "Tiempo limite: 3 minutos por nivel");
            mvprintw(12, 10, "Vidas: 3 (se pierden al tocar explosion o enemigo)");
            
            mvprintw(14, 5, "MODO DOS JUGADORES:");
            mvprintw(16, 10, "Siempre aparecen 4 enemigos");
            mvprintw(17, 10, "Los enemigos se mueven automaticamente");
            mvprintw(18, 10, "No hay puerta ni limite de tiempo");
            mvprintw(19, 10, "Gana quien tenga mas vidas");
            mvprintw(20, 10, "Destruir muros (#): 10 puntos");
            mvprintw(21, 10, "Eliminar enemigos (♣): 100 puntos");
            
            mvprintw(23, 5, "MECANICAS GENERALES:");
            mvprintw(25, 10, "Enemigos se mueven cada 1 segundo");
            mvprintw(26, 10, "Enemigos atacan cada 2 segundos si estan en tu posicion");
            mvprintw(27, 10, "Bombas explotan despues de 3 segundos");
            
            mvprintw(max_y - 2, max_x/2 - 20, "Presiona cualquier tecla para volver al menu...");
            refresh();
            getch();
            nodelay(stdscr, TRUE);
        }
        

        else if (opciones[seleccion] == "Puntajes") {
            nodelay(stdscr, FALSE);
            clear();
            int max_y, max_x;
            getmaxyx(stdscr, max_y, max_x);
            
            mvprintw(max_y/2 - 8, max_x/2 - 10, "PUNTAJES");
            
            mostrarPuntajes();
            nodelay(stdscr, TRUE);
        }
        

        else if (opciones[seleccion] == "Salir") {
            jugando = false;
            menu = false;
        }
    }
    }
    

    endwin();
    

    pthread_mutex_destroy(&mutex);
    

    sem_destroy(&sem1);
    sem_destroy(&sem2);
    
    return 0;
}
