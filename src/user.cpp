/* Copyright (C) 2007 The SpringLobby Team. All rights reserved. */
//
// Class: User
//

#include "user.h"
#include "battle.h"
#include "server.h"


void User::Said( const std::string& message )
{
}


void User::Say( const std::string& message )
{
  m_serv.SayPrivate( m_nick, message );
}


void User::DoAction( const std::string& message )
{
  m_serv.DoActionPrivate( m_nick, message );
}


Battle* User::GetBattle()
{
  return m_battle;
}


void User::SetBattle( Battle* battle )
{
  m_battle = battle;
}

void User::SetStatus( const UserStatus& status )
{
  m_status = status;
  // If user is host of a game, then his in_game status tells if the game is on!
  if ( m_battle != 0 ) {
    User& user = m_battle->GetFounder();
    if ( user.GetNick() == m_nick ) {
      m_battle->Update();
    }
  }

}

void User::SetBattleStatus( const UserBattleStatus& status, bool setorder )
{
  int order = m_bstatus.order;
  m_bstatus = status;
  if ( !setorder ) m_bstatus.order = order;
}


void User::SendMyUserStatus()
{
  m_serv.SendMyUserStatus();
}
