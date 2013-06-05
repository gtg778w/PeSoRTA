
LIBDIR=lib

LIBS= libPeSoRTA_sqrwav libPeSoRTA_base libPeSoRTA_cmusphinx libPeSoRTA_ffmpeg

all: $(LIBS)

libPeSoRTA_base:
	$(MAKE) -C base OUTLIBDIR=../$(LIBDIR)

libPeSoRTA_cmusphinx:
	$(MAKE) -C cmusphinx OUTLIBDIR=../$(LIBDIR)

libPeSoRTA_ffmpeg:
	$(MAKE) -C ffmpeg OUTLIBDIR=../$(LIBDIR)

libPeSoRTA_sqrwav:
	$(MAKE) -C sqrwav OUTLIBDIR=../$(LIBDIR)

clean:
	$(MAKE) -C base OUTLIBDIR=../$(LIBDIR) clean
	$(MAKE) -C cmusphinx OUTLIBDIR=../$(LIBDIR) clean
	$(MAKE) -C ffmpeg OUTLIBDIR=../$(LIBDIR) clean
	$(MAKE) -C sqrwav OUTLIBDIR=../$(LIBDIR) clean

