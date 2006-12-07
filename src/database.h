/* database.h -- header file for the database module.
   Copyright (C) 2003, 2004
     National Institute of Advanced Industrial Science and Technology (AIST)
     Registration Number H15PRO112

   This file is part of the m17n library.

   The m17n library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.

   The m17n library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the m17n library; if not, write to the Free
   Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   02111-1307, USA.  */

#ifndef _M17N_DATABASE_H_
#define _M17N_DATABASE_H_

#ifndef M17NDIR
#define M17NDIR "/usr/local/share/m17n"
#endif

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#ifndef PATH_SEPARATOR
#define PATH_SEPARATOR '/'
#endif

enum MDatabaseStatus
  {
    /* The database was defined automatically (from mdb.dir file(s)).*/
    MDB_STATUS_AUTO,
    /* The database was defined explicitely (by mdatabase_define ()). */
    MDB_STATUS_EXPLICIT,
    /* The databse is currently disabled. (usually because it is
       deleted from mdb.dir file(s)).  */
    MDB_STATUS_DISABLED
  };

typedef struct
{
  /* Name of the file containing the database.  */
  char *filename;
  /* Length of FILENAME.  */
  int len;
  /* Absolute path of filename.  */
  char *absolute_filename;
  /* The current status of the database.  */
  enum MDatabaseStatus status;
  /* When the database was loaded last.  0 if it has never been
     loaded.  */
  time_t time;
  char *lock_file, *uniq_file;
} MDatabaseInfo;

extern MPlist *mdatabase__dir_list;

extern void mdatabase__update (void);

extern MPlist *mdatabase__load_for_keys (MDatabase *mdb, MPlist *keys);

extern int mdatabase__check (MDatabase *mdb);

extern char *mdatabase__find_file (char *filename);

extern char *mdatabase__file (MDatabase *mdb);

extern int mdatabase__lock (MDatabase *mdb);

extern int mdatabase__save (MDatabase *mdb, MPlist *data);

extern int mdatabase__unlock (MDatabase *mdb);

#endif /* not _M17N_DATABASE_H_ */
