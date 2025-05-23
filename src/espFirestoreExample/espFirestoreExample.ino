/**
 * ABOUT:
 *
 * The example to perform batch get multiple Firestore documents.
 *
 * This example uses the ServiceAuth class for authentication.
 * See examples/App/AppInitialization for more authentication examples.
 *
 * The OAuth2.0 authentication or access token authorization is required for performing the batch get multiple Firestore documents.
 *
 * The complete usage guidelines, please read README.md or visit https://github.com/mobizt/FirebaseClient
 */
 #include <Arduino.h>
 #include <ArduinoJson.h>
 #include <FirebaseClient.h>
 #include <SPIFFS.h>
 #include <WiFiManager.h>
 #include "ExampleFunctions.h" // Provides the functions used in the examples.
 #include "secrets.h"          // Provides all the authentication
 #include <Keypad.h>
 #include <deque>

 #define ROW_NUM     4 // four rows
 #define COLUMN_NUM  4 // four columns
 #define LED_BUILTIN 1         // Using GPIO 1 for the LED lights
 #define LED_LOCK    7         // Using GPIO 7 for the built in LED (for Lock)
 
std::deque<String> pinQueue = {"1111", "2222", "3333", "4444"};

char keys[ROW_NUM][COLUMN_NUM] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte pin_rows[ROW_NUM]      = {8, 21, 20, 9}; // GPIO pins for rows
byte pin_column[COLUMN_NUM] = {3, 4, 5, 6};   // GPIO pins for columns

Keypad keypad = Keypad(makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM);

char inputBuffer[5]; // 4 characters + 1 for null terminator '\0'
int inputIndex = 0;

 void processData(AsyncResult &aResult);
 void batch_get_async(BatchGetDocumentOptions &options);
 void batch_write_await(Writes &writes);
 void handleJsonStream(String jsonString);
 void sendUnlockAcknowledgment();
 void sendLockAcknowledgement();
 String getIsLocked();
 bool createUserPins();
 char** getUserPins();
 size_t getUserPinsCount();
 void freeUserPins();
 
 // ServiceAuth is required for Google Cloud Functions functions.
 ServiceAuth sa_auth(FIREBASE_CLIENT_EMAIL, FIREBASE_PROJECT_ID, PRIVATE_KEY, 3000 /* expire period in seconds (<= 3600) */);
 
 FirebaseApp app;
 
 SSL_CLIENT ssl_client;
 
 // This uses built-in core WiFi/Ethernet for network connection.
 // See examples/App/NetworkInterfaces for more network examples.
 using AsyncClient = AsyncClientClass;
 AsyncClient aClient(ssl_client);
 
 Firestore::Documents Docs;
 
 AsyncResult firestoreResult;
 JsonDocument doc;
 
 bool taskCompleted = false;
 bool isWiFiConnected = false;
 bool sendAck = false;
 bool ackValue = true;  // true if door just unlocked, false otherwise
 bool jsonValid = false;
 bool unlockAck = false;
 bool lockAck = false;
 char** userPins = nullptr;
 size_t userPinCount = 0;
 
 void setup()
 {
     Serial.begin(115200);
     pinMode(LED_BUILTIN, OUTPUT);
     pinMode(LED_LOCK, OUTPUT);

     WiFiManager wfm;           // wifi manager object
     wfm.setDebugOutput(false); // suppressing debug info
     wfm.resetSettings();       // removes previous network settings (for testing use)
     WiFiManagerParameter custom_text_box("my_text", "Enter your string here", "default string", 50); // custom text box
     wfm.addParameter(&custom_text_box);  // custom parameter
     digitalWrite(LED_BUILTIN, HIGH);     // HIGH for not connected to wifi yet (first time setup)
     if (!wfm.autoConnect("SmartLock AP", "12345678")) {
        // Did not connect, print error message
        Serial.println("failed to connect and hit timeout");
 
        // Reset and try again
        ESP.restart();
        delay(1000);
     }

     // Connected!
     digitalWrite(LED_BUILTIN, LOW);  // LOW for connected to wifi
     Serial.println("WiFi connected");
     Serial.print("IP address: ");
     Serial.println(WiFi.localIP());
     isWiFiConnected = true;

     // Print out the custom text box value to serial monitor
     Serial.print("Custom text box entry: ");
     Serial.println(custom_text_box.getValue());
 
     /* No need, use wifi Provisioning instead now
     WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
 
     Serial.print("Connecting to Wi-Fi");
     while (WiFi.status() != WL_CONNECTED)
     {
         Serial.print(".");
         digitalWrite(LED_BUILTIN, HIGH);
         delay(300);
         digitalWrite(LED_BUILTIN, LOW);
         delay(300);
     }
     Serial.println();
     digitalWrite(LED_BUILTIN, LOW);
     Serial.print("Connected with IP: ");
     Serial.println(WiFi.localIP());
     Serial.println();
     isWiFiConnected = true; // <- Set here after WiFi is confirmed!
     */
 
     
 
     Firebase.printf("Firebase Client v%s\n", FIREBASE_CLIENT_VERSION);
     clearBuffer();
 
     set_ssl_client_insecure_and_buffer(ssl_client);
 
     app.setTime(get_ntp_time());
 
     Serial.println("Initializing app...");
     initializeApp(aClient, app, getAuth(sa_auth), auth_debug_print, "🔐 authTask");
 
     app.getApp<Firestore::Documents>(Docs);
 }
 
 unsigned long lastPollTime = 0; // Track the last time Firestore was queried
 const unsigned long pollInterval = 10000; // Poll every 10 seconds (adjust as needed)

//NEW SIMPLIFIED CODE
void loop() {
    app.loop(); // Maintain async Firebase tasks

    processKeypadInput();
    checkWiFiConnection();
    if (!isWiFiConnected) return; // Skip rest if no WiFi

    pollFirestorePeriodically();
    sendAcknowledgementIfNeeded();
}

//END SIMPLIFIED CODE
 
 void processData(AsyncResult &aResult)
 {
     // Exits when no result available when calling from the loop.
     if (!aResult.isResult())
         return;
 
     if (aResult.isEvent())
     {
         Firebase.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.eventLog().message().c_str(), aResult.eventLog().code());
     }
 
     if (aResult.isDebug())
     {
         Firebase.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());
     }
 
     if (aResult.isError())
     {
         Firebase.printf("Error task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());
     }
 
     if (aResult.available())
     {
        //  Firebase.printf("task: %s, payload: %s\n", aResult.uid().c_str(), aResult.c_str());
        //  String JsonString = aResult.c_str();
        //  handleJsonStream(JsonString); // deubug checker, also helps with deserilization of JsonDoc
         
        //  String isLockedVal = getIsLocked();
        //  Serial.println("isLocked value: " + getIsLocked());

        //  if (isLockedVal == "1") {
        //    // Unlock
        //    digitalWrite(LED_LOCK, HIGH);
        //    sendUnlockAcknowledgment();
        //  } else {
        //    // Lock
        //    digitalWrite(LED_LOCK, LOW);
        //  }

         if (aResult.uid() == "batchGetTask") {
            Serial.println("Processing batchGet result...");
            String JsonString = aResult.c_str();
            handleJsonStream(JsonString);
            String isLockedVal = getIsLocked();

            // Start of getting userPins and checking if it works
            if (createUserPins()) {
                char** pins = getUserPins();
                size_t count = getUserPinCount();

                Serial.println("User Pins:");
                for (size_t i = 0; i < count; ++i) {
                    Serial.println(pins[i]);  // Debug print each pin
                }


                // (Optional) Example usage: check if a certain pin is valid
                // String enteredPin = "1234";
                // bool valid = isPinValid(enteredPin); // If you write that helper

                // ✅ Free memory after you're done using userPins
                freeUserPins();
            } else {
                Serial.println("Failed to extract userPins.");
            }
            //

            Serial.println("isLocked value: " + isLockedVal);

            if (isLockedVal == "1") {
                digitalWrite(LED_LOCK, HIGH);
                sendAck = true;
                unlockAck = true;
                // sendUnlockAcknowledgment();
            } else {
                digitalWrite(LED_LOCK, LOW);
                sendAck = true;
                lockAck = true;
            }
        }
        else if (aResult.uid() == "batchWriteTask") {
            Serial.println("Processing batchWrite result...");
            // Maybe just log success/failure here; no further action needed?
        }
         
     }
 }

 void sendLockAcknowledgement() {
    /**
     * Sends to server false in the case the lock is (0)
    **/
    Serial.println("Sending unlock acknowledgment...");
    String documentPath = "users/FuduqA91EfdHhEA8JAQNJ3SwrRJ2";
    Document<Values::Value> updateDoc;
    updateDoc.setName(documentPath);
    updateDoc.add("unlockedAck", Values::Value(Values::BooleanValue(false)));
    DocumentMask mask("unlockedAck");
    Write write(mask, updateDoc, Precondition());
    Writes writes(write);
    batch_write_await(writes);
 }

 void sendUnlockAcknowledgment() {
    /**
     * Sends to server false in the case the lock is (1)
    **/
    Serial.println("Sending unlock acknowledgment...");
    String documentPath = "users/FuduqA91EfdHhEA8JAQNJ3SwrRJ2";
    Document<Values::Value> updateDoc;
    updateDoc.setName(documentPath);
    updateDoc.add("unlockedAck", Values::Value(Values::BooleanValue(true)));
    DocumentMask mask("unlockedAck");
    Write write(mask, updateDoc, Precondition());
    Writes writes(write);
    batch_write_await(writes);
}


 void handleJsonStream(String jsonString) {
    /**
     * This functions just prints the name and isLocked value from the Json string.
    */

    // make sure to clear the document in case it had previous data
    doc.clear();

    // Deserialize the JSON string
    DeserializationError error = deserializeJson(doc, jsonString);

    if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        jsonValid = false;
        return;
    }

    jsonValid = true;

    // Now you can access your fields easily
    const char* name = doc[0]["found"]["name"];
    const char* isLocked = doc[0]["found"]["fields"]["isLocked"]["integerValue"];
    bool unlockedAck = doc[0]["found"]["fields"]["unlockedAck"]["booleanValue"];
    const size_t pinCount = doc[0]["found"]["fields"]["userPins"]["arrayValue"]["values"].size();
    
    if (pinCount == 0) {
        Serial.println("No user PINs found.");
        return;
    }

    char** userPins = new char*[pinCount];

    for (size_t i = 0; i < pinCount; ++i) {
        const char* pin = doc[0]["found"]["fields"]["userPins"]["arrayValue"]["values"][i]["stringValue"];
        size_t len = strlen(pin) + 1;
        userPins[i] = new char[len];
        strncpy(userPins[i], pin, len - 1);
        userPins[i][len - 1] = '\0';
    }

    Serial.print("Name: ");
    Serial.println(name);

    Serial.print("isLocked: ");
    Serial.println(isLocked);

    Serial.print("unlockedAck");
    Serial.println(unlockedAck ? "true": "false");

    Serial.print("userPins: \n");
    for (size_t i = 0; i < pinCount; ++i) {
        Serial.println(userPins[i]);
    }

    for (size_t i = 0; i < pinCount; ++i) {
        delete[] userPins[i];
    }   
    delete[] userPins;
}

void clearBuffer() {
  for (int i = 0; i < 5; i++) {
    inputBuffer[i] = '\0';
  }
  inputIndex = 0;
}

String getIsLocked() {
    if (!jsonValid) return "error";

    const char* isLocked = doc[0]["found"]["fields"]["isLocked"]["integerValue"];
    return String(isLocked);
}


 
 void batch_get_async(BatchGetDocumentOptions &options)
 {
     Serial.println("Checking Lock Status...");
 
     // Async call with callback function.
     // processData call what prints to serial monitor
     Docs.batchGet(aClient, Firestore::Parent(FIREBASE_PROJECT_ID), options, processData, "batchGetTask");
 }

void batch_write_await(Writes &writes)
{
    Serial.println("Batch writing the documents... ");

    // Sync call which waits until the payload was received.
    String payload = Docs.batchWrite(aClient, Firestore::Parent(FIREBASE_PROJECT_ID), writes);
    if (aClient.lastError().code() == 0)
        Serial.println(payload);
    else
        Firebase.printf("Error, msg: %s, code: %d\n", aClient.lastError().message().c_str(), aClient.lastError().code());
}

//NEW SIMPLIFIED FUNCTIONS

void checkWiFiConnection() {
    bool currentWiFiStatus = (WiFi.status() == WL_CONNECTED);

    if (currentWiFiStatus != isWiFiConnected) {
        isWiFiConnected = currentWiFiStatus;
        if (isWiFiConnected) {
            Serial.println("WiFi reconnected!");
            digitalWrite(LED_BUILTIN, LOW);
        } else {
            Serial.println("WiFi lost!");
        }
    }

    if (!isWiFiConnected) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(300);
        digitalWrite(LED_BUILTIN, LOW);
        delay(300);
    }
}

void pollFirestorePeriodically() {
    unsigned long currentTime = millis();
    if (currentTime - lastPollTime >= pollInterval) {
        lastPollTime = currentTime;
        BatchGetDocumentOptions options;
        options.documents("users/FuduqA91EfdHhEA8JAQNJ3SwrRJ2");
        options.mask(DocumentMask("isLocked,unlockedAck,userPins"));
        batch_get_async(options);
    }
}

void processKeypadInput() {
    char key = keypad.getKey();

    if (key) {
        Serial.println(key);
        inputBuffer[inputIndex] = key;
        inputIndex++;

        if (inputIndex == 4) {
            inputBuffer[4] = '\0';
            String enteredPIN = String(inputBuffer);
            Serial.print("Entered PIN: ");
            Serial.println(enteredPIN);

            if (!pinQueue.empty() && enteredPIN == pinQueue.front()) {
                Serial.println("Correct OTP PIN! Unlocking...");
                digitalWrite(LED_LOCK, HIGH);
                sendUnlockAcknowledgment();
                pinQueue.pop_front(); // Remove used PIN
                Serial.println("PIN consumed. Remaining count: " + String(pinQueue.size()));
                delay(10000); // After ten seconds of a successful unlock, automatically lock the door
                digitalWrite(LED_LOCK, LOW);
            } else {
                digitalWrite(LED_LOCK, LOW);
                sendUnlockAcknowledgment();
                Serial.println("Invalid or expired PIN.");
            }

            clearBuffer(); // Reset after 4 digits
        }
    }
}

void sendAcknowledgementIfNeeded() {
    if (sendAck & unlockAck) {
        sendAck = false;
        unlockAck = false;
        Serial.println("Sending Unlock Acknowledgement...");
        sendUnlockAcknowledgment();
    } else if (sendAck & lockAck) {
        sendAck = false;
        lockAck = false;
        Serial.println("Sending Lock Acknowledgement...");
        sendLockAcknowledgement();
    }
}

bool createUserPins() {
    // Free any existing pins before creating new ones
    freeUserPins();

    if (!jsonValid) return false;

    JsonArray values = doc[0]["found"]["fields"]["userPins"]["arrayValue"]["values"];
    userPinCount = values.size();

    if (userPinCount == 0) return false;

    userPins = new char*[userPinCount];

    for (size_t i = 0; i < userPinCount; ++i) {
        const char* pin = values[i]["stringValue"];
        size_t len = strlen(pin) + 1;
        userPins[i] = new char[len];
        strncpy(userPins[i], pin, len - 1);
        userPins[i][len - 1] = '\0';  // null-terminate safely
    }

    return true;
}

char** getUserPins() {
    return userPins;
}

size_t getUserPinCount() {
    return userPinCount;
}

void freeUserPins() {
    if (userPins != nullptr) {
        for (size_t i = 0; i < userPinCount; ++i) {
            delete[] userPins[i];
        }
        delete[] userPins;
        userPins = nullptr;
        userPinCount = 0;
    }
}
// END SIMPLIFIED FUNCTIONS