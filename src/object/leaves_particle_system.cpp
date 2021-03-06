//  SuperTux
//  Copyright (C) 2006 Matthias Braun <matze@braunis.de>
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "object/leaves_particle_system.hpp"

#include <math.h>

#include "math/random_generator.hpp"
#include "supertux/globals.hpp"
#include "supertux/sector.hpp"
#include "util/reader.hpp"
#include "video/drawing_context.hpp"

// TODO: tweak values
namespace LEAVES {
static const float SPIN_SPEED = 20.0f;
static const float WIND_SPEED = 30.0f; // max speed of wind will be randf(WIND_SPEED) * randf(STATE_LENGTH)
static const float STATE_LENGTH = 5.0f;
static const float DECAY_RATIO = 0.2f; // ratio of attack speed to decay speed
static const float EPSILON = 0.5f; //velocity changes by up to this much each tick
static const float WOBBLE_DECAY = 0.99f; //wobble decays exponentially by this much each tick
static const float WOBBLE_FACTOR = 4 * .005f; //wobble approaches drift_speed by this much each tick
}

LeavesParticleSystem::LeavesParticleSystem() :
  state(RELEASING),
  timer(),
  gust_onset(0),
  gust_current_velocity(0)
{
  init();
}

LeavesParticleSystem::LeavesParticleSystem(const ReaderMapping& reader) :
  state(RELEASING),
  timer(),
  gust_onset(0),
  gust_current_velocity(0)
{
  init();
  parse(reader);
}

LeavesParticleSystem::~LeavesParticleSystem()
{
}

void LeavesParticleSystem::init()
{
  leavesimages[0] = Surface::create("images/objects/particles/leaf17.png");
  leavesimages[1] = Surface::create("images/objects/particles/leaf16.png");
  leavesimages[2] = Surface::create("images/objects/particles/leaf15.png");
  leavesimages[3] = Surface::create("images/objects/particles/leaf14.png");
  leavesimages[4] = Surface::create("images/objects/particles/leaf13.png");
  leavesimages[5] = Surface::create("images/objects/particles/leaf12.png");
  leavesimages[6] = Surface::create("images/objects/particles/leaf11.png");
  leavesimages[7] = Surface::create("images/objects/particles/leaf10.png");
  leavesimages[8] = Surface::create("images/objects/particles/leaf9.png");
  leavesimages[9] = Surface::create("images/objects/particles/leaf8.png");
  leavesimages[10] = Surface::create("images/objects/particles/leaf7.png");
  leavesimages[11] = Surface::create("images/objects/particles/leaf6.png");
  leavesimages[12] = Surface::create("images/objects/particles/leaf5.png");
  leavesimages[13] = Surface::create("images/objects/particles/leaf4.png");
  leavesimages[14] = Surface::create("images/objects/particles/leaf3.png");
  leavesimages[15] = Surface::create("images/objects/particles/leaf2.png");
  leavesimages[16] = Surface::create("images/objects/particles/leaf1.png");
  leavesimages[17] = Surface::create("images/objects/particles/leaf0.png");

  virtual_width = SCREEN_WIDTH * 2;

  timer.start(.01);

  // create some random leaves
  size_t leafcount = size_t(virtual_width/10.0);
  for(size_t i=0; i<leafcount; ++i) {
    auto particle = std::unique_ptr<LeavesParticle>(new LeavesParticle);
    int leavessize = graphicsRandom.rand(18);

    particle->pos.x = graphicsRandom.randf(virtual_width);
    particle->pos.y = graphicsRandom.randf(SCREEN_HEIGHT);
    particle->anchorx = particle->pos.x + (graphicsRandom.randf(-0.5, 0.5) * 16);
    // drift will change with wind gusts
    particle->drift_speed = graphicsRandom.randf(-0.5, 0.5) * 0.3;
    particle->wobble = 0.0;

    particle->texture = leavesimages[leavessize];
    particle->leaf_size = powf(leavessize+3,4); // since it ranges from 0 to 2

    particle->speed = 6.32 * (1 + (2 - leavessize)/2 + graphicsRandom.randf(1.8));

    // Spinning
    particle->angle = graphicsRandom.randf(360.0);
    particle->spin_speed = graphicsRandom.randf(-LEAVES::SPIN_SPEED,LEAVES::SPIN_SPEED);

    particles.push_back(std::move(particle));
  }
}

void LeavesParticleSystem::update(float elapsed_time)
{
  if(!enabled)
    return;

  // Simple ADSR wind gusts

  if (timer.check()) {
    // Change state
    state = (State) ((state + 1) % MAX_STATE);

    if(state == RESTING) {
      // stop wind
      gust_current_velocity = 0;
      // new wind strength
      gust_onset   = graphicsRandom.randf(-LEAVES::WIND_SPEED, LEAVES::WIND_SPEED);
    }
    timer.start(graphicsRandom.randf(LEAVES::STATE_LENGTH));
  }

  // Update velocities
  switch(state) {
    case ATTACKING:
      gust_current_velocity += gust_onset * elapsed_time;
      break;
    case DECAYING:
      gust_current_velocity -= gust_onset * elapsed_time * LEAVES::DECAY_RATIO;
      break;
    case RELEASING:
      // uses current time/velocity instead of constants
      gust_current_velocity -= gust_current_velocity * elapsed_time / timer.get_timeleft();
      break;
    case SUSTAINING:
    case RESTING:
      //do nothing
      break;
    default:
      assert(false);
  }

  float sq_g = sqrt(Sector::current()->get_gravity());

  for(auto i = particles.begin(); i != particles.end(); ++i) {
    LeavesParticle* particle = (LeavesParticle*)i->get();
    float anchor_delta;

    // Falling
    particle->pos.y += particle->speed * elapsed_time * sq_g;
    // Drifting (speed approaches wind at a rate dependent on leaf size)
    particle->drift_speed += (gust_current_velocity - particle->drift_speed) / particle->leaf_size + graphicsRandom.randf(-LEAVES::EPSILON,LEAVES::EPSILON);
    particle->anchorx += particle->drift_speed * elapsed_time;
    // Wobbling (particle approaches anchorx)
    particle->pos.x += particle->wobble * elapsed_time * sq_g;
    anchor_delta = (particle->anchorx - particle->pos.x);
    particle->wobble += (LEAVES::WOBBLE_FACTOR * anchor_delta) + graphicsRandom.randf(-LEAVES::EPSILON, LEAVES::EPSILON);
    particle->wobble *= LEAVES::WOBBLE_DECAY;
    // Spinning
    particle->angle += particle->spin_speed * elapsed_time;
    particle->angle = fmodf(particle->angle, 360.0);
  }
}

/* EOF */
