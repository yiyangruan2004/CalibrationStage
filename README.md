# CalibrationStage

## Significance, protocol, and method

CalibrationStage is a Qt/C++ desktop application for coordinating a PicoScope, function generator, and VMX motion controller during calibration measurements. The operator connects and configures each device, triggers individual captures or scans a bounded volume, and exports measurements as CSV data. Source code separates window behavior, scan orchestration, hardware integrations, export formatting, and testable utilities into focused modules.

## Repository structure

```text
.
|-- Application/          Local application build or deployment output
|-- Data/                 Runtime data output location
|-- Documents/            Project documents, reports, forms, and slides
|-- Reference/            Hardware manuals, calibration documents, and SDK installers
|   |-- ONDA/             ONDA hydrophone documentation and calibration records
|   |-- fg/               Function generator documentation and VISA installer archive
|   |-- pico/             PicoSDK installer archives
|   `-- vmx/              VMX/VXM controller documentation
|-- Root/                 Qt/C++ application source
|   |-- CMakeLists.txt    CMake project definition
|   |-- inc/              Vendored PicoSDK and VISA headers
|   |-- lib/              Windows import libraries
|   |-- main.cpp          Application entry point
|   |-- mainwindow.*      Main Qt window and UI behavior
|   |-- scan.*            Scan orchestration and focus selection
|   |-- filer.*           Logging and shared CSV capture metadata formatter
|   |-- device.*          Shared device interface
|   |-- fg.*              Function generator integration
|   |-- pico.*            PicoScope integration
|   |-- vmx.*             Motion controller integration and coordinate type
|   `-- tests/            QtTest coverage for scan and export utilities
|-- Visualization/        CSV examples and analysis notebook
|-- Summer update.docx    Project update document
|-- .gitattributes        Git attributes
|-- .gitignore            Git ignore rules
`-- README.md             Repository structure overview
```
