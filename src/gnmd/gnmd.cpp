// gnmd.cpp
//
// gncd(1) management daemon for GlassNet
//
//   (C) Copyright 2016 Fred Gleason <fredg@paravelsystems.com>
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

#include <signal.h>
#include <syslog.h>

#include <QCoreApplication>

#include <wh/whcmdswitch.h>

#include "gnmd.h"
#include "db.h"
#include "paths.h"
#include "receiver.h"

//
// Globals
//
bool global_is_exiting=false;

void SigHandler(int signo)
{
  switch(signo) {
  case SIGINT:
  case SIGTERM:
    global_is_exiting=true;
    break;
  }
}


MainObject::MainObject(QObject *parent)
  : QObject(parent)
{
  QString err_msg;
  WHCmdSwitch *cmd=
    new WHCmdSwitch(qApp->argc(),qApp->argv(),"gnmd",VERSION,
		    GNMD_USAGE);
  for(unsigned i=0;i<(cmd->keys());i++) {
    if(!cmd->processed(i)) {
      fprintf(stderr,"gnmd: unknown option\n");
      exit(256);
    }
  }

  //
  // Open Syslog
  //
  openlog("gnmd",LOG_PERROR,LOG_DAEMON);

  //
  // Open Database
  //
  gnmd_config=new Config();
  if(!gnmd_config->openDb(&err_msg,true)) {
    syslog(LOG_ERR,"unable to open database [%s]",
	   (const char *)err_msg.toUtf8());
    exit(256);
  }
  DbHeartbeat(this);

  //
  // Initialize Receiver Status
  //
  InitReceivers();

  //
  // Command Server
  //
  QTcpServer *server=new QTcpServer(this);
  server->listen(QHostAddress::Any,gnmd_config->receiverCallbackPort());
  std::map<int,QString> cmds;
  std::map<int,int> upper_limits;
  std::map<int,int> lower_limits;

  cmds[MainObject::Exit]="EXIT";
  upper_limits[MainObject::Exit]=0;
  lower_limits[MainObject::Exit]=0;

  cmds[MainObject::Addr]="ADDR";
  upper_limits[MainObject::Addr]=3;
  lower_limits[MainObject::Addr]=3;

  gnmd_cmd_server=
    new StreamCmdServer(cmds,upper_limits,lower_limits,server,this);
  connect(gnmd_cmd_server,SIGNAL(commandReceived(int,int,const QStringList &)),
	  this,SLOT(commandReceivedData(int,int,const QStringList &)));
  connect(gnmd_cmd_server,SIGNAL(disconnected(int)),
	  this,SLOT(receiverDisconnectedData(int)));

  //
  // Signals
  //
  gnmd_exit_timer=new QTimer(this);
  connect(gnmd_exit_timer,SIGNAL(timeout()),this,SLOT(exitData()));
  gnmd_exit_timer->start(100);
  ::signal(SIGINT,SigHandler);
  ::signal(SIGTERM,SigHandler);
}


void MainObject::commandReceivedData(int id,int cmd,const QStringList &args)
{
  /*
  printf("CMD: %d\n",cmd);
  for(int i=0;i<args.size();i++) {
    printf("[%d]: %s\n",i,(const char *)args[i].toUtf8());
  }
  printf("\n");
  */
  switch((MainObject::ReceiverCommands)cmd) {
  case MainObject::Exit:
    CloseReceiverConnection(id);
    break;

  case MainObject::Addr:
    if(!ProcessAddr(id,args)) {
      CloseReceiverConnection(id);
    }
    break;
  }
}


void MainObject::receiverDisconnectedData(int id)
{
  for(std::map<int,ReceiverConnection *>::iterator it=
	gnmd_rcvr_connections.begin();
      it!=gnmd_rcvr_connections.end();it++) {
    if(it->first==id) {
      QString sql=QString("update RECEIVERS set ")+
	"ONLINE=0,"+
	"INTERFACE_ADDRESS=null,"+
	"PUBLIC_ADDRESS=null where "+
	"MAC_ADDRESS='"+it->second->macAddress()+"'";
      SqlQuery::run(sql);
      delete it->second;
      gnmd_rcvr_connections.erase(it);
      break;
    }
  }
}


void MainObject::exitData()
{
  if(global_is_exiting) {
    InitReceivers();
    exit(0);
  }
}


bool MainObject::ProcessAddr(int id,const QStringList &args)
{
  ReceiverConnection *conn=NULL;
  QHostAddress addr;

  //
  // Validate Arguments
  //
  if(!Receiver::isMacAddress(args[0])) {
    return false;
  }
  if(!addr.setAddress(args[1])) {
    return false;
  }
  if(args[2].split(".").size()!=3) {
    return false;
  }

  QString sql=QString("select ID from RECEIVERS where ")+
    "MAC_ADDRESS='"+SqlQuery::escape(args[0])+"'";
  if(SqlQuery::rows(sql)==0) {
    return false;
  }
  conn=GetReceiverConnection(id,args[0]);
  sql=QString("update RECEIVERS set ")+
    "ONLINE=1,"+
    "FIRMWARE_VERSION='"+args[2]+"',"+
    "INTERFACE_ADDRESS='"+args[1]+"',"+
    "PUBLIC_ADDRESS='"+gnmd_cmd_server->peerAddress(id).toString()+"',"+
    "LAST_SEEN=now() where "+
    "MAC_ADDRESS='"+conn->macAddress()+"'";
  SqlQuery::run(sql);

  return true;
}


ReceiverConnection *MainObject::GetReceiverConnection(int id,const QString &mac)
{
  ReceiverConnection *conn=NULL;

  try {
    conn=gnmd_rcvr_connections.at(id);
  }
  catch(...) {
    if(!mac.isEmpty()) {
      conn=new ReceiverConnection(mac);
      gnmd_rcvr_connections[id]=conn;
    }
  }

  return conn;
}


void MainObject::CloseReceiverConnection(int id)
{
  gnmd_cmd_server->closeConnection(id);
  receiverDisconnectedData(id);
}


void MainObject::InitReceivers() const
{
  QString sql=QString("update RECEIVERS set ")+
    "ONLINE=0,"+
    "INTERFACE_ADDRESS=null,"+
    "PUBLIC_ADDRESS=null";
  SqlQuery::run(sql);
}


int main(int argc,char *argv[])
{
  QCoreApplication a(argc,argv);
  new MainObject();
  return a.exec();
}
