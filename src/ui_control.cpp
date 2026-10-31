#include "ui_control.h"
#include "trill_bar_control.h"
#include "trill_square_control.h"
#include "sensor_midi_acell.h"
#include "encoder_control.h"
#include "midi_mpe_control.h"
#include "bitmap.h"
#include <U8g2lib.h>

extern bool obtenerEstadoBoton(void);

void calibrarTrillBarDinamico(void);

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

PantallaActual pantallaActiva = PANTALLA_HOME;
bool modoEdicionActivo = false;
bool forzarRedibujado = true;

static int ultimaPosicionEncoder = 0;
static unsigned long ultimoParpadeo = 0;
static bool estadoParpadeo = true;

// Variables de tiempo asíncronas para el botón
static unsigned long tiempoPresionadoBoton = 0;
static bool botonPresionadoAnteriormente = false;

extern int trillMidiBarPosActual;
extern int trillMidiBarPresActual;

extern int squareMidiXActual;
extern int squareMidiYActual;
extern int squareMidiZActual;

bool calibracionEnProgreso_Bar = false;

// MANEJA LA PULSACIONES CORTAS DEL BOTÓN
void ui_manejarClickCorto()
{
  // ================================================================
  // INTERCEPTACIÓN DE LA CALIBRACIÓN DEL TRILL BAR
  // ================================================================
  if (pantallaActiva == PANTALLA_CALIBRADO_TRILL_BAR)
  {
    // 1. Avisamos al sistema de que estamos calibrando para bloquear interferencias
    // (Si usas la variable calibracionEnProgreso que se ve en tu encoder, la activamos aquí)
    calibracionEnProgreso_Bar = true;

    // 2. Ejecutamos el asistente interactivo de la barra
    calibrarTrillBarDinamico();

    // 3. ¡SOLO AL TERMINAR! Cambiamos la variable de pantalla al Home (Índice 0)
    pantallaActiva = PANTALLA_CALIBRADO_TRILL_BAR;

    // 4. Liberamos los bloqueos y forzamos el redibujado limpio del Home
    calibracionEnProgreso_Bar = false;
    modoEdicionActivo = false; // Nos aseguramos de que no entre en edición por error
    forzarRedibujado = true;
    return;
  }

  if (pantallaActiva == PANTALLA_HOME ||
      pantallaActiva == PANTALLA_MONITOR_TRILL ||
      pantallaActiva == PANTALLA_MONITOR_SQUARE ||
      pantallaActiva == PANTALLA_CALIBRANDO)
  {
    modoEdicionActivo = false;
    return;
  }
  if (!modoEdicionActivo)
  {
    modoEdicionActivo = true;
  }
  else
  {
    modoEdicionActivo = false;
    guardarLimpiosEnEEPROM();
  }
  forzarRedibujado = true;
}
//  ******************************* //

// CALIBRACIÓN ACELEROMETRO
void verificarBotonCalibracion()
{
  unsigned long tiempoActual = millis();
  bool botonEstaPulsado = obtenerEstadoBoton();

  // Caso A: Se acaba de presionar el botón
  if (botonEstaPulsado && !botonPresionadoAnteriormente)
  {
    tiempoPresionadoBoton = tiempoActual;
    botonPresionadoAnteriormente = true;
  }

  // Caso B: Se mantiene presionado -> Pasados 1.5s entra en calibración
  if (botonEstaPulsado && botonPresionadoAnteriormente && !calibracionEnProgreso)
  {
    if (tiempoActual - tiempoPresionadoBoton >= 1500)
    {
      pantallaActiva = PANTALLA_CALIBRANDO;
      iniciarCalibracionDinamica();
      forzarRedibujado = true;
    }
  }
  //  ****************************** //

  // Caso C: Se suelta el botón
  if (!botonEstaPulsado && botonPresionadoAnteriormente)
  {
    botonPresionadoAnteriormente = false;
    if (calibracionEnProgreso)
    {
      finalizarCalibracionDinamica();
      pantallaActiva = PANTALLA_HOME;
    }
    else
    {
      ui_manejarClickCorto();
    }
    forzarRedibujado = true;
  }
}

// INICIA LA PANTALLA Y EL LOGO DEL PRINCIPIO (llamada en el arranque)
void inicializarUI()
{
  u8g2.begin();

  u8g2.firstPage();
  do
  {
    // Pantalla De arranque
    u8g2.clearBuffer();
    u8g2.setDrawColor(1);
    u8g2.setBitmapMode(1);
    u8g2.drawBitmap(0, 0, 128 / 8, 64, arbol);
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(70, 30, "CORTEZA");
    u8g2.drawStr(80, 40, "MIDI");
    u8g2.sendBuffer();
  } while (u8g2.nextPage());

  delay(1000);

  pantallaActiva = PANTALLA_HOME;
  ultimaPosicionEncoder = obtenerPosicionEncoder();
  forzarRedibujado = true;
}
// ***************************** ////

// DIBUJO DE TODOS LOS MENUS GRÁFICOS
void verificarUI()
{
  unsigned long tiempoActual = millis();
  static unsigned long ultimoRefrescoPantalla = 0;

  verificarBotonCalibracion();

  if (verificarCambioEncoder())
  {
    int posicionActual = obtenerPosicionEncoder();
    int diferencia = posicionActual - ultimaPosicionEncoder;
    ultimaPosicionEncoder = posicionActual;
    forzarRedibujado = true;

    if (!modoEdicionActivo && !calibracionEnProgreso)
    {
      // Avanzamos de 1 en 1 de forma natural con la diferencia del encoder
      int nuevaPantalla = (int)pantallaActiva + diferencia;

      // 🔄 BUCLE LINEAL ABSOLUTO (Todas las pantallas visibles para siempre)
      // Si avanza más allá de la última pantalla, vuelve al principio (HOME)
      if (nuevaPantalla >= TOTAL_PANTALLAS)
      {
        nuevaPantalla = PANTALLA_HOME;
      }
      // Si retrocede por debajo de HOME, salta directamente a la última pantalla
      else if (nuevaPantalla < PANTALLA_HOME)
      {
        nuevaPantalla = TOTAL_PANTALLAS - 1;
      }

      // 🔄 Tu desvío de seguridad original e intacto para la pantalla de calibración
      if (nuevaPantalla == PANTALLA_CALIBRANDO)
      {
        if (pantallaActiva == PANTALLA_ZONA_MUERTA && diferencia > 0)
          nuevaPantalla = PANTALLA_CALIBRADO_TRILL_BAR;
        else if (pantallaActiva == PANTALLA_CALIBRADO_TRILL_BAR && diferencia < 0)
          nuevaPantalla = PANTALLA_ZONA_MUERTA;
      }

      pantallaActiva = (PantallaActual)nuevaPantalla;
    }

    else if (modoEdicionActivo)
    {
      switch (pantallaActiva)
      {
      case PANTALLA_MODO_MIDI:
        // Cada paso del encoder alterna entre CC (0) y MPE (1).
        if ((abs(diferencia) % 2) == 1)
          config.modoMidi = (config.modoMidi == 0) ? 1 : 0;
        break;

      case PANTALLA_CANAL:
        config.canalMidi += diferencia;
        if (config.canalMidi > 16)
          config.canalMidi = 1;
        if (config.canalMidi < 1)
          config.canalMidi = 16;
        break;

      case PANTALLA_CC_X:
        config.numeroCC_X += diferencia;
        if (config.numeroCC_X > 127)
          config.numeroCC_X = 1;
        if (config.numeroCC_X < 1)
          config.numeroCC_X = 127;
        break;

      case PANTALLA_CC_Y:
        config.numeroCC_Y += diferencia;
        if (config.numeroCC_Y > 127)
          config.numeroCC_Y = 1;
        if (config.numeroCC_Y < 1)
          config.numeroCC_Y = 127;
        break;

      case PANTALLA_CC_Z:
        config.numeroCC_Z += diferencia;
        if (config.numeroCC_Z > 127)
          config.numeroCC_Z = 1;
        if (config.numeroCC_Z < 1)
          config.numeroCC_Z = 127;
        break;

      // 🔄 SUBIDAS AQUÍ PARA COINCIDIR CON TU ENUM REAL
      case PANTALLA_CC_TRB_POS:
        config.trillBarCC_pos += diferencia;
        if (config.trillBarCC_pos > 127)
          config.trillBarCC_pos = 1;
        if (config.trillBarCC_pos < 1)
          config.trillBarCC_pos = 127;
        break;

      case PANTALLA_CC_TRB_PRES:
        config.trillBarCC_pres += diferencia;
        if (config.trillBarCC_pres > 127)
          config.trillBarCC_pres = 1;
        if (config.trillBarCC_pres < 1)
          config.trillBarCC_pres = 127;
        break;

      case PANTALLA_SQUARE_CC_X:
        config.trillSquareCC_X += diferencia;
        if (config.trillSquareCC_X > 127)
          config.trillSquareCC_X = 1;
        if (config.trillSquareCC_X < 1)
          config.trillSquareCC_X = 127;
        break;

      case PANTALLA_SQUARE_CC_Y:
        config.trillSquareCC_Y += diferencia;
        if (config.trillSquareCC_Y > 127)
          config.trillSquareCC_Y = 1;
        if (config.trillSquareCC_Y < 1)
          config.trillSquareCC_Y = 127;
        break;

      case PANTALLA_SQUARE_CC_Z:
        config.trillSquareCC_Z += diferencia;
        if (config.trillSquareCC_Z > 127)
          config.trillSquareCC_Z = 1;
        if (config.trillSquareCC_Z < 1)
          config.trillSquareCC_Z = 127;
        break;

      case PANTALLA_SUAVIZADO:
        config.suavizadoAlpha += (diferencia * 5);
        if (config.suavizadoAlpha > 99)
          config.suavizadoAlpha = 5;
        if (config.suavizadoAlpha < 5)
          config.suavizadoAlpha = 99;
        break;

      case PANTALLA_ZONA_MUERTA:
        config.zonaMuerta += diferencia;
        if (config.zonaMuerta > 30)
          config.zonaMuerta = 0;
        if (config.zonaMuerta < 0)
          config.zonaMuerta = 30;
        break;

      // 🔹 AGREGADAS LAS TRES PANTALLAS MPE QUE FALTABAN EN ESTE SWITCH
      case PANTALLA_MPE_BAR_MODO:
        // Lógica de edición para tus parámetros MPE (ej: alternar continuo/cuantizado)
        // Alterna entre 0 (Continuo) y 1 (Cuantizado)
        config.mpeBarModo += diferencia;
        if (config.mpeBarModo > 1)
          config.mpeBarModo = 0;
        if (config.mpeBarModo < 0)
          config.mpeBarModo = 1;
        break;

      case PANTALLA_MPE_BAR_OCTAVAS:
        // Lógica de edición para rango de octavas MPE
        // Rango típico de octavas para la longitud de la barra (de 1 a 8 octavas)
        config.mpeRangoOctavas += diferencia;
        if (config.mpeRangoOctavas > 8)
          config.mpeRangoOctavas = 1;
        if (config.mpeRangoOctavas < 1)
          config.mpeRangoOctavas = 8;
        break;

      case PANTALLA_MPE_BAR_NOTA:
        // Lógica de edición para nota base MPE
        // Nota base MIDI (0 = C-1, 127 = G9). Avanza de 1 en 1 pinchando notas reales
        config.mpeNotaBase += diferencia;
        if (config.mpeNotaBase > 127)
          config.mpeNotaBase = 1;
        if (config.mpeNotaBase < 0)
          config.mpeNotaBase = 127;
        break;

      case PANTALLA_MPE_BAR_ESCALA:
        // Avanza o retrocede de forma cíclica entre todas las escalas de la cabecera
        config.mpeEscala += diferencia;
        if (config.mpeEscala >= TOTAL_ESCALAS)
          config.mpeEscala = 0;
        if (config.mpeEscala < 0)
          config.mpeEscala = TOTAL_ESCALAS - 1;
        break;

      case PANTALLA_MPE_BAR_TRANSPOSICION:
        config.transposicion += diferencia;
        if (config.transposicion > 12)  config.transposicion = -12; // Da la vuelta al mínimo
        if (config.transposicion < -12) config.transposicion = 12;  // Da la vuelta al máximo
        break;  

      default:
        break;
      }
    }
  }

  if (pantallaActiva == PANTALLA_HOME)
  {
    modoEdicionActivo = false;
  }

  if (modoEdicionActivo || calibracionEnProgreso)
  {
    if (tiempoActual - ultimoParpadeo >= 250)
    {
      ultimoParpadeo = tiempoActual;
      estadoParpadeo = !estadoParpadeo;
      forzarRedibujado = true;
    }
  }
  // gestión de tiempos de refresco:
  bool permitirDibujo = false;

  if (pantallaActiva == PANTALLA_HOME ||
      pantallaActiva == PANTALLA_MONITOR_TRILL ||
      pantallaActiva == PANTALLA_MONITOR_SQUARE)
  {
    if (tiempoActual - ultimoRefrescoPantalla >= 33)
    {
      ultimoRefrescoPantalla = tiempoActual;
      permitirDibujo = true;
    }
  }
  else
  {
    if (forzarRedibujado)
    {
      forzarRedibujado = false;
      permitirDibujo = true;
    }
  }

  // ================================================================
  // BÚCLE DE RENDERIZADO POR PÁGINAS DE U8G2
  // ================================================================
  if (permitirDibujo)
  {
    u8g2.firstPage();
    do
    {
      char bufferTexto[32]; // Buffer de espacio seguro para las cadenas de texto
      int anchoBarraX = 0, anchoBarraY = 0, anchoBarraZ = 0;

      if (modoEdicionActivo)
      {
        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.drawStr(92, 11, "[EDIT]");
      }

      // DIBUJO PANTALLAS
      switch (pantallaActiva)
      {
      case PANTALLA_HOME:
      {
        // ================================================================
        // 1. CABECERA (Mejorada con aviso visual de modo claro CC / MPE)
        // ================================================================
        u8g2.setFont(u8g2_font_6x12_tr);
        u8g2.drawStr(2, 10, "CORTEZA");

        // 💡 AVISO VISUAL DE MODO: Cambiamos el "M:0/1" numérico por un texto legible "CC" o "MPE"
        if (config.modoMidi == 1)
        {
          snprintf(bufferTexto, sizeof(bufferTexto), "[MPE]");
        }
        else
        {
          snprintf(bufferTexto, sizeof(bufferTexto), "[CC] ");
        }
        u8g2.drawStr(58, 10, bufferTexto); // Posicionado limpiamente en la parte central-derecha

        // Canal MIDI (Desplazado ligeramente a la derecha para no pisarse)
        snprintf(bufferTexto, sizeof(bufferTexto), "CH:%d", config.canalMidi);
        u8g2.drawStr(96, 10, bufferTexto);
        u8g2.drawHLine(0, 13, 128);

        // ================================================================
        // 2. MITAD IZQUIERDA: LOS 5 MONITORES LINEALES (Holgados y espaciados)
        // ================================================================
        u8g2.setFont(u8g2_font_5x8_tr);

        // --- A. Eje X Torso ---
        anchoBarraX = map(ultimoMidiX, 0, 127, 0, 36);
        anchoBarraX = constrain(anchoBarraX, 0, 36);
        snprintf(bufferTexto, sizeof(bufferTexto), "X(CC%d)", config.numeroCC_X);
        u8g2.drawStr(5, 23, bufferTexto);
        u8g2.drawFrame(45, 17, 40, 7);
        u8g2.drawBox(47, 19, anchoBarraX, 3);

        // --- B. Eje Y Torso ---
        anchoBarraY = map(ultimoMidiY, 0, 127, 0, 36);
        anchoBarraY = constrain(anchoBarraY, 0, 36);
        snprintf(bufferTexto, sizeof(bufferTexto), "Y(CC%d)", config.numeroCC_Y);
        u8g2.drawStr(5, 33, bufferTexto);
        u8g2.drawFrame(45, 27, 40, 7);
        u8g2.drawBox(47, 29, anchoBarraY, 3);

        // --- C. Eje Z Torso ---
        anchoBarraZ = map(ultimoMidiZ, 0, 127, 0, 36);
        anchoBarraZ = constrain(anchoBarraZ, 0, 36);
        snprintf(bufferTexto, sizeof(bufferTexto), "Z(CC%d)", config.numeroCC_Z);
        u8g2.drawStr(5, 43, bufferTexto);
        u8g2.drawFrame(45, 37, 40, 7);
        u8g2.drawBox(47, 39, anchoBarraZ, 3);

        // --- D. Trill Bar POSICIÓN ---
        int anchoBarraTrillBarPos = map(trillMidiBarPosActual, 0, 127, 0, 36);
        anchoBarraTrillBarPos = constrain(anchoBarraTrillBarPos, 0, 36);

        snprintf(bufferTexto, sizeof(bufferTexto), "BP(CC%d)", config.trillBarCC_pos);
        u8g2.drawStr(5, 53, bufferTexto);

        u8g2.drawFrame(45, 47, 40, 7);
        if (anchoBarraTrillBarPos > 0)
        {
          u8g2.drawBox(47, 49, anchoBarraTrillBarPos, 3);
        }

        // --- E. Trill Bar PRESIÓN ---
        int anchoBarraTrillBarPres = map(trillMidiBarPresActual, 0, 127, 0, 36);
        anchoBarraTrillBarPres = constrain(anchoBarraTrillBarPres, 0, 36);

        snprintf(bufferTexto, sizeof(bufferTexto), "BR(CC%d)", config.trillBarCC_pres);
        u8g2.drawStr(5, 63, bufferTexto);

        u8g2.drawFrame(45, 57, 40, 7);
        if (anchoBarraTrillBarPres > 0)
        {
          u8g2.drawBox(47, 59, anchoBarraTrillBarPres, 3);
        }

        // ================================================================
        // 3. MITAD DERECHA: EL ECOSISTEMA DEL TRILL SQUARE (Estirado verticalmente)
        // ================================================================
        u8g2.drawFrame(92, 17, 35, 35);

        int puntoX = map(squareMidiXActual, 0, 127, 0, 31);
        int puntoY = map(squareMidiYActual, 0, 127, 0, 29);

        int posXFinal = 92 + 2 + puntoX;
        int posYFinal = 17 + 2 + (29 - puntoY);

        u8g2.drawDisc(posXFinal, posYFinal, 2);

        // Barra de presión (Z) del Trill Square
        int anchoZ = map(squareMidiZActual, 0, 127, 0, 31);
        u8g2.drawFrame(92, 57, 35, 7);
        if (anchoZ > 0)
        {
          u8g2.drawBox(94, 59, anchoZ, 3);
        }

        break;
      }

        // PANTALLA MONITOR TRILL
      case PANTALLA_MONITOR_TRILL:
      {
        // Título estético superior de la pantalla
        u8g2.setFont(u8g2_font_6x12_tr);
        u8g2.drawStr(5, 10, "MONITOR TRILL BAR");
        u8g2.drawHLine(0, 13, 128);

        u8g2.setFont(u8g2_font_5x8_tr);

        // 1. Barra y valor para la POSICIÓN
        int anchoBarraPos = map(trillMidiBarPosActual, 0, 127, 0, 70);
        anchoBarraPos = constrain(anchoBarraPos, 0, 70);
        snprintf(bufferTexto, sizeof(bufferTexto), "POS (CC%d)", config.trillBarCC_pos);
        u8g2.drawStr(5, 28, bufferTexto);
        u8g2.drawFrame(52, 22, 74, 7);          // Caja contenedora
        u8g2.drawBox(54, 24, anchoBarraPos, 3); // Relleno gráfico

        // Mostramos también el número MIDI exacto al lado
        snprintf(bufferTexto, sizeof(bufferTexto), "%d", trillMidiBarPosActual);
        u8g2.drawStr(5, 38, bufferTexto);

        // 2. Barra y valor para la PRESIÓN
        int anchoBarraPres = map(trillMidiBarPresActual, 0, 127, 0, 70);
        anchoBarraPres = constrain(anchoBarraPres, 0, 70);
        snprintf(bufferTexto, sizeof(bufferTexto), "PRE (CC%d)", config.trillBarCC_pres);
        u8g2.drawStr(5, 49, bufferTexto);
        u8g2.drawFrame(52, 43, 74, 7);
        u8g2.drawBox(54, 45, anchoBarraPres, 3);

        snprintf(bufferTexto, sizeof(bufferTexto), "%d", trillMidiBarPresActual);
        u8g2.drawStr(5, 59, bufferTexto);

        break;
      }
        // PANTALLA MONITOR SQUARE
      case PANTALLA_MONITOR_SQUARE:
      {
        u8g2.setFont(u8g2_font_6x12_tr);
        u8g2.drawStr(5, 10, "MONITOR TRILL SQ");
        u8g2.drawHLine(0, 13, 128);

        u8g2.setFont(u8g2_font_5x8_tr);

        // 1. Eje X (Horizontal): Muestra "X(CC 22): ValorMIDI"
        int anchoX = map(squareMidiXActual, 0, 127, 0, 55);
        anchoX = constrain(anchoX, 0, 55);
        snprintf(bufferTexto, sizeof(bufferTexto), "X(CC%d):%d", config.trillSquareCC_X, squareMidiXActual);
        u8g2.drawStr(5, 26, bufferTexto);
        u8g2.drawFrame(68, 20, 58, 6);
        u8g2.drawBox(70, 22, anchoX, 2);

        // 2. Eje Y (Vertical): Muestra "Y(CC 23): ValorMIDI"
        int anchoY = map(squareMidiYActual, 0, 127, 0, 55);
        anchoY = constrain(anchoY, 0, 55);
        snprintf(bufferTexto, sizeof(bufferTexto), "Y(CC%d):%d", config.trillSquareCC_Y, squareMidiYActual);
        u8g2.drawStr(5, 40, bufferTexto);
        u8g2.drawFrame(68, 34, 58, 6);
        u8g2.drawBox(70, 36, anchoY, 2);

        // 3. Presión Z: Muestra "Z(CC 24): ValorMIDI"
        int anchoZ = map(squareMidiZActual, 0, 127, 0, 55);
        anchoZ = constrain(anchoZ, 0, 55);
        snprintf(bufferTexto, sizeof(bufferTexto), "Z(CC%d):%d", config.trillSquareCC_Z, squareMidiZActual);
        u8g2.drawStr(5, 54, bufferTexto);
        u8g2.drawFrame(68, 48, 58, 6);
        u8g2.drawBox(70, 50, anchoZ, 2);

        break;
      }

        // PANTALLA MODO
      case PANTALLA_MODO_MIDI:
      {
        // Diseño reorganizado para que el valor seleccionado no se solape
        // con las descripciones de los modos en la pantalla OLED de 128x64.
        u8g2.setFont(u8g2_font_7x14_tf);
        u8g2.drawStr(6, 15, "MODO MIDI");
        u8g2.drawHLine(6, 19, 116);

        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.drawStr(6, 31, "CC: Mensj Conti");
        u8g2.drawStr(6, 42, "MPE: Mode Poly");

        // Zona inferior reservada para mostrar el modo seleccionado.
        u8g2.drawFrame(6, 48, 116, 15);
        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.drawStr(12, 59, "SELECCION:");

        if (!modoEdicionActivo || estadoParpadeo)
        {
          u8g2.setFont(u8g2_font_7x14_tf);
          u8g2.drawStr(86, 60, (config.modoMidi == 1) ? "MPE" : "CC");
        }
        break;
      }
      // PANTALLA CANAL
      case PANTALLA_CANAL:
        u8g2.setFont(u8g2_font_7x14_tf);
        u8g2.drawStr(10, 22, "CANAL MIDI");
        u8g2.drawHLine(10, 26, 75);
        if (!modoEdicionActivo || estadoParpadeo)
        {
          snprintf(bufferTexto, sizeof(bufferTexto), "Ch: %d", config.canalMidi);
          u8g2.setFont(u8g2_font_fub20_tr);
          u8g2.drawStr(20, 58, bufferTexto);
        }
        break;

      case PANTALLA_CC_X:
        u8g2.setFont(u8g2_font_7x14_tf);
        u8g2.drawStr(10, 22, "CC EJE X (PITCH)");
        u8g2.drawHLine(10, 26, 110);
        if (!modoEdicionActivo || estadoParpadeo)
        {
          snprintf(bufferTexto, sizeof(bufferTexto), "CC: %d", config.numeroCC_X);
          u8g2.setFont(u8g2_font_fub20_tr);
          u8g2.drawStr(20, 58, bufferTexto);
        }
        break;

      case PANTALLA_CC_Y:
        u8g2.setFont(u8g2_font_7x14_tf);
        u8g2.drawStr(10, 22, "CC EJE Y (ROLL)");
        u8g2.drawHLine(10, 26, 105);
        if (!modoEdicionActivo || estadoParpadeo)
        {
          snprintf(bufferTexto, sizeof(bufferTexto), "CC: %d", config.numeroCC_Y);
          u8g2.setFont(u8g2_font_fub20_tr);
          u8g2.drawStr(20, 58, bufferTexto);
        }
        break;

      case PANTALLA_CC_Z:
        u8g2.setFont(u8g2_font_7x14_tf);
        u8g2.drawStr(10, 22, "CC EJE Z (YAW)");
        u8g2.drawHLine(10, 26, 98);
        if (!modoEdicionActivo || estadoParpadeo)
        {
          snprintf(bufferTexto, sizeof(bufferTexto), "CC: %d", config.numeroCC_Z);
          u8g2.setFont(u8g2_font_fub20_tr);
          u8g2.drawStr(20, 58, bufferTexto);
        }
        break;

      case PANTALLA_CC_TRB_POS:
        u8g2.setFont(u8g2_font_7x14_tf);
        u8g2.drawStr(10, 22, "TRILL CC POS");
        u8g2.drawHLine(10, 26, 85);
        if (!modoEdicionActivo || estadoParpadeo)
        {
          snprintf(bufferTexto, sizeof(bufferTexto), "CC: %d", config.trillBarCC_pos);
          u8g2.setFont(u8g2_font_fub20_tr);
          u8g2.drawStr(20, 58, bufferTexto);
        }
        break;

      case PANTALLA_CC_TRB_PRES:
        u8g2.setFont(u8g2_font_7x14_tf);
        u8g2.drawStr(10, 22, "TRILL CC PRES");
        u8g2.drawHLine(10, 26, 95);
        if (!modoEdicionActivo || estadoParpadeo)
        {
          snprintf(bufferTexto, sizeof(bufferTexto), "CC: %d", config.trillBarCC_pres);
          u8g2.setFont(u8g2_font_fub20_tr);
          u8g2.drawStr(20, 58, bufferTexto);
        }
        break;

      case PANTALLA_SQUARE_CC_X:
        u8g2.setFont(u8g2_font_7x14_tf);
        u8g2.drawStr(10, 22, "SQUARE CC X"); // Horizontal
        u8g2.drawHLine(10, 26, 85);
        if (!modoEdicionActivo || estadoParpadeo)
        {
          snprintf(bufferTexto, sizeof(bufferTexto), "CC: %d", config.trillSquareCC_X);
          u8g2.setFont(u8g2_font_fub20_tr);
          u8g2.drawStr(20, 58, bufferTexto);
        }
        break;

      case PANTALLA_SQUARE_CC_Y:
        u8g2.setFont(u8g2_font_7x14_tf);
        u8g2.drawStr(10, 22, "SQUARE CC Y"); // Vertical
        u8g2.drawHLine(10, 26, 85);
        if (!modoEdicionActivo || estadoParpadeo)
        {
          snprintf(bufferTexto, sizeof(bufferTexto), "CC: %d", config.trillSquareCC_Y);
          u8g2.setFont(u8g2_font_fub20_tr);
          u8g2.drawStr(20, 58, bufferTexto);
        }
        break;

      case PANTALLA_SQUARE_CC_Z:
        u8g2.setFont(u8g2_font_7x14_tf);
        u8g2.drawStr(10, 22, "SQUARE CC Z (PRE)"); // Presión del dedo
        u8g2.drawHLine(10, 26, 110);
        if (!modoEdicionActivo || estadoParpadeo)
        {
          snprintf(bufferTexto, sizeof(bufferTexto), "CC: %d", config.trillSquareCC_Z);
          u8g2.setFont(u8g2_font_fub20_tr);
          u8g2.drawStr(20, 58, bufferTexto);
        }
        break;

      case PANTALLA_SUAVIZADO:
        u8g2.setFont(u8g2_font_7x14_tf);
        u8g2.drawStr(10, 22, "SUAVIZADO");
        u8g2.drawHLine(10, 26, 70);
        if (!modoEdicionActivo || estadoParpadeo)
        {
          snprintf(bufferTexto, sizeof(bufferTexto), "%d %%", config.suavizadoAlpha);
          u8g2.setFont(u8g2_font_fub20_tr);
          u8g2.drawStr(20, 58, bufferTexto);
        }
        break;

      case PANTALLA_ZONA_MUERTA:
        u8g2.setFont(u8g2_font_7x14_tf);
        u8g2.drawStr(10, 22, "ZONA MUERTA");
        u8g2.drawHLine(10, 26, 85);
        if (!modoEdicionActivo || estadoParpadeo)
        {
          snprintf(bufferTexto, sizeof(bufferTexto), "Val: %d", config.zonaMuerta);
          u8g2.setFont(u8g2_font_fub20_tr);
          u8g2.drawStr(20, 58, bufferTexto);
        }
        break;

      case PANTALLA_CALIBRANDO:
        u8g2.setFont(u8g2_font_7x14_tf);
        u8g2.drawStr(20, 22, "CALIBRACION");
        u8g2.drawHLine(15, 26, 98);
        if (estadoParpadeo)
        {
          u8g2.setFont(u8g2_font_6x12_tr);
          u8g2.drawStr(10, 48, "<- MUEVA EL TORSO ->");
        }
        break;

      // 👇 PANTALLA DEL TRILL BAR COMPLETA
      case PANTALLA_CALIBRADO_TRILL_BAR:
      {
        u8g2.setFont(u8g2_font_7x14_tf);
        u8g2.drawStr(10, 22, "CALIB. TRILL BAR");
        u8g2.drawHLine(10, 26, 110);

        int minVisible = (config.trillBarMinPresion <= 0) ? 450 : config.trillBarMinPresion;
        int maxVisible = (config.trillBarMaxPresion <= 0) ? 6500 : config.trillBarMaxPresion;

        u8g2.setFont(u8g2_font_6x12_tr);
        snprintf(bufferTexto, sizeof(bufferTexto), "MIN (Suave): %d", minVisible);
        u8g2.drawStr(15, 42, bufferTexto);

        snprintf(bufferTexto, sizeof(bufferTexto), "MAX (Fuerte): %d", maxVisible);
        u8g2.drawStr(15, 56, bufferTexto);
        break;
      }

      // =========================================
      // 🆕 RENDERIZADO GRÁFICO PANTALLAS MPE
      // =========================================
      case PANTALLA_MPE_BAR_MODO:
        u8g2.setFont(u8g2_font_7x14_tf);
        u8g2.drawStr(10, 22, "MPE BAR MODO");
        u8g2.drawHLine(10, 26, 110);

        u8g2.setFont(u8g2_font_6x12_tr);

       // Forzamos un control estricto de los dos únicos modos posibles
        if (config.mpeBarModo == 0) 
        {
            u8g2.drawStr(15, 48, "> Ribbon (Continuo)");
        } else 
        {
            u8g2.drawStr(15, 48, "> Keys (Cuant.)");
        }
        break;

      case PANTALLA_MPE_BAR_OCTAVAS:
        u8g2.setFont(u8g2_font_7x14_tf);
        u8g2.drawStr(10, 22, "MPE BAR RANGO");
        u8g2.drawHLine(10, 26, 110);

        u8g2.setFont(u8g2_font_6x12_tr);

        char textoOctavas[24]; // 🔹 Array de 24 bytes seguro
        snprintf(textoOctavas, sizeof(textoOctavas), "Octavas: %d", config.mpeRangoOctavas);
        u8g2.drawStr(20, 48, textoOctavas);

        break;

      case PANTALLA_MPE_BAR_NOTA:
      {
        u8g2.setFont(u8g2_font_7x14_tf);
        u8g2.drawStr(10, 22, "MPE BAR NOTA BASE");
        u8g2.drawHLine(10, 26, 110);

        u8g2.setFont(u8g2_font_6x12_tr);
        int octavaVisual = (config.mpeNotaBase / 12) - 1;
        const char *nombresNotas[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

        char textoNota[24]; // 🔹 Array de 24 bytes seguro
        snprintf(textoNota, sizeof(textoNota), "Nota: %s%d (MIDI %d)",
                 nombresNotas[config.mpeNotaBase % 12], octavaVisual, config.mpeNotaBase);
        u8g2.drawStr(15, 48, textoNota);
      }
      break;

      case PANTALLA_MPE_BAR_ESCALA:
        u8g2.setFont(u8g2_font_7x14_tf);
        u8g2.drawStr(10, 22, "MPE BAR ESCALA");
        u8g2.drawHLine(10, 26, 110);

        u8g2.setFont(u8g2_font_6x12_tr);

        // Al estar en modo edición mostramos el chivato visual, si no, la escala limpia
        char textoEscala[32];
        snprintf(textoEscala, sizeof(textoEscala), "> %s", nombresEscalas[config.mpeEscala]);
        u8g2.drawStr(15, 48, textoEscala);
        break;

      case PANTALLA_MPE_BAR_TRANSPOSICION:
      u8g2.setFont(u8g2_font_7x14_tf);
      u8g2.drawStr(10, 22, "MPE TRANSPOSE");
      u8g2.drawHLine(10, 26, 110);

      u8g2.setFont(u8g2_font_6x12_tr);
      char bufferTrans[32]; // Aseguramos la declaración del buffer local
      if (config.transposicion == 0)
      {
        snprintf(bufferTrans, sizeof(bufferTrans), "> Centro (0 semit)");
      }
      else if (config.transposicion > 0)
      {
        snprintf(bufferTrans, sizeof(bufferTrans), "> Subir (+%d semit)", config.transposicion);
      }
      else
      {
        snprintf(bufferTrans, sizeof(bufferTrans), "> Bajar (%d semit)", config.transposicion);
      }
      u8g2.drawStr(15, 48, bufferTrans);
      break;  
      }

    } while (u8g2.nextPage());
  }
}

void actualizarPantallaMuestra(const char *mensaje)
{
  u8g2.clearBuffer();

  u8g2.firstPage();
  do
  {
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.drawStr(10, 35, mensaje);
  } while (u8g2.nextPage());
}
