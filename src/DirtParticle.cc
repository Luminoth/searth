/*
====================
File: DirtParticle.cc
Author: Shane Lillie
Description: Dirt particle system class source

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
#include <cstdlib>

#include "SDL_opengl.h"

#include "DirtParticle.h"
#include "Terrain.h"
#include "Engine.h"
#include "Callstack.h"


/*
 *  DirtParticle methods
 *
 */


DirtParticleSystem::DirtParticle::DirtParticle(const Vector3<float>& position, const Vector3<float>& velocity, unsigned int texture)
    : Particle(position, velocity),
        m_texture(texture)
{
    m_lifetime = INFINITE_LIFETIME;

    m_width = PARTICLE_WIDTH;
    m_height = PARTICLE_HEIGHT;
}


void DirtParticleSystem::DirtParticle::on_update(World* const world, float elapsed_sec, const Particle* const head)
{
    /* -190 is gravity */
    Vector3<float> vavg(m_velocity.x(), m_velocity.y() + ((-190.0f / 2.0f) * elapsed_sec), m_velocity.z());
    Vector3<float> pos(m_position + (vavg * elapsed_sec));

    m_velocity.y(m_velocity.y() + (-190.0f * elapsed_sec));

    m_prev_position = m_position;

    if(world->collision(m_position, pos, vavg, NULL, static_cast<int>(m_width), static_cast<int>(m_height)))
        kill();
    else
        m_position = pos;
}


void DirtParticleSystem::DirtParticle::on_render(int window_width, int window_height) const
{
    if(is_dead())
        return;

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

            glTranslatef(m_position.x(), m_position.y(), m_position.z());

            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(m_texture));

/* FIXME: move most of this shit out into the system to make rendering faster */
            glBegin(GL_QUADS);
                glNormal3f(0.0f, 0.0f, 1.0f);
                glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, 0.0f);
                glTexCoord2f(1.0f, 0.0f); glVertex2f(m_width, 0.0f);
                glTexCoord2f(1.0f, 1.0f); glVertex2f(m_width, m_height);
                glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, m_height);
            glEnd();

        glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    if(depth_test)
        glEnable(GL_DEPTH_TEST);
}


/*
 *  DirtParticleSystem class constants
 *
 */


const int DirtParticleSystem::PARTICLE_WIDTH = 2;
const int DirtParticleSystem::PARTICLE_HEIGHT = 2;
const int DirtParticleSystem::MAX_PARTICLES = 500;


/*
 *  DirtParticleSystem methods
 *
 */


DirtParticleSystem::DirtParticleSystem(const Vector3<float>& origin, float force, float angle)
    : ParticleSystem(MAX_PARTICLES, origin),
        m_force(force), m_angle(angle), m_texture(0)
{
    Vector2<float> v;
    v.construct(force, angle);
    m_initial_velocity = v.vec3();

    create_texture();
}


DirtParticleSystem::~DirtParticleSystem()
{
    delete_texture();
}


void DirtParticleSystem::create_texture()
{
    if(m_texture)
        delete_texture();

    int surface = Engine::create_surface(PARTICLE_WIDTH, PARTICLE_HEIGHT, 32, "dirt_particle");
    if(surface < 0) return;

    Engine::lock_surface(surface);

        for(int x=0; x<PARTICLE_WIDTH; ++x)
            for(int y=0; y<PARTICLE_HEIGHT; ++y)
                Engine::pixel(surface, x, y, Engine::map_rgba(surface, 0, 192, 6, 255));

    Engine::unlock_surface(surface);

    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, PARTICLE_WIDTH, PARTICLE_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, Engine::surface_pixels(surface));

    Engine::unload_surface(surface);
}


void DirtParticleSystem::delete_texture()
{
    if(m_texture)
        glDeleteTextures(1, &m_texture);
    m_texture = 0;
}


Particle* const DirtParticleSystem::on_add() const
{
    // fuzz the velocity a bit
    Vector3<float> vel;
    vel.y(/*m_initial_velocity.y()*/ m_initial_velocity.length() + (rand() % 50) - 25.0f);
    vel.x(/*m_initial_velocity.x() +*/ (rand() % 50) - 25.0f);
    vel.z(m_initial_velocity.z());

    // fuzz the origin a bit
    Vector3<float> orig(origin());
    orig.x(orig.x() + (rand() % 24) - 12.0f);
    orig.y(orig.y() + (rand() % 24) - 12.0f);

    return new DirtParticle(orig, vel, m_texture);
}


void DirtParticleSystem::on_render(const Particle* const particle, int window_width, int window_height) const
{
    particle->render(window_width, window_height);
}
