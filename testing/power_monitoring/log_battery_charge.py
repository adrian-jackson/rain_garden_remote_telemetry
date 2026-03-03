import serial
import csv
import time

arduino_port = 'COM'  #UPDATE!
baud_rate = 9600 #UPDATE!
timeout = 60 #seconds
programStartTime = time.time()
hrsToRun = 12
filename = f'battery_voltage_log_{time.time()}.csv'

with open(csv_file_name, mode='w', newline='') as csv_file:
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
                        csv_writer.writerow([timestamp, curr_voltage])
                        csv_file.flush()
                        time.sleep(10)
        except (serial.SerialException, ValueError) as e:
            print(f"Error: {e}")