//*******************************************************************************
// PROJECT SETTINGS
//*******************************************************************************

//-- Project name (string). Will be use for output filename
ProjName = "8255I";

//-- List of project modules to build (array). If empty, ProjName will be added
ProjModules = [ ProjName ];

//-- List of library modules to build (array)
LibModules = [ "system", "bios", "dos" ];

//-- Additional compilation options (string) - header lookup for xio_io (the
//   real Xio_* PIC-interrupt calls this port needs - see MSXgl/lib/xio_io/),
//   rcx_io (RCX_INTERFACE UNAPI access - see MSXgl/lib/rcx_io/), and the
//   shared dostools library (see MSXgl/lib/dostools/)
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
AppID = "8I";

//*******************************************************************************
// BUILD TOOL OPTION
//*******************************************************************************

Verbose = true;
