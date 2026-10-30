#ifndef MIDI_MPE_CONTROL_H
#define MIDI_MPE_CONTROL_H

#include <Arduino.h>

// ================================================================
// INTERFAZ PÚBLICA DEL MOTOR MPE (MÓDULO INDEPENDIENTE)
// ================================================================

/**
 * @brief Procesa las lecturas del Trill Bar aplicando la física MPE (Continuo o Cuantizado).
 *        Esta función aísla las matemáticas MPE del modo CC tradicional.
 * 
 * @param posCruda  Posición X leída del sensor (0..3200)
 * @param presCruda Presión Z leída del sensor (touchSize)
 * @param numToques Cantidad de dedos detectados en el sensor
 */
void procesarBarMPE(int posCruda, int presCruda, int numToques);

/**
 * @brief Watchdog de seguridad. Apaga de golpe cualquier nota activa del Bar 
 *        y centra el Pitch Bend para evitar "stuck notes" al cambiar de menú.
 */
void mpeLimpiarVozBar(void);

#endif // MIDI_MPE_CONTROL_H