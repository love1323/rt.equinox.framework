/*******************************************************************************
 * Copyright (c) 2000, 2006 IBM Corporation and others.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at 
 * http://www.eclipse.org/legal/epl-v10.html
 * 
 * Contributors:
 *     IBM Corporation - initial API and implementation
 *     Kevin Cornell (Rational Software Corporation)
 *******************************************************************************/

/* Eclipse Program Launcher
 *
 * This program performs the launching of the java VM used to
 * start the Eclipse or RCP java application.
 * As an implementation detail, this program serves two other
 * purposes: display a splash window and write to a segment
 * of shared memory.
 *
 * The java application receives the following arguments.
 * -launcher <launcher absolute name. e.g. d:\eclipse\eclipse.exe>
 * -name <application name. e.g. Eclipse>
 * If the splash window is to be displayed, the java application
 * will receive two extra arguments:
 *     -showsplash  <splash time out in seconds e.g. 600>
 *
 * When the Java program starts, it should determine the location of
 * the splash bitmap to be used. The Java program initiates the 
 * displaying of the splash window by executing the splash command 
 * as follows:
 *
 * Process splashProcess = Runtime.getRuntime().exec( array );
 * Where array stores String arguments in the following order:
 * 0. <launcher absolute name. e.g. d:\eclipse\eclipse.exe>
 * 1. -name
 * 2. <application name. e.g. Eclipse>
 * 3. -showsplash
 * 4. <splash time out in seconds e.g. 600>
 * 5. <absolute path of the splash screen e.g. d:\eclipse\splash.bmp>
 *
 * When the Java program initialization is complete, the splash window
 * is brought down by destroying the splash process as follows:
 *
 *     splashProcess.destroy();
 *
 * Therefore, when the splash window is visible, there are actually three
 * processes running: 
 * 1) the main launcher process 
 * 2) the Java VM process (Eclipse or RCP application) 
 * 3) the splash window process.
 *
 * The splash screen process can also show progress information. The
 * communication between the Java VM process and the splash screen
 * process is done through the standard input pipe. Messages sent to
 * the splash screen process have this format:
 *
 *    <key>=<value><LF>
 *
 * and the recognized messages are:
 *
 *    value=50\n (changes the current progress value)
 *    maximum=100\n (changes the maximum progress value. Default is 100)
 *    message=starting...\n (changes the displayed message. Any char except LF is allowed)
 *    foreground=RRGGBB\n  (changes the foreground color of the message, i.e. cyan=(0 << 16 | 255 << 8 | 255 << 0))
 *    messageRect=10,10,100,20\n (changes the rectangle(x,y,width,height) where the message is displayed)
 *    progressRect=10,30,100,15\n (changes the rectangle(x,y,width,height) where the progress is displayed)
 *
 * Similarly, the Java application will receive two other arguments:
 *    -exitdata <shared memory id>
 *
 * The exitdata command can be used by the Java application
 * to provide specific exit data to the main launcher process. The 
 * following causes another instance of the launcher to write to the 
 * segment of shared memory previously created by the
 * main launcher.
 *
 * Process exitDataProcess = Runtime.getRuntime().exec( array );
 * exitDataProcess.waitFor();
 * Where array stores String arguments in the following order:
 * 0. <launcher absolute name. e.g. d:\eclipse\eclipse.exe>
 * 1. -name
 * 2. <application name. e.g. Eclipse>
 * 3. -exitdata
 * 4. <shared memory id e.g. c60_7b4>
 * 5. <exit data that either contain a series of characters>
 *
 * The exit data size must not exceed MAX_SHARED_LENGTH which is
 * 16Kb. The exit data process will exit with an exit code
 * different than 0 if that happens. The interpretation of the
 * exit data is dependent on the exit value of the java application.
 *
 * The main launcher recognizes the following exit codes from the
 * Java application:
 *
 *    0
 *       - Exit normally.
 *    RESTART_LAST_EC = 23
 *       - restart the java VM again with the same arguments as the previous one.
 *    RESTART_NEW_EC  = 24
 *       - restart the java VM again with the arguments taken from the exit data.
 *       The exit data format is a list of arguments separated by '\n'. The Java
 *       application should build this list using the arguments passed to it on
 *       startup. See below.
 *
 * Additionally, if the Java application exits with an exit code other than the
 * ones above, the main launcher will display an error message with the contents
 * of the exit data. If the exit data is empty, a generic error message is
 * displayed. The generic error message shows the exit code and the arguments
 * passed to the Java application.
 *
 * The options that can be specified by the user to the launcher are:
 *  -vm <javaVM>               the Java VM to be used
 *  -os <opSys>                the operating system being run on
 *  -arch <osArch>             the hardware architecture of the OS: x86, sparc, hp9000
 *  -ws <gui>                  the window system to be used: win32, motif, gtk, ...
 *  -nosplash                  do not display the splash screen. The java application will
 *                             not receive the -showsplash command.
 *  -name <name>               application name displayed in error message dialogs and
 *                             splash screen window. Default value is computed from the
 *                             name of the executable - with the first letter capitalized
 *                             if possible. e.g. eclipse.exe defaults to the name Eclipse.
 *  -startup                   the startup jar to execute. The argument is first assumed to be
 *                             relative to the path of the launcher. If such a file does not
 *                             exist, the argument is then treated as an absolute path.
 *                             The default is to execute a jar called startup.jar in the folder
 *                             where the launcher is located.
 *                             The jar must be an executable jar.
 *                             e.g. -startup myfolder/myJar.jar will cause the launcher to start
 *                             the application: java -jar <launcher folder>/myfolder/myJar.jar
 *  <userArgs>                 arguments that are passed along to the Java application
 *                             (i.e, -data <path>, -debug, -console, -consoleLog, etc) 
 *  -vmargs <userVMargs> ...   a list of arguments for the VM itself
 *
 * The -vmargs option and all user specified VM arguments must appear
 * at the end of the command line, after all arguments that are
 * being passed to Java application. 
 *
 * The argument order for the new Java VM process is as follows:
 *
 * <javaVM> <all VM args>
 *     -os <user or default OS value>
 *     -ws <user or default WS value>
 *     -arch <user or default ARCH value>
 *     -launcher <absolute launcher name>
 *     -name <application name>
 *     [-showsplash <splash time out>]
 *     [-exitdata <shared memory id>]
 *     <userArgs>
 *     -vm <javaVM>
 *     -vmargs <all VM args>
 *
 * where:
 *   <all VM args> =
 *     [<defaultVMargs | <userVMargs>]
 *     -jar
 *     <startup jar full path>
 *	
 * The startup jar must be an executable jar.
 * 
 *
 * See "Main.java" for a simple implementation of the Java
 * application.
 *
 * Configuration file
 *   The launcher gets arguments from the command line and/or from a configuration file.
 * The configuration file must have the same name and location as the launcher executable
 * and the extension .ini. For example, the eclipse.ini configuration file must be
 * in the same folder as the eclipse.exe or eclipse executable.
 *   The format of the ini file matches that of the command line arguments - one
 * argument per line.
 *   In general, the settings of the config file are expected to be overriden by the
 * command line.
 *   - launcher arguments (-os, -arch...) set in the config file are overriden by the command line
 *   - the -vmargs from the command line replaces in its entirety the -vmargs from the config file.
 *   - user arguments from the config file are prepended to the user arguments defined in the
 *     config file. This is consistent with the java behaviour in the following case:
 *     java -Dtest="one" -Dtest="two" ...  : test is set to the value "two"
 */

#include "eclipseOS.h"
#include "eclipseUtil.h"
#include "eclipseShm.h"
#include "eclipseJNI.h"
#include "eclipseConfig.h"
#include "eclipseCommon.h"

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#include <strings.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>

#define MAX_PATH_LENGTH   2000
#define MAX_SHARED_LENGTH   (16 * 1024)

/* Global Data */
static _TCHAR*  program     = NULL;       /* full pathname of the program */
static _TCHAR*  programDir  = NULL;       /* directory where program resides */
static _TCHAR*  javaVM      = NULL;       /* full pathname of the Java VM to run */
static _TCHAR*  jniLib		= NULL;		  /* full path of a java vm library for JNI invocation */
static _TCHAR*  jarFile     = NULL;		  /* full pathname of the startup jar file to run */
static _TCHAR*  sharedID    = NULL;       /* ID for the shared memory */

_TCHAR*  exitData    = NULL;		  /* exit data set from Java */
int		 initialArgc;
_TCHAR** initialArgv;

 	
/* Define the special exit codes returned from Eclipse. */
#define RESTART_LAST_EC    23
#define RESTART_NEW_EC     24

/* Define error messages. (non-NLS) */
static _TCHAR* exitMsg = _T_ECLIPSE("JVM terminated. Exit code=%d\n%s");
static _TCHAR* goVMMsg = _T_ECLIPSE("Start VM: %s\n");
static _TCHAR* pathMsg = _T_ECLIPSE("%s\n'%s' in your current PATH");
static _TCHAR* shareMsg = _T_ECLIPSE("No exit data available.");
static _TCHAR* noVMMsg =
_T_ECLIPSE("A Java Runtime Environment (JRE) or Java Development Kit (JDK)\n\
must be available in order to run %s. No Java virtual machine\n\
was found after searching the following locations:\n\
%s");
static _TCHAR* startupMsg =
_T_ECLIPSE("The %s executable launcher was unable to locate its \n\
companion startup jar file.");

static _TCHAR* homeMsg =
_T_ECLIPSE("The %s executable launcher was unable to locate its \n\
companion startup.jar file (in the same directory as the executable).");

#define DEFAULT_STARTUP 		_T_ECLIPSE("startup.jar")
#define CLASSPATH_PREFIX        _T_ECLIPSE("-Djava.class.path=")

/* Define constants for the options recognized by the launcher. */
#define CONSOLE      _T_ECLIPSE("-console")
#define CONSOLELOG   _T_ECLIPSE("-consoleLog")
#define DEBUG        _T_ECLIPSE("-debug")
#define OS           _T_ECLIPSE("-os")
#define OSARCH       _T_ECLIPSE("-arch")
#define NOSPLASH     _T_ECLIPSE("-nosplash")
#define LAUNCHER     _T_ECLIPSE("-launcher")
#define SHOWSPLASH   _T_ECLIPSE("-showsplash")
#define EXITDATA     _T_ECLIPSE("-exitdata")
#define STARTUP      _T_ECLIPSE("-startup")
#define LIBRARY		 _T_ECLIPSE("-library")
#define VM           _T_ECLIPSE("-vm")
#define WS           _T_ECLIPSE("-ws")
#define NAME         _T_ECLIPSE("-name")
#define VMARGS       _T_ECLIPSE("-vmargs")					/* special option processing required */
#define CP			 _T_ECLIPSE("-cp")
#define CLASSPATH    _T_ECLIPSE("-classpath")
#define JAR 		 _T_ECLIPSE("-jar")

/* Define the variables to receive the option values. */
static int     needConsole   = 0;				/* True: user wants a console	*/
static int     debug         = 0;				/* True: output debugging info	*/
static int     noSplash      = 0;				/* True: do not show splash win	*/

static _TCHAR*  showSplashArg = NULL;			/* showsplash data (main launcher window) */
static _TCHAR * startupArg    = NULL;			/* path of the startup.jar the user wants to run relative to the program path */
static _TCHAR*  vmName        = NULL;     		/* Java VM that the user wants to run */
static _TCHAR*  name          = NULL;			/* program name */	
static _TCHAR*  library       = NULL;			/* the shared library */

/* Define a table for processing command line options. */
typedef struct
{
	_TCHAR*  name;		/* the option recognized by the launcher */
	_TCHAR** value;		/* the variable where the option value is saved */
	int*   flag;		/* the variable that is set if the option is defined */
	int    remove;		/* the number of argments to remove from the list */
} Option;
static Option options[] = {
    { CONSOLE,		NULL,			&needConsole,	0 },
    { CONSOLELOG,	NULL,			&needConsole,	0 },
    { DEBUG,		NULL,			&debug,			0 },
    { NOSPLASH,     NULL,           &noSplash,		1 },
    { LIBRARY,		NULL,			NULL,			2 }, /* library was parsed by exe, just remove it */
    { OS,			&osArg,			NULL,			2 },
    { OSARCH,		&osArchArg,		NULL,			2 },
    { SHOWSPLASH,   &showSplashArg,	NULL,			2 },
    { STARTUP,		&startupArg,	NULL,			2 },
    { VM,           &vmName,		NULL,			2 },
    { NAME,         &name,			NULL,			2 },
    { WS,			&wsArg,			NULL,			2 } };
static int optionsSize = (sizeof(options) / sizeof(options[0]));

/* Define the required VM arguments (all platforms). */
static _TCHAR*  cp = NULL;
static _TCHAR*  cpValue = NULL;					
static _TCHAR** reqVMarg[] = { &cp, &cpValue, NULL };	/* required VM args */
static _TCHAR** userVMarg     = NULL;     				/* user specific args for the Java VM  */

/* Local methods */
static void     parseArgs( int* argc, _TCHAR* argv[] );
static void     getVMCommand( int argc, _TCHAR* argv[], _TCHAR **vmArgv[], _TCHAR **progArgv[] );
static _TCHAR** buildLaunchCommand( _TCHAR* program, _TCHAR** vmArgs, _TCHAR** progArgs );
static _TCHAR** parseArgList( _TCHAR *data );
static _TCHAR*  formatVmCommandMsg( _TCHAR* args[], _TCHAR* vmArgs[], _TCHAR* progArgs[] );
static _TCHAR*  getDefaultOfficialName();
static _TCHAR*  findStartupJar();
static _TCHAR** getRelaunchCommand( _TCHAR **vmCommand );

#ifdef _WIN32
static void     createConsole();
#endif

/* Record the arguments that were used to start the original executable */
JNIEXPORT void setInitialArgs(int argc, _TCHAR** argv, _TCHAR* lib) {
	initialArgc = argc;
	initialArgv = argv;
	library = lib;
}

/* this method must match the RunMethod typedef in eclipseMain.c */
/* vmArgs must be NULL terminated                                */
JNIEXPORT int run(int argc, _TCHAR* argv[], _TCHAR* vmArgs[])
{
    _TCHAR*   shippedVM    = NULL;
    _TCHAR*   vmSearchPath = NULL;
    _TCHAR**  vmCommand = NULL;
    _TCHAR**  vmCommandArgs = NULL;
    _TCHAR**  progCommandArgs = NULL;
    _TCHAR*   vmCommandMsg = NULL;
    _TCHAR**  relaunchCommand = NULL;
    _TCHAR*   errorMsg;
    int       exitCode;
    int 	  running = 1;
    int 	  jniLaunching = 1;
	 
	/* arg[0] should be the full pathname of this program. */
    program = _tcsdup( argv[0] );
	
    /* Parse command line arguments (looking for the VM to use). */
    /* Override configuration file arguments */
    parseArgs( &argc, argv );

	/* Initialize official program name */
	officialName = name != NULL ? _tcsdup( name ) : getDefaultOfficialName();

    /* Initialize the window system. */
    initWindowSystem( &argc, argv, !noSplash );
    
    /* Find the directory where the Eclipse program is installed. */
    programDir = getProgramDir();
    if (programDir == NULL)
    {
        errorMsg = malloc( (_tcslen(homeMsg) + _tcslen(officialName) + 10) * sizeof(_TCHAR) );
        _stprintf( errorMsg, homeMsg, officialName );
        displayMessage( officialName, errorMsg );
        free( errorMsg );
    	exit( 1 );
    }

    if (vmName != NULL) {
    	/* user specified VM */
    	javaVM = findCommand(vmName);
    	jniLaunching = isVMLibrary(javaVM);
    }
    else if (vmName == NULL)
    {
    	/* VM not specified, Determine which type of VM should be used. */
    	vmName = defaultVM;

        /* Try to find the VM shipped with eclipse. */
        shippedVM = malloc( (_tcslen( programDir ) + _tcslen( shippedVMDir ) + _tcslen( vmName ) + 10) * sizeof(_TCHAR) );
        _stprintf( shippedVM, _T_ECLIPSE("%s%s%s"), programDir, shippedVMDir, vmName );
        javaVM = findCommand( shippedVM );

        /* Format a message to indicate the default VM search path. */
        vmSearchPath = malloc( (_tcslen( pathMsg ) + _tcslen( shippedVM ) + _tcslen( vmName ) + 10) * sizeof(_TCHAR) );
        _stprintf( vmSearchPath, pathMsg, shippedVM, vmName );
        free( shippedVM );
        shippedVM = NULL;
	}

	/* If a Java VM has not been found yet */
	if (javaVM == NULL)
	{
		/* Either verify the VM specified by the user or
		   attempt to find the VM in the user's PATH. */
		javaVM = findCommand( vmName );
	}

	if(jniLaunching) {
		jniLib = findVMLibrary( javaVM );
		if(jniLib == NULL)
			jniLaunching = 0;
	}
	/* If the VM was not found, display a message and exit. */
	if (!jniLaunching && javaVM == NULL)
	{
		if (vmSearchPath != NULL) vmName = vmSearchPath; /* used default VM searching */
    	errorMsg = malloc( (_tcslen(noVMMsg) + _tcslen(officialName) + _tcslen(vmName) + 10) * sizeof(_TCHAR) );
    	_stprintf( errorMsg, noVMMsg, officialName, vmName );
    	displayMessage( officialName, errorMsg );
    	free( errorMsg );
    	exit(1);
	}	
	
	/* Find the startup.jar */
	jarFile = findStartupJar();
	if(jarFile == NULL) {
		errorMsg = malloc( (_tcslen(startupMsg) + _tcslen(officialName) + 10) * sizeof(_TCHAR) );
        _stprintf( errorMsg, startupMsg, officialName );
        displayMessage( officialName, errorMsg );
        free( errorMsg );
    	exit( 1 );
	}

#ifdef _WIN32
	if( jniLaunching && (debug || needConsole) ) {
		createConsole();
	}
#endif

    /* If the showsplash option was given and we are using JNI */
    if (!noSplash && showSplashArg && jniLaunching)
    {
    	showSplash(showSplashArg);
    }
    
    /* not using JNI launching, need some shared data */
    if (!jniLaunching && createSharedData( &sharedID, MAX_SHARED_LENGTH )) {
        if (debug) {
   			if (debug) displayMessage( officialName, shareMsg );
        }
    }
    
	/* the startup jarFile goes on the classpath */
    if(jniLaunching) {
    	/* JNI launching, classpath is set using -Djava.class.path */
		cp = malloc((_tcslen(CLASSPATH_PREFIX) + _tcslen(jarFile) + 1) * sizeof(_TCHAR));
		cp = _tcscpy(cp, CLASSPATH_PREFIX);
		_tcscat(cp, jarFile);
    } else {
    	/* exec java, jar is specified with -jar */
    	cp = JAR;
    	cpValue = malloc((_tcslen(jarFile) + 1) * sizeof(_TCHAR));
    	_tcscpy(cpValue, jarFile);
    }
	
    /* Get the command to start the Java VM. */
    userVMarg = vmArgs;
    getVMCommand( argc, argv, &vmCommandArgs, &progCommandArgs );
	
    if (!jniLaunching) {
    	vmCommand = buildLaunchCommand(javaVM, vmCommandArgs, progCommandArgs);
    }
    
    /* While the Java VM should be restarted */
    while(running)
    {
		vmCommandMsg = formatVmCommandMsg( vmCommand, vmCommandArgs, progCommandArgs );
		if (debug) _tprintf( goVMMsg, vmCommandMsg );

		if(jniLaunching) {
			exitCode = startJavaVM(jniLib, vmCommandArgs, progCommandArgs);
		} else {
			exitCode = launchJavaVM(vmCommand);
		}
		
	    switch( exitCode ) {
	        case 0: /* normal exit */
	        	running = 0;
	            break;
	        case RESTART_LAST_EC:
	        	if (jniLaunching) {
		        	/* copy for relaunch, +1 to ensure NULL terminated */
		        	relaunchCommand = malloc((initialArgc + 1) * sizeof(_TCHAR*));
		        	memcpy(relaunchCommand, initialArgv, (initialArgc + 1) * sizeof(_TCHAR*));
		        	relaunchCommand[initialArgc] = 0;
		        	running = 0;
	        	}
	        	break;
	        	
	        case RESTART_NEW_EC:
	        	if(!jniLaunching) {
	        		if (exitData != NULL) free(exitData);
	        		getSharedData( sharedID, &exitData );
	        	}
	            if (exitData != 0) {
	            	if (vmCommand != NULL) free( vmCommand );
	                vmCommand = parseArgList( exitData );
	                if (jniLaunching) {
	                	relaunchCommand = getRelaunchCommand(vmCommand);
	                	running = 0;
	                }
	            } else {
	                if (debug) displayMessage( officialName, shareMsg );
	            }
	            break;
			default: {
				_TCHAR *title = _tcsdup(officialName);
	            running = 0;
	            errorMsg = NULL;
	            if (!jniLaunching) {
	            	if (exitData != NULL) free(exitData);
	        		getSharedData( sharedID, &exitData );
	        	}
	            if (exitData != 0) {
	            	errorMsg = exitData;
	                if (_tcslen( errorMsg ) == 0) {
	            	    free( errorMsg );
	            	    errorMsg = NULL;
	                } else {
	                    _TCHAR *str;
	                	if (_tcsncmp(errorMsg, _T_ECLIPSE("<title>"), _tcslen(_T_ECLIPSE("<title>"))) == 0) {
							str = _tcsstr(errorMsg, _T_ECLIPSE("</title>"));
							if (str != NULL) {
								free( title );
								str[0] = _T_ECLIPSE('\0');
								title = _tcsdup( errorMsg + _tcslen(_T_ECLIPSE("<title>")) );
								str = _tcsdup( str + _tcslen(_T_ECLIPSE("</title>")) );
								free( errorMsg );
								errorMsg = str;
							}
	                	}
	                }
	            } else {
	                if (debug) displayMessage( title, shareMsg );
	            }
	            if (errorMsg == NULL) {
	                errorMsg = malloc( (_tcslen(exitMsg) + _tcslen(vmCommandMsg) + 10) * sizeof(_TCHAR) );
	                _stprintf( errorMsg, exitMsg, exitCode, vmCommandMsg );
	            }
	            displayMessage( title, errorMsg );
	            free( title );
	            free( errorMsg );
	            break;
	        }
	    }
	    free( vmCommandMsg );
    }
    
    if(relaunchCommand != NULL)
    	restartLauncher(program, relaunchCommand);
    	
    /* Cleanup time. */
    free( vmCommandArgs );
    free( progCommandArgs );
    free( jarFile );
    free( programDir );
    free( program );
    free( officialName );
    if ( vmCommand != NULL )	 free( vmCommand );
    if ( cp != JAR )			 free( cp );
    if ( cpValue != NULL)		 free( cpValue );
    if ( vmSearchPath != NULL )  free( vmSearchPath );
    if ( exitData != NULL )		 free( exitData );

    return 0;
}

static _TCHAR** buildLaunchCommand( _TCHAR* program, _TCHAR** vmArgs, _TCHAR** progArgs ) {
	int nVM = -1, nProg = -1;
	_TCHAR** result;
	
	while(vmArgs[++nVM] != NULL) {}
	while(progArgs[++nProg] != NULL) {}
	
	result = malloc((nVM + nProg + 2) * sizeof(_TCHAR*));
	memset(result, 0, (nVM + nProg + 2) * sizeof(_TCHAR*));
	result[0] = program;
	memcpy(result + 1, vmArgs, nVM * sizeof(_TCHAR*));
	memcpy(result + 1 + nVM, progArgs, nProg * sizeof(_TCHAR*));
	return result;
}
/*
 * Parse arguments of the command.
 */
static void parseArgs( int* pArgc, _TCHAR* argv[] )
{
	Option* option;
    int     remArgs;
    int     index;
    int     i;

    /* Ensure the list of user argument is NULL terminated. */
    /*argv[ *pArgc ] = NULL;*/

	/* For each user defined argument (excluding the program) */
    for (index = 1; index < *pArgc; index++){
        remArgs = 0;

        /* Find the corresponding argument is a option supported by the launcher */
        option = NULL;
        for (i = 0; option == NULL && i < optionsSize; i++)
        {
        	if (_tcsicmp( argv[ index ], options[ i ].name ) == 0) {
        	    option = &options[ i ];
        	    break;
        	}
       	}

       	/* If the option is recognized by the launcher */
       	if (option != NULL)
       	{
       		/* If the option requires a value and there is one, extract the value. */
       		if (option->value != NULL && (index+1) < *pArgc)
       			*option->value = argv[ index+1 ];

       		/* If the option requires a flag to be set, set it. */
       		if (option->flag != NULL)
       			*option->flag = 1;
       		remArgs = option->remove;
       	}

		/* Remove any matched arguments from the list. */
        if (remArgs > 0)
        {
            for (i = (index + remArgs); i <= *pArgc; i++)
            {
                argv[ i - remArgs ] = argv[ i ];
            }
            index--;
            *pArgc -= remArgs;
        }
    }
}

/*
 * Parse the data into a list of arguments separated by \n.
 */
static _TCHAR** parseArgList( _TCHAR* data ) {
    int totalArgs = 0, dst = 0, length;
    _TCHAR *ch1, *ch2, **execArg;
    length = _tcslen( data );
    ch1 = ch2 = data;
    while ((ch2 = _tcschr( ch1, _T_ECLIPSE('\n') )) != NULL) {
    	totalArgs++;
    	ch1 = ch2 + 1;
    }
    if (ch1 != data + length) totalArgs++;
    execArg = malloc( (totalArgs + 1) * sizeof( _TCHAR* ) );
    ch1 = ch2 = data;
    while ((ch2 = _tcschr( ch1, _T_ECLIPSE('\n') )) != NULL) {
    	execArg[ dst++ ] = ch1;
    	ch2[ 0 ] = _T_ECLIPSE('\0');
    	ch1 = ch2 + 1;
    }
    if (ch1 != data + length) execArg[ dst++ ] = ch1;
    execArg[ dst++ ] = NULL;
    return execArg;
}

/*
 * Get the command and arguments to start the Java VM.
 *
 * Memory allocated by this function is assumed to be
 * deallocated when the program terminates.
 *
 * Some of the arguments returned by this function were
 * passed directly from the main( argv ) array so they
 * should not be deallocated.
 * 
 * Arguments are split into 2: vm arguments and program arguments
 */
static void getVMCommand( int argc, _TCHAR* argv[], _TCHAR **vmArgv[], _TCHAR **progArgv[] )
{
	_TCHAR** vmArg;
    int     nReqVMarg = 0;
    int     nVMarg = 0;
    int     totalVMArgs;
    int		totalProgArgs;
    int     src;
    int     dst;

	/* If the user specified "-vmargs", add them instead of the default VM args. */
	vmArg = (userVMarg != NULL) ? userVMarg : getArgVM( javaVM ); 
 	
 	/* Calculate the number of VM arguments. */
 	while (vmArg[ nVMarg ] != NULL)
 		nVMarg++;

 	/* Calculate the number of required VM arguments. */
 	while (reqVMarg[ nReqVMarg ] != NULL)
 		nReqVMarg++;

	/* VM argument list */
	totalVMArgs = nVMarg + nReqVMarg + 1;
	*vmArgv = malloc( totalVMArgs * sizeof(_TCHAR*) );
	
	dst = 0;
	for (src = 0; src < nVMarg; src++){
		/*if the user specified a classpath, skip it */
		if(_tcscmp(vmArg[src], cp) == 0){
			src++;
			continue;
		}
    	(*vmArgv)[ dst++ ] = vmArg[ src ];
	}
		
	/* For each required VM arg */
	for (src = 0; src < nReqVMarg; src++)
		if( *(reqVMarg[src]) != NULL)
			(*vmArgv)[ dst++ ] = *(reqVMarg[ src ]);
	
	(*vmArgv)[dst] = NULL;
	
	/* Program arguments */
    /*  OS <os> + WS <ws> + ARCH <arch> + LAUNCHER <launcher> + NAME <officialName> +
     *  + LIBRARY <library> + SHOWSPLASH <cmd> + EXITDATA <cmd> + STARTUP <jar> + argv[] + VM + <vm> + 
     * VMARGS + vmArg + requiredVMargs
     *  + NULL)
     */
    totalProgArgs  = 2 + 2 + 2 + 2 + 2 + 2 + 2 + 2 + 2 + argc + 2 + 1 + nVMarg + nReqVMarg + 1;
	*progArgv = malloc( totalProgArgs * sizeof( _TCHAR* ) );
    dst = 0;
    
    /* Append the required options. */
    (*progArgv)[ dst++ ] = OS;
    (*progArgv)[ dst++ ] = osArg;
    (*progArgv)[ dst++ ] = WS;
    (*progArgv)[ dst++ ] = wsArg;
    if (_tcslen(osArchArg) > 0) {
        (*progArgv)[ dst++ ] = OSARCH;
        (*progArgv)[ dst++ ] = osArchArg;
    }

	/* Append the launcher command */
	(*progArgv)[ dst++ ] = LAUNCHER;
	(*progArgv)[ dst++ ] = program;

	/* Append the name command */
	(*progArgv)[ dst++ ] = NAME;
	(*progArgv)[ dst++ ] = 	officialName;
	
	/* And the shared library */
	if (library != NULL) {
		(*progArgv)[ dst++ ] = LIBRARY;
		(*progArgv)[ dst++ ] = library;
	}
	
	/* the startup jar */
	(*progArgv)[ dst++ ] = STARTUP;
	(*progArgv)[ dst++ ] = jarFile;
	
	/* Append the show splash window command, if defined. */
    if (!noSplash)
    {
        (*progArgv)[ dst++ ] = SHOWSPLASH;
    }
    
	/* Append the exit data command. */
	if (sharedID) {
		(*progArgv)[ dst++ ] = EXITDATA;
		(*progArgv)[ dst++ ] = sharedID;
	}

	/* Append the remaining user defined arguments. */
    for (src = 1; src < argc; src++)
    {
        (*progArgv)[ dst++ ] = argv[ src ];
    }

    /* Append VM and VMARGS to be able to relaunch using exit data. */
	(*progArgv)[ dst++ ] = VM;
	if(jniLib != NULL)
		(*progArgv)[ dst++ ] = jniLib;
	else
		(*progArgv)[ dst++ ] = javaVM;
    (*progArgv)[ dst++ ] = VMARGS;
    
	for (src = 0; src < nVMarg; src++)
    	(*progArgv)[ dst++ ] = vmArg[ src ];
	
    /* For each required VM arg */
    for (src = 0; src < nReqVMarg; src++)
    	if (*(reqVMarg[src]) != NULL)
    		(*progArgv)[ dst++ ] = *(reqVMarg[ src ]);

    (*progArgv)[ dst++ ] = NULL;

 }

 /* Format the JVM start command for error messages
  *
  * This method formats a string with the JVM start command (and all arguments)
  * that can be used in displaying error messages. The string returned from this
  * method is probably not NLS compliant and must be deallocated by the caller.
  * 
  * The arguments in the message are either args (if not null) or the combination 
  * of vmArgs + progArgs
  */
static _TCHAR*  formatVmCommandMsg( _TCHAR* args[], _TCHAR* vmArgs[], _TCHAR* progArgs[] )
{
	int   index;
    int   length = 0;
    _TCHAR** list;
    _TCHAR* ch;
    _TCHAR* message;

	/* Determine the length of the message buffer. */
	if(args != NULL) list = args;
	else             list = vmArgs;
	while(list != NULL) {
		for (index = 0; list[index] != NULL; index++)
		{
			length += _tcslen(list[index]) + 1;
		}
		if(list == vmArgs) list = progArgs;
		else 			   list = NULL;
	}
	message = malloc( (length + 5) * sizeof(_TCHAR) );
	
	/* Format the message such that options (args starting with '-') begin
	   on a new line. Otherwise, the Motif MessageBox does not automatically wrap
	   the messages and the message window can extend beyond both sides of the display. */
	ch = message;
	if(args != NULL) list = args;
	else             list = vmArgs;
	while(list != NULL) {
		for (index = 0; list[index] != NULL; index++)
		{
			if (list[index][0] == _T_ECLIPSE('-') && *(ch-1) == _T_ECLIPSE(' '))
				*(ch-1) = _T_ECLIPSE('\n');
			_tcscpy( ch, list[index] );
			ch += _tcslen( list[index] );
			*ch++ = _T_ECLIPSE(' ');
		}
		if(list == vmArgs) list = progArgs;
		else 			   list = NULL;
	}
	*ch = _T_ECLIPSE('\0');

	return message;
}

/*
 * Determine the default official application name
 *
 * This function provides the default application name that appears in a variety of
 * places such as: title of message dialog, title of splash screen window
 * that shows up in Windows task bar.
 * It is computed from the name of the launcher executable and
 * by capitalizing the first letter. e.g. "c:/ide/eclipse.exe" provides
 * a default name of "Eclipse".
 */
static _TCHAR* getDefaultOfficialName()
{
	_TCHAR *ch = NULL;
	
	/* Skip the directory part */
	ch = _tcsrchr( program, dirSeparator );
	if (ch == NULL) ch = program;
	else ch++;
	
	ch = _tcsdup( ch );
#ifdef _WIN32
	{
		/* Search for the extension .exe and cut it */
		_TCHAR *extension = _tcsrchr(ch, _T_ECLIPSE('.'));
		if (extension != NULL) 
		{
			*extension = _T_ECLIPSE('\0');
		}
	}
#endif
	/* Upper case the first character */
#ifndef LINUX
	{
		*ch = _totupper(*ch);
	}
#else
	{
		if (*ch >= 'a' && *ch <= 'z')
		{
			*ch -= 32;
		}
	}
#endif
	return ch;
}

/* Determine the Program Directory
 *
 * This function takes the directory where program executable resides and
 * determines the installation directory.
 */
_TCHAR* getProgramDir( )
{
	_TCHAR*  ch;
	_TCHAR*  programDir;

    programDir = malloc( (_tcslen( program ) + 1) * sizeof(_TCHAR) );
    _tcscpy( programDir, program );
    ch = _tcsrchr( programDir, dirSeparator );
	if (ch != NULL)
    {
    	*(ch+1) = _T_ECLIPSE('\0');
   		return programDir;
    }

    free( programDir );
    return NULL;
}

static _TCHAR* findStartupJar(){
	_TCHAR * file;
	_TCHAR * pluginsPath;
	struct _stat stats;
	int pathLength, progLength;
	
	if( startupArg != NULL ) {
		/* startup jar was specified on the command line */
		
		/* Construct the absolute name of the startup jar */
		file = malloc( (_tcslen( programDir ) + _tcslen( startupArg ) + 1) * sizeof( _TCHAR ) );
		file = _tcscpy( file, programDir );
	  	file = _tcscat( file, startupArg );
	
		/* If the file does not exist, treat the argument as an absolute path */
		if (_tstat( file, &stats ) != 0)
		{
			free( file );
			file = malloc( (_tcslen( startupArg ) + 1) * sizeof( _TCHAR ) );
			file = _tcscpy( file, startupArg );
			
			/* still doesn't exit? */
			if (_tstat( file, &stats ) != 0) {
				file = NULL;
			}
		}
		/* TODO What should the policy here be, if we didn't find what they
		 * specified?  (Its harder to specify equinox.startup on the mac.) */
		if(file != NULL)
			return file;
	}

	progLength = pathLength = _tcslen(programDir);
#ifdef MACOSX
	pathLength += 9;
#endif
	pluginsPath = malloc( (pathLength + 1 + 7 + 1) * sizeof(_TCHAR));
	_tcscpy(pluginsPath, programDir);
	if(pluginsPath[progLength - 1] != dirSeparator) {
		pluginsPath[progLength] = dirSeparator;
		pluginsPath[progLength + 1] = 0;
	}
#ifdef MACOSX
	_tcscat(pluginsPath, _T_ECLIPSE("../../../"));
#endif
	_tcscat(pluginsPath, _T_ECLIPSE("plugins"));
	
	/* equinox startup jar? */	
	file = findFile(pluginsPath, DEFAULT_EQUINOX_STARTUP);
	if(file != NULL)
		return file;
		
	return _tcsdup(DEFAULT_STARTUP);
}

/* 
 * Return the portion of the vmCommand that should be used for relaunching
 * 
 * The memory allocated for the command array must be freed
 */
static _TCHAR ** getRelaunchCommand( _TCHAR **vmCommand  )
{
	int i = -1, req = 0, begin = -1;
	int idx = 0;
	_TCHAR ** relaunch;
	
	if (vmCommand == NULL) return NULL;
	while(vmCommand[++i] != NULL){
		if ( begin == -1 && _tcsicmp( vmCommand[i], *reqVMarg[req] ) == 0) {
			if(reqVMarg[++req] == NULL || *reqVMarg[req] == NULL){
				begin = i + 1;
			}
		}
	}
	
	relaunch = malloc((1 + i + 1) * sizeof(_TCHAR *));
	relaunch[idx++] = program;
	if(begin == -1) {
		begin = 1;
	}
	for (i = begin; vmCommand[i] != NULL; i++){
		if (_tcsicmp(vmCommand[i], SHOWSPLASH) == 0) {
			/* remove if the next argument is not the bitmap to show */
			if(vmCommand[i + 1] != NULL && vmCommand[i + 1][0] == _T_ECLIPSE('-')) {
				i++;
				continue;
			}
		} else if(_tcsncmp(vmCommand[i], CLASSPATH_PREFIX, _tcslen(CLASSPATH_PREFIX)) == 0) {
			/* skip -Djava.class.path=... */
			continue;
		}
		relaunch[idx++] = vmCommand[i];
	}
	relaunch[idx] = NULL;
	return relaunch;
}

#ifdef _WIN32
static void createConsole() {
	long stdHandle;
	int conHandle;
	FILE *fp;
	
	AllocConsole();
	
	/* redirect stdout */
	stdHandle = (long)GetStdHandle(STD_OUTPUT_HANDLE);
	conHandle = _open_osfhandle(stdHandle, _O_TEXT);
	fp = _fdopen(conHandle, "w");
	*stdout = *fp;
	
	/* redirect stdin */
	stdHandle = (long)GetStdHandle(STD_INPUT_HANDLE);
	conHandle = _open_osfhandle(stdHandle, _O_TEXT);
	fp = _fdopen(conHandle, "r");
	*stdin = *fp;
	
	/* stderr */
	stdHandle = (long)GetStdHandle(STD_ERROR_HANDLE);
	conHandle = _open_osfhandle(stdHandle, _O_TEXT);
	fp = _fdopen(conHandle, "r");
	*stderr = *fp;	
}
#endif
