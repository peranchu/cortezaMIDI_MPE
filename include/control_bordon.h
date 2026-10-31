#ifndef CONTROL_BORDON_H
#define CONTROL_BORDON_H

#include <Arduino.h>

// ============================================================================
// ESTRUCTURA DE ESTADO INTERNO PARA EL BORDÓN
// ============================================================================
struct EstadoBordon {
    bool    dedoPuesto;       // Controla si hay un toque activo en el Square
    uint8_t notaActiva;       // Almacena la nota MIDI exacta que está sonando
    float   presionFiltrada;  // Almacena el valor suavizado del eje Z
};

// ============================================================================
// DECLARACIÓN DE FUNCIONES DEL MÓDULO
// ============================================================================
// Inicializa o resetea el estado del bordón
void bordon_inicializar();

// 🎯 Ahora la función recibe de forma directa lo que necesita para trabajar
void procesarSquareBordonMPE(int yCrudo, int presCruda, int numToques, 
                             uint8_t notaBordonMidi, uint8_t ccY, uint8_t ccZ);

                             
// 🎯 NUEVA FUNCIÓN: Motor genérico para enviar cualquier comando RPN en tus zonas
void mpeEnviarRPN(uint8_t parametroMSB, uint8_t parametroLSB, uint8_t valorData, uint8_t canal);
                             
#endif // CONTROL_BORDON_H
