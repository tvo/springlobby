#ifndef SL_PLAYBACKLIST_H_INCLUDED
#define SL_PLAYBACKLIST_H_INCLUDED

#include <map>
#include <wx/event.h>

class wxArrayString;

template <class PlaybackImp>
class PlaybackList : public wxEvtHandler
{
  public:
    typedef PlaybackImp
        PlaybackType;

    typedef unsigned int playback_id_t;

    //! @brief mapping from playback id number to playback object
    typedef std::map<playback_id_t, PlaybackType> playback_map_t;
    //! @brief iterator for playback map
    typedef typename playback_map_t::iterator playback_iter_t;
    //! @brief const iterator for playback map
    typedef typename playback_map_t::const_iterator playback_const_iter_t;

    virtual void LoadPlaybacks( const wxArrayString& filenames ) = 0;
    //!loads replays between two indices
//    virtual void LoadPlaybacks( const unsigned int from, const unsigned int to) = 0;

	PlaybackType& AddPlayback( const size_t index );
    void AddPlayback( PlaybackType* replay );
    void RemovePlayback( playback_id_t const& id );

    PlaybackType &GetPlaybackById( playback_id_t const& id );

    ///Playback& GetPlayback( int const index ) ;

    bool PlaybackExists( playback_id_t const& id );
    bool DeletePlayback( playback_id_t const& id );
    typename playback_map_t::size_type GetNumPlaybacks();

    void RemoveAll();

    const playback_map_t& GetPlaybacksMap() const;


  protected:
    PlaybackList() {};

    playback_map_t m_replays;

    unsigned long m_fails;

};

template <class ListImp>
ListImp& playbacklist();

#include "playbacklist.cpp"

#endif // SL_PLAYBACKLIST_H_INCLUDED
