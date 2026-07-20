################################################################################
# micro T-Kernel 3.00.05  makefile
################################################################################

OBJS += \
./mtkernel_3/kernel/sysdepend/cpu/nrf5/cpu_clock.o \
./mtkernel_3/kernel/sysdepend/cpu/nrf5/vector_tbl.o

C_DEPS += \
./mtkernel_3/kernel/sysdepend/cpu/nrf5/cpu_clock.d \
./mtkernel_3/kernel/sysdepend/cpu/nrf5/vector_tbl.d

mtkernel_3/kernel/sysdepend/cpu/nrf5/%.o: ../kernel/sysdepend/cpu/nrf5/%.c
	@echo 'Building file: $<'
	$(GCC) $(CFLAGS) -D$(TARGET) $(INCPATH) -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '
