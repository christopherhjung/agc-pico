
#include <core/dsky.h>
#include <stdio.h>
#include <sys/fcntl.h>
#include <termios.h>
#include <unistd.h>

int kbhit()
{
  struct termios oldt, newt;
  int            ch;
  int            oldf;

  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

  ch = getchar();

  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  fcntl(STDIN_FILENO, F_SETFL, oldf);

  if(ch != EOF)
  {
    ungetc(ch, stdin);
    return 1;
  }

  return 0;
}

void dsky_output_handle()
{
  if(kbhit())
  {
    int c = getchar();
    switch(c)
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
        if('1' <= c && c <= '9')
          dsky_keyboard_press(c - '1' + KEY_ONE);
    }
  }
}
