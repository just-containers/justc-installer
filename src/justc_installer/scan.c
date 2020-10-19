#include "scan.h"

uint64_t justc_uint64_scan(const char *s, size_t len, uint8_t base) {
    uint64_t result = 0;
    uint8_t t;
    size_t pos = 0;

    while(pos < len) {
        if(s[pos] >= '0' && s[pos] <= '9') {
            t = s[pos] - '0';
            if(t < base) {
                result *= base;
                result += s[pos] - '0';
            }
        }
        pos++;
    }
    return result;
}
