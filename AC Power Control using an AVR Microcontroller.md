**A Zero-Crossing Detection Approach**

Author: me

Date: (Original creation date)

---

### **1. Abstract**

This document describes a method for the precise control of AC power using an Atmel AVR microcontroller. While two primary methods exist for AC control—Phase Control and Zero-Crossing Control—this project implements the **Zero-Crossing Control** technique. This approach is used to build a spot welder, where precise, timed bursts of high current are required. The zero-crossing method is chosen for its relative simplicity and lower generation of electromagnetic interference (EMI) compared to phase control.

---

### **2. Bill of Materials (BOM)**

| Component | Reference Designator | Quantity |
| --- | --- | --- |
| Microcontroller | ATmega16 | 1 |
| TRIAC | BTA416 | 1 |
| Power Supply Module | GMS0405* | 1 |
| Potentiometer | 5mm Potentiometer | 1 |
| LCD Display | Character LCD 16x2 | 1 |
| Optocoupler | PC817 | 1 |
| Phototriac Driver | MOC3021 | 1 |
| Tactile Switch | YST-1103B | 4 |
| Electrolytic Capacitor | 470uF | 1 |
| Ceramic Capacitor | 20pF | 2 |
| Crystal Oscillator | 16MHz (SMD) | 1 |
| Mylar Capacitor | 100nF (104) | 2 |
| Resistor 1/4W | 10kΩ | 5 |
| Resistor 1/4W | 220Ω | 1 |
| Resistor 1/4W | 1kΩ | 1 |
| Resistor 1/4W | 10Ω | 1 |
| Resistor 1/4W | 91kΩ | 2 |
| Resistor 2W | 100Ω | 1 |

*Note: The GMS0405 part number appears to be for a power module. This could also refer to the HLK-PM01 seen in previous photos.*

---

### **3. Principle of Operation**

### **3.1. Zero-Crossing Detection**

The fundamental principle of this design is to precisely detect the **zero-crossing point** of the 60Hz AC sine wave. The zero-crossing point is the instant where the AC voltage crosses 0V, changing its polarity.

This detection is achieved safely using a **PC817 phototriac optocoupler**. The AC line voltage, limited by a resistor, drives the internal LED of the PC817. As the AC voltage oscillates, the LED turns on and off, causing the output phototransistor to generate a square wave that is synchronized with the AC line. A rising or falling edge of this signal is connected to the AVR's external interrupt pin (INT0), allowing the firmware to know exactly when each half-cycle begins.

### **3.2. Timed Triggering Algorithm**

To achieve precise control over the duration of the welding pulse, the firmware does not rely on simple, inaccurate software delays. Instead, it implements a more robust algorithm.

When a user sets a desired `triggertime` (e.g., 150ms), the firmware decomposes this value into two components:

1. **Full Cycles (`timecycle`):** The number of full AC half-cycles (8.333ms for 60Hz) that can fit within the total `triggertime`.
2. **Residual Time (`differencevalue`):** The remaining fractional time required to meet the exact `triggertime` after accounting for the full cycles.

The main TRIAC is then triggered for the calculated number of full cycles plus the residual time, resulting in a highly accurate power pulse. A configurable rest period (`spotrest`) is implemented between multi-pulse welding operations.

---

### **4. Firmware Implementation**

The following C code was implemented for the ATmega16 microcontroller.

```c
// --- Global Variables ---
int triggertime = 150;    // Desired trigger duration in milliseconds
int spotmult = 4;         // Number of welding spots per action
int spotrest = 200;       // Rest time between spots in milliseconds
int cyclevalue = 60;      // AC Line Frequency in Hz

// --- Calculated Timing Variables ---
int timecycle;            // Number of full half-cycles in triggertime
int differencevalue;      // Residual time for precise duration

/**
 * @brief Interrupt Service Routine for Zero-Crossing Detection.
 * This ISR is triggered by the PC817 on every zero-crossing event.
 */
ISR(INT0_vect)
{
    spot_action();
}

/**
 * @brief Pre-calculates the timing values based on global settings.
 */
void spotcal(void)
{
    // Calculate how many full half-cycles fit into the trigger time
    timecycle = triggertime / (1000 / cyclevalue);

    // Calculate the remaining time to compensate for integer division
    differencevalue = triggertime - timecycle * (1000 / cyclevalue);
}

/**
 * @brief Executes the spot welding sequence.
 * IMPORTANT DESIGN NOTE: Using blocking delays (delay_ms) inside an ISR context is
 * generally not recommended as it blocks all other system interrupts. For a more
 * robust design, hardware timers should be used for timing control.
 */
void spot_action(void)
{
    // Check if the trigger switch (S3) is pressed
    if(S3) {
        // Repeat the welding pulse for the configured number of spots
        for(int i=0; i<spotmult; i++){
            // Wait for almost one full half-cycle, slightly anticipating the next zero-cross
            delay_ms((1000 / cyclevalue) - 1);

            // --- Trigger ON ---
            PORTC = 0xff; // Fire the TRIAC

            // Hold the TRIAC on for the precisely calculated duration
            delay_ms(differencevalue);
            delay_ms(timecycle * (1000 / cyclevalue));

            // --- Trigger OFF ---
            PORTC = 0x00; // Turn off the TRIAC

            // Wait for the configured rest period before the next spot
            delay_ms(spotrest);
        }
    }
}

/**
 * @brief Main function: Initializes hardware and runs the main loop.
 */
int main(void)
{
    cli(); // Globally disable interrupts during setup

    spotcal(); // Pre-calculate timing values

    // --- Port Initialization ---
    DDRD = 0 << PD2;   // Set INT0 pin (PD2) as input
    PORTD = 1 << PD2;  // Enable pull-up resistor on PD2

    DDRC = 0xff;       // Set all PORTC pins as output
    PORTC = 0 << PC0;  // Ensure trigger output is initially off

    // --- Interrupt Initialization ---
    // Enable External Interrupt 0
    GICR |= 1 << INT0;
    // Configure INT0 to trigger on the rising edge
    MCUCR = (1 << ISC01) | (1 << ISC00);

    sei(); // Globally enable interrupts

    while(1) {
        // A small delay in the main loop can prevent potential race conditions
        // and improve stability in simple blocking designs.
        _delay_ms(1);
    }
}
```

---

### **5. Circuit Diagram**

![스크린샷 2025-06-28 오후 1 33 23](https://github.com/user-attachments/assets/626bdb3c-4778-49a1-b5d3-0abcf2d53e6c)


---

### **6. Component Roles**

- **PC817:** An optocoupler used to safely detect the AC line's zero-crossing point without any electrical connection between the high-voltage AC side and the low-voltage MCU side.
- **MOC3021:** A phototriac driver optocoupler. Its role is to provide galvanic isolation and safely deliver the trigger signal from the low-voltage ATmega16 to the gate of the high-power BTA416 TRIAC.
- **BTA416:** A high-power TRIAC. It functions as the main electronic switch, controlling the flow of the high AC current to the welding transformer's primary coil.
