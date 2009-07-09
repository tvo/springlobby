#ifndef SPRINGLOBBY_HEADERGUARD_UPDATERMAINWINDOW_H
#define SPRINGLOBBY_HEADERGUARD_UPDATERMAINWINDOW_H

#include <wx/frame.h>

class ActivityNoticePanel;
class wxCloseEvent;
class wxBoxSizer;
class wxButton;

class UpdaterMainwindow : public wxFrame {

    public:
        UpdaterMainwindow( const wxString& rev_string );
        virtual ~UpdaterMainwindow();

        void OnClose( wxCloseEvent& evt );
        void OnChangelog( wxCommandEvent& event );

    protected:
        wxBoxSizer* m_main_sizer;
        ActivityNoticePanel* m_activity_panel;
        wxButton* m_changelog;

        enum {
            ID_BUT_CHANGELOG = wxID_HIGHEST
        };

        DECLARE_EVENT_TABLE()
};

#endif // SPRINGLOBBY_HEADERGUARD_UPDATERMAINWINDOW_H

/**
    This file is part of SpringLobby,
    Copyright (C) 2007-09

    SpringLobby is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2 as published by
    the Free Software Foundation.

    SpringLobby is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with SpringLobby.  If not, see <http://www.gnu.org/licenses/>.
**/