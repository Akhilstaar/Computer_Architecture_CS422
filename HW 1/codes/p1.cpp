#include <stdio.h>
#include "pin.H"
#include <iostream>
#include <fstream>

using namespace std;

ofstream OutFile;

// counters
enum CounterType
{
    LOAD,
    STORE,
    NOP,
    DIRECT_CALL,
    INDIRECT_CALL,
    RETURN,
    UNCOND_BR,
    COND_BR,
    LOGICAL,
    ROTATE_SHIFT,
    FLAGOP,
    VECTOR,
    CMOV,
    MMX_SSE,
    SYSCALL,
    FLOATING_POINT,
    OTHER,
    NUM_COUNTERS // 17
};

UINT64 Counters[NUM_COUNTERS] = {0};
UINT64 icount = 0;
UINT64 fast_forward_count = 0;

// This function is called on every instruction regardless of fast-forwarding.
// It updates the total instruction count and also switches on instrumentation when appropriate.
VOID InsCount(UINT32 nins)
{
    icount += nins;
}

ADDRINT Terminate(void)
{
    return (icount >= fast_forward_count + 1000000000);
}

ADDRINT FastForward(void)
{
    return (icount >= fast_forward_count && icount);
}

VOID AddToCounter(UINT64 *counter)
{
    *counter += 1;
}

VOID AddToMemCounter(UINT64 *counter, UINT32 delta)
{
    *counter += delta;
}

VOID PrintResults()
{
    OutFile << "\n===================PARTA==================\n";
    OutFile << "---- Instrumentation Results ----" << endl;
    OutFile << "Loads: " << Counters[LOAD] << '\n';
    OutFile << "Stores: " << Counters[STORE] << '\n';
    OutFile << "NOPs: " << Counters[NOP] << '\n';
    OutFile << "Direct calls: " << Counters[DIRECT_CALL] << '\n';
    OutFile << "Indirect calls: " << Counters[INDIRECT_CALL] << '\n';
    OutFile << "Returns: " << Counters[RETURN] << '\n';
    OutFile << "Unconditional branches: " << Counters[UNCOND_BR] << '\n';
    OutFile << "Conditional branches: " << Counters[COND_BR] << '\n';
    OutFile << "Logical operations: " << Counters[LOGICAL] << '\n';
    OutFile << "Rotate and shift: " << Counters[ROTATE_SHIFT] << '\n';
    OutFile << "Flag operations: " << Counters[FLAGOP] << '\n';
    OutFile << "Vector instructions: " << Counters[VECTOR] << '\n';
    OutFile << "Conditional moves: " << Counters[CMOV] << '\n';
    OutFile << "MMX and SSE instructions: " << Counters[MMX_SSE] << '\n';
    OutFile << "System calls: " << Counters[SYSCALL] << '\n';
    OutFile << "Floating-point: " << Counters[FLOATING_POINT] << '\n';
    OutFile << "Others: " << Counters[OTHER] << '\n';
    
    OutFile << "\n===================PARTA==================\n";
    // Calculate total instructions counted in the instrumentation phase
    UINT64 total_instructions = 0;
    for (int i = 0; i < NUM_COUNTERS; i++)
    {
        total_instructions += Counters[i];
    }
    
    // Calculate CPI
    double cpi = (total_instructions > 0) ? static_cast<double>(((Counters[LOAD] + Counters[STORE])*69 + total_instructions)) / total_instructions : 0;
    OutFile << "CPI: " << cpi << endl;

    OutFile.close();
    return;
}

VOID ExitRoutine()
{
    PrintResults();
    exit(0);
}

// Instrumentation function for instructions.
VOID BBTraceRoutine(TRACE trace, VOID *v)
{
    // For every basic block in the trace
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
        {
            if (INS_Category(ins) == XED_CATEGORY_INVALID)
                continue;

            // Categorize instructions based on XED categories.
            xed_category_enum_t cat = static_cast<xed_category_enum_t>(INS_Category(ins));

            // mem operand instructions
            UINT32 memOperands = INS_MemoryOperandCount(ins);
            if (memOperands > 0)
            {
                for (UINT32 memOp = 0; memOp < memOperands; memOp++)
                {
                    if (INS_MemoryOperandIsRead(ins, memOp))
                    {
                        UINT32 size = INS_MemoryOperandSize(ins, memOp);
                        UINT32 chunks = (size + 3) / 4;
                        INS_InsertIfPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                        INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToMemCounter, IARG_PTR, &Counters[LOAD], IARG_UINT32, chunks, IARG_END);
                    }
                    if (INS_MemoryOperandIsWritten(ins, memOp))
                    {
                        UINT32 size = INS_MemoryOperandSize(ins, memOp);
                        UINT32 chunks = (size + 3) / 4;
                        INS_InsertIfPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                        INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToMemCounter, IARG_PTR, &Counters[STORE], IARG_UINT32, chunks, IARG_END);
                    }
                }
            }

            if (cat == XED_CATEGORY_NOP)
            {
                INS_InsertIfPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[NOP], IARG_END);
            }
            else if (cat == XED_CATEGORY_CALL)
            {
                if (INS_IsDirectCall(ins))
                {
                    INS_InsertIfPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                    INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[DIRECT_CALL], IARG_END);
                }
                else
                {
                    INS_InsertIfPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                    INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[INDIRECT_CALL], IARG_END);
                }
            }
            else if (cat == XED_CATEGORY_RET)
            {
                INS_InsertIfPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[RETURN], IARG_END);
            }
            else if (cat == XED_CATEGORY_X87_ALU)
            {
                INS_InsertIfPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[FLOATING_POINT], IARG_END);
            }
            else if (cat == XED_CATEGORY_UNCOND_BR)
            {
                INS_InsertIfPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[UNCOND_BR], IARG_END);
            }
            else if (cat == XED_CATEGORY_COND_BR)
            {
                INS_InsertIfPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[COND_BR], IARG_END);
            }
            else if (cat == XED_CATEGORY_LOGICAL)
            {
                INS_InsertIfPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[LOGICAL], IARG_END);
            }
            else if (cat == XED_CATEGORY_ROTATE || cat == XED_CATEGORY_SHIFT)
            {
                INS_InsertIfPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[ROTATE_SHIFT], IARG_END);
            }
            else if (cat == XED_CATEGORY_FLAGOP)
            {
                INS_InsertIfPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[FLAGOP], IARG_END);
            }
            else if (cat == XED_CATEGORY_AVX || cat == XED_CATEGORY_AVX2 ||
                     cat == XED_CATEGORY_AVX2GATHER || cat == XED_CATEGORY_AVX512)
            {
                INS_InsertIfPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[VECTOR], IARG_END);
            }
            else if (cat == XED_CATEGORY_CMOV)
            {
                INS_InsertIfPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[CMOV], IARG_END);
            }
            else if (cat == XED_CATEGORY_MMX || cat == XED_CATEGORY_SSE)
            {
                INS_InsertIfPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[MMX_SSE], IARG_END);
            }
            else if (cat == XED_CATEGORY_SYSCALL)
            {
                INS_InsertIfPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[SYSCALL], IARG_END);
            }
            else
            {
                INS_InsertIfPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[OTHER], IARG_END);
            }
        }
        
        // Check for termination condition before any other instrumentation.
        BBL_InsertIfCall(bbl, IPOINT_BEFORE, (AFUNPTR)Terminate, IARG_END);
        BBL_InsertThenCall(bbl, IPOINT_BEFORE, (AFUNPTR)ExitRoutine, IARG_END);

        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)InsCount, IARG_UINT32, BBL_NumIns(bbl), IARG_END);
    }
}

VOID Fini(INT32 code, VOID *v)
{
    OutFile << "Execution terminated before completing instrumentation on all specified instructions." << endl;
    PrintResults();
    return;
}

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "inscount.out", "specify output file name");
KNOB<UINT64> KnobFastForwardTill(KNOB_MODE_OVERWRITE, "pintool", "f", "0", "fast forward by this many instructions");

INT32 Usage()
{
    cerr << "This tool categorizes instructions, counts them dynamically, and calculates CPI" << '\n';
    cerr << '\n' << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

int main(int argc, char *argv[])
{
    if (PIN_Init(argc, argv))
        return Usage();

    OutFile.open(KnobOutputFile.Value().c_str());
    fast_forward_count = KnobFastForwardTill.Value();

    // Add instrumentation functions.
    TRACE_AddInstrumentFunction(BBTraceRoutine, 0); // BB-level instrumentation

    PIN_AddFiniFunction(Fini, 0);

    PIN_StartProgram();

    return 0;
}