
#include <limits.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "regs.h"
#include "memory.h"
#include "loader.h"
#include "syscall.h"
#include "dlite.h"
#include "options.h"
#include "stats.h"
#include "sim.h"
#include "decode.def"

#include "instr.h"

/* PARAMETERS OF THE TOMASULO'S ALGORITHM */

#define INSTR_QUEUE_SIZE         16

#define RESERV_INT_SIZE    5
#define RESERV_FP_SIZE     3
#define FU_INT_SIZE        3
#define FU_FP_SIZE         1

#define FU_INT_LATENCY     5
#define FU_FP_LATENCY      7

/* IDENTIFYING INSTRUCTIONS */

//unconditional branch, jump or call
#define IS_UNCOND_CTRL(op) (MD_OP_FLAGS(op) & F_CALL || \
                         MD_OP_FLAGS(op) & F_UNCOND)

//conditional branch instruction
#define IS_COND_CTRL(op) (MD_OP_FLAGS(op) & F_COND)

//floating-point computation
#define IS_FCOMP(op) (MD_OP_FLAGS(op) & F_FCOMP)

//integer computation
#define IS_ICOMP(op) (MD_OP_FLAGS(op) & F_ICOMP)

//load instruction
#define IS_LOAD(op)  (MD_OP_FLAGS(op) & F_LOAD)

//store instruction
#define IS_STORE(op) (MD_OP_FLAGS(op) & F_STORE)

//trap instruction
#define IS_TRAP(op) (MD_OP_FLAGS(op) & F_TRAP) 

#define USES_INT_FU(op) (IS_ICOMP(op) || IS_LOAD(op) || IS_STORE(op))
#define USES_FP_FU(op) (IS_FCOMP(op))

#define WRITES_CDB(op) (IS_ICOMP(op) || IS_LOAD(op) || IS_FCOMP(op))

//#define DEFAULT_VALUES ()

/* FOR DEBUGGING */

//prints info about an instruction
#define PRINT_INST(out,instr,str,cycle)	\
  myfprintf(out, "%d: %s", cycle, str);		\
  md_print_insn(instr->inst, instr->pc, out); \
  myfprintf(stdout, "(%d)\n",instr->index);

#define PRINT_REG(out,reg,str,instr) \
  myfprintf(out, "reg#%d %s ", reg, str);	\
  md_print_insn(instr->inst, instr->pc, out); \
  myfprintf(stdout, "(%d)\n",instr->index);

/* VARIABLES */

//instruction queue for tomasulo
static instruction_t* instr_queue[INSTR_QUEUE_SIZE];
//number of instructions in the instruction queue
static int instr_queue_size = 0;

//reservation stations (each reservation station entry contains a pointer to an instruction)
static instruction_t* reservINT[RESERV_INT_SIZE];
static instruction_t* reservFP[RESERV_FP_SIZE];

//functional units
static instruction_t* fuINT[FU_INT_SIZE];
static instruction_t* fuFP[FU_FP_SIZE];

//common data bus
static instruction_t* commonDataBus = NULL;

//The map table keeps track of which instruction produces the value for each register
static instruction_t* map_table[MD_TOTAL_REGS];

//the index of the last instruction fetched
static int fetch_index = 0;

/* FUNCTIONAL UNITS */


/* RESERVATION STATIONS */

/* ECE552 Assignment 3 - BEGIN CODE*/

static int done_insn_count = 0;
static int num_branch_insn = 0;
static int num_TRAPS = 0;
static int num_store_insn = 0;

/*
 * Description:
 *  Checks if there is a free Reservation Station for either an INT or FP op
 * Inputs:
 *  the current cycle and the op type
 * Returns: 0 if no appropriate RS entry available
 *          1-5/1-3 correponding to the reservation station num that is available
 */
static int is_RS_available(enum md_opcode curr_op){
  int is_available = -1;
  int i;
  if( USES_INT_FU(curr_op) ) {
    for (i = 0; i < RESERV_INT_SIZE; i++) {
      if( reservINT[i] == NULL ) {
        is_available = i;
        break;
      }
    }
  }
  else if ( USES_FP_FU(curr_op) ) {
    for (i = 0; i < RESERV_FP_SIZE; i++) {
      if( reservFP[i] == NULL ) {
        is_available = i;
        break;
      }
    }
  }
  return is_available;
}

static int is_FU_available(enum md_opcode curr_op, int reservedFUs[FU_INT_SIZE]) {
  int is_available = -1;
  int i,j;
  bool FU_in_use = false;
  if( USES_INT_FU(curr_op) ) {
    for( i = 0; i < FU_INT_SIZE; i++ ) {
      FU_in_use = false;
      if( fuINT[i] == NULL ) {
        for( j = 0; j < FU_INT_SIZE; j++ ){
          if( reservedFUs[j] == i ) {
            FU_in_use = true;
            break;
          }
        }

        if(!FU_in_use) {
          is_available = i;
          break;
        }
      }
    }
    //if( (reservedFUs[0] != -1) && (reservedFUs[1] != -1) && (reservedFUs[2] != -1)) {
    //  is_available = 2;
    //}
  }
  else if( USES_FP_FU(curr_op) ) {
    for( i = 0; i < FU_FP_SIZE; i++ ) {
      if( fuFP[i] == NULL ) {
        is_available = i;
        break;
      }
    }
  }
  return is_available;
}

void remove_from_IFQ() {
  int i;
  //printf(" - instr_queue_size at beg of remove from IFQ is %d\n", instr_queue_size);
  instr_queue_size = instr_queue_size-1;
  /* remove head by shifting all instructions to the front of the queue one by one */
  for( i = 0; i < INSTR_QUEUE_SIZE; i++ ){
    instr_queue[i] = instr_queue[i+1];
    //printf(" - instr_queue_size at instr_q_idx%d is %d\n", i,instr_queue_size);
  }
  //printf(" - instr_queue_size before last entry set to null is %d\n", instr_queue_size);
  //instr_queue[INSTR_QUEUE_SIZE] = NULL;
}

void deallocate_RS_FU(instruction_t* insn) {
  int i;
  if( USES_INT_FU(insn->op) ) {
    for( i = 0; i < RESERV_INT_SIZE; i++ ) {
      if( reservINT[i] && (reservINT[i]->index == insn->index) ){
        reservINT[i] = NULL;
      }
    }
    for( i = 0; i < FU_INT_SIZE; i++ ) {
      if( fuINT[i] && (fuINT[i]->index == insn->index) ){
        fuINT[i] = NULL;
      }
    }
  }
  else if( USES_FP_FU(insn->op) ) {
    for( i = 0; i < RESERV_FP_SIZE; i++ ) {
      if( reservFP[i] && (reservFP[i]->index == insn->index) ){
        reservFP[i] = NULL;
      }
    }
    for( i = 0; i < FU_FP_SIZE; i++ ) {
      if( fuFP[i] && (fuFP[i]->index == insn->index) ){
        fuFP[i] = NULL;
      }
    }
  }

}

/* ECE552 Assignment 3 - END CODE*/

/* 
 * Description: 
 * 	Checks if simulation is done by finishing the very last instruction
 *      Remember that simulation is done only if the entire pipeline is empty
 * Inputs:
 * 	sim_insn: the total number of instructions simulated
 * Returns:
 * 	True: if simulation is finished
 */
static bool is_simulation_done(counter_t sim_insn) {

  /* ECE552: YOUR CODE GOES HERE */
  /* We done when all instructions reach the WB stage (when they write to the CDB);
    OR in the case of stores they complete one cycle earlier (at the end of execute) */ 

  // simulation done if trace is NULL and IFQ is empty
  bool sim_done = false;
  //if( done_insn_count >= sim_insn && !instr_queue_size && (fetch_index >= sim_insn) && is_IFQ_empty() ) {
  if( done_insn_count >= sim_insn && (fetch_index >= sim_insn) ) {
    sim_done = true;
  }
  //printf("[DONE_INSN_COUNT] %d\n", done_insn_count);

  return sim_done;
}

/* 
 * Description: 
 * 	Retires the instruction from writing to the Common Data Bus
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void CDB_To_retire(int current_cycle) {

  /* ECE552: YOUR CODE GOES HERE */
  /* check the insn written at CDB and set the registers in the RS, and maptable to ready */
  int i;
  instruction_t* curr_insn = commonDataBus;
  if( curr_insn ) {
    for( i = 0; i < RESERV_INT_SIZE; i++ ) {
      if( reservINT[i] ) {
        if( curr_insn == reservINT[i]->Q[0] ) reservINT[i]->Q[0] = NULL;
        if( curr_insn == reservINT[i]->Q[1] ) reservINT[i]->Q[1] = NULL;
        if( curr_insn == reservINT[i]->Q[2] ) reservINT[i]->Q[2] = NULL;
      }
    }

    for( i = 0; i < RESERV_FP_SIZE; i++ ) {
      if( reservFP[i] ) {
        if( curr_insn == reservFP[i]->Q[0] ) reservFP[i]->Q[0] = NULL;
        if( curr_insn == reservFP[i]->Q[1] ) reservFP[i]->Q[1] = NULL;
        if( curr_insn == reservFP[i]->Q[2] ) reservFP[i]->Q[2] = NULL;
      }
    }

    /* remove Tag from Map Table */
    for(  i = 0; i < MD_TOTAL_REGS; i++ ) {
      if( map_table[i] && ( map_table[i]->index == curr_insn->index ) ) {
        map_table[i] = NULL;
        //break;
      }
    }

    //deallocate_RS_FU(curr_insn);
    commonDataBus = NULL;
    done_insn_count++;
  }


}


/* 
 * Description: 
 * 	Moves an instruction from the execution stage to common data bus (if possible)
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void execute_To_CDB(int current_cycle) {

  /* ECE552: YOUR CODE GOES HERE */
  /* insn in FU executes for the latency amount prescribed
   * CDB free next cycle; oldest instruction that can use it or a store (which does not use it, it finishes here)*/
  int i;
  instruction_t* oldest_insn = NULL;
  for( i = 0; i < FU_INT_SIZE; i++ ) {
    if( fuINT[i] ) {
      if( current_cycle >= fuINT[i]->tom_execute_cycle + FU_INT_LATENCY ) {
        /* if the insn is a STORE that does not write to the CDB*/
        if( !WRITES_CDB(fuINT[i]->op) ) {
          num_store_insn++;
          done_insn_count++;
          /* can free its RS and FU entry since the STORE insn does not write to the CDB
           * and practically retires at this stage */
          deallocate_RS_FU(fuINT[i]);
        }
        else {
          if( !oldest_insn ) oldest_insn = fuINT[i];
          else if( oldest_insn->index > fuINT[i]->index ) {
            oldest_insn = fuINT[i];
          }
        }
      }
    }
  }

  for( i = 0; i < FU_FP_SIZE; i++ ) {
    if( fuFP[i] ) {
      if( current_cycle >= fuFP[i]->tom_execute_cycle + FU_FP_LATENCY ) {
        // a STORE cannot be here since it only uses INT units
        if( WRITES_CDB(fuFP[i]->op) ) {
          if( !oldest_insn ) oldest_insn = fuFP[i];
          else if( oldest_insn->index > fuFP[i]->index ) {
            oldest_insn = fuFP[i];
          }
        }
      }
    }
  }

  if( oldest_insn ) {
    commonDataBus = oldest_insn;
    oldest_insn->tom_cdb_cycle = current_cycle;
    deallocate_RS_FU(oldest_insn);
    //printf("[EX_to_CDB] In cycle %d, oldest insn idx: %d, output reg is %d\n", current_cycle, oldest_insn->index, oldest_insn->r_out[0]);
    /* can deallocate the FU for the oldest insn that won access to the CDB_to_retire */
  }

}

/* 
 * Description: 
 * 	Moves instruction(s) from the issue to the execute stage (if possible). We prioritize old instructions
 *      (in program order) over new ones, if they both contend for the same functional unit.
 *      All RAW dependences need to have been resolved with stalls before an instruction enters execute.
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void issue_To_execute(int current_cycle) {
  /* ECE552: YOUR CODE GOES HERE */
  /* enter execution once all its dependences have been resolved */
  /* loads and stores do not have to wait for each other if accessing the same memory address */

  /* insn wait at this stage for all RAW hazards to be resolved*/
  /* look at oldest instruction in the reservation station; oldest by dispatch cycle */

  /* wait if source operands not ready (RAW hazard) */
  /* also wait if no FUs available*/

  /* All input operands are available with at most one of them being in the CDB this cycle
   * FU available in next cycle and this insn is the oldest one wanting to use it*/
  
  int FU_idx;
  int FU_idx_1;
  int FU_idx_2;
  int FU_idx_3;
  int i,j;

  //instruction_t* all_exec_ready_int_insn[FU_INT_SIZE];
  //for (i = 0; i < FU_INT_SIZE; i++) {
  //  all_exec_ready_int_insn[i] = NULL;
  //}

  int reservedFUs[FU_INT_SIZE] = {-1, -1, -1};
  instruction_t* oldest_insn = NULL;
  instruction_t* oldest_insn_2 = NULL;
  instruction_t* oldest_insn_3 = NULL;

  for( i = 0; i < RESERV_INT_SIZE; i++ ) {
    if( reservINT[i] != NULL ) {
      if( reservINT[i]->tom_execute_cycle > 0 ) continue;
      //printf("[S_TO_EX]In cycle %d - INT, RS[%d]->tom_issue_cycle %d\n", current_cycle, i, reservINT[i]->tom_issue_cycle);
      if( reservINT[i]->tom_issue_cycle < current_cycle ) {

        FU_idx = is_FU_available(reservINT[i]->op, reservedFUs);
        //printf("[S_TO_EX]In cycle %d - INT, FU entry is %d\n", current_cycle, FU_idx);
        if( !reservINT[i]->Q[0] && !reservINT[i]->Q[1] && !reservINT[i]->Q[2] && (FU_idx != -1) ){
          //if( !oldest_insn ) oldest_insn = reservINT[i];
          //else if( oldest_insn->index > reservINT[i]->index ) {
          //  oldest_insn = reservINT[i];
          //}
          
          if( !oldest_insn ) {
            oldest_insn = reservINT[i];
            FU_idx_1 = FU_idx;
            reservedFUs[0] = FU_idx;
          }
          else if( oldest_insn->index > reservINT[i]->index ) {
            oldest_insn_2 = oldest_insn;
            oldest_insn = reservINT[i];
            FU_idx_2 = FU_idx_1;
            FU_idx_1 = FU_idx;
            reservedFUs[0] = FU_idx;
            reservedFUs[1] = FU_idx_2;
          }
          else if( !oldest_insn_2 ){
            oldest_insn_2 = reservINT[i];
            FU_idx_2 = FU_idx;
            reservedFUs[1] = FU_idx_2;
          }
          else if( oldest_insn_2->index > reservINT[i]->index ) {
            //oldest_insn_3 = oldest_insn_2;
            oldest_insn_2 = reservINT[i];
            //FU_idx_3 = FU_idx_2;
            FU_idx_2 = FU_idx;
            reservedFUs[2] = FU_idx_2;
            //reservedFUs[3] = FU_idx_3;
          }
          //else if( !oldest_insn_3 || (oldest_insn_3->index > reservINT[i]->index) ) {
          //  oldest_insn_3 = reservINT[i];
          //  FU_idx_3 = FU_idx;
          //  reservedFUs[3] = FU_idx_3;
          //}
          //else printf("ABORT: FU entry overflow\n");
          
        }
      }
    }
  }

  if( oldest_insn ) {
    oldest_insn->tom_execute_cycle = current_cycle;
    fuINT[FU_idx_1] = oldest_insn;
  }
  
  if( oldest_insn_2 ) {
    oldest_insn_2->tom_execute_cycle = current_cycle;
    fuINT[FU_idx_2] = oldest_insn_2;
  }

  //if( oldest_insn_3 ) {
  //  oldest_insn_3->tom_execute_cycle = current_cycle;
  //  fuINT[FU_idx_3] = oldest_insn_3;
  //}

  for( i = 0; i < FU_INT_SIZE; i++ ){
    reservedFUs[i] = -1;
  }

  oldest_insn = NULL;
  for( i = 0; i < RESERV_FP_SIZE; i++ ) {
    if( reservFP[i] != NULL ) {
      //printf("[S_TO_EX]In cycle %d - FP, RS entry is %d\n", current_cycle, i);
      if( reservFP[i]->tom_execute_cycle > 0 ) continue;
      if( reservFP[i]->tom_issue_cycle < current_cycle ) {
        FU_idx = is_FU_available(reservFP[i]->op, reservedFUs);
        //printf("[S_TO_EX]In cycle %d - FP, FU entry is %d\n", current_cycle, FU_idx);
        if( !reservFP[i]->Q[0] && !reservFP[i]->Q[1] && !reservFP[i]->Q[2] && (FU_idx != -1) ){
          if( !oldest_insn ) oldest_insn = reservFP[i];
          else if( oldest_insn->index > reservFP[i]->index ) {
            oldest_insn = reservFP[i];
          }
        }
      }
    }
  }

  if( oldest_insn ) {
    oldest_insn->tom_execute_cycle = current_cycle;
    fuFP[FU_idx] = oldest_insn;
  }

}

/* 
 * Description: 
 * 	Moves instruction(s) from the dispatch stage to the issue stage
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void dispatch_To_issue(int current_cycle) {

  /* ECE552: YOUR CODE GOES HERE */

  instruction_t* curr_insn = NULL;
  //while (instr_queue_size > 0) {
    curr_insn = instr_queue[0];
    if( curr_insn == NULL) {
      return;
    }

    /* No need to allocate RS entries for cond/unconditional branches */
    if( IS_UNCOND_CTRL(curr_insn->op) || IS_COND_CTRL(curr_insn->op) ) {
      //curr_insn->tom_issue_cycle = current_cycle;
      done_insn_count++;
      num_branch_insn++;
      remove_from_IFQ();
      return;
    }

    
    int rdy_for_dispatch;
    rdy_for_dispatch = is_RS_available(curr_insn->op);
    if( rdy_for_dispatch != -1 ) {
      /* set RS entry and remove from IFQ */
      /* need to also note any RAW dependencies in the RS entry */
      //printf("In cycle %d, RS entry is %d\n", current_cycle, rdy_for_dispatch);
      //if(current_cycle == 2) printf("first insn at head should be issued\n");
      //if(current_cycle == 2) printf("fetch to dispatch in cycle 2\n");
      int src_reg_0 = curr_insn->r_in[0];
      int src_reg_1 = curr_insn->r_in[1];
      int src_reg_2 = curr_insn->r_in[2];
      
      if( (src_reg_0 != DNA) && map_table[src_reg_0] ) {
        curr_insn->Q[0] = map_table[src_reg_0];
      }
      if( (src_reg_1 != DNA) && map_table[src_reg_1] ) {
        curr_insn->Q[1] = map_table[src_reg_1];
      }
      if( (src_reg_2 != DNA) && map_table[src_reg_2] ) {
        curr_insn->Q[2] = map_table[src_reg_2];
      }
      

      int output_reg_0 = curr_insn->r_out[0];
      int output_reg_1 = curr_insn->r_out[1];
      if(output_reg_0 != DNA) map_table[output_reg_0] = curr_insn;
      if(output_reg_1 != DNA) map_table[output_reg_1] = curr_insn;

      if( USES_INT_FU(curr_insn->op) ) {
        curr_insn->tom_issue_cycle = current_cycle;
        reservINT[rdy_for_dispatch] = curr_insn;
        //printf("In cycle %d, for an INT insn, instr queue size before decrement is %d\n", current_cycle, instr_queue_size);
        remove_from_IFQ();
        //printf("In cycle %d, for an INT insn, instr queue size is %d\n", current_cycle, instr_queue_size);
      }
      else if ( USES_FP_FU(curr_insn->op) ) {
        curr_insn->tom_issue_cycle = current_cycle;
        reservFP[rdy_for_dispatch] = curr_insn;
        remove_from_IFQ();
        //printf("In cycle %d, for an FP insn, instr queue size is %d\n", current_cycle, instr_queue_size);
      }
    }
//}

}

/* 
 * Description: 
 * 	Grabs an instruction from the instruction trace (if possible)
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 * Returns:
 * 	None
 */
void fetch(instruction_trace_t* trace) {
  /* ECE552: YOUR CODE GOES HERE */
  /* a new instr fetched every cycle as long as the instruction queue is not full */
  int instr_i;
  instruction_t* curr_insn = NULL;

  if( instr_queue_size < INSTR_QUEUE_SIZE ){
    fetch_index++;
    curr_insn = get_instr(trace, fetch_index);
    while( (fetch_index <= sim_num_insn) && IS_TRAP(curr_insn->op) ) {
      fetch_index++;
      curr_insn = get_instr(trace, fetch_index);
      done_insn_count++;
      num_TRAPS++;
    }

    if( curr_insn != NULL ) {
      instr_i = instr_queue_size % INSTR_QUEUE_SIZE;
      instr_queue[instr_i] = curr_insn;
      instr_queue_size++;
      //printf("instr_i %d enters the queue (which now has size %d)\n", instr_i, instr_queue_size);
    }

  }
/*
  for ( instr_i = 0; instr_i < INSTR_QUEUE_SIZE; instr_i++ ) {
    if( !instr_queue[instr_i] ) {
      printf("in the if statemenT");
      fetch_index++;
      curr_insn = get_instr(trace, fetch_index);
      while( fetch_index <= sim_num_insn && IS_TRAP(curr_insn->op) ) {
        fetch_index++;
        curr_insn = get_instr(trace, fetch_index);
        done_insn_count++;
        num_TRAPS++;
      }
      instr_queue_size++;
      instr_queue[instr_i] = curr_insn;
      printf("instr_i: %d", instr_i);
    }
  }
  */
}

/* 
 * Description: 
 * 	Calls fetch and dispatches an instruction at the same cycle (if possible)
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void fetch_To_dispatch(instruction_trace_t* trace, int current_cycle) {

  fetch(trace);
  /* fetch until you find a non-trap instruction */
  /* branches removed from IFQ once they reach head*/
  /* ECE552: YOUR CODE GOES HERE */

  /* allocate RS entry */
  instruction_t* curr_insn = NULL;
  /* get insn at the head of IFQ and try to dispatch if no struct hazards*/
  if( !instr_queue_size ){
    return;
  }
  curr_insn = instr_queue[instr_queue_size-1]; //0
  
  if( (curr_insn != NULL) && curr_insn->tom_dispatch_cycle <= 0 ){
    //printf("Current instruction cycle in fetch_to_dispatch: %d\n\n", current_cycle);
    curr_insn->tom_dispatch_cycle = current_cycle;
    //instr_queue[0]->tom_dispatch_cycle = current_cycle;
  }

}

/* 
 * Description: 
 * 	Performs a cycle-by-cycle simulation of the 4-stage pipeline
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 * Returns:
 * 	The total number of cycles it takes to execute the instructions.
 * Extra Notes:
 * 	sim_num_insn: the number of instructions in the trace
 */
counter_t runTomasulo(instruction_trace_t* trace)
{
  //initialize instruction queue
  int i;
  for (i = 0; i < INSTR_QUEUE_SIZE; i++) {
    instr_queue[i] = NULL;
  }

  //initialize reservation stations
  for (i = 0; i < RESERV_INT_SIZE; i++) {
      reservINT[i] = NULL;
  }

  for(i = 0; i < RESERV_FP_SIZE; i++) {
      reservFP[i] = NULL;
  }

  //initialize functional units
  for (i = 0; i < FU_INT_SIZE; i++) {
    fuINT[i] = NULL;
  }

  for (i = 0; i < FU_FP_SIZE; i++) {
    fuFP[i] = NULL;
  }

  //initialize map_table to no producers
  int reg;
  for (reg = 0; reg < MD_TOTAL_REGS; reg++) {
    map_table[reg] = NULL;
  }

  int cycle = 1;
  while (true) {
     /* ECE552: YOUR CODE GOES HERE */
     CDB_To_retire(cycle);
     execute_To_CDB(cycle);
     issue_To_execute(cycle);
     dispatch_To_issue(cycle);
     fetch_To_dispatch(trace, cycle);

    //printf("In cycle %d fetch Index %d queue size %d \n", cycle, fetch_index, instr_queue_size);

     cycle++;

     if (is_simulation_done(sim_num_insn) ) { //|| (cycle >=50)
       print_all_instr(trace, sim_num_insn);
       printf("tom_total_cycles: %d\n", cycle);
       break;
     }
        
  }
  
  return cycle;
}
