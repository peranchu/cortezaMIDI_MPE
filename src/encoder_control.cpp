#include "encoder_control.h"
#include "sensor_midi_acell.h" // Para poder disparar la calibración si se desea
#include "trill_bar_control.h" // Para Llamar a la calibración dinámica el trill bar
#include "ui_control.h"        // Para poder pintar textos de aviso
#include <OneButton.h>

// Instancia de OneButton para el pin SW (Activo en BAJO, con Pull-Up interno)
OneButton botonEncoder(PIN_ENCODER_SW, true, true);

// Variables volátiles para las interrupciones del giro
volatile int posicionEncoder = 0;
volatile bool cambioDetectadoEncoder = false;

extern int pasoCalibTrill; // Traemos la variable del estado de calibración

/**
 * SERVICIO DE INTERRUPCIÓN (ISR): Decodificador por cuadratura robusto.
 * Evita saltos y pérdidas de pasos de forma matemática en microsegundos.
 */
void manejarGiroHardware()
{
    static uint8_t estadoAnterior = 0;

    // Leemos el estado físico actual de los pines CLK y DT
    uint8_t estadoActual = (digitalRead(PIN_ENCODER_CLK) << 1) | digitalRead(PIN_ENCODER_DT);

    // Creamos un índice combinando el estado anterior y el actual (4 bits en total)
    uint8_t indiceTransicion = (estadoAnterior << 2) | estadoActual;

    // Evaluamos las transiciones válidas de un clic completo del KY-040
    // Filtra y descarta los estados intermedios erráticos producidos por el ruido eléctrico
    if (indiceTransicion == 0b0010 || indiceTransicion == 0b1011 || indiceTransicion == 0b1101 || indiceTransicion == 0b0100)
    {
        posicionEncoder++;
        cambioDetectadoEncoder = true;
    }
    else if (indiceTransicion == 0b0001 || indiceTransicion == 0b0111 || indiceTransicion == 0b1110 || indiceTransicion == 0b1000)
    {
        posicionEncoder--;
        cambioDetectadoEncoder = true;
    }
    // Guardamos el estado actual para la siguiente transición
    estadoAnterior = estadoActual;
}
// ****************************** //

/**
 * Callbacks de OneButton para los eventos del botón (cortos)
 */
void clickCortoBoton()
{
    // El click corto ahora gestiona la navegación/edición del menú
    ui_manejarClickCorto();
}
// ********************************

/**
 *  Callbacks de OneButon PARA los enventos de botón (largos)
 */
void clickLargoBoton()
{
    // Si dejas pulsado el botón en caliente, lanzamos la calibración
    actualizarPantallaMuestra("CALIBRANDO...");
    ejecutarCalibracionDinamica();
}
// ************************ //

// INICIALIZACIÓN DEL ENCODER Y EL BOTÓN (Librería OneButton)
void inicializarEncoder()
{
    pinMode(PIN_ENCODER_CLK, INPUT_PULLUP);
    pinMode(PIN_ENCODER_DT, INPUT_PULLUP);

    // 1. Configurar interrupciones por hardware para el giro perfecto
    attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_CLK), manejarGiroHardware, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_DT), manejarGiroHardware, CHANGE);

    // 2. Configurar la máquina de estados de OneButton
    botonEncoder.attachClick(clickCortoBoton);
    botonEncoder.attachLongPressStop(clickLargoBoton);

    botonEncoder.setClickMs(350);
    botonEncoder.setPressMs(1000);
}
///  *************************** ///

// VERIFICA EL ESTADO DEL BOTÓN
void verificarEncoder()
{
    // Refresca constantemente la lectura y los temporizadores del botón
    botonEncoder.tick();
}
////// ********************* ///////

// Obtiene la posicion del encoder al girarlo dividido /4 para obtener resolución de 1 valor cada giro
int obtenerPosicionEncoder()
{
    return posicionEncoder / 4;
}
// ********************** ////

// VERIFICA SI LA POSICION DEL ENCODER HA CAMBIADO
bool verificarCambioEncoder()
{
    if (cambioDetectadoEncoder)
    {
        cambioDetectadoEncoder = false;
        return true;
    }
    return false;
}
///  ************************* ////

/**
 * Puente definitivo para que ui_control pueda leer el botón del encoder
 * Retorna true si el botón está físicamente presionado (Lectura LOW en INPUT_PULLUP)
 */
bool obtenerEstadoBoton(void)
{
    return (digitalRead(PIN_ENCODER_SW) == LOW);
}
///// ******************* //////