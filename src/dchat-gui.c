#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>


#define ME        "[CRISM]"
#define PROMPT    "$\n"
#define SEPARATOR " - "


typedef struct DWINDOW
{
    WINDOW* win;
    int y;
    int x;
    int w_total;
    int w;
    int h_total;
    int h;
    int y_cursor;
    int x_cursor;
    int y_count;
    int x_count;
} DWINDOW_T;


enum windows
{
    WINDOW_MSG,
    WINDOW_USR,
    WINDOW_INP
};


enum colors
{
    COLOR_WINDOW_MESSAGE = 1,
    COLOR_WINDOW_USER,
    COLOR_WINDOW_INPUT,
    COLOR_DATE_TIME,
    COLOR_NICKNAME,
    COLOR_CONTACT_NICKNAME,
    COLOR_SEPARATOR,
    COLOR_MESSAGE,
    COLOR_STDSCR,
};


static pthread_mutex_t _win_lock;
static DWINDOW_T* _win_msg;
static DWINDOW_T* _win_usr;
static DWINDOW_T* _win_inp;
static DWINDOW_T* _win_cur;


void
init_wins();


void
free_wins();


void
start_gui();


void
stop_gui();


void
set_position(DWINDOW_T* win, int y);


void
set_cursor(DWINDOW_T* win, int y);


void
win_resize(int signum);


int
print_string(DWINDOW_T* win, char* str, chtype attr);


int
print_line(DWINDOW_T* win, char* dt, char* nickname, char* msg);


void
append_text(DWINDOW_T* win, char* txt);


int
current_window();


DWINDOW_T*
get_win(int winnr);


void
winscrl(DWINDOW_T* win, int n);


void
on_key_tab();


void
on_key_enter();


void
on_key_backspace();


void
on_key_up();


void
on_page_up();


void
on_key_down();


void
on_page_down();


void
on_key_left();


void
on_key_right();


void
on_key_ascii(int ch);


void
handle_keyboard_input(int ch);


void
read_input();


void
init_colors(void);


void
init_gui(float ratio_height, float ratio_width);


WINDOW*
create_padwin(int max_height, int max_width, int height, int width, int starty,
              int startx, const int col_bkgd);


WINDOW*
create_win(int height, int width, int starty, int startx, const int col_bkgd);


void
refresh_winall();


void
refresh_pad(DWINDOW_T* win);


int
main()
{
    if (pthread_mutex_init(&_win_lock, NULL) != 0)
    {
        exit(1); //FIXME use fatal()
    }

    // check for resize events
    signal(SIGWINCH, win_resize);
    //pthread_mutex_lock(&_win_lock);
    start_gui();
    read_input();
    stop_gui();
    //pthread_mutex_unlock(&_win_lock);
    pthread_mutex_destroy(&_win_lock);
    return 0;
}


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


void
start_gui()
{
    initscr();            // initialize ncurses
    cbreak();             // interprete control characters (CTRL-C, ...)
    noecho();             // dont print escape codes
    keypad(stdscr, TRUE); // make use of special key (arrow, ...)
    init_colors();        // initialize available colors
    init_wins();          // initialize all available windows
    init_gui(0.9, 0.8);   // calculate size / position and render gui
}


void
stop_gui()
{
    // release resources and refresh screen
    free_wins();
    endwin();
    refresh();
    clear();
}


void
set_position(DWINDOW_T* win, int y)
{
    if(y < 0)
        y = 0;
    win->y_count = y;
    set_cursor(win, y - win->h);
}


void
set_cursor(DWINDOW_T* win, int y)
{

    if(y < 0)
        y = 0;
    if(y > win->y_count - win->h)
        y = win->y_count - win->h;

    win->y_cursor = y;
}


void
win_resize(int signum)
{
    stop_gui();
    start_gui();
}


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


int
print_line(DWINDOW_T* win, char* dt, char* nickname, char* msg)
{
    int len = 0;
    int ok  = OK;
    len = strlen(msg);
    // print formatted chat line
    wmove(win->win, win->y_count, 0);
    ok += print_string(win, dt,        A_BOLD   | COLOR_PAIR(COLOR_DATE_TIME));
    ok += print_string(win, SEPARATOR, A_BOLD   | COLOR_PAIR(COLOR_SEPARATOR));
    ok += print_string(win, nickname,  A_BOLD   | COLOR_PAIR(COLOR_NICKNAME));
    ok += print_string(win, PROMPT,    A_BOLD   | COLOR_PAIR(COLOR_SEPARATOR));
    ok += print_string(win, msg,       A_NORMAL | COLOR_PAIR(COLOR_WINDOW_MESSAGE));

    // append newline if non is given
    if (msg[len - 1] != '\n')
    {
        ok += print_string(win, "\n\n",  A_NORMAL | COLOR_PAIR(COLOR_MESSAGE));
    }
    else
    {
        ok += print_string(win, "\n",  A_NORMAL | COLOR_PAIR(COLOR_MESSAGE));
    }

    return ok;
}


void
append_text(DWINDOW_T* win, char* txt)
{
    int start, end;
    int row_after, col_after;
    time_t rawtime;
    struct tm* timeinfo;
    char* dt;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    dt = asctime(timeinfo);
    dt[strlen(dt) - 1] = '\0'; // remove \n

    if (print_line(win, dt, ME, txt) != 0) // FIXME: use dchat_conf and pdu instead
    {
        start = win->h;                   // start copying from the second page
        end   = win->y_count - start -
                2; // end copying right before the line where the error occured
        copywin(win->win, win->win, start, 0, 0, 0, end, win->w - 1, FALSE);
        set_position(win, end); // adjust y-cursor position
        print_line(win, dt, ME, txt);           // try to print line again
        getyx(win->win, row_after, col_after);  // adjust cursor position
        set_position(win, row_after); // adjust y-cursor position
    }
    else
    {
        getyx(win->win, row_after, col_after);
        set_position(win, row_after); // adjust y-cursor position
    }
}


int
current_window()
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


void
winscrl(DWINDOW_T* win, int n)
{
    // invert
    n *= -1;

    // up
    if (n < 0)
    {
        if (win->y_cursor > 0)
        {
            set_cursor(win, win->y_cursor + n);
        }
    }
    // down
    else if (n > 0)
    {
        set_cursor(win, win->y_cursor + n);
    }

    // position cursor at the top left corner
    wmove(win->win, win->y_cursor, 0);
    refresh_pad(win);
}


void
on_key_tab()
{
    int cur_win, next_win, x, y;
    next_win = (current_window() + 1) % 3;
    _win_cur = get_win(next_win);
    cur_win = current_window();

    if (cur_win == WINDOW_MSG || cur_win == WINDOW_USR)
    {
        // move to the first row of the current pad window
        if (_win_msg->y_count >= _win_msg->h)
        {
            wmove(_win_cur->win, _win_cur->y_cursor, 0);
        }
        else
        {
            wmove(_win_cur->win, 0, 0);
        }

        refresh_pad(_win_cur);
    }
    else
    {
        wmove(_win_cur->win, 0, _win_cur->x_cursor);
        wrefresh(_win_cur->win);
    }
}


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
    input[width] = '\0';
    // print value to message window
    append_text(_win_msg, input);
    free(input);
    // reset input buffer counter
    // reset input column counter
    _win_inp->x_count  = 0;
    _win_inp->x_cursor = 0;
    // clear input window and refresh windows
    wclear(_win_inp->win);
    refresh_winall();
}


void
on_key_backspace()
{
    int y_pos, x_pos;

    if (_win_inp->x_cursor > 0)
    {
        getyx(_win_inp->win, y_pos, x_pos);
        mvwdelch(_win_inp->win, y_pos, x_pos - 1);
        wrefresh(_win_inp->win);
        _win_inp->x_cursor--;
        _win_inp->x_count--;
    }
}


void
on_key_up()
{
    if (current_window() == WINDOW_MSG)
    {
        winscrl(_win_msg, 1);
    }
}


void
on_page_up()
{
    if (current_window() == WINDOW_MSG)
    {
        winscrl(_win_msg, _win_msg->h);
    }
}


void
on_key_down()
{
    if (current_window() == WINDOW_MSG)
    {
        winscrl(_win_msg, -1);
    }
}


void
on_page_down()
{
    if (current_window() == WINDOW_MSG)
    {
        winscrl(_win_msg, _win_msg->h * -1);
    }
}


void
on_key_left()
{
    int y_pos, x_pos;

    if (_win_inp->x_cursor > 0)
    {
        getyx(_win_inp->win, y_pos, x_pos);
        wmove(_win_inp->win, y_pos, x_pos - 1);
        wrefresh(_win_inp->win);
        _win_inp->x_cursor--;
    }
}


void
on_key_right()
{
    int y_pos, x_pos;
    int y_mpos, x_mpos;
    getyx(_win_inp->win, y_pos, x_pos);
    getmaxyx(_win_inp->win, y_mpos, x_mpos);

    if (x_pos < _win_inp->x_count && x_pos < x_mpos - 1)
    {
        wmove(_win_inp->win, y_pos, x_pos + 1);
        wrefresh(_win_inp->win);
        _win_inp->x_cursor++;
    }
}


void
on_key_ascii(int ch)
{
    int height = 0, width = 0;
    int max_height = 0, max_width = 0;
    // get max width
    getmaxyx(_win_inp->win, max_height, max_width);

    if (_win_inp->x_count < max_width - 1 && ch >= 20 && ch <= 126)
    {
        // print character
        winsch(_win_inp->win, ch);
        // move 1 char right
        getyx(_win_inp->win, height, width);
        //FIXME
        _win_cur = _win_inp;
        wmove(_win_inp->win, height, width + 1);
        wrefresh(_win_inp->win);
        // increase buf counter
        _win_inp->x_count++;
        _win_inp->x_cursor++;
    }
}


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
        init_pair(COLOR_NICKNAME,         COLOR_GREEN,   COLOR_BLACK);
        init_pair(COLOR_CONTACT_NICKNAME, COLOR_CYAN,    COLOR_BLACK);
        init_pair(COLOR_SEPARATOR,        COLOR_CYAN,    COLOR_BLACK);
        init_pair(COLOR_MESSAGE,          COLOR_BLACK,   COLOR_BLACK);
        init_pair(COLOR_STDSCR,           -1,            COLOR_YELLOW);
    }
}


void
init_gui(float ratio_height, float ratio_width)
{
    // dimension of virtual base window including padding
    int o_base    = 2;          // base offset
    int h_base    = LINES - o_base;
    int w_base    = COLS;
    int p_base    = 2;          // col/row padding
    // dimension of message field within virtual base window
    _win_msg->h       = h_base * ratio_height - p_base;
    _win_msg->h_total = _win_msg->h * 16;
    _win_msg->w       = w_base * ratio_width - p_base;
    _win_msg->w_total = _win_msg->w;
    // dimension of user field within virtual base window
    _win_usr->h       = _win_msg->h;
    _win_usr->h_total = _win_usr->h * 16;
    _win_usr->w       = w_base * (1 - ratio_width) - p_base / 2;
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
    _win_usr->x = _win_msg->x + _win_msg->w + p_base;
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


WINDOW*
create_padwin(int max_height, int max_width, int height, int width, int starty,
              int startx, const int col_bkgd)
{
    WINDOW* local_pad;
    local_pad = newpad(max_height, max_width);

    if (has_colors())
    {
        wbkgd(local_pad, COLOR_PAIR(col_bkgd));
    }

    prefresh(local_pad, 0, 0, starty, startx, starty + height - 1,
             startx + width - 1);
    return local_pad;
}


WINDOW*
create_win(int height, int width, int starty, int startx, const int col_bkgd)
{
    WINDOW* local_win;
    int attr = 0;
    local_win = newwin(height, width, starty, startx);

    if (has_colors())
    {
        wbkgd(local_win, COLOR_PAIR(col_bkgd));
    }

    wrefresh(local_win);
    return local_win;
}


void
refresh_winall()
{
    refresh_pad(_win_msg);
    wrefresh(_win_usr->win);
    wrefresh(_win_inp->win);
}


void
refresh_pad(DWINDOW_T* win)
{
    // autoscroll
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
