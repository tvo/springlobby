/* Copyright (C) 2007 The SpringLobby Team. All rights reserved. */

#include <wx/panel.h>
#include <wx/dcclient.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/intl.h>
#include <cmath>
#include <stdexcept>

#include "utils.h"
#include "uiutils.h"
#include "mapctrl.h"
#include "iunitsync.h"
#include "user.h"
#include "utils.h"
#include "ui.h"
#include "server.h"

#include "images/close.xpm"
#include "images/close_hi.xpm"
#include "images/start_ally.xpm"
#include "images/start_enemy.xpm"
#include "images/start_unused.xpm"
#include "images/player.xpm"
#include "images/bot.xpm"
#include "images/arm.xpm"
#include "images/core.xpm"
#include "images/up_down.xpm"
#include "images/upsel_down.xpm"
#include "images/up_downsel.xpm"
#include "images/not_found_icon.xpm"
#include "images/download_map.xpm"
#include "images/reload_map.xpm"


BEGIN_EVENT_TABLE( MapCtrl, wxPanel )
  EVT_PAINT( MapCtrl::OnPaint )
  EVT_SIZE( MapCtrl::OnResize )
  EVT_MOTION( MapCtrl::OnMouseMove )
  EVT_LEFT_DOWN( MapCtrl::OnLeftDown )
  EVT_LEFT_UP( MapCtrl::OnLeftUp )
  EVT_MOUSEWHEEL( MapCtrl::OnMouseWheel )
END_EVENT_TABLE()

const int boxsize = 8;
const int minboxsize = 40;

MapCtrl::MapCtrl( wxWindow* parent, int size, IBattle* battle, Ui& ui, bool readonly, bool fixed_size, bool draw_start_types, bool singleplayer ):
  wxPanel( parent, -1, wxDefaultPosition, wxSize(size, size), wxSIMPLE_BORDER|wxFULL_REPAINT_ON_RESIZE ),
  m_image(0),
  m_battle(battle),
  m_ui(ui),
  m_mapname(_T("")),
  m_draw_start_types(draw_start_types),
  m_fixed_size(fixed_size),
  m_ro(readonly),
  m_sp(singleplayer),
  m_mover_rect(-2),
  m_rect_area(RA_Main),
  m_last_rect_area(RA_Main),
  m_maction(MA_None),
  m_lastsize(-1,-1),
  m_close_img(0),
  m_close_hi_img(0),
  m_start_ally(0),
  m_start_enemy(0),
  m_start_unused(0),
  m_player_img(0),
  m_bot_img(0),
  m_nfound_img(0),
  m_reload_img(0),
  m_dl_img(0),
  m_bot_expanded(-1)
{
  SetBackgroundStyle( wxBG_STYLE_CUSTOM );
  SetBackgroundColour( *wxLIGHT_GREY );
  if ( !m_ro ) {
    m_close_img = new wxBitmap( close_xpm );
    m_close_hi_img = new wxBitmap( close_hi_xpm );
  }
  m_tmp_brect.ally = -1;
}


MapCtrl::~MapCtrl()
{
  FreeMinimap();
  delete m_close_img;
  delete m_close_hi_img;
  delete m_start_ally;
  delete m_start_enemy;
  delete m_start_unused;
  delete m_nfound_img;
  delete m_reload_img;
  delete m_dl_img;
}


void MapCtrl::SetBattle( IBattle* battle )
{
  m_battle = battle;
  UpdateMinimap();
}


wxRect MapCtrl::_GetMinimapRect()
{
  if ( m_image == 0 ) return wxRect();

  int cwidth, cheight, top = 0, left = 0;
  GetClientSize( &cwidth, &cheight );

  if ( m_image->GetWidth() < cwidth ) {
    left = (cwidth - m_image->GetWidth()) / 2;
  } else {
    top = (cheight - m_image->GetHeight()) / 2;
  }
  return wxRect( left, top, m_image->GetWidth(), m_image->GetHeight() );
}


wxRect MapCtrl::_GetStartRect( int index )
{
  ASSERT_LOGIC( BattleType() != BT_Multi, "MapCtrl::_GetStartRect(): Battle type is not BT_Multi" );
  BattleStartRect* sr = m_battle->GetStartRect( index );
  if ( sr == 0 ) return wxRect();
  if ( sr->deleted ) return wxRect();
  return _GetStartRect( *sr );
}


wxRect MapCtrl::_GetStartRect( const BattleStartRect& sr )
{
  int x1,y1,x2,y2;
  if ( sr.deleted ) return wxRect();
  wxRect mr = _GetMinimapRect();

  x1 = int( (mr.x + sr.left * mr.width / 200.0) + 0.5 );
  y1 = int( (mr.y + sr.top * mr.height / 200.0) + 0.5 );
  x2 = int( (mr.x + sr.right * mr.width / 200.0) + 0.5 );
  y2 = int( (mr.y + sr.bottom * mr.height / 200.0) + 0.5 );

  return wxRect( x1, y1, x2-x1, y2-y1 );
}


int MapCtrl::_GetNewRectIndex()
{
  for (int i = 0; i < 16; i++ ) {
    wxRect r = _GetStartRect( i );
    if ( r.IsEmpty() ) return i;
  }
  return -1;
}


BattleStartRect MapCtrl::_GetBattleRect( int x1, int y1, int x2, int y2, int ally )
{
  BattleStartRect br;
  wxRect mr = _GetMinimapRect();

  br.ally = ally;
  br.left = int( (200.0 * ( x1 - mr.x ) / mr.width) + 0.5 );
  br.top = int( (200.0 * ( y1 - mr.y ) / mr.height) + 0.5 );
  br.right = int( (200.0 * ( x2 - mr.x ) / mr.width) + 0.5 );
  br.bottom = int( (200.0 * ( y2 - mr.y ) / mr.height) + 0.5 );

  if ( br.left < 0 ) br.left = 0; if ( br.left > 200 ) br.left = 200;
  if ( br.top < 0 ) br.top = 0; if ( br.top > 200 ) br.top = 200;
  if ( br.right < 0 ) br.right = 0; if ( br.right > 200 ) br.right = 200;
  if ( br.bottom < 0 ) br.bottom = 0; if ( br.bottom > 200 ) br.bottom = 200;

  return br;
}


void MapCtrl::_SetMouseOverRect( int index )
{
  int oldindex = m_mover_rect;
  m_mover_rect = index;
  _SetCursor();

  if ( (index != oldindex) || (m_rect_area != m_last_rect_area) ) UpdateMinimap();
  m_last_rect_area = m_rect_area;
}


void MapCtrl::_SetCursor()
{
  if ( !m_image ) {
    if ( m_rect_area != RA_Main ) SetCursor( wxCursor( wxCURSOR_HAND ) );
    else SetCursor( wxCursor( wxCURSOR_ARROW ) );
    return;
  }

  if ( m_sp ) {

    if ( m_battle != 0 ) {
      if ( m_battle->GetStartType() != ST_Choose ) {
        SetCursor( wxCursor( wxCURSOR_ARROW ) );
        return;
      } else {
        SetCursor( wxCursor( wxCURSOR_ARROW ) );
        return;
      }
    }
    SetCursor( wxCursor( wxCURSOR_ARROW ) );
    return;

  }

  if ( !m_ro ) {
    if ( m_mover_rect >= 0 ) {
      if      ( m_rect_area == RA_UpLeft )    SetCursor( wxCursor( wxCURSOR_SIZENWSE ) );
      else if ( m_rect_area == RA_UpRight )   SetCursor( wxCursor( wxCURSOR_ARROW ) );
      else if ( m_rect_area == RA_DownLeft )  SetCursor( wxCursor( wxCURSOR_SIZENESW ) );
      else if ( m_rect_area == RA_DownRight ) SetCursor( wxCursor( wxCURSOR_SIZENWSE ) );
      else SetCursor( wxCursor( wxCURSOR_SIZING ) );
    } else if ( m_mover_rect == -1 ) {
      SetCursor( wxCursor( wxCURSOR_CROSS ) );
    } else {
      SetCursor( wxCursor( wxCURSOR_ARROW ) );
    }
  } else {
    if ( m_mover_rect >= 0 ) SetCursor( wxCursor( wxCURSOR_HAND ) );
    else SetCursor( wxCursor( wxCURSOR_ARROW ) );
  }
}


void MapCtrl::_RelocateBots()
{
  if ( m_battle == 0 ) return;

  for ( unsigned int i = 0; i < m_battle->GetNumBots(); i++ ) {
    BattleBot* bot = m_battle->GetBot( i );
    ASSERT_LOGIC( bot != 0, "bot == 0" );
    m_battle->GetFreePosition( bot->posx, bot->posy );
    if ( bot->posx == -1 ) m_battle->RemoveBot( i );
  }
}


void MapCtrl::_GetClosestStartPos( int fromx, int fromy, int& index, int& x, int& y, int& range )
{
  if ( m_battle == 0 ) return;
  UnitSyncMap map = m_battle->Map();

  int newrange;
  range = -1;
  index = -1;

  for ( int i = 0; i < map.info.posCount; i++ ) {
    int dx = fromx - map.info.positions[i].x;
    int dy = fromy - map.info.positions[i].y;
    newrange = (int)sqrt( dx*dx + dy*dy );
    if ( ( range == -1 ) || ( range > newrange )) {
      range = newrange;
      index = i;
    }
  }

  if ( index != -1 ) {
    x = map.info.positions[index].x;
    y = map.info.positions[index].y;
  }
}


void MapCtrl::LoadMinimap()
{
  debug_func("");
  if ( m_battle == 0 ) return;
  if ( m_image ) return;
  if ( !m_battle->MapExists() ) return;

  wxString map = m_battle->GetMapName();
  try {
    int w, h;
    GetClientSize( &w, &h );
    if ( w * h == 0 ) {
      m_image = 0;
      m_mapname = _T("");
      m_lastsize = wxSize( -1, -1 );
      return;
    }
    wxImage img = usync()->GetMinimap( STD_STRING(map), w, h, m_fixed_size );
    m_image = new wxBitmap( img );
    m_mapname = map;
    m_lastsize = wxSize( w, h );
    Refresh();
    Update();
  } catch (...) {
    m_image = 0;
    m_mapname = _T("");
  }
  if ( m_sp ) _RelocateBots();

}


void MapCtrl::FreeMinimap()
{
  if ( m_image ) {
    delete m_image;
    m_image = 0;
    m_mapname = _T("");
  }
}


void MapCtrl::UpdateMinimap()
{
  int w, h;
  _SetCursor();
  if ( m_battle == 0 ) return;
  GetClientSize( &w, &h );
  if ( (m_mapname != m_battle->GetMapName() ) || ( m_lastsize != wxSize(w, h) ) ) {
    FreeMinimap();
    LoadMinimap();
  }
  Refresh();
  Update();
}


void MapCtrl::_RequireImages()
{
  if ( !m_start_ally ) m_start_ally = new wxBitmap( start_ally_xpm );
  if ( !m_start_enemy ) m_start_enemy = new wxBitmap( start_enemy_xpm );
  if ( !m_start_unused ) m_start_unused = new wxBitmap( start_unused_xpm );
  if ( !m_player_img ) m_player_img = new wxBitmap( player_xpm );
  if ( !m_bot_img ) m_bot_img = new wxBitmap( bot_xpm );
}


void MapCtrl::_DrawStartRect( wxDC& dc, int index, const wxRect& sr, const wxColour& col, bool mouseover, int alphalevel )
{

  if ( sr.width * sr.height == 0 ) return;

  dc.SetBrush( wxBrush( *wxLIGHT_GREY, wxTRANSPARENT ) );

  wxImage img( sr.width, sr.height );
  wxColour light;
  light.Set( col.Red()+100>200?200:col.Red()+100, col.Green()+100>200?200:col.Green()+100, col.Blue()+100>200?200:col.Blue()+100  );
  img.SetRGB( wxRect( 0, 0, sr.width, sr.height ), light.Red(), light.Green(), light.Blue() );
  unsigned char *alpha = (unsigned char*)malloc( (sr.width)*(sr.height) );
  for ( int y = 0; y < sr.height; y++ ) {
    int a;
    if ( (y%3) == 0 ) a = alphalevel;
    else a = (alphalevel-40 < 0)? 0 : alphalevel-40;
    for ( int x = 0; x < sr.width; x++ ) {
      alpha[y*(sr.width)+x] = a;
    }
  }
  img.SetAlpha( alpha );
  wxBitmap bmpimg( img );
  dc.DrawBitmap( bmpimg, sr.x, sr.y, false );

  int twidth, theight;
/*  wxFont f( 12, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL|wxFONTFLAG_ANTIALIASED, wxFONTWEIGHT_LIGHT );
  dc.SetFont( f );*/
  if ( index != -1 ) {
    dc.GetTextExtent( wxString::Format( _T("%d"), index+1), &twidth, &theight );
    dc.SetTextForeground( col );
    _DrawOutlinedText( dc, wxString::Format( _T("%d"), index+1), sr.x + sr.width / 2 - twidth / 2, sr.y + sr.height / 2 - theight / 2 - 1, wxColour(50,50,50), *wxWHITE );
    //dc.DrawText( wxString::Format( _T("%d"), index+1), sr.x + sr.width / 2 - twidth / 2, sr.y + sr.height / 2 - theight / 2 - 1 );
  }

  dc.SetPen( wxPen( col ) );

  if ( mouseover && !m_ro ) {

    dc.DrawRectangle( sr.x+1, sr.y+1, sr.width-2, sr.height-2 );
    dc.SetPen( wxPen( *wxBLACK ) );
    dc.DrawRectangle( sr.x, sr.y, sr.width, sr.height );

    dc.SetBrush( wxBrush( *wxBLACK, wxSOLID ) );
    dc.DrawRectangle( sr.x, sr.y, boxsize, boxsize );
    dc.DrawRectangle( sr.x + sr.width - boxsize, sr.y + sr.height - boxsize, boxsize, boxsize );
    //dc.DrawRectangle( sr.x + sr.width - boxsize, sr.y + 1, boxsize, boxsize );
    //dc.DrawRectangle( sr.x, sr.y + sr.height - boxsize, boxsize, boxsize );

    if ( m_rect_area != RA_UpRight ) dc.DrawBitmap( *m_close_img, sr.x + sr.width - 16, sr.y + 2, true );
    else dc.DrawBitmap( *m_close_hi_img, sr.x + sr.width - 16, sr.y + 2, true );

  } else {
    dc.DrawRectangle( sr.x, sr.y, sr.width, sr.height );
  }
}


wxRect MapCtrl::_GetRefreshRect()
{
  int width, height;
  GetClientSize( &width, &height );
  return wxRect( 5, height - 27, 72, 22 );
}


wxRect MapCtrl::_GetDownloadRect()
{
  int width, height;
  GetClientSize( &width, &height );
  return wxRect( 5, height - 52, 87, 22 );
}


void MapCtrl::_DrawBackground( wxDC& dc )
{
  int width, height;
  GetClientSize( &width, &height );

  dc.SetPen( wxPen( *wxLIGHT_GREY ) );
  dc.SetBrush( wxBrush( *wxLIGHT_GREY, wxSOLID ) );

  if ( m_battle == 0 ) {
    dc.DrawRectangle( 0, 0, width, height );
    return;
  }

  wxRect r = _GetMinimapRect();

  // Draw background.
  if ( r.y > 0 ) {
    dc.DrawRectangle( 0, 0, width, (height - r.height) / 2 );
    dc.DrawRectangle( 0, r.y+r.height, width, (height - r.height) / 2 + 1 );
  }
  if ( r.x > 0 ) {
    dc.DrawRectangle( 0, 0, (width - r.width) / 2, height );
    dc.DrawRectangle( r.x+r.width, 0, (width - r.width) / 2 + 1, height );
  }

  // Draw minimap.
  if ( !m_image ) {

    if ( m_sp ) return;

    if ( !m_nfound_img ) m_nfound_img = new wxBitmap( not_found_icon_xpm );
    if ( !m_reload_img ) m_reload_img = new wxBitmap( reload_map_xpm );
    if ( !m_dl_img ) m_dl_img = new wxBitmap( download_map_xpm );

    dc.DrawBitmap( *m_nfound_img, width/2 - 32, height/2 - 32, true );
    if ( (m_rect_area == RA_Refresh) && (m_mdown_area != RA_Refresh) ) dc.DrawBitmap( *m_reload_img, 6, height - 28, true );
    else dc.DrawBitmap( *m_reload_img, 5, height - 27, true );
    if ( (m_rect_area == RA_Download) && (m_mdown_area != RA_Download) ) dc.DrawBitmap( *m_dl_img, 6, height - 53, true );
    else dc.DrawBitmap( *m_dl_img, 5, height - 52, true );

    dc.SetFont( wxFont( 9, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL ) );
    if ( m_rect_area == RA_Refresh ) _DrawOutlinedText( dc, _("Refresh"), 28, height - 25, wxColour(50,50,50), *wxWHITE );
    else _DrawOutlinedText( dc, _("Refresh"), 28, height - 25, *wxWHITE, wxColour(50,50,50) );
    if ( m_rect_area == RA_Download ) _DrawOutlinedText( dc, _("Download"), 28, height - 50, wxColour(50,50,50), *wxWHITE );
    else  _DrawOutlinedText( dc, _("Download"), 28, height - 50, *wxWHITE, wxColour(50,50,50) );

  } else {
    dc.DrawBitmap( *m_image, r.x, r.y, false );
    width = r.width;
    height = r.height;
  }

}


void MapCtrl::_DrawStartRects( wxDC& dc )
{
  for ( int i = 0; i < 15; i++ ) {
    wxRect sr = _GetStartRect( i );
    if ( sr.IsEmpty() ) continue;
    wxColour col;
    if ( i == m_battle->GetMyAlly() ) {
      col.Set( 0, 200, 0 );
    } else {
      col.Set( 200, 0, 0 );
    }
    _DrawStartRect( dc, i, sr, col, m_mover_rect == i );
  }
}


void MapCtrl::_DrawStartPositions( wxDC& dc )
{
  wxRect mr = _GetMinimapRect();
  m_map = m_battle->Map();
  _RequireImages();

  if ( m_battle->GetStartType() == ST_Fixed ) {

    wxFont f( 7, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_LIGHT );
    dc.SetFont( f );
    for ( int i = 0; i < m_map.info.posCount; i++ ) {
      int x = (int)( (double)((double)m_map.info.positions[i].x / (double)m_map.info.width) * (double)mr.width ) - 8;
      int y = (int)( (double)(m_map.info.positions[i].y / (double)m_map.info.height) * (double)mr.height ) - 8;
      dc.DrawBitmap( *m_start_ally, x+mr.x, y+mr.y, true );
      wxCoord w, h;
      dc.GetTextExtent( wxString::Format(_T("%d"), i+1 ), &w, &h );
      dc.DrawText( wxString::Format(_T("%d"), i+1 ), x+mr.x+(8-w/2), y+mr.y+(8-h/2) );
    }
  } else {
    // Draw startpositions
    for ( int i = 0; i < m_map.info.posCount; i++ ) {
      int x = (int)( (double)((double)m_map.info.positions[i].x / (double)m_map.info.width) * (double)mr.width ) - 8;
      int y = (int)( (double)(m_map.info.positions[i].y / (double)m_map.info.height) * (double)mr.height ) - 8;
      dc.DrawBitmap( *m_start_unused, x+mr.x, y+mr.y, true );
    }
  }
}


wxRect MapCtrl::_GetBotRect( BattleBot& bot, bool selected )
{
  ASSERT_LOGIC( m_battle != 0, "Bot == 0" );
  wxRect mr = _GetMinimapRect();
  m_map = m_battle->Map();
  int x = (int)( (double)((double)bot.posx / (double)m_map.info.width) * (double)mr.width ) - 8;
  int y = (int)( (double)(bot.posy / (double)m_map.info.height) * (double)mr.height ) - 8;
  int box_height = 70;
  int box_width = 75;
  if ( selected ) {
    bool we = x+box_width>mr.width;
    bool he = y+box_height>mr.height;
    if ( we ) {
      if ( he ) {
        //bro = BRO_BottomRight;
        return wxRect( mr.x+mr.width-box_width, mr.y+mr.height-box_height, box_width, box_height );
      } else {
        //bro = BRO_TopRight;
        return wxRect( mr.x+mr.width-box_width, y+mr.y-2, box_width, box_height );
      }
    } else if ( he ) {
      //bro = BRO_BottomLeft;
      return wxRect( x+mr.x-2, mr.y+mr.height-box_height, box_width, box_height );
    } else {
      //bro = BRO_TopLeft;
      return wxRect( x+mr.x-2, y+mr.y-2, box_width, box_height );
    }
  } else {
    //bro = BRO_TopLeft;
    return wxRect( x+mr.x-2, y+mr.y-2, 20, 20 );
  }
}


RectArea MapCtrl::_GetBotRectArea( const wxRect& botrect, int x, int y )
{
  x = x - botrect.x;
  y = y - botrect.y;
  wxRect close = _GetBotCloseRect();
  if ( close.Inside( x, y ) ) return RA_Close;
  wxRect side = _GetBotSideRect();
  if ( side.Inside( x, y ) ) return RA_Side;
  wxRect AllyUp = _GetBotUpAllyButtonRect();
  if ( AllyUp.Inside( x, y ) ) return RA_UpAllyButton;
  wxRect AllyDown = _GetBotDownAllyButtonRect();
  if ( AllyDown.Inside( x, y ) ) return RA_DownAllyButton;
  wxRect handicap = _GetBotHandicapRect();
  if ( handicap.Inside( x, y ) ) return RA_Handicap;
  wxRect HandicapUp = _GetBotUpHandicapButtonRect();
  if ( HandicapUp.Inside( x, y ) ) return RA_UpHandicapButton;
  wxRect HandicapDown = _GetBotDownHandicapButtonRect();
  if ( HandicapDown.Inside( x, y ) ) return RA_DownHandicapButton;

  wxRect bot( 0, 0, 16, 16 );
  if ( bot.Inside( x, y ) ) return RA_Move;
  return RA_Main;
}


void MapCtrl::_DrawOutlinedText( wxDC& dc, const wxString& str, int x, int y, const wxColour& outline, const wxColour& font )
{
  dc.SetTextForeground( outline );
  dc.DrawText( str, x, y );
  dc.DrawText( str, x, y+1 );
  dc.DrawText( str, x+1, y );
  dc.DrawText( str, x+2, y );
  dc.DrawText( str, x, y+2 ); // Hack to get font outline
  dc.DrawText( str, x+2, y+2 );
  dc.SetTextForeground( font );
  dc.DrawText( str, x+1, y+1 );

}

void MapCtrl::_DrawBot( wxDC& dc, BattleBot& bot, bool selected, bool moving )
{
  wxBitmap* img;
  if ( bot.aidll == "" ) img = m_player_img;
  else img = m_bot_img;

  wxFont f( 9, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_LIGHT );
  dc.SetFont( f );
  dc.SetTextForeground( *wxWHITE );

  wxRect r = _GetBotRect( bot, selected );
  wxColour col = wxColour( bot.bs.color_r,bot.bs.color_g,bot.bs.color_b );

  if ( selected ) {
    wxRect siderect = _GetBotSideRect();
    _DrawStartRect( dc, -1, r,ColourDelta( col, -40 ) , false, 180 );
    _DrawOutlinedText( dc, _("side:"), r.x+3, r.y+siderect.y-1, wxColour(50,50,50), *wxWHITE );

    if ( m_rect_area == RA_Side ) {
      _DrawStartRect( dc, -1, wxRect( r.x+siderect.x-1, r.y+siderect.y-1, siderect.width+2, siderect.height+2 ), col, false );
    }

    wxBitmap* bmp = 0;
    try {
      std::string mod = STD_STRING(m_battle->GetModName());
      usync()->SetCurrentMod( mod );
      usync()->GetSideCount();
      std::string side = usync()->GetSideName( bot.bs.side );
      bmp = new wxBitmap( usync()->GetSidePicture( side ) );
    } catch (...) {
      delete bmp;
      if ( bot.bs.side == 0 ) bmp = new wxBitmap( arm_xpm );
      else bmp = new wxBitmap( core_xpm );
    }

    dc.DrawBitmap( *bmp, r.x+siderect.x, r.y+siderect.y, true );
    delete bmp;

    wxRect updownallyrect = _GetBotUpAllyButtonRect();
    _DrawOutlinedText( dc, wxString::Format( _("ally:   %d"), bot.bs.ally + 1 ), r.x+3, r.y+updownallyrect.y, wxColour(50,50,50), *wxWHITE );
    //dc.DrawText( wxString::Format( _("ally: %d"), bot.bs.ally + 1 ), r.x+4, r.y+40 );

    if ( m_rect_area == RA_UpAllyButton ) dc.DrawBitmap( wxBitmap(upsel_down_xpm), r.x+updownallyrect.x, r.y+updownallyrect.y, true );
    else if ( m_rect_area == RA_DownAllyButton ) dc.DrawBitmap( wxBitmap(up_downsel_xpm), r.x+updownallyrect.x, r.y+updownallyrect.y, true );
    else dc.DrawBitmap( wxBitmap(up_down_xpm), r.x+updownallyrect.x, r.y+updownallyrect.y, true );

    wxRect updownhandicaprect = _GetBotUpHandicapButtonRect();
    wxFont b( 6, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_LIGHT );
    dc.SetFont( b );
    _DrawOutlinedText( dc, wxString::Format( _("bonus: %d%%"), bot.bs.handicap ), r.x+3, r.y+updownhandicaprect.y+2, wxColour(50,50,50), *wxWHITE );
    dc.SetFont( f );

    if ( m_rect_area == RA_UpHandicapButton ) dc.DrawBitmap( wxBitmap(upsel_down_xpm), r.x+updownhandicaprect.x, r.y+updownhandicaprect.y, true );
    else if ( m_rect_area == RA_DownHandicapButton ) dc.DrawBitmap( wxBitmap(up_downsel_xpm), r.x+updownhandicaprect.x, r.y+updownhandicaprect.y, true );
    else dc.DrawBitmap( wxBitmap(up_down_xpm), r.x+updownhandicaprect.x, r.y+updownhandicaprect.y, true );

    dc.SetPen( wxPen( col ) );
    dc.SetBrush( wxBrush( col, wxSOLID ) );

    dc.DrawRectangle( r.x+1, r.y+1, r.width-2, 18 );

    wxRect closerect = _GetBotCloseRect();
    if ( m_rect_area == RA_Close ) dc.DrawBitmap( *m_close_hi_img, r.x+closerect.x, r.y+closerect.y, true );
    else dc.DrawBitmap( *m_close_img, r.x+closerect.x, r.y+closerect.y, true );

    dc.DrawBitmap( *img, r.x+2, r.y+2, true );

  } else {

    dc.SetPen( wxPen( ColourDelta( col, -40 ) ) );
    dc.SetBrush( wxBrush( col, wxSOLID ) );
    dc.DrawRectangle( r.x, r.y, r.width, r.height );
    dc.DrawBitmap( *img, r.x+2, r.y+2, true );

    int w, h;
    wxString allystr = wxString::Format( _T("%d"), bot.bs.ally + 1 );
    dc.GetTextExtent( allystr, &w, &h );

    _DrawOutlinedText( dc, allystr, r.width - w - 3 + r.x, r.height - h - 1 + r.y, wxColour(50,50,50), *wxWHITE );
  }

}


void MapCtrl::_DrawSinglePlayer( wxDC& dc )
{
  debug_func("");
  if ( m_battle == 0 ) return;
  if ( !m_battle->MapExists() ) return;

  wxRect mr = _GetMinimapRect();
  m_map = m_battle->Map();
  _RequireImages();

  if ( m_battle->GetStartType() == ST_Fixed ) {
    wxFont f( 7, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_LIGHT );
    dc.SetFont( f );
  }

  for ( int i = 0; i < m_map.info.posCount; i++ ) {

    int x = (int)( (double)((double)m_map.info.positions[i].x / (double)m_map.info.width) * (double)mr.width ) - 8;
    int y = (int)( (double)(m_map.info.positions[i].y / (double)m_map.info.height) * (double)mr.height ) - 8;

    BattleBot* bot = 0;
    for ( unsigned int bi = 0; bi < m_battle->GetNumBots(); bi++ ) {
      BattleBot* tbot = m_battle->GetBot(bi);
      if ( tbot == 0 ) continue;
      if ( (tbot->posx == m_map.info.positions[i].x) && (tbot->posy == m_map.info.positions[i].y) ) {
        bot = tbot;
        break;
      }
    }

    if ( bot != 0 ) continue;

    dc.DrawBitmap( *m_start_ally, x+mr.x, y+mr.y, true );

    if ( m_battle->GetStartType() == ST_Fixed ) {
      wxCoord w, h;
      dc.GetTextExtent( wxString::Format(_T("%d"), i+1 ), &w, &h );
      dc.DrawText( wxString::Format(_T("%d"), i+1 ), x+mr.x+(8-w/2), y+mr.y+(8-h/2) );
    }
  }

  for ( unsigned int i = 0; i < m_battle->GetNumBots(); i++ ) {
    BattleBot* bot = m_battle->GetBot(i);
    if ( bot == 0 ) continue;

    bool expanded = (int)i == m_bot_expanded;
    _DrawBot( dc, *bot, (m_maction != MA_Move) && expanded, (m_maction == MA_Move) && expanded );

  }

}


void MapCtrl::OnPaint( wxPaintEvent& WXUNUSED(event) )
{
  debug_func("");
  wxPaintDC dc( this );

  _DrawBackground( dc );

  if ( m_battle == 0 ) return;

  if ( !m_image ) return;

  if ( m_sp ) {
    _DrawSinglePlayer( dc );
  } else {

    if ( m_draw_start_types ) {

      if ( m_battle->GetStartType() == ST_Choose ) {
        _DrawStartRects( dc );
      } else {
        _DrawStartPositions( dc );
      }

    }

    // Draw add rect.
    if ( m_tmp_brect.ally != -1 ) {
      _DrawStartRect( dc, m_tmp_brect.ally, _GetStartRect(m_tmp_brect), *wxWHITE, false );
    }

  }
}


void MapCtrl::OnResize( wxSizeEvent& event )
{
  UpdateMinimap();
}


void MapCtrl::OnMouseMove( wxMouseEvent& event )
{
  wxPoint p = event.GetPosition();
  if ( m_battle == 0 ) return;
  if ( p == wxDefaultPosition ) return;

  if ( m_sp ) {

    if ( !m_battle->MapExists() ) return;
    if ( m_maction == MA_Move ) {
      BattleBot* bot = m_battle->GetBot( m_bot_expanded );
      ASSERT_LOGIC( bot != 0, "MapCtrl::OnMouseMove(): bot = 0" );
      m_map = m_battle->Map();

      wxRect mr = _GetMinimapRect();
      wxRect r = _GetBotRect( *bot, false );

      bot->posx = (int)( ( (double)(event.GetX() - mr.x) / (double)mr.width ) * (double)m_map.info.width );
      bot->posy = (int)( ( (double)(event.GetY() - mr.y) / (double)mr.height ) * (double)m_map.info.height );
      boundry( bot->posx, 0, m_map.info.width );
      boundry( bot->posy, 0, m_map.info.height );

      int x, y, index, range;
      _GetClosestStartPos( bot->posx, bot->posy, index, x, y, range );
      if ( index != -1 ) {
        if ( range < (10.0 / (double)mr.width) * (double)m_map.info.width ) {
          bot->posx = x;
          bot->posy = y;
        }
      }
      RefreshRect( r.Union( _GetBotRect( *bot, false ) ), false );
      return;
    }

    if ( m_bot_expanded >= (int)m_battle->GetNumBots() ) m_bot_expanded = -1;

    if ( m_bot_expanded != -1 ) {
      BattleBot* bot = m_battle->GetBot( m_bot_expanded );
      ASSERT_LOGIC( bot != 0, "MapCtrl::OnMouseMove(): bot = 0" );
      wxRect r = _GetBotRect( *bot, true );
      if ( r.Inside( event.GetX(), event.GetY() )  ) {
        RectArea last = m_rect_area;
        m_rect_area = _GetBotRectArea( r, event.GetX(), event.GetY() );
        if ( last != m_rect_area ) RefreshRect( r, false );
      } else {
        m_bot_expanded = -1;
        RefreshRect( r, false );
      }
    } else {
      for ( unsigned int i = 0; i < m_battle->GetNumBots(); i++ ) {
        BattleBot* bot = m_battle->GetBot(i);
        if ( bot == 0 ) continue;
        wxRect r = _GetBotRect( *bot, false );
        if ( r.Inside( event.GetX(), event.GetY() )  ) {
          m_rect_area = _GetBotRectArea( r, event.GetX(), event.GetY() );
          m_bot_expanded = i;
          RefreshRect( _GetBotRect( *bot, true ), false );
          break;
        }
      }
    }
    return;
  }

  if ( !m_image ) {
    wxRect r = _GetRefreshRect();
    wxRect d = _GetDownloadRect();
    RectArea old = m_rect_area;
    if ( r.Inside( event.GetX(), event.GetY() ) ) {
      m_rect_area = RA_Refresh;
    } else if ( d.Inside( event.GetX(), event.GetY() ) ) {
      m_rect_area = RA_Download;
    } else {
      m_rect_area = RA_Main;
    }
    _SetCursor();
    if ( m_rect_area != old ) {
      Refresh();
      Update();
    }
    return;
  }

  if ( !m_draw_start_types ) return;

  if ( m_battle->GetStartType() != ST_Choose ) return;

  if ( m_maction == MA_Add ) { // We are currently adding a rect.

    wxRect oldr = _GetStartRect( m_tmp_brect );
    m_tmp_brect = _GetBattleRect( m_mdown_x<event.GetX()?m_mdown_x:event.GetX(), m_mdown_y<event.GetY()?m_mdown_y:event.GetY(), m_mdown_x>event.GetX()?m_mdown_x:event.GetX(), m_mdown_y>event.GetY()?m_mdown_y:event.GetY(), m_tmp_brect.ally );
    wxRect newr = _GetStartRect( m_tmp_brect );
    if ( newr != oldr ) RefreshRect( newr.Union( oldr ), false );
    return;

  } else if ( m_maction == MA_ResizeDownRight ) {

    wxRect sr = _GetStartRect( m_mdown_rect );

    wxRect nsr( sr.x, sr.y, event.GetX() - sr.x, event.GetY() - sr.y );
    if ( nsr.width < minboxsize ) nsr.SetWidth( minboxsize );
    if ( nsr.height < minboxsize ) nsr.SetHeight( minboxsize );
    BattleStartRect bsr = _GetBattleRect( nsr.x, nsr.y, nsr.x + nsr.width, nsr.y + nsr.height, m_mdown_rect );
    m_battle->AddStartRect( m_mdown_rect, bsr.left, bsr.top, bsr.right, bsr.bottom );
    if ( sr != nsr ) RefreshRect( sr.Union( nsr ), false );
    return;

  } else if ( m_maction == MA_ResizeUpLeft ) {

    wxRect sr = _GetStartRect( m_mdown_rect );

    wxRect nsr( event.GetX(), event.GetY(), (sr.x - event.GetX()) + sr.width, (sr.y - event.GetY()) + sr.height );
    if ( nsr.width < minboxsize ) {
      nsr.SetLeft( event.GetX() - minboxsize + nsr.width );
      nsr.SetWidth( minboxsize );
    }
    if ( nsr.height < minboxsize ) {
      nsr.SetTop( event.GetY() - minboxsize + nsr.height );
      nsr.SetHeight( minboxsize );
    }
    BattleStartRect bsr = _GetBattleRect( nsr.x, nsr.y, nsr.x + nsr.width, nsr.y + nsr.height, m_mdown_rect );
    m_battle->AddStartRect( m_mdown_rect, bsr.left, bsr.top, bsr.right, bsr.bottom );
    if ( sr != nsr ) RefreshRect( sr.Union( nsr ), false );
    return;

  }

  // Make sure point is inside minimap
  if ( _GetMinimapRect().Inside( p ) ) {

    // Check if point is in a startrect.
    for ( int i = 15; i >= 0; i-- ) {

      wxRect r = _GetStartRect( i );
      if ( r.IsEmpty() ) continue;

      if ( r.Inside( p ) ) {

        if ( !m_ro ) {
          if      ( (wxRect( r.x + r.width - m_close_img->GetWidth(), r.y + 1, m_close_img->GetWidth(), m_close_img->GetWidth() )).Inside( p ) ) m_rect_area = RA_UpRight;
          else if ( (wxRect( r.x, r.y, boxsize, boxsize )).Inside( p ) ) m_rect_area = RA_UpLeft;
          else if ( (wxRect( r.x + r.width - boxsize, r.y + r.height - boxsize, boxsize, boxsize )).Inside( p ) ) m_rect_area = RA_DownRight;
          //else if ( (wxRect( r.x, r.y + r.height - boxsize, boxsize, boxsize )).Inside( p ) ) m_rect_area = RA_DownLeft;
          else m_rect_area = RA_Main;
        }
        _SetMouseOverRect( i );

        return;

      }

    }
    if ( m_mover_rect != -1 ) _SetMouseOverRect( -1 );

  } else {
     if ( m_mover_rect != -2 ) _SetMouseOverRect( -2 );
  }

}


void MapCtrl::OnLeftDown( wxMouseEvent& event )
{
  if ( m_battle == 0 ) return;

  if ( m_sp ) {
    if ( m_bot_expanded != -1 ) m_mdown_area = m_rect_area;
    else return;
    if ( m_mdown_area == RA_Move ) m_maction = MA_Move;
    else m_maction = MA_None;
    BattleBot* bot = m_battle->GetBot( m_bot_expanded );
    ASSERT_LOGIC( bot != 0, "MapCtrl::OnLeftDown(): bot = 0" );
    RefreshRect( _GetBotRect( *bot, true ), false );
    return;
  }

  if ( !m_image ) {
    if ( m_rect_area != RA_Main ) {
      m_mdown_area = m_rect_area;
      Refresh();
      Update();
    }
    return;
  }

  if ( !m_draw_start_types ) return;

  if ( m_battle->GetStartType() != ST_Choose ) return;
  if ( !m_ro ) {
    // In edit mode
    if ( m_mover_rect >= 0 ) { // We are over an existing rect.

      m_mdown_area = m_rect_area;
      m_mdown_rect = m_mover_rect;
      m_mdown_x = event.GetX();
      m_mdown_y = event.GetY();

      if      ( m_mdown_area == RA_UpLeft ) {
        m_maction = MA_ResizeUpLeft;
      } else if ( m_mdown_area == RA_UpRight ) {
        m_maction = MA_Delete;
      } else if ( m_mdown_area == RA_DownLeft ) {
//        m_maction = MA_ResizeDownLeft;
      } else if ( m_mdown_area == RA_DownRight ) {
        m_maction = MA_ResizeDownRight;
      } else if ( m_mdown_area == RA_Main ) {
        m_maction = MA_Move;
      }

    } else if ( m_mover_rect == -1 ) { // We are over empty minimap area.

      m_maction = MA_Add; // Add new start rect.
      m_mdown_x = event.GetX();
      m_mdown_y = event.GetY();
      m_tmp_brect.ally = _GetNewRectIndex();

    } else if ( m_mover_rect == -2 ) { // We are outside the minimap.

    }
  } else {
    // Readonly.
    if ( m_mover_rect >= 0 ) {
      // Join ally rect that user clicked on
      if ( m_battle->GetMyAlly() != m_mover_rect ) {
        m_battle->SetMyAlly( m_mover_rect );
      }

    }
    m_maction = MA_None;
  }
}


void MapCtrl::OnLeftUp( wxMouseEvent& event )
{
  if ( m_battle == 0 ) return;

  if ( m_sp ) {
    if ( m_bot_expanded == -1 ) return;
    if ( m_rect_area != m_mdown_area ) return;
    BattleBot* bot = m_battle->GetBot( m_bot_expanded );
    ASSERT_LOGIC( bot != 0, "MapCtrl::OnLeftUp(): bot == 0" );

    if ( m_mdown_area == RA_UpAllyButton ) {
      bot->bs.ally = ( bot->bs.ally + 1 ) % 16;
      RefreshRect( _GetBotRect( *bot, true ), false );

    } else if ( m_mdown_area == RA_DownAllyButton ) {
      bot->bs.ally = (bot->bs.ally - 1) >= 0 ? (bot->bs.ally - 1) : 15;
      RefreshRect( _GetBotRect( *bot, true ), false );

    } else if ( m_mdown_area == RA_UpHandicapButton ) {
      if ( bot->bs.handicap == 100 ) bot->bs.handicap = 0;
      else bot->bs.handicap = ( bot->bs.handicap + 5 );
      RefreshRect( _GetBotRect( *bot, true ), false );

    } else if ( m_mdown_area == RA_DownHandicapButton ) {
      bot->bs.handicap = (bot->bs.handicap - 5) >= 0 ? (bot->bs.handicap - 5) : 100;
      RefreshRect( _GetBotRect( *bot, true ), false );

    } else if ( m_mdown_area == RA_Side ) {
      try {
        usync()->SetCurrentMod( STD_STRING(m_battle->GetModName()) );
        if ( usync()->GetSideCount() > 0 ) bot->bs.side = (bot->bs.side + 1) % usync()->GetSideCount();
        else bot->bs.side = 0;
      } catch(...) {}
      RefreshRect( _GetBotRect( *bot, true ), false );

    } else if ( m_mdown_area == RA_Close ) {
      wxRect r = _GetBotRect( *bot, true );
      m_battle->RemoveBot( m_bot_expanded );
      RefreshRect( r, false );
    }
    m_mdown_area = RA_Main;
    m_maction = MA_None;
    return;
  }

  if ( !m_image ) {
    if ( m_mdown_area == m_rect_area ) {
      if ( m_mdown_area == RA_Refresh ) {
        m_ui.ReloadUnitSync();
        UpdateMinimap();
      } else if ( m_mdown_area == RA_Download ) {
        m_ui.DownloadMap( m_battle->GetMapName() );
      }
    }
    m_mdown_area = RA_Main;
    Refresh();
    Update();
    return;
  }

  if ( !m_draw_start_types ) return;

  if ( m_battle->GetStartType() != ST_Choose ) return;

  if ( m_maction == MA_Add ) {

    m_tmp_brect.ally = -1;
    if ( ( abs( m_mdown_x - event.GetX() ) >= minboxsize ) && ( abs( m_mdown_y - event.GetY() ) >= minboxsize ) ) {
      BattleStartRect r = _GetBattleRect( m_mdown_x<event.GetX()?m_mdown_x:event.GetX(), m_mdown_y<event.GetY()?m_mdown_y:event.GetY(), m_mdown_x>event.GetX()?m_mdown_x:event.GetX(), m_mdown_y>event.GetY()?m_mdown_y:event.GetY() );
      m_battle->AddStartRect( _GetNewRectIndex(), r.left, r.top, r.right, r.bottom );
    } else {
      BattleStartRect r = _GetBattleRect( m_mdown_x, m_mdown_y, m_mdown_x + minboxsize, m_mdown_y + minboxsize );
      m_battle->AddStartRect( _GetNewRectIndex(), r.left, r.top, r.right, r.bottom );
    }
    m_battle->SendHostInfo( HI_StartRects );
    UpdateMinimap();
  } else if ( m_maction == MA_Delete ) {

    if ( (m_mdown_area == m_rect_area) && (m_mover_rect == m_mdown_rect) ) {
      m_battle->RemoveStartRect( m_mdown_rect );
      UpdateMinimap();
      m_battle->SendHostInfo( HI_StartRects );
    }

  } else if ( (m_maction == MA_ResizeDownRight)||(m_maction == MA_ResizeUpLeft) ) {
    m_battle->UpdateStartRect( m_mdown_rect );
    m_battle->SendHostInfo( HI_StartRects );
  }

  m_maction = MA_None;

}


void MapCtrl::OnMouseWheel( wxMouseEvent& event )
{
}
