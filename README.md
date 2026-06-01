# BOMBERMAN

Juego de Bomberman desarrollado en **C++**, utilizando **ncurses** para la interfaz en consola y **POSIX Threads (pthreads)** para la programación concurrente.

## Descripción

Este proyecto implementa una versión del clásico Bomberman con soporte para:

* Modo un jugador
* Modo dos jugadores
* Sistema de niveles progresivos
* Generación dinámica de mapas
* Powerups
* Sistema de puntajes
* Persistencia de récords en CSV
* Programación concurrente mediante hilos
* Sincronización con mutex y semáforos

El juego fue desarrollado como parte del curso **CC3086 - Programación de Microprocesadores**.

---

# Estructura del Proyecto

```text
BOMBERMAN/
├── README.md
├── .gitignore
├── Makefile
└── src/
    ├── main.cpp
    ├── bomberman
    └── puntajes.csv
```

---

# Requisitos

* C++11 o superior
* ncurses
* pthreads

---

# Instalación

## Ubuntu / Debian

```bash
sudo apt update
sudo apt install libncurses5-dev libncursesw5-dev
```

## macOS

```bash
brew install ncurses
```

## Windows (MSYS2)

```bash
pacman -S mingw-w64-x86_64-ncurses
```

---

# Compilación

```bash
g++ main.cpp -o bomberman -lpthread -lncurses
```

---

# Ejecución

```bash
./bomberman
```

---

# Controles

## Jugador 1

| Tecla | Acción        |
| ----- | ------------- |
| W     | Arriba        |
| A     | Izquierda     |
| S     | Abajo         |
| D     | Derecha       |
| E     | Colocar bomba |
| Y     | Salir         |

---

## Jugador 2

| Tecla | Acción        |
| ----- | ------------- |
| I     | Arriba        |
| J     | Izquierda     |
| K     | Abajo         |
| L     | Derecha       |
| O     | Colocar bomba |

---

# Mecánicas del Juego

## Modo Un Jugador

### Objetivo

* Eliminar todos los enemigos comunes (**E**)
* Encontrar y derrotar al enemigo con llave (**K**)
* Abrir la puerta
* Avanzar al siguiente nivel

### Características

* 3 vidas iniciales
* Cronómetro de 3 minutos
* Powerups ocultos
* Sistema de puntuación

---

## Modo Dos Jugadores

### Objetivo

* Eliminar al oponente
* Sobrevivir más tiempo
* Obtener la mayor cantidad de puntos

### Características

* Dos jugadores simultáneos
* Bombas independientes
* Enemigos compartidos
* Sin límite de tiempo

---

# Sistema de Puntuación

## Un Jugador

| Acción           | Puntos               |
| ---------------- | -------------------- |
| Destruir muro    | 10                   |
| Eliminar enemigo | 100                  |
| Completar nivel  | 50 + tiempo restante |

---

## Dos Jugadores

| Acción           | Puntos |
| ---------------- | ------ |
| Destruir muro    | 10     |
| Eliminar enemigo | 100    |

---

# Powerups

| Símbolo | Efecto                        |
| ------- | ----------------------------- |
| +       | Incrementa rango de explosión |
| $       | Incrementa vidas              |

Los powerups aparecen ocultos detrás de muros destructibles.

---

# Mapa

## Dimensiones

```text
31 columnas x 15 filas
```

## Elementos

| Símbolo | Descripción         |
| ------- | ------------------- |
| @       | Jugador             |
| E       | Enemigo             |
| K       | Enemigo con llave   |
| O       | Bomba               |
| *       | Explosión           |
| #       | Muro destructible   |
| =       | Muro indestructible |
| X       | Puerta cerrada      |
| >       | Puerta abierta      |
| +       | Powerup de rango    |
| $       | Powerup de vida     |

---

# Sistema de Bombas

## Características

* Máximo 3 bombas activas por jugador
* Explosión después de 3 segundos
* Alcance configurable mediante powerups
* Explosión en forma de cruz

## Efectos

Las explosiones pueden destruir:

* Muros destructibles
* Enemigos
* Jugadores
* Revelar powerups
* Abrir caminos

Duración aproximada:

```text
0.7 segundos
```

---

# Niveles

| Nivel | Enemigos |
| ----- | -------- |
| 1     | 1        |
| 2     | 2        |
| 3     | 3        |
| 4     | 4        |
| 5     | 5        |

Cada nivel debe completarse para desbloquear el siguiente.

---

# Menú Principal

* Un Jugador
* Dos Jugadores
* Seleccionar Nivel
* Controles
* Reglas
* Puntajes
* Salir

---

# Concurrencia y Paralelismo

El juego implementa programación concurrente utilizando **POSIX Threads (pthreads)**.

## Hilos Implementados

### Hilo de Movimiento de Enemigos

Responsable de:

* Mover enemigos automáticamente
* Elegir direcciones aleatorias
* Actualizar posiciones cada 2 segundos

---

### Hilo de Ataque de Enemigos

Responsable de:

* Detectar colisiones entre enemigos y jugadores
* Reducir vidas cuando existe contacto

---

### Hilo de Cronómetro

Responsable de:

* Reducir el tiempo restante del nivel
* Finalizar la partida cuando el tiempo llega a cero

---

### Hilo de Bombas

Responsable de:

* Esperar el tiempo de detonación
* Ejecutar explosiones
* Aplicar daño
* Limpiar explosiones
* Liberar espacio para nuevas bombas

Cada bomba genera su propio hilo independiente.

---

# Mecanismos de Sincronización

## Mutex Global

Se utiliza un mutex para proteger recursos compartidos:

```cpp
pthread_mutex_t mutex;
```

Protege:

* Mapa
* Jugadores
* Enemigos
* Bombas
* Puerta
* Puntajes
* Cronómetro

---

## Semáforos

Se utilizan dos semáforos:

```cpp
sem_t sem1;
sem_t sem2;
```

Su función es limitar:

```text
Máximo 3 bombas activas por jugador
```

Operaciones utilizadas:

```cpp
sem_trywait()
sem_post()
```

---

# Arquitectura General

La estructura principal del programa es:

```cpp
struct Juego
```

La cual almacena:

* Mapa
* Jugadores
* Enemigos
* Bombas
* Powerups
* Puerta
* Puntajes
* Tiempo restante
* Estado de la partida

Esto permite centralizar todo el estado del juego y compartirlo entre los distintos hilos.

---

# Archivo de Puntajes

Los puntajes se almacenan en:

```text
puntajes.csv
```

Formato:

```csv
fecha,nombre,puntaje
```

Ejemplo:

```csv
2025-05-31,Alejandro,1650
2025-05-31,Andres,1200
```

---

# Solución de Problemas

## Error: ncurses.h no encontrado

```bash
sudo apt install libncurses5-dev libncursesw5-dev
```

---

## Pantalla cortada

Se recomienda una terminal de al menos:

```text
80 columnas x 30 filas
```

---

## Problemas con las teclas

```bash
export TERM=xterm
./bomberman
```

---

# Tecnologías Utilizadas

* C++11
* ncurses
* pthreads
* STL (vector, string, fstream)
* Git
* GitHub

---

# Autores

* Andrés Pineda – 25212
* Diego Rodríguez – 25215
* Jimena Vásquez – 25092
* Alejandro Sagastume – 25257

---

# Curso

**CC3086 - Programación de Microprocesadores**
**Universidad del Valle de Guatemala**
