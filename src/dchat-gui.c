/*
 *  Copyright (c) 2014 Christoph Mahrl
 *
 *  This file is part of DChat.
 *
 *  DChat is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  DChat is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with DChat.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <stdarg.h>

#include "dchat-gui.h"


//*********************************
//        GLOBAL VARIABLES
//*********************************
static pthread_mutex_t _win_lock; //!< mutex for signaling a window lock
static DWINDOW_T* _win_msg;       //!< main chat window containing messages
static DWINDOW_T* _win_usr;       //!< window containing active contacts
static DWINDOW_T* _win_inp;       //!< window containing current user input
static DWINDOW_T*
_win_cur;       //!< pointer that holds the current selected window
static ipc
_ipc;           //!< holds file descriptor information to communicate with another process via ipc


int
main()
{
    pthread_t th_ipc;

    if (pthread_mutex_init(&_win_lock, NULL) != 0)
    {
        exit(1);
    }

    signal(SIGWINCH, resize_win); // check for resize events
    signal(SIGPIPE,
           SIG_IGN);     // prevent sigpipes if write() on broken pipes is used
    // start graphical user interface and wait for input
    start_gui();
    pthread_create(&th_ipc, NULL, (void*) th_ipc_connector, NULL);
    read_input();
    stop_gui();
    pthread_mutex_destroy(&_win_lock);
    return 0;
}


/**
 * Initialize colors available within this chat.
 * This function initializes all available colors
 * that are supported within this GUI.
 * @see enum colors
 */
void
init_colors(void)
{
    // check terminal support for colors
    if (has_colors())
    {
        start_color();
        use_default_colors();
        init_pair(COLOR_WINDOW_MESSAGE,   COLOR_WHITE,   COLOR_BLACK);
        init_pair(COLOR_WINDOW_USER,      COLOR_WHITE,   COLOR_BLACK);
        init_pair(COLOR_WINDOW_INPUT,     COLOR_WHITE,   COLOR_BLACK);
        init_pair(COLOR_DATE_TIME,        COLOR_CYAN,    COLOR_BLACK);
        init_pair(COLOR_SEPARATOR,        COLOR_CYAN,    COLOR_BLACK);
        init_pair(COLOR_NICKNAME_SELF,    COLOR_YELLOW,  COLOR_BLACK);
        init_pair(COLOR_NICKNAME_CONTACT, COLOR_GREEN,   COLOR_BLACK);
        init_pair(COLOR_NICKNAME_SYSTEM,  COLOR_RED,     COLOR_BLACK);
        init_pair(COLOR_MESSAGE_SELF,     COLOR_WHITE,   COLOR_BLACK);
        init_pair(COLOR_MESSAGE_CONTACT,  COLOR_WHITE,   COLOR_BLACK);
        init_pair(COLOR_MESSAGE_SYSTEM,   COLOR_RED,     COLOR_BLACK);
        init_pair(COLOR_STDSCR,           -1,            COLOR_YELLOW);
    }
}


/**
 * Initialize windows available within this chat.
 * This function initializes all available windows
 * that are used within this GUI.
 * @see enum windows of dchat-gui.h
 */
void
init_wins()
{
    _win_msg = malloc(sizeof(*_win_msg));
    memset(_win_msg, 0, sizeof(*_win_msg));
    _win_usr = malloc(sizeof(*_win_usr));
    memset(_win_usr, 0, sizeof(*_win_usr));
    _win_inp = malloc(sizeof(*_win_inp));
    memset(_win_inp, 0, sizeof(*_win_inp));
    _win_cur = malloc(sizeof(*_win_cur));
}


/**
 * Initialize graphical user interface.
 * This function initializes the chat GUI and renders
 * it on the screen.
 * @param ratio_height relative height of message window to available rows
 * @param ratio_width  relative width of message window to available columns
 */
void
init_gui(float ratio_height, float ratio_width)
{
    // dimension of virtual base window including padding
    int o_base    = 2;          // base offset
    int h_base    = LINES - o_base;
    int w_base    = COLS - 1;   // -1: remove scrollbar
    int p_base    = 2;          // col/row padding
    // dimension of message field within virtual base window
    _win_msg->h       = h_base * ratio_height - p_base;
    _win_msg->h_total = _win_msg->h * 16; // make virtual window 16 times the size
    _win_msg->w       = w_base * ratio_width - p_base;
    _win_msg->w_total = _win_msg->w;
    // dimension of user field within virtual base window
    _win_usr->h       = _win_msg->h;
    _win_usr->h_total = _win_usr->h * 16; // make virtual window 16 times the size
    _win_usr->w       = w_base - _win_msg->w - p_base;
    _win_usr->w_total = _win_usr->w;
    // dimension of input field within virtual base window
    _win_inp->h       = 1;
    _win_inp->h_total = _win_inp->h;
    _win_inp->w       = _win_msg->w;
    _win_inp->w_total = _win_inp->w;
    // position base
    int x_base  = p_base / 2;
    int y_base  = o_base;
    // position message field
    _win_msg->x = x_base;
    _win_msg->y = y_base;
    // position user field
    _win_usr->x = _win_msg->x + _win_msg->w + p_base / 2;
    _win_usr->y = _win_msg->y;
    // position input field
    _win_inp->x = x_base;
    _win_inp->y = _win_msg->y + _win_msg->h + p_base;
    // set standard background
    bkgd(COLOR_PAIR(COLOR_STDSCR));
    refresh();
    // draw windows
    _win_msg->win = create_padwin(_win_msg->h_total, _win_msg->w_total, _win_msg->h,
                                  _win_msg->w, _win_msg->y, _win_msg->x, COLOR_WINDOW_MESSAGE);
    _win_usr->win = create_padwin(_win_usr->h_total, _win_usr->w_total, _win_usr->h,
                                  _win_usr->w, _win_usr->y, _win_usr->x, COLOR_WINDOW_USER);
    _win_inp->win = create_win(_win_inp->h, _win_inp->w, _win_inp->y, _win_inp->x,
                               COLOR_WINDOW_INPUT);
    _win_cur = _win_inp; // focused window
}


/**
 * Initializes ncurses and starts the GUI.
 * This functions initializes ncurses with the required
 * prerequisites and starts/renders the chat GUI afterwards.
 */
void
start_gui()
{
    initscr();            // initialize ncurses
    cbreak();             // interprete control characters (CTRL-C, ...)
    noecho();             // dont print escape codes
    keypad(stdscr, TRUE); // make use of special key (arrow, ...)
    init_colors();        // initialize available colors
    init_wins();          // initialize all available windows
    init_gui(0.95, 0.75); // calculate size / position and render gui
}


/**
 * Frees resources used by the available chat windows.
 */
void
free_wins()
{
    delwin(_win_msg->win);
    delwin(_win_usr->win);
    delwin(_win_inp->win);
    free(_win_msg);
    free(_win_usr);
    free(_win_inp);
}


/**
 * Frees all resources used by ncurses and the GUI.
 * This function releases all resources and stops
 * the GUI.
 */
void
stop_gui()
{
    // release resources and refresh screen
    free_wins();
    endwin();
    refresh();
    erase();
}


/**
 * Creates a ncurses pad window.
 * This function creates a pad window and renders it on the screen.
 * @see newpad() of ncurses.h
 * @param max_height maximum height of pad window
 * @param max_width  maximum width of pad window
 * @param height     height of pad window shown on the screen
 * @param width      width of pad window shown on the screen
 * @param starty     y position of pad window shown on the screen
 * @param startx     x position of pad window shown on the screen
 * @param col_bkgd   background color of pad window shown on the screen
 * @return Pointer to ncurses window structure
 */
WINDOW*
create_padwin(int max_height, int max_width, int height, int width, int starty,
              int startx, const int col_bkgd)
{
    WINDOW* local_pad;
    local_pad = newpad(max_height, max_width);

    // check if terminal support colors
    if (has_colors())
    {
        wbkgd(local_pad, COLOR_PAIR(col_bkgd));
    }

    // render pad win
    prefresh(local_pad, 0, 0, starty, startx, starty + height - 1,
             startx + width - 1);
    return local_pad;
}


/**
 * Creates a ncurses window.
 * This function creates a window and renders it on the screen.
 * @see newwin() of ncurses.h
 * @param height     height of window shown on the screen
 * @param width      width of window shown on the screen
 * @param starty     y position of window shown on the screen
 * @param startx     x position of window shown on the screen
 * @param col_bkgd   background color of window shown on the screen
 * @return Pointer to ncurses window structure
 */
WINDOW*
create_win(int height, int width, int starty, int startx, const int col_bkgd)
{
    WINDOW* local_win;
    int attr = 0;
    local_win = newwin(height, width, starty, startx);

    // check if terminal support colors
    if (has_colors())
    {
        wbkgd(local_win, COLOR_PAIR(col_bkgd));
    }

    // render win
    wrefresh(local_win);
    return local_win;
}


/**
 * Refreshes the contents of the given chat window.
 * @param win pointer to a chat window structure containing the actual window.
 * @see DWINDOW_T in dchat-gui.h
 */
void
refresh_padwin(DWINDOW_T* win)
{
    // autoscroll pad win if cursor is increased
    if (win->y_count >= win->h)
    {
        prefresh(win->win, win->y_cursor, 0, win->y, win->x,
                 win->y + win->h - 1, win->x + win->w - 1);
    }
    else
    {
        prefresh(win->win, 0, 0, win->y, win->x, win->y + win->h - 1,
                 win->x + win->w - 1);
    }
}


/**
 * Refreshes the contents of the current chat window.
 */
void
refresh_current()
{
    int cur_win = current_winnr();

    if (cur_win == WINDOW_MSG || cur_win == WINDOW_USR)
    {
        refresh_padwin(_win_cur);
    }
    else
    {
        wrefresh(_win_cur->win);
    }
}


/**
 * Refreshes the contents of all available chat windows.
 */
void
refresh_screen()
{
    refresh_padwin(_win_msg);
    refresh_padwin(_win_usr);
    wrefresh(_win_inp->win);
    move_win(_win_cur, _win_cur->y_cursor, _win_cur->x_cursor);
    refresh_current();
}


/**
 * Returns the number of the current active chat window.
 * @return value of enum colors
 * @see enum colors in dchat-gui.h
 */
int
current_winnr()
{
    if (_win_cur == _win_msg)
    {
        return WINDOW_MSG;
    }

    if (_win_cur == _win_usr)
    {
        return WINDOW_USR;
    }

    if (_win_cur == _win_inp)
    {
        return WINDOW_INP;
    }

    return -1;
}


/**
 * Returns the currently active chat window.
 * @return Pointer to the currently active chat window.
 * @see enum windows in dchat-gui.h
 */
DWINDOW_T*
get_win(int winnr)
{
    if (winnr == WINDOW_MSG)
    {
        return _win_msg;
    }

    if (winnr == WINDOW_USR)
    {
        return _win_usr;
    }

    if (winnr == WINDOW_INP)
    {
        return _win_inp;
    }

    return NULL;
}


/**
 * Signal handler that handles resize events.
 * This signal handler functions restart and
 * rerenders the GUI if a terminal resize event has
 * been detected.
 * @param signum Signal number
 */
void
resize_win(int signum)
{
    stop_gui();
    start_gui();
}


/**
 * Scroll a chat window up/downwards.
 * This functions scrolls a chat window n rows
 * up or downwards respectively. If n is positive
 * the window will be scrolled upwards otherwise it
 * will be scrolled down.
 * @param n number of rows to scroll up/down
 */
void
scroll_win(DWINDOW_T* win, int n)
{
    // invert
    n *= -1;

    // up
    if (n < 0)
    {
        if (win->y_cursor > 0)
        {
            set_row_cursor(win, win->y_cursor + n);
        }
    }
    // down
    else if (n > 0)
    {
        set_row_cursor(win, win->y_cursor + n);
    }

    // move to current cursor position
    move_win(win, win->y_cursor, win->x_cursor);
    refresh_screen();
}


/**
 * Moves to a certain position within a chat window.
 * This functions moves to a position within a chat window
 * and sets this window as current window.
 * @param win Pointer to chat window structure
 * @param y y position
 * @param x x position
 * @see DWINDOW_T of dchat-gui.h
 */
void
move_win(DWINDOW_T* win, int y, int x)
{
    wmove(win->win, y, x);
    _win_cur = win;
}


/**
 * Sets the current active row position of a chat window.
 * @param win Pointer to chat window structure
 * @param y New row position
 */
void
set_row_position(DWINDOW_T* win, int y)
{
    if (y < 0)
    {
        y = 0;
    }
    else if (y > win->h_total)
    {
        y = win->h_total;
    }

    win->y_count = y;
    set_row_cursor(win, y - win->h);
}


/**
 * Sets the row cursor position of a chat window.
 * @param win Pointer to chat window structure
 * @param y New row cursor position
 */
void
set_row_cursor(DWINDOW_T* win, int y)
{
    if (y < 0)
    {
        y = 0;
    }
    else if (y > win->y_count - win->h)
    {
        y = win->y_count - win->h;
    }

    win->y_cursor = y;
}


/**
 * Increases/Decreases the current active column position of a chat window.
 * @param win Pointer to chat window structure
 * @param n Number of columns to add/subtract
 */
void
col_position(DWINDOW_T* win, int n)
{
    int x_count_before = win->x_count;
    win->x_count += n;

    if (win->x_count > win->w -1)
    {
        win->x_count = win->w - 1;
    }
    else if (win->x_count < 0)
    {
        win->x_count = 0;
    }

    // also set column cursor
    col_cursor(win, win->x_count - x_count_before);
}


/**
 * Increases/Decreases the column cursor position of a chat window.
 * @param win Pointer to chat window structure
 * @param n Number of columns to add/subtract
 */
void
col_cursor(DWINDOW_T* win, int n)
{
    win->x_cursor += n;

    if (win->x_cursor > win->x_count)
    {
        win->x_cursor = win->x_count;
    }
    else if (win->x_cursor > win->w -1)
    {
        win->x_cursor = win->w - 1;
    }
    else if (win->x_cursor < 0)
    {
        win->x_cursor = 0;
    }
}


/**
 * Reads keyboard hits until function key F1 has been typed.
 */
void
read_input()
{
    int ch;

    while ((ch = getch()) != KEY_F(1))
    {
        pthread_mutex_lock(&_win_lock);
        handle_keyboard_hit(ch);
        pthread_mutex_unlock(&_win_lock);
    }
}


/**
 * Handles a keyboard key hit.
 * Checks what kind of keyboard key has been typed. This
 * key will be handled appropriatly.
 * @param ch keyboard key
 */
void
handle_keyboard_hit(int ch)
{
    switch (ch)
    {
        case KEY_STAB:
        case 9: // TAB
            on_key_tab();
            break;

        case KEY_ENTER:
        case 10: // ENTER
            on_key_enter();
            break;

        case KEY_BACKSPACE:
        case KEY_DC:
        case 127: // BACKSPACE
            on_key_backspace();
            break;

        case KEY_UP:
            on_key_up();
            break;

        case KEY_PPAGE:
            on_page_up();
            break;

        case KEY_DOWN:
            on_key_down();
            break;

        case KEY_NPAGE:
            on_page_down();
            break;

        case KEY_LEFT:
            on_key_left();
            break;

        case KEY_RIGHT:
            on_key_right();
            break;

        default:
            on_key_ascii(ch);
    }
}


/**
 * Handles tab key hits.
 */
void
on_key_tab()
{
    int cur_win, next_win, x, y;
    next_win = (current_winnr() + 1) % WINDOW_AMOUNT; // determine next window
    _win_cur = get_win(next_win); // switch to next window
    cur_win = current_winnr();

    // move to new window
    if (_win_cur->y_count >= _win_cur->h)
    {
        move_win(_win_cur, _win_cur->y_cursor, _win_cur->x_cursor);
    }
    else
    {
        move_win(_win_cur, 0, _win_cur->x_cursor);
    }

    refresh_screen();
}


/**
 * Handles enter key hits.
 */
void
on_key_enter()
{
    int height, width, len, cur_row;
    char* input;
    // get width of input window
    getmaxyx(_win_inp->win, height, width);
    // allocate memory for displayed input
    input = malloc(width + 1);

    if (input == NULL)
    {
        exit(1);
    }

    // fetch value of buffer from input window
    mvwinnstr(_win_inp->win, 0, 0, input, _win_inp->x_count);
    input[_win_inp->x_count]     = '\n'; // append newline
    input[_win_inp->x_count + 1] = '\0';
    // print value to message window
    append_message(_win_msg, SELF, MSGTYPE_SELF, input);
    handle_sock_out(input); // write input to process via ipc
    free(input);
    // reset column cursor
    col_position(_win_inp, _win_inp->x_count * -1);
    move_win(_win_inp, _win_inp->y_cursor, _win_inp->x_cursor);
    // clear input window and refresh windows
    werase(_win_inp->win);
    refresh_screen();
}


/**
 * Handles backspace key hits.
 */
void
on_key_backspace()
{
    int y_pos, x_pos;

    // if cursor is not at the most left position
    if (_win_inp->x_cursor > 0)
    {
        getyx(_win_inp->win, y_pos, x_pos);
        mvwdelch(_win_inp->win, y_pos, x_pos - 1);
        wrefresh(_win_inp->win);
        col_position(_win_inp, -1);
    }
}


/**
 * Handles arrow up key hits.
 */
void
on_key_up()
{
    if (current_winnr() == WINDOW_MSG)
    {
        // scroll window up 1 row
        scroll_win(_win_msg, 1);
    }
}


/**
 * Handles page up key hits.
 */
void
on_page_up()
{
    if (current_winnr() == WINDOW_MSG)
    {
        // scroll window up 1 page
        scroll_win(_win_msg, _win_msg->h + 1);
    }
}


/**
 * Handles arrow down key hits.
 */
void
on_key_down()
{
    if (current_winnr() == WINDOW_MSG)
    {
        // move window down 1 row
        scroll_win(_win_msg, -1);
    }
}


/**
 * Handles page down key hits.
 */
void
on_page_down()
{
    if (current_winnr() == WINDOW_MSG)
    {
        // move window down 1 page
        scroll_win(_win_msg, _win_msg->h * -1 - 1);
    }
}


/**
 * Handles arrow left key hits.
 */
void
on_key_left()
{
    int y_pos, x_pos;

    // if cursor is not at the most left position
    if (_win_inp->x_cursor > 0)
    {
        getyx(_win_inp->win, y_pos, x_pos);
        move_win(_win_inp, y_pos, x_pos - 1);
        wrefresh(_win_inp->win);
        col_cursor(_win_inp, -1);
    }
}


/**
 * Handles arrow right key hits.
 */
void
on_key_right()
{
    int y_pos, x_pos;
    int y_mpos, x_mpos;
    getyx(_win_inp->win, y_pos, x_pos);
    getmaxyx(_win_inp->win, y_mpos, x_mpos);

    if (x_pos < _win_inp->x_count && x_pos < x_mpos - 1)
    {
        move_win(_win_inp, y_pos, x_pos + 1);
        wrefresh(_win_inp->win);
        col_cursor(_win_inp, 1);
    }
}


/**
 * Handles key hits that generate printable characters (ASCII).
 */
void
on_key_ascii(int ch)
{
    int height = 0, width = 0;
    int max_height = 0, max_width = 0;
    // get max width
    getmaxyx(_win_inp->win, max_height, max_width);

    if (_win_inp->x_count < _win_inp->w - 1 && ch >= 20 && ch <= 126)
    {
        // print character
        winsch(_win_inp->win, ch);
        // move 1 char right
        getyx(_win_inp->win, height, width);
        move_win(_win_inp, height, width + 1);
        wrefresh(_win_inp->win);
        // increase column cursor
        col_position(_win_inp, 1);
    }
}


/**
 * Prints a string at the current cursor position in the given chat window.
 * This function prints a given string in the given chat window with the
 * specified attributes.
 * @param win  Pointer to chat window structure
 * @param str  String to print
 * @param attr Ncurses attributes applied to string
 * @return OK on success, ERR on failure
 * @see ncurses.h
 */
int
print_string(DWINDOW_T* win, char* str, chtype attr)
{
    int len;
    wattron(win->win, attr);

    if (ERR == wprintw(win->win, "%s", str))
    {
        return ERR;
    }

    wattroff(win->win, attr);
    wrefresh(win->win);
    return OK;
}


/**
 * Prints a line at the current cursor position in the given chat window.
 * This function prints a given  message as chat line in the given chat window
 * and uses the colors specified for user input.
 * @param win  Pointer to chat window structure
 * @param nickname Nickname that will be print and that precedes the message.
 * @param msg  Message to print
 * @return OK on success, ERR on failure
 * @see ncurses.h
 */
int
print_line_self(DWINDOW_T* win, char* nickname, char* msg)
{
    return print_line(
               win,
               nickname, A_BOLD   | COLOR_PAIR(COLOR_NICKNAME_SELF),
               msg,      A_NORMAL | COLOR_PAIR(COLOR_MESSAGE_SELF));
}


/**
 * Prints a line at the current cursor position in the given chat window.
 * This function prints a given  message as chat line in the given chat window
 * and uses the colors specified for received messages from contacts.
 * @param win  Pointer to chat window structure
 * @param nickname Nickname that will be print and that precedes the message.
 * @param msg  Message to print
 * @return OK on success, ERR on failure
 * @see ncurses.h
 */
int
print_line_contact(DWINDOW_T* win, char* nickname, char* msg)
{
    return print_line(
               win,
               nickname, A_BOLD   | COLOR_PAIR(COLOR_NICKNAME_CONTACT),
               msg,      A_NORMAL | COLOR_PAIR(COLOR_MESSAGE_CONTACT));
}


/**
 * Prints a line at the current cursor position in the given chat window.
 * This function prints a given  message as chat line in the given chat window
 * and uses the colors specified for received messages from the system.
 * @param win  Pointer to chat window structure
 * @param nickname Nickname that will be print and that precedes the message.
 * @param msg  Message to print
 * @return OK on success, ERR on failure
 * @see ncurses.h
 */
int
print_line_system(DWINDOW_T* win, char* nickname, char* msg)
{
    return print_line(
               win,
               nickname, A_BOLD   | COLOR_PAIR(COLOR_NICKNAME_SYSTEM),
               msg,      A_NORMAL | COLOR_PAIR(COLOR_MESSAGE_SYSTEM));
}


/**
 * Prints a line at the current cursor position in the given chat window.
 * This function prints a given  message as chat line in the given chat window.
 * Supported attributes can be manually defined for nickname and the message itself.
 * @param win  Pointer to chat window structure
 * @param nickname Nickname that will be print and that precedes the message.
 * @param nickname_attr Ncurses attributes for nickname
 * @param msg  Message to print
 * @param msg_attr Ncurses attributes for message
 * @return OK on success, ERR on failure
 * @see ncurses.h
 */
int
print_line(DWINDOW_T* win, char* nickname, chtype nickname_attr,  char* msg,
           chtype msg_attr)
{
    int len = 0;
    int ok  = OK;
    time_t now = time (0);
    char dt[100];
    strftime (dt, 100, "%d. %b %Y %H:%M ", localtime (&now));
    // print formatted chat line
    wmove(win->win, win->y_count, 0);
    ok += print_string(win, dt,        A_BOLD   | COLOR_PAIR(COLOR_DATE_TIME));
    //ok += print_string(win, SEPARATOR, A_BOLD   | COLOR_PAIR(COLOR_SEPARATOR));
    ok += print_string(win, "[",       nickname_attr);
    ok += print_string(win, nickname,  nickname_attr);
    ok += print_string(win, "]",       nickname_attr);
    ok += print_string(win, PROMPT,    A_BOLD   | COLOR_PAIR(COLOR_SEPARATOR));
    ok += print_string(win, msg,       msg_attr);
    // append newline if non is given
    len = strlen(msg);

    if (msg[len - 1] != '\n')
    {
        ok += print_string(win, "\n\n",  msg_attr);
    }
    else
    {
        ok += print_string(win, "\n",  msg_attr);
    }

    return ok;
}


/**
 * Appends a message to the given window.
 * This functions appends a text to the given window using the given nickname
 * and message. It uses the type parameter to determine the colors for the
 * message. Furthermore the window will be autoscrolled using a history buffer
 * (only works with ncurses pads).
 * @param win  Pointer to chat window structure
 * @param nickname Nickname that will be print and that precedes the message.
 * @param type Type of message (contact, self, system)
 * @param fmt Format string of message
 * @param args Argument of format string
 * @see enum msgtypes of dchat-gui.h
 */
void
vappend_message(DWINDOW_T* win, char* nickname, int type, char* fmt,
                va_list args)
{
    int ok = 0, page = 1;
    int start, end;
    int row_after, col_after;
    va_list copy;
    size_t  len;
    char*   msg;
    // copy format string arguments
    va_copy(copy, args);
    len = vsnprintf(0, 0, fmt, copy); // determine length of formatted string
    va_end(copy);

    if ((msg = malloc(len + 1)) != 0)
    {
        vsnprintf(msg, len+1, fmt, args);
    }
    else
    {
        exit(1);
    }

    do
    {
        switch (type)
        {
            case MSGTYPE_SELF:
                ok = print_line_self(win, nickname, msg);
                break;

            case MSGTYPE_CONTACT:
                ok = print_line_contact(win, nickname, msg);
                break;

            case MSGTYPE_SYSTEM:
                ok = print_line_system(win, nickname, msg);
                break;

            default:
                ok = print_line_self(win, nickname, msg);
        }

        if (ok == 0)
        {
            // adjust cursor position
            getyx(win->win, row_after, col_after);
            set_row_position(win, row_after);
            break;
        }

        start = win->h * page;        // start copying from the second page
        end   = win->y_count -
                start; // end copying right before the line where the error occured
        copywin(win->win, win->win, start, 0, 0, 0, end, win->w - 1, FALSE);
        set_row_position(win, end);   // adjust cursor position
    }
    while (win->h * ++page < win->h_total); // retry if deleted page is not enough

    refresh_screen();
}


/**
 * Appends a message to the given window.
 * This functions appends a text to the given window using the given nickname
 * and message. It uses the type parameter to determine the colors for the
 * message. Furthermore the window will be autoscrolled using a history buffer
 * (only works with ncurses pads).
 * @param win  Pointer to chat window structure
 * @param nickname Nickname that will be print and that precedes the message.
 * @param type Type of message (contact, self, system)
 * @param fmt Format string of message
 * @param ... variable argumens
 * @see enum msgtypes of dchat-gui.h
 */
void
append_message(DWINDOW_T* win, char* nickname, int type, char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vappend_message(win, nickname, type, fmt, args);
    va_end(args);
}


/**
 * Appends a message to the given window (Thread-safe).
 * This functions thread-safely appends a text to the given window using the given nickname
 * and message. It uses the type parameter to determine the colors for the
 * message. Furthermore the window will be autoscrolled using a history buffer
 * (only works with ncurses pads).
 * @param win  Pointer to chat window structure
 * @param nickname Nickname that will be print and that precedes the message.
 * @param type Type of message (contact, self, system)
 * @param fmt Format string of message
 * @param ... variable argumens
 * @see enum msgtypes of dchat-gui.h
 */
void
append_message_sync(DWINDOW_T* win, char* nickname, int type, char* fmt, ...)
{
    va_list args;
    pthread_mutex_lock(&_win_lock);
    va_start(args, fmt);
    vappend_message(win, nickname, type, fmt, args);
    va_end(args);
    pthread_mutex_unlock(&_win_lock);
}


/**
 *  Read a line terminated with \\n from a file descriptor.
 *  Reads a line from the given file descriptor until \\n is found.
 *  @param fd   File descriptor to read from
 *  @param line Double pointer used for dynamic memory allocation since
 *              characters will be stored on the heap.
 *  @return: length of bytes read, 0 on EOF, -1 on error
 */
int
read_line(int fd, char** line)
{
    char* ptr;             // line pointer
    char* alc_ptr = NULL;  // used for realloc
    int len = 1;           // current length of string (at least 1 char)
    int ret;               // return value
    *line = NULL;

    do
    {
        // allocate memory for new character
        alc_ptr = realloc(*line, len + 1);

        if (alc_ptr == NULL)
        {
            if (*line != NULL)
            {
                free(*line);
            }

            exit(1);
        }

        *line = alc_ptr;
        ptr = *line + len - 1;
        len++;
    }
    while ((ret = read(fd, ptr, 1)) > 0 && *ptr != '\n');

    // on error or EOF of read
    if (ret <= 0)
    {
        free(*line);
        return ret;
    }

    // terminate string
    *(ptr) = '\0';
    return len - 1; // length of string excluding \0
}


/**
 *  Connects to a unix domain local socket.
 *  @param local_path Local socket path to connect to
 *  @return File descriptor or -1 on error
 */
int
unix_connect(char* local_path)
{
    int fd;
    char* error;
    struct sockaddr_un unix_addr;
    memset(&unix_addr, 0, sizeof(unix_addr));
    unix_addr.sun_family = PF_LOCAL;
    strcat(unix_addr.sun_path, local_path);

    if ((fd = socket(PF_LOCAL, SOCK_STREAM, 0)) == -1)
    {
        return -1;
    }

    if (connect(fd, (struct sockaddr*) &unix_addr, sizeof(unix_addr)) == -1)
    {
        close(fd);
        return -1;
    }

    return fd;
}


/**
 *  Initializes the global inter process communication structure, used for this GUI.
 *  @return 0 on success, -1 otherwise
 */
int
init_ipc()
{
    memset(&_ipc, 0, sizeof(_ipc));

    // mutex for reconnect
    if (pthread_mutex_init(&_ipc.lock, NULL) != 0)
    {
        return -1;
    }

    // reconnect condition
    if (pthread_cond_init(&_ipc.cond, NULL) != 0)
    {
        return -1;
    }

    return 0;
}


/**
 *  Closes all open sockets of the global IPC structure.
 */
void
free_unix_socks()
{
    if (_ipc.inp_sock != 0)
    {
        close(_ipc.inp_sock);
    }

    if (_ipc.out_sock != 0)
    {
        close(_ipc.out_sock);
    }

    if (_ipc.log_sock != 0)
    {
        close(_ipc.log_sock);
    }
}


/**
 *  Frees all ressources and closes all open sockets of the global IPC structure.
 */
void
free_ipc()
{
    free_unix_socks();
    pthread_mutex_destroy(&_ipc.lock);
    pthread_cond_destroy(&_ipc.cond);
}


/**
 *  Signals the IPC thread to reconnect to the UI unix sockets.
 */
void
signal_reconnect()
{
    pthread_mutex_lock(&_ipc.lock);
    _ipc.reconnect = 1;
    pthread_cond_signal(&_ipc.cond);
    pthread_mutex_unlock(&_ipc.lock);
}


/**
 *  Thread function that handles incoming data from the input UI socket.
 *  @param ptr Not used
 *  @return NULL
 */
void*
handle_sock_inp(void* ptr)
{
    char* line;     // line read from socket fd
    char* nickname;
    char* msg;
    char* save_ptr; // used for strtok
    char delim = ';';

    // is input socket initialized; no EOF and no error?
    while ( _ipc.inp_sock != 0 && read_line(_ipc.inp_sock, &line) > 0 )
    {
        // split line: line format -> nickname;message
        if ((nickname = strtok_r(line, &delim, &save_ptr)) == NULL)
        {
            free(line);
            continue;
        }
        else if ((msg = strtok_r(NULL, &delim, &save_ptr)) == NULL)
        {
            free(line);
            continue;
        }

        append_message_sync(_win_msg, nickname, MSGTYPE_CONTACT, msg);
        free(line);
    }

    // on error, eof, not initialized -> reconnect!
    append_message_sync(_win_msg, SYSTEM, MSGTYPE_SYSTEM,
                        "No connection to input socket: '%s'", strerror(errno));
    signal_reconnect();
    pthread_exit(NULL);
}


/**
 *  Thread function that handles outgoing data to the output UI socket.
 *  @param ptr Not used
 *  @return NULL
 */
void*
handle_sock_out(void* ptr)
{
    int len;
    len = strlen((char*)ptr);

    // on error, eof, not initialized -> reconnect!
    if (_ipc.out_sock == 0 || write(_ipc.out_sock, ptr, len) == -1)
    {
        append_message(_win_msg, SYSTEM, MSGTYPE_SYSTEM,
                       "No connection to output socket: '%s'", strerror(errno));
        signal_reconnect();
    }

    return NULL;
}


/**
 *  Thread function that handles incoming data from the logging UI socket.
 *  @param ptr Not used
 *  @return NULL
 */
void*
handle_sock_log(void* ptr)
{
    char* line;

    // is logging socket initialized; no EOF and no error?
    while ( _ipc.log_sock != 0 && read_line(_ipc.log_sock, &line) > 0 )
    {
        append_message_sync(_win_msg, SYSTEM, MSGTYPE_SYSTEM, line);
        free(line);
    }

    // on error, eof, not initialized -> reconnect!
    append_message_sync(_win_msg, SYSTEM, MSGTYPE_SYSTEM,
                        "No connection to logging socket: '%s'", strerror(errno));
    signal_reconnect();
    pthread_exit(NULL);
}


/**
 *  Thread function that handles the connections to the UI unix sockets.
 *  @param ptr Not used
 *  @return NULL
 */
void*
th_ipc_connector(void* ptr)
{
    int ival = 5;
    int* socks[] =
    {
        &_ipc.inp_sock,
        &_ipc.out_sock,
        &_ipc.log_sock
    };
    char* sock_paths[] =
    {
        INP_SOCK_PATH,
        OUT_SOCK_PATH,
        LOG_SOCK_PATH
    };
    pthread_t th_inp, th_out, th_log;

    // initialize global IPC structure
    if (init_ipc() == -1)
    {
        append_message_sync(_win_msg, SYSTEM, MSGTYPE_SYSTEM,
                            "Inter-Process-Communication failed!\n%sReason: '%s'", strerror(errno));
    }

    while (1)
    {
        // connect to all UI sockets
        for (int i = 0; i < sizeof(socks)/sizeof(int*) ; i++)
        {
            if ((*socks[i] = unix_connect(sock_paths[i])) == -1)
            {
                append_message_sync(_win_msg, SYSTEM, MSGTYPE_SYSTEM,
                                    "Connection to '%s' failed!\nReason: '%s'", sock_paths[i], strerror(errno));
                free_unix_socks();
                sleep(ival);
                i = -1;
                continue;
            }
        }

        append_message_sync(_win_msg, SYSTEM, MSGTYPE_SYSTEM,
                            "Connection established!");
        // start all socket threads
        pthread_create(&th_inp, NULL, (void*) handle_sock_inp, NULL);
        pthread_create(&th_log, NULL, (void*) handle_sock_log, NULL);
        pthread_mutex_lock(
            &_ipc.lock); // lock ipc mutex to start socket threads and to wait for reconnect cond.

        // check reconnect flag after wakeup if thread should reconnect to unix sockets
        while (!_ipc.reconnect)
        {
            pthread_cond_wait(&_ipc.cond, &_ipc.lock);
        }

        pthread_mutex_unlock(&_ipc.lock);
        // wait for threads to finish
        free_unix_socks();  // close all open sockets
        pthread_join(th_inp, NULL);
        pthread_join(th_log, NULL);
        append_message_sync(_win_msg, SYSTEM, MSGTYPE_SYSTEM, "Reconnecting...");
        _ipc.reconnect = 0; // reset reconnect condition
    }

    free_ipc();
    pthread_exit(NULL);
}
