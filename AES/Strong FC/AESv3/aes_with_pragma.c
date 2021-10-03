/*
*   Byte-oriented AES-256 implementation.
*   All lookup tables replaced with 'on the fly' calculations.
*/
#include "aes.h"
#include <string.h>
#include "assert.h"
#include "stdint.h"
#include "stdbool.h"

#define F(x)   (((x)<<1) ^ ((((x)>>7) & 1) * 0x1b))
#define FD(x)  (((x) >> 1) ^ (((x) & 1) ? 0x8d : 0))

#define BACK_TO_TABLES
#ifdef BACK_TO_TABLES

#define BUF_SIZE_OFFSET 12
#define BUF_SIZE ((1) << (BUF_SIZE_OFFSET))

#define UNROLL_FACTOR 2


const uint8_t sbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5,
    0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0,
    0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc,
    0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a,
    0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0,
    0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b,
    0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85,
    0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5,
    0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17,
    0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88,
    0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c,
    0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9,
    0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6,
    0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e,
    0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94,
    0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68,
    0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

#define rj_sbox(x)     sbox[(x)]

#else /* tableless subroutines */

/* -------------------------------------------------------------------------- */
uint8_t gf_alog(uint8_t x) // calculate anti-logarithm gen 3
{
    uint8_t atb = 1, z;

    alog : while (x--) {z = atb; atb <<= 1; if (z & 0x80) atb^= 0x1b; atb ^= z;}

    return atb;
} /* gf_alog */

/* -------------------------------------------------------------------------- */
uint8_t gf_log(uint8_t x) // calculate logarithm gen 3
{
    uint8_t atb = 1, i = 0, z;

    glog : do {
        if (atb == x) break;
        z = atb; atb <<= 1; if (z & 0x80) atb^= 0x1b; atb ^= z;
    } while (++i > 0);

    return i;
} /* gf_log */


/* -------------------------------------------------------------------------- */
uint8_t gf_mulinv(uint8_t x) // calculate multiplicative inverse
{
    return (x) ? gf_alog(255 - gf_log(x)) : 0;
} /* gf_mulinv */

/* -------------------------------------------------------------------------- */
uint8_t rj_sbox(uint8_t x)
{
    uint8_t y, sb;

    sb = y = gf_mulinv(x);
    y = (y<<1)|(y>>7); sb ^= y;  y = (y<<1)|(y>>7); sb ^= y;
    y = (y<<1)|(y>>7); sb ^= y;  y = (y<<1)|(y>>7); sb ^= y;

    return (sb ^ 0x63);
} /* rj_sbox */
#endif

/* -------------------------------------------------------------------------- */
uint8_t rj_xtime(uint8_t x)
{
    return (x & 0x80) ? ((x << 1) ^ 0x1b) : (x << 1);
} /* rj_xtime */

/* -------------------------------------------------------------------------- */
void aes_subBytes(uint8_t *buf)
{
    register uint8_t i = 16;

    sub : while (i--) {
    #pragma HLS PIPELINE
      buf[i] = rj_sbox(buf[i]);
    }
} /* aes_subBytes */

/* -------------------------------------------------------------------------- */
void aes_addRoundKey(uint8_t *buf, uint8_t *key)
{
    register uint8_t i = 16;

    addkey : while (i--) {
    #pragma HLS PIPELINE
	buf[i] ^= key[i];
    }
} /* aes_addRoundKey */

/* -------------------------------------------------------------------------- */
void aes_addRoundKey_cpy(uint8_t *buf, uint8_t *key, uint8_t *cpk)
{
    register uint8_t i = 16;

    cpkey : while (i--)  {
    #pragma HLS PIPELINE
        buf[i] ^= (cpk[i] = key[i]), cpk[16+i] = key[16 + i];
    }
} /* aes_addRoundKey_cpy */


/* -------------------------------------------------------------------------- */
void aes_shiftRows(uint8_t *buf)
{
    register uint8_t i, j; /* to make it potentially parallelable :) */

    i = buf[1]; buf[1] = buf[5]; buf[5] = buf[9]; buf[9] = buf[13]; buf[13] = i;
    i = buf[10]; buf[10] = buf[2]; buf[2] = i;
    j = buf[3]; buf[3] = buf[15]; buf[15] = buf[11]; buf[11] = buf[7]; buf[7] = j;
    j = buf[14]; buf[14] = buf[6]; buf[6]  = j;

} /* aes_shiftRows */

/* -------------------------------------------------------------------------- */
void aes_mixColumns(uint8_t *buf)
{
    register uint8_t i, a, b, c, d, e;

    mix : for (i = 0; i < 16; i += 4)
    {
        a = buf[i]; b = buf[i + 1]; c = buf[i + 2]; d = buf[i + 3];
        e = a ^ b ^ c ^ d;
        buf[i] ^= e ^ rj_xtime(a^b);   buf[i+1] ^= e ^ rj_xtime(b^c);
        buf[i+2] ^= e ^ rj_xtime(c^d); buf[i+3] ^= e ^ rj_xtime(d^a);
    }
} /* aes_mixColumns */

/* -------------------------------------------------------------------------- */
void aes_expandEncKey(uint8_t *k, uint8_t *rc)
{
    register uint8_t i;

    k[0] ^= rj_sbox(k[29]) ^ (*rc);
    k[1] ^= rj_sbox(k[30]);
    k[2] ^= rj_sbox(k[31]);
    k[3] ^= rj_sbox(k[28]);
    *rc = F( *rc);

    exp1 : for(i = 4; i < 16; i += 4) {
    #pragma HLS PIPELINE
	k[i] ^= k[i-4],   k[i+1] ^= k[i-3],
        k[i+2] ^= k[i-2], k[i+3] ^= k[i-1];
    }
    k[16] ^= rj_sbox(k[12]);
    k[17] ^= rj_sbox(k[13]);
    k[18] ^= rj_sbox(k[14]);
    k[19] ^= rj_sbox(k[15]);

    exp2 : for(i = 20; i < 32; i += 4) {
    #pragma HLS PIPELINE
	k[i] ^= k[i-4],   k[i+1] ^= k[i-3],
        k[i+2] ^= k[i-2], k[i+3] ^= k[i-1];
    }

} /* aes_expandEncKey */

/* -------------------------------------------------------------------------- */
void aes256_encrypt_ecb(uint8_t k[32], uint8_t buf[16])
{
    aes256_context ctx_body;
    aes256_context* ctx = &ctx_body;
    //INIT
    uint8_t rcon = 1;
    uint8_t i;

    ecb1 : for (i = 0; i < 32; i++){
    #pragma HLS PIPELINE
        ctx->enckey[i] = ctx->deckey[i] = k[i];
    }
    ecb2 : for (i = 8;--i;){
        aes_expandEncKey(ctx->deckey, &rcon);
    }

    //DEC
    //  IN_SIZE = 2 
    //  IN_BATCH_SIZE = 16                  
    //  BATCH_MEM_IN = buf, ctx->key
    //  IN_ALLOC_RULE : elem_x address range = [x]     
    aes_addRoundKey_cpy(buf, ctx->enckey, ctx->key);
    //  OUT_SIZE = 1 
    //  OUT_BATCH_SIZE = 16
    //  BATCH_MEM_OUT = buf
    //  OUT_ALLOC_RULE : elem_x address range = [x]


    ecb3 : for(i = 1, rcon = 1; i < 14; ++i)
    {
    //  IN_SIZE = 1 
    //  IN_BATCH_SIZE = 16                  
    //  BATCH_MEM_IN = buf
    //  IN_ALLOC_RULE : elem_x address range = [x]     
        aes_subBytes(buf);
        aes_shiftRows(buf);
    //  OUT_SIZE = 1 
    //  OUT_BATCH_SIZE = 16
    //  BATCH_MEM_OUT = buf
    //  OUT_ALLOC_RULE : elem_x address range = [(x-4*(x%4))%16]

    //  IN_SIZE = 4 
    //  IN_BATCH_SIZE = 4                  
    //  BATCH_MEM_IN = buf
    //  IN_ALLOC_RULE : elem_x address range = [x*4:x*4+3]     
        aes_mixColumns(buf);
    //  OUT_SIZE = 4 
    //  OUT_BATCH_SIZE = 4
    //  BATCH_MEM_OUT = buf
    //  OUT_ALLOC_RULE : elem_x address range = [x*4:x*4+3]
        if( i & 1 ) aes_addRoundKey( buf, &ctx->key[16]);
        else aes_expandEncKey(ctx->key, &rcon), aes_addRoundKey(buf, ctx->key);
    }
    aes_subBytes(buf);
    aes_shiftRows(buf);
    aes_expandEncKey(ctx->key, &rcon);
    aes_addRoundKey(buf, ctx->key);
} /* aes256_encrypt */

void aes_tiling(uint8_t* local_key, uint8_t* buf) {
    for (int k=0; k<BUF_SIZE/UNROLL_FACTOR; k+=16)
        aes256_encrypt_ecb(local_key, buf + k);        
}

void workload( uint8_t* key, uint8_t* data, int size ) {

    uint8_t local_key[UNROLL_FACTOR][32];

    int i,j,k;
    
    //  IN_SIZE = 1 
    //  IN_BATCH_SIZE = 32                  
    //  BATCH_MEM_IN = key
    //  IN_ALLOC_RULE : elem_x address range = [x]     
    for(i=0;i<32;i++){
	local_key[0][i] = key[i];
    }

    for (j=0; j<32; j++) {
        for (i=1; i<UNROLL_FACTOR; i++) {
	    local_key[i][j] = local_key[0][j];
	}
    }
    //  OUT_SIZE = UNROLL_FACTOR 
    //  OUT_BATCH_SIZE = 32
    //  BATCH_MEM_OUT = local_key
    //  OUT_ALLOC_RULE : elem_x address range = [:][x]

    int num_batches = (size + BUF_SIZE - 1) / BUF_SIZE;
    int tail_size = size % BUF_SIZE;
    if (tail_size == 0) tail_size = BUF_SIZE;

    uint8_t buf[UNROLL_FACTOR][BUF_SIZE/UNROLL_FACTOR];

    major_loop: for (i=0; i<num_batches; i++) {
        //  IN_SIZE = 16 
        //  IN_BATCH_SIZE = BUF_SIZE/IN_SIZE                  
        //  BATCH_MEM_IN = data
        //  IN_ALLOC_RULE : elem_x address range = [i*BUF_SIZE + x*IN_SIZE : i*BUF_SIZE + (x+1)*IN_SIZE]     
        reshape1: for (j=0; j<UNROLL_FACTOR; j++) {
	    for(k=0;k<BUF_SIZE/UNROLL_FACTOR;k++){
        	buf[i][k] = *(data + i*BUF_SIZE + j*BUF_SIZE/UNROLL_FACTOR + k);
	    }
	}
        //  OUT_SIZE = 16 
        //  OUT_BATCH_SIZE = BUF_SIZE/OUT_SIZE
        //  BATCH_MEM_OUT = buf
        //  OUT_ALLOC_RULE : elem_x address range = [x*OUT_SIZE/(BUF_SIZE/UNROLL_FACTOR)][(x*OUT_SIZE) % (BUF_SIZE/UNROLL_FACTOR) : ((x+1)*OUT_SIZE) %(BUF_SIZE/UNROLL_FACTOR)]     


        //pragma for major_loop for loop relevant for unroll_loop: 
        //  IN_SIZE = 16 
        //  IN_BATCH_SIZE = BUF_SIZE/IN_SIZE
        //  BATCH_MEM_IN = buf
        //  IN_ALLOC_RULE : elem_x address range = [x*IN_SIZE/(BUF_SIZE/UNROLL_FACTOR)][(x*IN_SIZE) % (BUF_SIZE/UNROLL_FACTOR) : ((x+1)*IN_SIZE) %(BUF_SIZE/UNROLL_FACTOR)]     
        unroll_loop: for (j=0; j<UNROLL_FACTOR; j++) {
	    aes_tiling(local_key[j], buf[j]);
	}
        //pragma for major_loop for loop relevant for unroll_loop: 
        //  OUT_SIZE = 16 
        //  OUT_BATCH_SIZE = BUF_SIZE/OUT_SIZE
        //  BATCH_MEM_OUT = buf
        //  OUT_ALLOC_RULE : elem_x address range = [x*OUT_SIZE/(BUF_SIZE/UNROLL_FACTOR)][(x*OUT_SIZE) % (BUF_SIZE/UNROLL_FACTOR) : ((x+1)*OUT_SIZE) %(BUF_SIZE/UNROLL_FACTOR)]     


        //  IN_SIZE = 16 
        //  IN_BATCH_SIZE = BUF_SIZE/IN_SIZE
        //  BATCH_MEM_IN = buf
        //  IN_ALLOC_RULE : elem_x address range = [x*IN_SIZE/(BUF_SIZE/UNROLL_FACTOR)][(x*IN_SIZE) % (BUF_SIZE/UNROLL_FACTOR) : ((x+1)*IN_SIZE) %(BUF_SIZE/UNROLL_FACTOR)]     
        reshape2: for (j=0; j<UNROLL_FACTOR; j++) {
	    for(k=0;k<BUF_SIZE/UNROLL_FACTOR;k++){
		*(data + i*BUF_SIZE + j*BUF_SIZE/UNROLL_FACTOR + k) =buf[j][k];
	    }
	}
        //  OUT_SIZE = 16 
        //  OUT_BATCH_SIZE = BUF_SIZE/OUT_SIZE
        //  BATCH_MEM_OUT = data
        //  OUT_ALLOC_RULE : elem_x address range = [i*BUF_SIZE + x*OUT_SIZE : i*BUF_SIZE + (x+1)*OUT_SIZE]     
    }
    return;
}

#define INPUT_SIZE_OF_AES 16
#define OUTPUT_SIZE_OF_AES 16
#define NUM_FUNC_INPUT_CNT_PER_BATCH BUF_SIZE/INPUT_SIZE_OF_AES
#define NUM_FUNC_OUTPUT_CNT_PER_BATCH BUF_SIZE/OUTPUT_SIZE_OF_AES


 
uint8_t orig_val[INPUT_SIZE_OF_AES];  uint8_t dup_val[INPUT_SIZE_OF_AES]; uint8_t key[32]; uint16_t orig_in; uint8_t orig_key[UNROLL_FACTOR]; uint8_t orig_out[OUTPUT_SIZE_OF_AES]; bool orig_issued; bool dup_issued; uint16_t dup_in; uint16_t in_count; uint16_t out_count; bool orig_done; bool qed_done; bool qed_check;
st state;


struct out2{
  bool qed_done;
  bool qed_check;
};


struct out3{
  bool qed_done;
  bool qed_check;
};


void aqed_in (uint8_t bmc_in[INPUT_SIZE_OF_AES], bool orig, bool dup) {
 
      bool issue_orig = (orig) && !orig_issued; 
      uint8_t key_sum = 0;
     bool eq=1;
     for (int i = 0; i< INPUT_SIZE_OF_AES; i++) {
         if(*(bmc_in + i) != orig_val[i]){
		eq = 0;
		break;
	}
     }

      bool issue_dup = (dup) && (!dup_issued) && (orig_issued && eq );

      

      if (issue_orig){
         orig_issued =1;    
         orig_in =in_count;
     	 for (int i = 0; i< INPUT_SIZE_OF_AES; i++) {
             orig_val[i] = *(bmc_in + i);
     	 }
      }

      if (issue_dup){
         dup_in =in_count;  
         dup_issued =1; 
      }
      in_count =in_count + 1; 
} 

 

struct out2 aqed_out (uint8_t acc_out[OUTPUT_SIZE_OF_AES]) {

   orig_done = orig_issued && (out_count>= orig_in);
   if (orig_issued && (out_count == orig_in) && !qed_done){
     	for (int i = 0; i< OUTPUT_SIZE_OF_AES; i++) {
             orig_out[i] = acc_out[i];
     	}
   } 
   if (orig_issued && dup_issued && (out_count == dup_in) && !qed_done){
        qed_done = 1;
        qed_check = 1;
        for (int i = 0; i< OUTPUT_SIZE_OF_AES; i++) {
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
   struct out2 o2;
   o2.qed_done = qed_done;
   o2.qed_check = qed_check;

   return o2;
}


struct out3 aqed_top (uint8_t bmc_in[BUF_SIZE], bool *orig, bool *dup) {

struct out2 o2;
struct out3 o3;
uint8_t key_in[32];
int j,k;
for(j=0;j<NUM_FUNC_INPUT_CNT_PER_BATCH;j++){//serializer-deserializer
	aqed_in(&bmc_in[j*INPUT_SIZE_OF_AES], orig[j], dup[j]);
	if(dup[j]) break;
}

workload(key_in, bmc_in, BUF_SIZE);


for(j=0;j<NUM_FUNC_OUTPUT_CNT_PER_BATCH;j++){//serializer-deserializer
	o2 = aqed_out(&bmc_in[j*OUTPUT_SIZE_OF_AES]);
	if(o2.qed_done) break;
}

o3.qed_done = o2.qed_done;
o3.qed_check = o2.qed_check;

return o3;

}

void wrapper(){
uint8_t bmc_in[BUF_SIZE];
bool orig[BUF_SIZE/INPUT_SIZE_OF_AES],dup[BUF_SIZE/INPUT_SIZE_OF_AES];
orig_in=0xFFFF; orig_issued=0; dup_issued=0; dup_in=0xFFFF; in_count=0; out_count=0; orig_done=0; qed_done=0; qed_check=0;
	for(int j=0;j<1;j++){
		struct out3 o3 = aqed_top(bmc_in, orig, dup);
		assert(!o3.qed_done || o3.qed_check);
	}
}

