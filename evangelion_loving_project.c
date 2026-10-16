/*
 * ============================================================================
 *  EVANGELION: PROYECTO DE CRIANZA
 *  RPG de vínculos / gestión de condiciones — 30 días en Tokio-3
 *
 *  Un solo archivo, sin dependencias. Todo el juego (datos, guardado,
 *  guardado de partida) vive acá.
 *
 *  COMPILAR (Windows):
 *      gcc -std=c11 -O2 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 \
 *          -o "Evangelion Loving Project.exe" evangelion_loving_project.c
 *
 *  COMPILAR (Linux/macOS):
 *      gcc -std=c11 -O2 -Wall -Wextra -o evangelion_loving_project evangelion_loving_project.c
 *
 *  ARCHIVOS QUE ESCRIBE (junto al .exe, no en el directorio de trabajo):
 *      partida_1.dat .. partida_3.dat   tres partidas en paralelo
 *      finales.dat                       máscara de los 8 finales ya vistos
 *
 *  PARA JUGAR: elegís una actividad por día y una sola. La misión del día dice
 *  cuál. Los vínculos van de -100 a +100: no todo el mundo mejora, y las
 *  decisiones de ayer se notan en los eventos de mañana.
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* -------------------------------------------------------------------------- */
/* Constantes                                                                 */
/* -------------------------------------------------------------------------- */

#define DIAS_TOTALES      30
#define PERSONAJES         8      /* ya no te puedes "socializar" contigo mismo */
#define MAX_MISIONES       3      /* activas en paralelo                     */
#define MAX_LOGROS        16
#define MAX_CITAS          9
#define MAX_DIARIO       30      /* un registro por día, los 30 días          */
#define NOTA_DIARIO      192
#define ENERGIA_MAX      100
#define COMBATE_RONDAS     8      /* tope duro: el combate siempre termina   */

/*
 * Vida de cada ángel.
 *
 * El daño por golpe es 5 + rand() % (6 + sincronizacion/12), y las rondas están
 * topadas en 8. Con poca sincronización la probabilidad de acierto ronda el
 * 40%, así que en una partida completa entran unos 3 golpes: unos 25 de daño.
 * Estos valores están calculados sobre eso, no sobre una build ideal: el primero
 * se gana con lo que tienes el día cinco, y el último exige llegar preparado.
 * Con 95 de vida el jefe final era inbatible salvo con tiradas perfectas.
 */
#define VIDA_ACUIFEROS  24
#define VIDA_SEMEJANTE  48
#define VIDA_EMBLEM     70

#define NOMBRE_PROYECTO "Evangelion: Proyecto de Crianza"
#define MAX_RANURAS      3      /* partidas en paralelo                     */

/* Índices de personaje. El orden es parte del formato de guardado. */
enum {
    P_ASUKA = 0,
    P_REI,
    P_MISATO,
    P_KAWORU,
    P_MARI,
    P_MANA,
    P_RITSUKO,
    P_MAYA
};

/* -------------------------------------------------------------------------- */
/* Actividades: un único enum para el menu y para las misiones. El menu está   */
/* ordenado exactamente igual que este enum, así que el número que elige el   */
/* jugador coincide con el tipo de la misión. Esto elimina el desajuste que   */
/* hacia que algunas misiones fueran imposibles de completar.                  */
/* -------------------------------------------------------------------------- */
typedef enum {
    ACT_ENTRENAR   = 1,
    ACT_ESTUDIAR   = 2,
    ACT_SOCIALIZAR = 3,
    ACT_DESCANSAR  = 4,
    ACT_SINCRO     = 5
} Actividad;

/* -------------------------------------------------------------------------- */
/* Etapas de relacion. La afinidad va de -100 a +100: no todo vínculo crece    */
/* para arriba. HOSTIL y EVITADO hacen que un personaje deje de estar          */
/* disponible para ciertos eventos, y eso es una pérdida real.                 */
/* -------------------------------------------------------------------------- */
typedef enum {
    ETAP_HOSTIL       = 0,   /* <= -30 */
    ETAP_EVITADO      = 1,   /* -29 .. -10 */
    ETAP_DESCONOCIDO  = 2,   /*  -9 ..   9 */
    ETAP_CONOCIDO     = 3,   /*  10 ..  24 */
    ETAP_AMIABLE      = 4,   /*  25 ..  49 */
    ETAP_CERCANO      = 5,   /*  50 ..  74 */
    ETAP_UNIDO        = 6    /*  75 .. 100 */
} Etapa;

#define ETAPAS        (ETAP_UNIDO + 1)
#define AFINIDAD_MAX  100
#define AFINIDAD_MIN (-100)

static const char* nombresEtapa[ETAPAS] = {
    "HOSTIL", "EVITADO", "DESCONOCIDO", "CONOCIDO", "AMIABLE", "CERCANO", "UNIDO"
};

/* -------------------------------------------------------------------------- */
/* Tipos                                                                      */
/* -------------------------------------------------------------------------- */

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
    char      descripcion[100];
    int       personajeIndex;   /* -1 si la mision no va con nadie */
    int       tipo;             /* Actividad, guardado como int por portabilidad */
    int       completada;
    int       recompensaEnergia;
    int       recompensaAfinidad;
    int       diaAsignada;
} Mision;

typedef struct {
    char nombre[30];
    char descripcion[100];
    int  desbloqueado;
} Logro;

typedef struct {
    int personajeIndex;
    int diaDisponible;
    int fueVisitada;
    int variante;            /* segunda cita con el mismo personaje */
} Cita;

/*
 * Diario: un renglón por día. No guarda el texto de los eventos (eso pesa y
 * no hace falta) sino un resumen de lo que pasó y cómo quedaron los vínculos
 * al cerrar el día. Es lo que hace legible el sistema de memoria: sin esto,
 * que Rei te evite el día 20 porque le diste el abrigo el 8 es indistinguible
 * de que simplemente nunca le caíste bien.
 */
typedef struct {
    int  dia;
    char notas[NOTA_DIARIO];
    int  vinculo[PERSONAJES];
} EntradaDiario;

/*
 * Memoria: lo que el mundo recuerda de ti, personaje por personaje.
 * Antes los eventos se disparaban siempre, así que rechazar a alguien no
 * tenía ninguna consecuencia. Con esto, el mundo reacciona.
 */
typedef struct {
    unsigned char dio_abrigo;       /* dia 8:  le diste tu abrigo a Rei        */
    unsigned char rechazo_escucha;  /* dia 10: le dijiste que no a Rei        */
    unsigned char le_mintiste;      /* aceptaste cuando no debias             */
    unsigned char veces_rechazada;  /* acumulado                              */
    unsigned char veces_ayudada;    /* veces que le diste una mano            */
    unsigned char le_confio;        /* le contaste algo tuyo                  */
} Memoria;

typedef struct {
    int  dia;                        /* 1 .. DIAS_TOTALES                     */
    Shinji shinji;
    Mision misiones[MAX_MISIONES];
    int  misionesCount;
    Logro logros[MAX_LOGROS];
    int  logrosCount;
    Cita citas[MAX_CITAS];
    int  citasCount;
    int  etapaVista[PERSONAJES];    /* ultima etapa cuyo texto ya se mostro  */
    Memoria memoria[PERSONAJES];     /* lo que cada uno recuerda de ti          */
    int  actividadHecha;             /* una sola por dia                      */
    int  crisisCount;
    int  misionesCompletadas;
    int  citasCompletadas;
    int  combatesGanados;
    int  combatesPerdidos;
    int  vecesMentiste;              /* decisiones con costo de confianza     */
    EntradaDiario diario[MAX_DIARIO];
    int  diarioCount;
    int  ultimaCrisis;              /* día de la última crisis: no van juntas */
} GameState;

/* -------------------------------------------------------------------------- */
/* Contenido: perfiles de personaje                                          */
/* -------------------------------------------------------------------------- */

typedef struct {
    const char* nombre;
    const char* rol;
    const char* ocio[ETAPAS];      /* reaccion al pasar el rato, por etapa     */
    const char* aviso[ETAPAS];     /* texto al ENTRAR a esa etapa, idem abajo  */
} Perfil;

static const Perfil perfiles[PERSONAJES] = {
/* --- Asuka: nunca pide permiso, y insulta antes de hablarte --- */
{ "Asuka", "Compiladora del EVA-02",
  { "Asuka no te mira. Dice \"¿Qué?\" y sigue con el ejercicio.",
    "Te contesta sin levantar la vista. Le da igual que estés ahí.",
    "Entrena sin hablar. A veces te mira de reojo y aparta la vista.",
    "\"Bueno, al menos no estorbas.\" Y casi te guiña.",
    "Se sienta a tu lado sin que se lo pidas. Mira el atardecer.",
    "\"Sí le contas esto a alguien, no te hablo más.\" Y no lo haría ni de coña.",
    "Se planta en medio del gym delante de todos. Esa máquina es suya." },
  { "Asuka no te contesta. Se da la vuelta y sigue con lo suyo.",
    "",
    "",
    "Te tira una botella de agua sin mirarte. Dice que se le cayó. Y no lo cuenta.",
    "Empieza a preguntarte qué piensas del EVA. Es la primera vez que te trata como a alguien.",
    "\"Cuenta hasta tres y después elegimos.\" Lo eligió ella, no yo.",
    "Me escribe a las tres de la mañana. Dice que no podía dormir. No pregunta nada." } },

/* --- Rei: habla en fragmentos. Es lo que hace. --- */
{ "Rei", "Piloto sustituta del EVA-00",
  { "Te mira. Se da la vuelta. No dice nada.",
    "Rei asiente sin mirarte. Es peor que el silencio.",
    "Rei asiente. Sigue caminando. Eso es toda la conversación.",
    "\"Entiendo.\" Lo dice tres veces. Es lo más que se le arranca en semanas.",
    "Rei se queda quieto a tu lado. El silencio, por una vez, está bien.",
    "Rei te espera al final del pasillo. Nadie se lo pidió.",
    "Rei dice tu nombre. Solo tu nombre. Lo repite para ver cómo suena." },
  { "Rei ya no te habla. No es una decisión. Es una ausencia.",
    "Rei contesta lo que le dices. Contesta al aire.",
    "",
    "Le dices mal tu nombre una vez. No te corrige. Lo repite bien.",
    "Le preguntaron a quién quería hablar. Rei dijo tu nombre.",
    "Rei se sienta a dormir junto a tu habitación. Dice que hace frío.",
    "Rei se ofrece a sincronizar contigo. No pregunta: lo anuncia." } },

/* --- Misato: brusca, cortante, nunca termina la frase --- */
{ "Misato", "Comandante de la Quinta Calle",
  { "Pregunta tu nombre. Es la primera vez en semanas. Y no hay nada detrás.",
    "Te da una orden y ninguna explicación. No piensa explicarte.",
    "Tiene el auricular puesto. Te grita que esperes sin sacar los ojos de la pantalla.",
    "Te contesta a monosílabos. Le queda bien, como si fuera su estilo.",
    "Te sirve un café sin que lo pidas. Y lo hace todos los días desde hace semanas.",
    "Te llama por el apodo de la escuela. No lo usa con nadie más.",
    "Te llama \"mi responsabilidad\". Y eso, en su idioma, es lo más frío que dijo." },
  { "Misato ya no te habla. Su departamento no tiene tu nombre.",
    "Responde por procedimiento. Tú eres un expediente con piernas.",
    "",
    "Se acuerda de un apodo que usabas de chico. No se lo había dicho nadie en años.",
    "Me incluye en una reunión de NERV aunque no hacía falta. Le resulta interesante.",
    "Me cuenta por qué no fue a buscarme aquel día. Sabe lo que le cuesta, y me lo dice igual.",
    "Me deja una llave del archivo de los EVA. \"Por si acaso.\" Ella nunca dice por si acaso." } },

/* --- Kaworu: habla como quien ya sabe cómo termina esto --- */
{ "Kaworu", "El quinto niño",
  { "Kaworu te sonríe. Es la sonrisa de un cartel. No hay nadie detrás.",
    "Te responde por cortesía. Contesta como contesta un mostrador.",
    "Te sonríe como si se conocieran de antes.",
    "Te habla. Y parece que le gusto de verdad la conversación.",
    "Se acuerda de cosas que nunca le contaste.",
    "Te espera donde no le pediste que te esperara.",
    "Deja de sonreír. Dice que por fin está hablando con alguien de verdad." },
  { "Se va en silencio. Ni siquiera cierra la puerta. No hacía falta.",
    "Te habla por cortesía. Y se nota. Y a él también.",
    "",
    "Sabe tu nombre aunque nunca se lo dijiste.",
    "Me escribe una carta. El papel no existe en el catálogo de NERV.",
    "Me toca el hombro para despedirse. Es el primer contacto que inicia él.",
    "Me entrega una pieza del EVA que no figura en ningún inventario. Nadie más sabe que la tengo." } },

/* --- Mari ----------------------------------------------------------- */
{ "Mari", "Supervisora de la red eléctrica",
  { "Mari te mira y decide que no le caes. Y te lo dice con una sonrisa.",
    "Mari te contesta por educación. Sabe exactamente cuánto mide esa educación.",
    "Mari te pide que le lleves un café. Y te mira igual.",
    "Mari te agradece. Mari no agradece. Que lo esté haciendo ya es mucho.",
    "Mari se ríe de tu chiste antes que el resto, y le gusta que lo cuentes.",
    "Mari se queda a dormir en tu casa \"porque era tarde\".",
    "Mari te lo dice en la cara: \"estuviste insoportable, no te quiero ver más\"." },
  { "Mari no responde los mensajes. Un día se lo toma de forma personal.",
    "Mari te responde \"ya voy\" y no va.",
    "",
    "Mari te escribe el nombre de un grupo y te manda la primera canción.",
    "Mari te deja un disco prestado. Dice que no es nada. Te pone la misma canción toda la semana.",
    "Mari te pide que vengas a un show. No dice de qué grupo. Sabe que ya vas a ir.",
    "Mari te presenta ante el resto como \"Shinji-kun\". Le importas como persona." } },

/* --- Mana ----------------------------------------------------------- */
{ "Mana", "compañera de clase (2-A)",
  { "Mana deja de hablarte delante de todos. Y hace ruido al hacerlo.",
    "Mana te contesta por cortesía. Se le nota y se sabe que se le nota.",
    "Mana te habla de las cosas que le gustan. Te está probando, no es casual.",
    "Mana te busca para hablar de cosas sin importancia. Se le da muy bien.",
    "Mana se queda a comer contigo en el patio todos los fines de semana.",
    "Mana te espera en la puerta del aula aunque no tengas clase con ella.",
    "Mana deja de decir que eres su mejor amigo. Empieza a decir \"Shinji-kun\"." },
  { "Mana se corrige cuando dice tu nombre. Es la primera vez que se corrige.",
    "Mana dice tu nombre corto, como si fuera de otra persona.",
    "",
    "Mana le dice a todo el curso que tú eres su mejor amigo.",
    "Mana te espera en la puerta del aula aunque no tengas clase con ella.",
    "Mana te escribe un cuento cutre contigo de protagonista y lo lee en voz alta en clase.",
    "Mana dice que le SUBE la presión lo que te pase a ti, y le da igual, y te lo dice miraándote." } },

/* --- Ritsuko -------------------------------------------------------- */
{ "Ritsuko", "Analista de datos de NERV",
  { "Ritsuko cierra la puerta antes de que entres. No dice por qué.",
    "Ritsuko responde lo que le preguntas y anota algo. Tú figuras en el.",
    "Ritsuko te mira un segundo de más mientras habla de porcentajes.",
    "Ritsuko te explica un dato que nadie le pidió. Le gusta explicar.",
    "Ritsuko se ríe de sus propios chistes antes de terminar la frase.",
    "Ritsuko te escribe todas las noches para contarte como va el mapa en pantalla.",
    "Ritsuko entrega tu expediente con una nota al margen que no explica. Y no te la explica." },
  { "Ritsuko te contesta con el protocolo entero. Palabra por palabra.",
    "Ritsuko responde a tus mensajes con \"recibido\".",
    "",
    "Ritsuko te pide que le ayudes a contar datos un domingo a la tarde.",
    "Ritsuko te enseña una pantalla que no debería enseñarte. Y te explica por qué puede hacerlo.",
    "Ritsuko te confiesa que tiene miedo. Contigo, específicamente, tiene miedo.",
    "Ritsuko rompe el protocolo por ti, y te lo cuenta antes de que la descubran." } },

{ "Maya", "Médica de la guardia de tierra",
  { "Maya no te recibe. Dice que no tiene turno. Es mentira y ambos lo saben.",
    "Maya te atiende sin mirarte y anota todo. La agenda dice que no sería nada de esto.",
    "Maya te entrega el informe con una mirada que dice que no deberías estar ahí.",
    "Maya te pregunta cómo duermes. Y vuelve a preguntar en la segunda consulta.",
    "Maya te deja un té en la mesa sin decir nada, y después lo niega todo.",
    "Maya te recuerda un cumpleaños que ni se te había olvidado que existía.",
    "Maya te da el alta y anota de tu expediente que no vas a hablar con nadie." },
  { "Maya no toma tus llamadas. En el informe escribió: \"contacto no posible\".",
    "Maya te contesta lo justo y mira el reloj mientras lo hace.",
    "",
    "Maya te reconoce de nombre en la lista de riesgo.",
    "Maya te saca la medicación de un golpe y te explica por qué: \"no la necesitas\".",
    "Maya te escribe para preguntarte cómo vas. Todos los días. Sin falta.",
    "Maya te acompaña a despertarte cuando entras en crisis, y se queda hasta que te duermes." } }
};

/* -------------------------------------------------------------------------- */
/* Contenido: arcos narrativos                                                */
/* -------------------------------------------------------------------------- */

typedef enum {
    ARCO_1 = 1,   /* 1 - 8   Asentamiento */
    ARCO_2,       /* 9 - 16  Amenaza      */
    ARCO_3,       /* 17 - 23 Combate      */
    ARCO_4        /* 24 - 30 Desenlace    */
} Arco;

static const char* nombreArco(Arco a) {
    switch (a) {
        case ARCO_1: return "ASENTAMIENTO";
        case ARCO_2: return "AMENAZA";
        case ARCO_3: return "COMBATE";
        default:     return "DESENLACE";
    }
}

static Arco arcoDelDia(int dia) {
    if (dia <= 8)  return ARCO_1;
    if (dia <= 16) return ARCO_2;
    if (dia <= 23) return ARCO_3;
    return ARCO_4;
}

/* -------------------------------------------------------------------------- */
/* Consola / IO                                                               */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* Presentación: color, cajas y barras                                        */
/* -------------------------------------------------------------------------- */

#define C_RESET "\x1b[0m"
#define C_DIM   "\x1b[2m"
#define C_BOLD  "\x1b[1m"
#define C_RED   "\x1b[31m"
#define C_GREEN "\x1b[32m"
#define C_YELL  "\x1b[33m"
#define C_BLUE  "\x1b[34m"
#define C_CYAN  "\x1b[36m"
#define C_GREY  "\x1b[90m"

/* El jugador puede apagar el color escribiendo 'c' en cualquier prompt. */
static int g_color = 1;

static const char* cc(const char* codigo) { return g_color ? codigo : ""; }
#define RST cc(C_RESET)

static void prepararConsola(void) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    /* Habilita secuencias ANSI en consolas antiguas de Windows. */
    DWORD modo = 0;
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetConsoleMode(h, &modo)) SetConsoleMode(h, modo | 0x0004);
#endif
}

static void limpiarPantalla(void) {
    printf("\x1b[2J\x1b[H");
}

/*
 * Ancho en pantalla de un texto UTF-8: cuenta caracteres, no bytes.
 * Sin esto, "días" mide 5 en strlen pero ocupa 4 columnas, y las cajas
 * quedan torcidas.
 */
static int anchoVisual(const char* s) {
    int n = 0;
    for (const unsigned char* p = (const unsigned char*)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;   /* no cuenta la continuación */
    return n;
}

/* Encabezado de pantalla: título opcional y subtítulo opcional. */
static void caja(const char* titulo, const char* subtitulo) {
    const int ANCHO = 62;
    printf("\n%s  +", cc(C_BLUE));
    for (int i = 0; i < ANCHO; i++) putchar('-');
    printf("+%s\n", RST);
    if (titulo) {
        int usado = anchoVisual(titulo);
        printf("%s  |%s  %s%s%s", cc(C_BLUE), RST, cc(C_BOLD), titulo, RST);
        for (int i = 0; i < ANCHO - usado - 2; i++) putchar(' ');
        printf("%s|%s\n", cc(C_BLUE), RST);
    }
    if (subtitulo) {
        int usado = anchoVisual(subtitulo);
        printf("%s  |%s  %s%s%s", cc(C_BLUE), RST, cc(C_DIM), subtitulo, RST);
        for (int i = 0; i < ANCHO - usado - 2; i++) putchar(' ');
        printf("%s|%s\n", cc(C_BLUE), RST);
    }
    printf("%s  +", cc(C_BLUE));
    for (int i = 0; i < ANCHO; i++) putchar('-');
    printf("+%s\n", RST);
}

static void regla(void) {
    printf("%s  ", cc(C_GREY));
    for (int i = 0; i < 64; i++) putchar('-');
    printf("%s\n", RST);
}

static void seccion(const char* t) {
    printf("\n  %s%s%s\n", cc(C_CYAN), t, RST);
}

/*
 * Lectura robusta de enteros: maneja basura, fuera de rango y EOF.
 * Antes, un scanf fallido podía dejar los menus girando para siempre.
 */
static int leerEntero(const char* prompt, int min, int max) {
    char buf[64];
    for (;;) {
        printf("%s", prompt);
        fflush(stdout);
        if (!fgets(buf, sizeof buf, stdin)) {
            printf("\n[Fin de la entrada]\n");
            return -1;
        }
        char* p = buf;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '\n') continue;

        /* 'c' apaga o enciende el color, en cualquier prompt. */
        if ((p[0] == 'c' || p[0] == 'C') &&
            (p[1] == '\0' || p[1] == '\n' || p[1] == '\r')) {
            g_color = !g_color;
            printf("  %scolor %s%s\n", cc(C_GREY),
                   g_color ? "activado" : "apagado", RST);
            continue;
        }

        char* end = NULL;
        long v = strtol(p, &end, 10);
        while (*end == ' ' || *end == '\t') end++;
        if (*end != '\0' && *end != '\n') {
            printf("Eso no es un número.\n");
            continue;
        }
        if (v < min || v > max) {
            printf("Fuera de rango. Va de %d a %d.\n", min, max);
            continue;
        }
        return (int)v;
    }
}

static int confirmar(const char* prompt) {
    char buf[16];
    for (;;) {
        printf("%s (s/n) > ", prompt);
        fflush(stdout);
        if (!fgets(buf, sizeof buf, stdin)) return 0;
        if (buf[0] == 's' || buf[0] == 'S') return 1;
        if (buf[0] == 'n' || buf[0] == 'N') return 0;
        printf("Escribi s o n.\n");
    }
}

static void pausa(void) {
    char buf[64];
    printf("\n  %s▸ Enter%s", cc(C_GREY), RST);
    fflush(stdout);
    if (!fgets(buf, sizeof buf, stdin)) exit(0);
}

static int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/*
 * Rendimientos decrecientes: sin esto, enfocarte en una sola stat te la lleva
 * al techo en dos semanas. Cada 25 puntos, el entrenamiento da un punto menos.
 */
static int gananciaConRendimientoDecreciente(int actual, int base) {
    int g = base - actual / 25;
    return g < 1 ? 1 : g;
}

static Etapa etapaDe(int afinidad) {
    if (afinidad <= -30) return ETAP_HOSTIL;
    if (afinidad <= -10) return ETAP_EVITADO;
    if (afinidad <  10)   return ETAP_DESCONOCIDO;
    if (afinidad <  25)   return ETAP_CONOCIDO;
    if (afinidad <  50)   return ETAP_AMIABLE;
    if (afinidad <  75)   return ETAP_CERCANO;
    return ETAP_UNIDO;
}

static void normalizar(Shinji* s) {
    s->fuerza         = clampi(s->fuerza, 0, 100);
    s->inteligencia   = clampi(s->inteligencia, 0, 100);
    s->animo          = clampi(s->animo, 0, 100);
    s->sincronizacion = clampi(s->sincronizacion, 0, 100);
    s->energia        = clampi(s->energia, 0, ENERGIA_MAX);
    s->confianza      = clampi(s->confianza, 0, 100);
    s->cobardia       = clampi(s->cobardia, 0, 100);
    s->rebeldia       = clampi(s->rebeldia, 0, 100);
    for (int i = 0; i < PERSONAJES; i++)
        s->afinidad[i] = clampi(s->afinidad[i], AFINIDAD_MIN, AFINIDAD_MAX);
}

/* -------------------------------------------------------------------------- */
/* Guardado versionado, en la carpeta del ejecutable                          */
/* -------------------------------------------------------------------------- */

#define SAVE_MAGIC   0x324C5645u   /* "EVL2" */
#define SAVE_VERSION 4u

typedef struct {
    unsigned int magic;
    unsigned int version;
    GameState     st;
} SaveFile;

static void rutaSave(char* buf, size_t n, int slot) {
#ifdef _WIN32
    char exe[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, exe, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        char* slash = strrchr(exe, '\\');
        if (slash) {
            *slash = '\0';
            snprintf(buf, n, "%s\\partida_%d.dat", exe, slot);
            return;
        }
    }
#endif
    snprintf(buf, n, "partida_%d.dat", slot);
}

/* Lee solo la cabecera de un slot, para mostrarlo en el menú sin cargarlo. */
static int peekPartida(int slot, int* dia) {
    char ruta[MAX_PATH + 32];
    rutaSave(ruta, sizeof ruta, slot);

    FILE* f = fopen(ruta, "rb");
    if (!f) return 0;

    SaveFile sf;
    size_t leidos = fread(&sf, sizeof sf, 1, f);
    fclose(f);

    if (leidos != 1 || sf.magic != SAVE_MAGIC || sf.version != SAVE_VERSION) return 0;
    if (sf.st.dia < 1 || sf.st.dia > DIAS_TOTALES + 1) return 0;

    *dia = sf.st.dia;
    return 1;
}

static int guardarPartida(const GameState* g, int slot) {
    char ruta[MAX_PATH + 32];
    rutaSave(ruta, sizeof ruta, slot);

    FILE* f = fopen(ruta, "wb");
    if (!f) {
        printf("No se pudo abrir la partida para escribir: %s\n", ruta);
        return 0;
    }
    SaveFile sf;
    sf.magic   = SAVE_MAGIC;
    sf.version = SAVE_VERSION;
    sf.st      = *g;

    size_t ok = fwrite(&sf, sizeof sf, 1, f);
    fclose(f);
    if (ok != 1) {
        printf("Error al escribir la partida.\n");
        return 0;
    }
    printf("  %sPartida guardada · día %d · ranura %d%s\n", cc(C_GREEN),
           g->dia, slot, RST);
    return 1;
}

static int cargarPartida(GameState* g, int slot) {
    char ruta[MAX_PATH + 32];
    rutaSave(ruta, sizeof ruta, slot);

    FILE* f = fopen(ruta, "rb");
    if (!f) {
        printf("No hay partida guardada (%s).\n", ruta);
        return 0;
    }

    SaveFile sf;
    size_t leidos = fread(&sf, sizeof sf, 1, f);
    fclose(f);

    if (leidos != 1 || sf.magic != SAVE_MAGIC) {
        printf("El archivo de partida no es válido. Se ignora.\n");
        return 0;
    }
    if (sf.version != SAVE_VERSION) {
        printf("Partida de versión %u; este build espera la %u. Se ignora.\n",
               sf.version, SAVE_VERSION);
        return 0;
    }

    /* Validación de rangos: un .dat corrupto ya no puede desbordar nada. */
    GameState* v = &sf.st;
    if (v->dia < 1 || v->dia > DIAS_TOTALES + 1 ||
        v->misionesCount < 0 || v->misionesCount > MAX_MISIONES ||
        v->logrosCount   < 0 || v->logrosCount   > MAX_LOGROS ||
        v->citasCount    < 0 || v->citasCount    > MAX_CITAS ||
        v->diarioCount   < 0 || v->diarioCount   > MAX_DIARIO ||
        v->ultimaCrisis  < 0 || v->ultimaCrisis  > DIAS_TOTALES) {
        printf("Partida corrupta (contadores fuera de rango). Se ignora.\n");
        return 0;
    }
    for (int i = 0; i < PERSONAJES; i++) {
        if (v->etapaVista[i] < (int)ETAP_HOSTIL ||
            v->etapaVista[i] > (int)ETAP_UNIDO) {
            printf("Partida corrupta (etapa inválida). Se ignora.\n");
            return 0;
        }
        if (v->shinji.afinidad[i] < AFINIDAD_MIN || v->shinji.afinidad[i] > AFINIDAD_MAX) {
            printf("Partida corrupta (afinidad fuera de rango). Se ignora.\n");
            return 0;
        }
    }
    for (int i = 0; i < v->misionesCount; i++) {
        if (v->misiones[i].personajeIndex < -1 ||
            v->misiones[i].personajeIndex >= PERSONAJES) {
            printf("Partida corrupta (misión inválida). Se ignora.\n");
            return 0;
        }
    }
    for (int i = 0; i < v->citasCount; i++) {
        if (v->citas[i].personajeIndex < 0 ||
            v->citas[i].personajeIndex >= PERSONAJES) {
            printf("Partida corrupta (cita inválida). Se ignora.\n");
            return 0;
        }
    }
    /* El diario se valida aparte: las notas son texto y hay que cortarlas,
       o un .dat con basura termina imprimiendo basura en pantalla. */
    for (int i = 0; i < v->diarioCount; i++) {
        v->diario[i].notas[NOTA_DIARIO - 1] = '\0';
        if (v->diario[i].dia < 1 || v->diario[i].dia > DIAS_TOTALES) {
            printf("Partida corrupta (diario con días imposibles). Se ignora.\n");
            return 0;
        }
        for (int p = 0; p < PERSONAJES; p++) {
            if (v->diario[i].vinculo[p] < AFINIDAD_MIN ||
                v->diario[i].vinculo[p] > AFINIDAD_MAX) {
                printf("Partida corrupta (diario con vínculos fuera de rango). Se ignora.\n");
                return 0;
            }
        }
    }

    *g = sf.st;
    normalizar(&g->shinji);
    return 1;
}

/* -------------------------------------------------------------------------- */
/* Diario                                                                     */
/* -------------------------------------------------------------------------- */

/*
 * Anota algo en el renglón de hoy. Se pueden llamar varias veces por día: las
 * notas se encadenan. Devuelve 0 solo si no hay lugar, que en una partida
 * normal no puede pasar (30 días, 30 renglones) pero no debe romper el juego.
 */
static int anotar(GameState* g, const char* texto) {
    EntradaDiario* e = NULL;

    for (int i = 0; i < g->diarioCount; i++)
        if (g->diario[i].dia == g->dia) { e = &g->diario[i]; break; }

    if (!e) {
        if (g->diarioCount >= MAX_DIARIO) return 0;
        e = &g->diario[g->diarioCount++];
        memset(e, 0, sizeof *e);
        e->dia = g->dia;
        e->notas[0] = '\0';
    }

    size_t usado = strlen(e->notas);
    size_t queda = sizeof e->notas - usado;
    if (queda <= 2) return 0;

    /* Separador entre notas del mismo día. */
    if (usado > 0) {
        e->notas[usado++] = ' ';
        e->notas[usado] = '\0';
        queda--;
    }
    snprintf(e->notas + usado, queda, "%s", texto);
    return 1;
}

/* Se llama al cerrar el día: congela cómo quedaron los vínculos. */
static void cerrarDiaEnDiario(GameState* g) {
    EntradaDiario* e = NULL;
    for (int i = 0; i < g->diarioCount; i++)
        if (g->diario[i].dia == g->dia) { e = &g->diario[i]; break; }

    /* Un día sin nada anotado igual entra: que falte un renglón se lee como
       un bug, y es más fácil de creer que como una decisión. */
    if (!e) {
        if (g->diarioCount >= MAX_DIARIO) return;
        e = &g->diario[g->diarioCount++];
        memset(e, 0, sizeof *e);
        e->dia = g->dia;
        snprintf(e->notas, sizeof e->notas, "Día sin nada que contar.");
    }
    for (int p = 0; p < PERSONAJES; p++)
        e->vinculo[p] = g->shinji.afinidad[p];
}

static void menuDiario(const GameState* g) {
    seccion("DIARIO");

    if (g->diarioCount == 0) {
        printf("  Todavia no hay nada anotado.\n\n");
        return;
    }

    for (int i = 0; i < g->diarioCount; i++) {
        const EntradaDiario* e = &g->diario[i];
        printf("\n  %sDÍA %d%s\n", cc(C_BOLD), e->dia, RST);
        printf("    %s\n", e->notas);

        /* Solo se muestran los vínculos que se movieron: ocho números fijos
           cada día no se leen y tapan lo que importa. */
        char movidos[NOTA_DIARIO];
        movidos[0] = '\0';
        size_t n = 0;
        for (int p = 0; p < PERSONAJES; p++) {
            /* El renglón de hoy todavía no tiene snapshot (se congela al
               cerrar el día), así que el de hoy se lee del estado actual. */
            int actual = (e->dia == g->dia) ? g->shinji.afinidad[p] : e->vinculo[p];
            int antes = (i > 0) ? g->diario[i - 1].vinculo[p] : 0;
            int d = actual - antes;
            if (d == 0) continue;
            int escrito = snprintf(movidos + n, sizeof movidos - n, "%s%s %+d",
                                   n ? ", " : "", perfiles[p].nombre, d);
            if (escrito < 0) break;
            n += (size_t)escrito;
            if (n >= sizeof movidos - 1) break;
        }
        if (movidos[0])
            printf("    %s%s: %s%s\n", cc(C_GREY), "vínculos", movidos, RST);
        else
            printf("    %svínculos: sin cambios%s\n", cc(C_GREY), RST);
    }
    printf("\n");
}

/* -------------------------------------------------------------------------- */
/* Estado inicial                                                             */
/* -------------------------------------------------------------------------- */

static void nuevaPartida(GameState* g) {
    memset(g, 0, sizeof *g);
    g->dia = 1;
    g->shinji.fuerza         = 30;
    g->shinji.inteligencia   = 30;
    g->shinji.animo          = 50;
    g->shinji.sincronizacion = 25;
    g->shinji.energia        = ENERGIA_MAX;

    /* Las seis primeras caen en la primera mitad, que es donde el juego te
       deja conocer gente. Las tres últimas están en el tramo final: con los
       días 27 a 30 sin citas, la semana que paga todo lo anterior llegaba
       sin nadie a quien leerle. */
    static const int diaCita[MAX_CITAS]  = { 5, 10, 15, 18, 22, 26, 27, 28, 30 };
    static const int persCita[MAX_CITAS] = { P_ASUKA, P_REI, P_KAWORU,
                                             P_MISATO, P_MARI, P_RITSUKO,
                                             P_MANA, P_REI, P_ASUKA };
    /* Segunda cita con el mismo personaje: el texto tiene que ser otro, o
       repetir el de la primera se lee como un bug. */
    static const int varCita[MAX_CITAS]  = { 0, 0, 0, 0, 0, 0, 0, 1, 1 };
    for (int i = 0; i < MAX_CITAS; i++) {
        g->citas[i].personajeIndex = persCita[i];
        g->citas[i].diaDisponible  = diaCita[i];
        g->citas[i].fueVisitada    = 0;
        g->citas[i].variante       = varCita[i];
    }
    g->citasCount = MAX_CITAS;
}

/* -------------------------------------------------------------------------- */
/* Logros                                                                     */
/* -------------------------------------------------------------------------- */

static void desbloquearLogro(GameState* g, const char* nombre, const char* desc) {
    for (int i = 0; i < g->logrosCount; i++) {
        if (strcmp(g->logros[i].nombre, nombre) == 0) {
            if (!g->logros[i].desbloqueado) {
                g->logros[i].desbloqueado = 1;
                printf("\n>> LOGRO DESBLOQUEADO: %s\n   %s\n", nombre, desc);
            }
            return;
        }
    }
    if (g->logrosCount >= MAX_LOGROS) return;

    Logro* l = &g->logros[g->logrosCount++];
    memset(l, 0, sizeof *l);
    strncpy(l->nombre, nombre, sizeof l->nombre - 1);
    strncpy(l->descripcion, desc, sizeof l->descripcion - 1);
    l->desbloqueado = 1;
    printf("\n>> LOGRO DESBLOQUEADO: %s\n   %s\n", nombre, desc);
}

static void mostrarLogros(const GameState* g, int soloDesbloqueados) {
    if (g->logrosCount == 0) {
        printf("todavía no desbloqueaste nada.\n");
        return;
    }
    int n = 0;
    for (int i = 0; i < g->logrosCount; i++)
        if (g->logros[i].desbloqueado) n++;

    if (soloDesbloqueados && n == 0) {
        printf("No has desbloqueado logros aún.\n");
        return;
    }
    for (int i = 0; i < g->logrosCount; i++) {
        const Logro* l = &g->logros[i];
        if (l->desbloqueado)
            printf("  %s✓%s %s%-24s%s %s\n", cc(C_GREEN), RST, cc(C_BOLD),
                   l->nombre, RST, l->descripcion);
        else if (!soloDesbloqueados)
            printf("  %s·%s %-24s %s(sin descubrir)%s\n", cc(C_GREY), RST, l->nombre,
                   cc(C_GREY), RST);
    }
    printf("\n  %s(%d de %d)%s\n", cc(C_GREY), n, g->logrosCount, RST);
}

static void chequearLogros(GameState* g) {
    Shinji* s = &g->shinji;

    if (s->sincronizacion >= 70 && g->crisisCount == 0)
        desbloquearLogro(g, "sincronización máxima",
                         "Alcanzaste 70 de sincronización sin pasar ninguna crisis.");
    if (s->fuerza >= 65 && s->confianza >= 30)
        desbloquearLogro(g, "Fuerza de titán",
                         "Fuerte y con confianza: entrenaste sin esconderte.");
    if (s->inteligencia >= 65 && g->misionesCompletadas >= 4)
        desbloquearLogro(g, "Mente Brillante",
                         "Estudiaste y adeás cumpliste lo que te dijeron.");
    if (s->animo >= 75)
        desbloquearLogro(g, "Ánimo Inquebrantable",
                         "Mantuviste el ánimo alto hasta el final.");
    if (s->confianza >= 50)
        desbloquearLogro(g, "Seguro de Si Mismo",
                         "Creiste en ti. Eso no se regala.");
    if (s->confianza >= 40 && s->cobardia >= 35)
        desbloquearLogro(g, "Afrontar el Miedo",
                         "Tuviste miedo y te quedaste igual. Es lo más difícil.");
    if (s->cobardia >= 65)
        desbloquearLogro(g, "Evitador",
                         "Huiste tanto que nadie te busco más.");
    if (s->rebeldia >= 45)
        desbloquearLogro(g, "Rebelde Imparable",
                         "Le discutiste al sistema.");
    if (s->afinidad[P_REI] >= 75 && s->afinidad[P_KAWORU] >= 75)
        desbloquearLogro(g, "Entre Dos",
                         "Rei y Kaworu en el mismo mes. Eso es raro y lo sabes.");
    if (s->afinidad[P_REI] >= 50 && s->afinidad[P_KAWORU] >= 50 && s->sincronizacion >= 70)
        desbloquearLogro(g, "Final Secreto",
                         "Conectaste con Rei y Kaworu a la vez. Ese final existe.");
    if (s->afinidad[P_REI] >= 75 || s->afinidad[P_KAWORU] >= 75 ||
        s->afinidad[P_ASUKA] >= 75)
        desbloquearLogro(g, "vínculo Profundo",
                         "Hay alguien que te sostiene de verdad.");
    if (g->citasCompletadas >= MAX_CITAS)
        desbloquearLogro(g, "Todas las Citas",
                         "No le dijiste que no a nadie.");
    if (g->misionesCompletadas >= 10)
        desbloquearLogro(g, "Eficiente",
                         "Diez misiones completadas en un mes.");
    if (g->combatesGanados >= 2)
        desbloquearLogro(g, "Cazador de Ángeles",
                         "Detuviste a dos ángeles.");
    if (g->combatesPerdidos >= 1 && s->animo >= 60)
        desbloquearLogro(g, "Bajo presión",
                         "Perdiste un combate y no te rompió el ánimo.");
    if (g->dia >= DIAS_TOTALES)
        desbloquearLogro(g, "Hasta el Final",
                         "Llegaste al día 30.");
}

/* -------------------------------------------------------------------------- */
/* Pantallas                                                                  */
/* -------------------------------------------------------------------------- */

static void mostrarCabecera(const GameState* g) {
    char titulo[64], sub[64];
    snprintf(titulo, sizeof titulo, "DÍA %d / %d", g->dia, DIAS_TOTALES);
    snprintf(sub, sizeof sub, "%s  ·  %s", nombreArco(arcoDelDia(g->dia)), NOMBRE_PROYECTO);
    caja(titulo, sub);
    putchar('\n');
}

/* Barra de 0 a max, coloreada según qué tan bien está el valor. */
static void barra(int v, int max, int ancho) {
    int llenos = clampi((v * ancho) / max, 0, ancho);
    const char* color = C_GREEN;
    if (v <= max / 3)      color = C_RED;
    else if (v <= max / 2) color = C_YELL;
    printf("%s", cc(color));
    for (int i = 0; i < ancho; i++) putchar(i < llenos ? '#' : '.');
    printf("%s", RST);
}

/* Afinidad de -100 a +100, con el cero en el centro de la barra. */
static void barraVinculo(int v) {
    int pos = clampi((v + 100) / 20, 0, 10);
    printf("%s", cc(C_GREY));
    for (int i = 0; i <= 10; i++) {
        if (i == pos) printf("%s%c%s", cc(C_CYAN), '#', RST);
        else putchar('.');
    }
}

/* Color según la etapa: rojo si te rechaza, verde si te quiere. */
static const char* colorEtapa(Etapa e) {
    if (e <= ETAP_EVITADO)    return C_RED;
    if (e >= ETAP_UNIDO)      return C_GREEN;
    if (e >= ETAP_AMIABLE)    return C_CYAN;
    if (e >= ETAP_CONOCIDO)   return C_BLUE;
    return C_GREY;
}

static void mostrarEstadisticas(const GameState* g) {
    const Shinji* s = &g->shinji;
    char v[16];

    /* La columna de valores mide 7 para que "100/100" no empuje la barra. */
    regla();
    printf("  %-16s ", "Fuerza");
    snprintf(v, sizeof v, "%d", s->fuerza);
    printf("%7s  ", v);  barra(s->fuerza, 100, 22); putchar('\n');

    printf("  %-16s ", "Inteligencia");
    snprintf(v, sizeof v, "%d", s->inteligencia);
    printf("%7s  ", v);  barra(s->inteligencia, 100, 22); putchar('\n');

    printf("  %-16s ", "Ánimo");
    snprintf(v, sizeof v, "%d", s->animo);
    printf("%7s  ", v);  barra(s->animo, 100, 22); putchar('\n');

    printf("  %-16s ", "Sincronización");
    snprintf(v, sizeof v, "%d", s->sincronizacion);
    printf("%7s  ", v);  barra(s->sincronizacion, 100, 22); putchar('\n');

    printf("  %-16s ", "Energía");
    snprintf(v, sizeof v, "%d/%d", s->energia, ENERGIA_MAX);
    printf("%7s  ", v);  barra(s->energia, ENERGIA_MAX, 22); putchar('\n');

    printf("\n  %s%-16s %4d%s   %s%-16s %4d%s\n", cc(C_DIM), "Confianza",
           s->confianza, RST, cc(C_DIM), "Cobardía", s->cobardia, RST);
    printf("  %s%-16s %4d%s\n", cc(C_DIM), "Rebeldía", s->rebeldia, RST);

    printf("\n%s  VÍNCULOS%s   %shostil · 0 · unido%s\n",
           cc(C_CYAN), RST, cc(C_GREY), RST);
    for (int i = 0; i < PERSONAJES; i++) {
        int a = s->afinidad[i];
        Etapa e = etapaDe(a);
        printf("  %s%-9s%s %s%-12s%s %4d  ",
               cc(C_BOLD), perfiles[i].nombre, RST,
               cc(colorEtapa(e)), nombresEtapa[e], RST, a);
        barraVinculo(a);
        putchar('\n');
    }
}

static void mostrarMisiones(const GameState* g) {
    seccion("MISIONES ACTIVAS");
    if (g->misionesCount == 0) {
        printf("  %sNo tienes misiones. Llega una por día, hasta %d a la vez.%s\n",
               cc(C_GREY), MAX_MISIONES, RST);
        return;
    }
    for (int i = 0; i < g->misionesCount; i++) {
        const Mision* m = &g->misiones[i];
        printf("  %s[%d]%s %s\n", cc(C_YELL), i + 1, RST, m->descripcion);
        printf("      %sdía %d%s   energía %s+%d%s",
               cc(C_GREY), m->diaAsignada, RST,
               cc(C_GREEN), m->recompensaEnergia, RST);
        if (m->personajeIndex >= 0)
            printf(", afinidad con %s %s+%d%s", perfiles[m->personajeIndex].nombre,
                   cc(C_GREEN), m->recompensaAfinidad, RST);
        printf("\n");
    }
}

/* -------------------------------------------------------------------------- */
/* vínculos: texto al subir de etapa                                          */
/* -------------------------------------------------------------------------- */

/*
 * Aviso de cambio de etapa. Ahora funciona en las DOS direcciones: caer a
 * EVITADO o HOSTIL también dice algo, y ese "algo" es el aviso de que el
 * vínculo se está cerrando.
 */
static void chequearCambioEtapa(GameState* g, int p) {
    if (p < 0 || p >= PERSONAJES) return;
    Etapa actual = etapaDe(g->shinji.afinidad[p]);
    if ((int)actual == g->etapaVista[p]) return;

    int anterior = g->etapaVista[p];
    g->etapaVista[p] = (int)actual;

    /* Esto va al diario aunque no haya texto de aviso: el cambio de etapa es
       justamente el dato que el jugador necesita para entender por qué el
       mundo reacciona veinte días después. */
    {
        char nota[NOTA_DIARIO];
        snprintf(nota, sizeof nota, "%s con %s: %s -> %s.",
                 ((int)actual > anterior) ? "subió" : "cayó",
                 perfiles[p].nombre, nombresEtapa[anterior], nombresEtapa[actual]);
        anotar(g, nota);
    }

    if (perfiles[p].aviso[actual][0] == '\0') return;

    const char* flecha = ((int)actual > anterior) ? "ahora es" : "dejo de ser";
    printf("\n  [ %s %s %s ]\n", perfiles[p].nombre, flecha, nombresEtapa[actual]);
    printf("  %s\n", perfiles[p].aviso[actual]);
}

static void subirAfinidad(GameState* g, int p, int delta) {
    if (p < 0 || p >= PERSONAJES) return;
    g->shinji.afinidad[p] =
        clampi(g->shinji.afinidad[p] + delta, AFINIDAD_MIN, AFINIDAD_MAX);
    chequearCambioEtapa(g, p);
}

/* -------------------------------------------------------------------------- */
/* Misiones                                                                   */
/* -------------------------------------------------------------------------- */

/* Saca las misiones completadas del array para liberar el slot. */
static void compactarMisiones(GameState* g) {
    int w = 0;
    for (int i = 0; i < g->misionesCount; i++) {
        if (!g->misiones[i].completada) {
            if (w != i) g->misiones[w] = g->misiones[i];
            w++;
        }
    }
    g->misionesCount = w;
}

static const char* verboDe(Actividad t) {
    switch (t) {
        case ACT_ENTRENAR:   return "Entrena con el EVA";
        case ACT_ESTUDIAR:   return "Estudia los archivos de NERV";
        case ACT_SOCIALIZAR: return "Pasa tiempo con alguien";
        case ACT_DESCANSAR:  return "Duerme de verdad";
        case ACT_SINCRO:     return "Sincroniza con el EVA-01";
        default:             return "Haz algo";
    }
}

static void asignarMision(GameState* g) {
    if (g->misionesCount >= MAX_MISIONES) return;

    Mision* m = &g->misiones[g->misionesCount];
    memset(m, 0, sizeof *m);

    m->tipo               = (int)((g->dia - 1) % 5 + 1);
    m->personajeIndex     = -1;
    m->completada         = 0;
    m->recompensaEnergia  = 12;
    m->recompensaAfinidad = 0;
    m->diaAsignada        = g->dia;

    if (m->tipo == ACT_SOCIALIZAR) {
        static const int candidatos[] = { P_ASUKA, P_REI, P_KAWORU, P_MISATO,
                                          P_MARI, P_RITSUKO, P_MANA, P_MAYA };
        const int nc = (int)(sizeof candidatos / sizeof *candidatos);
        int c = candidatos[rand() % nc];
        /* Rei es la que más cuesta, así que no la ponemos de arranque. Si igual
           sale, se reintenta: una misión que ya cumpliste no teaches nada. */
        for (int intento = 0; intento < 4 && c == P_REI; intento++)
            c = candidatos[rand() % nc];
        m->personajeIndex     = c;
        m->recompensaAfinidad = 10;
    }

    snprintf(m->descripcion, sizeof m->descripcion, "%s", verboDe((Actividad)m->tipo));
    if (m->personajeIndex >= 0) {
        size_t n = strlen(m->descripcion);
        snprintf(m->descripcion + n, sizeof m->descripcion - n,
                 " con %s.", perfiles[m->personajeIndex].nombre);
    }

    g->misionesCount++;
    printf("\n  %smisión del día:%s %s\n", cc(C_YELL), RST, m->descripcion);
}

static int verificarMision(const Mision* m, Actividad actividad, int objetivoSocial) {
    if (m->completada) return 0;
    if ((int)actividad != m->tipo) return 0;
    if (m->tipo == ACT_SOCIALIZAR)
        return (objetivoSocial >= 0 && objetivoSocial == m->personajeIndex);
    return 1;
}

static void procesarMision(GameState* g, Mision* m) {
    if (m->completada) return;
    m->completada = 1;
    g->misionesCompletadas++;

    g->shinji.energia = clampi(g->shinji.energia + m->recompensaEnergia, 0, ENERGIA_MAX);
    printf("\n   misión completada: energía +%d", m->recompensaEnergia);
    if (m->personajeIndex >= 0 && m->recompensaAfinidad > 0) {
        printf(", afinidad con %s +%d", perfiles[m->personajeIndex].nombre,
               m->recompensaAfinidad);
        subirAfinidad(g, m->personajeIndex, m->recompensaAfinidad);
    }
    printf("\n");
    pausa();
}

/* -------------------------------------------------------------------------- */
/* Actividades                                                                */
/* -------------------------------------------------------------------------- */

static int elegirConQuien(Shinji* s, const GameState* g) {
    printf("\n  Hablar con quién?\n");
    for (int i = 0; i < PERSONAJES; i++) {
        const char* marca = "  ";
        for (int j = 0; j < g->misionesCount; j++) {
            if (!g->misiones[j].completada &&
                g->misiones[j].tipo == ACT_SOCIALIZAR &&
                g->misiones[j].personajeIndex == i) { marca = " *"; break; }
        }
        printf("  %s %d. %-9s (%s %d)\n", marca, i + 1, perfiles[i].nombre,
               nombresEtapa[etapaDe(s->afinidad[i])], s->afinidad[i]);
    }
    printf("     %d. Nadie en particular\n", PERSONAJES + 1);
    int e = leerEntero("  > ", 1, PERSONAJES + 1);
    if (e <= 0) return -1;
    return (e == PERSONAJES + 1) ? -1 : e - 1;
}

/* Un vínculo en HOSTIL o EVITADO deja de estar disponible para eventos clave. */
static int vinculoDisponible(const GameState* g, int p, int margenAbajo) {
    return g->shinji.afinidad[p] > margenAbajo;
}

static void socializar(GameState* g, Actividad actividad) {
    Shinji* s = &g->shinji;
    int objetivo = elegirConQuien(s, g);

    /* elegirConQuien devuelve -1 para "nadie". El rango se comprueba igual:
       de acá sale el índice de memoria[], y no queremos que la dependable
       lectura de teclado sea la que garantiza que no se pase. */
    if (objetivo < 0 || objetivo >= PERSONAJES) {
        printf("  No hablaste con nadie. Escuchaste música y nada más.\n");
        s->animo     -= 2;
        s->energia   -= 8;
        s->confianza -= 1;
        anotar(g, "No hablaste con nadie.");
        return;
    }

    Etapa e = etapaDe(s->afinidad[objetivo]);
    printf("  %s\n", perfiles[objetivo].ocio[e]);

    s->energia -= 8;

    if (e <= ETAP_EVITADO) {
        /* Un vínculo roto no se arregla insistiendo: cuesta caro y rinde poco. */
        printf("  [La conversación no arranca. Insistir empeora las cosas.]\n");
        s->animo     -= 4;
        s->confianza -= 2;
        subirAfinidad(g, objetivo, -3);
        if (e == ETAP_HOSTIL) subirAfinidad(g, objetivo, -3);
        {
            char nota[NOTA_DIARIO];
            snprintf(nota, sizeof nota, "Insististe con %s y no sirvió de nada.",
                     perfiles[objetivo].nombre);
            anotar(g, nota);
        }
        return;   /* no puede completar misiones de socializar */
    }

    s->animo     += 3;
    s->confianza += 1;

    /* Entre más cerca está, más rinde la conversación (y más cuesta). */
    int mult = 1 + (int)e - (int)ETAP_CONOCIDO;
    if (mult < 1) mult = 1;
    subirAfinidad(g, objetivo, 5 * mult);
    g->memoria[objetivo].veces_ayudada++;

    /* Y el vínculo te devuelve algo. */
    if (e >= ETAP_CERCANO) { s->animo += 4; s->confianza += 2; }
    if (e >= ETAP_UNIDO)   { s->animo += 3; s->sincronizacion += 2; }

    /* Un vínculo en problemas te deja peor, no mejor. */
    if (objetivo == P_ASUKA && s->confianza < 20) s->animo -= 3;

    /* Se chequea acá para que "con quién" cuente como decisión informada. */
    for (int i = 0; i < g->misionesCount; i++) {
        if (verificarMision(&g->misiones[i], actividad, objetivo))
            procesarMision(g, &g->misiones[i]);
    }
}

static void realizarActividad(GameState* g) {
    Shinji* s = &g->shinji;

    /* Un solo turno de actividad por día. Antes se podía repetir infinitamente
       y arruinar el juego entero. */
    if (g->actividadHecha) {
        printf("\n  [Ya realizaste la actividad de hoy. Solo queda mirar cosas o terminar.]\n");
        return;
    }

    if (s->energia < 10) {
        printf("\n  [Estás agotado. Hoy solo se descansa.]\n");
        s->animo   += gananciaConRendimientoDecreciente(s->animo, 8);
        s->energia  = clampi(s->energia + 32, 0, ENERGIA_MAX);
        normalizar(s);
        printf("  Dormiste todo el día. Ánimo +8, energía +32.\n");
        g->actividadHecha = 1;
        pausa();
        return;
    }

    seccion("ACTIVIDAD DEL DÍA  ·  una sola por día");
    printf(" 1. Entrenar    (fuerza,         energía -18)\n");
    printf(" 2. Estudiar    (inteligencia,   energía -16)\n");
    printf(" 3. Socializar  (afinidad, ánimo, energía  -8)\n");
    printf(" 4. Descansar   (ánimo,          energía +32)\n");
    printf(" 5. Sincronizar (sincronización, energía -22)\n");

    int opcion = leerEntero("  > ", 1, 5);
    if (opcion <= 0) return;
    Actividad actividad = (Actividad)opcion;
    g->actividadHecha = 1;

    switch (actividad) {
        case ACT_ENTRENAR: {
            int gan = gananciaConRendimientoDecreciente(s->fuerza, 5);
            s->fuerza    += gan;
            s->energia   -= 18;
            s->confianza += 2;
            s->animo     += 1;
            printf("  Entrenaste hasta que te dolían los músculos. Fuerza +%d.\n", gan);
            break;
        }
        case ACT_ESTUDIAR: {
            int gan = gananciaConRendimientoDecreciente(s->inteligencia, 5);
            s->inteligencia += gan;
            s->energia      -= 16;
            s->confianza    += 1;
            printf("  Leíste los informes hasta tarde. No entiendes la mitad.\n");
            printf("  Pero crees que sí. Y eso es peor. Inteligencia +%d.\n", gan);
            break;
        }
        case ACT_SOCIALIZAR:
            socializar(g, actividad);
            return;   /* socializar ya chequeo las misiones */
        case ACT_DESCANSAR: {
            int gan = gananciaConRendimientoDecreciente(s->animo, 8);
            s->animo  += gan;
            s->energia = clampi(s->energia + 32, 0, ENERGIA_MAX);
            printf("  Dormiste. De verdad, no lo que yo llamó dormir.\n");
            printf("  Ánimo +%d, energía +32.\n", gan);
            break;
        }
        case ACT_SINCRO: {
            int gan = gananciaConRendimientoDecreciente(s->sincronizacion, 6);
            s->sincronizacion += gan;
            s->energia        -= 22;
            s->animo          -= 1;
            printf("  El EVA se movió antes que yo.\n");
            printf("  Eso siempre lo interpreté como bueno. Sincronización +%d.\n", gan);
            break;
        }
        default:
            break;
    }

    normalizar(s);
    {
        char nota[NOTA_DIARIO];
        snprintf(nota, sizeof nota, "Actividad: %s.", verboDe(actividad));
        anotar(g, nota);
    }

    for (int i = 0; i < g->misionesCount; i++) {
        if (verificarMision(&g->misiones[i], actividad, -1))
            procesarMision(g, &g->misiones[i]);
    }

    pausa();
}

/* -------------------------------------------------------------------------- */
/* Citas                                                                      */
/* -------------------------------------------------------------------------- */

static void visitarCita(GameState* g, Cita* c) {
    Shinji* s = &g->shinji;
    int p = c->personajeIndex;

    c->fueVisitada = 1;

    printf("\n================ CITA: %s ================\n", perfiles[p].nombre);
    printf("  %s -- %s\n\n", perfiles[p].nombre, perfiles[p].rol);

    /* Una cita con alguien que te evita no es una cita: es un aviso. */
    if (etapaDe(s->afinidad[p]) <= ETAP_EVITADO) {
        printf("  No viene. Viene alguien de su parte a decir que no viene.\n");
        printf("  Se te devuelve la cita. Es lo que hacen para no tener que explicar.\n");
        s->confianza -= 3;
        c->fueVisitada = 0;   /* no se gasta: puede volver a intentar otro dia */
        printf("\n  [Cita cancelada. No cuenta para nada.]\n");
        pausa();
        return;
    }

    g->memoria[p].veces_ayudada++;
    g->citasCompletadas++;
    {
        char nota[NOTA_DIARIO];
        snprintf(nota, sizeof nota, "Cita con %s.", perfiles[p].nombre);
        anotar(g, nota);
    }

    switch (p) {
        case P_ASUKA:
            if (c->variante) {
                printf("  Asuka no dice hola. Se sienta y espera a que hables.\n");
                printf("  \"Estuviste raro últimamente\", dice al final. \"No es nada.\"\n");
                printf("  Se queda hasta que te vas. No te lo dice.\n");
                s->animo     += 12;
                s->confianza += 10;
                subirAfinidad(g, p, 26);
                break;
            }
            printf("  Asuka: \"No te me duermas. Hoy entrenamos y lo hacemos bien.\"\n");
            printf("  Trabajan hasta que se hace de noche. Ella no se detiene ni una vez.\n");
            printf("  Al final dice \"ya está\". Le tiemblan las manos.\n");
            s->fuerza     += gananciaConRendimientoDecreciente(s->fuerza, 10);
            s->energia    -= 15;
            s->confianza += 6;
            subirAfinidad(g, p, 15);
            break;

        case P_REI:
            if (c->variante) {
                printf("  Rei: \"Shinji. Mañana no sé qué va a pasar.\"\n");
                printf("  \"Tampoco yo. No hace falta saberlo.\"\n");
                printf("  Se quedan en la puerta hasta que entre el frío.\n");
                s->animo     += 14;
                s->confianza += 8;
                subirAfinidad(g, p, 22);
                break;
            }
            printf("  Rei: \"Gracias por acompañarme.\"\n");
            printf("  \"No hacía falta que vinieras.\"\n");
            printf("  Se quedan callados un rato largo. A ninguno le molesta.\n");
            s->animo   += 10;
            s->energia -= 5;
            subirAfinidad(g, p, 20);
            break;

        case P_KAWORU:
            printf("  Kaworu: \"La música nos une, ¿no? Todo el mundo lo dice.\"\n");
            printf("  \"Pero tú lo sentiste. Eso es distinto.\"\n");
            printf("  Pone una canción. Mira cómo reacciona el mar.\n");
            s->animo          += 15;
            s->confianza      += 5;
            s->sincronizacion += 3;
            subirAfinidad(g, p, 25);
            break;

        case P_MISATO:
            printf("  Misato: \"Tengo una pregunta y no me importa la respuesta correcta.\"\n");
            printf("  Habla de tu padre, de NERV, de por qué estás ahí. Y escucha de verdad.\n");
            s->confianza += 8;
            s->animo      += 6;
            s->rebeldia  += 3;
            subirAfinidad(g, p, 18);
            break;

        case P_MARI:
            printf("  Mari: \"Vamos. No me vengas con que tienes que volver temprano.\"\n");
            printf("  Te saca a la calle y te hace escuchar música hasta que se pasa el día.\n");
            s->animo     += 18;
            s->energia   -= 10;
            s->confianza += 4;
            subirAfinidad(g, p, 22);
            break;

        case P_RITSUKO:
            printf("  Ritsuko: \"Tengo los datos de todo el mes. Te los muestro porque me lo pediste.\"\n");
            printf("  Te explica cómo funciona el mundo que te rodea. Y a tú te gusta escucharlo.\n");
            s->inteligencia += gananciaConRendimientoDecreciente(s->inteligencia, 8);
            s->confianza    += 5;
            subirAfinidad(g, p, 16);
            break;

        case P_MANA:
            printf("  Mana: \"Oye. ¿Tú sigues con lo del EVA? Yo no lo entiendo nada.\"\n");
            printf("  \"No importa. Te cuento cualquier cosa y listo.\"\n");
            printf("  Habla cuarenta minutos de cosas que no sirven para nada.\n");
            printf("  Es lo único del día que no se parece a una luxación.\n");
            s->animo     += 20;
            s->confianza += 6;
            s->energia  -= 5;
            subirAfinidad(g, p, 20);
            break;

        default:
            break;
    }

    normalizar(s);
    printf("\n  Afinidad con %s ahora: %d (%s)\n",
           perfiles[p].nombre, s->afinidad[p], nombresEtapa[etapaDe(s->afinidad[p])]);
    pausa();
}

static void menuCitas(GameState* g) {
    seccion("CITAS DISPONIBLES HOY");

    int disponibles[MAX_CITAS];
    int n = 0;
    for (int i = 0; i < g->citasCount; i++) {
        if (!g->citas[i].fueVisitada && g->citas[i].diaDisponible == g->dia)
            disponibles[n++] = i;
    }

    if (n == 0) {
        printf("  Hoy no hay citas.\n");
        return;
    }
    for (int i = 0; i < n; i++) {
        int p = g->citas[disponibles[i]].personajeIndex;
        printf("  %d. Con %-9s (%s %d)\n", i + 1, perfiles[p].nombre,
               nombresEtapa[etapaDe(g->shinji.afinidad[p])], g->shinji.afinidad[p]);
    }

    int e = leerEntero("  Cita (0 para volver) > ", 0, n);
    if (e >= 1)
        visitarCita(g, &g->citas[disponibles[e - 1]]);
}

/* -------------------------------------------------------------------------- */
/* Combate con Ángeles                                                         */
/* -------------------------------------------------------------------------- */

static void combate(GameState* g, const char* nombre, int vidaInicial, int dureza) {
    Shinji* s = &g->shinji;

    int vida  = vidaInicial;
    int ronda = 0;
    int apoyo = (s->confianza >= 30 && s->afinidad[P_MISATO] >= 20);

    printf("\n=============== %s ===============\n", nombre);
    if (apoyo) {
        printf("  %sMisato cubre tu frecuencia: \"Shinji, te escucho. Dale con todo.\"%s\n",
               cc(C_GREEN), RST);
        printf("  %s+20 de sincronización efectiva durante el combate.%s\n", cc(C_GREY), RST);
    } else if (s->confianza < 15) {
        printf("  %sNadie te cubre la frecuencia. Vas solo.%s\n", cc(C_RED), RST);
    }

    /* La pelea cae antes de la actividad del día, así que llegas con la energía
       que traías. Si viene baja, avisarlo: perder sin haber jugado se siente
       como una trampa, y acá lo es. */
    if (s->energia < 25)
        printf("  %sEntrás con %d de energía. Aguantás dos o tres golpes, no más.%s\n",
               cc(C_YELL), s->energia, RST);

    printf("  %sLuchas solo. La salida no está disponible.%s\n\n", cc(C_DIM), RST);

    while (vida > 0 && ronda < COMBATE_RONDAS) {
        ronda++;
        int sync = clampi(s->sincronizacion + (apoyo ? 20 : 0), 0, 200);

        printf("\n--- %sRonda %d%s ---\n", cc(C_CYAN), ronda, RST);
        printf("  %s%s%s %s[%d / %d]%s    %sEVA-01%s sync %s%d%s    energía %d\n",
               cc(C_RED), nombre, RST, cc(C_GREY), vida, vidaInicial, RST,
               cc(C_BLUE), RST, cc(C_YELL), sync, RST, s->energia);

        if (s->energia < 4) {
            printf("  [Sin energía. Te sacan del EVA.]\n");
            break;
        }

        printf("  1. Atacar\n");
        printf("  2. Sincronizar\n");
        printf("  3. Evitar el golpe\n");
        printf("  %s4. Gritar%s   %s(riesgo alto: pega el doble, pero te abre)%s\n",
               cc(C_YELL), RST, cc(C_GREY), RST);
        int op = leerEntero("  > ", 1, 4);
        if (op <= 0) op = 3;   /* entrada cortada: lo más barato, no lo más caro */

        switch (op) {
            case 1: {   /* Atacar */
                int prob = clampi((sync + s->fuerza) * 4 / 5, 10, 95);
                if (rand() % 100 < prob) {
                    int dmg = 5 + rand() % (6 + sync / 12);
                    vida -= dmg;
                    printf("  Impactaste. El ángel retrocede %d.\n", dmg);
                    s->confianza += 3;
                } else {
                    printf("  Le erraste. El EVA recibe el contraimpacto.\n");
                    s->energia  -= 8;
                    s->animo    -= 4;
                    s->cobardia += 1;
                }
                s->energia -= 6;
                break;
            }
            case 2:       /* Sincronizar */
                s->energia        -= 10;
                s->sincronizacion += 3 + rand() % 6;
                s->animo          -= 2;
                printf("  Te fundiste con el EVA. Todo se ordena por un instante.\n");
                break;
            case 4: {
                /* Gritar: el golpe entra siempre, y por eso se lleva un
                   pedazo de ti. El daño está por debajo del de dos ataques
                   seguidos, a propósito: si saliera más barato, las otras tres
                   jugadas dejan de existir y la pelea es un botón. */
                s->energia -= 14;
                int dmg = 9 + rand() % (6 + sync / 12);
                vida -= dmg;
                printf("  Gritás. No es un grito: es el EVA usando tu garganta.\n");
                printf("  El ángel retrocede %d.\n", dmg);
                s->confianza -= 2;
                s->animo     -= 6;
                s->cobardia  += 5;
                break;
            }
            default:      /* Evitar */
                s->energia        -= 4;
                s->sincronizacion -= 2;
                printf("  Esquivaste el golpe. No ganaste nada, pero no perdiste nada.\n");
                /* Evitar muchas veces seguidas si el ángel te castiga. */
                if (dureza > 0 && (ronda % 3) == 0) {
                    vida -= dureza;
                    printf("  Te encuentra igual. Te hace %d de daño.\n", dureza);
                }
                break;
        }
        normalizar(s);
    }

    printf("\n");
    if (vida <= 0) {
        g->combatesGanados++;
        anotar(g, "Detuviste al ángel.");
        printf("  >> Lo detuviste. El ángel se retira y Tokio-3 vuelve a respirar.\n");
        printf("     Tu nombre queda en el informe de NERV.\n");
        s->confianza += 10;
        s->animo     += 10;
        s->rebeldia  += 5;
        subirAfinidad(g, P_MISATO, 8);
        if (s->confianza > 40)      subirAfinidad(g, P_REI, 5);
        if (s->sincronizacion > 50) subirAfinidad(g, P_KAWORU, 5);
    } else {
        g->combatesPerdidos++;
        anotar(g, "No pudiste con el ángel.");
        printf("  >> No se pudo. Te sacan del EVA y el informe lo registra.\n");
        s->animo    -= 15;
        s->cobardia += 12;
        s->confianza -= 8;
        /* El informe te alcanza a todos. Que estén en contra o no cambia nada:
           la culpa es del que no apareció, no de quién. */
        if (s->afinidad[P_MISATO] > 0 || s->confianza < 20) subirAfinidad(g, P_MISATO, -8);
        if (s->confianza < 20) {
            subirAfinidad(g, P_REI, -8);
            subirAfinidad(g, P_ASUKA, -8);
        }
    }
    normalizar(s);
    pausa();
}

/* -------------------------------------------------------------------------- */
/* Eventos del día                                                            */
/* -------------------------------------------------------------------------- */

static void eventoDelDia(GameState* g) {
    Shinji* s = &g->shinji;
    int d = g->dia;

    seccion("EVENTO DEL DÍA");

    /* La pelea arranca antes de que elija actividad, así que no hay forma
       de guardar energía a propósito para el ángel. Avisar el día anterior es
       lo menos que le debemos: si pierde, que sea por una decisión suya. */
    if (d == 4 || d == 22 || d == 28)
        printf("  Mañana llega un ángel. Lo único que puedes preparar hoy es con qué\n"
               "  energía lo encaras.\n\n");

    switch (d) {
        /* ===== Arco 1 · Asentamiento (días 1 a 8) ===== */
        case 1:
            printf("  Shinji Ikari, 14 años, no sabe muy bien por qué está ahí.\n");
            printf("  En la habitación hay una cama, un escritorio y un póster de un EVA.\n");
            break;

        case 2:
            printf("  Primera mañana en la clase 2-A. El aula huele a tiza y a calor.\n");
            printf("  Hay seis asignaturas. En dos de ellas no entiendes nada todavía.\n");
            printf("  A tu lado hay un asiento vacío con tu nombre en la etiqueta.\n");
            if (confirmar("¿Te sientas en ese asiento?")) {
                printf("  Te sientas. La etiqueta dice SHINJI IKARI, en letra de imprenta.\n");
                printf("  Parece escrita por alguien que nunca vio tu nombre.\n");
                s->confianza += 3;
                subirAfinidad(g, P_ASUKA, 3);
            } else {
                printf("  Te quedas de pie hasta que la profesora te señala uno al fondo.\n");
                printf("  Al fondo no habla nadie.\n");
                s->confianza -= 3;
                s->cobardia += 2;
            }
            break;

        case 3:
            printf("  Asuka te espera en el gimnasio. \"Shinji. Tengo que hablar contigo.\"\n");
            if (confirmar("La escuchas?")) {
                printf("  Te explica por qué empezo a pilotar. Y no es lo que pensabas.\n");
                subirAfinidad(g, P_ASUKA, 12);
                s->confianza += 4;
            } else {
                printf("  \"Bueno. Como quieras.\" Y sale cerrando la puerta.\n");
                subirAfinidad(g, P_ASUKA, -5);
                s->cobardia += 4;
            }
            break;

        case 4:
            printf("  En el recreo Asuka se apoya en tu mesa sin pedir permiso.\n");
            printf("  \"Tú también estás solo, ¿no? Bien.\"\n");
            printf("  Y se queda ahí, sin decir nada más, veinte minutos.\n");
            if (etapaDe(s->afinidad[P_ASUKA]) >= ETAP_AMIABLE) {
                printf("  Al irse te toca el hombro. No te mira.\n");
                subirAfinidad(g, P_ASUKA, 4);
            } else {
                printf("  Al irse dice algo que no alcanzas a oír.\n");
            }
            break;

        case 5:
            printf("  Alerta de intrusión. Hay un ángel en la costa.\n");
            printf("  El EVA-01 está listo. Y no hay nadie más adentro de la que suele ir.\n");
            pausa();
            combate(g, "ÁNGEL DE LOS ACUIFEROS", VIDA_ACUIFEROS, 6);
            break;

        case 6:
            printf("  Examen de matemática. Treinta minutos.\n");
            printf("  La pregunta cuatro es la misma que alguien te explicó ayer.\n");
            if (s->inteligencia >= 55) {
                printf("  La resuelves de memoria, y está bien.\n");
                s->confianza += 5;
            } else {
                printf("  La resuelves de memoria, y está mal.\n");
                printf("  Miras el reloj. Todos miran el reloj.\n");
                s->confianza -= 5;
                s->cobardia += 4;
            }
            break;

        case 7:
            printf("  Llueve. El boletín de NERV aparece en todas las pantallas del aula.\n");
            printf("  Ritsuko lo traduce al pie en letra chica, para los que no leen.\n");
            printf("  Asuka lo lee dos veces y no le dice nada a nadie.\n");
            if (s->inteligencia >= 45) {
                printf("  Entiendes lo que dice el boletín antes que la mayoría.\n");
                printf("  No cambia nada. Pero lo entiendes.\n");
                subirAfinidad(g, P_RITSUKO, 5);
                s->confianza += 3;
            } else {
                printf("  No entiendes nada. Y finges que sí.\n");
            }
            break;

        case 8:
            printf("  Rei te espera en el pasillo. No parece casualidad.\n");
            printf("  \"Shinji.\" Hoy hace frío.\n");
            if (confirmar("¿Le das tu abrigo?")) {
                printf("  Lo toma con las dos manos. Como si no supiera qué hacer con él.\n");
                g->memoria[P_REI].dio_abrigo = 1;
                g->memoria[P_REI].veces_ayudada++;
                subirAfinidad(g, P_REI, 15);
                s->animo += 8;
            } else {
                printf("  Asiente y se va igual.\n");
                printf("  Y eso, para Rei, es una respuesta completa.\n");
                g->memoria[P_REI].veces_rechazada++;
                s->cobardia += 3;
            }
            break;

        /* ===== Arco 2 · Amenaza (días 9 a 16) ===== */
        case 9:
            printf("  Día sin clase: Okinawa arrasó dos ciudades la semana pasada.\n");
            printf("  En la clase nadie habla. Alguien llora y se hace el que no.\n");
            if (etapaDe(s->afinidad[P_REI]) <= ETAP_EVITADO) {
                printf("  Rei no se acerca a nadie. Ni a ti.\n");
                s->animo -= 3;
            } else if (etapaDe(s->afinidad[P_REI]) >= ETAP_CERCANO) {
                printf("  Rei se sienta a tu lado sin decir nada. Sigue así veinte minutos.\n");
                subirAfinidad(g, P_REI, 3);
                s->animo += 4;
            } else {
                printf("  Rei mira la ventana durante toda la clase.\n");
                s->animo -= 1;
            }
            break;

        case 10:
            /* La memoria decide si Rei te invita. Si ya te evitó, no aparece. */
            if (g->memoria[P_REI].veces_rechazada >= 1 ||
                !vinculoDisponible(g, P_REI, 0)) {
                printf("  Rei pasa por el pasillo. Te ve.\n");
                printf("  No se detiene. Sigue de largo.\n");
                printf("  [No hay nada que decidir. Esto ya lo decidiste antes.]\n");
                s->animo -= 4;
                subirAfinidad(g, P_REI, -6);
                break;
            }
            printf("  Rei te invita a hablar. En persona. Mirándote a la cara.\n");
            if (confirmar("¿Aceptas?")) {
                printf("  Rei te escucha y no lo interrumpe ni una vez.\n");
                printf("  Habla poco. Escucha entero. Con Rei es indistinguible.\n");
                g->memoria[P_REI].veces_ayudada++;
                subirAfinidad(g, P_REI, 10);
                s->confianza += 5;
            } else {
                printf("  \"Entiendo.\"\n");
                printf("  Y se da la vuelta. Esa palabra pesa más que un portazo.\n");
                g->memoria[P_REI].rechazo_escucha = 1;
                g->memoria[P_REI].veces_rechazada++;
                subirAfinidad(g, P_REI, -12);
                s->cobardia += 5;
            }
            break;

        case 11:
            printf("  Asuka y Ritsuko discuten en el pasillo. Fuerte.\n");
            printf("  De palabras, todavía. Están a punto de que dejen de serlo.\n");
            if (confirmar("¿Te metes?")) {
                printf("  Dices una sola frase y Asuka se calla.\n");
                printf("  No porque tenga razón. Porque le sorprende que hables.\n");
                subirAfinidad(g, P_ASUKA, 6);
                subirAfinidad(g, P_RITSUKO, 4);
                s->confianza += 6;
                s->rebeldia += 3;
            } else {
                printf("  Los dejas. Se separan solas, como se separan siempre.\n");
                s->cobardia += 3;
                s->confianza -= 2;
            }
            break;

        case 12:
            printf("  Misato te pregunta si confías en tu padre.\n");
            printf("  Es una pregunta capciosa, y ella lo sabe.\n");
            if (confirmar("Dices que si?")) {
                printf("  Misato te mira con lástima. \"Todos dicen que si.\"\n");
                s->cobardia  += 5;
                s->confianza -= 3;
            } else {
                printf("  Misato sonríe. \"Eso era lo que quería oír.\"\n");
                s->rebeldia += 6;
                subirAfinidad(g, P_MISATO, 6);
            }
            break;

        case 13:
            printf("  Ritsuko te pide los apuntes de la clase anterior.\n");
            printf("  Los perdiste. Los dos. Y ella lo sabe.\n");
            if (confirmar("¿Se lo confesas?")) {
                printf("  Se lo admites. Toma nota de las fechas y te explica.\n");
                printf("  \"No pasa nada. Yo también pierdo cosas.\"\n");
                subirAfinidad(g, P_RITSUKO, 8);
                s->confianza += 4;
            } else {
                printf("  Le dices que sí los tienes. Después no le pasas nada.\n");
                printf("  Los dos saben que no le pasaste nada.\n");
                subirAfinidad(g, P_RITSUKO, -6);
                s->confianza -= 4;
            }
            break;

        case 14:
            printf("  Excursión a la playa. Cuarenta minutos de ómnibus.\n");
            printf("  Asuka va delante. Rei va al lado de la ventana. Mana va contigo.\n");
            if (confirmar("¿Te sientas con Mana?")) {
                printf("  Habla durante cuarenta minutos y no para.\n");
                printf("  Es lo más parecido a tener a alguien de tu lado.\n");
                subirAfinidad(g, P_MANA, 10);
                s->animo += 5;
            } else {
                printf("  Te sientas solo. Desde ahí se ve el mar mejor.\n");
                subirAfinidad(g, P_MANA, -3);
                s->confianza -= 2;
            }
            break;

        case 15:
            printf("  Asuka te reta a entrenar juntos. Y está sería.\n");
            if (confirmar("¿Aceptas?")) {
                printf("  Entrenamiento intenso. Sales muerto. Pero sales mejor.\n");
                s->fuerza    += gananciaConRendimientoDecreciente(s->fuerza, 5);
                s->energia   -= 10;
                s->confianza += 5;
                subirAfinidad(g, P_ASUKA, 10);
            } else {
                printf("  \"Bueno.\"\n");
                printf("  Se queda mirando la puerta un rato largo.\n");
                subirAfinidad(g, P_ASUKA, -5);
                s->cobardia += 5;
            }
            break;

        case 16:
            printf("  Mari te frena en el pasillo con un auricular en la mano.\n");
            printf("  \"está canción es de un grupo que te va a gustar. No preguntes por qué.\"\n");
            printf("  Le das al play en el oído. Es mucho mejor de lo que esperabas.\n");
            subirAfinidad(g, P_MARI, 12);
            s->animo += 10;
            break;

        /* ===== Arco 3 · Combate (días 17 a 23) ===== */
        case 17:
            printf("  Medio curso. Las notas salen hoy.\n");
            printf("  Tu promedio está en el medio exacto, que es el peor lugar posible.\n");
            if (s->inteligencia >= 60) {
                printf("  Pasas. Con lo justo, y sin festejar nada.\n");
                s->confianza += 4;
                subirAfinidad(g, P_RITSUKO, 3);
            } else if (s->inteligencia >= 35) {
                printf("  Pasas raspando.\n");
            } else {
                printf("  No pasas.\n");
                printf("  La profesora dice que avises a tus padres.\n");
                printf("  A quién le avisas es la pregunta.\n");
                s->confianza -= 8;
                s->cobardia += 6;
            }
            break;

        case 18:
            printf("  Ritsuko te pide un dato de hace tres semanas.\n");
            printf("  \"Solo para una cosa. Solo una.\" Y ya le abriste la puerta.\n");
            if (confirmar("Le dices que si?")) {
                printf("  Te explica algo que no tendrías que saber. Y te lo explica igual.\n");
                s->inteligencia += gananciaConRendimientoDecreciente(s->inteligencia, 6);
                s->rebeldia      += 4;
                subirAfinidad(g, P_RITSUKO, 10);
            } else {
                printf("  \"Tienes razón.\" Y parece aliviada de que lo hayas rechazado.\n");
                subirAfinidad(g, P_RITSUKO, 4);
                s->confianza += 2;
            }
            break;

        case 19:
            printf("  Hay rumor de un ángel antes de la segunda clase.\n");
            printf("  Asuka se levanta de su silla en el medio de la explicación.\n");
            printf("  No dice nada. Sale. La profesora la deja salir.\n");
            if (etapaDe(s->afinidad[P_ASUKA]) >= ETAP_AMIABLE) {
                printf("  Al pasar deja algo en tu mesa: una botella de agua.\n");
                printf("  Sigue caliente. No lo pregunta y no lo pregunta nadie.\n");
                subirAfinidad(g, P_ASUKA, 8);
                s->animo += 4;
            } else {
                printf("  Asuka pasa por tu mesa sin mirarte.\n");
            }
            break;

        case 20:
            printf("  Un informe de NERV cruza tu expediente: \"respuesta emocional irregular\".\n");
            printf("  Ritsuko te lo muestra antes de que te llegue de verdad.\n");
            printf("  \"Yo puedo tapar eso.\" Y te lo está ofreciendo.\n");
            if (confirmar("Le pides que lo tape?")) {
                printf("  \"Entonces no hables de esto con nadie.\"\n");
                s->rebeldia += 6;
                subirAfinidad(g, P_RITSUKO, 12);
            } else {
                printf("  \"Bien. Que sea el expediente lo que decida, no yo.\"\n");
                s->confianza += 6;
                s->rebeldia  += 2;
                s->cobardia  += 3;
            }
            break;

        case 21:
            printf("  Festival cultural. Cada curso prepara algo y nadie se cuida.\n");
            printf("  A ti te ponen a llevar el cartel.\n");
            printf("  Es el peor trabajo posible y no lo puede rechazar nadie.\n");
            if (s->confianza >= 25) {
                printf("  Le preguntas a Ritsuko los números y ella los tiene.\n");
                printf("  El cartel queda bien. No va a ganar nada.\n");
                subirAfinidad(g, P_RITSUKO, 6);
                s->confianza += 3;
            } else {
                printf("  Lo haces solo y a las nueve de la noche.\n");
                printf("  El cartel queda bien. Nadie lo mira.\n");
                s->confianza += 1;
                s->cobardia += 2;
            }
            break;

        case 22:
            /* Día 22 se lee distinto según lo que haya pasado con Rei. */
            if (etapaDe(s->afinidad[P_REI]) <= ETAP_EVITADO) {
                printf("  Kaworu te propone una conversación que suena a que sabe demasiado.\n");
                printf("  \"Rei no vino a buscarte. Lo sé porque se lo preguntó.\"\n");
                printf("  \"¿Qué le dijiste? No, no me digas. Ya sé.\"\n");
                printf("  Y se queda esperando que le cuentes lo tuyo.\n");
                if (confirmar("¿Se lo contas?")) {
                    printf("  Le contas. Todo. Se lo cuenta a alguien más después, seguro.\n");
                    g->memoria[P_KAWORU].le_confio = 1;
                    s->animo     -= 5;
                    s->confianza += 3;
                    subirAfinidad(g, P_KAWORU, 10);
                } else {
                    printf("  \"Bueno. así se lo contaste.\" Y no insiste.\n");
                    g->memoria[P_KAWORU].veces_rechazada++;
                    subirAfinidad(g, P_KAWORU, -8);
                }
                break;
            }
            if (g->memoria[P_KAWORU].veces_rechazada >= 2) {
                printf("  Kaworu no aparece hoy. Nadie aviso por el, pero es lo mismo.\n");
                printf("  En su lugar hay un mensaje en la pantalla del comedor:\n");
                printf("  \"Shinji: no sé dónde estabas.\"\n");
                s->animo -= 6;
                break;
            }
            printf("  Kaworu te propone una conversación que suena a que sabe demasiado.\n");
            if (confirmar("Escuchas lo que tiene que decir?")) {
                printf("  \"No eres la primera persona que llega a Tokio-3 sin saber por qué.\"\n");
                printf("  \"Y la última que se va preguntaándose lo mismo.\"\n");
                g->memoria[P_KAWORU].veces_ayudada++;
                subirAfinidad(g, P_KAWORU, 15);
                s->animo     += 10;
                s->confianza += 5;
                s->rebeldia  += 3;
            } else {
                printf("  \"Lo que pensaba.\" Y sonríe. Esa sonrisa molesta.\n");
                g->memoria[P_KAWORU].veces_rechazada++;
                subirAfinidad(g, P_KAWORU, -6);
                s->cobardia += 4;
            }
            break;

        case 23: {
            printf("  El segundo ángel está en el centro de la ciudad.\n");
            int psico = s->inteligencia + s->confianza + s->afinidad[P_RITSUKO];
            printf("  Por la red, Ritsuko: \"Shinji, el patrón de ataques es...\n");
            if (g->memoria[P_RITSUKO].le_mintiste) {
                printf("  ...no. Cambio. Confirma tu identidad primero.\"\n");
                printf("  Ritsuko te pide la clave de acceso. La que le diste antes.\n");
                printf("  Se la negaste. Tarda un siglo más en contestar.\n");
                s->confianza -= 3;
            } else if (psico >= 120) {
                printf("  ...reconozco el patrón. Es mental. Está usando tu propia receta.\"\n");
                printf("  Ritsuko te dio la información que hizo la diferencia.\n");
                s->confianza += 8;
            } else {
                printf("  ...no llegó a terminar la frase.\"\n");
                printf("  Sin esa información, estás solo.\n");
                s->confianza -= 3;
            }
            pausa();
            combate(g, "SEMEJANTE", VIDA_SEMEJANTE, 10);
            break;
        }

        /* ===== Arco 4 · Desenlace (días 24 a 30) ===== */
        case 24:
            printf("  Últimos días de clase.\n");
            printf("  Alguien pregunta qué van a hacer después de esto.\n");
            printf("  La respuesta es siempre la misma y siempre incomoda.\n");
            printf("  \"Shinji-kun, ¿tú qué vas a hacer?\"\n");
            if (confirmar("¿Contestas?")) {
                printf("  Dices \"no sé\". Que es verdad, y suena a excusa.\n");
                s->confianza += 2;
            } else {
                printf("  Mira a otro lado hasta que cambie de tema.\n");
                s->confianza -= 4;
                s->cobardia += 4;
            }
            break;

        case 25:
            /* Misato deja de preguntar si no hay confianza. */
            if (etapaDe(s->afinidad[P_MISATO]) <= ETAP_EVITADO) {
                printf("  Misato pasa por tu puesto y no se detiene.\n");
                printf("  \"Shinji. Reporte de hoy.\"\n");
                printf("  Es lo único que dice. No pregunta nada más.\n");
                s->confianza -= 3;
                break;
            }
            printf("  Misato te pregunta, sin preámbulo: \"Vas a volar mañana?\"\n");
            printf("  \"Porque yo necesito saberlo. No por ti. Por la Quinta Calle.\"\n");
            if (confirmar("Le dices que si?")) {
                printf("  Anota algo y no te mira. \"Bueno.\" Y eso era todo.\n");
                g->memoria[P_MISATO].veces_ayudada++;
                s->confianza += 10;
                s->rebeldia  += 4;
                subirAfinidad(g, P_MISATO, 12);
            } else {
                printf("  Baja la vista. \"Bueno.\" Y cambia de tema para siempre.\n");
                g->memoria[P_MISATO].veces_rechazada++;
                g->memoria[P_MISATO].le_mintiste = 1;
                g->vecesMentiste++;
                subirAfinidad(g, P_MISATO, -12);
                s->cobardia += 8;
            }
            break;

        case 26:
            printf("  Último día de clase. La profesora dice lo de siempre y no lo de siempre.\n");
            printf("  Todo el mundo firma el acta. Nadie dice nada.\n");
            printf("  Asuka sale sin firmar. Nadie lo menciona.\n");
            if (etapaDe(s->afinidad[P_MANA]) >= ETAP_AMIABLE) {
                printf("  Mana te da un papel doblado. Es una canción. Dice que es para el viaje.\n");
                subirAfinidad(g, P_MANA, 6);
                s->animo += 5;
            } else {
                printf("  Te vas con el cuaderno bajo el brazo y sin nadie que te acompañe.\n");
            }
            break;

        case 27: {
            printf("  Te queda una unidad de EVA. La Asuka se separó de las demás.\n");

            /* Si Rei está rota, esto no se decide: se rompe desde adentro. */
            if (etapaDe(s->afinidad[P_REI]) == ETAP_HOSTIL) {
                printf("  Kaworu aparece sin que lo llames. Como siempre.\n");
                printf("  \"Vamos\", dice.\n");
                printf("  Y entonces Rei está en la puerta.\n");
                printf("  \"No.\"\n");
                printf("  Una palabra. Y Kaworu deja de moverse por completo.\n");
                printf("  \"sabía que ibas a decir eso\", te dice Rei. Y sale.\n");
                printf("  Kaworu se mira las manos.\n");
                printf("  \"Bueno\", dice. \"Bueno.\"\n");
                g->memoria[P_REI].veces_ayudada++;   /* el veto es suyo, no mio */
                s->animo    -= 12;
                s->confianza -= 6;
                break;
            }

            if (!vinculoDisponible(g, P_KAWORU, 0)) {
                printf("  Kaworu no aparece. Nadie te explica por qué.\n");
                printf("  Te quedas en la puerta del EVA con la mano en el cierre.\n");
                s->animo -= 8;
                break;
            }

            printf("  Kaworu aparece sin que lo llames.\n");
            printf("  \"¿Tú también lo estás pensando?\" dice, como si fuera obvio.\n");
            if (confirmar("Le pides que se vaya contigo?")) {
                printf("  Asiente. No pregunta a donde. \"Vamos.\"\n");
                g->memoria[P_KAWORU].veces_ayudada++;
                subirAfinidad(g, P_KAWORU, 20);
                s->rebeldia  += 10;
                s->confianza -= 5;
            } else {
                printf("  \"Entonces no.\" Y se va. Y no insiste.\n");
                g->memoria[P_KAWORU].veces_rechazada++;
                s->confianza += 5;
                s->rebeldia  += 2;
            }
            break;
        }

        case 28:
            printf("  Ya no hay clase. El aula está vacía y con las sillas arriba.\n");
            printf("  Pasas por el pasillo igual, porque es el camino más corto.\n");
            printf("  No es el más corto. Es el único que ya sabes.\n");
            if (etapaDe(s->afinidad[P_REI]) >= ETAP_CERCANO) {
                printf("  Rei está en el pasillo. Te espera.\n");
                printf("  \"Shinji.\" Y después nada, y se quedan ahí los dos.\n");
                subirAfinidad(g, P_REI, 5);
                s->animo += 6;
            } else if (etapaDe(s->afinidad[P_REI]) <= ETAP_EVITADO) {
                printf("  Rei está en el pasillo. Cruza al otro lado.\n");
                s->animo -= 4;
            } else {
                printf("  El pasillo está vacío.\n");
            }
            break;

        case 29: {
            printf("  El tercer ángel ataca el Cuartel General.\n");
            /* Rei te avisa antes que nadie, si todavía te habla. */
            if (etapaDe(s->afinidad[P_REI]) >= ETAP_CERCANO)
                printf("  \"Shinji.\" Rei en la red, y no dice nada más. No hace falta.\n");
            if (s->sincronizacion < 55) {
                printf("  No estás sincronizado. El EVA no te responde y no puedes salir.\n");
                if (etapaDe(s->afinidad[P_MISATO]) >= ETAP_CERCANO)
                    printf("  Misato no pregunta por qué. Solo apaga la comunicación.\n");
                else
                    printf("  Mira todo por la pantalla, desde el bunker.\n");
                g->combatesPerdidos++;
                s->animo    -= 12;
                s->cobardia += 8;
                pausa();
                break;
            }
            printf("  El EVA-01 está listo y esta vez responde a la primera.\n");
            combate(g, "ÁNGEL EMBLEM", VIDA_EMBLEM, 14);
            break;
        }

        case 30:
            /* Último día del mes. El epílogo se calcula después de esto. */
            printf("  Último día del mes.\n");
            printf("  Mañana hay un ángel, o no hay un ángel. Eso ya no lo sabes.\n\n");

            if (etapaDe(s->afinidad[P_REI]) >= ETAP_CERCANO) {
                printf("  Rei aparece en tu puerta. No llama: golpea dos veces y espera.\n");
                printf("  \"Shinji. Mañana.\" Y se queda en el pasillo hasta que abras.\n");
                subirAfinidad(g, P_REI, 3);
            } else if (etapaDe(s->afinidad[P_REI]) <= ETAP_EVITADO) {
                printf("  No viene nadie a tu puerta.\n");
                printf("  Nadie toca tu puerta desde el día ocho.\n");
            } else {
                printf("  Tu puerta no la toca nadie.\n");
            }

            if (etapaDe(s->afinidad[P_KAWORU]) >= ETAP_AMIABLE) {
                printf("  En el teléfono hay un mensaje sin número.\n");
                printf("  Dice: \"viene un barco\". No lo abres hasta mañana.\n");
                subirAfinidad(g, P_KAWORU, 3);
            } else if (etapaDe(s->afinidad[P_KAWORU]) <= ETAP_EVITADO) {
                printf("  Hay mensajes sin número en el teléfono. No los abres.\n");
            }

            if (etapaDe(s->afinidad[P_ASUKA]) >= ETAP_CERCANO) {
                printf("  Asuka no te escribió. No hace falta que lo haga.\n");
            }
            break;

        default: {
            switch (arcoDelDia(d)) {
                case ARCO_1:
                    printf("  día tranquilo en Tokio-3. El mar suena igual que ayer.\n");
                    break;
                case ARCO_2:
                    printf("  Tokio-3 está tenso. Nadie dice nada en los comedores.\n");
                    if (d % 3 == 0)
                        printf("  En la escuela, Asuka dice algo sobre tu padre. Ritsuko no le responde.\n");
                    break;
                case ARCO_3:
                    printf("  Los ángeles llevan semanas apareciendo más seguido.\n");
                    printf("  La Quinta Calle está sobredimensionada. Nadie lo dice en voz alta.\n");
                    break;
                default:
                    printf("  Los adultos están en reuniones. Siempre hay reuniones.\n");
                    printf("  Rei te mira como si supiera lo que viene.\n");
                    break;
            }
            break;
        }
    }
}

/* -------------------------------------------------------------------------- */
/* Crisis mental                                                              */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* Crisis mental                                                              */
/* -------------------------------------------------------------------------- */

/*
 * Por qué están separadas.
 *
 * La versión anterior disparaba con animo < 20 y, medida sobre seis partidas
 * completas de 30 días, no ocurrió ni una vez: el ánimo arranca en 50, las
 * actividades lo suben y casi nunca baja de ese piso. Es decir, una escena
 * entera escrita y nunca vista.
 *
 * El problema no era la escena, era el gatillo. Ahora depende de las dos
 * estadísticas que de verdad se mueven cuando jugás mal una partida: cobardía
 * alta (nunca encarás nada) y confianza baja (no te quedaste con nadie). Ese
 * es el perfil de alguien que no puede con esto, no solo de alguien triste.
 */
typedef enum {
    CRISIS_NINGUNA = 0,
    CRISIS_HABITACION,   /* la habitación se achica: la clásica */
    CRISIS_EVA,          /* la cabina se cierra y seguís ahí */
    CRISIS_SILENCIO      /* llamaste a alguien y nadie atiende */
} Crisis;

/* Devuelve qué crisis corresponde a este día, o CRISIS_NINGUNA. */
static Crisis queCrisis(GameState* g) {
    const Shinji* s = &g->shinji;

    /* Nunca dos días seguidos: una crisis seguida de otra es ruido, no tensión. */
    if (g->ultimaCrisis == g->dia) return CRISIS_NINGUNA;

    /* Un piso alto de cobardía con poca confianza: no aguantás más. */
    if (s->cobardia >= 35 && s->confianza < 30)
        return (g->dia % 2 == 0) ? CRISIS_EVA : CRISIS_HABITACION;

    /* Después de perder un combate, si venís arrastrando la derrota. */
    if (g->combatesPerdidos > g->combatesGanados && s->animo < 45)
        return CRISIS_EVA;

    /* Con la confianza alta tampoco pasa nada: una crisis sin nadie a quién
       llamar es solo otra forma de sufrir. */
    if (s->confianza < 15 && s->animo < 40)
        return CRISIS_SILENCIO;

    return CRISIS_NINGUNA;
}

static void eventoMental(GameState* g, Crisis cual) {
    Shinji* s = &g->shinji;
    g->crisisCount++;
    g->ultimaCrisis = g->dia;

    seccion("CRISIS");
    switch (cual) {
        case CRISIS_EVA:
            printf("  Shinji no puede. La cabina se cierra y seguís adentro.\n");
            printf("  El EVA no responde. No es que no pueda: es que no le importa.\n");
            break;
        case CRISIS_SILENCIO:
            printf("  Shinji marca el número. Da libre.\n");
            printf("  Vuelve a marcarlo. Da libre otra vez.\n");
            break;
        default:
            printf("  Shinji no puede. La habitación se hace pequeña.\n");
            break;
    }
    printf("  1. Enfrentar lo que está pasando.\n");
    printf("  2. Escapar.\n");
    printf("  3. Buscar a alguien.\n");

    int e = leerEntero("  > ", 1, 3);
    if (e <= 0) e = 4;

    const char* como;
    switch (e) {
        case 1:
            printf("  Aguantas. La habitación deja de girar.\n");
            s->confianza += 12;
            s->animo     += 18;
            s->cobardia  -= 8;
            como = "Crisis: la enfrentaste.";
            break;

        case 2:
            printf("  Corres. El pasillo es largo y lo recorres seis veces.\n");
            s->animo     -= 12;
            s->cobardia  += 12;
            s->confianza -= 5;
            como = "Crisis: escapaste.";
            break;

        case 3: {
            /* Ir con quien esté más cerca. */
            int mejor = -1, mejorAf = 5;
            for (int i = 0; i < PERSONAJES; i++) {
                if (s->afinidad[i] > mejorAf) { mejorAf = s->afinidad[i]; mejor = i; }
            }
            if (mejor < 0) {
                printf("  No hay a quién llamar. Marcas el número y lo cortas.\n");
                s->animo     += 2;
                s->confianza -= 3;
                como = "Crisis: llamaste y no atendió nadie.";
            } else {
                printf("  Llamas a %s. No dices nada. No hace falta.\n", perfiles[mejor].nombre);
                printf("  \"¿Estás bien?\"  \"No.\"  \"Bueno.\"\n");
                s->animo     += 12;
                s->confianza += 6;
                subirAfinidad(g, mejor, 4);
                como = "Crisis: llamaste y alguien atendió.";
            }
            break;
        }

        default:
            printf("  No haces nada. Y la crisis gana.\n");
            s->animo    -= 18;
            s->cobardia += 10;
            como = "Crisis: no hiciste nada.";
            break;
    }
    normalizar(s);
    {
        char nota[NOTA_DIARIO];
        snprintf(nota, sizeof nota, "%s", como);
        anotar(g, nota);
    }
    pausa();
}

/* -------------------------------------------------------------------------- */
/* Consecuencias: el mundo reacciona a tus decisiones                        */
/* -------------------------------------------------------------------------- */

static void consecuencias(GameState* g) {
    Shinji* s = &g->shinji;
    int cambios = 0;

    /* --- Vínculos rotos: el mundo deja de ofrecerte cosas --- */
    for (int i = 0; i < PERSONAJES; i++) {
        Etapa e = etapaDe(s->afinidad[i]);
        if (e == ETAP_HOSTIL && g->dia % 5 == 0) {
            printf("  [%s ya no te habla. Ni de paso.]\n", perfiles[i].nombre);
            cambios++;
        } else if (e == ETAP_EVITADO && g->dia % 4 == 0) {
            printf("  [%s te contesta por educación. Se nota.]\n", perfiles[i].nombre);
            cambios++;
        }
    }

    /* Un vínculo roto se lleva un poco de ti cada día. */
    for (int i = 0; i < PERSONAJES; i++) {
        if (etapaDe(s->afinidad[i]) <= ETAP_EVITADO) {
            s->confianza -= 1;
            s->animo     -= 1;
        }
    }

    /* --- Consecuencias concretas --- */
    if (s->afinidad[P_ASUKA] < 8 && s->confianza < 20 && g->dia > 12) {
        printf("  [Asuka ya no está en el gimnasio cuando llegas.]\n");
        cambios++;
    }
    if (s->afinidad[P_REI] < 5 && g->dia > 15) {
        printf("  [Rei dejo de esperarte en el pasillo.]\n");
        cambios++;
    }
    if (s->confianza < 12 && g->dia > 20) {
        printf("  [Misato cerro tu expediente. Nadie te avisa de nada.]\n");
        cambios++;
    }
    /* Reiniciar un vínculo roto cuesta caro. */
    if (etapaDe(s->afinidad[P_REI]) == ETAP_EVITADO && g->dia > 18) {
        printf("  [Rei te ve pasar y elige otro pasillo.]\n");
        cambios++;
    }

    for (int i = 0; i < PERSONAJES; i++) {
        if (etapaDe(s->afinidad[i]) == ETAP_UNIDO && (g->dia % 7) == 0) {
            printf("  [%s te manda un mensaje a la noche. Sin motivo aparente.]\n",
                   perfiles[i].nombre);
            cambios++;
            break;
        }
    }
    if (cambios == 0)
        printf("  Nada cambia alrededor de ti. Por ahora.\n");
}

/* -------------------------------------------------------------------------- */
/* Fin de día                                                                 */
/* -------------------------------------------------------------------------- */

static void finDeDia(GameState* g) {
    g->shinji.energia = clampi(g->shinji.energia + 5, 0, ENERGIA_MAX);
    normalizar(&g->shinji);

    {
        Crisis cual = queCrisis(g);
        if (cual != CRISIS_NINGUNA) eventoMental(g, cual);
    }

    compactarMisiones(g);
    consecuencias(g);
    chequearLogros(g);

    /* El snapshot va último a propósito: consecuencias() puede mover afinidad,
       y si el diario se congelara antes, ese cambio quedaría anotado hoy pero
       se mostraría mañana como si fuera de mañana. */
    cerrarDiaEnDiario(g);
}

static void mostrarAyuda(void) {
    seccion("CÓMO SE JUEGA");
    printf("  %sCada día haces UNA actividad. No se puede repetir.%s\n\n", cc(C_DIM), RST);

    printf("  %sENERGÍA%s\n", cc(C_YELL), RST);
    printf("    Es el límite. Entrenar, estudiar, sincronizar y socializar la gastan.\n");
    printf("    Descansar la recupera. Con menos de 10, el día se te va solo.\n\n");

    printf("  %sAFINIDAD%s  %s(de -100 a +100)%s\n", cc(C_YELL), RST, cc(C_GREY), RST);
    static const char* etapas[] = {
        "HOSTIL", "EVITADO", "DESCONOCIDO", "CONOCIDO", "AMIABLE", "CERCANO", "UNIDO"
    };
    static const char* rango[] = {
        "-30 o menos", "-10 o menos", "cerca de 0", "10 a 24",
        "25 a 49", "50 a 74", "75 o más"
    };
    static const char* tinte[] = {
        C_RED, C_RED, C_GREY, C_BLUE, C_CYAN, C_GREEN, C_GREEN
    };
    for (int i = 0; i < 7; i++)
        printf("    %s%-12s%s %s\n", cc(tinte[i]), etapas[i], RST, rango[i]);
    printf("    %sUn vínculo roto no se arregla insistiendo: cuesta caro y rinde menos.%s\n\n",
           cc(C_DIM), RST);

    printf("  %sMEMORIA%s\n", cc(C_YELL), RST);
    printf("    Cada persona recuerda lo que le hiciste. Rechazar tiene costo, y\n");
    printf("    el costo no es un número: es que la próxima vez no esté.\n\n");

    printf("  %sRENDIMIENTOS%s\n", cc(C_YELL), RST);
    printf("    Entrenar da cada vez menos. Nadie llega a 100 en un mes.\n\n");

    printf("  %sSe guarda solo al final de cada día. Escribí 'c' para apagar el color.%s\n",
           cc(C_GREY), RST);
}

/* -------------------------------------------------------------------------- */
/* Menu del día                                                               */
/* -------------------------------------------------------------------------- */

/* Elige ranura para guardar o cargar. Muestra qué hay en cada una. */
static int elegirRanura(const char* accion) {
    char prompt[64];
    seccion("RANURAS");
    for (int s = 1; s <= MAX_RANURAS; s++) {
        int dia = 0;
        if (peekPartida(s, &dia)) {
            printf("  %s%d%s  %s  ·  día %d de %d%s\n",
                   cc(C_BOLD), s, RST, cc(C_GREY), dia, DIAS_TOTALES, RST);
        } else {
            printf("  %s%d%s  %s%s%s\n",
                   cc(C_BOLD), s, RST, cc(C_GREY), "vacía", RST);
        }
    }
    snprintf(prompt, sizeof prompt, "\n  Ránura para %s > ", accion);
    return leerEntero(prompt, 1, MAX_RANURAS);
}

static void menuDia(GameState* g) {
    int hayCita = 0;
    for (int i = 0; i < g->citasCount; i++)
        if (!g->citas[i].fueVisitada && g->citas[i].diaDisponible == g->dia) hayCita = 1;

    for (;;) {
        printf("\n");
        regla();
        printf("  %s¿QUÉ HACES HOY?%s\n", cc(C_BOLD), RST);
        regla();
        printf("     1  Realizar actividad del día%s\n",
               g->actividadHecha ? "  \x1b[2m(ya hecho)\x1b[0m" : "");
        printf("     2  Ver misiones\n");
        printf("     3  Ver vínculos\n");
        printf("     4  Citas de hoy%s\n", hayCita ? "  \x1b[33m*\x1b[0m" : "");
        printf("     5  Ver logros\n");
        printf("     6  Guardar\n");
        printf("  %s  7%s  Terminar el día\n", cc(C_BOLD), RST);
        printf("     8  Cómo se juega\n");
        printf("     9  Leer el diario%s\n",
               (g->diarioCount > 0) ? "" : "  \x1b[2m(vacío)\x1b[0m");

        /* "Terminar el día" se queda en el 7: es lo que más se toca, y moverlo
           obliga a reconfigurar el dedo de quien ya jugaba. */
        int op = leerEntero("\n  ▸ ", 1, 9);
        if (op < 0) { g->dia = DIAS_TOTALES + 1; return; }   /* entrada cerrada */
        if (op == 7) return;

        switch (op) {
            case 1: realizarActividad(g); break;
            case 2: mostrarMisiones(g); pausa(); break;
            case 3: mostrarEstadisticas(g); pausa(); break;
            case 4: menuCitas(g); break;
            case 5: mostrarLogros(g, 1); pausa(); break;
            case 6: {
                int ranura = elegirRanura("guardar");
                if (ranura > 0) guardarPartida(g, ranura);
                break;
            }
            case 8: mostrarAyuda(); pausa(); break;
            case 9: menuDiario(g); pausa(); break;
            default: break;
        }
    }
}

/* -------------------------------------------------------------------------- */
/* Finales                                                                    */
/* -------------------------------------------------------------------------- */

static int mejorVinculo(const GameState* g, int* valor) {
    int mejor = -1, v = -1;
    for (int i = 0; i < PERSONAJES; i++) {
        if (g->shinji.afinidad[i] > v) { v = g->shinji.afinidad[i]; mejor = i; }
    }
    *valor = v;
    return mejor;
}

static void epilogoAncla(int mejor) {
    switch (mejor) {
        case P_REI:
            printf("  Rei no lo dice con palabras. Rei no dice las palabras.\n");
            printf("  Se queda en la puerta hasta que duermes. Todas las noches.\n");
            printf("  Y a la mañana siguiente está ahí de nuevo.\n");
            printf("  Para Rei, eso es querer a alguien.\n");
            break;
        case P_KAWORU:
            printf("  Kaworu se va un día, sin avisar, como vino.\n");
            printf("  Pero antes te deja un walkman con una pista guardada. Sin nombre.\n");
            printf("  La escuchaste mil veces. Nunca te aburriste de ella.\n");
            printf("  A veces te parece que la canción habla de ti.\n");
            break;
        case P_ASUKA:
            printf("  Asuka se burla de ti todo el día y entrena a tu lado.\n");
            printf("  Un día le dices gracias y se pone roja y te grita.\n");
            printf("  Al día siguiente sigue entrenando a tu lado como si nada.\n");
            printf("  No lo va a cambiar. Eso es lo suyo.\n");
            break;
        case P_MISATO:
            printf("  Misato sigue gritando por radio. Sigue sin llegar a tiempo.\n");
            printf("  Pero ahora te llama antes que a los demas, y espera tu voz.\n");
            printf("  Nadie lo comento. Nadie lo tiene que comentar.\n");
            break;
        case P_MARI:
            printf("  Mari sigue pasaándote un disco peor cada semana.\n");
            printf("  Y sigue contestando el teléfono a las tres de la mañana.\n");
            printf("  Shinji, eso es todo lo que hace falta.\n");
            break;
        case P_RITSUKO:
            printf("  Ritsuko dejo de llamarte Shinji-kun.\n");
            printf("  Empezó a llamarte Shinji. Un día, en una reunión, sin darse cuenta.\n");
            printf("  No lo corrigió. Y tú tampoco.\n");
            break;
        case P_MANA:
            printf("  Mana sigue diciendo delante de todos que eres su mejor amigo.\n");
            printf("  Ya no molesta. Es parte de cómo funciona el mundo, ahora.\n");
            break;
        default:
            printf("  Maya te sigue dejando el te sin decir nada.\n");
            printf("  Y sigue en cada visita te pregunta otra vez cómo duermes.\n");
            break;
    }
}

static int calcularFinal(const GameState* g) {
    const Shinji* s = &g->shinji;
    int v;
    int mejor = mejorVinculo(g, &v);

    int finalSecreto = (s->afinidad[P_REI]  >= 50 &&
                        s->afinidad[P_KAWORU] >= 50 &&
                        s->sincronizacion >= 70);

    /* Rei, Asuka y Misato: los tres que de verdad tenían algo contigo.
   Mari y Ritsuko no entran: tienen días que no dependen de tu elección, así
   que nunca llegan a estar del todo en contra. */
    int todosEnContra =
        etapaDe(s->afinidad[P_REI])     <= ETAP_EVITADO &&
        etapaDe(s->afinidad[P_ASUKA])   <= ETAP_EVITADO &&
        etapaDe(s->afinidad[P_MISATO])  <= ETAP_EVITADO;

    limpiarPantalla();
    printf("\n\n################################################################\n");
    printf("#               DÍA %d / %d -- RESULTADO FINAL                  #\n",
           DIAS_TOTALES, DIAS_TOTALES);
    printf("################################################################\n\n");

    printf("  Fuerza %d    Inteligencia %d    Ánimo %d    sincronización %d\n",
           s->fuerza, s->inteligencia, s->animo, s->sincronizacion);
    printf("  Confianza %d    cobardía %d    rebeldía %d\n",
           s->confianza, s->cobardia, s->rebeldia);
    printf("  Misiones %d    Citas %d    Combates ganados %d    Perdidos %d    Crisis %d\n\n",
           g->misionesCompletadas, g->citasCompletadas, g->combatesGanados,
           g->combatesPerdidos, g->crisisCount);

    printf("  ---- EPÍLOGO ----\n\n");

    /* Cada rama deja el indice del final; la cola común se imprime después. */
    int fin = 7;   /* por defecto: TOKIO-3 */

    if (finalSecreto) {
        printf("  [SECRETO] Alguien vino a Tokio-3 para buscar a Shinji Ikari.\n");
        printf("  No vino a NERV. Vino a el.\n\n");
        printf("  Rei estaba ahí primero, como si lo supiera.\n");
        printf("  \"Kaworu dijo que vendrías.\"\n");
        printf("  Y después Kaworu, que no está en ningún registro de ningún lado.\n\n");
        printf("  Los tres en la misma habitación. Dos de ellos no son de este mundo,\n");
        printf("  y ninguno lo menciona. Shinji se da cuenta. Shinji no pregunta.\n\n");
        printf("  \"Tú no estás perdido\", dice Rei.\n");
        printf("  \"No. perdí una vez y cerré la puerta por dentro\", dice el otro.\n");
        printf("  \"Bueno\", dice Rei. Y Shinji entiende que eso era una aceptación.\n\n");
        printf("  A la mañana siguiente, el Cuartel General de NERV está vacío.\n");
        printf("  No hay informe. No hay ángeles. No hay padre.\n");
        printf("  Solo un EVA-01 que sigue ahí, esperando, con la cabina abierta.\n\n");
        printf("  Shinji entra. Esta vez no porque se lo pidan.\n\n");
        printf("  [FIN SECRETO: UNIDO]\n");
        fin = 0;
    } else if (s->sincronizacion >= 80 && s->cobardia >= 45) {
        printf("  Lo lograste: sincronización casi total.\n");
        printf("  Y el precio de eso fue dejar de tener nombre propio.\n\n");
        printf("  En la sala de control dicen \"Unidad Uno, adelante\". No Shinji.\n");
        printf("  El EVA responde al instante, sin retraso y sin duda.\n");
        printf("  A veces tardas tres segundos en saber cuál era tu opinión.\n\n");
        printf("  Rei te busca y no encuentra a quién preguntar por ti.\n");
        printf("  \"¿Estás bien?\" te pregunta igual.\n");
        printf("  \"Si\", dices. Y es la primera vez que no del todo.\n\n");
        printf("  [FIN: INSTRUMENTO]\n");
        fin = 1;
    } else if (s->rebeldia >= 45 && s->confianza >= 25) {
        printf("  Un día desapareciste. Con la Asuka a cuestas, si estabas unido a ella.\n");
        printf("  Misato se hizo cargo. Entre las dos cosas, se hizo cargo.\n\n");
        printf("  Vives en otro lugar. No tienes que sincronizarte con nada.\n");
        printf("  Nadie te trae a la reunión del lunes.\n\n");
        printf("  Al principio lo llamaste libertad. Después, solamente estar.\n");
        printf("  [FIN: FUERA]\n");
        fin = 2;
    } else if (todosEnContra) {
        printf("  No fue el EVA lo que te dejo solo.\n\n");
        printf("  Hubo tres personas en Tokio-3 que podían pelear por ti, y las tres\n");
        printf("  desaparecieron.\n");
        printf("  Rei cruzo la puerta y no te saludo. Asuka dejo de escribirte.\n");
        printf("  Misato cerro tu expediente sin avisarte. Ninguna te lo dijo de frente.\n\n");
        printf("  Nadie te dejo plantado. Te dejaron de necesitar.\n");
        printf("  Que es peor, porque no hay a quien reclamar.\n\n");
        printf("  El informe de NERV dice que tu perfil emocional es \"irrecuperable\".\n");
        printf("  No está equivocado. Solo le falta la parte de por qué.\n\n");
        printf("  Shinji Ikari, 15 años, tiene un expediente impecable y\n");
        printf("  ninguna persona a la que llamar.\n");
        printf("  [FIN: SIN NADIE]\n");
        fin = 3;
    } else if (s->cobardia >= 65) {
        printf("  No pilotaste. Después del primer combate no pilotaste nunca más.\n\n");
        printf("  Te quedaste en Tokio-3: estudiando, mirando los informes de los demas.\n");
        printf("  Asuka dejo de escribirte. Rei dejo de esperarte.\n");
        printf("  NERV nunca te lo reprocho, y eso fue peor que si lo hubieran hecho.\n\n");
        printf("  A los 30, Shinji Ikari sigue en su búnker.\n");
        printf("  Y sigue sin responder el teléfono.\n");
        printf("  [FIN: NO PILOTO]\n");
        fin = 4;
    } else if (s->confianza >= 45 && mejor >= 0 && v >= 50) {
        printf("  Lo que te quedó fue una persona.\n\n");
        printf("  No un título, no un informe, no una unidad de combate.\n");
        printf("  Uno que se queda cuando la habitación se hace chica.\n\n");
        epilogoAncla(mejor);
        printf("\n  [FIN: ANCLA]\n");
        fin = 5;
    } else if (s->sincronizacion >= 60 && s->confianza >= 30) {
        printf("  Volviste a pilotear. No porque te lo pidieran.\n");
        printf("  Porque un día te bajaste del EVA y no sentiste nada al bajarte.\n\n");
        printf("  El informe dice: \"respuesta emocional estable\". Por primera vez,\n");
        printf("  eso no suena a un problema.\n\n");
        printf("  Rei está en el otro EVA. Asuka también. Los tres saben lo que hacen.\n");
        printf("  Nadie va a ir a buscarlos a casa.\n\n");
        printf("  [FIN: VOLVER]\n");
        fin = 6;
    } else {
        printf("  Sobreviviste. Ese es el final más raro de todos.\n\n");
        printf("  No llegaste a nada especial. No te rompiste del todo tampoco.\n");
        printf("  Los adultos te miran raro. Los compañeros te tratan normal.\n");
        printf("  En Tokio-3 desapareció la mitad de la ciudad en un día y lo seguimos\n");
        printf("  normalmente. Eso es lo que hacen los que sobreviven.\n\n");
        printf("  Shinji Ikari, 15 años, sigue sin saber muy bien por qué está ahí.\n");
        printf("  Pero está.\n");
        printf("  [FIN: TOKIO-3]\n");
        fin = 7;
    }

    /* En el final "SIN NADIE" no va: contradice lo que acaba de decir el texto. */
    if (mejor >= 0 && v > 0 && !finalSecreto && !todosEnContra)
        printf("\n  Vínculo más fuerte: %s (%d, %s)\n",
               perfiles[mejor].nombre, v, nombresEtapa[etapaDe(v)]);

    if (g->crisisCount > 0)
        printf("  (Crisis atravesadas: %d)\n", g->crisisCount);
    return fin;
}

/* -------------------------------------------------------------------------- */
/* Finales vistos: se guardan aparte, para que el rejuego tenga objetivo       */
/* -------------------------------------------------------------------------- */

#define FINAL_TOTAL 8

static const char* nombresFinales[FINAL_TOTAL] = {
    "UNIDO (secreto)", "INSTRUMENTO", "FUERA", "SIN NADIE",
    "NO PILOTO", "ANCLA", "VOLVER", "TOKIO-3"
};

/* El registro de finales vive junto al .exe, no en el directorio de trabajo:
   asi no se pierde al cambiar de carpeta y no ensucia donde se juega. */
static void rutaFinales(char* buf, size_t n) {
#ifdef _WIN32
    char exe[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, exe, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        char* slash = strrchr(exe, '\\');
        if (slash) {
            *slash = '\0';
            snprintf(buf, n, "%s\\finales.dat", exe);
            return;
        }
    }
#endif
    snprintf(buf, n, "finales.dat");
}

/* Los finales se indexan en este orden; cada uno tiene su bit. */
static int leerFinalesVistos(unsigned int* mascara) {
    char ruta[MAX_PATH + 32];
    rutaFinales(ruta, sizeof ruta);

    FILE* f = fopen(ruta, "rb");
    if (!f) { *mascara = 0; return 0; }
    unsigned int m = 0;
    size_t leidos = fread(&m, sizeof m, 1, f);
    fclose(f);
    *mascara = (leidos == 1) ? m : 0;
    return 1;
}

static void guardarFinalesVistos(unsigned int mascara) {
    char ruta[MAX_PATH + 32];
    rutaFinales(ruta, sizeof ruta);

    FILE* f = fopen(ruta, "wb");
    if (!f) return;
    fwrite(&mascara, sizeof mascara, 1, f);
    fclose(f);
}

static int contarFinales(unsigned int m) {
    int n = 0;
    for (int i = 0; i < FINAL_TOTAL; i++) if (m & (1u << i)) n++;
    return n;
}

static void mostrarProgresoFinales(unsigned int mascara) {
    int n = contarFinales(mascara);
    seccion("FINALES DESCUBIERTOS");
    for (int i = 0; i < FINAL_TOTAL; i++) {
        if (mascara & (1u << i))
            printf("  %s✓%s  %s%-16s%s %sdescubierto%s\n",
                   cc(C_GREEN), RST, cc(C_BOLD), nombresFinales[i], RST, cc(C_GREY), RST);
        else
            printf("  %s·%s  %s%-16s%s %ssin ver%s\n",
                   cc(C_GREY), RST, cc(C_GREY), nombresFinales[i], RST, cc(C_GREY), RST);
    }
    if (n >= FINAL_TOTAL) {
        printf("\n  %sLos ocho. Ya no queda nada por ver.%s\n", cc(C_GREEN), RST);
        printf("  %sPuedes empezar de nuevo sabiendo cómo termina cada uno.%s\n",
               cc(C_GREY), RST);
    } else {
        printf("\n  %s%d de %d.%s Quedan %d.\n",
               cc(C_BOLD), n, FINAL_TOTAL, RST, FINAL_TOTAL - n);
    }
}

/* -------------------------------------------------------------------------- */
/* Main                                                                       */
/* -------------------------------------------------------------------------- */

int main(void) {
    prepararConsola();
    srand((unsigned int)time(NULL));

    GameState g;
    memset(&g, 0, sizeof g);
    int ranura = 1;
    unsigned int finalesVistos = 0;
    leerFinalesVistos(&finalesVistos);

    limpiarPantalla();
    printf("\n");
    caja(NOMBRE_PROYECTO, "treinta días en Tokio-3 · un chico · ocho vínculos");

    printf("\n  %s1%s  Nueva partida\n", cc(C_BOLD), RST);
    printf("\n  %s2%s  Cargar partida guardada\n", cc(C_BOLD), RST);

    int yaVistos = contarFinales(finalesVistos);
    if (yaVistos > 0)
        printf("\n  %sFinales descubiertos: %d de %d%s\n",
               cc(C_GREY), yaVistos, FINAL_TOTAL, RST);

    int op = leerEntero("\n  ▸ ", 1, 2);
    if (op < 0) return 0;

    if (op == 2) {
        ranura = elegirRanura("cargar");
        if (ranura <= 0) { printf("\n  Nada que cargar.\n"); return 0; }
        if (!cargarPartida(&g, ranura)) {
            printf("  Esa ranura no se pudo leer. Empieza una nueva.\n");
            nuevaPartida(&g);
            ranura = 1;
        } else {
            printf("  %sPartida cargada · día %d · ranura %d%s\n",
                   cc(C_GREEN), g.dia, ranura, RST);
            pausa();
        }
    } else {
        ranura = elegirRanura("nueva");
        if (ranura <= 0) ranura = 1;
        nuevaPartida(&g);
    }

    while (g.dia >= 1 && g.dia <= DIAS_TOTALES) {
        limpiarPantalla();
        mostrarCabecera(&g);
        mostrarEstadisticas(&g);
        g.actividadHecha = 0;

        compactarMisiones(&g);
        eventoDelDia(&g);
        asignarMision(&g);
        menuDia(&g);

        if (g.dia > DIAS_TOTALES) break;   /* el jugador cerro la entrada */

        finDeDia(&g);

        /* Se avanza el día ANTES de guardar: el .dat tiene que contener el día
           siguiente para retomar, no el dia recien terminado. */
        g.dia++;
        printf("\n  %sdía %d terminado · autoguardado en la ranura %d%s\n",
               cc(C_GREY), g.dia - 1, ranura, RST);
        guardarPartida(&g, ranura);

        if (g.dia <= DIAS_TOTALES) pausa();
    }

    int idxFinal = calcularFinal(&g);

    /* Registrar el final para que la próxima vez sepas cuántos te faltan. */
    if (idxFinal >= 0 && idxFinal < FINAL_TOTAL && !(finalesVistos & (1u << idxFinal))) {
        finalesVistos |= (1u << idxFinal);
        guardarFinalesVistos(finalesVistos);
        printf("\n  %sFinal nuevo desbloqueado: %s%s\n",
               cc(C_GREEN), nombresFinales[idxFinal], RST);
    }

    printf("\n");
    seccion("LOGROS");
    mostrarLogros(&g, 1);

    mostrarProgresoFinales(finalesVistos);

    printf("\n  %sGracias por jugar.%s\n\n", cc(C_GREY), RST);
    pausa();
    return 0;
}
