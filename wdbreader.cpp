// wdbreader.cpp : Defines the entry point for the console application.
//


#include <Groundfloor/Atoms/GFDefines.h>
#include <Groundfloor/Atoms/GFInitialize.h>
#include <Groundfloor/Molecules/GFString.h>
#include <Groundfloor/Materials/GFFileCommunicator.h>
#include <Groundfloor/Materials/GFFileWriter.h>
#include <Groundfloor/Materials/GFFunctions.h>

#include <Groundfloor/Bookshelfs/GFBTable.h>
#include <Groundfloor/Bookshelfs/GFBFunctions.h>

#include <Groundfloor/Materials/GFGarbageCollector.h>

#include <Groundfloor/Bookshelfs/RemoteSquirrel.h>
#include <MySQLBooks/MySQLSquirrel.h>

#include <stdexcept>

#define DWORD unsigned int

// type 42: "Equip: Increases damage done by magical spells and effects by up to 25."

enum stattype { UNKNOWN = 0, HEALTH=1, AGILITY=3, STRENGTH=4, INTELLECT=5, SPIRIT=6, STAMINA=7,
                DEFENSE=12, DODGE=13, PARRY=14, BLOCK=15,
                MELEEHIT=16, RANGEDHIT=17, SPELLHIT=18,
                MELEECRIT=19, RANGEDCRIT=20, SPELLCRIT=21,
                MELEEHITAVOID=22, RANGEDHITAVOID=23, SPELLHITAVOID=24,
                MELEECRITAVOID=25, RANGEDCRITAVOID=26, SPELLCRITAVOID=27,
                MELEEHASTE=28, RANGEDHASTE=29, SPELLHASTE=30,
                HIT=31, CRIT=32,
                HITAVOID=33, CRITAVOID=34,
                RESILIENCE=35, HASTE=36, EXPERTISE=37,
                ATTACKPOWER=38, SPELLDAMAGE=42,
                MP5=43, ARMORPENETRATION=44, SPELLPOWER=45,
                HP5=46, SPELLPENETRATION=47, BLOCKVALUE=48
};
//:strength,:agility,:stamina,:intellect,:spirit,:armor,:attackpower,:hasterating,:hitrating,:critrating,:armorpen,:defense,:dodge,:parry,:block,:resilience,:expertise,:spellpower,:mp5notcasting,:mp5casting,:spellpenetration
char *statnames[] = {"","HEALTH","","agility","strength","intellect","spirit","stamina",
                     "","","","","defense","dodge","parry","block",
                     "hitrating","hitrating","hitrating",
                     "critrating","critrating","critrating",
                     "MELEEHITAVOID","RANGEDHITAVOID","SPELLHITAVOID",
                     "MELEECRITAVOID","RANGEDCRITAVOID","SPELLCRITAVOID",
                     "hasterating","hasterating","hasterating",
                     "hitrating","critrating",
                     "HITAVOID","CRITAVOID",
                     "resilience","hasterating","expertise",
                     "attackpower","","","","spellpower",
                     "mp5notcasting","armorpen","spellpower",
                     "hp5","spellpenetration","blockvalue"
};

char *subclassnames[5] = {"","Cloth","Leather","Mail","Plate"};
DWORD emptyword1 = 0;
char *gemsubclassnames[9] = {"Matches a Red Socket.","Matches a Blue Socket.","Matches a Yellow Socket.","Matches a Red or Blue Socket.","Matches a Yellow or Blue Socket.","Matches a Red or Yellow Socket.","Only fits in a meta gem slot.","","Matches a Red, Yellow or Blue Socket."};
DWORD emptyword2 = 0;

class CSimpleSpell: public TGFFreeable {
public:
   DWORD id;
   DWORD aura1;
   int value1;
   DWORD aura2;
   int value2;
   DWORD aura3;
   int value3;
   stattype aura1stat;
   stattype aura2stat;
   stattype aura3stat;
   DWORD extStat1;
   DWORD extStat2;
   DWORD extStat3;
   TGFString name;
   TGFString description;
   int stackamount;

   CSimpleSpell() : TGFFreeable() {
   }

   void fillRec( TGFBRecord *rec ) {
      rec->newValue()->setInteger(id);
      rec->newValue()->setString(&name);
      rec->newValue()->setString(&description);

      rec->newValue()->setInteger(aura1);
      rec->newValue()->setInteger(extStat1);
      rec->newValue()->setInteger(value1);
      
      rec->newValue()->setInteger(aura2);
      rec->newValue()->setInteger(extStat2);
      rec->newValue()->setInteger(value2);

      rec->newValue()->setInteger(aura3);
      rec->newValue()->setInteger(extStat3);
      rec->newValue()->setInteger(value3);

      rec->newValue()->setInteger(stackamount);
   }

   void fillParams( TRemoteSQL *qry ) {
      qry->findOrAddParam("id")->setInteger(id);
      qry->findOrAddParam("name")->setString(&name);
      qry->findOrAddParam("description")->setString(&description);

      qry->findOrAddParam("stat1")->setInteger(aura1);
      qry->findOrAddParam("ext1")->setInteger(extStat1);
      qry->findOrAddParam("val1")->setInteger(value1);

      qry->findOrAddParam("stat2")->setInteger(aura2);
      qry->findOrAddParam("ext2")->setInteger(extStat2);
      qry->findOrAddParam("val2")->setInteger(value2);

      qry->findOrAddParam("stat3")->setInteger(aura3);
      qry->findOrAddParam("ext3")->setInteger(extStat3);
      qry->findOrAddParam("val3")->setInteger(value3);

      qry->findOrAddParam("stackamount")->setInteger(stackamount);
   }
};

class CDBSpell: public TGFFreeable {
public:
   unsigned int id;
   int val1;
   TGFString name1;
   int val2;
   TGFString name2;
   int val3;
   TGFString name3;

   bool spellfound;

   CDBSpell( TRemoteSquirrelConnection *conn, unsigned int spellid ) : TGFFreeable() {
      spellfound = false;

      TGFString sql(
         "select spell.id as spellid, val1, s1.internalname as n1, val2, s2.internalname as n2, val3, s3.internalname as n3 \
          from spell \
         left outer join stattype s1 on (s1.basetype=spell.stat1 and s1.exttype=spell.ext1) \
         left outer join stattype s2 on (s2.basetype=spell.stat2 and s2.exttype=spell.ext2) \
         left outer join stattype s3 on (s3.basetype=spell.stat3 and s3.exttype=spell.ext3) \
         where spell.id=:spellid");
      TMySQLSquirrel query(conn);
      query.setQuery( &sql );
      query.findOrAddParam("spellid")->setInteger(spellid);
      if ( query.open() ) {
         if ( query.next() ) {
            spellfound = true;

            TGFBRecord *rec = new TGFBRecord();
            query.fetchRecord( rec );
            id = rec->getValue(0)->asInteger();
            val1 = rec->getValue(1)->asInteger();
            name1.setValue( rec->getValue(2)->asString() );
            val2 = rec->getValue(3)->asInteger();
            name2.setValue( rec->getValue(4)->asString() );
            val3 = rec->getValue(5)->asInteger();
            name3.setValue( rec->getValue(6)->asString() );
            delete rec;
         }
         query.close();
      }
   }


   void addStatsToParams( TMySQLSquirrel *qry ) {
      if ( name1.getLength() != 0 ) {
         TGFBValue *p = qry->findOrAddParam(&name1);
         p->setDouble( p->asDouble() + val1 );
      }
      if ( name2.getLength() != 0 ) {
         TGFBValue *p = qry->findOrAddParam(&name2);
         p->setDouble( p->asDouble() + val1 );
      }
      if ( name3.getLength() != 0 ) {
         TGFBValue *p = qry->findOrAddParam(&name3);
         p->setDouble( p->asDouble() + val1 );
      }
   }
};

TGFVector simplespells;

CSimpleSpell *findSimpleSpell( DWORD spellid ) {
   for ( unsigned int i = 0; i < simplespells.size(); i++ ) {
      CSimpleSpell *s = static_cast<CSimpleSpell *>( simplespells.elementAt(i) );
      if ( s->id == spellid ) {
         return s;
      }
   }

   return NULL;
}


struct dbcheader {
   unsigned char signature[4];
   DWORD records;
   DWORD fields;
   DWORD recordsize;
   DWORD stringblocksize;
};

struct wdbheader {
   unsigned char signature[4];
   DWORD version1;
   unsigned char language[4];
   DWORD unknown1;
   DWORD unknown2;
   DWORD version2;
};

struct itemheader {
   DWORD itemid;
   DWORD entrylength;
   DWORD classid;
   DWORD subclassid;
   DWORD unknown;
};

struct itemstruct {
   DWORD itemdisplayID;
   DWORD qualityID;
   DWORD typeBinFlag;
   DWORD buyPrice;
   DWORD faction;
   DWORD sellPrice;
   DWORD inventorySlotID;
   DWORD classBinFlag;
   DWORD raceBinFlag;
   DWORD itemLevel;
   DWORD requiredLevel;
   DWORD requiredSkillID;
   DWORD requiredSkillLevel;
   DWORD requiredSpellID;
   DWORD requiredRankID;
   DWORD requiredUnknownRank;
   DWORD requiredFactionID;
   DWORD requiredFactionLevel;
   DWORD stackUnique;
   DWORD stackAmount;
   DWORD containerSlots;
   DWORD numberOfStats;
};

struct itemstat {
   DWORD statID;
   int statValue;
};

struct itemrest1 {
   float damage1Min;
   float damage1Max;
   DWORD damage1TypeID;
   float damage2Min;
   float damage2Max;
   DWORD damage2TypeID;
   DWORD resistPhysical;
   DWORD resistHoly;
   DWORD resistFire;
   DWORD resistNature;
   DWORD resistFrost;
   DWORD resistShadow;
   DWORD resistArcane;
   DWORD weaponDelay;
   DWORD ammoType;
   float rangeModifier;
   DWORD spell1ID;
   DWORD spell1TriggerID;
   DWORD spell1Charges;
   DWORD spell1Cooldown;
   DWORD spell1CategoryID;
   DWORD spell1CategoryCooldown;
   DWORD spell2ID;
   DWORD spell2TriggerID;
   DWORD spell2Charges;
   DWORD spell2Cooldown;
   DWORD spell2CategoryID;
   DWORD spell2CategoryCooldown;
   DWORD spell3ID;
   DWORD spell3TriggerID;
   DWORD spell3Charges;
   DWORD spell3Cooldown;
   DWORD spell3CategoryID;
   DWORD spell3CategoryCooldown;
   DWORD spell4ID;
   DWORD spell4TriggerID;
   DWORD spell4Charges;
   DWORD spell4Cooldown;
   DWORD spell4CategoryID;
   DWORD spell4CategoryCooldown;
   DWORD spell5ID;
   DWORD spell5TriggerID;
   DWORD spell5Charges;
   DWORD spell5Cooldown;
   DWORD spell5CategoryID;
   DWORD spell5CategoryCooldown;
   DWORD bondID;
};

struct itemrest2 {
   DWORD bookTextID;
   DWORD bookPages;
   DWORD bookStationaryID;
   DWORD beginQuestID;
   DWORD lockID;
   DWORD materialID;
   DWORD sheathID;
   DWORD randompropertyID;
   DWORD randompropertyID2;
   DWORD blockValue;
   DWORD itemSetID;
   DWORD durabilityValue;
   DWORD itemAreaID;
   DWORD itemMapID;
   DWORD bagFamily;
   DWORD iRefID_TotemCategory;
   DWORD socketcolor_1;
   DWORD unknown1;
   DWORD socketcolor_2;
   DWORD unknown2;
   DWORD socketcolor_3;
   DWORD unknown3;
   DWORD iRefID_GemProperties;
   DWORD disenchantSkillLevel;
   DWORD armorDamageModifier;
   DWORD existingDuration;
   DWORD itemLimitCategory;
   DWORD holidayID;
};

struct fakestruct {
   DWORD aa;
   DWORD ab;
   DWORD ac;
   DWORD ad;
   DWORD ae;
   DWORD af;
   DWORD ag;
   DWORD ah;
   DWORD ai;
   DWORD aj;
   DWORD ak;
   DWORD al;
   DWORD am;
   DWORD an;
   DWORD ao;
   DWORD ap;
   DWORD aq;
   DWORD ar;
   DWORD ass;
   DWORD at;
   DWORD au;
   DWORD av;
   DWORD aw;
   DWORD ax;
   DWORD ay;
   DWORD az;
   DWORD ba;
   DWORD bb;
   DWORD bc;
   DWORD bd;
   DWORD be;
   DWORD bf;
   DWORD bg;
   DWORD bh;
   DWORD bi;
   DWORD bj;
   DWORD bk;
   DWORD bl;
   DWORD bm;
   DWORD bn;
   DWORD bo;
   DWORD bp;
   DWORD bq;
   DWORD br;
   DWORD bs;
   DWORD bt;
   DWORD bu;
   DWORD bv;
   DWORD bw;
   DWORD bx;
   DWORD by;
   DWORD bz;
   DWORD ca;
   DWORD cb;
   DWORD cc;
   DWORD cd;
   DWORD ce;
   DWORD cf;
   DWORD cg;
   DWORD ch;
   DWORD ci;
   DWORD cj;
   DWORD ck;
   DWORD cl;
   DWORD cm;
   DWORD cn;
   DWORD co;
   DWORD cp;
   DWORD cq;
   DWORD cr;
   DWORD cs;
   DWORD ct;
   DWORD cu;
   DWORD cv;
   DWORD cw;
   DWORD cx;
   DWORD cy;
   DWORD cz;
   DWORD da;
   DWORD db;
   DWORD dc;
   DWORD dd;
   DWORD de;
   DWORD df;
   DWORD dg;
   DWORD dh;
   DWORD di;
   DWORD dj;
   DWORD dk;
   DWORD dll;
   DWORD dm;
   DWORD dn;
   DWORD doo;
   DWORD dp;
   DWORD dq;
   DWORD dr;
   DWORD ds;
   DWORD dt;
   DWORD du;
   DWORD dv;
   DWORD dw;
   DWORD dx;
   DWORD dy;
   DWORD dz;
   DWORD ea;
   DWORD eb;
   DWORD ec;
   DWORD ed;
   DWORD ee;
   DWORD ef;
   DWORD eg;
   DWORD eh;
   DWORD ei;
   DWORD ej;
   DWORD ek;
   DWORD el;
   DWORD em;
   DWORD en;
   DWORD eo;
   DWORD ep;
   DWORD eq;
   DWORD er;
   DWORD es;
   DWORD et;
   DWORD eu;
   DWORD ev;
   DWORD ew;
   DWORD ex;
   DWORD ey;
   DWORD ez;
   DWORD fa;
   DWORD fb;
   DWORD fc;
   DWORD fd;
   DWORD fe;
   DWORD ff;
   DWORD fg;
   DWORD fh;
   DWORD fi;
   DWORD fj;
   DWORD fk;
   DWORD fl;
   DWORD fm;
   DWORD fn;
   DWORD fo;
   DWORD fp;
   DWORD fq;
   DWORD fr;
   DWORD fs;
   DWORD ft;
   DWORD fu;
   DWORD fv;
   DWORD fw;
   DWORD fx;
   DWORD fy;
   DWORD fz;
   DWORD ga;
   DWORD gb;
   DWORD gc;
   DWORD gd;
   DWORD ge;
   DWORD gf;
   DWORD gg;
   DWORD gh;
   DWORD gi;
   DWORD gj;
   DWORD gk;
   DWORD gl;
   DWORD gm;
   DWORD gn;
   DWORD go;
   DWORD gp;
   DWORD gq;
   DWORD gr;
   DWORD gs;
   DWORD gt;
   DWORD gu;
   DWORD gv;
   DWORD gw;
   DWORD gx;
   DWORD gy;
   DWORD gz;
};

class CSpellDbc: public TGFFreeable {
protected:
   int fldAura1;
   int fldAura2;
   int fldAura3;

   int fldExtStat1;
   int fldExtStat2;
   int fldExtStat3;

   int fldValueBase1;
   int fldValueBase2;
   int fldValueBase3;

   int fldValue1;
   int fldValue2;
   int fldValue3;

   int fldStack1;

   int fldName;
   int fldDescription;

public:
   CSpellDbc() : TGFFreeable() {
      this->fldAura1 = 0;
      this->fldValueBase1 = 0;
      this->fldValue1 = 0;
      this->fldName = 0;
      this->fldDescription = 0;
      this->fldExtStat1 = 0;
      this->fldStack1 = 0;
   }
   ~CSpellDbc() {
   }

   void foodForCalibration( TGFString *data, unsigned int fieldcount ) {
      DWORD *field = reinterpret_cast<DWORD *>(data->getPointer(0));
      fakestruct *f = reinterpret_cast<fakestruct *>(data->getPointer(0));
      if ( field[0] == 21634 ) {
         for ( unsigned int i = 1; i < fieldcount; i++ ) {
            DWORD v = field[i];
            if ( (this->fldValue1 == 0) && (v == 13) ) {
               this->fldValue1 = i;
               this->fldValue2 = i + 1;
               this->fldValue3 = i + 2;
            } else if ( this->fldValue1 != 0 ) {
               if ( (this->fldValueBase1 == 0) && (v == 1) ) {
                  this->fldValueBase1 = i;
                  this->fldValueBase2 = i + 1;
                  this->fldValueBase3 = i + 2;
               } else if ( (this->fldAura1 == 0) && (v == 85) ) {
                  this->fldAura1 = i;
                  this->fldAura2 = i + 1;
                  this->fldAura3 = i + 2;
               }
            }
         }
      } else if ( field[0] == 9346 ) {
         field[0] = 9346;
      } else if ( field[0] == 18384 ) {
         for ( unsigned int i = 1; i < fieldcount; i++ ) {
            DWORD v = field[i];
            if ( (this->fldExtStat1 == 0) && (v == 1792) ) {
               this->fldExtStat1 = i;
               this->fldExtStat2 = i + 1;
               this->fldExtStat3 = i + 2;
            }
         }
      } else if ( field[0] == 5 ) {
         for ( unsigned int i = 1; i < fieldcount; i++ ) {
            DWORD v = field[i];
            if ( (this->fldName == 0) && (v == 69) ) {
               this->fldName = i;
            } else if ( this->fldName != 0 ) {
               if ( ( this->fldDescription == 0 ) && (v == 81) ) {
                  this->fldDescription = i;
               }
            }
         }
      } else if ( field[0] == 57435 ) {
         field[0] = 57435;
      } else if ( field[0] == 68251 ) {
         field[0] = 68251; 
      } else if ( field[0] == 71396 ) {
         field[0] = 71396;
         for ( unsigned int i = 1; i < fieldcount; i++ ) {
            DWORD v = field[i];
            if ( (this->fldStack1 == 0) && (v == 20) ) {
               this->fldStack1 = i;
            }
         }
         
      }
   }

   bool parseIntoSimpleSpellStruct( TGFString *data, CSimpleSpell *spell, TGFString *stringblock ) {
      DWORD *field = reinterpret_cast<DWORD *>(data->getPointer(0));

      spell->id = field[0];
      spell->aura1 = field[this->fldAura1];
      spell->aura2 = field[this->fldAura2];
      spell->aura3 = field[this->fldAura3];
      spell->extStat1 = field[this->fldExtStat1];
      spell->extStat2 = field[this->fldExtStat2];
      spell->extStat3 = field[this->fldExtStat3];
      spell->value1 = field[this->fldValueBase1] + field[this->fldValue1];
      spell->value2 = field[this->fldValueBase2] + field[this->fldValue2];
      spell->value3 = field[this->fldValueBase3] + field[this->fldValue3];
      spell->aura1 = field[this->fldAura1];
      spell->aura2 = field[this->fldAura2];
      spell->aura3 = field[this->fldAura3];
      spell->name.setValue_ansi( stringblock->getPointer(field[this->fldName]) );
      spell->stackamount = field[this->fldStack1];
      if ( field[this->fldDescription] != 0 ) {
         spell->description.setValue_ansi( stringblock->getPointer(field[this->fldDescription]) );
      }

      return true;
   }

   bool isCalibrated() {
      return ( (fldValue1 != 0) && (fldValueBase1 != 0) && (fldAura1 != 0) && (fldDescription != 0) && (fldExtStat1 != 0) && (fldStack1 != 0) );
   }
};


class CDBSpellItemEnchantment: public TGFFreeable {
public:
   unsigned int id;
   unsigned int type1;
   unsigned int type2;
   unsigned int type3;
   unsigned int value1;
   unsigned int value2;
   unsigned int value3;
   unsigned int spell1;
   unsigned int spell2;
   unsigned int spell3;
   unsigned int stat1;
   unsigned int stat2;
   unsigned int stat3;
   unsigned int gemid;
   unsigned int conditionid;
   TGFString name;

   CDBSpellItemEnchantment() : TGFFreeable() {
      id = 0;
      type1 = 0;
      type2 = 0;
      type3 = 0;
      value1 = 0;
      value2 = 0;
      value3 = 0;
      spell1 = 0;
      spell2 = 0;
      spell3 = 0;
      stat1 = 0;
      stat2 = 0;
      stat3 = 0;
      gemid = 0;
      conditionid = 0;
      name.setValue_ansi("");
   }
};

class CSpellItemEnchantment: public TGFFreeable {
protected:
   unsigned int fldType1;
   unsigned int fldType2;
   unsigned int fldType3;

   unsigned int fldValue1;
   unsigned int fldValue2;
   unsigned int fldValue3;

   unsigned int fldSpellOrStat1;
   unsigned int fldSpellOrStat2;
   unsigned int fldSpellOrStat3;

   unsigned int fldName;

   unsigned int fldGemId;
   unsigned int fldConditionId;

public:
   CSpellItemEnchantment() : TGFFreeable() {
      fldType1 = 0;
      fldType2 = 0;
      fldType3 = 0;

      fldValue1 = 0;
      fldValue2 = 0;
      fldValue3 = 0;

      fldSpellOrStat1 = 0;
      fldSpellOrStat2 = 0;
      fldSpellOrStat3 = 0;

      fldName = 0;

      fldGemId = 0;
      fldConditionId = 0;
   }
   ~CSpellItemEnchantment() {
   }

   void foodForCalibration( TGFString *data, unsigned int fieldcount ) {
      DWORD *field = reinterpret_cast<DWORD *>(data->getPointer(0));
      fakestruct *f = reinterpret_cast<fakestruct *>(data->getPointer(0));
      if ( field[0] == 2 ) {
         for ( unsigned int i = 1; i < fieldcount; i++ ) {
            DWORD v = field[i];
            if ( (this->fldName == 0) && (v == 13) ) {
               this->fldName = i;
               break;
            }
         }
      } else if ( field[0] == 3518 ) { // +20 Strength
         for ( unsigned int i = 1; i < fieldcount; i++ ) {
            DWORD v = field[i];
            if ( (this->fldGemId == 0) && (v == 40111) ) {
               this->fldGemId = i;
            } else if ( (this->fldValue1 == 0) && (v == 20) ) {
               this->fldValue1 = i;
               this->fldValue2 = i + 1;
               this->fldValue3 = i + 2;
            } else if ( (this->fldType1 == 0) && (v == 5) ) {
               this->fldType1 = i;
               this->fldType2 = i + 1;
               this->fldType3 = i + 2;
            } else if ( (this->fldSpellOrStat1 == 0) && (v == STRENGTH) ) {
               this->fldSpellOrStat1 = i;
               this->fldSpellOrStat2 = i + 1;
               this->fldSpellOrStat3 = i + 2;
            }
         }
      }
   }

   bool parseIntoSimpleStruct( TGFString *data, CDBSpellItemEnchantment *ench, TGFString *stringblock ) {
      DWORD *field = reinterpret_cast<DWORD *>(data->getPointer(0));

      ench->id = field[0];

      ench->value1 = field[this->fldValue1];
      ench->value2 = field[this->fldValue2];
      ench->value3 = field[this->fldValue3];

      ench->type1 = field[this->fldType1];
      ench->type2 = field[this->fldType2];
      ench->type3 = field[this->fldType3];

      if ( ench->type1 != 5 ) {
         ench->spell1 = field[this->fldSpellOrStat1];
      } else {
         ench->stat1 =  field[this->fldSpellOrStat1];
      }
      if ( ench->type2 != 5 ) {
         ench->spell2 = field[this->fldSpellOrStat2];
      } else {
         ench->stat2 =  field[this->fldSpellOrStat2];
      }
      if ( ench->type3 != 5 ) {
         ench->spell3 = field[this->fldSpellOrStat3];
      } else {
         ench->stat3 = field[this->fldSpellOrStat3];
      }

      ench->gemid = field[this->fldGemId];
      ench->conditionid = field[this->fldConditionId];

      ench->name.setValue_ansi( stringblock->getPointer(field[this->fldName]) );

      return true;
   }

   bool isCalibrated() {
      return (
         (fldType1 != 0) &&
         (fldType2 != 0) &&
         (fldType3 != 0) &&
         (fldValue1 != 0) &&
         (fldValue2 != 0) &&
         (fldValue3 != 0) &&
         (fldSpellOrStat1 != 0) &&
         (fldSpellOrStat2 != 0) &&
         (fldSpellOrStat3 != 0) &&
         (fldName != 0) &&
         (fldGemId != 0) &&
         (fldConditionId != 0)
      );
   }
};



unsigned int readItem( TRemoteSquirrelConnection *conn, TGFString *data, unsigned int index ) {
   int newindex = index;

   itemheader ih;
   memcpy( &ih, data->getPointer(index), sizeof(itemheader) );

   
   int i = index + sizeof(itemheader);
   newindex += ih.entrylength;

   TGFString name1, name2, name3, name4;
   name1.setValue_ansi( data->getPointer(i) );
   i += name1.getLength() + 1;
   name2.setValue_ansi( data->getPointer(i) );
   i += name2.getLength() + 1;
   name3.setValue_ansi( data->getPointer(i) );
   i += name3.getLength() + 1;
   name4.setValue_ansi( data->getPointer(i) );
   i += name4.getLength() + 1;

/*
   printf( "entrylength: %d\n", ih.entrylength );
   printf( "classid: %d\n", ih.classid );
   printf( "subclassid: %d\n", ih.subclassid );

   printf( "name1: %s\n", name1.getValue() );
   printf( "name2: %s\n", name2.getValue() );
   printf( "name3: %s\n", name3.getValue() );
   printf( "name4: %s\n", name4.getValue() );
*/
   itemstruct is;
   memcpy( &is, data->getPointer(i), sizeof(itemstruct) );
   i += sizeof(itemstruct);


   if ( is.inventorySlotID == 0 ) {
      //return newindex;
   }

/*
   printf( "q: %d\n", is.qualityID );
   printf( "ilvl: %d\n", is.itemLevel );
*/
   //printf( "itemid: %d\n", ih.itemid );

   //printf("\n\n");
   TSquirrelReturnData err;

   int statsentryid = 0;
   bool alreadyexists = false;

   TGFString sSelectItem("select id, statsentry_id from item where id=:itemid");
   TMySQLSquirrel qrySelectItem(conn);
   qrySelectItem.setQuery(&sSelectItem);
   qrySelectItem.findOrAddParam("itemid")->setInteger(ih.itemid);
   if ( qrySelectItem.open(&err) ) {
      // item bestaat al
      if ( qrySelectItem.getRecordCount() > 0 ) {
         qrySelectItem.next();

         TGFBRecord *rec = new TGFBRecord();
         qrySelectItem.fetchRecord(rec);
         alreadyexists = true;
         statsentryid = rec->getValue(1)->asInteger();
         delete rec;
         qrySelectItem.close();

         if ( ih.itemid == 40111 ) {
            //printf("40111\n");
         } else {
            //return newindex;
         }
      }
      qrySelectItem.close();
   } else {
      printf( "error: %s\n", err.errorstring.getValue() );
   }


   int iStatsId = 0;

   TGFString sInsertStats(
            "insert into statsentry \
				( strength, agility, stamina, intellect, spirit, armor, attackpower, hasterating, hitrating, critrating, armorpen, defense, dodge, parry, block, resilience, expertise, spellpower, mp5notcasting, mp5casting, spellpenetration, hp5, blockvalue) \
				 values \
            (:strength,:agility,:stamina,:intellect,:spirit,:armor,:attackpower,:hasterating,:hitrating,:critrating,:armorpen,:defense,:dodge,:parry,:block,:resilience,:expertise,:spellpower,:mp5notcasting,:mp5casting,:spellpenetration,:hp5,:blockvalue)");
   TGFString sUpdateStats(
            "update statsentry \
            set strength=:strength, agility=:agility, stamina=:stamina, intellect=:intellect, spirit=:spirit, armor=:armor, \
            attackpower=:attackpower, hasterating=:hasterating, hitrating=:hitrating, critrating=:critrating, armorpen=:armorpen, \
            defense=:defense, dodge=:dodge, parry=:parry, block=:block, resilience=:resilience, expertise=:expertise, spellpower=:spellpower, \
            mp5notcasting=:mp5notcasting, mp5casting=:mp5casting, spellpenetration=:spellpenetration, hp5=:hp5, blockvalue=:blockvalue \
            where id=:id");

   TMySQLSquirrel qryInsertStats(conn);

   if ( alreadyexists ) {
      qryInsertStats.setQuery(&sUpdateStats);
      qryInsertStats.findOrAddParam("id")->setInteger( statsentryid );
   } else {
      qryInsertStats.setQuery(&sInsertStats);
   }


   int d = 49;
   for ( int j = 0; j < d; j++ ) {
      if ( strlen(statnames[j]) > 0 ) {
         qryInsertStats.findOrAddParam(statnames[j])->setDouble(0);
      }
   }

   qryInsertStats.findOrAddParam("mp5casting")->setDouble(0);
   qryInsertStats.findOrAddParam("spellpenetration")->setDouble(0);

   TGFString mp5("mp5notcasting");
   for ( int j = 0; j < is.numberOfStats; j++ ) {
      itemstat istat;
      memcpy( &istat, data->getPointer(i), sizeof(itemstat) );

      i += sizeof(itemstat);

      if ( (statnames[istat.statID] == NULL) || (strlen(statnames[istat.statID]) == 0) ) {
         printf("%d: %d = %d\n", ih.itemid, istat.statID, istat.statValue);
         system("pause");
         exit(1);
      }
      qryInsertStats.findOrAddParam(statnames[istat.statID])->setDouble(istat.statValue);
      if ( mp5.match_ansi( statnames[istat.statID] ) ) {
         qryInsertStats.findOrAddParam("mp5casting")->setDouble(istat.statValue);
      }
   }

   // data related to bind on account items that scale with level
   itemstat istat;
   memcpy( &istat, data->getPointer(i), sizeof(itemstat) );
   i += sizeof(itemstat);

   itemrest1 ir1;
   memcpy( &ir1, data->getPointer(i), sizeof(itemrest1) );
   i += sizeof(itemrest1);

   qryInsertStats.findOrAddParam("armor")->setInteger( ir1.resistPhysical );

   bool bASpellStatSet = false;
   if ( (ir1.spell1ID != 0) || (ir1.spell2ID != 0) || (ir1.spell3ID != 0) || (ir1.spell4ID != 0) || (ir1.spell5ID != 0) ) {
      CDBSpell *spell;
      if ( ir1.spell1ID != 0 ) {
         if ( ir1.spell1TriggerID == 1 ) {
            spell = new CDBSpell( conn, ir1.spell1ID );
            if ( spell->spellfound ) {
               spell->addStatsToParams( &qryInsertStats );
               bASpellStatSet = true;
            }
            delete spell;
         }
      }
      if ( ir1.spell2ID != 0 ) {
         if ( ir1.spell2TriggerID == 1 ) {
            spell = new CDBSpell( conn, ir1.spell2ID );
            if ( spell->spellfound ) {
               spell->addStatsToParams( &qryInsertStats );
               bASpellStatSet = true;
            }
            delete spell;
         }
      }
      if ( ir1.spell3ID != 0 ) {
         if ( ir1.spell3TriggerID == 1 ) {
            spell = new CDBSpell( conn, ir1.spell3ID );
            if ( spell->spellfound ) {
               spell->addStatsToParams( &qryInsertStats );
               bASpellStatSet = true;
            }
            delete spell;
         }
      }
      if ( ir1.spell4ID != 0 ) {
         if ( ir1.spell4TriggerID == 1 ) {
            spell = new CDBSpell( conn, ir1.spell4ID );
            if ( spell->spellfound ) {
               spell->addStatsToParams( &qryInsertStats );
               bASpellStatSet = true;
            }
            delete spell;
         }
      }
      if ( ir1.spell5ID != 0 ) {
         if ( ir1.spell5TriggerID == 1 ) {
            spell = new CDBSpell( conn, ir1.spell5ID );
            if ( spell->spellfound ) {
               spell->addStatsToParams( &qryInsertStats );
               bASpellStatSet = true;
            }
            delete spell;
         }
      }

      if ( !bASpellStatSet ) {
//         printf("Item %d uses old-style stats, add through wow-head\n", ih.itemid);
//         return newindex;
      }
   }
  
   // something dynamic here
   TGFString description;
   description.setValue_ansi( data->getPointer(i) );
   i += description.getLength() + 1; 

   itemrest2 ir2;
   memcpy( &ir2, data->getPointer(i), sizeof(itemrest2) );
   i += sizeof(itemrest2);

   qryInsertStats.findOrAddParam("blockvalue")->setInteger( ir2.blockValue );

   if ( qryInsertStats.open(&err) ) {
      iStatsId = qryInsertStats.getLastID("statsentry");

      qryInsertStats.close();
   } else {
      printf( "error: %s\n", err.errorstring.getValue() );
   }

   TMySQLSquirrel qryInsertItem(conn);

   TGFString sInsertItem( "insert into item ( id, name, lvl, statsentry_id, quality, invtype, subclassname, spell1, spell2, spell3, spell4, spell5, materialid, classid, subclassid, itemsetid) values (:id,:name,:lvl,:statsentry_id,:quality,:invtype,:subclassname,:spell1,:spell2,:spell3,:spell4,:spell5,:materialid,:classid,:subclassid,:itemsetid)");
   TGFString sUpdateItem( "update item set name=:name, lvl=:lvl, statsentry_id=:statsentry_id, quality=:quality, invtype=:invtype, subclassname=:subclassname, spell1=:spell1, spell2=:spell2, spell3=:spell3, spell4=:spell4, spell5=:spell5, materialid=:materialid, classid=:classid, subclassid=:subclassid, itemsetid=:itemsetid, isarmoryitem=:isarmoryitem where id=:id");

   if ( alreadyexists ) {
      qryInsertItem.setQuery(&sUpdateItem);

      iStatsId = statsentryid;
   } else {
      qryInsertItem.setQuery(&sInsertItem);
   }

   // : voor de paramname
   qryInsertItem.findOrAddParam("id")->setInteger( ih.itemid );
   qryInsertItem.findOrAddParam("name")->setString( &name1 );
   qryInsertItem.findOrAddParam("lvl")->setInteger( is.itemLevel );
   qryInsertItem.findOrAddParam("statsentry_id")->setInteger( iStatsId );
   qryInsertItem.findOrAddParam("quality")->setInteger( is.qualityID );
   qryInsertItem.findOrAddParam("invtype")->setInteger( is.inventorySlotID );

   qryInsertItem.findOrAddParam("spell1")->setInteger( ir1.spell1ID );
   qryInsertItem.findOrAddParam("spell2")->setInteger( ir1.spell2ID );
   qryInsertItem.findOrAddParam("spell3")->setInteger( ir1.spell3ID );
   qryInsertItem.findOrAddParam("spell4")->setInteger( ir1.spell4ID );
   qryInsertItem.findOrAddParam("spell5")->setInteger( ir1.spell5ID );

   qryInsertItem.findOrAddParam("materialid")->setInteger( ir2.materialID );
   qryInsertItem.findOrAddParam("classid")->setInteger( ih.classid );
   qryInsertItem.findOrAddParam("subclassid")->setInteger( ih.subclassid );
   qryInsertItem.findOrAddParam("itemsetid")->setInteger( ir2.itemSetID );

   qryInsertItem.findOrAddParam("isarmoryitem")->setInteger( 0 );

   if (ih.classid == 3) {
      // gem
      qryInsertItem.findOrAddParam("subclassname")->setString( gemsubclassnames[ih.subclassid] );
   } else {
      if ( ih.subclassid < 5 ) {
         qryInsertItem.findOrAddParam("subclassname")->setString( subclassnames[ih.subclassid] );
      } else {
         qryInsertItem.findOrAddParam("subclassname")->setString( "" );
      }
   }

   if ( qryInsertItem.open(&err) ) {
      qryInsertItem.close();
   } else {
      printf( "error: %s\n", err.errorstring.getValue() );
   }

   return newindex;
}

void loadSpellDbc( TRemoteSquirrelConnection *conn ) {
   TGFString sFilename("Spell.dbc");

   TGFString *data = new TGFString();

   TGFFileCommunicator c;
   c.filename.set( sFilename.getValue() );
   c.connect();

   TGFCommReturnData err;
   TGFString buffer;
   buffer.setSize(65535);
   while ( c.receive( &buffer, &err ) ) {
      data->append( &buffer );

      if ( err.eof ) {
         break;
      }
   }
   c.disconnect();



   unsigned int i = 0;

   dbcheader h;
   memcpy( &h, data->getPointer(i), sizeof(dbcheader) );
   i += sizeof(dbcheader);

   TGFString sig;
   sig.setValue( reinterpret_cast<const char *>(h.signature), 4 );
   
   printf( "%s\n", sig.getValue() );
   if ( !sig.match_ansi("WDBC") ) {
      printf( "Not a valid itemcache WDB-file\n" );
      return;
   }

   CSpellDbc dbc;

   TGFString *record = new TGFString();
   for ( unsigned int r = 0; r < h.records; r++ ) {
      record->setValue( data->getPointer(i), h.recordsize );
      dbc.foodForCalibration( record, h.fields );
      i += h.recordsize;

/*      if ( dbc.isCalibrated() ) {
         break;
      }
*/
   }

   TGFString stringblock;
   unsigned int iStrOffset = sizeof(dbcheader) + (h.records * h.recordsize);
   stringblock.setValue( data->getPointer( iStrOffset ), data->getLength() - iStrOffset );

   TGFFileWriter *fw = new TGFFileWriter();
   fw->open("strings.txt");
   fw->add( &stringblock );
   fw->setStopWhenEmpty(true);
   fw->start();
   fw->waitWhileRunning();
   delete fw;

   TGFString sqlInsert("insert into spell \
                       ( id, name, description, stat1, ext1, val1, stat2, ext2, val2, stat3, ext3, val3, stackamount) values \
                       (:id,:name,:description,:stat1,:ext1,:val1,:stat2,:ext2,:val2,:stat3,:ext3,:val3,:stackamount)");
   TGFString sqlUpdate("update spell set \
                       name=:name, description=:description, stat1=:stat1, ext1=:ext1, val1=:val1, stat2=:stat2, ext2=:ext2, val2=:val2, stat3=:stat3, ext3=:ext3, val3=:val3, stackamount=:stackamount \
                       where id=:id");
   TMySQLSquirrel qryInsert(conn);

   TSquirrelReturnData sqlerr;

   TGFString sSelect("select id from spell where id=:id");
   TMySQLSquirrel qrySelect(conn);

   i = sizeof(dbcheader); 
   for ( unsigned int r = 0; r < h.records; r++ ) {
      record->setValue( data->getPointer(i), h.recordsize );

      CSimpleSpell *spell = new CSimpleSpell();
      dbc.parseIntoSimpleSpellStruct( record, spell, &stringblock );
      spell->fillParams( &qryInsert );
      simplespells.addElement( spell );

      bool spellexists = false;

      qrySelect.setQuery(&sSelect);
      qrySelect.findOrAddParam("id")->setInteger( spell->id );
      if ( qrySelect.open() ) {
         if ( qrySelect.getRecordCount() != 0 ) {
            spellexists = true;
         }
         qrySelect.close();
      }

      if ( spellexists ) {
         qryInsert.setQuery( &sqlUpdate );
      } else {
         qryInsert.setQuery( &sqlInsert );
      }

      if ( qryInsert.open( &sqlerr ) ) {
         qryInsert.close();
      } else {
         printf( "%s\n", qryInsert.getCurrentQuery()->getValue() );
         printf( "error: %s\n", sqlerr.errorstring.getValue() );
      }

      i += h.recordsize;
   }
   delete record;

   delete data;
}


void loadEnchantmentDbc( TRemoteSquirrelConnection *conn ) {
   TGFString sFilename("SpellItemEnchantment.dbc");

   TGFString *data = new TGFString();

   TGFFileCommunicator c;
   c.filename.set( sFilename.getValue() );
   c.connect();

   TGFCommReturnData err;
   TGFString buffer;
   buffer.setSize(65535);
   while ( c.receive( &buffer, &err ) ) {
      data->append( &buffer );

      if ( err.eof ) {
         break;
      }
   }
   c.disconnect();


   unsigned int i = 0;

   dbcheader h;
   memcpy( &h, data->getPointer(i), sizeof(dbcheader) );
   i += sizeof(dbcheader);

   TGFString sig;
   sig.setValue( reinterpret_cast<const char *>(h.signature), 4 );
   
   printf( "%s\n", sig.getValue() );
   if ( !sig.match_ansi("WDBC") ) {
      printf( "Not a valid itemcache WDB-file\n" );
      return;
   }

   TGFString stringblock;
   unsigned int iStrOffset = sizeof(dbcheader) + (h.records * h.recordsize);
   stringblock.setValue( data->getPointer( iStrOffset ), data->getLength() - iStrOffset );

   TGFFileWriter *fw = new TGFFileWriter();
   fw->open("enchants.txt");
   fw->add( &stringblock );
   fw->setStopWhenEmpty(true);
   fw->start();
   fw->waitWhileRunning();
   delete fw;

   TGFString sQryInsert("insert into spellitemenchantment ( id, name) values (:id,:name)");
   TGFString sQryUpdate("update spellitemenchantment set name=:name, type1=:type1, type2=:type2, type3=:type3, value1=:value1, value2=:value2, value3=:value3, spell1=:spell1, spell2=:spell2, spell3=:spell3, stat1=:stat1, stat2=:stat2, stat3=:stat3, gemid=:gemid, conditionid=:conditionid where id=:id");
   TMySQLSquirrel qryInsert(conn);

   TGFString *record = new TGFString();

   CSpellItemEnchantment colibri;

   i = sizeof(dbcheader); 
   for ( unsigned int r = 0; r < h.records; r++ ) {
      record->setValue( data->getPointer(i), h.recordsize );

      colibri.foodForCalibration( record, h.fields );
      if ( colibri.isCalibrated() ) {
         break;
      }

      i += h.recordsize;
   }

   i = sizeof(dbcheader); 
   for ( unsigned int r = 0; r < h.records; r++ ) {
      record->setValue( data->getPointer(i), h.recordsize );

      CDBSpellItemEnchantment *ench = new CDBSpellItemEnchantment();
      colibri.parseIntoSimpleStruct( record, ench, &stringblock );

      qryInsert.setQuery(&sQryInsert);
      qryInsert.findOrAddParam("id")->setInteger( ench->id );
      qryInsert.findOrAddParam("name")->setString( &ench->name );
      if ( qryInsert.open() ) {
         qryInsert.close();
      }

      qryInsert.setQuery(&sQryUpdate);
      qryInsert.findOrAddParam("id")->setInteger( ench->id );
      qryInsert.findOrAddParam("name")->setString( &ench->name );
      qryInsert.findOrAddParam("type1")->setInteger( ench->type1 );
      qryInsert.findOrAddParam("type2")->setInteger( ench->type2 );
      qryInsert.findOrAddParam("type3")->setInteger( ench->type3 );
      qryInsert.findOrAddParam("value1")->setInteger( ench->value1 );
      qryInsert.findOrAddParam("value2")->setInteger( ench->value2 );
      qryInsert.findOrAddParam("value3")->setInteger( ench->value3 );
      qryInsert.findOrAddParam("spell1")->setInteger( ench->spell1 );
      qryInsert.findOrAddParam("spell2")->setInteger( ench->spell2 );
      qryInsert.findOrAddParam("spell3")->setInteger( ench->spell3 );
      qryInsert.findOrAddParam("stat1")->setInteger( ench->stat1 );
      qryInsert.findOrAddParam("stat2")->setInteger( ench->stat2 );
      qryInsert.findOrAddParam("stat3")->setInteger( ench->stat3 );
      qryInsert.findOrAddParam("gemid")->setInteger( ench->gemid );
      qryInsert.findOrAddParam("conditionid")->setInteger( ench->conditionid );
      if ( qryInsert.open() ) {
         qryInsert.close();
      }

      i += h.recordsize;
   }
   delete record;

   delete data;
}

class QuitException: public std::runtime_error {
public:
   QuitException( const char *msg ) : std::runtime_error( msg ) {
   }
};

int main(int argc, char* argv[]) {
   TGFString *sFilename = NULL;

   TMySQLSquirrelConnection conn;
   if ( initGroundfloor() ) {
      initGlobalGarbageCollector();
      try {
         conn.host.set( "127.0.0.1" );
         conn.port.set( 3306 );
         conn.username.set( "lfs" );
         conn.password.set( "" );
         if ( !conn.connect() ) {
            throw QuitException("Can't connect to database");
         }
         conn.selectDatabase("lfs");

         loadEnchantmentDbc(&conn);
         throw QuitException("etc");

         // hoeft maar 1 keer (per patch), en is al ingelezen
         //loadSpellDbc(&conn);
         //throw QuitException("Done");

         sFilename = new TGFString("itemcache.wdb");
/*
if ( argc == 2 ) {
            sFilename->setValue_ansi( argv[1] );
            if ( !GFFileExists( sFilename ) ) {
               throw QuitException("File doesn't exist");
            }
         } else {
            throw QuitException("Usage: wdbreader.exe [path-to-itemcache.wdb]");
         }
*/
         TGFFileCommunicator c;
         c.filename.set( sFilename->getValue() );
         c.connect();

         TGFString *data = new TGFString();

         TGFCommReturnData err;
         TGFString buffer;
         buffer.setSize(65535);
         while ( c.receive( &buffer, &err ) ) {
            data->append( &buffer );

            if ( err.eof ) {
               break;
            }
         }
         c.disconnect();

         wdbheader h;
         memcpy( &h, data->getPointer(0), sizeof(wdbheader) );

         TGFString sig;
         sig.setValue( reinterpret_cast<const char *>(h.signature), 4 );
         
         printf( "%s\n", sig.getValue() );
         if ( !sig.match_ansi("BDIW") ) {
            printf( "Not a valid itemcache WDB-file\n" );
            return 1;
         }

         unsigned int i = sizeof(wdbheader);
         while ( (i + 9) < data->getLength() ) {
            i = readItem( &conn, data, i );
            i += 8;
         }

         delete data;

      } catch ( std::runtime_error e ) {
         printf("%s\n\n", e.what() );
      }
      conn.disconnect();

      finiGlobalGarbageCollector();
      finiGroundfloor();
   }

   delete sFilename;

	return 0;
}

