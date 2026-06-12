#Makefile for VCS Compilation and Verdi Debugging.
#Step1: make clean
#Step2: make compile
#Step3: run "./simv -gui=verdi"  single-step debug mode.
#Step4: In Verdi, select Menu Simulation"->"Run/Continue F5"
#Step5: Open Source Editor, right click register names and select "Add to waveform"(Ctrl+W)
#In Single debug mode, we should press F5 continuously to see the full waveform.


#This file tells VCS which files to compile.
#Using a file list is cleaner than passing every files on the command line.
FILE_LIST = filelist.f

DEST_NAME = simv

#VCS Compilation Options.
#-Mupdate incremental comilation to save time.
VCS_OPS = -full64 \
	-sverilog \
	-debug_access+all \
	-lca \
	-kdb \
	-timescale=1ns/1ps \
	-l compile.log \
	-Mupdate -o $(DEST_NAME)

#Default make object.
all: filelist compile run verdi

#find all *.v files and put the absolute path into filelist.f
filelist: 
	find . -name "*.v" > $(FILE_LIST)

#VCS: Verilog Compile System to compile verilog hdl files.
compile:
	vcs $(VCS_OPS) -f $(FILE_LIST)

#run to generate fsdb file.
run:
	./$(DEST_NAME)

#Call verdi to debug.
#-F file_name : load an ASCII file containing a specified path to source files and simulator options.
#-ssf dumpFiles(s) : Load FSDB(*.fsdb).
#-recordhlc filename: Records all hi-level commands to a file for playback.
verdi:
	verdi -F $(FILELIST) -ssf My.fsdb

#clean temporary files.
clean:
	rm -rvf csrc simv.daidir *.log $(DEST_NAME)
	rm -rf *.fsdb
	rm -rvf verdiLog
	rm -rf $(FILE_LIST)




