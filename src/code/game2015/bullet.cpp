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
#include "hoverbike.h" //###

CLASS_DECLARATION(Weapon, BulletWeapon, NULL);

ResponseDef BulletWeapon::Responses[] =
{
   {NULL, NULL}
};

BulletWeapon::BulletWeapon() : Weapon()
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
   trace_t     trace2;

   if(HitSky(trace))
   {
      return;
   }

   ricochet = false;
   dir = end - start;
   dir.normalize();

   org = end - dir;

   ent = trace->ent->entity;

   if(!trace->intersect.valid && ent)
   {
      trace2 = G_FullTrace(end, vec_zero, vec_zero, (end + dir * 80), 5, owner, MASK_SHOT, "BulletWeapon::TraceAttack");
      if(trace2.intersect.valid && trace2.ent == trace->ent)
      {
         trace = &trace2;
      }
      else if(ent->isClient())
      {
         float pos = (end.z - ent->absmin.z) / (ent->absmax.z - ent->absmin.z);
         if(pos >= 0.6)
         {
            trace->intersect.parentgroup = -2;
            trace->intersect.damage_multiplier = 1;
         }
         else if(pos >= 0.45)
         {
            trace->intersect.parentgroup = -3;
            trace->intersect.damage_multiplier = 1;
         }
         else if(pos >= 0.30)
         {
            trace->intersect.parentgroup = -4;
            trace->intersect.damage_multiplier = 0.5;
         }
         else
         {
            trace->intersect.parentgroup = -5;
            trace->intersect.damage_multiplier = 0.3;
         }
      }
   }

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
         if(ent->edict->solid == SOLID_BSP)
         {
            // Take care of ricochet effects on the server
            if(server_effects && !ricochet)
            {
               if(ent == world)
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

                  int i;
                  for(i = 1; i <= maxclients->value; i++)
                  {
                     if(g_edicts[i].inuse && g_edicts[i].client)
                     {
                        Player *player;
                        player = (Player *)g_edicts[i].entity;

                        if(player->InCameraPVS(org))
                        {
                           gi.WriteByte(svc_temp_entity);
                           gi.WriteByte(TE_GUNSHOT);
                           gi.WritePosition(org.vec3());
                           gi.WriteDir(trace->plane.normal);
                           gi.WriteByte(0);
                           gi.unicast(player->edict, false);
                        }
                     }
                  }
               }
               else
               {
                  Particles(trace->endpos, trace->plane.normal, 8, 120, 0);
               }

               switch(surfflags & MASK_SURF_TYPE)
               {
               //case SURF_TYPE_METAL:
               //case SURF_TYPE_GRILL:
               case SURF_TYPE_MONITOR:
               //case SURF_TYPE_DUCT:
                  SpawnSparks(trace->endpos, trace->plane.normal, 4);
                  break;
               case SURF_TYPE_FLESH:
                  SpawnBlood(trace->endpos, trace->plane.normal, 12);
                  break;
               default:
                  break;
               }
            }
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
                  damage*0.85,
                  trace->endpos,
                  dir,
                  trace->plane.normal,
                  kick,
                  dflags,
                  meansofdeath,
                  (trace->intersect.parentgroup < -1 ? trace->intersect.parentgroup : -1),
                  -1,
                  (trace->intersect.parentgroup < -1 ? trace->intersect.damage_multiplier : 1));
            }
         }
      }
      else
      {
         surftype = SURF_TYPE_METAL;
         SpawnSparks(trace->endpos, trace->plane.normal, 4);
         ent->RandomGlobalSound("ricochet_metal", 1, CHAN_BODY);
         ricochet = true;
      }
   }

   if(ent->edict->solid == SOLID_BSP && ricochet && (server_effects || (numricochets < MAX_RICOCHETS)))
   {
      if(ent == world)
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

         int i;
         for(i = 1; i <= maxclients->value; i++)
         {
            if(g_edicts[i].inuse && g_edicts[i].client)
            {
               Player *player;
               player = (Player *)g_edicts[i].entity;

               if(player->InCameraPVS(org))
               {
                  gi.WriteByte(svc_temp_entity);
                  gi.WriteByte(TE_GUNSHOT);
                  gi.WritePosition(org.vec3());
                  gi.WriteDir(trace->plane.normal);
                  gi.WriteByte(0);
                  gi.unicast(player->edict, false);
               }
            }
         }
      }
      else
      {
         Particles(trace->endpos, trace->plane.normal, 8, 120, 0);
      }

      switch(surfflags & MASK_SURF_TYPE)
      {
      //case SURF_TYPE_METAL:
      //case SURF_TYPE_GRILL:
      case SURF_TYPE_MONITOR:
      //case SURF_TYPE_DUCT:
         SpawnSparks(trace->endpos, trace->plane.normal, 4);
         break;
      case SURF_TYPE_FLESH:
         SpawnBlood(trace->endpos, trace->plane.normal, 12);
         break;
      default:
         break;
      }
   }

   if(ricochet &&
      numricochets &&
      damage)
   {
      dir += Vector(trace->plane.normal) * 2;
      endpos = org + dir * 8192;

      //
      // since this is a ricochet, we don't ignore the weapon owner this time.
      //
      //###
      //if(DM_FLAG(DF_BBOX_BULLETS))
      if(deathmatch->value)
      {
         *trace = G_Trace(org, vec_zero, vec_zero, endpos, NULL, MASK_SHOT, "BulletWeapon::TraceAttack");
      }
      else
      {
         *trace = G_FullTrace(org, vec_zero, vec_zero, endpos, 5, NULL, MASK_SHOT, "BulletWeapon::TraceAttack");
      }

      FireTracer(trace->endpos, org, false);

      if(trace->fraction != 1.0)
      {
         endpos = trace->endpos;
         TraceAttack(org, endpos, damage * 0.8f, trace, numricochets - 1, kick, dflags, meansofdeath, 2);
      }
      //###
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
      
      //### first need to do a regular trace to check for hitting a hoverbike
      trace = G_Trace(src, vec_zero, vec_zero, end, owner, MASK_SHOT, "BulletWeapon::FireBullets");

      if(trace.fraction != 1)
      {
         Entity *hit;

         hit = trace.ent->entity;

         if(hit->isSubclassOf<Hoverbike>() || hit->isSubclassOf<HoverbikeBox>())
         {
            trace_t trace2;

            // also do a short full trace to see if we're hitting a player on a hoverbike
            end = trace.endpos + dir * 80;
            trace2 = G_FullTrace(Vector(trace.endpos), vec_zero, vec_zero, end, 5, owner, MASK_SHOT, "BulletWeapon::FireBullets");
            if(trace2.fraction != 1)
            {
               Entity *hit2;

               hit2 = trace2.ent->entity;
               if(hit2->takedamage && hit2->isClient())
               {
                  if(server_effects >= 2 && !(silenced && (owner->flags & FL_SILENCER)))
                     FireTracer(trace2.endpos, vec_zero, (server_effects == 3 ? true : false));

                  if(owner->isClient() && action_count < action_max && hit2 != owner && !(hit2->deadflag == DEAD_DEAD || !hitenemy && hit2->deadflag == DEAD_DYING) && !(hit2->flags & (FL_FORCEFIELD | FL_GODMODE)))
                  {
                     Player *client = (Player *)(Entity *)owner;
                     client->IncreaseActionLevel(action_per_bullet);
                     action_count++;
                     hitenemy = true;
                  }
                  // probably traced to the rider, so hit him instead
                  hit2->Damage(this, owner, mindamage + (int)G_Random(maxdamage - mindamage + 1),
                               trace.endpos, dir, trace.plane.normal, kick, dflags, meansofdeath,
                               trace.intersect.parentgroup, -1, trace.intersect.damage_multiplier);
                  return;
               }
            }
            
            if(server_effects >= 2 && !(silenced && (owner->flags & FL_SILENCER)))
               FireTracer(trace.endpos, vec_zero, (server_effects == 3 ? true : false));

            hit->Damage(this, owner, mindamage + (int)G_Random(maxdamage - mindamage + 1), trace.endpos, dir, trace.plane.normal, kick, dflags, meansofdeath, -1, -1, 1);

            return; // hit something already, so don't do a regular full trace
         }
      }

      //if(!damagedtarget && DM_FLAG(DF_BBOX_BULLETS))
      if(!damagedtarget && deathmatch->value)
      {
         trace = G_Trace(src, vec_zero, vec_zero, end, owner, MASK_SHOT, "BulletWeapon::FireBullets");

         if(server_effects >= 2 && !(silenced && (owner->flags & FL_SILENCER)))
            FireTracer(trace.endpos, vec_zero, (server_effects == 3 ? true : false));

         if(trace.fraction != 1.0)
         {
            if(owner->isClient() && action_count < action_max && trace.ent->entity != owner && trace.ent->entity->isSubclassOf<Sentient>() && !(trace.ent->entity->deadflag == DEAD_DEAD || !hitenemy && trace.ent->entity->deadflag == DEAD_DYING) && !(trace.ent->entity->flags & (FL_FORCEFIELD | FL_GODMODE)))
            {
               Player *client = (Player *)(Entity *)owner;
               client->IncreaseActionLevel(action_per_bullet);
               action_count++;
               hitenemy = true;
            }
            // do less than regular damage on a bbox hit
            TraceAttack(src, trace.endpos, mindamage + (int)G_Random(maxdamage - mindamage + 1), &trace, 
                        MAX_RICOCHETS, kick, dflags, meansofdeath, server_effects);
         }
      }
      else
      { //###
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
         if(server_effects >= 2 && !(silenced && (owner->flags & FL_SILENCER)))
            FireTracer(trace.endpos, vec_zero, (server_effects == 3 ? true : false));

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
      } // ###
   }
}

void BulletWeapon::FireTracer(Vector end, Vector start, qboolean instant)
{
   Entity   *tracer;
   Vector   src, dir;
   Player   *client;
   int      savedhand;
   //trace_t  trace;

   if(G_NearEntityLimit())
      return;

   if(!instant && start == vec_zero && G_Random(1.0) > 0.7)
      return;

   client = (Player *)(Entity *)owner;
   if(start != vec_zero)
      src = start;
   else if(owner->isClient() && client->ViewMode() == THIRD_PERSON)
   {
      savedhand = owner->client->pers.hand;
      owner->client->pers.hand = RIGHT_HANDED;
      GetMuzzlePosition(&src, &dir);
      owner->client->pers.hand = savedhand;
   }
   else
      GetMuzzlePosition(&src, &dir);
   //end = src + dir * 8192;
   //trace = G_Trace(src, vec_zero, vec_zero, end, owner, MASK_SHOT, "BulletWeapon::FireTracer");
   Vector tempangles, forward;
   dir = end - src;
   if(dir.length() < 150)
      return;
   //velocity = forward * 100;
   tracer = new Entity();

   tracer->angles = dir.toAngles();
   if(owner->isClient() || instant)
      tracer->angles[PITCH] = -tracer->angles[PITCH] + 90;
   else
      tracer->angles[PITCH] *= -1;

   tracer->setAngles(tracer->angles);

   if(owner->isClient())
   {
      tempangles = dir.toAngles();
      tempangles[PITCH] *= -1;
      AngleVectors(tempangles.vec3(), forward.vec3(), NULL, NULL);
      tracer->setModel("sprites/tracer.spr");
      tracer->setOrigin(src);
      tracer->setMoveType(MOVETYPE_FLY);
      tracer->velocity = forward * 1500;
      tracer->PostEvent(EV_Remove, min(floor(dir.length() / (1500 * FRAMETIME)) * FRAMETIME, 2.0));
   }
   else
   {
      if(instant)
      {
         tracer->setModel("sprites/tracer.spr");
         tracer->setOrigin(end);
      }
      else
      {
         tracer->setModel("models/tracer.def");
         tracer->setOrigin(src);
      }
      tracer->setMoveType(MOVETYPE_NONE);
      tracer->PostEvent(EV_Remove, 0.1f);
   }

   tracer->setSolidType(SOLID_NOT);
   tracer->setSize({ 0, 0, 0 }, { 0, 0, 0 });
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
}

// EOF

