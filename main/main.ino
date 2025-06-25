// Pin Definitions
const uint8_t HALL_A = 2;    // Hall Sensor A
const uint8_t HALL_B = 4;    // Hall Sensor B
const uint8_t HALL_C = 7;    // Hall Sensor C
const uint8_t POT     = A0;  // Potentiometer for speed control

// PWM output pins to MOSFET gates
const uint8_t PHASE_A = 11;  // OC2A
const uint8_t PHASE_B = 10;  // OC2B
const uint8_t PHASE_C = 9;   // OC1A

//  Variables
volatile uint8_t duty = 0;
volatile uint8_t lastState = 0;
volatile uint8_t currentState = 0;
String detectedDir = "";
String inputDir = "CW";  // Default direction

//  Setup 
void setup() {
  // Setup Hall sensors as inputs with pull-up
  pinMode(HALL_A, INPUT_PULLUP);
  pinMode(HALL_B, INPUT_PULLUP);
  pinMode(HALL_C, INPUT_PULLUP);

  // PWM Timer2 setup for pins 11 & 10 (PHASE_A & PHASE_B)
  TCCR2A = (1 << WGM21) | (1 << WGM20); // Fast PWM
  TCCR2B = (1 << CS22);                 // Prescaler 64

  // PWM Timer1 setup for pin 9 (PHASE_C)
  TCCR1A = (1 << WGM10);
  TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10); // Fast PWM, prescaler 64

  // Enable pin change interrupts for D2–D7
  PCICR |= (1 << PCIE2); // Enable interrupt group
  PCMSK2 |= (1 << PCINT18) | (1 << PCINT20) | (1 << PCINT23); // D2, D4, D7

  Serial.begin(9600);
  setDuty(0); // Start with 0% duty
}

// Main Loop
void loop() {
  // Speed control via potentiometer
  uint16_t potValue = analogRead(POT);
  uint8_t newDuty = map(potValue, 0, 1023, 0, 255);
  setDuty(newDuty);

  // Direction input from serial
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "CW" || cmd == "CCW") {
      inputDir = cmd;
      Serial.print("Desired direction set to: "); Serial.println(inputDir);
    }
  }

  // Print real-time status
  Serial.print("Rotor Pos: "); Serial.print(currentState);
  Serial.print(" | Actual: "); Serial.print(detectedDir);
  Serial.print(" | Desired: "); Serial.print(inputDir);

  if ((inputDir == "CW" && detectedDir != "Clockwise") ||
      (inputDir == "CCW" && detectedDir != "Counter-Clockwise")) {
    Serial.print(" [Mismatch]");
  } else {
    Serial.print(" [OK]");
  }

  Serial.print(" | PWM Duty: "); Serial.println(duty);
  delay(100);
}

// Set PWM Duty 
void setDuty(uint8_t d) {
  duty = d;
  OCR2A = duty; // Phase A
  OCR2B = duty; // Phase B
  OCR1A = duty; // Phase C
}

// -------------------- Enable One Phase Only --------------------
void enablePhase(uint8_t pin) {
  // Turn off all phases
  TCCR2A &= ~((1 << COM2A1) | (1 << COM2B1));
  TCCR1A &= ~(1 << COM1A1);

  // Turn on only the selected pin
  if (pin == PHASE_A) TCCR2A |= (1 << COM2A1);
  else if (pin == PHASE_B) TCCR2A |= (1 << COM2B1);
  else if (pin == PHASE_C) TCCR1A |= (1 << COM1A1);
}

// -------------------- Interrupt: Hall Sensor Change --------------------
ISR(PCINT2_vect) {
  uint8_t a = !digitalRead(HALL_A); // 3144 is active LOW
  uint8_t b = !digitalRead(HALL_B);
  uint8_t c = !digitalRead(HALL_C);

  // Determine current rotor state
  if (a) currentState = 1;
  else if (b) currentState = 2;
  else if (c) currentState = 3;

  // Detect actual direction
  if (lastState && lastState != currentState) {
    if ((lastState == 1 && currentState == 2) ||
        (lastState == 2 && currentState == 3) ||
        (lastState == 3 && currentState == 1)) {
      detectedDir = "Clockwise";
    } else {
      detectedDir = "Counter-Clockwise";
    }
  }
  lastState = currentState;

  // Apply PWM to correct phase based on inputDir
  if (inputDir == "CW") {
    if (currentState == 1) enablePhase(PHASE_A);
    else if (currentState == 2) enablePhase(PHASE_B);
    else if (currentState == 3) enablePhase(PHASE_C);
  } else if (inputDir == "CCW") {
    if (currentState == 1) enablePhase(PHASE_A);
    else if (currentState == 3) enablePhase(PHASE_B);
    else if (currentState == 2) enablePhase(PHASE_C);
  }
}
