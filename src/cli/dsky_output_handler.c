
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
      default:
        if('1' <= c && c <= '9')
          dsky_press_key(c - '1' + KEY_ONE);
    }
  }
}
