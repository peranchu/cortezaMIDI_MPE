#ifndef TRILL_SQUARE_CONTROL_H
#define TRILL_SQUARE_CONTROL_H

// Inicializa el sensor en la dirección I2C 0x21
void inicializarTrillSquare(void);

// Lee el sensor y envía los CC asignados en el menú
void actualizarTrillSquare(void);

// Variables globales para que la pantalla UI pueda leer los valores en vivo
extern int squareMidiXActual;
extern int squareMidiYActual;
extern int squareMidiZActual;

#endif // TRILL_SQUARE_CONTROL_H