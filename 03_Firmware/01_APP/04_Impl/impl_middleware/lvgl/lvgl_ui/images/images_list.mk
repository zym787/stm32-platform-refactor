# LVGL image pixel arrays are packed into W25Q64 by Tools/pack_assets.py.
# Keep the .c files on disk as packer inputs, but do not compile them into
# firmware .rodata.
GEN_CSRCS +=
