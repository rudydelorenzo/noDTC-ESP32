#include <Arduino.h>
#include <ELMo.h>
#include <LinkedList.h>
#include <WiFi.h>

#define uS_TO_S_FACTOR 1000000ULL	/* Conversion factor for micro seconds to seconds */
#define DEEP_SLEEP_TIME 180 		/* Seconds */
#define SLEEP_AFTER_SCAN 10 		/* Seconds */

const char *ssid = "WiFi-OBDII";
int codes[] = {0741, 1525};

bool clear = false;
bool debug = true;
ELMo ELM;

void setup() {
        // Define DeepSleep parameters
        esp_sleep_enable_timer_wakeup(DEEP_SLEEP_TIME * uS_TO_S_FACTOR);

        // Start serial (for debugging)
	if (debug) {
		Serial.begin(115200);
		while (!Serial);
	}

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


void extract(String s, LinkedList<int>* c) {
  for (int i = 0; i + 4 <= s.length(); i = i + 4) {
    if (s[i] == '4') i = i - 2;
    else {
      c->add(s.substring(i, i + 4).toInt());
    }
  }
}


void loop() {
	// Codes list will hold all values returned from the car, even 0000s
        LinkedList<int> presentCodes = LinkedList<int>();

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

	String confirmed = "";
	String pending = "";
        if (status.substring(6, 7).equals("8")) {
                // CEL is on
                confirmed = removeAlpha(ELM.send("03"));
        } else if (status.substring(6, 7).equals("0")) {
                pending = removeAlpha(ELM.send("07"));
        } else {
		// If the value is anything other than 8 oe 0 something is wrong
		if (debug) Serial.println("NONSENSE RECEIVED FOR STATUS - RESET!");
		ELM.stop();
		ESP.restart();
	}

	// Determine if codes should be cleared
	clear = false;
	bool allZeros = true;	// Signals if everything is 0, in that case we don't clear
	extract(confirmed, &presentCodes);
	extract(pending, &presentCodes);

        for (int i = 0; i < presentCodes.size(); i++) {
                if (presentCodes.get(i) != 0) allZeros = false;
                else continue;

                // If you get here, we have a legitimate code
                bool match = false;
                for (int j = 0; j < (sizeof(codes) / sizeof(codes[0])); j++) {
                        if (presentCodes.get(i) == codes[j]) match = true;
                }
                if (!match) {
                        clear = false;
                        break;
                }
                clear = true;
        }

        if (allZeros) clear = false;

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
