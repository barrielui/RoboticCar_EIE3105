# AGVCar_EIE3105
A small robotic car development project for EIE3105.  

It was an embedded system based on ARM microcontroller.  The AGV was equiped with track sensors (an array of IR emitters and reciever) and WiFi broadcast reciever.

This is the code for the final demonstration

# Specfications
1. Two wheels are driven by independent motors through PWM signals.  To perform a neat straight line movement and turning, the AGV is tuned with PID.
2. Different cars and a ball is scanned and label by a camera to WiFi broadcast system.  To save bandwidth, the data is coded in Hexadecimal format.  Decode process is performed by in microcontroller.

# Tasks
In the specified area, we are required to perform the following tasks
1.	Push the ball at least twice before shooting
2.	Stop the ball in shooting area, then shoot it.
3.	Time is counted for extra marks if I successfully scored.

