#include "aes.h"
#include <string.h>
#include "assert.h"
#include "stdint.h"
#include "stdbool.h"

#define BUF_SIZE_OFFSET 12
#define BUF_SIZE ((1) << (BUF_SIZE_OFFSET))

#define UNROLL_FACTOR 2

void aes_addRoundKey_cpy(uint8_t *buf, uint8_t *key, uint8_t *cpk)
{
    register uint8_t i = 16;

    cpkey : while (i--)  {
    #pragma HLS PIPELINE
        buf[i] ^= (cpk[i] = key[i]), cpk[16+i] = key[16 + i];
    }
} /* aes_addRoundKey_cpy */


void acc_4(uint8_t buf[16], struct aes256_context* ctx){

    aes_addRoundKey_cpy(buf, ctx->enckey, ctx->key);
}


#define IN_SIZE 2
#define OUT_SIZE 1
#define BATCH_SIZE 16


 
uint8_t orig_val[IN_SIZE];  uint8_t dup_val[IN_SIZE]; uint16_t orig_in; uint8_t orig_out[OUT_SIZE]; bool orig_issued; bool dup_issued; uint16_t dup_in; uint16_t in_count; uint16_t out_count; bool orig_done; bool qed_done; bool qed_check; 


struct out{
  bool qed_done;
  bool qed_check;
};

void aqed_in (uint8_t bmc_in[IN_SIZE], int orig, int dup) {
 
      bool issue_orig = (in_count==orig) && !orig_issued; 

     bool eq=1;
     for (int i = 0; i< IN_SIZE; i++) {
         if(*(bmc_in + i) != orig_val[i]){
		eq = 0;
		break;
	}
     }

      bool issue_dup = (in_count==dup) && (!dup_issued) && (orig_issued && eq );

      

      if (issue_orig){
         orig_issued =1;    
         orig_in =in_count;
     	 for (int i = 0; i< IN_SIZE; i++) {
             orig_val[i] = *(bmc_in + i);
     	 }
      }

      if (issue_dup){
         dup_in =in_count;  
         dup_issued =1; 
      }
      in_count =in_count + 1; 
} 

 

struct out aqed_out (uint8_t acc_out[OUT_SIZE]) {

   if (orig_issued && (out_count == orig_in) && !qed_done){
     	for (int i = 0; i< OUT_SIZE; i++) {
             orig_out[i] = acc_out[i];
     	}
   } 
   if (orig_issued && dup_issued && (out_count == dup_in) && !qed_done){
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


struct out aqed_top (uint8_t bmc_in[16], struct aes256_context *ctx, int orig, int dup) {

struct out o2;

int j,k;
for(j=0;j<BATCH_SIZE;j++){//serializer-deserializer
	 uint8_t temp[IN_SIZE];
        temp[0] = bmc_in[j];
        temp[1] = ctx->enckey[j]; 
	aqed_in(temp, orig, dup);
	if(j==dup) break;
}

acc_4(bmc_in, ctx);


for(j=0;j<BATCH_SIZE;j++){//serializer-deserializer
    o2 = aqed_out(&bmc_in[j]);
    if(o2.qed_done) break;
}

return o2;

}

void wrapper(){
	uint8_t bmc_in[16];
	struct aes256_context ctx;
	int orig, dup;
	orig_in=0xFFFF; orig_issued=0; dup_issued=0; dup_in=0xFFFF; in_count=0; out_count=0; orig_done=0; qed_done=0; qed_check=0;
	struct out o3 = aqed_top(bmc_in, &ctx, orig, dup);
	assert(!(orig<BATCH_SIZE && dup<BATCH_SIZE && o3.qed_done)||o3.qed_check);
}




