/*
====================
File: DirtParticle.h
Author: Shane Lillie
Description: Dirt particle system subclass

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

#if !defined DIRTPARTICLE_H
#define DIRTPARTICLE_H


#include "Particle.h"


class DirtParticleSystem : public ParticleSystem
{
private:
    class DirtParticle : public Particle
    {
    public:
        DirtParticle(const Vector3<float>& position, const Vector3<float>& velocity, unsigned int texture);

    private:
        virtual void on_update(World* const world, float elapsed_sec, const Particle* const head);
        virtual void on_render(int window_width, int window_height) const;

    private:
        unsigned int m_texture;
    };

private:
    static const int PARTICLE_WIDTH;
    static const int PARTICLE_HEIGHT;
    static const int MAX_PARTICLES;

public:
    DirtParticleSystem(const Vector3<float>& origin, float force, float angle);
    virtual ~DirtParticleSystem();

private:
    void create_texture();
    void delete_texture();

private:
    virtual void on_update(World* const world, float elapsed_sec)
    {
    }

    virtual Particle* const on_add() const;
    virtual void on_render(const Particle* const particle, int window_width, int window_height) const;

private:
    float m_force, m_angle;
    Vector3<float> m_initial_velocity;

    unsigned int m_texture;
};

#endif
