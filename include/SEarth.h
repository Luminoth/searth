/*
====================
File: SEarth.h
Author: Shane Lillie
Description: Scorched Earth engine header

Copyright 2003 Energon Software

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software Foundation,
Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
====================
*/


#if !defined SEARTH_H
#define SEARTH_H


#include "SDL_opengl.h"

#include "Engine.h"


class Terrain;


/*
 *  SEarth class
 *
 */


class SEarth : public Engine
{
private:
/* TODO: put this shit in the Engine class... */
    struct State
    {
        // these are for initial window setup
        // use the Engine class functions for up-to-date settings
        int window_depth;
        bool fullscreen;

        bool music;
        bool sounds;

        bool paused;
        bool fps;

        State()
            : window_depth(16), fullscreen(false),
                music(true), sounds(true),
                paused(false), fps(/*false*/true)
        {
        }
    };

public:
    SEarth() throw(Engine::EngineException);
    virtual ~SEarth();

public:
    void set_depth(int depth)
    {
        m_state.window_depth = depth;
    }

    void set_fullscreen(bool fullscreen)
    {
        m_state.fullscreen = fullscreen;
    }

    void set_music(bool music)
    {
        m_state.music = music;
    }

    void set_sounds(bool sounds)
    {
        m_state.sounds = sounds;
    }

private:
    bool create_window(const std::string& title);
    bool setup_extensions() const;
    void render_hud();
    void screenshot() const;

public:
    virtual bool main();

private:
    virtual void event_handler();
    virtual bool initialize_opengl() const;
    virtual void on_key_down(SDLKey key);
    virtual void on_key_up(SDLKey key);
    virtual void on_sigsegv();
    virtual void on_sigint();
    virtual void on_signal(int signum);

private:
    State m_state;
    Terrain* m_terrain;
};


#endif
