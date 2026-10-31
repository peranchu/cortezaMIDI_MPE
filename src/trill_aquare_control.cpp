#include <Arduino.h>
#include <Wire.h>
#include <Trill.h>
#include "trill_square_control.h"
#include "ui_control.h"
#include "sensor_midi_acell.h"
#include "control_bordon.h"

Trill trillSquare;

bool squareDetectado = false;
int squareMidiXActual = 0;
int squareMidiYActual = 0;
int squareMidiZActual = 0;

static int enviadoSquareX = -1;
static int enviadoSquareY = -1;
static int enviadoSquareZ = -1;

static unsigned long ultimoRefrescoSquare = 0;
const unsigned long INTERVALO_SQUARE = 20; // Cuidado no bajar de 15ms problemas con la lectura del chip

const uint8_t DIRECCION_SQUARE = 0x28;

void inicializarTrillSquare(void)
{
    if (trillSquare.setup(Trill::TRILL_SQUARE, DIRECCION_SQUARE, &Wire1) != 0)
    {
        Serial.println("❌ SQUARE no encontrado");
        squareDetectado = false;
    }
    else
    {
        Serial.println("✅ SQUARE OK");
        trillSquare.setMode(Trill::CENTROID);

        // 🚀 LIMPIEZA DE LÍNEA BASE PURA: Asegura que empiece desde 0 absoluto
        delay(50);
        trillSquare.updateBaseline();
        delay(50);

        squareDetectado = true;
    }
}

void actualizarTrillSquare(void)
{
    if (!squareDetectado)
        return;

    unsigned long tiempoActual = millis();
    if (tiempoActual - ultimoRefrescoSquare < INTERVALO_SQUARE)
        return;
    ultimoRefrescoSquare = tiempoActual;

    // Tu comando exacto de lectura de la librería
    trillSquare.read();

    // 🚀 COEFICIENTES DE SUAVIZADO (Ajusta entre 0.01 y 1.0)
    // 1.0 = Filtro desactivado (respuesta instantánea y cruda)
    // 0.1 = Suavizado muy fluido y analógico (elimina cualquier salto o temblor)
    const float ALFA_X = 0.50;
    const float ALFA_Y = 0.50;
    const float ALFA_Z = 0.10; // Un poco más reactivo para la presión

    // Capturamos el número de toques y la presión cruda para evaluar el despegue
    int numToques = trillSquare.getNumTouches();
    int zSqCrudo = (numToques > 0) ? trillSquare.touchSize(0) : 0;

    // 🚀 FILTRO DE DESPEGUE ANTILATIGAZO:
    // Si el sensor detecta toques pero la presión cruda es ridículamente baja (menor de 150),
    // consideramos que el dedo se está levantando y forzamos numToques a 0 para ir directo al else.
    // Esto ignora el último latigazo ruidoso de X provocado por el despegue a alta presión.
    if (numToques > 0 && zSqCrudo < 150)
    {
        numToques = 0;
    }

    // ============================================================================
    // 🎯 RAMA A: EL INSTRUMENTO ESTÁ EN MODO MPE GLOBAL
    // ============================================================================
    if (config.modoMidi == 1) // O el número de índice que use tu menú para activar el MPE
    {
         // Le pasamos las variables crudas de ambos ejes leídas del sensor físico
         int xSqCrudo = (numToques > 0) ? trillSquare.touchHorizontalLocation(0) : 0;
         int ySqCrudo = (numToques > 0) ? trillSquare.touchLocation(0) : 0;
        
        // 🎯 La nota base del bordon (24) se desplaza con tu menú (+ transposición) 
        // pero mantiene las 2 octavas de separación con la barra (48)
        uint8_t notaMidiCalculada = (uint8_t)constrain(config.mpeSquareNota + config.transposicion, 0, 127);

        // 🚀 PASAMOS LOS DATOS DIRECTOS: Limpio, seguro y libre de errores de tipo
        procesarSquareBordonMPE(xSqCrudo, ySqCrudo, zSqCrudo, numToques, 
                                notaMidiCalculada, 
                                (uint8_t)config.trillSquareCC_Y, 
                                (uint8_t)config.trillSquareCC_Z);  
    }

    // ============================================================================
    // 🎛️ RAMA B: EL INSTRUMENTO ESTÁ EN MODO ENVÍOS CC TRADICIONAL (Tu motor original)
    // ============================================================================
    else
    {
        if (numToques > 0)
        {
            int xSqCrudo = trillSquare.touchHorizontalLocation(0);
            int ySqCrudo = trillSquare.touchLocation(0);

            // Calculamos los objetivos en sentido natural (0 a 127)
            int targetX = map(constrain(xSqCrudo, 0, 1792), 0, 1792, 127, 0);
            int targetY = map(constrain(ySqCrudo, 0, 1792), 0, 1792, 127, 0);
            int targetZ = map(constrain(zSqCrudo, 150, 2000), 150, 2000, 127, 0);

            // Aplicamos el suavizado normal en los ejes estables
            squareMidiXActual = (ALFA_X * targetX) + ((1.0 - ALFA_X) * squareMidiXActual);
            squareMidiYActual = (ALFA_Y * targetY) + ((1.0 - ALFA_Y) * squareMidiYActual);

            // Tu inversión matemática limpia que funciona genial para el eje Z
            int zSuaveTemporal = (ALFA_Z * targetZ) + ((1.0 - ALFA_Z) * (127 - squareMidiZActual));
            squareMidiZActual = 127 - zSuaveTemporal;

            // Envío seguro de los CC
            evaluarYEnviarCC(config.trillSquareCC_X, squareMidiXActual, enviadoSquareX, (unsigned char)config.canalMidi);
            evaluarYEnviarCC(config.trillSquareCC_Y, squareMidiYActual, enviadoSquareY, (unsigned char)config.canalMidi);
            evaluarYEnviarCC(config.trillSquareCC_Z, squareMidiZActual, enviadoSquareZ, (unsigned char)config.canalMidi);
        }

        else
        {
            // 🚀 SOLUCIÓN AL CONGELADO: Cuando no hay toques, forzamos el reposo
            // Al levantar el dedo del Square, la presión cae a 0 inmediatamente
            if (squareMidiZActual != 0)
            {
                squareMidiZActual = 0;
                evaluarYEnviarCC(config.trillSquareCC_Z, 0, enviadoSquareZ, (unsigned char)config.canalMidi);

                // 2. 🎛️ CAPTURA DE SEGURIDAD PARA EL EJE X E Y:
                // Al levantar el dedo, obligamos a las variables enviadas a emparejarse con el valor analógico actual.
                // Esto "congela" los faders MIDI en su posición real de despegue, impidiendo que salten a 127.
                enviadoSquareX = squareMidiXActual;
                enviadoSquareY = squareMidiYActual;
            }
        }
    }
}

