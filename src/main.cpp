#include <Arduino.h>
#include "IRac.h"
#include "IRrecv.h"
#include "IRutils.h"

#ifndef IR_RECEIVE_PIN
#define IR_RECEIVE_PIN 4
#endif

IRrecv irrecv(IR_RECEIVE_PIN);
decode_results results;

void setup() {
  Serial.begin(115200);
  irrecv.enableIRIn();
  Serial.printf("AC Guard listening for an IR signal on GPIO %d\n", IR_RECEIVE_PIN);
}

void loop() {
  if (!irrecv.decode(&results)) return;

  if (results.decode_type != UNKNOWN) {
    stdAc::state_t state{};
    if (IRAcUtils::decodeToState(&results, &state)) {
      Serial.println(IRac::stateToString(&state));
    }
  }

  irrecv.resume();
}