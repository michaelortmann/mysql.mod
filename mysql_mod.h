/*
 * Copyright (C) 2003-2006 BarkerJr <http://barkerjr.net>
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

#define STDVARPROT (ClientData, Tcl_Interp *, int, char *[]);

#define CHECKDB \
  if (!dbc) \
  { \
    Tcl_SetObjResult(irp, \
                     Tcl_NewStringObj("Not connected to a database", -1)); \
    return TCL_ERROR; \
  }

#define list_delete egg_list_delete
#include "src/mod/module.h"
#undef list_delete
#include <mysql.h>

EXPORT_SCOPE char *mysql_start();
static char *mysql_stop();
static int mysql_expmem();
static void mysql_report(int *, int *);
static int tcl_mysql_connect STDVARPROT
static int tcl_mysql_close STDVARPROT
static int tcl_mysql_query STDVARPROT
static int tcl_mysql_escape STDVARPROT
static int tcl_mysql_errno STDVARPROT
static int tcl_mysql_ping STDVARPROT
static int tcl_mysql_connectioninfo STDVARPROT
static int tcl_mysql_insert_id STDVARPROT
static int tcl_mysql_connected STDVARPROT
static int tcl_mysql_affected_rows STDVARPROT
static void closedb(void);
static void freeit(char **);
static char *allocit(const char *);
