gcc -o start_sim start_sim.c -I ../../marss.dramsim/ptlsim/tools/ -static
gcc -o stop_sim stop_sim.c -I ../../marss.dramsim/ptlsim/tools/ -static
gcc -o kill_sim kill_sim.c -I ../../marss.dramsim/ptlsim/tools/ -static
gcc -o main main.c -static
