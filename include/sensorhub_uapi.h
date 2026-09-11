#ifndef SENSORHUB_UAPI_H
#define SENSORHUB_UAPI_H

#ifdef __KERNEL__
#include <linux/ioctl.h>
#include <linux/types.h>
typedef __u32 sensorhub_u32;
typedef __u64 sensorhub_u64;
typedef __s32 sensorhub_s32;
#else
#include <stdint.h>
#include <sys/ioctl.h>
typedef uint32_t sensorhub_u32;
typedef uint64_t sensorhub_u64;
typedef int32_t sensorhub_s32;
#endif

#define SENSORHUB_DEVICE_NAME "sensorhub0"
#define SENSORHUB_IOC_MAGIC 'S'

enum sensorhub_sample_type {
    SENSORHUB_SAMPLE_TEMP = 1,
    SENSORHUB_SAMPLE_ACCEL = 2,
    SENSORHUB_SAMPLE_GYRO = 3,
    SENSORHUB_SAMPLE_USER = 100,
};

struct sensorhub_sample {
    sensorhub_u64 seq;
    sensorhub_u64 timestamp_ns;
    sensorhub_u32 type;
    sensorhub_s32 value0;
    sensorhub_s32 value1;
    sensorhub_s32 value2;
};

struct sensorhub_config {
    sensorhub_u32 interval_ms;
    sensorhub_u32 enabled;
};

struct sensorhub_stats {
    sensorhub_u64 generated;
    sensorhub_u64 injected;
    sensorhub_u64 dropped;
    sensorhub_u64 read_bytes;
    sensorhub_u32 fifo_bytes;
    sensorhub_u32 fifo_capacity;
    sensorhub_u32 open_count;
};

#define SENSORHUB_IOC_GET_CONFIG _IOR(SENSORHUB_IOC_MAGIC, 1, struct sensorhub_config)
#define SENSORHUB_IOC_SET_CONFIG _IOW(SENSORHUB_IOC_MAGIC, 2, struct sensorhub_config)
#define SENSORHUB_IOC_GET_STATS  _IOR(SENSORHUB_IOC_MAGIC, 3, struct sensorhub_stats)
#define SENSORHUB_IOC_RESET      _IO(SENSORHUB_IOC_MAGIC, 4)

#endif
