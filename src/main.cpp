#include <Arduino.h>
#include "IRac.h"
#include "IRrecv.h"
#include "IRutils.h"

#ifndef IR_RECEIVE_PIN
#define IR_RECEIVE_PIN 4
#endif

// Initialize the IR receiver on GPIO 4, with buffer size of 1024, timeout of 50ms, and buffer save enabled.
// See the library's IRrecvDumpV2 example for more details on the parameters.
IRrecv irrecv(IR_RECEIVE_PIN, 1024, 100, true);
decode_results results;

void setup() {
  Serial.begin(115200);
  irrecv.enableIRIn();
  Serial.printf("AC Guard listening for an IR signal on GPIO %d\n", IR_RECEIVE_PIN);
}

void loop() {
  bool valid = irrecv.decode(&results);
  Serial.println(valid ? "Valid signal received" : "No valid signal received");

  if (!valid) {
    // Serial.println("Hi");
    return;
  }

  if (results.decode_type != UNKNOWN) {
    Serial.printf("Received %s signal with %d bits\n", typeToString(results.decode_type), results.bits);
    stdAc::state_t state{};
    if (IRAcUtils::decodeToState(&results, &state)) {
      Serial.println(IRAcUtils::resultAcToString(&results));
    }
  }

  Serial.println("Waiting for next signal...");
  irrecv.resume();
  Serial.println("Reached the end");
}