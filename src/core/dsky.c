#include "core/dsky.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "agc_engine.h"
#include "agc_simulator.h"
#include "core/ringbuffer.h"

double imu_angle[3] = {0};
double pimu[3]      = {0};

#define DEG_TO_RAD (M_PI / 180)
#define RAD_TO_DEG (180 / M_PI)
#define CA_ANGLE (0.043948 * DEG_TO_RAD)
#define FA_ANGLE (0.617981 / 3600.0 * DEG_TO_RAD)
#define ANGLE_INCR (360.0 / 32768 * DEG_TO_RAD)
#define PIPA_INCR (0.0585) // m/s per each PIPA pulse

double adjust(double x, double a, double b)
{
  return x - (b - a) * floor((x - a) / (b - a));
}

void modify_gimbal_angle(uint16_t axis, double delta)
{
  // ---- Calculate New Angle ----
  imu_angle[axis] = adjust(imu_angle[axis] + delta, 0, 2 * M_PI);

  // ---- Calculate Delta between the new Angle and already feeded IMU Angle ----
  double dx = adjust(imu_angle[axis] - pimu[axis], -M_PI, M_PI);

  // ---- Feed yaAGC with the new Angular Delta ----
  double sign = dx > 0 ? +1 : -1;
  double n    = floor(fabs(dx) / ANGLE_INCR);
  pimu[axis] = adjust(pimu[axis] + sign * ANGLE_INCR * n, 0, 2 * M_PI);

  uint16_t cdu = Simulator.state.erasable[0][26 + axis]; // read CDU counter (26 = 0x32 = CDUX)
  cdu          = cdu & 0x4000 ? -(cdu ^ 0x7FFF) : cdu; // converts from ones-complement to twos-complement
  cdu += sign * n; // adds the number of pulses
  Simulator.state.erasable[0][26 + axis] =
    cdu < 0 ? (-cdu) ^ 0x7FFF : cdu; // converts back to ones-complement and writes the counter
}

int16_t from_int15(uint16_t val) {
  return val & 0x4000 ? -(val & 0x3FFF) : val & 0x3FFF;
}

void gyro_fine_align(uint16_t chan, uint16_t val)
{
  uint16_t gyro_sign_minus  = val & 0x4000;
  uint16_t gyro_selection_a = val & 0x2000;
  uint16_t gyro_selection_b = val & 0x1000;
  uint16_t gyro_selection = (val >> 12) & 0x3;
  uint16_t gyro_enable      = val & 0x0800;
  int16_t sign             = gyro_sign_minus ? -1 : +1;
  int16_t gyro_pulses      = sign * (val & 0x07FF);

  if(!gyro_selection_a && gyro_selection_b)
  {
    modify_gimbal_angle(0, gyro_pulses * FA_ANGLE);
  }
  if(gyro_selection_a && !gyro_selection_b)
  {
    modify_gimbal_angle(1, gyro_pulses * FA_ANGLE);
  }
  if(gyro_selection_a && gyro_selection_b)
  {
    modify_gimbal_angle(2, gyro_pulses * FA_ANGLE);
  }
}

void gyro_coarse_align(uint16_t chan, uint16_t val){
  int16_t cdu_pulses = from_int15(val);
  modify_gimbal_angle(chan - 124, cdu_pulses * CA_ANGLE);
}

void rotate(double delta[3])
{
  // based on Transform_BodyAxes_StableMember {dp dq dr}

  double MPI    = sin(imu_angle[2]);
  double MQI    = cos(imu_angle[2]) * cos(imu_angle[0]);
  double MQM    = sin(imu_angle[0]);
  double MRI    = -cos(imu_angle[2]) * sin(imu_angle[0]);
  double MRM    = cos(imu_angle[0]);
  double nenner = MRM * MQI - MRI * MQM;

  //---- Calculate Angular Change ----
  double do_b = adjust(
    delta[0] - (delta[1] * MRM * MPI - delta[2] * MQM * MPI) / nenner, -M_PI, M_PI);
  double di_b = adjust((delta[1] * MRM - delta[2] * MQM) / nenner, -M_PI, M_PI);
  double dm_b = adjust((delta[2] * MQI - delta[1] * MRI) / nenner, -M_PI, M_PI);

  //--- Rad to Deg and call of Gimbal Angle Modification ----
  modify_gimbal_angle(0, do_b);
  modify_gimbal_angle(1, di_b);
  modify_gimbal_angle(3, dm_b);
}

double velocity[3] = {0};
double pipa[3]     = {0};
//************************************************************************************************
//*** Function: Modify PIPA Values to match simulated Speed                                   ****
//************************************************************************************************
void accelerate(double delta[3])
{
  // based on proc modify_pipaXYZ
  double sinOG = sin(imu_angle[0]);
  double sinIG = sin(imu_angle[1]);
  double sinMG = sin(imu_angle[2]);
  double cosOG = cos(imu_angle[0]);
  double cosIG = cos(imu_angle[1]);
  double cosMG = cos(imu_angle[2]);

  double dv[] = {
    cosMG * cosIG * delta[0] + (-cosOG * sinMG * cosIG + sinOG * sinIG) * delta[1]
      + (sinOG * sinMG * cosIG + cosOG * sinIG) * delta[2],
    sinMG * delta[0] + cosOG * cosMG * delta[1] - sinOG * cosMG * delta[2],
    -cosMG * sinIG * delta[0] + (cosOG * sinMG * sinIG + sinOG * cosIG) * delta[1]
      + (-sinOG * sinMG * sinIG + cosOG * cosIG) * delta[2]
  };

  for(int axis = 0; axis < 3; axis++)
  {
    velocity[axis] += dv[axis];
    uint16_t counts = floor((velocity[axis] - pipa[axis] * PIPA_INCR) / PIPA_INCR);

    pipa[axis] += counts;

    uint16_t p = Simulator.state.erasable[0][31 + axis]; // read PIPA counter (31 = 0x37 = PIPAX)
    p     = p & 0x4000 ? -(p ^ 0x7FFF) : p; // converts from ones-complement to twos-complement
    p += counts; // adds the number of pulses
    Simulator.state.erasable[0][31 + axis] = p < 0 ? (-p) ^ 0x7FFF : p;
  }
}

void dsky_input_handle(dsky_t* dsky)
{
  uint16_t channel;
  uint16_t value;
  while(dsky_channel_input(&channel, &value))
  {
    if(channel == 010)
    {
      if(dsky_update_digit(dsky, value))
        dsky_print(dsky);
    }
    else if(channel == 011)
    {
      dsky->comp_acty = (value & 2) != 0;
      dsky_print(dsky);
    }
    else if(channel == 0163)
    {
      int is_off    = (value & 040);
      dsky->noun.on = is_off == 0;
      dsky->verb.on = is_off == 0;
      dsky_print(dsky);
      *((uint16_t*)&dsky->flags) = value;
    }
    else if(channel == 124 || channel == 125 || channel == 126)
    {
      gyro_coarse_align(channel, value);
    }
    else if(channel == 127)
    {
      gyro_fine_align(channel, value);
    }
  }
}

void dsky_keyboard_press(Keyboard Keycode)
{
  dsky_channel_output(015, Keycode);
}

int dsky_channel_input(uint16_t* channel, uint16_t* value)
{
  packet_t packet;
  if(!ringbuffer_get(&ringbuffer_out, (unsigned char*)&packet))
    return 0;

  *channel = packet.channel;
  *value   = packet.value;
  return 1;
}

int dsky_channel_output(uint16_t channel, uint16_t value)
{
  packet_t packet = {.channel = channel, .value = value};

  return ringbuffer_put(&ringbuffer_in, (unsigned char*)&packet);
}

int map[] = {10, 10, 10, 1,  10, 10, 10, 10, 10, 10, 10,
             10, 10, 10, 10, 4,  10, 10, 10, 7,  10, 0,
             10, 10, 10, 2,  10, 3,  6,  8,  5,  9};

void dsky_row_init(dsky_row_t* row)
{
  row->plus   = 0;
  row->minus  = 0;
  row->first  = 10;
  row->second = 10;
  row->third  = 10;
  row->fourth = 10;
  row->fifth  = 10;
}

void dsky_two_init(dsky_two_t* two)
{
  two->on     = 1;
  two->first  = 10;
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

int dsky_update_digit(dsky_t* dsky, uint16_t value)
{
  int loc   = (value >> 11) & 0x0F;
  int sign  = (value >> 10) & 1;
  int left  = map[(value >> 5) & 0x1F];
  int right = map[value & 0x1F];

  switch(loc)
  {
    case 1:
      dsky->rows[2].fifth  = right;
      dsky->rows[2].fourth = left;
      dsky->rows[2].minus |= sign;
      break;
    case 2:
      dsky->rows[2].third  = right;
      dsky->rows[2].second = left;
      dsky->rows[2].plus   = sign;
      break;
    case 3:
      dsky->rows[2].first = right;
      dsky->rows[1].fifth = left;
      break;
    case 4:
      dsky->rows[1].fourth = right;
      dsky->rows[1].third  = left;
      dsky->rows[1].minus  = sign;
      break;
    case 5:
      dsky->rows[1].second = right;
      dsky->rows[1].first  = left;
      dsky->rows[1].plus   = sign;
      break;
    case 6:
      dsky->rows[0].fifth  = right;
      dsky->rows[0].fourth = left;
      dsky->rows[0].minus  = sign;
      break;
    case 7:
      dsky->rows[0].third  = right;
      dsky->rows[0].second = left;
      dsky->rows[0].plus   = sign;
      break;
    case 8:
      dsky->rows[0].first = right;
      break;
    case 9:
      dsky->noun.first  = left;
      dsky->noun.second = right;
      break;
    case 10:
      dsky->verb.first  = left;
      dsky->verb.second = right;
      break;
    case 11:
      dsky->prog.first  = left;
      dsky->prog.second = right;
      break;
    default:
      return 0;
  }

  return 1;
}

char digit2char(unsigned int digit)
{
  if(digit >= 10)
    return ' ';
  return '0' + digit;
}

void dsky_row_print(dsky_row_t* row)
{
  printf(
    "%c%c%c%c%c%c\n",
    row->minus    ? '-'
      : row->plus ? '+'
                  : ' ',
    digit2char(row->first),
    digit2char(row->second),
    digit2char(row->third),
    digit2char(row->fourth),
    digit2char(row->fifth));
}

void dsky_two_print(dsky_two_t* two)
{
  if(two->on)
    printf("%c%c", digit2char(two->first), digit2char(two->second));
  else
    printf("  ");
}

void dsky_print(dsky_t* dsky)
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
