#CXX=mpic++, this should really be the case after running configure

###############################################################################
# INSTALL DATA & EXTRA_DIST ###################################################
###############################################################################
datadir=${prefix}/data
docsdir=${prefix}/docs

#atmosphere models
atmospheresdir=${datadir}/atmospheres
atmospheres_DATA=data/atmospheres/table3.dat\
               data/atmospheres/table2p3combined.dat\
               data/atmospheres/table2.dat\
               data/atmospheres/ReadMe.txt
EXTRA_DIST=${atmospheres_DATA}

#energy profiles
energyprodir=${datadir}/energy_pro
energypro_DATA=data/energy_pro/Epro.dat\
               data/energy_pro/Epro_mod.dat\
               data/energy_pro/Epro_points.dat
EXTRA_DIST+=${energypro_DATA}

#OPAL/Alexander Furguson EOS and opacity files
opaleos2005dir=${datadir}/OPAL_EOS_2005
opaleos2005_DATA=data/OPAL_EOS_2005/EOS5_0*
EXTRA_DIST+=${opaleos2005_DATA}

GN93hzdir=${datadir}/OPAL_OPACITY_GN93
GN93hz_DATA=data/OPAL_OPACITY_GN93/GN93hz
EXTRA_DIST+=${GN93hz_DATA}

AFOPACITYGN93dir=${datadir}/AF_OPACITY_GN93_1994
AFOPACITYGN93_DATA=data/AF_OPACITY_GN93_1994/g*.*.tron
EXTRA_DIST+=${AFOPACITYGN93_DATA}

#eos files
eosdir=${datadir}/eos
eos_DATA=data/eos/convert.cpp\
         data/eos/Makefile\
         data/eos/eosCTOPAC\
         data/eos/eosY240Z002\
         data/eos/eosY300Z002\
         data/eos/eosNewY240Z002_wider_finer\
         data/eos/eosNewY240Z002.xml\
         data/eos/eosNewY300Z002_wider_finer\
         data/eos/eosNewY240Z0005_wider_finer
         
EXTRA_DIST+=${eos_DATA}

#ref calcs, will include .svn files :(, not a problem under git
refcalcsdir=${datadir}/ref_calcs
refcalcs1DNAdir=${refcalcsdir}/1DNA
refcalcs1DNA_DATA=data/ref_calcs/1DNA/1*\
              data/ref_calcs/1DNA/*.xml\
              data/ref_calcs/1DNA/*.txt
EXTRA_DIST+=${refcalcs1DNA_DATA}
refcalcs2DNAdir=${refcalcsdir}/2DNA
refcalcs2DNA_DATA=data/ref_calcs/2DNA/2*\
              data/ref_calcs/2DNA/*.xml\
              data/ref_calcs/2DNA/*.txt
EXTRA_DIST+=${refcalcs2DNA_DATA}
refcalcs3DNAdir=${refcalcsdir}/3DNA
refcalcs3DNA_DATA=data/ref_calcs/3DNA/3*\
              data/ref_calcs/3DNA/*.xml\
              data/ref_calcs/3DNA/*.txt
EXTRA_DIST+=${refcalcs3DNA_DATA}

#velocity profiles
velocityprodir=${datadir}/velocity_pro
velocitypro_DATA=data/velocity_pro/*.dat
EXTRA_DIST+=${velocitypro_DATA}

#Manuals
manualsdir=${docsdir}/manuals
manuals_DATA=\
  docs/manuals/SPHERLS_Ref_Manual.pdf\
  docs/manuals/SPHERLSgen_Ref_Manual.pdf\
  docs/manuals/SPHERLSanal_Ref_Manual.pdf\
  docs/manuals/Scripts_Ref_Manual.pdf\
  docs/manuals/SPHERLS_User_Guide.pdf
EXTRA_DIST+=${manuals_DATA}

#template xml files
templatexmldir=${docsdir}/templateXML
templatexml_DATA=docs/templateXML/*.xml
EXTRA_DIST+=${templatexml_DATA}

#examples
examplesdir=${docsdir}/examples
createaneosdir=${examplesdir}/create_an_eos
createaneos_DATA=\
  docs/examples/create_an_eos/eosNewY25Z01.xml\
  docs/examples/create_an_eos/README.txt
EXTRA_DIST+=${createaneos_DATA}

runningdir=${examplesdir}/running
running2dslicesdir=${examplerunningdir}/2D_slices
runningoutputdir=${examplerunningdir}/output
runningprofileplotsdir=${examplerunningdir}/profile_plots
running_DATA=\
  docs/examples/running/README.txt\
  docs/examples/running/SPHERLS.xml\
  docs/examples/running/SPHERLSgen.xml\
  docs/examples/running/SPHERLSgen_first_try.xml\
  docs/examples/running/plot_2DSlices.xml\
  docs/examples/running/plot_profile_output.xml\
  docs/examples/running/slurm_job_script_template.sh\
  docs/examples/running/starting_model_structure_first_try.pdf
EXTRA_DIST+=${running_DATA}

#scripts
EXTRA_DIST+=scripts/*.py

#python c++ extensions
EXTRA_DIST+=src/pythonextensions/hdf/hdfmodule.cpp
EXTRA_DIST+=src/pythonextensions/hdf/setup.py.in
EXTRA_DIST+=src/pythonextensions/eos/eos.pyx
EXTRA_DIST+=src/pythonextensions/eos/setup.py

#supporting libraries, included if --disable-include-crucial-libs not set in
#configuration
if INCLUDE_CRUCIAL_LIBS_IN_DIST
EXTRA_DIST+=libs/openmpi-1.6.3.tar.gz
EXTRA_DIST+=libs/petsc-3.1-p8.tar.gz
endif

#supporting libraries, included if --enable-optional-crucial-libs set in
#configuration
if INCLUDE_OPTIONAL_LIBS_IN_DIST
EXTRA_DIST+=libs/evtk.tar.gz
EXTRA_DIST+=libs/Cython-0.19.1.tar.gz
EXTRA_DIST+=libs/fftw-3.3.3.tar.gz
EXTRA_DIST+=libs/doxygen-1.8.3.1.src.tar.gz
EXTRA_DIST+=libs/hdf-4.2.8.tar.gz
EXTRA_DIST+=libs/jpegsrc.v8d.tar.gz#needed by hdf
EXTRA_DIST+=libs/ffmpeg-1.2.tar.gz
EXTRA_DIST+=libs/yasm-1.2.0.tar.gz #needed by ffmpeg
endif

###############################################################################
# DOCS ########################################################################
###############################################################################

EXTRA_DIST+=src/docs/userguide.tex
EXTRA_DIST+=scripts/Doxyfile
EXTRA_DIST+=src/SPHERLSanal/Doxyfile
EXTRA_DIST+=src/SPHERLSgen/Doxyfile
EXTRA_DIST+=src/SPHERLS/Doxyfile
  
###############################################################################
# BINS + SOURCE ###############################################################
###############################################################################
bin_SCRIPTS=./scripts/*.py
if HDF_ENABLE
bin_SCRIPTS+=./src/pythonextensions/lib/hdf.so
endif
if CYTHON_ENABLE
evtkdir=$(prefix)/bin/evtk
evtkexampledir=$(prefix)/bin/evtk/examples
evtkexample_DATA=./src/pythonextensions/lib/evtk/examples/*
evtk_DATA=./src/pythonextensions/lib/evtk/*.py ./src/pythonextensions/lib/evtk/*.pyc ./src/pythonextensions/lib/evtk/*.so ./src/pythonextensions/lib/evtk/LICENSE
bin_SCRIPTS+=./src/pythonextensions/lib/eos.so
endif

bin_PROGRAMS=	SPHERLS	SPHERLSanal	SPHERLSgen 
#dist_pkgdata_DATA=./data/energy_pro/*	./data/eos/*	./data/ref_cals/*	./data/velocity_pro/*
SPHERLS_SOURCES	=	\
	src/SPHERLS/dataManipulation.cpp	\
	src/SPHERLS/dataMonitoring.cpp	\
	src/SPHERLS/dataMonitoring.h	\
	src/SPHERLS/global.cpp	\
	src/SPHERLS/global.h	\
	src/SPHERLS/main.cpp	\
	src/SPHERLS/main.h	\
	src/SPHERLS/physEquations.cpp	\
	src/SPHERLS/physEquations.h	\
	src/SPHERLS/watchzone.cpp	\
	src/SPHERLS/dataManipulation.h	\
	src/SPHERLS/time.h	\
	src/SPHERLS/time.cpp	\
	src/SPHERLS/procTop.h	\
	src/SPHERLS/procTop.cpp	\
	src/SPHERLS/watchzone.h	\
	src/SPHERLS/profileData.cpp	\
	src/SPHERLS/profileData.h	\
	src/SPHERLS/fileExists.h	\
	src/SPHERLS/fileExists.cpp	\
	src/eos.cpp	\
	src/eos.h	\
	src/exception2.cpp	\
	src/exception2.h	\
	src/xmlFunctions.cpp	\
	src/xmlFunctions.h	\
	src/xmlParser.cpp	\
	src/xmlParser.h

SPHERLS_CPPFLAGS	=-Isrc/

SPHERLSanal_SOURCES	=	\
	src/SPHERLSanal/main.cpp	\
	src/SPHERLSanal/main.h	\
	src/eos.h	\
	src/eos.cpp	\
	src/exception2.cpp	\
	src/exception2.h	

if HDF_ENABLE
if CYTHON_ENABLE
BUILT_SOURCES=src/pythonextensions/lib/hdf.so
endif
else
BUILT_SOURCES=
endif
if CYTHON_ENABLE
BUILT_SOURCES+=src/pythonextensions/lib/eos.so
BUILT_SOURCES+=src/pythonextensions/lib/cevtk.so
endif

SPHERLSanal_CPPFLAGS=-Isrc/ 

SPHERLSgen_SOURCES	=	\
	src/SPHERLSgen/main.cpp	\
	src/SPHERLSgen/main.h	\
	src/exception2.cpp	\
	src/exception2.h	\
	src/xmlFunctions.cpp	\
	src/xmlFunctions.h	\
	src/eos.h	\
	src/eos.cpp	\
	src/xmlParser.h	\
	src/xmlParser.cpp
SPHERLSgen_CPPFLAGS	=	-Isrc/

if HDF_ENABLE
src/pythonextensions/lib/hdf.so: src/pythonextensions/hdf/hdfmodule.cpp src/pythonextensions/hdf/setup.py.in
	(cd	src/pythonextensions/hdf;	python setup.py build;python setup.py install --prefix='../' --install-lib=../lib)
endif

#make eos class available in python
if CYTHON_ENABLE
src/pythonextensions/lib/eos.so: src/pythonextensions/eos/eos.pyx src/pythonextensions/eos/setup.py src/eos.cpp src/eos.h src/exception2.h src/exception2.cpp
	(cd	src/pythonextensions/eos;	cp ../../eos.cpp ./eos_tmp.cpp;	cp ../../eos.h .;	cp ../../exception2.h .;	cp ../../exception2.cpp .;)
	(cd	src/pythonextensions/eos;	python setup.py build;python setup.py install --prefix='../' --install-lib=../lib)
endif

#make vtk file python module
cevtk_SOURCE=\
./src/pythonextensions/evtk/setup.py\
./src/pythonextensions/evtk/README.txt\
./src/pythonextensions/evtk/src/hl.py\
./src/pythonextensions/evtk/src/__init__.py\
./src/pythonextensions/evtk/src/cevtk.pyx\
./src/pythonextensions/evtk/src/vtk.py\
./src/pythonextensions/evtk/src/xml.py\
./src/pythonextensions/evtk/src/LICENSE\
./src/pythonextensions/evtk/src/examples/group.py\
./src/pythonextensions/evtk/src/examples/image.py\
./src/pythonextensions/evtk/src/examples/__init__.py\
./src/pythonextensions/evtk/src/examples/lowlevel.py\
./src/pythonextensions/evtk/src/examples/points.py\
./src/pythonextensions/evtk/src/examples/rectilinear.py\
./src/pythonextensions/evtk/src/examples/structured.py

EXTRA_DIST+=${cevtk_SOURCE}

src/pythonextensions/lib/cevtk.so: ${cevtk_SOURCE}
	(cd	src/pythonextensions/evtk;	python setup.py build;python setup.py install --prefix='../' --install-lib=../lib)
  
CLEANFILES=\
src/docs/userguide.pdf\
src/docs/userguide.out\
src/docs/userguide.log\
src/docs/userguide.aux\
src/pythonextensions/eos/exception2.h\
src/pythonextensions/eos/exception2.cpp\
src/pythonextensions/eos/eos_tmp.cpp\
src/pythonextensions/eos/eos.h\
src/pythonextensions/eos/eos.cpp\
src/pythonextensions/evtk/src/cevtk.c

DISTCLEANFILES=\
src/pythonextensions/hdf/setup.py

clean-local:
	-rm -rf src/pythonextensions/eos/build
	-rm -rf src/pythonextensions/hdf/build
	-rm -rf src/pythonextensions/lib
	-rm -rf src/pythonextensions/evtk/build

if MAKE_DOCS

#docs/manuals/SPHERLS_Ref_Manual.pdf
#docs/manuals/SPHERLSgen_Ref_Manual.pdf
#docs/manuals/SPHERLSanal_Ref_Manual.pdf
#docs/manuals/Scripts_Ref_Manual.pdf
#fuck doxygen, should just use latex
docs: docs/manuals/SPHERLS_User_Guide.pdf

.PHONY:docs

docs/manuals/SPHERLS_User_Guide.pdf: src/docs/userguide.tex
	(cd	src/docs;	pdflatex userguide.tex)
	(cd	src/docs;	pdflatex userguide.tex)#needs to be run twice to properly pick up bookmarks
	(cd	src/docs;	cp userguide.pdf ../../docs/manuals/SPHERLS_User_Guide.pdf)

#docs/manuals/SPHERLS_Ref_Manual.pdf:	${SPHERLS_SOURCES}	src/SPHERLS/Doxyfile
#	(cd	src/SPHERLS;echo	${PWD};	doxygen)
#	(cd	src/SPHERLS/docs/latex;	make	pdf;)
#	(cd	src/SPHERLS/docs/latex;mv	refman.pdf	../../../../docs/manuals/SPHERLS_Ref_Manual.pdf)

#docs/manuals/SPHERLSgen_Ref_Manual.pdf:	${SPHERLSgen_SOURCES}	src/SPHERLSgen/Doxyfile
#	(cd	src/SPHERLSgen;	doxygen)
#	(cd	src/SPHERLSgen/docs/latex;	make	pdf;)
#	(cd	src/SPHERLSgen/docs/latex;mv	refman.pdf	../../../../docs/manuals/SPHERLSgen_Ref_Manual.pdf)

#docs/manuals/SPHERLSanal_Ref_Manual.pdf:	${SPHERLSanal_SOURCES}	src/SPHERLSanal/Doxyfile
#	(cd	src/SPHERLSanal;	doxygen)
#	(cd	src/SPHERLSanal/docs/latex;	make	pdf;)
#	(cd	src/SPHERLSanal/docs/latex;mv	refman.pdf	../../../../docs/manuals/SPHERLSanal_Ref_Manual.pdf)

#docs/manuals/Scripts_Ref_Manual.pdf:	scripts/Doxyfile	scripts/*.py
#	(cd	scripts;	doxygen)
#	(cd	scripts/docs/latex;	make	pdf;)
#	(cd	scripts/docs/latex;mv	refman.pdf	../../../docs/manuals/Scripts_Ref_Manual.pdf)

endif