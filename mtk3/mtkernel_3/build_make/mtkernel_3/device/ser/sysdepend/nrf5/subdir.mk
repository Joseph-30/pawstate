################################################################################
# micro T-Kernel 3.00.05  makefile
################################################################################

OBJS += \
./mtkernel_3/device/ser/sysdepend/nrf5/ser_nrf5.o

C_DEPS += \
./mtkernel_3/device/ser/sysdepend/nrf5/ser_nrf5.d

# Each subdirectory must supply rules for building sources it contributes
mtkernel_3/device/ser/sysdepend/nrf5/%.o: ../mtkernel_3/device/ser/sysdepend/nrf5/%.c
	@echo 'Building file: $<'
	$(GCC) $(CFLAGS) -D$(TARGET) $(INCPATH) -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '
