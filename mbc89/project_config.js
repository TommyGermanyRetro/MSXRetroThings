//*******************************************************************************
// PROJECT SETTINGS
//*******************************************************************************

//-- Project name (string). Will be use for output filename
ProjName = "Mbc89";

//-- List of project modules to build (array). If empty, ProjName will be added
ProjModules = [ ProjName ];

//-- List of library modules to build (array) - vdp/print/input for the GUI on
//   top of the base system/bios/dos set SJA1000 already uses for UNAPI access,
//   memory for mbc89_store.c's Mem_Set/Mem_Copy (Increment 7 persistence)
LibModules = [ "system", "bios", "dos", "vdp", "print", "input", "memory" ];

//-- Additional sources to be compiled and linked with the project (array)
AddSources = [ "mbc89_sim.c", "mbc89_can.c", "mbc89_base.c", "mbc89_com.c", "mbc89_gui.c", "mbc89_store.c" ];

//-- Additional compilation options (string) - header lookup for xio_io (the
//   real Xio_* SET ADDR/SET MASK calls this port needs - see
//   MSXgl/lib/xio_io/), rcx_io (RCX_INTERFACE/CAN UNAPI access - see
//   MSXgl/lib/rcx_io/), and the shared dostools library (see
//   MSXgl/lib/dostools/)
CompileOpt = "-I../../lib/xio_io -I../../lib/rcx_io -I../../lib/dostools";

//-- Prebuilt project-external .lib archives to link (array)
AddLibs = [ "../../lib/xio_io/xio_io.lib", "../../lib/rcx_io/rcx_io.lib", "../../lib/dostools/dostools.lib" ];

//-- Target MSX machine version (string)
Machine = "1";

//-- Target program format (string)
Target = "DOS1";

//*******************************************************************************
// SIGNATURE SETTINGS
//*******************************************************************************

AppSignature = true;
AppCompany = "GL";
AppID = "89";

//*******************************************************************************
// BUILD TOOL OPTION
//*******************************************************************************

Verbose = true;
