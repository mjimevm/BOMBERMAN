# BOMBERMAN

Juego de Bomberman simple en C++ con soporte multijugador y múltiples niveles.

## Descripción

Este proyecto implementa un juego de Bomberman con las siguientes características:

- **Modo un jugador** - Completa niveles destruyendo enemigos y abriendo puertas
- **Modo dos jugadores** - Compite contra otro jugador en la misma pantalla
- **Múltiples niveles** - 5 niveles desbloqueables progresivamente
- **Sincronización con hilos** - Movimiento de enemigos, cronómetro y ataques en paralelo
- **Interfaz ncurses** - Interfaz de terminal interactiva
- **Sistema de powerups** - Aumenta rango de bombas (+) o vidas ($)
- **Enemigos inteligentes** - Un enemigo especial (K) con llave que controla la puerta

---

## Estructura del Proyecto

```text
BOMBERMAN/
├── README.md
├── .gitignore
├── Makefile
└── src/
    ├── main.cpp          ← Código principal del juego
    ├── main              ← Ejecutable compilado
    └── bomberman         ← Ejecutable compilado (binario)
```

---

## Requisitos

- **C++ 11 o superior**
- **ncurses** - Librería para interfaz de terminal
- **pthreads** - Para manejo de hilos

---

## Instalación

### Ubuntu/Debian

```bash
sudo apt-get install libncurses-dev
sudo apt-get install build-essential
```

### macOS (Homebrew)

```bash
brew install ncurses
```

### Windows (MSYS2)

```bash
pacman -S mingw-w64-x86_64-ncurses
```

---

## Compilación y Ejecución

### Opción 1: Con Makefile (Recomendado)

```bash
make          # Compila el proyecto
make run      # Compila y ejecuta
make clean    # Limpia archivos compilados
```

### Opción 2: Compilación manual

```bash
g++ -std=c++11 -o bomberman src/main.cpp -lncurses -lpthread
./bomberman
```

---

## Controles

### Jugador 1 (Modo un jugador)

| Tecla | Acción |
|------|------|
| W | Arriba |
| A | Izquierda |
| S | Abajo |
| D | Derecha |
| E | Colocar bomba |
| Y | Salir del juego |

### Jugador 2 (Modo dos jugadores)

| Tecla | Acción |
|------|------|
| I | Arriba |
| J | Izquierda |
| K | Abajo |
| L | Derecha |
| O | Colocar bomba |
| Y | Salir del juego |

---

## Mecánicas del Juego

### Modo Un Jugador

- **Objetivo:** Destruir todos los enemigos (E) y luego eliminar al enemigo con llave (K) para abrir la puerta
- **Puerta (X):** Se abre solo después de matar a K cuando no hay enemigos E
- **Tiempo:** 3 minutos por nivel
- **Vidas:** 3 vidas iniciales

### Sistema de Puntuación

| Acción | Puntos |
|------|------|
| Destruir muro (#) | 10 |
| Eliminar enemigo (E) | 100 |
| Pasar nivel | 50 + tiempo restante |

---

### Modo Dos Jugadores

- **Objetivo:** Ser el último jugador en pie o tener más vidas
- **Enemigos:** Siempre aparecen 4 enemigos sin llave
- **Tiempo:** Sin límite de tiempo

### Sistema de Puntuación

| Acción | Puntos |
|------|------|
| Destruir muro (#) | 10 |
| Eliminar enemigo (E) | 100 |

---

## Características Técnicas

### Sincronización

El juego utiliza múltiples mecanismos de concurrencia:

- **Mutex** - Protege acceso a estructuras compartidas
- **Semáforos** - Limita a 3 bombas activas por jugador

### Hilos Implementados

- Movimiento de enemigos (cada 2 segundos)
- Cronómetro (cada 1 segundo)
- Ataques de enemigos (cada 2 segundos)
- Explosión de bombas (detonación tras 3 segundos)

---

## Mapa

- **Tamaño:** 31 x 15 celdas
- **Muros indestructibles:** Bordes y patrón de tablero (=)
- **Muros destructibles:** Aleatorios (#)
- **Zonas seguras:** Áreas protegidas en las esquinas iniciales

---

## Explosiones

- Forma de cruz
- Se expanden en 4 direcciones
- Destruyen:
  - Muros destructibles
  - Enemigos
  - Jugadores
- Duración aproximada: 0.7 segundos

---

## Niveles

| Nivel | Enemigos |
|------|------|
| Nivel 1 | 1 enemigo |
| Nivel 2 | 2 enemigos |
| Nivel 3 | 3 enemigos |
| Nivel 4 | 4 enemigos |
| Nivel 5 | 5 enemigos |

Cada nivel se desbloquea al completar el anterior.

---

## Menú Principal

- Un jugador
- Dos jugadores
- Seleccionar Nivel
- Controles
- Reglas
- Puntajes
- Salir

---

## Símbolos del Juego

| Símbolo | Significado |
|------|------|
| @ | Jugador |
| E | Enemigo común |
| K | Enemigo con llave |
| O | Bomba |
| * | Explosión |
| # | Muro destructible |
| = | Muro indestructible |
| X | Puerta cerrada |
| > | Puerta abierta |
| $ | Powerup: rango de bomba |
| + | Powerup: vidas |

---

## Solución de Problemas

### Error: `"ncurses.h: No such file or directory"`

Instala las librerías de desarrollo:

```bash
sudo apt-get install libncurses-dev
```

---

### El juego se ve cortado en la terminal

Asegúrate de que tu terminal tenga al menos:

```text
35 columnas x 20 filas
```

---

### Las teclas no responden correctamente

Algunos terminales requieren:

```bash
export TERM=xterm
./bomberman
```

---

## Tecnologías Utilizadas

- **C++11** - Lenguaje de programación
- **ncurses** - Interfaz de usuario en terminal
- **pthreads** - Manejo de hilos POSIX
- **Git/GitHub** - Control de versiones

---

## Autores

- **Andrés Pineda** – 25212  
- **Diego Rodríguez** – 25215  
- **Jimena Vásquez** – 25092  
- **Alejandro Sagastume** – 25257

---

## Curso

**CC3086 - Programación de Microprocesadores**  
Universidad del Valle de Guatemala
