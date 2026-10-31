#include "midi_mpe_control.h"
#include "sensor_midi_acell.h"

// Le decimos al compilador que use las funciones MPE que ya tienes escritas en tu otro archivo
extern void mpeEnviarNotaOn(uint8_t nota, uint8_t velocidad, uint8_t canal);
extern void mpeEnviarNotaOff(uint8_t nota, uint8_t velocidad, uint8_t canal);
extern void mpeEnviarPitchBendNormalizado(float valor, uint8_t canal);
extern void mpeEnviarPresionCanal(uint8_t presion, uint8_t canal);

// Variables de estado aisladas para la inercia y memoria del dedo en MPE (Modo Ribbon)
static bool barDedoPuestoMPE = false;
static uint8_t barNotaActualMPE = 0;
static int barPuntoInicialXCrudo = -1;
static int barUltimoXCrudoConNota = -1;
static float barPresionFiltradaMPE = 0.0;

// Reutilizamos el filtro de presión analógico (0.15) que ya tienes calibrado en tu barra
static const float ALFA_BAR_PRES_MPE = 0.15;

// ================================================================
// CALCULADORA AUXILIAR: CUANTIZACIÓN DE NOTAS EN ESCALA
// ================================================================
static uint8_t calcularNotaMpeCuantizada(int posCruda)
{
    // Mantenemos tus márgenes de confort de 100 píxeles para que sea ergonómico
    int posAjustada = constrain(posCruda, 100, 3100);
    float factorPos = (float)(posAjustada - 100) / 3000.0f;

    // 1. MODO CROMÁTICO (Igual que antes pero protegido)
    if (config.mpeEscala == ESCALA_CROMATICA)
    {
        float totalSemitonos = config.mpeRangoOctavas * 12.0f;
        int notaCalculada = config.mpeNotaBase + (int)(factorPos * totalSemitonos);
        return (uint8_t)constrain(notaCalculada, 0, 127);
    }

    // --- LÓGICA DE ESCALAS DIATÓNICAS/PENTATÓNICAS ---
    int sizeofEscala = 7;
    const uint8_t *intervalos = intervalosMayor;
    
    switch (config.mpeEscala)
    {
    case ESCALA_MENOR_NAT:
        sizeofEscala = 7;
        intervalos = intervalosMenorNat;
        break;
    case ESCALA_DORICA:
        sizeofEscala = 7;
        intervalos = intervalosDorica;
        break;
    case ESCALA_FRIGIA:
        sizeofEscala = 7;
        intervalos = intervalosFrigia;
        break;
    case ESCALA_MIXOLIDIA:
        sizeofEscala = 7;
        intervalos = intervalosMixolidia;
        break;
    case ESCALA_MENOR_ARM:
        sizeofEscala = 7;
        intervalos = intervalosMenorArm;
        break;
    case ESCALA_PENTATONICA:
        sizeofEscala = 5;
        intervalos = intervalosPentatonica;
        break;
    case ESCALA_PENTA_MENOR:
        sizeofEscala = 5;
        intervalos = intervalosPentaMenor;
        break;

    // Asignación de las nuevas escalas exóticas
    case ESCALA_CELTIC_AMARA:
        sizeofEscala = 7;
        intervalos = intervalosCeltic;
        break;
    case ESCALA_HIJAZ:
        sizeofEscala = 7;
        intervalos = intervalosHijaz;
        break;
    case ESCALA_AKEBONO:
        sizeofEscala = 5;
        intervalos = intervalosAkebono;
        break;
    case ESCALA_INSEN:
        sizeofEscala = 5;
        intervalos = intervalosInsen;
        break;
    case ESCALA_PENTATONICA_BLUES:
        sizeofEscala = 6;
        intervalos = intervalosBlues;
        break; // 6 Notas
    case ESCALA_RAGA_BHAIRAV:
        sizeofEscala = 7;
        intervalos = intervalosRaga;
        break;

    default:
        sizeofEscala = 7;
        intervalos = intervalosMayor;
        break;
    }

    int totalTeclasEnBarra = config.mpeRangoOctavas * sizeofEscala;

    // 💡 REPARTO SIMÉTRICO: Multiplicamos por los índices máximos posibles (totalTeclas - 1)
    // para que la última nota tenga exactamente el mismo ancho en píxeles que las demás.
    int teclaActual = (int)(factorPos * (totalTeclasEnBarra - 1));

    // Control defensivo por si acaso
    if (teclaActual < 0)
        teclaActual = 0;
    if (teclaActual >= totalTeclasEnBarra)
        teclaActual = totalTeclasEnBarra - 1;

    int octavaDeLaTecla = teclaActual / sizeofEscala;
    int indiceIntervalo = teclaActual % sizeofEscala;

    int notaCalculada = config.mpeNotaBase + (octavaDeLaTecla * 12) + intervalos[indiceIntervalo];

    return (uint8_t)constrain(notaCalculada, 0, 127);
}
/////////////////////////////////////////////////////

// ================================================================
// LÓGICA DE PROCESAMIENTO MPE COMPLETA PARA EL TRILL BAR
// ================================================================
void procesarBarMPE(int posCruda, int presCruda, int numToques)
{
    int minFader = (config.trillBarMinPresion <= 0) ? 450 : config.trillBarMinPresion;
    int maxFader = (config.trillBarMaxPresion <= 0) ? 6500 : config.trillBarMaxPresion;

    if (numToques > 0 && presCruda < (minFader - 50))
    {
        numToques = 0;
    }

    if (numToques > 0)
    {
        // ================================================================
        // 1. FILTRADO Y MAPEO DE PRESIÓN (Tu lógica original intacta)
        // ================================================================
        barPresionFiltradaMPE = (ALFA_BAR_PRES_MPE * presCruda) + ((1.0f - ALFA_BAR_PRES_MPE) * barPresionFiltradaMPE);
        int targetMidiPres = map(constrain((int)barPresionFiltradaMPE, minFader, maxFader), minFader, maxFader, 0, 127);
        uint8_t presionMIDI = (uint8_t)constrain(targetMidiPres, 0, 127);

        // ================================================================
        // 2. MODO 0: RIBBON CONTINUO INTELIGENTE POR ESCALAS
        // ================================================================
        if (config.mpeBarModo == 0)
        {
              // Posición del dedo de 0.0 (izq) a 1.0 (der) - Tu línea original literal
            float factorPosicion = constrain((float)posCruda / 3200.0f, 0.0f, 1.0f);

            uint8_t notaMidiFinal = 0;
            float fraccionSemitono = 0.0f;

            // Definimos la escala cromática como una matriz real para unificar la matemática
            static const uint8_t intervalosCromatica[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

            int sizeofEscala = 7;
            const uint8_t* intervalos = intervalosMayor;

            // 🧠 El switch ahora gobierna el 100% de los modos de forma directa y sin filtros externos
            switch (config.mpeEscala) 
            {
                case ESCALA_CROMATICA:   sizeofEscala = 12; intervalos = intervalosCromatica;  break;
                case ESCALA_MENOR_NAT:   sizeofEscala = 7; intervalos = intervalosMenorNat;    break;
                case ESCALA_DORICA:      sizeofEscala = 7; intervalos = intervalosDorica;      break;
                case ESCALA_FRIGIA:      sizeofEscala = 7; intervalos = intervalosFrigia;      break;
                case ESCALA_MIXOLIDIA:   sizeofEscala = 7; intervalos = intervalosMixolidia;   break;
                case ESCALA_MENOR_ARM:   sizeofEscala = 7; intervalos = intervalosMenorArm;    break;
                case ESCALA_PENTATONICA: sizeofEscala = 5; intervalos = intervalosPentatonica; break;
                case ESCALA_PENTA_MENOR: sizeofEscala = 5; intervalos = intervalosPentaMenor;  break;
                case ESCALA_CELTIC_AMARA:sizeofEscala = 7; intervalos = intervalosCeltic;       break;
                case ESCALA_HIJAZ:       sizeofEscala = 7; intervalos = intervalosHijaz;        break;
                case ESCALA_AKEBONO:     sizeofEscala = 5; intervalos = intervalosAkebono;      break;
                case ESCALA_INSEN:       sizeofEscala = 5; intervalos = intervalosInsen;        break;
                case ESCALA_PENTATONICA_BLUES: sizeofEscala = 6; intervalos = intervalosBlues;  break;
                case ESCALA_RAGA_BHAIRAV:sizeofEscala = 7; intervalos = intervalosRaga;         break;
                default:                 sizeofEscala = 7; intervalos = intervalosMayor;        break;
            }

            // Calculamos el tamaño virtual total del fader medido en "teclas de escala"
            float totalTeclasEscala = (float)config.mpeRangoOctavas * (float)sizeofEscala;
            float posicionTeclaFlotante = factorPosicion * (totalTeclasEscala - 1.0f);
            
            int indiceTeclaBase = (int)posicionTeclaFlotante;
            float fraccionEntreTeclas = posicionTeclaFlotante - (float)indiceTeclaBase;

            // Nota entera izquierda de la escala en este tramo físico
            int octavaBase = indiceTeclaBase / sizeofEscala;
            int notaEnEscala = indiceTeclaBase % sizeofEscala;
            float notaMidiBase = (float)config.mpeNotaBase + (float)(octavaBase * 12) + (float)intervalos[notaEnEscala] + (float)config.transposicion;

            // Nota entera derecha (siguiente peldaño de la escala) para calcular el ancho del salto
            int indiceTeclaSig = indiceTeclaBase + 1;
            if (indiceTeclaSig >= (int)totalTeclasEscala) indiceTeclaSig = (int)totalTeclasEscala - 1;
            
            int octavaSig = indiceTeclaSig / sizeofEscala;
            int notaEnEscalaSig = indiceTeclaSig % sizeofEscala;
            float notaMidiSig = (float)config.mpeNotaBase + (float)(octavaSig * 12) + (float)intervalos[notaEnEscalaSig] + (float)config.transposicion;

            // 🎯 ASIGNACIÓN ESTRICTA: El Note On se emite fijado en el escalón izquierdo real
            notaMidiFinal = (uint8_t)constrain((int)notaMidiBase, 0, 127);
            
            // Y multiplicamos la fracción decimal por el salto elástico en semitonos (ej. 3 semitonos enteros)
            fraccionSemitono = fraccionEntreTeclas * (notaMidiSig - notaMidiBase);

            // Dividimos por los 48 semitonos de tu motor MPE original intacto
            float desvioPitchBendMPE = fraccionSemitono / 48.0f;

            // GESTIÓN DEL ATAQUE O CAMBIO DE NOTA EN CONTINUO (Tu motor original literal)
            if (!barDedoPuestoMPE)
            {
                barDedoPuestoMPE = true;
                barNotaActualMPE = notaMidiFinal;
                mpeEnviarNotaOn(barNotaActualMPE, 100, 2);
            }
            else if (notaMidiFinal != barNotaActualMPE)
            {
                // Solo cambia de nota cuando el dedo cruza a un nuevo intervalo real de la escala activa
                mpeEnviarNotaOff(barNotaActualMPE, 0, 2);
                barNotaActualMPE = notaMidiFinal;
                mpeEnviarNotaOn(barNotaActualMPE, 100, 2);
            }

            // Enviamos la micro-afinación elástica resultante
            mpeEnviarPitchBendNormalizado(desvioPitchBendMPE, 2);
        }
        // ================================================================
        // 3. MODO 1: KEYS (CUANTIZADO) (Tu lógica original intacta)
        // ================================================================
        else
        {
            if (!barDedoPuestoMPE)
            {
                barDedoPuestoMPE = true;
                barNotaActualMPE = calcularNotaMpeCuantizada(posCruda);
                mpeEnviarNotaOn(barNotaActualMPE, 100, 2);
                barUltimoXCrudoConNota = posCruda;
            }

            uint8_t notaCandidata = calcularNotaMpeCuantizada(posCruda);
            if (notaCandidata != barNotaActualMPE && abs(posCruda - barUltimoXCrudoConNota) > 40)
            {
                mpeEnviarNotaOff(barNotaActualMPE, 0, 2);
                barNotaActualMPE = notaCandidata;
                barUltimoXCrudoConNota = posCruda;
                mpeEnviarNotaOn(barNotaActualMPE, 100, 2);
            }
        }

        mpeEnviarPresionCanal(presionMIDI, 2);

        extern void setBoardColor(uint8_t r, uint8_t g, uint8_t b);
        setBoardColor(0, 0, presionMIDI > 10 ? presionMIDI : 10);
    }
    else
    {
        // ================================================================
        // RELEVOS AL LEVANTAR EL DEDO (Tu lógica original intacta)
        // ================================================================
        if (barDedoPuestoMPE)
        {
            barDedoPuestoMPE = false;
            mpeEnviarNotaOff(barNotaActualMPE, 0, 2);

            if (config.mpeBarModo == 0)
            {
                mpeEnviarPitchBendNormalizado(0.0f, 2);
            }
            barNotaActualMPE = 0;

            extern void setBoardColor(uint8_t r, uint8_t g, uint8_t b);
            setBoardColor(0, 0, 0);
        }
    }
}
//////////////////////////////////////////////////

// ================================================================
// WATCHDOG DE SEGURIDAD (Evita notas colgadas al cambiar de menús)
// ================================================================
void mpeLimpiarVozBar(void)
{
    if (barDedoPuestoMPE)
    {
        barDedoPuestoMPE = false;
        if (barNotaActualMPE != 0)
        {
            mpeEnviarNotaOff(barNotaActualMPE, 0, 2);
            mpeEnviarPitchBendNormalizado(0.0f, 2);
        }
        barNotaActualMPE = 0;
        barPuntoInicialXCrudo = -1;
        barUltimoXCrudoConNota = -1;
    }
}
////////////////////////////////////////////////

// ================================================================
// CONFIGURACIÓN AUTOMÁTICA DE LA ZONA MPE AL CAMBIAR DE MODO
// ================================================================
void gestionarCambioDeModoMPE(void)
{
    static int modoAnterior = -1;
    if (config.modoMidi == modoAnterior)
        return;

    modoAnterior = config.modoMidi;

    // Al cambiar de modo siempre soltamos la voz del Bar para no dejar
    // notas colgadas ni pitch bends activos.
    mpeLimpiarVozBar();

    if (config.modoMidi == 1)
    {
        // Zona inferior: master canal 1 + 15 miembros (2..16).
        // Master +/-2 semitonos, cada miembro +/-48 semitonos.
        mpeConfigurarZona(15, 2, 48);
    }
    else
    {
        // MCM = 0: el puerto vuelve a comportarse como MIDI normal.
        mpeDesactivarZona();
    }
}
