# EcoSort — Automated Waste Sorting System

Final Year Project. A camera-based waste classifier driving a physical sorting
mechanism: a YOLO model identifies an item on a platform, and an Arduino rotates
the platform to the matching bin.

Classes: `METAL`, `PLASTIC`, `PAPER`, with `GENERAL` as the fallback bin for
low-confidence or unrecognised items.

## Repository layout

| Path | What it is |
| --- | --- |
| `vision/sort_controller.py` | Detection loop with the operator confirm/reject workflow and running accuracy stats. |
| `vision/webcam_detect.py` | Minimal webcam viewer with detection overlay. Useful for checking the model without any hardware attached. |
| `firmware/ecosort_arduino.ino` | Arduino sketch: platform servo, flap gate, 16x2 I2C LCD, status LEDs, and four ultrasonic sensors for bin fill level. |
| `models/` | Trained weights. `yolov8_best.pt` and `yolov11_best.pt`. |
| `results/` | Confusion matrices, PR curves, training curves, system architecture diagram. |

## Results, and what they actually mean

Two very different numbers matter here, and only reporting the first one would be
misleading.

**Validation set:** the YOLOv11 run reached **0.925 mAP**, and YOLOv8 reached
**0.921**. See the confusion matrices and PR curves in `results/`.

**Live camera test (34 physical objects, 0.60 confidence threshold):** overall
accuracy was **38.2%**. The system was *cautious* rather than wrong — 52.9% of
objects were routed to the uncertain bin and only 8.8% were actively
mis-sorted. Of the results it did accept, **81.3%** were correct.

The gap between 0.925 mAP and 38.2% live accuracy is the honest headline finding
of this project: validation performance on curated images did not survive
contact with real lighting, angles, and unfamiliar objects. Angle consistency was
the weak point — only 3 of 16 multi-view objects were classified stably across
every view.

The live figures are provisional: they were scored against visually-inferred
classes, not a verified ground-truth label set.

## Known issue: the two halves disagree on protocol

`vision/sort_controller.py` and `firmware/ecosort_arduino.ino` are from
**different iterations of the system and do not currently talk to each other.**

- The firmware expects a single fused line: `RESULT:<CLASS>:<CONF>`, e.g. `RESULT:METAL:88`.
- `sort_controller.py` instead sends discrete commands: `LED YELLOW`, `LCD <class>`, `SERVO RIGHT`.

The firmware comments also refer to receiving results from a Raspberry Pi, while
the Python script opens a Windows COM port. Reconciling these — settling on the
`RESULT:` protocol on both sides — is the next step before the system runs
end-to-end.

## Running the vision side

```bash
pip install -r requirements.txt
python vision/webcam_detect.py          # model only, no hardware needed
```

For the full loop with hardware attached, set your serial port and run the
controller (see the protocol caveat above first):

```bash
ECOSORT_PORT=COM4 python vision/sort_controller.py
```

## Hardware

Arduino with an MG996R positional servo (platform rotation), an SG90 (flap
gate), a 16x2 I2C LCD at `0x27`, red/yellow/green status LEDs, confirm and reject
buttons, and four HC-SR04 ultrasonic sensors — one per compartment — for bin
fill detection. Pin assignments are at the top of the sketch and need to match
your wiring.

Items at or above 70% confidence sort automatically; below that, the system opens
a confirmation window for the operator to accept or reject the call.
