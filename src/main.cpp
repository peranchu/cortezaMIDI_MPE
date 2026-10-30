#include <Arduino.h>
#include "sensor_midi_acell.h"
#include "ui_control.h"
#include "encoder_control.h"
#include "trill_bar_control.h"
#include "trill_square_control.h"

// Variables de estado del acelerómetro requeridas por el sistema
bool mpu_detectado = false;
int lastMidiX = -999;
int lastMidiY = -999;
int lastMidiZ = -999;

// Timers para refrescos asíncronos y no bloquear el bucle principal
unsigned long ultimoRefrescoSensores = 0;
const unsigned long INTERVALO_SENSORES = 5; // Refresco cada 5ms (200Hz) para baja latencia

void setup() {
    // 1. Inicializar la comunicación serial para depuración si fuera necesario
    Serial.begin(115200);

    // 2. FORZAR EL ARRANQUE DE LA PILA USB MIDI (Si usas la librería de Adafruit)
    // Dependiendo de cómo lo tuvieras antes, asegúrate de que se ejecute aquí:
    #if defined(ARDUINO_ARCH_RP2040) && defined(USE_TINYUSB)
      // Si tu código usa un objeto MIDI nativo de TinyUSB, inicialízalo aquí:
      // TinyUSB_Device_Init(0); 
    #endif
    
    delay(200);

     // 1. CARGAR DATOS DESDE LA EEPROM PRIMERO (Llena el struct config con tus datos reales)
    cargarLimpiosDesdeEEPROM(); 
    
    // 2. Inicializar el hardware (USB MIDI, MPU6050, Memoria EEPROM y LED RGB)
    inicializarHardware();

    // 3. Inicializar los pines físicos y las interrupciones del Encoder Rotativo
    inicializarEncoder();

    // 4. Inicializar la pantalla OLED SH1106 con sus fuentes Bold modernas
    inicializarUI();

    delay(100);

    inicializarTrillBar();

    inicializarTrillSquare();
}

void loop() {
    // A. Leer los datos físicos del sensor MPU6050 y procesar la fusión/filtros MIDI
    leerSensor();

    // B. Gestionar los giros del encoder, los parpadeos y actualizar la pantalla OLED
    verificarUI();

    // C. Supervisar el estado de la conexión de la electrónica en segundo plano
    verificarEstadoConexion();

    actualizarTrillBar();

    actualizarTrillSquare();
}
