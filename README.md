In this MP, I have created my own Linux Kernel Module (a very simple one) which implements a reak-time CPU Scheduler based on Rate Monotonic Scheduling (RMS) for a single-core Processor. I also implemented admission control that allows processes to register if they fulfill the admission control criteria. The program also has some test applications (userapp.c, userapp.h) which allows ustest our applications (our our scheduling implementation). LKM (Kernel) concepts of List, Timer, caching were used for its implementation 

In order to run and test the program, cd into the directory and follow the commands
1. make
2. sudo insmod mp2.ko
3. ./userapp &
4. cat /proc/mp2/status 

This will printout all the values/content of the file on the console.
To remove the module
1. rmmod mp2


Sites Referenced:
1. https://cs423-uiuc.github.io/spring20/slides/06-mp1_tutorial.pdf
2. https://blog.sourcerer.io/writing-a-simple-linux-kernel-module-d9dc3762c234
3. http://tuxthink.blogspot.com/2012/01/creating-folder-under-proc-and-creating.html
4. https://linux.die.net/man/3/list_head
5. https://www.kernel.org/doc/htmldocs/kernel-api/API-kmalloc.html
6. https://www.fsl.cs.sunysb.edu/kernel-api/re257.html
7. https://www.fsl.cs.sunysb.edu/kernel-api/re256.html
8. https://embetronicx.com/tutorials/linux/device-drivers/spinlock-in-linux-kernel-1/
9. https://linux-kernel-labs.github.io/master/labs/device_drivers.html
10. https://gist.github.com/itrobotics/596443150c01ff54658e
11. https://www.oreilly.com/library/view/understanding-the-linux/0596005652/ch04s08.html
12. https://linux-kernel-labs.github.io/master/labs/deferred_work.html
13. https://embetronicx.com/tutorials/linux/device-drivers/example-linked-list-in-linux-kernel/
14. https://kernelnewbies.org/FAQ/LinkedLists
15. https://lwn.net/Articles/65178/
16. http://www.cs.toronto.edu/~simon/html/understand/node54.html#SECTION001316000000000000000
17. https://stackoverflow.com/questions/10067510/fixed-point-arithmetic-in-c-programming
18. https://www.kernel.org/doc/gorman/html/understand/understand011.html
19. https://stackoverflow.com/questions/42892616/slab-cache-allocate-stack-of-structures-without-predefined-functions
20. https://www.fsl.cs.sunysb.edu/kernel-api/re237.html
21. https://www.kernel.org/doc/htmldocs/kernel-api/API-kmem-cache-alloc.html
22. https://www.cs.bham.ac.uk/~exr/teaching/lectures/opsys/10_11/docs/kernelAPI/
23. https://www.kernel.org/doc/htmldocs/kernel-api/API-kstrtol.html
24. https://lwn.net/Articles/735887/
25. https://elixir.bootlin.com/linux/latest/ident/timer_list
26. http://actinid.org/atomik/downloads/doc/atomik-kmem_cache.pdf
27. https://github.com/torvalds/linux/blob/master/mm/slab.h
28. https://stackoverflow.com/questions/30372463/what-is-set-current-state-macro-in-kernel-thread

