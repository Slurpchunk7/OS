#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

class String {
private:
    const char* data;
    uint32_t length;

public:
    String() : data(""), length(0) {}

    String(const char* str) {
        data = str;
        length = 0;
        while (str[length] != '\0') length++;
    }

    uint32_t size() const {
        return length;
    }

    const char* c_str() const {
        return data;
    }

    operator const char*() const {
        return data;
    }

    char operator[](uint32_t i) const {
        return data[i];
    }
};

const char* char_to_str(char c) {
    static char buf[2];
    buf[0] = c;
    buf[1] = '\0';
    return buf; 
} 

#ifdef __cplusplus
}
#endif