#ifndef SPRINGLOBBY_HEADERGUARD_MAINCHATTAB_H
#define SPRINGLOBBY_HEADERGUARD_MAINCHATTAB_H

#include <wx/scrolwin.h>

class Ui;
class ChatPanel;
class Server;
class Channel;
class User;
class wxAuiNotebookEvent;
class SLChatNotebook;
class wxBoxSizer;
class wxImageList;
class wxString;

//! @brief The main chat tab.
class MainChatTab : public wxScrolledWindow
{
  public:
    MainChatTab( wxWindow* parent );
    ~MainChatTab();

    ChatPanel* GetActiveChatPanel();
    ChatPanel* GetChannelChatPanel( const wxString& channel );
    ChatPanel* GetUserChatPanel( const wxString& user );

    ChatPanel& ServerChat();

    ChatPanel* AddChatPanel( Channel& channel );
    ChatPanel* AddChatPanel( Server& server, const wxString& name );
    ChatPanel* AddChatPanel( const User& user );
    /** \brief this is only used if channel is left via raw command in server tab */
    bool RemoveChatPanel( ChatPanel* panel );

    void RejoinChannels();
    void LeaveChannels();

    void OnTabsChanged( wxAuiNotebookEvent& event );
    void OnTabClose( wxAuiNotebookEvent& event );
    void OnUserConnected( User& user );
    void OnUserDisconnected( User& user );

    void ChangeUnreadChannelColour( const wxColour& colour );
    void ChangeUnreadPMColour( const wxColour& colour );

    void UpdateNicklistHighlights();

    wxImage ReplaceChannelStatusColour( wxBitmap img, const wxColour& colour );

    void LoadPerspective( const wxString& perspective_name = wxEmptyString );
    void SavePerspective( const wxString& perspective_name = wxEmptyString );

  protected:

    wxWindow* m_close_window;
    SLChatNotebook* m_chat_tabs;
    wxBoxSizer* m_main_sizer;
    wxImageList* m_imagelist;
    ChatPanel* m_server_chat;
    ChatPanel* m_main_chat;
    int m_newtab_sel;

    enum {
        CHAT_TABS = wxID_HIGHEST
    };

    DECLARE_EVENT_TABLE()
};



#endif // SPRINGLOBBY_HEADERGUARD_MAINCHATTAB_H

/**
    This file is part of SpringLobby,
    Copyright (C) 2007-2010

    SpringLobby is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2 as published by
    the Free Software Foundation.

    springsettings is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with SpringLobby.  If not, see <http://www.gnu.org/licenses/>.
**/

