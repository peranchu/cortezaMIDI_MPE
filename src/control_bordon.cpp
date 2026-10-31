#include "control_bordon.h"

// 🎯 ENLAZADO PERFECTO: Usamos únicamente las funciones reales que existen en tu proyecto
extern void mpeEnviarNotaOn(uint8_t nota, uint8_t velocidad, uint8_t canal);
extern void mpeEnviarNotaOff(uint8_t nota, uint8_t velocidad, uint8_t canal);
extern void mpeEnviarPitchBendNormalizado(float valor, uint8_t canal);
extern void mpeEnviarPresionCanal(uint8_t presion, uint8_t canal);

// Variables de memoria privadas aisladas del módulo
static EstadoBordon estado = { false, 0, 0.0f };
static const float ALFA_SQUARE_PRES_BORDON = 0.20f;

// Filtros de repetición locales para no saturar el bus MIDI
static int ultimoZ_Enviado = -1;
static int ultimoY_Enviado = -1;

// ============================================================================
// IMPLEMENTACIÓN DE FUNCIONES DEL MOTOR MPE
// ============================================================================

void bordon_inicializar() 
{
    estado.dedoPuesto = false;
    estado.notaActiva = 0;
    estado.presionFiltrada = 0.0f;
    ultimoZ_Enviado = -1;
    ultimoY_Enviado = -1;

    // 🌐 CONFIGURACIÓN DE ZONA MPE REFACTORIZADA (Canal 1 Maestro)
    // Inicializamos la zona MPE (RPN 00:06) asignando un bloque de 14 canales (2 al 15)
    mpeEnviarRPN(0, 6, 14, 1);
}


void procesarSquareBordonMPE(int yCrudo, int presCruda, int numToques, 
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
        // 1. FILTRADO Y MAPEO CONTINUO MPE
        // Suavizado de la presión para el colchón
        estado.presionFiltrada = (ALFA_SQUARE_PRES_BORDON * presCruda) + ((1.0f - ALFA_SQUARE_PRES_BORDON) * estado.presionFiltrada);
        int targetPres = map(constrain((int)estado.presionFiltrada, minSquarePres, maxSquarePres), minSquarePres, maxSquarePres, 0, 127);
        uint8_t presionMIDI = (uint8_t)constrain(targetPres, 0, 127);

        // Mapeo vertical (Y) convertido a un rango flotante continuo (-1.0 a 1.0) para el Pitch Bend del colchón
        // Trill Square tiene una resolución nativa de 1792 en el eje vertical
        float factorY = (float)constrain(yCrudo, 0, 1792) / 1792.0f;
        float desvioPitchBend = (factorY * 2.0f) - 1.0f; // Escala el valor para ir desde -1.0 (abajo) hasta +1.0 (arriba)

        // 2. DISPARO ÚNICO DEL BORDÓN MPE
        if (!estado.dedoPuesto)
        {
            estado.dedoPuesto = true;
            estado.notaActiva = notaBordonMidi; 
            // Usamos el canal 3 de forma fija para que actúe como una zona independiente en Ableton
            mpeEnviarNotaOn(estado.notaActiva, 100, 3); 
        }

        // 3. ENVÍO DE CONTROLES CONTINUOS NATIVOS MPE (Canal 3)
        // Presión del colchón (Z) vía Aftertouch/Channel Pressure MPE
        if ((int)presionMIDI != ultimoZ_Enviado) {
            ultimoZ_Enviado = (int)presionMIDI;
            mpeEnviarPresionCanal(presionMIDI, 3);
        }
        
        // Movimiento vertical (Y) vía Pitch Bend continuo MPE
        int chequeoY = (int)(factorY * 127.0f);
        if (chequeoY != ultimoY_Enviado) {
            ultimoY_Enviado = chequeoY;
            mpeEnviarPitchBendNormalizado(desvioPitchBend, 3);
        }
    }
    else
    {
        // 4. APAGADO DEL BORDÓN AL LEVANTAR EL DEDO
        if (estado.dedoPuesto)
        {
            estado.dedoPuesto = false;
            mpeEnviarNotaOff(estado.notaActiva, 0, 3);
            estado.notaActiva = 0;
            
            // Retorno limpio a la posición de reposo central y presión cero
            mpeEnviarPresionCanal(0, 3);
            mpeEnviarPitchBendNormalizado(0.0f, 3);
            
            ultimoZ_Enviado = -1;
            ultimoY_Enviado = -1;
        }
    }
}
