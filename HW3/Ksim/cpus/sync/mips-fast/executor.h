#ifndef __EXECUTOR_H__
#define __EXECUTOR_H__

#include "mips.h"

class Mipc;

class Exe : public SimObject
{
public:
   Exe(Mipc *);
   ~Exe();
#ifdef BYPASS_ENABLED
   void update_bypass(PipeReg &ID_EX_NXT);
#endif
   FAKE_SIM_TEMPLATE;

   Mipc *_mc;
};
#endif
