/* 046267 Computer Architecture - Winter 20/21 - HW #1                  */
/* This file should hold your implementation of the predictor simulator */

#include "bp_api.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#define MAX_BTB_SIZE 32
#define MAX_HISTORY_SIZE 8
#define MAX_TAG_SIZE 30
#define FSM_MAX_SIZE 256

// Main data struct which saves arguments and the btb table = array of lines.

struct Line {
	uint32_t target;
	uint32_t tag;
	uint32_t localHistory;
	int fsm[FSM_MAX_SIZE];
};
typedef struct Line Line;
struct BTB {
	unsigned btbSize;
	unsigned historySize;
	unsigned tagSize;
	unsigned fsmState;
	bool isGlobalHist;
	bool isGlobalTable;
	int Shared;
	Line btbTable[MAX_BTB_SIZE];
	unsigned flush_num;          
	unsigned br_num;      	    	
};

//globals var, globalHistory and global fsm are in use only if isGlobalHist or isGlobalTable.
struct BTB btb;
uint32_t globalHistory = 0;
int global_fsm[FSM_MAX_SIZE];

//tag calculation by tbtSize and tagSize as seen in the tutorial.
uint32_t tag_calc(uint32_t pc){
	uint32_t shifted_pc = 0;
	uint32_t c = 0;
	for(unsigned i =0; i < btb.tagSize; i++){
		c = c << 1;
		c = c | 1;
	}
	switch(btb.btbSize) {
		case 1:
			shifted_pc =  pc >> 2;
			break;
		case 2:
			shifted_pc = pc >> 3;
			break;
		case 4:
			shifted_pc = pc >> 4;
			break;
		case 8:
			shifted_pc = pc >> 5;
			break;
		case 16:
			shifted_pc = pc >> 6;
			break;
		case 32:
			shifted_pc = pc >> 7;
			break;
	}
	return shifted_pc & c;
}

// table index calculation by tbtSize.
int btb_index_calc(uint32_t pc) {
	uint32_t c = 0;
	switch(btb.btbSize) {
		case 1:
			return 0;
		case 2:
			c = pc & 4;
			break;
		case 4:
			c = pc & 12;
			break;
		case 8:
			c = pc & 28;
			break;
		case 16:
			c = pc & 60;
			break;
		case 32:
			c = pc & 124;
			break;
	}
	return c >> 2;
}

int calculate_fsm_update(int old_val, bool taken) {
	int new_val = 0;
	if (taken) {
		new_val = old_val + 1;
		if (new_val > 3) {
			new_val = 3;
		}
	}
	else {
		new_val = old_val - 1;
		if (new_val < 0) {
			new_val = 0;
		}
	}
	return new_val;
}

//calculate the mask that using and find the relevant history out of the max size 
int findHistorSizeMask(unsigned history_size) {
	int mask = 0;
	switch (history_size) {
		case 1:
			mask = 1;
			break;
		case 2:
			mask = 3;
			break;
		case 3:
			mask = 7;
			break;
		case 4:
			mask = 15;
			break;
		case 5:
			mask = 31;
			break;
		case 6:
			mask = 63;
			break;
		case 7:
			mask = 127;
			break;
		case 8:
			mask = 255;
			break;
	}
	return mask;
}


// local fsm value update, max value is 3 and min value is 0.
void local_fsm_update(uint32_t curr_histroy, bool taken, int btb_index) {
	int old_val = btb.btbTable[btb_index].fsm[curr_histroy];
	(btb.btbTable[btb_index]).fsm[curr_histroy] = calculate_fsm_update(old_val,taken);
}

//global fsm value update
void global_fsm_update(uint32_t curr_history, bool taken, uint32_t pc) {
	int old_val = global_fsm[curr_history];
	//cheking which FSM to update according to the share bit
	if (btb.Shared == 0) {
		global_fsm[curr_history]= calculate_fsm_update( old_val, taken);
		return;
	}
	int index = 0;
	int mask = findHistorSizeMask(btb.historySize);
	if (btb.Shared == 1){
		pc = pc >> 2;
		index = (pc & mask) ^ curr_history;
		old_val = global_fsm[index];
		global_fsm[index]= calculate_fsm_update( old_val,  taken);
		return;
	}
	else if (btb.Shared == 2){
		pc = pc >> 16;
		index = (pc  & mask) ^ curr_history; /////
		old_val = global_fsm[index];
		global_fsm[index]= calculate_fsm_update(old_val, taken);
	}
}

// calculate the relevant history when knowing that the branch is in the TBT 
uint32_t findHistory(int btb_index) {
	if (btb.isGlobalHist) {
		return globalHistory;
	}
	
	return btb.btbTable[btb_index].localHistory;
}

//global history update
uint32_t history_update(uint32_t curr_hist, bool taken){
	uint32_t tmp = curr_hist;
	tmp = tmp << 1;
	if (taken) {
		tmp = tmp | 1;
	}
	return tmp & findHistorSizeMask(btb.historySize) ;
}

int BP_init(unsigned btbSize, unsigned historySize, unsigned tagSize, unsigned fsmState,
			bool isGlobalHist, bool isGlobalTable, int Shared){
	btb.btbSize = btbSize;
	btb.historySize = historySize;
	btb.tagSize = tagSize;
	btb.fsmState = fsmState;
	btb.isGlobalHist = isGlobalHist;
	btb.isGlobalTable = isGlobalTable;
	btb.Shared = Shared;
	btb.flush_num=0;           
	btb.br_num=0;      	    
	if(isGlobalTable){ // if global, has to initialize the table with fsmState 
		for(int i = 0; i < FSM_MAX_SIZE; i++) {
			global_fsm[i] = fsmState;
		}
	}
	return 0; 
}
 // BP_update for local fsm
void fsm_local(int btb_index, uint32_t pc, bool taken, uint32_t pred_dst) {
	uint32_t curr_history = findHistory(btb_index); // find history for both local and global cases
	local_fsm_update(curr_history, taken, btb_index);
	if(btb.isGlobalHist) {
		globalHistory = history_update(curr_history ,taken);
	}
	else {
		btb.btbTable[btb_index].localHistory = history_update(taken, curr_history);

	}
}
// BP_update for global fsm
void fsm_global(int btb_index, uint32_t pc, bool taken, uint32_t pred_dst) {
	int32_t curr_history = findHistory(btb_index); // find history for both local and global cases
	global_fsm_update(curr_history, taken, pc);
	if(btb.isGlobalHist) {
		globalHistory =history_update(curr_history,taken);
	}
	else {
		btb.btbTable[btb_index].localHistory= history_update(curr_history, taken);
	}
}




//Reading the local FSM in the not sharing case
bool is_taken(int state) {
	return state >= 2;
}

//Reading of the global FSM 
bool isTakenGlobal (uint32_t curr_history, uint32_t pc) {
	if (btb.Shared == 0) {
		return is_taken(global_fsm[curr_history]);
	}
	int index = 0;
	int mask = findHistorSizeMask(btb.historySize);
	if (btb.Shared == 1){
		index = ((pc >> 2) & mask) ^ curr_history ;
		return is_taken(global_fsm[index]);
	}

	index = ((pc >> 16) & mask) ^ curr_history ;
	return is_taken(global_fsm[index]);
	}




// BP_predict for global FSM
bool BP_predict_global(int btb_index, uint32_t pc, uint32_t *dst, uint32_t curr_history) {
	if (isTakenGlobal(curr_history,pc)) {
		*dst = btb.btbTable[btb_index].target; 
		return true;
		}
	else {
		*dst= pc+ 4;
		return false;
	}

}

// BP_predict for local FSM
bool BP_predict_local(int btb_index, uint32_t pc,uint32_t *dst, uint32_t curr_history) {
	if (is_taken(btb.btbTable[btb_index].fsm[curr_history])) {
		*dst = btb.btbTable[btb_index].target; 
		return true;
	}
	else {
		*dst= pc+ 4;
		return false;
	}
}


bool BP_predict(uint32_t pc, uint32_t *dst){
	int btb_index = btb_index_calc(pc);
	uint32_t tag = tag_calc(pc);
	//checks if the branch in the BTB
	if(btb.btbTable[btb_index].tag != tag) {
		*dst= pc+ 4;
		return false;
	}
	//the branch is indeed in the BTB
	uint32_t curr_history =  findHistory(btb_index);
	if (btb.isGlobalTable) {
		return BP_predict_global(btb_index, pc, dst, curr_history);
	}
	return BP_predict_local(btb_index, pc, dst, curr_history);
}


void BP_update(uint32_t pc, uint32_t targetPc, bool taken, uint32_t pred_dst){
	btb.br_num++;
	if (((targetPc != pred_dst) && (taken)) || ((!taken) && (pred_dst != pc + 4))) {
		btb.flush_num++;
	}
	int btb_index = btb_index_calc(pc);
	uint32_t tag = tag_calc(pc);
	if(btb.btbTable[btb_index].tag != tag) { //new line in the BTB
		btb.btbTable[btb_index].tag = tag;
		if (!btb.isGlobalHist) { //only initialize in case of local history
			btb.btbTable[btb_index].localHistory = 0; // initialize history
		}
		if (!btb.isGlobalTable) { // only initialize in case of local table
			for (int i = 0; i < FSM_MAX_SIZE; i++) { // initialize fsm
				btb.btbTable[btb_index].fsm[i] = btb.fsmState;
			}
		}
	}
	// here we have an entry for the branch in the BRB
	btb.btbTable[btb_index].target = targetPc;
	if(btb.isGlobalTable) {
		fsm_global(btb_index, pc, taken, pred_dst);
	}
	else {
		fsm_local(btb_index, pc, taken, pred_dst);
	}
}

void BP_GetStats(SIM_stats *curStats){
	curStats->br_num = btb.br_num;
	curStats->flush_num = btb.flush_num;
	int size = 0;
	//adding the BTB table
	size += btb.btbSize*(btb.tagSize + 30);
	//adding the history 
	if(btb.isGlobalHist) {
		size += btb.historySize;
	}
	else {
		size+= btb.historySize * btb.btbSize;
	}
	//adding the FSM
	if(btb.isGlobalTable) {
		size += (int)pow(2,btb.historySize+1);
	}
	else {
		size += pow(2,btb.historySize+1)*btb.btbSize;
	}
	//adding the valis bits-: isGLobalHist(1), isGlobalTable(1), FSMstate(2)+ historySize(2)+ tagsize(5) + shared(2)+ btbSize(5)
	size += btb.btbSize; ///////
	curStats->size= size;
}

