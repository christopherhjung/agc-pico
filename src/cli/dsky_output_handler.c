
#include <core/dsky.h>
#include <stdio.h>
#include <sys/fcntl.h>
#include <termios.h>
#include <unistd.h>

void dsky_output_handle()
{
  int c = getchar();
  switch(c)
  {
    case '0':
      dsky_press_key(KEY_ZERO);
      break;
    case 'V':
    case 'v':
      dsky_press_key(KEY_VERB);
      break;
    case 'N':
    case 'n':
      dsky_press_key(KEY_NOUN);
      break;
    case 'R':
    case 'r':
      dsky_press_key(KEY_RSET);
      break;
    case 'K':
    case 'k':
      dsky_press_key(KEY_KEY_REL);
      break;
    case 'P':
    case 'p':
      dsky_press_pro(0);
      break;
    case 'O':
    case 'o':
      dsky_press_pro(1);
      break;
    case 'E':
    case '\n':
      dsky_press_key(KEY_ENTER);
      break;
    case EOF:
      break;
    default:
      if('1' <= c && c <= '9')
        dsky_press_key(c - '1' + KEY_ONE);
  }
}
