/* Copyright (C) 2003 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifdef __GNUC__
#pragma implementation
#endif

#include "options.h"

#include "priv.h"

#include <my_sys.h>
#include <my_getopt.h>
#include <m_string.h>
#include <mysql_com.h>

#define QUOTE2(x) #x
#define QUOTE(x) QUOTE2(x)

char Options::run_as_service;
const char *Options::log_file_name= QUOTE(DEFAULT_LOG_FILE_NAME);
const char *Options::pid_file_name= QUOTE(DEFAULT_PID_FILE_NAME);
const char *Options::socket_file_name= QUOTE(DEFAULT_SOCKET_FILE_NAME);
const char *Options::password_file_name= QUOTE(DEFAULT_PASSWORD_FILE_NAME);
const char *Options::default_mysqld_path= QUOTE(DEFAULT_MYSQLD_PATH);
const char *Options::config_file= QUOTE(DEFAULT_CONFIG_FILE);
const char *Options::bind_address= 0;           /* No default value */
const char *Options::user= 0;                   /* No default value */
uint Options::monitoring_interval= DEFAULT_MONITORING_INTERVAL;
uint Options::port_number= DEFAULT_PORT;
/* just to declare */
char **Options::saved_argv;

/*
  List of options, accepted by the instance manager.
  List must be closed with empty option.
*/

enum options {
  OPT_LOG= 256,
  OPT_PID_FILE,
  OPT_SOCKET,
  OPT_PASSWORD_FILE,
  OPT_MYSQLD_PATH,
  OPT_RUN_AS_SERVICE,
  OPT_USER,
  OPT_MONITORING_INTERVAL,
  OPT_PORT,
  OPT_BIND_ADDRESS
};

static struct my_option my_long_options[] =
{
  { "help", '?', "Display this help and exit.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 },

  { "log", OPT_LOG, "Path to log file. Used only with --run-as-service.",
    (gptr *) &Options::log_file_name, (gptr *) &Options::log_file_name,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },

  { "pid-file", OPT_PID_FILE, "Pid file to use.",
    (gptr *) &Options::pid_file_name, (gptr *) &Options::pid_file_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },

  { "socket", OPT_SOCKET, "Socket file to use for connection.",
    (gptr *) &Options::socket_file_name, (gptr *) &Options::socket_file_name,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },

  { "passwd", 'P', "Prepare entry for passwd file and exit.", 0, 0, 0,
    GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 },

  { "bind-address", OPT_BIND_ADDRESS, "Bind address to use for connection.",
    (gptr *) &Options::bind_address, (gptr *) &Options::bind_address,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },

  { "port", OPT_PORT, "Port number to use for connections",
    (gptr *) &Options::port_number, (gptr *) &Options::port_number,
    0, GET_UINT, REQUIRED_ARG, DEFAULT_PORT, 0, 0, 0, 0, 0 },

  { "password-file", OPT_PASSWORD_FILE, "Look for Instane Manager users"
                                        " and passwords here.",
    (gptr *) &Options::password_file_name,
    (gptr *) &Options::password_file_name,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },

  { "default-mysqld-path", OPT_MYSQLD_PATH, "Where to look for MySQL"
                                            " Server binary.",
    (gptr *) &Options::default_mysqld_path, (gptr *) &Options::default_mysqld_path,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },

  { "monitoring-interval", OPT_MONITORING_INTERVAL, "Interval to monitor instances"
                                            " in seconds.",
                   (gptr *) &Options::monitoring_interval,
                   (gptr *) &Options::monitoring_interval,
                   0, GET_UINT, REQUIRED_ARG, DEFAULT_MONITORING_INTERVAL,
                   0, 0, 0, 0, 0 },

  { "run-as-service", OPT_RUN_AS_SERVICE,
    "Daemonize and start angel process.", (gptr *) &Options::run_as_service,
    0, 0, GET_BOOL, NO_ARG, 0, 0, 1, 0, 0, 0 },

  { "user", OPT_USER, "Username to start mysqlmanager",
                   (gptr *) &Options::user,
                   (gptr *) &Options::user,
                   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },

  { "version", 'V', "Output version information and exit.", 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 },

  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 }
};

static void version()
{
  printf("%s Ver %s for %s on %s\n", my_progname, mysqlmanager_version,
         SYSTEM_TYPE, MACHINE_TYPE);
}


static const char *default_groups[]= { "manager", 0 };


static void usage()
{
  version();

  printf("Copyright (C) 2003, 2004 MySQL AB\n"
  "This software comes with ABSOLUTELY NO WARRANTY. This is free software,\n"
  "and you are welcome to modify and redistribute it under the GPL license\n");
  printf("Usage: %s [OPTIONS] \n", my_progname);

  my_print_help(my_long_options);
  printf("\nThe following options may be given as the first argument:\n"
  "--print-defaults        Print the program argument list and exit\n"
  "--defaults-file=#       Only read manager configuration and instance\n"
  "                        setings from the given file #. The same file\n"
  "                        will be used to modify configuration of instances\n"
  "                        with SET commands.\n");
  my_print_variables(my_long_options);
}


static void passwd()
{
  char user[1024], *p;
  const char *pw1, *pw2;
  char pw1msg[]= "Enter password: ";
  char pw2msg[]= "Re-type password: ";
  char crypted_pw[SCRAMBLED_PASSWORD_CHAR_LENGTH + 1];

  fprintf(stderr, "Creating record for new user.\n");
  fprintf(stderr, "Enter user name: ");
  if (!fgets(user, sizeof(user), stdin))
  {
    fprintf(stderr, "Unable to read user.\n");
    return;
  }
  if ((p= strchr(user, '\n'))) *p= 0;

  pw1= get_tty_password(pw1msg);
  pw2= get_tty_password(pw2msg);

  if (strcmp(pw1, pw2))
  {
    fprintf(stderr, "Sorry, passwords do not match.\n");
    return;
  }

  make_scrambled_password(crypted_pw, pw1);
  printf("%s:%s\n", user, crypted_pw);
}


C_MODE_START

static my_bool
get_one_option(int optid,
               const struct my_option *opt __attribute__((unused)),
               char *argument __attribute__((unused)))
{
  switch(optid) {
  case 'V':
    version();
    exit(0);
  case 'P':
    passwd();
    exit(0);
  case '?':
    usage();
    exit(0);
  }
  return 0;
}

C_MODE_END


/*
  - Process argv of original program: get tid of --defaults-extra-file
    and print a message if met there.
  - call load_defaults to load configuration file section and save the pointer
    for free_defaults.
  - call handle_options to assign defaults and command-line arguments
  to the class members.
  if either of these function fail, return the error code.
*/

int Options::load(int argc, char **argv)
{
  saved_argv= argv;

  if (argc >= 2)
  {
    if (is_prefix(argv[1], "--defaults-file="))
    {
      Options::config_file= strchr(argv[1], '=') + 1;
    }
    if (is_prefix(argv[1], "--defaults-extra-file=") ||
        is_prefix(argv[1], "--no-defaults"))
    {
      /* the log is not enabled yet */
      fprintf(stderr, "The --defaults-extra-file and --no-defaults options"
                      " are not supported by\n"
                      "Instance Manager. Program aborted.\n");
      goto err;
    }
  }

  /* config-file options are prepended to command-line ones */
  load_defaults(config_file, default_groups, &argc,
                &saved_argv);

  if ((handle_options(&argc, &saved_argv, my_long_options,
                      get_one_option)) != 0)
    goto err;

  return 0;

err:
  return 1;
}

void Options::cleanup()
{
  /* free_defaults returns nothing */
  free_defaults(Options::saved_argv);
}
