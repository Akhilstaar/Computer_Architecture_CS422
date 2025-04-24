#ifndef __DECODE_H__
#define __DECODE_H__

#include "mips.h"

class Mipc;

class Decode : public SimObject {
public:
   Decode (Mipc*);
   ~Decode ();
#ifdef BYPASS_ENABLED
   Bool computeBypass();
#else 
   Bool detectStalls();
#endif   
  
   FAKE_SIM_TEMPLATE;

   Mipc *_mc;
};
#endif
