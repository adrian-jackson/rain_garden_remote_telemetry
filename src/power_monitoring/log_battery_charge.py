import serial
import csv
import time
from pathlib import Path

arduino_port = 'COM5'  #UPDATE!
baud_rate = 4800 #UPDATE!
timeout = 60 #seconds
programStartTime = time.time()
hrsToRun = 12
directory = Path('data') / 'power_monitoring'
filename = directory / f'battery_voltage_log_{time.time()}.csv'

with open(filename, mode='w', newline='') as csv_file:
    csv_writer = csv.writer(csv_file)
    csv_writer.writerow(['Timestamp', 'Voltage'])
    while time.time() < programStartTime + hrsToRun * 60 * 60:
        try:
            # Set up the serial connection
            with serial.Serial(arduino_port, baud_rate, timeout=timeout) as ser:
                time.sleep(2)  # Wait for the connection to establish
                while True:
                    if ser.in_waiting > 0:  # Check if there is data to read
                        curr_voltage = ser.readline().decode('utf-8').rstrip()  # Read a line of data
                        timestamp = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime())
                        print(timestamp, curr_voltage)
                        csv_writer.writerow([timestamp, curr_voltage])
                        csv_file.flush()
                        time.sleep(10)
        except (serial.SerialException, ValueError) as e:
            print(f"Error: {e}")