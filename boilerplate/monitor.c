#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched/signal.h>
#include <linux/mm.h>
#include <linux/kthread.h>
#include <linux/delay.h>

MODULE_LICENSE("GPL");

static struct task_struct *monitor_thread;

static int monitor_fn(void *data)
{
    while (!kthread_should_stop()) {
        struct task_struct *task;

        for_each_process(task) {

            if (!task->mm)
                continue;

            // Skip system processes
            if (task->pid < 1000)
                continue;

            unsigned long rss = get_mm_rss(task->mm) << PAGE_SHIFT;

            if (rss > (20 * 1024 * 1024)) {
                printk(KERN_INFO "Monitor: PID %d using %lu bytes\n", task->pid, rss);
            }
        }

        msleep(2000);
    }
    return 0;
}

static int __init monitor_init(void)
{
    printk(KERN_INFO "Safe Monitor module loaded\n");
    monitor_thread = kthread_run(monitor_fn, NULL, "monitor_thread");
    return 0;
}

static void __exit monitor_exit(void)
{
    printk(KERN_INFO "Monitor module unloaded\n");
    if (monitor_thread)
        kthread_stop(monitor_thread);
}

module_init(monitor_init);
module_exit(monitor_exit);
