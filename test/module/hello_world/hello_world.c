#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#define DEVICE_NAME "hello_world"
#define CLASS_NAME  "hw_class"

struct hw_node {
    int value;
    struct list_head list;
};

static dev_t devno;
static struct cdev hw_cdev;
static struct class *hw_class;

static LIST_HEAD(hw_list);
static spinlock_t hw_lock;

static struct task_struct *worker;

/* ===================== kthread ===================== */

static int hw_thread_fn(void *data) {
    struct hw_node *node;

    while (!kthread_should_stop()) {
        spin_lock(&hw_lock);
        if (!list_empty(&hw_list)) {
            node = list_first_entry(&hw_list, struct hw_node, list);
            list_del(&node->list);
            spin_unlock(&hw_lock);

            pr_info("hw_thread: consume value = %d\n", node->value);
            kfree(node);
        } else {
            spin_unlock(&hw_lock);
            set_current_state(TASK_INTERRUPTIBLE);
            schedule_timeout(HZ);
        }
    }

    return 0;
}

/* ===================== file ops ===================== */

static ssize_t hw_write(struct file *file, const char __user *buf, size_t len, loff_t *ppos) {
    int value;
    struct hw_node *node;

    if (len < sizeof(int))
        return -EINVAL;

    if (copy_from_user(&value, buf, sizeof(int)))
        return -EFAULT;

    node = kmalloc(sizeof(*node), GFP_KERNEL);
    if (!node)
        return -ENOMEM;

    node->value = value;
    INIT_LIST_HEAD(&node->list);

    spin_lock(&hw_lock);
    list_add_tail(&node->list, &hw_list);
    spin_unlock(&hw_lock);

    pr_info("hw_write: add value = %d\n", value);
    return sizeof(int);
}

static ssize_t hw_read(struct file *file, char __user *buf, size_t len, loff_t *ppos) {
    return 0; /* demo 不实现读取 */
}

static const struct file_operations hw_fops = {
    .owner = THIS_MODULE,
    .read = hw_read,
    .write = hw_write,
};

/* ===================== init / exit ===================== */

static int __init hw_init(void) {
    int ret;

    spin_lock_init(&hw_lock);

    ret = alloc_chrdev_region(&devno, 0, 1, DEVICE_NAME);
    if (ret)
        return ret;

    cdev_init(&hw_cdev, &hw_fops);
    ret = cdev_add(&hw_cdev, devno, 1);
    if (ret)
        goto err_cdev;

    hw_class = class_create(CLASS_NAME);
    if (IS_ERR(hw_class)) {
        ret = PTR_ERR(hw_class);
        goto err_class;
    }

    device_create(hw_class, NULL, devno, NULL, DEVICE_NAME);

    worker = kthread_run(hw_thread_fn, NULL, "hw_worker");
    if (IS_ERR(worker)) {
        ret = PTR_ERR(worker);
        goto err_thread;
    }

    pr_info("hello_world module loaded\n");
    return 0;

err_thread:
    device_destroy(hw_class, devno);
    class_destroy(hw_class);
err_class:
    cdev_del(&hw_cdev);
err_cdev:
    unregister_chrdev_region(devno, 1);
    return ret;
}

static void __exit hw_exit(void) {
    struct hw_node *node, *tmp;

    kthread_stop(worker);

    spin_lock(&hw_lock);
    list_for_each_entry_safe(node, tmp, &hw_list, list) {
        list_del(&node->list);
        kfree(node);
    }
    spin_unlock(&hw_lock);

    device_destroy(hw_class, devno);
    class_destroy(hw_class);
    cdev_del(&hw_cdev);
    unregister_chrdev_region(devno, 1);

    pr_info("hello_world module unloaded\n");
}

module_init(hw_init);
module_exit(hw_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("example");
MODULE_DESCRIPTION("hello_world char device with kthread + list");
