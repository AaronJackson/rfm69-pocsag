#include "pocsag.h"

// Bit reverse
uint8_t bitReverse8(uint8_t b) {
  b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
  b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
  b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
  return b;
}

// Calculate the BCH 31 21 5 checksum
void calculateBCH3121sum(uint32_t* x) {
#define ADDRESS_MASK (uint32_t)0xFFFFF800  //valid data is from POCSAG bits 1-21, 22-31 is the BCH, 32 is parity

// In order to save time use a pre-computed polynomial, g(x) = 10010110111
#define G_X 0x769

  const int k = 21;
  uint32_t generator = (uint32_t)G_X << k;
  const int n = NUM_BITS_INT;  //31

  //dividend is the bits in the data
  *x &= (uint32_t)ADDRESS_MASK;

  uint32_t dividend = *x;

  uint32_t mask = (uint32_t)1 << n;

  for (int i = 0; i < k; i++) {
    if (dividend & mask)
      dividend ^= generator;
    generator >>= 1;
    mask >>= 1;
  }

  *x |= dividend;
}


void calculateEvenParity(uint32_t* x) {
  int count = 0;

  for (int i = 1; i < NUM_BITS_INT; i++) {
    if ((*x) & ((uint32_t)1 << i)) {
      // There is a 1 bit
      count++;
    }
    //go to next bit
  }

  // If count is even, the parity bit is 0
  count = count % 2;

  *x |= count;
}

// Get 18bits for address field
uint32_t encodeAddress(uint32_t address) {
  address >>= 3;  // remove least significant 3 bits as these form the offset in a batch.

  address &= (uint32_t)0x0007FFFF;
  address <<= 2;

  // add the function bits
  address |= FUNCTION_CODE;

  address <<= 11;

  calculateBCH3121sum(&address);
  calculateEvenParity(&address);

  return htonl(address);
}


// Encode regular C string into 7bit ascii string.
struct Ascii7BitStruct* ascii7bitEncoder(const char* message) {

  int length = strlen(message);
      // int encoded_length = (int)((float)7/8 * length);
  int encoded_length = (int)(ceil((float)length / 7.0 * 8));

  unsigned char* encoded = calloc(sizeof(unsigned char), encoded_length + 1);
  memset(encoded, 0, encoded_length); // EOT

  int shift = 1;  //count of number of bits to right of each 7bit char
  uint8_t* curr = (uint8_t*)encoded;

  // remove the 8th bit then reverse, shift and pack
  for (int i = 0; i < length; i++) {
    uint16_t tmp = bitReverse8(message[i]);

    tmp &= 0x00fe;
    tmp >>= 1;

    tmp <<= shift;

    *curr |= (unsigned char)(tmp & 0x00ff);
    if (curr > encoded)
      *(curr - 1) |= (unsigned char)((tmp & 0xff00) >> 8);

    shift++;

    if (shift == 8)
      shift = 0;
    else if (length > 1)
      curr++;
  }

  struct Ascii7BitStruct* encodedString = malloc(sizeof(struct Ascii7BitStruct));
  encodedString->asciiPtr = encoded;
  encodedString->length = encoded_length + 1;

  return encodedString;
}

// Split the 7bit ascii string into frames of 20bit messages + chksum + parity
struct FrameStruct* splitMessageIntoFrames(struct Ascii7BitStruct* ascii7bitBuffer) {
  // 20bits of message
  int chunks = (ascii7bitBuffer->length / 3) + 1;

  uint32_t* batches = calloc(sizeof(uint32_t), chunks);

  unsigned char* curr = ascii7bitBuffer->asciiPtr;

  const unsigned char* end = curr + ascii7bitBuffer->length;

  for (int i = 0; i < chunks; i++) {
    if (end - curr > 3) {
      memcpy((unsigned char*)&batches[i], curr, 3);
    } else {
      memcpy((unsigned char*)&batches[i], curr, end - curr);
    }
    batches[i] = htonl(batches[i]);

    if (!(i % 2)) {
      if (end - curr >= 3)
        curr += 2;
      batches[i] &= 0xfffff000;
      batches[i] >>= 1;
    } else {
      if (end - curr >= 3)
        curr += 3;
      batches[i] &= 0x0fffff00;
      batches[i] <<= 3;
    }

    batches[i] |= ((uint32_t)1 << NUM_BITS_INT);  // set MSB, to signify that it is a message and not an address
    calculateBCH3121sum(&batches[i]);
    calculateEvenParity(&batches[i]);

    batches[i] = htonl(batches[i]);
  }

  curr = 0;

  struct FrameStruct* frames = malloc(sizeof(struct FrameStruct));

  frames->framePtr = batches;
  frames->length = chunks;

  return frames;
}