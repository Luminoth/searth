/*
====================
File: SEarth.cc
Author: Shane Lillie
Description: Scorched Earth engine source

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


#include <cstdio>
#include <iostream>

#include <sys/types.h>
#include <sys/stat.h>

#if !defined WIN32
    #include <unistd.h>
#endif

#include "SDL_opengl.h"

#include "SEarth.h"
#include "DirtParticle.h"
#include "LogicalFont.h"
#include "main.h"
#include "Callstack.h"
#include "Terrain.h"
#include "utilities.h"


/*
NOTE to self: SDL images are upside down, so GL tex coords are backwards in the t direction.
*/


/*
 *  constants
 *
 */


const int DEFAULT_WIDTH = 800;
const int DEFAULT_HEIGHT = 600;

#define TERRAINDIR "/terrain"

#if defined WIN32
    #define GL_BGR GL_BGR_EXT
#endif


/*
 *  globals
 *
 */


Vector3<float> g_tank_pos(70.0f, 500.0f, 0.0f);
Vector3<float> g_tank_vel;

Vector3<float> g_projectile_pos(100.0f, 217.0f, 0.0f);
Vector3<float> g_projectile_vel(200.0f, 200.0f, 0.0f);

Vector3<float> g_smoke_pos;
const Vector3<float> g_smoke_vel(0.0f, 25.0f, 0.0f);

Vector3<float> g_collision_pos;

DirtParticleSystem* g_dirt = NULL;


/*
 *  SEarth methods
 *
 */


SEarth::SEarth() throw(Engine::EngineException)
    : Engine(InitVideo | InitAudio | InitJoystick, data_directory() + NOIMAGE),
        m_terrain(NULL)
{
    ENTER_FUNCTION(SEarth::SEarth);

    redirect_log("searth.log", false);

    connect_system_signals(CatchSigSegv | CatchSigInt);

#if !defined NDEBUG
    log("Using debug build\n");
#endif
}


SEarth::~SEarth()
{
    if(m_terrain)
        delete m_terrain;
}


bool SEarth::create_window(const std::string& title)
{
    ENTER_FUNCTION(SEarth::create_window);

    if(m_state.fullscreen) {
        log("Trying fullscreen mode...\n");

        if(!create_gl_window(DEFAULT_WIDTH, DEFAULT_HEIGHT, m_state.window_depth, true, title, this)) {
            log("Fullscreen mode failed, trying windowed...\n");
            if(!create_gl_window(DEFAULT_WIDTH, DEFAULT_HEIGHT, m_state.window_depth, false, title, this)) {
                log("Windowed mode failed... giving up\n");
                return false;
            }
        }
    } else {
        log("Trying windowed mode...\n");

        if(!create_gl_window(DEFAULT_WIDTH, DEFAULT_HEIGHT, m_state.window_depth, false, title, this)) {
            log("Windowed mode failed, trying fullscreen...\n");
            if(!create_gl_window(DEFAULT_WIDTH, DEFAULT_HEIGHT, m_state.window_depth, true, title, this)) {
                log("Fullscreen mode failed... giving up\n");
                return false;
            }
        }
    }
    return true;
}


bool SEarth::setup_extensions() const
{
    if(!check_gl_extension("GL_EXT_bgra")) {
        error("BGRA not supported\n");
        return false;
    }
    log("BGRA supported\n");

    return true;
}


void SEarth::render_hud()
{
    ENTER_FUNCTION(SEarth::render_hud);

    // load the font
    static LogicalFont hud_font(font_directory() + "/cour.ttf", 14, TTF_STYLE_BOLD);
    if(!hud_font.valid()) {
        error("Could not load HUD font\n");
        do_quit();
        return;
    }

    // go 2d
    const GLboolean depth_test = glIsEnabled(GL_DEPTH_TEST);
    if(depth_test)
        glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
        glLoadIdentity();

        gluOrtho2D(0.0f, window_width(), 0.0f, window_height());

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
            glLoadIdentity();

            int x=0, y=window_height() - hud_font.height(), w=0, h=0;
            SDL_Surface* surface = NULL;
            GLuint texture=0;
            char text[256];

            SDL_Color color = { 0xff, 0xff, 0xff, 0x00 };

            if(m_state.fps) {
                snprintf(text, 32, "FPS: %d", fps());
                surface = hud_font.render_blended(text, color, w, h);

                glGenTextures(1, &texture);
                glBindTexture(GL_TEXTURE_2D, texture);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);

                y = window_height() - h;
                float s = static_cast<float>(w) / surface->w;
                float t = static_cast<float>(h) / surface->h;

                glBegin(GL_QUADS);
                    glNormal3f(0.0f, 0.0f, 1.0f);
                    glTexCoord2f(0.0f, t); glVertex2f(x, y);
                    glTexCoord2f(s, t); glVertex2f(x + w, y);
                    glTexCoord2f(s, 0.0f); glVertex2f(x + w, y + h);
                    glTexCoord2f(0.0f, 0.0f); glVertex2f(x, y + h);
                glEnd();

                glDeleteTextures(1, &texture);
                SDL_FreeSurface(surface);

                y -= hud_font.height();
            }

            static GLuint paused_texture = 0;
            static int pw=0, ph=0;
            static float ps=0, pt=0;
            if(m_state.paused) {
                if(!paused_texture) {
                    std::strncpy(text, "Paused", 32);
                    surface = hud_font.render_blended(text, color, w, h);

                    glGenTextures(1, &paused_texture);
                    glBindTexture(GL_TEXTURE_2D, paused_texture);

                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);

                    pw = w;
                    ph = h;

                    ps = static_cast<float>(pw) / surface->w;
                    pt = static_cast<float>(ph) / surface->h;

                    SDL_FreeSurface(surface);
                }
                glBindTexture(GL_TEXTURE_2D, paused_texture);

                glBegin(GL_QUADS);
                    glNormal3f(0.0f, 0.0f, 1.0f);
                    glTexCoord2f(0.0f, pt); glVertex2f(x, y);
                    glTexCoord2f(ps, pt); glVertex2f(x + pw, y);
                    glTexCoord2f(ps, 0.0f); glVertex2f(x + pw, y + ph);
                    glTexCoord2f(0.0f, 0.0f); glVertex2f(x, y + ph);
                glEnd();

                y -= hud_font.height();
            }

            static GLuint demo_texture = 0;
            static int dw=0, dh=0;
            static float ds=0.0f, dt=0.0f;
            if(!demo_texture) {
                std::strncpy(text, "SEarth Tech Demo (c) 2003 Energon Software", 256);
                surface = hud_font.render_blended(text, color, w, h);

                glGenTextures(1, &demo_texture);
                glBindTexture(GL_TEXTURE_2D, demo_texture);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);

                dw = w;
                dh = h;

                ds = static_cast<float>(dw) / surface->w;
                dt = static_cast<float>(dh) / surface->h;

                SDL_FreeSurface(surface);
            }
            glBindTexture(GL_TEXTURE_2D, demo_texture);

            x = (window_width() / 2) - (dw / 2);
            y = 5;

            glBegin(GL_QUADS);
                glNormal3f(0.0f, 0.0f, 1.0f);
                glTexCoord2f(0.0f, dt); glVertex2f(x, y);
                glTexCoord2f(ds, dt); glVertex2f(x + dw, y);
                glTexCoord2f(ds, 0.0f); glVertex2f(x + dw, y + dh);
                glTexCoord2f(0.0f, 0.0f); glVertex2f(x, y + dh);
            glEnd();

        glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    if(depth_test)
        glEnable(GL_DEPTH_TEST);

/* DELETE TEXTURES */
}


void SEarth::screenshot() const
{
    ENTER_FUNCTION(SEarth::screenshot);

/* FIXME: this is an accident waiting to happen */
    static const char name[] = PROGRAM_NAME;

    create_path(screenshot_directory(), 0755);

    int cur_shot = 0;
    char path[1024];
    std::snprintf(path, 1024, (screenshot_directory() + "/%s%d.bmp").c_str(), name, cur_shot);

    struct stat buf;
    while(!stat(path, &buf))
        std::snprintf(path, 1024, (screenshot_directory() + "/%s%d.bmp").c_str(), name, cur_shot++);

    if(Engine::screenshot(path))
        log("Wrote screenshot: %s\n", path);
    else
        log("Could not write screenshot: %s\n", path);
}


void render_background(int surface, GLfloat window_width, GLfloat window_height)
{
    if(surface < 0)
        return;

    static GLuint texture = 0;
    if(!texture) {
        glGenTextures(1, &texture);

        glBindTexture(GL_TEXTURE_2D, texture);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        if(SEarth::surface_Bpp(surface) != 3) {
            SEarth::error("Background must be 24 bits\n");
            return;
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, SEarth::surface_width(surface), SEarth::surface_height(surface),
            0, GL_BGR, GL_UNSIGNED_BYTE, SEarth::surface_pixels(surface));
    }

    const GLboolean depth_test = glIsEnabled(GL_DEPTH_TEST);
    if(depth_test)
        glDisable(GL_DEPTH_TEST);

    const GLboolean blend = glIsEnabled(GL_BLEND);
    if(blend)
        glDisable(GL_BLEND);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
        glLoadIdentity();

        gluOrtho2D(0.0f, window_width, 0.0f, window_height);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
            glLoadIdentity();

            glBindTexture(GL_TEXTURE_2D, texture);

            const GLfloat s = static_cast<GLfloat>(window_width) / static_cast<GLfloat>(SEarth::surface_width(surface));
            const GLfloat t = static_cast<GLfloat>(window_height) / static_cast<GLfloat>(SEarth::surface_height(surface));

            //glColor3f(1.0f, 1.0f, 1.0f);
            glBegin(GL_QUADS);
                glNormal3f(0.0f, 0.0f, 1.0f);
                glTexCoord2f(0.0f, t); glVertex2f(0.0f, 0.0f);
                glTexCoord2f(s, t); glVertex2f(window_width - 1.0f, 0.0f);
                glTexCoord2f(s, 0.0f); glVertex2f(window_width - 1.0f, window_height - 1.0f);
                glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, window_height - 1.0f);
            glEnd();
        glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    if(blend)
        glEnable(GL_BLEND);

    if(depth_test)
        glEnable(GL_DEPTH_TEST);
}


void render_tank(int surface, GLfloat window_width, GLfloat window_height)
{
    if(surface < 0)
        return;

    static GLuint texture = 0;
    if(!texture) {
        glGenTextures(1, &texture);

        glBindTexture(GL_TEXTURE_2D, texture);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        if(SEarth::surface_Bpp(surface) != 4) {
            SEarth::error("Tank must be 32 bits\n");
            return;
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SEarth::surface_width(surface), SEarth::surface_height(surface),
            0, GL_BGRA, GL_UNSIGNED_BYTE, SEarth::surface_pixels(surface));
    }

    const GLboolean depth_test = glIsEnabled(GL_DEPTH_TEST);
    if(depth_test)
        glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
        glLoadIdentity();

        gluOrtho2D(0.0f, window_width, 0.0f, window_height);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
            glLoadIdentity();

            glTranslatef(g_tank_pos.x(), g_tank_pos.y(), g_tank_pos.z());
            // rotate to match ground...

            glBindTexture(GL_TEXTURE_2D, texture);

            //glColor3f(1.0f, 1.0f, 1.0f);
            glBegin(GL_QUADS);
                glNormal3f(0.0f, 0.0f, 1.0f);
                glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, 0.0f);
                glTexCoord2f(1.0f, 1.0f); glVertex2f(SEarth::surface_width(surface) - 1.0f, 0.0f);
                glTexCoord2f(1.0f, 0.0f); glVertex2f(SEarth::surface_width(surface) - 1.0f, SEarth::surface_height(surface) - 1.0f);
                glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, SEarth::surface_height(surface) - 1.0f);
            glEnd();
        glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    if(depth_test)
        glEnable(GL_DEPTH_TEST);
}


void render_projectile(int flare, int projectile, GLfloat window_width, GLfloat window_height)
{
    if(flare < 0 || projectile < 0)
        return;

    static GLuint flare_texture = 0;
    if(!flare_texture) {
        glGenTextures(1, &flare_texture);

        glBindTexture(GL_TEXTURE_2D, flare_texture);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        if(SEarth::surface_Bpp(flare) != 4) {
            SEarth::error("Flare must be 32 bits\n");
            return;
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SEarth::surface_width(flare), SEarth::surface_height(flare),
            0, GL_BGRA, GL_UNSIGNED_BYTE, SEarth::surface_pixels(flare));
    }

    static GLuint projectile_texture = 0;
    if(!projectile_texture) {
        glGenTextures(1, &projectile_texture);

        glBindTexture(GL_TEXTURE_2D, projectile_texture);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        if(SEarth::surface_Bpp(projectile) != 4) {
            SEarth::error("Projectile must be 32 bits\n");
            return;
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SEarth::surface_width(projectile), SEarth::surface_height(projectile),
            0, GL_BGRA, GL_UNSIGNED_BYTE, SEarth::surface_pixels(projectile));
    }

    const GLboolean depth_test = glIsEnabled(GL_DEPTH_TEST);
    if(depth_test)
        glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
        glLoadIdentity();

        gluOrtho2D(0.0f, window_width, 0.0f, window_height);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
            glLoadIdentity();

            const float hw = SEarth::surface_width(projectile) / 2.0f;
            const float hh = SEarth::surface_height(projectile) / 2.0f;

            glTranslatef(g_projectile_pos.x() + hw, g_projectile_pos.y() + hh, g_tank_pos.z());

            Vector2<float> vec(g_projectile_vel.x(), g_projectile_vel.y());
            glRotatef(RAD_DEG(vec.angle()), 0.0f, 0.0f, 1.0f);

            glBindTexture(GL_TEXTURE_2D, flare_texture);

            glColor3f(0.6f, 0.3f, 0.0f);
            glBegin(GL_QUADS);
                glNormal3f(0.0f, 0.0f, 1.0f);
                glTexCoord2f(0.0f, 1.0f); glVertex2f(-SEarth::surface_width(projectile) - hw, -SEarth::surface_height(projectile));
                glTexCoord2f(1.0f, 1.0f); glVertex2f(-SEarth::surface_width(projectile) - hw + SEarth::surface_width(flare) - 1.0f, -SEarth::surface_height(projectile));
                glTexCoord2f(1.0f, 0.0f); glVertex2f(-SEarth::surface_width(projectile) - hw + SEarth::surface_width(flare) - 1.0f, -SEarth::surface_height(projectile) + SEarth::surface_height(flare) - 1.0f);
                glTexCoord2f(0.0f, 0.0f); glVertex2f(-SEarth::surface_width(projectile) - hw, -SEarth::surface_height(projectile) + SEarth::surface_height(flare) - 1.0f);
            glEnd();

            glBindTexture(GL_TEXTURE_2D, projectile_texture);

            glColor3f(1.0f, 1.0f, 1.0f);
            glBegin(GL_QUADS);
                glNormal3f(0.0f, 0.0f, 1.0f);
                glTexCoord2f(0.0f, 1.0f); glVertex2f(-hw, -hh);
                glTexCoord2f(1.0f, 1.0f); glVertex2f(-hw + SEarth::surface_width(projectile) - 1.0f, -hh);
                glTexCoord2f(1.0f, 0.0f); glVertex2f(-hw + SEarth::surface_width(projectile) - 1.0f, -hh + SEarth::surface_height(projectile) - 1.0f);
                glTexCoord2f(0.0f, 0.0f); glVertex2f(-hw, -hh + SEarth::surface_height(projectile) - 1.0f);
            glEnd();
        glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    if(depth_test)
        glEnable(GL_DEPTH_TEST);
}


void render_smoke(int surface, GLfloat window_width, GLfloat window_height)
{
    if(surface < 0)
        return;

    static GLuint texture = 0;
    if(!texture) {
        glGenTextures(1, &texture);

        glBindTexture(GL_TEXTURE_2D, texture);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        if(SEarth::surface_Bpp(surface) != 4) {
            SEarth::error("Smoke must be 32 bits\n");
            return;
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SEarth::surface_width(surface), SEarth::surface_height(surface),
            0, GL_BGRA, GL_UNSIGNED_BYTE, SEarth::surface_pixels(surface));
    }

    const GLboolean depth_test = glIsEnabled(GL_DEPTH_TEST);
    if(depth_test)
        glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
        glLoadIdentity();

        gluOrtho2D(0.0f, window_width, 0.0f, window_height);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
            glLoadIdentity();

            glTranslatef(g_smoke_pos.x() - (SEarth::surface_width(surface) / 2), g_smoke_pos.y() - (SEarth::surface_height(surface) / 2), g_smoke_pos.z());

            glBindTexture(GL_TEXTURE_2D, texture);

            //glColor3f(1.0f, 1.0f, 1.0f);
            glBegin(GL_QUADS);
                glNormal3f(0.0f, 0.0f, 1.0f);
                glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, 0.0f);
                glTexCoord2f(1.0f, 1.0f); glVertex2f(SEarth::surface_width(surface) - 1.0f, 0.0f);
                glTexCoord2f(1.0f, 0.0f); glVertex2f(SEarth::surface_width(surface) - 1.0f, SEarth::surface_height(surface) - 1.0f);
                glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, SEarth::surface_height(surface) - 1.0f);
            glEnd();
        glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    if(depth_test)
        glEnable(GL_DEPTH_TEST);
}



bool SEarth::main()
{
    ENTER_FUNCTION(SEarth::main);

    if(!create_window(WINDOW_TITLE))
        return false;

    print();

    while(!should_quit())
        event_loop();
    return true;
}


void SEarth::event_handler()
{
    if(!m_terrain) {
        try {
            m_terrain = new Terrain(data_directory() + TERRAINDIR + "/test.set", window_width(), window_height());
        } catch(Terrain::TerrainException& e) {
            error(e.what());
            do_quit();
            return;
        }
    }

    static int background = -1;
    if(background < 0)
        background = load_image(data_directory() +  "/images/space.tga");

    static int tank = -1;
    if(tank < 0) {
        tank = load_image(data_directory() +  "/images/tank.tga");

        // color based on player value
        // alpha value becomes color intensity
        // remember, tgas are BGR (fuck, not really now?)
        lock_surface(tank);

            for(int x=0; x<surface_width(tank); ++x) {
                for(int y=0; y<surface_height(tank); ++y) {
                    Uint8 r, g, b, a;
                    get_rgba(tank, pixel(tank, x, y), &r, &g, &b, &a);

                    pixel(tank, x, y, map_rgba(tank, 0, 0, a, a ? 255 : 0));
                }
            }

        unlock_surface(tank);
    }

    static int flare = -1;
    if(flare < 0)
        flare = load_image(data_directory() +  "/images/flare.tga");

    static int projectile = -1;
    if(projectile < 0)
        projectile = load_image(data_directory() +  "/images/projectile.tga");

    static int smoke = -1;
    if(smoke < 0)
        smoke = load_image(data_directory() +  "/images/smoke.tga");

    clear_window();

    render_background(background, window_width(), window_height());

    m_terrain->render();

/* TODO: write down these fucking physics formulas! */

    // move tank (-190.0f is gravity)
    bool tank_collision = false;
    if(!m_state.paused) {
        Vector3<float> vavg(g_tank_vel.x(), g_tank_vel.y() + ((-190.0f / 2.0f) * elapsed_sec()), g_tank_vel.z());
        Vector3<float> pos(g_tank_pos + (vavg * elapsed_sec()));

        g_tank_vel.y(g_tank_vel.y() + (-190.0f * elapsed_sec()));

        if(m_terrain->collision(g_tank_pos, pos, vavg, &g_collision_pos, tank)) {
            g_tank_pos = g_collision_pos;
            g_tank_vel.clear();

            // we hit the ground, but is it enough?
            const int slide = m_terrain->would_fall(static_cast<int>(g_tank_pos.x()), static_cast<int>(g_tank_pos.y()), tank);
            g_tank_pos.x(g_tank_pos.x() + slide);

            tank_collision = slide == 0;
        } else
            g_tank_pos = pos;
    } else
        tank_collision = true;

    // render tank
    render_tank(tank, window_width(), window_height());

    // render dirt
    static bool should_slide = false;
    if(g_dirt) {
        if(!m_state.paused)
            g_dirt->update(m_terrain, elapsed_sec());

        g_dirt->render(window_width(), window_height());

        render_smoke(smoke, window_width(), window_height());

        if(!m_state.paused) {
            Vector3<float> vavg(g_smoke_vel.x(), g_smoke_vel.y(), g_smoke_vel.z());
            Vector3<float> pos(g_smoke_pos + (vavg * elapsed_sec()));

            g_smoke_pos = pos;
        }

        if(!m_state.paused) {
            if(g_dirt->finished() && !should_slide) {
                delete g_dirt;
                g_dirt = NULL;
            }

            if(should_slide)
                should_slide = m_terrain->slide(elapsed_sec());
        }
    }

    // move projectile
    if(tank_collision && !g_dirt) {
        if(!m_state.paused) {
            // move projectile (-190.0f is gravity)
            Vector3<float> vavg(g_projectile_vel.x(), g_projectile_vel.y() + ((-190.0f / 2.0f) * elapsed_sec()), g_projectile_vel.z());
            Vector3<float> pos(g_projectile_pos + (vavg * elapsed_sec()));

            g_projectile_vel.y(g_projectile_vel.y() + (-190.0f * elapsed_sec()));

            if(m_terrain->collision(g_projectile_pos, pos, vavg, &g_collision_pos, projectile)) {
                // deform the terrain by 1/5 the velocity
                m_terrain->deform(g_collision_pos, static_cast<int>(g_projectile_vel.length() / 5));

                // make off the ground 1px
                const float x = -g_projectile_vel.x();
                const float y = -g_projectile_vel.y();
                pos = g_collision_pos + Vector3<float>(x / std::fabs(x), y / std::fabs(y), 0.0f);

                // create the dirt and shower us with particles (at half the impact velocity)
                g_dirt = new DirtParticleSystem(pos, g_projectile_vel.length() / -2.0f, g_projectile_vel.vec2().angle());
                g_dirt->emit_max();
                g_dirt->render(window_width(), window_height());

                g_smoke_pos = g_collision_pos;
                render_smoke(smoke, window_width(), window_height());

                // reset the projectile info
                g_projectile_pos = g_tank_pos + Vector3<float>(surface_width(tank), surface_height(tank), 0.0f);
#if 1
                g_projectile_vel = Vector3<float>(rand() % 300, rand() % 300, 0.0f);
#else
                g_projectile_vel = Vector3<float>(3, rand() % 300, 0.0f);
#endif

                should_slide = true;
            } else
                g_projectile_pos = pos;
        }

        if(!g_dirt)
            render_projectile(flare, projectile, window_width(), window_height());
    }

    render_hud();

    flip();
}


bool SEarth::initialize_opengl() const
{
    ENTER_FUNCTION(SEarth::initialize_opengl);

    if(!setup_extensions())
        return false;

    glShadeModel(GL_SMOOTH);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearDepth(1.0f);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    //glEnable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glEnable(GL_POLYGON_SMOOTH);    // anti-alias polygons

    glFrontFace(GL_CCW);

    glDepthFunc(GL_LEQUAL);

/* TODO: mess with this to get cool FX */
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

    glViewport(0, 0, window_width(), window_height());

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    const GLfloat ratio = static_cast<GLfloat>(window_width()) / (!window_height() ? 1.0f : static_cast<GLfloat>(window_height()));
    gluPerspective(45.0f, ratio, 0.1f, 100.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    return true;
}


void SEarth::on_key_down(SDLKey key)
{
    switch(key)
    {
    case SDLK_a:
        //if(Running == state->game_state) {
            dump_all_audio();
        //}
        break;
    case SDLK_f:
        m_state.fps = !m_state.fps;
        break;
    case SDLK_p:
        m_state.paused = !m_state.paused;
        break;
case SDLK_b:
    glIsEnabled(GL_BLEND) ? glDisable(GL_BLEND) : glEnable(GL_BLEND);
break;
case SDLK_g:
    glIsEnabled(GL_FOG) ? glDisable(GL_FOG) : glEnable(GL_FOG);
break;
case SDLK_l:
    glIsEnabled(GL_LIGHTING) ? glDisable(GL_LIGHTING) : glEnable(GL_LIGHTING);
break;
    case SDLK_s:
        //if(Running == state->game_state) {
            dump_surfaces();
        //}
        break;
    case SDLK_F11:
        screenshot();
    default:
        break;
    }
}


void SEarth::on_key_up(SDLKey key)
{
    switch(key)
    {
    case SDLK_ESCAPE:
    case SDLK_q:
        do_quit();
        break;
    default:
        break;
    }
}


void SEarth::on_sigsegv()
{
    error("Segmentation Fault caught, exiting cleanly...\n");
    Callstack::dump(std::cerr);
    exit(1);
}


void SEarth::on_sigint()
{
    log("Interrupt caught, exiting cleanly...\n");
    exit(0);
}


void SEarth::on_signal(int signum)
{
    log("Unknown signal caught, continuing anyway...\n");
}
