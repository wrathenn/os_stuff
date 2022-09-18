#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/delay.h>
#include <asm/io.h>

#define IRQ_NUMBER 1
#define KBD_PORT 0x60
#define KBD_SCANCODE_MASK 0x7f
#define KBD_STATUS_MASK 0x80

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Shatskiy Rostislav");

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

static struct workqueue_struct *queue;
static struct work_struct *work1, *work2;

char scancode;
char *key;

static void work1_handler(struct work_struct *work) {
    if (!(scancode & KBD_STATUS_MASK)) {
        char *key = ascii_map[(scancode & KBD_SCANCODE_MASK) - 1];
        printk("WQ: work1 data: %s is pressed\n", key);
        printk("WQ: Delayed output %s\n", key);
    }
}

static void work2_handler(struct work_struct *work) {
    printk("WQ: work2 current cpu: %u\n", smp_processor_id());
    msleep(10);
}

static irqreturn_t irq_handler(int irq, void *dev) {
    int ret;
    if (irq != IRQ_NUMBER)
        return IRQ_NONE;
    scancode = inb(KBD_PORT);
    if (queue) {
        ret = queue_work(queue, (struct work_struct *) work1); // постановка в очередь
        if (ret == 0)
            printk("work1 is already in queue");
        ret = queue_work(queue, (struct work_struct *) work2);
        if (ret == 0)
            printk("work2 is already in queue");
    }
    return IRQ_HANDLED;
}

int __init md_init(void) {
    int ret = request_irq(IRQ_NUMBER, (irq_handler_t) irq_handler, IRQF_SHARED, "test_irq_handler", (void *) (irq_handler));
    queue = alloc_workqueue("%s", 0, 100, "my_queue");
    work1 = kmalloc(sizeof(struct work_struct), GFP_KERNEL);
    INIT_WORK(work1, work1_handler);
    work2 = kmalloc(sizeof(struct work_struct), GFP_KERNEL);
    INIT_WORK(work2, work2_handler);
    return ret;
}

void __exit md_exit(void) {
    flush_workqueue(queue);
    destroy_workqueue(queue);
    free_irq(IRQ_NUMBER, (void *) (irq_handler));
}

module_init(md_init);
module_exit(md_exit);
