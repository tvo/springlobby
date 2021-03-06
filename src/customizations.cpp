/* Copyright (C) 2007-2009 The SpringLobby Team. All rights reserved. */

#include "customizations.h"
#include "springunitsynclib.h"

#include <wx/image.h>

/** @brief GetBackground
  *
  * @todo: document this function
  */
wxBitmap Customizations::GetBackground()
{
    return m_background;
}

wxSize Customizations::GetBackgroundSize()
{
    return wxSize( m_background.GetWidth(), m_background.GetHeight() );
}

const OptionsWrapper& Customizations::GetCustomizations()
{
    return m_customs;
}

/** @brief GetAppIcon
  *
  * @todo: document this function
  */
wxIcon Customizations::GetAppIcon()
{
    return m_app_ico;
}

/** @brief GetHelpUrl
  *
  * @todo: document this function
  */
wxString Customizations::GetHelpUrl()
{
    return m_help_url;
}

/** @brief GetModname
  *
  * @todo: document this function
  */
wxString Customizations::GetModname()
{
    return m_modname;
}

/** @brief Init
  *
  * @todo: document this function
  */
bool Customizations::Init(const wxString& modname)
{
	//!TODO require blocking usync init if it's not loaded
    m_modname = modname;
    if ( !usync().ModExists( m_modname ) )
        return false;
    susynclib().SetCurrentMod( m_modname );
    bool ret = m_customs.loadOptions( OptionsWrapper::ModCustomizations, m_modname );
    if ( ret ) {
        wxString icon_img_path = m_customs.getSingleValue( _T("icon") );
        wxBitmap icon_bmp (usync().GetImage( m_modname, icon_img_path ) );
        m_app_ico.CopyFromBitmap( icon_bmp );

        wxString bg_img_path = m_customs.getSingleValue( _T("bg_image") );
        m_background = wxBitmap( usync().GetImage( m_modname, bg_img_path ) );

        m_help_url = m_customs.getSingleValue( _T("help_url") );
    }
    return ret;
}

/** @brief Customizations
  *
  * @todo: document this function
  */
 Customizations::Customizations()
{

}

/** @brief SLcustomizations
  *
  * @todo: document this function
  */
Customizations& SLcustomizations()
{
    static LineInfo<Customizations> m( AT );
    static GlobalObjectHolder<Customizations, LineInfo<Customizations> > s_customizations( m );
    return s_customizations;
}

