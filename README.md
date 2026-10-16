# Evangelion: Proyecto de Crianza

Un RPG de vínculos y gestión de condiciones. Treinta días en Tokio-3, un chico de
catorce años, ocho personas y un EVA que responde.

Escrito en C. **Un solo archivo, sin dependencias, sin assets.** Todo —datos,
partidas, guardado— vive en `evangelion_loving_project.c`.

---

## Qué es

No es un juego de combate. Es un juego sobre lo que pasa cuando sos el único que
puede decidir y no sos capaz de decidir.

Cada día hacés **una sola actividad**. Los vínculos con los otros ocho personajes
van de **-100 a +100**: no todo el mundo mejora. Alguien puede Alejarte. Otro
puede dejar de hablarte. Y el mundo lo recuerda.

## Compilar

### Windows

```sh
gcc -std=c11 -O2 -Wall -Wextra -pedantic -finput-charset=UTF-8 -fexec-charset=UTF-8 ^
    -o "Evangelion Loving Project.exe" evangelion_loving_project.c
```

O, más simple, con un `Makefile` incluido:

```sh
make
```

### Linux / macOS

```sh
make
# o a mano:
gcc -std=c11 -O2 -Wall -Wextra -pedantic -o evangelion_loving_project evangelion_loving_project.c
```

Sin `-finput-charset` no hace falta: en Linux/macOS la salida va directo a la
terminal. El ejecutable queda en la carpeta donde lo compilaste.

## Cómo se juega

| Tecla | Qué hace |
|---|---|
| `1` | Realizar la actividad del día (una sola vez) |
| `2` | Ver las misiones |
| `3` | Ver los vínculos |
| `4` | Citas disponibles hoy |
| `5` | Ver logros |
| `6` | Guardar (elegís ranura) |
| `7` | Terminar el día |
| `8` | Cómo se juega |
| `9` | Leer el diario |
| `c` | Apagar / encender el color (funciona en cualquier prompt) |

## Los sistemas

**Actividades.** Entrenar, estudiar, socializar, descansar, sincronizar. Cada día
una sola, y la misión del día dice cuál te toca. La energía es el límite real:
bajarla de 10 te arruina el día.

**Vínculos.** Afinidad de -100 a +100 en siete etapas: `HOSTIL`, `EVITADO`,
`DESCONOCIDO`, `CONOCIDO`, `AMIABLE`, `CERCANO`, `UNIDO`. Los dos primeros
no son una etiqueta: alguien que te evita deja de estar disponible para
eventos que importan.

**Memoria.** El mundo recuerda lo que hiciste. Si le diste tu abrigo a Rei el día
8, eso pesa el día 20. Hay banderas explícitas, no sólo números.

**Combate.** Tres ángeles en treinta días. Cada pelea tiene una táctica real:
atacar, sincronizar, esquivar, o **gritar** — daño garantizado a un precio alto.
El combate ocurre *antes* de la actividad del día, así que la energía con la que
llegás importa: hay un aviso la vispera.

**Crisis.** No es "estar triste". Es no aguantar más, y sale de la combinación de
cobardía alta con confianza baja. Hay tres crisis distintas.

**Diario.** Un renglón por día con lo que pasó y qué vínculos se movieron. Es lo
que hace legible el sistema de memoria.

**Finales.** Ocho, con condiciones distintas. Se registran en `finales.dat`, así
que el rejuego tiene objetivo.

## Archivos que escribe

Junto al ejecutable, no en el directorio de trabajo:

| Archivo | Qué es |
|---|---|
| `partida_1.dat` … `partida_3.dat` | Tres partidas en paralelo (~9,7 KB cada una) |
| `finales.dat` | Máscara de los 8 finales ya vistos (4 bytes) |

Cada partida se autoguarda al terminar el día. Si borrás los `.dat` no pasa nada:
empezás de cero y conservás el registro de finales.

## Estado

```
gcc -std=c11 -O2 -Wall -Wextra -pedantic   →  0 avisos
gcc -fanalyzer                             →  0 hallazgos
-Wconversion -Wshadow -Wformat=2 ...       →  0 avisos
2916 líneas · un archivo · sin system()
```

`SAVE_VERSION 4`. Los archivos de partidas viejas se rechazan con un mensaje
claro en vez de romperse.

## Nota

Fan game. *Neon Genesis Evangelion* es de Gainax / Khara. Este proyecto no tiene
nada que ver con ellos: es un ejercicio de narrativa y de diseño de sistemas en
C, con textos propios.

Si alguna vez quisieras publicarlo, conviene mirá primero cómo los estudios
manejan el fan game, porque la propiedad intelectual del material original no es
tuya aunque el código sí lo sea.
