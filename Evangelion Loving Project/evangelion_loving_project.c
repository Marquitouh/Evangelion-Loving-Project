#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DIAS_TOTALES 30
#define PERSONAJES 9
#define MAX_MISIONES 5
#define MAX_LOGROS 10
#define MAX_CITAS 5

typedef struct {
    int fuerza;
    int inteligencia;
    int animo;
    int sincronizacion;
    int energia;
    int afinidad[PERSONAJES];
    int confianza;
    int cobardia;
    int rebeldia;
} Shinji;

typedef struct {
    char descripcion[100];
    int personajeIndex;    // personaje que asigna la misión
    int tipo;              // 1=entrenar, 2=estudiar, 3=socializar, 4=descansar, 5=sincronizar
    int completada;        // 0=no, 1=sí
    int recompensaEnergia;
    int recompensaAfinidad;
} Mision;

typedef struct {
    char nombre[30];
    char descripcion[100];
    int desbloqueado;
} Logro;

typedef struct {
    int personajeIndex;
    int diaDisponible;
    int fueVisitada;       // 0=no, 1=sí
} Cita;

const char* nombresPersonajes[PERSONAJES] = {
    "Asuka", "Rei", "Misato", "Kaworu", "Mari",
    "Mana", "Ritsuko", "Maya", "Shinji"
};

const char* personalidadesDesc[3] = {
    "Confianza",
    "Cobardía",
    "Rebeldía"
};

// ----------- Declaración de funciones --------------
void mostrarEstadisticas(Shinji s, int dia);
void eventoEspecial(int dia, Shinji* s, Mision misiones[], int* misionesCount);
void realizarActividad(Shinji* s, Mision misiones[], int misionesCount);
void calcularFinal(Shinji s, Logro logros[], int logrosCount);
void inicializarMision(Mision* m, const char* desc, int pIndex, int tipo, int eRecompensa, int aRecompensa);
void mostrarMisiones(Mision misiones[], int misionesCount);
int verificarMision(Shinji* s, Mision* m, int eleccionActividad, int objetivoPersonaje);
void asignarMision(Mision misiones[], int* misionesCount, int dia);
void procesarMision(Shinji* s, Mision* m);
void mostrarLogros(Logro logros[], int logrosCount);
void desbloquearLogro(Logro logros[], int* logrosCount, const char* nombre, const char* descripcion);
void eventoMental(Shinji* s);
void menuPrincipal(Shinji* s, Mision misiones[], int* misionesCount, Logro logros[], int* logrosCount, Cita citas[], int* citasCount, int dia);
void inicializarCita(Cita* c, int personajeIndex, int dia);
void mostrarCitasDisponibles(Cita citas[], int citasCount, int dia);
void visitarCita(Shinji* s, Cita* c, Logro logros[], int* logrosCount);
void chequearLogros(Shinji s, Logro logros[], int* logrosCount);

// ----------- Función principal --------------
int main() {
    srand(time(NULL));

        int opcionInicio;
    printf("========== Evangelion: Proyecto de Crianza ==========\n");
    printf("1. Nueva partida\n");
    printf("2. Cargar partida guardada\n> ");
    scanf("%d", &opcionInicio);
    getchar(); // limpiar buffer


    Shinji shinji;
    Mision misiones[MAX_MISIONES];
    int misionesCount = 0;
    Logro logros[MAX_LOGROS];
    int logrosCount = 0;
    Cita citas[MAX_CITAS];
    int citasCount = 0;

    if (opcionInicio == 2) {
        if (!cargarPartida(&shinji, misiones, &misionesCount, logros, &logrosCount, citas, &citasCount)) {
            printf("Iniciando nueva partida...\n");
            shinji = (Shinji){30, 30, 50, 25, 100, {0}, 0, 0, 0};
        }
    } else {
        shinji = (Shinji){30, 30, 50, 25, 100, {0}, 0, 0, 0};
    }

    printf("========== Evangelion: Proyecto de Crianza - Versión Completa ==========\n");

    for (int dia = 1; dia <= DIAS_TOTALES; dia++) {
        system("cls||clear");
        mostrarEstadisticas(shinji, dia);
        eventoEspecial(dia, &shinji, misiones, &misionesCount);
        asignarMision(misiones, &misionesCount, dia);
        menuPrincipal(&shinji, misiones, &misionesCount, logros, &logrosCount, citas, &citasCount, dia);
        chequearLogros(shinji, logros, &logrosCount);

        if (shinji.animo < 20) {
            printf("\n¡Shinji está en crisis mental! Debe afrontarla...\n");
            eventoMental(&shinji);
        }

        printf("\nPresiona ENTER para continuar al siguiente día...\n");
        getchar();
    }

    system("cls||clear");
    mostrarEstadisticas(shinji, DIAS_TOTALES);
    calcularFinal(shinji, logros, logrosCount);

    printf("\nLogros obtenidos:\n");
    mostrarLogros(logros, logrosCount);

    return 0;
}

// ----------- Implementación de funciones ------------

// Mostrar estadísticas y personalidad
void mostrarEstadisticas(Shinji s, int dia) {
    printf("\n╔═══════════ DÍA %d ═══════════╗\n", dia);
    printf(" Fuerza:         %d\n", s.fuerza);
    printf(" Inteligencia:   %d\n", s.inteligencia);
    printf(" Ánimo:          %d\n", s.animo);
    printf(" Sincronización: %d\n", s.sincronizacion);
    printf(" Energía:        %d\n", s.energia);
    printf(" Personalidad: Confianza:%d Cobardía:%d Rebeldía:%d\n", s.confianza, s.cobardia, s.rebeldia);
    printf(" Afinidad:\n");
    for (int i = 0; i < PERSONAJES; i++) {
        printf("  - %-8s: %d\n", nombresPersonajes[i], s.afinidad[i]);
    }
    printf("╚═════════════════════════════╝\n");
}

// Eventos especiales (agregados nuevos con asignación de misiones)
void eventoEspecial(int dia, Shinji* s, Mision misiones[], int* misionesCount) {
        switch (dia) {
        case 5:
            printf("[Evento] Combate contra un Ángel: ¡activando EVA-01!\n");
            s->sincronizacion += 10;
            break;
        case 10:
            printf("[Evento] Rei te invita a hablar.\n");
            printf("¿Quieres aceptar la invitación?\n1. Sí\n2. No\n> ");
            int r; scanf("%d", &r);
            if (r == 1) {
                printf("Rei te escucha y su afinidad aumenta.\n");
                s->afinidad[1] += 10;
                s->confianza += 5;
            } else {
                printf("Te retiraste. Cobardía aumenta.\n");
                s->cobardia += 5;
            }
            break;
        case 15:
            printf("[Evento] Asuka te reta a entrenar juntos.\n");
            printf("¿Aceptas?\n1. Sí\n2. No\n> ");
            int a; scanf("%d", &a);
            if (a == 1) {
                printf("Entrenamiento intenso, fuerza y afinidad con Asuka aumentan.\n");
                s->fuerza += 5;
                s->afinidad[0] += 10;
                s->confianza += 5;
                s->energia -= 10;
            } else {
                printf("Asuka se burla de tu decisión.\n");
                s->cobardia += 5;
            }
            break;
        case 20:
            printf("[Evento] Misato te pregunta sobre tu confianza en tu padre.\n");
            printf("1. Sí\n2. No\n> ");
            int m; scanf("%d", &m);
            if (m == 1) {
                printf("Misato te mira con lástima.\n");
                s->cobardia += 5;
            } else {
                printf("Misato sonríe: \"Eso quería oír.\"\n");
                s->rebeldia += 5;
                s->afinidad[2] += 5;
            }
            break;
        case 25:
            printf("[Evento] Kaworu te propone una conversación profunda.\n");
            printf("¿Aceptas?\n1. Sí\n2. No\n> ");
            int k; scanf("%d", &k);
            if (k == 1) {
                printf("Kaworu y vos comparten un momento especial.\n");
                s->afinidad[3] += 15;
                s->animo += 10;
                s->confianza += 5;
            } else {
                printf("Te retiras, pero Kaworu parece entender.\n");
                s->cobardia += 5;
            }
            break;
        default:
            printf("[Evento] Día tranquilo en Tokio-3.\n");
            break;
        }
}

// Asignar misiones dinámicamente para mantener el juego activo
void asignarMision(Mision misiones[], int* misionesCount, int dia) {
    if (*misionesCount >= MAX_MISIONES) return; // límite de misiones activas

    int tipo = (dia % 5) + 1; // tipo entre 1 y 5 para variedad
    int personaje = dia % PERSONAJES;

    char desc[100];
    switch(tipo) {
        case 1: snprintf(desc, 100, "Entrena para mejorar tu fuerza."); break;
        case 2: snprintf(desc, 100, "Estudia para mejorar tu inteligencia."); break;
        case 3: snprintf(desc, 100, "Socializa con %s para fortalecer la relación.", nombresPersonajes[personaje]); break;
        case 4: snprintf(desc, 100, "Descansa para recuperar energía."); break;
        case 5: snprintf(desc, 100, "Sincroniza con el EVA para mejorar tu rendimiento."); break;
    }

    inicializarMision(&misiones[*misionesCount], desc, personaje, tipo, 10, 7);
    printf("\n[Nueva misión asignada]: %s\n", desc);
    (*misionesCount)++;
}

// Inicializar misión
void inicializarMision(Mision* m, const char* desc, int pIndex, int tipo, int eRecompensa, int aRecompensa) {
    strncpy(m->descripcion, desc, 100);
    m->personajeIndex = pIndex;
    m->tipo = tipo;
    m->completada = 0;
    m->recompensaEnergia = eRecompensa;
    m->recompensaAfinidad = aRecompensa;
}

// Mostrar misiones activas
void mostrarMisiones(Mision misiones[], int misionesCount) {
    printf("\nMisiones activas:\n");
    for (int i = 0; i < misionesCount; i++) {
        if (!misiones[i].completada)
            printf("%d. [%s]\n", i + 1, misiones[i].descripcion);
    }
    if(misionesCount==0) printf("No hay misiones activas.\n");
}

// Verificar si actividad cumple misión
int verificarMision(Shinji* s, Mision* m, int eleccionActividad, int objetivoPersonaje) {
    if (m->completada) return 0;
    if (eleccionActividad != m->tipo) return 0;
    if (m->tipo == 3) { // socializar
        if (objetivoPersonaje == m->personajeIndex) return 1;
        else return 0;
    }
    return 1;
}

// Procesar recompensa misión cumplida
void procesarMision(Shinji* s, Mision* m) {
    if (m->completada) return;
    m->completada = 1;
    s->energia += m->recompensaEnergia;
    if(s->energia > 100) s->energia = 100;
    s->afinidad[m->personajeIndex] += m->recompensaAfinidad;
    printf("¡Misión completada! Energía +%d, afinidad con %s +%d\n",
    m->recompensaEnergia, nombresPersonajes[m->personajeIndex], m->recompensaAfinidad);
}

// Realizar actividad y verificar misiones
void realizarActividad(Shinji* s, Mision misiones[], int misionesCount) {
    if (s->energia < 10) {
        printf("\nEstás agotado. Solo podés descansar hoy.\n");
        s->animo += 5;
        s->energia += 15;
        return;
    }

    int opcion;
    printf("\nElige tu actividad del día:\n");
    printf("1. Entrenar (fuerza+5, energía-10)\n");
    printf("2. Estudiar (inteligencia+5, energía-10)\n");
    printf("3. Descansar (ánimo+5, energía+10)\n");
    printf("4. Sincronizar con EVA (sincronización+5, energía-15)\n");
    printf("5. Socializar (afinidad+7, energía-5)\n> ");
    scanf("%d", &opcion);

    int objetivoSocial = -1;

    switch (opcion) {
        case 1:
            s->fuerza += 5;
            s->energia -= 10;
            s->confianza += 2;
            break;
        case 2:
            s->inteligencia += 5;
            s->energia -= 10;
            break;
        case 3:
            s->animo += 5;
            s->energia += 10;
            break;
        case 4:
            s->sincronizacion += 5;
            s->energia -= 15;
            break;
        case 5:
            printf("¿Con quién querés hablar hoy?\n");
            for (int i = 0; i < PERSONAJES; i++) {
                printf("%d. %s\n", i + 1, nombresPersonajes[i]);
            }
            printf("> ");
            scanf("%d", &objetivoSocial);
            if (objetivoSocial >= 1 && objetivoSocial <= PERSONAJES) {
                objetivoSocial -= 1;
                s->afinidad[objetivoSocial] += 7;
                s->animo += 2;
                s->energia -= 5;
                s->confianza += 1;
                printf("Pasaste un rato agradable con %s.\n", nombresPersonajes[objetivoSocial]);
            } else {
                printf("No hablaste con nadie hoy.\n");
                objetivoSocial = -1;
            }
            break;
        default:
            printf("Te quedaste en tu cuarto escuchando música...\n");
            s->animo -= 2;
            s->energia -= 5;
            break;
    }

    // Revisar misiones cumplidas
    for (int i = 0; i < misionesCount; i++) {
        if (!misiones[i].completada) {
            if (verificarMision(s, &misiones[i], opcion, objetivoSocial)) {
                procesarMision(s, &misiones[i]);
            }
        }
    }
}

// Mostrar logros desbloqueados
void mostrarLogros(Logro logros[], int logrosCount) {
    if (logrosCount == 0) {
        printf("No has desbloqueado logros.\n");
        return;
    }
    for (int i = 0; i < logrosCount; i++) {
        if (logros[i].desbloqueado) {
            printf(" - %s: %s\n", logros[i].nombre, logros[i].descripcion);
        }
    }
}

// Añadir un logro si no existe ya
void desbloquearLogro(Logro logros[], int* logrosCount, const char* nombre, const char* descripcion) {
    for (int i = 0; i < *logrosCount; i++) {
        if (strcmp(logros[i].nombre, nombre) == 0) {
            if (!logros[i].desbloqueado) {
                logros[i].desbloqueado = 1;
                printf("\n¡Nuevo logro desbloqueado! %s\n", nombre);
            }
            return;
        }
    }
    // Nuevo logro
    if (*logrosCount < MAX_LOGROS) {
        strncpy(logros[*logrosCount].nombre, nombre, 30);
        strncpy(logros[*logrosCount].descripcion, descripcion, 100);
        logros[*logrosCount].desbloqueado = 1;
        (*logrosCount)++;
        printf("\n¡Nuevo logro desbloqueado! %s\n", nombre);
    }
}

// Evento mental - "combate interno"
void eventoMental(Shinji* s) {
    int eleccion;
    printf("\nEstás en una crisis mental. Elige cómo reaccionar:\n");
    printf("1. Enfrentar tus miedos con valentía.\n");
    printf("2. Escapar y evitar el problema.\n");
    printf("3. Buscar ayuda en tus amigos.\n> ");
    scanf("%d", &eleccion);

    switch (eleccion) {
        case 1:
            printf("Enfrentaste tus miedos y te fortaleciste.\n");
            s->confianza += 10;
            s->animo += 15;
            s->cobardia -= 5;
            break;
        case 2:
            printf("Evitaste la situación, pero te sentís peor.\n");
            s->animo -= 10;
            s->cobardia += 10;
            break;
        case 3:
            printf("Tus amigos te apoyan y te sientes mejor.\n");
            s->animo += 10;
            s->confianza += 5;
            break;
        default:
            printf("No hiciste nada. La crisis empeora.\n");
            s->animo -= 15;
            s->cobardia += 10;
            break;
    }

    if (s->animo > 100) s->animo = 100;
    if (s->animo < 0) s->animo = 0;
    if (s->cobardia < 0) s->cobardia = 0;
    if (s->confianza < 0) s->confianza = 0;
}

// Menú principal cada día
void menuPrincipal(Shinji* s, Mision misiones[], int* misionesCount, Logro logros[], int* logrosCount, Cita citas[], int* citasCount, int dia) {
    int opcion;
    do {
        printf("\n--- Menú Principal ---\n");
        printf("1. Realizar actividad diaria\n");
        printf("2. Ver misiones activas\n");
        printf("3. Ver logros\n");
        printf("4. Ver citas disponibles\n");
        printf("5. Guardar partida\n");
        printf("6. Salir del día\n> ");

        scanf("%d", &opcion);
        getchar(); // Limpiar buffer

switch (opcion) {
        case 1:
            realizarActividad(s, misiones, *misionesCount);
        break;
        case 2:
            mostrarMisiones(misiones, *misionesCount);
            break;
        case 3:
            mostrarLogros(logros, *logrosCount);
            break;
        case 4:
            mostrarCitasDisponibles(citas, *citasCount, dia);
            break;
        case 5:
            guardarPartida(*s, misiones, *misionesCount, logros, *logrosCount, citas, *citasCount);
            break;
        case 6:
            printf("Terminando el día %d...\n", dia);
            break;
        default:
            printf("Opción inválida. Intenta de nuevo.\n");
        break;
        }
    } while (opcion != 6);
}

// Inicializar cita
void inicializarCita(Cita* c, int personajeIndex, int dia) {
    c->personajeIndex = personajeIndex;
    c->diaDisponible = dia;
    c->fueVisitada = 0;
}

// Mostrar citas disponibles para el día
void mostrarCitasDisponibles(Cita citas[], int citasCount, int dia) {
    int disponibles = 0;
    printf("\nCitas disponibles hoy:\n");
    for (int i = 0; i < citasCount; i++) {
        if (!citas[i].fueVisitada && citas[i].diaDisponible == dia) {
            printf("%d. %s\n", i + 1, nombresPersonajes[citas[i].personajeIndex]);
            disponibles++;
        }
    }
    if (disponibles == 0) {
        printf("No hay citas disponibles hoy.\n");
        return;
    }
    printf("¿Querés visitar alguna cita? Ingresa número o 0 para salir:\n> ");
    int eleccion;
    scanf("%d", &eleccion);
    getchar(); // limpiar buffer
    if (eleccion > 0 && eleccion <= citasCount) {
        if (!citas[eleccion -1].fueVisitada && citas[eleccion -1].diaDisponible == dia) {
            visitarCita(NULL, &citas[eleccion -1], NULL, NULL);
        } else {
            printf("Esa cita no está disponible o ya fue visitada.\n");
        }
    }
}

// Visitar cita
void visitarCita(Shinji* s, Cita* c, Logro logros[], int* logrosCount) {
    printf("\nEstás en una cita con %s.\n", nombresPersonajes[c->personajeIndex]);
    c->fueVisitada = 1;

    // Evento sencillo de ejemplo
    switch (c->personajeIndex) {
        case 0: // Asuka
            printf("Asuka: \"¡No te me duermas! Hoy vamos a entrenar duro.\"\n");
            if (s != NULL) {
                s->fuerza += 10;
                s->afinidad[0] += 15;
                s->energia -= 15;
                printf("Ganas fuerza y afinidad con Asuka, pero pierdes algo de energía.\n");
            }
            break;
        case 1: // Rei
            printf("Rei: \"Gracias por acompañarme.\"\n");
            if (s != NULL) {
                s->afinidad[1] += 20;
                s->animo += 10;
                printf("Tu relación con Rei mejora mucho y te sientes mejor.\n");
            }
            break;
        case 3: // Kaworu
            printf("Kaworu: \"La música nos une, ¿no?\"\n");
            if (s != NULL) {
                s->afinidad[3] += 25;
                s->animo += 15;
                s->confianza += 5;
                printf("Una cita especial con Kaworu que mejora tu ánimo y confianza.\n");
            }
            break;
        default:
            printf("%s y vos tienen una conversación agradable.\n", nombresPersonajes[c->personajeIndex]);
            if (s != NULL) {
                s->afinidad[c->personajeIndex] += 10;
                s->animo += 5;
                printf("Afinidad y ánimo aumentan ligeramente.\n");
            }
            break;
    }

    if (s != NULL && logros != NULL && logrosCount != NULL) {
        // Por ejemplo, logro si afinidad alta con Rei y Kaworu (final secreto)
        if (s->afinidad[1] > 50 && s->afinidad[3] > 50 && s->sincronizacion > 70) {
            desbloquearLogro(logros, logrosCount, "Final Secreto", "Has desbloqueado el final secreto al conectar profundamente con Rei y Kaworu.");
        }
    }
}

// Chequear logros según stats
void chequearLogros(Shinji s, Logro logros[], int* logrosCount) {
    if (s.sincronizacion >= 80)
        desbloquearLogro(logros, logrosCount, "Sincronización Máxima", "Has alcanzado una sincronización máxima con el EVA.");
    if (s.fuerza >= 70)
        desbloquearLogro(logros, logrosCount, "Fuerza de Titán", "Entrenaste tu cuerpo hasta niveles increíbles.");
    if (s.inteligencia >= 70)
        desbloquearLogro(logros, logrosCount, "Mente Brillante", "Tus estudios te han convertido en alguien muy inteligente.");
    if (s.animo >= 80)
        desbloquearLogro(logros, logrosCount, "Ánimo Inquebrantable", "Mantienes un ánimo positivo y fuerte.");
    if (s.confianza >= 50)
        desbloquearLogro(logros, logrosCount, "Seguro de Sí Mismo", "Has desarrollado una gran confianza en vos mismo.");
    if (s.cobardia >= 50)
        desbloquearLogro(logros, logrosCount, "Miedo Profundo", "Tus miedos te han superado en algunos momentos.");
    if (s.rebeldia >= 50)
        desbloquearLogro(logros, logrosCount, "Rebelde Imparable", "Has desafiado a la autoridad en varias ocasiones.");
}

// Calcular y mostrar final según estadísticas
void calcularFinal(Shinji s, Logro logros[], int logrosCount) {
    printf("\n=== RESULTADO FINAL ===\n");

    int maxAfinidad = 0, mejorAmigo = -1;
    for (int i = 0; i < PERSONAJES; i++) {
        if (s.afinidad[i] > maxAfinidad) {
            maxAfinidad = s.afinidad[i];
            mejorAmigo = i;
        }
    }

    if (s.sincronizacion >= 70 && s.cobardia > 30) {
        printf("Final: Shinji se sacrifica con sincronización total... pero pierde su identidad.\n");
    } else if (s.confianza > 40 && mejorAmigo != -1) {
        printf("Final: Shinji desarrolla una fuerte relación con %s y se convierte en alguien seguro de sí.\n", nombresPersonajes[mejorAmigo]);
    } else if (s.rebeldia > 40) {
        printf("Final: Shinji desafía a NERV y escapa con ayuda de Kaworu y Misato.\n");
    } else if (s.cobardia > 50) {
        printf("Final: Shinji colapsa emocionalmente y se niega a pilotar el EVA.\n");
    } else {
        printf("Final: Shinji sobrevive en Tokio-3, sin destacar pero sin rendirse.\n");
    }

    // Mostrar logros al final
    printf("\nTus logros al finalizar:\n");
    mostrarLogros(logros, logrosCount);
}

// Cargar partida
void guardarPartida(Shinji s, Mision misiones[], int misionesCount, Logro logros[], int logrosCount, Cita citas[], int citasCount) {
    FILE* f = fopen("partida_guardada.dat", "wb");
    if (!f) {
        printf("Error al guardar la partida.\n");
        return;
    }

    fwrite(&s, sizeof(Shinji), 1, f);
    fwrite(&misionesCount, sizeof(int), 1, f);
    fwrite(misiones, sizeof(Mision), misionesCount, f);
    fwrite(&logrosCount, sizeof(int), 1, f);
    fwrite(logros, sizeof(Logro), logrosCount, f);
    fwrite(&citasCount, sizeof(int), 1, f);
    fwrite(citas, sizeof(Cita), citasCount, f);

    fclose(f);
    printf("✅ Partida guardada con éxito.\n");
}

int cargarPartida(Shinji* s, Mision misiones[], int* misionesCount, Logro logros[], int* logrosCount, Cita citas[], int* citasCount) {
    FILE* f = fopen("partida_guardada.dat", "rb");
    if (!f) {
        printf("No se encontró una partida guardada.\n");
        return 0;
    }

    fread(s, sizeof(Shinji), 1, f);
    fread(misionesCount, sizeof(int), 1, f);
    fread(misiones, sizeof(Mision), *misionesCount, f);
    fread(logrosCount, sizeof(int), 1, f);
    fread(logros, sizeof(Logro), *logrosCount, f);
    fread(citasCount, sizeof(int), 1, f);
    fread(citas, sizeof(Cita), *citasCount, f);

    fclose(f);
    printf("✅ Partida cargada con éxito.\n");
    return 1;
}
