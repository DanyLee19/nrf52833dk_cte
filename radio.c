#include "radio.h"
#include "led_assert.h"
#include <string.h>

#if RADIO_MODE == NRF_RADIO_MODE_BLE_1MBIT \
 || RADIO_MODE == NRF_RADIO_MODE_NRF_1MBIT \
 || RADIO_MODE == NRF_RADIO_MODE_NRF_2MBIT
#define RADIO_PACKET_PREAMBLELEN NRF_RADIO_PREAMBLE_LENGTH_8BIT
#elif RADIO_MODE == NRF_RADIO_MODE_BLE_2MBIT
#define RADIO_PACKET_PREAMBLELEN NRF_RADIO_PREAMBLE_LENGTH_16BIT
#endif
#define RADIO_PACKET_MAXLEN (RADIO_PACKET_PLSLEN+(RADIO_PACKET_LENLEN/8)+((RADIO_PACKET_LENLEN%8)>0))

typedef struct
{
  #if RADIO_PACKET_S0LEN > 0
  uint8_t S0[RADIO_PACKET_S0LEN];
  #endif
  #if RADIO_PACKET_LENLEN > 0
  uint8_t LEN[(RADIO_PACKET_LENLEN/8)+((RADIO_PACKET_LENLEN%8)>0)];
  #endif
  #if RADIO_PACKET_S1LEN > 0
  uint8_t S1[(RADIO_PACKET_S1LEN/8)+((RADIO_PACKET_S1LEN%8)>0)];
  #endif
  uint8_t PAYLOAD[RADIO_PACKET_MAXLEN];
} RADIO_packet_t;

static RADIO_packet_t tx_packet;
static RADIO_packet_t rx_packet;
static uint32_t iq_data[RADIO_DFE_MAXCNT];

void RADIO_init(void)
{
  assert((2400 <= RADIO_FREQMHZ) && (RADIO_FREQMHZ <= 2500));
  assert((0 <= RADIO_PACKET_LENLEN) && (RADIO_PACKET_LENLEN <= 0xF));
  assert((0 <= RADIO_PACKET_S0LEN) && (RADIO_PACKET_S0LEN <= 1));
  assert((0 <= RADIO_PACKET_S1LEN) && (RADIO_PACKET_S1LEN <= 0xF));
  assert((0 <= RADIO_PACKET_MAXLEN) && (RADIO_PACKET_MAXLEN <= 0xFF));
  assert((2 <= RADIO_PACKET_ADRSBASELEN) && (RADIO_PACKET_ADRSBASELEN <= 4));
  assert((0 <= RADIO_PACKET_ADRSPREF) && (RADIO_PACKET_ADRSPREF <= 0xFF));
  assert((0 <= RADIO_DFE_LEN8US) && (RADIO_DFE_LEN8US <= 0x3F));
  assert((0 <= RADIO_DFE_INEXTENSION) && (RADIO_DFE_INEXTENSION <= 1));
  assert((1 <= RADIO_DFE_TSWITCHSPACING) && (RADIO_DFE_TSWITCHSPACING <= 3));
  assert((1 <= RADIO_DFE_TSAMPLEREF) && (RADIO_DFE_TSAMPLEREF <= 6));
  assert((0 <= RADIO_DFE_SAMPLETYPE) && (RADIO_DFE_SAMPLETYPE <= 1));
  assert((1 <= RADIO_DFE_TSAMPLE) && (RADIO_DFE_TSAMPLE <= 6));
  assert((0 <= RADIO_DFE_REPEATPATTERN) && (RADIO_DFE_REPEATPATTERN <= 0xF));
  NRF_RADIO->INTENCLR = 0xFFFFFFFF;
  NRF_RADIO->PACKETPTR = 0;
  NRF_RADIO->FREQUENCY = RADIO_FREQMHZ-2400;
  NRF_RADIO->TXPOWER = RADIO_TXPOWER;
  NRF_RADIO->MODE = RADIO_MODE;
  NRF_RADIO->PCNF0 = 0;
  NRF_RADIO->PCNF0 |= RADIO_PACKET_LENLEN << 0;
  NRF_RADIO->PCNF0 |= RADIO_PACKET_S0LEN << 8;
  NRF_RADIO->PCNF0 |= RADIO_PACKET_S1LEN << 16;
  NRF_RADIO->PCNF0 |= RADIO_PACKET_PREAMBLELEN << 24;
  NRF_RADIO->PCNF1 = 0;
  NRF_RADIO->PCNF1 |= RADIO_PACKET_MAXLEN << 0;
  NRF_RADIO->PCNF1 |= RADIO_PACKET_PLSLEN << 8;
  NRF_RADIO->PCNF1 |= RADIO_PACKET_ADRSBASELEN << 16;
  NRF_RADIO->BASE0 = RADIO_PACKET_ADRSBASE;
  NRF_RADIO->PREFIX0 = RADIO_PACKET_ADRSPREF;
  NRF_RADIO->PREFIX1 = 0;
  NRF_RADIO->TXADDRESS = 0;
  NRF_RADIO->RXADDRESSES = 1;
  // Copied CRC config code
  NRF_RADIO->CRCCNF = RADIO_CRCCNF_LEN_Two << RADIO_CRCCNF_LEN_Pos;
  if ((NRF_RADIO->CRCCNF & RADIO_CRCCNF_LEN_Msk) == (RADIO_CRCCNF_LEN_Two << RADIO_CRCCNF_LEN_Pos))
  {
      NRF_RADIO->CRCINIT = 0xFFFFUL;   // Initial value
      NRF_RADIO->CRCPOLY = 0x11021UL;  // CRC poly: x^16 + x^12^x^5 + 1
  }
  else if ((NRF_RADIO->CRCCNF & RADIO_CRCCNF_LEN_Msk) == (RADIO_CRCCNF_LEN_One << RADIO_CRCCNF_LEN_Pos))
  {
      NRF_RADIO->CRCINIT = 0xFFUL;   // Initial value
      NRF_RADIO->CRCPOLY = 0x107UL;  // CRC poly: x^8 + x^2^x^1 + 1
  }
  // Switching pattern configuration
  /*
  NRF_RADIO->PSEL.DFEGPIO[0] = 11;
  NRF_RADIO->PSEL.DFEGPIO[1] = 12;
  NRF_RADIO->PSEL.DFEGPIO[2] = 13;
  NRF_RADIO->SWITCHPATTERN = 1;
  NRF_RADIO->SWITCHPATTERN = 2;
  NRF_RADIO->SWITCHPATTERN = 4;*/

  //

  NRF_RADIO->MODECNF0 = 0;
  NRF_RADIO->DFEMODE = RADIO_DFE_MODE;
  NRF_RADIO->CTEINLINECONF = 0;
  NRF_RADIO->DFECTRL1 = 0;
  NRF_RADIO->DFECTRL1 |= RADIO_DFE_LEN8US << 0;
  NRF_RADIO->DFECTRL1 |= RADIO_DFE_INEXTENSION << 7;
  NRF_RADIO->DFECTRL1 |= RADIO_DFE_TSWITCHSPACING << 8;
  NRF_RADIO->DFECTRL1 |= RADIO_DFE_TSAMPLEREF << 12;
  NRF_RADIO->DFECTRL1 |= RADIO_DFE_SAMPLETYPE << 15;
  NRF_RADIO->DFECTRL1 |= RADIO_DFE_TSAMPLE << 16;
  NRF_RADIO->DFECTRL1 |= RADIO_DFE_REPEATPATTERN << 20;
  NRF_RADIO->DFECTRL2 = 0;
  NRF_RADIO->DFECTRL2 |= RADIO_DFE_SWITCHOFFSET << 0;
  NRF_RADIO->DFECTRL2 |= RADIO_DFE_SAMPLEOFFSET << 16;
  NRF_RADIO->DFEPACKET.PTR = (uint32_t)(&iq_data);
  NRF_RADIO->DFEPACKET.MAXCNT = RADIO_DFE_MAXCNT;
}

void RADIO_transmit_packet(const uint8_t * input, int input_len)
{
  assert(input_len <= RADIO_PACKET_MAXLEN);
  assert(NRF_RADIO->STATE == NRF_RADIO_STATE_DISABLED);
  assert(NRF_RADIO->EVENTS_READY == 0);
  assert(NRF_RADIO->EVENTS_END == 0);
  assert(NRF_RADIO->EVENTS_DISABLED == 0);
  assert(NRF_RADIO->PACKETPTR == 0);
  memcpy(tx_packet.PAYLOAD, input, input_len);
  NRF_RADIO->PACKETPTR = (uint32_t)(&tx_packet);
  NRF_RADIO->TASKS_TXEN = 1;
  assert(NRF_RADIO->STATE == NRF_RADIO_STATE_TXRU);
  while(!NRF_RADIO->EVENTS_READY);
  assert(NRF_RADIO->STATE == NRF_RADIO_STATE_TXIDLE);
  NRF_RADIO->EVENTS_READY = 0;
  NRF_RADIO->TASKS_START = 1;
  assert(NRF_RADIO->STATE == NRF_RADIO_STATE_TX);
  while(!NRF_RADIO->EVENTS_END);
  NRF_RADIO->EVENTS_END = 0;
  NRF_RADIO->TASKS_DISABLE = 1;
  assert(NRF_RADIO->STATE == NRF_RADIO_STATE_TXDISABLE);
  while(!NRF_RADIO->EVENTS_DISABLED);
  assert(NRF_RADIO->STATE == NRF_RADIO_STATE_DISABLED);
  NRF_RADIO->EVENTS_DISABLED = 0;
  NRF_RADIO->PACKETPTR = 0;
}

int RADIO_receive_packet(uint8_t * output, int output_len, uint32_t * iq_output)
{
  assert(output_len <= RADIO_PACKET_MAXLEN);
  assert(NRF_RADIO->STATE == NRF_RADIO_STATE_DISABLED);
  assert(NRF_RADIO->EVENTS_READY == 0);
  assert(NRF_RADIO->EVENTS_END == 0);
  assert(NRF_RADIO->EVENTS_DISABLED == 0);
  assert(NRF_RADIO->PACKETPTR == 0);
  NRF_RADIO->DFEMODE = 2;
  NRF_RADIO->PACKETPTR = (uint32_t)(&rx_packet);
  NRF_RADIO->TASKS_RXEN = 1;
  assert(NRF_RADIO->STATE == NRF_RADIO_STATE_RXRU);
  while(!NRF_RADIO->EVENTS_READY);
  assert(NRF_RADIO->STATE == NRF_RADIO_STATE_RXIDLE);
  NRF_RADIO->EVENTS_READY = 0;
  NRF_RADIO->TASKS_START = 1;
  assert(NRF_RADIO->STATE == NRF_RADIO_STATE_RX);
  while(!NRF_RADIO->EVENTS_END);
  bool success = (NRF_RADIO->CRCSTATUS == 1);
  NRF_RADIO->EVENTS_END = 0;
  while(NRF_RADIO->STATE != NRF_RADIO_STATE_RXIDLE);
  NRF_RADIO->TASKS_DISABLE = 1;
  assert(NRF_RADIO->STATE == NRF_RADIO_STATE_RXDISABLE);
  while(!NRF_RADIO->EVENTS_DISABLED);
  assert(NRF_RADIO->STATE == NRF_RADIO_STATE_DISABLED);
  NRF_RADIO->EVENTS_DISABLED = 0;
  NRF_RADIO->PACKETPTR = 0;
  memcpy(output, rx_packet.PAYLOAD, output_len);
  memcpy(iq_output, iq_data, NRF_RADIO->DFEPACKET.AMOUNT);
  return success*NRF_RADIO->DFEPACKET.AMOUNT;
}
