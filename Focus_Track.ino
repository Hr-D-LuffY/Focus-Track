#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

// -------------------- PIN DEFINITIONS --------------------
#define TRIG_PIN     6
#define ECHO_PIN     7
#define BUZZER       3
#define PAUSE_BTN    2
#define RESET_BTN    4
#define GREEN_LED    8
#define RED_LED      9
#define IR_PIN       11

// -------------------- USER SYSTEM --------------------
#define NUM_USERS     4
#define DAILY_GOAL_MS 60000UL    // 1 Minute in milliseconds

struct UserData {
  unsigned long totalFocusTime;
  int  streak;
  int  score;
  bool goalHitToday;
};

const char* userNames[NUM_USERS] = { "HABIB", "FAHIM", "ARKO", "SADIA" };
UserData users[NUM_USERS];
int currentUser = 0;

// -------------------- OBJECTS --------------------
LiquidCrystal_I2C lcd(0x27, 16, 2);

// -------------------- STATE FLAGS --------------------
bool manualPause   = false;
bool leftSeat      = false;
bool gameOver      = false;
bool scrollMode    = false;
bool inLoginScreen = true;
bool showingStats  = false;

// -------------------- TIMING --------------------
unsigned long focusTime         = 0;
unsigned long lastSitTime       = 0;
unsigned long scrollTimer       = 0;
unsigned long pauseBtnTime      = 0;   // shared for both login & main screens
unsigned long resetBtnTime      = 0;   // shared for both login & main screens
unsigned long scoreDropTimer    = 0;
unsigned long displayCycleTimer = 0;

int score     = 100;
int distance  = 0;
int scrollPos = 0;

// -------------------- EEPROM --------------------
void saveUser(int idx) {
  EEPROM.put(idx * sizeof(UserData), users[idx]);
}

void loadAllUsers() {
  for (int i = 0; i < NUM_USERS; i++) {
    EEPROM.get(i * sizeof(UserData), users[i]);
    if (users[i].streak < 0 || users[i].streak > 999) users[i].streak = 0;
    if (users[i].score  < 0 || users[i].score  > 100) users[i].score  = 100;
    if (users[i].totalFocusTime > 999999999UL)         users[i].totalFocusTime = 0;
    users[i].goalHitToday = false;
  }
}

// -------------------- SETUP --------------------
void setup() {
  Serial.begin(9600);

  pinMode(TRIG_PIN,  OUTPUT);
  pinMode(ECHO_PIN,  INPUT);
  pinMode(IR_PIN,    INPUT);
  pinMode(BUZZER,    OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED,   OUTPUT);
  pinMode(PAUSE_BTN, INPUT_PULLUP);
  pinMode(RESET_BTN, INPUT_PULLUP);

  lcd.init();
  lcd.backlight();

  loadAllUsers();

  lcd.setCursor(3, 0);
  lcd.print("FOCUS-TRACK");
  lcd.setCursor(4, 1);
  lcd.print("Ready!");
  delay(1500);
  lcd.clear();

  showLoginScreen();
}

// -------------------- LOGIN --------------------
void showLoginScreen() {
  inLoginScreen = true;
  currentUser   = 0;
  // Initialize button timers HERE so first press always works
  pauseBtnTime  = millis();
  resetBtnTime  = millis();
  drawLoginScreen();
}

void drawLoginScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Select User:");
  lcd.setCursor(0, 1);
  lcd.print(">");
  lcd.print(userNames[currentUser]);
  lcd.print(" Str:");
  lcd.print(users[currentUser].streak);
  lcd.print("     ");
}

void handleLogin() {
  // PAUSE button = cycle through users
  if (digitalRead(PAUSE_BTN) == LOW) {
    if (millis() - pauseBtnTime > 300) {
      pauseBtnTime = millis();
      currentUser = (currentUser + 1) % NUM_USERS;
      drawLoginScreen();
    }
  }

  // RESET button = confirm selected user
  if (digitalRead(RESET_BTN) == LOW) {
    if (millis() - resetBtnTime > 300) {
      resetBtnTime = millis();
      confirmLogin();
    }
  }
}

void confirmLogin() {
  inLoginScreen = false;

  score = users[currentUser].score;
  if (score <= 0 || score > 100) score = 100;

  focusTime         = 0;
  scoreDropTimer    = millis();
  lastSitTime       = millis();
  displayCycleTimer = millis();

  // Reset button timers AFTER any blocking code
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Welcome!");
  lcd.setCursor(0, 1);
  lcd.print(userNames[currentUser]);
  delay(1200);
  lcd.clear();

  // Set button timers LAST, after delays, so debounce window is fresh
  pauseBtnTime = millis();
  resetBtnTime = millis();
}

// -------------------- MAIN LOOP --------------------
void loop() {
  if (inLoginScreen) {
    handleLogin();
    return;
  }

  handleButtons();

  if (gameOver) {
    showGameOver();
    return;
  }

  distance = getDistance();
  bool irDetected = (digitalRead(IR_PIN) == LOW);

  detectPresence(irDetected);

  if (!manualPause && leftSeat) {
    dropScore();
  }

  if (!leftSeat && !manualPause) {
    updateTimer();
    checkDailyGoal();
  }

  // Cycle display between time/score and streak/goal every 4 seconds
  if (millis() - displayCycleTimer >= 4000) {
    displayCycleTimer = millis();
    showingStats      = !showingStats;
    if (!manualPause && !leftSeat) lcd.clear();
  }

  if (manualPause || leftSeat) {
    if (!scrollMode) {
      lcd.clear();
      scrollMode = true;
      scrollPos  = 0;
    }
    showScrollDisplay();
  } else {
    if (scrollMode) {
      lcd.clear();
      scrollMode   = false;
      showingStats = false;
    }
    if (showingStats) {
      showStatsDisplay();
    } else {
      showNormalDisplay();
    }
  }

  sendBluetoothData();
  delay(100);   
}

// -------------------- DISTANCE --------------------
int getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long dur = pulseIn(ECHO_PIN, HIGH, 30000); // 30ms timeout prevents hangs
  return dur * 0.034 / 2;
}

// -------------------- PRESENCE --------------------
void detectPresence(bool irDetected) {
  if (distance > 100 && !irDetected) {
    if (!leftSeat) {
      leftSeat      = true;
      scrollPos     = 0;
      alertDanger();
    }
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED,   HIGH);
  } else {
    if (leftSeat) {
      leftSeat    = false;
      lastSitTime = millis();
      noAlert();
    }
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RED_LED,   LOW);
  }
}

// -------------------- TIMER --------------------
void updateTimer() {
  unsigned long now = millis();
  focusTime  += (now - lastSitTime);
  lastSitTime = now;
}

// -------------------- DAILY GOAL CHECK --------------------
void checkDailyGoal() {
  if (!users[currentUser].goalHitToday && focusTime >= DAILY_GOAL_MS) {
    users[currentUser].goalHitToday = true;
    users[currentUser].streak++;
    saveUser(currentUser);

    // Send Bluetooth notification
    for (int i = 0; i < 5; i++) {
      Serial.println("GOAL COMPLETED!");
      delay(50);
    }

    // Show celebration — non-blocking buzzer pattern
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("GOAL COMPLETE!");
    lcd.setCursor(0, 1);
    lcd.print("Streak:");
    lcd.print(users[currentUser].streak);
    lcd.print(" days!");

    for (int i = 0; i < 3; i++) {
      digitalWrite(GREEN_LED, HIGH);
      digitalWrite(BUZZER,    HIGH); delay(300);
      digitalWrite(GREEN_LED, LOW);
      digitalWrite(BUZZER,    LOW);  delay(300);
    }
    delay(1500);
    lcd.clear();

    // Refresh timers after all blocking delays
    pauseBtnTime  = millis();
    resetBtnTime  = millis();
    scoreDropTimer = millis();
    lastSitTime    = millis();
    displayCycleTimer = millis();
  }
}

// -------------------- SCORE DROP --------------------
void dropScore() {
  if (millis() - scoreDropTimer >= 3000) {
    scoreDropTimer = millis();
    shortBeep();

    if (score > 0) score--;

    if (score <= 0) {
      score    = 0;
      gameOver = true;
      users[currentUser].score = 0;
      saveUser(currentUser);
      triggerGameOver();
    }
  }
}

// -------------------- BUTTONS --------------------
void handleButtons() {
  unsigned long now = millis();

  // --- PAUSE button ---
  if (digitalRead(PAUSE_BTN) == LOW) {
    if (now - pauseBtnTime > 300) {
      pauseBtnTime = now;
      if (gameOver) return;

      manualPause = !manualPause;
      lcd.clear();
      scrollMode = false;

      if (manualPause) {
        noAlert();
        digitalWrite(GREEN_LED, LOW);
        digitalWrite(RED_LED,   LOW);
        users[currentUser].totalFocusTime += focusTime;
        users[currentUser].score           = score;
        saveUser(currentUser);
        lcd.setCursor(2, 0);
        lcd.print(">> PAUSED <<");
        lcd.setCursor(1, 1);
        lcd.print("No penalty! :)");
      } else {
        lastSitTime    = millis();
        scoreDropTimer = millis();
        leftSeat       = false;
        lcd.setCursor(1, 0);
        lcd.print(">> RESUMED! <<");
        lcd.setCursor(2, 1);
        lcd.print("Keep going!");
      }
      delay(800);
      lcd.clear();

      // Refresh timer after blocking delay so next press works immediately
      pauseBtnTime = millis();
      resetBtnTime = millis();
    }
  }

  // --- RESET button ---
  if (digitalRead(RESET_BTN) == LOW) {
    if (now - resetBtnTime > 300) {
      resetBtnTime = now;

      // Save current progress
      users[currentUser].totalFocusTime += focusTime;
      users[currentUser].score           = score;
      saveUser(currentUser);

      // Reset session state
      focusTime      = 0;
      score          = 100;
      manualPause    = false;
      leftSeat       = false;
      gameOver       = false;
      scrollMode     = false;
      scrollPos      = 0;
      showingStats   = false;
      noAlert();
      digitalWrite(GREEN_LED, LOW);
      digitalWrite(RED_LED,   LOW);

      lcd.clear();
      lcd.setCursor(2, 0);
      lcd.print("Reset Done!");
      lcd.setCursor(1, 1);
      lcd.print("Score -> 100%");
      delay(1000);
      lcd.clear();

      // Refresh timers after blocking delay
      scoreDropTimer    = millis();
      lastSitTime       = millis();
      displayCycleTimer = millis();
      pauseBtnTime      = millis();
      resetBtnTime      = millis();
    }
  }
}

// -------------------- ALERTS --------------------
void alertDanger() {
  // Short non-blocking-friendly beep pattern
  digitalWrite(BUZZER, HIGH); delay(200);
  digitalWrite(BUZZER, LOW);  delay(80);
  digitalWrite(BUZZER, HIGH); delay(200);
  digitalWrite(BUZZER, LOW);
}

void shortBeep() {
  digitalWrite(BUZZER, HIGH); delay(80);
  digitalWrite(BUZZER, LOW);
}

void noAlert() {
  digitalWrite(BUZZER, LOW);
}

// -------------------- GAME OVER --------------------
void triggerGameOver() {
  noAlert();
  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZER, HIGH); delay(400);
    digitalWrite(BUZZER, LOW);  delay(150);
  }
  digitalWrite(RED_LED,   HIGH);
  digitalWrite(GREEN_LED, LOW);
  lcd.clear();
  scrollMode = true;
  scrollPos  = 0;
}

void showGameOver() {
  String msg = "  SCORE ZERO! Sit & focus! Press RESET  ";
  if (millis() - scrollTimer > 350) {
    scrollTimer = millis();
    lcd.setCursor(0, 0);
    for (int i = 0; i < 16; i++) {
      lcd.print(msg[(scrollPos + i) % msg.length()]);
    }
    scrollPos++;
    if (scrollPos >= (int)msg.length()) scrollPos = 0;
  }
  lcd.setCursor(0, 1);
  lcd.print("Press RESET btn ");
  sendBluetoothData();
  delay(100);
}

// -------------------- NORMAL DISPLAY --------------------
void showNormalDisplay() {
  unsigned long mins = focusTime / 60000UL;
  unsigned long secs = (focusTime % 60000UL) / 1000UL;

  lcd.setCursor(0, 0);
  lcd.print("Time: ");
  if (mins < 10) lcd.print("0");
  lcd.print(mins);
  lcd.print(":");
  if (secs < 10) lcd.print("0");
  lcd.print(secs);
  lcd.print("      ");

  lcd.setCursor(0, 1);
  lcd.print("Score: ");
  lcd.print(score);
  lcd.print("%         ");
}

// -------------------- STATS DISPLAY --------------------
void showStatsDisplay() {
  unsigned long goalMins    = DAILY_GOAL_MS / 60000UL;
  unsigned long currentMins = focusTime / 60000UL;

  lcd.setCursor(0, 0);
  lcd.print("Goal:");
  lcd.print(currentMins);
  lcd.print("/");
  lcd.print(goalMins);
  lcd.print("min  ");

  lcd.setCursor(0, 1);
  lcd.print("Streak:");
  lcd.print(users[currentUser].streak);
  lcd.print(" days    ");
}

// -------------------- SCROLL DISPLAY --------------------
void showScrollDisplay() {
  String msg;
  if (leftSeat && !manualPause) {
    msg = "  LEFT SEAT - TIMER PAUSED   ";
  } else {
    msg = "  TIMER PAUSED - No Penalty  ";
  }

  if (millis() - scrollTimer > 350) {
    scrollTimer = millis();
    lcd.setCursor(0, 0);
    for (int i = 0; i < 16; i++) {
      lcd.print(msg[(scrollPos + i) % msg.length()]);
    }
    scrollPos++;
    if (scrollPos >= (int)msg.length()) scrollPos = 0;
  }

  lcd.setCursor(0, 1);
  lcd.print("Score: ");
  lcd.print(score);
  lcd.print("%         ");
}

// -------------------- BLUETOOTH --------------------
void sendBluetoothData() {
  Serial.println("-------------------");
  Serial.print("User: ");
  Serial.println(userNames[currentUser]);

  Serial.print("Status: ");
  if (gameOver)         Serial.println("GAME OVER");
  else if (manualPause) Serial.println("PAUSED");
  else if (leftSeat)    Serial.println("LEFT SEAT");
  else                  Serial.println("FOCUSING");

  Serial.print("Score: ");
  Serial.print(score);
  Serial.println("%");

  Serial.print("Streak: ");
  Serial.print(users[currentUser].streak);
  Serial.println(" days");

  Serial.print("Goal: ");
  Serial.println(users[currentUser].goalHitToday ? "YES" : "NO");
  Serial.println("-------------------");
}
