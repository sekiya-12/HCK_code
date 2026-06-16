import pandas as pd
import serial
import csv
import time

PORT = "/dev/cu.usbmodemF412FA629DF02" #/dev/cu.usbmodemXXX
BAUD = 115200
OUTPUT = "Latency.csv"
header = ["Server", "Client", "Latency"]

S = serial.Serial(PORT, BAUD, timeout=1)
time.sleep(1)
S.reset_input_buffer()

with open(OUTPUT, "w", newline='') as file:
    writer = csv.writer(file)
    writer.writerow(header)

    try:
        while True:
            line = S.readline().decode("utf-8", errors="ignore").strip()
            if not line:
                continue
            row = line.split(",")
            writer.writerow(row)
            file.flush()
    except KeyboardInterrupt:
        print(f"シリアル出力の読み取りを終了し，データを{OUTPUT}に出力しました．")
    finally:
        S.close()
time.sleep(1)

df = pd.read_csv(OUTPUT)
print(f"平均レイテンシ：{df["Latency"].mean()}ms")