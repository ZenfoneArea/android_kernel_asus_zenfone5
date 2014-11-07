#
# Makefile for sensor drivers
#

obj-$(CONFIG_VIDEO_IMX)        += imx/
obj-$(CONFIG_VIDEO_IMX111)     += imx111/
obj-$(CONFIG_VIDEO_IMX219)     += imx219/
obj-$(CONFIG_VIDEO_IMXVB)      += imx135VB/
obj-$(CONFIG_VIDEO_OV5693)     += ov5693/
obj-$(CONFIG_VIDEO_T4K37)      += t4k37/

obj-$(CONFIG_VIDEO_MT9M114)    += mt9m114.o
obj-$(CONFIG_VIDEO_MT9E013)    += mt9e013/

obj-$(CONFIG_VIDEO_MT9D113)    += mt9d113.o
CFFLAGS_mt9d113.o = -Werror

obj-$(CONFIG_VIDEO_MT9V113)    += mt9v113.o
CFFLAGS_mt9v113.o = -Werror

obj-$(CONFIG_VIDEO_OV5640)     += ov5640.o
CFFLAGS_ov5640.o = -Werror

obj-$(CONFIG_VIDEO_OV9724)     += ov9724.o
obj-$(CONFIG_VIDEO_OV2722)     += ov2722.o
obj-$(CONFIG_VIDEO_OV8830)     += ov8830.o
obj-$(CONFIG_VIDEO_MN34130)    += mn34130.o
mn34130-objs := mn34130/mn34130.o mn34130/dw9714.o

obj-$(CONFIG_VIDEO_S5K8AAY)    += s5k8aay.o
CFLAGS_s5k8aay.o += -Werror

obj-$(CONFIG_VIDEO_HM2056)     += hm2056.o
obj-$(CONFIG_VIDEO_GC0339)     += gc0339.o
obj-$(CONFIG_VIDEO_GC0310)     += gc0310.o

obj-$(CONFIG_VIDEO_MSRLIST_HELPER) += libmsrlisthelper.o
CFLAGS_libmsrlisthelper.o += -Werror

obj-$(CONFIG_VIDEO_AP1302)     += ap1302.o
obj-$(CONFIG_VIDEO_RT8515)     += rt8515.o
#
# Makefile for flash drivers
#

obj-$(CONFIG_VIDEO_LM3554) += lm3554.o
obj-$(CONFIG_VIDEO_LM3559) += lm3559.o


