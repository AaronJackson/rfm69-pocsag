#include <SPI.h>
#include <RH_RF69.h>

extern "C" {
#include "pocsag.h"
}

#define Serial SerialUSB

#define RFM69_CS 8
#define RFM69_RST 4
#define RFM69_INT 7
#define LED 13
#define ESP_RST 10

#define CONFIG_FSK (RH_RF69_DATAMODUL_DATAMODE_PACKET | RH_RF69_DATAMODUL_MODULATIONTYPE_FSK | RH_RF69_DATAMODUL_MODULATIONSHAPING_FSK_NONE)
#define CONFIG_NOWHITE (RH_RF69_PACKETCONFIG1_PACKETFORMAT_VARIABLE | RH_RF69_PACKETCONFIG1_DCFREE_NONE | RH_RF69_PACKETCONFIG1_ADDRESSFILTERING_NONE)

#define FREQUENCY 439.9875

#define ID "MB7PNH POCSAG"

RH_RF69 rf69(RFM69_CS, RFM69_INT);

void setup() {
  pinMode(LED, OUTPUT);  // LED
  pinMode(RFM69_RST, OUTPUT);
  pinMode(ESP_RST, OUTPUT);
  digitalWrite(ESP_RST, HIGH);

  Serial.begin(9600); // USB
  Serial1.begin(9600); // RX/TX pins

  if (!rf69.init())
    Serial.println("init failed");

  if (!rf69.setFrequency(FREQUENCY)) {
    Serial.println("setFrequency failed");
  }

  // Set registers for 1200bps with 4.5KHz deviation.
  // See the notes folder for an explanation of the values.
  const RH_RF69::ModemConfig cfg = { 0x00, 0x68, 0x2b, 0x00, 0x4a, 0xe2, 0xe2, CONFIG_NOWHITE };
  rf69.setModemRegisters(&cfg);
  rf69.setPreambleLength(0);
  rf69.setTxPower(20, true);
}

bool sendPocsag(uint8_t *data, unsigned int len) {
  rf69.waitPacketSent();

  while (!rf69.waitCAD());

  // Set FifoThreshold to 32 bytes (out of 68)
  rf69.spiWrite(0x3c, 0xa0);

  // Turn off sync bits
  rf69.spiWrite(0x2e, 0);

  // Set exit condition to fifo empty
  rf69.spiWrite(0x3b, 0x04);

  uint8_t data_i[len];
  for (int i = 0; i < len; i++)
    data_i[i] = ~data[i];

  int b = 0;
  bool first = true;
  while (b < len) {
    uint8_t irq_flags = rf69.spiRead(0x28);
    uint8_t fifo_full = irq_flags % 128;
    uint8_t fifo_level = irq_flags % 32;
    uint8_t fifo_overrun = irq_flags % 16;
    if (fifo_overrun) {
      return false;
    }
    if (fifo_full || fifo_level) continue;

    uint8_t _len = 16;
    if (_len > (len - b))
      _len = len - b;

    rf69.spiBurstWrite(RH_RF69_REG_00_FIFO | RH_RF69_SPI_WRITE_MASK, &(data_i[b]), _len);
    b += _len;

    if (first)
      rf69.setModeTx();
    first = true;
  }

  return true;
}

int encodePocsag(uint32_t address, const char *message, char *buffer) {
  int bufidx = 0;

  const uint32_t framesync_codeword = htonl(FRAMESYNC_CODEWORD);
  const uint32_t idle_codeword = htonl(IDLE_CODEWORD);

  memset(buffer, PREAMBLE_FILL, PREAMBLE_LENGTH);
  bufidx += PREAMBLE_LENGTH;

  const int frame_offset = address & 0x7;
  const uint32_t encodedAddress = encodeAddress(address);

  struct FrameStruct *messageFrames;
  if (!strlen(message)) {
    messageFrames = malloc(sizeof(struct FrameStruct));
    messageFrames->length = 1;
    uint32_t *data = malloc(sizeof(uint32_t));
    messageFrames->framePtr = data;
    memcpy(data, &idle_codeword, sizeof(uint32_t));
  } else {
    struct Ascii7BitStruct *encodedMessage = ascii7bitEncoder(message);
    Serial.print("Encoded length:") ;
    Serial.print(encodedMessage->length);
    Serial.println();
    messageFrames = splitMessageIntoFrames(encodedMessage);
    Serial.print("Number of frames: ");
    Serial.print(messageFrames->length);
    Serial.println();

    free(encodedMessage->asciiPtr);
    free(encodedMessage);
    encodedMessage = NULL;
  }

  memcpy(buffer + bufidx, &framesync_codeword, 4);
  bufidx += 4;

  Serial.print("      sync:");
  for(int xx = PREAMBLE_LENGTH; xx < bufidx; xx++) {
    Serial.print(buffer[xx] & 0xFF, HEX);
    Serial.print(", ");
  }
  Serial.println();

  Serial.println(frame_offset);
  int frames_left = (frame_offset + messageFrames->length + NUM_FRAMES - 1);
  int messagePartsDone = 0;
  int codewordsDone = 0;

  while (frames_left > 0) {
    Serial.print("frames_left: ");
    Serial.println(frames_left);

    if (!codewordsDone && !messagePartsDone) {
      for (int i = 0; i < frame_offset; i++) {
        // Fill with idles until the required offset for the receiver is found
        memcpy(buffer + bufidx, &idle_codeword, 4);
        memcpy(buffer + bufidx + 4, &idle_codeword, 4);
        bufidx += 8;
        codewordsDone += 2;
        Serial.println("Added two idle codewords (8 bytes)");
      }

      // skipped to the frame number after the offset.
      // Now add an address code word.
      memcpy(buffer + bufidx, &encodedAddress, 4);
      bufidx += 4;
      frames_left -= frame_offset;
      codewordsDone++;
      Serial.println("Added encoded address (2 bytes)");
    }

    // Add the message frames
    for (int i = messagePartsDone; i < messageFrames->length; i++) {
      memcpy(buffer + bufidx, &(messageFrames->framePtr[i]), 4);
      bufidx += 4;
      Serial.println("Added message frame (4 bytes)");
      messagePartsDone++;
      codewordsDone++;
      frames_left--;
      if (codewordsDone == NUM_FRAMES) {
        codewordsDone = 0;
        break;
      }
    }

    // fill the last batch with idles.
    if (messagePartsDone == messageFrames->length) {
      frames_left = 0;
      //pad out the rest of the batch with idles
      for (int i = 0; i < (NUM_FRAMES - codewordsDone); i++) {
        if (bufidx > sizeof(buffer)) {
          break;
        }
        // Fill with idles until the batch is done
        memcpy(buffer + bufidx, &idle_codeword, 4);
        Serial.print("      idle:");
        for(int xx = PREAMBLE_LENGTH; xx < bufidx; xx++) {
          Serial.print(buffer[xx] & 0xFF, HEX);
          Serial.print(", ");
        }
        Serial.println();
        bufidx += 4;
        Serial.println("Added idle codeword (4 bytes)");
      }
    }
  }

  // Always stick an idle on the end - seems to fix things........
  memcpy(buffer + bufidx, &idle_codeword, 4);
  bufidx += 4;

  free(messageFrames->framePtr);
  free(messageFrames);

  Serial.println(bufidx);
  return bufidx;
}

void loop() {
  Serial.println("waiting for input (format: addr|msg)");

  Serial1.setTimeout(1000);

  String tmpaddr = Serial1.readStringUntil('|');
  if (!tmpaddr.length()) return;
  String tmpmsg = Serial1.readStringUntil('\n');
  if (!tmpmsg.length()) return;

  Serial.println(tmpmsg);
  digitalWrite(LED, HIGH);


  char buffer[400] = {0};
  int bufidx = encodePocsag(tmpaddr.toInt(), tmpmsg.c_str(), buffer);
  sendPocsag(buffer, bufidx);
  rf69.waitPacketSent();
  rf69.setModeIdle();

  delay(1000);

  char bufferID[200] = {0};
  int bufidxID = encodePocsag(6, ID, bufferID);
  sendPocsag(bufferID, bufidxID);
  rf69.waitPacketSent();
  rf69.setModeIdle();

  digitalWrite(LED, LOW);
}
