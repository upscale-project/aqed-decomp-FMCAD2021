# AES 

The AES was decomposed into 8 sub-accelerators of which 7 have batch-size > 1 (considered for FC). 

aes\_with\_pragma.c - annotated AES
sub_acc.c - sub-accelerators with wrapper functions around them.
acc_<i>.c - Intra-batch FC for each sub accelerator.
Directories AESv<i> have each of the 4 versions of AES that we caught bugs in.

Cong, P. Wei, C. H. Yu, and P. Zhou, “Bandwidth optimization through on-chip memory restructuring for HLS,” in Proc. DAC. IEEE, 2017, pp. 1–6
