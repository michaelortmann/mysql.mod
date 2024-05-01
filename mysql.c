/*
 * Copyright (C) 2003, 2004 BarkerJr <http://barkerjr.net>
 *
 * This file is part of MySQL Module.
 *
 * MySQL Module is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#define MODULE_NAME "mysql"
#define MAKING_MYSQL

#include "mysql_mod.h"

#undef global

static Function *global = NULL;

/* Pointer soon to hold the connection to the database */
static MYSQL *dbc = NULL;

static char *database=NULL, *host=NULL, *user=NULL, *portsock=NULL;
static unsigned short mem = 0;

static Function mysql_table[] =
{
/* 0 - 5 */
  (Function) mysql_start,
  (Function) mysql_stop,
  (Function) mysql_expmem,
  (Function) mysql_report,
  (Function) closedb
};

static tcl_cmds mysql_cmds[] =
{
  {"mysql_connect",		tcl_mysql_connect},
  {"mysql_close",		tcl_mysql_close},
  {"mysql_query",		tcl_mysql_query},
  {"mysql_escape",		tcl_mysql_escape},
  {"mysql_errno",		tcl_mysql_errno},
  {"mysql_ping",		tcl_mysql_ping},
  {"mysql_connectioninfo",	tcl_mysql_connectioninfo},
  {"mysql_insert_id",		tcl_mysql_insert_id},
  {"mysql_connected",		tcl_mysql_connected},
  {"mysql_affected_rows",	tcl_mysql_affected_rows},
  {NULL,			NULL}
};

char *mysql_start(Function *global_funcs)
{
  global = global_funcs;
  module_register(MODULE_NAME, mysql_table, 0, 6);
  add_tcl_commands(mysql_cmds);
  return NULL;
}

static char *mysql_stop()
{
  closedb();
  rem_tcl_commands(mysql_cmds);
  module_undepend(MODULE_NAME);
  return NULL;
}

static int mysql_expmem() { return mem; }

static void mysql_report(int *idx, int *details)
{
  if (details)
  {
    if (dbc)
      dprintf(idx, "    Connected to %s on %s as %s.\n", database, host, user);
    else dprintf(idx, "    Disconnected from a database.\n");
  }
}

/*
 * Connects to the database.
 * mysql_connect <database> <hostname> [user] [password] [socket|port]
 */
static int tcl_mysql_connect STDVAR
{
  char *sock=NULL, *pass=NULL;
  unsigned short port=0;

  BADARGS(3, 6, " database hostname ?user? ?password? ?socket|port?");

  /* If we're already connected to a database, disconnect first. */
  closedb();

  database = allocit(argv[1]);
  host = allocit(argv[2]);
  if (!host || !database)
  {
    Tcl_SetObjResult(irp,
                     Tcl_NewStringObj("Unable to allocate memory to connect",
                                      -1));
    freeit(&host);
    freeit(&database);
    return TCL_ERROR;
  }

  if ((argc > 3) && (*argv[3]))
  {
    if (!(user = allocit(argv[3])))
    {
      Tcl_SetObjResult(irp,
                       Tcl_NewStringObj("Unable to allocate memory to connect",
                                        -1));
      freeit(&host);
      freeit(&database);
      return TCL_ERROR;
    }

    if ((argc > 4) && (*argv[4]))
    {
      pass = argv[4];
  /* If a socket or port was specified, we need to use them.  If the host is
     "localhost," we'll pipe the connection through the socket, otherwise we'll
     connect via the port */
      if ((argc > 5) && *argv[5])
      {
        if (!(portsock = allocit(argv[5])))
        {
          Tcl_SetObjResult(irp,
                           Tcl_NewStringObj("Unable to allocate memory to connect",
                                            -1));
          freeit(&host);
          freeit(&database);
          freeit(&user);
          return TCL_ERROR;
        }
        if ((strlen(host) == 9) && !strcmp(host, "localhost")) sock = portsock;
        else port = atoi(portsock);
      }
    }
  }

  /* Request a database connection object so we can connect. */
  if (!(dbc = mysql_init(dbc)))
  {
    Tcl_SetObjResult(irp,
                     Tcl_NewStringObj("Failed to connect to database: could not allocate memory",
                                      -1));
    freeit(&host);
    freeit(&database);
    freeit(&user);
    freeit(&portsock);
    return TCL_ERROR;
  }

  /* Connect to the database.  Use the username and/or password if they were
     specified, otherwise we'll send NULL. */
  if (mysql_real_connect(dbc, host, user, pass, database, port, sock, 0))
    return TCL_OK;
  else
  {
    Tcl_Obj *obj = Tcl_NewStringObj("Failed to connect to database: ", -1);
    Tcl_AppendToObj(obj, mysql_error(dbc), -1);
    Tcl_SetObjResult(irp, obj);
    dbc = NULL;
    freeit(&host);
    freeit(&database);
    freeit(&user);
    freeit(&portsock);
    return TCL_ERROR;
  }
}

/*
 * Kills the connection to the database.
 * mysql_close
 */
static int tcl_mysql_close STDVAR
{
  CHECKDB

  closedb();
  return TCL_OK;
}

/*
 * Sends a query to the server and returns the results, if any, to the script.
 * mysql_query <query>
 */
static int tcl_mysql_query STDVAR
{
  MYSQL_RES *result;
  unsigned short fields, x;
  MYSQL_ROW row;

  BADARGS(2, 2, " query");

  CHECKDB

  /* Attempt the query.  If we get an error, return a descriptive error. */
  if (mysql_query(dbc, argv[1]))
  {
    Tcl_Obj *obj = Tcl_NewStringObj("Query Failed: ", -1);
    Tcl_AppendToObj(obj, mysql_error(dbc), -1);
    Tcl_SetObjResult(irp, obj);
    return TCL_ERROR;
  }

  /* If there's no result, there's nothing to return.  We're done. */
  if (!(result = mysql_store_result(dbc))) return TCL_OK;

  fields = mysql_num_fields(result);

  /* Loop through each row of the result, appending the data into a series of
     nested lists, which we'll return to the script. */
  while ((row = mysql_fetch_row(result)))
  {
    Tcl_AppendResult(irp, "{", NULL);
    for (x = 0; x < fields; x++)
      Tcl_AppendElement(irp, (row[x]? row[x]: "NULL"));
    Tcl_AppendResult(irp, "} ", NULL);
  }

  /* Don't forget to free the memory used by the result. */
  mysql_free_result(result);

  return TCL_OK;
}

/*
 * Makes a string safe for sending to the MySQL server by escaping special
 * characters, such as backslashes and single quotes.
 * mysql_escape [byte] <string>
 */
static int tcl_mysql_escape STDVAR
{
  unsigned long length;
  char *in;

  BADARGS(2, 3, " ?bytes? string");

  if (argc > 2)
  {
    length = atoi(argv[1]);
    in = argv[2];
  }
  else
  {
    in = argv[1];
    length = strlen(in);
  }

  {
    /* Worst case, all characters will need to be escaped, doubling the size */
    char result[length * 2 + 1];

    /* If we're connected, we should really use 'real,' because it accounts for
       the current character set. */
    if (dbc) mysql_real_escape_string(dbc, result, in, length);
    else mysql_escape_string(result, in, length);

    Tcl_SetObjResult(irp, Tcl_NewStringObj(result, -1));
  }

  return TCL_OK;
}

/*
 * Fetches and return the error number.
 * mysql_errno
 */
static int tcl_mysql_errno STDVAR
{
  CHECKDB
  Tcl_SetObjResult(irp, Tcl_NewIntObj(mysql_errno(dbc)));
  return TCL_OK;
}

/*
 * Checks whether the connection to the server is working. If it has gone down,
 * an automatic reconnection is attempted.
 * mysql_ping
 */
static int tcl_mysql_ping STDVAR
{
  CHECKDB
  if (mysql_ping(dbc)) Tcl_SetObjResult(irp, Tcl_NewIntObj(mysql_errno(dbc)));
  return TCL_OK;
}

/* 
 * If connected, returns a list of database name, hostname, user, and port/sock.
 * mysql_connectioninfo
 */
static int tcl_mysql_connectioninfo STDVAR
{
  CHECKDB
  Tcl_Obj *objs[4];
  unsigned char count;
  objs[0] = Tcl_NewStringObj(database, -1);
  objs[1] = Tcl_NewStringObj(host, -1);
  if (user)
  {
    objs[2] = Tcl_NewStringObj(user, -1);
    if (portsock)
    {
      objs[3] = Tcl_NewStringObj(portsock, -1);
      count = 4;
    }
    else count = 3;
  }
  else count = 2;
  Tcl_SetObjResult(irp, Tcl_NewListObj(count, objs));
  return TCL_OK;
}

/*
 * Fetches and returns the ID from the last insertion.
 * mysql_insert_id
 */
static int tcl_mysql_insert_id STDVAR
{
  CHECKDB
  Tcl_SetObjResult(irp, Tcl_NewLongObj((unsigned long)mysql_insert_id(dbc)));
  return TCL_OK;
}

/*
 * Return whether the database is linked (true) or not (false).
 * mysql_connected
 */
static int tcl_mysql_connected STDVAR
{
  if (dbc) Tcl_SetObjResult(irp, Tcl_NewBooleanObj(1));
  else Tcl_SetObjResult(irp, Tcl_NewBooleanObj(0));
  return TCL_OK;
}

/*
 * Fetches and returns the number of rows that were changed by the last query.
 * mysql_affected_rows
 */
static int tcl_mysql_affected_rows STDVAR
{
  CHECKDB
  Tcl_SetObjResult(irp, Tcl_NewLongObj((unsigned long)mysql_affected_rows(dbc)));
  return TCL_OK;
}

/* Close the connection to the database. */
static void closedb(void)
{
  if (!dbc) return;
  mysql_close(dbc);
  dbc = NULL;
  freeit(&database);
  freeit(&host);
  freeit(&user);
  freeit(&portsock);
}

/* Frees the memory and accounts for it. */
static void freeit(char **p)
{
  if (!*p) return; /* It's pointing to null */
  mem -= strlen(*p) + 1;
  nfree (*p);
  *p = NULL;
}

/*
 * Allocates memory for the string to be copied into, copies it, and returns a
 * pointer to the new string.
 */
static char *allocit(const char *p)
{
  char *newp;
// If the memory isn't allocated, we have to stop here.
  if (!(newp = (char *)nmalloc(strlen(p) + 1))) return NULL;
  mem += strlen(p) + 1;
  strcpy(newp, p);
  return newp;
}
