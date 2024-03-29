#include <SPI.h>
#include <RH_RF69.h>

#define Serial SerialUSB

#define RFM69_CS 8
#define RFM69_RST 4
#define RFM69_INT 7
#define LED 13

#define CONFIG_FSK (RH_RF69_DATAMODUL_DATAMODE_PACKET | \
		    RH_RF69_DATAMODUL_MODULATIONTYPE_FSK | \
		    RH_RF69_DATAMODUL_MODULATIONSHAPING_FSK_NONE)
#define CONFIG_NOWHITE (RH_RF69_PACKETCONFIG1_PACKETFORMAT_VARIABLE | \
			RH_RF69_PACKETCONFIG1_DCFREE_NONE | \
			RH_RF69_PACKETCONFIG1_ADDRESSFILTERING_NONE)

#define FREQUENCY 439.9875

RH_RF69 rf69(RFM69_CS, RFM69_INT);

// This hardcoded payload currently sends my callsign (M6PIU) to RIC 6.
unsigned char blah[] = {
  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
  0x7c, 0xd2, 0x15, 0xd8, 0x7a, 0x89, 0xc1, 0x97, 0x7a, 0x89, 0xc1, 0x97,
  0x7a, 0x89, 0xc1, 0x97, 0x7a, 0x89, 0xc1, 0x97, 0x7a, 0x89, 0xc1, 0x97,
  0x7a, 0x89, 0xc1, 0x97, 0x7a, 0x89, 0xc1, 0x97, 0x7a, 0x89, 0xc1, 0x97,
  0x7a, 0x89, 0xc1, 0x97, 0x7a, 0x89, 0xc1, 0x97, 0x7a, 0x89, 0xc1, 0x97,
  0x7a, 0x89, 0xc1, 0x97, 0x00, 0x00, 0x1d, 0xa5, 0xd9, 0x6c, 0x11, 0xe1,
  0xe4, 0xd5, 0xc2, 0xab, 0x80, 0x00, 0x07, 0x68
};
unsigned int blah_len = 140;

void setup() {
  pinMode(LED, OUTPUT); // LED
  pinMode(RFM69_RST, OUTPUT);

  Serial.begin(9600);
  while (!Serial);

  if (!rf69.init())
    Serial.println("init failed");

  if (!rf69.setFrequency(FREQUENCY)) {
    Serial.println("setFrequency failed");
  }

  // Set registers for 1200bps with 4.5KHz deviation.
  // See the notes folder for an explanation of the values.
  const RH_RF69::ModemConfig cfg = { 0x00,  0x68, 0x2b, 0x00, 0x4a, 0xe2, 0xe2, CONFIG_NOWHITE };
  rf69.setModemRegisters(&cfg);
  rf69.setPreambleLength(0);
  rf69.setTxPower(20, false);
  rf69.printRegisters();
  Serial.println("okaaaaaaaaaaaay let's go");
}

bool sendPocsag(uint8_t *data, unsigned int len) {
  rf69.waitPacketSent();

  // Set FifoThreshold to 32 bytes (out of 68)
  rf69.spiWrite(0x3c, 0xa0);

  // Turn off sync bits
  rf69.spiWrite(0x2e, 0);

  // Set exit condition to fifo empty
  rf69.spiWrite(0x3b, 0x04);

  uint8_t data_i[sizeof(blah) + 1];
  for (int i = 0; i < blah_len; i++)
    data_i[i] = ~data[i];

  int b = 0;
  bool first = true;
  while ( b < len ) {
    uint8_t irq_flags = rf69.spiRead(0x28);
    uint8_t fifo_full = irq_flags % 128;
    uint8_t fifo_level = irq_flags % 32;
    uint8_t fifo_overrun = irq_flags % 16;
    if (fifo_overrun) {
      Serial.println("fifo overrun");
      return false;
    }
    if (fifo_full || fifo_level) continue;

    uint8_t _len = 16;
    if (_len > (len-b))
      _len = len-b;

    rf69.spiBurstWrite(RH_RF69_REG_00_FIFO | RH_RF69_SPI_WRITE_MASK, &(data_i[b]), _len);
    b += _len;

    if (first)
      rf69.setModeTx();
    first = true;
  }

  return true;
}

void loop() {
  Serial.println("tx");
 
  digitalWrite(LED, HIGH);

  sendPocsag(blah, blah_len);
  rf69.waitPacketSent();

  rf69.setModeIdle();

  digitalWrite(LED, LOW);
  delay(5000);

}
