# BABY AIR — Documentación técnica

## Estructura
```
Source/
  Parameters.h            IDs estables: midAir, highAir, link, trim, bypass
  PluginProcessor.*       Hilo de audio: DSP, medidores, detector de actividad, estado
  PluginEditor.*          Ventana (redimensionable, proporción fija 3:2), LINK, conexiones a parámetros
  DSP/AirEngine.h         Motor dinámico MID AIR / HIGH AIR
  DSP/Biquad.h            Filtros y envolventes
  DSP/ActivityDetector.h  Detector RMS → valor normalizado para la mascota (solo lee audio)
  GUI/DeviceArt.*         El "juguete": carcasa, gorra con logo, paneles, stickers, etiquetas (se cachea en imagen)
  GUI/BabyLookAndFeel.*   Perillas, interruptor LINK, botón BYPASS
  GUI/MeterPanel.*        Medidores reales L/R, PEAK, RMS, CLIP
  GUI/MascotEngine.h      Máquina de estados + física de la mascota + partículas (sin dependencias de JUCE)
  GUI/MascotView.*        Baby Demon en capas vectoriales (cola, alas, sudadera, cabeza, cuernos, halo, ojos, párpados, cejas, boca, patas)
Tests/                    DspTests, HostTest (carga el VST3 + snapshots + LINK), MascotTests
```

## DSP (diseño original; no copia ni afirma conocer algoritmos comerciales)
Topología paralela: la señal seca **no se filtra**; a ella se suman dos bandas realzadas.
Cada banda = band-pass 2.º orden (MID 4.5 kHz Q 0.9, HIGH 13 kHz Q 1.3, limitado a 0.40·fs) → ganancia dependiente
del nivel propio de la banda → × cantidad (0–100 %). La curva del realce:
subida suave desde el piso de ruido (no sube el siseo) → realce completo a niveles bajos/medios → reducción con
soft-knee a niveles altos (evita agudos duros y sibilancia). Detector estéreo enlazado (imagen estable).
Los band-pass de 2.º orden se eligieron tras medir que divisores de fase lineal/LR4 producían un hueco en el cruce.
Valores son puntos de partida experimentales (ver tabla en `AirEngine.h`): **no se hizo comparación auditiva con
Fresh Air** (no hay oído ni la referencia aquí); afinar por oído es el siguiente paso real.
* Suavizado de parámetros (25 ms), protección de denormales, bypass con crossfade, TRIM ±12 dB.
* Mono y estéreo, buffers variables 1–4096, 44.1–192 kHz, **latencia 0** (sin lookahead ni oversampling: no hay saturación).
* Sin asignaciones de memoria ni bloqueos en `processBlock`.
* `getBypassParameter()` apunta al parámetro BYPASS propio, así el botón del DAW usa el mismo crossfade.

## Hilos
Audio → GUI solo mediante `std::atomic` (picos, RMS, evento de clip, actividad y nivel). La GUI nunca escribe nada de vuelta;
la animación no puede alterar el sonido. Con el editor oculto, los timers bajan a 4 Hz y no se dibuja.
Detector (interno, no expuesto): umbral −40 dBFS, histéresis 4 dB, ataque 30 ms, release 350 ms.

## Mascota
Estados: SLEEPING → WAKING_UP → HAPPY → EXCITED → RETURNING_TO_SLEEP (transiciones por suavizado exponencial, ~60 fps;
30 fps durmiendo). Capas animadas independientes: ojos, párpados, cejas, boca, cabeza, alas, cola, cuerpo, patas, halo.
Corazones verdes: sistema de partículas aparte, intervalo mínimo 0.28 s, flotan, giran, se desvanecen. Clic en la mascota = pequeño rebote con corazones.
Está dibujada con vectores (no hay sprites): la figura del PNG de referencia no se usó como textura.

## LINK
Mantiene la **relación** entre MID AIR y HIGH AIR (razón; desplazamiento si alguno está cerca de 0), nunca los iguala.
Se recaptura la relación al activar LINK, al mover una perilla con LINK apagado y ante cambios del host. Es una función de la
interfaz: la automatización del DAW sobre una sola perilla no mueve la otra.

## Persistencia
Todos los parámetros vía `AudioProcessorValueTreeState` (automatizables, recall exacto verificado). También se guarda el tamaño de ventana
y el modo IN/OUT del medidor (no automatizables).
