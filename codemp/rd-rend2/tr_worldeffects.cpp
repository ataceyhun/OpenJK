#include "tr_worldeffects.h"

#include "tr_local.h"

namespace
{
	class WeatherWorld;
	struct WeatherWorldContext
	{
		float gravity;
		vec3_t windVelocity;
		qboolean running;

		WeatherWorld *world;
	};

	const float POINTCACHE_CELL_SIZE = 64.0f;

	// WeatherZone represents an area in the map which contains a weather
	// effect. The point cache is a voxelized version of the area it encompasses
	// where each bit is either a 0 for an inside area of the map, or 1 for an
	// outside area of the map.
	struct WeatherZone
	{
		vec3_t mins;
		vec3_t maxs;

		uint32_t *pointCache;
		size_t cacheSize;
		int cacheWidth;
		int cacheHeight;
		int cacheDepth;

		WeatherZone ( const vec3_t mins, const vec3_t maxs );
		~WeatherZone();

		qboolean IsPointOutside ( const vec3_t point ) const;
	};

	struct Particles
	{
		int count;
		qboolean spawned;

		float	*alphas;
		vec4_t	*colors;
		vec3_t	*positions;
		vec3_t	*velocities;
		float	*invMasses;

		qboolean *rendering;
		qboolean *fadingIn;
		qboolean *fadingOut;

		Particles ( int count );
		~Particles();

		void Swap ( Particles& particles );
	};

	struct ParticleCloudDescriptor
	{
		float height;
		float width;
		float gravity;
		float fade;
		qboolean alignWithVelocity;
		qboolean water;
		byte color[4];
	};

	class ParticleCloud
	{
	public:
		ParticleCloud ( int numParticles, shader_t *particleShader, ParticleCloudDescriptor& descriptor );

		~ParticleCloud();

		// Updates the particles in the particle cloud.
		void Update ( const WeatherWorldContext& context, float dt );

		// Submits the particle cloud to render queue
		void Submit ( const WeatherWorldContext& context ) const;

		// Renders the particle cloud
		void Render ( const srfParticleCloud_t& surf ) const;

	private:
		Particles	particles;
		ParticleCloudDescriptor desc;
		shader_t	*particleShader;
		glIndex_t	*indices;
		vec2_t		*texcoords;
		srfParticleCloud_t surface;
	};

	// This class contains all the 'weather zones' found in the map.
	class WeatherWorld
	{
	public:
		WeatherWorld();
		~WeatherWorld();

		// Adds a weather zone with the given mins/maxs
		void AddWeatherZone ( const vec3_t mins, const vec3_t maxs );

		// Creates a new particle cloud and adds it to the world
		ParticleCloud *CreateParticleCloud ( int numParticles, shader_t *particleShader, ParticleCloudDescriptor& descriptor );

		// Caches inside/outside regions.
		void Cache();

		// Clear all weather zones
		void Clear();

		// Tests if the given point is in an outside region.
		qboolean IsPointOutside ( const vec3_t pos ) const;

		// Renders the weather effects.
		void Render ( const WeatherWorldContext& context, float dt );

	private:
		qboolean	cached;
		qboolean	outsideShake;
		float		painOutside;

		int			numWeatherZones;
		WeatherZone	*weatherZones;

		int			numParticleClouds;
		ParticleCloud *particleClouds;

	};

	// Tests if a given point is inside the mins/maxs extents.
	qboolean IsPointInsideAABB ( const vec3_t point, const vec3_t mins, const vec3_t maxs )
	{
		for ( int i = 0; i < 3; i++ )
		{
			if ( point[i] < mins[i] || point[i] > maxs[i] )
			{
				return qfalse;
			}
		}

		return qtrue;
	}

	// Aligns a mins/maxs extents to a multiple of 'align'. This
	// will always expand (or retain) the bounded region.
	void AlignBoundingBox ( vec3_t mins, vec3_t maxs, float align )
	{
		mins[0] = floor ((mins[0] - align) / align) * align;
		mins[1] = floor ((mins[1] - align) / align) * align;
		mins[2] = floor ((mins[2] - align) / align) * align;

		maxs[0] = floor ((maxs[0] + align) / align) * align;
		maxs[1] = floor ((maxs[1] + align) / align) * align;
		maxs[2] = floor ((maxs[2] + align) / align) * align;
	}

	void GetGridCellPosition ( const vec3_t pos, const vec3_t mins, float gridSize, vec3_t gridPos )
	{
		vec3_t offset;
		VectorSubtract (pos, mins, offset);

		VectorScale (offset, 1.0f / gridSize, gridPos);

		for ( int i = 0; i < 3; i++ )
		{
			gridPos[i] = floor (gridPos[i]);
		}
	}

	WeatherZone::WeatherZone ( const vec3_t mins, const vec3_t maxs )
	{
		VectorCopy (mins, this->mins);
		VectorCopy (maxs, this->maxs);

		AlignBoundingBox (this->mins, this->maxs, POINTCACHE_CELL_SIZE);

		cacheWidth	= (maxs[0] - mins[0]) / POINTCACHE_CELL_SIZE;
		cacheHeight	= (maxs[1] - mins[1]) / POINTCACHE_CELL_SIZE;
		cacheDepth	= (maxs[2] - mins[2]) / POINTCACHE_CELL_SIZE;

		cacheSize	= sizeof (uint32_t) * (cacheWidth * cacheHeight * cacheDepth) / 32;
		pointCache	= (uint32_t *)Z_Malloc (cacheSize, TAG_POINTCACHE, qtrue);
	}

	WeatherZone::~WeatherZone()
	{
		Z_Free (pointCache);
	}

	qboolean WeatherZone::IsPointOutside ( const vec3_t point ) const
	{
		if ( !IsPointInsideAABB (point, mins, maxs) )
		{
			return qfalse;
		}

		vec3_t gridPos;
		GetGridCellPosition (point, mins, POINTCACHE_CELL_SIZE, gridPos);

		int element = gridPos[2] * cacheDepth + gridPos[1] * cacheHeight + gridPos[0];
		int compressed = element / 32;
		uint32_t bit = 1u << (element - (compressed * 32));

		return (qboolean)((pointCache[element] & bit) != 0);
	}

	WeatherWorld::WeatherWorld()
		: cached (qfalse)
		, outsideShake (qfalse)
		, painOutside (0.0f)
		, numWeatherZones (0)
		, numParticleClouds (0)
	{
		weatherZones = (WeatherZone *)Z_Malloc (12 * sizeof (WeatherZone), TAG_POINTCACHE, qtrue);
		particleClouds = (ParticleCloud *)Z_Malloc (10 * sizeof (ParticleCloud), TAG_POINTCACHE, qtrue);
	}

	WeatherWorld::~WeatherWorld()
	{
		for ( int i = 0; i < numWeatherZones; i++ )
		{
			weatherZones[i].~WeatherZone();
		}

		for ( int i = 0; i < numParticleClouds; i++ )
		{
			particleClouds[i].~ParticleCloud();
		}

		Z_Free (particleClouds);
		Z_Free (weatherZones);
	}

	void WeatherWorld::AddWeatherZone ( const vec3_t mins, const vec3_t maxs )
	{
		if (numWeatherZones == 12)
		{
			ri->Printf (PRINT_WARNING, S_COLOR_YELLOW "WARNING: Tried to add weather zone, but no free zones available.\n");
			return;
		}

		WeatherZone *zone = new (&weatherZones[numWeatherZones]) WeatherZone (mins, maxs);
		numWeatherZones++;
	}

	ParticleCloud *WeatherWorld::CreateParticleCloud ( int numParticles, shader_t *particleShader, ParticleCloudDescriptor& descriptor )
	{
		if ( numParticleClouds >= 10 )
		{
			ri->Printf (PRINT_WARNING, S_COLOR_YELLOW "WARNING: Tried to create another particle cloud, but no free clouds available.\n");
			return NULL;
		}

		return new (&particleClouds[numParticleClouds++]) ParticleCloud (numParticles, particleShader, descriptor);
	}

	void WeatherWorld::Cache()
	{
		if ( tr.world == NULL || cached )
		{
			return;
		}

		if ( numWeatherZones == 0 )
		{
			ri->Printf (PRINT_WARNING, S_COLOR_YELLOW "WARNING: No weather zones found. Creating zone using map bounds.\n");
			AddWeatherZone (tr.world->bmodels[0].bounds[0], tr.world->bmodels[0].bounds[1]);
		}

		qboolean markedOutside = qfalse;
		const vec3_t sampleOffset = { POINTCACHE_CELL_SIZE, POINTCACHE_CELL_SIZE, POINTCACHE_CELL_SIZE };
		for ( int i = 0; i < numWeatherZones; i++ )
		{
			WeatherZone& zone = weatherZones[i];
			vec3_t startSamplePoint;

			VectorAdd (zone.mins, sampleOffset, startSamplePoint);
			
			for ( int z = 0; z < zone.cacheDepth; z++ )
			{
				for ( int y = 0; y < zone.cacheHeight; y++ )
				{
					for ( int x = 0; x < zone.cacheWidth; x++ )
					{
						vec3_t pos;
						VectorSet (pos,
							x * POINTCACHE_CELL_SIZE,
							y * POINTCACHE_CELL_SIZE,
							z * POINTCACHE_CELL_SIZE);

						VectorAdd (pos, startSamplePoint, pos);

						int contents = ri->CM_PointContents (pos, 0);
						if ( contents & (CONTENTS_INSIDE | CONTENTS_OUTSIDE) )
						{
							qboolean outside = (qboolean)((contents & CONTENTS_OUTSIDE) != 0);
							if ( !cached )
							{
								cached = qtrue;
								markedOutside = outside;

								if ( !outside )
								{
									// If inside, then mark all cells as outside.
									Com_Memset (zone.pointCache, ~0, zone.cacheSize);
								}
							}
							else if ( markedOutside != outside )
							{
								Com_Error (ERR_DROP, "Weather Effect: Both indoor and outdoor brushs were found in the map.\n");
								return;
							}
							
							int element = z * zone.cacheDepth + y * zone.cacheHeight + x;
							int compressed = element / 32;
							uint32_t bit = 1u << (element - (compressed * 32));

							if ( markedOutside )
							{
								// Assume all things are inside to start with,
								// so need to mark outside points as such.
								if ( outside )
								{
									zone.pointCache[compressed] |= bit;
								}
							}
							else
							{
								// We assumed all things were outside, so
								// unset the bit to say this cell is inside.
								if ( !outside )
								{
									zone.pointCache[compressed] &= ~bit;
								}
							}
						}
					}
				}
			}
		}

		if ( !cached )
		{
			// Even if we found nothing, then assume everything is
			// outside.
			cached = qtrue;
			for ( int i = 0; i < numWeatherZones; i++ )
			{
				WeatherZone& zone = weatherZones[i];
				Com_Memset (zone.pointCache, ~0, zone.cacheSize);
			}
		}
	}

	void WeatherWorld::Clear()
	{
		for ( int i = 0; i < numWeatherZones; i++ )
		{
			weatherZones[i].~WeatherZone();
		}

		numWeatherZones = 0;
	}

	qboolean WeatherWorld::IsPointOutside ( const vec3_t point ) const
	{
		for ( int i = 0; i < numWeatherZones; i++ )
		{
			const WeatherZone& zone = weatherZones[i];

			if ( zone.IsPointOutside (point) )
			{
				return qtrue;
			}
		}

		return qfalse;
	}

	void WeatherWorld::Render ( const WeatherWorldContext& context, float dt )
	{
		for ( int i = 0; i < numParticleClouds; i++ )
		{
			particleClouds[i].Update (context, dt);
			particleClouds[i].Submit (context);
		}
	}

	Particles::Particles ( int count )
		: count (count)
		, spawned (qfalse)
	{
		alphas		= (float *)Z_Malloc (count * sizeof (float), TAG_POINTCACHE, qtrue);
		colors		= (vec4_t *)Z_Malloc (count * sizeof (vec4_t), TAG_POINTCACHE, qtrue);
		positions	= (vec3_t *)Z_Malloc (count * sizeof (vec3_t), TAG_POINTCACHE, qtrue);
		velocities	= (vec3_t *)Z_Malloc (count * sizeof (vec3_t), TAG_POINTCACHE, qtrue);
		invMasses	= (float *)Z_Malloc (count * sizeof (float), TAG_POINTCACHE, qtrue);

		rendering	= (qboolean *)Z_Malloc (count * sizeof (qboolean), TAG_POINTCACHE, qtrue);
		fadingIn	= (qboolean *)Z_Malloc (count * sizeof (qboolean), TAG_POINTCACHE, qtrue);
		fadingOut	= (qboolean *)Z_Malloc (count * sizeof (qboolean), TAG_POINTCACHE, qtrue);
	}

	Particles::~Particles()
	{
		Z_Free (alphas);
		Z_Free (colors);
		Z_Free (positions);
		Z_Free (velocities);
		Z_Free (invMasses);

		Z_Free (rendering);
		Z_Free (fadingIn);
		Z_Free (fadingOut);
	}

	// This constructor is a big ugly, and does way too much stuff. Should consider refactoring it at some point.
	ParticleCloud::ParticleCloud ( int numParticles, shader_t *particleShader, ParticleCloudDescriptor& descriptor )
		: particles (numParticles)
		, desc (descriptor)
		, particleShader (particleShader)
	{
		for ( int i = 0; i < numParticles; i++ )
		{
			particles.invMasses[i] = 1.0f / flrand (5.0f, 10.0f);
		}

		int numVertices = numParticles;

		surface.surfaceType = SF_PARTICLECLOUD;
		surface.particleCloud = static_cast<void *>(this);

		texcoords = (vec2_t *)Z_Malloc (sizeof (vec2_t) * numVertices, TAG_POINTCACHE);
		indices = (glIndex_t *)Z_Malloc (sizeof (glIndex_t) * numVertices, TAG_POINTCACHE);
		for ( int i = 0; i < numVertices; i++ )
		{
			indices[i] = i;
		}

		for ( int i = 0; i < numVertices; i++ )
		{
			VectorSet2 (texcoords[i], 1.0f, 0.0f);
		}

		size_t vertexSize = sizeof (particles.positions[0]) + sizeof (particles.colors[0]) + sizeof (*texcoords);

		surface.ibo = R_CreateIBO ((byte *)indices, sizeof (glIndex_t) * numVertices, VBO_USAGE_DYNAMIC);
		surface.vbo = R_CreateVBO (NULL, numVertices * vertexSize, VBO_USAGE_DYNAMIC);

		surface.vbo->ofs_xyz = 0;
		surface.vbo->ofs_vertexcolor = sizeof (particles.positions[0]);
		surface.vbo->ofs_st = surface.vbo->ofs_vertexcolor + sizeof (particles.colors[0]);

		surface.vbo->stride_xyz = vertexSize;
		surface.vbo->stride_vertexcolor = vertexSize;
		surface.vbo->stride_st = vertexSize;
	}

	ParticleCloud::~ParticleCloud()
	{
		Z_Free (indices);
		Z_Free (texcoords);
	}

	void ParticleCloud::Update ( const WeatherWorldContext& context, float dt )
	{
		vec3_t viewOrigin;
		vec3_t viewLeft, viewForward, viewDown;
		vec3_t viewLeftUp, viewLeftDown;

		const float spawnRangeMins = -500.0f * 1.25f;
		const float spawnRangeMaxs =  500.0f * 1.25f;

		if ( !context.running )
		{
			return;
		}

		// Decompose camera matrix
		VectorCopy (backEnd.viewParms.ori.origin, viewOrigin);
		VectorCopy (backEnd.viewParms.ori.axis[0], viewForward);
		VectorCopy (backEnd.viewParms.ori.axis[1], viewLeft);
		VectorCopy (backEnd.viewParms.ori.axis[2], viewDown);

		VectorScale (viewLeft, desc.width, viewLeft);
		VectorScale (viewDown, desc.height, viewDown);

		if ( desc.alignWithVelocity )
		{
			VectorScale (viewDown, -1.0f, viewDown);
		}

		VectorSubtract (viewLeft, viewDown, viewLeftUp);
		VectorAdd (viewLeft, viewDown, viewLeftDown);

		// Calculate global acceleration of all particles
		vec3_t force;
		VectorClear (force);
		force[2] = -desc.gravity;
		VectorAdd (force, context.windVelocity, force);

		// Spawn if necessary
		if ( !particles.spawned )
		{
			for ( int i = 0; i < particles.count; i++ )
			{
				particles.positions[i][0] = flrand (
					viewOrigin[0] + spawnRangeMins,
					viewOrigin[0] + spawnRangeMaxs);

				particles.positions[i][1] = flrand (
					viewOrigin[1] + spawnRangeMins,
					viewOrigin[1] + spawnRangeMaxs);

				particles.positions[i][2] = flrand (
					viewOrigin[2] + spawnRangeMins,
					viewOrigin[2] + spawnRangeMaxs);
			}

			particles.spawned = qtrue;
		}

		// Update all particles
		for ( int i = 0; i < particles.count; i++ )
		{
			vec3_t accel;

			particles.rendering[i] = qtrue;

			// accel = force / mass
			VectorScale (force, particles.invMasses[i], accel);

			// velocity' = 0.7 * (accel + velocity)
			VectorAdd (particles.velocities[i], accel, particles.velocities[i]);
			VectorScale (particles.velocities[i], 0.7f, particles.velocities[i]);

			particles.alphas[i] = 0.5f;

#if 0
			// position' = position + dt * velocity
			VectorMA (
				particles.positions[i],
				dt,
				particles.velocities[i],
				particles.positions[i]);
#endif

			VectorScale4 (desc.color, particles.alphas[i], particles.colors[i]);
		}
	}

	void ParticleCloud::Submit ( const WeatherWorldContext& context ) const
	{
		tr.currentEntityNum = REFENTITYNUM_WORLD;
		tr.shiftedEntityNum = tr.currentEntityNum << QSORT_REFENTITYNUM_SHIFT;

		R_AddDrawSurf ((surfaceType_t *)&surface, particleShader, 0, 0, 0, 0);
	}

	void ParticleCloud::Render ( const srfParticleCloud_t& surf ) const
	{
		RB_EndSurface();

		int numRendering = 0;
		for ( int i = 0, j = 0; i < particles.count; i++ )
		{
			if ( !particles.rendering[i] )
			{
				continue;
			}

			indices[j] = j;
			numRendering++;
			j++;
		}

		// Update VBOs
		R_BindVBO (surf.vbo);
		R_BindIBO (surf.ibo);

		GLbitfield mapBits = GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT;
		void *data = qglMapBufferRange (GL_ARRAY_BUFFER, 0, surf.vbo->vertexesSize, mapBits);

		for ( int i = 0; i < particles.count; i++ )
		{
			size_t offset = 0;

			memcpy ((char *)data + offset, particles.positions[i], sizeof (particles.positions[0]));
			offset += sizeof (particles.positions[0]);

			memcpy ((char *)data + offset, particles.colors[i], sizeof (particles.colors[0]));
			offset += sizeof (particles.colors[0]);

			memcpy ((char *)data + offset, texcoords[i], sizeof (texcoords[0]));
			offset += sizeof (texcoords[0]);

			data = (char *)data + offset;
		}

		qglUnmapBuffer (GL_ARRAY_BUFFER);

		// Update state
		GL_Cull (CT_TWO_SIDED);
		GL_Bind (particleShader->stages[0]->bundle[0].image[0]);
		GL_State (GLS_DEFAULT);

		GLSL_VertexAttribsState (ATTR_POSITION | ATTR_TEXCOORD0 | ATTR_COLOR);
		GLSL_BindProgram (&tr.weatherRainShader);

		// Set uniforms
		GLSL_SetUniformMatrix16(&tr.weatherRainShader, UNIFORM_MODELVIEWPROJECTIONMATRIX, glState.modelviewProjection);

		// Draw!
		qglEnable (GL_PROGRAM_POINT_SIZE);
		R_DrawElementsVBO (GL_POINTS, numRendering, 0, 0, numRendering);
	}
}

static qboolean Weather_ParseVector ( const char **text, int numComponents, float *result )
{
	const char *token;

	token = COM_ParseExt (text, qfalse);
	if ( strcmp (token, "(") != 0 )
	{
		ri->Printf (PRINT_WARNING, "WARNING: missing '(' in weather effect.\n");
		return qfalse;
	}

	for ( int i = 0; i < numComponents; i++ )
	{
		token = COM_ParseExt (text, qfalse);
		if ( token[0] == '\0' )
		{
			ri->Printf (PRINT_WARNING, "WARNING: missing vector element in weather effect.\n");
			return qfalse;
		}

		result[i] = atof (token);
	}

	token = COM_ParseExt (text, qfalse);
	if ( strcmp (token, ")") != 0 )
	{
		ri->Printf (PRINT_WARNING, "WARNING: missing ')' in weather effect.\n");
		return qfalse;
	}

	return qtrue;
}

static shader_t *Weather_CreateShader ( const char *name, const char *imagePath, colorGen_t rgbGen )
{
	image_t *image = R_FindImageFile (
		imagePath,
		IMGTYPE_COLORALPHA,
		IMGFLAG_NO_COMPRESSION | IMGFLAG_CLAMPTOEDGE);

	if ( image == NULL )
	{
		return NULL;
	}

	return R_CreateWeatherShader (name, GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA, image, rgbGen);
}

static void WeatherCommand_Die ( const char *, WeatherWorldContext& )
{
	R_ShutdownWorldEffects();
}

static void WeatherCommand_Clear ( const char *, WeatherWorldContext& context )
{
	context.world->Clear();
}

static void WeatherCommand_Freeze ( const char *, WeatherWorldContext& context )
{
	context.running = qfalse;
}

static void WeatherCommand_AddWeatherZone ( const char *command, WeatherWorldContext& context )
{
	vec3_t mins;
	vec3_t maxs;

	if ( !Weather_ParseVector (&command, 3, mins) || !Weather_ParseVector (&command, 3, maxs) )
	{
		return;
	}

	context.world->AddWeatherZone (mins, maxs);
}

static void WeatherCommand_AddWind ( const char *, WeatherWorldContext& context )
{

}

static void WeatherCommand_AddConstantWind ( const char *command, WeatherWorldContext& context )
{

}

static void WeatherCommand_AddGustingWind ( const char *, WeatherWorldContext& context )
{

}

static void WeatherCommand_AddLightRain ( const char *, WeatherWorldContext& context )
{
	shader_t *shader = Weather_CreateShader ("lightrain", "gfx/world/rain.jpg", CGEN_VERTEX);
	if ( shader == NULL )
	{
		ri->Printf (PRINT_WARNING, "WARNING: Failed to create custom shader for light rain.\n");
		return;
	}

	ParticleCloudDescriptor desc;
	desc.alignWithVelocity = qtrue;
	desc.water = qtrue;
	desc.fade = 100.0f;
	desc.gravity = 150.0f;
	desc.height = 80.0f;
	desc.width = 1.2f;
	VectorSet4 (desc.color, 0, 255, 0, 255);
	
	context.world->CreateParticleCloud (500, shader, desc);
}

static void WeatherCommand_AddRain ( const char *, WeatherWorldContext& context )
{

}

static void WeatherCommand_AddAcidRain ( const char *, WeatherWorldContext& context )
{
}

static void WeatherCommand_AddHeavyRain ( const char *, WeatherWorldContext& context )
{

}

static void WeatherCommand_AddSnow ( const char *, WeatherWorldContext& context )
{
}

static void WeatherCommand_AddSpaceDust ( const char *, WeatherWorldContext& context )
{
}

static void WeatherCommand_AddSand ( const char *, WeatherWorldContext& context )
{
}

static void WeatherCommand_AddFog ( const char *, WeatherWorldContext& context )
{
}

static void WeatherCommand_AddHeavyRainFog ( const char *, WeatherWorldContext& context )
{
}

static void WeatherCommand_AddLightFog ( const char *, WeatherWorldContext& context )
{
}

static void WeatherCommand_OutsideShake ( const char *, WeatherWorldContext& context )
{
}

static void WeatherCommand_OutsidePain ( const char *, WeatherWorldContext& context )
{
}

typedef struct worldEffectCommand_s worldEffectCommand_t;
static struct worldEffectCommand_s
{
	const char	*cmd;
	const char	*args;
	void		(*run)( const char *command, WeatherWorldContext& context );
} worldEffectCommands[] = {
	{ "die",			NULL,				WeatherCommand_Die },
	{ "clear",			NULL,				WeatherCommand_Clear },
	{ "freeze",			NULL,				WeatherCommand_Freeze },
	{ "zone",			"(mins) (maxs)",	WeatherCommand_AddWeatherZone },
	{ "wind",			NULL,				WeatherCommand_AddWind },
	{ "constantwind",	"(velocity)",		WeatherCommand_AddConstantWind },
	{ "gustingwind",	NULL,				WeatherCommand_AddGustingWind },
	{ "lightrain",		NULL,				WeatherCommand_AddLightRain },
	{ "rain",			NULL,				WeatherCommand_AddRain },
	{ "acidrain",		NULL,				WeatherCommand_AddAcidRain },
	{ "heavyrain",		NULL,				WeatherCommand_AddHeavyRain },
	{ "snow",			NULL,				WeatherCommand_AddSnow },
	{ "spacedust",		NULL,				WeatherCommand_AddSpaceDust },
	{ "sand",			NULL,				WeatherCommand_AddSand },
	{ "fog",			NULL,				WeatherCommand_AddFog },
	{ "heavyrainfog",	NULL,				WeatherCommand_AddHeavyRainFog },
	{ "light_fog",		NULL,				WeatherCommand_AddLightFog },
	{ "outsideshake",	NULL,				WeatherCommand_OutsideShake },
	{ "outsidepain",	NULL,				WeatherCommand_OutsidePain }
};
static const size_t numWorldEffectCommands = ARRAY_LEN (worldEffectCommands);

static const worldEffectCommand_t *GetWorldEffectCommand ( const char *command )
{
	for ( size_t i = 0; i < numWorldEffectCommands; i++ )
	{
		if ( Q_stricmp (worldEffectCommands[i].cmd, command) == 0 )
		{
			return &worldEffectCommands[i];
		}
	}

	return NULL;
}

static WeatherWorldContext worldContext;

void R_InitWorldEffects()
{
	memset (&worldContext, 0, sizeof (worldContext));
	worldContext.running = qtrue;
	
	WeatherWorld *context = (WeatherWorld *)Z_Malloc (sizeof (*worldContext.world), TAG_POINTCACHE);
	worldContext.world = new (context) WeatherWorld ();
}

void R_ShutdownWorldEffects()
{
	// Now that we handle explicitly handling the destructor, we need to
	// also check for null pointer ourselves!
	if ( worldContext.world != NULL )
	{
		worldContext.world->~WeatherWorld();
		worldContext.world = NULL;
	}
}

void R_RenderWorldEffects()
{
	float dt = backEnd.refdef.floatTime;
	dt = CLAMP (dt, 1.0f, 1000.0f) * 0.001f;

	worldContext.world->Render (worldContext, dt);
}

void RE_WorldEffectCommand ( const char *command )
{
	if ( command == NULL )
	{
		return;
	}

	const char *token;

	COM_BeginParseSession ("RE_WorldEffectCommand");
	token = COM_ParseExt (&command, qfalse);

	const worldEffectCommand_t *effectCmd = GetWorldEffectCommand (token);
	if ( effectCmd == NULL )
	{
		Com_Printf ("Weather Effect: please enter a valid command.\n");

		for ( size_t i = 0; i < numWorldEffectCommands; i++ )
		{
			const worldEffectCommand_t *cmd = &worldEffectCommands[i];
			if ( cmd->args == NULL )
			{
				Com_Printf ("\t%s\n", cmd->cmd);
			}
			else
			{
				Com_Printf ("\t%s %s\n",  cmd->cmd, cmd->args);
			}
		}
	
		return;
	}

	effectCmd->run (command, worldContext);
}

void RE_AddWeatherZone ( vec3_t mins, vec3_t maxs )
{
	worldContext.world->AddWeatherZone (mins, maxs);
}

void R_WorldEffect_f()
{
	char temp[2048] = {};

	ri->Cmd_ArgsBuffer (temp, sizeof( temp ));
	RE_WorldEffectCommand (temp);
}

void RB_SurfaceParticleCloud ( srfParticleCloud_t *surf )
{
	ParticleCloud *cloud = static_cast<ParticleCloud *>(surf->particleCloud);
	cloud->Render (*surf);
}
