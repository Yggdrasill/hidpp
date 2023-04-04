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

struct hidpp_feature {
  uint16_t  id;
  uint8_t   index;
  uint8_t   type;
};

enum HIDPP_FEATURE {
  FEAT_ROOT          = 0x0000,
  FEAT_FEATURESET    = 0x0001,
  FEAT_FWINFO        = 0x0003,
  FEAT_DEVNAMETYPE   = 0x0005,
  FEAT_BATINFO       = 0x1000,
  FEAT_KBDSPECIAL    = 0x1B00,
  FEAT_WDEVSTATUS    = 0x1D4B,
  FEAT_DFUCONTROL    = 0X00C2,
  FEAT_ADJUSTDPI     = 0X2201,
  FEAT_REPORTRATE    = 0X8060,
  FEAT_COLORLEDFX    = 0X8070,
  FEAT_DEVPROFILES   = 0X8100
};

static inline union hidpp_msg
msg_init(uint8_t type, uint8_t devidx, uint8_t cmd, uint8_t fcnt)
{
  return (union hidpp_msg) {
    .data = { type, devidx, cmd, fcnt << 4 | MSG_SWID, 0 }
  };
}

ssize_t msg_read(int fd, union hidpp_msg *msg)
{
  ssize_t retval;
  int expected;

  retval = read(fd, msg, sizeof(*msg) );
  if(retval < 0) goto error;

  expected = msg->data[0] == MSG_SHORT ? MSG_SHORT_LEN : MSG_LONG_LEN;
  if(retval != expected) retval = -2;
  else if(msg->data[3] == 0x8F) retval = -3;

error:
  return retval;
}

ssize_t msg_send(int fd, union hidpp_msg *msg)
{
  ssize_t retval;
  int bytes;

  bytes = msg->data[0] == MSG_SHORT ? MSG_SHORT_LEN : MSG_LONG_LEN;
  retval = write(fd, msg, bytes);

  return retval;
}

struct hidpp_feature
cmd_get_index(const int fd, const uint8_t devidx, const uint16_t id)
{
  union hidpp_msg msg;

  /* feature index 0 is always root feature, call featureIndex() */
  msg = msg_init(MSG_SHORT, devidx, 0x00, 0x00);
  /* break up function id into big-endian bytes, assumes LE host machine */
  msg.hidpp.args[0] = ( (char *)&id)[1];
  msg.hidpp.args[1] = ( (char *)&id)[0];

  /* TODO: handle errors */
  msg_send(fd, &msg);
  msg_read(fd, &msg);

  return (struct hidpp_feature) { id, msg.data[4], msg.data[5] };
}

struct hidpp_feature
cmd_get_id(const int fd, const uint8_t devidx,
          const uint8_t index, const uint8_t query)
{
  union hidpp_msg msg;

  /* call GetFeatureID() from IFeatureSet */
  msg = msg_init(MSG_SHORT, devidx, index, 0x01);
  msg.hidpp.args[0] = query;
  msg_send(fd, &msg);
  msg_read(fd, &msg);

  return (struct hidpp_feature) {
    msg.data[5] | msg.data[4] << 8,
    query,
    msg.data[6]
  };
}

char *cmd_get_name(uint16_t id)
{
  switch(id) {
    case FEAT_ROOT:         return "IRoot";
    case FEAT_FEATURESET:   return "IFeatureSet";
    case FEAT_FWINFO:       return "IFirmwareInfo";
    case FEAT_DEVNAMETYPE:  return "GetDeviceNameType";
    case FEAT_BATINFO:      return "BatteryLevelStatus";
    case FEAT_KBDSPECIAL:   return "SpecialKeysMSEButtons";
    case FEAT_WDEVSTATUS:   return "WirelessDeviceStatus";
    case FEAT_DFUCONTROL:   return "DfuControlSigned";
    case FEAT_ADJUSTDPI:    return "AdjustableDpi";
    case FEAT_REPORTRATE:   return "ReportRate";
    case FEAT_COLORLEDFX:   return "ColorLedEffects";
    case FEAT_DEVPROFILES:  return "OnboardProfiles";
  }

  return "";
}

void cmds_print(const int fd)
{
  union hidpp_msg msg;
  struct hidpp_feature feature;
  char *name;
  char *obsolete;
  char *hidden;
  char *internal;

  int n;
  int i;

  uint8_t fs_idx;

  feature = cmd_get_index(fd, 0xFF, FEAT_FEATURESET);
  /* call GetCount() from IFeatureSet */
  msg = msg_init(MSG_SHORT, 0xFF, feature.index, 0x00);
  msg_send(fd, &msg);
  msg_read(fd, &msg);
  n = msg.hidpp.args[0];
  fs_idx = feature.index;

  for(i = 0; i < n; i++) {
    feature = cmd_get_id(fd, 0xFF, fs_idx, i);
    name = cmd_get_name(feature.id);
    obsolete = feature.type & 0x80 ? " obsolete" : "";
    hidden = feature.type & 0x40 ? " hidden" : "";
    internal = feature.type & 0x20 ? " internal" : "";

    printf("%-20s index %02x [%04x]%s%s%s\n",
          name, feature.index, feature.id,
          obsolete, hidden, internal);
  }

  return;
}

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
