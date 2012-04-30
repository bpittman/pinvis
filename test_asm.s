.data  # start of data segment

x:    
      .long   1
      .long   5
      .long   2
      .long   18

sum:
      .long 0

.text  # start of code segment

.globl _start
_start:
      movl $4, %eax  # EAX will serve as a counter for 
                     # the number of words left to be summed 
      movl $0, %ebx  # EBX will store the sum
      movl $x, %ecx  # ECX will point to the current 
                     # element to be summed
top:  addl (%ecx), %ebx
      addl $4, %ecx  # move pointer to next element
      decl %eax  # decrement counter
      jnz top  # if counter not 0, then loop again
done: movl %ebx, sum  # done, store result in "sum"
movl $1, %eax   # This is the linux kernel command
                # number for exiting a program.
movl $0, %ebx   # This is the status number we will
                # return to the operating system.
int $0x80       # This wakes up the kernel to run
                # the exit command.
