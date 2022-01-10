#include <stdint.h>
#include <math.h>
#include "predictor.h"

/////////////////////////////////////////////////////////////
// 2bitsat
/////////////////////////////////////////////////////////////

/* With uint8_t we get exactly 8 bits which is how much each entry allocates */

/* 2-bit sat cntr uses 8192 bits; each entry uses 1B (round up from 2bits)
 * which means there are 8192/8 = 1024 bits = 2^10
 * STATES:
 *    NN: 0 (strong not-taken)
 *    NT: 1 (weak not-taken)
 *    TN: 2 (weak taken)
 *    TT: 3 (strong taken)
 */

#define NUM_ENTRIES_2BITSAT 1024
static uint8_t ctr_2bitsat[NUM_ENTRIES_2BITSAT];

void InitPredictor_2bitsat() {
  int i;
  for( i = 0; i < NUM_ENTRIES_2BITSAT; i++ ){
    ctr_2bitsat[i] = 1;   // weak not-taken
  }
}

bool GetPrediction_2bitsat(UINT32 PC) {
  /* take least significant 10 bits of the PC */
  int entry_idx = ( PC & 1023 );
  return (ctr_2bitsat[entry_idx] > 1);
}

void UpdatePredictor_2bitsat(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
  int entry_idx = ( PC & 1023 );
  if( resolveDir && predDir ) {
    /* predicted as TAKEN, truly TAKEN; both weak taken and strong taken become strongly taken*/
    ctr_2bitsat[entry_idx] = 3;
  }
  else if( resolveDir && !predDir ) {
    /* predicted as NOT-TAKEN, actually TAKEN*/
    if( ctr_2bitsat[entry_idx] == 0 ) ctr_2bitsat[entry_idx] = 1;
    else ctr_2bitsat[entry_idx] = 2;

  }
  else if( !resolveDir && predDir) {
    /* predicted as TAKEN, actually NOT-TAKEN*/
    if( ctr_2bitsat[entry_idx] == 3 ) ctr_2bitsat[entry_idx] = 2;
    else ctr_2bitsat[entry_idx] = 1;
  }
  else {
    /* predicted as NOT-TAKEN, truly NOT-TAKEN */
    ctr_2bitsat[entry_idx] = 0;
  }
  
}

/////////////////////////////////////////////////////////////
// 2level
/////////////////////////////////////////////////////////////
/* For a branch, its instruction address is used to index BHR table 
 * and the corresponding BHR contents (6 bits) are used to index PHT for making predictions */

#define NUM_ENTRIES_BHT 512
#define NUM_HISTORY_BITS 6
#define NUM_PHT_TABLES 8
#define NUM_ENTRIES_2BITSAT_2LVL 64

static uint8_t bht_[NUM_ENTRIES_BHT][NUM_HISTORY_BITS];
static uint8_t pht_all[NUM_PHT_TABLES][NUM_ENTRIES_2BITSAT_2LVL];

uint8_t GetBHTidx (int bht_idx) {
  uint8_t bht_entry = 0;
  uint8_t base_power = 1;
  int i;
  for( i = 0; i < NUM_HISTORY_BITS; i++ ){
    if( i == 0 ) bht_entry += bht_[bht_idx][i];
    else bht_entry += bht_[bht_idx][i] * (base_power << i);
  }
  return bht_entry;
}

void InitPredictor_2level() {
  int i,j;
  /* each BHT entry is a shift register represented as a 6 element array */
  for( i = 0; i < NUM_ENTRIES_BHT; i++ ){
    for( j = 0; j < NUM_HISTORY_BITS; j++ ) {
      bht_[i][j] = 0; // weak not-taken is considered not-taken
    }
  }

  for( i = 0; i < NUM_PHT_TABLES; i++ ){
    for( j = 0; j < NUM_ENTRIES_2BITSAT_2LVL; j++ ) {
      pht_all[i][j] = 1;
    }
  }
}

bool GetPrediction_2level(UINT32 PC) {
  /* PC:  | unused bits | BHT index | PHT index | 
   * BHT uses 512 (2^9) entries -> need 9 bits for its idx
   * PHT uses 8 pattern history tables -> need 3 bits for its idx */
  int bht_idx = (PC >> 3) & 127;
  int pht_idx = PC & 7;

  /* each BHT entry is a shift register that holds the 6 latest branch outcomes */
  uint8_t bht_entry = GetBHTidx(bht_idx);

  return (pht_all[pht_idx][bht_entry] > 1);
}

void UpdatePredictor_2level(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
  int bht_idx = (PC >> 3) & 127;
  int pht_idx = PC & 7;

  uint8_t bht_entry = GetBHTidx(bht_idx);

  if( resolveDir && predDir ) {
    /* predicted as TAKEN, truly TAKEN; both weak taken and strong taken become strongly taken*/
    pht_all[pht_idx][bht_entry] = 3;
  }
  else if( resolveDir && !predDir ) {
    /* predicted as NOT-TAKEN, actually TAKEN*/
    if( pht_all[pht_idx][bht_entry] == 0 ) pht_all[pht_idx][bht_entry] = 1;
    else pht_all[pht_idx][bht_entry] = 2;

  }
  else if( !resolveDir && predDir) {
    /* predicted as TAKEN, actually NOT-TAKEN*/
    if( pht_all[pht_idx][bht_entry] == 3 ) pht_all[pht_idx][bht_entry] = 2;
    else pht_all[pht_idx][bht_entry] = 1;
  }
  else {
    /* predicted as NOT-TAKEN, truly NOT-TAKEN */
    pht_all[pht_idx][bht_entry] = 0;
  }

  /* shift the true outcome into the appropriate BHT entry */
  int i;
  for( i = NUM_HISTORY_BITS - 1; i > 0; i-- ){
    bht_[bht_idx][i] = bht_[bht_idx][i-1];
  }
  bht_[bht_idx][0] = resolveDir;
}

/////////////////////////////////////////////////////////////
// openend - O-GEHL
/////////////////////////////////////////////////////////////

// have 128Kbits to use -> 128,000 bits
#define NUM_PREDICTOR_TABLES 5 // 4 to 12
#define NUM_ENTRIES_PRED_TABLE 4096
#define THRESHOLD 5
#define NUM_HISTORY_PATTERN_BITS 16

static signed int pred_tables[NUM_PREDICTOR_TABLES][NUM_ENTRIES_PRED_TABLE];
static int history_bits[NUM_HISTORY_PATTERN_BITS];
static int history_length[NUM_PREDICTOR_TABLES] = {0, 2, 4, 8, 16};

static int pred_sum;

int Get_GHist(){
  int ghist_value = 0;
  int base_power = 1;
  int i;
  for( i = 0; i < NUM_HISTORY_PATTERN_BITS; i++ ) {
    if( i == 0 ) ghist_value += history_bits[i];
    else ghist_value += history_bits[i] * (base_power << i);
  }
  return ghist_value;
}

int GetCurrPredTableIdx (UINT32 PC, int table_num, int ghist_value) {
  int curr_pred_table_idx;
  int curr_history_len;
  int curr_history_idx;

  if( table_num == 0 ) {
    curr_pred_table_idx = (PC & 4095);
  }
  else {
    curr_history_len = history_length[table_num];
    curr_history_idx = (1 << curr_history_len) - 1;
    curr_pred_table_idx = ( (PC & 4095) ^ (ghist_value & curr_history_idx ) ) % 4095;
  }

  return curr_pred_table_idx;
}

void UpdateCntr(bool dir, int pred_table_num, int table_idx) {
  /* TAKEN; 5-bit cntr saturates at 15 on upperbound */
  if( dir && (pred_tables[pred_table_num][table_idx] < 15) ) {
    pred_tables[pred_table_num][table_idx]++;
  }
  /* UNTAKEN; 5-bit cntr saturates at -15 on lowerbound */
  else if( !dir &&  pred_tables[pred_table_num][table_idx] > -15) {
    pred_tables[pred_table_num][table_idx]--;
  }
}

void InitPredictor_openend() {
  int i,j;
  for( i = 0; i < NUM_PREDICTOR_TABLES; i++ ){
    for( j = 0; j < NUM_ENTRIES_PRED_TABLE; j++ ){
      pred_tables[i][j] = 0; // should be weak not-taken
    }
  } 
}


bool GetPrediction_openend(UINT32 PC) {
  int curr_pred_table_idx;
  int ghist_value;
  int i;

  ghist_value = Get_GHist();

  pred_sum = NUM_PREDICTOR_TABLES/2; // -num/2 showed slightly lower values
  for( i = 0; i < NUM_PREDICTOR_TABLES; i++ ) {
    curr_pred_table_idx = GetCurrPredTableIdx(PC, i, ghist_value);
    pred_sum += pred_tables[i][curr_pred_table_idx];
  }

  return (pred_sum >= 0);
 }

void UpdatePredictor_openend(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
  // O-GEHL only updated on mispredictions or when absolute value of the computed Sum is < than a threshold
  int curr_pred_table_idx;
  int ghist_value;
  int i;

  ghist_value = Get_GHist();

  if( abs(pred_sum) <= THRESHOLD) {
    for( i = 0; i < NUM_PREDICTOR_TABLES; i++ ) {
      curr_pred_table_idx = GetCurrPredTableIdx(PC, i, ghist_value);
      UpdateCntr(resolveDir, i, curr_pred_table_idx);
    }
  }
  else if( resolveDir && !predDir ) {
    /* predicted as NOT-TAKEN, actually TAKEN*/
    for( i = 0; i < NUM_PREDICTOR_TABLES; i++ ) {
      curr_pred_table_idx = GetCurrPredTableIdx(PC, i, ghist_value);
      UpdateCntr(resolveDir, i, curr_pred_table_idx);
    }
  }
  else if( !resolveDir && predDir) {
    /* predicted as TAKEN, actually NOT-TAKEN*/
    for( i = 0; i < NUM_PREDICTOR_TABLES; i++ ) {
      curr_pred_table_idx = GetCurrPredTableIdx(PC, i, ghist_value);
      UpdateCntr(resolveDir, i, curr_pred_table_idx);
    }
  }

  for( i = NUM_HISTORY_PATTERN_BITS - 1; i > 0; i-- ){
    history_bits[i] = history_bits[i-1];
  }
  history_bits[0] = resolveDir;
}

