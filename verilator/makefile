VERILOG_SOURCE_DIR = ../rtl
VERILOG_SOURCES = $(wildcard $(VERILOG_SOURCE_DIR)/*.v)
TEST_BENCH_SOURCE = ./matrix_tb.cpp

generate: $(VERILOG_SOURCES)
	verilator --trace -cc $^ --exe $(TEST_BENCH_SOURCE)

build:
	make -C obj_dir -f VMatrix.mk VMatrix
