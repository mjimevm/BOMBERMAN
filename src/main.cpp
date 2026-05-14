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
    int vidas;
    int x;
    int y;
    int cantidad;
};

struct Enemigo {
    string name;
    int vidas;
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
    int autor; // para saber quien la coloco
};

struct Muro {
    int x;
    int y;
    bool destructible;
};
struct Puerta {
    int x;
    int y;
    bool abierta;
};

struct Juego {
    Mapa mapa;
    vector<Jugador> jugadores;
    vector<Enemigo> enemigos;
    vector<Bomba> bombas;
    vector<Muro> muros;
    
    // Modo un jugador
    int nivel = 1;
    int puntaje = 0;
    Puerta puerta = {0, 0, false};
};


//Estructura para enviar los datos de la bomba al hilo
struct Explosion {
    Juego* juego;
    Bomba bomba;
};

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

void interfaz_un_jugador (const Juego &j, bool modoUnJugador) {
    int uiy = j.mapa.alto + 1;
    if (modoUnJugador) {
        mvprintw(uiy, 0, "Nivel: %d  Puntaje: %d  Vidas: %d   (E) bomba  (Y) salir", 
            j.nivel, j.puntaje, j.jugadores[0].vidas);
        if (j.puerta.abierta) {
            mvprintw(uiy + 1, 0, "Puerta: (%d,%d) - Cruza la puerta (X) para pasar de nivel", j.puerta.x, j.puerta.y);
        } else {
            mvprintw(uiy + 1, 0, "Puerta cerrada - Destruye muros para encontrar la puerta");
        }
    } else {
        mvprintw(uiy, 0, "J1 Vidas: %d  Puntaje: %d     J2 Vidas: %d  Puntaje: %d", j.jugadores[0].vidas, j.jugadores[0].cantidad, j.jugadores[1].vidas, j.jugadores[1].cantidad);
    }
}

void dibujarMapa(const Juego &j, bool modoUnJugador) {
    erase();

    for (int y = 0; y < j.mapa.alto; y++) {
        for (int x = 0; x < j.mapa.largo; x++) {
            //mvaddch pone el caracter en la coordenada 
            mvaddch(y, x, (unsigned char)j.mapa.posiciones[y][x]);
        }
    }
    interfaz_un_jugador(j, modoUnJugador);
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

int distManhattan(int x1,int y1,int x2,int y2){
    return abs(x1-x2) + abs(y1-y2);
}

bool esCeldaLibreParaPuerta(const Juego& j, int x, int y) {
    if (x <= 0 || x >= j.mapa.largo - 1 || y <= 0 || y >= j.mapa.alto - 1) return false;
    return j.mapa.posiciones[y][x] == ' ';
}

void colocarPuertaLejosDelSpawn(Juego& j, int spawnX, int spawnY) {
    const int minDist = 20;

    // intentos random
    for (int t = 0; t < 2000; t++) {
        int x = randLargo(gen);
        int y = randAlto(gen);  

        if (!esCeldaLibreParaPuerta(j, x, y)) continue;
        if (distManhattan(spawnX, spawnY, x, y) < minDist) continue;

        j.puerta = {x, y, true};
        j.mapa.posiciones[y][x] = 'X';
        return;
    }
    int fx = j.mapa.largo - 2;
    int fy = j.mapa.alto - 2;
    if (j.mapa.posiciones[fy][fx] == ' ') {
        j.puerta = {fx, fy, true};
        j.mapa.posiciones[fy][fx] = 'X';
    }
}

//Expande la explosion de la bomba hacia arriba, abajo, izquierda y derecha
void explotarBomba(Juego &j, Bomba bomba) {
    marcarExplosion(j, bomba.x, bomba.y);

    for (int i = 1; i <= bomba.distancia; i++) {

            // derecha
            if (j.mapa.posiciones[bomba.y][bomba.x + i] == '#') {
                j.mapa.posiciones[bomba.y][bomba.x + i] = '*';
                if (bomba.autor < j.jugadores.size())
                    j.jugadores[bomba.autor].cantidad += 10; // o .puntaje si cambias el nombre
            }
            else {
                marcarExplosion(j, bomba.x + i, bomba.y);
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
                    if (bomba.autor < j.jugadores.size())
                        j.jugadores[bomba.autor].cantidad += 10;
                }
            }
            else {
                marcarExplosion(j, bomba.x, bomba.y + i);
            }

        // arriba
        if (j.mapa.posiciones[bomba.y - i][bomba.x] != '=') {

            if (j.mapa.posiciones[bomba.y - i][bomba.x] == '#') {
                j.mapa.posiciones[bomba.y - i][bomba.x] = '*';
                if (bomba.autor < j.jugadores.size())
                    j.jugadores[bomba.autor].cantidad += 10;
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

    // Quitar vida si algún jugador está sobre una explosión
    for (int i = 0; i < datos->juego->jugadores.size(); ++i) {
        int px = datos->juego->jugadores[i].x;
        int py = datos->juego->jugadores[i].y;
        if (datos->juego->mapa.posiciones[py][px] == '*') {
            datos->juego->jugadores[i].vidas--;
        }
    }

    pthread_mutex_unlock(&mutex);

    usleep(700000);

    pthread_mutex_lock(&mutex);

    limpiarExplosion(*datos->juego);

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
    bomba.autor = jugador;

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

void limpiarEstadoNivel(Juego& j) {
    j.bombas.clear();
    j.enemigos.clear();
    j.muros.clear();
    j.puerta = {0, 0, false};
}

void iniciarNivelUnJugador(Juego& j) {
    limpiarEstadoNivel(j);

    // (re)generar mapa
    inicializarMapa(j);

    // (re)colocar jugador 1 en spawn fijo
    j.jugadores.clear();
    inicializarJugadores(j, true);
    // Solo usas jugador 0 en este modo
    j.mapa.posiciones[j.jugadores[0].y][j.jugadores[0].x] = '@';

    // puerta lejos del spawn
    colocarPuertaLejosDelSpawn(j, j.jugadores[0].x, j.jugadores[0].y);
}

void inicializarEnemigos(Juego& j, int cuantos) {
    for (int e = 0; e < cuantos; ++e) {
        int x, y;
        do {
            x = randLargo(gen);
            y = randAlto(gen);
        } while(j.mapa.posiciones[y][x] != ' '); 
        Enemigo enemy = {"Enemigo", 1, x, y };
        j.enemigos.push_back(enemy);
        j.mapa.posiciones[y][x] = 'E';
    }
}

int main() {
    Juego bomberman;
    pthread_mutex_init(&mutex, NULL);
    
    //configuracion de ncurses
    initscr();             // Inicia ncurses
    noecho();              // ocultar las letras que se presionan
    curs_set(0);           // Oculta el cursor
    keypad(stdscr, TRUE);  // Habilita las flechas del teclado

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
    vector<string> opciones = {"Un jugador", "Dos jugadores", "Controles", "Puntajes", "Salir"};
    int input;
    int seleccion = 0;
    bool menu = true;

    while(menu){
    erase();
    //Menu de inicio
    mvprintw(0, 12, "%s", titulo);
    for(int i=13; i<13+opciones.size(); i++){
        if (i == seleccion + 13) {
            attron(A_REVERSE); // Resalta la opcion seleccionada
            mvprintw(i, 40, "%s", opciones[i - 13].c_str());
            attroff(A_REVERSE); // Quita el resaltado
        } else {
            mvprintw(i, 40, "%s", opciones[i - 13].c_str());
        }
    }
    refresh();
    input = getch();
    if (input == KEY_UP && seleccion > 0) {
        seleccion--;
    } else if (input == KEY_DOWN && seleccion < opciones.size() - 1) {
        seleccion++;
    } else if (input == 10) { // Enter presionado

        //bucle de juego segun la opcion seleccionada
        bool jugando = true;
        switch(opciones[seleccion].c_str()[0]) {
            case 'U': // Un jugador
                // Inicializar el mapa y los jugadores
                inicializarMapa(bomberman);
                inicializarJugadores(bomberman, true);
                inicializarEnemigos(bomberman, bomberman.nivel); // Agrega segun el nivel
                bomberman.mapa.posiciones[bomberman.jugadores[0].y][bomberman.jugadores[0].x] = '@';
                timeout(50);
                while(jugando) {
                    // Si es un modo jugador, dibuja la interfaz de un jugador
                    dibujarMapa(bomberman, true);

                    int ch = getch(); //Detecta la tecla presionada
                    switch(ch) {
                        // P1: WASD
                        case 'w': case 'W': moverJugador(bomberman, 0, 0, -1); break;
                        case 's': case 'S': moverJugador(bomberman, 0, 0, 1); break;
                        case 'a': case 'A': moverJugador(bomberman, 0, -1, 0); break;
                        case 'd': case 'D': moverJugador(bomberman, 0, 1, 0); break;
                        case 'e': case 'E': colocarBomba(bomberman, 0); break; // bomba jugador 1
                        case 'y': case 'Y': jugando = false; break; // Salir
                    }
                }
                break;
            case 'D': // Dos jugadores
                // Inicializar el mapa y los jugadores
                inicializarMapa(bomberman);
                inicializarJugadores(bomberman, false);
                bomberman.mapa.posiciones[bomberman.jugadores[0].y][bomberman.jugadores[0].x] = '@';
                bomberman.mapa.posiciones[bomberman.jugadores[1].y][bomberman.jugadores[1].x] = '@';

                // Bucle principal del juego
                timeout(50);
                while(jugando) {
                    dibujarMapa(bomberman, false);

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

                        case 'y': case 'Y': jugando = false; break; // Salir
                    }
                }
                break;
            case 'C': // Controles
                timeout(-1); // Espera indefinidamente por una tecla
                clear();
                mvprintw(5, 10, "Controles:");
                mvprintw(7, 12, "Jugador 1: W (arriba), A (izquierda), S (abajo), D (derecha), E (colocar bomba)");
                mvprintw(9, 12, "Jugador 2: I (arriba), J (izquierda), K (abajo), L (derecha), O (colocar bomba)");
                mvprintw(11, 12, "Salir del juego: Presiona 'Y' durante el juego");
                mvprintw(13, 12, "Presiona cualquier tecla para volver al menu...");
                getch();
                timeout(50); // Vuelve a esperar 50ms para el juego
                break;
            case 'P': //puntajes
                timeout(-1); // Espera indefinidamente por una tecla
                clear();
                mvprintw(5, 10, "Puntajes:");
                // Mostrar puntajes aquí
                mvprintw(11, 12, "Presiona cualquier tecla para volver al menu...");
                getch();
                timeout(50); // Vuelve a esperar 50ms para el juego
                break;
            case 'S': // Salir
                jugando = false;
                menu = false;
                break;
            }
        }
    }
    endwin(); // Termina ncurses
    pthread_mutex_destroy(&mutex);
    return 0;
};