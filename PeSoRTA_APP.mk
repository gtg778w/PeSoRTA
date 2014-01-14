
BINS= $(APP_BINDIR)/base_$(APP_NAME) $(APP_BINDIR)/cmusphinx_$(APP_NAME) \
$(APP_BINDIR)/ffmpeg_$(APP_NAME) $(APP_BINDIR)/sqrwav_$(APP_NAME) $(APP_BINDIR)/membound_$(APP_NAME)

PeSoRTA_apps: $(BINS)

libPeSoRTA:
	$(MAKE) -C $(PeSoRTADIR)

$(APP_BINDIR)/base_$(APP_NAME): $(APP_OBJS) libPeSoRTA
	$(CC) $(APP_LIBFLAGS1) -L $(PeSoRTA_LIBDIR) -o $(APP_BINDIR)/base_$(APP_NAME) \
	$(APP_OBJS) $(APP_LIBFLAGS2) \
	-lPeSoRTA_base

$(APP_BINDIR)/cmusphinx_$(APP_NAME): $(APP_OBJS) libPeSoRTA
	$(CC) $(APP_LIBFLAGS1) -L $(PeSoRTA_LIBDIR) -o $(APP_BINDIR)/cmusphinx_$(APP_NAME) \
	$(APP_OBJS) $(APP_LIBFLAGS2) \
	-lPeSoRTA_cmusphinx \
	-lpocketsphinx -lsphinxad -lsphinxbase

$(APP_BINDIR)/ffmpeg_$(APP_NAME): $(APP_OBJS) libPeSoRTA
	$(CC) $(APP_LIBFLAGS1) -L $(PeSoRTA_LIBDIR) -o $(APP_BINDIR)/ffmpeg_$(APP_NAME) \
	$(APP_OBJS) $(APP_LIBFLAGS2) \
	-lPeSoRTA_ffmpeg \
	-lavformat -lswresample -lswscale -lavcodec -lavfilter -lavutil \
	-lmp3lame -lopencore-amrnb -lopus -lspeex -lvorbis -lvorbisenc -lvpx \
	-lx264 -lz -lbz2 -lm

$(APP_BINDIR)/sqrwav_$(APP_NAME): $(APP_OBJS) libPeSoRTA
	$(CC) $(APP_LIBFLAGS1) -L $(PeSoRTA_LIBDIR) -o $(APP_BINDIR)/sqrwav_$(APP_NAME) \
	$(APP_OBJS) $(APP_LIBFLAGS2) \
	-lPeSoRTA_sqrwav \
	-lrt

$(APP_BINDIR)/membound_$(APP_NAME): $(APP_OBJS) libPeSoRTA
	$(CC) $(APP_LIBFLAGS1) -L $(PeSoRTA_LIBDIR) -o $(APP_BINDIR)/membound_$(APP_NAME) \
	$(APP_OBJS) $(APP_LIBFLAGS2) \
	-lPeSoRTA_membound
	
libPeSoRTA_clean:
	$(MAKE) -C $(PeSoRTADIR) clean;

PeSoRTA_apps_clean: libPeSoRTA_clean
	rm -rf $(BINS)

