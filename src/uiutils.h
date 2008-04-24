#ifndef SPRINGLOBBY_HEADERGUARD_UIUTILS_H
#define SPRINGLOBBY_HEADERGUARD_UIUTILS_H

class wxString;
class wxColour;
class wxImage;
class wxBitmap;

#define bool2yn(b) ((b)?_("Yes"):_("No"))


const int colour_values[][3] = { {240,210,0}, {128, 128, 128}, {0, 0, 128}, {0, 0, 255},
      {0, 128, 0}, {0, 255, 0}, {0, 128, 128}, {0, 255, 255}, {128, 0, 0}, {255, 0, 0},
      {128, 0, 128}, {255, 0, 255}, {128, 128, 0}, {255, 255, 0}, {192, 192, 192}, {0, 220, 250}
};

wxString RefineMapname( const wxString& mapname );
wxString RefineModname( const wxString& modname );
wxString RTFtoText( const wxString& rtfinput );
bool AreColoursSimilar( const wxColour& col1, const wxColour& col2, int mindiff = 10 );

void ColourDelta( int& r, int& g, int& b, const int& delta );
wxColour ColourDelta( const wxColour& colour, const int& delta );

wxString GetColorString( const wxColour& color );
wxColour GetColorFromStrng( const wxString color );

void BlendImage(wxImage& source, wxImage& dest,int img_dim);
wxBitmap* charArr2wxBitmap(const unsigned char * arg, int size);
wxBitmap* charArr2wxBitmapAddText(const unsigned char * arg, int size, const unsigned char * text, int text_size, unsigned int img_dim);

#include <wx/event.h>
class wxCommandEvent;

BEGIN_DECLARE_EVENT_TYPES()
    DECLARE_EVENT_TYPE(httpDownloadEvt, 42)
END_DECLARE_EVENT_TYPES()


class SL_GlobalEvtHandler : public wxEvtHandler
{

    SL_GlobalEvtHandler();

    DECLARE_EVENT_TABLE()

    public:
    static SL_GlobalEvtHandler& GetSL_GlobalEvtHandler();
     void OnHttpDownLoadComplete(wxCommandEvent& event);

};

#endif
// SPRINGLOBBY_HEADERGUARD_UIUTILS_H
