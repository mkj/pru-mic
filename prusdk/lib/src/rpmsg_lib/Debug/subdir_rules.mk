################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Each subdirectory must supply rules for building sources it contributes
pru_rpmsg.obj: ../pru_rpmsg.c $(GEN_OPTS) $(GEN_HDRS)
	@echo 'Building file: $<'
	@echo 'Invoking: PRU Compiler'
	"/home/a0273976local/ti/ccsv6/tools/compiler/ti-cgt-pru_2.1.1/bin/clpru" -v3 --include_path="/home/a0273976local/ti/ccsv6/tools/compiler/ti-cgt-pru_2.1.1/include" --include_path="../../../../include" -g --define=am3359 --define=pru0 --diag_warning=225 --display_error_number --diag_wrap=off --endian=little --hardware_mac=on --preproc_with_compile --preproc_dependency="pru_rpmsg.pp" $(GEN_OPTS__FLAG) "$(shell echo $<)"
	@echo 'Finished building: $<'
	@echo ' '

pru_virtqueue.obj: ../pru_virtqueue.c $(GEN_OPTS) $(GEN_HDRS)
	@echo 'Building file: $<'
	@echo 'Invoking: PRU Compiler'
	"/home/a0273976local/ti/ccsv6/tools/compiler/ti-cgt-pru_2.1.1/bin/clpru" -v3 --include_path="/home/a0273976local/ti/ccsv6/tools/compiler/ti-cgt-pru_2.1.1/include" --include_path="../../../../include" -g --define=am3359 --define=pru0 --diag_warning=225 --display_error_number --diag_wrap=off --endian=little --hardware_mac=on --preproc_with_compile --preproc_dependency="pru_virtqueue.pp" $(GEN_OPTS__FLAG) "$(shell echo $<)"
	@echo 'Finished building: $<'
	@echo ' '


