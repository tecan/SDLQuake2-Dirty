/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#ifndef __REF_H
#define __REF_H

#include "qcommon.h"

#define	EXTENDED_API_VERSION	2

#define	MAX_DLIGHTS		32
#define	MAX_ENTITIES	128
#define	MAX_PARTICLES	16384
#define	MAX_LIGHTSTYLES	256

#define POWERSUIT_SCALE		4.0F

#define SHELL_RED_COLOR		0xF2
#define SHELL_GREEN_COLOR	0xD0
#define SHELL_BLUE_COLOR	0xF3

#define SHELL_RG_COLOR		0xDC
//#define SHELL_RB_COLOR		0x86
#define SHELL_RB_COLOR		0x68
#define SHELL_BG_COLOR		0x78

//ROGUE
#define SHELL_DOUBLE_COLOR	0xDF // 223
#define	SHELL_HALF_DAM_COLOR	0x90
#define SHELL_CYAN_COLOR	0x72
//ROGUE

#define SHELL_WHITE_COLOR	0xD7

typedef struct entity_s
{
	struct model_s		*model;			// opaque type outside refresh
	float				angles[3];

	/*
	** most recent data
	*/
	float				origin[3];		// also used as RF_BEAM's "from"
	int					frame;			// also used as RF_BEAM's diameter

	/*
	** previous data for lerping
	*/
	float				oldorigin[3];	// also used as RF_BEAM's "to"
	int					oldframe;

	/*
	** misc
	*/
	float	backlerp;				// 0.0 = current, 1.0 = old
	int		skinnum;				// also used as RF_BEAM's palette index

	int		lightstyle;				// for flashing entities
	float	alpha;					// ignore if RF_TRANSLUCENT isn't set

	struct image_s	*skin;			// NULL for inline skin
	int		flags;

	//vec3_t	velocity;
} entity_t;

#define ENTITY_FLAGS  68

typedef struct
{
	vec3_t	origin;
	vec3_t	color;
	float	intensity;
} dlight_t;

typedef struct
{
	vec3_t	origin;
	int		color;
	float	alpha;
} particle_t;

typedef struct
{
	float		rgb[3];			// 0.0 - 2.0
	float		white;			// highest of rgb
} lightstyle_t;

typedef struct
{
	int			x, y, width, height;// in virtual screen coordinates
	float		fov_x, fov_y;
	float		vieworg[3];
	float		viewangles[3];
	float		blend[4];			// rgba 0-1 full screen blend
	float		time;				// time is uesed to auto animate
	int			rdflags;			// RDF_UNDERWATER, etc

	byte		*areabits;			// if not NULL, only areas with set bits will be drawn

	lightstyle_t	*lightstyles;	// [MAX_LIGHTSTYLES]

	int			num_entities;
	entity_t	*entities;

	int			num_dlights;
	dlight_t	*dlights;

	int			num_particles;
	particle_t	*particles;
} refdef_t;



#define	API_VERSION		3

//
// these are the functions exported by the refresh module
//
typedef struct
{
	// if api_version is different, the dll cannot be used
	int		api_version;

	// called when the library is loaded
	int		(EXPORT *Init) ( void *hinstance, void *wndproc );

	// called before the library is unloaded
	void	(EXPORT *Shutdown) (void);

	// All data that will be used in a level should be
	// registered before rendering any frames to prevent disk hits,
	// but they can still be registered at a later time
	// if necessary.
	//
	// EndRegistration will free any remaining data that wasn't registered.
	// Any model_s or skin_s pointers from before the BeginRegistration
	// are no longer valid after EndRegistration.
	//
	// Skins and images need to be differentiated, because skins
	// are flood filled to eliminate mip map edge errors, and pics have
	// an implicit "pics/" prepended to the name. (a pic name that starts with a
	// slash will not use the "pics/" prefix or the ".pcx" postfix)
	void	(EXPORT *BeginRegistration) (char *map);
	struct model_s * (EXPORT *RegisterModel) (char *name);
	struct image_s * (EXPORT *RegisterSkin) (char *name);
	struct image_s * (EXPORT *RegisterPic) (char *name);
	void	(EXPORT *SetSky) (char *name, float rotate, vec3_t axis);
	void	(EXPORT *EndRegistration) (void);

	void	(EXPORT *RenderFrame) (refdef_t *fd);

	void	(EXPORT *DrawGetPicSize) (int *w, int *h, char *name);	// will return 0 0 if not found
	void	(EXPORT *DrawPic) (int x, int y, char *name);
	void	(EXPORT *DrawStretchPic) (int x, int y, int w, int h, char *name);
	void	(EXPORT *DrawChar) (int x, int y, int c);
	void	(EXPORT *DrawTileClear) (int x, int y, int w, int h, char *name);
	void	(EXPORT *DrawFill) (int x, int y, int w, int h, int c);
	void	(EXPORT *DrawFadeScreen) (void);

	// Draw images for cinematic rendering (which can have a different palette). Note that calls
	void	(EXPORT *DrawStretchRaw) (int x, int y, int w, int h, int cols, int rows, byte *data);

	/*
	** video mode and refresh state management entry points
	*/
	void	(EXPORT *CinematicSetPalette)( const unsigned char *palette);	// NULL = game palette
	void	(EXPORT *BeginFrame)( float camera_separation );
	void	(EXPORT *EndFrame) (void);

	void	(EXPORT *AppActivate)( qboolean activate );
} refexport_t;

//
// these are the functions imported by the refresh module
//
typedef struct
{
	//for backwards compatibility
	int			APIVersion;

	//**********************************************************************
	// extended renderer API functions, check for NULL in ref before using!!
	//**********************************************************************
	int			(IMPORT *FS_FOpenFile) (const char *filename, FILE **file, handlestyle_t openHandle, qboolean *closeHandle);
	void		(IMPORT *FS_FCloseFile) (FILE *file);
	void		(IMPORT *FS_Read) (void *buffer, int len, FILE *f);
} refimportnew_t;

typedef struct
{
	void	(IMPORT *Sys_Error) (int err_level, const char *str, ...) __attribute__ ((format (printf, 2, 3)));

	void	(IMPORT *Cmd_AddCommand) (const char *name, void(*cmd)(void));
	void	(IMPORT *Cmd_RemoveCommand) (const char *name);
	int		(IMPORT *Cmd_Argc) (void);
	char	*(IMPORT *Cmd_Argv) (int i);
	void	(IMPORT *Cmd_ExecuteText) (int exec_when, char *text);

	void	(IMPORT *Con_Printf) (int print_level, const char *str, ...) __attribute__ ((format (printf, 2, 3)));

	// files will be memory mapped read only
	// the returned buffer may be part of a larger pak file,
	// or a discrete file from anywhere in the quake search path
	// a -1 return means the file does not exist
	// NULL can be passed for buf to just determine existance
	int		(IMPORT *FS_LoadFile) (const char *name, void **buf);
	void	(IMPORT *FS_FreeFile) (void *buf);

	// gamedir will be the current directory that generated
	// files should be stored to, ie: "f:\quake\id1"
	char	*(IMPORT *FS_Gamedir) (void);

	cvar_t	*(IMPORT *Cvar_Get) (const char *name, const char *value, int flags);
	cvar_t	*(IMPORT *Cvar_Set)( const char *name, const char *value );
	void	 (IMPORT *Cvar_SetValue)( const char *name, float value );

	qboolean	(IMPORT *Vid_GetModeInfo)( int *width, int *height, int mode );
	void		(IMPORT *Vid_MenuInit)( void );
	void		(IMPORT *Vid_NewWindow)( int width, int height );
} refimport_t;

// this is the only function actually exported at the linker level
typedef	refexport_t	(EXPORT *GetRefAPI_t) (refimport_t);
typedef	void (EXPORT *GetExtraAPI_t) (refimportnew_t);

#endif // __REF_H
