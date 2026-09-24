//*******************************************************************************
// PROJECT SETTINGS
//*******************************************************************************

//-- Project name (string). Will be use for output filename
ProjName = "8254";

//-- List of project modules to build (array). If empty, ProjName will be added
ProjModules = [ ProjName ];

//-- List of library modules to build (array)
LibModules = [ "system", "bios", "dos" ];

//-- Additional compilation options (string) - header lookup for the rcx_io
//   library (RCX_INTERFACE UNAPI access - see MSXgl/lib/rcx_io/) and the
//   shared dostools library (see MSXgl/lib/dostools/); no direct Xio_* call
//   in this tool, so xio_io is not needed
CompileOpt = "-I../../lib/rcx_io -I../../lib/dostools";

//-- Prebuilt project-external .lib archives to link (array)
AddLibs = [ "../../lib/rcx_io/rcx_io.lib", "../../lib/dostools/dostools.lib" ];

//-- Target MSX machine version (string)
Machine = "1";

//-- Target program format (string)
Target = "DOS1";

//*******************************************************************************
// SIGNATURE SETTINGS
//*******************************************************************************

AppSignature = true;
AppCompany = "GL";
AppID = "84";

//*******************************************************************************
// BUILD TOOL OPTION
//*******************************************************************************

Verbose = true;
