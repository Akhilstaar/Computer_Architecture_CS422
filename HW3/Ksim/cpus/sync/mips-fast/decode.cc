#include "decode.h"

Decode::Decode(Mipc *mc)
{
   _mc = mc;
}

Decode::~Decode(void) {}

// TODO: Implement bypass logic
#ifdef BYPASS_ENABLED
Bool Decode::check_bypass(PipeReg &IF_ID_NXT)
{
   Bool toStall = FALSE;

   if (IF_ID_NXT._requiresFP)
   {
      // requires Floating Point Argument
      if (IF_ID_NXT._regSRC1 != REG_DEFAULT)
      {

         // check whether the value required is being produced in EX stage as of now, if yes, then stall
         if ((_mc->ID_EX_CUR._writeFREG && (IF_ID_NXT._regSRC1 == _mc->ID_EX_CUR._decodedDST)))
         {
            if (_mc->ID_EX_CUR._memControl)
            { // if it is a load instructions, then do load interlock
               toStall = TRUE;
               _mc->_num_interlock++;
            }
            else
               IF_ID_NXT._bypSRC1 = BYPASS_EX_EX;
         }
         // check whether the value required is being produced in MEM stage as of now, if yes, then stall
         else if ((_mc->EX_MEM_CUR._writeFREG && (IF_ID_NXT._regSRC1 == _mc->EX_MEM_CUR._decodedDST)))
         {
#ifdef BYPASS_MEM_EX_ENABLED
            IF_ID_NXT._bypSRC1 = BYPASS_MEM_EX;
#else
            toStall = TRUE;
#endif
         }
      }

      // Similarly check for SRC2
      if (IF_ID_NXT._regSRC2 != REG_DEFAULT)
      {

         if ((_mc->ID_EX_CUR._writeFREG && (IF_ID_NXT._regSRC2 == _mc->ID_EX_CUR._decodedDST)))
         {
            if (_mc->ID_EX_CUR._memControl)
            { // if it is a load instructions, then do load interlock
               toStall = TRUE;
               _mc->_num_interlock++;
            }
            else
               IF_ID_NXT._bypSRC2 = BYPASS_EX_EX;
         }
         else if ((_mc->EX_MEM_CUR._writeFREG && (IF_ID_NXT._regSRC2 == _mc->EX_MEM_CUR._decodedDST)))
         {
#ifdef BYPASS_MEM_EX_ENABLED
            IF_ID_NXT._bypSRC2 = BYPASS_MEM_EX;
#else
            toStall = TRUE;
#endif
         }
      }
   }
   else
   {
      // Integer Ins
      if (IF_ID_NXT._regSRC1 != 0 && IF_ID_NXT._regSRC1 != REG_DEFAULT)
      {
         if ((_mc->ID_EX_CUR._writeREG && (IF_ID_NXT._regSRC1 == _mc->ID_EX_CUR._decodedDST)))
         {
            if (_mc->ID_EX_CUR._memControl)
            { // if it is a load instructions, then do load interlock
               toStall = TRUE;
               _mc->_num_interlock++;
            }
            else
               IF_ID_NXT._bypSRC1 = BYPASS_EX_EX;
         }
         else if ((_mc->EX_MEM_CUR._writeREG && (IF_ID_NXT._regSRC1 == _mc->EX_MEM_CUR._decodedDST)))
         {
#ifdef BYPASS_MEM_EX_ENABLED
            IF_ID_NXT._bypSRC1 = BYPASS_MEM_EX;
#else
            toStall = TRUE;
#endif
         }
      }

      // Integer Ins
      if (!_mc->is_subreg && IF_ID_NXT._regSRC2 != 0 && IF_ID_NXT._regSRC2 != REG_DEFAULT)
      {
         if ((_mc->ID_EX_CUR._writeREG && (IF_ID_NXT._regSRC2 == _mc->ID_EX_CUR._decodedDST)))
         {
            if (_mc->ID_EX_CUR._memControl)
            { // if it is a load instructions, then do load interlock
               toStall = TRUE;
               _mc->_num_interlock++;
            }
            else
               IF_ID_NXT._bypSRC2 = BYPASS_EX_EX;
         }
         else if ((_mc->EX_MEM_CUR._writeREG && (IF_ID_NXT._regSRC2 == _mc->EX_MEM_CUR._decodedDST)))
         {
#ifdef BYPASS_MEM_EX_ENABLED
            IF_ID_NXT._bypSRC2 = BYPASS_MEM_EX;
#else
            toStall = TRUE;
#endif
         }
      }
   }

   // Check for hi/lo registers
   if (IF_ID_NXT._loWrite)
   {
      if (_mc->ID_EX_CUR._loWPort)
      {
         IF_ID_NXT._bypSRC1 = BYPASS_EX_EX;
      }
      else if (_mc->EX_MEM_CUR._loWPort)
      {
#ifdef BYPASS_MEM_EX_ENABLED
         IF_ID_NXT._bypSRC1 = BYPASS_MEM_EX;
#else
         toStall = TRUE;
#endif
      }
   }

   if (IF_ID_NXT._hiWrite)
   {
      if (_mc->ID_EX_CUR._hiWPort)
      {
         IF_ID_NXT._bypSRC1 = BYPASS_EX_EX;
      }
      else if (_mc->EX_MEM_CUR._hiWPort)
      {
#ifdef BYPASS_MEM_EX_ENABLED
         IF_ID_NXT._bypSRC1 = BYPASS_MEM_EX;
#else
         toStall = TRUE;
#endif
      }
   }

   return toStall;
}
#else
Bool Decode::check_stall(PipeReg &IF_ID_NXT)
{
   Bool toStall = FALSE;

   if (IF_ID_NXT._requiresFP)
   {
      // requires Floating Point Argument
      if (IF_ID_NXT._regSRC1 != REG_DEFAULT)
      {

         // check whether the value required is being produced in EX stage as of now, if yes, then stall
         if ((_mc->ID_EX_CUR._writeFREG && (IF_ID_NXT._regSRC1 == _mc->ID_EX_CUR._decodedDST)))
         {
            toStall = TRUE;
         }

         // check whether the value required is being produced in MEM stage as of now, if yes, then stall
         if ((_mc->EX_MEM_CUR._writeFREG && (IF_ID_NXT._regSRC1 == _mc->EX_MEM_CUR._decodedDST)))
         {
            toStall = TRUE;
         }
      }

      // Similarly check for SRC2
      if (IF_ID_NXT._regSRC2 != REG_DEFAULT)
      {
         if ((_mc->ID_EX_CUR._writeFREG && (IF_ID_NXT._regSRC2 == _mc->ID_EX_CUR._decodedDST)))
         {
            toStall = TRUE;
         }

         if ((_mc->EX_MEM_CUR._writeFREG && (IF_ID_NXT._regSRC2 == _mc->EX_MEM_CUR._decodedDST)))
         {
            toStall = TRUE;
         }
      }
   }
   else
   {
      // Integer Ins
      if (IF_ID_NXT._regSRC1 != 0 && IF_ID_NXT._regSRC1 != REG_DEFAULT)
      {
         if ((_mc->ID_EX_CUR._writeREG && (IF_ID_NXT._regSRC1 == _mc->ID_EX_CUR._decodedDST)))
         {
            toStall = TRUE;
         }

         if ((_mc->EX_MEM_CUR._writeREG && (IF_ID_NXT._regSRC1 == _mc->EX_MEM_CUR._decodedDST)))
         {
            toStall = TRUE;
         }
      }

      // Integer Ins
      if (IF_ID_NXT._regSRC2 != 0 && IF_ID_NXT._regSRC2 != REG_DEFAULT)
      {
         if ((_mc->ID_EX_CUR._writeREG && (IF_ID_NXT._regSRC2 == _mc->ID_EX_CUR._decodedDST)))
         {
            toStall = TRUE;
         }

         if ((_mc->EX_MEM_CUR._writeREG && (IF_ID_NXT._regSRC2 == _mc->EX_MEM_CUR._decodedDST)))
         {
            toStall = TRUE;
         }
      }
   }

   // Check for hi/lo registers
   if (IF_ID_NXT._loWrite && (_mc->ID_EX_CUR._loWPort || _mc->EX_MEM_CUR._loWPort))
      toStall = TRUE;
   if (IF_ID_NXT._hiWrite && (_mc->ID_EX_CUR._hiWPort || _mc->EX_MEM_CUR._hiWPort))
      toStall = TRUE;

   return toStall;
}
#endif

void Decode::MainLoop(void)
{
   unsigned int ins;
   while (1)
   {
      AWAIT_P_PHI0; // @posedge -- copy input and detect hazard

      _mc->IF_ID_NXT = _mc->IF_ID_CUR;
      ins = _mc->IF_ID_NXT._ins;

#ifdef BYPASS_ENABLED
      _mc->IF_ID_NXT._bypSRC1 = BYPASS_NONE;
      _mc->IF_ID_NXT._bypSRC2 = BYPASS_NONE;
#endif
      // Call Dec with FALSE, to check if instruction.
      _mc->Dec(ins, FALSE);
      if (!_mc->IF_ID_NXT._isIllegalOp)
      {
#ifdef BYPASS_ENABLED
         _mc->_toStall = check_bypass(_mc->IF_ID_NXT);
#else
         _mc->_toStall = check_stall(_mc->IF_ID_NXT);
#endif
      }
      else
      {
         _mc->_toStall = TRUE; // consider illegal op as NOP. for now i.e continue execution after it.
      }

      if (!_mc->_toStall && _mc->IF_ID_NXT._isSyscall)
      {
         _mc->_waitForSyscall = TRUE;
      }

      AWAIT_P_PHI1; // @negedge

      if (_mc->_toStall)
         _mc->ID_EX_CUR.clear(); // or ID_EX_CUR = NOP
      else
      {
         _mc->Dec(ins, TRUE);
#ifdef MIPC_DEBUG
         fprintf(_mc->_debugLog, "<%llu> Decoded ins %#x\n", SIM_TIME, ins);
#endif
         _mc->ID_EX_CUR = _mc->ID_EX_NXT;
      }
   }
}
