#include "dsky.h"
#include "dsky.h"

#include <sys/fcntl.h>
#include <termios.h>
#include <unistd.h>

#include "agc_engine.h"
#include "ringbuffer.h"
#include <string.h>

int kbhit() {
  struct termios oldt, newt;
  int ch;
  int oldf;

  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

  ch = getchar();

  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  fcntl(STDIN_FILENO, F_SETFL, oldf);

  if (ch != EOF) {
    ungetc(ch, stdin);
    return 1;
  }

  return 0;
}


void dsky_io (dsky_t *dsky)
{

  int channel;
  int value;
  while (dsky_channel_input(&channel, &value))
  {
    if (channel == 010)
    {
      if (dsky_update_digit(dsky, value))
        dsky_print(dsky);
    }else if (channel == 011){
      dsky->comp_acty = (value & 2) != 0;
    }else if (channel == 0163){
      int is_off = (value & 040);
      dsky->noun.on = is_off == 0;
      dsky->verb.on = is_off == 0;
      dsky_print(dsky);
      *((uint16_t*)&dsky->flags) = value;
    }
  }

  if (kbhit())
  {
    int c = getchar();
    switch (c)
    {
    case '0':
      dsky_keyboard_press(KEY_ZERO);
      break;
    case 'V':
    case 'v':
      dsky_keyboard_press(KEY_VERB);
      break;
    case 'N':
    case 'n':
      dsky_keyboard_press(KEY_NOUN);
      break;
    case 'E':
    case '\n':
      dsky_keyboard_press(KEY_ENTER);
      break;
    default:
      if ('1' <= c && c <= '9')
        dsky_keyboard_press(c - '1' + KEY_ONE);
    }
  }
}

void dsky_keyboard_press (Keyboard Keycode)
{
  dsky_channel_output(015, Keycode);
}

int dsky_channel_input (int* channel, int* value)
{
  packet_t packet;
  if (!ringbuffer_get(&ringbuffer_out, (unsigned char*)&packet))
    return 0;

  *channel = packet.channel;
  *value = packet.value;
  return 1;
}

int dsky_channel_output (int channel, int value)
{
  packet_t packet = {
    .channel = channel,
    .value = value
  };

  return ringbuffer_put (&ringbuffer_in, (unsigned char*)&packet);
}

int map[] = {
 10,
 10, 10,
 1,
 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
 4,
 10, 10, 10,
 7,
 10,
 0,
 10, 10, 10,
 2,
 10,
 3,
 6,
 8,
 5,
 9
};

void dsky_row_init(dsky_row_t* row)
{
  row->plus = 0;
  row->minus = 0;
  row->first = 10;
  row->second = 10;
  row->third = 10;
  row->fourth = 10;
  row->fifth = 10;
}

void dsky_two_init(dsky_two_t* two)
{
  two->on = 1;
  two->first = 10;
  two->second = 10;
}

void dsky_init(dsky_t* dsky)
{
  memset(dsky, 0, sizeof(dsky_t));
  dsky->comp_acty = 0;
  dsky_two_init(&dsky->prog);
  dsky_two_init(&dsky->verb);
  dsky_two_init(&dsky->noun);
  dsky_row_init(&dsky->rows[0]);
  dsky_row_init(&dsky->rows[1]);
  dsky_row_init(&dsky->rows[2]);
}

int dsky_update_digit(dsky_t *dsky, uint16_t value)
{
  int loc = (value >> 11) & 0x0F;
  int sign = (value >> 10) & 1;
  int left = map[(value >> 5) & 0x1F];
  int right = map[value & 0x1F];

  switch (loc)
  {
  case 1:
    dsky->rows[2].fifth = right;
    dsky->rows[2].fourth = left;
    dsky->rows[2].minus |= sign;
    break;
  case 2:
    dsky->rows[2].third = right;
    dsky->rows[2].second = left;
    dsky->rows[2].plus = sign;
    break;
  case 3:
    dsky->rows[2].first = right;
    dsky->rows[1].fifth = left;
    break;
  case 4:
    dsky->rows[1].fourth = right;
    dsky->rows[1].third = left;
    dsky->rows[1].minus = sign;
    break;
  case 5:
    dsky->rows[1].second = right;
    dsky->rows[1].first = left;
    dsky->rows[1].plus = sign;
    break;
  case 6:
    dsky->rows[0].fifth = right;
    dsky->rows[0].fourth = left;
    dsky->rows[0].minus = sign;
    break;
  case 7:
    dsky->rows[0].third = right;
    dsky->rows[0].second = left;
    dsky->rows[0].plus = sign;
    break;
  case 8:
    dsky->rows[0].first = right;
    break;
  case 9:
    dsky->noun.first = left;
    dsky->noun.second = right;
    break;
  case 10:
    dsky->verb.first = left;
    dsky->verb.second = right;
    break;
  case 11:
    dsky->prog.first = left;
    dsky->prog.second = right;
    break;
  default:
    return 0;
  }

  return 1;
}

char digit2char(unsigned int digit)
{
  if (digit >= 10) return ' ';
  return '0' + digit;
}

void dsky_row_print(dsky_row_t *row)
{
  printf("%c%c%c%c%c%c\n", row->minus ? '-' : row->plus ? '+' : ' ', digit2char(row->first), digit2char(row->second), digit2char(row->third), digit2char(row->fourth), digit2char(row->fifth));
}

void dsky_two_print(dsky_two_t *two)
{
  if (two->on)
    printf("%c%c", digit2char(two->first), digit2char(two->second));
  else
    printf("  ");
}

void dsky_print(dsky_t *dsky)
{
  printf("%d\n", *((uint16_t*)&dsky->flags));
  printf("CA  PR\n");
  printf("%s", dsky->comp_acty ? "XX" : "  ");
  printf("  ");
  dsky_two_print(&dsky->prog);
  printf("\n");
  printf("VB  NO\n");
  dsky_two_print(&dsky->verb);
  printf("  ");
  dsky_two_print(&dsky->noun);
  printf("\n");
  dsky_row_print(&dsky->rows[0]);
  dsky_row_print(&dsky->rows[1]);
  dsky_row_print(&dsky->rows[2]);
}