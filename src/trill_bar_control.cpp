#include "trill.h"
#include "trill_bar_control.h"
#include "sensor_midi_acell.h"
#include "ui_control.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include "midi_mpe_control.h"

// ================================================================
// ENLACES EXTERNOS CON EL HARDWARE Y LA INTERFAZ
// ================================================================
extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2; // Restaurado a tu pantalla SH1106 real
extern bool forzarRedibujado;

Trill trillBar;
int enviadoBarPos = -1;
int enviadoBarPres = -1;

// Variables globales para almacenar el valor MIDI actual (0-127)
int trillMidiBarPosActual = 0;
int trillMidiBarPresActual = 0;

// 🚀 COEFICIENTES DE SUAVIZADO INDEPENDIENTES (Ajusta a tu gusto)
// 0.25 = Transición cremosa y analógica / 1.0 = Filtro desactivado (original)
static const float ALFA_BAR_POS = 0.25;
static const float ALFA_BAR_PRES = 0.15;

// Variables de inercia y estado exclusivas para el motor MPE del Ribbon
static bool barDedoPuestoMPE = false;     // Guarda si hay un dedo físicamente apoyado en MPE
static uint8_t barCanalMPE = 0;           // El canal MIDI dinámico que usará la barra
static uint8_t barNotaActualMPE = 0;      // Guarda qué nota está sonando para poder apagarla luego
static int barPuntoInicialXCrudo = -1;    // Guarda el píxel exacto del primer toque (para el Pitch Bend relativo)
static int barUltimoXCrudoConNota = -1;   // Guarda el píxel de la última nota activada (para la histéresis)
static float barPresionFiltradaMPE = 0.0; // El acumulador para suavizar la presión en MPE

// Declaración externa para poder mantener vivo el Square durante las esperas de la calibración
extern void actualizarTrillSquare(void);

void inicializarTrillBar(void)
{
    int error = trillBar.setup(Trill::TRILL_BAR, TRILL_BAR_I2C_ADDR, &Wire1);
    if (error != 0)
    {
        Serial.println("❌ Error: No se encuentra el Trill Bar");
    }
    else
    {
        Serial.println("✅ Trill Bar inicializado correctamente.");
        trillBar.setPrescaler(3);
        delay(10);
    }
}

// ================================================================
// RUTINA DE CALIBRACIÓN SÍNCRONA ORIGINAL (RESTAURADA AL 100%)
// ================================================================
// Declaración externa para poder mantener vivo el Square durante las esperas de la calibración
extern void actualizarTrillSquare(void);

// ================================================================
// RUTINA DE CALIBRACIÓN MEJORADA CON RESPALDO DE BUS Y SUAVIZADO (CORREGIDA)
// ================================================================
void calibrarTrillBarDinamico(void)
{
    int muestras = 50;

    // ============== PASO 1: TOQUE SUAVE ==============
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_7x14_tf);
    u8g2.drawStr(10, 22, "CALIB. TRILL BAR");
    u8g2.drawHLine(10, 26, 110);
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(92, 11, "[EDIT]");
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.drawStr(15, 48, "1. TOQUE SUAVE");
    u8g2.sendBuffer();

    // Espera activa de 1.5s manteniendo vivo al Square para que no se sature el bus
    unsigned long tiempoEspera = millis();
    while (millis() - tiempoEspera < 1500)
    {
        actualizarTrillSquare();
    }

    long acumuladoSuave = 0;
    float presionSuaveFiltrada = 400.0; // Semilla inicial estándar de repos

    for (int i = 0; i < muestras; i++)
    {
        trillBar.read();
        int targetPres = (trillBar.getNumTouches() > 0) ? trillBar.touchSize(0) : 400;

        // Filtramos la muestra con el mismo coeficiente de tu funcionamiento normal (0.15)
        presionSuaveFiltrada = (ALFA_BAR_PRES * targetPres) + ((1.0 - ALFA_BAR_PRES) * presionSuaveFiltrada);
        acumuladoSuave += (long)presionSuaveFiltrada;

        // Pausa de 15ms sin congelar el Square de fondo
        unsigned long tMuestra = millis();
        while (millis() - tMuestra < 15)
        {
            actualizarTrillSquare();
        }
    }
    config.trillBarMinPresion = (acumuladoSuave / muestras) + 50;

    // ============== PASO 2: APRIETE FUERTE ==============
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_7x14_tf);
    u8g2.drawStr(10, 22, "CALIB. TRILL BAR");
    u8g2.drawHLine(10, 26, 110);
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(92, 11, "[EDIT]");
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.drawStr(15, 48, "2. APRIETE FUERTE");
    u8g2.sendBuffer();

    tiempoEspera = millis();
    while (millis() - tiempoEspera < 1500)
    {
        actualizarTrillSquare();
    }

    long acumuladoFuerte = 0;
    float presionFuerteFiltrada = 1000.0; // Semilla inicial estimada para toque fuerte

    for (int i = 0; i < muestras; i++)
    {
        trillBar.read();

        // 🚀 CORRECCIÓN: Declaramos targetPres correctamente aquí también
        int targetPres = (trillBar.getNumTouches() > 0) ? trillBar.touchSize(0) : 6000;

        // Filtramos la muestra para calcular el límite real que experimenta tu mano
        presionFuerteFiltrada = (ALFA_BAR_PRES * targetPres) + ((1.0 - ALFA_BAR_PRES) * presionFuerteFiltrada);
        acumuladoFuerte += (long)presionFuerteFiltrada;

        unsigned long tMuestra = millis();
        while (millis() - tMuestra < 15)
        {
            actualizarTrillSquare();
        }
    }
    config.trillBarMaxPresion = acumuladoFuerte / muestras;

    if (config.trillBarMaxPresion <= config.trillBarMinPresion)
    {
        config.trillBarMaxPresion = config.trillBarMinPresion + 2000;
    }

    // ============== PASO 3: GUARDADO Y ÉXITO ==============
    guardarLimpiosEnEEPROM();
    actualizarPantallaMuestra("CALIBRACION OK");

    tiempoEspera = millis();
    while (millis() - tiempoEspera < 1000)
    {
        actualizarTrillSquare();
    }

    forzarRedibujado = true;
}

// ================================================================
// PROCESAMIENTO AUXILIAR PARA CUANTIZACIÓN DE NOTAS EN ESCALA
// ================================================================
static uint8_t calcularNotaMpeCuantizada(int posCruda)
{
    float totalSemitonos = config.mpeRangoOctavas * 12.0f;              // Ej: 2 octavas = 24 semitonos
    float factorPos = constrain((float)posCruda / 3200.0f, 0.0f, 1.0f); // Posición de 0.0 a 1.0
    int notaCalculada = config.mpeNotaBase + (int)(factorPos * totalSemitonos);

    if (notaCalculada < 0)
        return 0;
    if (notaCalculada > 127)
        return 127;
    return (uint8_t)notaCalculada;
}
//////////////////////////////////////////

// ================================================================
// LECTURA SUAVIZADA CON TU LOGICA COMPLETA DE ENVIOS
// ================================================================
void actualizarTrillBar(void)
{
    trillBar.read();

    // 🚧 EL MURO DE SEPARACIÓN: Preguntamos el modo antes de hacer nada
    if (config.modoMidi == 1)
    {
        // ============================================================
        // MOTOR MPE SEPARADO: Desvío directo al nuevo módulo aislado
        // ============================================================
        int numToques = trillBar.getNumTouches();
        int posicionCruda = (numToques > 0) ? trillBar.touchLocation(0) : 0;
        int presionCruda = (numToques > 0) ? trillBar.touchSize(0) : 0;

        procesarBarMPE(posicionCruda, presionCruda, numToques);
    }
    else
    {
        // ============================================================
        // TU MODO CC TRADICIONAL NATIVO (100% INTACTO, VIRGEN Y SIN TOCAR)
        // ============================================================
        if (trillBar.getNumTouches() > 0)
        {
            int posicionCruda = trillBar.touchLocation(0);
            int presionCruda = trillBar.touchSize(0);

            // Tu blindaje nativo contra ceros en la EEPROM
            int minFader = (config.trillBarMinPresion <= 0) ? 450 : config.trillBarMinPresion;
            int maxFader = (config.trillBarMaxPresion <= 0) ? 6500 : config.trillBarMaxPresion;

            // Calculamos los objetivos MIDI intermedios
            int targetMidiPos = map(constrain(posicionCruda, 0, 3200), 0, 3200, 0, 127);
            int targetMidiPres = map(constrain(presionCruda, minFader, maxFader), minFader, maxFader, 0, 127);

            // 🚀 CURVAS DE SUAVIZADO: Aplicación de la inercia analógica a tus variables
            trillMidiBarPosActual  = (ALFA_BAR_POS  * targetMidiPos)  + ((1.0 - ALFA_BAR_POS)  * trillMidiBarPosActual);
            trillMidiBarPresActual = (ALFA_BAR_PRES * targetMidiPres) + ((1.0 - ALFA_BAR_PRES) * trillMidiBarPresActual);

            // Envío seguro utilizando las variables suavizadas
            evaluarYEnviarCC(config.trillBarCC_pos, trillMidiBarPosActual, enviadoBarPos, (unsigned char)config.canalMidi);
            evaluarYEnviarCC(config.trillBarCC_pres, trillMidiBarPresActual, enviadoBarPres, (unsigned char)config.canalMidi);
        } 
        else 
        {
            // Al levantar el dedo, la presión cae a cero de golpe para evitar colas de sonido
            trillMidiBarPresActual = 0; 
            
            if (enviadoBarPres != 0 && enviadoBarPres != -1) {
                int targetMidiPres = 0;
                evaluarYEnviarCC(config.trillBarCC_pres, targetMidiPres, enviadoBarPres, (unsigned char)config.canalMidi);
            }
        }
    }
}
