#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <asm/io.h>

#define IRQ_NUMBER 1
#define KBD_PORT 0x60
#define KBD_STATUS_MASK 0x80
#define KBD_SCANCODE_MASK 0x7f

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Shatskiy Rostislav");

static const char *name = "mydev";
static char *ascii_map[] = {
        "[ESC]", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "[BACKSPACE]",
        "[Tab]", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "[", "]", "[Enter]", "[CTRL]",
        "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "\'", "`", "[LShift]", "\\",
        "Z", "X", "C", "V", "B", "N", "M", ",", ".", "/",
        "[RShift]", "[PrtSc]", "[Alt]", "[SPACE]", "[Caps]",
        "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "[Num]", "[Scroll]", "[Home(7)]",
        "[Up(8)]", "[PgUp(9)]", "-", "[Left(4)]", "[Center(5)]", "[Right(6)]", "+",
        "[End(1)]", "[Down(2)]", "[PgDn(3)]", "[Ins]", "[Del]"
};

static struct tasklet_struct tasklet;

static void tasklet_handler(unsigned long scancode) {
    char *key = ascii_map[(scancode & KBD_SCANCODE_MASK) - 1];
    printk("TASKLET >>> %s is pressed, state=%ld\n", key, tasklet.state);
}

static irqreturn_t interrupt_handler(int irq, void *dev) {
    if (irq != IRQ_NUMBER) {
        return IRQ_NONE;
    } else {
        int scancode = inb(KBD_PORT);
        if (!(scancode & KBD_STATUS_MASK)) {
            tasklet.data = scancode;
            tasklet_schedule(&tasklet);
            printk("Tasklet state=%ld\n", tasklet.state);
        }
        return IRQ_HANDLED;
    }
}

static int __init md_init(void) {
    tasklet_init(&tasklet, &tasklet_handler, 0);
    return request_irq(IRQ_NUMBER, (irq_handler_t) interrupt_handler, IRQF_SHARED, name, (void *) (interrupt_handler));
}

static void __exit md_exit(void) {
    tasklet_kill(&tasklet);
    free_irq(IRQ_NUMBER, (void *) (interrupt_handler));
}

module_init(md_init);
module_exit(md_exit);
