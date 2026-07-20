################################################################################
# micro T-Kernel 3.00.05  makefile
################################################################################

OBJS += \
./mtkernel_3/lib/libtk/sysdepend/cpu/nrf5/int_nrf5.o \
./mtkernel_3/lib/libtk/sysdepend/cpu/nrf5/ptimer_nrf5.o

C_DEPS += \
./mtkernel_3/lib/libtk/sysdepend/cpu/nrf5/int_nrf5.d \
./mtkernel_3/lib/libtk/sysdepend/cpu/nrf5/ptimer_nrf5.d

mtkernel_3/lib/libtk/sysdepend/cpu/nrf5/%.o: ../lib/libtk/sysdepend/cpu/nrf5/%.c
	@echo 'Building file: $<'
	$(GCC) $(CFLAGS) -D$(TARGET) $(INCPATH) -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '
