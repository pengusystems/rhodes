/* Copyright (C) 2007-2017 Thorsten Kukuk

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include "crypt-port.h"
#include "crypt-base.h"

/* The functions that use global state objects are isolated in their
   own files so that a statically-linked program that doesn't use them
   will not have the state objects in its data segment.  */

#if INCLUDE_crypt || INCLUDE_fcrypt
char *
crypt (const char *key, const char *setting)
{
  static struct crypt_data nr_crypt_ctx;
  return crypt_r (key, setting, &nr_crypt_ctx);
}
#endif

#if INCLUDE_crypt
SYMVER_crypt;
#endif

#if INCLUDE_fcrypt
strong_alias (crypt, fcrypt);
SYMVER_fcrypt;
#endif
