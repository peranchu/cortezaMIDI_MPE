#ifndef TRILL_BAR_CONTROL_H
#define TRILL_BAR_CONTROL_H

#include <Arduino.h>
#include <Trill.h>

// Dirección I2C de fábrica para el Trill Bar
#define TRILL_BAR_I2C_ADDR 0x20 

// Funciones globales expuestas hacia el main
void inicializarTrillBar(void);
void actualizarTrillBar(void);
void calibarTrillBarDinamico(void);

#endif // TRILL_BAR_CONTROL_H