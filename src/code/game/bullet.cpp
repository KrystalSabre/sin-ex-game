//-----------------------------------------------------------------------------
//
//  $Logfile:: /Quake 2 Engine/Sin/code/game/bullet.cpp                       $
// $Revision:: 71                                                             $
//   $Author:: Markd                                                          $
//     $Date:: 10/22/98 7:57p                                                 $
//
// Copyright (C) 2020 by Night Dive Studios, Inc.
// All rights reserved.
//
// See the license.txt file for conditions and terms of use for this code.
//
// DESCRIPTION:
// Base class for all bullet (hitscan) weapons.  Includes definition for shotgun.
// 

#include "g_local.h"
#include "specialfx.h"
#include "misc.h"
#include "bullet.h"
#include "q_shared.h"
#include "surface.h"
#include "player.h"

CLASS_DECLARATION(Weapon, BulletWeapon, NULL);

ResponseDef BulletWeapon::Responses[] =
{
   {NULL, NULL}
};

BulletWeapon::BulletWeapon()
{
   modelIndex("sprites/tracer.spr");
   modelIndex("sprites/bullethole.spr");
}

void BulletWeapon::TraceAttack(Vector start, Vector end, int damage, trace_t *trace, int numricochets, int kick, int dflags, int meansofdeath, 
                               qboolean server_effects)
{
   Vector		org;
   Vector		dir;
   Vector		endpos;
   int			surfflags;
   int			surftype;
   int			timeofs;
   Entity		*ent;
   qboolean    ricochet;

   if(HitSky(trace))
   {
      return;
   }

   ricochet = false;
   dir = end - start;
   dir.normalize();

   org = end - dir;

   ent = trace->ent->entity;

   if(!trace->surface)
   {
      surfflags = 0;
      surftype = 0;
   }
   else
   {
      surfflags = trace->surface->flags;
      surftype = SURFACETYPE_FROM_FLAGS(surfflags);
      surfaceManager.DamageSurface(trace, damage, owner);

      if(surfflags & SURF_RICOCHET)
         ricochet = true;
   }
   if(trace->intersect.valid && ent)
   {
      //
      // see if the parent group has ricochet turned on
      //
      if(trace->intersect.parentgroup >= 0)
      {
         int flags;

         flags = gi.Group_Flags(ent->edict->s.modelindex, trace->intersect.parentgroup);
         if(flags & MDL_GROUP_RICOCHET)
         {
            surftype = (flags >> 8) & 0xf;
            ricochet = true;
         }
      }
   }

   if(ent)
   {
      if(!(ent->flags & FL_SHIELDS))
      {
         // Take care of ricochet effects on the server
         if(server_effects && !ricochet)
         {
            timeofs = MAX_RICOCHETS - numricochets;
            if(timeofs > 0xf)
            {
               timeofs = 0xf;
            }
            gi.WriteByte(svc_temp_entity);
            gi.WriteByte(TE_GUNSHOT);
            gi.WritePosition(org.vec3());
            gi.WriteDir(trace->plane.normal);
            gi.WriteByte(0);
            gi.multicast(org.vec3(), MULTICAST_PVS);
         }

         if(ent->flags & FL_SPARKS)
         {
            MadeBreakingSound(org, owner);
         }

         if(ent->takedamage)
         {
            if(trace->intersect.valid)
            {
               // We hit a valid group so send in location based damage
               ent->Damage(this,
                  owner,
                  damage,
                  trace->endpos,
                  dir,
                  trace->plane.normal,
                  kick,
                  dflags,
                  meansofdeath,
                  trace->intersect.parentgroup,
                  -1,
                  trace->intersect.damage_multiplier);
            }
            else
            {
               // We didn't hit any groups, so send in generic damage
               ent->Damage(this,
                  owner,
                  damage,
                  trace->endpos,
                  dir,
                  trace->plane.normal,
                  kick,
                  dflags,
                  meansofdeath,
                  -1,
                  -1,
                  1);
            }
         }
      }
      else
      {
         surftype = SURF_TYPE_METAL;
         ricochet = true;
      }
   }

   if(ricochet && (server_effects || (numricochets < MAX_RICOCHETS)))
   {
      timeofs = MAX_RICOCHETS - numricochets;
      if(timeofs > 0xf)
      {
         timeofs = 0xf;
      }
      gi.WriteByte(svc_temp_entity);
      gi.WriteByte(TE_GUNSHOT);
      gi.WritePosition(org.vec3());
      gi.WriteDir(trace->plane.normal);
      gi.WriteByte(timeofs);
      gi.multicast(org.vec3(), MULTICAST_PVS);
   }

   if(ricochet &&
      numricochets &&
      damage)
   {
      dir += Vector(trace->plane.normal) * 2;
      endpos = org + dir * 8192;

      //
      // since this is a ricochet, we don't ignore the wewapon owner this time.
      //
      *trace = G_FullTrace(org, vec_zero, vec_zero, endpos, 5, NULL, MASK_SHOT, "BulletWeapon::TraceAttack");
      if(trace->fraction != 1.0)
      {
         endpos = trace->endpos;
         TraceAttack(org, endpos, damage * 0.8f, trace, numricochets - 1, kick, dflags, meansofdeath, true);
      }
   }
}

void BulletWeapon::FireBullets(int numbullets, Vector spread, int mindamage, int maxdamage, int dflags, int meansofdeath, qboolean server_effects)
{
   Vector	src;
   Vector	dir;
   Vector	end;
   trace_t	trace;
   Vector	right;
   Vector	up;
   int		i;
   int      action_per_bullet;
   int      action_count;
   int      action_max;
   qboolean hitenemy;

   assert(owner);
   if(!owner)
   {
      return;
   }

   GetMuzzlePosition(&src, &dir);

   owner->angles.AngleVectors(NULL, &right, &up);

   angles = dir.toAngles();
   setAngles(angles);

   action_per_bullet = action_level_increment;
   action_count = 0;
   action_max = 1;
   hitenemy = false;
   if(numbullets > 1)
   {
      action_max = numbullets * 0.6;
      action_per_bullet /= action_max;
   }

   for(i = 0; i < numbullets; i++)
   {
      end = src +
         dir   * 8192 +
         right * G_CRandom() * spread.x +
         up    * G_CRandom() * spread.y;

      trace = G_FullTrace(src, vec_zero, vec_zero, end, 5, owner, MASK_SHOT, "BulletWeapon::FireBullets");
#if 0
      Com_Printf("Server OWNER  Angles:%0.2f %0.2f %0.2f\n", owner->angles[0], owner->angles[1], owner->angles[2]);
      Com_Printf("Server Bullet Angles:%0.2f %0.2f %0.2f\n", angles[0], angles[1], angles[2]);
      Com_Printf("Right               :%0.2f %0.2f %0.2f\n", right[0], right[1], right[2]);
      Com_Printf("Up                  :%0.2f %0.2f %0.2f\n", up[0], up[1], up[2]);
      Com_Printf("Direction           :%0.2f %0.2f %0.2f\n", dir[0], dir[1], dir[2]);
      Com_Printf("Endpoint            :%0.2f %0.2f %0.2f\n", end[0], end[1], end[2]);
      Com_Printf("Server Trace Start  :%0.2f %0.2f %0.2f\n", src[0], src[1], src[2]);
      Com_Printf("Server Trace End    :%0.2f %0.2f %0.2f\n", trace.endpos[0], trace.endpos[1], trace.endpos[2]);
      Com_Printf("\n");
#endif
      if(server_effects >= 2)
         FireTracer(trace.endpos);

      if(trace.fraction != 1.0)
      {
         if(owner->isClient() && action_count < action_max && trace.ent->entity != owner && trace.ent->entity->isSubclassOf<Sentient>() && !(trace.ent->entity->deadflag == DEAD_DEAD || !hitenemy && trace.ent->entity->deadflag == DEAD_DYING) && !(trace.ent->entity->flags & (FL_FORCEFIELD | FL_GODMODE)))
         {
            Player *client = (Player *)(Entity *)owner;
            client->IncreaseActionLevel(action_per_bullet);
            action_count++;
            hitenemy = true;
         }
         TraceAttack(src, trace.endpos, mindamage + (int)G_Random(maxdamage - mindamage + 1), &trace, MAX_RICOCHETS, kick, dflags, meansofdeath, server_effects);
      }
   }
}

void BulletWeapon::FireTracer(Vector end)
{
   Entity   *tracer;
   Vector   src, dir;
   Player   *client;
   //int      savedhand;
   //trace_t  trace;
   if(G_Random(1.0) >= 0.7)
      return;

   /*client = (Player *)(Entity *)owner;
   if(owner->isClient() && client->ViewMode() == THIRD_PERSON)
   {
      savedhand = owner->client->pers.hand;
      owner->client->pers.hand = RIGHT_HANDED;
      GetMuzzlePosition(&src, &dir);
      owner->client->pers.hand = savedhand;
   }
   else*/
      GetMuzzlePosition(&src, &dir);
   //end = src + dir * 8192;
   //trace = G_Trace(src, vec_zero, vec_zero, end, owner, MASK_SHOT, "BulletWeapon::FireTracer");
   Vector tempangles, forward;
   dir = end - src;
   velocity = forward * 100;
   tracer = new Entity();

   tracer->angles = dir.toAngles();
   tracer->angles[PITCH] = -tracer->angles[PITCH] + 90;
   //tracer->angles[PITCH] *= -1;

   tracer->setAngles(tracer->angles);
   tempangles = dir.toAngles();
   tempangles[PITCH] *= -1;
   AngleVectors(tempangles.vec3(), forward.vec3(), NULL, NULL);

   tracer->setMoveType(MOVETYPE_FLY);
   tracer->setSolidType(SOLID_NOT);
   tracer->setModel("sprites/tracer.spr");
   tracer->setSize({ 0, 0, 0 }, { 0, 0, 0 });
   tracer->velocity = forward * 1500;
   tracer->setOrigin(src);
   tracer->edict->s.renderfx &= ~RF_FRAMELERP;
   tracer->edict->s.renderfx |= RF_DETAIL;
   tracer->edict->clipmask = 0x80000000;
   tracer->flags |= FL_DONTSAVE;
   /*if(owner->isClient())
   {
      tracer->edict->owner = owner->edict;
      tracer->edict->svflags |= SVF_ONLYPARENT;
   }*/

   VectorCopy(src, tracer->edict->s.old_origin);
   tracer->PostEvent(EV_Remove, floor(dir.length() / 150) / 10);
}

// EOF

