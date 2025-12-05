#  BINS — Bin Intelligent Notification System

<img width="500" height="300" alt="bins-pres-title" src="https://github.com/user-attachments/assets/9374f027-2701-47e4-b117-a4cc02b8f8de" />

## Description 
BIN-INS is a smart-campus IoT system designed to monitor waste-bin fill levels in real time using an ultrasonic sensor, LEDs, and a microcontroller. 
The system transmits bin capacity data to a cloud backend and mobile app, enabling facility staff to optimize waste collection routes and reduce unnecessary pickups.

## Hardware Prototype

The prototype integrates:

- **TTGO LoRa32 / ESP32 board** for wireless communication  
- **HC-SR04 ultrasonic sensor** for distance measurement  
- **Status LEDs** (green/yellow/red) for local fill-level indication  
- **Voltage regulation / power module** for safe component operation  
- **Breadboard assembly** for testing and iteration  

**Prototype Image:**

<img width="500" height="300" alt="bins-sys-image" src="https://github.com/user-attachments/assets/ec967db9-7539-473e-9b63-e968ad69a40a" />

## System Features

- Real-time fill-level detection  
- LoRa / Wi-Fi communication to backend microservices  
- Configurable threshold alerts  
- Battery-efficient sensing cycles  
- Mobile front-end interface for rapid monitoring  

### Technical Stack

| Layer | Technologies |
|-------|-------------|
| **Firmware** | ESP32 (Arduino), HC-SR04, GPIO, power mgmt |
| **Backend Microservices** | Spring Boot 3, Eureka, Config Server, API Gateway |
| **Database** | PostgreSQL (Supabase) |
| **Mobile App** | React Native Expo, TanStack Query, Axios |
