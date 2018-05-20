
APP = iDeal_hub

# Add any other object files to this list below
APP_OBJS = iDeal_hub.o HUB2_channel.o HUB2_commander.o HUB2_ctrl.o eeprom.o pins.o clk_idt5p49v6914a.o

all: build install

build: $(APP)

$(APP): $(APP_OBJS)
	$(CC)  $(LDFLAGS) -o $@ $(APP_OBJS) $(LDLIBS) -lpthread -liio

clean:
	-rm -f $(APP) *.elf *.gdb *.o

.PHONY: install image

install: $(APP)
	$(TARGETINST) -d $(APP) /bin/$(APP)
	$(TARGETINST) -d -p 0755 z_carrier_unius_startup.sh /etc/init.d/z_carrier_unius_startup.sh
	$(TARGETINST) -s /etc/init.d/z_carrier_unius_startup.sh /etc/rc5.d/S99z_carrier_unius_startup.sh

%.o: %.c
	$(CC) -c $(CFLAGS) -DUSE_IIO -UDBG_HW_SIMULATION -o $@ $<

help:
	@echo ""
	@echo "Quick reference for various supported build targets for $(INSTANCE)."
	@echo "----------------------------------------------------"
	@echo "  clean                  clean out build objects"
	@echo "  all                    build $(INSTANCE) and install to rootfs host copy"
	@echo "  build                  build subsystem"
	@echo "  install                install built objects to rootfs host copy"
