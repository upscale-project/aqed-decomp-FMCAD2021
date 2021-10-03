#include "aes.h"
#include <string.h>
#include "assert.h"
#include "stdint.h"
#include "stdbool.h"


#define UNROLL_FACTOR 2


void acc_1(uint8_t* key, uint8_t local_key[UNROLL_FACTOR][32]) {

    int i, j;
    
    for(i=0;i<32;i++){
	local_key[0][i] = key[i];
    }

    for (j=0; j<32; j++) {
        for (i=1; i<UNROLL_FACTOR; i++) {
	    local_key[i][j] = local_key[0][j];
	}
    }
}

#define IN_SIZE 1
#define OUT_SIZE UNROLL_FACTOR
#define BATCH_SIZE 32


 
uint8_t orig_val[IN_SIZE];  uint8_t dup_val[IN_SIZE]; uint16_t orig_in; bool orig_batch; uint8_t orig_out[OUT_SIZE]; bool orig_issued; bool dup_issued; uint16_t dup_in; bool dup_batch; uint16_t in_count; uint16_t out_count; bool orig_done; bool qed_done; bool qed_check; 


struct out{
  bool qed_done;
  bool qed_check;
};

void aqed_in (uint8_t bmc_in[IN_SIZE], int orig, int dup, bool orig_batch_idx, bool dup_batch_idx, bool batch) {
 
      bool issue_orig = (in_count==orig) && !orig_issued && (orig_batch_idx==batch); 

     bool eq=1;
     for (int i = 0; i< IN_SIZE; i++) {
         if(*(bmc_in + i) != orig_val[i]){
		eq = 0;
		break;
	}
     }

      bool issue_dup = (in_count==dup) && (dup_batch_idx==batch) && (!dup_issued) && (orig_issued && eq );

      

      if (issue_orig){
         orig_issued =1;    
         orig_in =in_count;
         orig_batch =orig_batch_idx;
     	 for (int i = 0; i< IN_SIZE; i++) {
             orig_val[i] = *(bmc_in + i);
     	 }
      }

      if (issue_dup){
         dup_in =in_count;  
         dup_batch =dup_batch_idx;
         dup_issued =1; 
      }
      in_count =in_count + 1; 
} 

 

struct out aqed_out (uint8_t acc_out[OUT_SIZE], bool batch) {

   if (orig_issued && (out_count == orig_in) && (batch == orig_batch) && !qed_done){
     	for (int i = 0; i< OUT_SIZE; i++) {
             orig_out[i] = acc_out[i];
     	}
   } 
   if (orig_issued && dup_issued && (out_count == dup_in) && (batch == dup_batch) && !qed_done){
        qed_done = 1;
        qed_check = 1;
        for (int i = 0; i< OUT_SIZE; i++) {
           if(orig_out[i] != acc_out[i]){
		   qed_check = 0;
		   break;
	   }
        }
   }
   if (out_count > dup_in){
        qed_done = 1;
   }
   out_count =out_count + 1; 
   struct out o2;
   o2.qed_done = qed_done;
   o2.qed_check = qed_check;

   return o2;
}


struct out aqed_top (uint8_t bmc_in[32], int orig, int dup, bool orig_batch_idx, bool dup_batch_idx, bool batch) {

struct out o2;

uint8_t local_key[UNROLL_FACTOR][32];
int j,k;
for(j=0;j<BATCH_SIZE;j++){//serializer-deserializer
	aqed_in(&bmc_in[j], orig, dup, orig_batch_idx, dup_batch_idx, batch);
	if(j==dup) break;
}

acc_1(bmc_in, local_key);


for(j=0;j<BATCH_SIZE;j++){//serializer-deserializer
    uint8_t temp[OUT_SIZE];
    for(k=0;k<OUT_SIZE;k++){
	temp[k] =local_key[k][j];
    }
    o2 = aqed_out(temp, batch);
    if(o2.qed_done) break;
}

return o2;

}

void wrapper(){
uint8_t bmc_in[2][32];
int orig, dup;
bool orig_batch_idx, dup_batch_idx;
orig_in=0xFFFF; orig_batch=1; orig_issued=0; dup_issued=0; dup_in=0xFFFF; dup_batch=1; in_count=0; out_count=0; orig_done=0; qed_done=0; qed_check=0;
bool batch=0;
	while(1){
		struct out o3 = aqed_top(bmc_in[batch], orig, dup, orig_batch_idx, dup_batch_idx, batch);
		assert(!(orig<BATCH_SIZE && dup<BATCH_SIZE && o3.qed_done) || o3.qed_check);
		if(batch==1 || o3.qed_done)
			break;
		batch = !batch;
	}
}



