#include <iostream>
#include <fstream>
#include <set>
#include <iomanip> // For formatting output
#include "pin.H"
#include <cstdlib>

/* Macro and type definitions */
#define BILLION 1000000000

/* Branch Predictor Constants */
#define BIMODAL_SIZE 512
#define SAG_BHT_SIZE 1024
#define SAG_PHT_SIZE 512
#define GAG_PHT_SIZE 512
#define GSHARE_PHT_SIZE 512
#define HYBRID_META_SIZE 512
#define GLOBAL_HISTORY_BITS 9

using namespace std;

/* Branch Predictor Types */
typedef enum
{
    BP_FNBT = 0,          // Static Forward not-taken, Backward taken
    BP_BIMODAL,           // Bimodal predictor
    BP_SAG,               // SAg predictor
    BP_GAG,               // GAg predictor
    BP_GSHARE,            // gshare predictor
    BP_HYBRID_SAG_GAG,    // Hybrid of SAg and GAg
    BP_HYBRID_MAJORITY,   // Hybrid with majority vote
    BP_HYBRID_TOURNAMENT, // Hybrid with tournament predictor
    BP_COUNT
} BP_TYPE;

/* Branch Statistics */
typedef struct
{
    UINT64 total;          // Total number of branches
    UINT64 forward;        // Forward branches
    UINT64 backward;       // Backward branches
    UINT64 mispredictions; // Total mispredictions
    UINT64 forward_misp;   // Forward branch mispredictions
    UINT64 backward_misp;  // Backward branch mispredictions
} BRANCH_STATS;

/* Global variables */
std::ostream *out = &cerr;

ADDRINT fastForwardDone = 0;
UINT64 icount = 0; // number of dynamically executed instructions

UINT64 fastForwardIns; // number of instruction to fast forward
UINT64 maxIns;         // maximum number of instructions to simulate

/* Global variables for branch predictors */
// Branch direction predictors
UINT8 bimodal_pht[BIMODAL_SIZE];                // 2-bit saturating counters for bimodal
UINT16 sag_bht[SAG_BHT_SIZE];                   // Branch history table for SAg (9-bit per entry)
UINT8 sag_pht[SAG_PHT_SIZE];                    // Pattern history table for SAg (2-bit counters)
UINT16 gag_ghr;                                 // Global history register for GAg
UINT8 gag_pht[GAG_PHT_SIZE];                    // Pattern history table for GAg (3-bit counters)
UINT8 gshare_pht[GSHARE_PHT_SIZE];              // Pattern history table for gshare (3-bit counters)
UINT8 hybrid_meta[HYBRID_META_SIZE];            // Meta-predictor table (2-bit counters)
UINT8 hybrid_meta_sag_gag[HYBRID_META_SIZE];    // Meta-predictor for SAg vs GAg
UINT8 hybrid_meta_gag_gshare[HYBRID_META_SIZE]; // Meta-predictor for GAg vs gshare
UINT8 hybrid_meta_gshare_sag[HYBRID_META_SIZE]; // Meta-predictor for gshare vs SAg

// Statistics
BRANCH_STATS bp_stats[BP_COUNT]; // Stats for branch predictors

/* Command line switches */
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "", "specify file name for HW1 output");
KNOB<UINT64> KnobFastForward(KNOB_MODE_WRITEONCE, "pintool", "f", "0", "number of instructions to fast forward in billions");

/* Utilities */

/* Print out help message. */
INT32 Usage()
{
    cerr << "CS422 Homework 2" << endl;
    cerr << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

VOID InsCount(UINT32 c)
{
    icount += c;
}

ADDRINT FastForward(void)
{
    return (icount >= fastForwardIns && icount < maxIns);
}

VOID FastForwardDone(void)
{
    fastForwardDone = 1;
}

ADDRINT IsFastForwardDone(void)
{
    return fastForwardDone;
}

ADDRINT Terminate(void)
{
    return (icount >= maxIns);
}

/* Helper functions for saturating counter operations */
inline UINT8 SatIncrement2bit(UINT8 counter)
{
    return (counter < 3) ? counter + 1 : 3;
}

inline UINT8 SatDecrement2bit(UINT8 counter)
{
    return (counter > 0) ? counter - 1 : 0;
}

inline UINT8 SatIncrement3bit(UINT8 counter)
{
    return (counter < 7) ? counter + 1 : 7;
}

inline UINT8 SatDecrement3bit(UINT8 counter)
{
    return (counter > 0) ? counter - 1 : 0;
}

VOID PrintResults(void)
{
    // *out << "===============================================" << endl;
    // Print branch predictor statistics
    *out << endl;
    *out << "Branch Predictor Statistics:" << endl;
    *out << "==========================================================" << endl;
    *out << "Predictor              Forward Branches      Backward Branches       Overall" << endl;
    *out << "                       Miss Rate             Miss Rate              Miss Rate" << endl;
    *out << "----------------------------------------------------------" << endl;

    const char *bp_names[] = {
        "FNBT",
        "Bimodal",
        "SAg",
        "GAg",
        "gshare",
        "Hybrid SAg-GAg",
        "Hybrid Majority",
        "Hybrid Tournament"};

    for (UINT32 i = 0; i < BP_COUNT; i++)
    {
        double forward_rate = bp_stats[i].forward > 0 ? (double)bp_stats[i].forward_misp / bp_stats[i].forward : 0;
        double backward_rate = bp_stats[i].backward > 0 ? (double)bp_stats[i].backward_misp / bp_stats[i].backward : 0;
        double overall_rate = bp_stats[i].total > 0 ? (double)bp_stats[i].mispredictions / bp_stats[i].total : 0;

        *out << std::left << std::setw(20) << bp_names[i] << " "
             << std::fixed << std::setprecision(6) << std::setw(20) << forward_rate << " "
             << std::fixed << std::setprecision(6) << std::setw(20) << backward_rate << " "
             << std::fixed << std::setprecision(6) << overall_rate << endl;
    }

    // *out << endl;
    // *out << "Indirect Branch Target Predictor Statistics:" << endl;
    // *out << "==========================================================" << endl;
    // *out << "Predictor               Miss Rate                BTB Miss Rate" << endl;
    // *out << "----------------------------------------------------------" << endl;

    // const char *tp_names[] = {
    //     "BTB indexed by PC",
    //     "BTB indexed by PC+history hash"};

    // for (UINT32 i = 0; i < TP_COUNT; i++)
    // {
    //     double miss_rate = tp_stats[i].total > 0 ? (double)tp_stats[i].mispredictions / tp_stats[i].total : 0;
    //     double btb_miss_rate = tp_stats[i].total > 0 ? (double)tp_btb_misses[i] / tp_stats[i].total : 0;

    //     *out << std::left << std::setw(25) << tp_names[i] << " "
    //          << std::fixed << std::setprecision(6) << std::setw(25) << miss_rate
    //          << std::fixed << std::setprecision(6) << btb_miss_rate << endl;
    // }

    *out << "===============================================" << endl;

    return;
}

/* Branch prediction functions */
BOOL PredictFNBT(ADDRINT pc, ADDRINT target)
{
    // Static predictor: forward branches not taken, backward branches taken
    return (target <= pc); // Backward branch is taken
}

BOOL PredictBimodal(ADDRINT pc)
{
    UINT32 index = pc % BIMODAL_SIZE;
    return bimodal_pht[index] >= 2; // Predict taken if counter >= 2
}

BOOL PredictSAg(ADDRINT pc)
{
    UINT32 bht_index = pc % SAG_BHT_SIZE;
    UINT16 history = sag_bht[bht_index];
    UINT32 pht_index = history % SAG_PHT_SIZE;
    return sag_pht[pht_index] >= 2; // Predict taken if counter >= 2
}

BOOL PredictGAg()
{
    UINT32 pht_index = gag_ghr % GAG_PHT_SIZE;
    return gag_pht[pht_index] >= 4; // Predict taken if counter >= 4 (3-bit counter midpoint)
}

BOOL PredictGshare(ADDRINT pc)
{
    UINT32 index = (pc ^ gag_ghr) % GSHARE_PHT_SIZE;
    return gshare_pht[index] >= 4; // Predict taken if counter >= 4 (3-bit counter midpoint)
}

BOOL PredictHybridSAgGAg(ADDRINT pc)
{
    UINT32 meta_index = gag_ghr % HYBRID_META_SIZE;
    if (hybrid_meta_sag_gag[meta_index] >= 2)
    {
        return PredictGAg(); // Use GAg prediction if meta-counter >= 2
    }
    else
    {
        return PredictSAg(pc); // Use SAg prediction if meta-counter < 2
    }
}

BOOL PredictHybridMajority(ADDRINT pc)
{
    BOOL sag_pred = PredictSAg(pc);
    BOOL gag_pred = PredictGAg();
    BOOL gshare_pred = PredictGshare(pc);

    // Simple majority vote
    if ((sag_pred && gag_pred) || (sag_pred && gshare_pred) || (gag_pred && gshare_pred))
    {
        return true; // At least 2 predictors say taken
    }
    else
    {
        return false; // At least 2 predictors say not taken
    }
}

BOOL PredictHybridTournament(ADDRINT pc)
{
    UINT32 meta_index = gag_ghr % HYBRID_META_SIZE;

    // First tournament: SAg vs GAg
    BOOL first_winner;
    if (hybrid_meta_sag_gag[meta_index] >= 2)
    {
        first_winner = PredictGAg();
    }
    else
    {
        first_winner = PredictSAg(pc);
    }

    // Second tournament: first_winner vs gshare
    if (hybrid_meta_gag_gshare[meta_index] >= 2)
    {
        return PredictGshare(pc);
    }
    else
    {
        return first_winner;
    }
}

/* Update branch predictor state */
VOID UpdateBranchPredictors(ADDRINT pc, BOOL taken)
{
    // Update Bimodal
    UINT32 bimodal_index = pc % BIMODAL_SIZE;
    if (taken)
    {
        bimodal_pht[bimodal_index] = SatIncrement2bit(bimodal_pht[bimodal_index]);
    }
    else
    {
        bimodal_pht[bimodal_index] = SatDecrement2bit(bimodal_pht[bimodal_index]);
    }

    // Update SAg
    UINT32 sag_bht_index = pc % SAG_BHT_SIZE;
    UINT16 sag_history = sag_bht[sag_bht_index];
    UINT32 sag_pht_index = sag_history % SAG_PHT_SIZE;

    if (taken)
    {
        sag_pht[sag_pht_index] = SatIncrement2bit(sag_pht[sag_pht_index]);
    }
    else
    {
        sag_pht[sag_pht_index] = SatDecrement2bit(sag_pht[sag_pht_index]);
    }

    // Update SAg history
    sag_bht[sag_bht_index] = ((sag_history << 1) | taken) & ((1 << GLOBAL_HISTORY_BITS) - 1);

    // Update GAg
    UINT32 gag_pht_index = gag_ghr % GAG_PHT_SIZE;
    if (taken)
    {
        gag_pht[gag_pht_index] = SatIncrement3bit(gag_pht[gag_pht_index]);
    }
    else
    {
        gag_pht[gag_pht_index] = SatDecrement3bit(gag_pht[gag_pht_index]);
    }

    // Update gshare
    UINT32 gshare_index = (pc ^ gag_ghr) % GSHARE_PHT_SIZE;
    if (taken)
    {
        gshare_pht[gshare_index] = SatIncrement3bit(gshare_pht[gshare_index]);
    }
    else
    {
        gshare_pht[gshare_index] = SatDecrement3bit(gshare_pht[gshare_index]);
    }

    // Update Hybrid Meta-predictors
    UINT32 meta_index = gag_ghr % HYBRID_META_SIZE;
    BOOL sag_correct = (PredictSAg(pc) == taken);
    BOOL gag_correct = (PredictGAg() == taken);
    BOOL gshare_correct = (PredictGshare(pc) == taken);

    // Update SAg vs GAg meta-predictor
    if (sag_correct && !gag_correct)
    {
        hybrid_meta_sag_gag[meta_index] = SatDecrement2bit(hybrid_meta_sag_gag[meta_index]);
    }
    else if (!sag_correct && gag_correct)
    {
        hybrid_meta_sag_gag[meta_index] = SatIncrement2bit(hybrid_meta_sag_gag[meta_index]);
    }

    // Update GAg vs gshare meta-predictor
    if (gag_correct && !gshare_correct)
    {
        hybrid_meta_gag_gshare[meta_index] = SatDecrement2bit(hybrid_meta_gag_gshare[meta_index]);
    }
    else if (!gag_correct && gshare_correct)
    {
        hybrid_meta_gag_gshare[meta_index] = SatIncrement2bit(hybrid_meta_gag_gshare[meta_index]);
    }

    // Update gshare vs SAg meta-predictor
    if (gshare_correct && !sag_correct)
    {
        hybrid_meta_gshare_sag[meta_index] = SatDecrement2bit(hybrid_meta_gshare_sag[meta_index]);
    }
    else if (!gshare_correct && sag_correct)
    {
        hybrid_meta_gshare_sag[meta_index] = SatIncrement2bit(hybrid_meta_gshare_sag[meta_index]);
    }

    // Finally, update global history register
    gag_ghr = ((gag_ghr << 1) | taken) & ((1 << GLOBAL_HISTORY_BITS) - 1);
}

/* Analysis function for conditional branches */
VOID AnalyzeConditionalBranch(ADDRINT pc, BOOL taken, BOOL isForward)
{
    // Update stats for each predictor
    for (INT32 i = 0; i < BP_COUNT; i++)
    {
        bp_stats[i].total++;
        if (isForward)
        {
            bp_stats[i].forward++;
        }
        else
        {
            bp_stats[i].backward++;
        }

        BOOL prediction = false;

        // Get prediction based on predictor type
        switch (i)
        {
        case BP_FNBT:
            prediction = !isForward;
            break;
        case BP_BIMODAL:
            prediction = PredictBimodal(pc);
            break;
        case BP_SAG:
            prediction = PredictSAg(pc);
            break;
        case BP_GAG:
            prediction = PredictGAg();
            break;
        case BP_GSHARE:
            prediction = PredictGshare(pc);
            break;
        case BP_HYBRID_SAG_GAG:
            prediction = PredictHybridSAgGAg(pc);
            break;
        case BP_HYBRID_MAJORITY:
            prediction = PredictHybridMajority(pc);
            break;
        case BP_HYBRID_TOURNAMENT:
            prediction = PredictHybridTournament(pc);
            break;
        }

        // Check for misprediction
        if (prediction != taken)
        {
            bp_stats[i].mispredictions++;
            if (isForward)
            {
                bp_stats[i].forward_misp++;
            }
            else
            {
                bp_stats[i].backward_misp++;
            }
        }
    }

    // Update predictor state with actual outcome
    UpdateBranchPredictors(pc, taken);
}

/* BTB Access functions */
// INT32 FindBTBEntry(ADDRINT pc, INT32 predictor_type)
// {
//     UINT32 set = pc % BTB_SETS;
//     BTB_ENTRY *btb = (predictor_type == TP_BTB_PC) ? btb_pc[set] : btb_hash[set];

//     for (INT32 i = 0; i < BTB_WAYS; i++)
//     {
//         if (btb[i].valid && btb[i].tag == pc)
//         {
//             // Update LRU - this entry becomes most recently used
//             for (INT32 j = 0; j < BTB_WAYS; j++)
//             {
//                 if (j != i && btb[j].valid && btb[j].lru < btb[i].lru)
//                 {
//                     btb[j].lru++;
//                 }
//             }
//             btb[i].lru = 0;
//             return i;
//         }
//     }

//     return -1; // Entry not found
// }

// INT32 FindLRUEntry(INT32 set, INT32 predictor_type)
// {
//     BTB_ENTRY *btb = (predictor_type == TP_BTB_PC) ? btb_pc[set] : btb_hash[set];
//     INT32 lru_way = 0;
//     UINT8 max_lru = 0;

//     for (INT32 i = 0; i < BTB_WAYS; i++)
//     {
//         if (!btb[i].valid)
//         {
//             return i; // Return first invalid entry
//         }
//         if (btb[i].lru > max_lru)
//         {
//             max_lru = btb[i].lru;
//             lru_way = i;
//         }
//     }

//     return lru_way; // Return LRU entry
// }

// ADDRINT PredictTarget(ADDRINT pc, INT32 predictor_type)
// {
//     UINT32 index;

//     if (predictor_type == TP_BTB_PC)
//     {
//         index = pc % BTB_SETS;
//     }
//     else
//     {
//         // Hash of PC and global history
//         index = (pc ^ tp_ghr) % BTB_SETS;
//     }

//     INT32 way = FindBTBEntry(pc, predictor_type);

//     if (way != -1)
//     {
//         if (predictor_type == TP_BTB_PC)
//         {
//             return btb_pc[index][way].target;
//         }
//         else
//         {
//             return btb_hash[index][way].target;
//         }
//     }

//     // BTB miss - implicitly predict next instruction
//     tp_btb_misses[predictor_type]++;
//     return 0; // Will be detected as a misprediction in the analysis routine
// }

// VOID UpdateBTB(ADDRINT pc, ADDRINT target, INT32 predictor_type)
// {
//     UINT32 index;

//     if (predictor_type == TP_BTB_PC)
//     {
//         index = pc % BTB_SETS;
//     }
//     else
//     {
//         // Hash of PC and global history
//         index = (pc ^ tp_ghr) % BTB_SETS;
//     }

//     INT32 way = FindBTBEntry(pc, predictor_type);

//     if (way == -1)
//     {
//         // Entry not found, allocate a new one
//         way = FindLRUEntry(index, predictor_type);

//         // Update all LRU counters
//         BTB_ENTRY *btb = (predictor_type == TP_BTB_PC) ? btb_pc[index] : btb_hash[index];
//         for (INT32 i = 0; i < BTB_WAYS; i++)
//         {
//             if (btb[i].valid)
//             {
//                 btb[i].lru++;
//             }
//         }
//     }

//     // Update entry
//     if (predictor_type == TP_BTB_PC)
//     {
//         btb_pc[index][way].valid = true;
//         btb_pc[index][way].tag = pc;
//         btb_pc[index][way].target = target;
//         btb_pc[index][way].lru = 0;
//     }
//     else
//     {
//         btb_hash[index][way].valid = true;
//         btb_hash[index][way].tag = pc;
//         btb_hash[index][way].target = target;
//         btb_hash[index][way].lru = 0;
//     }

//     // Update global history for TP_BTB_HASH
//     if (predictor_type == TP_BTB_HASH)
//     {
//         tp_ghr = ((tp_ghr << 1) | 1) & ((1 << GLOBAL_HISTORY_BITS) - 1); // Treat as taken
//     }
// }

/* Analysis function for indirect control transfers */
// VOID AnalyzeIndirectBranch(ADDRINT pc, ADDRINT target)
// {
//     // Update stats for each target predictor
//     for (INT32 i = 0; i < TP_COUNT; i++)
//     {
//         tp_stats[i].total++;

//         ADDRINT predicted_target = PredictTarget(pc, i);

//         // Check for misprediction
//         if (predicted_target != target && predicted_target != 0)
//         {
//             tp_stats[i].mispredictions++;
//         }

//         // Update BTB with actual target
//         UpdateBTB(pc, target, i);
//     }
// }

VOID ExitRoutine()
{
    PrintResults();
    exit(0);
}

/* Instruction instrumentation routine */
VOID Trace(TRACE trace, VOID *v)
{
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        BBL_InsertIfCall(bbl, IPOINT_BEFORE, (AFUNPTR)Terminate, IARG_END);
        BBL_InsertThenCall(bbl, IPOINT_BEFORE, (AFUNPTR)ExitRoutine, IARG_END);

        BBL_InsertIfCall(bbl, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
        BBL_InsertThenCall(bbl, IPOINT_BEFORE, (AFUNPTR)FastForwardDone, IARG_END);

        for (INS ins = BBL_InsHead(bbl);; ins = INS_Next(ins))
        {

            /* For conditional branches */
            if (INS_Category(ins) == XED_CATEGORY_COND_BR)
            {
                ADDRINT target = INS_DirectControlFlowTargetAddress(ins);
                BOOL isForward = (target > INS_Address(ins));

                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)IsFastForwardDone, IARG_END);
                INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)AnalyzeConditionalBranch,
                                   IARG_ADDRINT, INS_Address(ins), // PC
                                   IARG_BRANCH_TAKEN,              // Whether branch is taken
                                   IARG_BOOL, isForward,           // Whether branch is forward
                                   IARG_END);
            }

            /* For indirect control transfers */
            // if (INS_IsIndirectControlFlow(ins))
            // {
            //     INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)IsFastForwardDone, IARG_END);
            //     INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)AnalyzeIndirectBranch,
            //                        IARG_ADDRINT, INS_Address(ins), // PC
            //                        IARG_BRANCH_TARGET_ADDR,        // Actual target
            //                        IARG_END);
            // }

            if (ins == BBL_InsTail(bbl))
                break;
        }

        /* Called for each BBL */
        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)InsCount, IARG_UINT32, BBL_NumIns(bbl), IARG_END);
    }
}

/* Fini routine */
VOID Fini(INT32 code, VOID *v)
{
    *out << "Execution terminated before completing instrumentation on all specified instructions." << endl;
    PrintResults();
    return;
}

int main(int argc, char *argv[])
{
    // Initialize PIN library. Print help message if -h(elp) is specified
    // in the command line or the command line is invalid
    if (PIN_Init(argc, argv))
        return Usage();

    /* Set number of instructions to fast forward and simulate */
    fastForwardIns = KnobFastForward.Value() * BILLION;
    maxIns = fastForwardIns + BILLION;

    string fileName = KnobOutputFile.Value();

    if (!fileName.empty())
        out = new std::ofstream(fileName.c_str());

    // Register function to be called to instrument instructions
    TRACE_AddInstrumentFunction(Trace, 0);

    // Register function to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    cerr << "===============================================" << endl;
    cerr << "This application is instrumented by HW2" << endl;
    if (!KnobOutputFile.Value().empty())
        cerr << "See file " << KnobOutputFile.Value() << " for analysis results" << endl;
    cerr << "===============================================" << endl;

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}