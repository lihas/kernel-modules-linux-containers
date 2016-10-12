#include<linux/kernel.h>
#include<linux/module.h>


int start(void)
{
__asm__(

      "mov $0x3f8, %dx\n\t"
      "add %bl, %al\n\t"
      "add $'0', %al\n\t"
      "out %al, (%dx)\n\t"
      "mov $'\n', %al\n\t"
      "out %al, (%dx)\n\t"
        "hlt"
);

return 0;
}


void end(void)
{
return;
}


MODULE_LICENSE("GPL");
module_init(start);
module_exit(end);
