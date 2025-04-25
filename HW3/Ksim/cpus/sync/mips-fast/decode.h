#ifndef __DECODE_H__
#define __DECODE_H__

#include "mips.h"

class Mipc;

class Decode : public SimObject {
public:
   Decode (Mipc*);
   ~Decode ();
#ifdef BYPASS_ENABLED
   Bool check_bypass(PipeReg &IF_ID_NXT);
#else 
   Bool check_stall(PipeReg &IF_ID_NXT);
#endif   
  
   FAKE_SIM_TEMPLATE;

   Mipc *_mc;
};
#endif
