/* -*- mode: C; c-file-style: "gnu" -*- */
/* main.c  main() for message bus
 *
 * Copyright (C) 2003 Red Hat, Inc.
 *
 * Licensed under the Academic Free License version 1.2
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include "bus.h"
#include "loop.h"
#include <dbus/dbus-internals.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

static BusContext *context;
static dbus_bool_t got_sighup = FALSE;

static void
signal_handler (int sig)
{
  switch (sig)
    {
    case SIGHUP:
      got_sighup = TRUE;
    case SIGTERM:
      bus_loop_quit (bus_context_get_loop (context));
      break;
    }
}

static void
usage (void)
{
  fprintf (stderr, "dbus-daemon-1 [--version] [--session] [--system] [--config-file=FILE] [--print-address[=descriptor]]\n");
  exit (1);
}

static void
version (void)
{
  printf ("D-BUS Message Bus Daemon %s\n"
          "Copyright (C) 2002, 2003 Red Hat, Inc., CodeFactory AB, and others\n"
          "This is free software; see the source for copying conditions.\n"
          "There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n",
          VERSION);
  exit (0);
}

static void
check_two_config_files (const DBusString *config_file,
                        const char       *extra_arg)
{
  if (_dbus_string_get_length (config_file) > 0)
    {
      fprintf (stderr, "--%s specified but configuration file %s already requested\n",
               extra_arg, _dbus_string_get_const_data (config_file));
      exit (1);
    }
}

static void
check_two_addr_descriptors (const DBusString *addr_fd,
                            const char       *extra_arg)
{
  if (_dbus_string_get_length (addr_fd) > 0)
    {
      fprintf (stderr, "--%s specified but printing address to %s already requested\n",
               extra_arg, _dbus_string_get_const_data (addr_fd));
      exit (1);
    }
}

int
main (int argc, char **argv)
{
  DBusError error;
  DBusString config_file;
  DBusString addr_fd;
  const char *prev_arg;
  int print_addr_fd;
  int i;
  dbus_bool_t print_address;
  
  if (!_dbus_string_init (&config_file))
    return 1;

  if (!_dbus_string_init (&addr_fd))
    return 1;
  
  print_address = FALSE;
  
  prev_arg = NULL;
  i = 1;
  while (i < argc)
    {
      const char *arg = argv[i];
      
      if (strcmp (arg, "--help") == 0 ||
          strcmp (arg, "-h") == 0 ||
          strcmp (arg, "-?") == 0)
        usage ();
      else if (strcmp (arg, "--version") == 0)
        version ();
      else if (strcmp (arg, "--system") == 0)
        {
          check_two_config_files (&config_file, "system");

          if (!_dbus_string_append (&config_file, DBUS_SYSTEM_CONFIG_FILE))
            exit (1);
        }
      else if (strcmp (arg, "--session") == 0)
        {
          check_two_config_files (&config_file, "session");

          if (!_dbus_string_append (&config_file, DBUS_SESSION_CONFIG_FILE))
            exit (1);
        }
      else if (strstr (arg, "--config-file=") == arg)
        {
          const char *file;

          check_two_config_files (&config_file, "config-file");
          
          file = strchr (arg, '=');
          ++file;

          if (!_dbus_string_append (&config_file, file))
            exit (1);
        }
      else if (prev_arg &&
               strcmp (prev_arg, "--config-file") == 0)
        {
          check_two_config_files (&config_file, "config-file");
          
          if (!_dbus_string_append (&config_file, arg))
            exit (1);
        }
      else if (strcmp (arg, "--config-file") == 0)
        ; /* wait for next arg */
      else if (strstr (arg, "--print-address=") == arg)
        {
          const char *desc;

          check_two_addr_descriptors (&addr_fd, "print-address");
          
          desc = strchr (arg, '=');
          ++desc;

          if (!_dbus_string_append (&addr_fd, desc))
            exit (1);

          print_address = TRUE;
        }
      else if (prev_arg &&
               strcmp (prev_arg, "--print-address") == 0)
        {
          check_two_addr_descriptors (&addr_fd, "print-address");
          
          if (!_dbus_string_append (&addr_fd, arg))
            exit (1);

          print_address = TRUE;
        }
      else if (strcmp (arg, "--print-address") == 0)
        print_address = TRUE; /* and we'll get the next arg if appropriate */
      else
        usage ();
      
      prev_arg = arg;
      
      ++i;
    }

  if (_dbus_string_get_length (&config_file) == 0)
    {
      fprintf (stderr, "No configuration file specified.\n");
      usage ();
    }

  print_addr_fd = -1;
  if (print_address)
    {
      print_addr_fd = 1; /* stdout */
      if (_dbus_string_get_length (&addr_fd) > 0)
        {
          long val;
          int end;
          if (!_dbus_string_parse_int (&addr_fd, 0, &val, &end) ||
              end != _dbus_string_get_length (&addr_fd) ||
              val < 0 || val > _DBUS_INT_MAX)
            {
              fprintf (stderr, "Invalid file descriptor: \"%s\"\n",
                       _dbus_string_get_const_data (&addr_fd));
              exit (1);
            }

          print_addr_fd = val;
        }
    }
  
  dbus_error_init (&error);
  context = bus_context_new (&config_file, &error);
  _dbus_string_free (&config_file);
  if (context == NULL)
    {
      _dbus_warn ("Failed to start message bus: %s\n",
                  error.message);
      dbus_error_free (&error);
      exit (1);
    }

  /* Note that we don't know whether the print_addr_fd is
   * one of the sockets we're using to listen on, or some
   * other random thing. But I think the answer is "don't do
   * that then"
   */
  if (print_addr_fd >= 0)
    {
      DBusString addr;
      const char *a = bus_context_get_address (context);
      int bytes;
      
      _dbus_assert (a != NULL);
      if (!_dbus_string_init (&addr) ||
          !_dbus_string_append (&addr, a) ||
          !_dbus_string_append (&addr, "\n"))
        exit (1);

      bytes = _dbus_string_get_length (&addr);
      if (_dbus_write (print_addr_fd, &addr, 0, bytes) != bytes)
        {
          _dbus_warn ("Failed to print message bus address: %s\n",
                      _dbus_strerror (errno));
          exit (1);
        }

      if (print_addr_fd > 2)
        _dbus_close (print_addr_fd, NULL);

      _dbus_string_free (&addr);
    }
  
  /* FIXME we have to handle this properly below _dbus_set_signal_handler (SIGHUP, signal_handler); */
  _dbus_set_signal_handler (SIGTERM, signal_handler);
  
  _dbus_verbose ("We are on D-Bus...\n");
  bus_loop_run (bus_context_get_loop (context));
  
  bus_context_shutdown (context);
  bus_context_unref (context);

  /* If we exited on TERM we just exit, if we exited on
   * HUP we restart the daemon.
   */
  if (got_sighup)
    {
      /* FIXME execv (argv) basically */
    }
  
  return 0;
}
