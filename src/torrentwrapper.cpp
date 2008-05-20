/* Copyright (C) 2007 The SpringLobby Team. All rights reserved. */
//
// Class: TorrentWrapper
//

#ifndef NO_TORRENT_SYSTEM

#include "iunitsync.h"
#include "settings.h"
#include "utils.h"
#include "socket.h"
#include "base64.h"
#include "base16.h"

#include <libtorrent/entry.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/storage.hpp>
#include <libtorrent/hasher.hpp>
#include <libtorrent/file_pool.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/file.hpp>

#include <libtorrent/extensions/metadata_transfer.hpp>
#include <libtorrent/extensions/ut_pex.hpp>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/format.hpp>

#include <fstream>

#include <wx/tokenzr.h>
#include <wx/protocol/http.h>
#include <wx/filename.h>
#include <wx/file.h>
#include <wx/wfstream.h>
#include <wx/msgdlg.h>
#include <wx/app.h>
#include <wx/event.h>

#include "torrentwrapper.h"

TorrentWrapper* torrent()
{
  static TorrentWrapper* m_torr_wrap = 0;
  if (!m_torr_wrap)
    m_torr_wrap = new TorrentWrapper;
  return m_torr_wrap;
}


TorrentWrapper::TorrentWrapper():
m_connected(false),
ingame(false),
m_seed_count(0),
m_leech_count(0),
m_timer_count(0)
{
  m_tracker_urls.Add( _T("tracker.caspring.org"));
  m_tracker_urls.Add( _T("tracker2.caspring.org"));
  m_tracker_urls.Add( _T("backup-tracker.licho.eu"));
  m_torr = new libtorrent::session();
  m_torr->add_extension(&libtorrent::create_metadata_plugin);
  m_torr->add_extension(&libtorrent::create_ut_pex_plugin);
  m_torr->start_upnp();
  m_torr->start_natpmp();
  m_torr->start_lsd();
  m_socket_class = new Socket( *this );
  UpdateSettings();
}


TorrentWrapper::~TorrentWrapper()
{
  m_torr->stop_upnp();
  m_torr->stop_natpmp();
  m_torr->stop_lsd();
  DisconnectToP2PSystem();
  delete m_torr;
  delete m_socket_class;
}


////////////////////////////////////////////////////////
////                gui interface                   ////
////////////////////////////////////////////////////////

void TorrentWrapper::ConnectToP2PSystem()
{
  if ( m_connected ) return;
  m_socket_class->Connect( m_tracker_urls[0], DEFAULT_P2P_COORDINATOR_PORT );
  m_connected_tracker_index= 0;
  return; //TODO (BrainDamage #1#) what's this??
  for( unsigned int i = 0; i < m_tracker_urls.GetCount(); i++ )
  {
    m_socket_class->Connect( m_tracker_urls[i], DEFAULT_P2P_COORDINATOR_PORT );
    if ( m_connected )
    {
       m_connected_tracker_index = i;
       return;
    }
  }
}


void TorrentWrapper::DisconnectToP2PSystem()
{
  if ( m_connected )
  {
     {
       ScopedLocker<TorrentHandleToHash> torrent_handles_l(m_torrent_handles);
       for ( TorrentHandleToHash::from::iterator itor = torrent_handles_l.Get().from.begin(); itor != torrent_handles_l.Get().from.end(); itor++ )
        m_socket_class->Send( wxString::Format( _T("N-|%s\n"), itor->second.c_str() ) ); /// release all files requests
     }
     m_socket_class->Disconnect();
  }
}


bool TorrentWrapper::IsConnectedToP2PSystem()
{
  return m_connected;
}


void TorrentWrapper::UpdateSettings()
{
  if ( !ingame || sett().GetTorrentSystemSuspendMode() == 0 )
  {
    m_torr->set_upload_rate_limit(sett().GetTorrentUploadRate() * 1024);
    m_torr->set_download_rate_limit(sett().GetTorrentDownloadRate() *1024 );
  }
  else
  {
    m_torr->set_upload_rate_limit(sett().GetTorrentThrottledUploadRate() * 1024);
    m_torr->set_download_rate_limit(sett().GetTorrentThrottledDownloadRate() *1024 );
  }
  m_torr->set_max_connections(sett().GetTorrentMaxConnections());
  try
  {
    m_torr->listen_on(std::make_pair(sett().GetTorrentPort(), sett().GetTorrentPort()));
  } catch (std::exception& e)
  {
    e.what(); /// TODO (BrainDamage#1#): add message on failure
  }
}


bool TorrentWrapper::IsFileInSystem( const wxString& hash )
{
  ScopedLocker<HashToTorrentData> torrents_infos_l(m_torrents_infos);
  return torrents_infos_l.Get().find(hash) != torrents_infos_l.Get().end();
}


void TorrentWrapper::RemoveFile( const wxString& hash )
{
  wxString name;
  {
    ScopedLocker<HashToTorrentData> torrents_infos_l(m_torrents_infos);
    HashToTorrentData::iterator it=torrents_infos_l.Get().find(hash);
    if (it==torrents_infos_l.Get().end()) return; /// file not in system
    name = it->second.name;
  }
  {
    ScopedLocker<TorrentHandleToHash> torrent_handles_l(m_torrent_handles);
    TorrentHandleToHash::to::iterator iter = torrent_handles_l.Get().to.find(hash);
    if ( iter == torrent_handles_l.Get().to.end() ) return; /// torrent handler not found
    if ( iter->second.is_seed() ) m_seed_count--;
    else m_leech_count--;
    m_torr->remove_torrent( iter->second );
  }
  {
    ScopedLocker<OpenTorrents> open_torrents_l(m_open_torrents);
    OpenTorrents::iterator itor = open_torrents_l.Get().find(name);
    if ( itor == open_torrents_l.Get().end() ) return; /// bad hash request
    open_torrents_l.Get().erase(itor);
  }
}


int TorrentWrapper::GetTorrentSystemStatus()
{
  if (!m_connected) return 0;
  if (ingame) return 2;
  return 1;
}


////////////////////////////////////////////////////////
////               lobby interface                  ////
////////////////////////////////////////////////////////


bool TorrentWrapper::RequestFileByHash( const wxString& hash )
{
  if (ingame) return false;
  if ( !m_connected ) return false;
  if ( m_leech_count > 4 ) return false;

  wxString name;
  {
    ScopedLocker<HashToTorrentData> torrents_infos_l(m_torrents_infos);
    HashToTorrentData::iterator it=torrents_infos_l.Get().find(hash);
    if (it==torrents_infos_l.Get().end()) return false;
    if ( it->second.hash.IsEmpty() ) return false; /// the file is not present in the system
    name=it->second.name;
  }

  {
    ScopedLocker<OpenTorrents> open_torrents_l(m_open_torrents);
    OpenTorrents::iterator itor = open_torrents_l.Get().find(name);
    if ( itor != open_torrents_l.Get().end() ) return true; /// don't request twice the same file
  }

  if ( !JoinTorrent( hash ) ) return false;
  m_socket_class->Send( wxString::Format( _T("N+|%s\n"), hash.c_str() ) ); /// request for seeders for the file
  m_leech_count++;
  {
    ScopedLocker<OpenTorrents> open_torrents_l(m_open_torrents);
    open_torrents_l.Get()[name] = false; /// not seeding when just joined
  }
  return true;
}


bool TorrentWrapper::RequestFileByName( const wxString& name )
{
    ScopedLocker<NameToHash> name_to_hash_l(m_name_to_hash);
    NameToHash::from::iterator iter = name_to_hash_l.Get().from.find(name);
    if( iter == name_to_hash_l.Get().from.end() ) return false;
    return RequestFileByHash( iter->second );
}


void TorrentWrapper::SetIngameStatus( bool status )
{
  if ( status == ingame ) return; /// no change needed
  ingame = status;
  std::vector<libtorrent::torrent_handle> TorrentList = m_torr->get_torrents();
  if ( ingame ) /// going ingame, pause all torrents (or throttle speeds) and disable dht
  {
    if ( sett().GetTorrentSystemSuspendMode() == 0 ) for ( unsigned int i = 0; i < TorrentList.size(); i++) TorrentList[i].pause();
    else
    {
      m_torr->set_upload_rate_limit(sett().GetTorrentThrottledUploadRate() * 1024);
      m_torr->set_download_rate_limit(sett().GetTorrentThrottledDownloadRate() *1024 );
    }
    m_torr->stop_dht();
  }
  else/// game closed, resume all torrents (or reset normal speed) and reactivate dht
  {
    m_torr->start_dht();
    if ( sett().GetTorrentSystemSuspendMode() == 0 ) for ( unsigned int i = 0; i < TorrentList.size(); i++) TorrentList[i].resume();
    else
    {
      m_torr->set_upload_rate_limit(sett().GetTorrentUploadRate() * 1024);
      m_torr->set_download_rate_limit(sett().GetTorrentDownloadRate() *1024 );
    }
  }

}


void TorrentWrapper::UpdateFromTimer( int mselapsed )
{
  m_timer_count++;
  if ( m_timer_count < 20 ) return;////update every 2 sec
  m_timer_count = 0;
  if (!ingame && m_connected) FixTorrentList();
}


////////////////////////////////////////////////////////
//// private functions to interface with the system ////
////////////////////////////////////////////////////////


std::map<int,TorrentInfos> TorrentWrapper::CollectGuiInfos()
{
  std::map<int,TorrentInfos> ret;

  TorrentInfos globalinfos;
  globalinfos.seeding = false;
  globalinfos.progress = 0.0f;
  globalinfos.downloaded = 0;
  globalinfos.uploaded = 0;
  globalinfos.outspeed = m_torr->status().upload_rate;
  globalinfos.inspeed = m_torr->status().download_rate;
  globalinfos.numcopies = 0.0f;
  globalinfos.filesize = 0;
  ret[0] = globalinfos;

  if ( ingame || !m_connected ) return ret; /// stop updating the gui if disconneted

  std::vector<libtorrent::torrent_handle> TorrentList = m_torr->get_torrents();
  for( std::vector<libtorrent::torrent_handle>::iterator i = TorrentList.begin(); i != TorrentList.end(); i++)
  {
    wxLogMessage(_T("CollectGuiInfos for %s"),WX_STRING(i->name()).c_str());
    TorrentInfos CurrentTorrent;
    CurrentTorrent.name = WX_STRING(i->name()).BeforeFirst(_T('|'));
    CurrentTorrent.seeding = i->is_seed();
    CurrentTorrent.progress = i->status().progress;
    CurrentTorrent.downloaded = i->status().total_payload_download;
    CurrentTorrent.uploaded = i->status().total_payload_upload;
    CurrentTorrent.inspeed = i->status().download_payload_rate;;
    CurrentTorrent.outspeed = i->status().upload_payload_rate;;
    CurrentTorrent.numcopies = i->status().distributed_copies;
    CurrentTorrent.filesize = i->get_torrent_info().total_size();
    {
      ScopedLocker<TorrentHandleToHash> torrent_handles_l(m_torrent_handles);
      TorrentHandleToHash::iterator itor = torrent_handles_l.Get().from.find(*i);
      if ( itor == torrent_handles_l.Get().end() ) continue;
      CurrentTorrent.hash = itor->second;
    }
    ret[s2l(CurrentTorrent.hash)] = CurrentTorrent;
  }
  return ret;
}


void TorrentWrapper::SendMessageToCoordinator( const wxString& message )
{
  if ( m_connected ) m_socket_class->Send( message + _T("\n") );
}


bool TorrentWrapper::JoinTorrent( const wxString& hash )
{
  wxLogMessage(_T("(1) Joining torrent, hash=%s"),hash.c_str());
  if (ingame) return false;
  wxLogMessage(_T("(2) Joining torrent."));

  MediaType type;
  wxString torrent_name;
  wxString torrent_infohash_b64;
  {
    ScopedLocker<HashToTorrentData> torrents_infos_l(m_torrents_infos);
    HashToTorrentData::iterator it=torrents_infos_l.Get().find(hash);
    if(it==torrents_infos_l.Get().end()){
      wxLogMessage(_T("(3) Joining torrent: hash not found"));
      return false;
    }
    type=it->second.type;
    torrent_name=it->second.name;
    torrent_infohash_b64=it->second.infohash;
  }
  wxString path = sett().GetSpringDir();
  wxString name;
  if(path.size()>0&&(path.Last()!=wxChar('/')))path+=wxChar('/');
  if ( type == map )
  {
    path = path + _T("maps/");
    name = torrent_name + _T("|MAP");
  }
  else
  {
    path = path + _T("mods/");
    name = torrent_name + _T("|MOD");
  }
  wxLogMessage(_T("(3) Joining torrent: downloading info file"));
  if ( !wxFileName::IsFileReadable( sett().GetSpringDir() + _T("/torrents/") + hash + _T(".torrent") ) ) /// file descriptor not present, download it
  {
     if (!DownloadTorrentFileFromTracker( hash )) return false;
  }
  /// read torrent from file
  std::ifstream in( wxString( sett().GetSpringDir() + _T("/torrents/") + hash + _T(".torrent") ).mb_str(), std::ios_base::binary);
  in.unsetf(std::ios_base::skipws);
  libtorrent::entry e = libtorrent::bdecode(std::istream_iterator<char>(in), std::istream_iterator<char>());
  wxLogMessage(_T("(4) Joining torrent: add_torrent(%s,[%s],%s,[%s])"),m_tracker_urls[m_connected_tracker_index].c_str(),torrent_infohash_b64.c_str(),name.c_str(),path.c_str());
  {
    ScopedLocker<TorrentHandleToHash> torrent_handles_l(m_torrent_handles);
    torrent_handles_l.Get().to[hash] = m_torr->add_torrent(libtorrent::torrent_info(e), boost::filesystem::path( STD_STRING( path ) ) );
  }
  /*
  wxLogMessage(_T("torrent b64 infohash: %s"), torrent_infohash_b64.c_str() );
  std::string torrent_infohash_binary = wxBase64::Decode(torrent_infohash_b64 );

  wxString torrent_infohash_reencoded=wxBase64::Encode(reinterpret_cast<const wxUint8*>(torrent_infohash_binary.c_str()),torrent_infohash_binary.size());

  if(torrent_infohash_reencoded!=torrent_infohash_b64){
    wxLogMessage(_T("Base64 decoding phailed!"));
  }

  wxString torrent_infohash_b16=ToBase16((unsigned char *)&torrent_infohash_binary[0],torrent_infohash_binary.size());

  wxLogMessage( _T("torrent b16 infohash: %s"),torrent_infohash_b16.c_str());

  libtorrent::sha1_hash infohash( torrent_infohash_binary );

  wxLogMessage(_T("(4) Joining torrent: add_torrent(%s,[%s],%s,[%s])"),m_tracker_urls[m_connected_tracker_index].c_str(),torrent_infohash_reencoded.c_str(),name.c_str(),path.c_str());

  m_torr->add_torrent( m_tracker_urls[m_connected_tracker_index].mb_str(), infohash, name.mb_str(), boost::filesystem::path( STD_STRING( path ) ) );
  */
  wxLogMessage(_T("(5) Joining torrent: done"));
  return true;
}


void TorrentWrapper::CreateTorrent( const wxString& hash, const wxString& name, MediaType type )
{
  if (ingame) return;


  if ( sett().GetSpringDir().IsEmpty() ) return; /// no good things can happend if you don't know which folder to r/w files from
  bool creationsuccess = true;
  if ( !wxFileName::DirExists( sett().GetSpringDir() + _T("/torrents/")  ) ) creationsuccess = wxFileName::Mkdir(  sett().GetSpringDir() + _T("/torrents/")  );
  if (!creationsuccess) return;

  libtorrent::torrent_info newtorrent;

  wxString StringFilePath = sett().GetSpringDir();
  switch (type)
  {
    case map:
      StringFilePath += _T("/maps/");
    case mod:
      StringFilePath += _T("/mods/");
  }
  StringFilePath += name;
  boost::filesystem::path InputFilePath = complete(boost::filesystem::path( STD_STRING( StringFilePath ) ) );

  newtorrent.add_file( InputFilePath.branch_path(), boost::filesystem::file_size( InputFilePath ) );

  for ( unsigned int i = 0; i < m_tracker_urls.GetCount(); i++ )
  {
    newtorrent.add_tracker( STD_STRING(m_tracker_urls[i] +  _T(":DEFAULT_P2P_TRACKER_PORT/announce") ) );
  }

  wxFile torrentfile( StringFilePath );
  if ( !torrentfile.IsOpened() ) return;
  /// calculate the hash for all pieces
  int num = newtorrent.num_pieces();
  std::vector<char> buf(newtorrent.piece_size(0));
  for ( int i = 0; i < num; ++i)
  {
    torrentfile.Read(&buf[0], newtorrent.piece_size(i));
    libtorrent::hasher h(&buf[0], newtorrent.piece_size(i));
    newtorrent.set_hash(i, h.final());
  }

  switch (type)
  {
    case map:
      newtorrent.set_comment( wxString( name + _T("|MAP") ).mb_str() );
    case mod:
      newtorrent.set_comment( wxString( name + _T("|MOD") ).mb_str() );
  }

  newtorrent.create_torrent(); /// creates the torrent and publishes on the tracker
}


bool TorrentWrapper::DownloadTorrentFileFromTracker( const wxString& hash )
{
  wxLogMessage(_T("torrent system downloading torrent info %s"), hash.c_str() );

  if ( sett().GetSpringDir().IsEmpty() ) return false; /// no good things can happend if you don't know which folder to r/w files from
  bool creationsuccess = true;
  if ( !wxFileName::DirExists(  sett().GetSpringDir() + _T("/torrents/")  ) ) creationsuccess = wxFileName::Mkdir(  sett().GetSpringDir() + _T("/torrents/")  );
  if (!creationsuccess) return false;
  wxHTTP fileRequest;
  //versionRequest.SetHeader(_T("Content-type"), _T(""));
  /// normal timeout is 10 minutes.. set to 10 secs.
  fileRequest.SetTimeout(10);
  fileRequest.Connect( m_tracker_urls[m_connected_tracker_index], 80);
  wxInputStream *stream = fileRequest.GetInputStream( _T("/torrents/") + hash + _T(".torrent") );
  bool ret = false;
  if (fileRequest.GetError() == wxPROTO_NOERR)
  {

    wxFileOutputStream output(sett().GetSpringDir() +  _T("/torrents/") + hash + _T(".torrent") );
    if ( output.Ok() )
    {
      stream->Read(output);
      ret = true;
    }

  }

  wxDELETE(stream);
  fileRequest.Close();
  if (ret) wxLogMessage(_T("torrent system downloading torrent info %s successful"), hash.c_str() );
  else wxLogMessage(_T("torrent system downloading torrent info %s failed"), hash.c_str() );
  return ret;
}


void TorrentWrapper::FixTorrentList()
{
  wxLogMessage(_T("torrent system doing maintenance") );
  std::vector<libtorrent::torrent_handle> TorrentList = m_torr->get_torrents();
  //std::map<wxString,wxString> InvertedSeedRequests;
  //InvertedSeedRequests.swap(m_seed_requests);

  m_seed_count = 0;
  m_leech_count = 0;

  std::vector<wxString> torrents_to_join;/// threads rule 4

  {/// threads rule 1,5,6 with JoinTorrent() call
    ScopedLocker<HashToTorrentData> torrent_infos_l(m_torrents_infos);/// threads rule 8
    ScopedLocker<SeedRequests> seed_requests_l(m_seed_requests);
    ScopedLocker<OpenTorrents> open_torrents_l(m_open_torrents);

    for ( SeedRequests::iterator i = seed_requests_l.Get().begin(); i != seed_requests_l.Get().end(); i++ )
    {
      if( m_seed_count > 9 ) break;
      if (  ( usync()->MapExists( i->first, i->second ) || usync()->ModExists( i->first ) ) && (open_torrents_l.Get().find( i->first ) == open_torrents_l.Get().end()) && (torrent_infos_l.Get().find(i->second) != torrent_infos_l.Get().end()) ) /// torrent is requested and present, but not joined yet
      {
        torrents_to_join.push_back(i->second);
        m_seed_count++;
        open_torrents_l.Get()[i->first] = true;
      }
    }
  }
  for(std::vector<wxString>::iterator i=torrents_to_join.begin();i!=torrents_to_join.end();++i){
    JoinTorrent( *i );
  }

  bool do_reload_unitsync=false;
  for( std::vector<libtorrent::torrent_handle>::iterator i = TorrentList.begin(); i != TorrentList.end(); i++)
  {
    wxLogMessage(_T("Fixing torrent list entry for %s"), WX_STRING(i->name()).c_str());
    if ( i->is_seed() ) m_seed_count++;
    else
    {
      m_leech_count++;
      break;
    }
    wxString StrippedName;
    {
      ScopedLocker<TorrentHandleToHash> torrent_handles_l(m_torrent_handles);
      TorrentHandleToHash::iterator itor = torrent_handles_l.Get().from.find(*i);
      if ( itor == torrent_handles_l.Get().end() ) continue;
      ScopedLocker<HashToTorrentData> torrent_infos_l(m_torrents_infos);
      HashToTorrentData::iterator iter = torrent_infos_l.Get().find( itor->second );
      if ( iter == torrent_infos_l.Get().end() ) continue;
      StrippedName = iter->second.name;
    }

    bool decrease_leech_count=false;
    bool do_remove_torrent=false;/// threads rule 4
    wxString notify_message;
    {/// threads rule 1, 3, 6
      ScopedLocker<SeedRequests> seed_requests_l(m_seed_requests);/// threads rule 8
      ScopedLocker<OpenTorrents> open_torrents_l(m_open_torrents);

      OpenTorrents::iterator open_torrent_i=open_torrents_l.Get().find(StrippedName);

      bool is_ok=true;
      if(open_torrent_i==open_torrents_l.Get().end()){
        is_ok=false;
      }
      if ( is_ok && !(open_torrent_i->second) ) ///torrent has finished download, refresh unitsync and remove file from list
      {
        notify_message= _T("N-|")  + seed_requests_l.Get().from[StrippedName] + _T("\n"); ///notify the system we don't need the file anymore
        decrease_leech_count = true;
        open_torrents_l.Get().erase(open_torrent_i);
        do_reload_unitsync=true;
        do_remove_torrent=true;
      }else
      if ( seed_requests_l.Get().from.find( StrippedName ) == seed_requests_l.Get().from.end() )/// if torrent not in request list but still seeding then remove
      {
        do_remove_torrent=true;
        if(is_ok)open_torrents_l.Get().erase(open_torrent_i);
      }
    }
    if(do_remove_torrent)
    {
      m_torr->remove_torrent( *i );
      {
        ScopedLocker<TorrentHandleToHash> torrent_handles_l(m_torrent_handles);
        torrent_handles_l.Get().erase( torrent_handles_l.Get().from.find(*i) );
      }
    }
    if(!notify_message.empty())m_socket_class->Send(notify_message);
    if(decrease_leech_count)m_leech_count--;
  }

  if(do_reload_unitsync)usync()->ReloadUnitSyncLib();
}


void TorrentWrapper::ReceiveandExecute( const wxString& msg )
{
  wxLogMessage(_T("torrent: %s"), msg.c_str() );

  wxArrayString data = wxStringTokenize( msg, _T('|') );

  if ( data.GetCount() == 0 ) return;
  // T+|hash|name|type 	 informs client that new torrent was added to server (type is either MOD or MAP)
  else if ( data.GetCount() > 3 && data[0] == _T("T+") ) {
    TorrentData newtorrent;
    newtorrent.hash = data[1];
    newtorrent.name = data[2];
    if ( data[3] == _T("MAP") ) newtorrent.type = map;
    else if ( data[3] == _T("MOD") ) newtorrent.type = mod;

    int tmp_type=newtorrent.type;
    wxLogMessage(_T("m_torrent_infos[%s] = {hash=%s, name=%s, type=%d}"),data[1].c_str(),newtorrent.hash.c_str(),newtorrent.name.c_str(),tmp_type);

    {/// threads rule 3
      ScopedLocker<HashToTorrentData> torrent_infos_l(m_torrents_infos);
      torrent_infos_l.Get()[data[1]] = newtorrent;
      ScopedLocker<NameToHash> name_to_hash_l(m_name_to_hash);
      name_to_hash_l.Get().from[newtorrent.name] = newtorrent.hash;
    }


   // m_socket_class->Send(  _T("IH|") + data[1] + _T("\n") );
  // T-|hash 	 informs client that torrent was removed from server
  } else if ( data[0] == _T("T-") && data.GetCount() > 1 ) {
    ScopedLocker<HashToTorrentData> torrent_infos_l(m_torrents_infos);
    HashToTorrentData::iterator itor = torrent_infos_l.Get().find(data[1]);
    if( itor == torrent_infos_l.Get().end() ) return;
    torrent_infos_l.Get().erase( itor );
    ScopedLocker<NameToHash> name_to_hash_l(m_name_to_hash);
    NameToHash::to::iterator iter = name_to_hash_l.Get().to.find(data[1]);
    if( iter == name_to_hash_l.Get().to.end() ) return;
    name_to_hash_l.Get().erase( iter );
  // S+|hash|seeders|leechers 	 tells client that seed is needed for this torrent
  } else if ( data.GetCount() > 1 && data[0] == _T("S+") ) {
    wxString name;
    {
      ScopedLocker<HashToTorrentData> torrent_infos_l(m_torrents_infos);
      HashToTorrentData::iterator itor = torrent_infos_l.Get().find(data[1]);
      if ( itor == torrent_infos_l.Get().end() ) return;
      name =  itor->second.name;
    }
    {
      ScopedLocker<SeedRequests> seed_requests_l(m_seed_requests);
      seed_requests_l.Get().from[name] = data[1];
    }
    unsigned long seeders=0;
    unsigned long leechers=0;
    if(data.GetCount() > 2)data[2].ToULong(&seeders);
    if(data.GetCount() > 3)data[3].ToULong(&leechers);
  // S-|hash 	 tells client that seed is no longer neede for this torrent
  } else if ( data.GetCount() > 1 && data[0] == _T("S-") ) {
    wxString name;
    {
      ScopedLocker<HashToTorrentData> torrent_infos_l(m_torrents_infos);
      HashToTorrentData::iterator itor = torrent_infos_l.Get().find(data[1]);
      if ( itor == torrent_infos_l.Get().end() ) return;
      name =  itor->second.name;
    }
    {
      ScopedLocker<SeedRequests> seed_requests_l(m_seed_requests);
      SeedRequests::iterator iter = seed_requests_l.Get().find(name);
      if ( iter == seed_requests_l.Get().end() ) return;
      seed_requests_l.Get().from.erase(iter);
    }
  // M+|hash|url 	 It tells the client if url is given that http mirror exists for given hash, else there are no mirrors.
  } else if ( data[0] == _T("M+") && data.GetCount() > 2 ) {
    {
      ScopedLocker<TorrentHandleToHash> torrent_handles_l(m_torrent_handles);
      TorrentHandleToHash::to::iterator iter = torrent_handles_l.Get().to.find(data[1]);
      if ( iter == torrent_handles_l.Get().to.end() ) return;
      for( unsigned int index = 2; index < data.GetCount(); index++ )
        iter->second.add_url_seed( STD_STRING( data[index] ) );
    }
  // PING 	 every minute - client must respond with its own "PING"
  } else if ( data[0] == _T("PING") ) {
    m_socket_class->Send( _T("PING\n") );
  //IH|hash|infohash infos the client about torrent's infohash b64 encoded
  } else if ( data.GetCount() > 2 && data[0] == _T("IH") ) {
    ScopedLocker<HashToTorrentData> torrent_infos_l(m_torrents_infos);
    HashToTorrentData::iterator itor = torrent_infos_l.Get().find(data[1]);
    if ( itor == torrent_infos_l.Get().end() ) return;
    itor->second.infohash = data[2];
  }
}


void TorrentWrapper::OnConnected( Socket* sock )
{
  wxLogMessage(_T("torrent system connected") );
  m_connected = true;

  m_torr->start_dht();

  /// threads rule 8 plus here we want to lock it all so that other thread wont get inconsistent data
  ScopedLocker<HashToTorrentData> torrent_infos_l(m_torrents_infos);
  ScopedLocker<SeedRequests> seed_requests_l(m_seed_requests);
  ScopedLocker<OpenTorrents> open_torrents_l(m_open_torrents);
  ScopedLocker<TorrentHandleToHash> torrent_handles_l(m_torrent_handles);

  torrent_infos_l.Get().clear();
  seed_requests_l.Get().clear();
  open_torrents_l.Get().clear();
  torrent_handles_l.Get().clear();

  m_seed_count = 0;
  m_leech_count = 0;

}


void TorrentWrapper::OnDisconnected( Socket* sock )
{
  wxLogMessage(_T("torrent system disconnected") );
  std::vector<libtorrent::torrent_handle> TorrentList = m_torr->get_torrents();
  for( std::vector<libtorrent::torrent_handle>::iterator i = TorrentList.begin(); i != TorrentList.end(); i++) m_torr->remove_torrent(*i); ///remove all torrents upon disconnect
  m_connected = false;

  m_torr->stop_dht();


  /// threads rule 8 plus here we want to lock it all so that other thread wont get inconsistent data
  ScopedLocker<HashToTorrentData> torrent_infos_l(m_torrents_infos);
  ScopedLocker<SeedRequests> seed_requests_l(m_seed_requests);
  ScopedLocker<OpenTorrents> open_torrents_l(m_open_torrents);
  ScopedLocker<TorrentHandleToHash> torrent_handles_l(m_torrent_handles);


  torrent_infos_l.Get().clear();
  seed_requests_l.Get().clear();
  open_torrents_l.Get().clear();
  torrent_handles_l.Get().clear();


  m_seed_count = 0;
  m_leech_count = 0;
}


void TorrentWrapper::OnDataReceived( Socket* sock )
{
  if ( sock == 0 ) return;

  wxString data;


  do {

    data = _T("");
    if ( sock->Receive( data ) ) {
      m_buffer += data;
      wxString cmd;
      if ( ( cmd = m_buffer.BeforeFirst( '\n' ) ) != _T("") )
      {
        m_buffer = m_buffer.AfterFirst( '\n' );
        ReceiveandExecute( cmd );
      }
    }
  } while ( !data.IsEmpty() );
}

#endif