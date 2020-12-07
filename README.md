# Linux Rate-Monotonic Scheduler (RMS)

I have created my own Linux Kernel Module (a very simple one) which implements a reak-time CPU Scheduler based on Rate Monotonic Scheduling (RMS) for a single-core Processor. I also implemented admission control that allows processes to register if they fulfill the admission control criteria. The program also has some test applications (userapp.c, userapp.h) which allows ustest our applications (our our scheduling implementation). LKM (Kernel) concepts of List, Timer, caching were used for its implementation 

In order to run and test the program, cd into the directory and follow the commands
```
make
sudo insmod mp2.ko
./userapp &
cat /proc/mp2/status 
```

This will printout all the values/content of the file on the console.
To remove the module
```
1. rmmod mp2
```
