# EcoSort — AI-Powered Smart Waste Sorting Robot

Final Year Project — Bachelor of Science (Honours) in Intelligent Robotics
Faculty of Artificial Intelligence and Engineering, Multimedia University
Session 2025/2026

**Author:** Hamar Fajar Adel A (1221305879)
**Supervisor:** Dr. Sarina

---

## Overview

EcoSort is a low-cost waste sorting robot for small settings such as homes,
schools, and small offices, where industrial sorting machines are far too
expensive to use. The user places one item at a time on the platform, a camera
takes a picture, and a YOLO11 Nano model classifies it as **plastic**, **paper**,
or **metal**. Anything the model does not recognise with enough confidence goes
to a fourth **general waste** bin. A servo mechanism then moves the item to the
matching bin, and ultrasonic sensors report each bin's fill level to a phone
through the Blynk IoT platform.

The project contributes to United Nations Sustainable Development Goal 12
(Responsible Consumption and Production) by making recycling automatic and
affordable in everyday places where manual sorting has not worked well.

The system was built in two phases. The first phase used a laptop, an Arduino, a
USB webcam, and a single servo to prove the concept. The second phase made it
stand-alone by moving the model onto a **Raspberry Pi 5** and adding the physical
bins, the confirmation buttons, and the metal-confirmation sensor.

## Objectives

1. Train a machine-learning model to identify waste by type.
2. Build a low-cost robot that automatically sorts waste into plastic, paper, and
   metal.
3. Enable real-time monitoring of bin fill levels through IoT integration.

## Repository layout

| Path | What it is |
| --- | --- |
| `vision/sort_controller.py` | Main detection loop: runs the model, shows the confirm/reject window for low-confidence items, keeps running accuracy stats, and sends commands to the board over serial. |
| `vision/webcam_detect.py` | Minimal webcam viewer with detection overlay. Handy for checking the model with no hardware attached. |
| `firmware/ecosort_arduino.ino` | Arduino sketch: platform servo (MG996R), flap-gate servo (SG90), 16×2 I2C LCD, status LEDs, confirm/reject buttons, and four HC-SR04 ultrasonic sensors for bin fill level. |
| `models/` | Trained weights: `yolov8_best.pt` and `yolov11_best.pt`. |
| `results/` | Confusion matrices, PR curve, training curves, and the system architecture diagram. |
| `requirements.txt` | Python dependencies for the vision side. |

## Results

These are the results reported in the final report.

**Model (validation set).** The deployed YOLO11 Nano model reached **0.925
mAP@50** overall, chosen after nine training experiments comparing YOLOv8 Nano and
YOLO11 Nano on the rebuilt dataset.

| Class | mAP@50 |
| --- | --- |
| Metal | 0.907 |
| Paper | 0.976 |
| Plastic | 0.891 |
| **Overall** | **0.925** |

**Physical test (50 real items, one at a time, normal room lighting).** The
camera correctly classified **38 of 50 items (76%)**. The set was 15 plastic, 15
paper, 15 metal, and 5 random objects outside the three classes.

| Class | Items | Correct | Accuracy |
| --- | --- | --- | --- |
| Plastic | 15 | 12 | 80.0% |
| Paper | 15 | 13 | 86.7% |
| Metal | 15 | 9 | 60.0% |
| Random / other | 5 | 4 | 80.0% |
| **Overall** | **50** | **38** | **76.0%** |

Paper scored highest because its shape and texture are consistent. Metal scored
lowest: reflective surfaces and irregular shapes lowered the model's confidence,
so several metal items fell below the 70% threshold and were sent to general
waste rather than mis-sorted. This is the intended behaviour — it is better to
put an uncertain item in general waste than in the wrong bin.

**Inductive metal sensor (bench test).** Tested on its own, the inductive sensor
correctly detected metal in **13 of 15 items (86.7%)** and stayed off for every
non-metal item. It was **not** included in the final integrated build (see
Limitations).

**IoT bin monitoring.** The four HC-SR04 sensors measured fill distance to about
1 cm, a five-consecutive-reading check prevented false alarms, and a Blynk push
notification reached the phone in about 2–3 seconds over Wi-Fi.

## How the sorting works

Items at or above **70% confidence** sort automatically. Below that, the system
opens a confirmation window and the user accepts the result with the **Confirm**
button or rejects it with the **Reject** button. Rejected or unconfirmed items go
to general waste.

## Hardware

- **Raspberry Pi 5** — runs the camera and the YOLO11 Nano model (final build).
- **Arduino Mega 2560** — drives the servos, LCD, LEDs, buttons, and ultrasonic
  sensors.
- **MG996R** positional servo for platform rotation, **SG90** servo for the flap
  gate.
- **16×2 I2C LCD** at address `0x27`, red/yellow/green status LEDs, confirm and
  reject buttons.
- **4× HC-SR04** ultrasonic sensors (one per bin) for fill-level monitoring.
- **Inductive proximity sensor** for metal confirmation (bench-tested only).
- **Cardboard enclosure** to keep the cost down.

Pin assignments are at the top of the firmware sketch and must match your wiring.

## Running the vision side

```bash
pip install -r requirements.txt
python vision/webcam_detect.py          # model only, no hardware needed
```

For the full loop with hardware attached, set your serial port and run the
controller:

```bash
ECOSORT_PORT=COM4 python vision/sort_controller.py
```

## Limitations and notes

- **Inductive sensor not integrated.** The sensor worked correctly on its own
  (86.7%), but during integration its serial output stopped toggling and could
  not be fixed on the bench, so it was left out of the final build. The working
  system therefore relies on the camera result plus the confirm/reject buttons,
  not full camera-and-sensor fusion.

- **Metal is the weakest class.** Reflective, crumpled, and irregularly shaped
  items (shiny cans, dark crushed plastic film) lower the model's confidence.
  Rigid transparent plastic bottles are handled well.

- **The code here is the earlier laptop/Arduino iteration.**
  `vision/sort_controller.py` opens a Windows COM port and sends discrete
  commands (`LED YELLOW`, `LCD <class>`, `SERVO RIGHT`), while the firmware also
  understands a fused `RESULT:<CLASS>:<CONF>` line. The final report's stand-alone
  build moved this loop onto a Raspberry Pi 5 with Blynk for the IoT dashboard.
  Reconciling both halves on a single `RESULT:` protocol is the natural next step
  before this exact code runs end-to-end.

## Future work

- Add "clean" background images (no waste) and a larger validation set for more
  realistic model numbers.
- Repair or replace the inductive sensor and complete camera-and-sensor fusion.
- Extend beyond three material classes once the mechanical platform allows it.
- Finish the continuous-learning dashboard that was drafted but not built.
