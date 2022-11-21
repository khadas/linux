ifeq ($(CONFIG_AMLOGIC_RAMDUMP),y)
zreladdr-y    += 0x02108000
params_phys-y    := 0x02100100
initrd_phys-y    := 0x02a00000
else
zreladdr-y    += 0x00108000
params_phys-y    := 0x00100100
initrd_phys-y    := 0x00a00000
endif