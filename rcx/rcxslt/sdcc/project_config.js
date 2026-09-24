//*******************************************************************************
// PROJECT SETTINGS
//*******************************************************************************

//-- Project name (string). Will be use for output filename
ProjName = "RCXSLT";

//-- List of project modules to build (array). If empty, ProjName will be added
ProjModules = [ ProjName ];

//-- List of library modules to build (array)
LibModules = [ "system", "bios", "dos" ];

//-- Additional compilation options (string) - header lookup for the shared
//   dostools library (see MSXgl/lib/dostools/) - only its Dos_ReadLine/
//   PrintHex2 helpers are used here, no Xio_*/RCX call at all (this tool is
//   generic BIOS-only, like the BASIC original; shared across the XIO and
//   RCX SDCC ports since those helpers are plain MSX-DOS console I/O,
//   unrelated to either UNAPI driver)
CompileOpt = "-I../../lib/dostools";

//-- Prebuilt project-external .lib archives to link (array)
AddLibs = [ "../../lib/dostools/dostools.lib" ];

//-- Target MSX machine version (string)
Machine = "1";

//-- Target program format (string)
Target = "DOS1";

//*******************************************************************************
// SIGNATURE SETTINGS
//*******************************************************************************

AppSignature = true;
AppCompany = "GL";
AppID = "RS";

//*******************************************************************************
// BUILD TOOL OPTION
//*******************************************************************************

Verbose = true;
