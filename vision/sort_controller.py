import os
import cv2
import time
import serial
from pathlib import Path
from ultralytics import YOLO

# ===== SETTINGS =====
PORT = os.getenv("ECOSORT_PORT", "COM4")   # CHECK YOUR COM PORT
BAUD = 9600
MODEL_PATH = Path(__file__).resolve().parent.parent / "models" / "yolov8_best.pt"

# ===== SETUP =====
try:
    ser = serial.Serial(PORT, BAUD, timeout=1)
    time.sleep(2) 
    print(f"Connected to {PORT}")
except Exception as e:
    print(f"Serial Error: {e}")
    exit()

print("Loading AI Model...")
model = YOLO(MODEL_PATH)
cap = cv2.VideoCapture(0)

# ===== STATS COUNTERS =====
yes_count = 0
no_count = 0
total_count = 0

cooldown_until = 0

print("System Ready. Press 'Q' to quit.")

try:
    while True:
        ret, frame = cap.read()
        if not ret: break
        
        # Overlay stats on the camera feed
        cv2.putText(frame, f"Correct: {yes_count}", (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
        cv2.putText(frame, f"Errors: {no_count}", (10, 60), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)
        cv2.imshow("Sorting Robot", frame)
        
        if cv2.waitKey(1) & 0xFF == ord('q'): break

        if time.time() < cooldown_until: continue

        results = model.predict(source=frame, conf=0.55, verbose=False)
        det = results[0]

        if len(det.boxes) > 0:
            box = det.boxes[0]
            cls_name = det.names[int(box.cls[0])]
            
            print(f"Detected: {cls_name}")
            
            # 1. Hardware Sequence
            ser.write(f"LED YELLOW\n".encode())
            ser.write(f"LCD {cls_name}\n".encode()) 
            
            # 2. Wait for Feedback
            print("Waiting for feedback...")
            feedback = "TIMEOUT"
            
            start_wait = time.time()
            while time.time() - start_wait < 10:
                if ser.in_waiting:
                    try:
                        line = ser.readline().decode().strip()
                        if line in ["YES", "NO", "TIMEOUT"]:
                            feedback = line
                            break
                    except: pass
                time.sleep(0.1)
            
            print(f"Outcome: {feedback}")

            # 3. SORTING LOGIC
            if feedback == "YES" or feedback == "TIMEOUT":
                # --- CORRECT SORT ---
                print(">>> Sorting to CORRECT Bin (Right)")
                ser.write(f"LED GREEN\n".encode())
                ser.write(f"SERVO RIGHT\n".encode())
                
                # Update Stats
                yes_count += 1
            
            elif feedback == "NO":
                # --- TRASH SORT ---
                print(">>> Sorting to TRASH Bin (Left)")
                ser.write(f"LED RED\n".encode()) # Red usually means 'Stop/Wrong', here it means 'Trash'
                ser.write(f"SERVO LEFT\n".encode()) 
                
                # Update Stats
                no_count += 1

            # Wait for servo movement
            time.sleep(2)
            ser.write(f"SERVO NEUTRAL\n".encode())
            
            # 4. Reset
            total_count += 1
            accuracy = (yes_count / total_count) * 100 if total_count > 0 else 0
            print(f"--- Stats: Correct={yes_count} | Incorrect={no_count} | Accuracy={accuracy:.1f}% ---")
            
            ser.write(f"RESET\n".encode())
            cooldown_until = time.time() + 4

finally:
    ser.close()
    cap.release()
    cv2.destroyAllWindows()