//*******************************************************************************
// PROJECT SETTINGS
//*******************************************************************************

//-- Project name (string). Will be use for output filename
ProjName = "XIOPIC";

//-- List of project modules to build (array). If empty, ProjName will be added
ProjModules = [ ProjName ];

//-- List of library modules to build (array)
LibModules = [ "system", "bios", "dos", "memory" ];

//-- Additional compilation options (string) - header lookup for the shared
//   xio_io library (see MSXgl/lib/xio_io/) and the shared dostools library
//   (see MSXgl/lib/dostools/)
CompileOpt = "-I../../lib/xio_io -I../../lib/dostools";

//-- Prebuilt project-external .lib archives to link (array)
AddLibs = [ "../../lib/xio_io/xio_io.lib", "../../lib/dostools/dostools.lib" ];

//-- Target MSX machine version (string)
Machine = "1";

//-- Target program format (string)
Target = "DOS1";

//*******************************************************************************
// SIGNATURE SETTINGS
//*******************************************************************************

AppSignature = true;
AppCompany = "GL";
AppID = "XP";

//*******************************************************************************
// BUILD TOOL OPTION
//*******************************************************************************

Verbose = true;
