#include <ctype.h>
#include <ncurses.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <time.h>
#include <sys/ioctl.h>
#include <getopt.h>
#include "stats.h"
#include "shmmap.h"
#include "screen.h"

int screen_is_active;
static int detailed;
static int scroll_start; 
static int scroll_delta;

static char status_bar[256];
static char sortbykey[32];

char filter_by[32];
double refresh_interval = 3;

static double status_bar_time;
static int top_color_pair = CP_BLACK;

static void screen_refresh_cb(EV_P_ int LINES, int COLS);
static void screen_key_cb(EV_P_ int);

static struct ev_timer refresh_timer_w;

static struct ev_io stdin_io_w;
static struct ev_signal sigint_w;
static struct ev_signal sigterm_w;
static struct ev_signal sigwinch_w;

static unsigned long bkgd_attr[2];

static void refresh_timer_cb(EV_P_ struct ev_timer *w, int revents);
static void stdin_io_cb(EV_P_ struct ev_io *w, int revents);
static void sigint_cb(EV_P_ ev_signal *w, int revents);
static void sigwinch_cb(EV_P_ ev_signal *w, int revents);

json_object *data_map;

#define MIN(x, y) ({                            \
  typeof(x) _min1 = (x);                    \
  typeof(y) _min2 = (y);                    \
  (void) (&_min1 == &_min2);                \
  _min1 < _min2 ? _min1 : _min2; })

#define MAX(x, y) ({                            \
  typeof(x) _max1 = (x);                    \
  typeof(y) _max2 = (y);                    \
  (void) (&_max1 == &_max2);                \
  _max1 > _max2 ? _max1 : _max2; })

int screen_init(double interval)
{
  ev_timer_init(&refresh_timer_w, &refresh_timer_cb, 0.001, interval);
  ev_io_init(&stdin_io_w, &stdin_io_cb, STDIN_FILENO, EV_READ);
  ev_signal_init(&sigint_w, &sigint_cb, SIGINT);
  ev_signal_init(&sigterm_w, &sigint_cb, SIGTERM);
  ev_signal_init(&sigwinch_w, &sigwinch_cb, SIGWINCH);
  return 0;
}

void screen_start(EV_P)
{
  short f[2], b[2];

  /* Begin curses magic. */
  if (initscr() == NULL) {
    fprintf(stderr, "cannot initialize screen: %m\n");
    exit(1);
  }

  cbreak();
  noecho();
  nonl();
  clear();
  intrflush(stdscr, 0);
  
  keypad(stdscr, 1);
  nodelay(stdscr, 1);

  bkgd_attr[0] = getbkgd(stdscr);
  pair_content(bkgd_attr[0], &f[0], &b[0]);

  if (!has_colors())
    fprintf(stderr, "terminal has no color capabilities\n");

  use_default_colors();
  start_color();

  init_pair(CP_BLACK, -1, -1);
  init_pair(CP_RED, COLOR_RED, -1);
  init_pair(CP_YELLOW, COLOR_YELLOW, -1);
  init_pair(CP_GREEN, COLOR_GREEN, -1);
  /* init_pair(CP_BLUE, COLOR_BLUE, -1); */
  init_pair(CP_MAGENTA, COLOR_MAGENTA, -1);
  /* init_pair(CP_CYAN, COLOR_CYAN, -1); */
  /* init_pair(CP_WHITE, COLOR_WHITE, -1); */

  bkgd(COLOR_PAIR(1));
  bkgd_attr[1] = getbkgd(stdscr);
  pair_content(bkgd_attr[1], &f[1], &b[1]);

  curs_set(0); /* Hide the cursor. */

  ev_timer_start(EV_A_ &refresh_timer_w);
  ev_io_start(EV_A_ &stdin_io_w);
  ev_signal_start(EV_A_ &sigint_w);
  ev_signal_start(EV_A_ &sigterm_w);
  ev_signal_start(EV_A_ &sigwinch_w);

  /* Sort by load by default */
  snprintf(sortbykey, sizeof(sortbykey), "load_eff");

  detailed = 0;
  screen_is_active = 1;
}

void screen_refresh(EV_P)
{
  if (!ev_is_pending(&refresh_timer_w))
    ev_feed_event(EV_A_ &refresh_timer_w, EV_TIMER);
}

void screen_stop(EV_P)
{
  ev_timer_stop(EV_A_ &refresh_timer_w);
  endwin();

  screen_is_active = 0;
}

static void refresh_timer_cb(EV_P_ ev_timer *w, int revents)
{
  get_shm_map();
  
  switch(groupby) {
  case 4:
    group_ratesbytags(5, "fid", "server", "client", "jid", "uid");
    break;
  case 3:
    group_ratesbytags(4, "fid", "server", "jid", "uid");
    break;
  case 2:
    group_ratesbytags(3, "fid", "server", "uid");
    break;
  case 1:
    group_ratesbytags(2, "fid", "server");
    break;
  case 5:
    group_ratesbytags(1, "server");
    break;
  default:
    group_ratesbytags(5, "fid", "server", "client", "jid", "uid");
    break;
  }

  screen_refresh_cb(EV_A_ LINES, COLS);
  ev_clear_pending(EV_A_ w);
  refresh();
}

static void stdin_io_cb(EV_P_ ev_io *w, int revents)
{
  int key = getch();
  if (key == ERR)
    return;


  if (screen_key_cb != NULL) {
    (*screen_key_cb)(EV_A_ key);
    return;
  }

  /* TODO Remove switch. */
  switch (key) {
  case ' ':
  case '\n':
    screen_refresh(EV_A);
    break;
  case 'q':
    ev_break(EV_A_ EVBREAK_ALL);
    break;
  default:
    fprintf(stderr, "unknown command `%c': try `h' for help\n", key);
    break;
  }
}

static void sigwinch_cb(EV_P_ ev_signal *w, int revents)
{

  struct winsize ws;
  if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) < 0) {
    fprintf(stderr, "cannot get window size: %m\n");
    return;
  }

  LINES = ws.ws_row;
  COLS = ws.ws_col;
  resizeterm(LINES, COLS);

  screen_refresh(EV_A);
}

static void sigint_cb(EV_P_ ev_signal *w, int revents)
{
  ev_break(EV_A_ EVBREAK_ALL);
}


static void screen_key_cb(EV_P_ int key)
{
  switch (tolower(key)) {
  case ' ':
    /*
  case '\n':
    ev_feed_event(EV_A_ &top_timer_w, EV_TIMER);
    break;
    */
  case 'q':
    ev_break(EV_A_ EVBREAK_ALL); /* XXX */
    return;
  case 'f':
    groupby = 1;
    break;
  case 'u':
    groupby = 2;
    break;
  case 'j':
    groupby = 3;
    break;
  case 'c':
    groupby = 4;
    break;
  case 's':
    groupby = 5;
    break;
  case 'l':
    snprintf(sortbykey, sizeof(sortbykey), "load_eff");
    break;
  case 'i':
    snprintf(sortbykey, sizeof(sortbykey), "iops");   
    break;
  case 'b':
    snprintf(sortbykey, sizeof(sortbykey), "bytes");
    break;
  case 'd':
    if (detailed) {
      detailed = 0;
    }
    else {
      detailed = 1;
    }
    break;
  case KEY_DOWN:
    scroll_delta += 1;
    break;
  case KEY_HOME:
    scroll_delta = INT_MIN / 2;
    break;
  case KEY_END:
    scroll_delta = INT_MAX / 2;
    break;
  case KEY_UP:
    scroll_delta -= 1;
    break;
  case KEY_NPAGE:
    scroll_delta += LINES;
    break;
  case KEY_PPAGE:
    scroll_delta -= LINES;
    break;
  default:
    if (isascii(key)) {
      status_bar_time = ev_now(EV_A);
      snprintf(status_bar, sizeof(status_bar), "unknown command `%c'", key);
    }
    break;
  }

  switch(groupby) {
  case 4:
    group_ratesbytags(5, "fid", "server", "client", "jid", "uid");
    break;
  case 3:
    group_ratesbytags(4, "fid", "server", "jid", "uid");
    break;
  case 2:
    group_ratesbytags(3, "fid", "server", "uid");
    break;
  case 1:
    group_ratesbytags(2, "fid", "server");
    break;
  case 5:
    group_ratesbytags(1, "server");
    break;
  default:
    group_ratesbytags(5, "fid", "server", "client", "jid", "uid");
    break;
  }

  screen_refresh(EV_A);
}

void status_bar_vprintf(EV_P_ const char *fmt, va_list args)
{
  vsnprintf(status_bar, sizeof(status_bar), fmt, args);
  status_bar_time = ev_now(EV_A);
  screen_refresh(EV_A);
}
void status_bar_printf(EV_P_ const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  status_bar_vprintf(EV_A_ fmt, args);
  va_end(args);
}


/* Sort by specified field value */
static int sort_da(const void *j1, const void *j2) {
  json_object * const *jso1, * const *jso2;
  double d1, d2;
  
  jso1 = (json_object* const*)j1;
  jso2 = (json_object* const*)j2;

  json_object *val1, *val2;
  if (!json_object_object_get_ex(*jso1, sortbykey, &val1) &&
      !json_object_object_get_ex(*jso2, sortbykey, &val2))
    return 0;
  if (!json_object_object_get_ex(*jso1, sortbykey, &val1))
      return 1;
  if (!json_object_object_get_ex(*jso2, sortbykey, &val2))
      return -1;

  d1 = json_object_get_double(val1)*100;
  d2 = json_object_get_double(val2)*100;
  
  return (int)(d2 - d1);

}

double cf = 1.0/(1024*1024);
enum json_tokener_error error = json_tokener_success;

static void screen_refresh_cb(EV_P_ int LINES, int COLS)
{
  //  return;
  time_t now = ev_now(EV_A);
  int line = 0, i;

  json_object *eid, *tid;      

  erase();

  json_object *data_array = json_object_new_array();
  json_object *events = json_object_new_object();

  json_object_object_add(events, "nclients", json_object_new_string(""));
  json_object_object_add(events, "load", json_object_new_string(""));
  json_object_object_add(events, "load_eff", json_object_new_string(""));
  json_object_object_add(events, "iops", json_object_new_string(""));
  json_object_object_add(events, "bytes", json_object_new_string(""));
  //struct timeval ts,te;
  //gettimeofday(&ts, NULL); 

  json_object_object_foreach(screen_map, s, se) {
    //printf("%s %s\n", s, json_object_to_json_string(se));
    json_object_object_foreach(se, t, te) {
      json_object *tags = NULL;
      if (strcmp(t, "time") == 0) continue;      
      
      tags = json_tokener_parse_verbose(t, &error);
      if (error != json_tokener_success) {
        fprintf(stderr, "tags format incorrect `%s': %s\n", t, json_tokener_error_desc(error));
        goto end;
      }

      if (json_object_object_get_ex(te, "nclients", &eid))
	json_object_object_add(tags, "nclients", json_object_get(eid));      
      if (json_object_object_get_ex(te, "load", &eid))
	json_object_object_add(tags, "load", json_object_get(eid));      
      if (json_object_object_get_ex(te, "load_eff", &eid))
	json_object_object_add(tags, "load_eff", json_object_get(eid));      
      if (json_object_object_get_ex(te, "iops", &eid))
	json_object_object_add(tags, "iops", json_object_get(eid));
      if (json_object_object_get_ex(te, "bytes", &eid))
	json_object_object_add(tags, "bytes", json_object_get(eid));

      if (detailed == 1) {
	json_object_object_foreach(te, event, val) {
	  if ((strcmp(event, "load") == 0) || (strcmp(event, "load_eff") == 0) || (strcmp(event, "iops") == 0) ||
	      (strcmp(event, "bytes") == 0) || (json_object_get_double(val) < 2)) continue;
	
	  json_object_object_add(events, event, json_object_new_string(""));
	  json_object_object_add(tags, event, json_object_get(val));
	}
      }
      json_object_array_add(data_array, json_object_get(tags));
    end:
      if(tags)
	json_object_put(tags);
    }
  }
  json_object_array_sort(data_array, sort_da);
  int data_length = json_object_array_length(data_array);

  /* Construct header */
  char header_tags[512] = "";
  char *cur = header_tags, * const end = header_tags + sizeof(header_tags);
  json_object_object_foreach(group_tags, t, v) {
    cur += snprintf(cur, end - cur, "%-14.14s ", t);
  }

  json_object_object_foreach(events, e, val) {
    cur += snprintf(cur, end - cur, "%10.10s ", e);
  }

  mvprintw(line, 0, "%s", header_tags);
  mvchgat(line, 0, -1, A_STANDOUT, CP_BLACK, NULL);
  line++;

  int new_start = scroll_start + scroll_delta;
  int max_start = data_length - (LINES - line - 1);
  
  new_start = MIN(new_start, max_start);
  new_start = MAX(new_start, 0);

  int j;
  for (j = new_start; j < data_length && line < (LINES - 1); j++) {
    json_object *de = json_object_array_get_idx(data_array, j);

    /* Construct row */
    char row_tags[512] = "";
    char *cur = row_tags, * const end = row_tags + sizeof(row_tags);
    json_object_object_foreach(group_tags, t, v) {
      if (json_object_object_get_ex(de, t, &tid))
	cur += snprintf(cur, end - cur, "%-14.14s ", json_object_get_string(tid));
      else
	cur += snprintf(cur, end - cur, "%-14s ", "");
    }

    json_object_object_foreach(events, e, val) {
      if (json_object_object_get_ex(de, e, &eid))
	cur += snprintf(cur, end - cur, "%10.2f ", json_object_get_double(eid));     
      else
	cur += snprintf(cur, end - cur, "%10.1f ", 0.0);
    }
    mvprintw(line++, 0, "%s", row_tags); 
  }

  json_object_put(data_array);
  json_object_put(events);

  move(line, 0);
  clrtobot();
  
  if (new_start != scroll_start || status_bar_time + 4 < now)
    status_bar_printf(EV_A_ "%d-%d out of %d",
                      new_start + (data_length != 0),
                      j, data_length);
  
  mvprintw(LINES - 1, 0, "%.*s", COLS, status_bar);
  
  int prog_ctime_len = strlen(program_invocation_short_name) + 28;
  
  if (strlen(status_bar) + prog_ctime_len < COLS)
    mvprintw(LINES - 1, COLS - prog_ctime_len, "%s - %s",
             program_invocation_short_name, ctime(&now));
  
  mvchgat(LINES - 1, 0, -1, A_STANDOUT, CP_BLACK, NULL);

  scroll_start = new_start;
  scroll_delta = 0;  

  //gettimeofday(&te, NULL); 
  //printf("time for processing %f\n", (double)(te.tv_sec - ts.tv_sec) + (double)(te.tv_usec - ts.tv_usec)/1000000. );

}

static void usage(void)
{
  fprintf(stderr,
          "Usage: %s [OPTION...]\n"
          "The pfstop program provides a dynamic real-time view of parallel filesystem usage as part of PFSTRASE (Parallel FileSystem TRacing and Analysis SErvice).\n"
          "\n"
          "Input Arguments:\n"
          "  -n, --interval     Set refresh interval in seconds (default=3.0)\n"
          "  -u, --user         Filter displayed usage for a specific user ID (uid)" 
          "  -h, --help         Print this message\n"
          "\n"
          "Display modifiers:\n"
          "  f      Group by filesystem ID (fid)\n"
          "  u      Group by user ID (uid)\n"
          "  j      Group by job ID (jid)\n"
          "  c      Group by pfs client\n"
          "  s      Group by pfs server\n"
          "\n"
          "  l      Sort by effective load (load_eff)\n"
          "  i      Sort by IOPS\n"
          "  b      Sort by bytes\n"
          "\n"
          "  q      Quit \n",
          program_invocation_short_name);
}

int main(int argc, char *argv[])
{

  struct option opts[] = {
    { "help",   no_argument, 0, 'h' },
    { "interval", required_argument, 0, 'n' },
    { "user", required_argument, 0, 'u' },
    { NULL,     0, 0, 0 },
  };

  int c;
  while((c = getopt_long(argc, argv, "h:nu:", opts, 0)) != -1) {
    switch (c) {
      case 'n':
        refresh_interval = atof(optarg);
        break;
      case 'u':
        filter_by = optarg
        break;
      case 'h':
        usage();
        exit(0);
      case '?':
        fprintf (stderr, "Try '%s --help' for more information.\n", program_invocation_short_name);
        exit(1);
    }
  }

  shmmap_client_init();
  
  screen_init(refresh_interval);
  screen_start(EV_DEFAULT);

  ev_run(EV_DEFAULT, 0);
  
  screen_stop(EV_DEFAULT);

  shmmap_client_kill();

  return EXIT_SUCCESS;
}
