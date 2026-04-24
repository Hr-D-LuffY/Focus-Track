# 🎯 Focus Track
### A Microcontroller-Based Smart Study Productivity Tracking System

> **CSE360 — Computer Interface | Course Project**

Focus Track is an Arduino-based embedded system that helps students stay focused during study sessions. It uses an ultrasonic sensor and IR sensor to detect whether you're sitting at your desk, tracks your focus time, manages a score system, and rewards consistent study habits through streaks and daily goals — all displayed on a 16×2 LCD with Bluetooth data output.

---

## 📸 System Overview

```
┌─────────────────────────────────────────────────────┐
│                    FOCUS TRACK                      │
│                                                     │
│  [Ultrasonic]  [IR Sensor]  →  Arduino Uno          │
│       ↓             ↓              ↓                │
│   Presence Detection        LCD + LEDs + Buzzer     │
│                                    ↓                │
│                          Bluetooth Serial Output    │
└─────────────────────────────────────────────────────┘
```

---

## ✨ Features

- **Multi-User Login** — Supports 4 named user profiles (HABIB, FAHIM, ARKO, SADIA) selectable at startup
- **Presence Detection** — Dual sensor system (HC-SR04 ultrasonic + IR) to reliably detect if you've left your seat
- **Focus Timer** — Tracks total focused time; pauses automatically when you leave
- **Score System** — Starts at 100%, drops by 1% every 3 seconds when seat is left unpaused; Game Over at 0%
- **Daily Goal & Streaks** — Hit your daily focus goal to increment your streak counter
- **Manual Pause** — Pause the session without penalty (e.g., planned breaks)
- **Persistent Storage** — User scores, streaks, and total focus time saved to EEPROM (survives power-off)
- **Bluetooth Output** — Real-time status data streamed over Serial for app integration
- **Scrolling LCD Display** — Status messages scroll across the 16×2 LCD
- **Visual & Audio Alerts** — Green/Red LEDs and buzzer for seat-left warnings and goal completion

---

## 🔧 Hardware Requirements

| Component | Specification | Quantity |
|---|---|---|
| Microcontroller | Arduino Uno (or compatible) | 1 |
| Ultrasonic Sensor | HC-SR04 | 1 |
| IR Sensor | Generic digital IR module | 1 |
| LCD Display | 16×2 with I2C module (0x27) | 1 |
| Buzzer | Active buzzer 5V | 1 |
| LED | Green 5mm | 1 |
| LED | Red 5mm | 1 |
| Push Button | Momentary tactile switch | 2 |
| Resistors | 220Ω (for LEDs) | 2 |
| Bluetooth Module | HC-05 / HC-06 (optional) | 1 |
| Breadboard & Wires | — | — |

---

## 📌 Pin Configuration

| Pin | Component | Mode |
|---|---|---|
| `2` | PAUSE Button | INPUT_PULLUP |
| `3` | Buzzer | OUTPUT |
| `4` | RESET Button | INPUT_PULLUP |
| `6` | HC-SR04 TRIG | OUTPUT |
| `7` | HC-SR04 ECHO | INPUT |
| `8` | Green LED | OUTPUT |
| `9` | Red LED | OUTPUT |
| `11` | IR Sensor | INPUT |
| `A4` | LCD SDA (I2C) | I2C |
| `A5` | LCD SCL (I2C) | I2C |

---

## 🔌 Circuit Diagram

```
Arduino Uno
    │
    ├── Pin 6  ──────→ HC-SR04 TRIG
    ├── Pin 7  ←────── HC-SR04 ECHO
    ├── Pin 11 ←────── IR Sensor OUT
    │
    ├── Pin 3  ──────→ Buzzer (+)   → GND
    ├── Pin 8  ──────→ 220Ω ──→ Green LED → GND
    ├── Pin 9  ──────→ 220Ω ──→ Red LED   → GND
    │
    ├── Pin 2  ←────── PAUSE Button (other end → GND)
    ├── Pin 4  ←────── RESET Button (other end → GND)
    │
    ├── A4 (SDA) ────→ LCD I2C SDA
    ├── A5 (SCL) ────→ LCD I2C SCL
    │
    ├── 5V  ─────────→ HC-SR04 VCC, IR VCC, LCD VCC, Buzzer VCC
    └── GND ─────────→ All component GNDs
```

---

## 📚 Libraries Required

Install these via **Arduino IDE → Tools → Manage Libraries**:

| Library | Purpose |
|---|---|
| `LiquidCrystal_I2C` | I2C LCD control |
| `Wire` | I2C communication (built-in) |
| `EEPROM` | Persistent storage (built-in) |

---

## 🚀 Getting Started

**1. Clone the repository**
```bash
git clone https://github.com/Hr-D-LuffY/focus-track.git
cd focus-track
```

**2. Open in Arduino IDE**
```
File → Open → focus_track_fixed.ino
```

**3. Install Libraries**
```
Tools → Manage Libraries → Search "LiquidCrystal I2C" → Install
```

**4. Select Board & Port**
```
Tools → Board → Arduino Uno
Tools → Port → COMx (Windows) or /dev/ttyUSBx (Linux/Mac)
```

**5. Upload**
```
Click Upload (→) or press Ctrl+U
```

---

## 🎮 How to Use

### Login Screen
- Press **PAUSE** button to cycle through users
- Press **RESET** button to confirm and log in

### During a Session
| Situation | System Response |
|---|---|
| Sitting at desk | Green LED ON, timer running |
| Left seat (no pause) | Red LED ON, buzzer beeps, score drops |
| Daily goal reached | Celebration beeps, streak incremented |
| Score reaches 0% | Game Over screen, must reset |

### Button Controls
| Button | Action |
|---|---|
| **PAUSE** | Toggle manual pause (no score penalty) |
| **RESET** | Save progress and reset score to 100% |

### Display Modes
The LCD cycles every 4 seconds between:
- **Normal View** — `Time: MM:SS` / `Score: XX%`
- **Stats View** — `Goal: X/Ymin` / `Streak: Z days`

---

## 📊 Bluetooth / Serial Data Format

Data is sent over Serial (9600 baud) every loop cycle:

```
-------------------
User: HABIB
Status: FOCUSING
Score: 87%
Streak: 5 days
Goal: NO
-------------------
```

**Status values:** `FOCUSING` | `PAUSED` | `LEFT SEAT` | `GAME OVER`

Special event flag:
```
GOAL COMPLETED!
```

---

## 💾 EEPROM Storage

Each user profile occupies `sizeof(UserData)` bytes in EEPROM:

```cpp
struct UserData {
  unsigned long totalFocusTime;  // cumulative ms
  int  streak;                   // consecutive days goal was hit
  int  score;                    // last saved score
  bool goalHitToday;             // resets on power-on
};
```

Data persists across power cycles. `goalHitToday` resets to `false` on every boot (intentional — designed for daily use).

---

## 📁 Repository Structure

```
focus-track/
│
├── focus_track_fixed.ino   # Main Arduino sketch
├── README.md               # This file
├── .gitignore              # Arduino build artifacts
└── LICENSE                 # MIT License
```

---

## 🧠 System Flow

```
Power ON
   │
   ▼
Welcome Screen (1.5s)
   │
   ▼
Login Screen ──[PAUSE]──→ Cycle Users
   │ [RESET]
   ▼
Session Start
   │
   ├──[Presence Detected]──→ Timer Running → Check Daily Goal
   │                                              │
   │                                         [Goal Hit] → Streak++, Celebrate
   │
   ├──[Left Seat]──→ Score Drop every 3s → [Score = 0] → Game Over
   │
   ├──[PAUSE btn]──→ Manual Pause (no penalty) → [PAUSE btn] → Resume
   │
   └──[RESET btn]──→ Save Progress → Reset Score to 100%
```

---

## 👥 Team Members

| Name | Role |
|---|---|
| Habib | Arduino Programming |
| Fahim | Hardware Design & Circuit | 
| Arko | System Logic & Testing |
| Sadia | Documentation & Demo |


---

## 📖 Course Information

- **Course:** CSE360 — Computer Interface
- **Institution:** BRAC UNIVERSITY
- **Semester:** Spring 2026

---
