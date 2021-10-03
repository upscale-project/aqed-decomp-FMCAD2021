#include "aes.h"
#include <string.h>
#include "assert.h"
#include "stdint.h"
#include "stdbool.h"

#define BUF_SIZE_OFFSET 12
#define BUF_SIZE ((1) << (BUF_SIZE_OFFSET))

#define UNROLL_FACTOR 2



void acc_7(uint8_t buf[UNROLL_FACTOR][BUF_SIZE/UNROLL_FACTOR], uint8_t* data) {
    int i,j,k;
    i = 0;
        reshape2: for (j=0; j<UNROLL_FACTOR; j++) {
	    for(k=0;k<BUF_SIZE/UNROLL_FACTOR;k++){
		*(data + i*BUF_SIZE + j*BUF_SIZE/UNROLL_FACTOR + k) =buf[j][k];
	    }
	}
}


#define IN_SIZE 16
#define OUT_SIZE 16
#define BATCH_SIZE BUF_SIZE/IN_SIZE


 
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
         dup_issued =1; 
         dup_batch =dup_batch_idx;
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



struct out aqed_top (uint8_t bmc_in[UNROLL_FACTOR][BUF_SIZE/UNROLL_FACTOR], int orig, int dup, bool orig_batch_idx, bool dup_batch_idx, bool batch) {

struct out o2;

uint8_t data[BUF_SIZE];
int j,k;
for(j=0;j<BATCH_SIZE;j++){//serializer-deserializer
    uint8_t temp[IN_SIZE];
    for(k=0;k<IN_SIZE;k++){
	temp[k] =bmc_in[(j*IN_SIZE)/(BUF_SIZE/UNROLL_FACTOR)][(j*IN_SIZE) % (BUF_SIZE/UNROLL_FACTOR)+k];
    }
    aqed_in(temp, orig, dup, orig_batch_idx, dup_batch_idx, batch);
    if(dup==j) break;
}

acc_7(bmc_in, data);

for(j=0;j<BATCH_SIZE;j++){//serializer-deserializer
	o2 = aqed_out(&data[j*OUT_SIZE], batch);
	if(o2.qed_done) break;
}


return o2;

}

void wrapper(){
uint8_t bmc_in[2][UNROLL_FACTOR][BUF_SIZE/UNROLL_FACTOR];
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




