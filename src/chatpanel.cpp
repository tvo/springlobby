/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


//
// Class: ChatPanel
//
// Created on: Wed May  2 21:07:18 2007
//

#include <wx/intl.h>
#include <wx/datetime.h>
#include "chatpanel.h"
#include "system.h"
#include "utils.h"
#include "ui.h"

BEGIN_EVENT_TABLE(ChatPanel, wxPanel)

  EVT_TEXT_ENTER ( CHAT_TEXT, ChatPanel::OnSay   )
  EVT_BUTTON     ( CHAT_SEND, ChatPanel::OnSay   )
  EVT_SIZE       (            ChatPanel::OnResize)

END_EVENT_TABLE()

/*
//! @brief ChatPanel constructor.
//!
//! @param parent the parent wxWindow.
//! @param show_nick_list set to true if the nick list should be created.
//! @todo Fix nick list
//! @todo Enlarge the say input.
ChatPanel::ChatPanel( wxWindow* parent, bool show_nick_list ) : wxPanel( parent, -1 )
{
  // Setting default values
  m_show_nick_list = show_nick_list;
  m_channel = NULL;
  
  _CreateControls();
  
}
*/

ChatPanel::ChatPanel( wxWindow* parent, Channel& chan )
: wxPanel( parent, -1),m_channel(&chan),m_server(NULL),m_user(NULL),m_type(CPT_Channel),m_show_nick_list(true)
{
  std::cout << "** ChatPanel::ChatPanel( wxWindow* parent, Channel& chan )" << std::endl;
  _CreateControls( );
  _SetChannel( &chan );
}

ChatPanel::ChatPanel( wxWindow* parent, User& user )
: wxPanel( parent, -1),m_channel(NULL),m_server(NULL),m_user(&user),m_type(CPT_User),m_show_nick_list(false)
{
  _CreateControls( );
}

ChatPanel::ChatPanel( wxWindow* parent, Server& serv )
: wxPanel( parent, -1),m_channel(NULL),m_server(&serv),m_user(NULL),m_type(CPT_Server),m_show_nick_list(false)
{
  std::cout << "** ChatPanel::ChatPanel( wxWindow* parent, Server& serv )" << std::endl;
  _CreateControls( );
  std::cout << "** ChatPanel::ChatPanel( wxWindow* parent, Server& serv )" << std::endl;
}

void ChatPanel::_CreateControls( )
{
  std::cout << "** ChatPanel::_CreateControls()" << std::endl;

  // Creating sizers
  m_main_sizer = new wxBoxSizer( wxHORIZONTAL );
  m_chat_sizer = new wxBoxSizer( wxVERTICAL );
  m_say_sizer = new wxBoxSizer( wxHORIZONTAL );
  
  if ( m_show_nick_list ) {
    
    m_splitter = new wxSplitterWindow( this, -1, wxDefaultPosition, wxDefaultSize, wxSP_3D );
    m_nick_panel = new wxPanel( m_splitter, -1 );
    m_chat_panel = new wxPanel( m_splitter, -1 );
    
    m_nick_sizer = new wxBoxSizer( wxVERTICAL );
    
    m_nicklist = new NickListCtrl( m_nick_panel );
    
    m_nick_filter = new wxComboBox( m_nick_panel, -1, _("Show all"), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY );
    
    m_nick_sizer->Add( m_nicklist, 1, wxEXPAND );
    m_nick_sizer->Add( m_nick_filter, 0, wxEXPAND | wxTOP, 2 );
    
    m_nick_panel->SetSizer( m_nick_sizer );
    
  } else {
    
    m_chat_panel = this;
    m_nick_sizer = NULL;
    m_nicklist = NULL;
    m_nick_filter = NULL;
    m_splitter = NULL;
    
  }
  
  // Creating ui elements
  m_chatlog_text = new wxTextCtrl( m_chat_panel, -1, _(""), wxDefaultPosition, wxDefaultSize, 
                             wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH | wxTE_AUTO_URL );

  m_say_text = new wxTextCtrl( m_chat_panel, CHAT_TEXT, _(""), wxDefaultPosition, wxSize(100,26), wxTE_PROCESS_ENTER );
  m_say_button = new wxButton( m_chat_panel, CHAT_SEND, _T("Send"), wxDefaultPosition, wxSize(80,26) );
  
  // Adding elements to sizers
  m_say_sizer->Add( m_say_text, 1, wxEXPAND );
  m_say_sizer->Add( m_say_button );
  
  m_chat_sizer->Add( m_chatlog_text, 1, wxEXPAND );
  m_chat_sizer->Add( m_say_sizer, 0, wxEXPAND | wxTOP, 2 );
  
  if ( m_show_nick_list ) {
    m_chat_panel->SetSizer( m_chat_sizer );
    
    m_splitter->SplitVertically( m_chat_panel, m_nick_panel, 100 );
    
    m_main_sizer->Add( m_splitter, 1, wxEXPAND | wxALL, 2 );
    
  } else {
    m_main_sizer->Add( m_chat_sizer, 4, wxEXPAND | wxALL, 2 );
  }
  
  // Assign sizer to panel
  SetSizer( m_main_sizer );
  
  
  if ( m_show_nick_list ) {
    wxSize s = m_splitter->GetSize();
    m_splitter->SetSashPosition( s.GetWidth() - 210, true );
  }
}


//! @brief ChatPanel destructor.
ChatPanel::~ChatPanel()
{

}

void ChatPanel::OnResize( wxSizeEvent& event )
{
  SetSize( event.GetSize() );
  Layout();
  if ( m_show_nick_list ) {
    wxSize s = m_splitter->GetSize();
    m_splitter->SetSashPosition( s.GetWidth() - 210, true );
  }
}


void ChatPanel::OnSay( wxCommandEvent& event )
{
  Say( m_say_text->GetValue() );
  m_say_text->SetValue( _("") );
}

//! @brief Output a message said in the channel.
//!
//! @param who nick of the person who said something.
//! @param message the message to be outputted.
//! @todo Fix nicer format of chat messages and highlight messages to self
void ChatPanel::Said( const wxString& who, const wxString& message )
{
  m_chatlog_text->SetDefaultStyle(wxTextAttr(*wxBLACK));
  LogTime();
  m_chatlog_text->AppendText( _(" ") + who + _(": ")+ message + _("\n") );
}


void ChatPanel::DidAction( const wxString& who, const wxString& action )
{
  m_chatlog_text->SetDefaultStyle(wxTextAttr(*wxBLACK));
  LogTime();
  m_chatlog_text->AppendText( _(" ") + who + _(" ") + action + _(".\n") );
}


//! @brief Output motd sent by server
//!
//! @param message The MOTD message to output
void ChatPanel::Motd( const wxString& message )
{
  std::cout << "** ChatPanel::Motd()" << std::endl;
  m_chatlog_text->SetDefaultStyle(wxTextAttr(wxColour(0, 80, 128)));
  m_chatlog_text->AppendText( _(" ** motd ** ")+ message + _("\n") );
}


void ChatPanel::UnknownCommand( const wxString& command, const wxString& params )
{
  m_chatlog_text->SetDefaultStyle(wxTextAttr(*wxBLACK));
  LogTime();
  m_chatlog_text->SetDefaultStyle(wxTextAttr(wxColour(128, 0, 0)));
  m_chatlog_text->AppendText( _T(" !! Command: \"") + command + _T("\" params: \"") + params + _("\".\n") );
}


void ChatPanel::Joined( User& who )
{
  m_chatlog_text->SetDefaultStyle(wxTextAttr(*wxBLACK));
  LogTime();
  m_chatlog_text->SetDefaultStyle(wxTextAttr(wxColour(0, 80, 0)));
  m_chatlog_text->AppendText( _(" ** ")+ WX_STRING(who.GetNick()) + _T(" joined the channel.\n") );
  m_nicklist->AddUser( who );
  //m_nicklist->UpdateSize();
}


void ChatPanel::Parted( User& who, const wxString& message )
{
  m_chatlog_text->SetDefaultStyle(wxTextAttr(*wxBLACK));
  LogTime();
  m_chatlog_text->SetDefaultStyle(wxTextAttr(wxColour(0, 80, 0)));
  m_chatlog_text->AppendText( _(" ** ")+ WX_STRING(who.GetNick()) + _T(" left the channel ( ") + message + _(" ).\n") );
  m_nicklist->RemoveUser( who );
//  m_nicklist->UpdateSize();
}

void ChatPanel::SetTopic( const wxString& who, const wxString& message )
{
  m_chatlog_text->SetDefaultStyle(wxTextAttr(wxColour(0, 0, 80)));
  m_chatlog_text->AppendText( _(" ** Channel topic: ")+ message + _T("\n * Set by ") + who + _(".\n") );  
}


//! @brief Set the Channel object
//!
//! @param channel the Channel object.
void ChatPanel::_SetChannel( Channel* channel )
{
  if ( m_channel != NULL ) {
    m_channel->uidata.panel = NULL;
  }
  
  m_channel = channel;
  
  if ( m_channel != NULL ) {
    m_channel->uidata.panel = this;
  }
  //m_nicklist->SetUserList( (UserList*)channel );
  //m_nicklist->SetItemCount( channel->GetNumUsers() );

}




/*
//! @brief Get Channel object
//!
//! @return Name of the chat/channel.
Channel* ChatPanel::GetChannel()
{
  
}
*/


void ChatPanel::Say( const wxString& message )
{
  std::cout << "** ChatPanel::Say()" << std::endl;
  if ( m_type == CPT_Channel ) {
    assert( m_channel != NULL );
    m_channel->Say( STL_STRING(message) );
  }
}

void ChatPanel::Part()
{
  std::cout << "** ChatPanel::Part()" << std::endl;
  assert( m_channel != NULL );
  m_channel->Leave();
  m_channel->uidata.panel = NULL;
}

void ChatPanel::LogTime()
{
  wxDateTime now = wxDateTime::Now();
  m_chatlog_text->AppendText( _("[") + now.Format( _("%H:%M") ) + _("]") );

}

