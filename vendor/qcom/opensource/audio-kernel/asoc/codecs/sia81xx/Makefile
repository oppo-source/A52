# SIA81xx PA object

EN_SIA81XX_TUNING = y
TUNING_PLATFORM = qcom

snd-soc-sia81xx-objs := \
		sia81xx.o \
		sia81xx_regmap.o \
		sia81xx_aux_dev_if.o \
		sia81xx_set_vdd.o \
		sia8101_regs.o \
		sia8108_regs.o \
		
ifeq ($(EN_SIA81XX_TUNING),y)

EXTRA_CFLAGS += -DSIA81XX_TUNING

snd-soc-sia81xx-objs += sia81xx_socket.o

ifeq ($(TUNING_PLATFORM),qcom)
snd-soc-sia81xx-objs += sia81xx_afe.o
endif

ifeq ($(TUNING_PLATFORM),mtk)
snd-soc-sia81xx-objs += sia81xx_tuning_if_dev.o
endif

endif

		
obj-y += snd-soc-sia81xx.o
