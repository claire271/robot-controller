// Standard header files
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
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

//Measured in ints (4 bytes), so actually offset by 0x2000
#define OFFSET_SHAREDRAM 0x800
#define OFFSET_POS_IN OFFSET_SHAREDRAM + 0
#define OFFSET_PWM_OUT OFFSET_SHAREDRAM + 3

#define PRUSS0_SHARED_DATARAM    4

void delay(int milliseconds);
void catch_SIGINT(int sig);
void cleanup();
void* pidloop(void* arg);
void* ioloop(void* arg);

static void *sharedMem;

static unsigned int *sharedMem_int;

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;

//IO variables
int pos_in = 0;

//PID variables
unsigned int pid_freq = 50;    //(hz)
unsigned int io_freq = 50;      //(hz)
unsigned int ui_freq = 20;      //(hz)
int pid_range = 200 * 500;      //The pwm output range from the center to one side (in 5ns increments)
float pid_output = 0;           //Range from -1 to 1
float pid_input = 0;
float pid_setpoint = 0;
float pid_p = 1;
float pid_d = 0;
float pid_i = 2;
float pid_oldin = 0;
float pid_iacc = 0;

//PID implementation adapted from the Arduino PID library

float kp = 1.0;
float ki = 1.0 / 50;
float kd = 0.0 * 50;

float iterm = 0;
float last_in = 0;
float qmin = -1;
float qmax = 1;

//Testing variables

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

  //Setting up ncurses
  initscr();			/* Start curses mode 		  */
  start_color();			/* Start color 			*/
  init_pair(1, COLOR_CYAN, COLOR_BLACK);
  init_pair(2, COLOR_GREEN, COLOR_BLACK);

  for(;;) {
    //Print out current count
    pthread_mutex_lock( &mutex1 );
    attron(COLOR_PAIR(1));
    mvprintw(0,0,"POS IN:  %i\n",pos_in);
    attron(COLOR_PAIR(2));
    mvprintw(1,0,"PID OUT: %f\n",pid_output);
    attroff(COLOR_PAIR(1));
    pthread_mutex_unlock( &mutex1 );

    refresh();
    //Timer loop code
    gettimeofday(&tv,NULL);
    unsigned long cur_time = 1000000 * tv.tv_sec + tv.tv_usec;
    if(old_time + 1000000L/ui_freq > cur_time) {
      usleep(old_time + 1000000/ui_freq - cur_time);
    }
    old_time += 1000000/ui_freq;
  }
  return 0;
}

void cleanup() {
  //Disable PRU and close memory mapping
  prussdrv_pru_disable(PRU_NUM); 
  prussdrv_exit ();

  //Cleanup ncurses
  endwin();
}

void* pidloop(void *arg) {
  //Setting up timer stuff
  struct timeval tv;
  gettimeofday(&tv,NULL);
  unsigned long old_time = 1000000 * tv.tv_sec + tv.tv_usec;

  //Other variables

  for(;;) {
    pthread_mutex_lock( &mutex1 );
    pid_input = pos_in / 10000.f;

    float error = pid_setpoint - pid_input;

    iterm += ki * error;
    if(iterm > qmax) iterm = qmax;
    if(iterm < qmin) iterm = qmin;

    float dinput = pid_input - last_in;

    pid_output = kp * error + iterm - kd * dinput;

    if(pid_output > qmax) pid_output = qmax;
    if(pid_output < qmin) pid_output = qmin;

    last_in = pid_input;

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

  //Initializing pwm output
  sharedMem_int[OFFSET_PWM_OUT] = 200 * 1500;

  for(;;) {
    pthread_mutex_lock( &mutex1 );
    pos_in = sharedMem_int[OFFSET_POS_IN];
    sharedMem_int[OFFSET_PWM_OUT] = 1500 * 200 + pid_output * pid_range;
    pthread_mutex_unlock( &mutex1 );

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

