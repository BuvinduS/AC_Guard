// STEP 1 - smoke test: proves IRremoteESP8266 builds on the host and that
//   raw microsecond timings -> IRrecv::decode -> IRAcUtils::decodeToState works.
//
// FIXTURE CATEGORY: SYNTHETIC. Timings are produced by the library's own Daikin
// encoder, so this proves our plumbing, NOT that any real AC behaves this way.
#include <unity.h>
#include <cstdint>
#include <vector>
#include "IRac.h"
#include "IRrecv.h"
#include "IRsend.h"
#include "IRutils.h"
#include "ir_Daikin.h"

// Records what IRsend "transmits" as alternating mark/space durations (us).
// mark()/space() are only virtual when UNIT_TEST is defined.
class RecordingSend : public IRsend {
 public:
  std::vector<uint32_t> us;
  bool lastWasSpace = true;
  RecordingSend() : IRsend(0) {}
  uint16_t mark(uint16_t usec) override {
    if (us.empty() || lastWasSpace) us.push_back(usec); else us.back() += usec;
    lastWasSpace = false;
    return 0;
  }
  void space(uint32_t usec) override {
    if (us.empty()) us.push_back(0);
    if (lastWasSpace) us.back() += usec; else us.push_back(usec);
    lastWasSpace = true;
  }
};

static std::vector<uint16_t> synthDaikin(uint8_t temp) {
  RecordingSend tx;
  IRDaikinESP ac(0);
  ac.begin(); ac.on(); ac.setMode(kDaikinCool); ac.setTemp(temp);
  tx.sendDaikin(ac.getRaw());
  return std::vector<uint16_t>(tx.us.begin(), tx.us.end());
}

// Our (future) adapter in miniature: us timings -> library capture buffer.
// rawbuf[0] is unused, entries are in kRawTick (2 us) units, and decoders read
// past rawlen, so the buffer is padded with zero sentinels.
struct Capture {
  std::vector<uint16_t> ticks;
  decode_results res;
  explicit Capture(const std::vector<uint16_t> &us) : ticks(us.size() + 1 + 8, 0) {
    for (size_t i = 0; i < us.size(); i++) ticks[i + 1] = us[i] / kRawTick;
    res.rawbuf = ticks.data();
    res.rawlen = (uint16_t)(us.size() + 1);
    res.overflow = false;
    res.decode_type = UNKNOWN;
    res.bits = 0;
  }
};

void setUp(void) {}
void tearDown(void) {}

void test_synthetic_daikin_temperatures_roundtrip() {
  IRrecv irrecv(0);
  for (uint8_t t = 18; t <= 30; t++) {
    Capture cap(synthDaikin(t));
    TEST_ASSERT_TRUE(irrecv.decode(&cap.res));
    TEST_ASSERT_EQUAL(DAIKIN, cap.res.decode_type);
    stdAc::state_t st{};
    TEST_ASSERT_TRUE(IRAcUtils::decodeToState(&cap.res, &st));
    TEST_ASSERT_TRUE(st.celsius);
    TEST_ASSERT_EQUAL_FLOAT(t, st.degrees);
  }
}

// decode() returns true even for garbage (hash fallback), so success must be
// judged by decode_type != UNKNOWN, never by the return value alone.
void test_every_single_bit_flip_is_rejected() {
  const auto base = synthDaikin(24);
  IRrecv irrecv(0);
  int flipped = 0;
  for (size_t i = 1; i < base.size(); i += 2) {        // odd index = space
    if (base[i] > 3000 || base[i] < 300) continue;     // skip header/footer/gap
    auto us = base;
    us[i] = (us[i] < 800) ? 1280 : 428;                // short <-> long space = 1 bit flip
    Capture cap(us);
    irrecv.decode(&cap.res);
    stdAc::state_t st{};
    const bool valid = cap.res.decode_type != UNKNOWN &&
                       IRAcUtils::decodeToState(&cap.res, &st);
    TEST_ASSERT_FALSE_MESSAGE(valid, "corrupted frame produced a valid AC state");
    flipped++;
  }
  TEST_ASSERT_GREATER_THAN(100, flipped);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_synthetic_daikin_temperatures_roundtrip);
  RUN_TEST(test_every_single_bit_flip_is_rejected);
  return UNITY_END();
}