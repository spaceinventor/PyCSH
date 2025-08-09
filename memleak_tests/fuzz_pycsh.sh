
dir_path=$(dirname $(realpath $0))

LD_PRELOAD=$(gcc -print-file-name=libasan.so) \
PYTHONMALLOC=malloc \
ASAN_OPTIONS=detect_leaks=1 \
python3 $dir_path/fuzz_pycsh.py
