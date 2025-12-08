#include <micro_ros_arduino.h>

#include <micro_ros_arduino.h>

/*
 * ESP32 FINGERPRINT ATTENDANCE SCANNER (v2.2)
 *
 * This code is the client for the Google Apps Script backend.
 * It is a "dumb" scanner: it captures a fingerprint, looks up the
 * mapped ID, and sends that ID to the server in a secure JSON payload.
 * The server handles all logic (CHECK_IN vs. CHECK_OUT).
 *
 * NEW in v2.2:
 * - Added ALL_OUT button feature at PIN 23
 * - Button press sends special ALL_OUT request to server
 * - Includes debounce logic to prevent multiple triggers
 */

#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>
#include <Preferences.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <time.h>

// =================================================================
// 🚨🚨🚨 1. CONFIGURATION (REQUIRED CHANGES) 🚨🚨🚨
// =================================================================

// Wi-Fi Credentials
const char* ssid = "Airtel_Zerotouch";
const char* password = "Airtel@123";

// Google Apps Script URL
const char* scriptUrl = "https://script.google.com/macros/s/AKfycbxe_2ZAKC8tFGWBvn4UjIhla6lKjWnaXGkECjwyQZULo2zBbK0bnUBgMFVIFPQogdkE9A/exec";

// Secret Key (MUST match Google Script)
const char* SECRET_KEY = "x2VQTpWYKz09xckRHJvoVKrnrmMA5VBw";

// 🆕 ALL_OUT Button Configuration
#define ALL_OUT_BUTTON_PIN 23
#define Buzz 13
#define BUTTON_DEBOUNCE_MS 2000  // 2 seconds to prevent accidental multiple presses
#define IndiLed 2
// Fingerprint Sensor Configuration
#define RX_PIN 16
#define TX_PIN 17
#define BAUD 57600
HardwareSerial mySerial(2);
Adafruit_Fingerprint finger(&mySerial);

// NTP Configuration
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800;  // IST (India) is UTC + 5h 30m
const int daylightOffset_sec = 0;

// System Limits
#define MAX_ATTENDANCE_USERS 30

// Global Storage Object
Preferences preferences;

// Global state variables
bool continuousVerifyMode = true;
unsigned long lastButtonPress = 0;  // 🆕 For button debounce


// =================================================================
//                 2. HELPER FUNCTIONS
// =================================================================

int waitForSerialInt() {
  while (!Serial.available()) { delay(100); }
  int num = Serial.parseInt();
  while (Serial.available()) Serial.read();
  return num;
}

String waitForSerialString() {
  while (!Serial.available()) { delay(100); }
  String str = Serial.readStringUntil('\n');
  str.trim();
  while (Serial.available()) Serial.read();
  return str;
}

void waitForSerialEnter() {
  Serial.println("Waiting for ENTER key press...");
  while (Serial.available()) { Serial.read(); }
  while (Serial.read() != '\n') { delay(10); }
  while (Serial.available()) { Serial.read(); }
}

void saveUserIdMapping(uint8_t fingerId, int studentId, const String& name) {
  preferences.begin("mapping", false);
  String id_key = "uid_" + String(fingerId);
  String name_key = "name_" + String(fingerId);
  preferences.putInt(id_key.c_str(), studentId);
  preferences.putString(name_key.c_str(), name);
  preferences.end();
  Serial.print("✅ Mapped Finger ID ");
  Serial.print(fingerId);
  Serial.print(" to Student ID ");
  Serial.print(studentId);
  Serial.print(" and Name: ");
  Serial.println(name);
}

int getStudentIdFromFingerId(uint8_t fingerId) {
  preferences.begin("mapping", true);
  String key = "uid_" + String(fingerId);
  int studentId = preferences.getInt(key.c_str(), 0);
  preferences.end();
  return studentId;
}

String getStudentNameFromFingerId(uint8_t fingerId) {
  preferences.begin("mapping", true);
  String key = "name_" + String(fingerId);
  String studentName = preferences.getString(key.c_str(), "UNKNOWN");
  preferences.end();
  return studentName;
}

void connectWiFi() {
  Serial.println("\nConnecting to WiFi...");
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi Connected. IP: " + WiFi.localIP().toString());
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  } else {
    Serial.println("\n❌ WiFi Connection Failed.");
  }
}

void ensureWiFiConnected() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }
  Serial.println("Wi-Fi connection lost. Reconnecting...");
  WiFi.disconnect();
  connectWiFi();
}


// =================================================================
//                 NETWORK SEND FUNCTION
// =================================================================

void sendAttendance(int studentId, const String& studentName) {
  ensureWiFiConnected();
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ Wi-Fi disconnected. Log not sent.");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  if (http.begin(client, scriptUrl)) {
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<256> doc;
    doc["secret"] = SECRET_KEY;
    doc["id"] = studentId;
    doc["name"] = studentName;

    String jsonPayload;
    serializeJson(doc, jsonPayload);

    Serial.print("Sending Payload: "); 
    Serial.println(jsonPayload);

    int httpCode = http.POST(jsonPayload);

    if (httpCode > 0) {
      String responsePayload = http.getString();
      Serial.print("HTTP Code: "); 
      Serial.println(httpCode);
      Serial.print("Response: "); 
      Serial.println(responsePayload);

      if (httpCode == 200) {
        StaticJsonDocument<128> responseDoc;
        DeserializationError err = deserializeJson(responseDoc, responsePayload);

        if (err) {
          Serial.println("❌ Failed to parse server response as JSON.");
        } else {
          const char* status = responseDoc["status"];
          if (strcmp(status, "success") == 0) {
            const char* action = responseDoc["action"];


            Serial.print("✅ Server confirmed: "); 
            Serial.println(action);
            Serial.println(strcmp(action,"CHECK_IN"));

            if(strcmp(action,"CHECK_IN") == 0){
              Serial.println("andar agaya");
              LoggedIN();
            }
            else if(strcmp(action,"CHECK_OUT") == 0 || strcmp(action,"ALL_OUT") == 0){
              LoggedIN();
              LoggedIN();
            }
          
          } else {
            const char* message = responseDoc["message"];
            Serial.print("❌ Server returned an error: "); 
            Serial.println(message);
            ErrorBeep();
          }
        }
      } else if (httpCode == HTTP_CODE_MOVED_PERMANENTLY || httpCode == HTTP_CODE_FOUND) {
        http.end();
        
        String currentUrl = scriptUrl;
        int redirectCount = 0;

        while (redirectCount < 3) {
          String newUrl = http.getLocation();
          Serial.print("🔁 Redirected to: "); 
          Serial.println(newUrl);

          if (newUrl.length() == 0) {
            Serial.println("❌ Redirect response had no Location header!");
            break;
          }

          currentUrl = newUrl;
          redirectCount++;

          Serial.print("➡️  Connecting to: "); 
          Serial.println(currentUrl);

          if (!http.begin(client, currentUrl)) {
            Serial.println("❌ Failed to initiate HTTP connection securely.");
            break;
          }
          
          http.addHeader("Content-Type", "application/json");

          String newJsonPayload;
          serializeJson(doc, newJsonPayload);

          Serial.print("📤 Sending Payload: "); 
          Serial.println(newJsonPayload);

          int newHttpCode = http.GET();
          Serial.print("📥 HTTP Code: "); 
          Serial.println(newHttpCode);
          
          if (newHttpCode == HTTP_CODE_OK) {
            String newResponsePayload = http.getString();
            Serial.print("Response: "); 
            Serial.println(newResponsePayload);

            StaticJsonDocument<128> responseDoc;
            DeserializationError err = deserializeJson(responseDoc, newResponsePayload);

            if (err) {
              Serial.println("❌ Failed to parse server response as JSON.");
            } else {
              const char* status = responseDoc["status"];
              if (strcmp(status, "success") == 0) {
                Serial.print("✅ Server confirmed: ");
                Serial.println((const char*)responseDoc["action"]);
                Serial.println(status);


                const char* action = responseDoc["action"];

                Serial.println(strcmp(action,"CHECK_IN"));
                Serial.println(strcmp(action,"CHECK_OUT"));
                Serial.println(strcmp(action,"ALL_OUT"));

                if(strcmp(action,"CHECK_IN") == 0){
                  Serial.println("andar agaya");
                  LoggedIN();
                }
                else if(strcmp(action,"CHECK_OUT") == 0 || strcmp(action,"ALL_OUT") == 0){
                  LoggedIN();
                  LoggedIN();
                }
              } else {
                Serial.print("❌ Server returned an error: ");
                Serial.println((const char*)responseDoc["message"]);
                ErrorBeep();
              }
            }
            break;

          } else if (newHttpCode == HTTP_CODE_FOUND || newHttpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            http.end();
            delay(200);
            continue;

          } else {
            Serial.print("❌ HTTP POST request failed, code: ");
            Serial.println(newHttpCode);
            Serial.println("   (Might be an SSL/time issue or invalid URL)");
            break;
          }
        }
        
        if (redirectCount >= 3) {
          Serial.println("⚠️ Too many redirects. Aborting.");
        }
      } else {
        Serial.println("❌ Server returned an HTTP error. Check deployment settings.");
      }

    } else {
      Serial.print("❌ HTTP POST request failed, code: ");
      Serial.println(httpCode);
      Serial.println("   (This is often an SSL/Time sync issue or invalid URL)");
    }

    http.end();
  } else {
    Serial.println("❌ Failed to initiate HTTP connection securely.");
  }
}

// =================================================================
//                      CORE LOGIC
// =================================================================

void printHelp() {
  Serial.println("\n--- 🆘 Command Help ---");
  Serial.println("CONTROLS:");
  Serial.println("  start   - ✅ Start continuous fingerprint verification mode.");
  Serial.println("  stop    - 🛑 Stop continuous mode and enter command mode.");
  Serial.println("  help    - 🙋‍♂️ Display this help menu.");
  Serial.println("  allout  - 🔴 Trigger ALL_OUT request manually.");
  Serial.println("\nCOMMANDS (Only work when stopped):");
  Serial.println("  e <id>  - Enroll a new fingerprint at a specific ID (e.g., e 5).");
  Serial.println("  d <id>  - Delete a fingerprint at a specific ID (e.g., d 5).");
  Serial.println("  v       - Perform a single verification scan.");
  Serial.println("  i       - Show sensor information and template count.");
  Serial.println("  all     - 📋 Display all stored users (Finger ID, Student ID, Name).");
  Serial.println("\nBUTTON:");
  Serial.println("  PIN 23  - 🔴 Physical button to trigger ALL_OUT request.");
  Serial.println("------------------------\n");
}

void displayAllUsers() {
  Serial.println("\n--- 📋 Stored User Data ---");
  int userCount = 0;
  for (uint8_t i = 0; i < MAX_ATTENDANCE_USERS; i++) {
    int studentId = getStudentIdFromFingerId(i);
    if (studentId > 0) {
      String name = getStudentNameFromFingerId(i);
      Serial.print("  Finger ID: ");
      Serial.print(i);
      Serial.print("\t | Student ID: ");
      Serial.print(studentId);
      Serial.print("\t | Name: ");
      Serial.println(name);
      userCount++;
    }
  }
  if (userCount == 0) {
    Serial.println("  No users are currently enrolled.");
  }
  Serial.print("Total Users: ");
  Serial.println(userCount);
  Serial.println("---------------------------\n");
}

void enrollFingerprint(uint8_t id) {
  if (id >= MAX_ATTENDANCE_USERS) {
    Serial.print("Enrollment ID must be between 0 and ");
    Serial.println(MAX_ATTENDANCE_USERS - 1);
    return;
  }

  if (continuousVerifyMode) {
    Serial.println("🛑 Cannot enroll while in continuous verification mode. Type 'stop' first.");
    return;
  }

  Serial.print("Enrolling Fingerprint ID #");
  Serial.println(id);

  Serial.println("Enter the associated Student/Employee ID (e.g., 1001) and press ENTER:");
  int studentId = waitForSerialInt();

  if (studentId <= 0) {
    Serial.println("❌ Invalid Student/Employee ID. Enrollment cancelled.");
    return;
  }

  Serial.println("Enter the Student/Employee Name (e.g., Alice Smith) and press ENTER:");
  String studentName = waitForSerialString();

  Serial.print("Attempting to enroll for Student ID: ");
  Serial.print(studentId);
  Serial.print(" / Name: ");
  Serial.println(studentName);

  Serial.println("Remove finger, then press ENTER when ready for Image 1.");
  waitForSerialEnter();

  Serial.println("Place finger on sensor (1/2)...");
  while (finger.getImage() != FINGERPRINT_OK) delay(200);
  if (finger.image2Tz(1) != FINGERPRINT_OK) {
    Serial.println("Failed image #1");
    return;
  }
  Serial.println("Image #1 converted.");

  Serial.println("Remove finger...");
  while (finger.getImage() != FINGERPRINT_NOFINGER) delay(100);

  Serial.println("\n✅ Finger removed. Press ENTER when ready for the second scan.");
  waitForSerialEnter();

  Serial.println("Place same finger again (2/2)...");
  while (finger.getImage() != FINGERPRINT_OK) delay(200);
  if (finger.image2Tz(2) != FINGERPRINT_OK) {
    Serial.println("Failed image #2");
    return;
  }

  Serial.println("Creating model...");
  if (finger.createModel() != FINGERPRINT_OK) {
    Serial.println("Model failed (Inconsistent scans).");
    return;
  }

  Serial.println("Storing model...");
  if (finger.storeModel(id) == FINGERPRINT_OK) {
    Serial.print("✅ Fingerprint Stored successfully at ID #");
    Serial.println(id);
    saveUserIdMapping(id, studentId, studentName);
  } else {
    Serial.println("❌ Failed to store model in sensor.");
  }
}

void runVerificationLogic(uint8_t fingerId) {
  if (fingerId != 0xFF) {
    int studentId = getStudentIdFromFingerId(fingerId);

    if (studentId > 0) {
      String studentName = getStudentNameFromFingerId(fingerId);

      Serial.print("\n------------------------------------");
      Serial.print("\nScan Detected for ID: ");
      Serial.print(studentId);
      Serial.print(" (");
      Serial.print(studentName);
      Serial.println(")");
      Serial.println("Sending to server for processing...");

      sendAttendance(studentId, studentName);

      Serial.println("------------------------------------");

      delay(3000);

    } else {
      Serial.print("❌ Match found, but no Student ID mapped to Finger ID: ");
      Serial.println(fingerId);
      delay(1000);
    }
  }
}

uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();

  if (p != FINGERPRINT_OK) {
    if (p == FINGERPRINT_PACKETRECIEVEERR) { 
      Serial.println("❌ Comm error. Check wires."); 
    }
    return 0xFF;
  }

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) { return 0xFF; }

  p = finger.fingerFastSearch();

  if (p != FINGERPRINT_OK) {
    return 0xFF;
  }

  Serial.print("Found Finger ID: ");
  Serial.print(finger.fingerID);
  Serial.print(" (Confidence: ");
  Serial.print(finger.confidence);
  Serial.println(")");
  return finger.fingerID;
}

void sensorInfo() {
  Serial.println("\n-- Sensor Parameters --");
  finger.getParameters();
  Serial.print("Capacity: ");
  Serial.println(finger.capacity);
  finger.getTemplateCount();
  Serial.print("Templates Stored: ");
  Serial.println(finger.templateCount);
  Serial.println("-----------------------\n");
}

void deleteFingerprint(uint8_t id) {
  uint8_t p = finger.deleteModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.print("✅ Fingerprint ID #");
    Serial.print(id);
    Serial.println(" deleted.");
    preferences.begin("mapping", false);
    preferences.remove(("uid_" + String(id)).c_str());
    preferences.remove(("name_" + String(id)).c_str());
    preferences.end();
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.print("❌ Fingerprint ID #");
    Serial.print(id);
    Serial.println(" is empty.");
  } else {
    Serial.print("❌ Failed to delete ID #");
    Serial.print(id);
    Serial.println(".");
  }
}

// =================================================================
//                      SETUP AND LOOP
// =================================================================

void setup() {
  pinMode(Buzz,OUTPUT);
  StartUP();
  Serial.begin(115200);
  while (!Serial) delay(100);

  mySerial.begin(BAUD, SERIAL_8N1, RX_PIN, TX_PIN);

  // 🆕 Configure ALL_OUT button
  pinMode(ALL_OUT_BUTTON_PIN, INPUT);
  pinMode(IndiLed,OUTPUT);
  Serial.println("🔴 ALL_OUT button configured at PIN 23 (active LOW)");

  Serial.println("\n*** Fingerprint Attendance System Initializing (V2.2) ***");

  connectWiFi();

  if (finger.verifyPassword()) {
    Serial.println("✅ Fingerprint sensor detected successfully.");
  } else {
    Serial.println("❌ Fingerprint sensor not found. Check wiring.");
    while (1) delay(1);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Synchronizing time (required for https)...");
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      while (!getLocalTime(&timeinfo, 5000)) {
        delay(100);
        Serial.print(".");
      }
    }
    Serial.println("\n✅ Time successfully synced.");
    char timeBuffer[32];
    strftime(timeBuffer, sizeof(timeBuffer), "%d/%m/%Y %H:%M:%S", &timeinfo);
    Serial.print("Current time: ");
    Serial.println(timeBuffer);
  } else {
    Serial.println("❌ No WiFi, cannot sync time. HTTPS requests will fail until connected.");
  }

  finger.getTemplateCount();
  Serial.print("Sensor contains ");
  Serial.print(finger.templateCount);
  Serial.println(" templates.");

  Serial.println("\n***************************************************");
  Serial.println("✅ System started in CONTINUOUS VERIFICATION mode.");
  Serial.println("   Type 'stop' to enter command mode or 'help' for info.");
  Serial.println("   🔴 Press button at PIN 23 for ALL_OUT");
  Serial.println("***************************************************");
}

void loop() {
  // 🆕 Check for ALL_OUT button press (active LOW with debounce)
  if (digitalRead(ALL_OUT_BUTTON_PIN) == LOW) {
    unsigned long currentTime = millis();
    
    // Check if enough time has passed since last button press
    if (currentTime - lastButtonPress >= BUTTON_DEBOUNCE_MS) {
      lastButtonPress = currentTime;
      sendAttendance(-1, "ALL_OUT");
      
      
      // Wait for button release
      while (digitalRead(ALL_OUT_BUTTON_PIN) == LOW) {
        delay(10);
      }
      delay(100);  // Additional debounce after release
    }
  }

  // Handle Serial commands
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();

    if (line.isEmpty()) return;

    if (line.equalsIgnoreCase("start")) {
      if (!continuousVerifyMode) {
        continuousVerifyMode = true;
        Serial.println("\n*************************************************");
        Serial.println("✅ CONTINUOUS VERIFICATION MODE STARTED. Scan finger now.");
        Serial.println("Type 'stop' and press ENTER to return to command mode.");
        Serial.println("*************************************************");
      } else {
        Serial.println("Already in continuous mode.");
      }
      while (Serial.available()) Serial.read();
      return;
    } else if (line.equalsIgnoreCase("stop")) {
      if (continuousVerifyMode) {
        continuousVerifyMode = false;
        Serial.println("\n*************************************************");
        Serial.println("🛑 CONTINUOUS VERIFICATION MODE STOPPED.");
        Serial.println("   Now in Command Mode. Type 'help' for options.");
        Serial.println("*************************************************");
      } else {
        Serial.println("Already stopped.");
      }
      return;
    } else if (line.equalsIgnoreCase("help")) {
      printHelp();
      return;
    } else if (line.equalsIgnoreCase("allout")) {
      // 🆕 Manual ALL_OUT trigger via serial command
      sendAttendance(-1, "ALL_OUT");
      return;
    }

    if (!continuousVerifyMode) {
      char command = line.charAt(0);
      int spaceIndex = line.indexOf(' ');
      uint8_t id = 0;
      if (spaceIndex > 0) {
        id = line.substring(spaceIndex + 1).toInt();
      }

      if (command == 'e') {
        enrollFingerprint(id);
      } else if (command == 'd') {
        deleteFingerprint(id);
      } else if (command == 'i') {
        sensorInfo();
      } else if (command == 'v') {
        Serial.println("Waiting for single verification scan...");
        runVerificationLogic(getFingerprintID());
      } else if (line.equalsIgnoreCase("all")) {
        displayAllUsers();
      } else {
        Serial.print("Unknown command. ");
        printHelp();
      }
    } else {
      Serial.println("🛑 System is in continuous mode. Type 'stop' to enter commands.");
    }
  }

  // Continuous mode fingerprint verification
  if (continuousVerifyMode) {
    ensureWiFiConnected();
    runVerificationLogic(getFingerprintID());
    delay(50);
  }
}
