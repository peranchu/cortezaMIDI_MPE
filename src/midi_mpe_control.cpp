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
    // Límites de calibración leídos desde tu estructura config original
    int minFader = (config.trillBarMinPresion <= 0) ? 450 : config.trillBarMinPresion;
    int maxFader = (config.trillBarMaxPresion <= 0) ? 6500 : config.trillBarMaxPresion;

    // Filtro defensivo de despegue antilatigazo exclusivo para el modo MPE
    if (numToques > 0 && presCruda < (minFader - 50))
    {
        numToques = 0;
    }

    if (numToques > 0)
    {
        // --- 1. PROCESADO CON DEDO PUESTO ---

        // Filtramos la presión con tu misma inercia analógica
        barPresionFiltradaMPE = (ALFA_BAR_PRES_MPE * presCruda) + ((1.0f - ALFA_BAR_PRES_MPE) * barPresionFiltradaMPE);
        int targetMidiPres = map(constrain((int)barPresionFiltradaMPE, minFader, maxFader), minFader, maxFader, 0, 127);
        uint8_t presionMIDI = (uint8_t)constrain(targetMidiPres, 0, 127);

        // GESTIÓN DEL ATAQUE (Primer instante de toque)
        if (!barDedoPuestoMPE)
        {
            barDedoPuestoMPE = true;
            barPuntoInicialXCrudo = posCruda;
            barUltimoXCrudoConNota = posCruda;
            barPresionFiltradaMPE = (float)presCruda;

            if (config.mpeBarModo == 0)
            {
                // 🔹 MODO CONTINUO (Afinación relativa / Violín)
                barNotaActualMPE = (uint8_t)constrain(config.mpeNotaBase, 0, 127);
                mpeEnviarNotaOn(barNotaActualMPE, 100, 2); // Dispara en Canal Miembro 2
                mpeEnviarPitchBendNormalizado(0.0f, 2);    // Centrado de inicio
            }
            else
            {
                // 🔸 MODO CUANTIZADO (Notas discretas de la escala)
                barNotaActualMPE = calcularNotaMpeCuantizada(posCruda);
                mpeEnviarNotaOn(barNotaActualMPE, 100, 2);
            }
        }

        // GESTIÓN DEL DESLIZAMIENTO (Expresión continua)
        if (config.mpeBarModo == 0)
        {
            // 🔹 Continuo: Medimos cuánto se desplaza el dedo desde el píxel de ataque inicial
            float desviacionNormalizada = ((float)posCruda - (float)barPuntoInicialXCrudo) / 3200.0f;
            mpeEnviarPitchBendNormalizado(desviacionNormalizada, 2);
        }
        else
        {
            // 🔸 Cuantizado: Cambio de nota (Legato) protegido con histéresis de 40 píxeles
            uint8_t notaCandidata = calcularNotaMpeCuantizada(posCruda);
            if (notaCandidata != barNotaActualMPE && abs(posCruda - barUltimoXCrudoConNota) > 40)
            {
                mpeEnviarNotaOff(barNotaActualMPE, 0, 2);
                barNotaActualMPE = notaCandidata;
                barUltimoXCrudoConNota = posCruda;
                mpeEnviarNotaOn(barNotaActualMPE, 100, 2);
            }
        }

        // Enviamos la tercera dimensión MPE (Aftertouch por presión)
        mpeEnviarPresionCanal(presionMIDI, 2);

        // 💡 TEST VISUAL SEGURO: Si el software responde a la presión, encendemos el LED en azul dinámico
        extern void setBoardColor(uint8_t r, uint8_t g, uint8_t b);
        setBoardColor(0, 0, presionMIDI > 10 ? presionMIDI : 10);
    }
    else
    {
        // --- 2. GESTIÓN DEL DESPEGUE (Release) ---
        if (barDedoPuestoMPE)
        {
            barDedoPuestoMPE = false;
            mpeEnviarNotaOff(barNotaActualMPE, 0, 2);

            if (config.mpeBarModo == 0)
            {
                mpeEnviarPitchBendNormalizado(0.0f, 2); // Devolvemos el tono al centro por seguridad
            }
            barNotaActualMPE = 0;
            barPuntoInicialXCrudo = -1;
            barUltimoXCrudoConNota = -1;

            // 💡 LED apagado (o a color base) al levantar el dedo
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
