# BABY AIR by PAPI SERCH — Instalación y compilación

## Plugin ya compilado (este entorno)
`build/BabyAir_artefacts/Release/VST3/BABY AIR.vst3` — **Linux x86_64, VST3**.
Cópialo a `~/.vst3/` (usuario) o `/usr/lib/vst3/` (sistema).

> **FL Studio corre en Windows/macOS.** Este entorno es Linux, así que el `.vst3` compilado aquí **no** se
> puede cargar en FL Studio. Para tu DAW hay que compilar el mismo código en tu sistema (instrucciones abajo).
> El código es multiplataforma; en Windows/macOS no se probó aquí.

## Compilar en tu computadora
Requisitos: CMake ≥ 3.22, compilador C++17 (Visual Studio 2022 / Xcode / GCC o Clang) y Git.

```bash
git clone <tu repositorio> BabyAir && cd BabyAir
cmake -B build -DCMAKE_BUILD_TYPE=Release            # descarga JUCE 8.0.8 automáticamente
cmake --build build --config Release --target BabyAir_VST3
# opcional: -DBABYAIR_JUCE_DIR=/ruta/a/JUCE para usar una copia local
```
* **Windows (GitHub Actions):** sube el proyecto a GitHub; el workflow `.github/workflows/build.yml` compila solo. En la pestaña *Actions* abre la ejecución y descarga el artefacto `BABY-AIR-windows`; copia `BABY AIR.vst3` a `C:\Program Files\Common Files\VST3\` y en FL Studio: *Options → Manage plugins → Find plugins*.
* **macOS:** se generan **VST3 y AU** (binario universal arm64+x86_64). VST3 → `~/Library/Audio/Plug-Ins/VST3/`, AU → `~/Library/Audio/Plug-Ins/Components/`.
* **AU** solo se puede compilar en macOS (necesita Xcode); no se pudo generar aquí.
* **AAX** no incluido: requiere el SDK de Avid y su licencia.

## Licencia de JUCE
JUCE es AGPLv3 / licencia comercial. Para **distribuir o vender** BABY AIR necesitas una licencia comercial de JUCE o publicar el código bajo AGPLv3.

## Pruebas
```bash
cmake --build build --target BabyAirDspTests BabyAirTests
./build/BabyAirDspTests                                                    # DSP: respuesta, THD, ruido, estéreo, fs 44.1–192 kHz
xvfb-run -a ./build/BabyAirTests_artefacts/Release/BabyAirTests "build/BabyAir_artefacts/Release/VST3/BABY AIR.vst3"   # carga el VST3 como host real
xvfb-run -a ./build/BabyAirTests_artefacts/Release/BabyAirTests --snapshots out/   # PNGs de la interfaz en los 5 estados
g++ -std=c++17 -ISource Tests/MascotTests.cpp -o mascot_tests && ./mascot_tests    # máquina de estados de la mascota
```
