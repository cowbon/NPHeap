sudo insmod kernel_module/npheap.ko
sudo chmod 777 /dev/npheap
./benchmark/benchmark_one_obj
cat *.log > trace
sort -n -k 3 trace > sorted_trace
./benchmark/validate 10 65536 < sorted_trace
rm -f *.log
sudo rmmod npheap
