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
// cl_ents.c -- entity parsing and management

#include "client.h"


extern	struct model_s	*cl_mod_powerscreen;

//PGM
int	vidref_val;
//PGM

/*
=========================================================================

FRAME PARSING

=========================================================================
*/

#if 0

typedef struct
{
	int		modelindex;
	int		num; // entity number
	int		effects;
	vec3_t	origin;
	vec3_t	oldorigin;
	vec3_t	angles;
	qboolean present;
} projectile_t;

#define	MAX_PROJECTILES	64
projectile_t	cl_projectiles[MAX_PROJECTILES];

void CL_ClearProjectiles (void)
{
	int i;

	for (i = 0; i < MAX_PROJECTILES; i++) {
//		if (cl_projectiles[i].present)
//			Com_DPrintf("PROJ: %d CLEARED\n", cl_projectiles[i].num);
		cl_projectiles[i].present = false;
	}
}

/*
=====================
CL_ParseProjectiles

Flechettes are passed as efficient temporary entities
=====================
*/
void CL_ParseProjectiles (void)
{
	int		i, c, j;
	byte	bits[8];
	byte	b;
	projectile_t	pr;
	int lastempty = -1;
	qboolean old = false;

	c = MSG_ReadByte (&net_message);
	for (i=0 ; i<c ; i++)
	{
		bits[0] = MSG_ReadByte (&net_message);
		bits[1] = MSG_ReadByte (&net_message);
		bits[2] = MSG_ReadByte (&net_message);
		bits[3] = MSG_ReadByte (&net_message);
		bits[4] = MSG_ReadByte (&net_message);
		pr.origin[0] = ( ( bits[0] + ((bits[1]&15)<<8) ) <<1) - 4096;
		pr.origin[1] = ( ( (bits[1]>>4) + (bits[2]<<4) ) <<1) - 4096;
		pr.origin[2] = ( ( bits[3] + ((bits[4]&15)<<8) ) <<1) - 4096;
		VectorCopy(pr.origin, pr.oldorigin);

		if (bits[4] & 64)
			pr.effects = EF_BLASTER;
		else
			pr.effects = 0;

		if (bits[4] & 128) {
			old = true;
			bits[0] = MSG_ReadByte (&net_message);
			bits[1] = MSG_ReadByte (&net_message);
			bits[2] = MSG_ReadByte (&net_message);
			bits[3] = MSG_ReadByte (&net_message);
			bits[4] = MSG_ReadByte (&net_message);
			pr.oldorigin[0] = ( ( bits[0] + ((bits[1]&15)<<8) ) <<1) - 4096;
			pr.oldorigin[1] = ( ( (bits[1]>>4) + (bits[2]<<4) ) <<1) - 4096;
			pr.oldorigin[2] = ( ( bits[3] + ((bits[4]&15)<<8) ) <<1) - 4096;
		}

		bits[0] = MSG_ReadByte (&net_message);
		bits[1] = MSG_ReadByte (&net_message);
		bits[2] = MSG_ReadByte (&net_message);

		pr.angles[0] = 360*bits[0]/256;
		pr.angles[1] = 360*bits[1]/256;
		pr.modelindex = bits[2];

		b = MSG_ReadByte (&net_message);
		pr.num = (b & 0x7f);
		if (b & 128) // extra entity number byte
			pr.num |= (MSG_ReadByte (&net_message) << 7);

		pr.present = true;

		// find if this projectile already exists from previous frame 
		for (j = 0; j < MAX_PROJECTILES; j++) {
			if (cl_projectiles[j].modelindex) {
				if (cl_projectiles[j].num == pr.num) {
					// already present, set up oldorigin for interpolation
					if (!old)
						VectorCopy(cl_projectiles[j].origin, pr.oldorigin);
					cl_projectiles[j] = pr;
					break;
				}
			} else
				lastempty = j;
		}

		// not present previous frame, add it
		if (j == MAX_PROJECTILES) {
			if (lastempty != -1) {
				cl_projectiles[lastempty] = pr;
			}
		}
	}
}

/*
=============
CL_LinkProjectiles

=============
*/
void CL_AddProjectiles (void)
{
	int		i, j;
	projectile_t	*pr;
	entity_t		ent;

	memset (&ent, 0, sizeof(ent));

	for (i=0, pr=cl_projectiles ; i < MAX_PROJECTILES ; i++, pr++)
	{
		// grab an entity to fill in
		if (pr->modelindex < 1)
			continue;
		if (!pr->present) {
			pr->modelindex = 0;
			continue; // not present this frame (it was in the previous frame)
		}

		ent.model = cl.model_draw[pr->modelindex];

		// interpolate origin
		for (j=0 ; j<3 ; j++)
		{
			ent.origin[j] = ent.oldorigin[j] = pr->oldorigin[j] + cl.lerpfrac * 
				(pr->origin[j] - pr->oldorigin[j]);

		}

		if (pr->effects & EF_BLASTER)
			CL_BlasterTrail (pr->oldorigin, ent.origin);
		V_AddLight (pr->origin, 200, 1, 1, 0);

		VectorCopy (pr->angles, ent.angles);
		V_AddEntity (&ent);
	}
}
#endif

/*
=================
CL_ParseEntityBits

Returns the entity number and the header bits
=================
*/
//int	bitcounts[32];	/// just for protocol profiling
int CL_ParseEntityBits (uint32 *bits)
{
	uint32		b, total;
	uint32		number;

	total = MSG_ReadByte (&net_message);
	if (total & U_MOREBITS1)
	{
		b = MSG_ReadByte (&net_message);
		total |= b<<8;
	}
	if (total & U_MOREBITS2)
	{
		b = MSG_ReadByte (&net_message);
		total |= b<<16;
	}
	if (total & U_MOREBITS3)
	{
		b = MSG_ReadByte (&net_message);
		total |= b<<24;
	}

	// count the bits for net profiling
	/*for (i=0 ; i<32 ; i++)
		if (total&(1<<i))
			bitcounts[i]++;*/

	if (total & U_NUMBER16)
	{
		number = MSG_ReadShort (&net_message);
		if (number > MAX_EDICTS)
			Com_Error (ERR_DROP, "CL_ParseEntityBits: Bad entity number %u", number);
	}
	else
	{
		number = MSG_ReadByte (&net_message);
	}

	*bits = total;

	return number;
}

/*
==================
CL_ParseDelta

Can go from either a baseline or a previous packet_entity
==================
*/
void CL_ParseDelta (const entity_state_t *from, entity_state_t *to, int number, int bits)
{
	// set everything to the state we are delta'ing from
	*to = *from;

	if (cls.serverProtocol != ENHANCED_PROTOCOL_VERSION)
		VectorCopy (from->origin, to->old_origin);
	else if (!(bits & U_OLDORIGIN) && !(from->renderfx & RF_BEAM))
		VectorCopy (from->origin, to->old_origin);

	to->number = number;

	if (bits & U_MODEL)
		to->modelindex = MSG_ReadByte (&net_message);
	if (bits & U_MODEL2)
		to->modelindex2 = MSG_ReadByte (&net_message);
	if (bits & U_MODEL3)
		to->modelindex3 = MSG_ReadByte (&net_message);
	if (bits & U_MODEL4)
		to->modelindex4 = MSG_ReadByte (&net_message);
		
	if (bits & U_FRAME8)
		to->frame = MSG_ReadByte (&net_message);
	if (bits & U_FRAME16)
		to->frame = MSG_ReadShort (&net_message);

	if ((bits & U_SKIN8) && (bits & U_SKIN16))		//used for laser colors
		to->skinnum = MSG_ReadLong(&net_message);
	else if (bits & U_SKIN8)
		to->skinnum = MSG_ReadByte(&net_message);
	else if (bits & U_SKIN16)
		to->skinnum = MSG_ReadShort(&net_message);

	if ( (bits & (U_EFFECTS8|U_EFFECTS16)) == (U_EFFECTS8|U_EFFECTS16) )
		to->effects = MSG_ReadLong(&net_message);
	else if (bits & U_EFFECTS8)
		to->effects = MSG_ReadByte(&net_message);
	else if (bits & U_EFFECTS16)
		to->effects = MSG_ReadShort(&net_message);

	if ( (bits & (U_RENDERFX8|U_RENDERFX16)) == (U_RENDERFX8|U_RENDERFX16) )
		to->renderfx = MSG_ReadLong(&net_message);
	else if (bits & U_RENDERFX8)
		to->renderfx = MSG_ReadByte(&net_message);
	else if (bits & U_RENDERFX16)
		to->renderfx = MSG_ReadShort(&net_message);

	if (bits & U_ORIGIN1)
		to->origin[0] = MSG_ReadCoord (&net_message);
	if (bits & U_ORIGIN2)
		to->origin[1] = MSG_ReadCoord (&net_message);
	if (bits & U_ORIGIN3)
		to->origin[2] = MSG_ReadCoord (&net_message);
		
	if (bits & U_ANGLE1)
		to->angles[0] = MSG_ReadAngle(&net_message);
	if (bits & U_ANGLE2)
		to->angles[1] = MSG_ReadAngle(&net_message);
	if (bits & U_ANGLE3)
		to->angles[2] = MSG_ReadAngle(&net_message);

	if (bits & U_OLDORIGIN)
		MSG_ReadPos (&net_message, to->old_origin);

	//if (bits & U_COPYOLD)
	//	VectorCopy (to->origin, to->old_origin);

	if (bits & U_SOUND)
		to->sound = MSG_ReadByte (&net_message);

	if (bits & U_EVENT)
		to->event = MSG_ReadByte (&net_message);
	else
		to->event = 0;

	if (bits & U_SOLID)
		to->solid = MSG_ReadShort (&net_message);

	//if (cl.enhancedServer && bits & U_VELOCITY)
	//	MSG_ReadPos (&net_message, to->velocity);
}

/*
==================
CL_DeltaEntity

Parses deltas from the given base and adds the resulting entity
to the current frame
==================
*/
static void CL_DeltaEntity (frame_t *frame, int newnum, const entity_state_t *old, int bits)
{
	centity_t	*ent;
	entity_state_t	*state;

	ent = &cl_entities[newnum];

	state = &cl_parse_entities[cl.parse_entities & (MAX_PARSE_ENTITIES-1)];
	cl.parse_entities++;
	frame->num_entities++;

	CL_ParseDelta (old, state, newnum, bits);

	// some data changes will force no lerping
	if (state->modelindex != ent->current.modelindex
		|| state->modelindex2 != ent->current.modelindex2
		|| state->modelindex3 != ent->current.modelindex3
		|| state->modelindex4 != ent->current.modelindex4
		|| state->event == EV_PLAYER_TELEPORT
		|| state->event == EV_OTHER_TELEPORT
		|| abs((int)(state->origin[0] - ent->current.origin[0])) > 512
		|| abs((int)(state->origin[1] - ent->current.origin[1])) > 512
		|| abs((int)(state->origin[2] - ent->current.origin[2])) > 512
		)
	{
		ent->serverframe = -99;
	}

	if (ent->serverframe != cl.frame.serverframe - 1)
	{	// wasn't in last update, so initialize some things
		ent->trailcount = 1024;		// for diminishing rocket / grenade trails
		// duplicate the current state so lerping doesn't hurt anything
		ent->prev = *state;
		if (state->event == EV_OTHER_TELEPORT)
		{
			VectorCopy (state->origin, ent->prev.origin);
			VectorCopy (state->origin, ent->lerp_origin);
		}
		else
		{
			VectorCopy (state->old_origin, ent->prev.origin);
			VectorCopy (state->old_origin, ent->lerp_origin);
		}
	}
	else
	{	// shuffle the last state to previous
		//vec3_t	diff;
		//VectorSubtract (ent->current.origin, ent->prev.origin, diff);
		//VectorScale (diff, cl_predict->value, diff);
		//VectorAdd (state->origin, diff, state->origin);
		ent->prev = ent->current;
	}

	ent->serverframe = cl.frame.serverframe;
	ent->current = *state;

	//only update our 'baseline' if the server knows what it is too
	//if (cls.serverProtocol == ENHANCED_PROTOCOL_VERSION)
	//	memcpy (&cl_entities[newnum].baseline, state, sizeof(entity_state_t));
}

static void ShowBits (uint32 bits)
{
	if (cl_shownet->intvalue < 4 && cl_shownet->intvalue != -2)
		return;

	if (bits &	U_ORIGIN1)
		Com_Printf ("   ...U_ORIGIN1\n", LOG_CLIENT);

	if (bits & U_ORIGIN2)
		Com_Printf ("   ...U_ORIGIN2\n", LOG_CLIENT);

	if (bits & U_ANGLE2)
		Com_Printf ("   ...U_ANGLE2\n", LOG_CLIENT);

	if (bits & U_ANGLE3)
		Com_Printf ("   ...U_ANGLE3\n", LOG_CLIENT);

	if (bits & U_FRAME8)
		Com_Printf ("   ...U_FRAME8\n", LOG_CLIENT);

	if (bits & U_EVENT)
		Com_Printf ("   ...U_EVENT\n", LOG_CLIENT);

	if (bits & U_REMOVE)
		Com_Printf ("   ...U_REMOVE\n", LOG_CLIENT);

	if (bits & U_MOREBITS1)
		Com_Printf ("   ...U_MOREBITS1\n", LOG_CLIENT);

	if (bits & U_NUMBER16)
		Com_Printf ("   ...U_NUMBER16\n", LOG_CLIENT);

	if (bits & U_ORIGIN3)
		Com_Printf ("   ...U_ORIGIN3\n", LOG_CLIENT);

	if (bits & U_ANGLE1)
		Com_Printf ("   ...U_ANGLE1\n", LOG_CLIENT);

	if (bits & U_MODEL)
		Com_Printf ("   ...U_MODEL\n", LOG_CLIENT);

	if (bits & U_RENDERFX8)
		Com_Printf ("   ...U_RENDERFX8\n", LOG_CLIENT);

	if (bits & U_EFFECTS8)
		Com_Printf ("   ...U_EFFECTS8\n", LOG_CLIENT);

	if (bits & U_MOREBITS2)
		Com_Printf ("   ...U_MOREBITS2\n", LOG_CLIENT);

	if (bits & U_SKIN8)
		Com_Printf ("   ...U_SKIN8\n", LOG_CLIENT);

	if (bits & U_FRAME16)
		Com_Printf ("   ...U_FRAME16\n", LOG_CLIENT);

	if (bits & U_RENDERFX16)
		Com_Printf ("   ...U_RENDERFX16\n", LOG_CLIENT);

	if (bits & U_EFFECTS16)
		Com_Printf ("   ...U_EFFECTS16\n", LOG_CLIENT);

	if (bits & U_MODEL2)
		Com_Printf ("   ...U_MODEL2\n", LOG_CLIENT);

	if (bits & U_MODEL3)
		Com_Printf ("   ...U_MODEL3\n", LOG_CLIENT);

	if (bits & U_MODEL4)
		Com_Printf ("   ...U_MODEL4\n", LOG_CLIENT);

	if (bits & U_MOREBITS3)
		Com_Printf ("   ...U_MOREBITS3\n", LOG_CLIENT);

	if (bits & U_OLDORIGIN)
		Com_Printf ("   ...U_OLDORIGIN\n", LOG_CLIENT);

	if (bits & U_SKIN16)
		Com_Printf ("   ...U_SKIN16\n", LOG_CLIENT);

	if (bits & U_SOUND)
		Com_Printf ("   ...U_SOUND\n", LOG_CLIENT);

	if (bits & U_SOLID)
		Com_Printf ("   ...U_SOLID\n", LOG_CLIENT);

	//if (bits & U_VELOCITY)
	//	Com_Printf ("   ...U_VELOCITY\n", LOG_CLIENT);

	/*
	if (bits & U_COPYOLD)
		Com_Printf ("   ...U_COPYOLD\n", LOG_CLIENT);
	*/
}

typedef struct unpacker_s
{
	int		num_bits_remaining;
	int		next_bit_to_read;
	byte	buffer[MAX_EDICTS * sizeof(uint16)];	//worst case
} unpacker_t;

uint32 BP_unpack (unpacker_t *unpacker, uint32 num_bits_to_unpack)
{
	uint32 result = 0;

	Q_assert(num_bits_to_unpack <= unpacker->num_bits_remaining);

	while (num_bits_to_unpack)
	{
		uint32 byte_index = (unpacker->next_bit_to_read / 8);
		uint32 bit_index = (unpacker->next_bit_to_read % 8);

		uint32 src_mask = (1 << (7 - bit_index));
		uint32 dest_mask = (1 << (num_bits_to_unpack - 1));

		if (unpacker->buffer[byte_index] & src_mask)
			result |= dest_mask;

		num_bits_to_unpack--;
		unpacker->next_bit_to_read++;
	}

	unpacker->num_bits_remaining -= num_bits_to_unpack;

	return result;
}

/*
==================
CL_ParsePacketEntities

An svc_packetentities has just been parsed, deal with the
rest of the data stream.
==================
*/
static void CL_ParsePacketEntities (const frame_t *oldframe, frame_t *newframe)
{
	int			newnum;
	uint32			bits;
	entity_state_t	*oldstate;
	int			oldindex, oldnum;

	newframe->parse_entities = cl.parse_entities;
	newframe->num_entities = 0;

	// delta from the entities present in oldframe
	oldindex = 0;
	if (!oldframe)
		oldnum = 99999;
	else
	{
		if (oldindex >= oldframe->num_entities)
			oldnum = 99999;
		else
		{
			oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
			oldnum = oldstate->number;
		}
	}

	for (;;)
	{
		newnum = CL_ParseEntityBits (&bits);
		if (newnum >= MAX_EDICTS)
			Com_Error (ERR_DROP,"CL_ParsePacketEntities: bad number:%i", newnum);

		if (net_message.readcount > net_message.cursize)
			Com_Error (ERR_DROP,"CL_ParsePacketEntities: end of message");

		if (!newnum)
			break;

		if (cl_shownet->intvalue >= 4)
			Com_Printf ("%i bytes.\n", LOG_CLIENT, net_message.readcount-1);

		while (oldnum < newnum)
		{	// one or more entities from the old packet are unchanged
			if (cl_shownet->intvalue >= 3)
				Com_Printf ("   unchanged: %i\n", LOG_CLIENT, oldnum);
			CL_DeltaEntity (newframe, oldnum, oldstate, 0);
			
			oldindex++;

			if (oldindex >= oldframe->num_entities)
				oldnum = 99999;
			else
			{
				oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
				oldnum = oldstate->number;
			}
		}

		if (bits & U_REMOVE)
		{	
			// the entity present in oldframe is not in the current frame
			if (!oldframe)
				Com_Error (ERR_DROP, "CL_ParsePacketEntities: U_REMOVE with no oldframe");

			if (cl_shownet->intvalue >= 3)
				Com_Printf ("   remove: %i\n", LOG_CLIENT, newnum);

			if (oldnum != newnum)
				Com_DPrintf ("U_REMOVE: oldnum != newnum\n");

			//reset the baseline.
			//cl_entities[newnum].baseline.number = 0;

			oldindex++;

			if (oldindex >= oldframe->num_entities)
				oldnum = 99999;
			else
			{
				oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
				oldnum = oldstate->number;
			}
			continue;
		}

		if (oldnum == newnum)
		{	// delta from previous state
			if (!oldframe)
				Com_Error (ERR_DROP, "CL_ParsePacketEntities: delta with no oldframe");

			if (cl_shownet->intvalue >= 3)
				Com_Printf ("   delta: %i\n", LOG_CLIENT, newnum);
			CL_DeltaEntity (newframe, newnum, oldstate, bits);

			oldindex++;

			if (oldindex >= oldframe->num_entities)
				oldnum = 99999;
			else
			{
				oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
				oldnum = oldstate->number;
			}
			continue;
		}

		if (oldnum > newnum)
		{	// delta from baseline
			if (cl_shownet->intvalue >= 3)
				Com_Printf ("   baseline: %i\n", LOG_CLIENT, newnum);
			ShowBits (bits);

			CL_DeltaEntity (newframe, newnum, &cl_entities[newnum].baseline, bits);

			continue;
		}

	}

	//suck up removed entities from packed bits in new protocol
	/*if (cls.serverProtocol == ENHANCED_PROTOCOL_VERSION && oldframe)
	{
		unpacker_t	unpacker;
		int			byteCount;
		int			maxEntNum;

		maxEntNum = cl_parse_entities[(oldframe->parse_entities + oldframe->num_entities-1) & (MAX_PARSE_ENTITIES-1)].number + 1;

		if (maxEntNum < 256)
		{
			for (;;)
			{
				bits = MSG_ReadByte (&net_message);
				if (!bits)
					break;
			}
		}
		else
		{
			memset (&unpacker, 0, sizeof(unpacker));

			byteCount = MSG_ReadByte (&net_message);

			MSG_ReadData (&net_message, unpacker.buffer, byteCount);
			unpacker.num_bits_remaining = byteCount * 8;
			for (;;)
			{
				bits = BP_unpack (&unpacker, 10);
			}
		}
	}*/

	// any remaining entities in the old frame are copied over
	while (oldnum != 99999)
	{	
		// one or more entities from the old packet are unchanged
		if (cl_shownet->intvalue >= 3)
			Com_Printf ("   unchanged: %i\n", LOG_CLIENT, oldnum);
		CL_DeltaEntity (newframe, oldnum, oldstate, 0);
		
		oldindex++;

		if (oldindex >= oldframe->num_entities)
			oldnum = 99999;
		else
		{
			oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
			oldnum = oldstate->number;
		}
	}
}



/*
===================
CL_ParsePlayerstate
===================
*/
static void CL_ParsePlayerstate (const frame_t *oldframe, frame_t *newframe, int extraflags)
{
	int			flags;
	player_state_new	*state;
	int			i;
	int			statbits;
	qboolean	enhanced;

	state = &newframe->playerstate;
	enhanced = (cls.serverProtocol == ENHANCED_PROTOCOL_VERSION);

	// clear to old value before delta parsing
	if (oldframe)
		*state = oldframe->playerstate;
	else
		memset (state, 0, sizeof(*state));

	flags = MSG_ReadShort (&net_message);

	//
	// parse the pmove_state_t
	//
	if (flags & PS_M_TYPE)
		state->pmove.pm_type = MSG_ReadByte (&net_message);

	if (flags & PS_M_ORIGIN)
	{
		if (!enhanced)
			extraflags |= EPS_PMOVE_ORIGIN2;
		state->pmove.origin[0] = MSG_ReadShort (&net_message);
		state->pmove.origin[1] = MSG_ReadShort (&net_message);
	}

	if (extraflags & EPS_PMOVE_ORIGIN2)
		state->pmove.origin[2] = MSG_ReadShort (&net_message);

	if (flags & PS_M_VELOCITY)
	{
		if (!enhanced)
			extraflags |= EPS_PMOVE_VELOCITY2;
		state->pmove.velocity[0] = MSG_ReadShort (&net_message);
		state->pmove.velocity[1] = MSG_ReadShort (&net_message);
	}

	if (extraflags & EPS_PMOVE_VELOCITY2)
		state->pmove.velocity[2] = MSG_ReadShort (&net_message);

	if (flags & PS_M_TIME)
		state->pmove.pm_time = MSG_ReadByte (&net_message);

	if (flags & PS_M_FLAGS)
		state->pmove.pm_flags = MSG_ReadByte (&net_message);

	if (flags & PS_M_GRAVITY)
		state->pmove.gravity = MSG_ReadShort (&net_message);

	if (flags & PS_M_DELTA_ANGLES)
	{
		state->pmove.delta_angles[0] = MSG_ReadShort (&net_message);
		state->pmove.delta_angles[1] = MSG_ReadShort (&net_message);
		state->pmove.delta_angles[2] = MSG_ReadShort (&net_message);
	}

	if (cl.attractloop)
		state->pmove.pm_type = PM_FREEZE;		// demo playback

	//
	// parse the rest of the player_state_t
	//
	if (flags & PS_VIEWOFFSET)
	{
		state->viewoffset[0] = MSG_ReadChar (&net_message) * 0.25f;
		state->viewoffset[1] = MSG_ReadChar (&net_message) * 0.25f;
		state->viewoffset[2] = MSG_ReadChar (&net_message) * 0.25f;
	}

	if (flags & PS_VIEWANGLES)
	{
		if (!enhanced)
			extraflags |= EPS_VIEWANGLE2;
		state->viewangles[0] = MSG_ReadAngle16 (&net_message);
		state->viewangles[1] = MSG_ReadAngle16 (&net_message);
	}

	if (extraflags & EPS_VIEWANGLE2)
		state->viewangles[2] = MSG_ReadAngle16 (&net_message);

	if (flags & PS_KICKANGLES)
	{
		state->kick_angles[0] = MSG_ReadChar (&net_message) * 0.25f;
		state->kick_angles[1] = MSG_ReadChar (&net_message) * 0.25f;
		state->kick_angles[2] = MSG_ReadChar (&net_message) * 0.25f;
	}

	if (flags & PS_WEAPONINDEX)
	{
		state->gunindex = MSG_ReadByte (&net_message);
	}

	if (flags & PS_WEAPONFRAME)
	{
		if (!enhanced)
			extraflags |= EPS_GUNOFFSET|EPS_GUNANGLES;
		state->gunframe = MSG_ReadByte (&net_message);
	}

	if (extraflags & EPS_GUNOFFSET)
	{
		state->gunoffset[0] = MSG_ReadChar (&net_message)*0.25f;
		state->gunoffset[1] = MSG_ReadChar (&net_message)*0.25f;
		state->gunoffset[2] = MSG_ReadChar (&net_message)*0.25f;
	}

	if (extraflags & EPS_GUNANGLES)
	{
		state->gunangles[0] = MSG_ReadChar (&net_message)*0.25f;
		state->gunangles[1] = MSG_ReadChar (&net_message)*0.25f;
		state->gunangles[2] = MSG_ReadChar (&net_message)*0.25f;
	}

	if (flags & PS_BLEND)
	{
		state->blend[0] = MSG_ReadByte (&net_message)/255.0f;
		state->blend[1] = MSG_ReadByte (&net_message)/255.0f;
		state->blend[2] = MSG_ReadByte (&net_message)/255.0f;
		state->blend[3] = MSG_ReadByte (&net_message)/255.0f;
	}

	if (flags & PS_FOV)
		state->fov = (float)MSG_ReadByte (&net_message);

	if (flags & PS_RDFLAGS)
		state->rdflags = MSG_ReadByte (&net_message);

	//r1q2 extensions
	if (enhanced)
	{
		if (flags & PS_BBOX)
		{
			int x, zd, zu;
			int solid;

			solid = MSG_ReadShort (&net_message);

			x = 8*(solid & 31);
			zd = 8*((solid>>5) & 31);
			zu = 8*((solid>>10) & 63) - 32;

			state->mins[0] = state->mins[1] = -(float)x;
			state->maxs[0] = state->maxs[1] = (float)x;
			state->mins[2] = -(float)zd;
			state->maxs[2] = (float)zu;
			Com_Printf ("received bbox from server: (%f, %f, %f), (%f, %f, %f)\n", LOG_CLIENT, state->mins[0], state->mins[1], state->mins[2], state->maxs[0], state->maxs[1], state->maxs[2]);
		}
	}

	if (!enhanced)
		extraflags |= EPS_STATS;

	// parse stats
	if (extraflags & EPS_STATS)
	{
	statbits = MSG_ReadLong (&net_message);

		if (statbits)
		{
	for (i=0 ; i<MAX_STATS ; i++)
		if (statbits & (1<<i) )
			state->stats[i] = MSG_ReadShort(&net_message);
}
	}
}


/*
==================
CL_FireEntityEvents

==================
*/
static void CL_FireEntityEvents (const frame_t *frame)
{
	entity_state_t		*s1;
	int					pnum, num;

	for (pnum = 0 ; pnum<frame->num_entities ; pnum++)
	{
		num = (frame->parse_entities + pnum)&(MAX_PARSE_ENTITIES-1);
		s1 = &cl_parse_entities[num];
		if (s1->event)
			CL_EntityEvent (s1);

		if (s1->effects & EF_TELEPORTER)
			CL_TeleporterParticles (s1);

	}
}

/*
================
CL_ParseFrame
================
*/
void CL_ParseFrame (int extrabits)
{
	byte		cmd;
	int			len;
	int			extraflags;
	uint32		serverframe;
	frame_t		*old;

	//memset (&cl.frame, 0xCC, sizeof(cl.frame));

#if 0
	CL_ClearProjectiles(); // clear projectiles for new frame
#endif

	//HACK: we steal last bits of this int for the offset
	//if serverframe gets that high then the server has been on the same map
	//for over 19 days... how often will this legitimately happen, and do we
	//really need the possibility of the server running same map for 13 years...
	serverframe = MSG_ReadLong (&net_message);

	if (cls.serverProtocol != ENHANCED_PROTOCOL_VERSION)
	{
		cl.frame.serverframe = serverframe;
	cl.frame.deltaframe = MSG_ReadLong (&net_message);
	}
	else
	{
		uint32	offset;
		
		offset = serverframe & 0xF8000000;
		offset >>= 27;
		
		serverframe &= 0x07FFFFFF;

		cl.frame.serverframe = serverframe;

		if (offset == 31)
			cl.frame.deltaframe = -1;
		else
			cl.frame.deltaframe = serverframe - offset;
	}

	if (cls.state != ca_active)
		cl.frame.servertime = 0;
	else
		cl.frame.servertime = (cl.frame.serverframe - cl.initial_server_frame) *100; //r1: fix for precision loss with high serverframes

	//HACK UGLY SHIT
	//moving the extrabits from cmd over so that the 4 that come from extraflags (surpressCount) don't conflict
	extraflags = extrabits >> 1;

	// BIG HACK to let old demos continue to work
	if (cls.serverProtocol != 26)
	{
		byte	data;
		data = MSG_ReadByte (&net_message);

		//r1: HACK to get extra 4 bits of otherwise unused data
		if (cls.serverProtocol == ENHANCED_PROTOCOL_VERSION)
		{
			cl.surpressCount = (data & 0x0F);
			extraflags |= (data & 0xF0) >> 4;
		}
		else
		{
			cl.surpressCount = data;
		}
	}

	if (cl_shownet->intvalue >= 3)
		Com_Printf ("   frame:%i  delta:%i\n", LOG_CLIENT, cl.frame.serverframe,
		cl.frame.deltaframe);

	// If the frame is delta compressed from data that we
	// no longer have available, we must suck up the rest of
	// the frame, but not use it, then ask for a non-compressed
	// message 
	if (cl.frame.deltaframe <= 0)
	{
		cl.frame.valid = true;		// uncompressed frame
		old = NULL;
		cls.demowaiting = false;	// we can start recording now
	}
	else
	{
		old = &cl.frames[cl.frame.deltaframe & UPDATE_MASK];
		if (!old->valid)
		{	// should never happen
			Com_Printf ("Delta from invalid frame (not supposed to happen!).\n", LOG_CLIENT);
		}
		if (old->serverframe != cl.frame.deltaframe)
		{	// The frame that the server did the delta from
			// is too old, so we can't reconstruct it properly.
			Com_DPrintf ("Delta frame too old.\n");
		}
		else if (cl.parse_entities - old->parse_entities > MAX_PARSE_ENTITIES-128)
		{
			Com_DPrintf ("Delta parse_entities too old.\n");
		}
		else
			cl.frame.valid = true;	// valid delta parse
	}

	// clamp time 
	if (cl.time > cl.frame.servertime)
		cl.time = cl.frame.servertime;
	else if (cl.time < cl.frame.servertime - 100)
		cl.time = cl.frame.servertime - 100;

	// read areabits
	len = MSG_ReadByte (&net_message);
	MSG_ReadData (&net_message, &cl.frame.areabits, len);

	// read playerinfo
	if (cls.serverProtocol != ENHANCED_PROTOCOL_VERSION)
	{
	cmd = MSG_ReadByte (&net_message);
	SHOWNET(svc_strings[cmd]);
	if (cmd != svc_playerinfo)
		Com_Error (ERR_DROP, "CL_ParseFrame: 0x%.2x not playerinfo", cmd);
	}
	CL_ParsePlayerstate (old, &cl.frame, extraflags);

	// read packet entities
	if (cls.serverProtocol != ENHANCED_PROTOCOL_VERSION)
	{
	cmd = MSG_ReadByte (&net_message);
	SHOWNET(svc_strings[cmd]);
	if (cmd != svc_packetentities)
		Com_Error (ERR_DROP, "CL_ParseFrame: 0x%.2x not packetentities", cmd);
	}
	CL_ParsePacketEntities (old, &cl.frame);

#if 0
	if (cmd == svc_packetentities2)
		CL_ParseProjectiles();
#endif

	// save the frame off in the backup array for later delta comparisons
	cl.frames[cl.frame.serverframe & UPDATE_MASK] = cl.frame;

	if (cl.frame.valid)
	{
		// getting a valid frame message ends the connection process
		if (cls.state != ca_active)
		{
			cls.state = ca_active;

			//r1: fix for precision loss with high serverframes (when map runs for over several hours)
			cl.initial_server_frame = cl.frame.serverframe;
			cl.frame.servertime = (cl.frame.serverframe - cl.initial_server_frame) * 100;

			cl.force_refdef = true;
			cl.predicted_origin[0] = cl.frame.playerstate.pmove.origin[0]*0.125f;
			cl.predicted_origin[1] = cl.frame.playerstate.pmove.origin[1]*0.125f;
			cl.predicted_origin[2] = cl.frame.playerstate.pmove.origin[2]*0.125f;
			VectorCopy (cl.frame.playerstate.viewangles, cl.predicted_angles);
			if (cls.disable_servercount != cl.servercount
				&& cl.refresh_prepped)
				SCR_EndLoadingPlaque ();	// get rid of loading plaque

			cl.sound_prepped = true;	// can start mixing ambient sounds
		}
	
		// fire entity events
		CL_FireEntityEvents (&cl.frame);

		//r1: save function call
		if (!(!cl_predict->intvalue || (cl.frame.playerstate.pmove.pm_flags & PMF_NO_PREDICTION)))
			CL_CheckPredictionError ();
	}
}

/*
==========================================================================

INTERPOLATE BETWEEN FRAMES TO GET RENDERING PARMS

==========================================================================
*/

/*struct model_s *S_RegisterSexedModel (entity_state_t *ent, char *base)
{
	int				n;
	char			*p;
	struct model_s	*mdl;
	char			model[MAX_QPATH];
	char			buffer[MAX_QPATH];

	// determine what model the client is using
	model[0] = 0;
	//n = CS_PLAYERSKINS + ent->number - 1;
	n = cl.playernum + CS_PLAYERSKINS;
	if (cl.configstrings[n][0])
	{
		p = strchr(cl.configstrings[n], '\\');
		if (p)
		{
			p += 1;
			strcpy(model, p);
			p = strchr(model, '/');
			if (p)
				*p = 0;
		}
	}
	// if we can't figure it out, they're male
	if (!model[0])
		strcpy(model, "male");

	Com_sprintf (buffer, sizeof(buffer), "players/%s/%s", model, base+1);
	mdl = re.RegisterModel(buffer);
	if (!mdl) {
		// not found, try default weapon model
		Com_sprintf (buffer, sizeof(buffer), "players/%s/weapon.md2", model);
		mdl = re.RegisterModel(buffer);
		if (!mdl) {
			// no, revert to the male model
			Com_sprintf (buffer, sizeof(buffer), "players/%s/%s", "male", base+1);
			mdl = re.RegisterModel(buffer);
			if (!mdl) {
				// last try, default male weapon.md2
				Com_sprintf (buffer, sizeof(buffer), "players/male/weapon.md2");
				mdl = re.RegisterModel(buffer);
			}
		} 
	}

	return mdl;
}*/

/*
===============
CL_AddPacketEntities

===============
*/
static void CL_AddPacketEntities (const frame_t *frame)
{
	entity_t			ent = {0};
	const entity_state_t	*s1;
	float				autorotate;
	int					i;
	int					pnum;
	centity_t			*cent;
	int					autoanim;
	const clientinfo_t		*ci;
	uint32					effects, renderfx;
	float				time;

	time = (float)cl.time;

	// bonus items rotate at a fixed rate
	autorotate = anglemod(time*0.1f);

	// brush models can auto animate their frames
	autoanim = 2*cl.time/1000;

	//memset (&ent, 0, sizeof(ent));

	for (pnum = 0 ; pnum<frame->num_entities ; pnum++)
	{
		s1 = &cl_parse_entities[(frame->parse_entities+pnum)&(MAX_PARSE_ENTITIES-1)];

		cent = &cl_entities[s1->number];

		effects = s1->effects;
		renderfx = s1->renderfx;

		// set frame
		if (effects & EF_ANIM01)
			ent.frame = autoanim & 1;
		else if (effects & EF_ANIM23)
			ent.frame = 2 + (autoanim & 1);
		else if (effects & EF_ANIM_ALL)
			ent.frame = autoanim;
		else if (effects & EF_ANIM_ALLFAST)
			ent.frame = cl.time / 100;
		else
			ent.frame = s1->frame;

		// quad and pent can do different things on client
		if (effects & EF_PENT)
		{
			effects &= ~EF_PENT;
			effects |= EF_COLOR_SHELL;
			renderfx |= RF_SHELL_RED;
		}

		if (effects & EF_QUAD)
		{
			effects &= ~EF_QUAD;
			effects |= EF_COLOR_SHELL;
			renderfx |= RF_SHELL_BLUE;
		}
//======
// PMM
		if (effects & EF_DOUBLE)
		{
			effects &= ~EF_DOUBLE;
			effects |= EF_COLOR_SHELL;
			renderfx |= RF_SHELL_DOUBLE;
		}

		if (effects & EF_HALF_DAMAGE)
		{
			effects &= ~EF_HALF_DAMAGE;
			effects |= EF_COLOR_SHELL;
			renderfx |= RF_SHELL_HALF_DAM;
		}
// pmm
//======
		ent.oldframe = cent->prev.frame;
		ent.backlerp = 1.0f - cl.lerpfrac;

#if 0
		if (effects & EF_ANIM_ALL) {
			//ent.oldframe = ent.frame;
			//r1: updated to become useful. loops between 0 and ent->s.framenum
			//ent.frame = ;

			ent.frame = ent.oldframe = (cl.time/500) % (s1->frame + 1);
			//set previous frame (fix lerping)
			/*if ( != ent.oldframe) {
				ent.oldframe = (cl.time/500) % (s1->frame + 1) - 1;
				ent.frame = (cl.time/500) % (s1->frame + 1);
				Com_Printf ("old %d new %d\n", ent.oldframe, ent.frame);
			}*/
			//	else
			//		ent.oldframe = ((cl.time/500) % (s1->frame + 1)) - 1;
			//} else {
			//	ent.oldframe = 0;
			//}*/
			
		} else if (effects & EF_ANIM_ALLFAST) {
			//r1: updated to become useful. loops between 0 and ent->s.framenum
			ent.frame = (cl.time/100) % (s1->frame + 1);

			//set previous frame (fix lerping)
			if (ent.frame)
				ent.oldframe = ((cl.time/100) % (s1->frame + 1)) - 1;
			else
				ent.oldframe = 0;
		}
#endif

		if (renderfx & (RF_FRAMELERP|RF_BEAM))
		{	// step origin discretely, because the frames
			// do the animation properly

			//r1: beam lerp fix
			if (renderfx & RF_BEAM)
			{
				ent.oldorigin[0] = cent->prev.old_origin[0] + cl.lerpfrac * (cent->current.old_origin[0] - cent->prev.old_origin[0]);
				ent.oldorigin[1] = cent->prev.old_origin[1] + cl.lerpfrac * (cent->current.old_origin[1] - cent->prev.old_origin[1]);
				ent.oldorigin[2] = cent->prev.old_origin[2] + cl.lerpfrac * (cent->current.old_origin[2] - cent->prev.old_origin[2]);

				ent.origin[0] = cent->prev.origin[0] + cl.lerpfrac * (cent->current.origin[0] - cent->prev.origin[0]);
				ent.origin[1] = cent->prev.origin[1] + cl.lerpfrac * (cent->current.origin[1] - cent->prev.origin[1]);
				ent.origin[2] = cent->prev.origin[2] + cl.lerpfrac * (cent->current.origin[2] - cent->prev.origin[2]);
			}
			else
			{
			VectorCopy (cent->current.origin, ent.origin);
			VectorCopy (cent->current.old_origin, ent.oldorigin);
		}
		}
		else
		{	
#ifdef _DEBUG
			for (i=0 ; i<3 ; i++)
			{
				//cent->prev.origin[i] = cent->current.origin[i];
			
				ent.origin[i] = ent.oldorigin[i] = cent->prev.origin[i] + cl.lerpfrac * 
					(cent->current.origin[i] - cent->prev.origin[i]);
			}
			//VectorCopy (cent->current.origin, cent->prev.origin);
#else

			for (i=0 ; i<3 ; i++)
			{
				ent.origin[i] = ent.oldorigin[i] = cent->prev.origin[i] + cl.lerpfrac * 
					(cent->current.origin[i] - cent->prev.origin[i]);
			}
#endif

			//Com_Printf ("lerpfrac = %f\n", cl.lerpfrac);
		}

		// create a new entity
	
		// tweak the color of beams
		if ( renderfx & RF_BEAM )
		{	// the four beam colors are encoded in 32 bits of skinnum (hack)
			ent.alpha = 0.30f;
			ent.skinnum = (s1->skinnum >> ((randomMT() % 4)*8)) & 0xff;
			ent.model = NULL;
		}
		else
		{
			// set skin
			if (s1->modelindex == 255)
			{	// use custom player skin
				ent.skinnum = 0;
				ci = &cl.clientinfo[s1->skinnum & 0xff];
				ent.skin = ci->skin;
				ent.model = ci->model;
				if (!ent.skin || !ent.model)
				{
					ent.skin = cl.baseclientinfo.skin;
					ent.model = cl.baseclientinfo.model;
				}
//============
//PGM
				if (renderfx & RF_USE_DISGUISE)
				{
					if(!strncmp((char *)ent.skin, "players/male", 12))
					{
						ent.skin = re.RegisterSkin ("players/male/disguise.pcx");
						ent.model = re.RegisterModel ("players/male/tris.md2");
					}
					else if(!strncmp((char *)ent.skin, "players/female", 14))
					{
						ent.skin = re.RegisterSkin ("players/female/disguise.pcx");
						ent.model = re.RegisterModel ("players/female/tris.md2");
					}
					else if(!strncmp((char *)ent.skin, "players/cyborg", 14))
					{
						ent.skin = re.RegisterSkin ("players/cyborg/disguise.pcx");
						ent.model = re.RegisterModel ("players/cyborg/tris.md2");
					}
				}
//PGM
//============
			}
			else
			{
				ent.skinnum = s1->skinnum;
				ent.skin = NULL;
				ent.model = cl.model_draw[s1->modelindex];
			}
		}

		// only used for black hole model right now, FIXME: do better

		// r1: was ==, why?
		if (renderfx & RF_TRANSLUCENT && !(renderfx & RF_BEAM))
			ent.alpha = 0.70f;

		// render effects (fullbright, translucent, etc)
		if ((effects & EF_COLOR_SHELL))
			ent.flags = 0;	// renderfx go on color shell entity
		else
			ent.flags = renderfx;

		// calculate angles
		if (effects & EF_ROTATE)
		{	// some bonus items auto-rotate
			ent.angles[0] = 0;
			ent.angles[1] = autorotate;
			ent.angles[2] = 0;
		}
		// RAFAEL
		else if (effects & EF_SPINNINGLIGHTS)
		{
			ent.angles[0] = 0;
			ent.angles[1] = anglemod(time/2) + s1->angles[1];
			ent.angles[2] = 180;
			{
				vec3_t forward;
				vec3_t start;

				AngleVectors (ent.angles, forward, NULL, NULL);
				VectorMA (ent.origin, 64, forward, start);
				V_AddLight (start, 100, 1, 0, 0);
			}
		}
		else
		{	// interpolate angles
			float	a1, a2;

			for (i=0 ; i<3 ; i++)
			{
				a1 = cent->current.angles[i];
				a2 = cent->prev.angles[i];
				ent.angles[i] = LerpAngle (a2, a1, cl.lerpfrac);
			}
		}

		if (s1->number == cl.playernum+1)
		{
			ent.flags |= RF_VIEWERMODEL;	// only draw from mirrors
			// FIXME: still pass to refresh

			if (effects & EF_FLAG1)
				V_AddLight (ent.origin, 225, 1.0, 0.1f, 0.1f);
			else if (effects & EF_FLAG2)
				V_AddLight (ent.origin, 225, 0.1f, 0.1f, 1.0);
			else if (effects & EF_TAGTRAIL)						//PGM
				V_AddLight (ent.origin, 225, 1.0, 1.0, 0.0f);	//PGM
			else if (effects & EF_TRACKERTRAIL)					//PGM
				V_AddLight (ent.origin, 225, -1.0f, -1.0f, -1.0f);	//PGM

			continue;
		}

		// if set to invisible, skip
		if (!s1->modelindex)
			continue;

		if (effects & EF_BFG)
		{
			ent.flags |= RF_TRANSLUCENT;
			ent.alpha = 0.30f;
		}

		// RAFAEL
		if (effects & EF_PLASMA)
		{
			ent.flags |= RF_TRANSLUCENT;
			ent.alpha = 0.6f;
		}

		if (effects & EF_SPHERETRANS)
		{
			ent.flags |= RF_TRANSLUCENT;
			// PMM - *sigh*  yet more EF overloading
			if (effects & EF_TRACKERTRAIL)
				ent.alpha = 0.6f;
			else
				ent.alpha = 0.3f;
		}

		//Com_Printf ("%d %d:", s1->modelindex, s1->number);

		// add to refresh list
		V_AddEntity (&ent);

		// color shells generate a seperate entity for the main model
		if (effects & EF_COLOR_SHELL)
		{
			ent.flags = renderfx | RF_TRANSLUCENT;
			ent.alpha = 0.30f;
			V_AddEntity (&ent);
		}

		ent.skin = NULL;		// never use a custom skin on others
		ent.skinnum = 0;
		ent.flags = 0;
		ent.alpha = 0;

		// duplicate for linked models
		if (s1->modelindex2)
		{
			if (s1->modelindex2 == 255)
			{	// custom weapon
				ci = &cl.clientinfo[s1->skinnum & 0xff];
				i = (s1->skinnum >> 8); // 0 is default weapon model
				if (!cl_vwep->intvalue || i > MAX_CLIENTWEAPONMODELS - 1)
					i = 0;
				ent.model = ci->weaponmodel[i];
				if (!ent.model) {
					if (i != 0)
						ent.model = ci->weaponmodel[0];
					if (!ent.model)
						ent.model = cl.baseclientinfo.weaponmodel[0];
				}
			}
			else
				ent.model = cl.model_draw[s1->modelindex2];

			// PMM - check for the defender sphere shell .. make it translucent
			// replaces the previous version which used the high bit on modelindex2 to determine transparency
			if (!Q_stricmp (cl.configstrings[CS_MODELS+(s1->modelindex2)], "models/items/shell/tris.md2"))
			{
				ent.alpha = 0.32f;
				ent.flags = RF_TRANSLUCENT;
			}
			// pmm

			V_AddEntity (&ent);

			//PGM - make sure these get reset.
			ent.flags = 0;
			ent.alpha = 0;
			//PGM
		}
		if (s1->modelindex3)
		{
			ent.model = cl.model_draw[s1->modelindex3];
			V_AddEntity (&ent);
		}
		if (s1->modelindex4)
		{
			ent.model = cl.model_draw[s1->modelindex4];
			V_AddEntity (&ent);
		}

		if ( effects & EF_POWERSCREEN )
		{
			ent.model = cl_mod_powerscreen;
			ent.oldframe = 0;
			ent.frame = 0;
			ent.flags |= (RF_TRANSLUCENT | RF_SHELL_GREEN);
			ent.alpha = 0.30f;
			V_AddEntity (&ent);
		}

		//r1: moved teleporter here so effect doesn't stop on ploss
		//Com_Printf ("ft = %f\n", cls.realtime/10.0);

		// add automatic particle trails
		if ( (effects&~EF_ROTATE) )
		{
			if (effects & EF_ROCKET)
			{
				CL_RocketTrail (cent->lerp_origin, ent.origin, cent);
				V_AddLight (ent.origin, 200, 1, 0.23f, 0);
			}
			// PGM - Do not reorder EF_BLASTER and EF_HYPERBLASTER. 
			// EF_BLASTER | EF_TRACKER is a special case for EF_BLASTER2... Cheese!
			else if (effects & EF_BLASTER)
			{
//				CL_BlasterTrail (cent->lerp_origin, ent.origin);
//PGM
				if (effects & EF_TRACKER)	// lame... problematic?
				{
					CL_BlasterTrail2 (cent->lerp_origin, ent.origin);
					V_AddLight (ent.origin, 200, 0, 1, 0);		
				}
				else
				{
					CL_BlasterTrail (cent->lerp_origin, ent.origin);
					V_AddLight (ent.origin, 200, 1, 1, 0);
				}
//PGM
			}
			else if (effects & EF_HYPERBLASTER)
			{
				if (effects & EF_TRACKER)						// PGM	overloaded for blaster2.
					V_AddLight (ent.origin, 200, 0, 1, 0);		// PGM
				else											// PGM
					V_AddLight (ent.origin, 250, 1, 1, 1);
			}
			else if (effects & EF_GIB)
			{
				CL_DiminishingTrail (cent->lerp_origin, ent.origin, cent, effects);
			}
			else if (effects & EF_GRENADE)
			{
				CL_DiminishingTrail (cent->lerp_origin, ent.origin, cent, effects);
			}
			else if (effects & EF_FLIES)
			{
				CL_FlyEffect (cent, ent.origin);
			}
			else if (effects & EF_BFG)
			{
				float j;
				static float bfg_lightramp[6] = {300.0, 400.0, 600.0, 300.0, 150.0, 75.0};

				if (effects & EF_ANIM_ALLFAST)
				{
					CL_BfgParticles (&ent);
					j = 200.0;
				}
				else
				{
					//r1: protect against access violation
					uint32 frameindex;

					frameindex = s1->frame;
					if (frameindex > 6)
						frameindex = 6;

					j = bfg_lightramp[frameindex];
				}
				V_AddLight (ent.origin, j, 0, 1, 0);
			}
			// RAFAEL
			else if (effects & EF_TRAP)
			{
				float j;
				ent.origin[2] += 32;
				CL_TrapParticles (&ent);
				j = (float)((randomMT()%100) + 100);
				V_AddLight (ent.origin, j, 1, 0.8f, 0.1f);
			}
			else if (effects & EF_FLAG1)
			{
				CL_FlagTrail (cent->lerp_origin, ent.origin, 242);
				V_AddLight (ent.origin, 225, 1, 0.1f, 0.1f);
			}
			else if (effects & EF_FLAG2)
			{
				CL_FlagTrail (cent->lerp_origin, ent.origin, 115);
				V_AddLight (ent.origin, 225, 0.1f, 0.1f, 1);
			}
//======
//ROGUE
			else if (effects & EF_TAGTRAIL)
			{
				CL_TagTrail (cent->lerp_origin, ent.origin, 220);
				V_AddLight (ent.origin, 225, 1.0, 1.0, 0.0);
			}
			else if (effects & EF_TRACKERTRAIL)
			{
				if (effects & EF_TRACKER)
				{
					float intensity;

					intensity = 50 + (500 * ((float)sin(time/500.0f) + 1.0f));
					// FIXME - check out this effect in rendition
					if(vidref_val == VIDREF_GL)
						V_AddLight (ent.origin, intensity, -1.0, -1.0, -1.0);
					else
						V_AddLight (ent.origin, -1.0f * intensity, 1.0, 1.0, 1.0);
					}
				else
				{
					CL_Tracker_Shell (cent->lerp_origin);
					V_AddLight (ent.origin, 155, -1.0, -1.0, -1.0);
				}
			}
			else if (effects & EF_TRACKER)
			{
				CL_TrackerTrail (cent->lerp_origin, ent.origin, 0);
				// FIXME - check out this effect in rendition
				if(vidref_val == VIDREF_GL)
					V_AddLight (ent.origin, 200, -1, -1, -1);
				else
					V_AddLight (ent.origin, -200, 1, 1, 1);
			}
//ROGUE
//======
			// RAFAEL
			else if (effects & EF_GREENGIB)
			{
				CL_DiminishingTrail (cent->lerp_origin, ent.origin, cent, effects);				
			}
			// RAFAEL
			else if (effects & EF_IONRIPPER)
			{
				CL_IonripperTrail (cent->lerp_origin, ent.origin);
				V_AddLight (ent.origin, 100, 1, 0.5, 0.5);
			}
			// RAFAEL
			else if (effects & EF_BLUEHYPERBLASTER)
			{
				V_AddLight (ent.origin, 200, 0, 0, 1);
			}
			// RAFAEL
			else if (effects & EF_PLASMA)
			{
				if (effects & EF_ANIM_ALLFAST)
				{
					CL_BlasterTrail (cent->lerp_origin, ent.origin);
				}
				V_AddLight (ent.origin, 130, 1, 0.5, 0.5);
			}
		}

		VectorCopy (ent.origin, cent->lerp_origin);
	}
}



/*
==============
CL_AddViewWeapon
==============
*/
static void CL_AddViewWeapon (const player_state_new *ps, const player_state_new *ops)
{
	entity_t	gun = {0};		// view model
	int			i;

	// allow the gun to be completely removed
	if (!cl_gun->intvalue)
		return;

	// don't draw gun if in wide angle view
	if (ps->fov > 90)
		return;

	//memset (&gun, 0, sizeof(gun));

	if (gun_model)
		gun.model = gun_model;	// development tool
	else
		gun.model = cl.model_draw[ps->gunindex];

	if (!gun.model)
		return;

	// set up gun position
	for (i=0 ; i<3 ; i++)
	{
		gun.origin[i] = cl.refdef.vieworg[i] + ops->gunoffset[i]
			+ cl.lerpfrac * (ps->gunoffset[i] - ops->gunoffset[i]);
		gun.angles[i] = cl.refdef.viewangles[i] + LerpAngle (ops->gunangles[i],
			ps->gunangles[i], cl.lerpfrac);
	}

	if (gun_frame)
	{
		gun.frame = gun_frame;	// development tool
		gun.oldframe = gun_frame;	// development tool
	}
	else
	{
		gun.frame = ps->gunframe;
		if (gun.frame == 0)
			gun.oldframe = 0;	// just changed weapons, don't lerp from old
		else
			gun.oldframe = ops->gunframe;
	}

	gun.flags = RF_MINLIGHT | RF_DEPTHHACK | RF_WEAPONMODEL;
	gun.backlerp = 1.0f - cl.lerpfrac;
	VectorCopy (gun.origin, gun.oldorigin);	// don't lerp at all
	V_AddEntity (&gun);
}

extern int keydown[];

/*
===============
CL_CalcViewValues

Sets cl.refdef view values
===============
*/
static void CL_CalcViewValues (void)
{
	int			i;
	float		lerp, backlerp;
//	centity_t	*ent;
	const frame_t		*oldframe;
	const player_state_new	*ps, *ops;

	// find the previous frame to interpolate from
	ps = &cl.frame.playerstate;
	i = (cl.frame.serverframe - 1) & UPDATE_MASK;
	oldframe = &cl.frames[i];
	if (oldframe->serverframe != cl.frame.serverframe-1 || !oldframe->valid)
		oldframe = &cl.frame;		// previous frame was dropped or invalid
	ops = &oldframe->playerstate;

	// see if the player entity was teleported this frame
	if ( fabs(ops->pmove.origin[0] - ps->pmove.origin[0]) > 256*8
		|| abs(ops->pmove.origin[1] - ps->pmove.origin[1]) > 256*8
		|| abs(ops->pmove.origin[2] - ps->pmove.origin[2]) > 256*8)
		ops = ps;		// don't interpolate

	//ent = &cl_entities[cl.playernum+1];
	if (cl_nolerp->intvalue)
		lerp = 1.0;
	else
		lerp = cl.lerpfrac;

	// calculate the origin
	if ((cl_predict->intvalue) && !(cl.frame.playerstate.pmove.pm_flags & PMF_NO_PREDICTION))
	{	// use predicted values
		uint32	delta;

		if (cl_backlerp->intvalue)
			backlerp = 1.0f - lerp;
		else
			backlerp = 1.0;

		for (i=0 ; i<3 ; i++)
		{
			cl.refdef.vieworg[i] = cl.predicted_origin[i] + ops->viewoffset[i] 
				+ cl.lerpfrac * (ps->viewoffset[i] - ops->viewoffset[i])
				- backlerp * cl.prediction_error[i];
		}

		// smooth out stair climbing
		delta = cls.realtime - cl.predicted_step_time;
		if (delta < 100)
		{
#ifdef _DEBUG
			//Com_Printf ("delta = %d\n", delta);
#endif
			cl.refdef.vieworg[2] -= cl.predicted_step * (100 - delta) * 0.01f;
		}
	}
	else
	{	// just use interpolated values
		for (i=0 ; i<3 ; i++)
			cl.refdef.vieworg[i] = ops->pmove.origin[i]*0.125f + ops->viewoffset[i] 
				+ lerp * (ps->pmove.origin[i]*0.125f + ps->viewoffset[i] 
				- (ops->pmove.origin[i]*0.125f + ops->viewoffset[i]) );
	}

	// if not running a demo or on a locked frame, add the local angle movement
	if (Com_ServerState() == ss_demo)
	{
		if (!keydown[K_SHIFT])
		{
			cl.refdef.viewangles[0] = LerpAngle (ops->viewangles[0], ps->viewangles[0], lerp);
			cl.refdef.viewangles[1] = LerpAngle (ops->viewangles[1], ps->viewangles[1], lerp);
			cl.refdef.viewangles[2] = LerpAngle (ops->viewangles[2], ps->viewangles[2], lerp);
		}
		else
		{
			VectorCopy (cl.predicted_angles, cl.refdef.viewangles);
		}
	}
	else if ( cl.frame.playerstate.pmove.pm_type < PM_DEAD)
	{	// use predicted values
		VectorCopy (cl.predicted_angles, cl.refdef.viewangles);
	}
	else
	{	// just use interpolated values
		if (cl.frame.playerstate.pmove.pm_type >= PM_DEAD && ops->pmove.pm_type < PM_DEAD)
		{
			//r1: fix for server no longer sending viewangles every frame.
			cl.refdef.viewangles[0] = LerpAngle (cl.predicted_angles[0], ps->viewangles[0], lerp);
			cl.refdef.viewangles[1] = LerpAngle (cl.predicted_angles[1], ps->viewangles[1], lerp);
			cl.refdef.viewangles[2] = LerpAngle (cl.predicted_angles[2], ps->viewangles[2], lerp);
		}
		else
		{
		cl.refdef.viewangles[0] = LerpAngle (ops->viewangles[0], ps->viewangles[0], lerp);
		cl.refdef.viewangles[1] = LerpAngle (ops->viewangles[1], ps->viewangles[1], lerp);
		cl.refdef.viewangles[2] = LerpAngle (ops->viewangles[2], ps->viewangles[2], lerp);
	}
	}

	cl.refdef.viewangles[0] += LerpAngle (ops->kick_angles[0], ps->kick_angles[0], lerp);
	cl.refdef.viewangles[1] += LerpAngle (ops->kick_angles[1], ps->kick_angles[1], lerp);
	cl.refdef.viewangles[2] += LerpAngle (ops->kick_angles[2], ps->kick_angles[2], lerp);

	AngleVectors (cl.refdef.viewangles, cl.v_forward, cl.v_right, cl.v_up);

	// interpolate field of view
	cl.refdef.fov_x = ops->fov + lerp * (ps->fov - ops->fov);

	// don't interpolate blend color
	cl.refdef.blend[0] = ps->blend[0];
	cl.refdef.blend[1] = ps->blend[1];
	cl.refdef.blend[2] = ps->blend[2];
	cl.refdef.blend[3] = ps->blend[3];

	// add the weapon
	CL_AddViewWeapon (ps, ops);
}

void CL_AddLocalEnts (void);

extern cvar_t *cl_lents;

/*
===============
CL_AddEntities

Emits all entities, particles, and lights to the refresh
===============
*/
void CL_AddEntities (void)
{
	//entity_t *currententity;

	//int i;
	if (cls.state != ca_active)
		return;

	if (cl.time > cl.frame.servertime)
	{
		if (cl_showclamp->intvalue)
			Com_Printf ("high clamp %i\n", LOG_CLIENT, cl.time - cl.frame.servertime);
		cl.time = cl.frame.servertime;
		cl.lerpfrac = 1.0;
	}
	else if (cl.time < cl.frame.servertime - 100)
	{
		if (cl_showclamp->intvalue)
			Com_Printf ("low clamp %i\n", LOG_CLIENT, cl.frame.servertime-100 - cl.time);
		cl.time = cl.frame.servertime - 100;
		cl.lerpfrac = 0;
	}
	else
		cl.lerpfrac = 1.0f - (cl.frame.servertime - cl.time) * 0.01f;

	if (cl_timedemo->intvalue)
		cl.lerpfrac = 1.0;

//	CL_AddPacketEntities (&cl.frame);
//	CL_AddTEnts ();
//	CL_AddParticles ();
//	CL_AddDLights ();
//	CL_AddLightStyles ();

	CL_CalcViewValues ();
	// PMM - moved this here so the heat beam has the right values for the vieworg, and can lock the beam to the gun
	CL_AddPacketEntities (&cl.frame);
#if 0
	CL_AddProjectiles ();
#endif
	if (cl_lents->intvalue)
		CL_AddLocalEnts ();
	CL_AddTEnts ();
	CL_AddParticles ();
	CL_AddDLights ();
	CL_AddLightStyles ();
}



/*
===============
CL_GetEntityOrigin

Called to get the sound spatialization origin
===============
*/
void CL_GetEntityOrigin (int ent, vec3_t origin)
{
	/*centity_t	*old;

	if (ent < 0 || ent >= MAX_EDICTS)
		Com_Error (ERR_DROP, "CL_GetEntityOrigin: bad ent");

	if (ent == (cl.playernum+1))
	{
		VectorCopy (cl.refdef.vieworg, org);
	}
	else
	{
		old = &cl_entities[ent];
		VectorCopy (old->lerp_origin, org);
	}

	// FIXME: bmodel issues...*/
	centity_t	*cent;
	cmodel_t	*cmodel;
	vec3_t		midPoint;

	if (ent < 0 || ent >= MAX_EDICTS)
		Com_Error(ERR_DROP, "CL_GetEntityOrigin: ent = %i", ent);

	// Player entity
	if (ent == cl.playernum + 1)
	{
		VectorCopy (cl.refdef.vieworg, origin);
		return;
	}

	cent = &cl_entities[ent];

	if (cent->current.renderfx & (RF_FRAMELERP|RF_BEAM))
	{
		// Calculate origin
		origin[0] = cent->current.old_origin[0] + (cent->current.origin[0] - cent->current.old_origin[0]) * cl.lerpfrac;
		origin[1] = cent->current.old_origin[1] + (cent->current.origin[1] - cent->current.old_origin[1]) * cl.lerpfrac;
		origin[2] = cent->current.old_origin[2] + (cent->current.origin[2] - cent->current.old_origin[2]) * cl.lerpfrac;
	}
	else
	{
		// Calculate origin
		origin[0] = cent->prev.origin[0] + (cent->current.origin[0] - cent->prev.origin[0]) * cl.lerpfrac;
		origin[1] = cent->prev.origin[1] + (cent->current.origin[1] - cent->prev.origin[1]) * cl.lerpfrac;
		origin[2] = cent->prev.origin[2] + (cent->current.origin[2] - cent->prev.origin[2]) * cl.lerpfrac;
	}

	// If a brush model, offset the origin
	if (cent->current.solid == 31)
	{
		cmodel = cl.model_clip[cent->current.modelindex];
		if (!cmodel)
			return;

		VectorAverage(cmodel->mins, cmodel->maxs, midPoint);
		VectorAdd(origin, midPoint, origin);
	}
}
