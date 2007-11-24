/* Copyright (C) 2007 The SpringLobby Team. All rights reserved. */
//
// Class: NickListCtrl
//

#include <wx/imaglist.h>
#include <wx/menu.h>
#include <wx/string.h>
#include <wx/intl.h>
#include <stdexcept>

#include "nicklistctrl.h"
#include "utils.h"
#include "iconimagelist.h"
#include "user.h"
#include "ui.h"
#include "mainwindow.h"


int wxCALLBACK NickListSortCallback(long item1, long item2, long sortData);

BEGIN_EVENT_TABLE( NickListCtrl, wxListCtrl )
  EVT_LIST_ITEM_ACTIVATED( NICK_LIST, NickListCtrl::OnActivateItem )
  EVT_LIST_COL_CLICK( NICK_LIST, NickListCtrl::OnColClick )
  EVT_CONTEXT_MENU( NickListCtrl::OnShowMenu )
END_EVENT_TABLE()


NickListCtrl::NickListCtrl( wxWindow* parent,Ui& ui, bool show_header, wxMenu* popup ):
  wxListCtrl( parent, NICK_LIST, wxDefaultPosition, wxDefaultSize,
              wxSUNKEN_BORDER | wxLC_REPORT | (int)(!show_header) * wxLC_NO_HEADER | wxLC_SINGLE_SEL ),
  m_ui(ui),
  m_menu(popup)
{
  wxListItem col;
  col.SetText( _("s") );
  col.SetImage( -1 );
  InsertColumn( 0, col );
  col.SetText( _("c") );
  col.SetImage( -1 );
  InsertColumn( 1, col );
  col.SetText( _("r") );
  col.SetImage( -1 );
  InsertColumn( 2, col );
  col.SetText( _("Nickname") );
  col.SetImage( ICON_DOWN );
  InsertColumn( 3, col );

  m_sortorder[0].col = 0;
  m_sortorder[0].direction = false;
  m_sortorder[1].col = 3;
  m_sortorder[1].direction = true;
  m_sortorder[2].col = 2;
  m_sortorder[2].direction = true;
  m_sortorder[3].col = 1;
  m_sortorder[3].direction = true;
  Sort( );

#ifdef __WXMSW__
  SetColumnWidth( 0, 45 );
#else
  SetColumnWidth( 0, 20 );
#endif
  SetColumnWidth( 1, 20 );
  SetColumnWidth( 2, 20 );
  SetColumnWidth( 3, 128 );

  SetImageList( &icons(), wxIMAGE_LIST_NORMAL );
  SetImageList( &icons(), wxIMAGE_LIST_SMALL );
  SetImageList( &icons(), wxIMAGE_LIST_STATE );

}


NickListCtrl::~NickListCtrl()
{

}

void NickListCtrl::AddUser( User& user )
{
  int index = InsertItem( 0, IconImageList::GetUserListStateIcon( user.GetStatus(), false, user.GetBattle() != 0 ) );
  SetItemData( index, (wxUIntPtr)&user );
  ASSERT_LOGIC( index != -1, "index = -1" );
  UserUpdated( index );
  Sort();
}

void NickListCtrl::RemoveUser( const User& user )
{
  for (int i = 0; i < GetItemCount() ; i++ ) {
    if ( &user == (User*)GetItemData( i ) ) {
      DeleteItem( i );
      Sort();
      return;
    }
  }
  debug_error( "Didn't find the user to remove." );
}


void NickListCtrl::UserUpdated( User& user )
{
  int index = GetUserIndex( user );
  ASSERT_LOGIC( index != -1, "index = -1" );
  UserUpdated( index );
}


void NickListCtrl::UserUpdated( const int& index )
{
  User& user = *((User*)GetItemData( index ));
  SetItemImage( index, IconImageList::GetUserListStateIcon( user.GetStatus(), false, user.GetBattle() != 0 ) );
  SetItemColumnImage( index, 1, IconImageList::GetFlagIcon( user.GetCountry() ) );
  SetItemColumnImage( index, 2, IconImageList::GetRankIcon( user.GetStatus().rank ) );
  SetItem( index, 3, WX_STRING(user.GetNick()) );
  SetItemData(index, (long)&user );
  Sort();

}


void NickListCtrl::ClearUsers()
{
  while ( GetItemCount() > 0 ) {
    DeleteItem( 0 );
  }
}


int NickListCtrl::GetUserIndex( User& user )
{
  for ( int i = 0; i < GetItemCount() ; i++ ) {
    if ( (unsigned long)&user == GetItemData( i ) ) return i;
  }
  debug_error( "didn't find the user." );
  return -1;
}


void NickListCtrl::OnActivateItem( wxListEvent& event )
{
  int index = event.GetIndex();
  if ( index == -1 ) return;
  User* user = (User*)event.GetData();
  m_ui.mw().OpenPrivateChat( *user );
  //m_ui.mw().OnTabsChanged( event2 );
}


void NickListCtrl::OnShowMenu( wxContextMenuEvent& event )
{
  debug_func("");
  if ( m_menu != 0 ) PopupMenu( m_menu );
}

void NickListCtrl::OnColClick( wxListEvent& event )
{
  if ( event.GetColumn() == -1 ) return;
  wxListItem col;
  GetColumn( m_sortorder[0].col, col );
  col.SetImage( -1 );
  SetColumn( m_sortorder[0].col, col );

  int i;
  for ( i = 0; m_sortorder[i].col != event.GetColumn() and i < 4; ++i ) {}
  if (i > 3) { i = 3; }
  for ( ; i > 0; i--) { m_sortorder[i] = m_sortorder[i-1]; }
  m_sortorder[0].col = event.GetColumn();
  m_sortorder[0].direction = !m_sortorder[0].direction;


  GetColumn( m_sortorder[0].col, col );
  col.SetImage( ( m_sortorder[0].direction )?ICON_UP:ICON_DOWN );
  SetColumn( m_sortorder[0].col, col );

  Sort();
}


void NickListCtrl::Sort()
{
  for (int i = 3; i >= 0; i--) {
    switch ( m_sortorder[ i ].col ) {
      case 0 : SortItems( ( m_sortorder[ i ].direction )?&ComparePlayerstatusUP:&ComparePlayerstatusDOWN , 0 ); break;
      case 1 : SortItems( ( m_sortorder[ i ].direction )?&ComparePlayercountryUP:&ComparePlayercountryDOWN , 0 ); break;
      case 2 : SortItems( ( m_sortorder[ i ].direction )?&ComparePlayerrankUP:&ComparePlayerrankDOWN , 0 ); break;
      case 3 : SortItems( ( m_sortorder[ i ].direction )?&ComparePlayernameUP:&ComparePlayernameDOWN , 0 ); break;
    }
  }
}


int wxCALLBACK NickListCtrl::ComparePlayernameUP(long item1, long item2, long sortData )
{
    // inverse the order

    if ( WX_STRING(((User*)item1)->GetNick()).MakeUpper() < WX_STRING(((User*)item2)->GetNick()).MakeUpper() )
        return -1;
    if ( WX_STRING(((User*)item1)->GetNick()).MakeUpper() > WX_STRING(((User*)item2)->GetNick()).MakeUpper() )
        return 1;

    return 0;
}


int wxCALLBACK NickListCtrl::ComparePlayernameDOWN(long item1, long item2, long sortData )
{
    // inverse the order
    if ( WX_STRING(((User*)item1)->GetNick()).MakeUpper() < WX_STRING(((User*)item2)->GetNick()).MakeUpper() )
        return 1;
    if ( WX_STRING(((User*)item1)->GetNick()).MakeUpper() > WX_STRING(((User*)item2)->GetNick()).MakeUpper() )
        return -1;

    return 0;
}


int wxCALLBACK NickListCtrl::ComparePlayerstatusUP(long item1, long item2, long sortData )
{
  User* user1 = (User*)item1;
  User* user2 = (User*)item2;
  ASSERT_LOGIC( user1 != 0, "user1 = 0" );
  ASSERT_LOGIC( user2 != 0, "user2 = 0" );

  int u1 = 0, u2 = 0;

  if ( user1->GetStatus().bot )
    u1 += 1000;
  if ( user2->GetStatus().bot )
    u2 += 1000;
  if ( user1->GetStatus().moderator )
    u1 += 100;
  if ( user2->GetStatus().moderator )
    u2 += 100;
  if ( user1->GetStatus().in_game )
    u1 += -10;
  if ( user2->GetStatus().in_game )
    u2 += -10;

    // inverse the order
    if ( u1 < u2 )
        return -1;
    if ( u1 > u2 )
        return 1;

    return 0;
}

int wxCALLBACK NickListCtrl::ComparePlayerstatusDOWN(long item1, long item2, long sortData )
{
  User* user1 = (User*)item1;
  User* user2 = (User*)item2;
  ASSERT_LOGIC( user1 != 0, "user1 = 0" );
  ASSERT_LOGIC( user2 != 0, "user2 = 0" );

  int u1 = 0, u2 = 0;

  if ( user1->GetStatus().bot )
    u1 += 1000;
  if ( user2->GetStatus().bot )
    u2 += 1000;
  if ( user1->GetStatus().moderator )
    u1 += 100;
  if ( user2->GetStatus().moderator )
    u2 += 100;
  if ( user1->GetStatus().in_game )
    u1 += -10;
  if ( user2->GetStatus().in_game )
    u2 += -10;


    // inverse the order
    if ( u1 < u2 )
        return 1;
    if ( u1 > u2 )
        return -1;

    return 0;
}


int wxCALLBACK NickListCtrl::ComparePlayerrankUP(long item1, long item2, long sortData )
{
    // inverse the order

    if ( ((User*)item1)->GetStatus().rank < ((User*)item2)->GetStatus().rank )
        return -1;
    if ( ((User*)item1)->GetStatus().rank > ((User*)item2)->GetStatus().rank )
        return 1;

    return 0;
}

int wxCALLBACK NickListCtrl::ComparePlayerrankDOWN(long item1, long item2, long sortData )
{
    // inverse the order
    if ( ((User*)item1)->GetStatus().rank < ((User*)item2)->GetStatus().rank )
        return 1;
    if ( ((User*)item1)->GetStatus().rank > ((User*)item2)->GetStatus().rank )
        return -1;

    return 0;
}


int wxCALLBACK NickListCtrl::ComparePlayercountryUP(long item1, long item2, long sortData )
{
    // inverse the order

    if ( WX_STRING(((User*)item1)->GetCountry()).MakeUpper() < WX_STRING(((User*)item2)->GetCountry()).MakeUpper() )
        return -1;
    if ( WX_STRING(((User*)item1)->GetCountry()).MakeUpper() > WX_STRING(((User*)item2)->GetCountry()).MakeUpper() )
        return 1;

    return 0;
}

int wxCALLBACK NickListCtrl::ComparePlayercountryDOWN(long item1, long item2, long sortData )
{
    // inverse the order
    if ( WX_STRING(((User*)item1)->GetCountry()).MakeUpper() < WX_STRING(((User*)item2)->GetCountry()).MakeUpper() )
        return 1;
    if ( WX_STRING(((User*)item1)->GetCountry()).MakeUpper() > WX_STRING(((User*)item2)->GetCountry()).MakeUpper() )
        return -1;

    return 0;
}