/*
====================
File: Terrain.cc
Author: Shane Lillie
Description: Terrain class source

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


#include <cassert>
#include <cmath>
#include <cstring>
#include <string>
#include <fstream>

#include <errno.h>

#include "SDL_opengl.h"

#include "Terrain.h"
#include "SEarth.h"
#include "Vector.h"
#include "utilities.h"


/*
 *  external globals
 *
 */


extern int errno;


/*
 *  Terrain methods
 *
 */


Terrain::Terrain(const std::string& filename, int width, int height) throw(TerrainException)
    : m_terrain(NULL), m_width(width), m_height(height)
{
    m_textures[0] = m_textures[1] = m_textures[2] = 0;
    m_surfaces[0] = m_surfaces[1] = m_surfaces[2] = -1;

    std::ifstream infile(filename.c_str());
    if(!infile)
        throw TerrainException(std::string("Could not open terrain file: ") + std::strerror(errno));

    m_terrain = new bool*[m_width];
    for(int i=0; i<m_width; ++i) {
        m_terrain[i] = new bool[m_height];
        std::memset(m_terrain[i], 0, m_height * sizeof(bool));
    }

    int x = 0;
    while(!infile.eof() && x < m_width) {
        int y = 0;
        infile >> y;

        // out texture is only this high for now
        if(y >= 512/*m_height*/)
            y = 511/*m_height-1*/;
        else if(y < 0)
            y = 0;

        skip_whitespace(infile);

        char ch = infile.get();
        if(ch == ',') {
            int count = 0;
            infile >> count;

            if(count && count > 0) {
                int total = x + count;
                if(total > m_width)
                    total = m_width;

                for(int i=y; i>=0; --i)
                    for(int j=x; j<total; ++j)
                        m_terrain[j][i] = true;
                x = total;
            }
        } else if(ch == 'r') {
            int top = 0;
            infile >> top;

            int step = 1;

            skip_whitespace(infile);

            ch = infile.get();
            if(ch == ',')
                infile >> step;
            else
                infile.putback(ch);

            const int y_count = top - y;
            if(y_count < 0 && step > 0 || y_count > 0 && step < 0)
                step *= -1;

            int total = x + (std::abs(y_count) / std::abs(step));
            for(int i=x; i<total; ++i) {
                for(int j=y; j>=0; --j)
                    m_terrain[i][j] = true;
                y += step;
            }
            x = total;
        } else {
            infile.putback(ch);

            for(int i=y; i>=0; --i)
                m_terrain[x][i] = true;
            x++;
        }
    }

    infile.clear();
    infile.close();

    create_textures();
}


Terrain::~Terrain()
{
    delete_textures();
    free_surfaces();

    if(m_terrain) {
        for(int i=0; i<m_width; ++i)
            delete[] m_terrain[i];
        delete[] m_terrain;
    }
}


bool Terrain::collision(const Vector3<float>& s0, const Vector3<float>& s1, const Vector3<float>& v, Vector3<float>* const s2, int surface) const
{
    assert(m_terrain);

    SEarth::lock_surface(surface);

    Vector3<float> pos(s0);

    bool ret = false;
    do {
        // break if we crossed the new position
        // both x and y
        if(((s0.x() <= s1.x() && pos.x() >= s1.x())  ||
            (s0.x() >= s1.x() && pos.x() <= s1.x())) &&
           ((s0.y() <= s1.y() && pos.y() >= s1.y()) ||
            (s0.y() >= s1.y() && pos.y() <= s1.y())))
            break;

        if(collision(pos, surface)) {
            // move backward to non-collision spot
            do pos -= v * .001f;
            while(collision(pos, surface));

            if(s2) *s2 = pos;

            ret = true;
            break;
        }

        // test in ms
        pos += v * .001f;
    } while(true);

    SEarth::unlock_surface(surface);

    return ret;
}


void Terrain::deform(const Vector3<float>& pos, int radius)
{
    assert(m_terrain);

    m_last_deform_pos = pos;
    m_last_deform_radius = radius + 1;  // plus one because we go from r=1 to r<=radius

    lock_surfaces();

    for(int angle=0; angle<360; ++angle) {
        // note r=1..radius (inclusive)
        for(int r=1; r<=radius; ++r) {
            int x = static_cast<int>(pos.x() + (r * std::cos(DEG_RAD(angle))));
            int y = static_cast<int>(pos.y() + (r * std::sin(DEG_RAD(angle))));

            if(x < 0)
                x = 0;
            else if(x >= m_width)
                x = m_width-1;

            if(y < 0)
                y = 0;
            else if(y >= m_height)
                y = m_height-1;

            remove_point(x, y);
        }
    }

    unlock_surfaces();

    // re-generate the textures
    generate_textures();
}


bool Terrain::slide(float elapsed_sec)
{
    assert(m_terrain);

    const int x1 = static_cast<int>(m_last_deform_pos.x() - m_last_deform_radius);
    const int x2 = static_cast<int>(m_last_deform_pos.x() + m_last_deform_radius);

    const int xs = x1 < 0 ? 0 : x1;
    const int xe = x2 >= m_width ? m_width - 1 : x2;

    const int yt = static_cast<int>(m_last_deform_pos.y() - m_last_deform_radius);
    const int ys = yt < 0 ? 0 : yt;

    lock_surfaces();

    bool ret = false;

    // only look at the points that were changed
    for(int x=xs; x<xe; ++x)
        if(slide(x, ys, elapsed_sec))
            ret = true;

    unlock_surfaces();

    // re-generate the textures
    generate_textures();

    return ret;
}


void Terrain::render()
{
    assert(m_terrain);

    if(!m_textures[0] || !m_textures[1] || !m_textures[2])
        create_textures();

    const GLboolean depth_test = glIsEnabled(GL_DEPTH_TEST);
    if(depth_test)
        glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
        glLoadIdentity();

        gluOrtho2D(0.0f, m_width, 0.0f, m_height);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
            glLoadIdentity();

            //glColor3f(1.0f, 1.0f, 1.0f);
            glNormal3f(0.0f, 0.0f, 1.0f);

            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(m_textures[0]));

            glBegin(GL_QUADS);
                glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, 0.0f);
                glTexCoord2f(1.0f, 0.0f); glVertex2f(511.0f, 0.0f);
                glTexCoord2f(1.0f, 1.0f); glVertex2f(511.0f, 511.0f);
                glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, 511.0f);
            glEnd();

            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(m_textures[1]));

            glBegin(GL_QUADS);
                glTexCoord2f(0.0f, 0.0f); glVertex2f(511.0f, 0.0f);
                glTexCoord2f(1.0f, 0.0f); glVertex2f(767.0f, 0.0f);
                glTexCoord2f(1.0f, 1.0f); glVertex2f(767.0f, 511.0f);
                glTexCoord2f(0.0f, 1.0f); glVertex2f(511.0f, 511.0f);
            glEnd();

            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(m_textures[2]));

            glBegin(GL_QUADS);
                glTexCoord2f(0.0f, 0.0f); glVertex2f(767.0f, 0.0f);
                glTexCoord2f(1.0f, 0.0f); glVertex2f(799.0f, 0.0f);
                glTexCoord2f(1.0f, 1.0f); glVertex2f(799.0f, 511.0f);
                glTexCoord2f(0.0f, 1.0f); glVertex2f(767.0f, 511.0f);
            glEnd();

        glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    if(depth_test)
        glEnable(GL_DEPTH_TEST);
}


bool Terrain::collision(const Vector3<float>& s0, const Vector3<float>& s1, const Vector3<float>& v, Vector3<float>* const s2, int width, int height) const
{
    assert(m_terrain);

    Vector3<float> pos(s0);
    do {
        // break if we crossed the new position
        // both x and y
        if(((s0.x() <= s1.x() && pos.x() >= s1.x())  ||
            (s0.x() >= s1.x() && pos.x() <= s1.x())) &&
           ((s0.y() <= s1.y() && pos.y() >= s1.y()) ||
            (s0.y() >= s1.y() && pos.y() <= s1.y())))
            break;

        if(collision(pos, width, height)) {
            // move backward to non-collision spot
            do pos -= v * .001f;
            while(collision(pos, width, height));

            if(s2) *s2 = pos;
            return true;
        }

        // test in ms
        pos += v * .001f;
    } while(true);
    return false;
}


void Terrain::generate_textures()
{
    if(m_surfaces[0] < 0 || m_surfaces[1] < 0 || m_surfaces[2] < 0)
        create_textures();

    if(m_textures[0] || m_textures[1] || m_textures[2])
        delete_textures();

    // generate 3 textures
    glGenTextures(3, static_cast<GLuint*>(m_textures));

    /* create the textures */

    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(m_textures[0]));

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, SEarth::surface_pixels(m_surfaces[0]));

    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(m_textures[1]));

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, SEarth::surface_pixels(m_surfaces[1]));

    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(m_textures[2]));

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, SEarth::surface_pixels(m_surfaces[2]));
}


int Terrain::would_fall(int x, int y, int surface)
{
    assert(m_terrain);
    assert(x >= 0 && x < m_width);
    assert(y >= 0 && y < m_height);

    // we're at the bottom dude
    if(y <= 0)
        return 0;

    const Uint32 black = SEarth::map_rgba(surface, 0, 0, 0, 0);
    const int xe = x + SEarth::surface_width(surface) >= m_width ? m_width : x + SEarth::surface_width(surface);

    int left = 0, t=0;
    for(int i=x; i<xe; ++i) {
        // figure out where bottom pixel of surface is
        t=-1;
        while(t < SEarth::surface_height(surface)) {
            if(SEarth::pixel(surface, i-x, ++t) == black)
                break;
        }

        // look one below that pixel
        if(m_terrain[i][y + t - 1])
            break;
        else
            ++left;
    }

    int right = 0;
    for(int i=xe-1; i>=x; --i) {
        // figure out where bottom pixel of surface is
        t=-1;
        while(t < SEarth::surface_height(surface)) {
            if(SEarth::pixel(surface, i - x, ++t) == black)
                break;
        }

        if(m_terrain[i][y + t - 1])
            break;
        else
            ++right;
    }

    const int half_width = SEarth::surface_width(surface) / 2;

    if(left >= SEarth::surface_width(surface) || right >= SEarth::surface_width(surface))
        // gravity should drop us
        return 0;
    else if(left > right)
        return left > half_width ? -(SEarth::surface_width(surface) - left) : 0;
    return  right > half_width ? SEarth::surface_width(surface) - right : 0;
}


void Terrain::create_textures()
{
    if(m_surfaces[0] >= 0 || m_surfaces[1] >= 0 || m_surfaces[2] >= 0)
        free_surfaces();

    if(m_textures[0] || m_textures[1] || m_textures[2])
        delete_textures();

    m_surfaces[0] = SEarth::create_surface(512, 512, 32, "terrain1");
    if(m_surfaces[0] < 0) return;

    m_surfaces[1]= SEarth::create_surface(256, 512, 32, "terrain2");
    if(m_surfaces[1] < 0) {
        free_surfaces();
        return;
    }

    m_surfaces[2] = SEarth::create_surface(32, 512, 32, "terrain3");
    if(m_surfaces[2] < 0) {
        free_surfaces();
        return;
    }

    lock_surfaces();

    // copy over the pixels
    for(int x=0; x<m_width; ++x) {
        int color = 0;

        for(int y=511; y>=0; --y) {
            int xf = 0;
            int current = m_surfaces[0];

            // choose the appropriate texture to write to
            if(x >= 768) {
                xf = 768;
                current = m_surfaces[2];
            } else if(x >= 512) {
                xf = 512;
                current = m_surfaces[1];
            }


            if(m_terrain[x][y]) {
                // go brown as we get deeper
                SEarth::pixel(current, x - xf, y, SEarth::map_rgba(current, color, 192 - color, 6, 255));

                if(color < 128)
                    color += 1;
            } else
                // make anything that's not terrain transparent
                SEarth::pixel(current, x - xf, y, SEarth::map_rgba(current, 0, 0, 0, 0));
        }
    }

    unlock_surfaces();

    generate_textures();
}


void Terrain::delete_textures()
{
    if(m_textures[0])
        glDeleteTextures(1, static_cast<GLuint*>(&m_textures[0]));
    m_textures[0] = 0;

    if(m_textures[1])
        glDeleteTextures(1, static_cast<GLuint*>(&m_textures[1]));
    m_textures[1] = 0;

    if(m_textures[2])
        glDeleteTextures(1, static_cast<GLuint*>(&m_textures[2]));
    m_textures[2] = 0;
}


void Terrain::lock_surfaces()
{
    assert(m_surfaces[0] >= 0 && m_surfaces[1] >= 0 && m_surfaces[2] >= 0);

    SEarth::lock_surface(m_surfaces[0]);
    SEarth::lock_surface(m_surfaces[1]);
    SEarth::lock_surface(m_surfaces[2]);
}


void Terrain::unlock_surfaces()
{
    assert(m_surfaces[0] >= 0 && m_surfaces[1] >= 0 && m_surfaces[2] >= 0);

    SEarth::unlock_surface(m_surfaces[0]);
    SEarth::unlock_surface(m_surfaces[1]);
    SEarth::unlock_surface(m_surfaces[2]);
}


void Terrain::free_surfaces()
{
    if(m_surfaces[0] >= 0)
        SEarth::unload_surface(m_surfaces[0]);
    m_surfaces[0] = -1;

    if(m_surfaces[1] >= 0)
        SEarth::unload_surface(m_surfaces[1]);
    m_surfaces[1] = -1;

    if(m_surfaces[2] >= 0)
        SEarth::unload_surface(m_surfaces[2]);
    m_surfaces[2] = -1;
}


bool Terrain::slide(int column, int start, float elapsed_sec)
{
    assert(m_terrain);
    assert(column >= 0 && column < m_width);
    assert(start >= 0 && start < m_height);

    /* 190 is gravity */
    const int amt = static_cast<int>(std::ceil(190.0f * elapsed_sec));

    bool ret = false;
    for(int y=start; y<m_height; ++y) {
        if(!m_terrain[column][y])
            continue;

        const int yt = y-amt;
        const int ye = yt < 0 ? 0 : yt;

        // find the first collision from the top down
        for(int i=y-1; i>=ye; --i) {
            if(m_terrain[column][i]) {
                // if we hit a collision right off,
                // we're done with this point
                if(i == y-1)
                    break;

                swap_points(column, y, column, i+1);
                ret = true;
                break;
            } else if(i == ye) {
                // we didn't collide, so go all the way down
                swap_points(column, y, column, ye);
                ret = true;
            }
        }
    }
    return ret;
}


bool Terrain::collision(const Vector3<float>& pos, int surface) const
{
    assert(m_terrain);

    if((pos.x() + SEarth::surface_width(surface)) >= m_width || pos.x() < 0.0f || pos.y() < 0.0f)
        return true;

    const int yt = static_cast<int>(pos.y()) + SEarth::surface_height(surface) - 1;

    const int tys = yt >= m_height ? m_height-1 : yt;
    const int ye = pos.y() < 0.0f ? 0 : static_cast<int>(pos.y());

    const int sys = yt >= m_height ? yt - m_height - 1 : SEarth::surface_height(surface);

    const int xt = static_cast<int>(pos.x()) + SEarth::surface_width(surface) - 1;

    const int xs = pos.x() < 0.0f ? 0 : static_cast<int>(pos.x());
    const int xe = xt >= m_width ? m_width-1 : xt;

    for(int ty=tys, sy=sys; ty >= ye; --ty, --sy) {
        for(int tx=xs, sx=0; tx<xe; ++tx, ++sx) {
            // got pixel on the ground
            if(m_terrain[tx][ty]) {
                const Uint32 black = SEarth::map_rgba(surface, 0, 0, 0, 0);
                const Uint32 pixel = SEarth::pixel(surface, sx, sy);

                // got a pixel on the surface
                if(pixel != black)
                    return true;
            }
        }
    }
    return false;
}


bool Terrain::collision(const Vector3<float>& pos, int width, int height) const
{
    assert(m_terrain);

    if((pos.x() + width) >= m_width || pos.x() < 0.0f || pos.y() < 0.0f)
        return true;

    const int yt = static_cast<int>(pos.y()) + height - 1;

    const int ys = yt > m_height ? m_height : yt;
    const int ye = pos.y() < 0.0f ? 0 : static_cast<int>(pos.y());

    const int xt = static_cast<int>(pos.x()) + width - 1;

    const int xs = pos.x() < 0.0f ? 0 : static_cast<int>(pos.x());
    const int xe = xt > m_width ? m_width : xt;

    for(int y=ys; y >= ye; --y)
        for(int x=xs; x<xe; ++x)
            if(m_terrain[x][y])
                return true;
    return false;
}


void Terrain::remove_point(int x, int y)
{
    assert(x >= 0 && x < m_width);
    assert(y >= 0 && y < m_height);

    m_terrain[x][y] = false;

    if(x >= 768)
        SEarth::pixel(m_surfaces[2], x - 768, y, SEarth::map_rgba(m_surfaces[2], 0, 0, 0, 0));
    else if(x >= 512)
        SEarth::pixel(m_surfaces[1], x - 512, y, SEarth::map_rgba(m_surfaces[1], 0, 0, 0, 0));
    else
        SEarth::pixel(m_surfaces[0], x, y, SEarth::map_rgba(m_surfaces[0], 0, 0, 0, 0));
}


void Terrain::swap_points(int x1, int y1, int x2, int y2)
{
    assert(x1 >= 0 && x1 < m_width);
    assert(y1 >= 0 && y1 < m_height);
    assert(x2 >= 0 && x2 < m_width);
    assert(y2 >= 0 && y2 < m_height);

    // swap in the terrain
    const bool temp = m_terrain[x1][y1];
    m_terrain[x1][y1] = m_terrain[x2][y2];
    m_terrain[x2][y2] = temp;

    /* swap on the surfaces */

    int s1 = m_surfaces[0];
    int xoff1 = 0;
    if(x1 >= 768) {
        s1 = m_surfaces[2];
        xoff1 = 768;
    } else if(x1 >= 512) {
        s1 = m_surfaces[1];
        xoff1 = 512;
    }

    int s2 = m_surfaces[0];
    int xoff2 = 0;
    if(x2 >= 768) {
        s2 = m_surfaces[2];
        xoff2 = 768;
    } else if(x2 >= 512) {
        s2 = m_surfaces[1];
        xoff2 = 512;
    }

    Uint32 pixel = SEarth::pixel(s1, x1 - xoff1, y1);
    SEarth::pixel(s1, x1 - xoff1, y1, SEarth::pixel(s2, x2 -xoff2, y2));
    SEarth::pixel(s2, x2 - xoff2, y2, pixel);
}
