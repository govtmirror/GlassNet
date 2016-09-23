// mysqlconfig.cpp
//
// Class for gnmc(1) and gnmd(8) configuration.
//
// (C) Copyright 2016 Fred Gleason <fredg@paravelsystems.com>
//     All Rights Reserved.
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License version 2 as
//   published by the Free Software Foundation.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public
//   License along with this program; if not, write to the Free Software
//   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//

#include <QObject>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>

#include "mysqlconfig.h"

Config::Config()
{
}


QString Config::mysqlHostname() const
{
  return config_mysql_hostname;
}


QString Config::mysqlDbname() const
{
  return config_mysql_dbname;
}


QString Config::mysqlUsername() const
{
  return config_mysql_username;
}


QString Config::mysqlPassword() const
{
  return config_mysql_password;
}


bool Config::openDb(QString *err_msg)
{
  QSettings s(GLASSNET_CONF_FILE,QSettings::IniFormat);

  config_mysql_hostname=
    s.value("MysqlHostname",GLASSNET_DEFAULT_MYSQL_HOSTNAME).toString();
  config_mysql_dbname=
    s.value("MysqlDbname",GLASSNET_DEFAULT_MYSQL_DBNAME).toString();
  config_mysql_username=
    s.value("MysqlUsername",GLASSNET_DEFAULT_MYSQL_USERNAME).toString();
  config_mysql_password=
    s.value("MysqlPassword",GLASSNET_DEFAULT_MYSQL_PASSWORD).toString();

  QSqlDatabase db=QSqlDatabase::addDatabase("QMYSQL3");
  if(!db.isValid()) {
    *err_msg=QObject::tr("Unable to add MySQL database");
    return false;
  }
  db.setHostName(mysqlHostname());
  db.setDatabaseName(mysqlDbname());
  db.setUserName(mysqlUsername());
  db.setPassword(mysqlPassword());
  if(!db.open()) {
    *err_msg=db.lastError().text();
    return false;
  }

  *err_msg=QObject::tr("OK");
  return true;
}