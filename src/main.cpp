#include <Arduino.h>
#include <ELMo.h>
#include <LinkedList.h>
#include <WiFi.h>

#define uS_TO_S_FACTOR 1000000ULL	/* Conversion factor for micro seconds to seconds */
#define DEEP_SLEEP_TIME 180 		/* Seconds */
#define SLEEP_AFTER_SCAN 10 		/* Seconds */

const char *ssid = "WiFi-OBDII";
int codes[] = {0000, 0741, 1525};

bool clear = false;
bool debug = true;
ELMo ELM;

void setup() {
        // Define DeepSleep parameters
        esp_sleep_enable_timer_wakeup(DEEP_SLEEP_TIME * uS_TO_S_FACTOR);

        // Start serial (for debugging)
        Serial.begin(115200);
        while (!Serial);

        // Connect to WiFi
        if (debug) {
                Serial.print("Connecting to WiFi: ");
                Serial.println(ssid);
        }

        WiFi.setHostname("ELMo_ESP32");
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid);

        int attempts = 1.;
        while (WiFi.status() != WL_CONNECTED) {
                // After 5 connecion attempts, deep sleep. The car is off or adapter is unplugged.
                if (attempts > 5) {
			if (debug) Serial.println("ERROR: COULDN'T CONNECT TO WIFI - DEEP SLEEP");
                        esp_deep_sleep_start();
                }
                if (debug) Serial.println("Failed to Connect, retrying in 1 second...");
                delay(1000);
                attempts++;
        }

        if (debug) {
                Serial.println("Connected to WiFi");
                Serial.print("IP address: ");
                Serial.println(WiFi.localIP());
        }

        // Connect to ELM
        ELM.setDebug(debug);
        ELM.setTimeout(10);
        if (!ELM.initialize()) {
                if (debug) Serial.println("FAILED TO INITIALIZE - DEEP SLEEPING");
                esp_deep_sleep_start();
        }
}

void loop() {
	// Codes list will hold all values returned from the car, even 0000s
        LinkedList<int> codes = LinkedList<int>();

        // Good practice to always ensure that ELM is connected
        if (!ELM.connected()) {
                if (debug) Serial.println("CLIENT DISCONNECTED - RESET!");
                ELM.stop();
                ESP.restart();
        }
	
	// Get all the codes, confirmed and pending
        String status = ELM.send("0101");
        if (status == "NO DATA") {
                if (debug) Serial.println("STATUS RECEIVED NO DATA - RESET!");
                ELM.stop();
                ESP.restart();
        }

        if (status.substring(6, 7).equals("8")) {
                // CEL is on
                String confirmedCodes = ELM.send("03");
                for (int i = 0; i < confirmedCodes.length(); i++) {
                        Serial.println((char)confirmedCodes[i]);
                        Serial.println((int)confirmedCodes[i]);
                        Serial.println();
                }
                // String code1 = rawCodes.substring(3, 8);
                // String code2 = rawCodes.substring(25, 30);
        } else if (status.substring(6, 7).equals("0")) {
                String pending = ELM.send("07");
                clear = true;
                String code1 = pending.substring(3, 8);
                String code2 = pending.substring(25, 30);
                if (!(code1.equals("00 00") || code1.equals("07 41") ||
                      code1.equals("15 25")))
                        clear = false;
                if (!(code2.equals("00 00") || code2.equals("07 41") ||
                      code2.equals("15 25")))
                        clear = false;
        }

	// Determine if codes should be cleared
	clear = false;


	// Clear codes if necessary
        if (clear) {
                if (debug) Serial.println("CODES MUST BE CLEARED");
		// ELM.send("04");
        }

        // Sleep till next loop
        // TODO: use TaskScheduler
        if (debug) Serial.println("DONE! SLEEPING FOR 10 SEC...");
        delay(SLEEP_AFTER_SCAN * 1000);
}

String removeAlpha(String s) {
        for (int i = 0; i < s.length(); i++) {
                char c = s[i];
                if (!isDigit(c)) {
                        s.remove(i, 1);
                        i--;
                }
        }
        return s;
}
