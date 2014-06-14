#pragma once

#include "qcommon/q_shared.h"

typedef struct srfParticleCloud_s srfParticleCloud_t;

// Initialize the world effects system.
void R_InitWorldEffects();

// Shutdown the world effects system.
void R_ShutdownWorldEffects();

// Command handler for weather commands.
void R_WorldEffect_f();

// Exported function for handling weather commands.
void RE_WorldEffectCommand ( const char *command );

// Adds a new weather zone to the world.
void RE_AddWeatherZone ( vec3_t mins, vec3_t maxs );

// Renders the world effects.
void R_RenderWorldEffects();

// Backend function to render particle clouds
void RB_SurfaceParticleCloud ( srfParticleCloud_t *surf );
