#include "sensor_midi_acell.h"
#include "ui_control.h"
#include "encoder_control.h"
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>

#define RGB_PIN 16
#define NUM_PIXELS 1

Adafruit_NeoPixel pixels(NUM_PIXELS, RGB_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_USBD_MIDI usb_midi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI_OBJ);
Adafruit_MPU6050 mpu;

int ultimoMidiX = 0;
int ultimoMidiY = 0;
int ultimoMidiZ = 0;

extern bool mpu_detectado;
extern int lastMidiX, lastMidiY, lastMidiZ;
unsigned long lastTime = 0;

float anguloX = 0.0f;
float anguloY = 0.0f;
float anguloZ = 0.0f;

float smoothedX = 64.0f;
float smoothedY = 64.0f;
float smoothedZ = 64.0f;
bool primer_filtrado = true;

float anguloCentroX = 0.0f;
float anguloCentroY = 0.0f;
float xMinLimit = -20.0f;
float xMaxLimit = 20.0f;
float yMinLimit = -20.0f;
float yMaxLimit = 20.0f;

// Definición de las zonas muertas en grados (ajustables a tu gusto)
const float DEADZONE_X = 2.0f; // ±2 grados de margen adelante/atrás
const float DEADZONE_Y = 2.0f; // ±2 grados de margen izquierda/derecha
const float DEADZONE_Z = 3.0f; // ±3 grados de margen para la rotación (un poco más libre)

const int ADDR_CALIBRACION = 0;

// Gestiona Las Posiciones de la Memoria EEPROM
const int ADDR_FIRMA = 32;
const int ADDR_MENU = 36;
const int ADDR_MPE = ADDR_MENU + 56; // 92

// 9. CONSTANTES DEL PROTOCOLO MPE
const uint8_t MPE_CANAL_MAESTRO = 1;
const uint8_t MPE_CANAL_MIEMBRO_MIN = 2;
const uint8_t MPE_CANAL_MIEMBRO_MAX = 16;
const uint8_t MPE_NUM_MIEMBROS = 15;
const uint8_t MPE_CC_TIMBRE = 74;   // CC74: timbre en MPE
const int MPE_PITCHBEND_CENTRO = 0; // la librería usa rango firmado -8192..+8191
const int MPE_PITCHBEND_MIN = -8192;
const int MPE_PITCHBEND_MAX = 8191;

ConfigMenu config = {
    .canalMidi = 1,
    .numeroCC_X = 16,
    .numeroCC_Y = 17,
    .numeroCC_Z = 18,
    .trillBarCC_pos = 20,
    .trillBarCC_pres = 21,
    .trillSquareCC_X = 22, // Eje Horizontal X
    .trillSquareCC_Y = 23, // Eje Vertical Y
    .trillSquareCC_Z = 24, // Presión Z
    .suavizadoAlpha = 50,
    .zonaMuerta = 5,
    .trillBarMinPresion = 450,
    .trillBarMaxPresion = 6500,
    .modoMidi = 0,        // Por seguridad, arrancar siempre en el modo CC actual
    .mpeBarModo = 0,      // arrancar en continuo
    .mpeEscala = 0,       // escala mayor
    .mpeNotaBase = 48,    // C3
    .mpeRangoOctavas = 2, // 2 octavas
    .mpeSquareModo = 0,   // Y selecciona la nota
    .mpeSquareNota = 48,  // C3 si se usa nota fija
    .mpeVersion = 1,
};

bool calibracionEnProgreso = false;

// Función auxiliar para aplicar la zona muerta en los ángulos antes del mapeo MIDI
float aplicarZonaMuertaAngulo(float valor, float zonaMuerta)
{
    if (valor > zonaMuerta)
        return valor - zonaMuerta;
    if (valor < -zonaMuerta)
        return valor + zonaMuerta;
    return 0.0f; // Bloquea en el centro exacto
}

void setBoardColor(uint8_t r, uint8_t g, uint8_t b)
{
    pixels.setPixelColor(0, pixels.Color(r, g, b));
    pixels.show();
}

void guardarLimpiosEnEEPROM()
{
    noInterrupts();
    EEPROM.begin(128);
    EEPROM.put(ADDR_CALIBRACION + 0, anguloCentroX);
    EEPROM.put(ADDR_CALIBRACION + 4, anguloCentroY);
    EEPROM.put(ADDR_CALIBRACION + 8, xMinLimit);
    EEPROM.put(ADDR_CALIBRACION + 12, xMaxLimit);
    EEPROM.put(ADDR_CALIBRACION + 16, yMinLimit);
    EEPROM.put(ADDR_CALIBRACION + 20, yMaxLimit);
    EEPROM.write(ADDR_FIRMA, 0xA5);
    EEPROM.put(ADDR_MENU + 0, config.canalMidi);
    EEPROM.put(ADDR_MENU + 4, config.numeroCC_X);
    EEPROM.put(ADDR_MENU + 8, config.numeroCC_Y);
    EEPROM.put(ADDR_MENU + 12, config.numeroCC_Z);
    EEPROM.put(ADDR_MENU + 16, config.suavizadoAlpha);
    EEPROM.put(ADDR_MENU + 20, config.zonaMuerta);
    EEPROM.put(ADDR_MENU + 24, config.trillBarMinPresion);
    EEPROM.put(ADDR_MENU + 28, config.trillBarMaxPresion);
    EEPROM.put(ADDR_MENU + 32, config.trillBarCC_pos);
    EEPROM.put(ADDR_MENU + 36, config.trillBarCC_pres);
    EEPROM.put(ADDR_MENU + 40, config.trillSquareCC_X);
    EEPROM.put(ADDR_MENU + 44, config.trillSquareCC_Y);
    EEPROM.put(ADDR_MENU + 48, config.trillSquareCC_Z);
    EEPROM.put(ADDR_MENU + 52, config.modoMidi); // Dirección 88: selector CC/MPE
    EEPROM.put(ADDR_MPE + 0, config.mpeBarModo);
    EEPROM.put(ADDR_MPE + 4, config.mpeEscala);
    EEPROM.put(ADDR_MPE + 8, config.mpeNotaBase);
    EEPROM.put(ADDR_MPE + 12, config.mpeRangoOctavas);
    EEPROM.put(ADDR_MPE + 16, config.mpeSquareModo);
    EEPROM.put(ADDR_MPE + 20, config.mpeSquareNota);
    EEPROM.put(ADDR_MPE + 24, config.mpeVersion);

    EEPROM.commit();
    interrupts();
}

void cargarLimpiosDesdeEEPROM()
{
    noInterrupts();
    EEPROM.begin(128);
    byte firma = EEPROM.read(ADDR_FIRMA);
    if (firma == 0xA5)
    {
        EEPROM.get(ADDR_CALIBRACION + 0, anguloCentroX);
        EEPROM.get(ADDR_CALIBRACION + 4, anguloCentroY);
        EEPROM.get(ADDR_CALIBRACION + 8, xMinLimit);
        EEPROM.get(ADDR_CALIBRACION + 12, xMaxLimit);
        EEPROM.get(ADDR_CALIBRACION + 16, yMinLimit);
        EEPROM.get(ADDR_CALIBRACION + 20, yMaxLimit);
        EEPROM.get(ADDR_MENU + 0, config.canalMidi);
        EEPROM.get(ADDR_MENU + 4, config.numeroCC_X);
        EEPROM.get(ADDR_MENU + 8, config.numeroCC_Y);
        EEPROM.get(ADDR_MENU + 12, config.numeroCC_Z);
        EEPROM.get(ADDR_MENU + 16, config.suavizadoAlpha);
        EEPROM.get(ADDR_MENU + 20, config.zonaMuerta);
        EEPROM.get(ADDR_MENU + 24, config.trillBarMinPresion);
        EEPROM.get(ADDR_MENU + 28, config.trillBarMaxPresion);
        EEPROM.get(ADDR_MENU + 32, config.trillBarCC_pos);
        EEPROM.get(ADDR_MENU + 36, config.trillBarCC_pres);
        EEPROM.get(ADDR_MENU + 40, config.trillSquareCC_X);
        EEPROM.get(ADDR_MENU + 44, config.trillSquareCC_Y);
        EEPROM.get(ADDR_MENU + 48, config.trillSquareCC_Z);
        EEPROM.get(ADDR_MENU + 52, config.modoMidi);
        EEPROM.get(ADDR_MPE + 0, config.mpeBarModo);
        EEPROM.get(ADDR_MPE + 4, config.mpeEscala);
        EEPROM.get(ADDR_MPE + 8, config.mpeNotaBase);
        EEPROM.get(ADDR_MPE + 12, config.mpeRangoOctavas);
        EEPROM.get(ADDR_MPE + 16, config.mpeSquareModo);
        EEPROM.get(ADDR_MPE + 20, config.mpeSquareNota);
        EEPROM.get(ADDR_MPE + 24, config.mpeVersion);

        // ... (Aquí se quedan todas tus lecturas EEPROM.get normales de la calibración y el menú) ...

        // ================================================================
        // ESCUDO DE PROTECCIÓN INTEGRAL: AUTO-RECUPERACIÓN DE FÁBRICA
        // ================================================================

        // 1. Canal MIDI (Debe estar entre 1 y 16)
        if (config.canalMidi < 1 || config.canalMidi > 16)
        {
            config.canalMidi = 1;
        }

        // 2. Controladores MIDI del Torso / Acelerómetro (Valores originales)
        if (config.numeroCC_X < 0 || config.numeroCC_X > 127)
            config.numeroCC_X = 16;
        if (config.numeroCC_Y < 0 || config.numeroCC_Y > 127)
            config.numeroCC_Y = 17;
        if (config.numeroCC_Z < 0 || config.numeroCC_Z > 127)
            config.numeroCC_Z = 18;

        // 3. Parámetros del Filtro y Algoritmo del Torso
        if (config.suavizadoAlpha < 5 || config.suavizadoAlpha > 99)
            config.suavizadoAlpha = 50;
        if (config.zonaMuerta < 0 || config.zonaMuerta > 30)
            config.zonaMuerta = 5;

        // 4. Calibración de Presión del Trill Bar
        if (config.trillBarMinPresion <= 0)
            config.trillBarMinPresion = 450;
        if (config.trillBarMaxPresion <= 0)
            config.trillBarMaxPresion = 6500;

        // 5. Modo MIDI: solo se aceptan CC (0) o MPE (1).
        // Valores antiguos/no inicializados vuelven al modo CC seguro.
        if (config.modoMidi != 0 && config.modoMidi != 1)
            config.modoMidi = 0;

        // 6. Controladores MIDI del Pad Trill Square
        if (config.trillSquareCC_X < 0 || config.trillSquareCC_X > 127)
            config.trillSquareCC_X = 22;
        if (config.trillSquareCC_Y < 0 || config.trillSquareCC_Y > 127)
            config.trillSquareCC_Y = 23;
        if (config.trillSquareCC_Z < 0 || config.trillSquareCC_Z > 127)
            config.trillSquareCC_Z = 24;

        // BLOQUE MPE
        if (config.mpeVersion != 1)
        {
            config.mpeBarModo = 0;
            config.mpeEscala = 0;
            config.mpeNotaBase = 48;
            config.mpeRangoOctavas = 2;
            config.mpeSquareModo = 0;
            config.mpeSquareNota = 48;
            config.mpeVersion = 1;
        }

        // Validación de Rangos
        if (config.mpeBarModo != 0 && config.mpeBarModo != 1)
            config.mpeBarModo = 0;

        // 9 escalas: 0..8
        if (config.mpeEscala < 0 || config.mpeEscala > 8)
            config.mpeEscala = 0;

        if (config.mpeNotaBase < 0 || config.mpeNotaBase > 127)
            config.mpeNotaBase = 48;

        if (config.mpeRangoOctavas < 1 || config.mpeRangoOctavas > 4)
            config.mpeRangoOctavas = 2;

        if (config.mpeSquareModo != 0 && config.mpeSquareModo != 1)
            config.mpeSquareModo = 0;

        if (config.mpeSquareNota < 0 || config.mpeSquareNota > 127)
            config.mpeSquareNota = 48;
    }
    else
    {
        interrupts();
        guardarLimpiosEnEEPROM();
        return;
    }
    interrupts();
}

void iniciarCalibracionDinamica()
{
    calibracionEnProgreso = true;
    setBoardColor(255, 255, 255);
    xMinLimit = 90.0f;
    xMaxLimit = -90.0f;
    yMinLimit = 90.0f;
    yMaxLimit = -90.0f;
}

void actualizarCalibracionDinamica(float anguloActualX, float anguloActualY)
{
    if (!calibracionEnProgreso)
        return;
    if (anguloActualX < xMinLimit)
        xMinLimit = anguloActualX;
    if (anguloActualX > xMaxLimit)
        xMaxLimit = anguloActualX;
    if (anguloActualY < yMinLimit)
        yMinLimit = anguloActualY;
    if (anguloActualY > yMaxLimit)
        yMaxLimit = anguloActualY;
}

void finalizarCalibracionDinamica()
{
    if (!calibracionEnProgreso)
        return;
    calibracionEnProgreso = false;
    setBoardColor(0, 255, 0);
    if (abs(xMaxLimit - xMinLimit) < 10.0f)
    {
        xMinLimit = -30.0f;
        xMaxLimit = 30.0f;
    }
    if (abs(yMaxLimit - yMinLimit) < 10.0f)
    {
        yMinLimit = -30.0f;
        yMaxLimit = 30.0f;
    }
    anguloCentroX = (xMaxLimit + xMinLimit) / 2.0f;
    anguloCentroY = (yMaxLimit + yMinLimit) / 2.0f;
    guardarLimpiosEnEEPROM();
}

void ejecutarCalibracionDinamica()
{
    iniciarCalibracionDinamica();
}

void inicializarHardware()
{
    // I2C0
    Wire.setSDA(4);
    Wire.setSCL(5);
    Wire.begin();
    Wire.setClock(400000);

    delay(10);

    // 🚀 NUEVO: Inicializamos el segundo canal físico para los sensores táctiles
    Wire1.setSDA(2); // Asignamos GP2 a la línea SDA1
    Wire1.setSCL(3); // Asignamos GP3 a la línea SCL1
    Wire1.begin();
    Wire1.setClock(400000); // ¡Fast Mode a 400 kHz también para los sensores!

    pixels.begin();
    pixels.setBrightness(30);
    setBoardColor(0, 0, 255);
    EEPROM.begin(64);
    USBDevice.setManufacturerDescriptor("Waveshare");
    USBDevice.setProductDescriptor("RP2040 CORTEZA MIDI");
    MIDI_OBJ.begin(MIDI_CHANNEL_OMNI);
    delay(400);
    setBoardColor(0, 255, 255);
    if (mpu.begin(0x68, &Wire))
    {
        mpu_detectado = true;
        mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
        mpu.setGyroRange(MPU6050_RANGE_500_DEG);
        mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
        cargarLimpiosDesdeEEPROM();
        lastTime = micros();
        setBoardColor(0, 255, 0);
    }
    else
    {
        mpu_detectado = false;
        setBoardColor(255, 0, 0);
    }
}

// ENVIO Y EVALUACIÓN MENSAJES MIDI CC
void enviarMensajeCC(unsigned char numeroCC, unsigned char valor, unsigned char canal)
{
    MIDI_OBJ.sendControlChange(numeroCC, valor, canal);
}

void evaluarYEnviarCC(unsigned char numeroCC, int valorActual, int &valorEnviado, unsigned char canal)
{
    if (valorActual != valorEnviado)
    {
        valorEnviado = valorActual;
        enviarMensajeCC(numeroCC, (unsigned char)valorActual, canal);
    }
}
// - /////////// - FIN ENVIO Y VALIDACIÓN MIDI CC - //////////////////////////////////////

// PROCESADO DATOS PARA CONVERTIRLOS MIDI MODO CC
void procesarDatos(DatosSensor datos)
{
    unsigned long currentTime = micros();
    float dt = (currentTime - lastTime) / 1000000.0f;
    lastTime = currentTime;
    if (dt > 0.1f || dt <= 0.0f)
        dt = 0.025f;

    float accAngleX = atan2(datos.accelY, datos.accelZ) * 57.2958f;
    float accAngleY = atan2(-datos.accelX, sqrt(datos.accelY * datos.accelY + datos.accelZ * datos.accelZ)) * 57.2958f;

    anguloX = 0.98f * (anguloX + datos.gyroX * dt) + 0.02f * accAngleX;
    anguloY = 0.98f * (anguloY + datos.gyroY * dt) + 0.02f * accAngleY;
    anguloZ += datos.gyroZ * dt;

    // CAMBIO 1: Incrementamos el muelle para dar firmeza y absorber micro-movimientos
    anguloZ *= 0.988f;

    if (calibracionEnProgreso)
    {
        actualizarCalibracionDinamica(anguloX, anguloY);
        return;
    }

    // NUEVO: Definición de zonas muertas centrales en grados (modificables a tu gusto)
    const float DEADZONE_ANGULO_X = 2.0f; // ±2 grados de tolerancia física en X
    const float DEADZONE_ANGULO_Y = 2.0f; // ±2 grados de tolerancia física en Y
    const float DEADZONE_ANGULO_Z = 3.0f; // ±3 grados de tolerancia física en Z

    // NUEVO: Filtramos los ángulos para clavar el centro antes de mapear a MIDI
    float anguloXFiltrado = aplicarZonaMuertaAngulo(anguloX, DEADZONE_ANGULO_X);
    float anguloYFiltrado = aplicarZonaMuertaAngulo(anguloY, DEADZONE_ANGULO_Y);
    float anguloZFiltrado = aplicarZonaMuertaAngulo(anguloZ, DEADZONE_ANGULO_Z);

    // NUEVO: Mapeo compensando los grados de las zonas muertas para mantener el recorrido limpio
    int targetMidiX = map(constrain(anguloXFiltrado, xMinLimit + DEADZONE_ANGULO_X, xMaxLimit - DEADZONE_ANGULO_X), xMinLimit + DEADZONE_ANGULO_X, xMaxLimit - DEADZONE_ANGULO_X, 0, 127);
    int targetMidiY = map(constrain(anguloYFiltrado, yMinLimit + DEADZONE_ANGULO_Y, yMaxLimit - DEADZONE_ANGULO_Y), yMinLimit + DEADZONE_ANGULO_Y, yMaxLimit - DEADZONE_ANGULO_Y, 0, 127);
    int targetMidiZ = map(constrain(anguloZFiltrado, -40.0f + DEADZONE_ANGULO_Z, 40.0f - DEADZONE_ANGULO_Z), -40.0f + DEADZONE_ANGULO_Z, 40.0f - DEADZONE_ANGULO_Z, 0, 127);

    if (primer_filtrado)
    {
        smoothedX = targetMidiX;
        smoothedY = targetMidiY;
        smoothedZ = targetMidiZ;
        primer_filtrado = false;
    }

    int deltaX = targetMidiX - lastMidiX;
    if (lastMidiX == -999)
        lastMidiX = targetMidiX;
    else if (abs(deltaX) > config.zonaMuerta)
    {
        lastMidiX += (deltaX > 0) ? (deltaX - config.zonaMuerta) : (deltaX + config.zonaMuerta);
    }

    int deltaY = targetMidiY - lastMidiY;
    if (lastMidiY == -999)
        lastMidiY = targetMidiY;
    else if (abs(deltaY) > config.zonaMuerta)
    {
        lastMidiY += (deltaY > 0) ? (deltaY - config.zonaMuerta) : (deltaY + config.zonaMuerta);
    }

    int deltaZ = targetMidiZ - lastMidiZ;
    if (lastMidiZ == -999)
        lastMidiZ = targetMidiZ;
    else if (abs(deltaZ) > config.zonaMuerta)
    {
        lastMidiZ += (deltaZ > 0) ? (deltaZ - config.zonaMuerta) : (deltaZ + config.zonaMuerta);
    }

    float alphaDinamico = config.suavizadoAlpha / 100.0f;
    smoothedX = (alphaDinamico * lastMidiX) + ((1.0f - alphaDinamico) * smoothedX);
    smoothedY = (alphaDinamico * lastMidiY) + ((1.0f - alphaDinamico) * smoothedY);
    smoothedZ = (alphaDinamico * lastMidiZ) + ((1.0f - alphaDinamico) * smoothedZ);

    int finalMidiX = (int)(smoothedX + 0.5f);
    int finalMidiY = (int)(smoothedY + 0.5f);
    int finalMidiZ = (int)(smoothedZ + 0.5f);

    ultimoMidiX = constrain(finalMidiX, 0, 127);
    ultimoMidiY = constrain(finalMidiY, 0, 127);
    ultimoMidiZ = constrain(finalMidiZ, 0, 127);

    static int enviadoX = -1, enviadoY = -1, enviadoZ = -1;
    evaluarYEnviarCC((unsigned char)config.numeroCC_X, ultimoMidiX, enviadoX, (unsigned char)config.canalMidi);
    evaluarYEnviarCC((unsigned char)config.numeroCC_Y, ultimoMidiY, enviadoY, (unsigned char)config.canalMidi);
    evaluarYEnviarCC((unsigned char)config.numeroCC_Z, ultimoMidiZ, enviadoZ, (unsigned char)config.canalMidi);
}
// -////////// - FIN PROCESADO DATOS ENVIO CC - /////////////////////////////

// LECTURA DATOS ACELERÓMETRO
DatosSensor leerSensor(void)
{
    DatosSensor datos = {0};
    if (!mpu_detectado)
        return datos;

    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    datos.accelX = a.acceleration.x;
    datos.accelY = a.acceleration.y;
    datos.accelZ = a.acceleration.z;
    datos.gyroX = g.gyro.x;
    datos.gyroY = g.gyro.y;
    datos.gyroZ = g.gyro.z;

    // 2. CORRECCIÓN CRÍTICA: Convertimos Radianes/s a Grados/s (* 57.2958f)
    // Sin esta multiplicación, el eje Z acumula valores invisibles para el mapa
    datos.gyroX = g.gyro.x * 57.2958f;
    datos.gyroY = g.gyro.y * 57.2958f;
    datos.gyroZ = g.gyro.z * 57.2958f;

    // Procesamos los filtros internamente como antes
    procesarDatos(datos);

    // Devolvemos la estructura para que main.cpp no proteste
    return datos;
}
// - ///////// - FIN LECTURA DATOS ACELEROMETRO - ///////////////////////////

// ESTADO CONEXIÓN ACELERÓMETRO
void verificarEstadoConexion()
{
    if (!mpu_detectado)
    {
        setBoardColor(255, 0, 0);
    }
}
/////////////////////////////////////

/*
    --------------------------------------
        MODO mpc
    --------------------------------------
*/

// MODO MPC ENVIO Y VALIDACIÓN MENSAJES MIDI
static uint8_t mpeLimitar7(int valor)
{
    if (valor < 0)
        return 0;
    if (valor > 127)
        return 127;
    return (uint8_t)valor;
}

uint8_t mpeCanalMiembro(int canal) // Protege el canal maestro canal 1
{
    if (canal < MPE_CANAL_MIEMBRO_MIN)
        return MPE_CANAL_MIEMBRO_MIN;
    if (canal > MPE_CANAL_MIEMBRO_MAX)
        return MPE_CANAL_MIEMBRO_MAX;
    return (uint8_t)canal;
}

static uint8_t mpeCanalValido(int canal)
{
    if (canal < 1)
        return 1;
    if (canal > 16)
        return 16;
    return (uint8_t)canal;
}
// - ///////////- FIN MODO MPC VALIDACIÓN y ENVÍo MIDI - ////////////////////////////////////

// -  NOTAS -
// nota` y `velocidad` se limitan a 0..127.
// En Note On, la velocidad nunca puede ser 0.
// En Note Off se puede usar velocidad 0 (release velocity neutra).

void mpeEnviarNotaOn(uint8_t nota, uint8_t velocidad, uint8_t canal)
{
    uint8_t c = mpeCanalMiembro(canal);
    uint8_t n = mpeLimitar7(nota);
    uint8_t v = mpeLimitar7(velocidad);

    // Velocidad 0 equivaldría a Note Off: forzar mínimo 1
    if (v == 0)
        v = 1;

    MIDI_OBJ.sendNoteOn(n, v, c);
}

void mpeEnviarNotaOff(uint8_t nota, uint8_t velocidad, uint8_t canal)
{
    uint8_t c = mpeCanalMiembro(canal);
    uint8_t n = mpeLimitar7(nota);
    uint8_t v = mpeLimitar7(velocidad);

    MIDI_OBJ.sendNoteOff(n, v, c);
}
//- //////////- FIN NOTAS - //////////////////////////////////

// PITCH BEND
// Centro exacto = `0`.
//`-1.0` = bend mínimo, `+1.0` = bend máximo.
// Para el modo continuo del Bar, la posición normalizada se pasará a `mpeEnviarPitchBendNormalizado()`.
void mpeEnviarPitchBend(int valor, uint8_t canal)
{
    uint8_t c = mpeCanalMiembro(canal);

    if (valor < MPE_PITCHBEND_MIN)
        valor = MPE_PITCHBEND_MIN;
    if (valor > MPE_PITCHBEND_MAX)
        valor = MPE_PITCHBEND_MAX;

    MIDI_OBJ.sendPitchBend(valor, c);
}

void mpeEnviarPitchBendNormalizado(float valor, uint8_t canal)
{
    if (valor < -1.0f)
        valor = -1.0f;
    if (valor > 1.0f)
        valor = 1.0f;

    // Mismo criterio que la librería: 8192 hacia abajo, 8191 hacia arriba
    int v;
    if (valor < 0.0f)
        v = (int)(valor * 8192.0f);
    else
        v = (int)(valor * 8191.0f);

    mpeEnviarPitchBend(v, canal);
}
// - ////////- FIN PITCH BEND - ////////////////////////////

// CHANNEL PRESURE Y CC74
//`sendAfterTouch(presion, canal)` genera `0xD0`, que es el aftertouch correcto para MPE.
// El timbre va exclusivamente por CC74 en canales miembro.
void mpeEnviarPresionCanal(uint8_t presion, uint8_t canal)
{
    uint8_t c = mpeCanalMiembro(canal);
    uint8_t p = mpeLimitar7(presion);

    // Channel Pressure (0xD0), NO un CC de aftertouch
    MIDI_OBJ.sendAfterTouch(p, c);
}

void mpeEnviarTimbre(uint8_t timbre, uint8_t canal)
{
    uint8_t c = mpeCanalMiembro(canal);
    uint8_t t = mpeLimitar7(timbre);

    MIDI_OBJ.sendControlChange(MPE_CC_TIMBRE, t, c);
}
// -////////////////- FIN CHANNEL PRESURE CC74 - /////////////////////////////////

// MENSAJES RPN
/*
* No usar `beginRpn()` / `sendRpnValue()` de la librería: su caché 
* `mCurrentRpnNumber` es **global al objeto MIDI**, 
* no por canal, y puede omitir la selección del RPN 
* al alternar entre canales 2–16. Los CC se envían 
* directamente para que cada bloque sea determinista.
*/
void mpeEnviarRpnNulo(uint8_t canal)
{
    uint8_t c = mpeCanalValido(canal);
    MIDI_OBJ.sendControlChange(101, 127, c); // RPN MSB = null
    MIDI_OBJ.sendControlChange(100, 127, c); // RPN LSB = null
}

void mpeEnviarRpn(uint16_t numeroRpn, uint16_t valor14, uint8_t canal)
{
    uint8_t c = mpeCanalValido(canal);

    uint8_t numMsb = (numeroRpn >> 7) & 0x7F;
    uint8_t numLsb = numeroRpn & 0x7F;
    uint8_t valMsb = (valor14 >> 7) & 0x7F;
    uint8_t valLsb = valor14 & 0x7F;

    MIDI_OBJ.sendControlChange(101, numMsb, c); // RPN MSB
    MIDI_OBJ.sendControlChange(100, numLsb, c); // RPN LSB
    MIDI_OBJ.sendControlChange(6,   valMsb, c); // Data Entry MSB
    MIDI_OBJ.sendControlChange(38,  valLsb, c); // Data Entry LSB
    mpeEnviarRpnNulo(c);

    //Orden estándar: selección MSB → LSB, luego valor MSB → LSB, y finalmente RPN nulo.
    //El RPN nulo evita que un Data Entry posterior cambie el parámetro por accidente.
}
// - /////////////- FIN MENSAJES RPN - ///////////////////////////

// CONFIGURACIÓN DE LA ZONA MPE
void mpeEnviarConfiguracionMpe(uint8_t canalesMiembro)
{
    if (canalesMiembro > 15) canalesMiembro = 15;

    // RPN 6 (MPE Configuration) siempre en el canal maestro
    mpeEnviarRpn(6, canalesMiembro, MPE_CANAL_MAESTRO);
}

void mpeEnviarRangoPitchBend(uint8_t canal, uint8_t semitonos, uint8_t cents)
{
    if (semitonos > 96) semitonos = 96;
    if (cents > 127) cents = 127;

    // RPN 0: semitonos en MSB, cents en LSB
    uint16_t valor = ((uint16_t)semitonos << 7) | cents;
    mpeEnviarRpn(0, valor, canal);
}

void mpeDesactivarZona(void)
{
    mpeEnviarRpn(6, 0, MPE_CANAL_MAESTRO);
}

void mpeConfigurarZona(uint8_t canalesMiembro,
                       uint8_t rangoMaestroSemitonos,
                       uint8_t rangoMiembroSemitonos)
{
    // 1. Activar la zona inferior con N miembros
    mpeEnviarConfiguracionMpe(canalesMiembro);

    // 2. Sensibilidad del canal maestro
    mpeEnviarRangoPitchBend(MPE_CANAL_MAESTRO, rangoMaestroSemitonos, 0);

    // 3. Sensibilidad de cada miembro (compatibilidad con sintes que la leen por canal)
    for (uint8_t c = MPE_CANAL_MIEMBRO_MIN; c <= MPE_CANAL_MIEMBRO_MAX; c++)
    {
        mpeEnviarRangoPitchBend(c, rangoMiembroSemitonos, 0);
    }
}
// - //////////- FIN ZONA MPE - ///////////////////////////////////

// FAse2
//uint8_t rangoBar = (uint8_t)(config.mpeRangoOctavas * 12);
//mpeConfigurarZona(MPE_NUM_MIEMBROS, 2, rangoBar);

