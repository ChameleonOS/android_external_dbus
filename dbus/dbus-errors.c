/* -*- mode: C; c-file-style: "gnu" -*- */
/* dbus-errors.c Error reporting
 *
 * Copyright (C) 2002  Red Hat Inc.
 * Copyright (C) 2003  CodeFactory AB
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
#include "dbus-errors.h"
#include "dbus-internals.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/**
 * @defgroup DBusErrors Error reporting
 * @ingroup  DBus
 * @brief Error reporting
 *
 * Types and functions related to reporting errors.
 *
 *
 * In essence D-BUS error reporting works as follows:
 *
 * @code
 * DBusResultCode result = DBUS_RESULT_SUCCESS;
 * dbus_some_function (arg1, arg2, &result);
 * if (result != DBUS_RESULT_SUCCESS)
 *   printf ("an error occurred\n");
 * @endcode
 *
 * @todo add docs with DBusError
 * 
 * @{
 */

typedef struct
{
  const char *name; /**< error name */
  char *message; /**< error message */

  unsigned int const_message : 1; /** Message is not owned by DBusError */

  unsigned int dummy2 : 1; /**< placeholder */
  unsigned int dummy3 : 1; /**< placeholder */
  unsigned int dummy4 : 1; /**< placeholder */
  unsigned int dummy5 : 1; /**< placeholder */

  void *padding1; /**< placeholder */
  
} DBusRealError;

/**
 * Returns a longer message describing an error name.
 * If the error name is unknown, returns the name
 * itself.
 *
 * @param error the error to describe
 * @returns a constant string describing the error.
 */
static const char*
message_from_error (const char *error)
{
  if (strcmp (error, DBUS_ERROR_FAILED) == 0)
    return "Unknown error";
  else if (strcmp (error, DBUS_ERROR_NO_MEMORY) == 0)
    return "Not enough memory available";
  else if (strcmp (error, DBUS_ERROR_IO_ERROR) == 0)
    return "Error reading or writing data";
  else if (strcmp (error, DBUS_ERROR_BAD_ADDRESS) == 0)
    return "Could not parse address";
  else if (strcmp (error, DBUS_ERROR_NOT_SUPPORTED) == 0)
    return "Feature not supported";
  else if (strcmp (error, DBUS_ERROR_LIMITS_EXCEEDED) == 0)
    return "Resource limits exceeded";
  else if (strcmp (error, DBUS_ERROR_ACCESS_DENIED) == 0)
    return "Permission denied";
  else if (strcmp (error, DBUS_ERROR_AUTH_FAILED) == 0)
    return "Could not authenticate to server";
  else if (strcmp (error, DBUS_ERROR_NO_SERVER) == 0)
    return "No server available at address";
  else if (strcmp (error, DBUS_ERROR_TIMEOUT) == 0)
    return "Connection timed out";
  else if (strcmp (error, DBUS_ERROR_NO_NETWORK) == 0)
    return "Network unavailable";
  else if (strcmp (error, DBUS_ERROR_ADDRESS_IN_USE) == 0)
    return "Address already in use";
  else if (strcmp (error, DBUS_ERROR_DISCONNECTED) == 0)
    return "Disconnected.";
  else if (strcmp (error, DBUS_ERROR_INVALID_ARGS) == 0)
    return "Invalid argumemts.";
  else if (strcmp (error, DBUS_ERROR_NO_REPLY) == 0)
    return "Did not get a reply message.";
  else if (strcmp (error, DBUS_ERROR_FILE_NOT_FOUND) == 0)
    return "File doesn't exist.";
  else
    return error;
}

/**
 * Initializes a DBusError structure. Does not allocate
 * any memory; the error only needs to be freed
 * if it is set at some point. 
 *
 * @param error the DBusError.
 */
void
dbus_error_init (DBusError *error)
{
  DBusRealError *real;

  _dbus_assert (error != NULL);

  _dbus_assert (sizeof (DBusError) == sizeof (DBusRealError));

  real = (DBusRealError *)error;
  
  real->name = NULL;  
  real->message = NULL;

  real->const_message = TRUE;
}

/**
 * Frees an error that's been set (or just initialized),
 * then reinitializes the error as in dbus_error_init().
 *
 * @param error memory where the error is stored.
 */
void
dbus_error_free (DBusError *error)
{
  DBusRealError *real;

  real = (DBusRealError *)error;

  if (!real->const_message)
    dbus_free (real->message);

  dbus_error_init (error);
}

/**
 * Assigns an error name and message to a DBusError.
 * Does nothing if error is #NULL. The message may
 * be NULL only if the error is DBUS_ERROR_NO_MEMORY.
 *
 * @param error the error.
 * @param name the error name (not copied!!!)
 * @param message the error message (not copied!!!)
 */
void
dbus_set_error_const (DBusError  *error,
		      const char *name,
		      const char *message)
{
  DBusRealError *real;

  if (error == NULL)
    return;

  /* it's a bug to pile up errors */
  _dbus_assert (error->name == NULL);
  _dbus_assert (error->message == NULL);
  _dbus_assert (name != NULL);

  if (message == NULL)
    message = message_from_error (name);
  
  real = (DBusRealError *)error;
  
  real->name = name;
  real->message = (char *)message;
  real->const_message = TRUE;
}

/**
 * Moves an error src into dest, freeing src and
 * overwriting dest. Both src and dest must be initialized.
 * src is reinitialized to an empty error. dest may not
 * contain an existing error. If the destination is
 * #NULL, just frees and reinits the source error.
 *
 * @param src the source error
 * @param dest the destination error or #NULL
 */
void
dbus_move_error (DBusError *src,
                 DBusError *dest)
{
  _dbus_assert (!dbus_error_is_set (dest));

  if (dest)
    {
      dbus_error_free (dest);
      *dest = *src;
      dbus_error_init (src);
    }
  else
    dbus_error_free (src);
}

/**
 * Checks whether the error is set and has the given
 * name.
 * @param error the error
 * @param name the name
 * @returns #TRUE if the given named error occurred
 */
dbus_bool_t
dbus_error_has_name (const DBusError *error,
                     const char      *name)
{
  _dbus_assert (error != NULL);
  _dbus_assert (name != NULL);
  _dbus_assert ((error->name != NULL && error->message != NULL) ||
                (error->name == NULL && error->message == NULL));
  
  if (error->name != NULL)
    {
      DBusString str1, str2;
      _dbus_string_init_const (&str1, error->name);
      _dbus_string_init_const (&str2, name);
      return _dbus_string_equal (&str1, &str2);
    }
  else
    return FALSE;
}

/**
 * Checks whether an error occurred (the error is set).
 *
 * @param error the error object
 * @returns #TRUE if an error occurred
 */
dbus_bool_t
dbus_error_is_set (const DBusError *error)
{
  _dbus_assert (error != NULL);  
  _dbus_assert ((error->name != NULL && error->message != NULL) ||
                (error->name == NULL && error->message == NULL));
  return error->name != NULL;
}

/**
 * Assigns an error name and message to a DBusError.
 * Does nothing if error is #NULL.
 *
 * The format may be NULL only if the error is DBUS_ERROR_NO_MEMORY.
 *
 * If no memory can be allocated for the error message, 
 * an out-of-memory error message will be set instead.
 *
 * @todo stdio.h shouldn't be included in this file,
 * should write _dbus_string_append_printf instead
 * 
 * @param error the error.
 * @param name the error name (not copied!!!)
 * @param format printf-style format string.
 */
void
dbus_set_error (DBusError  *error,
		const char *name,
		const char *format,
		...)
{
  DBusRealError *real;
  va_list args;
  int message_length;
  char *message;
  char c;

  if (error == NULL)
    return;

  /* it's a bug to pile up errors */
  _dbus_assert (error->name == NULL);
  _dbus_assert (error->message == NULL);
  _dbus_assert (name != NULL);

  if (format == NULL)
    format = message_from_error (name);
  
  va_start (args, format);
  /* Measure the message length */
  message_length = vsnprintf (&c, 1, format, args) + 1;
  va_end (args);
  
  message = dbus_malloc (message_length);
  
  if (!message)
    {
      dbus_set_error_const (error, DBUS_ERROR_NO_MEMORY, NULL);
      return;
    }
  
  va_start (args, format);  
  vsprintf (message, format, args);  
  va_end (args);

  real = (DBusRealError *)error;
  
  real->name = name;
  real->message = message;
  real->const_message = FALSE;
}

/** @} */
