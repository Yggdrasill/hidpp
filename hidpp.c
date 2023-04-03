#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#define TEST 0

#define VENDOR_ID       0x046D
#define PRODUCT_ID      0xC08F

#define MSG_SHORT       0x10
#define MSG_LONG        0x11

#define MSG_SHORT_LEN   7
#define MSG_LONG_LEN    20

#define MSG_SWID        0x0A

struct _hidpp_msg {
  uint8_t type;
  uint8_t devidx;
  uint8_t cmd;
  uint8_t fcnt;
  uint8_t args[16];
};

union hidpp_msg {
  struct _hidpp_msg hidpp;
  uint8_t data[20];
};

int main(void)
{
  union hidpp_msg msg = { 0 };
  union hidpp_msg response = { 0 };
  int fd;
  int bytes;
  int features;

#if TEST
  msg.hidpp.type = 0x11;    // long message
  msg.hidpp.devidx = 0xFF;   // wired mouse or transceiver, otherwise 1-6
  msg.hidpp.cmd = 0x0E;     // LED control
  msg.hidpp.fcnt = 0x3E;    // Byte sent by G Hub for solid LED colours
  msg.hidpp.args[0] = 0x00; // LED 0
  msg.hidpp.args[1] = 0x01; // mode, solid color
  /* next three lines are colours */
  msg.hidpp.args[2] = 0xFF;
  msg.hidpp.args[3] = 0x00;
  msg.hidpp.args[4] = 0x00;
  msg.hidpp.args[5] = 0x02; // G Hub sends this, for what reason I do not know.
#else
  msg.hidpp.type = 0x10;
  msg.hidpp.devidx = 0xFF;
  msg.hidpp.cmd = 0x00;
  msg.hidpp.fcnt = 0x0E;
  msg.hidpp.args[0] = 0x00;
  msg.hidpp.args[1] = 0x01;
#endif

  fd = open("/dev/hidraw1", O_RDWR);

  write(fd, &msg, sizeof(msg) );

#if TEST
  msg.data[4] = 0x01;       // LED 1
  write(fd, &msg, sizeof(msg) );
#endif

  bytes = read(fd, &response, sizeof(response) );

  for(int i = 0; i < bytes; i++) {
    printf("%02x ", response.data[i]);
  }
  puts("");

  msg.hidpp.cmd = response.hidpp.args[0];
  write(fd, &msg, sizeof(msg) );
  bytes = read(fd, &response, sizeof(response) );
  features = response.hidpp.args[0];

  for(int i = 0; i < bytes; i++) {
    printf("%02x ", response.data[i]);
  }
  printf("\n-----------------------------------------------------------\n");

  msg.hidpp.fcnt |= (1 << 4);
  for(int i = 0; i < features; i++) {
    msg.hidpp.args[0] = i;
    write(fd, &msg, sizeof(msg) );
    bytes = read(fd, &response, sizeof(response) );
    for(int j = 0; j < bytes; j++) {
      printf("%02x ", response.data[j]);
    }
    puts("");
  }

  return 0;
}
