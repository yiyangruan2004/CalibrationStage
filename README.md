# CalibrationStage

## Project background and significance

CalibrationStage is a C++/Qt application developed for the Chen Lab to coordinate instrument control and spatial measurements during acoustic field mapping and calibration. The system combines a function generator, a PicoScope oscilloscope, and a self-assembled gantry containing three motorized stages and one free stage. The three motorized axes provide X, Y, and Z positioning for measurements across a selected region.

Acoustic field measurements require excitation and signal acquisition to be repeated at many positions. CalibrationStage brings these operations into a single graphical user interface (GUI), reducing the manual work involved in switching between instrument controls, repositioning the stage, and recording measurements. Users can inspect individual waveforms during alignment, acquire spatial scans, and save results with their measurement settings for later analysis.

The application communicates with each instrument through its corresponding interface:

| Instrument | Communication interface | Role in the measurement |
| --- | --- | --- |
| Keysight 33500B function generator, as identified in the GUI | VISA, using SCPI commands | Configures sinusoidal burst excitation and sends a burst when triggered. |
| PicoScope using the PS5000A API | Official PicoSDK | Configures acquisition, waits for an external trigger, and retrieves the measured waveform. |
| Velmex motion controller | Serial port through Qt SerialPort, configured for 9600 baud, 8 data bits, no parity, and one stop bit | Moves the three motorized axes, reads coordinates, sets the coordinate origin, and accepts a stop command. |

Instrument connections and measurement controls are organized into **Connection** and **Control** tabs. The GUI provides connection indicators, configuration controls, stage coordinates, a waveform chart, a pressure readout, and scan progress. Connection identifiers and many measurement settings are retained between sessions through `QSettings`. Logging records timestamped messages and severity levels, while information and warning message boxes provide completion notices and troubleshooting guidance.

Users can adjust the function generator's frequency, amplitude, and burst cycle count. PicoScope controls include voltage range, sample count, and timebase. The native acquisition code opens the scope at 16-bit resolution, enables channel A with AC coupling, and configures a rising-edge external trigger. For an individual measurement, the application arms the scope, triggers the function generator, reads the captured samples, updates the waveform chart, and exports the capture. The chart displays amplitude in millivolts against sample index.

Stage controls support incremental movement along each axis, movement to entered coordinates, and setting the current position as zero. Movement buttons also have keyboard shortcuts. Coordinates and movement increments are displayed in inches; internally, the program represents positions as integer motor steps using a conversion of **0.00025 inch per step**. This is the software's coordinate conversion and does not establish the mechanical positioning accuracy of the assembled gantry.

Automated acquisition follows this sequence:

1. The user defines minimum and maximum X, Y, and Z coordinates and a scan step size. Holding one or two axes fixed allows a plane or line to be sampled using the same scan mechanism.
2. The program checks instrument readiness, coordinate ordering, and step size, then constructs a serpentine trajectory. X traversal reverses between successive Y rows, and the rows repeat across Z layers.
3. The stage moves to the maximum corner and then the minimum corner for boundary inspection. The interface enters a ready state and waits for the user to select **Continue**.
4. At each scan position, the application moves the stage, arms the PicoScope, triggers the function generator, and reads a waveform. The chart and remaining-point count update as acquisition progresses.
5. Pausing finishes the current measurement and returns all controls and device configuration to the user. Continuing checks device readiness and resumes at the next unmeasured point on the existing trajectory. The **Kill** control sends the motion controller's stop command and cancels the scan.
6. After all points have been acquired, the program exports the scan and returns the stage to the position selected by its peak-value comparison.

The boundary inspection consists of visiting the two specified corners. Scan results contain one peak-derived pressure value per sampled position; complete waveforms are saved by the individual-trigger export. The native waveform reader retains the sign of the sample with the greatest absolute amplitude, and the scan selects its return position by comparing these signed peak values.

CSV exports preserve excitation frequency, excitation amplitude, the recorded scope range, sample count, and position metadata. Individual captures use `Trig.csv`, followed by numbered names such as `Trig_1.csv`, and contain a peak-pressure entry followed by waveform voltages. Completed scans use `Scan.csv` and numbered variants, with data rows ordered as `X,Y,Z,pressure`. Existing filenames are skipped when choosing the next output name. Scan data is written at completion; cancellation resets the accumulated scan data without exporting a partial scan.

Exported X, Y, and Z values are in inches, converted from internal motor steps using `0.00025` inch per step. Scan row 9 labels the data columns as `X(in),Y(in),Z(in),P(kPa)`. Pressure export divides the measured voltage in millivolts by `pico.sens`, the hydrophone sensitivity selected in the GUI (default **0.2149 mV/kPa**). The exports contain metadata lines before the measurement rows, so analysis code must account for that format when importing them.

The measurement output directory is `../Data` relative to the application's working directory, while `log.txt` is appended in the working directory itself. These locations depend on how the application is launched. The included visualization notebook provides examples of three-dimensional field plots, two-dimensional maps, line profiles, and a representative waveform using the accompanying CSV datasets.

The source also contains a WebAssembly simulation path. In that mode, instrument connections and stage movement are simulated, and one fixed, noise-free waveform is cached and reused for subsequent reads. Changing the sample count or offset rebuilds the cached waveform. This supports demonstrating the interface and scan workflow without connected hardware.

## Repository structure

The project keeps application code in one flat source folder, with tests and vendor interfaces alongside it:

```text
.
|-- Root/
|   |-- CMakeLists.txt       Application and test build; shared sources listed once
|   |-- main.cpp / .h       Application entry point
|   |-- mainwindow.cpp / .h GUI behavior, chart, settings, and scan controls
|   |-- mainwindow.ui       Qt Designer layout
|   |-- device.cpp / .h     Common device interface and connection states
|   |-- fg.cpp / .h         Function generator
|   |-- pico.cpp / .h       PicoScope acquisition and simulation
|   |-- vmx.cpp / .h        Motion controller and coordinate conversion
|   |-- scan.cpp / .h       Scan interface, trajectory, and acquisition sequence
|   |-- filer.cpp / .h      Logging, message boxes, and CSV export
|   |-- tests/              Scan lifecycle tests with hardware stubs
|   |-- inc/                Bundled PicoSDK and VISA headers
|   `-- lib/                Windows instrument import libraries
|-- Visualization/          Analysis notebook and example CSV datasets
|-- Data/                   Local measurement output (launch-directory dependent)
|-- docs/                   Local project documents and hardware references
|-- .vscode/                Editor configuration pointing to Root/
|-- GUI.zip                 GUI archive
|-- .gitignore              Local exclusion rules
`-- README.md               Project background and repository structure
```

`MainWindow` owns the devices and one reusable `Scan`. The window handles controls and display updates; `Scan` runs boundary checks, acquisition, and export on its own thread, with one internal scan state and state-update signals. Its public scan actions are boundary checking and pausable scanning; cancellation follows the VMX kill flag. Devices belong to the scan thread during a run and return to the UI thread afterward; `Filer` writes results and messages. Headers expose interfaces, while scan algorithms live in `scan.cpp`. The Qt Designer layout and dashboard theme remain in `mainwindow.ui` and `mainwindow.cpp`.

`Root/CMakeLists.txt` is the single build definition. It requires CMake 3.16 or newer and C++17, and uses Qt Widgets and Charts. Native builds also link Qt SerialPort and the bundled PicoSDK/VISA import libraries. WebAssembly builds use the existing instrument simulations. Tests are optional (`-DBUILD_TESTING=ON`); their target reuses the application's UI and scan sources with device stubs and Qt Test. New application builds do not require Qt Test by default; existing build directories retain their selected setting.

`Visualization/Visualization.ipynb` uses pandas, NumPy, and Matplotlib for the included measurements. Local documents in `docs/` and generated output in `Root/build/` are ignored by Git and are not required to understand the source layout.
