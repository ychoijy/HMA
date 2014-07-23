HMA
===

Ubuntu환경에서 실험환경 구축하기

- 초기 셋팅 방법 (marss.dramsim & DRAMSim2)

  1. scons, g++, SDL library 등을 설치

    sudo apt-get install git g++ scons zlib1g-dev libsdl1.2-dev libsdl1.2debian
  
  2. marss.dramsim & DRAMSim2 를 git을 통해 다운로드.

    git clone https://github.com/dramninjasUMD/marss.dramsim.git
    git clone git://github.com/dramninjasUMD/DRAMSim2.git
  
  
  3. 다운받은 DRAMSim2 폴더에서 라이브러리 컴파일

    make libdramsim.so

    * 3번 과정을 수행하기 전에 minisim에 들어갈 input 데이타를 뽑기 위해서 DRAMSim2의 BusPacket.cpp파일을 손봐야됨.

  4. 다운받은 marss.dramsim 폴더에서 scons를 이용해 DRAMSim2 라이브러리를 marss.dramsim에 연동 컴파일

    scons dramsim=/home/ychoijy/HMA/DRAMSim2/    <- 뒤의 path는 본인의 DRAMSim2폴더

    * 위 명령어 실행시 컴파일 에러가 발생하여 qemu실행 파일이 생성되지 않을때
    
      qemu/hw/eepro100.c 에 있는 "#define BIT"로 시작하는 문장들을 qemu/qemu-common.h. 에 복사

  5. 제대로 컴파일되었는지 확인 -> ldd qemu/qemu-system-x86_64 | grep dramsim  을 해서 결과가(1줄) 나와야됨. 아무것도 안나오면 다시 1~4수행


- 추가적으로 필요한 것들(kernel & ramdisk img & minisim)

  1. kernel

    www.kernel.org에서 받기. (3.12.24버전까지만 ptlsim이 잘작동)

  2. ramdisk img 
  
    본인이 ramdisk를 만들거나 여기 git의 image폴더에 있는 ramdisk.img파일을 받기

  3. minisim
  
    다른곳에는 없으니 여기서 받기...(minisim의 출처는 어떤논문저자분...)
    
    
- 실행하는 방법

  1. marss.dramsim폴더의 simconfig파일에서 -dramsim-pwd를 DRAMSim2폴더의 절대경로로 지정해준다.
  
  2. marss.dramsim폴더에서 ./qemu/qemu-system-x86_64 -m 4096 -kernel 커널 절대경로/arch/x86/boot/bzImage -initrd 램디스크이미지절대경로 -append "root=/dev/ram init=/bin/ash" -L ./qemu/pc-bios -simconfig ./simconfig      <-- 명령어 실행
  
  3. qemu가 부팅된 후에 benchmark 폴더로 이동을 해서 ./start_sim; ./main; ./stop_sim; ./kill_sim   <-- 수행
  
    *여기 들어가 있는것들은 램디스크 이미지를 mount -o loop ./ramdisk.img ./rootfs 로 마운트 해서 benchmark폴더에 직접 넣은것. 소스파일은 git폴더의 src_own에 있는 것들을 static 옵션으로 컴파일 해서 넣은것.
  
  4. marss.dramsim폴더에 sim_out_my_dram.ini.tmp 파일로 trace가 나온다. ( 이 파일의 용량 증가가 멈추면 완성된 것이다.)
  

- trace 가공 및 결과 산출

  1. minisim/test폴더에 1_copy.sh 스크립트를 수행 후 2_trans.sh 스크립트를 수행 (파일 내용 중에 경로 부분을 잘 설정해서..)

  3. minisim폴더의 run.sh 스크립트를 수행
  
  
  
