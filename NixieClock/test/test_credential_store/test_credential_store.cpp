#include <Arduino.h>
#include <unity.h>

#include "CredentialStore.h"

void test_obfuscation_round_trips_ascii_credentials() {
  const String original = "The_LAN_Before_Time";
  const String encoded = NixieWiFiManagerDetail::obfuscateToHex(original);
  String decoded;

  TEST_ASSERT_NOT_EQUAL(original, encoded);
  TEST_ASSERT_TRUE(NixieWiFiManagerDetail::deobfuscateFromHex(encoded, decoded));
  TEST_ASSERT_EQUAL_STRING(original.c_str(), decoded.c_str());
}

void test_empty_password_round_trips() {
  const String original = "";
  const String encoded = NixieWiFiManagerDetail::obfuscateToHex(original);
  String decoded = "unchanged";

  TEST_ASSERT_EQUAL_STRING("", encoded.c_str());
  TEST_ASSERT_TRUE(NixieWiFiManagerDetail::deobfuscateFromHex(encoded, decoded));
  TEST_ASSERT_EQUAL_STRING("", decoded.c_str());
}

void test_invalid_hex_is_rejected() {
  String decoded;

  TEST_ASSERT_FALSE(NixieWiFiManagerDetail::deobfuscateFromHex("ABC", decoded));
  TEST_ASSERT_FALSE(NixieWiFiManagerDetail::deobfuscateFromHex("ZZ", decoded));
}

void setup() {
  UNITY_BEGIN();
  RUN_TEST(test_obfuscation_round_trips_ascii_credentials);
  RUN_TEST(test_empty_password_round_trips);
  RUN_TEST(test_invalid_hex_is_rejected);
  UNITY_END();
}

void loop() {}
