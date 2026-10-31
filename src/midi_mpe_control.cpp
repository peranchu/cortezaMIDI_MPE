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
    float totalSemitonos = config.mpeRangoOctavas * 12.0f; // Ej: 2 octavas = 24 semitonos
    float factorPos = constrain((float)posCruda / 3200.0f, 0.0f, 1.0f);
    int notaCalculada = config.mpeNotaBase + (int)(factorPos * totalSemitonos);

    if (notaCalculada < 0)
        return 0;
    if (notaCalculada > 127)
        return 127;
    return (uint8_t)notaCalculada;
}

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
        // 1. Filtrado y mapeo de presión (Tu lógica intacta)
        barPresionFiltradaMPE = (ALFA_BAR_PRES_MPE * presCruda) + ((1.0f - ALFA_BAR_PRES_MPE) * barPresionFiltradaMPE);
        int targetMidiPres = map(constrain((int)barPresionFiltradaMPE, minFader, maxFader), minFader, maxFader, 0, 127);
        uint8_t presionMIDI = (uint8_t)constrain(targetMidiPres, 0, 127);

        // 2. MODO CONTINUO (RIBBON) TOTALMENTE AUTÓNOMO
        if (config.mpeBarModo == 0)
        {
            // Calculamos cuántos semitonos reales caben en la barra según tu menú de octavas
            float totalSemitonosMenu = config.mpeRangoOctavas * 12.0f;
            
            // Posición del dedo de 0.0 (izq) a 1.0 (der)
            float factorPosicion = constrain((float)posCruda / 3200.0f, 0.0f, 1.0f);
            
            // Nota flotante exacta: Nota Base + los semitonos que te desplaces
            float notaFlotanteReal = config.mpeNotaBase + (factorPosicion * totalSemitonosMenu);
            
            // La nota MIDI física que enviamos en el Note On es la parte entera
            uint8_t notaMidiFinal = (uint8_t)constrain((int)notaFlotanteReal, 0, 127);
            
            // La fracción sobrante (los decimales) es la desafinación fina entre tecla y tecla
            float fraccionSemitono = notaFlotanteReal - (int)notaFlotanteReal;

            // En MPE estándar, el Pitch Bend se inicializa a +/-48 semitonos por miembro.
            // Para movernos exactamente esa fracción de semitono, dividimos por 48.
            float desvioPitchBendMPE = fraccionSemitono / 48.0f;

            // GESTIÓN DEL ATAQUE O CAMBIO DE NOTA EN CONTINUO
            if (!barDedoPuestoMPE)
            {
                barDedoPuestoMPE = true;
                barNotaActualMPE = notaMidiFinal;
                mpeEnviarNotaOn(barNotaActualMPE, 100, 2);
            }
            else if (notaMidiFinal != barNotaActualMPE)
            {
                // Si el deslizamiento cruza la frontera de un semitono entero, 
                // hacemos un ligado limpio (legato) para actualizar la nota base en el sinte
                mpeEnviarNotaOff(barNotaActualMPE, 0, 2);
                barNotaActualMPE = notaMidiFinal;
                mpeEnviarNotaOn(barNotaActualMPE, 100, 2);
            }

            // Enviamos el micro-tono continuo calculado
            mpeEnviarPitchBendNormalizado(desvioPitchBendMPE, 2);
        }
        // 3. MODO KEYS (CUANTIZADO)
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
