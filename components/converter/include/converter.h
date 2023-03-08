#ifndef COMPONENTS_CONVERTER_H_
#define COMPONENTS_CONVERTER_H_

#ifdef __cplusplus
extern "C" {
#endif

//#define LINE_MAX 1024

typedef struct ConvLetter {
        char    	win1251;
        uint16_t    unicode;
} Letter;

char* utf8tow1251(const char* utf);
char* w1251toutf(const char* w1251);

#ifdef __cplusplus
}
#endif

#endif // COMPONENTS_CONVERTER_H_
