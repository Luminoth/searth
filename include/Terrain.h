/*
====================
File: Terrain.h
Author: Shane Lillie
Description: Terrain class header

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

#if !defined TERRAIN_H
#define TERRAIN_H


#include <stdexcept>

#include "World.h"
#include "Vector.h"


class Terrain : public World
{
public:
    class TerrainException : public std::exception
    {
    public:
        TerrainException(const std::string& what) throw() : _what(what) { }
        virtual ~TerrainException() throw() { }
        virtual const char* what() const throw() { return _what.c_str(); }
    private:
        std::string _what;
    };

public:
    Terrain(const std::string& filename, int width, int height) throw(TerrainException);
    virtual ~Terrain();

/* FIXME:
instead of return bool, return type of collision (ground, floor, wall)
ceiling collision isn't collision
*/

public:
    // tests for a collision on a per-pixel level
    bool collision(const Vector3<float>& s0, const Vector3<float>& s1, const Vector3<float>& v, Vector3<float>* const s2, int surface) const;

    // deforms the terrain in a circle
    // r=1..radius (inclusive)
    void deform(const Vector3<float>& pos, int radius);

    // slides the dirt down
    // retrurns true if dirt actually fell
    bool slide(float elapsed_sec);

    // generates the terrain textures
    // doesn't modify or re-create the surfaces
    void generate_textures();

    // returns the amount that a tank would
    // fall based on how much ground
    // is underneath it
    int would_fall(int x, int y, int surface);

public:
    virtual void render();
    virtual bool collision(const Vector3<float>& s0, const Vector3<float>& s1, const Vector3<float>& v, Vector3<float>* const s2, int width, int height) const;

private:
    void create_textures();
    void delete_textures();

    void lock_surfaces();
    void unlock_surfaces();
    void free_surfaces();

    bool slide(int column, int start, float elapsed_sec);
    bool collision(const Vector3<float>& pos, int surface) const;
    bool collision(const Vector3<float>& pos, int width, int height) const;

    void remove_point(int x, int y);
    void swap_points(int x1, int y1, int x2, int y2);

public:
    bool** m_terrain;
    int m_width, m_height;

    int m_surfaces[3];
    unsigned int m_textures[3];

    Vector3<float> m_last_deform_pos;
    int m_last_deform_radius;
};


#endif
