/* Copyright (C) 2007 The SpringLobby Team. All rights reserved. */
//
// Class: ChatLog
//
#include <wx/string.h>
#include <wx/file.h>
#include <wx/filefn.h>
#include <wx/dir.h>
#include <wx/datetime.h>
#include <wx/intl.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <stdexcept>

#include "chatlog.h"
#include "settings.h"
#include "utils/conversion.h"
#include "ui.h"

#include "utils/customdialogs.h"

bool ChatLog::m_parent_dir_exists = true;

ChatLog::ChatLog( const wxString& server, const wxString& room ):
//  m_logfile( 0 ),
		m_server( server ),
		m_room( room )

{
#ifdef __WXMSW__
	m_server.Replace( wxT( ":" ), wxT( "_" ) ) ;
#endif
	// testing.
	// m_active = OpenLogFile(m_server,m_room) ;
	wxLogMessage( _T( "ChatLog::ChatLog( %s, %s )" ), m_server.c_str(), m_room.c_str() ) ;
	m_active = OpenLogFile( m_server, m_room );
}


ChatLog::~ChatLog()
{
	wxLogMessage( _T( "%s -- ChatLog::~ChatLog()" ), m_room.c_str() );
	if ( m_active && m_logfile.IsOpened() ) {
		wxDateTime now = wxDateTime::Now();
		WriteLine( _( "### Session Closed at [" ) + now.Format( _T( "%Y-%m-%d %H:%M" ) ) + _( "]" ) );
#ifndef __WXMSW__
		WriteLine( _T( " \n \n \n" ) );
#else
		WriteLine( _T( " \r\n \r\n \r\n" ) );
#endif
		// crashes right there on close.
		m_logfile.Flush();
		m_logfile.Close();
	}
	// it is safe to delete a null pointer.
// 	delete m_logfile;
//  m_logfile = 0;
}


bool ChatLog::AddMessage( const wxString& text )
{
	if ( !LogEnabled() ) {
		return true;
	}
	else if ( !m_logfile.IsOpened() ) {
		m_active = OpenLogFile( m_server, m_room );
	}
	if ( m_active )
	{
		wxString line = LogTime() + _T( " " ) + text;
#ifdef __WXMSW__
		line << _T( "\r" );
#endif
		line << _T( "\n" );
		return WriteLine( line );
	}
	else return false;
}


bool ChatLog::LogEnabled()
{
	return sett().GetChatLogEnable();
}


wxString ChatLog::_GetPath()
{
	return sett().GetChatLogLoc();
}


bool ChatLog::CreateFolder( const wxString& server )
{
	if ( !( wxDirExists( _GetPath() ) || wxMkdir( _GetPath(), 0777 ) ) ) {
		wxLogWarning( _T( "can't create logging folder: %s" ), _GetPath().c_str() );
		customMessageBox( SL_MAIN_ICON, _( "Couldn't create folder. \nBe sure that there isn't a write protection.\n" ) + _GetPath() + _( "Log function is disabled until restart SpringLobby." ), _( "Log Warning" ) );
		m_parent_dir_exists = false;
		return false;
	}
	if ( !( wxDirExists( _GetPath() + wxFileName::GetPathSeparator() + server ) || wxMkdir( _GetPath() + wxFileName::GetPathSeparator() + server, 0777 ) ) ) {
		wxLogWarning( _T( "can't create logging folder: %s" ), wxString( _GetPath() + wxFileName::GetPathSeparator() + server ).c_str() );
		customMessageBox( SL_MAIN_ICON, _( "Couldn't create folder. \nBe sure that there isn't a write protection.\n" ) + _GetPath() + wxFileName::GetPathSeparator() + server + _( "Log function is disabled until restart SpringLobby." ), _T( "Log Warning" ) );
		m_parent_dir_exists = false;
		return false;
	}
	return true;
}


bool ChatLog::WriteLine( const wxString& text )
{
//  try
//  {
//    ASSERT_LOGIC( m_logfile, _T("m_logfile = 0") );
//  } catch(...) {return false;}
	if ( !m_logfile.Write( text, wxConvUTF8 ) ) {
		m_active = false;
		wxLogWarning( _T( "can't write message to log (%s)" ),  wxString( m_server + _T( "::" ) + m_room ).c_str() );
		customMessageBox( SL_MAIN_ICON, _( "Couldn't write message to log.\nLogging will be disabled for room " ) + m_server + _T( "::" ) + m_room + _( ".\n\nRejoin room to reactivate logging." ), _( "Log Warning" ) );
		return false;
	}
	return true;
}

void ChatLog::FillLastLineArray()
{
    wxTextFile tmp( m_current_logfile_path );
    tmp.Open();
    if ( !tmp.IsOpened() )
        return;

    m_last_lines = wxArrayString();
    int load_lines  = sett().GetAutoloadedChatlogLinesCount();
    const size_t line_no = tmp.GetLineCount();
    const size_t first_line_no = line_no - load_lines ;

    for ( size_t i = first_line_no; i < line_no; ++i ) {
        wxString tmp_str = tmp[i];
        if ( ! tmp_str.StartsWith( _T("###") ) )
            m_last_lines.Add( tmp_str );
    }
}

bool ChatLog::OpenLogFile( const wxString& server, const wxString& room )
{
	m_current_logfile_path = _GetPath() + wxFileName::GetPathSeparator() + server + wxFileName::GetPathSeparator() + room + _T( ".txt" );
    FillLastLineArray();
	wxLogMessage( _T( "OpenLogFile( %s, %s )" ), server.c_str(), room.c_str() ) ;

//  delete m_logfile;
//  m_logfile = 0;

	if ( m_parent_dir_exists && LogEnabled() && CreateFolder( server ) ) {
		if ( wxFileExists( m_current_logfile_path ) ) {
			m_logfile.Open( m_current_logfile_path, wxFile::write_append );
		} else {
			m_logfile.Open( m_current_logfile_path, wxFile::write );
		}
		if ( !m_logfile.IsOpened() ) {
			wxLogWarning( _T( "Can't open log file %s" ), m_current_logfile_path.c_str() ) ;
			customMessageBox( SL_MAIN_ICON, _( "Can't open log file. \nBe sure that there isn't a write protection.\n" ) + m_current_logfile_path, _( "Log Warning" ) ) ;

		}
		else {
			wxDateTime now = wxDateTime::Now();
			wxString text = _T( "### Session Start at [" ) + now.Format( _T( "%Y-%m-%d %H:%M" ) ) + _T( "]" );
#ifdef __WXMSW__
			text << _T( "\r" );
#endif
			text << _T( "\n" );
			return WriteLine( text );

		}
	}
	return false;
}


wxString ChatLog::LogTime()
{
	wxDateTime now = wxDateTime::Now();
	return  _T( "[" ) + now.Format( _T( "%H:%M" ) ) + _T( "]" );
}

void ChatLog::OpenInEditor()
{
    ui().OpenFileInEditor( m_current_logfile_path );
}

const wxArrayString& ChatLog::GetLastLines( ) const
{
    return m_last_lines;
}
