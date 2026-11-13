// APIClientStub.mm - local stand-in for the vendor client.
//
// The upstream dylib linked a 69 MB static library that talked to the vendor
// licence server before it would show the menu. This build is the no-key
// variant: the whole client is replaced by local stubs, the paid callback
// fires immediately, and nothing leaves the device.
//
// Every symbol declared in APIClient.h is implemented here, so the linker
// never needs the vendor archive.
#import <Foundation/Foundation.h>

#import "APIClient.h"

namespace {

// The menu only needs to know that the session is valid.
const char* kValidUntil = "2099-12-31 23:59:59";
const char* kPackageName = "Mizze Free Fire No Key";

}  // namespace

extern "C" {

void apiclient_set_token(const char* token) { (void)token; }
void apiclient_set_udid(const char* udid) { (void)udid; }
void apiclient_set_language(const char* language) { (void)language; }
void apiclient_set_contact_button_title(const char* title) { (void)title; }
void apiclient_set_description(const char* description) { (void)description; }
void apiclient_hide_ui(bool isHide) { (void)isHide; }
void apiclient_strict_mode(bool isStrictMode) { (void)isStrictMode; }
void apiclient_silent_mode(bool isSilentMode) { (void)isSilentMode; }
void apiclient_show_udid_guide(bool showGuide) { (void)showGuide; }
void apiclient_set_window_mode(int mode) { (void)mode; }

// The whole point of the no-key build: the callback runs now.
void apiclient_paid(apiclient_callback callback) {
  if (callback) callback();
}

const char* apiclient_request_udid(void) { return ""; }
const char* apiclient_get_key(void) { return ""; }
const char* apiclient_get_udid(void) { return ""; }
const char* apiclient_get_expired_at(void) { return kValidUntil; }
const char* apiclient_get_expired_at_local(void) { return kValidUntil; }
const char* apiclient_get_activated_at(void) { return kValidUntil; }
const char* apiclient_get_activated_at_local(void) { return kValidUntil; }
const char* apiclient_get_device_model(void) { return "iPhone"; }
const char* apiclient_get_login_ip(void) { return ""; }
const char* apiclient_get_package_name(void) { return kPackageName; }

const char* apiclient_get_package_data(const char* key) {
  (void)key;
  return "";
}

void apiclient_on_check_package(apiclient_dict_callback success,
                                apiclient_dict_callback failure) {
  (void)failure;
  if (success) success("{\"status\":1}");
}

void apiclient_on_check_device(apiclient_dict_callback success,
                               apiclient_dict_callback failure) {
  (void)failure;
  if (success) success("{\"status\":1}");
}

void apiclient_on_login(const char* inputKey, apiclient_dict_callback success,
                        apiclient_dict_callback failure) {
  (void)inputKey;
  (void)failure;
  if (success) success("{\"status\":1,\"expired_at\":\"2099-12-31\"}");
}

}  // extern "C"
