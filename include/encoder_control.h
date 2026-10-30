#ifndef ENCODER_CONTROL_H
#define ENCODER_CONTROL_H

#include <Arduino.h>

// Pines físicos en la RP2040-Zero
const uint8_t PIN_ENCODER_CLK = 6; 
const uint8_t PIN_ENCODER_DT  = 7; 
const uint8_t PIN_ENCODER_SW  = 14; 

// Prototipos públicos
void inicializarEncoder();
void verificarEncoder(); // Esta función refrescará el tick() de OneButton
int obtenerPosicionEncoder();
bool verificarCambioEncoder();
bool obtenerEstadoBoton(void);

#endif // ENCODER_CONTROL_H