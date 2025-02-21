#include <stdio.h>
#include "pin.H"
#include <iostream>
#include <fstream>

using namespace std;

ofstream OutFile;

// counters
enum CounterType {
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

// Analysis function to increment counters by a delta
VOID AddToCounter(UINT64 *counter, UINT32 delta) {
    *counter += delta;
}

// Instrumentation function
VOID Instruction(INS ins, VOID *v) {

    // Check if it's a Type B (has memory operands)
    UINT32 memOperands = INS_MemoryOperandCount(ins);
    if (memOperands > 0) {
        for (UINT32 memOp = 0; memOp < memOperands; memOp++) {
            if (INS_MemoryOperandIsRead(ins, memOp)) {
                UINT32 size = INS_MemoryOperandSize(ins, memOp);
                UINT32 chunks = (size + 3) / 4;
                INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter,
                    IARG_PTR, &Counters[LOAD],
                    IARG_UINT32, chunks,
                    IARG_END);
            }
            if (INS_MemoryOperandIsWritten(ins, memOp)) {
                UINT32 size = INS_MemoryOperandSize(ins, memOp);
                UINT32 chunks = (size + 3) / 4;
                INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter,
                    IARG_PTR, &Counters[STORE],
                    IARG_UINT32, chunks,
                    IARG_END);
            }
        }
    }

    // Categorize into Type A
    xed_category_enum_t cat = INS_Category(ins);
    if (cat == XED_CATEGORY_NOP) {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter,
            IARG_PTR, &Counters[NOP],
            IARG_UINT32, 1,
            IARG_END);
    }
    else if (cat == XED_CATEGORY_CALL) {
        if (INS_IsDirectCall(ins)) {
            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter,
                IARG_PTR, &Counters[DIRECT_CALL],
                IARG_UINT32, 1,
                IARG_END);
        }
        else {
            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter,
                IARG_PTR, &Counters[INDIRECT_CALL],
                IARG_UINT32, 1,
                IARG_END);
        }
    }
    else if (cat == XED_CATEGORY_RET) {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter,
            IARG_PTR, &Counters[RETURN],
            IARG_UINT32, 1,
            IARG_END);
    }
    else if (cat == XED_CATEGORY_UNCOND_BR) {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter,
            IARG_PTR, &Counters[UNCOND_BR],
            IARG_UINT32, 1,
            IARG_END);
    }
    else if (cat == XED_CATEGORY_COND_BR) {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter,
            IARG_PTR, &Counters[COND_BR],
            IARG_UINT32, 1,
            IARG_END);
    }
    else if (cat == XED_CATEGORY_LOGICAL) {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter,
            IARG_PTR, &Counters[LOGICAL],
            IARG_UINT32, 1,
            IARG_END);
    }
    else if (cat == XED_CATEGORY_ROTATE || cat == XED_CATEGORY_SHIFT) {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter,
            IARG_PTR, &Counters[ROTATE_SHIFT],
            IARG_UINT32, 1,
            IARG_END);
    }
    else if (cat == XED_CATEGORY_FLAGOP) {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter,
            IARG_PTR, &Counters[FLAGOP],
            IARG_UINT32, 1,
            IARG_END);
    }
    else if (cat == XED_CATEGORY_AVX || cat == XED_CATEGORY_AVX2 || cat == XED_CATEGORY_AVX2GATHER || cat == XED_CATEGORY_AVX512) {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter,
            IARG_PTR, &Counters[VECTOR],
            IARG_UINT32, 1,
            IARG_END);
    }
    else if (cat == XED_CATEGORY_CMOV) {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter,
            IARG_PTR, &Counters[CMOV],
            IARG_UINT32, 1,
            IARG_END);
    }
    else if (cat == XED_CATEGORY_MMX || cat == XED_CATEGORY_SSE) {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter,
            IARG_PTR, &Counters[MMX_SSE],
            IARG_UINT32, 1,
            IARG_END);
    }
    else if (cat == XED_CATEGORY_SYSCALL) {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter,
            IARG_PTR, &Counters[SYSCALL],
            IARG_UINT32, 1,
            IARG_END);
    }
    else if (cat == XED_CATEGORY_X87_ALU) {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter,
            IARG_PTR, &Counters[FLOATING_POINT],
            IARG_UINT32, 1,
            IARG_END);
    }
    else {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter,
            IARG_PTR, &Counters[OTHER],
            IARG_UINT32, 1,
            IARG_END);
    }
}

// Fini function to write results
VOID Fini(INT32 code, VOID *v) {
    OutFile << "Loads: " << Counters[LOAD] << endl;
    OutFile << "Stores: " << Counters[STORE] << endl;
    OutFile << "NOPs: " << Counters[NOP] << endl;
    OutFile << "Direct calls: " << Counters[DIRECT_CALL] << endl;
    OutFile << "Indirect calls: " << Counters[INDIRECT_CALL] << endl;
    OutFile << "Returns: " << Counters[RETURN] << endl;
    OutFile << "Unconditional branches: " << Counters[UNCOND_BR] << endl;
    OutFile << "Conditional branches: " << Counters[COND_BR] << endl;
    OutFile << "Logical operations: " << Counters[LOGICAL] << endl;
    OutFile << "Rotate and shift: " << Counters[ROTATE_SHIFT] << endl;
    OutFile << "Flag operations: " << Counters[FLAGOP] << endl;
    OutFile << "Vector instructions: " << Counters[VECTOR] << endl;
    OutFile << "Conditional moves: " << Counters[CMOV] << endl;
    OutFile << "MMX and SSE instructions: " << Counters[MMX_SSE] << endl;
    OutFile << "System calls: " << Counters[SYSCALL] << endl;
    OutFile << "Floating-point: " << Counters[FLOATING_POINT] << endl;
    OutFile << "Others: " << Counters[OTHER] << endl;

    // Calculate total instructions
    UINT64 total = 0;
    for (int i = 0; i < NUM_COUNTERS; i++) {
        total += Counters[i];
    }
    OutFile << "Total instructions: " << total << endl;

    OutFile.close();
}

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "inscount.out", "specify output file name");

INT32 Usage() {
    cerr << "This tool categorizes instructions and counts them dynamically" << endl;
    cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

int main(int argc, char *argv[]) {
    if (PIN_Init(argc, argv)) return Usage();

    OutFile.open(KnobOutputFile.Value().c_str());

    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    PIN_StartProgram();
    
    return 0;
}