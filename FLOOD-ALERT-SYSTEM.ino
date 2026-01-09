#include <WiFi.h>
#include <HTTPClient.h>

// --- REQUIRED FOR TINY_GSM LIBRARY ---
// Define the specific modem being used (SIM900A)
#define TINY_GSM_MODEM_SIM900 // Correct for your module
#include <TinyGsmClient.h>
// -------------------------------------
// SIM900A Definitions: Connect module to Hardware Serial 2 (Pins 26/27)
#define GSM_SERIAL Serial2
// --- PINS FOR LEFT SIDE OF NODEMCU (26/27) ---
// ** SWAPPED PIN ASSIGNMENTS TO CORRECT FOR POTENTIAL WIRING ERROR (TX <-> RX) **
HardwareSerial GSM(2);
const int GSM_TX = 27; 
const int GSM_RX = 26;
// --------------------------------------------------

// Target phone numbers for SMS alerts and HIGH CALL alert (first number only)
// !!! IMPORTANT: REPLACE ALL PLACEHOLDERS WITH YOUR RECIPIENT NUMBERS (include country code) !!!
const char* SMS_TARGET_NUMBERS[] = {
    "+639564324153",  // 1. Primary number (THIS NUMBER WILL BE CALLED ON HIGH ALERT)
    "+639310165386",  // 2. Placeholder - REPLACE ME (SMS only)
    "+639456114984",  // 3. Placeholder - REPLACE ME (SMS only)
    "+63",  // 4. Placeholder - REPLACE ME (SMS only)
    "+"   // 5. Placeholder - REPLACE ME (SMS only)
};
const int SMS_TARGET_COUNT = 5; // Total number of recipients

// Initialize GSM modem
TinyGsm modem(GSM_SERIAL);

// Pins (ESP32 ADC capable)
const int lowSensor  = 34;  // Low-level sensor
const int highSensor = 35;  // High-level sensor

// WiFi credentials
const char* ssid     = "POCO X7";
const char* password = "ahhdaddy";

// Webhook URLs (Make.com - General/Email Alerts - uses WiFi)
const char* LOW_WEBHOOK  = "https://hook.eu2.make.com/uiym49vlq18fxucdafdk8b1c55fhqujg";
const char* HIGH_WEBHOOK = "https://hook.eu2.make.com/aa5evyzwsa2xch8bcogybvb6dtx33s6g";

// Thresholds (adjust to your calibration)
const int LOW_THRESHOLD  = 800;
const int HIGH_THRESHOLD = 800;

// Cooldowns
unsigned long lastLowEmail  = 0;
unsigned long lastHighEmail = 0;
const unsigned long emailCooldown = 10UL * 60UL * 1000UL; // 10 minutes
unsigned long cooldownEnd = 0; // strict 20s cooldown

// State
bool lowWet  = false;
bool highWet = false;

// Debug log timing
unsigned long lastDebug = 0;

// -------------------------------------------------
// Generic function to send a POST request with a custom JSON payload (for email/general alerts)
// -------------------------------------------------
void postJsonWebhook(const char* url, const char* label, const String& payload) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("[%s] WiFi not connected, cannot send webhook.\n", label);
    return;
  }

  HTTPClient http;
  if (http.begin(url)) {
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(payload);

    if (httpCode > 0) {
      Serial.printf("[%s] Webhook sent, code: %d\n", label, httpCode);
      String resp = http.getString();
      Serial.printf("[%s] Response: %s\n", label, resp.c_str());
    } else {
      Serial.printf("[%s] Webhook failed: %s\n", label, http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
     Serial.printf("[%s] Unable to connect to URL: %s\n", label, url);
  }
}

// -------------------------------------------------
// Send SMS alert via SIM900A module to all target numbers
// -------------------------------------------------
void sendSmsAlert(const char* message, const char* level) {
    Serial.printf("[%s SMS] Attempting to send SMS to %d recipients...\n", level, SMS_TARGET_COUNT);

    // Only attempt SMS if the modem was successfully initialized and connected to the network
    if (modem.isNetworkConnected()) {
      bool success = true;
      for (int i = 0; i < SMS_TARGET_COUNT; i++) {
        const char* recipient = SMS_TARGET_NUMBERS[i];
        Serial.printf("[%s SMS] Sending to recipient %d (%s)... ", level, i + 1, recipient);
        
        if (modem.sendSMS(recipient, message)) {
            Serial.println("SUCCESS");
        } else {
            Serial.println("FAILED");
            success = false;
            // Introduce a short delay on failure to give the modem time to recover
            delay(500); 
        }
      }
      if (success) {
        Serial.printf("[%s SMS] All SMS messages sent successfully.\n", level);
      } else {
        Serial.printf("[%s SMS] One or more SMS messages failed to send.\n", level);
      }
    } else {
       Serial.printf("[%s SMS] SMS failed: Modem is not connected to the network. Skipping all sends.\n", level);
    }
}


// -------------------------------------------------
// Make a voice call to the primary target number using RAW AT COMMANDS
// This function sends ATD<number>; to dial and ATH to hang up.
// -------------------------------------------------
// -------------------------------------------------
// Make a voice call to the primary target number using RAW AT COMMANDS
// -------------------------------------------------
void makeCallAlert() {
  const char* targetNumber = SMS_TARGET_NUMBERS[0];
  Serial.printf("[HIGH CALL] Attempting to call: %s\n", targetNumber);

  // Build ATD command
  String command = "ATD" + String(targetNumber) + ";";
  Serial.printf("[HIGH CALL] Sending command: %s\n", command.c_str());

  // Send ATD command
  GSM.println(command);

  // Wait for response (e.g., OK, RING, CONNECT)
  unsigned long startTime = millis();
  bool callStarted = false;

  while (millis() - startTime < 10000) {  // 10s timeout
    if (GSM.available()) {
      String response = GSM.readStringUntil('\n');
      response.trim();
      Serial.println("[HIGH CALL] GSM Response: " + response);

      if (response.indexOf("OK") >= 0 || response.indexOf("CONNECT") >= 0 || response.indexOf("RING") >= 0) {
        callStarted = true;
        break;
      }
    }
  }

  if (callStarted) {
    Serial.println("[HIGH CALL] Call initiated successfully.");

    delay(10000);  // Let it ring for 10s

    // Hang up
    Serial.println("[HIGH CALL] Hanging up (ATH)...");
    GSM.println("ATH");

    startTime = millis();
    bool hangupConfirmed = false;

    while (millis() - startTime < 5000) {  // 5s timeout
      if (GSM.available()) {
        String response = GSM.readStringUntil('\n');
        response.trim();
        Serial.println("[HIGH CALL] GSM Response: " + response);

        if (response.indexOf("OK") >= 0) {
          hangupConfirmed = true;
          break;
        }
      }
    }

    if (hangupConfirmed) {
      Serial.println("[HIGH CALL] Call finished (ATH OK).");
    } else {
      Serial.println("[HIGH CALL] WARNING: No OK from ATH, call may still be active.");
    }
  } else {
    Serial.println("[HIGH CALL] Failed to initiate call (no valid response).");
  }
}
// -------------------------------------------------
// Wrapper function to send both Email/General (WiFi) and SMS/Call (SIM900A) alerts
// -------------------------------------------------
void handleDualAlerts(const char* level) {
  // Low Flood Alert SMS
  String lowFloodMsg =
    "⚠️ LOW FLOOD ALERT ⚠️\n"
    "Water detected at low level.\n"
    "Please monitor the situation.\n\n"
    "An email with details has been sent.";

  // Rising Flood Alert SMS
  String risingFloodMsg =
    "🚨 RISING FLOOD ALERT 🚨\n"
    "Water levels are increasing!\n"
    "Take precautionary measures immediately.\n\n"
    "An email with full details has been sent.";

    if (strcmp(level, "LOW") == 0) {
        // 1. Send Email/General Alert (via Wi-Fi Webhook)
        String emailPayload = String("{\"alert\":\"LOW\"}");
        postJsonWebhook(LOW_WEBHOOK, "Email LOW", emailPayload);

        // 2. Send SMS Alert (via SIM900A)
        sendSmsAlert(lowFloodMsg.c_str(), "LOW");

        // NOTE: No call feature for LOW alert
    } else if (strcmp(level, "HIGH") == 0) {
        // 1. Send Email/General Alert (via Wi-Fi Webhook)
        String emailPayload = String("{\"alert\":\"HIGH\"}");
        postJsonWebhook(HIGH_WEBHOOK, "Email HIGH", emailPayload);

        // 2. Send SMS Alert (via SIM900A)
        sendSmsAlert(risingFloodMsg.c_str(), "HIGH");

        // 3. Make Voice Call Alert (via SIM900A)
        makeCallAlert(); 
    }
}


// -------------------------------------------------
// Check if sensor stays wet >= duration ms inside a window
// -------------------------------------------------
bool checkWetForDuration(int pin, int threshold, unsigned long duration, unsigned long window) {
  unsigned long start = millis();
  unsigned long wetTime = 0;
  bool isWetPrev = false;
  unsigned long wetStart = 0;

  while (millis() - start < window) {
    int val = analogRead(pin);
    bool isWet = (val > threshold);

    if (isWet && !isWetPrev) {
      wetStart = millis(); // start wet period
    }
    if (!isWet && isWetPrev) {
      wetTime += millis() - wetStart; // add wet duration
    }

    isWetPrev = isWet;

    if (wetTime >= duration) return true;

    delay(200);
  }

  // Final check if still wet at end
  if (isWetPrev) wetTime += millis() - wetStart;

  return (wetTime >= duration);
}

// -------------------------------------------------
void setup() {
  Serial.begin(115200); // Using 115200 for faster debug logs
  GSM.begin(9600, SERIAL_8N1, GSM_RX, GSM_TX); 
  delay(1000);
  

  // 1. Initialize GSM module (SIM900A)

  // We will try 115200 first, then fall back to 9600
  long baudRates[] = {9600, 115200};
  bool modemOk = false;
  
  Serial.println("Initializing modem (SIM900A) - Trying multiple baud rates...");

  for (int i = 0; i < 2 && !modemOk; i++) {
    long currentBaud = baudRates[i];
    
    // Close and re-open Serial2 with the new baud rate
    if (i > 0) GSM_SERIAL.end();
    GSM_SERIAL.begin(currentBaud, SERIAL_8N1, GSM_RX, GSM_TX);
    
    Serial.printf("Attempting modem init at %ld baud...\n", currentBaud);

    // Try modem.init() up to 5 times at this baud rate
    for (int retry = 0; retry < 5; retry++) {
      if (modem.init()) {
        modemOk = true;
        Serial.printf("Modem initialized successfully! Baud Rate: %ld. Operator: %s\n", currentBaud, modem.getOperator().c_str());
        break; // Exit retry loop
      }
      Serial.printf("...Failed init attempt %d at %ld baud. Waiting 1 second.\n", retry + 1, currentBaud);
      delay(1000);
    }
  }

  if (!modemOk) {
    Serial.println("FATAL: Failed to initialize modem at both 115200 and 9600 baud. SMS alerts will not work.");
  }


  // 2. Initialize WiFi (for email webhook)
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
}

// -------------------------------------------------
void loop() {
  unsigned long now = millis();

  // Strict 20s cooldown
  if (now < cooldownEnd) {
    if (now - lastDebug >= 5000) {
      Serial.println("In 20s cooldown, ignoring sensors...");
      lastDebug = now;
    }
    return;
  }

  // Read sensors
  int lowVal  = analogRead(lowSensor);
  int highVal = analogRead(highSensor);
  bool lowDetected  = (lowVal  > LOW_THRESHOLD);
  bool highDetected = (highVal > HIGH_THRESHOLD);

  // Debug every 2s
  if (now - lastDebug >= 2000) {
    Serial.printf("LOW:%d (%s) | HIGH:%d (%s)\n",
                  lowVal,  lowDetected  ? "WET" : "DRY",
                  highVal, highDetected ? "WET" : "DRY");
    lastDebug = now;
  }

  // HIGH alert (priority if both wet)
  if (lowDetected && highDetected) {
    if ((now - lastHighEmail >= emailCooldown) || !highWet) {
      if (checkWetForDuration(highSensor, HIGH_THRESHOLD, 4000, 10000)) {
        Serial.println("HIGH ALERT TRIGGERED! Sending dual alerts...");
        // Send BOTH email/general (WiFi), SMS, and CALL alerts
        handleDualAlerts("HIGH");

        lastHighEmail = now;
        cooldownEnd = now + 20000; // strict 20s
        highWet = true;
        lowWet = true;
      }
    }
  }
  // LOW only
  else if (lowDetected) {
    if ((now - lastLowEmail >= emailCooldown) || !lowWet) {
      if (checkWetForDuration(lowSensor, LOW_THRESHOLD, 4000, 10000)) {
        Serial.println("LOW ALERT TRIGGERED! Sending dual alerts...");
        // Send BOTH email/general (WiFi) and SMS alerts
        handleDualAlerts("LOW");

        lastLowEmail = now;
        cooldownEnd = now + 20000; // strict 20s
        lowWet = true;
        highWet = false;
      }
    }
  }
  // Dry state
  else {
    lowWet = false;
    highWet = false;
  }
}
