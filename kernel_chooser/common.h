/* taken from http://www.termsys.demon.co.uk/vtansi.htm */
#define VT_RESET			0
#define VT_BRIGHT			1
#define VT_DIM				2
#define VT_UNDERSCORE	4
#define VT_BLINK			5
#define VT_REVERSE		7
#define VT_HIDDEN			8

#define FG_BLACK		30
#define FG_RED			31
#define FG_GREEN		32
#define FG_YELLOW		33
#define FG_BLUE			34
#define FG_MAGENTA	35
#define FG_CYAN			36
#define FG_WHITE		37

#define BG_BLACK		40
#define BG_RED			41
#define BG_GREEN		42
#define BG_YELLOW		43
#define BG_BLUE			44
#define BG_MAGENTA	45
#define BG_CYAN			46
#define BG_WHITE		47

// colors
#define COLOR_DEFAULT 0
#define COLOR_LOG_DEBUG 1
#define COLOR_LOG_WARN 2
#define COLOR_LOG_ERROR 3
#define COLOR_MENU_BORDER 4
#define COLOR_MENU_TEXT 5
#define COLOR_MENU_TITLE 6
#define COLOR_POPUP 7

// print helpers
#define FATAL(x,args...)	{nc_error(x,##args);fatal_error=1;}
#define ERROR(x,args...) 	nc_push_message(COLOR_LOG_ERROR,"[ERROR]",x,##args)
#define WARN(x,args...)		nc_push_message(COLOR_LOG_WARN,"[WARN ]",x,##args)
#ifdef DEVELOPMENT
#define INFO(x,args...)		nc_push_message(COLOR_DEFAULT,"[INFO ]",x,##args)
#define DEBUG(x,args...) 	nc_push_message(COLOR_LOG_DEBUG,"[DEBUG]",x,##args)
#define SHELL // allow the user to drop into a shell provided by busybox
#else
#define INFO(x,args...)
#define DEBUG(x,args...)
#endif

#define MAX_LINE 255
#define COMMAND_LINE_SIZE    1024
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

extern int fatal_error;

/* from nGUI.c */
int nc_push_message(int, char *, char *,...);
void nc_error(char *,...);
void nc_status(char *);

/* from fbGUI.c */
void fb_init(void);
void fb_destroy(void);
void fb_background(void);
void fb_crefresh(int,int,int,int);