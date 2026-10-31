#ifndef SENSOR_MIDI_ACELL_H
#define SENSOR_MIDI_ACELL_H

#include <Arduino.h>

// 1. ESTRUCTURA DE CONFIGURACIÓN DEL MENÚ (Sincronizada con ui_control)
struct ConfigMenu
{
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
    int modoMidi;        // 0 = MIDI CC (actual), 1 = MIDI MPE
    int mpeBarModo;      // 0 = CONTINUO, 1 = CUANTIZADO
    int mpeEscala;       // índice de escala (0..8)
    int mpeNotaBase;     // 0..127
    int mpeRangoOctavas; // 1..4
    int mpeSquareModo;   // 0 = Y ESCALA, 1 = NOTA FIJA
    int mpeSquareNota;   // 0..127
    int mpeVersion;      // versión del bloque MPE (1)
    int transposicion;   
};

extern ConfigMenu config;

// 2. ESTRUCTURA DE CAPTURA DE DATOS FÍSICOS DEL SENSOR
struct DatosSensor
{
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

// 10. TRANSMISIÓN MIDI MPE (helpers)
uint8_t mpeCanalMiembro(int canal);

void mpeEnviarNotaOn(uint8_t nota, uint8_t velocidad, uint8_t canal);
void mpeEnviarNotaOff(uint8_t nota, uint8_t velocidad, uint8_t canal);

void mpeEnviarPitchBend(int valor, uint8_t canal);              // firmado -8192..+8191
void mpeEnviarPitchBendNormalizado(float valor, uint8_t canal); // -1.0..+1.0

void mpeEnviarPresionCanal(uint8_t presion, uint8_t canal);     // Channel Pressure 0xD0
void mpeEnviarTimbre(uint8_t timbre, uint8_t canal);            // CC74

void mpeEnviarRpn(uint16_t numeroRpn, uint16_t valor14, uint8_t canal);
void mpeEnviarRpnNulo(uint8_t canal);

void mpeEnviarConfiguracionMpe(uint8_t canalesMiembro);         // RPN 6 en canal 1
void mpeEnviarRangoPitchBend(uint8_t canal, uint8_t semitonos, uint8_t cents); // RPN 0
void mpeDesactivarZona(void);                                   // RPN 6 = 0
void mpeConfigurarZona(uint8_t canalesMiembro,
                       uint8_t rangoMaestroSemitonos,
                       uint8_t rangoMiembroSemitonos);

                       
#endif // SENSOR_MIDI_ACELL_H
