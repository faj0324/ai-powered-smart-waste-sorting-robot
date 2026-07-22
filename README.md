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
takes a picture, and a **YOLO11 Nano** model classifies it as **plastic**,
**paper**, or **metal**. Anything the model does not recognise with enough
confidence goes to a fourth **general waste** bin. A servo mechanism then moves
the item to the matching bin, and ultrasonic sensors report each bin's fill level
to a phone through the **Blynk** IoT platform.

The project contributes to United Nations Sustainable Development Goal 12
(Responsible Consumption and Production) by making recycling automatic and
affordable in everyday places where manual sorting has not worked well.

The system was built in two phases. The first phase used a laptop, an Arduino, a
USB webcam, and a single servo to prove the concept. The second phase made it
stand-alone by moving the model onto a **Raspberry Pi 5** and adding the physical
bins, the confirm/reject buttons, and the metal-confirmation sensor.

## Objectives

1. Train a machine-learning model to classify waste as plastic, paper, or metal.
2. Build a low-cost robot that automatically sorts waste by type.
3. Enable real-time monitoring of bin fill levels through IoT integration.

All three objectives were achieved at prototype level.

## Repository layout

| Path | What it is |
| --- | --- |
| `vision/sort_controller.py` | Main detection loop: runs the model, opens the confirm/reject window for low-confidence items, keeps running accuracy stats, and sends commands to the board over serial. |
| `vision/webcam_detect.py` | Minimal webcam viewer with a detection overlay. Handy for checking the model with no hardware attached. |
| `firmware/ecosort_arduino.ino` | Arduino sketch: platform servo (MG996R), flap-gate servo (SG90), 16×2 I2C LCD, status LEDs, confirm/reject buttons, and four HC-SR04 ultrasonic sensors for bin fill level. |
| `models/yolov11_best.pt` | **The deployed model** (YOLO11 Nano, Experiment 9). |
| `models/yolov8_best.pt` | The runner-up (YOLOv8 Nano, Experiment 8), kept for reference from the final comparison. |
| `results/` | Confusion matrices, PR curve, training curves, and the system architecture diagram. |
| `requirements.txt` | Python dependencies for the vision side. |

## The model

The final model was chosen after **nine training experiments**. The biggest single
improvement came from dropping a generic "trash" class and rebuilding the dataset
around three clean recyclable classes, which pushed mAP@50 from 0.715 (Experiment
6) to 0.930 (Experiment 7).

The last two experiments were a **direct head-to-head** between the two candidate
architectures — same 640×640 input, same Tesla T4 GPU, same dataset (Version 5),
with only the model changed:

| Metric | YOLOv8 Nano (Exp 8) | YOLO11 Nano (Exp 9, **deployed**) |
| --- | --- | --- |
| Overall mAP@50 | 0.923 | **0.925** |
| Plastic mAP@50 | 0.881 | **0.891** |
| Paper mAP@50 | 0.977 | 0.976 |
| Metal mAP@50 | 0.912 | 0.907 |
| Model file size | 6.3 MB | **5.5 MB** |

**YOLO11 Nano was deployed.** It improved the weakest class (plastic) by a full
percentage point, and its smaller weights file loads faster and uses less RAM on
the Raspberry Pi 5 — which matters more on the robot than the tiny dips in paper
and metal. YOLOv8 Nano is included here only as the runner-up from that
comparison.

The deployed model was trained with COCO pre-trained weights, 45 epochs, 640×640
input, on Lightning AI Studio (Tesla T4). At runtime, items scoring **≥ 70%**
sort automatically; anything below 70% is treated as uncertain and sent to general
waste. That 70% cut-off was set from physical testing, where hard-to-read items
typically scored 55–68%.

> Note: the two sample scripts in `vision/` currently load `yolov8_best.pt`.
> Point `MODEL_PATH` at `yolov11_best.pt` to run the deployed configuration.

## Results

These are the results reported in the final report.

**Validation set (deployed YOLO11 Nano).** Overall **0.925 mAP@50**.

| Class | mAP@50 |
| --- | --- |
| Metal | 0.907 |
| Paper | 0.976 |
| Plastic | 0.891 |
| **Overall** | **0.925** |

**Physical test — 50 real items, one at a time, normal room lighting.** The
camera correctly classified **38 of 50 (76%)**. The set was 15 plastic, 15 paper,
15 metal, and 5 random objects outside the three classes.

| Class | Items | Correct | Accuracy |
| --- | --- | --- | --- |
| Plastic | 15 | 12 | 80.0% |
| Paper | 15 | 13 | 86.7% |
| Metal | 15 | 9 | 60.0% |
| Random / other | 5 | 4 | 80.0% |
| **Overall** | **50** | **38** | **76.0%** |

Paper scored highest because its shape and texture are consistent. Metal scored
lowest: reflective surfaces and irregular shapes lower the model's confidence, so
several metal items fell below the 70% threshold and were sent to general waste
rather than mis-sorted. This is the intended behaviour — an uncertain item is
better off in general waste than in the wrong bin.

**Inductive metal sensor (bench test).** On its own the sensor correctly detected
metal in **13 of 15 items (86.7%)** and stayed off for every non-metal item. It
was **not** included in the final integrated build (see Limitations).

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
- **MG996R** positional servo for platform rotation; **SG90** servo for the flap
  gate.
- **16×2 I2C LCD** at address `0x27`; red/yellow/green status LEDs; confirm and
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

## Limitations

- **Metal is the weakest class.** The camera is least confident on metal;
  reflective surfaces and irregular shapes push many correct predictions below the
  70% threshold, so they end up in general waste. The inductive sensor was meant
  to cover this, but even on the bench, small or distant metal gave a weak
  response.
- **Sensor integration failed.** The inductive sensor worked on its own but could
  not be integrated cleanly: its output stayed latched and would not toggle, and a
  short on the battery line later damaged it — so full camera-and-sensor fusion
  could not be validated on the final build.
- **Wiring stability.** Jumper-wire connections on the cardboard frame loosened as
  the platform rotated, causing frequent faults and rebuilds during development.
- **Structural weakness.** The cardboard enclosure was cheap and easy to modify
  but flexed under load, and the chute lost shape without bracing, which affected
  alignment.
- **Small test sample.** The physical test was 50 items. That is enough to show
  the trend but too small for firm per-class percentages — a single misread shifts
  a class by about seven points, so the numbers are approximate.
- **One item at a time**, and the camera still depends on lighting.

## Future work

- **Replace the rotating platform with a linear actuator.** A chute guide sliding
  along a straight path to the correct bin would be more precise and load-bearing
  than the servo-driven cardboard platform (omitted here due to budget).
- **Move the hand-wired electronics onto a custom PCB.** One board for the Arduino
  connections, sensor wiring, servo wiring, and power regulation would be far more
  reliable and reproducible than the current two hand-wired circuits.
- **Finish and validate the continuous-learning dashboard.** A local dashboard was
  designed to let the user re-label mis-sorted items and feed them back into
  training, so the model learns from its own mistakes; it was started but not
  fully tested by the deadline.
- **Add more material classes** (glass, cardboard, e-waste) — each needs annotated
  data and a physical bin slot, which ties into the sorting-mechanism upgrade.
- **Swap the 16×2 LCD for a small touchscreen** to show the camera feed, per-class
  confidence, and history at once, and to move confirm/reject on-screen.
- **Build a more durable enclosure** from laser-cut acrylic or 3D-printed panels
  for better mechanical stability, especially under the chute.
- **Protect the sensor interface and re-test fusion.** Isolating the sensor's
  signal path with an optocoupler, a logic-level shifter, and a flyback diode
  would keep battery-line spikes off the microcontroller logic, so camera-and-
  sensor fusion can be retried and recover many of the metal items the camera
  misses.
