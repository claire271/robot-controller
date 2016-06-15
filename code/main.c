// Standard header files
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <stdlib.h>
#include <ncurses.h>
#include <pthread.h>

// Driver header file
#include <pruss/prussdrv.h>
#include <pruss/pruss_intc_mapping.h>	 

#define PRU_NUM 0
#define PORT "../server/serial1" 

//Measured in ints (4 bytes), so actually offset by 0x2000
#define OFFSET_SHAREDRAM 0x800
#define OFFSET_POS_IN OFFSET_SHAREDRAM + 0
#define OFFSET_PWM0_OUT OFFSET_SHAREDRAM + 4
#define OFFSET_PWM1_OUT OFFSET_SHAREDRAM + 5
#define OFFSET_PWM2_OUT OFFSET_SHAREDRAM + 6 
#define OFFSET_PWM3_OUT OFFSET_SHAREDRAM + 7
#define OFFSET_LEFT_IN OFFSET_SHAREDRAM + 9
#define OFFSET_RIGHT_IN OFFSET_SHAREDRAM + 8

#define PRUSS0_SHARED_DATARAM    4

void delay(int milliseconds);
void catch_SIGINT(int sig);
void cleanup();
void* pidloop(void* arg);
void* ioloop(void* arg);
void setPID(float Kp, float Ki, float Kd);

static void *sharedMem;

static unsigned int *sharedMem_int;

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;

//UI variables
unsigned int ui_freq = 50;      //(hz)

//IO variables
unsigned int io_freq = 50;      //(hz)
int pos_in = 0;
int pid_range = 200 * 500;      //The pwm output range from the center to one side (in 5ns increments)

float left_out = 0;
float right_out = 0;
int left_in = 0;
int right_in = 0;

//PID variables
//PID implementation adapted from the Arduino PID library
unsigned int pid_freq = 50;    //(hz)

float pid_output = 0;
float pid_input = 0;
float pid_setpoint = 0;
float disp_setpoint = 0;
float pid_auto_sp1 = 0;
float pid_auto_sp2 = 1;

float disp_kp;
float disp_ki;
float disp_kd;
float pid_kp;
float pid_ki;
float pid_kd;

float pid_iterm = 0;
float pid_prev_in = 0;
float pid_qmin = -1;
float pid_qmax = 1;

int main (void)
{
  unsigned int ret;
  tpruss_intc_initdata pruss_intc_initdata = PRUSS_INTC_INITDATA;

  signal(SIGINT, catch_SIGINT);

  /* Initialize the PRU */
  prussdrv_init ();		
    
  /* Open PRU */
  ret = prussdrv_open(PRU_EVTOUT_0);
  if (ret)
    {
      printf("prussdrv_open open failed\n");
      return (ret);
    }
    
  // Excecute the PRU program
  prussdrv_exec_program (PRU_NUM, "./pru.bin");

  //Setting up threads
  pthread_t pidthread;
  pthread_create(&pidthread, NULL, &pidloop, NULL);

  pthread_t iothread;
  pthread_create(&iothread, NULL, &ioloop, NULL);

  //Setting up timer stuff
  struct timeval tv;
  gettimeofday(&tv,NULL);
  unsigned long old_time = 1000000 * tv.tv_sec + tv.tv_usec;

  //Setting up serial output
  struct termios tio;
  int tty_fd;
  
  memset(&tio,0,sizeof(tio));
  tio.c_iflag=0;
  tio.c_oflag=0;
  tio.c_cflag=CS8|CREAD|CLOCAL;           // 8n1, see termios.h for more information
  tio.c_lflag=0;
  tio.c_cc[VMIN]=1;
  tio.c_cc[VTIME]=5;
  
  tty_fd=open(PORT, O_RDWR | O_NONBLOCK);      
  cfsetospeed(&tio,B115200);            // 115200 baud
  cfsetispeed(&tio,B115200);            // 115200 baud
  
  tcsetattr(tty_fd,TCSANOW,&tio);

  //Setting up ncurses
  //initscr();			/* Start curses mode 		  */
  //start_color();			/* Start color 			*/
  /*
  init_pair(1, COLOR_CYAN, COLOR_BLACK);
  init_pair(2, COLOR_GREEN, COLOR_BLACK);
  init_pair(3, COLOR_WHITE, COLOR_BLACK);
  init_pair(4, COLOR_WHITE, COLOR_BLUE);
  init_pair(5, COLOR_BLUE, COLOR_WHITE);
  init_pair(6, COLOR_RED, COLOR_WHITE);
  cbreak();
  noecho();
  nodelay(stdscr,TRUE);
  keypad(stdscr,TRUE);

  //UI Variables
  int focus = 0;
  int editing = 0;
  float tmp_edit = 0;
  */

  printf("INIT Finished\n");

  unsigned char prev = 0x00;
  int index = 0;
  unsigned short values[2];
  for(;;) {
    unsigned char curr = 0x00;
    int count = read(tty_fd,&curr,1);

    //Looking for timeout
    gettimeofday(&tv,NULL);
    unsigned long cur_time = 1000000 * tv.tv_sec + tv.tv_usec;
    if(cur_time - old_time > 100000) {
      pthread_mutex_lock( &mutex2 );
      left_out = 0.0f;
      right_out = 0.0f;
      pthread_mutex_unlock( &mutex2 );
    }
    if(count <= 0) continue;
    //curr should be valid now

    //Restart frame if end found
    if(prev == 0xFF && curr == 0xFF) {
      //Restart timeout timer
      old_time = cur_time;

      prev = 0x00;
      index = 0;
      //printf(".");
      //fflush(stdout);
      pthread_mutex_lock( &mutex2 );
      left_out = ((int)(values[0]) - 0x8000)/1000.0f;
      right_out = ((int)(values[1]) - 0x8000)/1000.0f;
      //printf("%f %f ",left_out,righo_out);
      pthread_mutex_unlock( &mutex2 );
      continue;
    }

    //Normal processing
    if(index % 2) {
      unsigned short value = curr << 8 | prev;
      values[index >> 1] = value;
    }
    index++;

    //Prepare for next cycle
    prev = curr;

    //Debug output
    printf("%i %i\n",left_in,right_in);

    /*
    //Reading input
    int c = getch();
    switch(c) {
    case '\n':
      editing = !editing;
      if(!editing) {
        setPID(disp_kp,disp_ki,disp_kd);
        pid_setpoint = disp_setpoint;
      }
      else {
        tmp_edit = 0;
      }
      break;
    case KEY_UP:
      focus--;
      break;
    case KEY_DOWN:
      focus++;
      break;
    case 127:
      tmp_edit /= 10;
      break;
    }
    if(c >= '0' && c <= '9') {
      tmp_edit = tmp_edit * 10 + (c - '0');
    }
    if(editing) {
      switch(focus) {
      case 0:
        disp_kp = tmp_edit;
        break;
      case 1:
        disp_ki = tmp_edit;
        break;
      case 2:
        disp_kd = tmp_edit;
        break;
      case 3:
        disp_setpoint = tmp_edit;
        break;
      }
    }

    //Displaying output
    pthread_mutex_lock( &mutex1 );
    clear();
    attron(COLOR_PAIR(3));
    mvprintw(0,0,"P");
    mvprintw(1,0,"I");
    mvprintw(2,0,"D");
    mvprintw(3,0,"SETPOINT");
    mvprintw(8,0,"PID AUTOTUNE SETPOINT 1");
    mvprintw(9,0,"PID AUTOTUNE SETPOINT 2");

    if(focus == 0) {
      if(editing) attron(COLOR_PAIR(6));
      else attron(COLOR_PAIR(5));
    }
    else attron(COLOR_PAIR(4));
    mvprintw(0,2,"%f",disp_kp);
    if(focus == 1) {
      if(editing) attron(COLOR_PAIR(6));
      else attron(COLOR_PAIR(5));
    }
    else attron(COLOR_PAIR(4));
    mvprintw(1,2,"%f",disp_ki);
    if(focus == 2) {
      if(editing) attron(COLOR_PAIR(6));
      else attron(COLOR_PAIR(5));
    }
    else attron(COLOR_PAIR(4));
    mvprintw(2,2,"%f",disp_kd);
    if(focus == 3) {
      if(editing) attron(COLOR_PAIR(6));
      else attron(COLOR_PAIR(5));
    }
    else attron(COLOR_PAIR(4));
    mvprintw(3,9,"%f",disp_setpoint);
    if(focus == 4) {
      if(editing) attron(COLOR_PAIR(6));
      else attron(COLOR_PAIR(5));
    }
    else attron(COLOR_PAIR(4));
    mvprintw(3,9,"%f",disp_setpoint);
    
    attron(COLOR_PAIR(1));
    //mvprintw(5,0,"PID ERR: %f\n",pid_setpoint - pos_in/10000.f);
    mvprintw(5,0,"POS IN: %i\n",pos_in);
    attron(COLOR_PAIR(2));
    mvprintw(6,0,"PID OUT: %f\n",pid_output);
    //attroff(COLOR_PAIR(1));

    

    pthread_mutex_unlock( &mutex1 );

    refresh();
    */
    /*
    //Timer loop code
    gettimeofday(&tv,NULL);
    unsigned long cur_time = 1000000 * tv.tv_sec + tv.tv_usec;
    if(old_time + 1000000L/ui_freq > cur_time) {
      usleep(old_time + 1000000/ui_freq - cur_time);
    }
    old_time += 1000000/ui_freq;
    */
  }
  return 0;
}

void cleanup() {
  //Disable PRU and close memory mapping
  prussdrv_pru_disable(PRU_NUM); 
  prussdrv_exit ();

  //Cleanup ncurses
  //endwin();
}

void* pidloop(void *arg) {
  //Setting up timer stuff
  struct timeval tv;
  gettimeofday(&tv,NULL);
  unsigned long old_time = 1000000 * tv.tv_sec + tv.tv_usec;

  //Other variables

  //Initializing PID
  setPID(1,2,0);

  for(;;) {
    pthread_mutex_lock( &mutex1 );
    pid_input = pos_in / 10000.f;

    float error = pid_setpoint - pid_input;

    pid_iterm += pid_ki * error;
    if(pid_iterm > pid_qmax) pid_iterm = pid_qmax;
    if(pid_iterm < pid_qmin) pid_iterm = pid_qmin;

    float dinput = pid_input - pid_prev_in;

    pid_output = pid_kp * error + pid_iterm - pid_kd * dinput;

    if(pid_output > pid_qmax) pid_output = pid_qmax;
    if(pid_output < pid_qmin) pid_output = pid_qmin;

    pid_prev_in = pid_input;

    pthread_mutex_unlock( &mutex1 );

    //Timer loop code
    gettimeofday(&tv,NULL);
    unsigned long cur_time = 1000000 * tv.tv_sec + tv.tv_usec;
    if(old_time + 1000000L/pid_freq > cur_time) {
      usleep(old_time + 1000000/pid_freq - cur_time);
    }
    old_time += 1000000/pid_freq;
  }
  return 0;
}

void* ioloop(void *arg) {
  //Setting up timer stuff
  struct timeval tv;
  gettimeofday(&tv,NULL);
  unsigned long old_time = 1000000 * tv.tv_sec + tv.tv_usec;

  /* Allocate Shared PRU memory. */
  prussdrv_map_prumem(PRUSS0_SHARED_DATARAM, &sharedMem);
  sharedMem_int = (unsigned int*) sharedMem;

  //Initializing pwm values
  sharedMem_int[OFFSET_PWM0_OUT] = 0;
  sharedMem_int[OFFSET_PWM1_OUT] = 0;
  sharedMem_int[OFFSET_PWM2_OUT] = 0;
  sharedMem_int[OFFSET_PWM3_OUT] = 0;

  //Initializing encoders
  sharedMem_int[OFFSET_LEFT_IN] = 0;
  sharedMem_int[OFFSET_RIGHT_IN] = 0;

  for(;;) {
    //pthread_mutex_lock( &mutex1 );
    //pos_in = sharedMem_int[OFFSET_POS_IN];
    //sharedMem_int[OFFSET_PWM0_OUT] = 1000/5 * 500;
    //pthread_mutex_unlock( &mutex1 );

    pthread_mutex_lock( &mutex2 );
    //OUTPUT
    //Limiting range from -1 to 1
    if(left_out > 1) left_out = 1;
    if(left_out < -1) left_out = -1;
    if(left_out > 0) {
      sharedMem_int[OFFSET_PWM0_OUT] = 1000/5 * 1000 * left_out;
      sharedMem_int[OFFSET_PWM1_OUT] = 0;
    }
    else {
      sharedMem_int[OFFSET_PWM0_OUT] = 0;
      sharedMem_int[OFFSET_PWM1_OUT] = 1000/5 * 1000 * -left_out;
    }
    if(right_out > 1) right_out = 1;
    if(right_out < -1) right_out = -1;
    if(right_out > 0) {
      sharedMem_int[OFFSET_PWM2_OUT] = 1000/5 * 1000 * right_out;
      sharedMem_int[OFFSET_PWM3_OUT] = 0;
    }
    else {
      sharedMem_int[OFFSET_PWM2_OUT] = 0;
      sharedMem_int[OFFSET_PWM3_OUT] = 1000/5 * 1000 * -right_out;
    }
    //INPUT
    left_in = sharedMem_int[OFFSET_LEFT_IN];
    right_in = -sharedMem_int[OFFSET_RIGHT_IN];
    pthread_mutex_unlock( &mutex2 );

    //Timer loop code
    gettimeofday(&tv,NULL);
    unsigned long cur_time = 1000000 * tv.tv_sec + tv.tv_usec;
    if(old_time + 1000000L/io_freq > cur_time) {
      usleep(old_time + 1000000/io_freq - cur_time);
    }
    old_time += 1000000/io_freq;
  }
  return 0;
}

void delay(int milliseconds)
{
  long pause;
  clock_t now,then;

  pause = milliseconds*(CLOCKS_PER_SEC/1000);
  now = then = clock();
  while( (now-then) < pause )
    now = clock();
}

void catch_SIGINT(int sig) {
  cleanup();
  exit(0);
}

void setPID(float Kp, float Ki, float Kd) {
  disp_kp = Kp;
  disp_ki = Ki;
  disp_kd = Kd;

  pid_kp = Kp;
  pid_ki = Ki / pid_freq;
  pid_kd = Kd * pid_freq;
}
