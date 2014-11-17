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


#ifndef DCHAT_GUI_H
#define DCHAT_GUI_H


//*********************************
//         CHAT SETTINGS
//*********************************
#define SELF      "CRISM"
#define SYSTEM    "SYSTEM"
#define PROMPT    "$\n"
#define SEPARATOR " - "


//*********************************
//         STRUCTURES/ENUMS
//*********************************

/*!
 * Structure of a chat window.
 * Specifies dimension and position of window.
 * It also contains counters for the current
 * cursor position as well as a row/column counter
 * that marks the current end position of a window.
 */
typedef struct DWINDOW
{
    WINDOW* win;  //<! Pointer to ncurses window structure
    int y;        //<! Y coordinate of window
    int x;        //<! X coordinate of window
    int w_total;  //<! Total width of window
    int w;        //<! Width of window currently shown
    int h_total;  //<! Total height of window
    int h;        //<! Height of window currently shown
    int y_cursor; //<! Cursor row position of window
    int x_cursor; //<! Cursor column position of window
    int y_count;  //<! Row pointer of window
    int x_count;  //<! Column pointer of window
} DWINDOW_T;


/*!
 * Source of message.
 * This enum defines the possible sources of messages.
 * A message can be retrieved from the system (log, warn, ...),
 * from a contact or the message was composed by the
 * user himself.
 */
enum msgtypes
{
    MSGTYPE_SELF,
    MSGTYPE_CONTACT,
    MSGTYPE_SYSTEM
};


/*!
 * Type of window.
 * This enum defines constants for the possible windows
 * within this GUI.
 */
enum windows
{
    WINDOW_MSG,
    WINDOW_USR,
    WINDOW_INP,
    WINDOW_AMOUNT
};


/*!
 * Type of color.
 * This enum defines all possible colors
 * that can be used within this chat GUI.
 */
enum colors
{
    COLOR_WINDOW_MESSAGE = 1,
    COLOR_WINDOW_USER,
    COLOR_WINDOW_INPUT,
    COLOR_DATE_TIME,
    COLOR_SEPARATOR,
    COLOR_NICKNAME_SELF,
    COLOR_NICKNAME_CONTACT,
    COLOR_NICKNAME_SYSTEM,
    COLOR_MESSAGE_SELF,
    COLOR_MESSAGE_CONTACT,
    COLOR_MESSAGE_SYSTEM,
    COLOR_STDSCR,
};


//*********************************
//        INIT FUNCTIONS
//*********************************
void init_colors(void);
void init_wins();
void init_gui(float ratio_height, float ratio_width);
void start_gui();
void free_wins();
void stop_gui();


//*********************************
//        RENDER FUNCTIONS
//*********************************
WINDOW* create_padwin(int max_height, int max_width, int height, int width,
                      int starty, int startx, const int col_bkgd);
WINDOW* create_win(int height, int width, int starty, int startx,
                   const int col_bkgd);
void refresh_padwin(DWINDOW_T* win);
void refresh_current();
void refresh_screen();


//*********************************
//        WINDOW FUNCTIONS
//*********************************
int current_winnr();
DWINDOW_T* get_win(int winnr);
void resize_win(int signum);
void scroll_win(DWINDOW_T* win, int n);
void move_win(DWINDOW_T* win, int y, int x);
void set_row_position(DWINDOW_T* win, int y);
void set_row_cursor(DWINDOW_T* win, int y);
void col_position(DWINDOW_T* win, int n);
void col_cursor(DWINDOW_T* win, int n);


//*********************************
//    INPUT HANDLER FUNCTIONS
//*********************************
void read_input();
void handle_keyboard_hit(int ch);
void on_key_tab();
void on_key_enter();
void on_key_backspace();
void on_key_up();
void on_page_up();
void on_key_down();
void on_page_down();
void on_key_left();
void on_key_right();
void on_key_ascii(int ch);


//*********************************
//       PRINT FUNCTIONS
//*********************************
int print_string(DWINDOW_T* win, char* str, chtype attr);
int print_line_self(DWINDOW_T* win, char* nickname, char* msg);
int print_line_contact(DWINDOW_T* win, char* nickname, char* msg);
int print_line_system(DWINDOW_T* win, char* nickname, char* msg);
int print_line(DWINDOW_T* win, char* nickname, chtype nickname_attr,  char* msg,
               chtype msg_attr);
void vappend_message(DWINDOW_T* win, char* nickname, int type, char* fmt,
                     va_list args);
void append_message(DWINDOW_T* win, char* nickname, int type, char* fmt, ...);
void append_message_sync(DWINDOW_T* win, char* nickname, int type, char* fmt,
                         ...);


#endif
