################################################################################
# micro T-Kernel 3.00.05  makefile
################################################################################

OBJS += \
./mtkernel_3/kernel/sysdepend/microbit/devinit.o \
./mtkernel_3/kernel/sysdepend/microbit/hw_setting.o \
./mtkernel_3/kernel/sysdepend/microbit/power_save.o

C_DEPS += \
./mtkernel_3/kernel/sysdepend/microbit/devinit.d \
./mtkernel_3/kernel/sysdepend/microbit/hw_setting.d \
./mtkernel_3/kernel/sysdepend/microbit/power_save.d

mtkernel_3/kernel/sysdepend/microbit/%.o: ../kernel/sysdepend/microbit/%.c
	@echo 'Building file: $<'
	$(GCC) $(CFLAGS) -D$(TARGET) $(INCPATH) -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '
