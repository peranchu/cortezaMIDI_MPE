/**
 * @brief Procesa las lecturas del Trill Bar aplicando la física MPE (Continuo o Cuantizado).
 *        Esta función aísla las matemáticas MPE del modo CC tradicional.
 * 
 * @param posCruda  Posición X leída del sensor (0..3200)
 * @param presCruda Presión Z leída del sensor (touchSize)
 * @param numToques Cantidad de dedos detectados en el sensor
 */

#ifndef MIDI_MPE_CONTROL_H
#define MIDI_MPE_CONTROL_H

#include <Arduino.h>

// ================================================================
// 🎵 ECOSISTEMA DE ESCALAS MUSICALES PARA EL MODO CUANTIZADO
// ================================================================
enum TipoEscala {
    ESCALA_CROMATICA = 0,
    ESCALA_MAYOR,
    ESCALA_MENOR_NAT,
    ESCALA_DORICA,
    ESCALA_FRIGIA,
    ESCALA_MIXOLIDIA,
    ESCALA_MENOR_ARM,
    ESCALA_PENTATONICA,
    ESCALA_PENTA_MENOR,
    // 🛸 NUEVAS: HANDPAN Y EXÓTICAS
    ESCALA_CELTIC_AMARA,  // Celtic Minor / Amara (Típica de Handpan)
    ESCALA_HIJAZ,         // Árabe / Flamenca dominante (Mística oriental)
    ESCALA_AKEBONO,       // Tradicional Japonesa (Meditativa y flotante)
    ESCALA_INSEN,         // Otra Japonesa exótica (Koto / Aire oscuro)
    ESCALA_PENTATONICA_BLUES, // Escala de Blues con la "Blue Note" (6 notas)
    ESCALA_RAGA_BHAIRAV,  // Escala Clásica de la India
    TOTAL_ESCALAS
};

// Declaramos los arrays como 'inline const' (C++17) para que se puedan
// compartir entre módulos sin problemas de enlaces duplicados.
inline const char* nombresEscalas[] = {
    "Cromatica", 
    "Mayor", 
    "Menor Nat", 
    "Dorica", 
    "Frigia", 
    "Mixolidia", 
    "Menor Arm", 
    "Penta May", 
    "Penta Men",
    "Handpan Amara", // Celtic Minor (Handpan clásica)
    "Arab Hijaz",    // Exótica Árabe
    "Jap Akebono",   // Meditativa Zen
    "Jap Insen",     // Koto místico
    "Blues Note",    // Escala Blues completa
    "Raga India"     // Sonoridad Hindú pura
};

// Tablas de intervalos musicales (semitonos desde la nota raíz Do)
inline const uint8_t intervalosMayor[]       = {0, 2, 4, 5, 7, 9, 11}; // 7 notas
inline const uint8_t intervalosMenorNat[]    = {0, 2, 3, 5, 7, 8, 10}; // 7 notas
inline const uint8_t intervalosDorica[]      = {0, 2, 3, 5, 7, 9, 10}; // 7 notas
inline const uint8_t intervalosFrigia[]      = {0, 1, 3, 5, 7, 8, 10}; // 7 notas
inline const uint8_t intervalosMixolidia[]   = {0, 2, 4, 5, 7, 9, 10}; // 7 notas
inline const uint8_t intervalosMenorArm[]    = {0, 2, 3, 5, 7, 8, 11}; // 7 notas
inline const uint8_t intervalosPentatonica[] = {0, 2, 4, 7, 9};         // 5 notas
inline const uint8_t intervalosPentaMenor[]  = {0, 3, 5, 7, 10};       // 5 notas
// Intervalos nuevos:
inline const uint8_t intervalosCeltic[]      = {0, 2, 3, 5, 7, 8, 10}; // 7 notas (Afinación Rey del Hang)
inline const uint8_t intervalosHijaz[]       = {0, 1, 4, 5, 7, 8, 10}; // 7 notas (Intervalo gigante entre 2ª y 3ª)
inline const uint8_t intervalosAkebono[]     = {0, 2, 3, 7, 8};         // 5 notas (Estructura flotante japonesa)
inline const uint8_t intervalosInsen[]       = {0, 1, 5, 7, 10};        // 5 notas (Afinación tradicional de Koto)
inline const uint8_t intervalosBlues[]       = {0, 3, 5, 6, 7, 10};     // 6 notas (¡Ojo, esta tiene 6 notas!)
inline const uint8_t intervalosRaga[]        = {0, 1, 4, 5, 7, 8, 11}; // 7 notas (Modo Bhairav)

// ================================================================
// INTERFAZ PÚBLICA DEL MOTOR MPE (MÓDULO INDEPENDIENTE)
// ================================================================

// comportamiento de la barra MPE
enum ModoBarraMPE {
    MPE_BAR_CONTINUO = 0, // El Ribbon clásico que ya tienes
    MPE_BAR_CUANTIZADO_KEYS = 1,    // El modo por teclas (Keys) que ya funciona
};

void procesarBarMPE(int posCruda, int presCruda, int numToques);

/**
 * @brief Watchdog de seguridad. Apaga de golpe cualquier nota activa del Bar 
 *        y centra el Pitch Bend para evitar "stuck notes" al cambiar de menú.
 */
void mpeLimpiarVozBar(void);

/**
 * @brief Detecta el cambio CC <-> MPE del menú y configura o desactiva la zona
 *        MPE automaticamente (MCM + rangos de pitch bend). Llamar en cada loop().
 */
void gestionarCambioDeModoMPE(void);

#endif // MIDI_MPE_CONTROL_H