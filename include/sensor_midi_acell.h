#ifndef SENSOR_MIDI_ACELL_H
#define SENSOR_MIDI_ACELL_H

#include <Arduino.h>

// 1. ESTRUCTURA DE CONFIGURACIÓN DEL MENÚ (Sincronizada con ui_control)
struct ConfigMenu {
    int canalMidi;
    int numeroCC_X;
    int numeroCC_Y;
    int numeroCC_Z;
    int trillBarCC_pos;
    int trillBarCC_pres;
    int trillSquareCC_X;
    int trillSquareCC_Y;
    int trillSquareCC_Z;
    int suavizadoAlpha;
    int zonaMuerta;
    int trillBarMinPresion;
    int trillBarMaxPresion;
    int modoMidi; // 0 = MIDI CC (actual), 1 = MIDI MPE
};

extern ConfigMenu config;

// 2. ESTRUCTURA DE CAPTURA DE DATOS FÍSICOS DEL SENSOR
struct DatosSensor {
    float accelX;
    float accelY;
    float accelZ;
    float gyroX;
    float gyroY;
    float gyroZ;
};

// 3. VARIABLES GLOBALES COMPARTIDAS (Visibles en ui_control y main)
extern ConfigMenu config;
extern int ultimoMidiX;
extern int ultimoMidiY;
extern int ultimoMidiZ;
extern bool calibracionEnProgreso;

// 4. DECLARACIÓN DE FUNCIONES DEL SISTEMA
void inicializarHardware(void);
DatosSensor leerSensor(void);
void verificarEstadoConexion(void);
void procesarDatos(DatosSensor datos);

// 5. DECLARACIÓN DE TRANSMISIÓN MIDI Y ENVIADORES INDEPENDIENTES
void enviarMensajeCC(unsigned char numeroCC, unsigned char valor, unsigned char canal);
void evaluarYEnviarCC(unsigned char numeroCC, int valorActual, int &valorEnviado, unsigned char canal);

// 6. DECLARACIÓN DEL MOTOR DE CALIBRACIÓN DINÁMICA ASÍNCRONA
void iniciarCalibracionDinamica(void);
void actualizarCalibracionDinamica(float anguloActualX, float anguloActualY);
void finalizarCalibracionDinamica(void);
void ejecutarCalibracionDinamica(void); // Alias / Puente de compatibilidad

// 7. CONTROL DEL LED RGB DE LA PLACA
void setBoardColor(uint8_t r, uint8_t g, uint8_t b);

// 8. FUNCIONES DE ALMACENAMIENTO DE MEMORIA FIJA (EEPROM)
void guardarLimpiosEnEEPROM(void);
void cargarLimpiosDesdeEEPROM(void);

#endif // SENSOR_MIDI_ACELL_H
