#include <stdint.h>

#define htonl(x) __builtin_bswap32((uint32_t) (x))

#define PREAMBLE_LENGTH 72 //min 576 bits
#define FRAME_LENGTH 64 //1 batch = 8 frames * 8bytes
#define NUM_FRAMES (FRAME_LENGTH / 4) //16x 4bytes = 64bytes
/* #define NUM_FRAMES 16 */

#define PREAMBLE_FILL 0xAA
#define FUNCTION_CODE 0x03 //3 is for alpha mode

#define NUM_BITS_INT (sizeof(uint32_t)*8 -1)

const uint32_t FRAMESYNC_CODEWORD = 0x7CD215D8;
const uint32_t IDLE_CODEWORD = 0x7A89C197;

// Store a chain of POCSAG encoded frames
struct FrameStruct
{
    uint32_t* framePtr;
    int length;
};

// Store a c string converted to an array of 7bit "bytes"
struct Ascii7BitStruct
{
    unsigned char *asciiPtr;
    int length;
};

uint8_t bitReverse8(uint8_t b);
void calculateBCH3121sum (uint32_t *x);
void calculateEvenParity (uint32_t *x);
uint32_t encodeAddress(uint32_t address);
struct Ascii7BitStruct* ascii7bitEncoder(const char* message);
struct FrameStruct* splitMessageIntoFrames(struct Ascii7BitStruct *ascii7bitBuffer);
