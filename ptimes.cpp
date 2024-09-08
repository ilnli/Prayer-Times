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
#include <unistd.h>

#include "cmdline.h"
#include "prayertimes.hpp"

#define DAEMON_NAME "ptimes"
#define PID_FILE "/run/ptimes.pid"
#define BUF_SIZE 256

#define PLAYER "/usr/bin/aplay"
#define AZAN "/usr/share/sounds/ptimes/azan.wav"

#define SECONDSINDAY 86400

#define NODEBUG /* change to DEBUG for debugging */

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
    int seconds;
    char time24[6];
} prayer_t;

void signal_handler(int sig) {
    switch(sig) {
        case SIGHUP:
            syslog(LOG_WARNING, "Received SIGHUP signal.");
            break;
        case SIGTERM:
            syslog(LOG_WARNING, "Received SIGTERM signal.");
            exit(EXIT_SUCCESS);
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

int set_prayer_options(PrayerTimes *prayer_times) {

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
    if(opts->dhuhr_minutes_given) {
        prayer_times->set_dhuhr_minutes(opts->dhuhr_minutes_arg);
    }
    if(opts->maghrib_minutes_given) {
        prayer_times->set_maghrib_minutes(opts->maghrib_minutes_arg);
    }
    if(opts->isha_minutes_given) {
        prayer_times->set_isha_minutes(opts->isha_minutes_arg);
    }
    if(opts->fajr_angle_given) {
        prayer_times->set_fajr_angle(opts->fajr_angle_arg);
    }
    if(opts->maghrib_angle_given) {
        prayer_times->set_maghrib_angle(opts->maghrib_angle_arg);
    }
    if(opts->isha_angle_given) {
        prayer_times->set_isha_angle(opts->isha_angle_arg);
    }

    return 0;
}

int get_next_prayer(PrayerTimes *prayer_times, prayer_t *prayer) {
    time_t time_of_day, curr_time = time(NULL);
    double timezone = NAN;
	double times[PrayerTimes::TimesCount];

    timezone = PrayerTimes::get_effective_timezone(curr_time);

    /* Only check times between today and tomorrow otherwise give error */
    for(time_t j = curr_time; j <= curr_time + SECONDSINDAY; j += SECONDSINDAY) {
        prayer_times->get_prayer_times(j, opts->latitude_arg, 
            opts->longitude_arg, timezone, times);
        for (int i = 0; i < PrayerTimes::TimesCount; i++) {
            /* Skip time for Sunrise (1) and Sunset (4) */
            if (i == 1 || i == 4)
                i++;

            time_of_day = PrayerTimes::float_time_to_epoch(times[i], j);
            if(time_of_day >= curr_time) {
                prayer->name_id = i;
                prayer->seconds = time_of_day - curr_time;
                strcpy(prayer->time24, PrayerTimes::float_time_to_time24(times[i]).c_str());
                return 1;
            }
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
            /* child process */
            syslog(LOG_INFO, "Forking azan player");
            execl(PLAYER, "aplay", AZAN, (char *) 0);
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
    signal(SIGCHLD, SIG_IGN);
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
        syslog(LOG_INFO, "Starting the daemonizing process");

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
            syslog(LOG_INFO, DAEMON_NAME" is already running");
            exit(EXIT_FAILURE);
        }
        sprintf(buf, "%ld\n", (long) sid);
        write(lfd, buf, strlen(buf));

        /* Close out the standard file descriptors */
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }
}

int main(int argc, char *argv[])
{
    int last_prayer = -1;
    PrayerTimes prayer_times;
    prayer_t next_prayer;

    parse_cmdline(argc, argv);
    set_prayer_options(&prayer_times);

    daemonize();


    syslog(LOG_INFO, 
        "%s daemon started with parameters latitude=%.5lf, longitude=%.5lf", 
        DAEMON_NAME, opts->latitude_arg, opts->longitude_arg);
 
    while(true) {
        if(get_next_prayer(&prayer_times, &next_prayer)) {
            if(next_prayer.seconds != 0) {
                syslog(LOG_INFO, "%s will be in %d minutes at %s", TimeName[next_prayer.name_id], 
                    next_prayer.seconds/60, next_prayer.time24);

                /* Wait till next prayer */
                sleep(next_prayer.seconds);
            } else {
                /* If it's time for prayer then sleep for 1 second, 
                 * so that we don't start utilizing too much CPU.
                 */
                sleep(1);
            }

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

