#include <curses.h>
#include <menu.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <wait.h>

#include "common3.h"
#include "menu3.h"
#include "nGUI2.h"

int	menu_sizex, // size fo the menu_window ( getmaxyx does not work )
		menu_sizey,
		entries_count; // count of created entries ( useful for error handling )
ITEM **items; // ncurses menu items
MENU *menu; // ncurses menu
WINDOW 	*menu_window, // ncurses menu window
				*messages_win; // ncurses messages window
char 	**local_entries; // our padded copy of the items names

struct _default_entries
{
	const char *name;
	const int num;
};

const struct _default_entries default_entries[] =
{
	{
		.name = "reboot",
		.num = MENU_REBOOT
	},
	{
		.name = "shutdown",
		.num = MENU_HALT
	},
	{
		.name = "recovery",
		.num = MENU_RECOVERY
	}
#ifdef SHELL
	,
	{
		.name = "emergency shell",
		.num = MENU_SHELL
	}
#endif
};

void print_in_middle(WINDOW *win, int starty, int startx, int width, char *string)
{
	int length, x, y;
	float temp;

	if(win == NULL)
		win = stdscr;
	getyx(win, y, x);
	if(startx != 0)
		x = startx;
	if(starty != 0)
		y = starty;
	if(width == 0)
		width = 80;

	length = strlen(string);
	temp = (width - length)/ 2;
	x = startx + (int)temp;
	mvwprintw(win, y, x, "%s", string);
	refresh();
}

/** copy src string to *dst, padding equally from right and left
 * @dest: destination char *
 * @src: source char *
 * @sizex: size of the padded string
*/
int copy_with_padd(char **dest, int sizex, char *src)
{
	int len;

	len = strlen(src);
	*dest = malloc((sizex+1)*sizeof(char));
	if(!*dest)
	{
		FATAL("malloc - %s\n",strerror(errno));
		return -1;
	}
	memset(*dest,' ',sizex);
	strncpy((*dest + ((sizex - len)/2)),src,len);
	*((*dest)+sizex)='\0';
	return 0;
}

void fill_box(int x, int y, int w, int h) {
	int i,j;
	for(j = y; j <= y + h; j++)
		for(i = x; i <= x + w; i++)
			mvaddch(j, i, ' ');
	refresh();
}

void create_box(int x, int y, int w, int h)
{
	fill_box(x,y,w,h);
	mvaddch(y, x, ACS_ULCORNER);
	mvaddch(y, x + w, ACS_URCORNER);
	mvaddch(y + h, x, ACS_LLCORNER);
	mvaddch(y + h, x + w, ACS_LRCORNER);
	mvhline(y, x + 1, ACS_HLINE, w - 1);
	mvhline(y + h, x + 1, ACS_HLINE, w - 1);
	mvvline(y + 1, x, ACS_VLINE, h - 1);
	mvvline(y + 1, x + w, ACS_VLINE, h - 1);
	refresh();
}

int nc_init(void)
{
	int sizex,sizey;

	/* Initialize variables to free in case of errors */
	menu = NULL;
	messages_win = menu_window = NULL;
	items = NULL;
	local_entries = NULL;
	entries_count = 0;

	/* Initialize curses */
	initscr(); // TODO: error checking
	start_color();
	cbreak();
	noecho();
	curs_set(0);
	keypad(stdscr, TRUE);

	// colors
	init_pair(COLOR_LOG_DEBUG, COLOR_CYAN, COLOR_BLACK);
	init_pair(COLOR_LOG_WARN, COLOR_YELLOW, COLOR_BLACK);
	init_pair(COLOR_LOG_ERROR, COLOR_RED, COLOR_BLACK);
	init_pair(COLOR_MENU_BORDER, COLOR_BLACK, COLOR_BLUE);
	init_pair(COLOR_MENU_TEXT, COLOR_WHITE, COLOR_BLUE);
	init_pair(COLOR_MENU_TITLE, COLOR_CYAN, COLOR_BLUE);

	/* Create messages window */
	sizey = (LINES * MSG_HEIGHT_PERC)/100;
	sizex = (COLS * MSG_WIDTH_PERC)/100;
	messages_win = newwin(sizey-1,sizex,(LINES-sizey)+2,0);
	scrollok(messages_win,TRUE);
	mvhline(LINES-sizey+1,0,ACS_HLINE,sizex);

	refresh();
	return 0;
}

void nc_destroy_menu(void)
{
	unpost_menu(menu);
	free_menu(menu);
	delwin(menu_window);
	if(items[entries_count]) // items are NULL-terminated
		free_item(items[entries_count]);
	while(entries_count--)
	{
		free(local_entries[entries_count]);
		free_item(items[entries_count]);
	}
	free(items);
	free(local_entries);
}

void nc_destroy(void)
{
	/*delwin(messages_win);
	 * WARNING: call this function here causes a kernel panic.
	 * NOTE:DEBUG("messages_win=%p",messages_win) return a valid pointer...
	 * we have to find why this happens
	 * NOTE: btw, kernel saying that all memory has freed...strange behaviour.
	 */
	clear();
	endwin();
}

void draw_menu_border(void)
{
	int startx,starty,len;
	/* Print a border around the main window and print a title */
	getbegyx(menu_window,starty,startx);
	startx -= 1;
	starty -= 3;
	len = strlen(PROMPT);
	attron(COLOR_PAIR(COLOR_MENU_BORDER));
	create_box(startx,starty,menu_sizex+1,menu_sizey+3);
	mvhline(starty+2, startx+1, ACS_HLINE, menu_sizex);
	mvaddch(starty+2,startx,ACS_LTEE);
	mvaddch(starty+2,startx+menu_sizex+1,ACS_RTEE);
	attron(COLOR_PAIR(COLOR_MENU_TITLE));
	mvprintw(starty+1,startx + ((menu_sizex - len)/2),"%s",PROMPT);
	attroff(COLOR_PAIR(COLOR_MENU_TITLE));
	wbkgd(menu_window,COLOR_PAIR(COLOR_MENU_TEXT));
	wrefresh(menu_window);
	refresh();
}

int nc_compute_menu(menu_entry *list)
{
	int n_choices, default_count;
	menu_entry *current;

	/* Compute menu size */
	menu_sizey = (LINES * MENU_HEIGHT_PERC)/100;
	menu_sizex = (COLS * MENU_WIDTH_PERC)/100;

	menu_sizex-=2; // borders
	menu_sizey-=5; // borders + head

	/* Create items */
	// count entries
	for(n_choices=0,current=list;current;current=current->next,n_choices++);
	// count the default ones
	default_count = ARRAY_SIZE(default_entries);
	// sum
	n_choices+= default_count;
	items = (ITEM **)malloc((n_choices+1)*sizeof(ITEM *));
	local_entries = malloc((n_choices+1)*sizeof(char *));
	if(!items || !local_entries)
	{
		FATAL("malloc - %s\n",strerror(errno));
		goto error;
	}
	// create default entries
	for(entries_count=0;entries_count<default_count;entries_count++)
		if(copy_with_padd(local_entries+entries_count,menu_sizex,(char*)default_entries[entries_count].name))
			goto error;
		else
		{
			// HACK: store src pointer in item description
			items[entries_count] = new_item(local_entries[entries_count], default_entries[entries_count].name);
			if(!items[entries_count])
			{
				free(local_entries[entries_count]);
				FATAL("new_item - %s\n",strerror(errno));
				goto error;
			}
		}
	for(current=list;current;current=current->next,entries_count++)
		if(copy_with_padd(local_entries+entries_count,menu_sizex,current->name))
			goto error;
		else
		{
			// HACK: store src pointer in item description
			items[entries_count] = new_item(local_entries[entries_count], current->name);
			if(!items[entries_count])
			{
				free(local_entries[entries_count]);
				FATAL("new_item - %s\n",strerror(errno));
				goto error;
			}
		}
	items[entries_count] = new_item(NULL,NULL);
	/* Create menu */
	menu = new_menu((ITEM **)items);
	if(!menu)
	{
		FATAL("new_menu - %s\n",strerror(errno));
		goto error;
	}
	/* Set menu option not to show the description */
	menu_opts_off(menu, O_SHOWDESC);

	/* Change item color */
	set_menu_back(menu, COLOR_PAIR(COLOR_MENU_TEXT));

	/* Create the window to be associated with the menu */
	menu_window = newwin( menu_sizey, menu_sizex, 5, (COLS-menu_sizex)/2);
	if(!menu_window)
	{
		FATAL("newwin - %s\n",strerror(errno));
		goto error;
	}

	keypad(menu_window, TRUE);
	if(wattron(menu_window,COLOR_PAIR(COLOR_MENU_TEXT))==ERR)
		ERROR("wattron - %s\n",strerror(errno));
	/* Set main window and sub window */
	if(set_menu_win(menu, menu_window)==ERR)
	{
		FATAL("set_menu_win - %s\n",strerror(errno));
		goto error;
	}
	// set menu size
	if(set_menu_format(menu, menu_sizey,1) != E_OK)
		ERROR("set_menu_format - %s\n",strerror(errno));

	/* Set menu mark  */
	if(set_menu_mark(menu, NULL)!= E_OK)
		ERROR("set_menu_mark - %s\n",strerror(errno));

	draw_menu_border();
	return 0;
	error:
	nc_destroy_menu();
	return -1;
}

int nc_get_user_choice(menu_entry *list)
{
	int c,default_count;
	default_count = ARRAY_SIZE(default_entries);

	/* Post the menu */
	if(post_menu(menu)!=E_OK)
		goto error;
	wrefresh(menu_window);
	wrefresh(messages_win);
	refresh();

	while((c = wgetch(menu_window)) != 10)
	{
		DEBUG("key %i (%c)\n", c, c);
		switch(c)
		{
			case 278:
			case KEY_DOWN:
				menu_driver(menu, REQ_DOWN_ITEM);
				break;
			//case KEY_VOLUMEUP: TODO
			case KEY_UP:
				menu_driver(menu, REQ_UP_ITEM);
				break;
			case KEY_NPAGE:
				menu_driver(menu, REQ_SCR_DPAGE);
				break;
			case KEY_PPAGE:
				menu_driver(menu, REQ_SCR_UPAGE);
				break;
		}
		wrefresh(menu_window);
	}

	// HACK: use c as temporary variable
	c = item_index(current_item(menu));
	if (c < default_count)
		c= default_entries[c].num;
	else if (c == default_count)
		c = -5;
	else
		c = c - default_count + 1;
	return c;

	error:
	return MENU_FATAL_ERROR;
}

void nc_push_message(int i, char *prefix, char *fmt,...)
{
	va_list ap;

	if(!messages_win)
		return;

	va_start(ap,fmt);
	wattron(messages_win, COLOR_PAIR(i));
	wprintw(messages_win,prefix);
	wattroff(messages_win, COLOR_PAIR(i));
	vwprintw(messages_win,fmt,ap);
	wrefresh(messages_win);
	va_end(ap);
}

void nc_wait_enter(void)
{
	while(getch() != 10);
}

void nc_print_header(void)
{
	const char *strings[] = HEADER;
	int y,x;

	for(y=0;strings[y];y++)
	{
		x = (COLS-strlen(strings[y]))/2;
		mvprintw(y,x,"%s",strings[y]);
	}
}

/** wait for a keypress while coutdown.
 * if user press something return 0.
 * -1 otherwise
 */
int nc_wait_for_keypress(void)
{
	int x,y,timeout;

	y = (LINES/2)-1;
	x = (COLS - snprintf(NULL,0,WAIT_MESSAGE,0))/2;

	timeout(1000); // wait one second between keypress checks

	for (timeout=TIMEOUT_BOOT; timeout>0; timeout--) {
		mvprintw(y,x,WAIT_MESSAGE, timeout);
		refresh();
		if (getch() != ERR) {
			mvprintw(y,x,"%*s",COLS-x-1," ");
			return 0;
		}
	}
	return -1;
}