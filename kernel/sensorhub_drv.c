#include <linux/atomic.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/kfifo.h>
#include <linux/ktime.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/random.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#include "sensorhub_uapi.h"

#define SENSORHUB_FIFO_BYTES (64 * 1024)
#define SENSORHUB_DEFAULT_INTERVAL_MS 500

struct sensorhub_device {
    struct miscdevice miscdev;
    spinlock_t lock;
    wait_queue_head_t read_queue;
    struct kfifo fifo;
    struct hrtimer timer;
    struct sensorhub_config config;
    atomic_t open_count;
    u64 next_seq;
    u64 generated;
    u64 injected;
    u64 dropped;
    u64 read_bytes;
    struct dentry *debugfs_dir;
};

static struct sensorhub_device *g_sensorhub;

static bool sensorhub_has_data(struct sensorhub_device *dev)
{
    bool has_data;
    unsigned long flags;

    spin_lock_irqsave(&dev->lock, flags);
    has_data = !kfifo_is_empty(&dev->fifo);
    spin_unlock_irqrestore(&dev->lock, flags);
    return has_data;
}

static void sensorhub_apply_config(struct sensorhub_device *dev,
                                   const struct sensorhub_config *cfg)
{
    /*
     * 初学者提示：
     * ioctl 和 sysfs 都能修改采样配置，所以统一走这个函数。
     * 这样不会出现一个入口忘记重启定时器、另一个入口更新了状态的情况。
     */
    WRITE_ONCE(dev->config.interval_ms, cfg->interval_ms);
    WRITE_ONCE(dev->config.enabled, cfg->enabled ? 1 : 0);

    if (cfg->enabled)
        hrtimer_start(&dev->timer, ms_to_ktime(cfg->interval_ms), HRTIMER_MODE_REL);
    else
        hrtimer_cancel(&dev->timer);
}

static void sensorhub_push_sample(struct sensorhub_device *dev,
                                  const struct sensorhub_sample *sample,
                                  bool injected)
{
    struct sensorhub_sample item = *sample;
    unsigned int copied;
    unsigned long flags;

    /*
     * 初学者提示：
     * kfifo 是内核提供的环形缓冲区。定时器负责生产数据，
     * 用户进程负责 read 消费数据，两边共享 fifo，所以必须加锁。
     * 定时器上下文不能睡眠，因此这里使用 spinlock，而不是 mutex。
     */
    spin_lock_irqsave(&dev->lock, flags);
    item.seq = dev->next_seq++;
    copied = kfifo_in(&dev->fifo, &item, sizeof(item));
    if (copied != sizeof(item)) {
        dev->dropped++;
    } else if (injected) {
        dev->injected++;
    } else {
        dev->generated++;
    }
    spin_unlock_irqrestore(&dev->lock, flags);

    wake_up_interruptible(&dev->read_queue);
}

static enum hrtimer_restart sensorhub_timer_cb(struct hrtimer *timer)
{
    struct sensorhub_device *dev = container_of(timer, struct sensorhub_device, timer);
    struct sensorhub_sample sample;
    u32 random_value;

    if (!READ_ONCE(dev->config.enabled))
        return HRTIMER_NORESTART;

    get_random_bytes(&random_value, sizeof(random_value));
    memset(&sample, 0, sizeof(sample));
    sample.timestamp_ns = ktime_get_real_ns();
    sample.type = SENSORHUB_SAMPLE_TEMP;
    sample.value0 = 25000 + (random_value % 8000);
    sample.value1 = 0;
    sample.value2 = 0;

    sensorhub_push_sample(dev, &sample, false);

    hrtimer_forward_now(timer, ms_to_ktime(READ_ONCE(dev->config.interval_ms)));
    return HRTIMER_RESTART;
}

static int sensorhub_open(struct inode *inode, struct file *file)
{
    file->private_data = g_sensorhub;
    atomic_inc(&g_sensorhub->open_count);
    return 0;
}

static int sensorhub_release(struct inode *inode, struct file *file)
{
    struct sensorhub_device *dev = file->private_data;

    atomic_dec(&dev->open_count);
    return 0;
}

static ssize_t sensorhub_read(struct file *file, char __user *buf,
                              size_t count, loff_t *ppos)
{
    struct sensorhub_device *dev = file->private_data;
    struct sensorhub_sample samples[64];
    unsigned int copied = 0;
    size_t max_bytes;
    unsigned long flags;
    int ret;

    if (count < sizeof(struct sensorhub_sample))
        return -EINVAL;

    /*
     * 非阻塞读：没有数据时立即返回 -EAGAIN。
     * 阻塞读：没有数据时睡眠，直到定时器或 write 注入数据后唤醒。
     */
    if (!sensorhub_has_data(dev)) {
        if (file->f_flags & O_NONBLOCK)
            return -EAGAIN;

        ret = wait_event_interruptible(dev->read_queue, sensorhub_has_data(dev));
        if (ret)
            return ret;
    }

    count -= count % sizeof(struct sensorhub_sample);
    max_bytes = min_t(size_t, count, sizeof(samples));

    /*
     * 不能在 spinlock 里 copy_to_user，因为访问用户空间可能睡眠。
     * 所以先从 kfifo 拷到内核栈上的临时数组，再释放锁，再拷给用户态。
     */
    spin_lock_irqsave(&dev->lock, flags);
    copied = kfifo_out(&dev->fifo, samples, max_bytes);
    dev->read_bytes += copied;
    spin_unlock_irqrestore(&dev->lock, flags);

    if (copy_to_user(buf, samples, copied))
        return -EFAULT;

    return copied;
}

static ssize_t sensorhub_write(struct file *file, const char __user *buf,
                               size_t count, loff_t *ppos)
{
    struct sensorhub_device *dev = file->private_data;
    struct sensorhub_sample sample;

    if (count != sizeof(sample))
        return -EINVAL;

    if (copy_from_user(&sample, buf, sizeof(sample)))
        return -EFAULT;

    sample.timestamp_ns = ktime_get_real_ns();
    sample.type = SENSORHUB_SAMPLE_USER;
    sensorhub_push_sample(dev, &sample, true);

    return sizeof(sample);
}

static __poll_t sensorhub_poll(struct file *file, poll_table *wait)
{
    struct sensorhub_device *dev = file->private_data;
    __poll_t mask = 0;

    /*
     * poll/epoll 的核心：把当前进程挂到等待队列上。
     * 当 fifo 有数据后，驱动 wake_up_interruptible，epoll_wait 就会返回。
     */
    poll_wait(file, &dev->read_queue, wait);

    if (sensorhub_has_data(dev))
        mask |= EPOLLIN | EPOLLRDNORM;

    mask |= EPOLLOUT | EPOLLWRNORM;
    return mask;
}

static long sensorhub_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct sensorhub_device *dev = file->private_data;
    struct sensorhub_config cfg;
    struct sensorhub_stats stats;
    unsigned long flags;

    switch (cmd) {
    case SENSORHUB_IOC_GET_CONFIG:
        cfg = dev->config;
        if (copy_to_user((void __user *)arg, &cfg, sizeof(cfg)))
            return -EFAULT;
        return 0;

    case SENSORHUB_IOC_SET_CONFIG:
        if (copy_from_user(&cfg, (void __user *)arg, sizeof(cfg)))
            return -EFAULT;
        if (cfg.interval_ms < 10 || cfg.interval_ms > 60000)
            return -EINVAL;

        sensorhub_apply_config(dev, &cfg);
        return 0;

    case SENSORHUB_IOC_GET_STATS:
        memset(&stats, 0, sizeof(stats));
        spin_lock_irqsave(&dev->lock, flags);
        stats.generated = dev->generated;
        stats.injected = dev->injected;
        stats.dropped = dev->dropped;
        stats.read_bytes = dev->read_bytes;
        stats.fifo_bytes = kfifo_len(&dev->fifo);
        stats.fifo_capacity = kfifo_size(&dev->fifo);
        stats.open_count = atomic_read(&dev->open_count);
        spin_unlock_irqrestore(&dev->lock, flags);
        if (copy_to_user((void __user *)arg, &stats, sizeof(stats)))
            return -EFAULT;
        return 0;

    case SENSORHUB_IOC_RESET:
        spin_lock_irqsave(&dev->lock, flags);
        kfifo_reset(&dev->fifo);
        dev->generated = 0;
        dev->injected = 0;
        dev->dropped = 0;
        dev->read_bytes = 0;
        spin_unlock_irqrestore(&dev->lock, flags);
        return 0;

    default:
        return -ENOTTY;
    }
}

static const struct file_operations sensorhub_fops = {
    .owner = THIS_MODULE,
    .open = sensorhub_open,
    .release = sensorhub_release,
    .read = sensorhub_read,
    .write = sensorhub_write,
    .poll = sensorhub_poll,
    .unlocked_ioctl = sensorhub_ioctl,
    .llseek = no_llseek,
};

static ssize_t interval_ms_show(struct device *device,
                                struct device_attribute *attr, char *buf)
{
    return sysfs_emit(buf, "%u\n", READ_ONCE(g_sensorhub->config.interval_ms));
}

static ssize_t interval_ms_store(struct device *device,
                                 struct device_attribute *attr,
                                 const char *buf, size_t count)
{
    struct sensorhub_config cfg = g_sensorhub->config;
    unsigned int interval_ms;
    int ret;

    ret = kstrtouint(buf, 10, &interval_ms);
    if (ret)
        return ret;

    if (interval_ms < 10 || interval_ms > 60000)
        return -EINVAL;

    cfg.interval_ms = interval_ms;
    sensorhub_apply_config(g_sensorhub, &cfg);
    return count;
}

static DEVICE_ATTR_RW(interval_ms);

static ssize_t enabled_show(struct device *device,
                            struct device_attribute *attr, char *buf)
{
    return sysfs_emit(buf, "%u\n", READ_ONCE(g_sensorhub->config.enabled));
}

static ssize_t enabled_store(struct device *device,
                             struct device_attribute *attr,
                             const char *buf, size_t count)
{
    struct sensorhub_config cfg = g_sensorhub->config;
    bool enabled;
    int ret;

    ret = kstrtobool(buf, &enabled);
    if (ret)
        return ret;

    cfg.enabled = enabled ? 1 : 0;
    sensorhub_apply_config(g_sensorhub, &cfg);
    return count;
}

static DEVICE_ATTR_RW(enabled);

static ssize_t stats_show(struct device *device,
                          struct device_attribute *attr, char *buf)
{
    struct sensorhub_stats stats;
    unsigned long flags;

    spin_lock_irqsave(&g_sensorhub->lock, flags);
    stats.generated = g_sensorhub->generated;
    stats.injected = g_sensorhub->injected;
    stats.dropped = g_sensorhub->dropped;
    stats.read_bytes = g_sensorhub->read_bytes;
    stats.fifo_bytes = kfifo_len(&g_sensorhub->fifo);
    stats.fifo_capacity = kfifo_size(&g_sensorhub->fifo);
    stats.open_count = atomic_read(&g_sensorhub->open_count);
    spin_unlock_irqrestore(&g_sensorhub->lock, flags);

    return sysfs_emit(buf,
                      "generated=%llu injected=%llu dropped=%llu read_bytes=%llu fifo_bytes=%u fifo_capacity=%u open_count=%u\n",
                      (unsigned long long)stats.generated,
                      (unsigned long long)stats.injected,
                      (unsigned long long)stats.dropped,
                      (unsigned long long)stats.read_bytes,
                      stats.fifo_bytes,
                      stats.fifo_capacity, stats.open_count);
}

static DEVICE_ATTR_RO(stats);

static struct attribute *sensorhub_attrs[] = {
    &dev_attr_interval_ms.attr,
    &dev_attr_enabled.attr,
    &dev_attr_stats.attr,
    NULL,
};

static const struct attribute_group sensorhub_attr_group = {
    .attrs = sensorhub_attrs,
};

static int sensorhub_debugfs_show(struct seq_file *seq, void *unused)
{
    struct sensorhub_stats stats;
    unsigned long flags;

    spin_lock_irqsave(&g_sensorhub->lock, flags);
    stats.generated = g_sensorhub->generated;
    stats.injected = g_sensorhub->injected;
    stats.dropped = g_sensorhub->dropped;
    stats.read_bytes = g_sensorhub->read_bytes;
    stats.fifo_bytes = kfifo_len(&g_sensorhub->fifo);
    stats.fifo_capacity = kfifo_size(&g_sensorhub->fifo);
    stats.open_count = atomic_read(&g_sensorhub->open_count);
    spin_unlock_irqrestore(&g_sensorhub->lock, flags);

    seq_printf(seq, "interval_ms: %u\n", READ_ONCE(g_sensorhub->config.interval_ms));
    seq_printf(seq, "enabled: %u\n", READ_ONCE(g_sensorhub->config.enabled));
    seq_printf(seq, "generated: %llu\n", (unsigned long long)stats.generated);
    seq_printf(seq, "injected: %llu\n", (unsigned long long)stats.injected);
    seq_printf(seq, "dropped: %llu\n", (unsigned long long)stats.dropped);
    seq_printf(seq, "read_bytes: %llu\n", (unsigned long long)stats.read_bytes);
    seq_printf(seq, "fifo_bytes: %u\n", stats.fifo_bytes);
    seq_printf(seq, "fifo_capacity: %u\n", stats.fifo_capacity);
    seq_printf(seq, "open_count: %u\n", stats.open_count);
    return 0;
}

static int sensorhub_debugfs_open(struct inode *inode, struct file *file)
{
    return single_open(file, sensorhub_debugfs_show, inode->i_private);
}

static const struct file_operations sensorhub_debugfs_fops = {
    .owner = THIS_MODULE,
    .open = sensorhub_debugfs_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int __init sensorhub_init(void)
{
    int ret;

    g_sensorhub = kzalloc(sizeof(*g_sensorhub), GFP_KERNEL);
    if (!g_sensorhub)
        return -ENOMEM;

    spin_lock_init(&g_sensorhub->lock);
    init_waitqueue_head(&g_sensorhub->read_queue);
    atomic_set(&g_sensorhub->open_count, 0);
    g_sensorhub->config.interval_ms = SENSORHUB_DEFAULT_INTERVAL_MS;
    g_sensorhub->config.enabled = 1;

    ret = kfifo_alloc(&g_sensorhub->fifo, SENSORHUB_FIFO_BYTES, GFP_KERNEL);
    if (ret)
        goto err_free_dev;

    hrtimer_init(&g_sensorhub->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    g_sensorhub->timer.function = sensorhub_timer_cb;

    g_sensorhub->miscdev.minor = MISC_DYNAMIC_MINOR;
    g_sensorhub->miscdev.name = SENSORHUB_DEVICE_NAME;
    g_sensorhub->miscdev.fops = &sensorhub_fops;
    g_sensorhub->miscdev.mode = 0666;

    ret = misc_register(&g_sensorhub->miscdev);
    if (ret)
        goto err_free_fifo;

    ret = sysfs_create_group(&g_sensorhub->miscdev.this_device->kobj,
                             &sensorhub_attr_group);
    if (ret)
        goto err_deregister_misc;

    g_sensorhub->debugfs_dir = debugfs_create_dir("sensorhub", NULL);
    if (IS_ERR(g_sensorhub->debugfs_dir)) {
        g_sensorhub->debugfs_dir = NULL;
    } else {
        debugfs_create_file("stats", 0444, g_sensorhub->debugfs_dir,
                            g_sensorhub, &sensorhub_debugfs_fops);
    }

    hrtimer_start(&g_sensorhub->timer,
                  ms_to_ktime(g_sensorhub->config.interval_ms),
                  HRTIMER_MODE_REL);

    pr_info("sensorhub: loaded, device /dev/%s\n", SENSORHUB_DEVICE_NAME);
    return 0;

err_deregister_misc:
    misc_deregister(&g_sensorhub->miscdev);
err_free_fifo:
    kfifo_free(&g_sensorhub->fifo);
err_free_dev:
    kfree(g_sensorhub);
    g_sensorhub = NULL;
    return ret;
}

static void __exit sensorhub_exit(void)
{
    hrtimer_cancel(&g_sensorhub->timer);
    debugfs_remove_recursive(g_sensorhub->debugfs_dir);
    sysfs_remove_group(&g_sensorhub->miscdev.this_device->kobj,
                       &sensorhub_attr_group);
    misc_deregister(&g_sensorhub->miscdev);
    kfifo_free(&g_sensorhub->fifo);
    kfree(g_sensorhub);
    pr_info("sensorhub: unloaded\n");
}

module_init(sensorhub_init);
module_exit(sensorhub_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Linux SensorHub Project");
MODULE_DESCRIPTION("Virtual sensor character device for Linux systems programming practice");
