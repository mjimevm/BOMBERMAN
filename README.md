# BOMBERMAN

Juego de Bomberman desarrollado en **C++**, utilizando **ncurses** para la interfaz gráfica en consola y **POSIX Threads (pthreads)** para la ejecución concurrente de los diferentes componentes del juego.

## Descripción

Este proyecto implementa una versión del clásico Bomberman con soporte para:

* Modo un jugador.
* Modo dos jugadores.
* Sistema de niveles progresivos.
* Generación dinámica de mapas.
* Sistema de puntajes.
* Persistencia de récords en CSV.
* Powerups.
* Programación concurrente mediante hilos.
* Sincronización utilizando mutex y semáforos.

El objetivo principal es utilizar bombas para destruir obstáculos, derrotar enemigos y avanzar entre niveles, o competir contra otro jugador en el modo multijugador.

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
* ncurses / ncursesw
* POSIX Threads (pthread)

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
g++ main.cpp -o bomberman -lpthread -lncursesw
```

---

# Ejecución

```bash
./bomberman
```

---

# Controles

## Jugador 1

| Tecla | Acción          |
| ----- | --------------- |
| W     | Arriba          |
| A     | Izquierda       |
| S     | Abajo           |
| D     | Derecha         |
| E     | Colocar bomba   |
| Y     | Salir del juego |

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

* Eliminar todos los enemigos comunes (**♣**).
* Derrotar al enemigo con llave (**♠**).
* Abrir la puerta (**▣**).
* Alcanzar la salida para avanzar al siguiente nivel.

### Características

* 3 vidas iniciales.
* Cronómetro de 3 minutos por nivel.
* Sistema de puntajes.
* Powerups ocultos.
* Progresión entre niveles.

---

## Modo Dos Jugadores

### Objetivo

* Eliminar al jugador rival.
* Obtener más puntos que el oponente.
* Sobrevivir más tiempo.

### Características

* Dos jugadores simultáneos.
* Bombas independientes.
* Enemigos compartidos.
* Sin límite de tiempo.

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

| Símbolo | Efecto                                         |
| ------- | ---------------------------------------------- |
| ♦       | Incrementa el rango de explosión de las bombas |
| ♥       | Otorga una vida adicional                      |

Los powerups aparecen ocultos detrás de muros destructibles.

---

# Mapa

## Dimensiones

```text
31 columnas x 15 filas
```

## Elementos del Juego

| Símbolo     | Descripción                    |
| ----------- | ------------------------------ |
| ●           | Jugador 1                      |
| ⚬           | Jugador 2                      |
| ♣           | Enemigo común                  |
| ♠           | Enemigo con llave              |
| ¤           | Bomba                          |
| ✦           | Explosión                      |
| #           | Muro destructible              |
| ╔ ╗ ╚ ╝ ═ ║ | Bordes y muros indestructibles |
| □           | Puerta cerrada                 |
| ▣           | Puerta abierta                 |
| ♦           | Powerup de rango               |
| ♥           | Powerup de vida                |

---

# Sistema de Bombas

## Características

* Máximo 3 bombas activas por jugador.
* Explosión automática después de 3 segundos.
* Alcance ampliable mediante powerups.
* Explosión en forma de cruz.

## Efectos

Las explosiones pueden destruir:

* Muros destructibles.
* Enemigos.
* Jugadores.
* Obstáculos.
* Revelar powerups ocultos.

## Elementos Relacionados

| Símbolo | Descripción       |
| ------- | ----------------- |
| ¤       | Bomba activa      |
| ✦       | Área de explosión |

Duración aproximada:

```text
0.7 segundos
```

---

# Niveles

| Nivel   | Cantidad de Enemigos |
| ------- | -------------------- |
| Nivel 1 | 1                    |
| Nivel 2 | 2                    |
| Nivel 3 | 3                    |
| Nivel 4 | 4                    |
| Nivel 5 | 5                    |

Cada nivel debe completarse para desbloquear el siguiente.

---

# Menú Principal

El juego incluye las siguientes opciones:

* Un Jugador
* Dos Jugadores
* Seleccionar Nivel
* Controles
* Reglas
* Puntajes
* Salir

---

# Concurrencia y Paralelismo

El proyecto implementa programación concurrente utilizando **POSIX Threads (pthreads)**.

## Hilos Implementados

### Hilo de Movimiento de Enemigos

Responsable de:

* Seleccionar direcciones aleatorias.
* Actualizar posiciones de enemigos.
* Simular movimiento autónomo cada 2 segundos.

### Hilo de Ataque de Enemigos

Responsable de:

* Detectar colisiones entre enemigos y jugadores.
* Reducir vidas cuando existe contacto.

### Hilo de Cronómetro

Responsable de:

* Reducir el tiempo restante cada segundo.
* Finalizar la partida cuando el tiempo llega a cero.

### Hilo de Bombas

Responsable de:

* Esperar el tiempo de detonación.
* Ejecutar explosiones.
* Aplicar daño a enemigos y jugadores.
* Limpiar explosiones.
* Liberar espacio para nuevas bombas.

Cada bomba genera un hilo independiente.

---

# Mecanismos de Sincronización

| Mecanismo / Recurso    | Implementación          | Propósito                            |
| ---------------------- | ----------------------- | ------------------------------------ |
| Mutex Global           | `pthread_mutex_t mutex` | Evitar condiciones de carrera        |
| Semáforo Jugador 1     | `sem_t sem1`            | Limitar a 3 bombas activas           |
| Semáforo Jugador 2     | `sem_t sem2`            | Limitar a 3 bombas activas           |
| `pthread_mutex_lock()` | Exclusión mutua         | Acceso seguro a recursos compartidos |
| `sem_trywait()`        | Reserva de recurso      | Verificar disponibilidad de bombas   |
| `sem_post()`           | Liberación de recurso   | Permitir nuevas bombas               |

## Recursos Compartidos Protegidos

* Mapa
* Jugadores
* Enemigos
* Bombas
* Puerta
* Puntajes
* Cronómetro
* Powerups

---

# Arquitectura General

La estructura principal del proyecto es:

```cpp
struct Juego
```

Esta estructura centraliza:

* Estado del mapa.
* Información de jugadores.
* Enemigos.
* Bombas activas.
* Powerups.
* Puerta.
* Puntajes.
* Tiempo restante.
* Estado de la partida.

Todos los hilos comparten esta estructura mediante mecanismos de sincronización.

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
* ncurses / ncursesw
* POSIX Threads (pthreads)
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

**CC3086 – Programación de Microprocesadores**
**Universidad del Valle de Guatemala**
