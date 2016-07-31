/*
 * Show Q
 * Copyright (c) 2007-2016 Errol van de l'Isle
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#ifndef UUID_CPP_H
#define UUID_CPP_H

#include <uuid.h>
#include <string>

namespace uuid {
class uuid {
public:
  uuid()
  {
    uuid_generate(m_uuid);
  }
  uuid(const std::string &uuid_string)
  {
    uuid_parse(uuid_string.c_str(), m_uuid);
  }
  uuid(const char *uuid_string)
  {
    if (uuid_string)
      uuid_parse(uuid_string, m_uuid);
    else
      uuid_clear(m_uuid);
  }
  std::string unparse()
  {
    char buffer[42];
    uuid_unparse(m_uuid, buffer);
    return buffer;
  }
  void clear()
  {
    uuid_clear(m_uuid);
  }
  bool is_null()
  {
    return uuid_is_null(m_uuid) == 1;
  }
  uuid_t m_uuid;
};

inline bool operator==(const uuid &lhs, const uuid &rhs)
{
  return uuid_compare(lhs.m_uuid, rhs.m_uuid) == 0;
}
inline bool operator!=(const uuid &lhs, const uuid &rhs)
{
  return !(lhs == rhs);
}
inline bool operator<(const uuid &lhs, const uuid &rhs)
{
  return uuid_compare(lhs.m_uuid, rhs.m_uuid) < 0;
}
inline bool operator>(const uuid &lhs, const uuid &rhs)
{
  return lhs < rhs;
}
inline bool operator<=(const uuid &lhs, const uuid &rhs)
{
  return !(lhs > rhs);
}
inline bool operator>=(const uuid &lhs, const uuid &rhs)
{
  return !(lhs < rhs);
}
}

#endif // UUID_CPP_H

