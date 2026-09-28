# Magnetometer — AK09916 3D Calibration

Standalone AK09916 magnetometer example inside the ICM-20948.

## What it does

- STM32 communicates with the ICM-20948 over SPI
- ICM-20948 internal I²C master communicates with AK09916
- Verifies AK09916 identity
- Collects 3D min/max data while the board is rotated
- Computes hard-iron offset and per-axis scale
- Converts corrected data to microtesla (µT)

## Calibration

Rotate the board slowly through as many orientations as possible, not only flat rotations around Z.

The calibrated output is aligned to the accelerometer/gyroscope frame used by the INS project:

```text
Xmag_aligned = +Xmag
Ymag_aligned = -Ymag
Zmag_aligned = -Zmag
```

For final installation, recalibrate the magnetometer in the actual vehicle/drone environment because motors, power cables, steel parts and magnets can distort the field.
