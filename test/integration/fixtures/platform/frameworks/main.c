#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

int main() {
  CFAbsoluteTime time = CFAbsoluteTimeGetCurrent();
  CFStringRef message = SecCopyErrorMessageString(errSecSuccess, NULL);
  if (message) {
    CFRelease(message);
  }
  return time > 0 ? 0 : 1;
}
