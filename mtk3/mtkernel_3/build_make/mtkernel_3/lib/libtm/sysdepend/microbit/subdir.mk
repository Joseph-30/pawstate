################################################################################
# micro T-Kernel 3.00.05  makefile
################################################################################

OBJS += \
./mtkernel_3/lib/libtm/sysdepend/microbit/tm_com.o

C_DEPS += \
./mtkernel_3/lib/libtm/sysdepend/microbit/tm_com.d

mtkernel_3/lib/libtm/sysdepend/microbit/%.o: ../lib/libtm/sysdepend/microbit/%.c
	@echo 'Building file: $<'
	$(GCC) $(CFLAGS) -D$(TARGET) $(INCPATH) -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '
