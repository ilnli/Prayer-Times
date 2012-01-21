/*
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <syslog.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "cmdline.h"
#include "prayertimes.hpp"

#define DAEMON_NAME "ptimes"
#define PID_FILE "/var/run/ptimes.pid"
#define BUF_SIZE 256

#define PLAYER "/usr/bin/alsaplayer"
#define AZAN "/root/programs/ptimes/audio/Amazing-Azan.wav"

#define SECONDSINDAY 86400

#define _free(p) \
    do { if (p) { free(p); p=0; } } while (0)

static const char *config_path = "/etc/";
static const char *config_fname = "ptimes.conf";

typedef struct gengetopt_args_info *opts_t;
static opts_t opts = NULL;

static const char* TimeName[] =
{
	"Fajr",
	"Sunrise",
	"Dhuhr",
	"Asr",
	"Sunset",
	"Maghrib",
	"Isha",
};

typedef struct _prayer {
    char name_id;
    int minutes;
} prayer_t;

void signal_handler(int sig) {
    switch(sig) {
        case SIGHUP:
            syslog(LOG_WARNING, "Received SIGHUP signal.");
            break;
        case SIGTERM:
            syslog(LOG_WARNING, "Received SIGTERM signal.");
            break;
        default:
            syslog(LOG_WARNING, "Unhandled signal (%d) %s", strsignal(sig));
            break;
    }
}

// FIXME: Revisit clean up function
static void cleanup()
{
  if (opts)
    {
      cmdline_parser_free(opts);
      _free(opts);
    }
}

static int config_exists(const char *fpath)
{
  FILE *f = fopen(fpath, "r");
  int have_config = 0;
  if (f != NULL)
    {
      have_config = 1;
      fclose(f);
      f = NULL;
    }
  return (have_config);
}

static int parse_config(int argc, char **argv)
{
  struct cmdline_parser_params *pp = NULL;
  int have_config = 0;
  char *fpath = NULL;

  asprintf(&fpath, "%s/%s", config_path, config_fname);
  if (fpath == NULL)
    {
      cleanup();
      perror("ERROR");
      exit (EXIT_FAILURE);
    }

  have_config = config_exists(fpath);
  if (have_config == 0)
    {
      _free(fpath);
      return (have_config);
    }

  pp = cmdline_parser_params_create();
  pp->check_required = 0;

  have_config = (int)(cmdline_parser_config_file(fpath, opts, pp) == 0);
  if (have_config == 1)
    {
      _free(fpath);
      pp->check_required = 1;
      pp->initialize = 0;
      pp->override = 1;
      have_config = (int)(cmdline_parser_ext(argc, argv, opts, pp) == 0);
    }

  _free(fpath);
  _free(pp);

  return (have_config);
}

static void parse_cmdline(int argc, char **argv)
{
  int have_config = 0;

  opts = (opts_t) calloc(1, sizeof(struct gengetopt_args_info));
  if (opts == NULL) {
      perror("ERROR");
      exit(EXIT_FAILURE);
  }

  have_config = parse_config(argc, argv);

  if (have_config == 0)
    {
      if (cmdline_parser(argc, argv, opts) != 0)
        {
          cleanup();
          exit (EXIT_FAILURE);
        }
    }
}

int set_prayer_options(PrayerTimes *prayer_times, time_t *date, double *timezone) {

    if(opts->timezone_given) {            // --timezone
        *timezone = opts->timezone_arg;
    } else {
        *timezone = prayer_times->get_effective_timezone(*date);
    }

    if(opts->calc_method_given) {         // --calc-method
        if (strcmp(opts->calc_method_arg, "jafari") == 0)
            prayer_times->set_calc_method(PrayerTimes::Jafari);

        else if (strcmp(opts->calc_method_arg, "karachi") == 0)
            prayer_times->set_calc_method(PrayerTimes::Karachi);

        else if (strcmp(opts->calc_method_arg, "isna") == 0)
            prayer_times->set_calc_method(PrayerTimes::ISNA);

        else if (strcmp(opts->calc_method_arg, "mwl") == 0)
            prayer_times->set_calc_method(PrayerTimes::MWL);

        else if (strcmp(opts->calc_method_arg, "makkah") == 0)
            prayer_times->set_calc_method(PrayerTimes::Makkah);

        else if (strcmp(opts->calc_method_arg, "egypt") == 0)
            prayer_times->set_calc_method(PrayerTimes::Egypt);

        else if (strcmp(opts->calc_method_arg, "custom") == 0)
            prayer_times->set_calc_method(PrayerTimes::Custom);
    }
    if(opts->asr_juristic_method_given) { // --asr-juristic-method
        if (strcmp(opts->asr_juristic_method_arg, "shafii") == 0)
            prayer_times->set_asr_method(PrayerTimes::Shafii);

        else if (strcmp(opts->asr_juristic_method_arg, "hanafi") == 0)
            prayer_times->set_asr_method(PrayerTimes::Hanafi);
    }
    if(opts->high_lats_method_given) {    // --high-lats-method
        if (strcmp(opts->high_lats_method_arg, "none") == 0)
            prayer_times->set_high_lats_adjust_method(PrayerTimes::None);

        else if (strcmp(opts->high_lats_method_arg, "midnight") == 0)
            prayer_times->set_high_lats_adjust_method(PrayerTimes::MidNight);

        else if (strcmp(opts->high_lats_method_arg, "oneseventh") == 0)
            prayer_times->set_high_lats_adjust_method(PrayerTimes::OneSeventh);

        else if (strcmp(opts->high_lats_method_arg, "anglebased") == 0)
            prayer_times->set_high_lats_adjust_method(PrayerTimes::AngleBased);
    }

    return 0;
}

int get_next_prayer(PrayerTimes *prayer_times, double timezone, time_t date, prayer_t *prayer) {
    time_t now, time_of_day;
	double times[PrayerTimes::TimesCount];

    now = time(NULL);

    /* Only check times between today and tomorrow otherwise give error */
    for(int j = 0; j <= SECONDSINDAY; j += SECONDSINDAY) {
        date = date + j;

        prayer_times->get_prayer_times(date, opts->latitude_arg, opts->longitude_arg, timezone, times);
        for (int i = 0; i < PrayerTimes::TimesCount; i++) {
            time_of_day = PrayerTimes::float_time_to_epoch(times[i], date);

            if((now <= time_of_day)) {
                prayer->name_id = i;
                prayer->minutes = (time_of_day - now) / 60;
                return 1;
            }

            /* Skip time for Sunrise (1) and Sunset (4) */
            if (i == 1 || i == 4)
                i++;
        }
    }
    return 0;
}

void play_azan() {
    switch(fork()) {
        case -1:
            syslog(LOG_INFO, "Unable to fork Azan player process");
            exit(EXIT_FAILURE);
            break;
        case 0:
            syslog(LOG_INFO, "Forking azan player");
            /* child process */
            execl(PLAYER, DAEMON_NAME"Player", AZAN, (char *) 0);
            exit(EXIT_SUCCESS);
            break;
        default:
            /* parent process */
            break;
    }
}

void daemonize(void) {
    int lfd, rc;
    char buf[BUF_SIZE];

#if defined(DEBUG)
    int daemonize = 0;
#else
    int daemonize = 1;
#endif
 
    /* Setup signal handling before we start */
    signal(SIGHUP, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGQUIT, signal_handler);


    // Setup syslog logging - see SETLOGMASK(3)
#if defined(DEBUG)
    setlogmask(LOG_UPTO(LOG_DEBUG));
    openlog(DAEMON_NAME, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER);
#else
    setlogmask(LOG_UPTO(LOG_INFO));
    openlog(DAEMON_NAME, LOG_CONS | LOG_PID, LOG_USER);
#endif
 
    /* Our process ID and Session ID */
    pid_t pid, sid;
 
    if (daemonize) {
        syslog(LOG_INFO, "starting the daemonizing process");
 
        /* Fork off the parent process */
        pid = fork();
        if (pid < 0) {
            exit(EXIT_FAILURE);
        }
        /* If we got a good PID, then
           we can exit the parent process. */
        if (pid > 0) {
            exit(EXIT_SUCCESS);
        }
 
        /* Change the file mode mask */
        umask(0);
 
        /* Create a new SID for the child process */
        sid = setsid();
        if (sid < 0) {
            /* Log the failure */
            exit(EXIT_FAILURE);
        }
 
        /* Change the current working directory */
        if ((chdir("/")) < 0) {
            /* Log the failure */
            exit(EXIT_FAILURE);
        }

        /* Run only once instance of the daemon */
        lfd = open(PID_FILE, O_RDWR|O_CREAT, 0640);
        if(lfd < 0) {
            exit(EXIT_FAILURE);
        }
        if(lockf(lfd, F_TLOCK, 0) < 0) {
            syslog(LOG_INFO, DAEMON_NAME" is already running, exiting");
            exit(EXIT_FAILURE);
        }
        sprintf(buf, "%ld\n", (long) pid);
        write(lfd, buf, strlen(buf));
 
        /* Close out the standard file descriptors */
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }
}


int main(int argc, char *argv[])
{
    daemonize();

    int last_prayer = -1;
    PrayerTimes prayer_times;
	time_t date = time(NULL);
    double timezone = NAN;
    prayer_t next_prayer;

    parse_cmdline(argc, argv);
    set_prayer_options(&prayer_times, &date, &timezone);

	if (isnan(opts->timezone_arg))
		timezone = PrayerTimes::get_effective_timezone(date);


    syslog(LOG_INFO, "%s daemon starting up with parameters timezone=%.1lf, latitude=%.5lf, longitude=%.5lf", 
            DAEMON_NAME, opts->timezone_arg, opts->latitude_arg, opts->longitude_arg);
 
 
    while(true) {
        if(get_next_prayer(&prayer_times, timezone, date, &next_prayer)) {
            syslog(LOG_INFO, "%s will be in %d minutes", TimeName[next_prayer.name_id], next_prayer.minutes);
            /* 
             * Wait till next prayer and if it's time for prayer sleep for 
             * 1 second, so that we don't start utilizing too much CPU.
             */
            next_prayer.minutes == 0 ? sleep(1) : sleep(next_prayer.minutes*60);

            /* Make sure we don't keep on alerting for same prayer in the loop */
            if(last_prayer != next_prayer.name_id) {
                syslog(LOG_INFO, "Time for %s", TimeName[next_prayer.name_id]);
                play_azan();    
                last_prayer = next_prayer.name_id;
            }
        }
    }

    syslog(LOG_INFO, "%s daemon exiting", DAEMON_NAME);
 
    cleanup();
    exit(0);
}

