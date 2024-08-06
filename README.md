# PeatPulse
An Enviromental Monitoring For Peat Bogs

This project formed part of a group project looking to solve a unique problem:

- How can enviromental information about peat bogs be recorded over a long period of time?

## Challanges
1. Peat Bogs are remote - No Mobile Signal
2. Loong intervals between service - Reliable and Long Battery
3. Must be non intrusive to Wildlife.

**This Repo Contains the Code for this project**

## Particular Highlights
1. Mesh Networking.
2. Local ESP32 Webserver to retrieve data.
3. Sensor Fault Detection.

## Mesh Netowrking With Mesh Reforming Capability
<p align="center">
<img width="926" alt="Screenshot 2024-08-06 at 17 19 33" src="https://github.com/user-attachments/assets/8e0b5736-a817-4b2a-9a7e-3f7c6687bc7b">
</p>
<p align="center">
This image shows how the painless mesh network implemented in this project works at a high level, allowing for thousands of devices to be connected to a singular main node for data collection and storage.
</p>


## Local Webserver With Download, Upload and Delete Capability
<p align="center">
<img width="1178" alt="Screenshot 2024-08-06 at 17 20 10" src="https://github.com/user-attachments/assets/98e61d38-c537-4dc1-9ce2-d52b46b44292">
</p>
<p align="center">
This image shows the local webserver accessable from the main ESP32 node. This node would store all the data from the sensor nodes and could be accessed via Wi-Fi to download any collected data.
</p>
