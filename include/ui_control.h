#ifndef UI_CONTROL_H
#define UI_CONTROL_H

enum PantallaActual {
  // 🟢 BLOQUE 1: PANTALLAS GLOBALES Y MONITORES (Siempre visibles)
  PANTALLA_HOME = 0,
  PANTALLA_MONITOR_TRILL,
  PANTALLA_MONITOR_SQUARE,
  PANTALLA_CANAL,
  PANTALLA_SUAVIZADO,
  PANTALLA_ZONA_MUERTA,
  PANTALLA_CALIBRANDO,          // Estado para la pantalla de calibración dinámica
  PANTALLA_CALIBRADO_TRILL_BAR, // Pantalla calibración trillbar
  PANTALLA_MODO_MIDI,           // Selector entre MIDI CC y MPE (La frontera)

  // 🔹 BLOQUE 2: PANTALLAS EXCLUSIVAS MPE (Solo visibles si modoMidi == 1)
  PANTALLA_MPE_BAR_MODO,        
  PANTALLA_MPE_BAR_OCTAVAS,     
  PANTALLA_MPE_BAR_NOTA,        

  // 🔸 BLOQUE 3: PANTALLAS EXCLUSIVAS CC (Solo visibles si modoMidi == 0)
  PANTALLA_CC_X,         
  PANTALLA_CC_Y,         
  PANTALLA_CC_Z, 
  PANTALLA_CC_TRB_POS,
  PANTALLA_CC_TRB_PRES,
  PANTALLA_SQUARE_CC_X,
  PANTALLA_SQUARE_CC_Y,
  PANTALLA_SQUARE_CC_Z,        

  TOTAL_PANTALLAS 
};

// 2. EXPOSICIÓN DE LA VARIABLE GLOBAL
// Al poner 'extern', le dices al encoder que la variable real vive en ui_control.cpp
extern PantallaActual pantallaActiva;

// Funciones globales de la interfaz
void inicializarUI();
void verificarUI();
void actualizarPantallaMuestra(const char *mensaje);
void ui_manejarClickCorto();


#endif