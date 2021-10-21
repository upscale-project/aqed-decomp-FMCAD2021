# AES 

The AES was decomposed into 8 sub-accelerators of which 7 have batch-size > 1 (considered for strong FC and intra-batch FC)   

In each repository you will find 4 versions of the AES in Directories **AESv\<i\>** that we verified.  

In each **AESv\<i\>**-   
   
+**aes\_with\_pragma.c** - annotated AES  
+**sub_acc.c** - sub-accelerators with wrapper functions around them.  
+**acc\_\<i\>.c** - Harness connected to each sub accelerator to perform verification.  
 + To run the verification on any sub accelerator, `$cbmc acc\_<i>.c --function wrapper`  


Cong, P. Wei, C. H. Yu, and P. Zhou, “Bandwidth optimization through on-chip memory restructuring for HLS,” in Proc. DAC. IEEE, 2017, pp. 1–6
