#include "control_bordon.h"

// Enlazamos las funciones MPE nativas de tu proyecto
extern void mpeEnviarNotaOn(uint8_t nota, uint8_t velocidad, uint8_t canal);
extern void mpeEnviarNotaOff(uint8_t nota, uint8_t velocidad, uint8_t canal);
extern void mpeEnviarPitchBendNormalizado(float valor, uint8_t canal);
extern void mpeEnviarPresionCanal(uint8_t presion, uint8_t canal);

// Puentes seguros de controladores continuos en tu proyecto
extern void mpeEnviarRPN(uint8_t parametroMSB, uint8_t parametroLSB, uint8_t valorData, uint8_t canal);
extern void mpeEnviarCC74(uint8_t valor, uint8_t canal); 
extern void mpeEnviarCCGenerico(uint8_t numeroCC, uint8_t valor, uint8_t canal); 

// ============================================================================
// INSTANCIAS DE MEMORIA ÚNICAS Y PRIVADAS DEL MÓDULO
// ============================================================================
static EstadoBordon estado = { false, 0, 0.0f };
static const float ALFA_SQUARE_PRES_BORDON = 0.20f;

// Filtros de repetición locales para mantener el bus MIDI limpio
static int ultimoZ_Enviado = -1;
static int ultimoY_Enviado = -1;
static int ultimoX_Enviado = -1; 

// 🚀 VARIABLES DEL MOTOR DE VIBRATO (LFO POR SOFTWARE)
static float faseLFO = 0.0f;
static unsigned long ultimoTiempoLFO = 0;

void bordon_inicializar() 
{
    estado.dedoPuesto = false;
    estado.notaActiva = 0;
    estado.presionFiltrada = 0.0f;
    ultimoZ_Enviado = -1;
    ultimoY_Enviado = -1;
    ultimoX_Enviado = -1;
    faseLFO = 0.0f;
    ultimoTiempoLFO = 0;

    // Blindaje de dos zonas MPE para Ableton (Barra abajo, Bordón fijo arriba)
    mpeEnviarRPN(0, 6, 4, 1);
    mpeEnviarRPN(0, 6, 1, 16);
}

void procesarSquareBordonMPE(int xCrudo, int yCrudo, int presCruda, int numToques, 
                             uint8_t notaBordonMidi, uint8_t ccY, uint8_t ccZ)
{
    int minSquarePres = 250; 
    int maxSquarePres = 4000;

    if (numToques > 0 && presCruda < minSquarePres)
    {
        numToques = 0;
    }

    if (numToques > 0)
    {
        // ============================================================================
        // 1. FILTRADO Y MAPEO CONTINUO MPE
        // ============================================================================
        
        // 🔁 PRESIÓN (Z) -> Aftertouch MPE (0 a 127)
        estado.presionFiltrada = (ALFA_SQUARE_PRES_BORDON * presCruda) + ((1.0f - ALFA_SQUARE_PRES_BORDON) * estado.presionFiltrada);
        int targetPres = map(constrain((int)estado.presionFiltrada, minSquarePres, maxSquarePres), minSquarePres, maxSquarePres, 0, 127);
        uint8_t presionMIDI = (uint8_t)constrain(targetPres, 0, 127);

        // ↕️ VERTICAL (Y) -> Modulación de textura (ccY de tu menú)
        int targetY = map(constrain(yCrudo, 0, 1792), 0, 1792, 0, 127);
        uint8_t ccModulationMIDI = (uint8_t)constrain(targetY, 0, 127);

        // ↔️ HORIZONTAL (X) -> CC 74 MPE (Timbre / Brillo)
        int targetX = map(constrain(xCrudo, 0, 1792), 0, 1792, 0, 127);
        uint8_t cc74TimbreMIDI = (uint8_t)constrain(targetX, 0, 127);

        // ============================================================================
        // 2. DISPARO ÚNICO DEL BORDÓN MPE
        // ============================================================================
        if (!estado.dedoPuesto)
        {
            estado.dedoPuesto = true;
            estado.notaActiva = (uint8_t)notaBordonMidi; 
            
            faseLFO = 0.0f; // Reinicia el ciclo para que empiece centrado
            ultimoTiempoLFO = millis();
            
            mpeEnviarPitchBendNormalizado(0.0f, 15);
            mpeEnviarNotaOn(estado.notaActiva, 100, 15); // Canal 15 (Protegido)
        }

        // ============================================================================
        // 3. GENERADOR DE VIBRATO ORGÁNICO AUTOMÁTICO (LFO)
        // ============================================================================
        unsigned long tiempoActual = millis();
        float deltaTiempo = (tiempoActual - ultimoTiempoLFO) / 1000.0f; // Pasar a segundos
        ultimoTiempoLFO = tiempoActual;

        // 🎛️ VELOCIDAD DEL VIBRATO: 6.0f Hz (6 oscilaciones por segundo, muy natural)
        faseLFO += 2.0f * PI * 6.0f * deltaTiempo;
        if (faseLFO > 2.0f * PI) faseLFO -= 2.0f * PI;

        // 🎛️ INTENSIDAD DEL VIBRATO: Crece dinámicamente con la presión del dedo (Z)
        // El factor 0.015f crea una micro-desafinación acústica preciosa sin llegar a desafinar del todo
        float factorPresion = (float)presionMIDI / 127.0f; 
        float amplitudVibrato = factorPresion * 0.015f; 

        // Calculamos la onda de vaivén y la inyectamos en el Pitch Bend continuamente
        float desvioPitchBend = sin(faseLFO) * amplitudVibrato;
        mpeEnviarPitchBendNormalizado(desvioPitchBend, 15);

        // ============================================================================
        // 4. ENVÍO DE CONTROLES CONTINUOS FILTRADOS (Canal 15)
        // ============================================================================
        if ((int)presionMIDI != ultimoZ_Enviado) {
            ultimoZ_Enviado = (int)presionMIDI;
            mpeEnviarPresionCanal(presionMIDI, 15);
        }
        
        if ((int)ccModulationMIDI != ultimoY_Enviado) {
            ultimoY_Enviado = (int)ccModulationMIDI;
            mpeEnviarCCGenerico(ccY, ccModulationMIDI, 15); 
        }

        if ((int)cc74TimbreMIDI != ultimoX_Enviado) {
            ultimoX_Enviado = (int)cc74TimbreMIDI;
            mpeEnviarCC74(cc74TimbreMIDI, 15);
        }
    }
    else
    {
        // ============================================================================
        // 5. APAGADO DEL BORDÓN AL LEVANTAR EL DEDO
        // ============================================================================
        if (estado.dedoPuesto)
        {
            estado.dedoPuesto = false;
            mpeEnviarNotaOff(estado.notaActiva, 0, 15);
            estado.notaActiva = 0;
            
            // Retorno limpio a la afinación neutra perfecta y controles a cero
            mpeEnviarPitchBendNormalizado(0.0f, 15);
            mpeEnviarPresionCanal(0, 15);
            mpeEnviarCC74(0, 15); 
            mpeEnviarCCGenerico(ccY, 0, 15); 
            
            ultimoZ_Enviado = -1;
            ultimoY_Enviado = -1;
            ultimoX_Enviado = -1;
        }
    }
}
