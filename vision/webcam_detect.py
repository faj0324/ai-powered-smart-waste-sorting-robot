from ultralytics import YOLO
from pathlib import Path
import cv2

MODEL_PATH = Path(__file__).resolve().parent.parent / "models" / "yolov8_best.pt"
model = YOLO(MODEL_PATH)


cap = cv2.VideoCapture(0)  # 0 = webcam

while True:
    ret, frame = cap.read()
    if not ret:
        break

    results = model(frame, conf=0.4)
    annotated = results[0].plot()

    cv2.imshow("Waste Detection", annotated)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()
