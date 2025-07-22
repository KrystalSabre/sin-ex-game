//-----------------------------------------------------------------------------
//
//  $Logfile:: /Quake 2 Engine/Sin/code/game/bullet.h                         $
// $Revision:: 22                                                             $
//   $Author:: Markd                                                          $
//     $Date:: 2/26/99 5:54p                                                  $
//
// Copyright (C) 2020 by Night Dive Studios, Inc.
// All rights reserved.
//
// See the license.txt file for conditions and terms of use for this code.
//
// DESCRIPTION:
// Base class for all bullet (hitscan) weapons.  Includes definition for shotgun.
// 

#ifndef __BULLET_H__
#define __BULLET_H__

#include "g_local.h"
#include "item.h"
#include "weapon.h"

#define MAX_RICOCHETS 10

class EXPORT_FROM_DLL BulletWeapon : public Weapon
{
protected:
   qboolean damagedtarget; //###

   virtual void TraceAttack(Vector start, Vector end, int damage, trace_t *trace, int numricochets, 
                            int kick, int dflags, int meansofdeath, qboolean server_effects);
   virtual void FireTracer(Vector end, Vector start = vec_zero);

public:
   CLASS_PROTOTYPE(BulletWeapon);

   BulletWeapon();
   virtual void FireBullets(int numbullets, Vector spread, int mindamage, int maxdamage, int dflags,
                            int meansofdeath, qboolean server_effects);
   //###
   virtual void Archive(Archiver &arc)   override;
   virtual void Unarchive(Archiver &arc) override;
   //###
};

//###
inline EXPORT_FROM_DLL void BulletWeapon::Archive(Archiver &arc)
{
   Weapon::Archive(arc);

   arc.WriteBoolean(damagedtarget);
}

inline EXPORT_FROM_DLL void BulletWeapon::Unarchive(Archiver &arc)
{
   Weapon::Unarchive(arc);

   arc.ReadBoolean(&damagedtarget);
}
//###

#endif /* BULLET.h */

// EOF

