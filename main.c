#include "trap.h"
#include "print.h"
#include "debug.h"
#include "memory.h"

void KMain(void)
{
   char *string = "Hello and Welcome";
   int64_t value = 0x123456789ABCD;

   init_idt();
   enable_interrupts();  // IDT/PIC/PITは設定済みなのでここで初めて割り込みを許可する
   init_memory();

   printk("%s\n", string);
   // printk("This value is equal to %x", value);
   // ASSERT(0);
}
