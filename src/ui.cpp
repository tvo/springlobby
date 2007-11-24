/* Copyright (C) 2007 The SpringLobby Team. All rights reserved. */
//
// Class: Ui
//

#include <wx/msgdlg.h>
#include <wx/textdlg.h>
#include <stdexcept>
#include <wx/msgdlg.h>
#include <wx/thread.h>
#include <wx/intl.h>
#include <wx/utils.h>

#include "spring.h"
#include "ui.h"
#include "tasserver.h"
#include "settings.h"
#include "server.h"
#include "spring.h"
#include "channel.h"
#include "utils.h"
#include "connectwindow.h"
#include "mainwindow.h"
#include "user.h"
#include "chatpanel.h"
#include "battlelisttab.h"
#include "battleroomtab.h"
#include "socket.h"
#include "battle.h"
#include "mainchattab.h"
#include "mainjoinbattletab.h"
#include "agreementdialog.h"


Ui::Ui() :
  m_serv(0),
  m_main_win(0),
  m_con_win(0)
{
  m_main_win = new MainWindow( *this );
  m_spring = new Spring(*this);
}

Ui::~Ui() {
  Disconnect();
  delete m_main_win;
  delete m_spring;
}

Server& Ui::GetServer()
{
  ASSERT_LOGIC( m_serv != 0, "m_serv NULL!" );
  return *m_serv;
}

bool Ui::GetServerStatus() const
{
  return (bool)(m_serv);
}


ChatPanel* Ui::GetActiveChatPanel()
{
  return mw().GetActiveChatPanel();
}


MainWindow& Ui::mw()
{
  ASSERT_LOGIC( m_main_win != 0, "m_main_win = 0" );
  return *m_main_win;
}

//! @brief Shows the main window on screen
void Ui::ShowMainWindow()
{
  ASSERT_LOGIC( m_main_win != 0, "m_main_win = 0" );
  mw().Show(true);
}


//! @brief Show the connect window on screen
//!
//! @note It will create the ConnectWindow if not allready created
void Ui::ShowConnectWindow()
{
  if ( m_con_win == 0 ) {
    ASSERT_LOGIC( m_main_win != 0, "m_main_win = 0" );
    m_con_win = new ConnectWindow( m_main_win, *this );
  }
  m_con_win->CenterOnParent();
  m_con_win->Show(true);
}


//! @brief Connects to default server or opens the ConnectWindow
//!
//! @todo Fix Auto Connect
//! @see DoConnect
void Ui::Connect()
{
  ShowConnectWindow();
}


void Ui::Reconnect()
{
  std::string servname = sett().GetDefaultServer();

  std::string pass  = sett().GetServerAccountPass(servname);
  if ( !sett().GetServerAccountSavePass(servname) ) {
    wxString pass2 = WX_STRING(pass);
    if ( !AskPassword( _("Server password"), _("Password"), pass2 ) ) return;
    pass = STD_STRING(pass2);
  }

  if ( IsConnected() ) Disconnect();
  DoConnect( WX_STRING(servname), WX_STRING(sett().GetServerAccountNick(servname)), WX_STRING(pass) );
}


void Ui::Disconnect()
{
  if ( m_serv != 0 ) {
    m_serv->Disconnect();
    Socket* sock = m_serv->GetSocket();
    m_serv->SetSocket( 0 );
    delete sock; sock = 0;
    delete m_serv; m_serv = 0;
  }
}


//! @brief Opens the accutial connection to a server.
void Ui::DoConnect( const wxString& servername, const wxString& username, const wxString& password )
{
  std::string host;
  int port;
  Socket* sock;

  if ( !sett().ServerExists( STD_STRING(servername) ) ) {
    ASSERT_LOGIC( false, "Server does not exist in settings" );
    return;
  }

  Disconnect();

  // Create new Server object
  m_serv = new TASServer( *this );
  sock = new Socket( *m_serv );
  m_serv->SetSocket( sock );
  //m_serv->SetServerEvents( &se() );

  m_serv->SetUsername( STD_STRING(username) );
  m_serv->SetPassword( STD_STRING(password) );

  if ( sett().GetServerAccountSavePass( STD_STRING(servername) ) ) {
    if ( m_serv->IsPasswordHash(STD_STRING(password)) ) sett().SetServerAccountPass( STD_STRING(servername), STD_STRING(password) );
    else sett().SetServerAccountPass( STD_STRING(servername), m_serv->GetPasswordHash( STD_STRING(password) ) );
  } else {
    sett().SetServerAccountPass( STD_STRING(servername), "" );
  }
  sett().SaveSettings();

  host = sett().GetServerHost( STD_STRING(servername) );
  port = sett().GetServerPort( STD_STRING(servername) );

  m_serv->uidata.panel = m_main_win->GetChatTab().AddChatPannel( *m_serv, servername );
  m_serv->uidata.panel->StatusMessage( _T("Connecting to server ") + servername + _T("...") );

  // Connect
  m_serv->Connect( host, port );

}


bool Ui::DoRegister( const wxString& servername, const wxString& username, const wxString& password )
{
  std::string host;
  int port;

  if ( !sett().ServerExists( STD_STRING(servername) ) ) {
    ASSERT_LOGIC( false, "Server does not exist in settings" );
    return false;
  }

  // Create new Server object
  TASServer* serv = new TASServer( *this );
  Socket* sock = new Socket( *serv, true );
  serv->SetSocket( sock );

  host = sett().GetServerHost( STD_STRING(servername) );
  port = sett().GetServerPort( STD_STRING(servername) );

  return serv->Register( host, port, STD_STRING(username), STD_STRING(password) );

}


bool Ui::IsConnected() const
{
  if ( m_serv != 0 )
    return m_serv->IsConnected();
  return false;
}

void Ui::JoinChannel( const wxString& name, const wxString& password )
{
  if ( m_serv != 0 )
    m_serv->JoinChannel( STD_STRING(name), STD_STRING(password) );
}


void Ui::StartHostedBattle()
{
  ASSERT_LOGIC( m_serv != 0, "m_serv = 0" );
  m_serv->StartHostedBattle();
  sett().SetLastHostMap( STD_STRING(m_serv->GetCurrentBattle()->GetMapName()) );
}


void Ui::StartSinglePlayerGame( SinglePlayerBattle& battle )
{
  m_spring->Run( battle );
}


bool Ui::IsSpringRunning()
{
   return m_spring->IsRunning();
}


//! @brief Quits the entire application
void Ui::Quit()
{
  ASSERT_LOGIC( m_main_win != 0, "m_main_win = 0" );
  sett().SaveSettings();
  m_main_win->Close();
}


void Ui::ReloadUnitSync()
{
  usync()->FreeUnitSyncLib();
  usync()->LoadUnitSyncLib( WX_STRING(sett().GetSpringDir()), WX_STRING(sett().GetUnitSyncUsedLoc()) );
  if ( m_main_win != 0 ) m_main_win->OnUnitSyncReloaded();
}


void Ui::DownloadMap( const wxString& map )
{
  wxString mapname = map;
  mapname = mapname.SubString(0, mapname.Find( '.', true ) - 1 );
  mapname.Replace(_T(" "), _T("*") );
  mapname.Replace(_T("-"), _T("*") );
  mapname.Replace(_T("_"), _T("*") );
  wxString url = _T("http://www.unknown-files.net/spring/search/") + mapname + _T("/filename/");
  OpenWebBrowser ( url );
}


void Ui::DownloadMod( const wxString& mod )
{
  wxString modname = mod;
  //all the following manipulation is necessary because the publish name on UF doesn't necessary reflect the file name
  //and the mod filename isn't accessible trought unitsync
  modname.Replace(_T(" "), _T("*") );
  modname.Replace(_T("-"), _T("*") );
  modname.Replace(_T("_"), _T("*") );
  modname.Replace(_T("VERSION"), _T("*") );
  modname.Replace(_T("Version"), _T("*") );
  modname.Replace(_T("version"), _T("*") );
  modname.Replace(_T("VER"), _T("*") );
  modname.Replace(_T("Ver"), _T("*") );
  modname.Replace(_T("ver"), _T("*") );
  modname.Replace(_T("V"), _T("*") );
  modname.Replace(_T("v"), _T("*") );
  modname.Replace(_T("."), _T("*") );
  modname.Replace(_T("ALPHA"), _T("*") );
  modname.Replace(_T("Alpha"), _T("*") );
  modname.Replace(_T("alpha"), _T("*") );
  modname.Replace(_T("BETA"), _T("*") );
  modname.Replace(_T("Beta"), _T("*") );
  modname.Replace(_T("beta"), _T("*") );
  wxString url = _T("http://www.unknown-files.net/spring/search/") + modname + _T("/");
  OpenWebBrowser ( url );
}


void Ui::OpenWebBrowser( const wxString& url )
{
  if ( sett().GetWebBrowserPath() == _T("use default") || sett().GetWebBrowserPath().IsEmpty() )
  {
      if ( !wxLaunchDefaultBrowser( url ) ) wxMessageBox( _("Couldn't launch browser. URL is: ") + url, _("Couldn't launch browser.")  );
  }
  else
  {
    if ( !wxExecute ( sett().GetWebBrowserPath() + _T(" ") + url, wxEXEC_ASYNC ) ) wxMessageBox( _("Couldn't launch browser. URL is: ") + url + _("\nBroser path is: ") + sett().GetWebBrowserPath(), _("Couldn't launch browser.")  );
  }
}


//! @brief Display a dialog asking a question with OK and Canel buttons
//!
//! @return true if OK button was pressed
//! @note this does not return until the user pressed any of the buttons or closed the dialog.
bool Ui::Ask( const wxString& heading, const wxString& question )
{
  wxMessageDialog ask_dlg( &mw(), question, heading, wxYES_NO );
  return ( ask_dlg.ShowModal() == wxID_YES );
}


bool Ui::AskPassword( const wxString& heading, const wxString& message, wxString& password )
{
  wxPasswordEntryDialog pw_dlg( &mw(), message, heading, password );
  int res = pw_dlg.ShowModal();
  password = pw_dlg.GetValue();
  return ( res == wxID_OK );
}


bool Ui::AskText( const wxString& heading, const wxString& question, wxString& answer )
{
  wxTextEntryDialog name_dlg( &mw(), question, heading, answer, wxOK | wxCANCEL | wxCENTRE );
  int res = name_dlg.ShowModal();
  answer = name_dlg.GetValue();

  return ( res == wxID_OK );
}


void Ui::ShowMessage( const wxString& heading, const wxString& message )
{
  wxMessageDialog msg( &mw(), message, heading, wxOK);
  msg.ShowModal();
}


bool Ui::ExecuteSayCommand( const wxString& cmd )
{

  if ( !IsConnected() ) return false;
  if ( (cmd.BeforeFirst(' ').Lower() == _T("/join")) || (cmd.BeforeFirst(' ').Lower() == _T("/j")) ) {
    wxString channel = cmd.AfterFirst(' ');
    wxString pass = channel.AfterFirst(' ');
    if ( !pass.IsEmpty() ) channel = channel.BeforeFirst(' ');
    if ( channel.StartsWith(_T("#")) ) channel.Remove( 0, 1 );

    m_serv->JoinChannel( STD_STRING(channel), STD_STRING(pass) );
    return true;
  } else if ( cmd.BeforeFirst(' ').Lower() == _T("/away") ) {
    m_serv->GetMe().Status().away = true;
    m_serv->GetMe().SendMyUserStatus();
    return true;
  } else if ( cmd.BeforeFirst(' ').Lower() == _T("/back") ) {
    if ( IsConnected() ) {
      m_serv->GetMe().Status().away = false;
      m_serv->GetMe().SendMyUserStatus();
      return true;
    }
  } else if ( cmd.BeforeFirst(' ').Lower() == _T("/ingame") ) {
    if ( cmd.AfterFirst(' ') != wxEmptyString ) return false;
    m_serv->RequestInGameTime();
    return true;
  } else if ( cmd.BeforeFirst(' ').Lower() == _T("/help") ) {
    wxString topic = cmd.AfterFirst(' ');
    ConsoleHelp( topic.Lower() );
    return true;
  }
  return false;
}


void Ui::ConsoleHelp( const wxString& topic )
{
  ChatPanel* panel = GetActiveChatPanel();
  if ( panel == 0 ) {
    ShowMessage( _("Help error"), _("Type /help in a chat box.") );
    return;
  }
  if ( topic == wxEmptyString ) {
    panel->ClientMessage( _("SpringLobby commands help.") );
    panel->ClientMessage( _("") );
    panel->ClientMessage( _("Global commands:") );
    panel->ClientMessage( _("  \"/away\" - Sets your status to away.") );
    panel->ClientMessage( _("  \"/back\" - Resets your away status.") );
    panel->ClientMessage( _("  \"/help [topic]\" - Put topic if you want to know more specific information about a command.") );
    panel->ClientMessage( _("  \"/join channel [password] [,channel2 [password2]]\" - Join a channel.") );
    panel->ClientMessage( _("  \"/j\" - Alias to /join.") );
    panel->ClientMessage( _("  \"/ingame\" - Show how much time you have in game.") );
    panel->ClientMessage( _("  \"/rename newalias\" - Changes your nickname to newalias.") );
    panel->ClientMessage( _("  \"/sayver\" - Say what version of springlobby you have in chat.") );
    panel->ClientMessage( _("  \"/ver\" - Display what version of SpringLobby you have.") );
    panel->ClientMessage( _("") );
    panel->ClientMessage( _("Chat commands:") );
    panel->ClientMessage( _("  \"/me action\" - Say IRC style action message.") );
    panel->ClientMessage( _("") );
    panel->ClientMessage( _("If you are missing any commands, go to #springlobby and try to type it there :)") );
//    panel->ClientMessage( _("  \"/\" - .") );
  } else if ( topic == _T("topics") ) {
    panel->ClientMessage( _("No topics written yet.") );
  } else {
    panel->ClientMessage( _("The topic \"") + topic + _("\" was not found. Type \"/help topics\" only for available topics.") );
  }
}


ChatPanel* Ui::GetChannelChatPanel( const wxString& channel )
{
  return mw().GetChannelChatPanel( channel );
}


////////////////////////////////////////////////////////////////////////////////////////////
// EVENTS
////////////////////////////////////////////////////////////////////////////////////////////


void Ui::OnUpdate( int mselapsed )
{
  if ( m_serv != 0 ) {
    m_serv->Update( mselapsed );
  }
}


//! @brief Called when connected to a server
//!
//! @todo Display in servertab
void Ui::OnConnected( Server& server, const std::string& server_name, const std::string& server_ver, bool supported )
{
  debug_func( "" );

  m_main_win->EnableChatTab();
  m_main_win->EnableMultiplayerTab();

  if ( !IsSpringCompatible () ){
    if ( m_spring->TestSpringBinary() ) {
      wxString message = _("Your spring version");
      message += _T(" (") + WX_STRING( usync()->GetSpringVersion() ) + _T(") ");
      message +=  _("is not supported by the lobby server that requires version");
      message += _T(" (") +  WX_STRING( m_server_spring_ver ) + _T(").\n\n");
      message += _("Online play will be disabled.");
      wxMessageBox ( message, _("Spring error"), wxICON_EXCLAMATION );
    } else {
      wxMessageBox( _("Couldn't get your spring version.\n\nOnline play will be disabled."), _("Spring error"), wxICON_EXCLAMATION );
    }
    m_main_win->DisableMultiplayerTab();
  }
  server.uidata.panel->StatusMessage( _T("Connected to ") + WX_STRING(server_name) + _T(".") );

  //server.uidata.panel = m_main_win->GetChatTab().AddChatPannel( server, WX_STRING(server_name) );
}


bool Ui::IsSpringCompatible( )
{
  if ( !m_spring->TestSpringBinary() ) return false;
  if ( m_server_spring_ver == "*" ) return true; // Server accepts any version.
  if ( (usync()->GetSpringVersion() == m_server_spring_ver ) && ( m_server_spring_ver != "" ) ) return true;
  else return false;
}


void Ui::OnLoggedIn( )
{
  mw().GetChatTab().RejoinChannels();
}


void Ui::OnDisconnected( Server& server )
{
  debug_func( "" );
  if ( m_main_win == 0 ) return;

  mw().GetJoinTab().LeaveCurrentBattle();
  mw().GetJoinTab().GetBattleListTab().RemoveAllBattles();

  if ( server.uidata.panel != 0 ) {
    server.uidata.panel->StatusMessage( _T("Disconnected from server.") );
    server.uidata.panel->SetServer( 0 );
    server.uidata.panel = 0;
  }
  mw().GetChatTab().CloseAllChats();

  m_main_win->DisableChatTab();
  m_main_win->DisableMultiplayerTab();

}


//! @brief Called when client has joined a channel
//!
//! @todo Check if a pannel allready exists for this channel
void Ui::OnJoinedChannelSuccessful( Channel& chan )
{
  debug_func( "" );

  chan.uidata.panel = 0;
  m_main_win->OpenChannelChat( chan );
  if ( chan.GetName() == "springlobby" ) {
    chan.uidata.panel->ClientMessage( wxEmptyString );
    chan.uidata.panel->ClientMessage( _("This is the SpringLobby channel, please report any problems you are having with SpringLobby here and the friendly developers will help you.") );
    chan.uidata.panel->ClientMessage( wxEmptyString );
  }
}


//! @brief Called when something is said in a channel
void Ui::OnChannelSaid( Channel& channel, User& user, const std::string& message )
{
  debug_func( "" );
  if ( channel.uidata.panel == 0 ) {
    debug_error( "ud->panel NULL" );
    return;
  }
  channel.uidata.panel->Said( WX_STRING(user.GetNick()), WX_STRING( message ) );
}


void Ui::OnChannelDidAction( Channel& channel , User& user, const std::string& action )
{
  debug_func( "" );
  if ( channel.uidata.panel == 0 ) {
    debug_error( "ud->panel NULL" );
    return;
  }
  channel.uidata.panel->DidAction( WX_STRING(user.GetNick()), WX_STRING( action ) );
}


void Ui::OnChannelMessage( const std::string& channel, const std::string& msg )
{
  ChatPanel* panel = GetChannelChatPanel( WX_STRING(channel) );
  if ( panel != 0 ) {
    panel->StatusMessage( WX_STRING(msg) );
  }
}


//! @brief Called when client is leaving a channel
//!
//! @todo Tell ChatPanel the channel is no longer joined
void Ui::OnLeaveChannel( Channel& channel )
{

}


void Ui::OnUserJoinedChannel( Channel& chan, User& user )
{
  //debug_func( "" );
  if ( chan.uidata.panel == 0 ) {
    debug_error( "ud->panel NULL" );
    return;
  }
  chan.uidata.panel->Joined( user );
}


void Ui::OnUserLeftChannel( Channel& chan, User& user, const std::string& reason )
{
  //debug_func( "" );
  if ( chan.uidata.panel == 0 ) {
    debug_error( "ud->panel NULL" );
    return;
  }
  chan.uidata.panel->Parted( user, WX_STRING(reason) );
}


void Ui::OnChannelTopic( Channel& channel , const std::string user, const std::string& topic )
{
  debug_func( "" );
  if ( channel.uidata.panel == 0 ) {
    debug_error( "ud->panel NULL" );
    return;
  }
  channel.uidata.panel->SetTopic( WX_STRING(user), WX_STRING(topic) );
}


void Ui::OnChannelList( const std::string& channel, const int& numusers )
{

}


void Ui::OnUserOnline( User& user )
{
/*  UiUserData* data = new UiUserData();
  data->panel = 0;

  user.SetUserData( (void*)data );*/

  m_main_win->GetChatTab().OnUserConnected( user );
}


void Ui::OnUserOffline( User& user )
{
  m_main_win->GetChatTab().OnUserDisconnected( user );
  if ( user.uidata.panel ) {
    user.uidata.panel->SetUser( 0 );
    user.uidata.panel = 0;
  }
/*  UiUserData* data = (UiUserData*)user.GetUserData();
  if ( data == 0) return;

  delete data;*/
}


void Ui::OnUserStatusChanged( User& user )
{
  for ( int i = 0; i < m_serv->GetNumChannels(); i++ ) {
    Channel& chan = m_serv->GetChannel( i );
    if ( ( chan.UserExists(user.GetNick()) ) && ( chan.uidata.panel != 0 ) ) {
      chan.uidata.panel->UserStatusUpdated( user );
    }
  }
  if ( user.GetStatus().in_game ) mw().GetJoinTab().GetBattleListTab().UserUpdate( user );
}


void Ui::OnUnknownCommand( Server& server, const std::string& command, const std::string& params )
{
  if ( server.uidata.panel != 0 ) server.uidata.panel->UnknownCommand( WX_STRING(command), WX_STRING(params) );
}


void Ui::OnMotd( Server& server, const std::string& message )
{
  if ( server.uidata.panel != 0 ) server.uidata.panel->Motd( WX_STRING(message) );
}


void Ui::OnServerMessage( Server& server, const std::string& message )
{
  ChatPanel* panel = GetActiveChatPanel();
  if ( panel != 0 ) {
    panel->StatusMessage( WX_STRING(message) );
  } else {
    ShowMessage( _("Server message"), WX_STRING(message) );
  }
}


void Ui::OnUserSaid( User& user, const std::string message, bool fromme )
{
  if ( user.uidata.panel == 0 ) {
    m_main_win->OpenPrivateChat( user );
  }
  if ( fromme ) user.uidata.panel->Said( WX_STRING(m_serv->GetMe().GetNick()), WX_STRING(message) );
  else user.uidata.panel->Said( WX_STRING(user.GetNick()), WX_STRING(message) );
}


void Ui::OnBattleOpened( Battle& battle )
{
  mw().GetJoinTab().GetBattleListTab().AddBattle( battle );
  User& user = battle.GetFounder();
  for ( int i = 0; i < m_serv->GetNumChannels(); i++ ) {
    Channel& chan = m_serv->GetChannel( i );
    if ( ( chan.UserExists(user.GetNick()) ) && ( chan.uidata.panel != 0 ) ) {
      chan.uidata.panel->UserStatusUpdated( user );
    }
  }
}


void Ui::OnBattleClosed( Battle& battle )
{
  mw().GetJoinTab().GetBattleListTab().RemoveBattle( battle );
  BattleRoomTab* br = mw().GetJoinTab().GetBattleRoomTab();
  if ( br != 0 ) {
    if ( &br->GetBattle() == &battle ) mw().GetJoinTab().LeaveCurrentBattle();
  }
  for ( unsigned int b = 0; b < battle.GetNumUsers(); b++ ) {
    User& user = battle.GetUser( b );
    user.SetBattle(0);
    for ( int i = 0; i < m_serv->GetNumChannels(); i++ ) {
      Channel& chan = m_serv->GetChannel( i );
      if ( ( chan.UserExists(user.GetNick()) ) && ( chan.uidata.panel != 0 ) ) {
        chan.uidata.panel->UserStatusUpdated( user );
      }
    }
  }
}


void Ui::OnUserJoinedBattle( Battle& battle, User& user )
{
  mw().GetJoinTab().GetBattleListTab().UpdateBattle( battle );

  BattleRoomTab* br = mw().GetJoinTab().GetBattleRoomTab();
  if ( br != 0 ) {
    if ( &br->GetBattle() == &battle ) br->OnUserJoined( user );
  }
  for ( int i = 0; i < m_serv->GetNumChannels(); i++ ) {
    Channel& chan = m_serv->GetChannel( i );
    if ( ( chan.UserExists(user.GetNick()) ) && ( chan.uidata.panel != 0 ) ) {
      chan.uidata.panel->UserStatusUpdated( user );
    }
  }
}


void Ui::OnUserLeftBattle( Battle& battle, User& user )
{
  mw().GetJoinTab().GetBattleListTab().UpdateBattle( battle );
  BattleRoomTab* br = mw().GetJoinTab().GetBattleRoomTab();
  if ( br != 0 ) {
    if ( &br->GetBattle() == &battle ) {
      br->OnUserLeft( user );
      if ( &user == &m_serv->GetMe() ) mw().GetJoinTab().LeaveCurrentBattle();
    }
  }
  for ( int i = 0; i < m_serv->GetNumChannels(); i++ ) {
    Channel& chan = m_serv->GetChannel( i );
    if ( ( chan.UserExists(user.GetNick()) ) && ( chan.uidata.panel != 0 ) ) {
      chan.uidata.panel->UserStatusUpdated( user );
    }
  }
}


void Ui::OnBattleInfoUpdated( Battle& battle )
{
  m_main_win->GetJoinTab().GetBattleListTab().UpdateBattle( battle );
  if ( m_main_win->GetJoinTab().GetCurrentBattle() == &battle ) {
    mw().GetJoinTab().UpdateCurrentBattle();
  }
}


void Ui::OnJoinedBattle( Battle& battle )
{
  mw().GetJoinTab().JoinBattle( battle );
  /*if ( !Spring::TestSpringBinary() ) {
    wxMessageBox( _("Your spring settings are probably not configured correctly,\nyou should take another look at your settings before trying\nto play online."), _("Spring settings error"), wxOK );
  }*/
  if ( battle.GetNatType() != NAT_None ) {
    wxMessageBox( _("This game uses NAT traversal that is not yet supported\nby SpringLobby.\n\nYou will not be able to play in this battle."), _("NAT traversal"), wxOK );
  }
}


void Ui::OnHostedBattle( Battle& battle )
{
  mw().GetJoinTab().HostBattle( battle );
}


void Ui::OnUserBattleStatus( Battle& battle, User& user )
{
  mw().GetJoinTab().BattleUserUpdated( user );
}


void Ui::OnRequestBattleStatus( Battle& battle )
{
  BattleRoomTab* br = mw().GetJoinTab().GetBattleRoomTab();
  if ( br != 0 ) {
    if ( &br->GetBattle() == &battle ) {
      br->GetBattle().OnRequestBattleStatus();
    }
  }
}


void Ui::OnBattleStarted( Battle& battle )
{
  debug_func("");
  BattleRoomTab* br = mw().GetJoinTab().GetBattleRoomTab();
  if ( br != 0 ) {
    if ( &br->GetBattle() == &battle ) {
      battle.GetMe().BattleStatus().ready = false;
      battle.SendMyBattleStatus();
      battle.GetMe().Status().in_game = true;
      battle.GetMe().SendMyUserStatus();
      m_spring->Run( battle );
    }
  }
  mw().GetJoinTab().GetBattleListTab().UpdateBattle( battle );
}


void Ui::OnSaidBattle( Battle& battle, const std::string& nick, const std::string& msg )
{
  BattleRoomTab* br = mw().GetJoinTab().GetBattleRoomTab();
  if ( br != 0 ) {
    br->GetChatPanel().Said( WX_STRING(nick), WX_STRING(msg) );
  }
}


void Ui::OnBattleAction( Battle& battle, const std::string& nick, const std::string& msg )
{
  BattleRoomTab* br = mw().GetJoinTab().GetBattleRoomTab();
  if ( br != 0 ) {
    br->GetChatPanel().DidAction( WX_STRING(nick), WX_STRING(msg) );
  }
}


void Ui::OnSpringTerminated( bool success )
{
  if ( m_serv == 0 ) return;

  m_serv->GetMe().Status().in_game = false;
  m_serv->GetMe().SendMyUserStatus();
}


void Ui::OnBattleStartRectsUpdated( Battle& battle )
{
  mw().GetJoinTab().UpdateCurrentBattle();
}


void Ui::OnBattleDisableUnit( Battle& battle, const std::string& unitname )
{
  BattleRoomTab* br = mw().GetJoinTab().GetBattleRoomTab();
  if ( br != 0 ) {
    //std::string fullname = usync()->GetFullUnitName( battle.opts().modname, usync()->GetUnitIndex( battle.opts().modname, unitname ) );
    br->GetChatPanel().StatusMessage( WX_STRING( unitname ) + _T(" disabled.") );
  }
  //mw().GetJoinTab().UpdateCurrentBattle();
  mw().GetJoinTab().UpdateCurrentBattle(true);
}


void Ui::OnBattleEnableUnit( Battle& battle, const std::string& unitname )
{
  BattleRoomTab* br = mw().GetJoinTab().GetBattleRoomTab();
  if ( br != 0 ) {
    br->GetChatPanel().StatusMessage( WX_STRING(unitname) + _T(" disabled.") );
  }
  //mw().GetJoinTab().UpdateCurrentBattle();
  mw().GetJoinTab().UpdateCurrentBattle(true);
}


void Ui::OnBattleEnableAllUnits( Battle& battle )
{
  BattleRoomTab* br = mw().GetJoinTab().GetBattleRoomTab();
  if ( br != 0 ) {
    br->GetChatPanel().StatusMessage( _T("All units enabled.") );
  }
  mw().GetJoinTab().UpdateCurrentBattle(true);
}


void Ui::OnAcceptAgreement( const std::string& agreement )
{
  AgreementDialog dlg( m_main_win, WX_STRING(agreement) );
  if ( dlg.ShowModal() == 1 ) {
    m_serv->AcceptAgreement();
    m_serv->Login();
  }
}


void Ui::OnBattleBotAdded( Battle& battle, BattleBot& bot )
{
  BattleRoomTab* br = mw().GetJoinTab().GetBattleRoomTab();
  if ( br != 0 ) {
    if ( &br->GetBattle() == &battle ) br->OnBotAdded( bot );
  }
}


void Ui::OnBattleBotRemoved( Battle& battle, BattleBot& bot )
{
  BattleRoomTab* br = mw().GetJoinTab().GetBattleRoomTab();
  if ( br != 0 ) {
    if ( &br->GetBattle() == &battle ) br->OnBotRemoved( bot );
  }
}


void Ui::OnBattleBotUpdated( Battle& battle, BattleBot& bot )
{
  BattleRoomTab* br = mw().GetJoinTab().GetBattleRoomTab();
  if ( br != 0 ) {
    if ( &br->GetBattle() == &battle ) br->OnBotUpdated( bot );
  }
}


void Ui::OnRing( const std::string& from )
{
  m_main_win->RequestUserAttention();
  wxBell();
}
